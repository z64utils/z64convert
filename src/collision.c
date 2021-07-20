/* <z64.me> collision.c: for functions relating to collision meshes */
#include <string.h>
#include <stdint.h>

#include "objex.h"
#include "put.h"
#include "err.h"
#include "collision.h"
#include "collision_flags.h"
#include <limits.h>
#include <math.h>

#define MAX(A, B) ((A > B) ? A : B )

#define MAXVERTS 8192 /* max verts in vertex buffer */

//static void *polytype_derive(const char *name_, struct polytype *dst)
static void *polytype_derive(struct objex_f *face, struct polytype *dst)
{
	const char *name_ = 0;
	if (face->mtl && face->mtl->attrib)
		name_ = face->mtl->attrib;
	if (!name_)
	{
		memset(dst, 0, sizeof(*dst));
		return success;
	}
	char name[1024];
	uint32_t hi=0, lo=0;
	uint16_t v1=0, v2=0, v3=0;
	int type = -1;
	int i;
	struct collision_flag *f = collision_flag;
	if (strlen(name_) + 1 >= sizeof(name))
		return errmsg("attribs '%.*s' too long", 64, name_);
//	debugf("attrib list '%s'\n", name_);
	for (i = 0; i < collision_flag_count; ++i, ++f)
	{
		strcpy(name, name_);
		/* if no string length defined, use big number for length */
		if (!f->name_numchar)
			f->name_numchar = -1;
		/* TODO use strtok_r instead */
		/* NOTE: '-' excluded b/c it is used for angles < 0 */
		const char *delim = " .,|#:;<>~/\\\r\n";
		for (char *tok = strtok(name, delim); tok; tok = strtok(0, delim))
		{
//			debugf("'%s' vs '%s'\n", tok, f->name);
			/* skip mismatches */
			if (strncmp(tok, f->name, f->name_numchar))
				continue;
//			debugf("match '%s'\n", f->name);
			int a0, a1, a2, invalid = 0;
			char *arg_start = tok + f->name_numchar;
			uint32_t lsh = MAX(f->hi, f->lo);
			uint32_t *w = ( f->hi > f->lo ) ? &hi : &lo;
			uint32_t and = f->and;
			switch (f->type)
			{
				case CF_WATERBOX:
					type = CF_WATERBOX;
					invalid = sscanf(arg_start, "_%d_%d_%d", &a0, &a1, &a2);
					if (invalid >= 2)
					{
						dst->hi = (a0 & 0x1F) << 8; // underwater light id 1F00
						dst->hi |= (a1 & 0xFF); // underwater camera id 00FF
						
						// optional room id
						if (invalid >= 3)
						{
							dst->hi |= (a2 & 0x3F) << 13;
						}
						else
						{
							// max value
							// active in all rooms by default
							dst->hi |= 0x0007E000;
						}
					} else {
						// default waterbox active in all rooms
						dst->hi = 0x0007E000;
					}
					invalid = 0;
					break;
				case CF_IGNORE:
					v1 |= f->hi;
					break;
				case CF_WARP:
					/* TODO WARP_scene_entrance_etc */
					invalid = sscanf(arg_start, "%d", &a0) != 1;
					a0 += 1;
					if (a0 <= 0)
						return errmsg("warp < 0 means off; use value >= 0");
					else if (a0 > 15)
						return errmsg("warp > 15 (exceeds maximum");
					*w |= (a0 << lsh) & and;
					break;
				case CF_CAMERA:
				case CF_ECHO:
				case CF_LIGHTING:
					invalid = sscanf(arg_start, "%d", &a0) != 1;
					*w |= (a0 << lsh) & and;
					break;
				case CF_CONVEYOR:
					v2 |= 0x2000; // set bit to enable conveyor surfaces
					invalid = sscanf(arg_start, "%d_%d", &a1, &a0) != 2;
					// test speed range
					if (a0 > 3)
						return errmsg(
							"collision type '%s' invalid 'speed'\n"
							" (valid values are 0, 1, 2, 3)", tok
						);

					// clamp to between 0 and 360, positive
					if (a1 < 0) {
						a1 = -a1;
						a1 %= 360;
						a1 = -a1;
					} else if (a1 >= 360)
						a1 %= 360;
					// then convert to a value 0 - 63
					a1 = round( (float)(a1 * 63) / 360.0f );
					if (a1 > 0x3F)
						a1 = 0x3F;

					// inheritance flag, and apply other flags
					if (strstr(tok, "INHERIT"))
						*w |= 4 << lsh;
					*w |= a0 << lsh; // speed
					*w |= a1 << (lsh + 3); // direction
					break;
				default:
					hi |= f->hi;
					lo |= f->lo;
//					debugf(" > generic\n");
//					debugf(" > %08X %08X\n", hi, lo);
					break;
			}
			if (invalid)
				return errmsg(
				"collision attribute '%s' incomplete arguments (see manual)"
				, tok
				);
//			debugf("%s\n", f->name);
		}
	}
	dst->hi = hi;
	dst->lo = lo;
	dst->v1 = v1;
	dst->v2 = v2;
	dst->v3 = v3;
	dst->type = type;
	
	return success;
}

