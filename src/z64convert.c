#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h> /* PATH_MAX */
#include <stdint.h>
#include <math.h>
#include "objex.h"
#include "zobj.h"
#include "texture.h"
#include "err.h"
#include "put.h"
#include "collider.h"
#include "collision.h"
#include "doc.h"

#define DSTDERR docs

/* revisions:
 * v1.0.2
 * better circular dependency detection (bug report by Zeldaboy14)
 * QOL improvement on proxy skeleton array output: report empty items
 * v1.0.1
 * dynamic vertexshading mode implementation is now complete (rankaisija)
 * v1.0.0 rev 0
 * added --silent argument
 * added --print-palettes argument for printing palettes
 */

#include "vfile.h"
#include "binary-header.h"

static const char *sgRval = 0;

struct model
{
	const char *inFn;
	const char *outFn;
	const char *namHeader;
	const char *namLinker;
	const char *only;
	const char *except;
	struct objex *obj;
	FILE *docs;
	float scale;
	unsigned baseOfs;
	int playAs;
	void (*die)(const char *fmt, ...);
};

#include <wow.h>
int printPalettes = 0;
enum binaryHeaderFlags binaryHeader = 0;

/* DONE --only "bunnyhood,riggedmesh,etc" argument for saying to only
 *      convert the groups named (this can be a single gui textbox */

static char errstr[1024] = {0};
#define fail(...) { \
		sgRval = errstr; \
		if (!errstr[0]) \
		snprintf(errstr, sizeof(errstr), __VA_ARGS__); \
		goto L_cleanup; \
}
#define fail0(...) { \
		if (!errstr[0]) \
		snprintf(errstr, sizeof(errstr), __VA_ARGS__); \
		return 0; \
}

char *xstrndup(const char *s, int num)
{
	char *out;
	
	if (!s || num <= 0)
		return 0;
	
	if (!(out = malloc(num+1)))
		return 0;
	memcpy(out, s, num);
	out[num] = '\0';
	return out;
}

char *DUMMYgetcwd(void)
{
	char cwd[PATH_MAX];
	if (wow_getcwd(cwd, sizeof(cwd)))
		return strdup(cwd);
	return 0;
//	return strdup(".");
}

static int min_int(int a, int b)
{
	return a < b ? a : b;
}

/* returns true if two strings are equal, or if the beginning
 * of the longer one matches the entire length of the shorter
 */
static int streq(const char *a, const char *b)
{
	int i = min_int(strlen(a), strlen(b));
	return !memcmp(a, b, i);
}

static int isMesh(struct objex_g *g)
{
	if (!g
	   || streq(g->name, "collision.")
	   || streq(g->name, "collider.")
	)
		return 0;
	
	return 1;
}

static void *commaListGroupAssert(
	struct objex_g *list
	, const char *hay
	, const char *id
)
{
	const char *haystack = hay;
	while (1)
	{
		const char *end = strchr(haystack, ',');
		if (!end)
			end = haystack + strlen(haystack);
		
		struct objex_g *g;
		for (g = list; g; g = g->next)
		{
			/* only compare if string lengths equal */
			if ((end - haystack) == strlen(g->name)
			   && !memcmp(haystack, g->name, end - haystack)
			)
				break;
		}
		if (!g)
		{
			sprintf(errstr
				, "group '%.*s' specified in list '%s', but doesn't exist"
				, (int)(end - haystack), haystack, id
			);
			return 0;
		}
		
		/* advance to next */
		haystack = end;
		if (!*haystack) /* end of string */
			break;
		++haystack;     /* skip comma */
	}
	
	return list;
}

static int commaListContains(const char *haystack, const char *needle)
{
	if (!haystack || !needle)
		return 0;
	
	while (1)
	{
		const char *end = strchr(haystack, ',');
		if (!end)
			end = haystack + strlen(haystack);
		
		/* only compare if string lengths equal */
		if ((end - haystack) == strlen(needle)
		   && !memcmp(haystack, needle, end - haystack)
		)
			return 1;
		
		/* advance to next */
		haystack = end;
		if (!*haystack) /* end of string */
			break;
		++haystack;     /* skip comma */
	}
	
	return 0;
}

static int isExcluded(struct objex_g *g, const char *only, const char *except)
{
	if (!g || (!only && !except))
		return 0;
	
	const char *name = g->name;
	
	/* exclude if not in "only" */
	if (only && !commaListContains(only, name))
		return 1;
	
	/* exclude if in "except" */
	if (except && commaListContains(except, name))
		return 1;
	
	return 0;
}

