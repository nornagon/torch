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

Adrift game;

Adrift::Adrift() {
	// the fov_settings_type that will be used for non-player-held lights.
	fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);
}

/* this bit's annoying. These recalc functions update the representation of one
 * cell. I have two of them, one for when the cell in question is actually in
 * view, and one for when it's just gone out of sight. The reason they're
 * different is so I can make the map forget monsters that are out of FOV, but
 * remember items. The latter are far less likely to move, you see.
 *
 * I don't think this is easily genericable.
 */
void recalc_recall(s16 x, s16 y) {
	u16 ch, col;
	Cell *l = game.map.at(x,y);
	if (l->objects.head) {
		ch = objdesc[l->objects.head->data.type].ch;
		col = objdesc[l->objects.head->data.type].col;
	} else if ((game.map.block.at(x,y)->visible && torch.buf.luxat(x,y)->lval > 0) || !celldesc[l->type].forgettable) {
		ch = celldesc[l->type].ch;
		col = celldesc[l->type].col;
	} else { ch = ' '; col = RGB15(0,0,0); }
	mapel *m = torch.buf.at(x,y);
	m->ch = ch;
	m->col = col;
}

void recalc_visible(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (l->creatures.head) {
		mapel *m = torch.buf.at(x,y);
		m->ch = creaturedesc[l->creatures.head->data.type].ch;
		m->col = creaturedesc[l->creatures.head->data.type].col;
	} else recalc_recall(x,y);
}

bool isvowel(char c) {
	return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

bool get_items() {
	List<Object> &os = game.map.at(game.player.x, game.player.y)->objects;
	if (os.empty()) return false;

	Node<Object> *k;
	while ((k = os.pop())) {
		char *name = objdesc[k->data.type].name;
		if (k->data.quantity == 1) {
			const char *a_an = isvowel(name[0]) ? "n" : "";
			iprintf("You pick up a%s %s.\n", a_an, name);
		} else {
			iprintf("You pick up %d %ss.\n", k->data.quantity, name);
		}

		// go through the bag to see if we can stack.
		stack_item_push(game.player.bag, k);
	}

	game.cooldown = 4;
	return true;
}

void process_keys() {
	if (game.cooldown <= 0) {
		scanKeys();
		u32 keys = keysHeld();

		u32 down = keysDown();
		if (down & KEY_X) {
			inventory(); return;
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

	} else if (game.cooldown > 0)
		game.cooldown--;
}

void process_sight() {
	game.player.light->x = game.player.x << 12;
	game.player.light->y = game.player.y << 12;
	cast_sight(game.fov_sight, &game.map.block, game.player.light);
}

void handler() {
	process_keys();
	process_sight();
	draw_lights(game.fov_light, &game.map.block, game.map.lights);

	for (int y = torch.buf.scroll.y; y < torch.buf.scroll.y + 24; y++)
		for (int x = torch.buf.scroll.x; x < torch.buf.scroll.x + 32; x++) {
			int32 v = torch.buf.luxat(x, y)->lval;
			mapel *m = torch.buf.at(x, y);
			m->recall = min(1<<12, max(v, m->recall));

			blockel *b = game.map.block.at(x, y);
			if (b->visible && torch.buf.luxat(x,y)->lval > 0) {
				recalc_visible(x,y);
			} else if (torch.buf.cacheat(x,y)->was_visible) {
				recalc_recall(x,y);
			}
			b->visible = false;
		}
}

void new_game() {
	game.map.resize(128,128);
	game.map.block.resize(128,128);
	torch.buf.resize(128,128);

	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));

	generate_terrarium();

	game.player.exist();
	recalc_visible(game.player.x, game.player.y);

	torch.buf.scroll.x = game.player.x - 16;
	torch.buf.scroll.y = game.player.y - 12;
	torch.buf.bounded(torch.buf.scroll.x, torch.buf.scroll.y);

	torch.run(handler);
}

void init_world() {
	lcdMainOnBottom();
	new_game();
}
