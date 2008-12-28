#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>
#include "entities/creature.h"

struct Creature {
	u16 type;
	s16 x,y;
	s16 cooldown, regen_cooldown;
	s32 hp;
	s32 strength, agility, resilience, aim, melee;
	void setPos(s16 x0, s16 y0) { x = x0; y = y0; }
	s16 max_hp() { return resilience*2 + strength + agility; }
	void init(u16 _type);
	void acted() {
		cooldown += desc()->cooldown;
	}
	void regenerate();
	void move(s16 xp, s16 yp);
	CreatureDesc *desc() { return &creaturedesc[type]; }
};

#endif /* CREATURE_H */
