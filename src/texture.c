/* <z64.me> texture to n64 utility */
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "zobj.h" /* objexUdata */
#include "texture.h"
#include "put.h"
#include "err.h"
#include "watermark.h"
#include "doc.h"

#define TMEM_MAX 4096
#define acfunc_colors 4 // FIXME TODO needs customization

/* external dependencies */
#include "stb_image.h"
#include "n64texconv.h"

static FILE *getDocs(struct objex *obj)
{
	if (!obj || !obj->udata)
		return errmsg("objex no logfile");
	return ((struct objexUdata *)obj->udata)->docs;
}

static unsigned getBase(struct objex *obj)
{
	if (!obj || !obj->udata)
		return 0;
	return ((struct objexUdata *)obj->udata)->baseOfs;
}

/* TODO allow customization */
//static enum n64texconv_acgen acfunc = N64TEXCONV_ACGEN_EDGEXPAND;
static enum n64texconv_acgen parse_alphamode(const char *str)
{
	int i;
	
	struct {
		enum n64texconv_acgen acfunc;
		const char *name;
	} wow[] = {
		{ N64TEXCONV_ACGEN_EDGEXPAND, "edge" }
		, { N64TEXCONV_ACGEN_AVERAGE, "average" }
		, { N64TEXCONV_ACGEN_WHITE, "white" }
		, { N64TEXCONV_ACGEN_BLACK, "black" }
		, { N64TEXCONV_ACGEN_USER, "image" }
	};
	
	if (!str)
		return N64TEXCONV_ACGEN_EDGEXPAND;
	
	for (i = 0; i < sizeof(wow) / sizeof(*wow); ++i)
		if (!strcmp(str, wow[i].name))
			return wow[i].acfunc;
	
	return N64TEXCONV_ACGEN_MAX;
}
	
static const char *fmtStr[] = { "rgba", "yuv", "ci", "ia", "i" };
static const char *bppStr[] = { "4", "8", "16", "32" };
static const int fmtCount = (sizeof(fmtStr) / sizeof(*fmtStr));
static const int bppCount = (sizeof(bppStr) / sizeof(*bppStr));
static const char *gbiFmtStr[] = {
	"G_IM_FMT_RGBA"
	, "G_IM_FMT_YUV"
	, "G_IM_FMT_CI"
	, "G_IM_FMT_IA"
	, "G_IM_FMT_I"
};
static const char *gbiBppStr[] = {
	"G_IM_SIZ_4b"
	, "G_IM_SIZ_8b"
	, "G_IM_SIZ_16b"
	, "G_IM_SIZ_32b"
};

static struct objex_palette *pushpal(
	struct objex *objex
	, void *calloc(size_t, size_t)
	, const int Ncolors
)
{
	struct objex_palette *pal;
	
	if (!(pal = calloc(1, sizeof(*pal))))
		return errmsg(ERR_NOMEM);
	
	/* allocate color buffer */
	if (!(pal->colors = calloc(1, Ncolors * 4)))
		return errmsg(ERR_NOMEM);
	pal->colorsNum = Ncolors;
	
	/* link into list */
#if 0 /* prepend */
	pal->next = objex->pal;
	objex->pal = pal;
#else /* append */
	int ct = 0;
	if (objex->pal)
	{
		struct objex_palette *w;
		ct = 1;
		for (w = objex->pal; w->next; w = w->next)
			++ct;
		w->next = pal;
	}
	else
		objex->pal = pal;
#endif
	pal->objex = objex;
	pal->index = ct;
	
	return pal;
}

static void *getFmtBpp(
	struct objex_texture *tex
	, enum n64texconv_fmt *_fmt
	, enum n64texconv_bpp *_bpp
)
{
	enum n64texconv_fmt fmt;
	enum n64texconv_bpp bpp;
	char *ss = tex->format;
	
	if (!ss)
		return errmsg("no format specified for '%s'", tex->name);
	
	/* read fmt part */
	for (fmt = 0; fmt < fmtCount; ++fmt)
		if (!memcmp(ss, fmtStr[fmt], strlen(fmtStr[fmt])))
			break;
	/* advance to immediately after fmt part */
	if (fmt < fmtCount)
		ss += strlen(fmtStr[fmt]);
	/* read bpp part */
	for (bpp = 0; bpp < bppCount; ++bpp)
		if (!memcmp(ss, bppStr[bpp], strlen(bppStr[bpp])))
			break;
	/* ensure format is valid */
	if (fmt == fmtCount || bpp == bppCount)
		return errmsg("unknown format '%s'", tex->format);
	
	*_fmt = fmt;
	*_bpp = bpp;
	
	return success;
}

