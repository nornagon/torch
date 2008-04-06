#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>

enum OBJ_TYPE {
	J_NONE,
	J_ROCK,
};

struct ObjDesc {
	u16 ch, col;
	char *name;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0) {}
	u16 type;
};

#endif /* OBJECT_H */
