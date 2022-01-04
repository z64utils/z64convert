/* <z64.me> zobj.c: utilities for writing zobj-specific data */

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>

#include "zobj.h"
#include "err.h"
#include "put.h"
#include "texture.h" /* texUdata */
#include <gfxasm.h>
#include "doc.h"

#include "ksort.h"

#define F3DEX_GBI_2 1
#include "gbi.h"

/* external dependencies */
#include "n64texconv.h"

struct objex_g *gCurrentGroup = 0;

#define VBUF_MAX 32

static FILE *getDocs(struct objex *obj)
{
	if (!obj || !obj->udata)
		return errmsg("objex no logfile");
	return ((struct objexUdata *)obj->udata)->docs;
}

void objexUdataFree(void *udata_)
{
	struct objexUdata *udata = udata_;
	if (!udata) return;
	
	void *next = 0;
	for (struct reloc *r = udata->identityMatrixList; r; r = next)
	{
		next = r->next;
		free(r);
	}
	
	free(udata);
};
struct zobjProxyArray *zobjProxyArray_new(const char *name, int num)
{
	struct zobjProxyArray *proxy;
	
	if (!(proxy = calloc(1, sizeof(*proxy))))
		return errmsg("out of memory");
	
	if (!(proxy->name = strdup(name)))
	{
		free(proxy);
		return errmsg("out of memory");
	}
	
	if (!(proxy->array = calloc(num, sizeof(*proxy->array))))
	{
		free(proxy->name);
		free(proxy);
		return errmsg("out of memory");
	}
	
	proxy->num = num;
	
	return proxy;
}
void zobjProxyArray_free(struct zobjProxyArray *p)
{
	if (!p)
		return;
	
	if (p->name)
		free(p->name);
	
	if (p->array)
		free(p->array);
	
	free(p);
}

/* matrix.c */
void guRTSF(float r, float p, float h, float x, float y, float z, float sx, float sy, float sz, float mf[4][4]) {
    float dtor = 3.1415926f / 180.0f; //Degrees to Radians
    float sinr, sinp, sinh, cosr, cosp, cosh; //Sine Roll, Sine Pitch, Sine Heading, Cosine Roll, Cosine Pitch, Cosine Heading

    //Math
    r *= dtor; //r (degrees) to radians
    p *= dtor; //p (degrees) to radians
    h *= dtor; //h (degrees) to radians
    sinr = sin(r);//Sine Roll
    cosr = cos(r);//Cosine Roll
    sinp = sin(p);//Sine Pitch
    cosp = cos(p);//Cosine Pitch
    sinh = sin(h);//Sine Heading
    cosh = cos(h);//Cosine Heading

    //Floating-Point Matrix
    mf[0][0] = ((cosp * cosh) * 1.0f) * sx;
    mf[0][1] = (cosp * sinh) * 1.0f;
    mf[0][2] = (-sinp) * 1.0f;
    mf[0][3] = 0.0f;

    mf[1][0] = (sinr * sinp * cosh - cosr * sinh) * 1.0f;
    mf[1][1] = ((sinr * sinp * sinh + cosr * cosh) * 1.0f) * sy;
    mf[1][2] = (sinr * cosp) * 1.0f;
    mf[1][3] = 0.0f;

    mf[2][0] = (cosr * sinp * cosh + sinr * sinh) * 1.0f;
    mf[2][1] = (cosr * sinp * sinh - sinr * cosh) * 1.0f;
    mf[2][2] = ((cosr * cosp) * 1.0f) * sz;
    mf[2][3] = 0.0f;

    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
    mf[3][3] = 1.0f;

    //return mf;
    
    /*int a;
    for(a=0;a<4;a++) {
    	int b;
    	for(b=0;b<4;b++) {
    		printf("mf[%d][%d] = %f\n",a,b,mf[a][b]);
    	}
    }*/
}

/*static long FTOFIX32(float x) {
    unsigned output = (long)((x) * (float)0x00010000);
    return output;
}*/

#define FTOFIX32(X) (long)((X)*(float)0x00010000)

void *guMtxF2L(float mf[4][4]) {
    static unsigned char block[0x40];
    long e1 = 0, e2 = 0;
    long ai = 0, af = 0;
    unsigned char *rval=block;
    int i, j;

    for ( i = 0; i < 4; i++)
    {
        for ( j = 0; j < 2; j++)
        {
            e1 = FTOFIX32(mf[i][j * 2]);
            e2 = FTOFIX32(mf[i][(j * 2) + 1]);
            ai = (e1 & 0xFFFF0000) | ((e2 >> 16) & 0xFFFF);
            //Console.Write("{0:X8} ", ai);
            put32(rval,ai); rval+=4;
        }
        //Console.Write("\n");
    }
    for ( i = 0; i < 4; i++)
    {
        for ( j = 0; j < 2; j++)
        {
            e1 = FTOFIX32(mf[i][j * 2]);
            e2 = FTOFIX32(mf[i][(j * 2) + 1]);
            af = ((e1 << 16) & 0xFFFF0000) | (e2 & 0xFFFF);
            //Console.Write("{0:X8} ", af);
            put32(rval,af); rval+=4;
        }
        //Console.Write("\n");
    }
    return block;
}
/* end matrix.c */