/* sort objex_f by _pvt member */
static int f_sort_mtl(const void *a, const void *b)
{
	const struct objex_f *f0 = a, *f1 = b;
	
	return ((intptr_t)f0->mtl) - ((intptr_t)f1->mtl);
}

static inline float min4(float a, float b, float c, float d)
{
	return fmin(fmin(a, b), fmin(c, d));
}

static inline float max4(float a, float b, float c, float d)
{
	return fmax(fmax(a, b), fmax(c, d));
}

/* get boundaries of connected triangles */
static void tri_touch_bounds(
	struct objex_v *v
	, struct objex_f *arr
	, struct objex_f *cur
	, struct boundingBox *stats
	, struct objex_material *mtl
	, int arrNum
)
{
	/* mark as processed so we don't process it again */
	cur->_pvt = stats;
	
	struct objex_f *iter;
	for (iter = arr; iter - arr < arrNum; ++iter)
	{
		/* skip if previously processed */
		if (iter->_pvt)
			continue;
		
		/* test for shared vertices */
		if (iter->v.x == cur->v.x
			|| iter->v.x == cur->v.y
			|| iter->v.x == cur->v.z

			|| iter->v.y == cur->v.y
			|| iter->v.z == cur->v.z

			|| iter->v.y == cur->v.z
		)
		{
#define DO_BOUNDS(MIN_X, X, FUNC) \
	stats->MIN_X = FUNC( \
		stats->MIN_X \
		, v[iter->v.x].X \
		, v[iter->v.y].X \
		, v[iter->v.z].X \
	)
			DO_BOUNDS(min_x, x, min4);
			DO_BOUNDS(min_y, y, min4);
			DO_BOUNDS(min_z, z, min4);
			
			DO_BOUNDS(max_x, x, max4);
			DO_BOUNDS(max_y, y, max4);
			DO_BOUNDS(max_z, z, max4);
#undef DO_BOUNDS
			/* a triangle sharing vertices with the current *
			 * triangle has been found; add it to the list  *
			 * and recursively test for any that it touches */
			tri_touch_bounds(v, arr, iter, stats, mtl, arrNum);
		}
	}
}

void *collision_init(struct objex_g *g)
{
	if (!g)
		return success;
	
	/* clear all these to 0 */
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
		f->_pvt = 0;
	
	return success;
}

struct boundingBox *collision_bounds(
	struct objex_g *g
	, struct objex_material *mtl
)
{
	static struct boundingBox bounds;
	struct objex_v *v = g->objex->v;
	
	/* walk every triangle */
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
	{
		if (f->_pvt)
			continue;
		const struct boundingBox stats = {
			.min_x = SHRT_MAX, .max_x = SHRT_MIN
			, .min_y = SHRT_MAX, .max_y = SHRT_MIN
			, .min_z = SHRT_MAX, .max_z = SHRT_MIN
			, .boneIndex = -1
		};
		bounds = stats;
		if (v[f->v.x].weight && v[f->v.x].weight->bone)
			bounds.boneIndex = v[f->v.x].weight->bone->index;
		tri_touch_bounds(v, g->f, f, &bounds, mtl, g->fNum);
		return &bounds;
	}
	
	return 0;
}

