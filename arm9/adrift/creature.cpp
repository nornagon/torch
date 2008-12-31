#include "creature.h"
#include "adrift.h"
#include "mersenne.h"
#include "list.h"
#include "util.h"
#include "combat.h"
#include "gfxPrimitives.h" // for randwalk
#include <stdio.h>

#include "entities/creature.h"

Creature::Creature() {
	init(CREATURE_NONE);
}
Creature::Creature(u16 _type) {
	init(_type);
}

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

void Creature::behave() {
	if (cooldown > 0) {
		cooldown--;
		return;
	}

	if (!desc()->peaceful) {
		if (adjacent(game.player.x, game.player.y, x, y)) {
			monster_hit_you(this);
			acted();
			return;
		}
		if (game.map.block.at(x,y)->visible &&
		    !desc()->stationary &&
		    within(game.player.x, game.player.y, x, y, 6)) {
			bresenstate st(x, y, game.player.x, game.player.y);
			st.step();
			if (game.map.walkable(st.posx(), st.posy())) {
				move(st.posx(), st.posy());
				cooldown += desc()->cooldown / 2;
				return;
			}
		}
	}
	if (hp < max_hp() && !game.map.block.at(x,y)->visible) {
		regenerate();
		return;
	}
	if (desc()->shy && game.map.block.at(x,y)->visible) {
		if (within(game.player.x, game.player.y, x, y, 6)) {
			bresenstate st(0,0, x - game.player.x, y - game.player.y);
			st.step();
			if (game.map.walkable(x + st.posx(), y + st.posy())) {
				move(x + st.posx(), y + st.posy());
				cooldown += desc()->cooldown / 2;
				return;
			}
		}
	}
	if (desc()->wanders) {
		s16 x_ = x, y_ = y;
		randwalk(x_,y_);
		if (game.map.walkable(x_,y_)) {
			move(x_,y_);
			acted();
			return;
		}
	}
	if (type == VENUS_FLY_TRAP) {
		// eat blowflies
		for (int j = y-1; j <= y+1; j++)
			for (int i = x-1; i <= x+1; i++) {
				if (i == x && j == y) continue;
				Cell *c = game.map.at(i,j);
				if (c->creature && c->creature->type == BLOWFLY) {
					if (game.map.block.at(i,j)->visible || game.map.block.at(x,y)->visible)
						iprintf("The venus fly trap gobbles up the blowfly.\n");

					// TODO: function for removing creatures
					game.map.monsters.remove(c->creature);
					delete c->creature;
					c->creature = NULL;
					acted();
					return;
				}
			}
	}
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
	assert(!game.map.occupied(_x,_y));
	assert(game.map.at(x,y)->creature == this);
	Cell *c = game.map.at(x,y);
	c->creature = NULL;
	game.map.at(_x,_y)->creature = this;
	setPos(_x,_y);
}
