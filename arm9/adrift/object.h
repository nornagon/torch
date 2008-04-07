#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>

enum OBJ_TYPE {
	J_NONE,
	J_ROCK,
};

struct ObjDesc {
	u16 ch, col;
	bool stackable;
	char *name;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0), quantity(1) {}
	u16 type;
	u8 quantity;
};

#endif /* OBJECT_H */
