#ifndef MAP_H
#define MAP_H 1

#include <nds.h>
#include <string.h>

#include "llpool.h"
#include "fov.h"
#include "process.h"

typedef enum {
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_FIRE,
	T_LIGHT,
	T_WATER,
	T_NONE
} CELL_TYPE;

typedef enum {
	L_FIRE,
	L_GLOWER,
	L_NONE
} LIGHT_TYPE;


// a direction can be stored in 4 bits.
typedef unsigned int DIRECTION;
#define D_NONE  0
#define D_NORTH 1
#define D_SOUTH 2
#define D_EAST  4
#define D_WEST  8
#define D_NORTHEAST (D_NORTH|D_EAST)
#define D_SOUTHEAST (D_SOUTH|D_EAST)
#define D_NORTHWEST (D_NORTH|D_WEST)
#define D_SOUTHWEST (D_SOUTH|D_WEST)

// These are for lighting purposes only. A cell being lit from the north-west is
// different from a cell being lit from the north and the west (due to opaque
// things). D_NORTHWEST in lighting refers to the northwestern *corner* of the
// cell, but D_NORTH_AND_WEST refers to the northern and the western sides being
// lit separately.
#define D_BOTH 0x10
#define D_NORTH_AND_EAST (D_NORTHEAST|D_BOTH)
#define D_SOUTH_AND_EAST (D_SOUTHEAST|D_BOTH)
#define D_NORTH_AND_WEST (D_NORTHWEST|D_BOTH)
#define D_SOUTH_AND_WEST (D_SOUTHWEST|D_BOTH)

// light_t holds information about a specific light source, as well as
// intermediate information used by processes that alter the light source (such
// as flickering)
typedef struct {
	LIGHT_TYPE type : 8;
	s32 x,y; // position in the map
	int32 dx,dy; // position delta (for flickering)
	int32 r,g,b;
	u8 radius;
	s32 dr; // radius delta
	u8 flickered;
} light_t;

// cell_t
typedef struct {
	u8 ch;
	u16 col;
	int32 recall;
	int32 light;
	CELL_TYPE type : 8;
	unsigned int blocked_from : 4;
	bool visible : 1;
	DIRECTION seen_from : 5;
} cell_t;

// a cache_t corresponds to a tile currently being shown on the screen. It's
// used for holding information about what pixels are already there on the
// screen, for purposes of not having to draw it again.
typedef struct {
	int32 lr,lg,lb, // TODO: do these really belong in the cache?
	      last_lr, last_lg, last_lb;
	int32 last_light;
	u16 last_col;
	u8 dirty : 2; // if a cell is marked dirty, it gets drawn. (and dirty gets decremented.)
	bool was_visible : 1;
} cache_t;

// map_t holds map information as well as game state. TODO: perhaps general game
// state should be seperated out into a game_t? Maybe also a player_t.
typedef struct map_s {
	u32 w,h;

	llpool_t *process_pool;
	node_t *processes;
	fov_settings_type *fov_light;

	cell_t* cells;

	cache_t* cache;
	int cacheX, cacheY; // top-left corner of cache. Should be kept positive.
	s32 scrollX, scrollY; // top-left corner of screen. Should be kept positive.

	s32 pX,pY;
	bool torch_on : 1;
} map_t;

// cell at (x,y)
static inline cell_t *cell_at(map_t *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

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

// XXX: these should probably live in another set of files again, leaving
// map.[ch] for just the data structures
static inline bool opaque(cell_t* cell) {
	return cell->type == T_TREE;
}

static inline bool flickers(light_t *light) {
	return light->type == L_FIRE;
}

// request a new process node and add it to the list, returning the process.
process_t *new_process(map_t *map);

// reset the cache
void reset_cache(map_t *map);

// clear everything in the map.
void reset_map(map_t *map);

// alloc space for a new map and return a pointer to it. Calls reset.
map_t *create_map(u32 w, u32 h);

// fill the map with random stuff by the drunkard's walk.
void random_map(map_t *map);

// load a map from a string.
void load_map(map_t *map, size_t len, const char *desc);


#endif /* MAP_H */
