#include <stdint.h>

struct colliderJoint
{
	int center_x;
	int center_y;
	int center_z;
	int scale;
	int radius;
	int joint;
	int boneIndex;
	
	uint32_t bodyFlags;
	uint32_t touchFlags;
	uint32_t touchFx;
	uint32_t bumpFlag;
	uint32_t bumpFx;
	uint32_t tchBmpBdy;
};

extern void *collider_init(struct objex_g *g);
extern struct boundingBox *collider_bounds(struct objex_g *g);
extern const char *collider_errmsg(void);
extern void colliderJoint_boneIndex_ascend(struct colliderJoint *list, int num);

