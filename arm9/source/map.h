#ifndef MAP_H
#define MAP_H 1

#include <nds.h>
#include <string.h>

#define MAX_LIGHTS 32

typedef enum {
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_FIRE,
	T_LIGHT,
	NO_TYPE
} CELL_TYPE;

typedef enum {
	L_FIRE,
	L_GLOWER,
	NO_LIGHT
} LIGHT_TYPE;

#define D_NONE  0
#define D_NORTH 1
#define D_SOUTH 2
#define D_EAST  4
#define D_WEST  8
#define D_NORTHEAST (D_NORTH|D_EAST)
#define D_SOUTHEAST (D_SOUTH|D_EAST)
#define D_NORTHWEST (D_NORTH|D_WEST)
#define D_SOUTHWEST (D_SOUTH|D_WEST)

#define D_BOTH 0x10
#define D_NORTH_AND_EAST (D_NORTHEAST|D_BOTH)
#define D_SOUTH_AND_EAST (D_SOUTHEAST|D_BOTH)
#define D_NORTH_AND_WEST (D_NORTHWEST|D_BOTH)
#define D_SOUTH_AND_WEST (D_SOUTHWEST|D_BOTH)
typedef unsigned int DIRECTION;

/*************/
/*** light ***/
typedef struct {
	s32 x,y; // position in the map
	int32 dx,dy; // position delta (for flickering)
	int32 r,g,b;
	u8 radius;
	s32 dr; // radius delta
	LIGHT_TYPE type : 8;
} light_t;

/************/
/*** cell ***/
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

/*************/
/*** cache ***/
typedef struct {
	int32 lr,lg,lb, // TODO: do these really belong in the cache?
	      last_lr, last_lg, last_lb;
	int32 last_light;
	u16 last_col;
	u8 dirty : 2;
	bool was_visible : 1;
} cache_t;

/***********/
/*** map ***/
typedef struct {
	u32 w,h;
	cell_t* cells;
	cache_t* cache;
	int cacheX, cacheY; // top-left corner of cache. Should be kept positive.
	light_t lights[MAX_LIGHTS];
	u8 num_lights;
	s32 pX,pY, scrollX, scrollY;
	bool torch_on : 1;
} map_t;

// cell at (x,y)
static inline cell_t *cell_at(map_t *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

// cache for map cell at (x,y)
static inline cache_t *cache_at(map_t *map, int x, int y) {
	x -= map->scrollX;
	y -= map->scrollY;
	x += map->cacheX;
	y += map->cacheY;
	if (x >= 32) x -= 32;
	if (y >= 24) y -= 24;
	return &map->cache[y*32+x];
}

// XXX: these should probably live in another set of files again, leaving
// map.[ch] for just the data structures
static inline bool opaque(cell_t* cell) {
	return cell->type == T_TREE;
}

static inline bool flickers(light_t *light) {
	return light->type == L_FIRE;
}

// reset the cache
void reset_cache(map_t *map);

// clear everything in the map.
void reset_map(map_t *map, CELL_TYPE fill);

// alloc space for a new map and return a pointer to it. Calls reset.
map_t *create_map(u32 w, u32 h, CELL_TYPE fill);

// fill the map with random stuff by the drunkard's walk.
void random_map(map_t *map);

// load a map from a string.
void load_map(map_t *map, size_t len, const char *desc);


#endif /* MAP_H */

