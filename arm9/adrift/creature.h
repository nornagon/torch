#ifndef CREATURE_H
#define CREATURE_H 1

#include <nds/jtypes.h>
#include "entities/creature.h"
#include "list.h"
#include "lightsource.h"

struct Creature : public listable<Creature> {
	POOLED(Creature)

	private:
	void doBehaviour(); // wrapped function to avoid goto :)

	public:
	Creature();
	Creature(u16 type);
	u16 type;
	s16 x,y;
	s16 cooldown, regen_cooldown;
	s32 hp;
	s32 strength, agility, resilience, aim, melee;
	lightsource *light;
	s16 max_hp() { return resilience*2 + strength + agility; }

	void init(u16 _type);
	CreatureDesc *desc() { return &creaturedesc[type]; }

	bool canMove(s16 xp, s16 yp);
	// moves the creature, updating map cells
	void move(s16 xp, s16 yp);
	void setPos(s16 x0, s16 y0) { x = x0; y = y0; }

	void acted() {
		cooldown += desc()->cooldown;
	}
	void regenerate();

	// AI
	void behave();

	~Creature();
};

#endif /* CREATURE_H */
