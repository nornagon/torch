#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

#include "entities/object.h"
#include "direction.h"

struct Object : public listable<Object> {
	Object(): type(0), quantity(1), state(NULL) {}
	const ObjectDesc* desc() { return &objectdesc[type]; }
	u16 type;
	u16 quantity;
	void *state;
	DIRECTION orientation;
};

Object *addObject(s16 x, s16 y, u16 type);
void stack_item_push(List<Object> &container, Object *obj);

#endif /* OBJECT_H */
