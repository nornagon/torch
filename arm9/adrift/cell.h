#ifndef CELL_H
#define CELL_H 1

#include <nds/jtypes.h>
#include "list.h"

class Object; class Creature;

struct Cell {
	u16 type;
	List<Object> objects;
	Node<Creature> creature;

	void reset();
};

#endif /* CELL_H */
