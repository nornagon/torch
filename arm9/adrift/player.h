#ifndef PLAYER_H
#define PLAYER_H 1

#include <nds/jtypes.h>
#include "list.h"
#include "object.h"
#include "creature.h"
#include "lightsource.h"
#include "direction.h"
#include "datastream.h"

struct Player : public Creature {
	UNPOOLED

	public:
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
	// cooldown. XXX Important that this be called rather than
	// game.player.move(), which will fall through to Creature::move and explode
	// if you start passing it DIRECTIONs and bools.
	void moveDir(DIRECTION dir, bool run);

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

	friend DataStream& operator <<(DataStream&, Player&);
	friend DataStream& operator >>(DataStream&, Player&);
};
DataStream& operator <<(DataStream&, Player&);
DataStream& operator >>(DataStream&, Player&);

#endif /* PLAYER_H */