void *collision_write(
	VFILE *bin
	, struct objex_g *g
	, unsigned baseOfs
	, unsigned *headOfs
)
{
#define FAIL(...) { \
   if (vbuf) free(vbuf); \
   return errmsg(__VA_ARGS__); \
}
	if (!g)
		return success;
	
	struct header {
		unsigned vbuf_ofs;
		unsigned vbuf_num;
		unsigned ctype_ofs;
		unsigned ctype_num;
		unsigned water_ofs;
		unsigned water_num;
		unsigned tri_ofs;
		unsigned tri_num;
		unsigned ofs;
	} header = {0};
	
	struct boundingBox world = {
		.min_x = SHRT_MAX, .max_x = SHRT_MIN
		, .min_y = SHRT_MAX, .max_y = SHRT_MIN
		, .min_z = SHRT_MAX, .max_z = SHRT_MIN
	};
	
	struct xyz16 {
		short x;
		short y;
		short z;
	} *vbuf = 0, *vert;
	
	struct objex *objex = g->objex;
	struct objex_v *v = objex->v;
	int vbufNum = 0;
	
	struct xyz16 *vbuf_push(struct objex_g *g, float x_, float y_, float z_)
	{
		short x = round(x_ - g->origin.x);
		short y = round(y_ - g->origin.y);
		short z = round(z_ - g->origin.z);
		for (vert = vbuf; vert - vbuf < vbufNum; ++vert)
		{
			if (vert->x == x && vert->y == y && vert->z == z)
				return vert;
		}
		if (vert - vbuf == MAXVERTS)
			return errmsg("collision group '%s' too many vertices"
				, g->name);
		vert->x = x;
		vert->y = y;
		vert->z = z;
		vbufNum += 1;
		return vert;
	}
	
	/* zero-initialize every triangle's '_pvt' member */
	if (!collision_init(g))
		FAIL(0);
	
	/* sort triangles by material */
	qsort(g->f, g->fNum, sizeof(*g->f), f_sort_mtl);
	
	if (!(vbuf = malloc(MAXVERTS * sizeof(*vbuf))))
		FAIL(ERR_NOMEM);
	
	/* construct vertex buffer */
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
	{
		if (!vbuf_push(g, v[f->v.x].x, v[f->v.x].y, v[f->v.x].z)
			|| !vbuf_push(g, v[f->v.y].x, v[f->v.y].y, v[f->v.y].z)
			|| !vbuf_push(g, v[f->v.z].x, v[f->v.z].y, v[f->v.z].z)
		)
			FAIL(0);
	}
	
	/* construct and write collision types */
	/* (also asserts every triangle has material and attribute) */
	header.ctype_ofs = vfalign(bin, 4);
	header.ctype_num = 0;
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
	{
		/* different settings */
		if (f == g->f || f->mtl != f[-1].mtl)
		{
			struct polytype pt;
			
			/*if (!f->mtl)
				FAIL(
				"collision group '%s' contains triangles without materials"
				, g->name
				);
			
			if (!f->mtl->attrib)
				FAIL(
				"collision material '%s' (in group '%s') missing attributes"
				, f->mtl->name, g->name
				);*/
			
			if (!polytype_derive(f, &pt))
				FAIL(0);
			
			/* skip waterbox types */
			if (pt.type == CF_WATERBOX)
				continue;
			
			vfput32(bin, pt.hi);
			vfput32(bin, pt.lo);
			header.ctype_num++;
		}
		
		/* calculate world bounding box */
#define DO_BOUNDS(MIN_X, X, FUNC) \
	world.MIN_X = FUNC( \
		world.MIN_X \
		, v[f->v.x].X \
		, v[f->v.y].X \
		, v[f->v.z].X \
	)
		DO_BOUNDS(min_x, x, min4);
		DO_BOUNDS(min_y, y, min4);
		DO_BOUNDS(min_z, z, min4);
		
		DO_BOUNDS(max_x, x, max4);
		DO_BOUNDS(max_y, y, max4);
		DO_BOUNDS(max_z, z, max4);
#undef DO_BOUNDS
	}
	
	/* construct and write waterboxes */
	header.water_ofs = vfalign(bin, 4);
	header.water_num = 0;
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
	{
		/* different settings */
		if (f == g->f || f->mtl != f[-1].mtl)
		{
			struct polytype pt;
			struct boundingBox *bounds;
			
			if (!polytype_derive(f, &pt))
				FAIL(0);
			
			/* skip non-waterbox types */
			if (pt.type != CF_WATERBOX)
				continue;
			
			/* while there are collections of connected
			 * triangles that utilize this material */
			while ((bounds = collision_bounds(g, f->mtl)))
			{
				short x = round(bounds->min_x);
				short y = bounds->min_y;
				short z = round(bounds->min_z);
				short x_len = round(bounds->max_x - bounds->min_x);
				short z_len = round(bounds->max_z - bounds->min_z);
				/* write and increment waterbox */
				vfput16(bin, x);         /* 0x00 */
				vfput16(bin, y);         /* 0x02 */
				vfput16(bin, z);         /* 0x04 */
				vfput16(bin, x_len);     /* 0x06 */
				vfput16(bin, z_len);     /* 0x08 */
				vfput16(bin, 0);         /* 0x0A */
				vfput32(bin, pt.hi);     /* 0x0C */
				header.water_num++;     /* 0x10 */
			}
		}
	}
	
	/* construct and write collision triangles */
	header.tri_ofs = vfalign(bin, 4);
	header.tri_num = 0;
	header.ctype_num = 0; /* reset counter */
	int is_water = 0;
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
	{
		static struct polytype pt = {0};
		short type;
		short a, b, c;
		short x, y, z, w;
		int is_double_sided = (f->mtl && f->mtl->attrib)
			? strstr(f->mtl->attrib, "DOUBLE_SIDED") != 0
			: 0
		;
		
		/* different settings */
		if (f == g->f || f->mtl != f[-1].mtl)
		{
			if (!polytype_derive(f, &pt))
				FAIL(0);
			
			/* waterbox */
			is_water = pt.type == CF_WATERBOX;
			
			/* increment collision type only if it isn't a waterbox */
			if (!is_water)
				header.ctype_num++;
		}
		
		/* skip waterbox triangles */
		if (is_water)
			continue;
		
		/* construct triangle */
		type = header.ctype_num - 1;
		a = vbuf_push(g, v[f->v.x].x, v[f->v.x].y, v[f->v.x].z) - vbuf;
		b = vbuf_push(g, v[f->v.y].x, v[f->v.y].y, v[f->v.y].z) - vbuf;
		c = vbuf_push(g, v[f->v.z].x, v[f->v.z].y, v[f->v.z].z) - vbuf;
	
		/* calculate xyzw (from spinout's FixCollision()) */
		void fix_collision(short a, short b, short c)
		{
			int v1, v2, v3;
			int p1[3], p2[3], p3[3];
			int dx[2], dy[2], dz[2];
			int dn, i;
			int ni[3];
			float nf[3], uv[3];
			float nd;
			
			v1 = a;
			v2 = b;
			v3 = c;
			
			p1[0] = vbuf[v1].x;
			p1[1] = vbuf[v1].y;
			p1[2] = vbuf[v1].z;
			
			p2[0] = vbuf[v2].x;
			p2[1] = vbuf[v2].y;
			p2[2] = vbuf[v2].z;
			
			p3[0] = vbuf[v3].x;
			p3[1] = vbuf[v3].y;
			p3[2] = vbuf[v3].z;
			
			dx[0] = p1[0] - p2[0]; dx[1] = p2[0] - p3[0];
			dy[0] = p1[1] - p2[1]; dy[1] = p2[1] - p3[1];
			dz[0] = p1[2] - p2[2]; dz[1] = p2[2] - p3[2];

			ni[0] = (dy[0] * dz[1]) - (dz[0] * dy[1]);
			ni[1] = (dz[0] * dx[1]) - (dx[0] * dz[1]);
			ni[2] = (dx[0] * dy[1]) - (dy[0] * dx[1]);
			
			nf[0] = (float)(dy[0] * dz[1]) - (dz[0] * dy[1]);
			nf[1] = (float)(dz[0] * dx[1]) - (dx[0] * dz[1]);
			nf[2] = (float)(dx[0] * dy[1]) - (dy[0] * dx[1]);
			
			/*calculate length of normal vector*/
			nd = sqrt((nf[0]*nf[0]) + (nf[1]*nf[1]) + (nf[2]*nf[2]));
			
			for (i = 0; i < 3; i++)
			{
				if (nd != 0)
					uv[i] = nf[i] / nd; /*uv being the unit normal vector*/
				nf[i] = uv[i]*0x7FFF;  /*nf being the way OoT uses it*/
			}
			
			/*distance from origin...*/
			dn = (int)round(
				(
					(uv[0] * p1[0])
					+ (uv[1] * p1[1])
					+ (uv[2] * p1[2])
				) * -1
			);
			if (dn < 0)
				dn+=0x10000;
			w = dn;

			for (i = 0; i < 3; i++)
			{
				ni[i] = (int)round(nf[i]);
				if (ni[i] < 0)
					ni[i] += 0x10000;
				switch (i)
				{
					case 0: x = ni[i]; break;
					case 1: y = ni[i]; break;
					case 2: z = ni[i]; break;
				}
			}
		}
		
		void write_triangle(short a, short b, short c)
		{
			/* write triangle */
			fix_collision(a, b, c);
			vfput16(bin, type);
			vfput16(bin, a | pt.v1);
			vfput16(bin, b | pt.v2);
			vfput16(bin, c | pt.v3);
			vfput16(bin, x);
			vfput16(bin, y);
			vfput16(bin, z);
			vfput16(bin, w);
			header.tri_num += 1;
		}
		
		/* write this triangle */
		write_triangle(a, b, c);
		
		/* double-sided collision (a and c are swapped) */
		if (is_double_sided)
			write_triangle(c, b, a);
	}
	
	/* write vertex buffer */
	header.vbuf_ofs = vfalign(bin, 4);
	header.vbuf_num = vbufNum;
	for (vert = vbuf; vert - vbuf < vbufNum; ++vert)
	{
		vfput16(bin, vert->x);
		vfput16(bin, vert->y);
		vfput16(bin, vert->z);
	}
	
	/* write header */
	header.ofs = vfalign(bin, 4);
	/*0x00*/vfput16(bin, round(world.min_x));          /* xyz min   */
	/*0x02*/vfput16(bin, round(world.min_y));
	/*0x04*/vfput16(bin, round(world.min_z));
	/*0x06*/vfput16(bin, round(world.max_x));          /* xyz max   */
	/*0x08*/vfput16(bin, round(world.max_y));
	/*0x0A*/vfput16(bin, round(world.max_z));
	/*0x0C*/vfput16(bin, header.vbuf_num);             /* num verts */
	/*0x0E*/vfput16(bin, 0);                           /* padding   */
	/*0x10*/vfput32(bin, header.vbuf_ofs + baseOfs);   /* ofs verts */
	/*0x14*/vfput16(bin, header.tri_num);              /* num tris  */
	/*0x16*/vfput16(bin, 0);                           /* padding   */
	/*0x18*/vfput32(bin, header.tri_ofs + baseOfs);    /* ofs tris  */
	/*0x1C*/vfput32(bin, header.ctype_ofs + baseOfs);  /* ofs types */
	/*0x20*/vfput32(bin, 0);                           /* cameras   */
	/*0x24*/vfput16(bin, header.water_num);            /* num water */
	/*0x26*/vfput16(bin, 0);                           /* padding   */
	if (header.water_num)
	/*0x28*/vfput32(bin, header.water_ofs + baseOfs);  /* ofs water */
	else
	/*0x28*/vfput32(bin, 0);                           /* ofs water */
	/*0x2C = length of header*/
	
	/* cleanup */
	if (headOfs)
		*headOfs = header.ofs;
	free(vbuf);
	return success;
#undef FAIL
}


const char *collision_errmsg(void)
{
	return error_reason;
}

