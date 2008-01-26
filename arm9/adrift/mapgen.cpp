#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

void tree(cell_t *cell) {
	cell->type = T_TREE;
	cell->ch = '*';
	cell->col = RGB15(4,31,1);
	cell->opaque = true;
	cell->forgettable = false;
}

void ground(cell_t *cell) {
	cell->type = T_GROUND;
	cell->ch = '.';
	cell->col = RGB15(17,9,6);
	cell->opaque = false;
	cell->forgettable = true;
}

void randwalk(s32 &x, s32 &y) {
	unsigned int a = genrand_int32();
	switch (a & 3) {
		case 0: x++; break;
		case 1: x--; break;
		case 2: y++; break;
		case 3: y--; break;
	}
}

void generate_terrarium(Map *map) {
	s32 cx = map->w/2, cy = map->h/2;

	map->reset();
	map->reset_cache();

	s32 x, y;

	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++)
			tree(map->at(x,y));

	unsigned int i;
	for (i = 0; i < 100; i++) {
		// symmetry
		ground(map->at(cx, cy));
		ground(map->at(map->w-cx, cy));
		ground(map->at(cx, map->h-cy));
		ground(map->at(map->w-cx, map->h-cy));

		ground(map->at(cy, cx));
		ground(map->at(map->h-cy, cx));
		ground(map->at(cy, map->w-cx));
		ground(map->at(map->h-cy, map->w-cx));

		randwalk(cx,cy);
		map->bounded(cx,cy);
	}

	map->pX = map->w/2;
	map->pY = map->h/2;

	refresh_blockmap(map);
}
