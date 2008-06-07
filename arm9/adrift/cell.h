#ifndef CELL_H
#define CELL_H 1

#include <nds/jtypes.h>
#include "list.h"

enum CELL_TYPE {
	T_NONE = 0,
	T_TREE,
	T_GROUND,
	T_GLASS,
	T_WATER,
	T_FIRE,
	MAX_CELL_TYPE
};

struct CellDesc {
	u16 ch, col;
	bool solid : 1;
	bool opaque : 1;
	bool forgettable : 1;
};

extern CellDesc celldesc[];

class Object; class Creature;

struct Cell {
	CELL_TYPE type;
	List<Object> objects;
	Node<Creature> creature;

	void reset();
};

#endif /* CELL_H */
