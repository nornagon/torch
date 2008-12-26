#include "object.h"
#include "adrift.h"

Node<Object> addObject(s16 x, s16 y, u16 type) {
	Node<Object> on(new NodeV<Object>);
	on->type = type;
	stack_item_push(game.map.at(x,y)->objects, on);
	return on;
}

void stack_item_push(List<Object> &container, Node<Object> obj) {
	Node<Object> contobj = container.top();
	for (; contobj; contobj = contobj.next()) {
		if (contobj->type == obj->type && objectdesc[obj->type].stackable)
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
