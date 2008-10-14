#include "list.h"
#include "creature.h"
#include "adrift.h"
#include "util.h"
#include "combat.h"
#include "gfxPrimitives.h" // for randwalk
#include <stdio.h>

#include "entities/creature.h"

void step_creature(Node<Creature> creature) {
	if (creature->cooldown <= 0) {
		if (creature->desc()->wanders) {
				s16 x = creature->x, y = creature->y;
				randwalk(x,y);
				if (!game.map.solid(x,y) && !game.map.occupied(x,y)) {
					game.map.at(creature->x, creature->y)->creature = NULL;
					game.map.at(x,y)->creature = creature;
					creature->setPos(x,y);
					creature->cooldown += creature->desc()->cooldown;
				}
		}
		if (creature->type == VENUS_FLY_TRAP) {
			if (adjacent(game.player.x, game.player.y, creature->x, creature->y)) {
				monster_hit_you(creature);
				creature->cooldown += creature->desc()->cooldown;
			} else {
				s16 x = creature->x, y = creature->y;
				for (int j = y-1; j <= y+1; j++)
					for (int i = x-1; i <= x+1; i++) {
						if (i == x && j == y) continue;
						Cell *c = game.map.at(i,j);
						if (c->creature->type == BLOWFLY) {
							if (game.map.block.at(i,j)->visible || game.map.block.at(x,y)->visible)
								iprintf("The venus fly trap gobbles up the blowfly.\n");

							// TODO: function for removing creatures
							game.monsters.remove(c->creature);
							delete c->creature;
							c->creature = NULL;
						}
					}
			}
		}
	}
	if (creature->cooldown > 0) creature->cooldown--;
}
