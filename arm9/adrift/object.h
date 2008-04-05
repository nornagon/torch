#ifndef OBJECT_H
#define OBJECT_H 1

#include <nds/jtypes.h>

enum OBJ_TYPE {
	J_NONE,
};

struct ObjDesc {
	u16 ch, col;
};

extern ObjDesc objdesc[];

struct Object {
	Object(): type(0) {}
	u16 type;
};

#endif /* OBJECT_H */
