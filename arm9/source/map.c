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
					type->end(obj, map);
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
			p->end(p, map);
		free_node(map->process_pool, map->processes);
		map->processes = next;
	}
	flush_free(map->process_pool);
}

map_t *create_map(u32 w, u32 h) {
	map_t *ret = malloc(sizeof(map_t));
	memset(ret, 0, sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	memset(ret->cells, 0, w*h*sizeof(cell_t));
	ret->cache = malloc(32*24*sizeof(cache_t)); // screen sized
	memset(ret->cache, 0, 32*24*sizeof(cache_t));

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
			if (y != 0 && opaque(cell_at(map, x, y-1))) blocked_from |= D_NORTH;
			if (y != map->h - 1 && opaque(cell_at(map, x, y+1))) blocked_from |= D_SOUTH;
			if (x != map->w - 1 && opaque(cell_at(map, x+1, y))) blocked_from |= D_EAST;
			if (x != 0 && opaque(cell_at(map, x-1, y))) blocked_from |= D_WEST;
			cell_at(map, x, y)->blocked_from = blocked_from;
		}
}

light_t *new_light(int32 radius, int32 r, int32 g, int32 b) {
	light_t *light = malloc(sizeof(light_t));
	memset32(light, 0, sizeof(light_t)/4);
	light->radius = radius;
	light->r = r;
	light->g = g;
	light->b = b;
	return light;
};

node_t *_push_process(map_t *map, node_t **stack, process_func process, process_func end, void* data) {
	node_t *node = request_node(map->process_pool);
	process_t *proc = node_data(node);
	proc->process = process;
	proc->end = end;
	proc->data = data;
	*stack = push_node(*stack, node);
	return node;
}