int texture_textureBpp(struct objex_texture *tex)
{
	enum n64texconv_fmt fmt;
	enum n64texconv_bpp bpp;
	
	if (!tex)
		return -1;
	
	if (!getFmtBpp(tex, &fmt, &bpp))
		return -1;
	
	return bpp;
}

int texture_textureFmt(struct objex_texture *tex)
{
	enum n64texconv_fmt fmt;
	enum n64texconv_bpp bpp;
	
	if (!tex)
		return -1;
	
	if (!getFmtBpp(tex, &fmt, &bpp))
		return -1;
	
	return fmt;
}

static int pathIsAbsolute(const char *path)
{
#ifdef _WIN32 /* win32: */
	return (isalpha(path[0]) && path[1] == ':');
#else /* linux: */
	return (*path == '/');
#endif
}

void *texture_loadAll(struct objex *obj)
{
	/* load all the textures */
	for (struct objex_texture *tex = obj->tex; tex; tex = tex->next)
	{
		struct texUdata *udata = 0;
		const char *which;
		const char *instead = tex->instead;
		char fn[PATH_MAX];
		int n;
		
		/* load texture data */
		which = instead ? instead : tex->filename;
		
		/* assertion: will full path to file fit? */
		if ((strlen(tex->objex->mtllibDir)
			+ 1 /* slash */
			+ strlen(which)
			+ 1 /* 0 term */) > sizeof(fn)
		)
			return errmsg("path to texture image '%s' too long", which);
		
		/* construct absolute path to file */
		if (pathIsAbsolute(which))
			strcpy(fn, which);
		else
		{
			strcpy(fn, tex->objex->mtllibDir);
			strcat(fn, "/");
			strcat(fn, which);
		}
		
		/* load texture file */
		if (!(tex->pix = stbi_load(fn, &tex->w, &tex->h, &n, 4)))
			return errmsg(
				"texture image '%s' failed: %s"
				, which, stbi_failure_reason()
			);
		
		/* udata */
		if (!(tex->udata = udata = calloc(1, sizeof(*udata))))
			return errmsg(ERR_NOMEM);
		udata->free = free;
		udata->virtDiv = 1; /* we divide by this, guarantee non-0 */
		udata->uvMult.w = 1; /* we multiply by this, 1 = default */
		udata->uvMult.h = 1;
		
		/* we just loaded pixel data for a texture strip */
		if (which == instead)
		{
			int main_w;
			int main_h;
			int n;
			/* so now retrieve dimensions from original texture */
			which = tex->filename;
			/* assertion: will full path to file fit? */
			if ((strlen(tex->objex->mtllibDir)
				+ 1 /* slash */
				+ strlen(which)
				+ 1 /* 0 term */) > sizeof(fn)
			)
				return errmsg("path to texture image '%s' too long", which);
			
			/* construct absolute path to file */
			if (pathIsAbsolute(which))
				strcpy(fn, which);
			else
			{
				strcpy(fn, tex->objex->mtllibDir);
				strcat(fn, "/");
				strcat(fn, which);
			}
			
			/* load texture file */
			if (!stbi_info(fn, &main_w, &main_h, &n))
				return errmsg(
					"texture image '%s' failed: %s"
					, which, stbi_failure_reason()
				);
			
			/* dimension assertions */
			if (tex->w != main_w || tex->h <= main_h || (tex->h % main_h))
				return errmsg(
					"texture '%s' strip '%s' not multiple of '%s'"
					, tex->name, instead, which
				);
			
			/* and calculate divisor */
			udata->virtDiv = tex->h / main_h;
		}
		
		/* one texture otherwise */
		else
		{
			/* detect mirror, crop texture */
			/* TODO wrapping detection (not important for now) */
			unsigned char *p = tex->pix;
			int w = tex->w;
			int h = tex->h;
			int i;
			
			/* only optimize textures at least this height */
			if (h >= 16)
			{
				/* test vertical mirror */
				for (i = 0; i < h / 2; ++i)
				{
					if (memcmp(p + i * w * 4 /* first row */
						, p + (h - (i + 1)) * w * 4 /* last row */
						, w * 4 /* row width (bytes) */
					))
						break;
				}
				/* image is mirrored vertically */
				if (i == h / 2)
				{
					/* halve texture vertically */
					h = tex->h = h / 2;
					udata->uvMult.h = 2;
					udata->isTmirror = 1;
				}
			}
			
			/* only optimize textures at least this width */
			if (w >= 16)
			{
				/* test horizontal mirror */
				for (i = 0; i < h; ++i)
				{
					uint32_t *p1 = (void*)(p + i * w * 4); /* left side */
					uint32_t *p2 = p1 + (w - 1); /* right side */
					int c;
					
					for (c = 0; c < w / 2; ++c)
					{
						if (*p1 != *p2)
							break;
						++p1;
						--p2;
					}
					if (c < w / 2)
						break;
				}
				/* image is mirrored horizontally */
				if (i == h)
				{
					/* halve texture horizontally */
					for (i = 1; i < h; ++i)
					{
						void *p1 = p + i * w * 4 / 2; /* left side cropped */
						void *p2 = p + i * w * 4;     /* left side full */
						memcpy(p1, p2, w * 4 / 2);    /* copy half row */
					}
					w = tex->w = w / 2;
					udata->uvMult.w = 2;
					udata->isSmirror = 1;
				}
			}
		}
		
		if ((tex->w & 7)
			|| (tex->h < 16 && (tex->h & 7)) /* allow heights like 42 */
		)
		{
			return errmsg(
				"texture image '%s' dimensions are not a multiple of 8"
				, fn
			);
		}
	}
	return success;
}

