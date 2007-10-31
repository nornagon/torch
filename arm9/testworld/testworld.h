#ifndef TESTWORLD_H
#define TESTWORLD_H 1

#include <nds.h>
#include "light.h"
#include "fov.h"
#include "map.h"

typedef struct player_s {
	node_t *bag;
	node_t *obj;
	light_t *light;
} player_t;

typedef struct game_s {
	fov_settings_type *fov_light;

	player_t *player;

	unsigned int frm; // 'global cooldown' sorta thing
} game_t;

static inline game_t *game(map_t *map) {
	return ((game_t*)map->game);
}

objecttype_t ot_unknown;
objecttype_t *OT_UNKNOWN;

typedef enum {
	T_NONE = 0,
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_WATER,
} CELL_TYPE;

map_t *init_test();

#endif /* TESTWORLD_H */
