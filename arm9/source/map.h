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
	NO_TYPE
} CELL_TYPE;

typedef enum {
	L_FIRE,
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

/*** light ***/
typedef struct {
	s32 x,y;
	u16 col;
	u8 radius;
	LIGHT_TYPE type : 8;
} light_t;
/*     ~     */

/*** cell ***/
typedef struct {
	u8 ch;
	u16 col;
	int32 light;
	int32 recall;
	CELL_TYPE type : 8;
	u8 dirty : 2;
	bool visible : 1;
	DIRECTION seen_from : 5;
} cell_t;
/*    ~~    */

/*** map ***/
typedef struct {
	u32 w,h;
	cell_t* cells;
	light_t lights[MAX_LIGHTS];
	u8 num_lights;
	s32 pX,pY, scrollX, scrollY;
	bool torch_on : 1;
} map_t;
/*    ~    */

static inline cell_t *cell_at(map_t *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

// clear everything in the map.
void reset_map(map_t *map, CELL_TYPE fill);

// alloc space for a new map and return a pointer to it. Calls reset.
map_t *create_map(u32 w, u32 h, CELL_TYPE fill);

// fill the map with random stuff by the drunkard's walk.
void random_map(map_t *map);

// load a map from a string.
void load_map(map_t *map, size_t len, const char *desc);


#endif /* MAP_H */
