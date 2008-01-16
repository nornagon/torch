#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

void tree(cell_t *cell) {
	*cell = DEFAULT_TREE;
}

void ground(cell_t *cell) {
	*cell = DEFAULT_GROUND;
}

void generate_terrarium(map_t *map) {
	s32 cx = map->w/2, cy = map->h/2;

	reset_map(map);
	reset_cache(map);

	s32 x, y;

	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++)
			tree(cell_at(map, x, y));

	unsigned int i;
	for (i = 0; i < 100; i++) {
		// symmetry
		ground(cell_at(map, cx, cy));
		ground(cell_at(map, map->w-cx, cy));
		ground(cell_at(map, cx, map->h-cy));
		ground(cell_at(map, map->w-cx, map->h-cy));

		ground(cell_at(map, cy, cx));
		ground(cell_at(map, map->h-cy, cx));
		ground(cell_at(map, cy, map->w-cx));
		ground(cell_at(map, map->h-cy, map->w-cx));

		unsigned int a = genrand_int32();
		switch (a & 7) {
			case 0: cx++; break;
			case 1: cx--; break;
			case 2: cy++; break;
			case 3: cy--; break;
			case 4: cx++; break;
			case 5: cx++; break;
			case 6: cy--; break;
			case 7: cy--; break;
		}
		bounded(map, &cx, &cy);
	}

	map->pX = map->w/2;
	map->pY = map->h/2;

	refresh_blockmap(map);
}
