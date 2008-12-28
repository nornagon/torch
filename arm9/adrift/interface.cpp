#include "interface.h"
#include <nds.h>
#include "torch.h"
#include "list.h"
#include "object.h"
#include "adrift.h"
#include "text.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef NATIVE
#include "native.h"
#endif

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


u32 waitkey() {
	u32 keys = 0;
	while (!keys) {
		swiWaitForVBlank();
		scanKeys();
		keys = keysDown();
	}
	return keys;
}

void printcenter(u16 y, u16 color, const char *fmt, ...) {
#ifndef NATIVE
	va_list ap;
	char foo[100];
	va_start(ap, fmt);
	int len = vsnprintf(foo, 100, fmt, ap);
	va_end(ap);
	int width = textwidth(foo);
	text_render_raw(128-width/2, y, foo, len, color | BIT(15));
#endif
}

enum ACTION {
	ACT_NONE,
	ACT_DROP,
	ACT_THROW,
	ACT_EQUIP,
	ACT_EAT,
	ACT_DRINK,
	ACT_USE,
};

void perform(Node<Object> obj, ACTION act) {
	switch (act) {
		case ACT_DROP:
			game.player.drop(obj);
			break;
		case ACT_USE:
			game.player.use(obj);
			break;
		case ACT_THROW:
			game.player.setprojectile(obj);
			break;
		case ACT_EAT:
			game.player.eat(obj);
			break;
		case ACT_DRINK:
			game.player.drink(obj);
			break;
		default:
			break;
	}
}

struct menuitem {
	const char *text;
	ACTION action;
} itemmenu[] = {
	{ "Eat", ACT_EAT },
	{ "Drink", ACT_DRINK },
	{ "Use", ACT_USE },
	{ "Throw", ACT_THROW },
	{ "Drop", ACT_DROP },
	{ 0 }
};

bool withitem(Node<Object> obj) {
#ifndef NATIVE
	int sel = 0;
	text_display_clear();
	if (obj->quantity == 1) {
		printcenter(40, 0xffff, "%s", obj->desc()->name);
	} else {
		printcenter(40, 0xffff, "%d %ss", obj->quantity, obj->desc()->name);
	}
	while (1) {
		int i = 0;
		ACTION act = ACT_NONE;
		for (menuitem* k = itemmenu; k->text; k++) {
			bool validaction = false;
			switch (k->action) {
				case ACT_EAT:   validaction = obj->desc()->edible; break;
				case ACT_DRINK: validaction = obj->desc()->drinkable; break;
				case ACT_USE:   validaction = obj->desc()->usable; break;
				case ACT_DROP:  validaction = true; break;
				case ACT_THROW: validaction = true; break;
				default: validaction = false; break;
			}
			if (validaction) {
				if (sel == i) act = k->action;
				printcenter(54 + i*9, sel == i ? 0xffff : RGB15(18,18,18), "%s", k->text);
				i++;
			}
		}
		int keys = waitkey();
		if (keys & KEY_DOWN) sel++;
		if (keys & KEY_UP) sel--;
		if (keys & KEY_B) return false;
		if (keys & KEY_A) {
			perform(obj, act);
			return true;
		}
		if (sel < 0) sel = 0;
		if (sel >= i) sel = i - 1;
	}
#else
	return true;
#endif
}

void inventory() {
#ifndef NATIVE
	int selected = 0;
	int start = 0;
	int length = game.player.bag.length();

	Node<Object> sel;

	lcdMainOnTop();

	text_console_disable();

	u16* subscr = (u16*)BG_BMP_RAM_SUB(0);

	while (1) {
		swiWaitForVBlank();
		text_display_clear();

		// print the border
		hline(subscr, 4, 255-5, 4, RGB15(31,31,31));
		hline(subscr, 4, 255-5, 191-5, RGB15(31,31,31));
		vline(subscr, 4, 4, 191-5, RGB15(31,31,31));
		vline(subscr, 255-5, 4, 191-5, RGB15(31,31,31));
		tprintf(8,1, 0xffff, "Inventory");

		Node<Object> o = game.player.bag.top();

		// push o up to the start
		int i = 0;
		for (; i < start; i++) o = o.next();

		for (i = 0; i < 19 && o; i++, o = o.next()) {
			const char *name = o->desc()->name;
			u16 color = selected == i+start ? RGB15(31,31,31) : RGB15(18,18,18);
			if (o->quantity == 1)
				tprintf(17, 12+i*9, color, "%s", name);
			else if (o->desc()->plural)
				tprintf(17, 12+i*9, color, "%d %s", o->quantity, o->desc()->plural);
			else
				tprintf(17, 12+i*9, color, "%d %ss", o->quantity, name);
			if (i+start == selected) {
				sel = o;
				tprintf(8, 12+i*9, color, "*");
			}
		}
		u32 keys = waitkey();
		if (keys & KEY_B) break;
		if (keys & KEY_A && sel) {
			if (withitem(sel)) break;
			continue;
		}
		if (keys & KEY_UP) selected = max(0,selected-1);
		else if (keys & KEY_DOWN) selected = min(length-1, selected+1);

		if (length > 19) {
			if (selected < start+3)
				start = max(0,start-1);
			else if (selected > start+15)
				start = min(length-19,start+1);
		}
	}

	lcdMainOnBottom();

	text_console_enable();
#endif
}

void overview() {
#ifndef NATIVE
	text_console_disable();

	u16* subscr = (u16*)BG_BMP_RAM_SUB(0);
	text_display_clear();
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.contains(x,y)) {
				u16 color = game.map.at(x,y)->desc()->color;
				if (x == game.player.x && y == game.player.y)
					color = RGB15(31,31,31);
				else if (game.map.at(x,y)->desc()->forgettable)
					color = 0;
				else {
					u32 r = color & 0x001f,
							g = (color & 0x03e0) >> 5,
							b = (color & 0x7c00) >> 10;
					int32 recall = torch.buf.at(x,y)->recall;
					r = (r * recall) >> 12;
					g = (g * recall) >> 12;
					b = (b * recall) >> 12;
					color = RGB15(r,g,b);
				}
				pixel(subscr,
				      x - torch.buf.getw() / 2 + 256/2,
				      y - torch.buf.geth() / 2 + 192/2,
				      color);
			}
		}
	}

	int playerfade = 0;
	while (1) {
		u32 held = 0;
		scanKeys();
		held = keysHeld();
		int32 k = (COS[playerfade] >> 1) + (1<<11);
		u16 color = RGB15((31 * k) >> 12, (31 * k) >> 12, (31 * k) >> 12);
		pixel(subscr, game.player.x - torch.buf.getw() / 2 + 256/2,
									game.player.y - torch.buf.geth() / 2 + 192/2,
									color);
		playerfade = (playerfade + 8) % 0x1ff;
		if (!(held & KEY_L)) break;
		swiWaitForVBlank();
	}
	text_console_enable();
#endif
}

void playerdeath() {
	statusbar();
	iprintf("You die... press start to continue.\n");
	while (!(waitkey() & KEY_START));
}

void statusbar() {
#ifndef NATIVE
	u16* subscr = (u16*)BG_BMP_RAM_SUB(0);
	memset(&subscr[256*(192-9)], 0, 256*9*2);
	tprintf(2,192-9,0xffff, "HP:%d/%d", game.player.obj->hp, game.player.obj->max_hp());
#endif
}
