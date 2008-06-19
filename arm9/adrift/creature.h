#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>

struct Creature {
	u16 type;
	s16 x,y;
	s16 hp;
	void setPos(s16 x0, s16 y0) { x = x0; y = y0; }
};

struct CreatureDesc {
	u16 ch, col;
	s16 maxhp;
	const char *name;
};

extern CreatureDesc creaturedesc[];

// enum { ... }
#define ONLY_CNAMES 1
#include "creature.cpp"
#undef ONLY_CNAMES

#endif /* CREATURE_H */
