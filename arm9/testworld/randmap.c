#include "randmap.h"
#include "testworld.h"
#include "mersenne.h"

#include "willowisp.h"
#include "fire.h"
#include "globe.h"
#include "rocks.h"

// turn the cell into a ground cell.
void ground(cell_t *cell) {
	unsigned int a = genrand_int32();
	cell->type = T_GROUND;
	unsigned int b = a & 3; // top two bits of a
	a >>= 2; // get rid of the used random bits
	switch (b) {
		case 0:
			cell->ch = '.'; break;
		case 1:
			cell->ch = ','; break;
		case 2:
			cell->ch = '\''; break;
		case 3:
			cell->ch = '`'; break;
	}
	b = a & 3; // next two bits of a (0..3)
	a >>= 2;
	u8 g = a & 7; // three bits (0..7)
	a >>= 3;
	u8 r = a & 7; // (0..7)
	a >>= 3;
	cell->col = RGB15(17+r,9+g,6+b); // more randomness in red/green than in blue
	cell->opaque = false;
	cell->forgettable = true;
}

void lake(map_t *map, s32 x, s32 y) {
	u32 wisppos = genrand_int32() & 0x3f; // between 0 and 63
	u32 i;
	// scour out a lake by random walk
	for (i = 0; i < 64; i++) {
		cell_t *cell = cell_at(map, x, y);
		if (i == wisppos) // place the wisp
			new_mon_WillOWisp(map, x, y);
		if (!has_objtype(cell, OT_FIRE)) { // if the cell doesn't have a fire in it
			cell->type = T_WATER;
			cell->ch = '~';
			cell->col = RGB15(6,9,31);
			cell->opaque = false;
		}
		u32 a = genrand_int32();
		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}
		bounded(map, &x, &y);
	}
}

void random_map(map_t *map) {
	s32 x,y;
	reset_map(map);

	// clear out the map to a backdrop of trees
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			cell->opaque = true;
			cell->type = T_TREE;
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	// start in the centre
	x = map->w/2; y = map->h/2;

	node_t *something = new_object(map, OT_UNKNOWN, NULL);
	insert_object(map, something, x, y);


	// place some fires
	u32 light1 = genrand_int32() >> 21, // between 0 and 2047
			light2 = light1 + 40;

	u32 lakepos = (genrand_int32()>>21) + 4096; // hopefully away from the fires

	u32 i;
	for (i = 8192; i > 0; i--) { // 8192 steps of the drunkard's walk
		cell_t *cell = cell_at(map, x, y);
		if (i == light1 || i == light2) { // place a fire here
			// fires go on the ground
			if (cell->type != T_GROUND) ground(cell);
			new_obj_fire(map, x, y, 9<<12);
		} else if (i == lakepos) {
			lake(map, x, y);
		} else if (cell->type == T_TREE) { // clear away some tree
			ground(cell);
		}

		if (genrand_int32() < (0.005)*0xffffffff) {
			u32 a = genrand_int32();
			a = (a & 3) + ((a >> 2) & 3);
			new_obj_rock(map, x, y, a + 1);
		}

		u32 a = genrand_int32();
		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}

		// don't run off the edge of the map
		bounded(map, &x, &y);
	}

	// place the stairs at the end
	cell_t *cell = cell_at(map, x, y);
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);

	// and the player at the beginning
	map->pX = map->w/2;
	map->pY = map->h/2;

	// update opacity information
	refresh_blockmap(map);
}
