#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>

struct Creature {
	u16 type;
	s16 x,y;
	s16 hp;
	s16 cooldown;
	void setPos(s16 x0, s16 y0) { x = x0; y = y0; }
};

#endif /* CREATURE_H */
