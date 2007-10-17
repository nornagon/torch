#include "mt19937ar.h"
#include "map.h"
#include <malloc.h>
#include "mem.h"

void reset_cache(map_t *map) {
	u32 x, y;
	for (y = 0; y < 24; y++)
		for (x = 0; x < 32; x++) {
			cache_t* cache = &map->cache[y*32+x];
			cache->lr = cache->lg = cache->lb = 0;
			cache->last_lr = cache->last_lg = cache->last_lb = 0;
			cache->last_light = 0;
			cache->last_col = 0;
			cache->dirty = 0;
			cache->was_visible = 0;
		}
	map->cacheX = map->cacheY = 0;
}

void reset_map(map_t* map, CELL_TYPE fill) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = cell_at(map, x, y);
			cell->type = fill;
			cell->ch = 0;
			cell->col = 0;
			cell->light = 0;
			cell->recall = 0;
			cell->visible = 0;
			cell->blocked_from = 0;
			cell->seen_from = 0;
		}
	map->num_lights = 0;
}
map_t *create_map(u32 w, u32 h, CELL_TYPE fill) {
	map_t *ret = malloc(sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	ret->cache = malloc(32*24*sizeof(cache_t));
	reset_map(ret, fill);
	reset_cache(ret);

	return ret;
}

void refresh_blockmap(map_t *map) {
	s32 x,y;
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			unsigned int blocked_from = 0;
			if (opaque(cell_at(map, x, y-1))) blocked_from |= D_NORTH;
			if (opaque(cell_at(map, x, y+1))) blocked_from |= D_SOUTH;
			if (opaque(cell_at(map, x+1, y))) blocked_from |= D_EAST;
			if (opaque(cell_at(map, x-1, y))) blocked_from |= D_WEST;
			cell_at(map, x, y)->blocked_from = blocked_from;
		}
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
			l->r = 1.00*(1<<12);
			l->g = 0.65*(1<<12);
			l->b = 0.26*(1<<12);
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
	refresh_blockmap(map);
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
				{
					light_t *l = &map->lights[map->num_lights++];
					l->x = x;
					l->y = y;
					l->r = 1.00*(1<<12);
					l->g = 0.65*(1<<12);
					l->b = 0.26*(1<<12);
					l->radius = 9;
					l->type = L_FIRE;
				}
				break;
			case 'o':
				cell->col = RGB15(12,0,31);
				goto make;
			case 'r':
				cell->col = RGB15(31,0,0);
				goto make;
			case 'g':
				cell->col = RGB15(0,31,0);
				goto make;
			case 'b':
				cell->col = RGB15(0,0,31);
make:
				cell->type = T_LIGHT;
				cell->ch = 'o';
				{
					light_t *l = &map->lights[map->num_lights];
					l->x = x;
					l->y = y;
					if (c == 'o') {
						l->r = 0.39*(1<<12);
						l->g = 0.05*(1<<12);
						l->b = 1.00*(1<<12);
					} else if (c == 'r') {
						l->r = 1.00*(1<<12);
						l->g = 0.07*(1<<12);
						l->b = 0.07*(1<<12);
					} else if (c == 'g') {
						l->r = 0.07*(1<<12);
						l->g = 1.00*(1<<12);
						l->b = 0.07*(1<<12);
					} else {
						l->r = 0.07*(1<<12);
						l->g = 0.07*(1<<12);
						l->b = 1.00*(1<<12);
					}
					l->radius = 8;
					l->type = L_GLOWER;
					map->num_lights++;
				}
				break;
			case '\n':
				x = -1; // gets ++'d later
				y++;
				break;
		}
		x++;
	}
	refresh_blockmap(map);
}