/* process textures sharing palette slots */
void *texture_procSharedPalettes(struct objex *obj)
{
	int prevHighest = 0;
	while (1)
	{
		struct objex_palette *palStruct;
		struct objex_texture *tex;
		int bpp = 0;
		int fmt = N64TEXCONV_CI;
		int pal_max;
		int pal_colors;
		int highest;
		int slot;
		void *pal;
		void *pbuf;
		struct n64texconv_palctx *pctx = 0;
		struct list *rme = 0;
		int total_invisible = 0;
		unsigned int pal_bytes;
		void *next;
		
		/* select the highest not-yet-processed slot */
		highest = 0;
		for (tex = obj->tex; tex; tex = tex->next)
		{
			/* skip previous highest */
			if (tex->paletteSlot > highest
				&& (prevHighest == 0 || tex->paletteSlot < prevHighest)
			)
			{
				highest = tex->paletteSlot;
				bpp = texture_textureBpp(tex);
			}
		}
		
		/* no more left to process */
		if (!highest)
			break;
		/* previous highest, for next iteration */
		prevHighest = highest;
		slot = highest;
		
//		debugf("shared slot %d\n", highest);
		
	/* initialize a palette context */
		/* derive max colors */
		if (bpp == N64TEXCONV_4)
			pal_max = 16;
		else
		{
			bpp = N64TEXCONV_8;
			pal_max = 256;
		}
		
		if (!(palStruct = pushpal(obj, calloc, pal_max)))
		{
			return errmsg(0);
		}
		pbuf = palStruct->colors;
		
		/* where colors will be stored */
		pal = pbuf;
		
		/* allocate palette context */
		pctx = n64texconv_palette_new(
			pal_max
			, pal
			, calloc
			, realloc
			, free
		);
		
		/* register textures into palette */
		for (tex = obj->tex; tex; tex = tex->next)
		{
			int num_invisible;
			void *png = tex->pix;
			int w = tex->w;
			int h = tex->h;
			enum n64texconv_acgen acfunc = parse_alphamode(tex->alphamode);
			if (acfunc >= N64TEXCONV_ACGEN_MAX)
				return errmsg("texture '%s' unknown alphamode '%s'"
					, tex->name, tex->alphamode
				);
			
			if (tex->paletteSlot != slot)
				continue;
			
			if (texture_textureBpp(tex) != bpp)
			{
				return errmsg("palette slot %d: mixed formats", slot);
//				debugf("palette slot %d: mixed formats", slot);
//				exit(EXIT_FAILURE);
			}
			
			/* we need this for later */
			tex->palette = palStruct;
			if (tex->isUsed)
				palStruct->isUsed = 1;
			
			/* link data into list */
			struct list *item = malloc(sizeof(*item));
			item->udata = tex;
			item->next = rme;
			item->data = png;
			item->fn   = 0;//name;
			item->w    = w;
			item->h    = h;
			rme = item;
			
			/* simplify colors first */
			/* rgba8888 -> rgba5551 -> rgba8888 */
			n64texconv_to_n64_and_back(
				png /* in-place conversion */
				, 0
				, 0
				, N64TEXCONV_RGBA
				, N64TEXCONV_16
				, w
				, h
			);
			
			/* generate alpha pixel colors */
			num_invisible =
			n64texconv_acgen(
				png
				, w
				, h
				, acfunc
				, acfunc_colors
				, calloc
				, realloc
				, free
				, fmt
			);
			
			/* white/black means only one invisible pixel    *
			 * color is shared across all converted textures */
			if (num_invisible &&
				(
					acfunc == N64TEXCONV_ACGEN_BLACK
					|| acfunc == N64TEXCONV_ACGEN_WHITE
				)
			)
				total_invisible = 1;
			/* otherwise, each has unique invisible pixel(s) */
			else
				total_invisible += num_invisible;
			
			if (num_invisible < 0 || total_invisible >= (pal_max/2))
			{
				return errmsg(
					"ran out of alpha colors "
					"(change alpha color mode or use fewer textures)"
				);
			}
			
			/* add image to queue */
			n64texconv_palette_queue(pctx, png, w, h, 0);
			
//			debugf("  %s (%s)\n", tex->name, tex->filename);
		}
		
		/* quantize images */
//		debugf("invisible = %d\n", total_invisible);
		n64texconv_palette_alpha(pctx, total_invisible);
		pal_colors = n64texconv_palette_exec(pctx);
					
		/* convert palette colors (rgba8888 -> rgba5551) */
		n64texconv_to_n64(
			pal /* in-place conversion */
			, pal
			, 0
			, 0
			, N64TEXCONV_RGBA
			, N64TEXCONV_16
			, pal_colors
			, 1
			, &pal_bytes
		);
		
		/* convert every texture */
		for (struct list *item = rme; item; item = item->next)
		{
			n64texconv_to_n64(
				item->data /* in-place conversion */
				, item->data
				, pal
				, pal_colors
				, fmt
				, bpp
				, item->w
				, item->h
				, &item->sz
			);
			struct objex_texture *tex = item->udata;
			struct texUdata *udata = tex->udata;
			if (!udata)
				return errmsg("texture '%s' not loaded", tex->name);
			udata->fileSz = item->sz;
			
			udata->gbiFmtStr = gbiFmtStr[fmt];
			udata->gbiBppStr = gbiBppStr[bpp];
			udata->gbiBpp = bpp;
		}
		
		/* watermark ci texture data*/
		/* NOTE: pal_colors is important here, not pal_max! */
		watermark(pal, pal_bytes, pal_colors, rme);
		
		/* free n64texconv palette context */
		n64texconv_palette_free(pctx);
		
		/* free all the rme items */
		for (struct list *item = rme; item; item = next)
		{
			next = item->next;
			free(item);
		}
		
		/* end of zztexview stuff */
		palStruct->colorsSize = pal_bytes;
		palStruct->colorsNum = pal_colors;
	}
	return success;
}

