#include "mt19937ar.h"
#include "map.h"
#include <malloc.h>

void reset_map(map_t* map, CELL_TYPE fill) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = &map->cells[y*w+x];
			cell->type = fill;
			cell->ch = 0;
			cell->col = 0;
			cell->light = 0;
			cell->recall = 0;
			cell->dirty = 0;
		}
	map->num_lights = 0;
}
map_t *create_map(u32 w, u32 h, CELL_TYPE fill) {
	map_t *ret = malloc(sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	reset_map(ret, fill);

	return ret;
}

void random_map(map_t *map) {
	s32 x,y;
	reset_map(map, T_TREE);
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	x = map->w/2; y = map->h/2;
	u32 i = 8192;
	u32 light1 = genrand_int32() >> 21, // between 0 and 2047
			light2 = light1 + 40;
	for (; i > 0; i--) {
		cell_t *cell = cell_at(map, x, y);
		u32 a = genrand_int32();
		if (i == light1 || i == light2) {
			cell->type = T_FIRE;
			cell->ch = 'w';
			cell->col = RGB15(31,12,0);
			light_t *l = &map->lights[map->num_lights];
			l->x = x;
			l->y = y;
			l->col = RGB15(31,20,8);
			l->radius = 9;
			l->type = L_FIRE;
			map->num_lights++;
		} else if (cell->type == T_TREE) {
			cell->type = T_GROUND;
			u8 b = a & 3; // top two bits of a
			a >>= 2; // get rid of the used random bits
			switch (b) {
				case 1:
					cell->ch = ','; break;
				case 2:
					cell->ch = '\''; break;
				case 3:
					cell->ch = '`'; break;
				default:
					cell->ch = '.'; break;
			}
			b = a & 3;
			a >>= 2;
			u8 g = a & 7;
			a >>= 3;
			u8 r = a & 7;
			a >>= 3;
			cell->col = RGB15(17+r,9+g,6+b);
		}

		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= map->w) x = map->w-1;
		if (y >= map->h) y = map->h-1;
	}
	cell_t *cell = cell_at(map, x, y);
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);
}

void load_map(map_t *map, size_t len, const char *desc) {
	s32 x,y;
	reset_map(map, T_GROUND);
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			u32 a = genrand_int32();
			u8 b = a & 3; // top two bits of a
			a >>= 2; // get rid of the used random bits
			switch (b) {
				case 1:
					cell->ch = ','; break;
				case 2:
					cell->ch = '\''; break;
				case 3:
					cell->ch = '`'; break;
				default:
					cell->ch = '.'; break;
			}
			b = a & 3;
			a >>= 2;
			u8 g = a & 7;
			a >>= 3;
			u8 r = a & 7;
			a >>= 3;
			cell->col = RGB15(17+r,9+g,6+b);
		}

	for (x = 0, y = 0; len > 0; len--) {
		u8 c = *desc++;
		cell_t *cell = cell_at(map, x, y);
		switch (c) {
			case '*':
				cell->type = T_TREE;
				cell->ch = '*';
				cell->col = RGB15(4,31,1);
				break;
			case '@':
				map->pX = x;
				map->pY = y;
				break;
			case 'w':
				cell->type = T_FIRE;
				cell->ch = 'w';
				cell->col = RGB15(31,12,0);
				light_t *l = &map->lights[map->num_lights];
				l->x = x;
				l->y = y;
				l->col = RGB15(31,20,8);
				l->radius = 9;
				l->type = L_FIRE;
				map->num_lights++;
				break;
			case '\n':
				x = -1; // gets ++'d later
				y++;
				break;
		}
		x++;
	}
}