static void *posMtx(VFILE *bin, short x, short y, short z)
{
	unsigned char mtx[] = {
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		x>>8, x&255, y>>8, y&255, z>>8, z&255, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	int sz = sizeof(mtx);
	
	if (vfwrite(mtx, 1, sz, bin) != sz)
		return errmsg("write failure");
	
	return success;
}

/* XXX please review and confirm this makes a
 * translation matrix, not just local matrix
 */
static void *localMatrix(VFILE *bin, struct objex_bone *bone, size_t ofs)
{
	struct reloc *r;
	struct objexUdata *udata;
	struct objex_skeleton *sk;
	if (!bone
		|| !(sk = bone->skeleton)
		|| !sk->objex
		|| !(udata = sk->objex->udata)
		|| !sk->segmentIsLocal
	)
		return success;
	
	unsigned start;
	unsigned end;
	unsigned baseOfs = udata->baseOfs;
	float x = 0;
	float y = 0;
	float z = 0;
	struct
	{
		float x, y, z;
	} stack = {0, 0, 0};
	
//	fprintf(stderr, "search index %d (%s)\n", index, sk->name);
	/* recursive bone walk */
	void walk(struct objex_bone *b)
	{
		//fprintf(stderr, "bone '%s' %f %f %f\n", b->name, stack.x, y, z);
		stack.x += b->x;
		stack.y += b->y;
		stack.z += b->z;
		if (b == bone)
		{
			x = stack.x;
			y = stack.y;
			z = stack.z;
		}
		
		if (b->child)
			walk(b->child);
		
		/* restore stack before moving on to sibling */
		stack.x -= b->x;
		stack.y -= b->y;
		stack.z -= b->z;
		
		if (b->next)
			walk(b->next);
	}
	walk(sk->bone);
	
	const char identity[] = {
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	const int sz = sizeof(identity);
	
#if 0 /* old way: registering a list for later use */
	/* ensure not already in list */
	for (struct reloc *r = udata->identityMatrixList; r; r = r->next)
		if (r->offset == addr)
			return success;
	
	/* allocate one */
	if (!(r = calloc(1, sizeof(*r))))
		return errmsg(ERR_NOMEM);
	
	/* propagate and link into list */
	r->next = udata->identityMatrixList;
	udata->identityMatrixList = r;
	r->offset = ofs;
#else /* new way: updating files as we go */
	
#endif
	
	/* out of file range, can't write identity matrix */
	if (ofs < sk->segment || (ofs - sk->segment) >= (vftell(bin) - sz))
		return errmsg(
			"local matrix %08X (segment %08X) out of range (%08lX)"
			, (int)ofs, sk->segment, vftell(bin)
		);
	
	/* go write identity matrix at offset in file */
	vfseek(bin, ofs - baseOfs, SEEK_SET);
	start = vftell(bin);
//	fprintf(stderr, "identity matrix goes at %08lX (%f, %f, %f)\n", vftell(bin), x, y, z);
	float mat[4][4];
	guRTSF(0, 0, 0, x, y, z, 1, 1, 1, mat);
	void *mat64 = guMtxF2L(mat);
	if (vfwrite(mat64/*identity*/, 1, sz, bin) != sz)
		return errmsg("write failure");
	end = vftell(bin);
	
	/* update local matrix notes */
	if (!udata->localMatrixBlock.start
		|| start < udata->localMatrixBlock.start
	)
		udata->localMatrixBlock.start = start;
	if (end > udata->localMatrixBlock.end)
		udata->localMatrixBlock.end = end;
	
	/* return to end of file before exiting function */
	vfseek(bin, 0, SEEK_END);
	
	return success;
}

void *zobj_initObjex(struct objex *obj, const unsigned int baseOfs, FILE *docs)
{
	if (!obj)
		return errmsg("objex == 0");
	struct objexUdata *udata;
	if (!(obj->udata = udata = calloc(1, sizeof(*udata))))
		return errmsg("out of memory");
	
	udata->free = objexUdataFree;
	udata->baseOfs = baseOfs;
	udata->docs = docs;
	
	return success;
}

/* compiled vertex bitfield, for storing various settings */
struct cvBf
{
	unsigned
		useColor : 1        /* use vertex color */ /* TODO D9 command state changes */
		, usePbody : 1      /* assigned to physics body skeleton */
		, useMatrix : 1     /* uses matrix transform */
		, useCurmatrix : 1  /* uses current limb's matrix */
//		, first : 1         /* force to start of list */
		, matrixId : 8      /* id of matrix vertices assigned to */
	;
};

/* used for sorting every unique setting */
union cvBfV
{
	struct cvBf bf;
	unsigned v;
};

/* a compiled vertex */
struct compvert
{
	int16_t x;
	int16_t y;
	int16_t z;
	int16_t s;
	int16_t t;
	uint32_t rgba;
	union cvBfV bf;
	struct objex_bone *bone;
};

/* compvert_sort_subgroup */
#define compvert_lt(a, b)  \
( \
	( \
		((a).bf).bf.useCurmatrix \
		!= ((b).bf).bf.useCurmatrix \
	) ? \
		( \
			(((a).bf).bf.useCurmatrix) \
				? 1 \
				: 0 \
		) \
	: ( \
		(((a).bf).v) < (((b).bf).v) \
	) \
)
KSORT_INIT(compvert, struct compvert, compvert_lt)

/* sort compvert by subgroup */
static int compvert_sort_subgroup(const void *a, const void *b)
{
	const struct compvert *f0 = a, *f1 = b;
	struct cvBf bf0 = f0->bf.bf;
	struct cvBf bf1 = f1->bf.bf;
	
	/* place vertices using current limb's matrix at beginning */
	if (bf0.useCurmatrix != bf1.useCurmatrix)
	{
		if (bf0.useCurmatrix)  /* place bf0 first */
			return -1;
		return 1;              /* place bf1 first */
	}
	
	/* compare other settings */
	if (f0->bf.v < f1->bf.v)
		return -1;
	if (f0->bf.v > f1->bf.v)
		return 1;
	return 0;
	//return f0->subgroup - f1->subgroup;
}

static void groupUdata_free(void *udata_)
{
	struct groupUdata *udata = udata_;
	if (!udata)
		return;
	
	void *next = 0;
	for (struct reloc *r = udata->reloc; r; r = next)
	{
		next = r->next;
		free(r);
	}
	
	free(udata);
}

static struct groupUdata *groupUdata_new(void)
{
	struct groupUdata *gUdata;
	if (!(gUdata = calloc(1, sizeof(*gUdata))))
		return errmsg(ERR_NOMEM);
	gUdata->free = groupUdata_free;
	return gUdata;
}

static struct compvert
compbuf_new(
	struct objex_g *g
	, struct objex_v *v
	, struct objex_vt *vt
	, struct objex_vn *vn
	, struct objex_vc *vc
	, struct objex_texture *tex
	, struct objex_material *mtl
)
{
	float w = 1;
	float h = 1;
	if (tex)
	{
		w = tex->w;
		h = tex->h;
		if (tex->udata)
		{
			struct texUdata *udata = tex->udata;
			
			w *= udata->uvMult.w;
			h *= udata->uvMult.h;
			
			h /= udata->virtDiv;
		}
	}
	struct compvert result = {
		.x = round(v->x - g->origin.x)
		, .y = round(v->y - g->origin.y)
		, .z = round(v->z - g->origin.z)
		, .s = qs105(vt->x * w)
		, .t = qs105((1.0f - vt->y) * h)
		, .rgba = 0
		, .bone = 0
	};
	
	if (mtl)
	{
		switch (mtl->vertexshading)
		{
			case OBJEX_VTXSHADE_NORMAL:
				if (vc)
					vc = 0;
				break;
			
			case OBJEX_VTXSHADE_COLOR:
				if (vn)
					vn = 0;
				break;
			
			case OBJEX_VTXSHADE_DYNAMIC:
				if (vc
					&& vc->r == 255
					&& vc->g == 255
					&& vc->b == 255
					&& vc->a == 255
				)
					vc = 0;
				break;
			
			case OBJEX_VTXSHADE_NONE:
				vc = 0;
				vn = 0;
				break;
		}
	}
	if (vc)
	{
		/* vertex color */
		result.rgba = (vc->r << 24) | (vc->g << 16) | (vc->b << 8) | vc->a;
		result.bf.bf.useColor = 1;
	}
	else if (vn)
	{
		/* normal */
		result.rgba = 0xFF;
		unsigned char x = vn->x * 127.0;
		unsigned char y = vn->y * 127.0;
		unsigned char z = vn->z * 127.0;
		result.rgba |= x << 24;
		result.rgba |= y << 16;
		result.rgba |= z <<  8;
	}
	else
	{
		/* none; use last texcoord byte for best compression */
		int b = result.t & 0xff;
		result.rgba  = b;
		result.rgba |= b << 24;
		result.rgba |= b << 16;
		result.rgba |= b <<  8;
	}
	
	/* vertex describes bone */
	if (v->weight && v->weight->bone && v->weight->bone->udata)
	{
		struct boneUdata *bUdat;
		result.bone = v->weight->bone;
		
		/* is root bone and root has parent, so override it */
		if (result.bone == result.bone->skeleton->bone/*root*/
			&& result.bone->skeleton->parent/*has parent*/
		) {
			//debugf("%s override\n", result.bone->name);
			result.bone = result.bone->skeleton->parent;
		}
		else if (result.bone->skeleton->isPbody)
			result.bf.bf.usePbody = 1;
		bUdat = result.bone->udata;
		//bUdat = v->weight->bone->udata;
		assert(bUdat);
		result.bf.bf.useMatrix = 1;
		result.bf.bf.matrixId = bUdat->matrixIndex;
	}
	return result;
}

static int compbuf_push(
	struct compvert *buf
	, int *total
	, int max
	, struct compvert n
)
{
	struct compvert *cb;
	for (cb = buf; cb < buf + *total; ++cb)
	{
		if (cb->x == n.x
			&& cb->y == n.y
			&& cb->z == n.z
			&& cb->s == n.s
			&& cb->t == n.t
			&& cb->rgba == n.rgba
			&& cb->bone == n.bone
		) return cb - buf;
	}
	
	/* if you got here, it isn't in the list yet */
	
	/* buffer is full; no room for more */
	if (*total >= max)
	{
		*total = max + 1;
		return 0;
	}
	
	/* push into list */
	*cb = n;
	*total += 1;
	return cb - buf;
}

static void *writeBone(
	VFILE *fp
	, struct objex_bone *b
	, void *calloc(size_t, size_t)
	, void free(void *)
	, int *index
	, int *matrixIndex
	, const int forcePass
)
{
	struct boneUdata *bUdata;
	assert(calloc);
	assert(free);
	assert(index);
	assert(matrixIndex);
	if (*index < 0)
		assert(fp);
	if (!b)
		return success;
	
	/* get baseOfs from objex */
	struct objex *objex = b->skeleton->objex;
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs;
	assert(objexUdata);
	baseOfs = objexUdata->baseOfs;
	
	/* udata not yet allocated, so set it up */
	if (!(bUdata = b->udata))
	{
		if (!(bUdata = calloc(1, sizeof(*bUdata))))
			return errmsg(ERR_NOMEM);
		b->udata = bUdata;
		bUdata->free = free;
	}
	
	/* first pass: collect indices */
	if (*index >= 0)
	{
		/* index where written in file */
		bUdata->index = *index;
		bUdata->matrixIndex = *matrixIndex;
		
		/* bones pointing to meshes */
		if (b->g)
			*matrixIndex += 1;
		
		/* ignore matrix index of first bone, since we snap vertices
		 * assigned to it to Link's skeleton (e.g. Bunny Hood snaps to
		 * Link's head and has two external matrices instead of three)
		 */
		if (!b->parent /* root bone */
			&& b->skeleton->parent /* has parent that overrides root */
		)
			*matrixIndex = 0;
		*index += 1;
	}
	
	/* second pass: write bone display lists */
	else if (*index == -1)
	{
		/* bones pointing to meshes */
		if (b->g && !zobj_writeDlist(fp, b->g, calloc, free, forcePass))
			return 0;
	}
	
	/* final pass: write bones */
	else //if (*index < 0)
	{
		char *extra = b->skeleton->extra;
		int16_t x = round(b->x);
		int16_t y = round(b->y);
		int16_t z = round(b->z);
		uint8_t child = (b->child)
			? ((struct boneUdata*)b->child->udata)->index
			: -1
		;
		uint8_t next = (b->next)
			? ((struct boneUdata*)b->next->udata)->index
			: -1
		;
		uint32_t dlist = (b->g)
			? ((struct groupUdata*)b->g->udata)->dlistOffset + baseOfs
			: 0
		;
		int linkfmt = (extra) ? !!strstr(extra, "z64player") : 0;
		int bytes = 12 + linkfmt * 4;
		unsigned char entry[16];
		
		/* construct limb entry */
		put16(entry + 0, x);
		put16(entry + 2, y);
		put16(entry + 4, z);
		put8 (entry + 6, child);
		put8 (entry + 7, next);
		put32(entry + 8, dlist);
		put32(entry + 12, dlist);
		
		/* write limb entry */
		vfwrite(entry, 1, bytes, fp);
	}
	
	/* write child if there is one */
	if (b->child
		&& !writeBone(fp, b->child, calloc, free, index, matrixIndex, forcePass)
	)
		return 0;
	
	/* write next if there is one */
	if (b->next
		&& !writeBone(fp, b->next, calloc, free, index, matrixIndex, forcePass)
	)
		return 0;
	
	return success;
}

static void *ow_if_exist(char *str, char *ss, const char *ow, int *found)
{
	if (!str || !ss || !ow)
		return success;
	
	/* not in string */
	if (!(str = strstr(str, ss)))
		return success;
	
	/* won't fit */
	if (strlen(ow) > strlen(ss))
		return errmsg("expansion '%s' too long for '%s'", ow, ss);
	
	/* found one */
	*found += 1;
	memset(str, ' ', strlen(ss));
	memcpy(str, ow, strlen(ow));
	
	return success;
}

static const char *hex2str(unsigned int x)
{
	static char buf[16];
	sprintf(buf, "0x%08X", x);
	return buf;
}

static const char *dec2str(unsigned int d)
{
	static char buf[16];
	sprintf(buf, "%d", d);
	return buf;
}

static void strins(char **dst, char *src)
{
	memmove(*dst + strlen(src), *dst, strlen(*dst) + 1/*term*/);
	memcpy(*dst, src, strlen(src));
}

static void *expand_loadtexel(
	char which
	, struct objex_material *mtl
	, char **at
	, int palette_only
)
{
	int o;
	int is4bit;
	char tbuf[4096];
	
	struct mtlUdata *mtlUdata = mtl->udata;
	struct objex *objex = mtl->objex;
	struct objexUdata *udata = objex->udata;
	struct objex_texture *tex = (which == '0') ? mtl->tex0 : mtl->tex1;
	unsigned int baseOfs = udata->baseOfs;
	o = (which == '0') ? 0 : OBJEX_GBIVAR_CMS1 - OBJEX_GBIVAR_CMS0;
	
	/* TODO the magic value 15 appears twice in this function */
	
	/* no texture, so no need to expand this */
	if (!tex)
		return success;
	
	is4bit = ((struct texUdata*)tex->udata)->gbiBpp == N64TEXCONV_4;
	
//	at += strlen(at);
	
	/* palette prefix (if applicable) */
	if (palette_only)
	{
		if (mtlUdata->wrotePalette)
			return success;
		if (tex->palette)
		{
			strins(at, "gsDPSetTextureLUT(G_TT_RGBA16),\n");
			const char *tlut = "gsDPLoadTLUT_pal256(";
			if (is4bit)
				tlut = "gsDPLoadTLUT_pal16(15, ";
			sprintf(tbuf, "%s_texel%cpalette),\n", tlut, which);
			strins(at, tbuf);
//			strins(at, "gsDPSetTextureLUT(G_TT_RGBA16),\n");
			/* this notes that we should not override this */
			mtlUdata->wrotePalette = 1;
			//fprintf(stderr, "result = ~%s~\n", *at);
		}
		else
			strins(at, "gsDPSetTextureLUT(G_TT_NONE),\n");
	}
	else /* texture only */
	{
		if (mtlUdata->wroteTexture)
		{
			/* multiblock */
			if (is4bit)
				sprintf(
					tbuf
					, "gsDPLoadMultiBlock_4b(_texel%caddress, 0x0100, 1, _texel%cformat, _texel%cwidth, _texel%cheight, 15, %s, %s, %s, %s, %s, %s),\n"
					, which, which, which, which
					, mtl->gbivar[OBJEX_GBIVAR_CMS0    + o]
					, mtl->gbivar[OBJEX_GBIVAR_CMT0    + o]
					, mtl->gbivar[OBJEX_GBIVAR_MASKS0  + o]
					, mtl->gbivar[OBJEX_GBIVAR_MASKT0  + o]
					, mtl->gbivar[OBJEX_GBIVAR_SHIFTS0 + o]
					, mtl->gbivar[OBJEX_GBIVAR_SHIFTT0 + o]
				);
			else
				sprintf(
					tbuf
					, "gsDPLoadMultiBlock(_texel%caddress, 0x0100, 1, _texel%cformat, _texel%cbitdepth, _texel%cwidth, _texel%cheight, 0, %s, %s, %s, %s, %s, %s),\n"
					, which, which, which, which, which
					, mtl->gbivar[OBJEX_GBIVAR_CMS0    + o]
					, mtl->gbivar[OBJEX_GBIVAR_CMT0    + o]
					, mtl->gbivar[OBJEX_GBIVAR_MASKS0  + o]
					, mtl->gbivar[OBJEX_GBIVAR_MASKT0  + o]
					, mtl->gbivar[OBJEX_GBIVAR_SHIFTS0 + o]
					, mtl->gbivar[OBJEX_GBIVAR_SHIFTT0 + o]
				);
			strins(at, tbuf);
			return success;
		}
		/* non-multiblock */
		if (is4bit)
			sprintf(
				tbuf
				, "gsDPLoadTextureBlock_4b(_texel%caddress, _texel%cformat, _texel%cwidth, _texel%cheight, 15, %s, %s, %s, %s, %s, %s),\n"
				, which, which, which, which
				, mtl->gbivar[OBJEX_GBIVAR_CMS0    + o]
				, mtl->gbivar[OBJEX_GBIVAR_CMT0    + o]
				, mtl->gbivar[OBJEX_GBIVAR_MASKS0  + o]
				, mtl->gbivar[OBJEX_GBIVAR_MASKT0  + o]
				, mtl->gbivar[OBJEX_GBIVAR_SHIFTS0 + o]
				, mtl->gbivar[OBJEX_GBIVAR_SHIFTT0 + o]
			);
		else
			sprintf(
				tbuf
				, "gsDPLoadTextureBlock(_texel%caddress, _texel%cformat, _texel%cbitdepth, _texel%cwidth, _texel%cheight, 0, %s, %s, %s, %s, %s, %s),\n"
				, which, which, which, which, which
				, mtl->gbivar[OBJEX_GBIVAR_CMS0    + o]
				, mtl->gbivar[OBJEX_GBIVAR_CMT0    + o]
				, mtl->gbivar[OBJEX_GBIVAR_MASKS0  + o]
				, mtl->gbivar[OBJEX_GBIVAR_MASKT0  + o]
				, mtl->gbivar[OBJEX_GBIVAR_SHIFTS0 + o]
				, mtl->gbivar[OBJEX_GBIVAR_SHIFTT0 + o]
			);
		strins(at, tbuf);
		mtlUdata->wroteTexture = 1;
	}
	
	return success;
}

static void *expand_loadtexels(
	struct objex_material *mtl
	, char *gbi
	, int *found
)
{
	char *at;
	char buf[16];
	sprintf(buf, "_loadtexels");
	
	if (!(at = strstr(gbi, buf)))
		return success;
	
	struct objex *objex = mtl->objex;
	struct objexUdata *udata = objex->udata;
	struct mtlUdata *mtlUdata = mtl->udata;
	unsigned int baseOfs = udata->baseOfs;
	
	/* overwrite _loadtexelX */
	memset(at, ' ', strlen(buf));
	
	/* shift everything along */
	/* FIXME TODO invalid write size of 1 (valgrind);
	 *       _loadtexels is expected
	 *       to be at the end of gbi anyway, so this isn't in
	 *       dire need of a fix; just be aware of it */
	//memmove(at + 2048, at, strlen(at)+1);
//	memset(at, '\0', 2048);
	
//	*at = '\0'; /* for subsequent strcat */
	
	/* TODO the magic value 15 appears twice in this function */
	
	mtlUdata->wrotePalette = 0;
	mtlUdata->wroteTexture = 0;
	if (!expand_loadtexel('0', mtl, &at, 1))
		return 0;
	if (!expand_loadtexel('1', mtl, &at, 1))
		return 0;
	if (!expand_loadtexel('0', mtl, &at, 0))
		return 0;
	if (!expand_loadtexel('1', mtl, &at, 0))
		return 0;
	
	/* overwrite 0-term with a ' ' */
//	at += strlen(at);
//	*at = ' ';
	
	*found += 1;
	return success;
}

/* returns pointer to first non-whitespace character of next line,
 * or 0 once it reaches the end of the string (skips blank lines)
 */
static char *nextline(char *str)
{
	char *next;
	
	if (!str)
		return 0;
	
	if (!(next = strchr(str, '\n')))
		next = strchr(str, '\r');
	if (!next)
		return 0;
	str = next;
	
	while (*str && (iscntrl(*str) || isblank(*str)))
		++str;
	if (!*str)
		return 0;
	
	return str;
}

static void *mtl_gbi_vars(struct objex_material *mtl, char *gbi)
{
	assert(mtl);
	assert(gbi);
	
	struct objex *objex = mtl->objex;
	struct objexUdata *udata = objex->udata;
	struct objex_texture *tex0 = mtl->tex0;
	struct objex_texture *tex1 = mtl->tex1;
	struct texUdata *t0 = 0, *t1 = 0;
	unsigned int baseOfs = udata->baseOfs;
	unsigned int t0palette = 0;
	unsigned int t1palette = 0;
	if (tex0) {
		t0 = tex0->udata;
		t0palette = (tex0->palette) ? tex0->palette->fileOfs : 0;
		t0palette += baseOfs;
	}
	if (tex1) {
		t1 = tex1->udata;
		t1palette = (tex1->palette) ? tex1->palette->fileOfs : 0;
		t1palette += baseOfs;
	}
	
	while (1)
	{
		int found = 0;
		if (!expand_loadtexels(mtl, gbi, &found))
			return 0;
		if (!found)
			break;
	}
	
	int num2(const int x)
	{
		int r;
		int n = 1;
		for (r = 2; r < x; r *= 2)
			++n;
		return n;
	}
	
	/* contains '_group="' */
	if (strstr(gbi, "_group=\""))
	{
		struct objex_g *g;
		char name[256];
		char *ss = strstr(gbi, "_group=\"");
		char *start = strchr(gbi, '"') + 1;
		char *end = strchr(start, '"');
		char *nl = nextline(ss);
		
		if (!end || (nl && end > nl))
			return errmsg("'%.20s' malformatted", ss);
		
		if (end - start >= sizeof(name))
			return errmsg("'%.*s' name too long", (int)(end - start), start);
		
		memcpy(name, start, end - start);
		name[end - start] = '\0';
		
		if (!(g = objex_g_find(objex, name)))
			return errmsg("'%s' group not found", name);
		
		if (g == gCurrentGroup)
			return errmsg(
				"'%s' contains self-referential branch by way of material '%s'"
				, name, mtl->name
			);
		
		memset(ss, ' ', end - ss);
		sprintf(name, "0x%04X%04X", 0xDEAD, g->index);
		memcpy(ss, name, strlen(name));
	}
	
	if (t0)
	{
		unsigned int addr = t0->fileOfs + baseOfs;
		int w = tex0->w;
		int h = tex0->h / t0->virtDiv;
		int maskS = num2(w);
		int maskT = num2(h);
		int wTile = w;
		int hTile = h;
		if (t0->isSmirror)
			wTile *= 2;
		if (t0->isTmirror)
			hTile *= 2;
		// if t0 has segment override, use that
		if (tex0->pointer)
			addr = tex0->pointer;
		while (1)
		{
		int f = 0;
		if (!ow_if_exist(gbi, "_texel0address", hex2str(addr), &f)
			|| !ow_if_exist(gbi, "_texel0format", t0->gbiFmtStr, &f)
			|| !ow_if_exist(gbi, "_texel0bitdepth", t0->gbiBppStr, &f)
			|| !ow_if_exist(gbi, "_texel0width-1", dec2str(wTile-1), &f)
			|| !ow_if_exist(gbi, "_texel0height-1", dec2str(hTile-1), &f)
			|| !ow_if_exist(gbi, "_texel0width", dec2str(w), &f)
			|| !ow_if_exist(gbi, "_texel0height", dec2str(h), &f)
			|| !ow_if_exist(gbi, "_texel0palette", hex2str(t0palette), &f)
			|| !ow_if_exist(gbi, "_texel0masks", dec2str(maskS), &f)
			|| !ow_if_exist(gbi, "_texel0maskt", dec2str(maskT), &f)
		)
			return 0;
		if (!f) break;
		}
	}
	
	if (t1)
	{
		unsigned int addr = t1->fileOfs + baseOfs;
		int w = tex1->w;
		int h = tex1->h / t1->virtDiv;
		int maskS = num2(w);
		int maskT = num2(h);
		int wTile = w;
		int hTile = h;
		if (t1->isSmirror)
			wTile *= 2;
		if (t1->isTmirror)
			hTile *= 2;
		// if t1 has segment override, use that
		if (tex1->pointer)
			addr = tex1->pointer;
		while (1)
		{
		int f = 0;
		if (!ow_if_exist(gbi, "_texel1address", hex2str(addr), &f)
			|| !ow_if_exist(gbi, "_texel1format", t1->gbiFmtStr, &f)
			|| !ow_if_exist(gbi, "_texel1bitdepth", t1->gbiBppStr, &f)
			|| !ow_if_exist(gbi, "_texel1width-1", dec2str(wTile-1), &f)
			|| !ow_if_exist(gbi, "_texel1height-1", dec2str(hTile-1), &f)
			|| !ow_if_exist(gbi, "_texel1width", dec2str(w), &f)
			|| !ow_if_exist(gbi, "_texel1height", dec2str(h), &f)
			|| !ow_if_exist(gbi, "_texel1palette", hex2str(t1palette), &f)
			|| !ow_if_exist(gbi, "_texel1masks", dec2str(maskS), &f)
			|| !ow_if_exist(gbi, "_texel1maskt", dec2str(maskT), &f)
		)
			return 0;
		if (!f) break;
		}
	}
	
	return success;
}

static void *group_reloc(struct objex_g *g, size_t ofs)
{
	struct reloc *r = calloc(1, sizeof(*r));
	struct groupUdata *udata = g->udata;
	if (!r)
		return errmsg(ERR_NOMEM);
	if (!udata && !(udata = g->udata = groupUdata_new()))
		return errmsg(ERR_NOMEM);
	r->next = udata->reloc;
	r->offset = ofs;
	udata->reloc = r;
	return success;
}

void *zobj_writeUsemtl(VFILE *bin, struct objex_material *mtl)
{
	if (!mtl)
		return success;
	
	if (!mtl->gbi)
		return success;
	
	/* get baseOfs from objex */
	FILE *docs = 0;
	struct objex *objex = mtl->objex;
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs = 0;
	
	if (objexUdata)
		baseOfs = objexUdata->baseOfs;
	
	if (!(docs = getDocs(objex)))
		return 0;
	
	/* standalone material */
	if (mtl->isStandalone)
	{
		assert(objexUdata);
		
		/* already written, load via pointer and early exit */
		if (mtl->hasWritten)
		{
			vfput64s(bin, 0xDE000000, baseOfs + mtl->useMtlOfs);
			return success;
		}
		
		/* align */
		vfalign(bin, 8);
		
		/* not yet written */
		mtl->hasWritten = 1;
		mtl->useMtlOfs = vftell(bin);
		
		/* print docs */
		// fprintf(docs, DOCS_DEF "MTL_" DOCS_SPACE "  0x%08X\n"
		// 	, Canitize(mtl->name, 1)
		// 	, (int)vftell(bin) + baseOfs
		// );
		document_doc(
			mtl->name,
			NULL,
			(int)vftell(bin) + baseOfs,
			T_MTL
		);
		
		/* texture dimensions */
		for (int i = 0; i < 2; ++i)
		{
			struct objex_texture *tex = (i == 0) ? mtl->tex0 : mtl->tex1;
			char buf[16];
			struct texUdata *udata;
			int w;
			int h;
			
			if (!tex || !(udata = tex->udata))
				continue;
			
			w = tex->w;
			h = tex->h / udata->virtDiv;
			
			sprintf(buf, "TEXEL%d_W", i);
			// fprintf(docs, DOCS_DEF "MTL_" DOCS_SPACE_2 " %d\n"
			// 	, Canitize(mtl->name, 1)
			// 	, buf
			// 	, w
			// );
			document_doc(
				mtl->name,
				buf,
				w,
				T_MTL
			);
			
			sprintf(buf, "TEXEL%d_H", i);
			// fprintf(docs, DOCS_DEF "MTL_" DOCS_SPACE_2 " %d\n"
			// 	, Canitize(mtl->name, 1)
			// 	, buf
			// 	, h
			// );
			document_doc(
				mtl->name,
				buf,
				h,
				T_MTL
			);
		}
	}
	
	char *sp = 0;
	char *tok = 0;
	char *dup = malloc(mtl->gbiLen);
	if (!dup)
		return errmsg(ERR_NOMEM);
	memcpy(dup, mtl->gbi, mtl->gbiLen);
//		debugf("gbi = '%s'\n", dup);
	const char *delim = "\r\n";
	
//	fprintf(stderr, "gbi = '%s'\n", dup);
	if (!mtl_gbi_vars(mtl, dup))
		return 0;
//	fprintf(stderr, "change to '%s'\n", dup);
	
	int settimg = 0;
	int settilesize = 0;
	for (tok = strtok_r(dup, delim, &sp)
		; tok
		; tok = strtok_r(0, delim, &sp)
	)
	{
//			debugf("gbi = '%s' %p\n", mtl->gbi, mtl->gbi);
//			debugf("gbi: '%s'\n", dup);
		uint32_t *r;
//		fprintf(stderr, "exec '%s'\n", tok);
		if ((r = f3dex2_exec(tok)))
			do
			{
//					debugf("%08X %08X\n", r[0], r[1]);
				/* DE command relocs */
				if ((r[0] >> 24) == G_DL && (r[1] >> 16) == 0xDEAD)
				{
					struct objex_g *g;
					int idx = r[1] & 0xFFFF;
					if (!(g = objex_g_index(objex, idx)))
						return errmsg("group reloc index '%d' fail", idx);
					
					/* register r[1]'s address as a relocation */
					if (!group_reloc(g, vftell(bin) + 4))
						return 0;
				}
				
				/* standalone materials get primcolor & more written */
				if (mtl->isStandalone)
				{
					const char *ex = 0;
					char buf[32];
					int ofs = 0;
					
					switch (r[0] >> 24)
					{
						case G_SETPRIMCOLOR:
							ex = "PRIMCOLOR";
							ofs = 4;
							break;
						
						case G_SETENVCOLOR:
							ex = "ENVCOLOR";
							ofs = 4;
							break;
						
						case G_SETTIMG:
							sprintf(buf, "TIMG%d", settimg);
							settimg += 1;
							ex = buf;
							ofs = 4;
							break;
						
						case G_SETTILESIZE:
							sprintf(buf, "TILESIZE%d", settilesize);
							settilesize += 1;
							ex = buf;
							break;
					}
					
					/* offsets are a bad idea; for example, we want
					 * primcolor to point to FA command, not its
					 * second word; this allows consistency
					 */
					ofs = 0;
					if (ex)
						document_doc(
							mtl->name,
							ex,
							(int)vftell(bin) + baseOfs + ofs,
							T_MTL
						);
						// fprintf(docs, DOCS_DEF "MTL_" DOCS_SPACE_2 " 0x%08X\n"
						// 	, Canitize(mtl->name, 1)
						// 	, ex
						// 	, (int)vftell(bin) + baseOfs + ofs
						// );
						
				}
				vfput32(bin, r[0]);
				vfput32(bin, r[1]);
			}
			while ((r = f3dex2_exec(tok)));
		else if (gfxasm_fatal())
		{
			free(dup);
			return errmsg("%s", gfxasm_error());
		}
	}
	free(dup);
	
	/* standalone must end in DF command */
	if (mtl->isStandalone)
		vfput64s(bin, 0xDF000000, 0x00000000);
	
	return success;
}

void *zobj_writeDlist(
	VFILE *bin
	, struct objex_g *g
	, void *calloc(size_t, size_t)
	, void free(void *)
	, const int forcePass
)
{
	if (!g/* || g->udata*/)
		return success;
	
	gCurrentGroup = g;
	
	/* empty group */
	if (!g->fNum)
	{
		uint32_t *dlOfs = &((struct groupUdata*)g->udata)->dlistOffset;
		
		/* not 0xffffffff; has already been written */
		if (!(*dlOfs & 3))
			return success;
		
		/* otherwise, note offset */
		*dlOfs = vftell(bin);
		
		/* and write to file */
		Gfx gfx;
		Gfx *p = &gfx;
		gSPEndDisplayList(p++);
		vfput32(bin, gfx.hi);
		vfput32(bin, gfx.lo);
		return success;
	}
	
	FILE *docs = 0;
	struct groupUdata *gUdata;
	int matrixBone = -1;
	size_t vbufStart = 0;
	struct compvert vbuf[VBUF_MAX];
	struct objex *objex = g->objex;
	struct objex_v *v = objex->v;
	struct objex_vt *vt = objex->vt;
	struct objex_vn *vn = objex->vn;
	struct objex_vc *vc = objex->vc;
	struct objex_f *f = 0;
	struct objex_texture *tex = 0;
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs;
	int vtotal;
	int passStart = 1;
	int passEnd = 2;
	int isEmpty = 0;
	if (forcePass)
		passStart = passEnd = forcePass;
	
	if (!(docs = getDocs(objex)))
		return 0;
	
	/* get baseOfs from objex */
	assert(objexUdata);
	baseOfs = objexUdata->baseOfs;
	
	if (!(gUdata = g->udata))
	{
		if (!(gUdata = groupUdata_new()))
			return errmsg(ERR_NOMEM);
		g->udata = gUdata;
	}
	
	/* default matrix applied to skeletal display list */
#if 0 /* moved into zobj_writePbody */
	{
		struct objex_v *one = v + g->f->v.x;
		if (one->weight && one->weight->bone && one->weight->bone->udata)
		{
			struct boneUdata *bUdat = one->weight->bone->udata;
			matrixBone = bUdat->matrixIndex;
		}
	}
#endif
	if (g->bone && g->bone->udata && matrixBone <= 0)
	{
//		fprintf(stderr, "group '%s' has bone\n", g->name);
		matrixBone = ((struct boneUdata*)g->bone->udata)->matrixIndex;
	}
//	fprintf(stderr, "%s: matrixBone = %d\n", g->name, matrixBone);
	
	#define compbuf_compile()       \
	x = compbuf_push(vbuf, &vtotal  \
		, VBUF_MAX                   \
		, compbuf_new(               \
			g                         \
			, &v[f->v.x]              \
			, &vt[f->vt.x]            \
			, &vn[f->vn.x]            \
			, (f->vc.x < 0) ? 0 : &vc[f->vc.x] \
			, tex                     \
			, f->mtl                  \
		)                            \
	); (void)x;                     \
	y = compbuf_push(vbuf, &vtotal  \
		, VBUF_MAX                   \
		, compbuf_new(               \
			g                         \
			, &v[f->v.y]              \
			, &vt[f->vt.y]            \
			, &vn[f->vn.y]            \
			, (f->vc.y < 0) ? 0 : &vc[f->vc.y] \
			, tex                     \
			, f->mtl                  \
		)                            \
	); (void)y;                     \
	z = compbuf_push(vbuf, &vtotal  \
		, VBUF_MAX                   \
		, compbuf_new(               \
			g                         \
			, &v[f->v.z]              \
			, &vt[f->vt.z]            \
			, &vn[f->vn.z]            \
			, (f->vc.z < 0) ? 0 : &vc[f->vc.z] \
			, tex                     \
			, f->mtl                  \
		)                            \
	); (void)z;
	
	/* sort triangles by material priority */
	objex_g_sortByMaterialPriority(g);
	
	/* two passes:
	 * pass 1: write vertex buffers
	 * pass 2: write display list
	 */

	for (int pass = passStart; pass <= passEnd; ++pass)
	{
		Gfx gfxLast = {0};
		vtotal = 0;
		struct objex_f *fStart = g->f;
		size_t vbufCur = 0;
		vtotal = 0;
		int prevLimb = matrixBone;
		int prevPbody = -1;
		int isPbody = 0; /* is physics body */
		struct cvBf Obf = { 0 };
		int firstV = 0;
		
		/* is physics body */
		if (g->bone
			&& g->bone->skeleton->isPbody
			&& g->hasSplit /* hasSplit is necessary; if (!hasSplit),
			                * handle as any other mesh assigned to
			                * a single bone would be */
		)
			isPbody = 1;
		
		/* for physics bodies, force writing matrix at start
		 * of display list regardless
		 */
		if (isPbody)
			matrixBone = -1;
		
		/* dlists and vertex buffers must be 8-byte aligned */
		vfalign(bin, 8);
		
		if (pass == 1)
			vbufStart = vftell(bin);
		else if (pass == 2)
		{
			gUdata->dlistOffset = vftell(bin);
			gUdata->hasWritten = 1;
		}
		vbufCur = vbufStart + baseOfs;
						
		/* change state: use Ov's state */
		/* TODO ugly nested function */
		void *state_change(struct compvert *Ov)
		{
			struct cvBf bf = Ov->bf.bf;
			
			if (bf.useColor != Obf.useColor && firstV != 0) {
				Gfx gfx;
				Gfx *p = &gfx;
				int setbits = 0;
				if ((bf.useColor) == 0)
					setbits = G_LIGHTING;
				/* toggle vertex colors */
				gSPGeometryMode(p++, G_LIGHTING, setbits);
				vfput32(bin, gfx.hi);
				vfput32(bin, gfx.lo);
			}
			firstV = 1;
			Obf = bf;
			
			/* change in vertex weight */
			//debugf("do a state change %d vs %d\n", v - vbuf, Ov - vbuf);
			if (bf.matrixId != prevLimb
				|| bf.usePbody != prevPbody /* Pbody toggled */
			)
			{
				Gfx gfx;
				Gfx *p = &gfx;
				if (1 == 0)//if (Ov->subgroup & CV_CURMATRIX)
				{
					/* subgroup describes limb matrix */
					/* TODO try a popmatrix here sometime
					 * it is not necessary in order for the
					 * meshes to display properly, so may
					 * be fine...
					 */
					assert(0);
				}
				else
				{
					/* entire mesh is a physics body */
					/* TODO do only if should */
					if (isPbody)
					{
						struct objex_bone *b;
						uint32_t mtxaddr;
						int limb;
						struct boneUdata *bUdat;
						prevPbody = bf.usePbody;
						
						debugf("Pbody %d '%s'\n"
							, ((struct boneUdata*)Ov->bone->udata)->matrixIndex
							, Ov->bone->name
						);
						//debugf("  %d(old) vs %d(new)\n", prevLimb, bf.matrixId);
						//debugf("  %d(old) vs %d(new)\n", prevPbody, bf.usePbody);
						
						/* for Pbodies attached to bones, we always
						 * start by loading the matrix of the parent
						 */
/* TODO FIXME we need to test custom Pbodies to see both how and
 *            whether a root transformation should be loaded each
 *            time for parentless Pbodies; for example, we may
 *            want to do a +1 on anything parentless and assume
 *            the parent matrix to be at 0 in its ram segment
 *            and do the load/apply thing each time, unless it is
 *            a better idea to actually just use the load type 03
 *            for each instead. possible optimization opportunity?
 */
						if ((b = g->bone->skeleton->parent))
						{
							if (!(bUdat = b->udata))
								return errmsg("pBody parent unprocessed");
							limb = bUdat->matrixIndex;
							mtxaddr = b->skeleton->segment;
							mtxaddr += 0x40 * limb;
							gfx.hi = 0xDA380003;
							gfx.lo = mtxaddr;
						
							/* write local identity matrices */
							//if (!localMatrix(bin, b->skeleton, mtxaddr, limb))
							//if (!localMatrix(bin, Ov->bone, mtxaddr))
							//	return 0;
							
							// write DA command
							if (gfx.hi != gfxLast.hi || gfx.lo != gfxLast.lo)
							{
								vfput32(bin, gfx.hi);
								vfput32(bin, gfx.lo);
								gfxLast = gfx;
							}
							prevLimb = limb;
						}
						
						/* if vertex is assigned to a Pbody, we then
						 * apply the matrix of the bone it uses
						 */
//						if (Ov->bone->skeleton->isPbody)
						if (bf.usePbody)
						{
							/* now we multiply it by this submatrix */
							limb = bf.matrixId;
							mtxaddr = g->bone->skeleton->segment;
							mtxaddr += 0x40 * limb;
							/* matrix relative to parent (apply) */
							if (b)
								gfx.hi = 0xDA380001;
							/* no parent, so we load instead */
							else
								gfx.hi = 0xDA380003;
							gfx.lo = mtxaddr;
						
							/* write local identity matrices */
							//if (!localMatrix(bin, g->bone->skeleton, mtxaddr, limb))
							//if (!localMatrix(bin, Ov->bone, mtxaddr))
							//	return 0;
							
							//debugf("%08X %08X\n", gfx.hi, gfx.lo);
							
							// write DA command */
							if (gfx.hi != gfxLast.hi || gfx.lo != gfxLast.lo)
							{
								vfput32(bin, gfx.hi);
								vfput32(bin, gfx.lo);
								gfxLast = gfx;
							}
							prevPbody = bf.usePbody;
							prevLimb = limb;
						}
					}
					/* standard skeleton matrix */
					else if (g->bone
						&& bf.matrixId != prevLimb /* omit redundancy */
					)
					{
//						fprintf(stderr, "%s: %d != %d\n", g->name, bf.matrixId, prevLimb);
						/* subgroup describes other matrix */
						uint32_t mtxaddr = g->bone->skeleton->segment;
						int limb = bf.matrixId;
						prevLimb = limb;
						mtxaddr += 0x40 * limb;
						//gSPMatrix(p++, mtxaddr, G_MTX_PUSH | G_MTX_LOAD);
						
						gfx.hi = 0xDA380003;
						gfx.lo = mtxaddr;
						// write DA command */
						if (gfx.hi != gfxLast.hi || gfx.lo != gfxLast.lo)
						{
							vfput32(bin, gfx.hi);
							vfput32(bin, gfx.lo);
							gfxLast = gfx;
						}
						
						/* write local identity matrices */
						//if (!localMatrix(bin, g->bone->skeleton, mtxaddr, limb))
						//if (!localMatrix(bin, Ov->bone, mtxaddr))
						//	return 0;
					}
				}
			}
			return success;
		}
		
		/* TODO ugly nested function */
		void *binflush(void)
		{
			/* when dealing with triangles that are empties,
			 * we write no geometry here */
			if (isEmpty)
				goto L_jump2clear;
			/* we want vertices using matrixBone to be first, so we
			 * don't have to write a DA command for them
			 */
			for (int i = 0; i < vtotal; ++i)
			{
				struct cvBf *bf = &vbuf[i].bf.bf;
				if ((bf->useMatrix && bf->matrixId == matrixBone)
					|| (bf->usePbody == prevPbody && bf->matrixId == prevLimb)
					|| (isPbody && !bf->usePbody)
					|| isPbody  /* XXX fixes hilda dress (costs extra da commands) */
				)
					bf->useCurmatrix = 1;
			}
			
			/* sort vertices by subgroup */
			ks_mergesort(compvert, vtotal, vbuf, 0);
			
			if (pass == 1)
			{
				/* write_vbuf */
				for (int i = 0; i < vtotal; ++i)
				{
					vfput16(bin, vbuf[i].x);
					vfput16(bin, vbuf[i].y);
					vfput16(bin, vbuf[i].z);
					vfput16(bin, 0);
					vfput16(bin, vbuf[i].s);
					vfput16(bin, vbuf[i].t);
					vfput32(bin, vbuf[i].rgba);
				}
			}
			else /* pass == 2 */
			{
				Gfx gfx;
				Gfx *p;
				struct compvert *Ov = vbuf;
				int vidx = 0;
				struct compvert *vWalk;
				
				/* step through vertices one at a time, performing
				 * material/matrix state changes where necessary
				 */
				for (vWalk = vbuf; vWalk - vbuf < vtotal; ++vWalk)
				{
					/* subgroup of current vertex doesn't match previous */
					/* TODO FIXME bone comparison */
					if (vWalk->bf.v != Ov->bf.v)
					//if (vWalk->subgroup != Ov->subgroup || vWalk->bone != Ov->bone)
					{
						int vnum = vWalk - Ov;
						
						/* write Ov's state */
						if (!state_change(Ov))
							return 0;
						
						/* generate command */
						p = &gfx;
						gSPVertex(p++, (vbufCur + vidx * 16), vnum, vidx);
					
						/* write command into file */
						vfput32(bin, gfx.hi);
						vfput32(bin, gfx.lo);
						
						/* advance */
						vidx += vnum;
						Ov = vWalk;
					}
				}
				/* write last vertex group */
				if (vWalk > Ov)
				{
					int vnum = vWalk - Ov;
					
					/* write Ov's state */
					if (!state_change(Ov))
						return 0;
					
					p = &gfx;
					gSPVertex(p++, (vbufCur + vidx * 16), vnum, vidx);
					
					/* write commands into file */
					vfput32(bin, gfx.hi);
					vfput32(bin, gfx.lo);
				}
				
				/* write_faces */
				struct objex_f *fEnd = f;
				for (f = fStart; f < fEnd; ++f)
				{
					p = &gfx;
					
					if (fEnd - f == 1)
					{
						/* one triangle */
						int x, y, z;
						compbuf_compile();
						
						gSP1Triangle(p++, x, y, z, 0);
					}
					else
					{
						/* two triangles */
						int x0, y0, z0;
						int x, y, z;
						
						compbuf_compile();
						x0 = x;
						y0 = y;
						z0 = z;
						
						/*if (x == y || y == z || x == z)
						{
							fprintf(stderr, "invalid f %d %d %d\n", x, y, z);
						}*/
			
						++f;
						compbuf_compile();
						
						gSP2Triangles(p++, x0, y0, z0, 0, x, y, z, 0);
					}
					
					/* write commands into file */
					vfput32(bin, gfx.hi);
					vfput32(bin, gfx.lo);
				}
			}
			
			vbufCur += vtotal * 16;
			
			/* always clear these */
		L_jump2clear:
			vtotal = 0;
			fStart = f;
			return success;
		}
		
		/* write one group */
		void *oneGroup(struct objex_g *g)
		{
			/* every face */
			fStart = g->f;
			isEmpty = 0;
			for (f = g->f; f - g->f < g->fNum; ++f)
			{
				/* on material change, write zobj's usemtl equivalent */
				if (!g->noMtl && (f == g->f || f->mtl != f[-1].mtl))
				{
					if (f > fStart)
					{
						if (!binflush())
							return 0;
					}
					
					if (f->mtl && f->mtl->tex0)
						tex = f->mtl->tex0;
					else
						tex = 0;
					
					if (pass == 2)
					{
						/* restore limb matrix before branch if needed */
						if (f->mtl
							&& f->mtl->isEmpty
							&& prevLimb != matrixBone
							&& g->bone
						)
						{
							struct objex_bone *b = g->bone;
							struct boneUdata *bUdat;
							Gfx gfx;
							int limb;
							unsigned mtxaddr;
							
							if (!(bUdat = b->udata))
								return errmsg("bone '%s' unprocessed", b->name);
							
							limb = bUdat->matrixIndex;
							mtxaddr = b->skeleton->segment;
							mtxaddr += 0x40 * limb;
							gfx.hi = 0xDA380003;
							gfx.lo = mtxaddr;
							vfput32(bin, gfx.hi);
							vfput32(bin, gfx.lo);
							prevLimb = -1; /* just in case stacked DE's */
						}
						
						/* write material (or branch) */
						if (!zobj_writeUsemtl(bin, f->mtl))
							return 0;
					}
					
					if (f->mtl)
						isEmpty = f->mtl->isEmpty;
					else
						isEmpty = 0;
					
//					if (f->mtl)
//						debugf("'%s' = %d\n", f->mtl->name, f->mtl->isEmpty);
				}
				
				/* push triangle's vertices into vertex buffer */
				int x, y, z;
				int Ovtotal = vtotal;
				compbuf_compile();
				
				/* vbuf exceeded */
				if (vtotal > VBUF_MAX)
				{
					vtotal = Ovtotal;
					
					if (!binflush())
						return 0;
					
					/* retry this triangle next iteration */
					f--;
				}
			}
			if (f > fStart)
			{
				if (!binflush())
					return 0;
			}
			return g;
		} /* oneGroup */
		
		if (pass == 2)
		{
			unsigned int posMtxAddr = 0;
			
			/* has POSMTX attrib */
			if (g->attrib && strstr(g->attrib, "POSMTX"))
			{
				posMtxAddr = vftell(bin) + baseOfs;
				if (!posMtx(bin, g->origin.x, g->origin.y, g->origin.z))
					return 0;
			}
			
			/* print docs */
			// fprintf(docs, DOCS_DEF "DL_" DOCS_SPACE "   0x%08X\n"
			// 	, Canitize(g->name, 1)
			// 	, (int)vftell(bin) + baseOfs
			// );
			document_doc(
				g->name,
				NULL,
				(int)vftell(bin) + baseOfs,
				T_DL
			);
			debugf(" > '%s' address %08X\n", g->name, (int)vftell(bin) + baseOfs);
			
			/* has attributes */
			if (g->attrib)
			{
				/* world positioning matrix (must precede bbmtx)
				 * (ex: search 01000040 in Z2_TOWN_room_00.zmap) */
				if (strstr(g->attrib, "POSMTX"))
					vfput64s(bin, 0xDA380003, posMtxAddr);
				/* explicit limb billboard */
				if (strstr(g->attrib, "LIMBMTX"))
				{
					if (matrixBone < 0)
						return errmsg(
							"group '%s' incompatible with attrib LIMBMTX;"
							" is it a Pbody?"
							, g->name
						);
					vfput64s(
						bin
						, 0xDA380003
						, g->bone->skeleton->segment + 0x40 * matrixBone
					);
				}
				/* explicit spherical billboard */
				if (strstr(g->attrib, "BBMTXS"))
					vfput64s(bin, 0xDA380001, 0x01000000);
				/* explicit cylindrical billboard */
				if (strstr(g->attrib, "BBMTXC"))
					vfput64s(bin, 0xDA380001, 0x01000040);
			}
		}
		
		/* is not a Pbody, so we can write it this way */
		if (!isPbody)
		{
			if (!oneGroup(g))
				return 0;
			continue;
		}
		
		/* is a Pbody, so we recombine all skeleton groups */
		void *oneBone(struct objex_bone *b) {
			if (b->g && !oneGroup(b->g)) return 0;
			if (b->child && !oneBone(b->child)) return 0;
			if (b->next && !oneBone(b->next)) return 0;
			return b;
		}
		if (g->skeleton && !oneBone(g->skeleton->bone))
			return 0;
	} /* passes */
	
	/* generic display list written */
	if (forcePass == 0)/* end display list */{
		Gfx gfx;
		Gfx *p = &gfx;
		gSPEndDisplayList(p++);
		vfput32(bin, gfx.hi);
		vfput32(bin, gfx.lo);
	}
	
	/* proxy feature */
	if (g->attrib && strstr(g->attrib, "PROXY"))
	{
		unsigned int n;
		
		/* write proxy Dlist, change pointer to use it */
		n = vftell(bin);
		/* XXX the DExx01 indicates do not unnest this proxy later */
		vfput64s(bin, 0xDE010100, baseOfs + gUdata->dlistOffset);
		gUdata->dlistOffset = n;
		
		/* proxy docs */
		// fprintf(docs, DOCS_DEF "DL_" DOCS_SPACE "   0x%08X\n"
		// 	, CanitizeProxy(g->name, 1)
		// 	, n + baseOfs
		// );
		document_doc(
			g->name,
			NULL,
			n + baseOfs,
			T_DL
		);
	}
	
	return success;
}

void zobj_indexSkeleton(
	struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
)
{
	int i;
	int mI;
	
	/* initial pass: get skeleton indices, etc */
	i = mI = 0;
	writeBone(0, sk->bone, calloc, free, &i, &mI, 0);
}

uint32_t unnest_de(VFILE *bin, uint32_t offset, uint32_t baseOfs)
{
	uint32_t wow[4];
	uint32_t old = vftell(bin);
//	goto L_ret;
	
	/* invalid segment */
	if (offset < baseOfs || offset > baseOfs + bin->end)
		goto L_ret;
	
	vfseek(bin, offset - baseOfs, SEEK_SET);
	wow[0] = vfget32(bin);
	wow[1] = vfget32(bin);
	wow[2] = vfget32(bin);
	wow[3] = vfget32(bin);
	if ((wow[0] >> 24) != 0xde)
		goto L_ret;
	/* skip unnesting for DE xx 01 */
	if (wow[0] & 0x0000ff00)
		goto L_ret;
	if ((wow[0] >> 16) != 0xde01
		&& wow[2] != 0xdf000000
	)
		goto L_ret;
	vfseek(bin, old, SEEK_SET);
	return unnest_de(bin, wow[1], baseOfs);
L_ret:
	vfseek(bin, old, SEEK_SET);
	return offset;
}

/* walk a display list and unnest any DEs you find */
void zobjProxyArray__unnest_one(
	VFILE *bin
	, uint32_t dlist
	, uint32_t baseOfs
)
{
	uint32_t wow[2];
	size_t old = vftell(bin);
	
	/* invalid segment */
	if (dlist < baseOfs || dlist > baseOfs + bin->end)
		return;
	
	vfseek(bin, dlist - baseOfs, SEEK_SET);
	//fprintf(stderr, "sought %08X\n", vftell(bin));
	//int first = 0;
	while (1)
	{
		wow[0] = vfget32(bin);
		wow[1] = vfget32(bin);
		//if ((++first) < 5)
		//fprintf(stderr, " > %08X %08X\n", wow[0], wow[1]);
		
		if ((wow[0] >> 24) == 0xde)
		{
			uint32_t new;
			new = unnest_de(bin, wow[1], baseOfs);
			if (new != wow[1])
			{
				vfseek(bin, -4, SEEK_CUR);
				vfput32(bin, new);
			}
			zobjProxyArray__unnest_one(bin, wow[1], baseOfs);
		}
		
		if ((wow[0] >> 24) == 0xdf
			|| (wow[0] >> 16) == 0xde01
		)
			break;
	}
	//fprintf(stderr, "done\n");
	
	vfseek(bin, old, SEEK_SET);
}

void zobjProxyArray_unnest(
	VFILE *bin
	, struct zobjProxyArray *proxyArray
	, uint32_t baseOfs
)
{
	uint32_t *array = proxyArray->array;
	int i;
	
	/* TODO this whole setup was made to remedy something that
	 *      no longer needs remedied... */
	/* XXX  actually, it does need remedied still (TP Link's head
	 *      is missing w/o it because the DE's nest too deeply */
	//return;
	
	/* DE commands stop working after nesting
	 * deeply enough, so recursively unnest them!
	 */
	for (i = 0; i < proxyArray->num; ++i)
	{
		//fprintf(stderr, "unnest %08X\n", array[i]);
		zobjProxyArray__unnest_one(bin, array[i], baseOfs);
	}
	
	vfseek(bin, 0, SEEK_END);
}

void *zobjProxyArray_print(
	FILE *docs
	, VFILE *bin
	, struct zobjProxyArray *proxyArray
	, uint32_t baseOfs
)
{
	uint32_t *array = proxyArray->array;
	int i;
	/* DE commands stop working after nesting
	 * deeply enough, so we need a way to unnest them!
	 */
	/* write C version */
	fprintf(docs
		, "static const uint32_t proxy_%s[] = {"
		, Canitize(proxyArray->name, 0)
	);
	for (i = 0; i < proxyArray->num; ++i)
	{
		uint32_t unnested = unnest_de(bin, array[i], baseOfs);
		
		fprintf(docs, "\n\t""0x%08X, 0x%08X,"
			, 0xDE010000, unnested
		);
		
		if (!unnested)
		{
			fprintf(docs, " /* ERROR! */");
			/*return errmsg(
				"proxy array '%s' contains an element resolving to 0"
				, proxyArray->name
			);*/
		}
	}
	fprintf(docs, "\n};\n");
	
	return success;
}

void *zobj_writeSkeleton(
	VFILE *bin
	, struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
	, unsigned int *headerOffset
	, struct zobjProxyArray **proxyArray
)
{
	int numBones;
	int numMatrix;
	int index;
	int matrixIndex;
	unsigned headOfs;
	unsigned boneOfs;
	unsigned bonelistOfs;
	struct objex_g *g = sk->g;
	int stride = (sk->extra) ? !!strstr(sk->extra, "z64player") : 0;
	stride = 12 + stride * 4;
	FILE *docs = 0;
	
	if (!(docs = getDocs(sk->objex)))
		return 0;
	
	struct groupUdata *gUdata;
	if (!(gUdata = g->udata))
	{
		if (!(gUdata = groupUdata_new()))
			return errmsg(ERR_NOMEM);
		g->udata = gUdata;
	}
	else /* already allocated */
		return success;
	
	/* get baseOfs from objex */
	struct objex *objex = sk->objex;
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs;
	assert(objexUdata);
	baseOfs = objexUdata->baseOfs;
	
	/* initial pass: get skeleton indices, etc */
	index = matrixIndex = 0;
	if (!writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, 0))
		return 0;
	numBones = index;
	numMatrix = matrixIndex;
	
	/* not meant to write to file, so go no further */
	if (!bin)
		return success;
	
	/* second pass: write bone display lists */
	index = matrixIndex = -1;
	if (!writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, 0))
		return 0;
	
	/* optional pass: bone proxy */
	if (g->attrib && strstr(g->attrib, "PROXY") && bin)
	{
		enum x
		{
			WRITE_FILE     = 1
			, WRITE_TABLE  = 1 << 1
		};
	
		/* proxy docs */
		if (g->attrib && strstr(g->attrib, "NOSKEL") == 0)
			document_doc(
				g->name,
				NULL,
				(int)vftell(bin) + baseOfs,
				T_PROXY
			);
			// fprintf(docs, DOCS_DEF "PROXY_" DOCS_SPACE "0x%08X\n"
			// 	, Canitize(g->name, 1)
			// 	, (int)vftell(bin) + baseOfs
			// );
		
		int skproxyindex;
		void *skproxy(VFILE *fp, struct objex_bone *b, enum x x)
		{
			struct boneUdata *bUdata;
			if (!b)
				return success;
			
			/* get baseOfs from objex */
			struct objex *objex = b->skeleton->objex;
			struct objexUdata *objexUdata = objex->udata;
			unsigned int baseOfs;
			assert(objexUdata);
			baseOfs = objexUdata->baseOfs;
			
			if (b->g && b->g->name/*XXX sometimes groups don't have names*/)
			{
				struct objex_g *g = b->g;
				struct groupUdata *gUdata;
				char *attrib = g->attrib;
				unsigned int n;
				
				if (!(gUdata = g->udata))
					return errmsg("group '%s' missing udata", g->name);
				
				/* write proxy Dlist, change pointer to use it */
				if (bin && (x & WRITE_FILE))
				{
					n = vftell(bin);
					vfput64s(bin, 0xDE010000, baseOfs + gUdata->dlistOffset);
					gUdata->dlistOffset = n;
				
					/* proxy docs */
					document_doc(
						g->name,
						NULL,
						n + baseOfs,
						T_DL
					);
					// fprintf(docs, DOCS_DEF "DL_" DOCS_SPACE "   0x%08X\n"
					// 	, CanitizeProxy(g->name, 1)
					// 	, n + baseOfs
					// );
				}
				
				if (x & WRITE_TABLE)
				{
					uint32_t *array = (*proxyArray)->array;
					array[skproxyindex] = gUdata->dlistOffset + baseOfs;
					skproxyindex += 1;
#if 0
					uint32_t unnest(uint32_t offset)
					{
						uint32_t wow[4];
//						goto L_ret;
						vfseek(bin, offset, SEEK_SET);
						wow[0] = vfget32(bin);
						wow[1] = vfget32(bin);
						wow[2] = vfget32(bin);
						wow[3] = vfget32(bin);
						if ((wow[0] >> 24) != 0xde)
							goto L_ret;
						if ((wow[0] >> 16) != 0xde01
							&& wow[2] != 0xdf000000
						)
							goto L_ret;
						wow[1] -= baseOfs;
						vfseek(bin, 0, SEEK_END);
						return unnest(wow[1]);
					L_ret:
						vfseek(bin, 0, SEEK_END);
						return offset;
					}
					/* FIXME DE commands stop working after nesting
					 * deeply enough, so we need a way to unnest them!
					 */
					fprintf(docs, "\n\t""0x%08X, 0x%08X,"
						, 0xDE010000, gUdata->dlistOffset + baseOfs
					);
#endif
				}
			}
			
			/* write child if there is one */
			if (b->child && !skproxy(fp, b->child, x))
				return 0;
			
			/* write next if there is one */
			if (b->next && !skproxy(fp, b->next, x))
				return 0;
			
			return success;
		}
		
		/* prepare C version */
		if (!(*proxyArray = zobjProxyArray_new(g->name, numMatrix)))
			return 0;
		
#if 0
		/* write C version */
		fprintf(docs
			, "static const uint32_t proxy_%s[] = {"
			, Canitize(g->name, 0)
		);
#endif
		/* skeleton riggedmesh proxy table */
		skproxyindex = 0;
		skproxy(bin, sk->bone, WRITE_TABLE);
		skproxyindex = 0;
#if 0
		fprintf(docs, "\n};\n");
#endif
		
		/* write macro definitions */
		skproxy(bin
			, sk->bone
			/* only write to binary if a skeleton will point to them */
			, (strstr(g->attrib, "NOSKEL")) ? 0 : WRITE_FILE
		);
	}
	
	/* if NOSKEL attrib present, you can go ahead and bail out here */
	if (g->attrib && strstr(g->attrib, "NOSKEL"))
		return success;
	
	/* final pass: write bones to file */
	boneOfs = vftell(bin);
	index = matrixIndex = -2;
	if (!writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, 0))
		return 0;
	
	/* bone list */
	bonelistOfs = vftell(bin);
	for (int i = 0; i < numBones; ++i)
		vfput32(bin, boneOfs + baseOfs + stride * i);
	
	/* write header offset to docs */
	headOfs = vftell(bin);
	if (headerOffset)
		*headerOffset = headOfs;
