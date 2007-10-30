#ifndef TESTWORLD_H
#define TESTWORLD_H 1

#include <nds.h>
#include "light.h"
#include "fov.h"
#include "map.h"

//-------------{{{ game struct-----------------------------------------------
typedef struct game_s {
	light_t *player_light;
	bool torch_on;
	fov_settings_type *fov_light;

	unsigned int frm; // 'global cooldown' sorta thing
} game_t;

static inline game_t *game(map_t *map) {
	return ((game_t*)map->game);
}
//---------------------------}}}---------------------------------------------

typedef enum {
	T_NONE = 0,
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_WATER,
} CELL_TYPE;

map_t *init_test();

#endif /* TESTWORLD_H */
