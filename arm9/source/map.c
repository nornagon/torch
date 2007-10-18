#include "mt19937ar.h"
#include "map.h"
#include <malloc.h>
#include "mem.h"
#include "process.h"
#include "mt19937ar.h"

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

	while (map->processes) {
		node_t *next = map->processes->next;
		((process_t*)node_data(map->processes))->end(node_data(map->processes));
		free_node(map->process_pool, map->processes);
		map->processes = next;
	}
	flush_free(map->process_pool);
	alloc_space(map->process_pool, 32);
}
map_t *create_map(u32 w, u32 h, CELL_TYPE fill) {
	map_t *ret = malloc(sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	ret->cache = malloc(32*24*sizeof(cache_t));
	ret->processes = NULL;
	ret->process_pool = new_llpool(sizeof(process_t));
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


//--------------------------------XXX-----------------------------------------
// everything below here is game-specific, and should be moved to another file

void process_light(process_t *process, map_t *map) {
	light_t *l = (light_t*)process->data;

	if (l->type == L_FIRE) {
		if (l->flickered == 4 || l->flickered == 0)
			l->dr = (genrand_gaussian32() >> 19) - 4096;

		if (l->flickered == 0) {
			u32 m = genrand_int32();
			if (m < (u32)(0xffffffff*0.6)) l->dx = l->dy = 0;
			else {
				if (m < (u32)(0xffffffff*0.7)) {
					l->dx = 0; l->dy = 1;
				} else if (m < (u32)(0xffffffff*0.8)) {
					l->dx = 0; l->dy = -1;
				} else if (m < (u32)(0xffffffff*0.9)) {
					l->dx = 1; l->dy = 0;
				} else {
					l->dx = -1; l->dy = 0;
				}
				if (opaque(cell_at(map, l->x + l->dx, l->y + l->dy)))
					l->dx = l->dy = 0;
			}
			l->flickered = 8; // flicker every 8 frames
		} else {
			l->flickered--;
		}
	}

	// don't bother if it's completely outside the screen.
	if (l->x + l->radius + (l->dr >> 12) < map->scrollX ||
			l->x - l->radius - (l->dr >> 12) > map->scrollX + 32 ||
			l->y + l->radius + (l->dr >> 12) < map->scrollY ||
			l->y - l->radius - (l->dr >> 12) > map->scrollY + 24) return;

	fov_circle(map->fov_light, (void*)map, (void*)l, l->x + l->dx, l->y + l->dy, l->radius + 2);
	cell_t *cell = cell_at(map, l->x + l->dx, l->y + l->dy);
	if (cell->visible) {
		cell->light += (1<<12);
		cache_t *cache = cache_at(map, l->x + l->dx, l->y + l->dy);
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

void end_light(process_t *process) {
	free(process->data);
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
			light_t *l = (light_t*)malloc(sizeof(light_t));
			l->x = x;
			l->y = y;
			l->r = 1.00*(1<<12);
			l->g = 0.65*(1<<12);
			l->b = 0.26*(1<<12);
			l->radius = 9;
			l->type = L_FIRE;
			node_t *node = request_node(map->process_pool);
			process_t *process = node_data(node);
			process->process = process_light;
			process->end = end_light;
			process->data = (void*)l;
			map->processes = push_node(map->processes, node);
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
					light_t *l = (light_t*)malloc(sizeof(light_t));
					l->x = x;
					l->y = y;
					l->r = 1.00*(1<<12);
					l->g = 0.65*(1<<12);
					l->b = 0.26*(1<<12);
					l->radius = 9;
					l->type = L_FIRE;
					node_t *node = request_node(map->process_pool);
					process_t *process = node_data(node);
					process->process = process_light;
					process->end = end_light;
					process->data = (void*)l;
					map->processes = push_node(map->processes, node);
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
					light_t *l = malloc(sizeof(light_t));
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
					node_t *node = request_node(map->process_pool);
					process_t *process = node_data(node);
					process->process = process_light;
					process->end = end_light;
					process->data = (void*)l;
					map->processes = push_node(map->processes, node);
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
