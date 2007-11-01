#ifndef ROCKS_H
#define ROCKS_H 1

#include "torch.h"

void new_obj_rock(map_t *map, s32 x, s32 y, u8 quantity);
void obj_rock_string(object_t *obj);

objecttype_t ot_rock;
objecttype_t *OT_ROCK;

#endif /* ROCKS_H */
