#ifndef ADRIFT_H
#define ADRIFT_H 1

#include "light.h"
#include "fov.h"
#include "map.h"

enum CELL_TYPE {
	T_NONE,
	T_TREE,
	T_GROUND,
	T_GLASS,
};

typedef struct player_s {
	List<Object> bag;
	Node<Object> *obj;
	light_t *light;
} player_t;

typedef struct game_s {
	fov_settings_type *fov_light, *fov_sight;

	player_t *player;

	unsigned int frm;
} game_t;

static inline game_t *game(Map *map) {
	return (game_t*)map->game;
}

#endif /* ADRIFT_H */
