#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

#include "entities/object.h"

struct Object {
	Object(): type(0), quantity(1) {}
	const ObjectDesc* desc() { return &objectdesc[type]; }
	u16 type;
	u16 quantity;
};

void stack_item_push(List<Object> &container, Node<Object> obj);

#endif /* OBJECT_H */
