#include "list.h"
#include "creature.h"
#include "adrift.h"
#include "util.h"
#include "combat.h"

void step_creature(Node<Creature> creature) {
	if (creature->type == C_FLYTRAP) {
		if (creature->cooldown <= 0 && adjacent(game.player.x, game.player.y,
					creature->x, creature->y)) {
			monster_hit_you(creature);
			creature->cooldown += 13;
		}
	}
	if (creature->cooldown > 0) creature->cooldown--;
}
