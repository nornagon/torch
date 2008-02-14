#ifndef CELL_H
#define CELL_H 1

#include <nds.h>
#include "list.h"

enum CELL_TYPE {
	T_NONE = 0,
	T_TREE,
	T_GROUND,
	T_GLASS,
	T_WATER,
	MAX_CELL_TYPE
};

struct CellDesc {
	u16 ch, col;
	bool solid : 1;
	bool opaque : 1;
};

extern CellDesc celldesc[];

class Object; class Creature;

struct Cell {
	CELL_TYPE type;
	List<Object> objs;
	List<Creature> creatures;
};

#endif /* CELL_H */