//	fprintf(docs
//		, "#define %s 0x%08X /* %s */\n"
//		, sk->name, baseOfs + headOfs, sk->name
//	);
	document_doc(
		sk->g->name,
		NULL,
		(int)baseOfs + headOfs,
		T_SKEL
	);
	// fprintf(docs, DOCS_DEF "SKEL_" DOCS_SPACE " 0x%08X\n"
	// 	, Canitize(sk->g->name, 1)
	// 	, (int)baseOfs + headOfs
	// );
	document_doc(
		sk->g->name,
		"NUMBONES",
		numBones,
		T_SKEL
	);
	// fprintf(docs, DOCS_DEF "SKEL_" DOCS_SPACE_2 " %d\n"
	// 	, Canitize(sk->g->name, 1)
	// 	, "NUMBONES"
	// 	, numBones
	// );
	document_doc(
		sk->g->name,
		"NUMBONES_DT",
		numBones,
		T_SKEL
	);
	// fprintf(docs, DOCS_DEF "SKEL_" DOCS_SPACE_2 " (%d+1)\n"
	// 	, Canitize(sk->g->name, 1)
	// 	, "NUMBONES_DT"
	// 	, numBones
	// );
	
	/* note header offset for later */
	gUdata->dlistOffset = headOfs;
	
	/* write header to file */
	/* XXX you could save 4 bytes by not writing the matrix limb count
	 * on skeletons that don't use that field
	 */
	vfput32(bin, bonelistOfs + baseOfs);
	vfput32(bin, numBones << 24);
	vfput32(bin, numMatrix << 24);
	
	return sk;
}

