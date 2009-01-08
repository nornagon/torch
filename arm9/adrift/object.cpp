#include "object.h"
#include "adrift.h"

Object::Object(u16 _type): type(_type), quantity(1), orientation(D_NONE)
{
}

Object *addObject(s16 x, s16 y, u16 type, u8 quantity) {
	Object *on = new Object(type);
	on->quantity = quantity;
	stack_item_push(game.map.at(x,y)->objects, on);
	return on;
}

void stack_item_push(List<Object> &container, Object *&obj) {
	Object *contobj = container.head();
	for (; contobj; contobj = contobj->next()) {
		if (contobj->type == obj->type && obj->desc()->stackable)
			break;
	}
	if (contobj) {
		int overflow = (int)contobj->quantity + (int)obj->quantity - 255;
		if (overflow > 0) {
			obj->quantity = overflow;
			contobj->quantity = 255;
			container.push(obj);
		} else {
			contobj->quantity += obj->quantity;
			delete obj;
			obj = contobj;
		}
	} else {
		container.push(obj);
	}
}

DataStream& operator <<(DataStream& s, Object &o)
{
	s << o.type << o.quantity;
	s << o.orientation;
	return s;
}

DataStream& operator >>(DataStream& s, Object &o)
{
	s >> o.type >> o.quantity;
	s >> o.orientation;
	return s;
}
