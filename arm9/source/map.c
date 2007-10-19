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
			cache->last_col_final = 0;
			cache->dirty = 0;
			cache->was_visible = 0;
		}
	// note that the cache is origin-agnostic, so the top-left corner of cache
	// when scrollX = 0 does not have to correspond with cacheX = 0. The caching
	// mechanisms will deal just fine whereever the origin is. Really, we don't
	// need to set cacheX or cacheY here, but we do (just in case something weird
	// happened to them)
	map->cacheX = map->cacheY = 0;
}

void reset_map(map_t* map) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = cell_at(map, x, y);
			cell->type = T_NONE;
			cell->ch = 0;
			cell->col = 0;
			cell->light = 0;
			cell->recall = 0;
			cell->visible = 0;
			cell->blocked_from = 0;
			cell->seen_from = 0;

			// clear and the object list
			while (cell->objects) {
				node_t *next = cell->objects->next;
				object_t *obj = (object_t*)node_data(cell->objects);
				objecttype_t *type = &map->objtypes[obj->type];
				if (type->end)
					type->end(obj);
				free_node(map->object_pool, cell->objects);
				cell->objects = next;
			}
		}
	// free all the objects
	flush_free(map->object_pool);

	// clear and free the process list
	while (map->processes) {
		node_t *next = map->processes->next;
		process_t *p = (process_t*)node_data(map->processes);
		if (p->end)
			p->end(p);
		free_node(map->process_pool, map->processes);
		map->processes = next;
	}
	flush_free(map->process_pool);

	// clear and free
}

