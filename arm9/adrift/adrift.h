#ifndef ADRIFT_H
#define ADRIFT_H 1

#include "light.h"
#include "fov.h"
#include "map.h"

enum CELL_TYPE {
	T_TREE = 0,
	T_GROUND,
};

static const cell_t DEFAULT_TREE = {
	T_TREE, RGB15(4,31,1), '*',
	.opaque = true, .forgettable = false
};
static const cell_t DEFAULT_GROUND = {
	T_GROUND, RGB15(17,9,6), '.',
	.opaque = false, .forgettable = false
};

typedef struct player_s {
	node_t *bag;
	node_t *obj;
	light_t *light;
} player_t;

typedef struct game_s {
	fov_settings_type *fov_light;

	player_t *player;

	unsigned int frm;
} game_t;

static inline game_t *game(map_t *map) {
	return (game_t*)map->game;
}

#endif /* ADRIFT_H */
