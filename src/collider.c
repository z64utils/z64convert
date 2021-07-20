/* <z64.me> collider.c: where all collider-related functions live */

#include "objex.h"
#include "err.h"
#include "collider.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static inline float min4(float a, float b, float c, float d)
{
	return fmin(fmin(a, b), fmin(c, d));
}

static inline float max4(float a, float b, float c, float d)
{
	return fmax(fmax(a, b), fmax(c, d));
}

/* calculate center of three vertices */
static struct objex_v v_center(
	struct objex_v *v1
	, struct objex_v *v2
	, struct objex_v *v3
)
{
	return (struct objex_v){
		.x = (v1->x + v2->x + v3->x) * 0.33333333333f
		, .y = (v1->y + v2->y + v3->y) * 0.33333333333f
		, .z = (v1->z + v2->z + v3->z) * 0.33333333333f
	};
}

/* get boundaries of triangle island */
static void tri_island_bounds(
	struct objex_v *v
	, struct objex_f *arr
	, struct objex_f *cur
	, struct boundingBox *stats
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
			stats->mtl = cur->mtl;
			tri_island_bounds(v, arr, iter, stats, arrNum);
		}
	}
}

static
int sort_colliderJoint_boneIndex_ascend(const void *a_, const void *b_)
{
	const struct colliderJoint *a = a_;
	const struct colliderJoint *b = b_;
	
	return a->boneIndex - b->boneIndex;
}

void colliderJoint_boneIndex_ascend(struct colliderJoint *list, int num)
{
	qsort(list, num, sizeof(*list), sort_colliderJoint_boneIndex_ascend);
}

void *collider_init(struct objex_g *g)
{
	struct objex *obj = g->objex;
	
	/* complain if collider is of type joint but not rigged */
	if (strstr(g->name, "joint") && !obj->v[g->f->v.x].weight)
		return errmsg("joint group '%s' is not rigged", g->name);
	
	/* clear all these to 0 */
	for (struct objex_f *f = g->f; f - g->f < g->fNum; ++f)
		f->_pvt = 0;
	
	return success;
}

struct boundingBox *collider_bounds(struct objex_g *g)
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
		tri_island_bounds(v, g->f, f, &bounds, g->fNum);
//		debugf("stats are in\n");
		return &bounds;
	}
	
	return 0;
}

const char *collider_errmsg(void)
{
	return error_reason;
}