void *zobj_writePbody(
	VFILE *bin
	, struct objex_skeleton *sk
	, void *calloc(size_t, size_t)
	, void free(void *)
)
{
	int index;
	int matrixIndex;
	unsigned int vdata;
	struct objex_g *g = sk->g;
	
	/* initial pass: get skeleton indices, etc */
	index = matrixIndex = 0;
	if (!writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, 0))
		return 0;
	
	/* this stuff fixes Old_ZZ2020_Packed */
	if (!sk->isPbody) /* is necessary or actual Pbodies get broken */
	{
		if (sk->parent)
			g->bone = sk->parent;
		else
		{
			struct objex *objex = sk->objex;
			struct objex_v *one = objex->v + g->f->v.x;
			if (one->weight && one->weight->bone)
				g->bone = one->weight->bone;
		}
	}
//	fprintf(stderr, "pbody '%s'\n", g->bone->name);
	/* end fix */
	
	zobj_writeDlist(bin, g, calloc, free, 0);
	((struct groupUdata*)g->udata)->isPbody = 1;
	
	/* write local identity matrices */
	if (sk->isPbody)
	{
		void *walk(struct objex_bone *b)
		{
			struct boneUdata *bUdat;
			if (!(bUdat = b->udata))
				return errmsg("pBody parent unprocessed");
			int limb = bUdat->matrixIndex;
			unsigned mtxaddr = b->skeleton->segment;
			mtxaddr += 0x40 * limb;
			
			if (!localMatrix(bin, b, mtxaddr))
				return 0;
			
			if (b->child
				&& !walk(b->child)
			) return 0;
			
			if (b->next
				&& !walk(b->next)
			) return 0;
			
			return success;
		}
		walk(sk->bone);
		/* write identity matrices */
	//	localMatrix(bin, sk, mtxaddr, limb);
	}
	
	return success;
