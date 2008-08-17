#include "player.h"
#include "torch.h"
#include "adrift.h"
#include "recalc.h"
#include <stdio.h>

// drops the object if it's in the player's bag
void Player::drop(Node<Object> obj) {
	if (!game.player.bag.remove(obj)) return;
	game.map.at(game.player.x, game.player.y)->objects.push(obj);
	if (obj->quantity == 1)
		iprintf("You drop a %s\n", obj->desc().name);
	else
		iprintf("You drop %d %ss\n", obj->quantity, obj->desc().name);
}

void Player::exist() {
	obj = Node<Creature>(new NodeV<Creature>);
	obj->type = C_PLAYER;
	obj->setPos(x,y);
	obj->hp = 20;
	game.map.at(x,y)->creature = obj;
	light = new_light(7<<12, (int32)(1.00*(1<<12)), (int32)(0.90*(1<<12)), (int32)(0.85*(1<<12)));
	projectile = NULL;
}

void Player::move(DIRECTION dir) {
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
		if (dpX && dpY) game.cooldown = 3;
		else game.cooldown = 2;

		cell = game.map.at(x + dpX, y + dpY);

		// move the player object
		game.map.at(x, y)->creature = NULL;
		game.map.at(x + dpX, y + dpY)->creature = obj;
		recalc(&game.map.block, x, y);
		recalc(&game.map.block, x + dpX, y + dpY);

		x += dpX;
		y += dpY;

		game.map.block.pX = x;
		game.map.block.pY = y;

		obj->setPos(x,y);

		torch.onscreen(x,y,8);
	}
}

void Player::use(Node<Object> item) {
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

void Player::setprojectile(Node<Object> proj) {
	projectile = proj;
}

void Player::chuck(s16 destx, s16 desty) {
	// don't throw if there's nothing to throw, or if the player tries to throw
	// at herself.
	if (!projectile || (destx == x && desty == y)) return;

	// split the stack
	Node<Projectile> thrown(new NodeV<Projectile>);
	thrown->obj = Node<Object>(new NodeV<Object>);
	thrown->obj->quantity = 1;
	thrown->obj->type = projectile->type;
	thrown->st.reset(x,y,destx,desty);
	game.map.at(x,y)->objects.push(thrown->obj);

	projectile->quantity--;
	if (projectile->quantity <= 0) { // ran out of stuff
		bag.remove(projectile);
		projectile = NULL;
	}

	game.projectiles.push(thrown);
	game.cooldown += 6;
}
