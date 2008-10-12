#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>
#include "entities/creature.h"

struct Creature {
	u16 type;
	s16 x,y;
	s16 cooldown;
	s32 hp;
	s32 strength, agility, aim, melee;
	void setPos(s16 x0, s16 y0) { x = x0; y = y0; }
	s16 max_hp() { return strength + agility; }
	void init(u16 _type) {
		type = _type;
		cooldown = 0;
		strength = desc()->strength;
		agility = desc()->agility;
		aim = desc()->aim;
		melee = desc()->melee;
		hp = max_hp();
	}
	CreatureDesc *desc() { return &creaturedesc[type]; }
};

#endif /* CREATURE_H */