map_t *create_map(u32 w, u32 h) {
	map_t *ret = malloc(sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	ret->cache = malloc(32*24*sizeof(cache_t)); // screen sized

	ret->processes = NULL;
	ret->process_pool = new_llpool(sizeof(process_t));

	ret->object_pool = new_llpool(sizeof(object_t));

	reset_map(ret);
	reset_cache(ret);

	return ret;
}

void refresh_blockmap(map_t *map) {
	// each cell needs to know which cells around them are opaque for purposes of
	// direction-aware lighting.

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

process_t *new_process(map_t *map) {
	node_t *node = request_node(map->process_pool);
	map->processes = push_node(map->processes, node);
	return node_data(node);
}

void insert_object(map_t *map, node_t *obj_node, s32 x, s32 y) {
	cell_t *target = cell_at(map, x, y);
	object_t *obj = node_data(obj_node);
	objecttype_t *objtype = &map->objtypes[obj->type];
	obj->x = x;
	obj->y = y;
	node_t *prev = NULL;
	node_t *head = target->objects;
	// walk through the list
	while (head) {
		object_t *k = node_data(head);
		// if this object k is of less importance than the object we're inserting,
		// we want to insert obj before k.
		// We use <= rather than < to reduce insert time in the case where there are
		// a large number of objects of equal importance. In such a case, a new
		// insertion will come closer to the beggining of the list.
		if (map->objtypes[k->type].importance <= objtype->importance)
			break;
		prev = head;
		head = head->next;
	}
	// head is either NULL or something less important than the inserted object.
	obj_node->next = head;
	if (prev)
		prev->next = obj_node;
	else
		target->objects = obj_node;
}


//--------------------------------XXX-----------------------------------------
// everything below here is game-specific, and should be moved to another file

void draw_light(light_t *l, map_t *map) {
	// don't bother calculating if the light's completely outside the screen.
	if (l->x + l->radius + (l->dr >> 12) < map->scrollX ||
	    l->x - l->radius - (l->dr >> 12) > map->scrollX + 32 ||
	    l->y + l->radius + (l->dr >> 12) < map->scrollY ||
	    l->y - l->radius - (l->dr >> 12) > map->scrollY + 24) return;

	// calculate lighting values
	fov_circle(map->fov_light, (void*)map, (void*)l,
			l->x + (l->dx>>12), l->y + (l->dy>>12), l->radius + 2);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	cell_t *cell = cell_at(map, l->x + (l->dx>>12), l->y + (l->dy>>12));
	if (cell->visible) {
		cell->light += (1<<12);
		cache_t *cache = cache_at(map, l->x + (l->dx>>12), l->y + (l->dy>>12));
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

void process_light(process_t *process, map_t *map) {
	light_t *l = (light_t*)process->data;

	if (l->type == L_FIRE) { // clever feet that flicker like fire
		if (l->flickered == 4 || l->flickered == 0) // radius changes more often than origin.
			l->dr = (genrand_gaussian32() >> 19) - 4096;

		if (l->flickered == 0) {
			// shift the origin around. Waiting on blue_puyo for sub-tile origin
			// shifts in libfov.
			u32 m = genrand_int32();
			if (m < (u32)(0xffffffff*0.6)) l->dx = l->dy = 0;
			else {
				if (m < (u32)(0xffffffff*0.7)) {
					l->dx = 0; l->dy = 1<<12;
				} else if (m < (u32)(0xffffffff*0.8)) {
					l->dx = 0; l->dy = -(1<<12);
				} else if (m < (u32)(0xffffffff*0.9)) {
					l->dx = 1<<12; l->dy = 0;
				} else {
					l->dx = -(1<<12); l->dy = 0;
				}
				if (opaque(cell_at(map, l->x + (l->dx>>12), l->y + (l->dy>>12))))
					l->dx = l->dy = 0;
			}
			l->flickered = 8; // flicker every 8 frames
		} else {
			l->flickered--;
		}
	}

	draw_light(l, map);
}

void end_light(process_t *process) {
	free(process->data); // the light_t struct we were keeping
}

u32 random_colour(object_t *obj, map_t *map) {
	return ((map->objtypes[obj->type].ch)<<16) | (genrand_int32()&0xffff);
}

// TODO: preprocessor magic to make this easier
typedef struct {
	process_t *light_proc;
	process_t *thought_proc;
	light_t *light;
} mon_WillOWisp_t;

void mon_WillOWisp_light(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	draw_light(wisp->light, map);
}

void mon_WillOWisp_thought(process_t *process, map_t *map) {
}

void new_mon_WillOWisp(map_t *map, s32 x, s32 y) {
	mon_WillOWisp_t *wisp = malloc(sizeof(mon_WillOWisp_t));

	process_t *light_proc = new_process(map);
	light_proc->process = mon_WillOWisp_light;
	light_proc->data = wisp;

	process_t *thought_proc = new_process(map);
	thought_proc->process = mon_WillOWisp_thought;
	thought_proc->data = wisp;

	wisp->light_proc = light_proc;
	wisp->thought_proc = thought_proc;

	light_t *light = malloc(sizeof(light_t));
	light->type = L_GLOWER;
	light->x = x;
	light->y = y;
	light->dx = 0;
	light->dy = 0;
	light->dr = 0;

	// TODO: magic to make this easier
	light->r = 0.23*(1<<12);
	light->g = 0.87*(1<<12);
	light->b = 1.00*(1<<12);

	light->radius = 4;

	wisp->light = light;

  node_t *obj_node = request_node(map->object_pool);
  object_t *obj = node_data(obj_node);
  obj->type = 1;
  insert_object(map, obj_node, x, y);
}


objecttype_t objects[] = {
	// 0: unknown object
	{ .ch = '?',
	  .col = RGB15(31,31,31),
	  .importance = 3,
	  .display = random_colour,
	  .end = NULL
	},
	// 1: will o' wisp
	{ .ch = 'o',
	  .col = RGB15(7,31,27),
	  .importance = 128,
	  .display = NULL,
	  .end = NULL,
	}
};


void lake(map_t *map, s32 x, s32 y) {
	u32 i;
	for (i = 0; i < 64; i++) {
		cell_t *cell = cell_at(map, x, y);
		if (cell->type != T_FIRE) {
			cell->type = T_WATER;
			cell->ch = '~';
			cell->col = RGB15(6,9,31);
		}
		u32 a = genrand_int32();
		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}
	}
}

void random_map(map_t *map) {
	s32 x,y;
	reset_map(map);
	map->objtypes = objects;

	// clear out the map to a backdrop of trees
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			cell->type = T_TREE;
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	// start in the centre
	x = map->w/2; y = map->h/2;

	node_t *something = request_node(map->object_pool);
	object_t *obj = node_data(something);
	obj->type = 0;
	insert_object(map, something, x, y);


	// place some fires
	u32 light1 = genrand_int32() >> 21, // between 0 and 2047
			light2 = light1 + 40;

	u32 lakepos = (genrand_int32()>>21) + 4096; // hopefully away from the fires

	u32 i;
	for (i = 8192; i > 0; i--) { // 8192 steps of the drunkard's walk
		cell_t *cell = cell_at(map, x, y);
		u32 a = genrand_int32();
		if (i == 8191) {
			new_mon_WillOWisp(map, x, y);
		}
		if (i == light1 || i == light2) { // place a fire here
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

			// add the fire to the process list
			process_t *process = new_process(map);
			process->process = process_light;
			process->end = end_light;
			process->data = (void*)l;
		} else if (i == lakepos) {
			lake(map, x, y);
		} else if (cell->type == T_TREE) { // clear away some tree
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
		}

		if (a & 1) { // pick some bits off the number (there are still some left)
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}

		// don't run off the edge of the map
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= map->w) x = map->w-1;
		if (y >= map->h) y = map->h-1;
	}

	// place the stairs at the end
	cell_t *cell = cell_at(map, x, y);
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);

	// update opacity information
	refresh_blockmap(map);
}

void load_map(map_t *map, size_t len, const char *desc) {
	s32 x,y;
	reset_map(map);
	map->objtypes = objects;

	// clear the map out to ground
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			cell->type = T_GROUND;
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

	// read the map from the string
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
					// add the light to the process list
					process_t *process = new_process(map);
					process->process = process_light;
					process->end = end_light;
					process->data = (void*)l;
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

					process_t *process = new_process(map);
					process->process = process_light;
					process->end = end_light;
					process->data = (void*)l;
				}
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
