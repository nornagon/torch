#include "player.h"
#include "torch.h"
#include "adrift.h"
#include <stdio.h>

// drops the object if it's in the player's bag
void Player::drop(Node<Object>* obj) {
	if (!game.player.bag.remove(obj)) return;
	game.map.at(game.player.x, game.player.y)->objects.push(obj);
	if (obj->data.quantity == 1)
		iprintf("You drop a %s\n", obj->data.desc().name);
	else
		iprintf("You drop %d %ss\n", obj->data.quantity, obj->data.desc().name);
}

void Player::exist() {
	obj = Node<Creature>::pool.request_node();
	game.map.at(x, y)->creatures.push(obj);
	obj->data.type = C_PLAYER;
	obj->data.setPos(x,y);
	light = new_light(8<<12, (int32)(1.00*(1<<12)), (int32)(0.90*(1<<12)), (int32)(0.85*(1<<12)));
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
		game.map.at(x, y)->creatures.remove(obj);
		game.map.at(x + dpX, y + dpY)->creatures.push(obj);
		// TODO: i assume these two squares are visible... correct?
		recalc_visible(x, y);
		recalc_visible(x + dpX, y + dpY);

		x += dpX;
		y += dpY;

		game.map.block.pX = x;
		game.map.block.pY = y;

		obj->data.setPos(x,y);

		torch.onscreen(x,y,8);
	}
}

void Player::use(Node<Object> *item) {
	switch (item->data.type) {
		case J_ROCK:
			if (item->data.quantity > 1) {
				iprintf("You bang the rocks together, but nothing seems to happen.\n");
			} else {
				iprintf("You wave the rock around in the air.\n");
			}
			break;
		default:
			break;
	}
}

void Player::setprojectile(Node<Object> *proj) {
	projectile = proj;
}

void Player::chuck(s16 destx, s16 desty) {
	// don't throw if there's nothing to throw, or if the player tries to throw
	// at herself.
	if (!projectile || (destx == x && desty == y)) return;

	// split the stack
	Node<Projectile>* thrown = Node<Projectile>::pool.request_node();
	thrown->data.obj = Node<Object>::pool.request_node();
	thrown->data.object()->quantity = 1;
	thrown->data.object()->type = projectile->data.type;
	thrown->data.st.reset(x,y,destx,desty);
	game.map.at(x,y)->objects.push(thrown->data.obj);

	projectile->data.quantity--;
	if (projectile->data.quantity <= 0) { // ran out of stuff
		bag.remove(projectile);
		projectile = NULL;
	}

	game.projectiles.push(thrown);
	game.cooldown += 6;
}
