#include "combat.h"
#include "adrift.h"
#include <stdio.h>
#include "mersenne.h"

#include "entities/creature.h"

bool hits(Node<Creature> attacker, Node<Creature> target) {
	s32 attack = attacker->melee;
	s32 ac = target->melee + target->agility/4;
	s32 roll = rand32() % (attack + ac);
	return roll >= ac;
}

s16 damage_done(Node<Creature> attacker, Node<Creature> target) {
	s16 min = attacker->desc()->natural_min;
	s16 max = attacker->desc()->natural_max;
	return min + (rand32() % (max-min+1)) + attacker->strength/2;
}

bool you_hit_monster(Node<Creature> target) {
	bool died = false;
	if (hits(game.player.obj, target)) {
		s16 damage = damage_done(game.player.obj, target);
		const char *name = target->desc()->name;
		iprintf("Smack! You hit the %s for %d points of damage.\n", name, damage);
		target->hp -= damage;
		if (target->hp <= 0) {
			iprintf("The %s dies.\n", name);
			game.map.monsters.remove(target); // TODO linked list remove... ugh
			target.free();
			died = true;
			game.map.at(target->x,target->y)->creature = NULL;
		}
		game.player.exercise_strength();
	} else {
		iprintf("You miss.\n");
	}
	game.player.exercise_melee();
	return died;
}

void monster_hit_you(Node<Creature> monster) {
	if (hits(monster, game.player.obj)) {
		s16 damage = damage_done(monster, game.player.obj);
		iprintf("The %s hits you for %d points of damage.\n", monster->desc()->name, damage);
		game.player.obj->hp -= damage;
		game.player.exercise_resilience();
	} else {
		iprintf("The %s misses.\n", monster->desc()->name);
		game.player.exercise_agility();
	}
}
