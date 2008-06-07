#include "object.h"
#include <nds/arm9/video.h>

#ifdef ONLY_ONAMES
#define OBJ(def,ch,col,stk,name) J_##def
#else
#define OBJ(def,ch,col,stk,name) { ch, col, stk, name }
#endif

#ifdef ONLY_ONAMES
enum {
#else
ObjDesc objdesc[] = {          /* stk  name */
#endif
	OBJ( NONE, 'X', RGB15(31,31,0),  1, "unknown" ),
	OBJ( ROCK, '8', RGB15(18,18,18), 1, "rock"    ),
};

#undef OBJ

#ifndef ONLY_ONAMES
void stack_item_push(List<Object> &container, Node<Object> obj) {
	Node<Object> contobj = container.top();
	for (; contobj; contobj = contobj.next()) {
		if (contobj->type == obj->type && objdesc[obj->type].stackable)
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
#endif
