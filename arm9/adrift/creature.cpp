#include "creature.h"
#include "adrift.h"

void Creature::move(s16 _x, s16 _y) {
	// XXX omg hax
	Cell *c = game.map.at(x,y);
	Node<Creature> selfnode = c->creature;
	c->creature = NULL;
	game.map.at(_x,_y)->creature = selfnode;
	setPos(_x,_y);
}
