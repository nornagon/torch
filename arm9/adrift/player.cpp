#include "player.h"
#include "torch.h"
#include "adrift.h"
#include "recalc.h"
#include <stdio.h>

#include "entities/creature.h"

Player::Player() {
	clear();
}

void Player::exist() {
	clear();
	assert(!light);
	light = new lightsource(7<<12, (int32)(1.00*(1<<12)), (int32)(0.90*(1<<12)), (int32)(0.85*(1<<12)));
	light->flicker = FLICKER_RADIUS;
}

void Player::clear() {
	init(PLAYER);
	if (light) delete light;
	light = NULL;
	target = NULL;
	projectile = NULL;
	for (int i = 0; i < E_NUMSLOTS; i++) equipment[i] = NULL;
	bag.delete_all();
	strength_xp = agility_xp = resilience_xp = melee_xp = aim_xp = 0;
}

void Player::moveDir(DIRECTION dir, bool run) {
	int dpX = D_DX[dir],
	    dpY = D_DY[dir];

	if (x + dpX < 0 || x + dpX >= torch.buf.getw()) { dpX = 0; }
	if (y + dpY < 0 || y + dpY >= torch.buf.geth()) { dpY = 0; }

	Cell *cell;

	if (!game.map.walkable(x + dpX, y + dpY)) {
		// your path is blocked
		if (dpX && dpY) {
			// if we could just go left or right, do that. This results in 'sliding'
			// along walls when moving diagonally
			if (game.map.walkable(x + dpX, y))
				dpY = 0;
			else if (game.map.walkable(x, y + dpY))
				dpX = 0;
			else
				dpX = dpY = 0;
		} else
			dpX = dpY = 0;
	}

	if (dpX || dpY) {
		// moving diagonally takes longer. 5*sqrt(2) ~= 7
		if (run) {
			if (dpX && dpY) game.cooldown = 3;
			else game.cooldown = 2;
		} else {
			// TODO: walking should result in faster regeneration than running...
			if (dpX && dpY) game.cooldown = 7;
			else game.cooldown = 5;
		}

		cell = game.map.at(x + dpX, y + dpY);

		// move the player object
		assert(game.map.walkable(x + dpX, y + dpY));
		move(x + dpX, y + dpY);
		recalc(&game.map.block, x, y);
		recalc(&game.map.block, x - dpX, y - dpY);

		game.map.block.pX = x;
		game.map.block.pY = y;

		torch.onscreen(x,y,8);
	}
}

void Player::use(Object *item) {
	switch (item->type) {
		case ROCK:
			if (item->quantity > 1) {
				iprintf("You bang the rocks together, but nothing seems to happen.\n");
			} else {
				iprintf("You wave the rock around in the air.\n");
			}
			break;
		default:
			break;
	}
}

// drops the object from the player's bag
void Player::drop(Object *o) {
	removeFromBag(o);
	game.map.at(x, y)->objects.push(o);
	if (o->quantity == 1)
		iprintf("You drop a %s\n", o->desc()->name);
	else {
		if (o->desc()->plural) {
			iprintf("You drop %d %s\n", o->quantity, o->desc()->plural);
		} else {
			iprintf("You drop %d %ss\n", o->quantity, o->desc()->name);
		}
	}
}

void Player::eat(Object *item) {
	iprintf("You eat the %s.\n", item->desc()->name);
	item->quantity--;
	if (item->quantity <= 0)
		removeFromBag(item);
}

void Player::drink(Object *item) {
	iprintf("You drink the %s.\n", item->desc()->name);
	item->quantity--;
	if (item->quantity <= 0)
		removeFromBag(item);
}

void Player::equip(Object *item) {
	assert(bag.contains(item));
	u8 slot = item->desc()->equip;
	assert(slot < E_NUMSLOTS);
	equipment[slot] = item;
}

void Player::chuck(s16 destx, s16 desty) {
	// don't throw if there's nothing to throw, or if the player tries to throw
	// at herself.
	if (!projectile || (destx == x && desty == y)) return;

	// split the stack
	Projectile *thrown = new Projectile;
	thrown->obj = new Object;
	thrown->obj->quantity = 1;
	thrown->obj->type = projectile->type;
	thrown->st.reset(x,y,destx,desty);
	game.map.at(x,y)->objects.push(thrown->obj);

	projectile->quantity--;
	if (projectile->quantity <= 0) { // ran out of stuff
		removeFromBag(projectile);
		delete projectile;
		projectile = NULL;
	}

	game.map.projectiles.push(thrown);
	cooldown += 6;
}

void Player::removeFromBag(Object *obj) {
	assert(bag.contains(obj));
	u8 slot = obj->desc()->equip;
	if (slot < E_NUMSLOTS && equipment[slot] == obj)
		equipment[slot] = NULL;
	bag.remove(obj);
}

bool Player::isEquipped(Object *obj) {
	u8 slot = obj->desc()->equip;
	return slot < E_NUMSLOTS && equipment[slot] == obj;
}

#define DEF_EXERCISE(stat) \
	void Player::exercise_##stat(int n) { \
		game.player.stat##_xp++; \
		if (game.player.stat##_xp > game.player.stat*4) { \
			game.player.stat++; \
			game.player.stat##_xp -= game.player.stat; \
			iprintf("Your \1\x03\x6a%s\2 skill is now \1\x03\x6a%d\2.\n", #stat, game.player.stat); \
		} \
	}

DEF_EXERCISE(strength)
DEF_EXERCISE(agility)
DEF_EXERCISE(resilience)
DEF_EXERCISE(aim)
DEF_EXERCISE(melee)

#undef DEF_EXERCISE

DataStream& operator <<(DataStream& s, Player& p)
{
	s << (Creature&)p;
	s << *p.light;
	s << p.strength_xp << p.agility_xp << p.resilience_xp << p.aim_xp << p.melee_xp;
	s << p.bag;
	return s;
}
DataStream& operator >>(DataStream& s, Player& p)
{
	p.clear();
	s >> (Creature&)p;
	p.light = new lightsource;
	s >> *p.light;
	s >> p.strength_xp >> p.agility_xp >> p.resilience_xp >> p.aim_xp >> p.melee_xp;
	s >> p.bag;
	return s;
}
