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

struct Player {
	List<Object> bag;
	Node<Object> *obj;
	light_t *light;
};

struct Game {
	Game();
	Player player;

	fov_settings_type *fov_light, *fov_sight;

	unsigned int frm;
};

extern Game game;

#endif /* ADRIFT_H */
