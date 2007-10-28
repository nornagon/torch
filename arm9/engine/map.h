#ifndef MAP_H
#define MAP_H 1

#include <nds.h>
#include <string.h>

#include "list.h"
#include "process.h"
#include "cache.h"
#include "object.h"

#include "direction.h"

typedef enum {
	T_NONE = 0,
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_WATER,
} CELL_TYPE;

// cell_t
typedef struct {
	u16 type;
	u8 ch;
	u16 col;

	int32 light; // total intensity of the light falling on this cell
	int32 recall; // how much the player remembers this cell XXX: could probably be a u16?

	// blocked_from is cache for working out which sides of an opaque cell we
	// should light
	unsigned int blocked_from : 4;

	// visible is true if the cell is on-screen *and* in the player's LOS
	bool visible : 1;
	// which direction is the player seeing this cell from?
	DIRECTION seen_from : 5;

	// can light pass through the cell?
	bool opaque : 1;

	// what things are here? A ball of string and some sealing wax.
	node_t *objects;
} cell_t;

// map_t holds map information as well as game state. TODO: perhaps general game
// state should be seperated out into a game_t? Maybe also a player_t.
typedef struct map_s {
	u32 w,h;

	llpool_t *process_pool;
	node_t *high_processes; // important things that need to be run first
	node_t *processes;

	llpool_t *object_pool;

	cell_t* cells;

	cache_t* cache;
	int cacheX, cacheY; // top-left corner of cache. Should be kept positive.
	s32 scrollX, scrollY; // top-left corner of screen. Should be kept positive.

	s32 pX, pY;
	void* game; // game-specific data structure
} map_t;

// cell at (x,y)
static inline cell_t *cell_at(map_t *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

// binds x and y inside the map edges
static inline void bounded(map_t *map, s32 *x, s32 *y) {
	if (*x < 0) *x = 0;
	else if (*x >= map->w) *x = map->w-1;
	if (*y < 0) *y = 0;
	else if (*y >= map->h) *y = map->h-1;
}

// alloc space for a new map and return a pointer to it.
map_t *create_map(u32 w, u32 h);

// resize the map by resetting, then freeing and reallocating all the cells.
void resize_map(map_t *map, u32 w, u32 h);

// push a new process on the process stack, returning the new process node.
node_t *_push_process(map_t *map, node_t **proc_stack,
		process_func process, process_func end, void* data);

// push a normal-priority process on the map's process stack
static inline node_t *push_process(map_t *map, process_func process, process_func end, void* data) {
	return _push_process(map, &map->processes, process, end, data);
}

// push a high-priority process on the map's high process stack
static inline node_t *push_high_process(map_t *map, process_func process, process_func end, void* data) {
	return _push_process(map, &map->high_processes, process, end, data);
}

// remove the normal-priority process proc from the process list and add it to
// the free pool.
static inline void free_process(map_t *map, node_t *proc) {
	map->processes = remove_node(map->processes, proc);
	free_node(map->process_pool, proc);
}

// free all the normal-priority processes
void free_processes(map_t *map, node_t *procs[], unsigned int num);

// free normal-priority processes that aren't the one specified.
// procs should be a pointer to an *array* of nodes, not the head of a list. num
// should be the number of nodes in the array.
void free_other_processes(map_t *map, process_t *this_proc, node_t *procs[], unsigned int num);

// create a new object. this doesn't add the object to any lists, so you'd
// better do it yourself. returns the object node created.
node_t *new_object(map_t *map, objecttype_t *type, void* data);

// remove the object from its owning cell, and add the node to the free pool
void free_object(map_t *map, node_t *obj_node);

// perform an insert of obj into the map cell at (x,y) sorted by importance.
// insert_object walks the list, so it's O(n), but it's better in the case of
// inserting a high-importance object (because fewer comparisons of importance
// have to be made)
void insert_object(map_t *map, node_t *obj, s32 x, s32 y);

// remove obj from loc and add it to the cell at (x,y).
void move_object(map_t *map, cell_t *loc, node_t *obj, s32 x, s32 y);

// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
// etc.
void displace_object(node_t *obj_node, map_t *map, int dX, int dY);

// will return the first instance of an object of type objtype it comes across
// in the given cell, or NULL if there are none.
node_t *has_objtype(cell_t *cell, objecttype_t *objtype);

// cache for *screen* coordinates (x,y).
static inline cache_t *cache_at_s(map_t *map, int x, int y) {
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
static inline cache_t *cache_at(map_t *map, int x, int y) {
	return cache_at_s(map, x - map->scrollX, y - map->scrollY);
}

// reset the cache
void reset_cache(map_t *map);

// clear everything in the map.
void reset_map(map_t *map);

// refresh cells' awareness of their surroundings
void refresh_blockmap(map_t *map);

#endif /* MAP_H */
