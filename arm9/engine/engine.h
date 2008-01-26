#ifndef ENGINE_H
#define ENGINE_H 1

#include "map.h"
#include "list.h"

// initialise the DS
void torch_init();

// run some processes, freeing NULL ones if necessary
void run_processes(Map *map, List<process_t> processes);

// scroll the screen one tile in the direction dir. This function moves the
// "viewport" in the direction specified, via DMA. This function acts on
// whatever is the current backbuffer.
void scroll_screen(Map *map, int dsX, int dsY);

// mark the entire screen for redrawing. Slow!
void dirty_screen();

// reset the luminance window (readjust your eyes)
void reset_luminance();

// draw the map
void draw(Map *map);

// run the game
void run(Map *map);

#endif /* ENGINE_H */
