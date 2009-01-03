#include "cell.h"
#include "object.h"
#include "creature.h"

#include "entities/terrain.h"

void Cell::reset() {
	type = TERRAIN_NONE;
	objects.delete_all();
	creature = NULL;
}

Cell::~Cell() {
	objects.delete_all();
}

DataStream& operator <<(DataStream& s, Cell& c)
{
	s << c.type;
	s << c.objects;
	if (c.creature && c.creature->type != PLAYER) {
		s << true;
		s << *c.creature;
	} else
		s << false;
	return s;
}

DataStream& operator >>(DataStream& s, Cell& c)
{
	s >> c.type;
	s >> c.objects;
	bool has_creature;
	s >> has_creature;
	if (has_creature) {
		c.creature = new Creature;
		s >> *c.creature;
	} else {
		c.creature = NULL;
	}
	return s;
}
