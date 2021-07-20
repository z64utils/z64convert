#ifndef COLLISION_H_INCLUDED
#define COLLISION_H_INCLUDED

#include "vfile.h"

extern void *collision_write(VFILE *bin, struct objex_g *g, unsigned baseOfs, unsigned *headOfs);
extern const char *collision_errmsg(void);

#endif /* !COLLISION_H_INCLUDED */
