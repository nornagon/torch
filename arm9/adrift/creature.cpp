#include "creature.h"
#include "adrift.h"
#include "mersenne.h"

void Creature::init(u16 _type) {
	type = _type;
	cooldown = 0;
	regen_cooldown = 0;
	strength = desc()->strength;
	agility = desc()->agility;
	aim = desc()->aim;
	melee = desc()->melee;
	resilience = desc()->resilience;
	hp = max_hp();
}

void Creature::regenerate() {
	if (regen_cooldown <= 0) {
		if (hp < max_hp()) {
			int hp_gain = 1 + (rand32() % (1 + resilience / 8));
			if (hp_gain + hp >= max_hp())
				hp = max_hp();
			else
				hp += hp_gain;
		}
		regen_cooldown += 20+rand4()*2;
	} else regen_cooldown--;
}

void Creature::move(s16 _x, s16 _y) {
	assert(!game.map.at(_x,_y)->creature);
	assert(game.map.at(x,y)->creature == this);
	Cell *c = game.map.at(x,y);
	c->creature = NULL;
	game.map.at(_x,_y)->creature = this;
	setPos(_x,_y);
}
