#ifndef CELL_H
#define CELL_H 1

#include <nds/jtypes.h>
#include "list.h"
#include "entities/terrain.h"
#include "datastream.h"

class Object; class Creature;

struct Cell {
	u16 type;
	List<Object> objects;
	Creature *creature;

	TerrainDesc *desc() { return &terraindesc[type]; }

	void reset();

	~Cell();

	friend DataStream& operator <<(DataStream&, Cell&);
	friend DataStream& operator >>(DataStream&, Cell&);
};
DataStream& operator <<(DataStream&, Cell&);
DataStream& operator >>(DataStream&, Cell&);

#endif /* CELL_H */
