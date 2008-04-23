#include "object.h"
#include <nds/arm9/video.h>

ObjDesc objdesc[] = { /* stk  name      abilities*/
	{ 'X', RGB15(31,31,0),  1, "unknown", 0 },
	{ '8', RGB15(18,18,18), 1, "rock",    ABIL_EQUIP },
};

void stack_item_push(List<Object> &container, Node<Object>* obj) {
	Node<Object> *contobj = container.head;
	for (; contobj; contobj = contobj->next) {
		if (contobj->data.type == obj->data.type && objdesc[obj->data.type].stackable)
			break;
	}
	if (contobj) {
		if ((int)contobj->data.quantity + (int)obj->data.quantity > 255) {
			int overflow = (int)contobj->data.quantity + (int)obj->data.quantity - 255;
			obj->data.quantity = overflow;
			contobj->data.quantity = 255;
		} else {
			contobj->data.quantity += obj->data.quantity;
			obj = 0;
		}
	}
	if (obj) container.push(obj);
}

