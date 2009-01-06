#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

#include "entities/object.h"
#include "direction.h"

struct Object : public listable<Object> {
	POOLED(Object)

	public:
	Object(): type(0), quantity(1), orientation(D_NONE) {}
	Object(u16 type);
	const ObjectDesc* desc() { return &objectdesc[type]; }
	u16 type;
	u8 quantity;
	DIRECTION orientation;

	friend DataStream& operator <<(DataStream&, Object&);
	friend DataStream& operator >>(DataStream&, Object&);
};
DataStream& operator <<(DataStream&, Object&);
DataStream& operator >>(DataStream&, Object&);

Object *addObject(s16 x, s16 y, u16 type);
void stack_item_push(List<Object> &container, Object *obj);

#endif /* OBJECT_H */
