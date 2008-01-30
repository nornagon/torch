#include "map.h"
#include <malloc.h>
#include "mem.h"
#include "process.h"
#include "mersenne.h"

Map::Map(s32 w_, s32 h_) {
	w = w_; h = h_;

	handler = NULL;
	game = NULL;

	cells = new Cell[w*h];
	memset(cells, 0, w*h*sizeof(Cell));
	cache = new cache_t[32*24]; // screen sized
	memset(cache, 0, 32*24*sizeof(cache_t));

	pX = pY = 0;
	scrollX = scrollY = 0;

	reset();
	reset_cache();
}

void Map::reset_cache() {
	u32 x, y;
	for (y = 0; y < 24; y++)
		for (x = 0; x < 32; x++) {
			cache_t* c = &cache[y*32+x];
			c->lr = c->lg = c->lb = 0;
			c->last_lr = c->last_lg = c->last_lb = 0;
			c->last_light = 0;
			c->last_col = 0;
			c->last_col_final = 0;
			c->dirty = 0;
			c->was_visible = 0;
		}
	// note that the cache is origin-agnostic, so the top-left corner of cache
	// when scrollX = 0 does not have to correspond with cacheX = 0. The caching
	// mechanisms will deal just fine whereever the origin is. Really, we don't
	// need to set cacheX or cacheY here, but we do (just in case something weird
	// happened to them)
	cacheX = cacheY = 0;
}

void free_process_list(Map *map, List<process_t> &list) {
	while (list.head) {
		Node<process_t> *node = list.pop();
		process_t *p = *node;
		if (p->end)
			p->end(p, map);
		node->free();
	}
}

void Map::reset() {
	s32 x, y;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			Cell* cell = at(x, y);

			// clear the object list
			while (cell->objects.head) {
				Node<Object> *node = cell->objects.pop();
				Object *obj = *node;
				ObjType *type = obj->type;
				if (type->end)
					type->end(obj, this);
				node->free();
			}
			memset(cell, 0, sizeof(Cell));
		}
	// free all the objects
	Node<Object>::pool.flush_free();

	// clear and free the process lists
	free_process_list(this, processes);
	Node<process_t>::pool.flush_free();
}

void resize_map(Map *map, u32 w, u32 h) {
	map->reset();
	map->reset_cache();
	free(map->cells);
	map->cells = new Cell[w*h];
	// NOTE: we don't reset cells to 0 here.
}

void refresh_blockmap(Map *map) {
	// each cell needs to know which cells around them are opaque for purposes of
	// direction-aware lighting.

	s32 x,y;
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			unsigned int blocked_from = 0;
			if (y != 0 && map->at(x, y-1)->opaque) blocked_from |= D_NORTH;
			if (y != map->h - 1 && map->at(x, y+1)->opaque) blocked_from |= D_SOUTH;
			if (x != map->w - 1 && map->at(x+1, y)->opaque) blocked_from |= D_EAST;
			if (x != 0 && map->at(x-1, y)->opaque) blocked_from |= D_WEST;
			map->at(x, y)->blocked_from = blocked_from;
		}
}

Node<process_t> *push_process(Map *map, process_func process, process_func end, void* data) {
	Node<process_t> *node = Node<process_t>::pool.request_node();
	process_t *proc = *node;
	proc->process = process;
	proc->end = end;
	proc->data = data;
	proc->counter = 0;
	map->processes.push(node);
	return node;
}

Node<Object> *new_object(Map *map, ObjType *type, void* data) {
  Node<Object> *node = Node<Object>::pool.request_node();
  Object *obj = *node;
  obj->type = type;
  obj->data = data;
  obj->quantity = 1;
  return node;
}

void insert_object(Map *map, Node<Object> *obj_node, s32 x, s32 y) {
	Cell *target = map->at(x, y);
	Object *obj = *obj_node;
	ObjType *objtype = obj->type;
	obj->x = x;
	obj->y = y;
	Node<Object> *prev = NULL;
	Node<Object> *head = target->objects.head;
	// walk through the list
	while (head) {
		Object *k = *head;
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
		target->objects.head = obj_node;
}

void move_object(Map *map, Node<Object> *obj_node, s32 x, s32 y) {
	Object *obj = *obj_node;
	Cell *loc = map->at(obj->x, obj->y);
	loc->objects.remove(obj_node);
	insert_object(map, obj_node, x, y);
}

void free_other_processes(Map *map, process_t *this_proc, Node<process_t> *procs[], unsigned int num) {
	for (; num > 0; procs++, num--) {
		if (**procs != this_proc)
			free_process(map, *procs);
	}
}

void free_processes(Map *map, Node<process_t> *procs[], unsigned int num) {
	for (; num > 0; procs++, num--)
		free_process(map, *procs);
}

void free_object(Map *map, Node<Object> *obj_node) {
	Object *obj = *obj_node;
	Cell *cell = map->at(obj->x, obj->y);
	cell->objects.remove(obj_node);
	obj_node->free();
}

// free num objects, beginning at objs. that's an *array* of node pointers, not
// the head of a list.
void free_objects(Map *map, Node<Object> *objs[], unsigned int num) {
	for (; num > 0; objs++, num--)
		(*objs)->free();
}

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void displace_object(Node<Object> *obj_node, Map *map, int dX, int dY) {
	if (!dX && !dY) return; // nothing to see here

	Object *obj = *obj_node;

	// keep it in the map!
	if (obj->x + dX < 0 || obj->x + dX >= map->w ||
			obj->y + dY < 0 || obj->y + dY >= map->h ||
			map->at(obj->x + dX, obj->y + dY)->opaque)
		dX = dY = 0;

	if (dX || dY) {
		// move the object
		move_object(map, obj_node, obj->x + dX, obj->y + dY);
	}
}

// does the cell have any objects of the given object type in it?
// XXX: could be optimised by looking at importance and finishing early
Node<Object> *has_objtype(Cell *cell, ObjType *objtype) {
	Node<Object> *k = cell->objects.head;
	for (; k; k = k->next)
		if (((Object*)*k)->type == objtype) return k;
	return NULL;
}