#if 0
	/* second pass: write bone vertex data as one block */
	index = matrixIndex = -1;
	vdata = vftell(bin);
	writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, baseOfs, 1);
	
	/* third pass: write bone meshes as one display list */
	index = matrixIndex = -1;
	writeBone(bin, sk->bone, calloc, free, &index, &matrixIndex, baseOfs + vdata, 2);
	
	/* end display list */
	{
		Gfx gfx;
		Gfx *p = &gfx;
		gSPEndDisplayList(p++);
		vfput32(bin, gfx.hi);
		vfput32(bin, gfx.lo);
	}
	
	return sk;
#endif
}

void *zobj_writeStdAnim(
	VFILE *bin
	, struct objex_animation *anim
	, int one_animated_skeleton
)
{
	FILE *docs;
/* from old zzconvert */
#define ToDegrees(angleRadians) (angleRadians*180.0f/M_PI)
#define ROTATION_CONSTANT 182.044444444
	
	/* animation data must be 4-byte aligned */
	vfalign(bin, 4);
	
	struct objex *objex = anim->sk->objex;
	unsigned int baseOfs;
	baseOfs = ((struct objexUdata *)(objex->udata))->baseOfs;
	
	if (!(docs = getDocs(objex)))
		return 0;
	
	uint32_t pal_ofs = vftell(bin);
	uint16_t limit;
	uint32_t list_ofs;
	int f;
	int l;
	int pal_len = 0;
	
	int limbs = anim->sk->boneNum;
	int frames = anim->frameNum;
	struct objex_frame *frame = anim->frame;
	
	uint8_t xT_same = 1;
	uint8_t yT_same = 1;
	uint8_t zT_same = 1;
	
	struct
	{
		uint16_t x, y, z;
	} bonerot[256];
	
	struct
	{
		uint8_t x, y, z;
	} boneSame[256];
	
	/* worst case scenario: maxbones * 3(xyz) + 3(xyzLoc) u16 slots */
	/* going with 256 * 4 instead though */
	uint16_t pal[256 * 4/*256 * 3 + 3*/];
	
	/* set same = 1 on all until we check that they aren't the same */
	memset(boneSame, 1, sizeof(boneSame));
	
	short xformLoc(float v)
	{
		return (short)round(v);
	}
	
	uint16_t xformRot(float v)
	{
		v = ToDegrees(v);
		while(v < 0)
			v += 360;
		v = fmod(v, 360);
		v *= ROTATION_CONSTANT;
		return round(v);
	}
	
	uint16_t exist_list_u16(uint16_t *hay, int *total, const uint16_t ndl)
	{
		int i;
		for (i = 0; i < *total; i++)
			if (hay[i] == ndl)
				return i;
		
		hay[i] = ndl;
		*total += 1;
		return i;
	}
	
	/* get rotation of each bone on first frame */
	for (l = 0; l < limbs; l++)
	{
		bonerot[l].x = xformRot(frame[0].rot[l].x);
		bonerot[l].y = xformRot(frame[0].rot[l].y);
		bonerot[l].z = xformRot(frame[0].rot[l].z);
	}
	
	/* get location of entire skeleton on first frame */
	short xT = xformLoc(frame[0].pos.x);
	short yT = xformLoc(frame[0].pos.y);
	short zT = xformLoc(frame[0].pos.z);
	
	/* which translation values remain the same for every frame? */
	// X test
#define QUICKTEST(MEMB, XT, XTSAME) \
	for (f = 1; f < frames; f++) \
	{ \
		if (xformLoc(frame[f].pos.MEMB) != XT) \
		{ \
			XTSAME=0; \
			break; \
		} \
	} \
	if (XTSAME) \
		XT = exist_list_u16(pal, &pal_len, XT);
	QUICKTEST(x, xT, xT_same)
	QUICKTEST(y, yT, yT_same)
	QUICKTEST(z, zT, zT_same)
#undef QUICKTEST
	
	/* which rotations remain the same on all axes for every frame? */
	for (f = 1; f < frames; f++)
	{
		for (l = 0 ; l < limbs; l++)
		{
			uint16_t v;
			
#define QUICKTEST(MEMB) \
			v = xformRot(frame[f].rot[l].MEMB); \
			if (v != bonerot[l].MEMB) \
				boneSame[l].MEMB = 0;
			QUICKTEST(x)
			QUICKTEST(y)
			QUICKTEST(z)
#undef QUICKTEST
		}
	}
	for (l = 0 ; l < limbs; l++)
	{
#define QUICKTEST(MEMB) \
		if (boneSame[l].MEMB) \
			bonerot[l].MEMB=exist_list_u16(pal, &pal_len, bonerot[l].MEMB);
		QUICKTEST(x)
		QUICKTEST(y)
		QUICKTEST(z)
#undef QUICKTEST
	}
	
	/* you write `pal` to file here */
	for (int i = 0; i < pal_len; ++i)
	{
//		if (i < 0x40 / 2) fprintf(stderr, "pal[%d] = %04X\n", i, pal[i]);
		vfput16(bin, pal[i]);
	}
	limit = pal_len;
	
// now go and write all the interlaced data
	/* interlaced translations for each frame */
	if (!xT_same)
	{
		xT = (vftell(bin) - pal_ofs) / 2;
		for (f = 0; f < frames; f++)
			vfput16(bin, frame[f].pos.x); // X Loc
	}
	if (!yT_same)
	{
		yT = (vftell(bin) - pal_ofs) / 2;
		for (f = 0; f < frames; f++)
			vfput16(bin, frame[f].pos.y); // Y Loc
	}
	if (!zT_same)
	{
		zT = (vftell(bin) - pal_ofs) / 2;
		for (f = 0; f < frames; f++)
			vfput16(bin, frame[f].pos.z); // Z Loc
	}
	/* interlaced rotations for each bone */
	for (l = 0 ; l < limbs; l++)
	{
		// WAS casting to short, but don't! these aren't negative anyway
		// and casting to short causes problems on Windows!
		if (!boneSame[l].x)
		{
			bonerot[l].x = (vftell(bin) - pal_ofs) / 2;
			for (f = 0; f < frames; f++)
				vfput16(bin, xformRot(frame[f].rot[l].x)); // X Rot
		}
		if (!boneSame[l].y) {
			bonerot[l].y = (vftell(bin) - pal_ofs) / 2;
			for (f = 0; f < frames; f++)
				vfput16(bin, xformRot(frame[f].rot[l].y)); // Y Rot
		}
		if (!boneSame[l].z) {
			bonerot[l].z = (vftell(bin) - pal_ofs) / 2;
			for (f = 0; f < frames; f++)
				vfput16(bin, xformRot(frame[f].rot[l].z)); // Z Rot
		}
	}
	
	/* now write the index ("texture" part) */
	vfalign(bin, 4);
	list_ofs = vftell(bin);
	vfput16(bin, xT); // X Loc
	vfput16(bin, yT); // X Loc
	vfput16(bin, zT); // X Loc
	for (l = 0 ; l < limbs; l++)
	{
		vfput16(bin, bonerot[l].x); // X Rot
		vfput16(bin, bonerot[l].y); // Y Rot
		vfput16(bin, bonerot[l].z); // Z Rot
	}
	
	/* TODO warning 3Q old man */
//	char warning[32]={0};
//	if (list_ofs-pal_ofs>5*1024)
//		sprintf(warning," /* warning: A000 */");
//	OUTPUT_POINTER("ANIM_",obj->anim[a].name,warning,len)
	
	/* write animation header */
	vfalign(bin, 4);
	
	/* use extended form only when multiple
	 * animations with differing skeletons detected
	 */
//	debugf("anim %08X : %s\n", (int)vftell(bin), anim->name);
	if (one_animated_skeleton)
	{
		document_doc(
			anim->name,
			NULL,
			(int)vftell(bin) + baseOfs,
			T_ANIM
		);
		// fprintf(docs, DOCS_DEF "ANIM_" DOCS_SPACE " 0x%08X\n"
		// 	, Canitize(anim->name, 1)
		// 	, (int)vftell(bin) + baseOfs
		// );
	}
	
	/* multiple animated skeletons */
	else
	{
		char skelName[1024];
		strcpy(skelName, Canitize(anim->sk->name, 1));
		document_doc(
			skelName,
			anim->name,
			(int)vftell(bin) + baseOfs,
			T_ANIM
		);
		// fprintf(docs, DOCS_DEF "ANIM_" DOCS_SPACE_2 " 0x%08X\n"
		// 	, skelName
		// 	, Canitize(anim->name, 1)
		// 	, (int)vftell(bin) + baseOfs
		// );
	}
	vfput16(bin, frames);
	vfput16(bin, 0);
//	c_zobj_list_add(vftell(bin)); /* relocation data for C embedding */
	vfput32(bin, pal_ofs + baseOfs);
//	c_zobj_list_add(vftell(bin));
	vfput32(bin, list_ofs + baseOfs);
	vfput16(bin, limit); // "limit", we won't be needing this
	vfput16(bin, 0);
	
	return success;
}

