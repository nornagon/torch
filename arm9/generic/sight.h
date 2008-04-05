#ifndef SIGHT_H
#define SIGHT_H 1

#include <nds.h>
#include "fov.h"
#include "lightsource.h"
#include "blockmap.h"

void cast_sight(fov_settings_type *settings, blockmap *block, lightsource *l);

bool sight_opaque(void *map_, int x, int y);
void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_);

#endif /* SIGHT_H */
