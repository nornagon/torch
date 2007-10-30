#ifndef RANDMAP_H
#define RANDMAP_H 1

#include "torch.h"

void ground(cell_t *cell);
void lake(map_t *map, s32 x, s32 y);

void random_map(map_t *map);

#endif /* RANDMAP_H */
