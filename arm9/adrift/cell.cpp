#include "cell.h"
#include "object.h"
#include "creature.h"
#include "adrift.h"

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
	return s;
}

DataStream& operator >>(DataStream& s, Cell& c)
{
	s >> c.type;
	s >> c.objects;
	return s;
}
