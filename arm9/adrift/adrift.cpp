#include "adrift.h"
#include "mapgen.h"
#include "light.h"
#include "sight.h"
#include "text.h"
#include "interface.h"
#include "combat.h"
#include "behaviour.h"

#include "assert.h"

#include <stdarg.h>

#include "torch.h"
#include <nds/jtypes.h>

#include "mersenne.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "recalc.h"

#include "entities/terrain.h"
#include "entities/object.h"

#ifdef NATIVE
#include <sys/time.h>
#endif

void new_game();

Adrift game;

Adrift::Adrift() {
	// the fov_settings_type that will be used for non-player-held lights.
	fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);
}

void Map::spawn(u16 type, s16 x, s16 y) {
	Creature* c = new Creature;
	c->init(type);
	c->setPos(x,y);
	assert(!occupied(x,y));
	at(x,y)->creature = c;
	monsters.push(c);
}

void Map::reset() {
	for (s16 y = 0; y < h; y++)
		for (s16 x = 0; x < w; x++)
			at(x,y)->reset();
	lights.delete_all();
	animations.delete_all();
	monsters.delete_all();
	projectiles.delete_all();
}

bool isvowel(char c) {
	c = tolower(c);
	return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

bool get_items() {
	List<Object> &os = game.map.at(game.player.x, game.player.y)->objects;
	if (os.empty()) return false;

	Object *k;
	while ((k = os.pop())) {
		const char *name = k->desc()->name;
		if (k->quantity == 1) {
			const char *a_an = isvowel(name[0]) ? "n" : "";
			iprintf("You pick up a%s %s.\n", a_an, name);
		} else {
			if (k->desc()->plural) {
				iprintf("You pick up %d %s.\n", k->quantity, k->desc()->plural);
			} else {
				iprintf("You pick up %d %ss.\n", k->quantity, name);
			}
		}

		// go through the bag to see if we can stack.
		stack_item_push(game.player.bag, k);
	}

	game.cooldown += 4;
	return true;
}

void seek_and_destroy() {
	if (!game.player.target) {
		// acquire a target
		u16 mindist = 16;
		Creature *candidate = 0;
		for (int y = game.player.y-4; y < game.player.y+4; y++)
			for (int x = game.player.x-4; x < game.player.x+4; x++) {
				u16 dist = dist2(x,y,game.player.x,game.player.y);
				if (dist <= 16 && game.map.occupied(x,y) &&
				    !(x == game.player.x && y == game.player.y) &&
				    game.map.block.at(x,y)->visible) {
				  // try to select aggressive creatures over peaceful ones
				  Creature *considered = game.map.at(x,y)->creature;
				  if (!candidate) candidate = considered;
					else if ((candidate->desc()->peaceful && !considered->desc()->peaceful) || dist < mindist) {
				  	candidate = considered;
				  	mindist = dist;
					}
				}
			}
		if (candidate) game.player.target = candidate;
	}
	if (game.player.target) {
		s16 targx = game.player.target->x, targy = game.player.target->y;
		if (dist2(game.player.x,game.player.y,targx,targy) > 16) {
			game.player.target = NULL; return;
		}
		if (!game.map.block.at(targx,targy)->visible) {
			game.player.target = NULL; return;
		}
		if (adjacent(targx,targy,game.player.x,game.player.y)) {
			if (you_hit_monster(game.player.target))
				game.player.target = NULL;
			game.cooldown += 5;
			return;
		} else {
			bresenstate st(game.player.x, game.player.y, targx, targy);
			st.step();
			game.player.move(direction(st.posx(), st.posy(), game.player.x, game.player.y), true);
		}
	}
}

void process_keys() {
	if (game.cooldown <= 0) {
		scanKeys();
		u32 keys = keysHeld();
		u32 down = keysDown();
		touchPosition touch = touchReadXY();

		if (down & KEY_START) {
			new_game();
			return;
		}

		if (down & KEY_SELECT) {
			test_map();

			torch.buf.scroll.x = game.player.x - 16;
			torch.buf.scroll.y = game.player.y - 12;
			torch.buf.bounded(torch.buf.scroll.x, torch.buf.scroll.y);
			torch.dirty_screen();
			torch.reset_luminance();

			return;
		}

		if (down & KEY_X) {
			inventory(); return;
		}
		if (down & KEY_L) {
			overview(); return;
		}

		if (down & KEY_TOUCH && touch.px != 0 && touch.py != 0) {
			if (!(keys & KEY_R) && game.player.projectile) {
				game.player.chuck(torch.buf.scroll.x + touch.px/8,
													torch.buf.scroll.y + touch.py/8);
			}
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
			game.player.move(dir, keys & KEY_B);
			return;
		}

		if (down & KEY_A) {
			seek_and_destroy();
			if (!game.player.target) {
				// no monsters in range
				for (int x = game.player.x-1; x <= game.player.x+1; x++) {
					for (int y = game.player.y-1; y <= game.player.y+1; y++) {
						Object *o = game.map.at(x,y)->objects.head();
						if (!o) continue;
						if (o->type == VENDING_MACHINE) {
							if (game.player.x == x+D_DX[o->orientation] && game.player.y == y+D_DY[o->orientation]) {
								iprintf("You kick the vending machine. ");
								if (o->quantity > 0 && rand4() < 5) {
									if (rand4() & 1) {
										iprintf("Clunk! A can rolls out.\n");
										addObject(game.player.x, game.player.y, CAN_OF_STEWED_BEEF);
									} else {
										iprintf("Clunk! A bottle rolls out.\n");
										addObject(game.player.x, game.player.y, BOTTLE_OF_WATER);
									}
									o->quantity--;
								} else {
									iprintf("Nothing happens.\n");
								}
								game.cooldown += 5;
							}
						}
					}
				}
			} else return;
		}
		if (keys & KEY_A) {
			seek_and_destroy(); return;
		}

	} else if (game.cooldown > 0)
		game.cooldown--;
}

void update_projectiles() {
	Projectile *p = game.map.projectiles.head();
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
			if (game.map.solid(p->st.posx(),p->st.posy())) {
				collided = true;
				break;
			}
			x = p->st.posx();
			y = p->st.posy();
		}

		if ((x == destx && y == desty) || collided) {
			Projectile *next = p->next();
			game.map.projectiles.remove(p);
			stack_item_push(game.map.at(x,y)->objects, p->obj);
			delete p;
			p = next;
		} else {
			game.map.at(x,y)->objects.push(p->obj);
			p = p->next();
		}
	}
}

