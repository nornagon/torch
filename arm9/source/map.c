#include "map.h"
#include <malloc.h>
#include "mem.h"
#include "process.h"
#include "mersenne.h"

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

void free_process_list(map_t *map, node_t **list) {
	while (*list) {
		node_t *next = (*list)->next;
		process_t *p = (process_t*)node_data(*list);
		if (p->end)
			p->end(p, map);
		free_node(map->process_pool, *list);
		*list = next;
	}
}

void reset_map(map_t* map) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = cell_at(map, x, y);
			cell->type = 0;
			cell->ch = 0;
			cell->col = 0;
			cell->light = 0;
			cell->recall = 0;
			cell->visible = 0;
			cell->blocked_from = 0;
			cell->seen_from = 0;

			// clear the object list
			while (cell->objects) {
				node_t *next = cell->objects->next;
				object_t *obj = (object_t*)node_data(cell->objects);
				objecttype_t *type = obj->type;
				if (type->end)
					type->end(obj, map);
				free_node(map->object_pool, cell->objects);
				cell->objects = next;
			}
		}
	// free all the objects
	flush_free(map->object_pool);

	// clear and free the process lists
	free_process_list(map, &map->processes);
	//free_process_list(map, &map->high_processes);
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

void resize_map(map_t *map, u32 w, u32 h) {
	reset_map(map);
	reset_cache(map);
	free(map->cells);
	map->cells = malloc(w*h*sizeof(cell_t));
	// NOTE: we don't reset cells to 0 here.
}

void refresh_blockmap(map_t *map) {
	// each cell needs to know which cells around them are opaque for purposes of
	// direction-aware lighting.

	s32 x,y;
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			unsigned int blocked_from = 0;
			if (y != 0 && cell_at(map, x, y-1)->opaque) blocked_from |= D_NORTH;
			if (y != map->h - 1 && cell_at(map, x, y+1)->opaque) blocked_from |= D_SOUTH;
			if (x != map->w - 1 && cell_at(map, x+1, y)->opaque) blocked_from |= D_EAST;
			if (x != 0 && cell_at(map, x-1, y)->opaque) blocked_from |= D_WEST;
			cell_at(map, x, y)->blocked_from = blocked_from;
		}
}

node_t *_push_process(map_t *map, node_t **stack, process_func process, process_func end, void* data) {
	node_t *node = request_node(map->process_pool);
	process_t *proc = node_data(node);
	proc->process = process;
	proc->end = end;
	proc->data = data;
	push_node(stack, node);
	return node;
}

node_t *new_object(map_t *map, objecttype_t *type, void* data) {
  node_t *node = request_node(map->object_pool);
  object_t *obj = node_data(node);
  obj->type = type;
  obj->data = data;
  return node;
}

void insert_object(map_t *map, node_t *obj_node, s32 x, s32 y) {
	cell_t *target = cell_at(map, x, y);
	object_t *obj = node_data(obj_node);
	objecttype_t *objtype = obj->type;
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
		if (k->type->importance <= objtype->importance)
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

void free_other_processes(map_t *map, process_t *this_proc, node_t *procs[], unsigned int num) {
	for (; num > 0; procs++, num--) {
		if (node_data(*procs) != this_proc)
			free_process(map, *procs);
	}
}

void free_processes(map_t *map, node_t *procs[], unsigned int num) {
	for (; num > 0; procs++, num--)
		free_process(map, *procs);
}

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

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void displace_object(node_t *obj_node, map_t *map, int dX, int dY) {
	if (!dX && !dY) return; // nothing to see here

	object_t *obj = node_data(obj_node);

	// keep it in the map!
	if (obj->x + dX < 0 || obj->x + dX >= map->w ||
			obj->y + dY < 0 || obj->y + dY >= map->h ||
			cell_at(map, obj->x + dX, obj->y + dY)->opaque)
		dX = dY = 0;

	if (dX || dY) {
		// move the object
		cell_t *cell = cell_at(map, obj->x, obj->y);
		move_object(map, cell, obj_node, obj->x + dX, obj->y + dY);
	}
}

// does the cell have any objects of the given object type in it?
// XXX: could be optimised by looking at importance and finishing early
node_t *has_objtype(cell_t *cell, u16 objtype) {
	node_t *k = cell->objects;
	for (; k; k = k->next)
		if (((object_t*)node_data(k))->type == objtype) return k;
	return NULL;
}