void *texture_procTexture(struct objex_texture *tex)
{
	const char *errstr;
	enum n64texconv_fmt fmt;
	enum n64texconv_bpp bpp;
	struct texUdata *udata = tex->udata;
	struct objex *obj = tex->objex;
	void *png;
	void *pal = 0;
	unsigned int sz;
	int n;
	int w = tex->w;
	int h = tex->h;
	int palColors = 0;
	int do_watermark = 0;
	
#undef fail
#define fail(...) {                    \
   errmsg(__VA_ARGS__);                \
   return 0;                           \
}
	
	if (!tex || (udata && udata->fileSz))
		return success;
	
	/* texture already loaded */
	png = tex->pix;
	
	if (!png || !udata)
		return errmsg("texture '%s' not loaded", tex->name);
	
	/* custom format string */
	if (tex->format)
	{
		if (!getFmtBpp(tex, &fmt, &bpp))
			return errmsg(0);
	}
	else if (tex->paletteSlot || tex->instead)
	{
		/* palette slots without a format
		 * specified default to ci8
		 */
		fmt = N64TEXCONV_CI;
		bpp = N64TEXCONV_8;
	}
	else
	{
		/* format must be specified if texture pointer override
		 * (except when texture bank (tex->instead) is specified) */
		if (tex->pointer && !tex->format)
			fail(
				"'%s': format must be specified for texture %08X"
				, tex->filename
				, tex->pointer
			);
		/* no format specified, so let's try for the best */
		if ((errstr = n64texconv_best_format(png, &fmt, &bpp, w, h)))
			fail("'%s' conversion error: %s", tex->filename, errstr);
		/* fall back to rgba16 if rgba32 */
		if (bpp == N64TEXCONV_32)
			bpp = N64TEXCONV_16;
		/* force ci8 on rgba16 */
		if (fmt == N64TEXCONV_RGBA && bpp == N64TEXCONV_16)
		{
			fmt = N64TEXCONV_CI;
			bpp = N64TEXCONV_8;
		}
	}
	/* yuv, sneaky zelda fans */
	if (fmt == N64TEXCONV_YUV)
		fail("yuv is not supported");
	if (tex->paletteSlot && fmt != N64TEXCONV_CI)
		fail("texture '%s' palette slot %d but format == '%s'"
			, tex->name, tex->paletteSlot, fmtStr[fmt]
		);
	
	/* ensure minimum size magic */
	{const char *x = n64texconv_min_size(&fmt, &bpp, w, h);
	if (x)
		fail("texture '%s' conversion error: %s", tex->name, x);
	}
	
	/* texture already has a palette and has already been converted */
	if (tex->palette)
	{
		pal = tex->palette->colors;
		palColors = tex->palette->colorsNum;
		if (!pal)
			fail("texture '%s' palette error", tex->name);
	}
	
	/* texture hasn't been processed in any prior steps */
	else
	{
		int num_invisible;
		unsigned int pal_bytes = 0;
		enum n64texconv_acgen acfunc;
		
		/* default alphamode will be `image` for `i` */
		if (fmt == N64TEXCONV_I && !tex->alphamode)
			acfunc = N64TEXCONV_ACGEN_USER;
		/* otherwise, use requested mode (`edge` if none specified) */
		else
			acfunc = parse_alphamode(tex->alphamode);
		
		if (acfunc >= N64TEXCONV_ACGEN_MAX)
			return errmsg("texture '%s' unknown alphamode '%s'"
				, tex->name, tex->alphamode
			);
		num_invisible =
		n64texconv_acgen(
			png
			, w
			, h
			, acfunc
			, acfunc_colors
			, calloc
			, realloc
			, free
			, fmt
		);
		
		if (fmt == N64TEXCONV_CI)
		{
			/* generate and watermark palette */
			struct objex_palette *palStruct;
			int pal_max = (bpp == N64TEXCONV_4) ? 16 : 256;
			int pal_num;
			
			if (!(palStruct = pushpal(obj, calloc, 256)))
				return errmsg(0);
			tex->palette = palStruct;
			
			/* generate palette from image */
			pal_num =
			n64texconv_palette_ify(
				png /* src */
				, palStruct->colors /* dest */
				, w
				, h
				, pal_max - num_invisible
				, 0
				, calloc
				, realloc
				, free
			);
			
			/* convert palette colors (rgba8888 -> rgba5551) */
			n64texconv_to_n64(
				palStruct->colors /* in-place conversion */
				, palStruct->colors
				, 0
				, 0
				, N64TEXCONV_RGBA
				, N64TEXCONV_16
				, pal_num
				, 1
				, &pal_bytes
			);
			
			do_watermark = 1;
			palStruct->colorsNum = pal_num;
			palStruct->colorsSize = pal_bytes;
			palColors = pal_num;
			pal = palStruct->colors;
			tex->palette = palStruct;
		}
		
		/* convert texture */
		if ((errstr = n64texconv_to_n64(
				png /* in-place conversion */
				, png
				, pal
				, palColors
				, fmt
				, bpp
				, w
				, h
				, &sz
			))
		)	fail("texture '%s' conversion error: %s", tex->name, errstr);
		
		/* complain when TMEM is exceeded */
		if (sz + pal_bytes > TMEM_MAX
			&& !tex->instead /* warn only if not image strip */
		)
			fail("texture '%s' TMEM overflow (0x%08X > 0x%08X)"
				" (try smaller texture or format)"
				, tex->name, sz + pal_bytes, TMEM_MAX
			);
		
		/* palette override: watermarking */
		if (do_watermark)
		{
			/* list containing exactly one item */
			struct list item = {0};
			struct objex_palette *p = tex->palette;
			item.next = 0;
			item.data = png;
			item.fn   = 0;
			item.w    = w;
			item.h    = h;
			item.sz   = sz;
			
			watermark(pal, p->colorsSize, p->colorsNum, &item);
		}
		
		udata->fileSz = sz;
		
		udata->gbiFmtStr = gbiFmtStr[fmt];
		udata->gbiBppStr = gbiBppStr[bpp];
		udata->gbiBpp = bpp;
	}
	
	if (tex->palette && tex->isUsed)
		tex->palette->isUsed = 1;
	
	return success;
