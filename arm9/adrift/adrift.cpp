#include "adrift.h"
#include "mapgen.h"
#include "light.h"
#include "sight.h"
#include "text.h"
#include "interface.h"

#include <stdarg.h>

#include "torch.h"
#include <nds/jtypes.h>

#include "mersenne.h"

#include <stdio.h>
#include <string.h>

#include "recalc.h"

Adrift game;

Adrift::Adrift() {
	// the fov_settings_type that will be used for non-player-held lights.
	fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);
}
void Map::reset() {
	for (s16 y = 0; y < h; y++)
		for (s16 x = 0; x < w; x++)
			at(x,y)->reset();
}

bool isvowel(char c) {
	return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

bool get_items() {
	List<Object> &os = game.map.at(game.player.x, game.player.y)->objects;
	if (os.empty()) return false;

	Node<Object> k;
	while ((k = os.pop())) {
		const char *name = objdesc[k->type].name;
		if (k->quantity == 1) {
			const char *a_an = isvowel(name[0]) ? "n" : "";
			iprintf("You pick up a%s %s.\n", a_an, name);
		} else {
			iprintf("You pick up %d %ss.\n", k->quantity, name);
		}

		// go through the bag to see if we can stack.
		stack_item_push(game.player.bag, k);
	}

	game.cooldown += 4;
	return true;
}

void seek_and_destroy() {
	if (!game.player.target) {
		u16 mindist = 16;
		Node<Creature> candidate;
		for (int y = game.player.y-4; y < game.player.y+4; y++)
			for (int x = game.player.x-4; x < game.player.x+4; x++)
				if (game.map.occupied(x,y) &&
				    !(x == game.player.x && y == game.player.y) &&
				    game.map.block.at(x,y)->visible &&
				    dist2(x,y,game.player.x,game.player.y) < mindist)
						candidate = game.map.at(x,y)->creature;
		if (candidate) game.player.target = candidate;
	}
	if (game.player.target) {
		s16 targx = game.player.target->x, targy = game.player.target->y;
		if (dist2(game.player.x,game.player.y,targx,targy) > 16) {
			game.player.target = NULL; return;
		}
		if (game.map.block.at(targx,targy)->visible) {
			//iprintf("I can see it!\n");
		} else {
			game.player.target = NULL; return;
		}
		if (adjacent(targx,targy,game.player.x,game.player.y)) {
			s16 damage = 1 + (rand32() % 8);
			const char *name = creaturedesc[game.player.target->type].name;
			iprintf("Smack! You hit the %s for %d points of damage.\n", name, damage);
			game.player.target->hp -= damage;
			if (game.player.target->hp <= 0) {
				iprintf("The %s dies.\n", name);
				delete game.player.target;
				game.player.target = NULL;
				game.map.at(targx,targy)->creature = NULL;
			}
			game.cooldown += 5;
			return;
		} else {
			bresenstate st(game.player.x, game.player.y, targx, targy);
			st.step();
			game.player.move(direction(st.posx(), st.posy(), game.player.x, game.player.y));
		}
	} else {
		iprintf("no target\n");
	}
}

void process_keys() {
	if (game.cooldown <= 0) {
		scanKeys();
		u32 keys = keysHeld();
		u32 down = keysDown();
		touchPosition touch = touchReadXY();

		if (down & KEY_X) {
			inventory(); return;
		}

		if (down & KEY_TOUCH && game.player.projectile && touch.px != 0 && touch.py != 0) {
			game.player.chuck(torch.buf.scroll.x + touch.px/8,
			                  torch.buf.scroll.y + touch.py/8);
			return;
		}

		if (keys & KEY_Y) {
			if (get_items()) return;
		}

		DIRECTION dir = 0;
		if (keys & KEY_RIGHT)
			dir |= D_EAST;
		else if (keys & KEY_LEFT)
			dir |= D_WEST;
		if (keys & KEY_DOWN)
			dir |= D_SOUTH;
		else if (keys & KEY_UP)
			dir |= D_NORTH;

		if (dir) {
			game.player.move(dir);
			return;
		}

		if (keys & KEY_A) {
			seek_and_destroy(); return;
		}

	} else if (game.cooldown > 0)
		game.cooldown--;
}

void draw_projectiles() {
	Node<Projectile> p = game.projectiles.top();
	while (p) {
		s16 x = p->st.posx();
		s16 y = p->st.posy();

		game.map.at(x,y)->objects.remove(p->obj);

		s16 ox = x, oy = y;
		s16 destx = p->st.destx();
		s16 desty = p->st.desty();
		bool collided = false;
		while ((x-ox)*(x-ox)+(y-oy)*(y-oy) < 2*2) {
			if (x == destx && y == desty) break;
			p->st.step();
			// TODO use a real flag
			if (celldesc[game.map.at(p->st.posx(),p->st.posy())->type].opaque) {
				collided = true;
				break;
			}
			x = p->st.posx();
			y = p->st.posy();
		}

		if ((x == destx && y == desty) || collided) {
			Node<Projectile> next = p.next();
			game.projectiles.remove(p);
			stack_item_push(game.map.at(x,y)->objects, p->obj);
			delete p;
			p = next;
		} else {
			game.map.at(x,y)->objects.push(p->obj);
			p = p.next();
		}
	}
}

void process_sight() {
	// go 1 over the boundaries to be sure we mark everything properly, even if
	// we scrolled just now...
	for (int y = torch.buf.scroll.y-1; y < torch.buf.scroll.y + 24 + 1; y++)
		for (int x = torch.buf.scroll.x-1; x < torch.buf.scroll.x + 32 + 1; x++)
			game.map.block.at(x, y)->visible = false;
	game.player.light->x = game.player.x << 12;
	game.player.light->y = game.player.y << 12;
	cast_sight(game.fov_sight, &game.map.block, game.player.light);
}

void handler() {
	process_keys();
	draw_projectiles();

	process_sight();
	draw_lights(game.fov_light, &game.map.block, game.map.lights);

	refresh(&game.map.block);
}

void new_game() {
	game.map.resize(128,128);
	game.map.block.resize(128,128);
	torch.buf.resize(128,128);

	// TODO: replace with gettimeofday() or similar
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));

	generate_terrarium();

	game.player.exist();
	recalc(&game.map.block, game.player.x, game.player.y);

	torch.buf.scroll.x = game.player.x - 16;
	torch.buf.scroll.y = game.player.y - 12;
	torch.buf.bounded(torch.buf.scroll.x, torch.buf.scroll.y);

	torch.run(handler);
}

void init_world() {
	lcdMainOnBottom();
	new_game();
}
