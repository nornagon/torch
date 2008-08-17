#include "list.h"
#include "creature.h"
#include "adrift.h"
#include "util.h"
#include "combat.h"

#include "entities/creature.h"

void step_creature(Node<Creature> creature) {
	if (creature->type == VENUS_FLY_TRAP) {
		if (creature->cooldown <= 0 && adjacent(game.player.x, game.player.y,
					creature->x, creature->y)) {
			monster_hit_you(creature);
			creature->cooldown += 13;
		}
	}
	if (creature->cooldown > 0) creature->cooldown--;
}
