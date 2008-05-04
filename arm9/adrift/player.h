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
	Node<Creature> *obj;
	lightsource *light;

	// project an existence into the world
	void exist();

	void move(DIRECTION dir);
	void drop(Node<Object>* obj);

	void use(Node<Object>* item);

	s16 x, y;
};

#endif /* PLAYER_H */
