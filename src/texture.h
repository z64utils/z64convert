#ifndef TEXTURE_H_INCLUDED
#define TEXTURE_H_INCLUDED
#include "objex.h"
#include "vfile.h"

struct mtlUdata
{
	objex_udata_free  free;
	int wrotePalette;
	int wroteTexture;
};

struct texUdata
{
	objex_udata_free  free;
	const char *gbiFmtStr;
	const char *gbiBppStr;
	int    gbiBpp;
	size_t fileOfs;
	size_t fileSz;
	size_t virtDiv; /* virtual size divisor; divide fileSz by this
	                 * to get the size of a single texture in the
	                 * strip (e.g. eyesBank.fileSz / virtSz will
	                 * give the fileSz of one texture in the strip */
	int writeTexturesVar;  /* variable used inside writeTextures func */
	struct
	{
		float w;  /* these get multiplied into UV coordinates  */
		float h;  /* (a last-minute addition for u/v mirroring */
	} uvMult;
	int isSmirror;
	int isTmirror;
};

extern void *texture_loadAll(struct objex *obj);
extern void *texture_procSharedPalettes(struct objex *obj);
extern void *texture_procTextures(struct objex *obj);
extern const char *texture_errmsg(void);
extern void *texture_writeTextures(VFILE *bin, struct objex *obj);
extern void *texture_writePalettes(VFILE *bin, struct objex *obj);
extern void *texture_writeTexture(VFILE *bin, struct objex_texture *tex);
#endif /* TEXTURE_H_INCLUDED */

