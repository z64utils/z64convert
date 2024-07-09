/* <z64.me> objex parsing */

#include <stdio.h>
#include <stddef.h>

#ifndef OBJEX_H_INCLUDED
#define OBJEX_H_INCLUDED

#define OBJ_NAMECONST /*const*/
#define OBJ_NAMECONST_ISCONST 0 /* change to 1 for consts */

#if OBJ_NAMECONST_ISCONST
#	define OBJ_NAME_FREE(X) do { } while (0)
#	define OBJ_NAME_EQUAL(A, B) (A == B)
#	define OBJ_NAME_FIND(OBJEX, NAME) objexString_new(OBJEX, NAME)
#else
#	define OBJ_NAME_FREE(X) free(X)
#	define OBJ_NAME_EQUAL(A, B) !strcmp(A, B)
#	define OBJ_NAME_FIND(OBJEX, NAME) NAME
#endif

/* opaque */
struct objexString;

enum objex_vertexshading
{
	OBJEX_VTXSHADE_NORMAL
	, OBJEX_VTXSHADE_COLOR
    , OBJEX_VTXSHADE_ALPHA
	, OBJEX_VTXSHADE_DYNAMIC
	, OBJEX_VTXSHADE_NONE
};

enum objex_flag
{
	OBJEXFLAG_NO_MULTIASSIGN = (1 << 0)
	, OBJEX_UNKNOWN_DIRECTIVES = (1 << 1) /*die on unknown directives*/
};

struct boundingBox
{
	float min_x; float max_x;
	float min_y; float max_y;
	float min_z; float max_z;
	struct objex_material *mtl;
	int boneIndex;
};

enum objex_gbivar /* NOTE: do not change order! */
{
	OBJEX_GBIVAR_CMS0 = 0
	, OBJEX_GBIVAR_CMT0
	, OBJEX_GBIVAR_MASKS0
	, OBJEX_GBIVAR_MASKT0
	, OBJEX_GBIVAR_SHIFTS0
	, OBJEX_GBIVAR_SHIFTT0
	
	, OBJEX_GBIVAR_CMS1
	, OBJEX_GBIVAR_CMT1
	, OBJEX_GBIVAR_MASKS1
	, OBJEX_GBIVAR_MASKT1
	, OBJEX_GBIVAR_SHIFTS1
	, OBJEX_GBIVAR_SHIFTT1
	
	, OBJEX_GBIVAR_NUM
};

struct objex;
struct objex_g;
struct objex_file;

typedef void (*objex_udata_free)(void *);

/* public functions */
void *objex_assert_vertex_boundaries(
	struct objex *objex, long long int min, long long int max
);
extern struct objex *objex_load(
	FILE *fopen(const char *, const char *)
	, size_t fread(void *, size_t, size_t, FILE *)
	, void *calloc(size_t, size_t)
	, void *malloc(size_t)
	, void free(void *)
	, int chdir(const char *)
	, int chdirfile(const char *)
	, char *getcwd(void)
	, const char *fn
	, const unsigned skelSeg
	, const float scale
	, enum objex_flag flags
);
extern const char *objex_errmsg(void);
extern void *objex_divide(struct objex *objex, FILE *docs);
extern void objex_localize(struct objex *objex);
extern void objex_resolve_common(struct objex *dst, struct objex *needle, struct objex *haystack);
extern void objex_resolve_common_array(struct objex *dst, struct objex *src[], int srcNum);
extern void objex_free(struct objex *objex, void free(void *));
extern struct objex_skeleton *objex_group_split(
	struct objex *objex
	, struct objex_g *g
	, void *calloc(size_t, size_t)
);
extern void objex_scale(struct objex *objex, float scale);
extern void *objex_group_matrixBones(
	struct objex *objex
	, struct objex_g *g
);
extern struct objex_g *objex_g_find(struct objex *objex, const char *name);
extern struct objex_g *objex_g_index(struct objex *objex, const int index);
extern void objex_g_sortByMaterialPriority(struct objex_g *g);
extern void objex_g_get_center_radius(struct objex_g *g, float *x, float *y, float *z, float *r);

/* vec3f */
struct objex_xyz
{
	float  x;
	float  y;
	float  z;
};

/* 3 integers */
struct objex_vec3i
{
	int x;
	int y;
	int z;
};

/* palette */
struct objex_palette
{
	void *udata;
	struct objex_palette *next;
	void *colors;
	struct objex *objex;
	int colorsNum;
	int colorsSize;
	unsigned int fileOfs;
	unsigned int pointer; /* pointer override */
	int isUsed;
	int index; /* Nth item initialized, starting at 0 (order created) */
};

/* texture */
struct objex_texture
{
	void *udata;
	struct objex_texture *next;
	struct objex_palette *palette;
	struct objex_texture *commonRef;
	struct objex_texture *copyOf;
	struct objex_file *file;
	struct objex *objex;
	OBJ_NAMECONST char *filename;  /* filename used for loading texture */
	char *format;    /* custom format string */
	char *alphamode; /* alphamode magic */
	OBJ_NAMECONST char *name;      /* name used for finding */
	OBJ_NAMECONST char *instead;   /* file to use pixel data from instead */
	void *pix;       /* rgba pixel data */
	int isUsed;
	int isMultiFile;
	int alwaysUsed;
	int alwaysUnused;
	int priority;
	int paletteSlot; /* any slot > 0 is valid */
	int w;
	int h;
	unsigned int pointer; /* pointer override */
	int index; /* Nth item initialized, starting at 0 (order read) */
	/* do not allow mixing formats of shared palette slots */
	int fmt;
	int bpp;
	unsigned sz;
	unsigned crc32;
};

