#include "buf.h"

torchbuf::torchbuf() {
	w = 0; h = 0;
	map = 0;
	light = 0;
}

torchbuf::torchbuf(s16 w, s16 h) {
	resize(w,h);
}

void torchbuf::resize(s16 _w, s16 _h) {
	w = _w;
	h = _h;

	if (map) delete [] map;
	if (light) delete [] light;

	map = new mapel[w*h];
	light = new luxel[32*24]; // TODO: remove constants

	scroll.reset();
	cache.reset();
}

void torchbuf::reset() {
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			at(x,y)->reset();

	for (int y = 0; y < 24; y++)
		for (int x = 0; x < 32; x++)
			light[y*32+x].reset();
}

/*

void Map::reset() {
	for (s32 y = 0; y < h; y++)
		for (s32 x = 0; x < w; x++) {
			Cell* cell = at(x, y);

			// clear the object list
			while (cell->objects.head) {
				Node<Object> *node = cell->objects.pop();
				Object *obj = *node;
				ObjType *type = obj->type;
				if (type->end)
					type->end(obj);
				node->free();
			}
			memset(cell, 0, sizeof(Cell));
		}
	// free all the objects
	Node<Object>::pool.flush_free();

	// clear and free the process lists
	free_process_list(processes);
	Node<Process>::pool.flush_free();
}

void Map::resize(u32 _w, u32 _h) {
	reset();
	reset_cache();
	w = _w; h = _h;
	if (cells) delete[] cells;
	cells = new Cell[w*h];
}

void Map::refresh_blockmap() {
	// each cell needs to know which cells around them are opaque for purposes of
	// direction-aware lighting.

	s32 x,y;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			unsigned int blocked_from = 0;
			if (y != 0 && at(x, y-1)->opaque) blocked_from |= D_NORTH;
			if (y != h - 1 && at(x, y+1)->opaque) blocked_from |= D_SOUTH;
			if (x != w - 1 && at(x+1, y)->opaque) blocked_from |= D_EAST;
			if (x != 0 && at(x-1, y)->opaque) blocked_from |= D_WEST;
			at(x, y)->blocked_from = blocked_from;
		}
}

Node<Process> *Map::push_process(process_func process, process_func end, void* data) {
	Node<Process> *node = Node<Process>::pool.request_node();
	Process *proc = *node;
	proc->process = process;
	proc->end = end;
	proc->data = data;
	proc->counter = 0;
	processes.push(node);
	return node;
}

Node<Object> *Map::new_object(ObjType *type, void* data) {
  Node<Object> *node = Node<Object>::pool.request_node();
  Object *obj = *node;
  obj->type = type;
  obj->data = data;
  obj->quantity = 1;
  return node;
}

void Map::insert_object(Node<Object> *obj_node, s32 x, s32 y) {
	Cell *target = at(x, y);
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

void Map::move_object(Node<Object> *obj_node, s32 x, s32 y) {
	Object *obj = *obj_node;
	Cell *loc = at(obj->x, obj->y);
	loc->objects.remove(obj_node);
	insert_object(obj_node, x, y);
}

void Map::free_processes(Node<Process> *procs[], unsigned int num) {
	for (; num > 0; procs++, num--)
		free_process(*procs);
}

void Map::free_object(Node<Object> *obj_node) {
	Object *obj = *obj_node;
	Cell *cell = at(obj->x, obj->y);
	cell->objects.remove(obj_node);
	obj_node->free();
}

// free num objects, beginning at objs. that's an *array* of node pointers, not
// the head of a list.
void free_objects(Node<Object> *objs[], unsigned int num) {
	for (; num > 0; objs++, num--)
		(*objs)->free();
}

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void Map::displace_object(Node<Object> *obj_node, int dX, int dY) {
	if (!dX && !dY) return; // nothing to see here

	Object *obj = *obj_node;

	// keep it in the map!
	if (obj->x + dX < 0 || obj->x + dX >= w ||
			obj->y + dY < 0 || obj->y + dY >= h ||
			at(obj->x + dX, obj->y + dY)->opaque)
		dX = dY = 0;

	if (dX || dY) {
		// move the object
		move_object(obj_node, obj->x + dX, obj->y + dY);
	}
}

// does the cell have any objects of the given object type in it?
// XXX: could be optimised by looking at importance and finishing early
Node<Object> *has_objtype(Cell *cell, ObjType *objtype) {
	Node<Object> *k = cell->objects.head;
	for (; k; k = k->next)
		if (((Object*)*k)->type == objtype) return k;
	return NULL;
}*/
