#include "list.h"
#include "creature.h"
#include "adrift.h"
#include "util.h"
#include "combat.h"
#include "gfxPrimitives.h" // for randwalk

#include "entities/creature.h"

void step_creature(Node<Creature> creature) {
	if (creature->desc()->wanders) {
		if (creature->cooldown <= 0) {
			s16 x = creature->x, y = creature->y;
			randwalk(x,y);
			if (!game.map.solid(x,y) && !game.map.occupied(x,y)) {
				game.map.at(creature->x, creature->y)->creature = NULL;
				game.map.at(x,y)->creature = creature;
				creature->setPos(x,y);
				creature->cooldown += 5;
			}
		}
	}
	if (creature->type == VENUS_FLY_TRAP) {
		if (creature->cooldown <= 0 && adjacent(game.player.x, game.player.y,
					creature->x, creature->y)) {
			monster_hit_you(creature);
			creature->cooldown += 13;
		}
	}
	if (creature->cooldown > 0) creature->cooldown--;
}
