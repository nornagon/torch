#include "list.h"
#include "creature.h"
#include "adrift.h"
#include "util.h"
#include "combat.h"
#include "gfxPrimitives.h" // for randwalk
#include <stdio.h>

#include "entities/creature.h"

void behave(Node<Creature> creature) {
	if (!creature->desc()->peaceful) {
		if (adjacent(game.player.x, game.player.y, creature->x, creature->y)) {
			monster_hit_you(creature);
			creature->acted();
			return;
		}
		if (game.map.block.at(creature->x, creature->y)->visible &&
		    !creature->desc()->stationary &&
		    within(game.player.x, game.player.y, creature->x, creature->y, 6)) {
			bresenstate st(creature->x, creature->y, game.player.x, game.player.y);
			st.step();
			if (!game.map.solid(st.posx(), st.posy()) && !game.map.occupied(st.posx(), st.posy())) {
				creature->move(st.posx(), st.posy());
				creature->cooldown += creature->desc()->cooldown / 2;
				return;
			}
		}
	}
	if (creature->desc()->wanders) {
		s16 x = creature->x, y = creature->y;
		randwalk(x,y);
		if (!game.map.solid(x,y) && !game.map.occupied(x,y)) {
			creature->move(x,y);
			creature->acted();
			return;
		}
	}
	if (creature->type == VENUS_FLY_TRAP) {
		// eat blowflies
		s16 x = creature->x, y = creature->y;
		for (int j = y-1; j <= y+1; j++)
			for (int i = x-1; i <= x+1; i++) {
				if (i == x && j == y) continue;
				Cell *c = game.map.at(i,j);
				if (c->creature && c->creature->type == BLOWFLY) {
					if (game.map.block.at(i,j)->visible || game.map.block.at(x,y)->visible)
						iprintf("The venus fly trap gobbles up the blowfly.\n");

					// TODO: function for removing creatures
					game.monsters.remove(c->creature);
					delete c->creature;
					c->creature = NULL;
					creature->acted();
					return;
				}
			}
	}
}

void step_creature(Node<Creature> creature) {
	if (creature->cooldown <= 0) {
		behave(creature);
	}
	if (creature->cooldown > 0) creature->cooldown--;
}
