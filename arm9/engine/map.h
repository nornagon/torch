#ifndef MAP_H
#define MAP_H 1

#include <nds.h>
#include <string.h>

#include "list.h"
#include "process.h"
#include "cache.h"
#include "object.h"

#include "direction.h"

// cell_t
typedef struct {
	u16 type;
	u16 col;
	u8 ch;

	u8 recalled_ch;
	u16 recalled_col;

	// can light pass through the cell?
	bool opaque : 1;

	bool forgettable : 1;

	// blocked_from is cache for working out which sides of an opaque cell we
	// should light
	unsigned int blocked_from : 4;

	// visible is true if the cell is on-screen *and* in the player's LOS
	bool visible : 1;

	// which direction is the player seeing this cell from?
	DIRECTION seen_from : 5;

	int16 light; // total intensity of the light falling on this cell
	int16 recall; // how much the player remembers this cell

	// what things are here? A ball of string and some sealing wax.
	List<Object> objects;
} cell_t;

// map_t holds map information as well as game state.
struct Map {
	s32 w,h;

	Map(s32 w, s32 h);
	void reset();
	void reset_cache();

	List<process_t> processes;

	void (*handler)(struct Map *map);

	cell_t* cells;

	cache_t* cache;
	int cacheX, cacheY; // top-left corner of cache. Should be kept positive.
	s32 scrollX, scrollY; // top-left corner of screen. Should be kept positive.

	s32 pX, pY;
	void* game; // game-specific data structure

	inline cell_t *at(s32 x, s32 y) {
		return &cells[y*w+x];
	}

	inline void bounded(s32 &x, s32 &y) {
		if (x < 0) x = 0;
		else if (x >= w) x = w-1;
		if (y < 0) y = 0;
		else if (y >= h) y = h-1;
	}
};

// cell at (x,y)
static inline cell_t *cell_at(Map *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

// binds x and y inside the map edges
static inline void bounded(Map *map, s32 *x, s32 *y) {
	if (*x < 0) *x = 0;
	else if (*x >= map->w) *x = map->w-1;
	if (*y < 0) *y = 0;
	else if (*y >= map->h) *y = map->h-1;
}

// alloc space for a new map and return a pointer to it.
Map *create_map(u32 w, u32 h);

// resize the map by resetting, then freeing and reallocating all the cells.
void resize_map(Map *map, u32 w, u32 h);

// push a new process on the process stack, returning the new process node.
Node<process_t> *push_process(Map *map, process_func process, process_func end, void* data);

// remove the process proc from the process list and add it to the free pool.
static inline void free_process(Map *map, Node<process_t> *proc) {
	map->processes.remove(proc);
	proc->free();
}

// free all processes
void free_processes(Map *map, Node<process_t> *procs[], unsigned int num);

// free all processes that aren't the one specified.
// procs should be a pointer to an *array* of nodes, not the head of a list. num
// should be the number of nodes in the array.
void free_other_processes(Map *map, process_t *this_proc, Node<process_t> *procs[], unsigned int num);

// create a new object. this doesn't add the object to any lists, so you'd
// better do it yourself. returns the object node created.
Node<Object> *new_object(Map *map, ObjType *type, void* data);

// remove the object from its owning cell, and add the node to the free pool
void free_object(Map *map, Node<Object> *obj_node);

// perform an insert of obj into the map cell at (x,y) sorted by importance.
// insert_object walks the list, so it's O(n), but it's better in the case of
// inserting a high-importance object (because fewer comparisons of importance
// have to be made)
void insert_object(Map *map, Node<Object> *obj, s32 x, s32 y);

// remove obj from its cell and add it to the cell at (x,y).
void move_object(Map *map, Node<Object> *obj, s32 x, s32 y);

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void displace_object(Node<Object> *obj_node, Map *map, int dX, int dY);

// will return the first instance of an object of type objtype it comes across
// in the given cell, or NULL if there are none.
Node<Object> *has_objtype(cell_t *cell, ObjType *objtype);

// cache for *screen* coordinates (x,y).
static inline cache_t *cache_at_s(Map *map, int x, int y) {
	// Because we don't want to move data in the cache around every time the
	// screen scrolls, we're going to move the origin instead. The cache at
	// (cacheX, cacheY) corresponds to the cell in the top-left corner of the
	// screen. When we want to scroll one tile to the right, we move the origin of
	// the cache one tile to the right, and mark all the cells on the right edge
	// as dirty. If (cacheX, cacheY) were (0, 0) before the scroll, afterwards
	// they will be (1, 0). Note that this means the right edge of the screen
	// would normally be outside of the cache memory -- so we 'wrap' around the
	// right edge of cache memory and the column cacheX = 0 now represents the
	// right edge of the screen. Similarly for the left edges and scrolling left,
	// and for the top and bottom edges.
	x += map->cacheX;
	y += map->cacheY;
	if (x >= 32) x -= 32;
	if (y >= 24) y -= 24;
	return &map->cache[y*32+x];
}

// cache for cell at map coordinates (x,y) (NOT screen coordinates!)
static inline cache_t *cache_at(Map *map, int x, int y) {
	return cache_at_s(map, x - map->scrollX, y - map->scrollY);
}

// reset the cache
void reset_cache(Map *map);

// clear everything in the map.
void reset_map(Map *map);

// refresh cells' awareness of their surroundings
void refresh_blockmap(Map *map);

#endif /* MAP_H */
