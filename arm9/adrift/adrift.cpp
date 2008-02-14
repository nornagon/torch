#include "world.h"
#include "generic.h"
#include "adrift.h"

#include "mapgen.h"

#include "torch.h"

#include <nds.h>

#include "sight.h"
#include "light.h"
#include "mersenne.h"

#include "assert.h"

#include "draw.h"
#include "nocash.h"

Adrift game;

Adrift::Adrift() {
	// the fov_settings_type that will be used for non-player-held lights.
	fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);
}

void recalc(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	u16 ch, col;
	if (l->creatures.head) {
		ch = creaturedesc[l->creatures.head->data.type].ch;
		col = creaturedesc[l->creatures.head->data.type].col;
	} else if (l->objs.head) {
		//ch = '@'; col = RGB15(31,31,31);
	} else {
		ch = celldesc[l->type].ch;
		col = celldesc[l->type].col;
	}
	mapel *m = torch.buf.at(x,y);
	m->ch = ch;
	m->col = col;
}

bool solid(s16 x, s16 y) {
	Cell *cell = game.map.at(x, y);
	if (celldesc[cell->type].solid) return true;
	return false;
}

void move_player(DIRECTION dir) {
	s32 pX = game.player.x, pY = game.player.y;

	int dpX = D_DX[dir],
	    dpY = D_DY[dir];

	if (pX + dpX < 0 || pX + dpX >= torch.buf.getw()) { dpX = 0; }
	if (pY + dpY < 0 || pY + dpY >= torch.buf.geth()) { dpY = 0; }

	Cell *cell;

	if (solid(pX + dpX, pY + dpY)) {
		// your path is blocked
		if (dpX && dpY) {
			// if we could just go left or right, do that. This results in 'sliding'
			// along walls when moving diagonally
			if (!solid(pX + dpX, pY))
				dpY = 0;
			else if (!solid(pX, pY + dpY))
				dpX = 0;
			else
				dpX = dpY = 0;
		} else
			dpX = dpY = 0;
	}

	if (dpX || dpY) {
		// moving diagonally takes longer. 5*sqrt(2) ~= 7
		if (dpX && dpY) game.frm = 7;
		else game.frm = 5;

		cell = game.map.at(pX + dpX, pY + dpY);

		// move the player object
		game.map.at(pX, pY)->creatures.remove(game.player.obj);
		game.map.at(pX + dpX, pY + dpY)->creatures.push(game.player.obj);
		recalc(pX, pY);
		recalc(pX + dpX, pY + dpY);

		pX += dpX; game.player.x = pX;
		pY += dpY; game.player.y = pY;

		// TODO: move to engine
		s32 dsX = 0, dsY = 0;
		// keep the screen vaguely centred on the player (gap of 8 cells)
		if (pX - torch.buf.scroll.x < 8 && torch.buf.scroll.x > 0) { // it's just a scroll to the left
			dsX = (pX - 8) - torch.buf.scroll.x;
			torch.buf.scroll.x = pX - 8;
		} else if (pX - torch.buf.scroll.x > 24 && torch.buf.scroll.x < torch.buf.getw()-32) {
			dsX = (pX - 24) - torch.buf.scroll.x;
			torch.buf.scroll.x = pX - 24;
		}

		if (pY - torch.buf.scroll.y < 8 && torch.buf.scroll.y > 0) {
			dsY = (pY - 8) - torch.buf.scroll.y;
			torch.buf.scroll.y = pY - 8;
		} else if (pY - torch.buf.scroll.y > 16 && torch.buf.scroll.y < torch.buf.geth()-24) {
			dsY = (pY - 16) - torch.buf.scroll.y;
			torch.buf.scroll.y = pY - 16;
		}

		if (dsX || dsY)
			torch.scroll(dsX, dsY);
	}
}

void new_player() {
	game.player.obj = Node<Creature>::pool.request_node();
	game.map.at(game.player.x, game.player.y)->creatures.push(game.player.obj);
	game.player.obj->data.type = C_PLAYER;
	recalc(game.player.x, game.player.y);
	game.player.light = new_light(7<<12, (int32)(1.00*(1<<12)), (int32)(0.90*(1<<12)), (int32)(0.85*(1<<12)));
}

void updaterecall(s16 x, s16 y) {
	Cell *c = game.map.at(x, y);
	if (!celldesc[c->type].forgettable) {
		int32 v = torch.buf.luxat(x, y)->lval;
		mapel *m = torch.buf.at(x, y);
		m->recall = min(1<<12, max(v, m->recall));
	} else {
		torch.buf.at(x, y)->recall = 0;
	}
}

void process_sight() {
	game.player.light->x = game.player.x << 12;
	game.player.light->y = game.player.y << 12;
	fov_circle(game.fov_sight, &game.map.block, game.player.light, game.player.x, game.player.y, 32);
	luxel *e = torch.buf.luxat(game.player.x, game.player.y);
	game.map.block.at(game.player.x, game.player.y)->visible = true;
	e->lval = (1<<12);
	e->lr = game.player.light->r;
	e->lg = game.player.light->g;
	e->lb = game.player.light->b;
	updaterecall(game.player.x, game.player.y);
}

void process_keys() {
	if (game.frm == 0) {
		scanKeys();
		u32 keys = keysHeld();

		DIRECTION dir = 0;
		if (keys & KEY_RIGHT)
			dir |= D_EAST;
		if (keys & KEY_LEFT)
			dir |= D_WEST;
		if (keys & KEY_DOWN)
			dir |= D_SOUTH;
		if (keys & KEY_UP)
			dir |= D_NORTH;

		if (!dir) return;

		move_player(dir);
	}
	if (game.frm > 0)
		game.frm--;
}

void handler() {
	process_keys();
	process_sight();

	for (int y = 0; y < 24; y++)
		for (int x = 0; x < 32; x++)
			updaterecall(x+torch.buf.scroll.x, y+torch.buf.scroll.y);
}

void new_game() {
	game.map.resize(128,128);
	game.map.block.resize(128,128);
	torch.buf.resize(128,128);

	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));

	generate_terrarium();

	new_player();

	torch.buf.scroll.x = game.player.x - 16;
	torch.buf.scroll.y = game.player.y - 12;
	torch.buf.bounded(torch.buf.scroll.x, torch.buf.scroll.y);

	torch.run(handler);
}

void init_world() {
	lcdMainOnBottom();
	new_game();
}
