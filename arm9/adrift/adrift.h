#ifndef ADRIFT_H
#define ADRIFT_H 1

#ifdef NATIVE
#include "native.h"
#endif

#include "map.h"
#include "fov.h"
#include "creature.h"
#include "player.h"

struct Adrift {
	Adrift();
	Player player;
	Map map;

	fov_settings_type *fov_light, *fov_sight;

	int cooldown;

	void save(const char *filename);
	void load(const char *filename);
};

extern Adrift game;

#endif /* ADRIFT_H */