node_t *new_object(map_t *map, u16 type, void* data) {
  node_t *node = request_node(map->object_pool);
  object_t *obj = node_data(node);
  obj->type = type;
  obj->data = data;
  return node;
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

void move_object(map_t *map, cell_t *loc, node_t *obj, s32 x, s32 y) {
	loc->objects = remove_node(loc->objects, obj);
	insert_object(map, obj, x, y);
}

inline void free_process(map_t *map, node_t *proc) {
	map->processes = remove_node(map->processes, proc);
	free_node(map->process_pool, proc);
}

// free processes that aren't the one specified.
// procs should be a pointer to an *array* of nodes, not the head of a list. num
// should be the number of nodes in the array.
void free_other_processes(map_t *map, process_t *this_proc, node_t *procs[], unsigned int num) {
	for (; num > 0; procs++, num--) {
		if (node_data(*procs) != this_proc)
			free_process(map, *procs);
	}
}

// free all the processes
void free_processes(map_t *map, node_t *procs[], unsigned int num) {
	for (; num > 0; procs++, num--)
		free_process(map, *procs);
}

// remove the object from its owning cell, and add the node to the free pool
void free_object(map_t *map, node_t *obj_node) {
	object_t *obj = node_data(obj_node);
	cell_t *cell = cell_at(map, obj->x, obj->y);
	cell->objects = remove_node(cell->objects, obj_node);
	free_node(map->object_pool, obj_node);
}

// free num objects, beginning at objs. that's an *array* of node pointers, not
// the head of a list.
void free_objects(map_t *map, node_t *objs[], unsigned int num) {
	for (; num > 0; objs++, num--)
		free_object(map, *objs);
}

//--------------------------------XXX-----------------------------------------
// everything below here is game-specific, and should be moved to another file

typedef enum {
	OT_UNKNOWN = 0,
	OT_WISP,
	OT_FIRE,
	OT_LIGHT
} OBJECT_TYPE;

// TODO: make draw_light take the parameters of the light (colour, radius,
// position) and build up the lighting struct to pass to fov_circle on its
// ownsome?
void draw_light(light_t *l, map_t *map) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < map->scrollX ||
	    ((l->x - l->radius) >> 12) > map->scrollX + 32 ||
	    ((l->y + l->radius) >> 12) < map->scrollY ||
	    ((l->y - l->radius) >> 12) > map->scrollY + 24) return;

	// calculate lighting values
	fov_circle(map->fov_light, (void*)map, (void*)l,
			l->x>>12, l->y>>12, (l->radius>>12) + 1);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	cell_t *cell = cell_at(map, l->x>>12, l->y>>12);
	if (cell->visible) {
		cell->light += (1<<12);
		cache_t *cache = cache_at(map, l->x>>12, l->y>>12);
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

typedef struct {
	node_t *light_node;
	node_t *obj_node;
	unsigned int flickered;
	int32 radius;
	light_t *light;
} obj_fire_t;

void obj_fire_process(process_t *process, map_t *map) {
	obj_fire_t *f = (obj_fire_t*)process->data;
	light_t *l = (light_t*)f->light;

	if (f->flickered == 4 || f->flickered == 0) // radius changes more often than origin.
		l->radius = f->radius + (genrand_gaussian32() >> 19) - 4096;

	if (f->flickered == 0) {
		// shift the origin around. Waiting on blue_puyo for sub-tile origin
		// shifts in libfov.
		object_t *obj = node_data(f->obj_node);
		int32 x = obj->x << 12,
		      y = obj->y << 12;
		u32 m = genrand_int32();
		if (m < (u32)(0xffffffff*0.6)) { l->x = x; l->y = y; }
		else {
			if (m < (u32)(0xffffffff*0.7)) {
				l->x = x; l->y = y + (1<<12);
			} else if (m < (u32)(0xffffffff*0.8)) {
				l->x = x; l->y = y - (1<<12);
			} else if (m < (u32)(0xffffffff*0.9)) {
				l->x = x + (1<<12); l->y = y;
			} else {
				l->x = x - (1<<12); l->y = y;
			}
			if (opaque(cell_at(map, l->x>>12, l->y>>12)))
			{ l->x = x; l->y = y; }
		}
		f->flickered = 8; // flicker every 8 frames
	} else {
		f->flickered--;
	}

	draw_light(l, map);
}

void obj_fire_proc_end(process_t *process, map_t *map) {
	// free the structures we were keeping
	obj_fire_t *fire = (obj_fire_t*)process->data;
	free_object(map, fire->obj_node);
	free(fire->light);
	free(fire);
}

void obj_fire_obj_end(object_t *object, map_t *map) {
	obj_fire_t *fire = (obj_fire_t*)object->data;
	free_process(map, fire->light_node);
	free(fire->light);
	free(fire);
}

// takes x and y as 32.0 cell coordinates, radius as 20.12
void new_obj_fire(map_t *map, s32 x, s32 y, int32 radius) {
	obj_fire_t *fire = malloc(sizeof(obj_fire_t));

	fire->radius = radius;
	fire->flickered = 0;

	light_t *light = new_light(radius, 1.00*(1<<12), 0.65*(1<<12), 0.26*(1<<12));
	fire->light = light;
	light->x = x << 12;
	light->y = y << 12;

	fire->light_node = push_process(map, obj_fire_process, obj_fire_proc_end, fire);
	fire->obj_node = new_object(map, OT_FIRE, fire);
	insert_object(map, fire->obj_node, x, y);
}


// a glowing, coloured light
typedef struct {
	node_t *light_node; // the light-drawing process node
	node_t *obj_node; // the presence object node

	light_t *light;

	// cache the display colour/character so we don't have to recalculate every
	// frame.
	u32 display;
} obj_light_t;

// boilerplate entity-freeing functions. TODO: how to make this easier?
void obj_light_proc_end(process_t *proc, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)proc->data;
	free_object(map, obj_light->obj_node);
	free(obj_light);
}

void obj_light_obj_end(object_t *obj, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)obj->data;
	free_process(map, obj_light->light_node);
	free(obj_light);
}

void obj_light_process(process_t *proc, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)proc->data;
	draw_light(obj_light->light, map);
}

u32 obj_light_display(object_t *obj, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)obj->data;
	return obj_light->display;
}

void new_obj_light(map_t *map, s32 x, s32 y, light_t *light) {
	obj_light_t *obj_light = malloc(sizeof(obj_light_t));
	obj_light->light = light;
	light->x = x<<12; light->y = y<<12;
	// we cache the display characteristics of the light for performance reasons.
	obj_light->display = ('o'<<16) |
		RGB15((light->r * (31<<12))>>24,
		      (light->g * (31<<12))>>24,
		      (light->b * (31<<12))>>24);
	// create the lighting process and store the node for cleanup purposes
	obj_light->light_node = push_process(map,
			obj_light_process, obj_light_proc_end, obj_light);
	// create the worldly presence of the light
	obj_light->obj_node = new_object(map, OT_LIGHT, obj_light);
	// and add it to the map
	insert_object(map, obj_light->obj_node, x, y);
}