void update_animations() {
	Animation *ani = game.map.animations.head();
	while (ani) {
		assert(ani->obj->desc()->animation);
		if (ani->frame >= 0) {
			// dirty the cache iff it's on the screen
			if (ani->x >= torch.buf.scroll.x && ani->x < torch.buf.scroll.x+32 &&
			    ani->y >= torch.buf.scroll.y && ani->y < torch.buf.scroll.y+24)
				torch.buf.cacheat(ani->x, ani->y)->dirty = 1;
			ani->obj->quantity = ani->frame/4; // whee, hacks
			int len = strlen(ani->obj->desc()->animation);
			if (ani->obj->quantity >= len) {
				ani->obj->quantity = 0;
				ani->frame = -rand8();
			}
		}
		ani->frame++;

		ani = ani->next();
	}
}

void process_sight() {
	// go 1 over the boundaries to be sure we mark everything properly, even if
	// we scrolled just now...
	for (int y = torch.buf.scroll.y-1; y < torch.buf.scroll.y + 24 + 1; y++)
		for (int x = torch.buf.scroll.x-1; x < torch.buf.scroll.x + 32 + 1; x++) {
			if (x >= torch.buf.getw() || x < 0 || y >= torch.buf.geth() || y < 0) continue;
			game.map.block.at(x,y)->visible = false;
		}
	game.player.light->x = game.player.x << 12;
	game.player.light->y = game.player.y << 12;
	cast_sight(game.fov_sight, &game.map.block, game.player.light);
}

void step_monsters() {
	for (Creature *m = game.map.monsters.head(); m; m = m->next()) {
		step_creature(m);
	}
}

void handler() {
	process_keys();
	update_projectiles();
	update_animations();
	step_monsters();

	game.player.light->update_flicker();
	process_sight();
	draw_lights(game.fov_light, &game.map.block, game.map.lights);
	{
		lightsource *k = game.map.lights.head();
		for (; k; k = k->next()) {
			k->update_flicker();
			draw_light(game.fov_light, &game.map.block, k);
		}
	}

	u32 keys = keysHeld();
	u32 down = keysDown();
	touchPosition touch = touchReadXY();
	if (down & KEY_TOUCH && keys & KEY_R && touch.px != 0 && touch.py != 0) {
		luxel *l = torch.buf.luxat(torch.buf.scroll.x + touch.px/8, torch.buf.scroll.y + touch.py/8);
		iprintf("c:%d,%d,%d v:%d, low:%d\n", l->lr, l->lg, l->lb, l->lval, torch.get_low_luminance());
	}

	refresh(&game.map.block);

	if (game.player.hp <= 0) {
		playerdeath();
		game.player.clear();
		new_game();
	}

	if (game.cooldown <= 0) game.player.regenerate();

	statusbar();
}

void new_game() {
	game.player.exist();

	generate_terrarium();

	game.player.setPos(game.player.x,game.player.y);
	game.map.at(game.player.x,game.player.y)->creature = &game.player;

	torch.buf.scroll.x = game.player.x - 16;
	torch.buf.scroll.y = game.player.y - 12;
	torch.buf.bounded(torch.buf.scroll.x, torch.buf.scroll.y);
	torch.dirty_screen();
	torch.reset_luminance();
}

void init_world() {
	lcdMainOnBottom();

#ifndef NATIVE
	// TODO: replace with gettimeofday() or similar
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));
#else
	{
		struct timeval tv;
		gettimeofday(&tv,NULL);
		init_genrand(tv.tv_usec+tv.tv_sec*1000000);
	}
#endif

	new_game();

	torch.run(handler);
}