static void *retriveColliderMtlAttribs(
	struct boundingBox *bounds
	, struct colliderJoint *me
)
{
	if (!bounds->mtl->attrib)
		return success;
	
	const char *att = bounds->mtl->attrib;
	const char *ss;
	
	if ((ss = strstr(att, "collider.bodyFlags"))
	   && sscanf(ss, "bodyFlags %X", &me->bodyFlags) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.bodyFlags");
	
	if ((ss = strstr(att, "collider.touchFlags"))
	   && sscanf(ss, "touchFlags %X", &me->touchFlags) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.touchFlags");
	
	if ((ss = strstr(att, "collider.touchFx"))
	   && sscanf(ss, "touchFx %X", &me->touchFx) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.touchFx");
	
	if ((ss = strstr(att, "collider.bumpFlag"))
	   && sscanf(ss, "bumpFlag %X", &me->bumpFlag) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.bumpFlag");
	
	if ((ss = strstr(att, "collider.bumpFx"))
	   && sscanf(ss, "bumpFx %X", &me->bumpFx) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.bumpFx");
	
	if ((ss = strstr(att, "collider.tchBmpBdy"))
	   && sscanf(ss, "tchBmpBdy %X", &me->tchBmpBdy) != 1
	)
		fail0("error: attrib '%s' not hex", "collider.tchBmpBdy");
	
	return success;
}

static void model_free(struct model *model)
{
	if (!model)
		fail("model_free error: model not loaded?");
	
	if (model->obj)
		objex_free(model->obj, free);
	
	free(model);
	
L_cleanup:
	return;
}

static const char *model_commit(struct model *model)
{
	VFILE *zobj = 0;
	
	if (!model)
		fail("model_commit error: model not loaded?");
	
	if (model->obj->fileNum > 1)
	{
		const char *outFn = model->outFn;
		struct objex *tmp = model->obj;
		
		for (int i = 0; i < tmp->fileNum; ++i)
		{
			struct objex_file *file = tmp->file + i;
			struct objex *obj = file->objex;
			char buf[1024];
			int doBreak = 0;
			
			obj->v = tmp->v;
			obj->vt = tmp->vt;
			obj->vn = tmp->vn;
			obj->vc = tmp->vc;
			obj->vNum = tmp->vNum;
			obj->vtNum = tmp->vtNum;
			obj->vnNum = tmp->vnNum;
			obj->vcNum = tmp->vcNum;
			
			model->obj = obj;
			snprintf(buf, sizeof(buf), "%s_%s", outFn, file->name);
			model->outFn = buf;
			model->baseOfs = file->baseOfs;
			if (model_commit(model))
				doBreak = 1;
			
			obj->v = 0;
			obj->vt = 0;
			obj->vn = 0;
			obj->vc = 0;
			obj->vNum = 0;
			obj->vtNum = 0;
			obj->vnNum = 0;
			obj->vcNum = 0;
			
			if (doBreak)
				break;
		}
		
		model->obj = tmp;
		model->outFn = outFn;
		return sgRval;
	}
	
	const char *outMode = "wb+";
	const char *in = model->inFn;
	const char *out = model->outFn;
	const char *namHeader = model->namHeader;
	const char *namLinker = model->namLinker;
	const char *only = model->only;
	const char *except = model->except;
	unsigned baseOfs = model->baseOfs;
	struct objex *obj = model->obj;
	int playAs = model->playAs;
	FILE *docs = model->docs;
	void (*die)(const char *fmt, ...) = model->die;
	FILE *header = 0;
	FILE *linker = 0;
	
//	zobj_FILE = wow_fopen(out, outMode);
//	if (!zobj_FILE)
//		fail("failed to open out file for writing");
//	if (!(zobj = vfopen_FILE(zobj_FILE, 512*1024/*512kb*/, wow_fwrite, die)))
	if (!(zobj = vfopen_delayed(out, outMode, 512*1024 /*512kb*/, wow_fwrite, die, wow_fopen)))
		fail(ERR_NOMEM);
	
	/* set aside space for binary header, which will be written later */
	if (binaryHeader && !(binaryHeader & BINHEAD_FOOTER))
		for (int i = 0; i < BINARYHEADER_SIZE; ++i)
			vfputc(0, zobj);
	
	/* write textures and palettes to out file */
	if (!texture_writeTextures(zobj, obj)
	   || !texture_writePalettes(zobj, obj)
	)
		fail(texture_errmsg());
	
	/* write any standalone materials */
	for (struct objex_material *mtl = obj->mtl; mtl; mtl = mtl->next)
	{
		/* skip any that aren't standalone or aren't used */
		if (!mtl->isStandalone || !mtl->isUsed)
			continue;
		
		if (!zobj_writeUsemtl(zobj, mtl))
			fail(zobj_errmsg());
	}
	
	/* write collision triangle meshes */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		unsigned headOfs;
		
		/* skip those that should be skipped */
		if (isExcluded(g, only, except))
			continue;
		
		/* skip those with names not starting with 'collision.' */
		if (!streq(g->name, "collision."))
			continue;
		
		g->hasSplit = 1; /* don't write display list */
		/* attempt to write */
		if (!collision_write(zobj, g, baseOfs, &headOfs))
			fail(collision_errmsg());
		
		document_assign(
			g->name + strlen("collision.")
			, NULL
			, baseOfs + headOfs
			, T_COLL
		);
		// fprintf(docs, DOCS_DEF "COLL_" DOCS_SPACE " 0x%08X\n"
		// 	, Canitize(g->name + strlen("collision."), 1)
		// 	, baseOfs + headOfs
		// );
//		fprintf(docs, "'%s' : 0x%08X\n", g->name, baseOfs + headOfs);
	}
	
//	fprintf(DSTDERR, "offset %08lX\n", vftell(zobj));
	struct zobjProxyArray *proxyArrayList[256] = {0};
	int proxyArrayNum = 0;
	/* first pass: write skeletal (non-Pbody) meshes */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		/* skip non-mesh groups */
		if (!isMesh(g))
			continue;
		
		/* vertices within are divided between bones,
		 * the referenced skeleton is not a Pbody, and
		 * the mesh does not have a 'NOSPLIT' attrib
		 */
		if (g->hasWeight/*g->hasDeforms*/
		   && !g->skeleton->parent
		   && (!g->attrib || !strstr(g->attrib, "NOSPLIT"))
		)
		{
			struct objex_skeleton *sk;
			struct zobjProxyArray *proxyArray = 0;
			
			/* successfully divided */
			if ((sk = objex_group_split(obj, g, calloc)))
			{
				unsigned int headerOffset = -1;
				if (!zobj_writeSkeleton(
					/* we don't skip actual writing if excluded; we
					 * still run this function b/c the udata it
					 * generates is useful */
					isExcluded(g, only, except) ? 0 : zobj
					, sk
					, calloc
					, free
					, &headerOffset
					, &proxyArray
					))
					fail(zobj_errmsg());
				
				if (proxyArray)
					proxyArrayList[proxyArrayNum++] = proxyArray;
			}
		}
	}
	
//	fprintf(DSTDERR, "offset %08lX\n", vftell(zobj));
	/* second pass: write all other meshes */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		//fprintf(stderr, "ok '%s'\n", g->name);
		/* skip non-mesh groups and exceptions */
		if (!isMesh(g) || isExcluded(g, only, except))
			continue;
		
		/* skip written groups and split groups */
//		debugf("encountered '%s'\n", g->name);
		if (zobj_g_written(g) || g->hasSplit)
			continue;
		
		/* skip groups that don't own their data */
		if (g->fOwns == 0)
			continue;
		
		/* unwritten groups, Pbodies, whole undivided meshes */
		debugf("%s no udata\n", g->name);
		if (g->hasWeight /*g->hasDeforms*/ && g->fOwns)
		{
			debugf("%s hasDeforms\n", g->name);
			/*if (!objex_group_matrixBones(obj, g))
			        debugf("%s\n", objex_errmsg());
			   else
			   {
			        zobj_indexSkeleton(g->skeleton, calloc, free);
			   }*/
			struct objex_skeleton *sk;
			
			/* successfully divided */
			if ((sk = objex_group_split(obj, g, calloc)))
			{
				if (!zobj_writePbody(
					zobj
					, sk
					, calloc
					, free
					))
					fail(zobj_errmsg());
				
#if 0 /* TODO pLimb causes bloat with things like Link's fist etc */
				/* write rankaisija's pLimb stuff */
				static int wow = 0;
				if (!wow)
				{
					fprintf(docs, "\n" "typedef struct skLimb {\n"
						"   struct {\n"
						"      float x, y, z;\n"
						"   } loc;\n"
						"   unsigned short child;\n"
						"   unsigned short sibling;\n"
						"} skLimb;\n");
					wow = 1;
				}
				fprintf(docs
					, "\n" "struct skLimb %s_limb[] = {"
					, g->name
				);
				/* TODO move this elsewhere */
				void ugly_quickfix(struct objex_bone *bone)
				{
					/* .loc, .child, .sibling */
					fprintf(docs
						, "\n   { { %f, %f, %f }, %d, %d}, /* %02d: %s */"
						, bone->x
						, bone->y
						, bone->z
						, bone->child ? bone->child->index : -1
						, bone->next  ? bone->next->index  : -1
						, bone->index
						, bone->name
					);
					if (bone->child)
						ugly_quickfix(bone->child);
					if (bone->next)
						ugly_quickfix(bone->next);
				}
				/* root bone; recursion does the rest */
				ugly_quickfix(sk->bone);
				fprintf(docs, "\n};\n\n");
#endif
			}
		}
		else if (!zobj_writeDlist(zobj, g, calloc, free, 0))
		{
			fail(zobj_errmsg());
		}
	}
	
//	fprintf(DSTDERR, "offset %08lX\n", vftell(zobj));
	/* write animations in standard format */
	zobj_writeStdAnims(zobj, obj);
	
	/* handle relocations */
	if (!zobj_doRelocs(zobj, obj))
		fail(zobj_errmsg());
	
	/* now that we've reloc'd, print proxyArrayLists */
	for (int i = 0; i < proxyArrayNum; ++i)
	{
		struct zobjProxyArray *p = proxyArrayList[i];
		/* unnesting no longer necessary? leave it this way for safety */
		zobjProxyArray_unnest(zobj, p, baseOfs);
		
		if (!zobjProxyArray_print(docs, zobj, p, baseOfs))
			fail(zobj_errmsg());
		zobjProxyArray_free(p);
	}
	
//	/* identity matrices */
//	zobj_doIdentityMatrices(zobj, obj);
	
	/* play-as data */
	if (playAs && !zobj_doPlayAsData(zobj, obj))
		fail(zobj_errmsg());
	
	/* binary header */
	if (binaryHeader)
		binaryHeaderWrite(zobj, obj, baseOfs, binaryHeader);
	
	/* make sure zobj is 16-byte aligned */
	vfalign(zobj, 16);
	
	/* header/linker output to stdout */
	if (namHeader && !strcmp(namHeader, "-"))
		header = stdout;
	if (namLinker && !strcmp(namLinker, "-"))
		linker = stdout;
	
	if (namHeader && !namLinker)
	{
		if (!header)
			header = fopen(namHeader, "w");
		document_mergeDefineHeader(header);
	}
	else if (namHeader && namLinker)
	{
		if (!header)
			header = fopen(namHeader, "w");
		if (!linker)
			linker = fopen(namLinker, "w");
		
		/* write header and linker in separate steps in case either is stdout */
		document_mergeExternHeader(header, 0, NULL);
		document_mergeExternHeader(0, linker, NULL);
	}
	document_free();
	if (header && header != stdout && header != stderr)
		fclose(header);
	if (linker && linker != stdout && linker != stderr)
		fclose(linker);
	
L_cleanup:
	/* cleanup */
	if (zobj && vfclose(zobj))
		fail("failed to write zobj '%s'", out);
	return sgRval;
}

static struct model *model_load(
	const char *in
	, const char *out
	, const char *namHeader
	, const char *namLinker
	, const char *only
	, const char *except
	, unsigned baseOfs
	, float scale
	, FILE *docs
	, int playAs
	, void (die)(const char *fmt, ...)
)
{
	struct model *m = calloc(1, sizeof(*m));
	struct objex *obj;
	
	m->inFn = in;
	m->outFn = out;
	m->namHeader = namHeader;
	m->namLinker = namLinker;
	m->only = only;
	m->except = except;
	m->baseOfs = baseOfs;
	m->playAs = playAs;
	m->docs = docs;
	m->scale = scale;
	m->die = die;
	
	obj = objex_load(
		wow_fopen, wow_fread, calloc, malloc, free
		, wow_chdir
		, wow_chdir_file
		, DUMMYgetcwd
		, in
		, 0x0D000000 /* default skeleton segment */
		, scale
		, OBJEXFLAG_NO_MULTIASSIGN
	);
	if (!obj)
		fail(objex_errmsg());
	
	/* fail if 'only' or 'except' contains unknown group name */
	if (only && !commaListGroupAssert(obj->g, only, "only"))
		fail("only");
	if (except && !commaListGroupAssert(obj->g, except, "except"))
		fail("except");
	
	/* for n64 models, ensure it fits in a signed short */
	if (!objex_assert_vertex_boundaries(obj, SHRT_MIN, SHRT_MAX))
		fail(objex_errmsg());
	
	/* allocates and initializes objexUdata */
	zobj_initObjex(obj, baseOfs, docs);
	
	#if 0 // Do not see this as necessary, Objex2 gives errors for this already
	if (obj->softinfo.animation_framerate
	   && !streq(obj->softinfo.animation_framerate, "20")
	   && obj->anim
	)
	{
		document_assign(
			NULL
			, "/* WARNING:\n"
			" * your 3D software is set to %sfps, so the converted\n"
			" * animations may run faster or slower than expected\n"
			" */\n"
			, strtol(obj->softinfo.animation_framerate, NULL, 10)
			, DOC_INT
		);
		// fprintf(docs,
		// 	"/* WARNING:\n"
		// 	" * your 3D software is set to %sfps, so the converted\n"
		// 	" * animations may run faster or slower than expected\n"
		// 	" */\n"
		// 	, obj->softinfo.animation_framerate
		// );
	}
	#endif
	
	
	/* load and process textures and palettes */
	if (
//		!fprintf(stderr, "loading textures...\n") ||
		!texture_loadAll(obj)
//		|| !fprintf(stderr, "processing shared palettes...\n")
	   || !texture_procSharedPalettes(obj)
//		|| !fprintf(stderr, "processing textures...\n")
	   || !texture_procTextures(obj)
	)
		fail(texture_errmsg());
	
	/* fallback gbi macros for map_Kd textures */
	for (struct objex_material *mtl = obj->mtl; mtl; mtl = mtl->next)
	{
		/* skip those that already have gbi */
		if (mtl->gbi)
			continue;
		mtl->gbi = strdup(
/*"gsSPTexture(qu016(0.999985), qu016(0.999985), 0, G_TX_RENDERTILE, G_ON),\n\
   gsDPPipeSync(),\n\
   gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA), G_RM_AA_ZB_OPA_SURF2),\n\
   gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED),\n\
   gsDPSetPrimColor(0, qu08(0.5), 0xFF, 0xFF, 0xFF, 0xFF),\n\
   gsSPSetGeometryMode(G_LIGHTING),\n\
   gsSPClearGeometryMode(G_CULL_BACK),\n\
   gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),\n\
   _loadtexels,\n\
   gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, qu102(_texel0width-1), qu102(_texel0height-1))\n"*/
			"gsSPTexture(qu016(0.999985), qu016(0.999985), 0, G_TX_RENDERTILE, G_ON),\n\
gsDPPipeSync(),\n\
gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA), G_RM_AA_ZB_OPA_SURF2),\n\
gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED),\n\
gsDPSetPrimColor(0, qu08(0.5), 0xFF, 0xFF, 0xFF, 0xFF),\n\
gsSPGeometryMode(G_CULL_FRONT | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_FOG, G_SHADE | G_CULL_BACK | G_ZBUFFER | G_LIGHTING),\n\
_loadtexels,\n\
gsDPSetTileSize(G_TX_RENDERTILE, 0, 0, qu102(_texel0width-1), qu102(_texel0height-1))\n"
/*gbivar   cms0     \"G_TX_NOMIRROR | G_TX_CLAMP\"\
   gbivar   cmt0     \"G_TX_NOMIRROR | G_TX_CLAMP\"\
   gbivar   shifts0  0\
   gbivar   shiftt0  0\*/
		);
		if (!mtl->gbi)
			fail(ERR_NOMEM);
		mtl->gbiLen = strlen(mtl->gbi) + 2048;
		mtl->gbi = realloc(mtl->gbi, mtl->gbiLen + 8);
		if (!mtl->gbi)
			fail(ERR_NOMEM);
		memset(mtl->gbi + strlen(mtl->gbi)
			, 0
			, mtl->gbiLen - strlen(mtl->gbi)
		);
		if (!mtl->tex0)
		{
			strcpy(mtl->gbi, "gsSPTexture(qu016(0.999985), qu016(0.999985), 0, G_TX_RENDERTILE, G_ON),\n\
gsDPPipeSync(),\n\
gsDPSetRenderMode(AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_FOG, G_BL_A_SHADE, G_BL_CLR_IN, G_BL_1MA), G_RM_AA_ZB_OPA_SURF2),\n\
gsDPSetCombineLERP(TEXEL0, 0, SHADE, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, COMBINED, 0, 0, 0, 0, COMBINED),\n\
gsDPSetPrimColor(0, qu08(0.5), 0xFF, 0xFF, 0xFF, 0xFF),\n\
gsSPSetGeometryMode(G_LIGHTING),\n\
gsSPClearGeometryMode(G_CULL_BACK),\n\
gsSPClearGeometryMode(G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR),\n\
"
			);
		}
	}
	
	/* process materials */
	if (!texture_procMaterials(obj))
		fail(texture_errmsg());
	
	/* collider C output */
	/* TODO @rankaisija please adapt this to use doc.c or doc.h in some way */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		/* skip those that should be skipped */
		if (isExcluded(g, only, except)
		   || !streq(g->name, "collider.")
		)
			continue;
		
		/* collider group attributes */
		unsigned int type, atFlags, acFlags, maskA, maskB, shape;
		type = 0;
		atFlags = 0;
		acFlags = 0;
		maskA = 0;
		maskB = 0;
		shape = 0;
		if (g->attrib)
		{
			const char *ss;
			if ((ss = strstr(g->attrib, "collider.type"))
			   && sscanf(ss, "type %X", &type) != 1
			)
				fail("error: attrib '%s' not hex", "collider.type");
			
			if ((ss = strstr(g->attrib, "collider.atFlags"))
			   && sscanf(ss, "atFlags %X", &atFlags) != 1
			)
				fail("error: attrib '%s' not hex", "collider.atFlags");
			
			if ((ss = strstr(g->attrib, "collider.acFlags"))
			   && sscanf(ss, "acFlags %X", &acFlags) != 1
			)
				fail("error: attrib '%s' not hex", "collider.acFlags");
			
			if ((ss = strstr(g->attrib, "collider.maskA"))
			   && sscanf(ss, "maskA %X", &maskA) != 1
			)
				fail("error: attrib '%s' not hex", "collider.maskA");
			
			if ((ss = strstr(g->attrib, "collider.maskB"))
			   && sscanf(ss, "maskB %X", &maskB) != 1
			)
				fail("error: attrib '%s' not hex", "collider.maskB");
			
			/* these are all only 8-bit values */
			type &= 255;
			atFlags &= 255;
			acFlags &= 255;
			maskA &= 255;
			maskB &= 255;
		}
		
		/* collider name */
		const char *Ctype;
		const char *name = g->name;
		name += strlen("collider.");
		Ctype = name;
		if (strcspn(name, ".") + 1 < strlen(name))
			name += strcspn(name, ".") + 1;
		
#define INDENT "\t"
		if (streq(Ctype, "joint"))
		{
			g->hasSplit = 1; /* don't write display list */
			struct boundingBox *bounds;
			struct colliderJoint list[256] = {0}, *me;
			int idx;
			int joint = 1;
			if (!collider_init(g))
				fail(collider_errmsg());
//			debugf("%s stats:\n", g->name);
			for (idx = 0, joint = 1; joint < 256; ++idx, ++joint)
			{
				bounds = collider_bounds(g);
				if (!bounds)
					break;
				me = list + idx;
				me->center_x = round((bounds->min_x + bounds->max_x)*0.5f);
				me->center_y = round((bounds->min_y + bounds->max_y)*0.5f);
				me->center_z = round((bounds->min_z + bounds->max_z)*0.5f);
				me->radius =
					round(
						(fabs(bounds->min_x - bounds->max_x)
							+ fabs(bounds->min_y - bounds->max_y)
							+ fabs(bounds->min_z - bounds->max_z)) /*sum diameter*/
						* 0.3333333f /* average diameter */
						* 0.5f /* radius = half diameter */
					);
				me->scale = 1000 / scale;
				me->joint = joint;
				me->boneIndex = bounds->boneIndex;
				
				if (!bounds->mtl)
					fail("'%s' no material", g->name);
				
				/* get collider material attribs */
				if (!retriveColliderMtlAttribs(bounds, me))
					return 0;
				//debugf("derived bounding box, bone %d\n", bounds->boneIndex);
			}
			if (joint >= 256)
			{
				fail("error: '%s' too many joints", g->name);
			}
			/* print count */
			document_assign(
				name
				, NULL
				, idx
				, T_JOINTCOUNT
			);
			// fprintf(docs, "#define  JOINTCOUNT_%s  %d\n"
			// 	, Canitize(name, 1), idx
			// );
			/* print init */
			{
				shape = 0x00; /* must be 0, means sphere */
				fprintf(docs
					, "static const uint32_t jointlist_%s[] = {\n\t"
					"/*TpAtAcMa    MbSp        count    */\n\t"
				        /*  TpAtAcMa    MbSp        count    */
				        /*0x06000919, 0x10000000, 0x00000006,*/
					"0x%02X%02X%02X%02X, 0x%02X%02X%02X%02X, 0x%08X, "
					, Canitize(name, 0)
					, type, atFlags, acFlags, maskA
					, maskB, shape, 0, 0
					, idx
				);
			}
			/* print list */
			fprintf(docs, "\n\t" "(uintptr_t)(uint32_t[%d]){\n", idx * 9);
			fprintf(docs, "\t\t" "/*bodyFlags touchFlags  touchFx     bumpFlags   bumpFx      tchBmpBdy     jj00xxxx    yyyyzzzz    rrrrssss*/\n");
			for (int i = 0; i < idx; ++i)
			{
				fprintf(docs, "\t\t");
				me = list + i;
				/*fprintf(stderr, "[%d] = {\n\t.dim = {\n\t\t.joint = %d\n\t\t"
				        ", .center = { %d, %d, %d }\n\t\t"
				        ", .radius = %d\n\t\t"
				        ", .scale = %d\n\t}\n}\n"
				        , me->joint - 1
				        , me->joint
				        , me->center_x, me->center_y, me->center_z
				        , me->radius
				        , me->scale
				   );*/
				int16_t j, x, y, z, rad, size;
				
				j = me->joint;
				x = me->center_x;
				y = me->center_y;
				z = me->center_z;
				rad = me->radius;
				size = me->scale;
				
				/* 00 */ fprintf(docs, "0x%08X, ", me->bodyFlags);
				/* 04 */ fprintf(docs, "0x%08X, ", me->touchFlags);
				/* 08 */ fprintf(docs, "0x%08X, ", me->touchFx);
				/* 0C */ fprintf(docs, "0x%08X, ", me->bumpFlag);
				/* 10 */ fprintf(docs, "0x%08X, ", me->bumpFx);
				/* 14 */ fprintf(docs, "0x%08X, ", me->tchBmpBdy);
				/* 18 */
				
				fprintf(docs, "0x%08X, ", (j << 24) | (x & 0xFFFF));
				fprintf(docs, "0x%08X, ", ((y & 0xFFFF) << 16) | (z & 0xFFFF));
				fprintf(docs, "0x%08X", ((rad & 0xFFFF) << 16) | (size & 0xFFFF));
				if (i + 1 < idx)
					fprintf(docs, ",");
				fprintf(docs, "\n");
			}
			fprintf(docs, "\t" "}\n};\n");
			/*bodyFlags touchFlag   touchFx     bumpFlag    bumpFX      tchBmpBdy     jj00xxxx    yyyyzzzz    rad size
			   0 0x00000000, 0x00000000, 0x00000000, 0xFFCFFFFF, 0x00000000, 0x00010000, 0x01000000, 0x00000000, 0x000C0064,
			   1 0x00000000, 0x00000000, 0x00000000, 0xFFCFFFFF, 0x00000000, 0x00010100, 0x02000C80, 0x00000000, 0x000A0064,
			   2 0x00000000, 0x00000000, 0x00000000, 0xFFCFFFFF, 0x00000000, 0x00010100, 0x030004B0, 0x00000000, 0x000A0064,
			   3 0x00000000, 0x00000000, 0x00000000, 0xFFCFFFFF, 0x00000000, 0x00010000, 0x04000A8C, 0x00000000, 0x000A0064,
			   4 0x00000000, 0x00000000, 0x00000000, 0xFFCFFFFF, 0x00000000, 0x00010100, 0x050004B0, 0x00000000, 0x000A0064*/
			
			/* print callback code */
/*
   void jointmap_%s(uintptr_t func, int limb, void *jointlist)
   {
        typedef void fptr(int, void *);
        fptr *exec = (fptr*)(func);
        switch (limb)
        {
                case 0xdead:
                        exec(0x8000, list);
                        exec(0x8001, list);
                        break;
                case 0xbeef:
                        exec(0x7000, list);
                        exec(0x7001, list);
                        break;
        }
   }*/
			fprintf(docs
				, "#define jointmap_%s(...) (jointmap_%s)((uintptr_t)__VA_ARGS__)\n"
				, Canitize(name, 0), Canitize(name, 0)
			);
			fprintf(docs
				, "static void (jointmap_%s)(uintptr_t func, int limb, void *list)\n"
				"{\n"
				INDENT "typedef void fptr(int, void *);\n"
				INDENT "fptr *exec = (fptr*)(func);\n"
				INDENT "switch (limb)\n"
				INDENT "{\n"
				, Canitize(name, 0)
			);
			colliderJoint_boneIndex_ascend(list, idx);
			for (int i = 0; i < idx; ++i)
			{
				if (!i || list[i-1].boneIndex != list[i].boneIndex)
				{
					if (i)
						fprintf(docs, "\n"INDENT INDENT "break;\n");
					fprintf(docs, INDENT "case %d:", list[i].boneIndex+1);
				}
				fprintf(docs, "\n"INDENT INDENT "%s(%d, %s);"
					, "exec"
					, list[i].joint
					, "list"
				);
			}
			fprintf(docs, "\n"INDENT INDENT "break;\n");
			fprintf(docs, INDENT "}\n}\n");
		} /* joint / joints */
		else if (streq(Ctype, "cylinder"))
		{
			int idx;
			struct boundingBox *bounds = 0;
			struct colliderJoint me_ = {0};
			struct colliderJoint *me = &me_;
			int16_t x, y, z, rad, height, yshift;
			
			if (!collider_init(g))
				fail(collider_errmsg());
			
			for (idx = 0; ; ++idx)
			{
				struct boundingBox *b;
				b = collider_bounds(g);
				if (!b)
					break;
				bounds = b;
			}
			
			if (idx == 0)
				fail("failed to retrieve mesh from '%s'", g->name);
			if (idx > 1)
				fail("'%s' contains more than one (1) individual mesh"
					, g->name
				);
			
			/* get collider material attribs */
			if (!retriveColliderMtlAttribs(bounds, me))
				return 0;
			
			/* dimensions */
			x = 0;//round((bounds->min_x + bounds->max_x)*0.5f);
			y = 0;//round((bounds->min_y + bounds->max_y)*0.5f);
			z = 0;//round((bounds->min_z + bounds->max_z)*0.5f);
			rad =
				round(
					(fabs(bounds->min_x - bounds->max_x)
						+ fabs(bounds->min_z - bounds->max_z)) /*sum diameter*/
					* 0.5f /* average diameter */
					* 0.5f /* radius = half diameter */
				);
			height = round((bounds->max_y - bounds->min_y));
			yshift = round(bounds->min_y);
			
			/* print init */
			{
				shape = 0x01; /* means cylinder */
				fprintf(docs
					, "static const uint32_t cylinder_%s[] = {\n\t"
					"/*TpAtAcMa    MbSp      */\n\t"
				        /*  TpAtAcMa    MbSp     */
				        /*0x06000919, 0x10000000,*/
					"0x%02X%02X%02X%02X, 0x%02X%02X%02X%02X,\n\t"
					, Canitize(name, 0)
					, type, atFlags, acFlags, maskA
					, maskB, shape, 0, 0
				);
			}
			
			/* ColliderBodyInit */
			fprintf(docs, "/*bodyFlags touchFlags  touchFx     bumpFlags   bumpFx      tchBmpBdy     rrrrhhhh    ssssxxxx    yyyyzzzz*/\n\t");
			/* 00 */ fprintf(docs, "0x%08X, ", me->bodyFlags);
			/* 04 */ fprintf(docs, "0x%08X, ", me->touchFlags);
			/* 08 */ fprintf(docs, "0x%08X, ", me->touchFx);
			/* 0C */ fprintf(docs, "0x%08X, ", me->bumpFlag);
			/* 10 */ fprintf(docs, "0x%08X, ", me->bumpFx);
			/* 14 */ fprintf(docs, "0x%08X, ", me->tchBmpBdy);
			/* 18 */
			
			fprintf(docs, "0x%08X, ", ((rad & 0xFFFF) << 16) | (height & 0xFFFF));
			fprintf(docs, "0x%08X, ", ((yshift & 0xFFFF) << 16) | (x & 0xFFFF));
			fprintf(docs, "0x%08X", ((y & 0xFFFF) << 16) | (z & 0xFFFF));
			fprintf(docs, "\n};\n");
			
		} /* cylinder */
		else
			fail("unknown collider type '%s'", Ctype);
	}
	
	/* mark all materials as unused here */
	for (struct objex_material *m = obj->mtl; m; m = m->next)
		m->isUsed = m->alwaysUsed;
	
	/* mark all textures as unused here */
	for (struct objex_texture *m = obj->tex; m; m = m->next)
		m->isUsed = m->alwaysUsed;
	
	/* mark all palettes as unused here */
	for (struct objex_palette *m = obj->pal; m; m = m->next)
		m->isUsed = 0;
	
	/* mark textures used by non-exception mesh groups as used */
	for (struct objex_g *g = obj->g; g; g = g->next)
	{
		/* skip non-mesh groups and exceptions */
		if (!isMesh(g) || isExcluded(g, only, except))
			continue;
		
		/* test material of every triangle */
		for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
		{
			struct objex_material *mtl = f->mtl;
			if (mtl)
			{
				mtl->isUsed = 1;
				if (mtl->tex0 && !mtl->tex0->alwaysUnused)
					mtl->tex0->isUsed = 1;
				if (mtl->tex1 && !mtl->tex1->alwaysUnused)
					mtl->tex1->isUsed = 1;
			}
		}
	}
	
	/* if applicable, internally divide the objex into more objex's */
	if (!objex_divide(obj, docs))
		fail(objex_errmsg());
	
L_cleanup:
	/* cleanup */
	m->obj = obj;
	return m;
}

const char *z64convert(
	int argc
	, const char *argv[]
	, FILE *docs
	, void (die)(const char *fmt, ...)
)
{
	float scale = 1000;
	const char *in = 0;
	const char *out = 0;
	const char *only = 0;
	const char *except = 0;
	unsigned int baseOfs = 0x06000000;
	struct model *model = 0;
	int playAs = 0;
	const char *namHeader = 0;
	const char *namLinker = 0;
	
	for (int i = 1; i < argc; ++i)
	{
		if (streq(argv[i], "--in"))
		{
			in = argv[++i];
			document_setFileName(argv[i]);
		}
		else if (streq(argv[i], "--out"))
			out = argv[++i];
		else if (streq(argv[i], "--scale"))
		{
			if (!argv[i+1]
			   || sscanf(argv[++i], "%f", &scale) != 1
			)
				return "invalid arguments";
		}
		else if (streq(argv[i], "--address"))
		{
			if (!argv[i+1]
			   || sscanf(argv[++i], "%X", &baseOfs) != 1
			)
				return "invalid arguments";
		}
		else if (streq(argv[i], "--playas"))
			playAs = 1;
		else if (streq(argv[i], "--silent"))
		{         /* do nothing */
		}
		else if (streq(argv[i], "--print-palettes"))
			printPalettes = 1;
		else if (streq(argv[i], "--binary-header"))
		{
			const char *rv = binaryHeaderFlagsFromString(argv[++i], &binaryHeader);
			
			if (rv)
				return rv;
		}
		else if (streq(argv[i], "--only"))
		{
			only = argv[++i];
			if (!only)
				return "--only incomplete";
		}
		else if (streq(argv[i], "--except"))
		{
			except = argv[++i];
			if (!except)
				return "--except incomplete";
		}
		else if (streq(argv[i], "--gui_errors"))
		{        /* do nothing */
			;
		}
		else if (streq(argv[i], "--header"))
		{
			namHeader = argv[++i];
		}
		else if (streq(argv[i], "--linker"))
		{
			namLinker = argv[++i];
		}
		else
			fail("unknown argument '%.64s'", argv[i]);
	}
	
	if (!in)
		return "no in file specified";
	if (!out)
		return "no out file specified";
	if (only && except)
		return "'only' and 'except' cannot be used simultaneously";
	
	/* ad hoc multi-file test */
	#if 0
	{
		struct model *quicktest(const char *in, const char *out)
		{
			return model_load(
				in
				, out
				, namHeader
				, namLinker
				, only
				, except
				, baseOfs
				, scale
				, docs
				, playAs
				, die
			);
		}
		
		#define TESTDIR "test/CommonSimple/"
		#define BINDIR "bin/"
		baseOfs = 0x03000000;
		struct model *room0 = quicktest(TESTDIR "room0.objex", BINDIR "room0.bin");
		struct model *room1 = quicktest(TESTDIR "room1.objex", BINDIR "room1.bin");
		baseOfs = 0x02000000;
		struct model *common = quicktest(TESTDIR "collision.objex", BINDIR "common.bin");
		
		if (sgRval)
			return sgRval;
		
		struct objex *rooms[] = { room0->obj, room1->obj };
		objex_resolve_common_array(common->obj, rooms, sizeof(rooms) / sizeof(*rooms));
		
		if (model_commit(common) || model_commit(room0) || model_commit(room1))
			return sgRval;
		
		model_free(room0);
		model_free(room1);
		model_free(common);
		
		return sgRval;
	}
	#endif
	
	/* load model */
	if (!(model = model_load(
			in
			, out
			, namHeader
			, namLinker
			, only
			, except
			, baseOfs
			, scale
			, docs
			, playAs
			, die
		))
		|| sgRval
	)
		goto L_cleanup;
	
	/* commit model */
	if (model_commit(model))
		goto L_cleanup;
	
	/* cleanup */
L_cleanup:
	model_free(model);
	return sgRval;
}

#include <time.h>
const char *z64convert_flavor(void)
{
	static const char *ben[]={
		"there's water in the sky, mario"
		, "gbi macro energy"
		, "now with multi-texturing"
		, "custom collider magic, how"
		, "flowing hair, scarves, and capes"
		, "environment configuration, eventually"
		, "rewriting this software cost me my soul"
		, "i hope i didn't forget anything"
		, "still 12 and on my 10th edgelord phase"
		, "now 1000000% more open source"
		, "thank you Dragorn for writing the blender plugin"
	};
	
	srand(time(NULL));
	return ben[rand()%(sizeof(ben)/sizeof(*ben))];
}