void *zobj_writeStdAnims(VFILE *bin, struct objex *objex)
{
	struct objex_animation *anim;
	struct objex_skeleton *sk;
	int one_animated_skeleton = 1;
	
	if (!objex->anim)
		return success;
	
	/* see if all animations use same skeleton */
	sk = objex->anim->sk;
	for (anim = objex->anim; anim; anim = anim->next)
	{
		if (anim->sk != sk)
			one_animated_skeleton = 0;
	}
	
	for (anim = objex->anim; anim; anim = anim->next)
	{
		if (!zobj_writeStdAnim(bin, anim, one_animated_skeleton))
			return 0;
	}
	return success;
}

int zobj_g_written(struct objex_g *g)
{
	if (!g)
		return 1;
	
	if (!g->udata)
		return 0;
	
	struct groupUdata *udata = g->udata;
	
	if (udata->hasWritten)
		return 1;
	
	return 0;
}

/* write play-as data */
void *zobj_doPlayAsData(VFILE *bin, struct objex *objex)
{
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs;
	int total = 0;
	size_t total_ofs;
	
	/* we don't need relocs */
	if (!objexUdata)
		return success;
	
	baseOfs = objexUdata->baseOfs;
	
	debugf("play-as data\n");
	
	/* count total entries to write */
	for (struct objex_g *g = objex->g; g; g = g->next)
	{
		struct groupUdata *udata = g->udata;
		
		if (!udata || (g->hasSplit && !udata->isPbody))
			continue;
		
		total++;
	}
	
	vfalign(bin, 16);
	vfwrite("!PlayAsManifest0", 1, 16, bin);
	total_ofs = vftell(bin);
	vfput16(bin, 0);
	
	/* for each group */
	for (struct objex_g *g = objex->g; g; g = g->next)
	{
		struct groupUdata *udata = g->udata;
		
//		if (!udata || (g->hasSplit && !udata->isPbody))
		if (!udata)
			continue;
		
		/* has split up and isn't Pbody, it is skeleton header */
		if (g->hasSplit && !udata->isPbody)
		{
			//debugf("!!! SKELETON '%s' !!!\n", g->name);
			/* we don't use + 1 here, b/c it goes sk.gname\0ofs */
			vfwrite("skeleton.", 1, strlen("skeleton."), bin);
			++total; /* skeleton.riggedmesh, etc */
		}
		
		debugf(" > '%s' 0x%X\n"
			, g->name
			, (udata->dlistOffset + baseOfs) & 0xFFFFFF
		);
		
		vfwrite(g->name, 1, strlen(g->name) + 1, bin);
		vfput32(bin, (udata->dlistOffset + baseOfs) & 0xFFFFFF);
	}
	
	/* write this info for zzplayas (used by OoTO) */
	if (objexUdata->localMatrixBlock.start)
	{
		const char *start = "--localMatrixBlock.start";
		const char *end = "--localMatrixBlock.end";
		
		vfwrite(start, 1, strlen(start) + 1, bin);
		vfput32(bin, objexUdata->localMatrixBlock.start);
		
		vfwrite(end, 1, strlen(end) + 1, bin);
		vfput32(bin, objexUdata->localMatrixBlock.end);
		
		total += 2;
	}
	
	/* write this info for zzplayas (pool safety) */
	if (objexUdata->earlyPool.end)
	{
		const char *start = "--earlyPool.start";
		const char *end = "--earlyPool.end";
		
		vfwrite(start, 1, strlen(start) + 1, bin);
		vfput32(bin, objexUdata->earlyPool.start);
		
		vfwrite(end, 1, strlen(end) + 1, bin);
		vfput32(bin, objexUdata->earlyPool.end);
		
		total += 2;
	}
	
	/* meta total */
	vfput16(bin, 0);
#if 0 /* old code looks like */
	for(a=0;a<obj->meta_total;a++) {
		objmeta_t *m=obj->meta+a;
		U32wr(data+len, m->type); len+=4;
		U32wr(data+len, m->ofs); len+=4;
		U32wr(data+len, m->span); len+=4;
		char *ss=m->name;
		memcpy(data+len,ss,strlen(ss)+1); len+=strlen(ss)+1; // skip text and 0
	}
#endif
	
	/* end of footer (if not aligned by 16) */
	if (vftell(bin) & 15)
	{
		int x = 16 - (vftell(bin) & 15);
		vfput8(bin, x);
		/* still unaligned */
		if (vftell(bin) & 15)
			vfwrite("!EndPlayAsManifest!", 1, 16 - (vftell(bin) & 15), bin);
	}
	
	/* write updated total */
	vfseek(bin, total_ofs, SEEK_SET);
	vfput16(bin, total);
	
	/* return to end of file */
	vfseek(bin, 0, SEEK_END);
	
	return success;
}

