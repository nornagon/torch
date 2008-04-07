#include "adrift.h"
#include "mapgen.h"
#include "light.h"
#include "sight.h"
#include "text.h"

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

inline bool solid(s16 x, s16 y) {
	Cell *cell = game.map.at(x, y);
	return celldesc[cell->type].solid;
}

inline bool occupied(s16 x, s16 y) {
	return !game.map.at(x,y)->creatures.empty();
}

inline bool walkable(s16 x, s16 y) {
	return !(solid(x,y) || occupied(x,y));
}

void move_player(DIRECTION dir) {
	s32 pX = game.player.x, pY = game.player.y;

	int dpX = D_DX[dir],
	    dpY = D_DY[dir];

	if (pX + dpX < 0 || pX + dpX >= torch.buf.getw()) { dpX = 0; }
	if (pY + dpY < 0 || pY + dpY >= torch.buf.geth()) { dpY = 0; }

	Cell *cell;

	if (!walkable(pX + dpX, pY + dpY)) {
		// your path is blocked
		if (dpX && dpY) {
			// if we could just go left or right, do that. This results in 'sliding'
			// along walls when moving diagonally
			if (walkable(pX + dpX, pY))
				dpY = 0;
			else if (walkable(pX, pY + dpY))
				dpX = 0;
			else
				dpX = dpY = 0;
		} else
			dpX = dpY = 0;
	}

	if (dpX || dpY) {
		// moving diagonally takes longer. 5*sqrt(2) ~= 7
		if (dpX && dpY) game.cooldown = 7;
		else game.cooldown = 5;

		cell = game.map.at(pX + dpX, pY + dpY);

		// move the player object
		game.map.at(pX, pY)->creatures.remove(game.player.obj);
		game.map.at(pX + dpX, pY + dpY)->creatures.push(game.player.obj);
		// TODO: i assume these two squares are visible... correct?
		recalc_visible(pX, pY);
		recalc_visible(pX + dpX, pY + dpY);

		pX += dpX; game.player.x = pX;
		pY += dpY; game.player.y = pY;

		torch.onscreen(pX,pY,8);
	}
}

bool vowel(char c) {
	return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

bool get_items() {
	List<Object> &os = game.map.at(game.player.x, game.player.y)->objects;
	if (os.empty()) return false;

	Node<Object> *k;
	while ((k = os.pop())) {
		game.player.bag.push(k);
		char *name = objdesc[k->data.type].name;
		const char *a_an = vowel(name[0]) ? "n" : "";
		iprintf("You pick up a%s %s.\n", a_an, name);
	}

	game.cooldown = 4;
	return true;
}

inline void pixel(u16* surf, u8 x, u8 y, u16 color) {
	surf[y*256+x] = color|BIT(15);
}

void hline(u16* surf, u8 x1, u8 x2, u8 y, u16 color) {
	for (; x1 <= x2; x1++)
		pixel(surf, x1, y, color);
}
void vline(u16* surf, u8 x, u8 y1, u8 y2, u16 color) {
	for (; y1 <= y2; y1++)
		pixel(surf, x, y1, color);
}

void tprintf(int x, int y, u16 color, const char *format, ...) {
	va_list ap;
	char foo[100];
	va_start(ap, format);
	int len = vsnprintf(foo, 100, format, ap);
	va_end(ap);
	text_render_raw(x, y, foo, len, color | BIT(15));
}

void inventory() {
	int selected = 0;
	int start = 0;
	int length = game.player.bag.length();

	text_console_disable();

	u16* subscr = (u16*)BG_BMP_RAM_SUB(0);

	while (1) {
		text_display_clear();

		// print the border
		hline(subscr, 4, 255-5, 4, RGB15(31,31,31));
		hline(subscr, 4, 255-5, 191-5, RGB15(31,31,31));
		vline(subscr, 4, 4, 191-5, RGB15(31,31,31));
		vline(subscr, 255-5, 4, 191-5, RGB15(31,31,31));
		tprintf(8,1, 0xffff, "Inventory");

		Node<Object> *o = game.player.bag.head;

		// push o up to the start
		int i = 0;
		for (; i < start; i++) o = o->next;

		for (i = 0; i < 19 && o; i++, o = o->next) {
			const char *name = objdesc[o->data.type].name;
			u16 color = selected == i+start ? RGB15(31,31,31) : RGB15(18,18,18);
			tprintf(17, 12+i*9, color, "%s", name);
			if (i+start == selected)
				tprintf(8, 12+i*9, color, "*");
		}
		u32 keys = 0;
		while (!keys) {
			swiWaitForVBlank();
			scanKeys();
			keys = keysDown();
		}
		if (keys & KEY_B) break;
		if (keys & KEY_UP) selected = max(0,selected-1);
		else if (keys & KEY_DOWN) selected = min(length-1, selected+1);

		if (selected < start+3)
			start = max(0,start-1);
		else if (selected > start+15)
			start = min(length-19,start+1);
	}

	text_console_enable();
}

void new_player() {
	game.player.obj = Node<Creature>::pool.request_node();
	game.map.at(game.player.x, game.player.y)->creatures.push(game.player.obj);
	game.player.obj->data.type = C_PLAYER;
	recalc_visible(game.player.x, game.player.y);
	game.player.light = new_light(7<<12, (int32)(1.00*(1<<12)), (int32)(0.90*(1<<12)), (int32)(0.85*(1<<12)));
}

void updaterecall(s16 x, s16 y) {
	int32 v = torch.buf.luxat(x, y)->lval;
	mapel *m = torch.buf.at(x, y);
	m->recall = min(1<<12, max(v, m->recall));
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
			move_player(dir);
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
			updaterecall(x, y);
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
