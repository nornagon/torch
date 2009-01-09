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
#include "gfxPrimitives.h"
#ifdef NATIVE
#include "native.h"
#endif

inline void pixel(u16* surf, u8 x, u8 y, u16 color, int32 alpha=1<<12) {
	if (alpha < 1<<12) {
		u16 targ_color = surf[y*256+x];
		int rt, gt, bt; // target
		extractComponents(targ_color, rt,gt,bt);
		int rs, gs, bs; // source
		//if (rt != 0 && gt != 0 && bt != 0) printf("alpha %d,%d,%d to %d,%d,%d", rs, gs, bs, rt, gt, bt);
		extractComponents(color, rs,gs,bs);
		int rf, gf, bf; // final
		rf = ((rt * ((1<<12)-alpha)) >> 12) + ((rs * alpha) >> 12);
		gf = ((gt * ((1<<12)-alpha)) >> 12) + ((gs * alpha) >> 12);
		bf = ((bt * ((1<<12)-alpha)) >> 12) + ((bs * alpha) >> 12);
		color = RGB15(rf,gf,bf);
	}
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

void perform(Object *obj, ACTION act) {
	switch (act) {
		case ACT_DROP:
			game.player.drop(obj);
			break;
		case ACT_USE:
			game.player.use(obj);
			break;
		case ACT_THROW:
			game.player.projectile = obj;
			break;
		case ACT_EAT:
			game.player.eat(obj);
			break;
		case ACT_DRINK:
			game.player.drink(obj);
			break;
		case ACT_EQUIP:
			game.player.equip(obj);
			break;
		default:
			break;
	}
}

struct menuitem {
	const char *text;
	ACTION action;
} itemmenu[] = {
	{ "Equip", ACT_EQUIP },
	{ "Eat", ACT_EAT },
	{ "Drink", ACT_DRINK },
	{ "Use", ACT_USE },
	{ "Throw", ACT_THROW },
	{ "Drop", ACT_DROP },
	{ 0 }
};

bool withitem(Object *obj) {
#ifndef NATIVE
	int sel = 0;
	text_display_clear();
	if (obj->quantity == 1) {
		printcenter(40, 0xffff, "%s", obj->desc()->name);
	} else if (obj->desc()->plural) {
		printcenter(40, 0xffff, "%d %s", obj->quantity, obj->desc()->plural);
	} else {
		printcenter(40, 0xffff, "%d %ss", obj->quantity, obj->desc()->name);
	}
	while (1) {
		int i = 0;
		ACTION act = ACT_NONE;
		for (menuitem* k = itemmenu; k->text; k++) {
			bool validaction = false;
			switch (k->action) {
				case ACT_EQUIP: validaction = obj->desc()->equip < E_NUMSLOTS; break;
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

const char *equipdesc(Object *o) {
	switch (o->desc()->equip) {
		case E_WEAPON: return " (wielded)";

		case E_HEAD:
		case E_WRIST:
		case E_FEET:
		case E_BODY:   return " (worn)";
	}
	assert(!"Impossible equipment location");
	return NULL;
}

void inventory() {
#ifndef NATIVE
	int selected = 0;
	int start = 0;
	int length = game.player.bag.length();

	Object *sel = 0;

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

		Object *o = game.player.bag.head();

		// push o up to the start of the on-screen section of the inventory
		int i = 0;
		for (; i < start; i++) o = o->next();

		for (i = 0; i < 19 && o; i++, o = o->next()) {
			const char *name = o->desc()->name;
			u16 color = selected == i+start ? RGB15(31,31,31) : RGB15(18,18,18);
			const char *equiptext = "";
			if (game.player.isEquipped(o)) {
				color = selected == i+start ? RGB15(0,31,0) : RGB15(0,18,0);
				equiptext = equipdesc(o);
			}
			if (o->quantity == 1)
				tprintf(17, 12+i*9, color, "%s%s", name, equiptext);
			else if (o->desc()->plural)
				tprintf(17, 12+i*9, color, "%d %s%s", o->quantity, o->desc()->plural, equiptext);
			else
				tprintf(17, 12+i*9, color, "%d %ss%s", o->quantity, name, equiptext);
			if (i+start == selected) {
				sel = o;
				tprintf(8, 12+i*9, 0xffff, "*");
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

struct pixel_i_helper {
	u16 *target;
	int32 alpha;
	u16 color;
};
void pixel_i(s16 x, s16 y, void *info) {
	pixel_i_helper *p = (pixel_i_helper*)info;
	if (x >= 0 && x < 256 && y >= 0 && y < 192)
		pixel(p->target, x,y, p->color, p->alpha);
}

void refresh_map_i(s16 x, s16 y, void *info) {
	pixel_i_helper *p = (pixel_i_helper*)info;
	int offx = torch.buf.getw() / 2 - 256/2,
	    offy = torch.buf.geth() / 2 - 192/2;
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
		pixel(p->target, x - offx, y - offy, color);
	}
}

void overview() {
#ifndef NATIVE
	text_console_disable();

	int offx = torch.buf.getw() / 2 - 256/2,
	    offy = torch.buf.geth() / 2 - 192/2;

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
				pixel(subscr, x - offx, y - offy, color);
			}
		}
	}

	int playerfade = 0;
	int rad = 0;
	while (1) {
		u32 held = 0;
		scanKeys();
		held = keysHeld();

		int32 k = (COS[playerfade] >> 1) + (1<<11);
		u16 color = RGB15((31 * k) >> 12, (31 * k) >> 12, (31 * k) >> 12);
		pixel(subscr, game.player.x - offx,
									game.player.y - offy,
									color);
		playerfade = (playerfade + 8) % 0x1ff;

		struct pixel_i_helper p;
		p.target = subscr;
		p.alpha = 1<<10;
		k = (COS[(playerfade+0xff)%0x1ff] >> 1) + (1<<11);
		p.color = RGB15((31 * k) >> 12, (31 * k) >> 12, (31 * k) >> 12);
		hollowCircleNoClip(game.player.x, game.player.y, rad, refresh_map_i, &p);
		rad = playerfade / 64;
		hollowCircleNoClip(game.player.x - offx, game.player.y - offy, rad, pixel_i, &p);

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
	static u16* subscr = (u16*)BG_BMP_RAM_SUB(0);
	static int hptw = textwidth("HP:");
	int hp = game.player.hp, max = game.player.max_hp();
	char buf[8];
	memset(&subscr[256*(192-9)], 0, 256*9*2);

	tprintf(2,192-9, 0xffff, "HP:");

	u16 color = RGB15(0,31,0);
	if (hp <= (max*(int32)(0.5*(1<<12)))>>12) {
		// hp <= 50% max ==> red
		color = RGB15(31,0,0);
	} else if (hp <= (max*(int32)(0.75*(1<<12)))>>12) {
		// hp <= 75% max ==> yellow
		color = RGB15(31,31,0);
	}

	sniprintf(buf, 8, "%d", hp);
	tprintf(2+hptw,192-9, color, buf);
	int curoff = textwidth(buf);
	tprintf(2+hptw+curoff,192-9,0xffff, "/%d", max);
#endif
}