#undef fail
}

void *texture_procTextures(struct objex *obj)
{
	struct objex_texture *tex;
	
	for (tex = obj->tex; tex; tex = tex->next)
	{
		if (!texture_procTexture(tex))
			return errmsg(0);
	}
	
	return success;
}

void *texture_writeTexture(VFILE *bin, struct objex_texture *tex)
{
	FILE *docs;
	
	if (!(docs = getDocs(tex->objex)))
		return 0;
	
	if (!tex)
		return errmsg("uninitialized texture");
	
	/* skip unused textures */
	if (!tex->isUsed)
		return success;
	
	/* mark palette as used */
	if (tex->palette)
		tex->palette->isUsed = 1;
	
	struct objexUdata *oudat = tex->objex->udata;
	struct texUdata *udata = tex->udata;
	unsigned char *png8 = tex->pix;
	
	if (!udata || !png8)
		return errmsg("uninitialized texture '%s'", tex->name);
	
	/* get texture offset */
	udata->fileOfs = vftell(bin);
//	fprintf(docs, "texture '%s' %08X (%08X)\n", tex->name, (int)udata->fileOfs, (int)udata->fileSz);
	// fprintf(docs, DOCS_DEF "TEX_" DOCS_SPACE "  0x%08X\n"
	// 	, Canitize(pathTail(tex->name), 1)
	// 	, (int)udata->fileOfs + getBase(tex->objex)
	// );
	document_doc(
		pathTail(tex->name),
		NULL,
		(int)udata->fileOfs + getBase(tex->objex),
		T_TEX
	);
	
	/* texture bank */
	if (tex->instead)
	{
		/* first texture bank encountered */
		if (!oudat->earlyPool.end)
		{
			oudat->earlyPool.start = udata->fileOfs;
			oudat->earlyPool.end = udata->fileOfs;
		}
		
		/* part of early texture bank collection in file */
		if (udata->fileOfs == oudat->earlyPool.end)
			oudat->earlyPool.end += udata->fileSz;
		
		/* write individual textures within texture bank */
		for (int i = 0; i < udata->virtDiv; ++i)
		{
			char buffer[128];

			sprintf(buffer, "%s_%d", pathTail(tex->name), i);
			// fprintf(docs, DOCS_DEF "TEX_" DOCS_SPACE "  0x%08X\n"
			// 	, CanitizeNum(pathTail(tex->name), 1, i)
			// 	, (int)(udata->fileOfs
			// 	+ getBase(tex->objex)
			// 	+ i * (udata->fileSz / udata->virtDiv))
			// );
			document_doc(
				buffer,
				NULL,
				(int)(udata->fileOfs
				+ getBase(tex->objex)
				+ i * (udata->fileSz / udata->virtDiv)),
				T_TEX
			);
		}
	}
	
	/* write texture into file */
	vfwrite(png8, 1, udata->fileSz, bin);
//	for (int i = 0; i < udata->fileSz; ++i)
//		vfput8(bin, png8[i]);
	
	return success;
}

