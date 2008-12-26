#include "combat.h"
#include "adrift.h"
#include <stdio.h>
#include "mersenne.h"

#include "entities/creature.h"

bool you_hit_monster(Node<Creature> target) {
	bool died = false;
	s32 attack = game.player.obj->melee;
	s32 their_ac = target->melee + target->agility/4;
	s32 roll = rand32() % (attack + their_ac);
	if (roll >= their_ac) {
		s16 damage = 1 + (rand4() & 0x3);
		const char *name = creaturedesc[target->type].name;
		iprintf("Smack! You hit the %s for %d points of damage.\n", name, damage);
		target->hp -= damage;
		if (target->hp <= 0) {
			iprintf("The %s dies.\n", name);
			game.map.monsters.remove(target); // TODO linked list remove... ugh
			delete target;
			died = true;
			game.map.at(target->x,target->y)->creature = NULL;
		}
	} else {
		iprintf("You miss.\n");
	}
	game.player.melee_xp++;
	if (game.player.melee_xp > game.player.obj->melee) {
		game.player.obj->melee++;
		game.player.melee_xp -= game.player.obj->melee;
		iprintf("Your \1\x03\x6amelee\2 skill is now \1\x03\x6a%d\2.\n", game.player.obj->melee);
	}
	return died;
}

void monster_hit_you(Node<Creature> monster) {
	iprintf("The %s hits you.\n", creaturedesc[monster->type].name);
}
