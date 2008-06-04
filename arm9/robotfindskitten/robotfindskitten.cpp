#include "world.h"
#include "messages.h"

#include "fov.h"
#include "mersenne.h"
#include "lightsource.h"
#include "light.h"
#include "sight.h"
#include "text.h"
#include "draw.h"

#include <stdio.h>

#define NUM_NKIS 15

enum {
	T_GROUND,
	T_NKI,
	T_KITTEN
};

struct nki {
	u32 id;
	u16 ch, col;
	lightsource light;
	void hit();
};

void nki::hit() {
}

struct rfk_t {
	fov_settings_type *fov_light, *fov_sight;
	s16 robotx, roboty;
	s16 kittenx, kitteny;
	u16 underneath; // omg hax to avoid having a real game map
	List<nki> nkis;
	blockmap block;
} rfk;

void new_nki(s32 x, s32 y) {
	Node<nki> *i = Node<nki>::pool.request_node();
	i->data.id = genrand_int32() % MESSAGES;
	u32 a = rand32();
	if (a & 1) {
		a >>= 1;
		i->data.ch = ('a' + a % 25);
	} else {
		a >>= 1;
		i->data.ch = ('A' + a % 25);
	}
	int32 r = 1 + (rand32() & 0xfff);
	int32 g = 1 + (rand32() & 0xfff);
	int32 b = 1 + (rand32() & 0xfff);
	i->data.col = RGB15((r*31)>>12, (g*31)>>12, (b*31)>>12);

	set_light(&i->data.light, (rand32() & 0x1fff) + (3<<12), r, g, b);
	i->data.light.x = x<<12;
	i->data.light.y = y<<12;

	mapel *m = torch.buf.at(x,y);
	m->ch = i->data.ch;
	m->col = i->data.col;

	rfk.nkis.push(i);
	rfk.block.at(x,y)->opaque = true;
}

void gen_map() {
	torch.buf.reset();
	torch.buf.cache.reset();
	rfk.block.reset();

	s16 x, y;
	for (y = 0; y < torch.buf.geth(); y++)
		for (x = 0; x < torch.buf.getw(); x++) {
			mapel *m = torch.buf.at(x, y);
			m->ch = '.';
			u32 a = genrand_int32();
			m->col = RGB15(15 + (a&3), 15 + ((a>>2)&3), 15 + ((a>>4)&3));
		}

	int i;
	for (i = 0; i < NUM_NKIS; i++) {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		mapel *m = torch.buf.at(x, y);
		if (m->ch != '.') {
			continue;
		}
		new_nki(x, y);
	}
	mapel *m;
	do {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		m = torch.buf.at(x, y);
	} while (m->ch != '.');
	rfk.kittenx = x;
	rfk.kitteny = y;
	new_nki(x, y); // muaha

	do {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		m = torch.buf.at(x, y);
	} while (m->ch != '.');
	rfk.robotx = x;
	rfk.roboty = y;
	m->ch = '#';
	rfk.underneath = m->col;
	m->col = RGB15(31,31,31);

	rfk.block.refresh();
}

void found_kitten() {
	/*u16 *vram = (u16*)BG_BMP_RAM_SUB(0);
	u32 rx, kx;
	for (rx = 256/4, kx = 3*256/4; rx + 6 < kx; rx++, kx--) {
		swiWaitForVBlank();
		text_display_clear();
		drawch(vram, rx, 192/2-4, '#', 0xffff);
		drawch(vram, kx, 192/2-4, nki->display>>16, nki->display & 0xffff);
	}
	text_render_raw(50, 192/2+10, "You found kitten! Way to go, robot!", 35, 0xffff);
	do {
		swiWaitForVBlank();
		scanKeys();
	} while (!(keysDown() & KEY_A));
	text_console_clear();
	text_display_clear();
	gen_map(map);*/
}

void process_robot() {
	static int frm = 0;
	scanKeys();

	if (frm == 0) {
		u32 keys = keysHeld();

		int dx = 0, dy = 0;
		if (keys & KEY_LEFT)
			dx = -1;
		if (keys & KEY_RIGHT)
			dx = 1;
		if (keys & KEY_DOWN)
			dy = 1;
		if (keys & KEY_UP)
			dy = -1;
		if (!dx && !dy) goto fin;

		if (rfk.robotx + dx < 0 || rfk.robotx + dx >= torch.buf.getw()) dx = 0;
		if (rfk.roboty + dy < 0 || rfk.roboty + dy >= torch.buf.geth()) dy = 0;
		mapel *m = torch.buf.at(rfk.robotx + dx, rfk.roboty + dy);
		if (m->ch == '.') {
			if (dx && dy) frm = 7;
			else frm = 5;
			mapel *mhere = torch.buf.at(rfk.robotx, rfk.roboty);
			mhere->ch = '.'; mhere->col = rfk.underneath;
			rfk.robotx += dx;
			rfk.roboty += dy;
			m->ch = '#'; rfk.underneath = m->col; m->col = RGB15(31,31,31);
		} else if (rfk.robotx + dx == rfk.kittenx && rfk.roboty + dy == rfk.kitteny) {
			//found_kitten(((object_t*)node_data(cell->objects))->data);
			frm = 0;
		} else {
			for (Node<nki> *i = rfk.nkis.head; i; i = i->next) {
				if (i->data.light.x>>12 == rfk.robotx + dx && i->data.light.y>>12 == rfk.roboty + dy) {
					i->data.hit();
					frm = 10;
					break;
				}
			}
		}
	} else if (frm > 0)
		frm--;

fin:
	lightsource l;
	set_light(&l, 4<<12, 0.5*(1<<12), 0.5*(1<<12), 0.5*(1<<12));
	l.x = rfk.robotx << 12;
	l.y = rfk.roboty << 12;
	cast_sight(rfk.fov_sight, &rfk.block, &l);

	for (Node<nki> *i = rfk.nkis.head; i; i = i->next) {
		draw_light(rfk.fov_light, &rfk.block, &i->data.light);
	}
}

void init_world() {
	torch.buf.resize(256/8, 192/8);
	rfk.block.resize(256/8, 192/8);

	rfk.fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	rfk.fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);

	gen_map();

	torch.run(process_robot);
}