void *texture_writeTextures(VFILE *bin, struct objex *obj)
{
	struct objex_texture *tex;
	
	vfalign(bin, 8);
	
	for (tex = obj->tex; tex; tex = tex->next)
	{
		if (!texture_writeTexture(bin, tex))
			return errmsg(0);
	}
	
	return success;
}

void *texture_writePalette(VFILE *bin, struct objex_palette *pal)
{
	FILE *docs;
	
	if (!(docs = getDocs(pal->objex)))
		return 0;
	
	unsigned char *color8;
	extern int printPalettes;
	if (!pal || !pal->colors)
		return errmsg("uninitialized palette");
	
	/* skip unused palettes */
	if (!pal->isUsed)
		return success;
	
	vfalign(bin, 8);
	
	/* write palette into file */
	pal->fileOfs = vftell(bin);
	color8 = pal->colors;
	//fprintf(docs, "palette %08X (sz %d)\n", (int)pal->fileOfs, pal->colorsSize);
	if (printPalettes)
		document_doc(
			int2str(pal->index),
			NULL,
			(int)pal->fileOfs + getBase(pal->objex),
			T_PAL
		);
		// fprintf(docs, DOCS_DEF "PAL_" DOCS_SPACE "  0x%08X\n"
		// 	, int2str(pal->index)
		// 	, (int)pal->fileOfs + getBase(pal->objex)
		// );
	vfwrite(color8, 1, pal->colorsSize, bin);
//	for (int i = 0; i < pal->colorsSize; ++i)
//		vfput8(bin, color8[i]);
	
	return success;
}

