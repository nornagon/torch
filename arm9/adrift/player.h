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

	public: // data members
	Creature *target; // currently targetted creature for seek and destroy
	// overrides the light in Creature, since the player light is Different(TM).
	// In particular, the player's light is not part of the game.map.lights list,
	// since for performance reasons it is calculated during initial sight FOV
	// traversal in order to avoid a second identical FOV calculation for the
	// player light as opposed to the player's sight (i.e. what the player can
	// and cannot see).
	lightsource *light;

	// what the player is ready to throw. a reference to some object in the
	// player's bag -- make sure to check they haven't dropped it or similar
	// before throwing!
	Object *projectile;

	s32 strength_xp, agility_xp, resilience_xp, aim_xp, melee_xp;

	public: // functions
	Player();

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