/* material */
struct objex_material
{
	void *udata;
	struct objex_material *next;
	struct objex_texture *tex0;
	struct objex_texture *tex1;
	struct objex_file *file;
	struct objex *objex;
	OBJ_NAMECONST char *name;
	char *gbi;
	char *gbivar[OBJEX_GBIVAR_NUM];
	char *attrib;
	int gbiLen;
	int attribLen;
	int isUsed;
	int isStandalone;
	int isEmpty;
	int isMultiFile;
	int alwaysUsed;
	int hasWritten;
	int priority;
	enum objex_vertexshading vertexshading;
	unsigned int useMtlOfs;
	int index; /* Nth item initialized, starting at 0 (order read) */
};

/* bone */
struct objex_bone
{
	void *udata;
	struct objex_skeleton *skeleton;
	struct objex_bone *parent;
	struct objex_bone *child;
	struct objex_bone *next;
	struct objex_g *g; /* group bone points to post-split */
	OBJ_NAMECONST char  *name;
	float  x;
	float  y;
	float  z;
	int index;
};

/* skeleton */
struct objex_skeleton
{
	void *udata;
	struct objex_skeleton *next;
	struct objex_bone *parent;
	struct objex_bone *bone;
	struct objex *objex;
	struct objex_g *g; /* group bone points to post-split */
	char *extra;
	OBJ_NAMECONST char *name;
	unsigned segment;
	int segmentIsLocal;
	int boneNum;
	int isPbody; /* used for deforming physics body */
	int index; /* Nth item initialized, starting at 0 (order read) */
};

/* frame */
struct objex_frame
{
	void *udata;
	struct objex_xyz  pos;
	struct objex_xyz *rot;
	int ms;
};

/* animation */
struct objex_animation
{
	void *udata;
	struct objex_animation *next;
	struct objex_frame *frame;
	struct objex_skeleton *sk;
	OBJ_NAMECONST char *name;
	int frameNum;
	int is_keyed;
	int index; /* Nth item initialized, starting at 0 (order read) */
};

/* vertex weight */
struct objex_weight
{
	struct objex_bone *bone;
	float influence;
};

/* vertex */
struct objex_v
{
	struct objex_weight *weight;
	float  x;
	float  y;
	float  z;
	int    weightNum;
};

/* vertex color */
struct objex_vc
{
	unsigned char  r;
	unsigned char  g;
	unsigned char  b;
	unsigned char  a;
};

/* vertex normal */
struct objex_vn
{
	float  x;
	float  y;
	float  z;
};

/* vertex texture UV */
struct objex_vt
{
	float  x;
	float  y;
};

/* face */
struct objex_f
{
	void *udata;
	void *_pvt; /* private, internal use only */
	struct objex_material *mtl;
	struct objex_vec3i v;
	struct objex_vec3i vt;
	struct objex_vec3i vn;
	struct objex_vec3i vc;
};

/* group */
struct objex_g
{
	void *udata;
	struct objex_g *next;
	struct objex_f *f;
	struct objex *objex;
	struct objex_bone *bone;
	struct objex_file *file;
	struct objex_skeleton *skeleton;
	OBJ_NAMECONST char *name;
	char *attrib;
	struct {
		float x;
		float y;
		float z;
	} origin;
	int fNum;
	int fOwns; /* owns `f` data */
	int hasDeforms; /* contains vertices for 2 or more bones */
	int hasWeight; /* contains vertices for 1 or more bones */
	int noMtl; /* don't write inline materials */
	int isPbody; /* is physics body */
	int hasSplit; /* has been divided into more groups */
	int index; /* Nth item initialized, starting at 0 (order read) */
	int priority;
	unsigned isCollisionAddr;
};

/* file */
struct objex_file
{
	OBJ_NAMECONST char *name;
	struct objex_g *head;
	struct objex *objex;
	unsigned int baseOfs;
	struct objex_g **g_tmp;
	int isCommon;
};

/* a parsed objex file */
struct objex
{
	void *udata;
	struct objex_v  *v;
	struct objex_vn *vn;
	struct objex_vt *vt;
	struct objex_vc *vc;
	struct objex_skeleton *sk;
	struct objex_material *mtl;
	struct objex_animation *anim;
	struct objex_texture *tex;
	struct objex_palette *pal;
	struct objex_g *g;
	struct objex_file *file;
	char *mtllibDir; /* directory of mtllib (used for texture fopen) */
	int vNum;
	int vnNum;
	int vtNum;
	int vcNum;
	int gNum;
	int fileNum;
	int mtlNum;
	struct
	{
		char *animation_framerate;
	} softinfo;
	struct objexString *string;
	unsigned int baseOfs;
};
#endif /* OBJEX_H_INCLUDED */

