#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>

enum CREATURE_TYPE {
	C_NONE = 0,
	C_PLAYER,
	MAX_CREATURE_TYPE
};

struct CreatureDesc {
	u16 ch, col;
	const char *name;
};

extern CreatureDesc creaturedesc[];

struct Creature {
	u16 type;
};

#endif /* CREATURE_H */
