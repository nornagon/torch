#include "combat.h"
#include "adrift.h"
#include <stdio.h>
#include "mersenne.h"

#include "entities/creature.h"

bool you_hit_monster(Node<Creature> target) {
	bool died = false;
	s16 damage = 1 + (rand32() % 8);
	const char *name = creaturedesc[target->type].name;
	iprintf("Smack! You hit the %s for %d points of damage.\n", name, damage);
	target->hp -= damage;
	if (target->hp <= 0) {
		iprintf("The %s dies.\n", name);
		game.monsters.remove(target); // TODO linked list remove... ugh
		delete target;
		died = true;
		game.map.at(target->x,target->y)->creature = NULL;
	}
	return died;
}

void monster_hit_you(Node<Creature> monster) {
	iprintf("The %s hits you.\n", creaturedesc[monster->type].name);
}
