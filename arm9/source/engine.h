#ifndef ENGINE_H
#define ENGINE_H 1

#include "map.h"
#include "list.h"

// initialise the DS
void torch_init();

// run some processes, freeing NULL ones if necessary
void run_processes(map_t *map, node_t **processes);

// scroll the screen one tile in the direction dir. This function moves the
// "viewport" in the direction specified, via DMA. This function acts on
// whatever is the current backbuffer.
void scroll_screen(map_t *map, DIRECTION dir);

#endif /* ENGINE_H */
