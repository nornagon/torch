#ifndef CELL_H
#define CELL_H 1

#include <nds/jtypes.h>
#include "list.h"
#include "entities/terrain.h"

class Object; class Creature;

struct Cell {
	u16 type;
	List<Object> objects;
	Creature *creature;

	TerrainDesc *desc() { return &terraindesc[type]; }

	void reset();
};

#endif /* CELL_H */
