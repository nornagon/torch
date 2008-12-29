#include "object.h"
#include "adrift.h"

Object *addObject(s16 x, s16 y, u16 type) {
	Object *on = new Object;
	on->type = type;
	stack_item_push(game.map.at(x,y)->objects, on);
	return on;
}

void stack_item_push(List<Object> &container, Object *obj) {
	Object *contobj = container.head();
	for (; contobj; contobj = contobj->next()) {
		if (contobj->type == obj->type && obj->desc()->stackable)
			break;
	}
	if (contobj) {
		if ((int)contobj->quantity + (int)obj->quantity > 255) {
			int overflow = (int)contobj->quantity + (int)obj->quantity - 255;
			obj->quantity = overflow;
			contobj->quantity = 255;
		} else {
			contobj->quantity += obj->quantity;
			obj = 0;
		}
	}
	if (obj) container.push(obj);
}
