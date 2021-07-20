/* <z64.me> zobj.c: utilities for writing zobj-specific data */

#ifndef ZOBJ_H_INCLUDED
#define ZOBJ_H_INCLUDED
#include <stdio.h>
#include <stdint.h>
#include "objex.h"
#include "vfile.h"

struct objexUdata
{
	objex_udata_free  free;
	unsigned int baseOfs;
	FILE *docs;
	struct reloc *identityMatrixList;
	struct {
		unsigned start;
		unsigned end;
	} localMatrixBlock;
	
	/* earliest known pool inside the zobj (texture blocks) */
	struct {
		unsigned start;
		unsigned end;
	} earlyPool;
};

struct boneUdata
{
	objex_udata_free  free;
	int index;        /* index within written skeleton */
	int matrixIndex;  /* index used for matrices */
//	uint32_t dlist0;  /* high poly */
//	uint32_t dlist1;  /* low poly */
};

struct reloc
{
	struct reloc *next;
	size_t offset;
};

struct groupUdata
{
	objex_udata_free  free;
	struct reloc *reloc;
	uint32_t dlistOffset; /* offset of display list in file */
	int hasWritten; /* has been written to file */
	int isPbody;
};

struct skUdata
{
	objex_udata_free  free;
//	uint32_t fileOffset; /* offset in file where skeleton starts */
	uint32_t fileHeader; /* offset of header in file */
};

/* proxy array; follows format `06020A78 06021F78 etc` */
struct zobjProxyArray
{
	char *name;
	uint32_t *array; /* contains num items */
	int num; /* num proxies */
};
void zobjProxyArray_print(
	FILE *docs
	, VFILE *bin
	, struct zobjProxyArray *proxyArray
	, uint32_t baseOfs
);
void zobjProxyArray_free(struct zobjProxyArray *p);
void zobjProxyArray_unnest(
	VFILE *bin
	, struct zobjProxyArray *proxyArray
	, uint32_t baseOfs
);

extern const char *zobj_errmsg(void);
extern 
void *zobj_writeDlist(
	VFILE *bin
	, struct objex_g *g
	, void *calloc(size_t, size_t)
	, void free(void *)
	, const int forcePass
);
extern void *zobj_writeSkeleton(
	VFILE *bin
	, struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
	, unsigned int *headerOffset
	, struct zobjProxyArray **proxyArray
);
extern void *zobj_writePbody(
	VFILE *bin
	, struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
);
extern const char *zobj_errmsg(void);
extern void zobj_indexSkeleton(
	struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
);
extern void *zobj_writeTexture(
	VFILE *bin
	, struct objex_texture *tex
	, void *calloc(size_t, size_t)
	, void *realloc(void *, size_t)
	, void free(void *)
);
extern void *zobj_initObjex(struct objex *obj, const unsigned int baseOfs, FILE *docs);
extern void *texture_procMaterials(struct objex *obj);
extern void *zobj_writeStdAnims(VFILE *bin, struct objex *objex);
extern void *zobj_doRelocs(VFILE *bin, struct objex *objex);
extern int zobj_g_written(struct objex_g *g);
extern void *zobj_doPlayAsData(VFILE *bin, struct objex *objex);
extern void *zobj_doIdentityMatrices(VFILE *bin, struct objex *objex);
extern void *zobj_writeUsemtl(VFILE *bin, struct objex_material *mtl);
#endif /* ZOBJ_H_INCLUDED */

