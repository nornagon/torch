#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

enum OBJ_TYPE {
	J_NONE,
	J_ROCK,
};

struct ObjDesc {
	u16 ch, col;
	bool stackable;
	char *name;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0), quantity(1) {}
	u16 type;
	u8 quantity;
};

void stack_item_push(List<Object> &container, Node<Object>* obj);

#endif /* OBJECT_H */
