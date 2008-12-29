#ifndef PLAYER_H
#define PLAYER_H 1

#include "list.h"
#include "object.h"
#include "creature.h"
#include "lightsource.h"
#include "direction.h"
#include <nds/jtypes.h>

struct Player : public Creature {
	List<Object> bag; // inventory
	Creature *target; // currently targetted creature for seek and destroy
	lightsource *light;

	// what the player is ready to throw. a reference to some object in the
	// player's bag -- make sure to check they haven't dropped it or similar
	// before throwing!
	Object *projectile;

	// initialise and create a new light source.
	void exist();

	// reset the player struct, freeing all allocated memory
	void clear();

	// move in given direction, sliding along obstacles and incrementing
	// cooldown.
	void move(DIRECTION dir, bool run);
	// set the player's position
	//void setPos(s16 x, s16 y);

	void drop(Object *item);
	void use(Object *item);
	void eat(Object *item);
	void drink(Object *item);

	// 'throw' was reserved :(
	void chuck(s16 destx, s16 desty);

	s32 strength_xp, agility_xp, resilience_xp, aim_xp, melee_xp;
	void exercise_strength(int n = 1);
	void exercise_agility(int n = 1);
	void exercise_resilience(int n = 1);
	void exercise_aim(int n = 1);
	void exercise_melee(int n = 1);
};

#endif /* PLAYER_H */
