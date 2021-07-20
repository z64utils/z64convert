#ifndef PUT_H_INCLUDED
#define PUT_H_INCLUDED

static inline void put8(void *data, unsigned char b)
{
	unsigned char *d = data;
	*d = b;
}
static inline void put16(void *data, unsigned short b)
{
	unsigned char *d = data;
	d[0] = b >> 8;
	d[1] = b;
}
static inline void put32(void *data, unsigned int b)
{
	unsigned char *d = data;
	d[0] = b >> 24;
	d[1] = b >> 16;
	d[2] = b >> 8;
	d[3] = b;
}

static inline int get8(void *data)
{
	unsigned char *d = data;
	return d[0];
}

static inline unsigned short get16(void *data)
{
	unsigned char *d = data;
	return (d[0] << 8) | d[1];
}

static inline unsigned int get32(void *data)
{
	unsigned char *d = data;
	return (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
}

static inline void fput8(FILE *bin, unsigned char b)
{
	fputc(b, bin);
}
static inline void fput16(FILE *bin, unsigned short b)
{
	fputc(b >> 8, bin);
	fputc(b     , bin);
}
static inline void fput32(FILE *bin, unsigned int b)
{
	fputc(b >> 24, bin);
	fputc(b >> 16, bin);
	fputc(b >>  8, bin);
	fputc(b      , bin);
}
static inline void fput64s(FILE *bin, unsigned int hi, unsigned int lo)
{
	fput32(bin, hi);
	fput32(bin, lo);
}

static inline unsigned int fget8(FILE *bin)
{
	unsigned int rv = 0;
	rv |= fgetc(bin) <<  0;
	return rv;
}
static inline unsigned int fget16(FILE *bin)
{
	unsigned int rv = 0;
	rv |= fgetc(bin) <<  8;
	rv |= fgetc(bin) <<  0;
	return rv;
}
static inline unsigned int fget32(FILE *bin)
{
	unsigned int rv = 0;
	rv |= fgetc(bin) << 24;
	rv |= fgetc(bin) << 16;
	rv |= fgetc(bin) <<  8;
	rv |= fgetc(bin) <<  0;
	return rv;
}

static inline size_t falign(FILE *bin, int alignment)
{
	size_t f = ftell(bin);
	f &= (alignment - 1);
	if (!f)
		return ftell(bin);
	int i;
	for (i = 0; i < alignment - f; ++i)
		fputc(0, bin);
	return ftell(bin);
}

#endif /* PUT_H_INCLUDED */

