#ifndef PLAYER_H
#define PLAYER_H 1

#include "list.h"
#include "object.h"
#include "creature.h"
#include "lightsource.h"
#include "direction.h"
#include <nds/jtypes.h>

struct Player {
	List<Object> bag;
	Node<Creature> obj;
	Node<Creature> target;
	lightsource *light;

	Node<Object> projectile;

	// project an existence into the world
	void exist();

	void clear();

	void move(DIRECTION dir, bool run);

	void drop(Node<Object> item);
	void use(Node<Object> item);
	void eat(Node<Object> item);
	void drink(Node<Object> item);

	void setprojectile(Node<Object> proj);
	void chuck(s16 destx, s16 desty);

	s16 x, y;

	s32 strength_xp, agility_xp, resilience_xp, aim_xp, melee_xp;
	void exercise_strength(int n = 1);
	void exercise_agility(int n = 1);
	void exercise_resilience(int n = 1);
	void exercise_aim(int n = 1);
	void exercise_melee(int n = 1);
};

#endif /* PLAYER_H */
