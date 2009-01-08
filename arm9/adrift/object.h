#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>
#include "list.h"

#include "entities/object.h"
#include "direction.h"

struct Object : public listable<Object> {
	POOLED(Object)

	public: // data members
	u16 type;
	u8 quantity;
	DIRECTION orientation;

	public: // functions
	Object(): type(0), quantity(1), orientation(D_NONE) {}
	Object(u16 type);

	inline const ObjectDesc* desc() { return &objectdesc[type]; }

	public: // serialisation
	friend DataStream& operator <<(DataStream&, Object&);
	friend DataStream& operator >>(DataStream&, Object&);
};
DataStream& operator <<(DataStream&, Object&);
DataStream& operator >>(DataStream&, Object&);

// Create and add an object to the map.
Object *addObject(s16 x, s16 y, u16 type, u8 quantity = 1);
// Push the given object into the given container. If the object can stack with
// another object already in the container, increase the quantity of the
// existing object and destroy the pushed object, updating the pointer passed
// to point to the new stack. If stacking would cause overflow, stack up as
// many as possible and split the remainder out into a new object in the bag
// (in fact just decreases the quantity of the object you pass, while
// increasing the quantity of the existing stack). If it can't stack, just
// pushes the object into the container.
void stack_item_push(List<Object> &container, Object *&obj);

#endif /* OBJECT_H */
