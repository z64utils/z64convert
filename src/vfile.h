/* vfile.h: virtual file so writing is faster */

/* how it works:
 * * constantly fputc'ing etc is slow, esp on windows;
 * * we instead allocate a buffer that gets filled, and
 * * write its contents to the disk with one fwrite
 * * when finished
 */

#ifndef VFILE_H_INCLUDED
#define VFILE_H_INCLUDED 1

#include <stdio.h>
#include <stdlib.h>

#ifndef VFILE_VISIBILITY
#define VFILE_VISIBILITY
#endif

typedef struct vfile
{
	FILE *dest;
	unsigned char *data;
	long size;
	long ofs;
	long end;
	char *filename;
	char *mode;
	FILE *(*Pfopen)(const char *, const char *);
	size_t (*Pfwrite)(const void *, size_t, size_t, FILE *);
	void (*Pdie)(const char *fmt, ...);
} VFILE;

static
int vfile__fit(VFILE *stream, size_t bytes)
{
	if (!stream)
		return 0;
	
	if (!bytes)
		return 1;
	
	/* won't fit, attempt realloc */
	if (bytes > (stream->size - stream->ofs))
	{
		stream->size *= 2;
		unsigned char *Ndata = realloc(stream->data, stream->size);
		
		if (!Ndata)
		{
			if (stream->Pdie)
				stream->Pdie("virtual file out of memory");
			return 0;
		}
		
		stream->data = Ndata;
	}
	
	return 1;
}

VFILE_VISIBILITY
int vfclose(VFILE *stream)
{
	if (stream->Pfopen && stream->filename && stream->mode)
	{
		if (!(stream->dest = stream->Pfopen(stream->filename, stream->mode)))
			return EOF;
	}
	
	if (!stream || !stream->dest || !stream->end)
		return EOF;
	
	if (stream->Pfwrite(stream->data, 1, stream->end, stream->dest) != stream->end)
		return EOF;
	
	fclose(stream->dest);
	free(stream->filename);
	free(stream->mode);
	free(stream->data);
	free(stream);
	
	return 0;
}

VFILE_VISIBILITY
int vfputc(int c, VFILE *stream)
{
	if (!vfile__fit(stream, 1))
		return EOF;
	
	/* write into buffer */
	stream->data[stream->ofs] = c;
	stream->ofs += 1;
	
	/* push end farther along */
	if (stream->ofs > stream->end)
		stream->end = stream->ofs;
	
	return (unsigned char)c;
}

VFILE_VISIBILITY
int vfgetc(VFILE *stream)
{
	if (!stream || stream->ofs == stream->end)
		return EOF;
	
	int c = stream->data[stream->ofs];
	stream->ofs += 1;
	
	return c;
}

/*VFILE_VISIBILITY
long vftell(VFILE *stream)
{
	if (!stream)
		return -1;
	
	return stream->ofs;
}*/

VFILE_VISIBILITY
size_t vfwrite(const void *ptr, size_t size, size_t nmemb, VFILE *stream)
{
	if (!vfile__fit(stream, size * nmemb))
		return 0;
	
	/* copy to byte array */
	memcpy(stream->data + stream->ofs, ptr, size * nmemb);
	
	/* increment offset */
	stream->ofs += size * nmemb;
	
	/* push end farther along */
	if (stream->ofs > stream->end)
		stream->end = stream->ofs;
	
	return size * nmemb;
}

static VFILE *vfile__new(
	long size
	, size_t (Pfwrite)(const void *, size_t, size_t, FILE *)
	, void (die)(const char *fmt, ...)
)
{
	VFILE *vf;
	
	if (!Pfwrite)
		return 0;
	
	if (!(vf = calloc(1, sizeof(*vf))))
		return 0;
	
	if (!(vf->data = malloc(size)))
	{
		free(vf);
		return 0;
	}
	
	/* init */
	vf->size = size;
	vf->Pfwrite = Pfwrite;
	vf->Pdie = die;
	
	return vf;
}

VFILE_VISIBILITY
VFILE *vfopen_FILE(
	FILE *dest
	, long size
	, size_t (Pfwrite)(const void *, size_t, size_t, FILE *)
	, void (die)(const char *fmt, ...)
)
{
	VFILE *vf;
	
	if (!dest)
		return 0;
	
	if (!(vf = vfile__new(size, Pfwrite, die)))
		return 0;
	
	/* init */
	vf->dest = dest;
	
	return vf;
}

VFILE_VISIBILITY
VFILE *vfopen_delayed(
	const char *filename
	, const char *mode
	, long size
	, size_t (Pfwrite)(const void *, size_t, size_t, FILE *)
	, void (die)(const char *fmt, ...)
	, FILE *Pfopen(const char *, const char *)
)
{
	VFILE *vf;
	
	if (!Pfopen)
		return 0;
	
	if (!(vf = vfile__new(size, Pfwrite, die)))
		return 0;
	
	/* init */
	vf->Pfopen = Pfopen;
	if (!(vf->filename = strdup(filename)))
	{
		free(vf->data);
		free(vf);
		return 0;
	}
	if (!(vf->mode = strdup(mode)))
	{
		free(vf->filename);
		free(vf->data);
		free(vf);
		return 0;
	}
	
	return vf;
}

VFILE_VISIBILITY
long vftell(VFILE *stream)
{
	if (!stream)
		return -1;
	
	return stream->ofs;
}

VFILE_VISIBILITY
int vfseek(VFILE *stream, long offset, int whence)
{
	if (!stream)
		return 1;
	
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
		return 1;
	
	switch (whence)
	{
		case SEEK_CUR:
			stream->ofs += offset;
			break;
		
		case SEEK_SET:
			stream->ofs = offset;
			break;
		
		case SEEK_END:
			stream->ofs = stream->end - offset;
			break;
	}
	
	if (stream->ofs > stream->end)
		stream->ofs = stream->end;
	
	return 0; /* success */
}

/* vfile.h versions */
static inline void vfput8(VFILE *bin, unsigned char b)
{
	vfputc(b, bin);
}
static inline void vfput16(VFILE *bin, unsigned short b)
{
	vfputc(b >> 8, bin);
	vfputc(b     , bin);
}
static inline void vfput32(VFILE *bin, unsigned int b)
{
	vfputc(b >> 24, bin);
	vfputc(b >> 16, bin);
	vfputc(b >>  8, bin);
	vfputc(b      , bin);
}
static inline void vfput64s(VFILE *bin, unsigned int hi, unsigned int lo)
{
	vfput32(bin, hi);
	vfput32(bin, lo);
}

static inline unsigned int vfget8(VFILE *bin)
{
	unsigned int rv = 0;
	rv |= vfgetc(bin) <<  0;
	return rv;
}
static inline unsigned int vfget16(VFILE *bin)
{
	unsigned int rv = 0;
	rv |= vfgetc(bin) <<  8;
	rv |= vfgetc(bin) <<  0;
	return rv;
}
static inline unsigned int vfget32(VFILE *bin)
{
	unsigned int rv = 0;
	rv |= vfgetc(bin) << 24;
	rv |= vfgetc(bin) << 16;
	rv |= vfgetc(bin) <<  8;
	rv |= vfgetc(bin) <<  0;
	return rv;
}

static inline size_t vfalign(VFILE *bin, int alignment)
{
	size_t f = vftell(bin);
	f &= (alignment - 1);
	if (!f)
		return vftell(bin);
	int i;
	for (i = 0; i < alignment - f; ++i)
		vfputc(0, bin);
	return vftell(bin);
}

#endif /* VFILE_H_INCLUDED */