void *texture_writePalettes(VFILE *bin, struct objex *obj)
{
	struct objex_palette *pal;
	
	for (pal = obj->pal; pal; pal = pal->next)
	{
		if (!texture_writePalette(bin, pal))
			return errmsg(0);
	}
	
	return success;
}

static void *appendMirror(char **str)
{
	int append = 0;
	int len = strlen(*str);
	if (len)
		append = 1;
	if (!(*str =
		realloc(*str, len + 32)
	))
		return errmsg(ERR_NOMEM);
	if (append)
		strcat(*str, " | ");
	strcat(*str, "G_TX_MIRROR");
	
	return success;
}

/* NOTE this should be called after textures have been processed */
void *texture_procMaterials(struct objex *obj)
{
	struct objex_material *mat;
	
	for (mat = obj->mtl; mat; mat = mat->next)
	{
		struct objex_texture
			*tex0 = mat->tex0
			, *tex1 = mat->tex1
		;
		struct texUdata
			*udata0 = (tex0) ? tex0->udata : 0
			, *udata1 = (tex1) ? tex1->udata : 0
		;
		
		struct mtlUdata *udata;
		if (!(udata = mat->udata = calloc(1, sizeof(*udata))))
			return errmsg(ERR_NOMEM);
		udata->free = free;
		
		/* append mirroring (where applicable) */
		if (udata0)
		{
			if (udata0->isSmirror)
				if (!(appendMirror(&mat->gbivar[OBJEX_GBIVAR_CMS0])))
					return 0;
			
			if (udata0->isTmirror)
				if (!(appendMirror(&mat->gbivar[OBJEX_GBIVAR_CMT0])))
					return 0;
		}
		if (udata1)
		{
			if (udata1->isSmirror)
				if (!(appendMirror(&mat->gbivar[OBJEX_GBIVAR_CMS1])))
					return 0;
			
			if (udata1->isTmirror)
				if (!(appendMirror(&mat->gbivar[OBJEX_GBIVAR_CMT1])))
					return 0;
		}
		
		/* multi-textured material */
		if (tex0 && tex1)
		{
			unsigned int sz0 = udata0->fileSz / udata0->virtDiv;
			unsigned int sz1 = udata1->fileSz / udata1->virtDiv;
			unsigned int szP = 0;
			
			/* TODO if you really wanted to, two ci4 palettes could
			 *      work b/c ci4 palettes don't take up full palette
			 *      block in TMEM; in theory, ci8 could too if the
			 *      sizes add up to 0x200 or less... unless the hardware
			 *      and/or common graphics plugins don't work this way
			 */
			if (tex0->palette
				&& tex1->palette
				&& tex0->palette != tex1->palette
			)
				return errmsg(
				"material '%s' textures '%s', '%s' use different palettes"
				" (make them share a palette, or change formats)"
				, mat->name, tex0->name, tex1->name);
			
			if (tex0->palette)
				szP = tex0->palette->colorsSize;
			if (tex1->palette)
				szP = tex1->palette->colorsSize;
			
			if (sz0 + sz1 + szP > TMEM_MAX)
				return errmsg(
				"material '%s' textures '%s', '%s' can't all fit in TMEM"
				" (resize and/or change formats/depths)"
				, mat->name, tex0->name, tex1->name);
		}
	}
	return success;
}

const char *texture_errmsg(void)
{
	return error_reason;
}

