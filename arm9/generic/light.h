#ifndef LIGHT_H
#define LIGHT_H 1

#include <nds.h>
#include "fov.h"
#include "map.h"
#include "lightsource.h"
#include "direction.h"
#include "list.h"

#include "blockmap.h"

// TODO: make draw_light take the parameters of the light (colour, radius,
// position) and build up the lighting struct to pass to fov_circle on its
// ownsome?
void draw_light(fov_settings_type *settings, blockmap *map, lightsource *l);

void draw_lights(fov_settings_type *settings, blockmap *map, List<lightsource*> lights);

// standard light-casting opacity test for libfov. Checks map edges, cell
// opacity and player occupation (via map->pX/pY).
bool opacity_test(void *map, int x, int y);

// standard light-casting application function. will only light visible cells.
void apply_light(void *map, int x, int y, int dxblah, int dyblah, void *src);

// if some light source is in direction d from the cell, which sides of the cell
// do we want to light? for cases such as:
//
//      v--- light source
// #    w
// #      <--+--- we only want to light the right face of these cells, not the
// #     <---+    right *and* the top
// #    <----+
static inline DIRECTION seen_from(DIRECTION d, blockel *cell) {
	bool opa, opb;
	switch (d) {
		case D_NORTHWEST:
		case D_NORTH_AND_WEST:
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_NORTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_WEST;
			break;
		case D_SOUTHWEST:
		case D_SOUTH_AND_WEST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_SOUTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_WEST;
			break;
		case D_NORTHEAST:
		case D_NORTH_AND_EAST:
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_EAST;
			if (opa && opb) return D_NORTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_EAST;
			break;
		case D_SOUTHEAST:
		case D_SOUTH_AND_EAST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_EAST;
			if (opa && opb) return D_SOUTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_EAST;
			break;
	}
	return d;
}

#endif /* LIGHT_H */
