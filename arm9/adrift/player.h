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

	void move(DIRECTION dir, bool run);
	void drop(Node<Object> obj);

	void use(Node<Object> item);
	void setprojectile(Node<Object> proj);
	void chuck(s16 destx, s16 desty);

	s16 x, y;
};

#endif /* PLAYER_H */