u32 random_colour(object_t *obj, map_t *map) {
	return ((map->objtypes[obj->type].ch)<<16) | (genrand_int32()&0xffff);
}

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void displace_object(node_t *obj_node, map_t *map, int dX, int dY) {
	if (!dX && !dY) return; // nothing to see here

	object_t *obj = node_data(obj_node);

	// keep it in the map!
	if (obj->x + dX < 0 || obj->x + dX >= map->w ||
			obj->y + dY < 0 || obj->y + dY >= map->h ||
			opaque(cell_at(map, obj->x + dX, obj->y + dY)))
		dX = dY = 0;

	if (dX || dY) {
		// move the object
		cell_t *cell = cell_at(map, obj->x, obj->y);
		move_object(map, cell, obj_node, obj->x + dX, obj->y + dY);
	}
}

#include "willowisp.h" // evil hacks, i know, but i got sick of scrolling through it

objecttype_t objects[] = {
	// 0: unknown object
	[OT_UNKNOWN] = {
		.ch = '?',
		.col = RGB15(31,31,31),
		.importance = 3,
		.display = random_colour,
		.end = NULL
	},
	// 1: will o' wisp
	[OT_WISP] = {
		.ch = 'o',
		.col = RGB15(7,31,27),
		.importance = 128,
		.display = NULL,
		.end = mon_WillOWisp_obj_end
	},
	// 2: fire
	[OT_FIRE] = {
		.ch = 'w',
		.col = RGB15(31,12,0),
		.importance = 64,
		.display = NULL,
		.end = obj_fire_obj_end
	},
	// 3: light
	[OT_LIGHT] = {
		.ch = 'o',
		.col = 0,
		.importance = 64,
		.display = obj_light_display,
		.end = obj_light_obj_end
	},
};


// does the cell have any objects of the given object type in it?
// XXX: could be optimised by looking at importance and finishing early
bool has_objtype(cell_t *cell, u16 objtype) {
	node_t *k = cell->objects;
	for (; k; k = k->next)
		if (((object_t*)node_data(k))->type == objtype) return true;
	return false;
}


// binds x and y inside the map edges
static inline void bounded(map_t *map, s32 *x, s32 *y) {
	if (*x < 0) *x = 0;
	else if (*x >= map->w) *x = map->w-1;
	if (*y < 0) *y = 0;
	else if (*y >= map->h) *y = map->h-1;
}


void lake(map_t *map, s32 x, s32 y) {
	u32 wisppos = genrand_int32() & 0x3f; // between 0 and 63
	u32 i;
	// scour out a lake by random walk
	for (i = 0; i < 64; i++) {
		cell_t *cell = cell_at(map, x, y);
		if (i == wisppos) // place the wisp
			new_mon_WillOWisp(map, x, y);
		if (!has_objtype(cell, 2)) { // if the cell doesn't have a fire in it
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
		bounded(map, &x, &y);
	}
}

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
		if (i == light1 || i == light2) { // place a fire here
			// fires go on the ground
			if (cell->type != T_GROUND) ground(cell);
			new_obj_fire(map, x, y, 9<<12);
		} else if (i == lakepos) {
			lake(map, x, y);
		} else if (cell->type == T_TREE) { // clear away some tree
			ground(cell);
		}

		u32 a = genrand_int32();
		if (a & 1) { // pick some bits off the number
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

	// update opacity information
	refresh_blockmap(map);
}

void load_map(map_t *map, size_t len, const char *desc) {
	s32 x,y;
	reset_map(map);
	map->objtypes = objects;

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
				new_obj_light(map, x, y,
						new_light(8<<12, 0.39*(1<<12), 0.05*(1<<12), 1.00*(1<<12)));
				break;
			case 'r':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 1.00*(1<<12), 0.07*(1<<12), 0.07*(1<<12)));
				break;
			case 'g':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 1.00*(1<<12), 0.07*(1<<12)));
				break;
			case 'b':
				ground(cell);
				new_obj_light(map, x, y,
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