void *zobj_doIdentityMatrices(VFILE *bin, struct objex *objex)
{
#if 0
	if (!bin || !objex || !objex->udata)
		return success;
	
	const char identity[] = {
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	
	/* XXX
	 * this function isn't used b/c we just write identity
	 * matrices where appropriate while writing the binary
	 */
	
	/* return to end of file before exiting function */
	vfseek(bin, 0, SEEK_END);
#endif
	return success;
}

void *zobj_doRelocs(VFILE *bin, struct objex *objex)
{
#define LOCALDIE(...) { \
	vfseek(bin, 0, SEEK_END); \
	return errmsg(__VA_ARGS__); \
}
	struct objexUdata *objexUdata = objex->udata;
	unsigned int baseOfs;
	
	/* we don't need relocs */
	if (!objexUdata)
		return success;
	
	baseOfs = objexUdata->baseOfs;
	
	/* do group relocs */
	for (struct objex_g *g = objex->g; g; g = g->next)
	{
		struct groupUdata *udata = g->udata;
		
		if (!udata)
			continue;
		
		/* contains relocs but hasn't been written */
		if (udata->reloc && !zobj_g_written(g))
			LOCALDIE(
			"group '%s' referenced despite its exclusion from build recipe"
			, g->name
			);
		
		for (struct reloc *r = udata->reloc; r; r = r->next)
		{
			vfseek(bin, r->offset, SEEK_SET);
			vfput32(bin, udata->dlistOffset + baseOfs);
			
//			debugf("reloc %08X written\n", r->offset);
		}
	}
	
	/* return to end of file before exiting function */
	vfseek(bin, 0, SEEK_END);
	
	return success;
#undef LOCALDIE
}

const char *zobj_errmsg(void)
{
	return error_reason;
}

