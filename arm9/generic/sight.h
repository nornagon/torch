#ifndef SIGHT_H
#define SIGHT_H 1

#include <nds.h>

bool sight_opaque(void *map_, int x, int y);
void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_);

#endif /* SIGHT_H */
