#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

struct ObjDesc {
	u16 ch, col;
	bool stackable;
	const char *name;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0), quantity(1) {}
	const ObjDesc& desc() { return objdesc[type]; }
	u16 type;
	u8 quantity;
};

void stack_item_push(List<Object> &container, Node<Object>* obj);

// enum { ... };
#define ONLY_ONAMES 1
#include "object.cpp"
#undef ONLY_ONAMES

#endif /* OBJECT_H */
