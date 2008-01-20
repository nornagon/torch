#include "world.h"
#include "messages.h"

#include "fov.h"
#include "generic.h"
#include "mersenne.h"
#include "light.h"
#include "sight.h"
#include "text.h"
#include "draw.h"

#define NUM_NKIS 15

enum {
	T_GROUND,
	T_NKI,
	T_KITTEN
};

fov_settings_type *fov_light, *fov_sight;

objecttype_t ot_robot = {
	.ch = '#',
	.col = RGB15(31,31,31),
	.importance = 0xff,
};
objecttype_t *OT_ROBOT = &ot_robot;

void new_robot(map_t *map, s32 x, s32 y) {
	map->game = new_object(map, OT_ROBOT, NULL);
	insert_object(map, map->game, x, y);
	map->pX = x;
	map->pY = y;
}

typedef struct nki_s {
	u32 id;
	u32 display;
	light_t light;
} nki_t;

u32 show_nki(object_t *obj, map_t *map) {
	return ((nki_t*)obj->data)->display;
}

void end_nki(object_t *obj, map_t *map) {
	free(obj->data);
}

objecttype_t ot_nki = {
	.ch = '!',
	.col = 0,
	.importance = 1,
	.display = show_nki,
	.end = end_nki
};
objecttype_t *OT_NKI = &ot_nki;

void nki_light(process_t *proc, map_t *map) {
	draw_light(map, fov_light, &((nki_t*)proc->data)->light);
}

void new_nki(map_t *map, s32 x, s32 y) {
	nki_t *nki = malloc(sizeof(nki_t));
	memset(nki, 0, sizeof(nki_t));
	nki->id = genrand_int32() % MESSAGES;
	u32 a = genrand_int32();
	if (a & 1) {
		a >>= 1;
		nki->display = ('a' + a % 25) << 16;
	} else {
		a >>= 1;
		nki->display = ('A' + a % 25) << 16;
	}
	int32 r = 1 + (genrand_int32() & 0xfff);
	int32 g = 1 + (genrand_int32() & 0xfff);
	int32 b = 1 + (genrand_int32() & 0xfff);
	nki->display |= RGB15((r*31)>>12, (g*31)>>12, (b*31)>>12);

	set_light(&nki->light, (genrand_int32() & 0x1fff) + (3<<12), r, g, b);
	nki->light.x = x<<12;
	nki->light.y = y<<12;

	push_process(map, nki_light, NULL, nki);
	insert_object(map, new_object(map, OT_NKI, nki), x, y);
}

void nki_hit(object_t *obj) {
	nki_t *nki = obj->data;
	iprintf("%s\n", messages[nki->id]);
}

void gen_map(map_t *map) {
	reset_map(map);
	u32 x, y;
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *c = cell_at(map, x, y);
			c->ch = '.';
			c->opaque = false;
			c->forgettable = true;
			u32 a = genrand_int32();
			c->col = RGB15(15 + (a&3), 15 + ((a>>2)&3), 15 + ((a>>4)&3));
		}

	int i;
	for (i = 0; i < NUM_NKIS; i++) {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		cell_t *c = cell_at(map, x, y);
		if (c->type == T_NKI) {
			i--;
			continue;
		}
		c->type = T_NKI;
		new_nki(map, x, y);
	}
	cell_t *c;
	do {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		c = cell_at(map, x, y);
	} while (c->type == T_NKI);
	c->type = T_KITTEN;
	new_nki(map, x, y); // muaha

	do {
		x = genrand_int32() % (256/8);
		y = genrand_int32() % (192/8);
		c = cell_at(map, x, y);
	} while (c->type != T_GROUND);
	new_robot(map, x, y);
}

void found_kitten(map_t *map, nki_t *nki) {
	u16 *vram = (u16*)BG_BMP_RAM_SUB(0);
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
	gen_map(map);
}

void process_robot(map_t *map) {
	static int frm = 0;
	scanKeys();

	light_t l;
	set_light(&l, 4<<12, 0.5*(1<<12), 0.5*(1<<12), 0.5*(1<<12));
	l.x = map->pX << 12;
	l.y = map->pY << 12;
	fov_circle(fov_sight, map, &l, map->pX, map->pY, 32);
	cell_t *cell = cell_at(map, map->pX, map->pY);
	cell->light = 1<<12;
	cell->visible = true;
	cache_t *cache = cache_at(map, map->pX, map->pY);
	cache->lr = l.r;
	cache->lg = l.g;
	cache->lb = l.b;

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
		if (!dx && !dy) return;

		if (map->pX + dx < 0 || map->pX + dx >= map->w) dx = 0;
		if (map->pY + dy < 0 || map->pY + dy >= map->w) dy = 0;
		cell_t *cell = cell_at(map, map->pX + dx, map->pY + dy);
		switch (cell->type) {
			case T_NKI:
				nki_hit(node_data(cell->objects));
				frm = 10;
				break;
			case T_KITTEN:
				found_kitten(map, ((object_t*)node_data(cell->objects))->data);
				frm = 0;
				break;
			default:
				if (dx && dy) frm = 7;
				else frm = 5;
				cache_at(map, map->pX, map->pY)->dirty = 2;
				map->pX += dx;
				map->pY += dy;
				cache_at(map, map->pX, map->pY)->dirty = 2;
				move_object(map, map->game, map->pX, map->pY);
		}
	} else if (frm > 0)
		frm--;
}

void init_world() {
	map_t *map = create_map(256/8, 192/8);

	fov_light = malloc(sizeof(fov_settings_type));
	fov_settings_init(fov_light);
	fov_settings_set_shape(fov_light, FOV_SHAPE_OCTAGON);
	fov_settings_set_opacity_test_function(fov_light, opacity_test);
	fov_settings_set_apply_lighting_function(fov_light, apply_light);

	fov_sight = malloc(sizeof(fov_settings_type));
	fov_settings_init(fov_sight);
	fov_settings_set_shape(fov_sight, FOV_SHAPE_SQUARE);
	fov_settings_set_opacity_test_function(fov_sight, sight_opaque);
	fov_settings_set_apply_lighting_function(fov_sight, apply_sight);

	init_genrand(IPC->time.rtc.seconds + IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
			IPC->time.rtc.weekday*7*24*60*60);
	gen_map(map);

	map->handler = process_robot;

	run(map);
}
