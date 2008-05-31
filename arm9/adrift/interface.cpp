#include "interface.h"
#include <nds.h>
#include "torch.h"
#include "list.h"
#include "object.h"
#include "adrift.h"
#include "text.h"
#include <stdarg.h>
#include <stdio.h>

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
	va_list ap;
	char foo[100];
	va_start(ap, fmt);
	int len = vsnprintf(foo, 100, fmt, ap);
	va_end(ap);
	int width = textwidth(foo);
	text_render_raw(128-width/2, y, foo, len, color | BIT(15));
}

enum ACTION {
	ACT_NONE,
	ACT_DROP,
	ACT_THROW,
	ACT_EQUIP,
	ACT_EAT,
	ACT_USE,
};

void perform(Node<Object>* obj, ACTION act) {
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
		default:
			break;
	}
}

struct menuitem {
	const char *text;
	ACTION action;
} itemmenu[] = {
	{ "Use", ACT_USE },
	{ "Throw", ACT_THROW },
	{ "Drop", ACT_DROP },
	{ 0 }
};

bool canuse(Object *obj) {
	return obj->type == J_ROCK;
}

bool withitem(Node<Object>* obj) {
	int sel = 0;
	text_display_clear();
	if (obj->data.quantity == 1) {
		printcenter(40, 0xffff, "%s", obj->data.desc().name);
	} else {
		printcenter(40, 0xffff, "%d %ss", obj->data.quantity, obj->data.desc().name);
	}
	while (1) {
		int i = 0;
		ACTION act = ACT_NONE;
		for (menuitem* k = itemmenu; k->text; k++) {
			bool validaction = false;
			switch (k->action) {
				case ACT_USE:   validaction = canuse(&obj->data); break;
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
}

void inventory() {
	int selected = 0;
	int start = 0;
	int length = game.player.bag.length();

	//ACTION act = ACT_NONE;
	Node<Object> *sel = NULL;

	lcdMainOnTop();

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
			if (o->data.quantity == 1)
				tprintf(17, 12+i*9, color, "%s", name);
			else
				tprintf(17, 12+i*9, color, "%d %ss", o->data.quantity, name);
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

	/*switch (act) {
		case ACT_DROP:
			game.player.drop(sel);
			break;
		case ACT_THROW:
			break;
		case ACT_USE:
			game.player.use(sel);
			break;
		default:
			break;
	}*/
}

