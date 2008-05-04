#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

enum OBJ_TYPE {
	J_NONE,
	J_ROCK,
};

#define CAN_USE   0x01
#define CAN_EAT   0x02
#define CAN_EQUIP 0x04

enum ACTION {
	ACT_NONE,
	ACT_DROP,
	ACT_THROW,
	ACT_EQUIP,
	ACT_EAT,
	ACT_USE,
};

struct ObjDesc {
	u16 ch, col;
	bool stackable;
	char *name;
	u16 abilities;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0), quantity(1) {}
	const ObjDesc& desc() { return objdesc[type]; }
	u16 type;
	u8 quantity;
};

void stack_item_push(List<Object> &container, Node<Object>* obj);

#endif /* OBJECT_H */
