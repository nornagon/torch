#include "loadmap.h"

#include "fire.h"
#include "globe.h"
#include "testworld.h"

void load_map(map_t *map, size_t len, const char *desc) {
	s32 x,y;
	reset_map(map);

	// clear the map out to ground
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++)
			ground(cell_at(map, x, y));

	// read the map from the string
	for (x = 0, y = 0; len > 0; len--) {
		u8 c = *desc++;
		cell_t *cell = cell_at(map, x, y);
		switch (c) {
			case '*':
				cell->type = T_TREE;
				cell->ch = '*';
				cell->col = RGB15(4,31,1);
				cell->opaque = true;
				break;
			case '@':
				ground(cell);
				map->pX = x;
				map->pY = y;
				break;
			case 'w':
				ground(cell);
				new_obj_fire(map, x, y, 9<<12);
				break;
			case 'o':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 0.39*(1<<12), 0.05*(1<<12), 1.00*(1<<12)));
				break;
			case 'r':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 1.00*(1<<12), 0.07*(1<<12), 0.07*(1<<12)));
				break;
			case 'g':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 1.00*(1<<12), 0.07*(1<<12)));
				break;
			case 'b':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 0.07*(1<<12), 1.00*(1<<12)));
				break;
			case '\n': // reached the end of the line, so move down
				x = -1; // gets ++'d later
				y++;
				break;
		}
		x++;
	}

	// update opacity map
	refresh_blockmap(map);
}
