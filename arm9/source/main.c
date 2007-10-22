#include "nds.h"
#include <nds/arm9/console.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "mersenne.h"
#include "fov.h"
#include "mem.h"
#include "test_map.h"
#include "draw.h"
#include "map.h"
#include "light.h"

#include "list.h"
#include "process.h"

#include "util.h"
#include "engine.h"

extern u32 vblnks, frames, hblnks, vblnkDirty; // XXX: hax

typedef struct game_s {
	s32 pX, pY;
	light_t *player_light;
	bool torch_on;
	fov_settings_type *fov_light;
} game_t;

static inline game_t *game(map_t *map) {
	return ((game_t*)map->game);
}

//---------------------------------------------------------------------------
// map stuff
typedef enum {
	OT_UNKNOWN = 0,
	OT_WISP,
	OT_FIRE,
	OT_LIGHT
} OBJECT_TYPE;

// TODO: make draw_light take the parameters of the light (colour, radius,
// position) and build up the lighting struct to pass to fov_circle on its
// ownsome?
void draw_light(light_t *l, map_t *map) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < map->scrollX ||
	    ((l->x - l->radius) >> 12) > map->scrollX + 32 ||
	    ((l->y + l->radius) >> 12) < map->scrollY ||
	    ((l->y - l->radius) >> 12) > map->scrollY + 24) return;

	// calculate lighting values
	fov_circle(game(map)->fov_light, (void*)map, (void*)l,
			l->x>>12, l->y>>12, (l->radius>>12) + 1);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	cell_t *cell = cell_at(map, l->x>>12, l->y>>12);
	if (cell->visible) {
		cell->light += (1<<12);
		cache_t *cache = cache_at(map, l->x>>12, l->y>>12);
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

typedef struct {
	node_t *light_node;
	node_t *obj_node;
	unsigned int flickered;
	int32 radius;
	light_t *light;
} obj_fire_t;

void obj_fire_process(process_t *process, map_t *map) {
	obj_fire_t *f = (obj_fire_t*)process->data;
	light_t *l = (light_t*)f->light;

	if (f->flickered == 4 || f->flickered == 0) // radius changes more often than origin.
		l->radius = f->radius + (genrand_gaussian32() >> 19) - 4096;

	if (f->flickered == 0) {
		// shift the origin around. Waiting on blue_puyo for sub-tile origin
		// shifts in libfov.
		object_t *obj = node_data(f->obj_node);
		int32 x = obj->x << 12,
		      y = obj->y << 12;
		u32 m = genrand_int32();
		if (m < (u32)(0xffffffff*0.6)) { l->x = x; l->y = y; }
		else {
			if (m < (u32)(0xffffffff*0.7)) {
				l->x = x; l->y = y + (1<<12);
			} else if (m < (u32)(0xffffffff*0.8)) {
				l->x = x; l->y = y - (1<<12);
			} else if (m < (u32)(0xffffffff*0.9)) {
				l->x = x + (1<<12); l->y = y;
			} else {
				l->x = x - (1<<12); l->y = y;
			}
			if (cell_at(map, l->x>>12, l->y>>12)->opaque)
			{ l->x = x; l->y = y; }
		}
		f->flickered = 8; // flicker every 8 frames
	} else {
		f->flickered--;
	}

	draw_light(l, map);
}

void obj_fire_proc_end(process_t *process, map_t *map) {
	// free the structures we were keeping
	obj_fire_t *fire = (obj_fire_t*)process->data;
	free_object(map, fire->obj_node);
	free(fire->light);
	free(fire);
}

void obj_fire_obj_end(object_t *object, map_t *map) {
	obj_fire_t *fire = (obj_fire_t*)object->data;
	free_process(map, fire->light_node);
	free(fire->light);
	free(fire);
}

// takes x and y as 32.0 cell coordinates, radius as 20.12
void new_obj_fire(map_t *map, s32 x, s32 y, int32 radius) {
	obj_fire_t *fire = malloc(sizeof(obj_fire_t));

	fire->radius = radius;
	fire->flickered = 0;

	light_t *light = new_light(radius, 1.00*(1<<12), 0.65*(1<<12), 0.26*(1<<12));
	fire->light = light;
	light->x = x << 12;
	light->y = y << 12;

	fire->light_node = push_process(map, obj_fire_process, obj_fire_proc_end, fire);
	fire->obj_node = new_object(map, OT_FIRE, fire);
	insert_object(map, fire->obj_node, x, y);
}


// a glowing, coloured light
typedef struct {
	node_t *light_node; // the light-drawing process node
	node_t *obj_node; // the presence object node

	light_t *light;

	// cache the display colour/character so we don't have to recalculate every
	// frame.
	u32 display;
} obj_light_t;

// boilerplate entity-freeing functions. TODO: how to make this easier?
void obj_light_proc_end(process_t *proc, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)proc->data;
	free_object(map, obj_light->obj_node);
	free(obj_light);
}

void obj_light_obj_end(object_t *obj, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)obj->data;
	free_process(map, obj_light->light_node);
	free(obj_light);
}

void obj_light_process(process_t *proc, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)proc->data;
	draw_light(obj_light->light, map);
}

u32 obj_light_display(object_t *obj, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)obj->data;
	return obj_light->display;
}

void new_obj_light(map_t *map, s32 x, s32 y, light_t *light) {
	obj_light_t *obj_light = malloc(sizeof(obj_light_t));
	obj_light->light = light;
	light->x = x<<12; light->y = y<<12;
	// we cache the display characteristics of the light for performance reasons.
	obj_light->display = ('o'<<16) |
		RGB15((light->r * (31<<12))>>24,
		      (light->g * (31<<12))>>24,
		      (light->b * (31<<12))>>24);
	// create the lighting process and store the node for cleanup purposes
	obj_light->light_node = push_process(map,
			obj_light_process, obj_light_proc_end, obj_light);
	// create the worldly presence of the light
	obj_light->obj_node = new_object(map, OT_LIGHT, obj_light);
	// and add it to the map
	insert_object(map, obj_light->obj_node, x, y);
}

u32 random_colour(object_t *obj, map_t *map) {
	return ((map->objtypes[obj->type].ch)<<16) | (genrand_int32()&0xffff);
}

#include "willowisp.h" // evil hacks, i know, but i got sick of scrolling through it

objecttype_t objects[] = {
	// 0: unknown object
	[OT_UNKNOWN] = {
		.ch = '?',
		.col = RGB15(31,31,31),
		.importance = 3,
		.display = random_colour,
		.end = NULL
	},
	// 1: will o' wisp
	[OT_WISP] = {
		.ch = 'o',
		.col = RGB15(7,31,27),
		.importance = 128,
		.display = NULL,
		.end = mon_WillOWisp_obj_end
	},
	// 2: fire
	[OT_FIRE] = {
		.ch = 'w',
		.col = RGB15(31,12,0),
		.importance = 64,
		.display = NULL,
		.end = obj_fire_obj_end
	},
	// 3: light
	[OT_LIGHT] = {
		.ch = 'o',
		.col = 0,
		.importance = 64,
		.display = obj_light_display,
		.end = obj_light_obj_end
	},
};


void lake(map_t *map, s32 x, s32 y) {
	u32 wisppos = genrand_int32() & 0x3f; // between 0 and 63
	u32 i;
	// scour out a lake by random walk
	for (i = 0; i < 64; i++) {
		cell_t *cell = cell_at(map, x, y);
		if (i == wisppos) // place the wisp
			new_mon_WillOWisp(map, x, y);
		if (!has_objtype(cell, 2)) { // if the cell doesn't have a fire in it
			cell->type = T_WATER;
			cell->ch = '~';
			cell->col = RGB15(6,9,31);
			cell->opaque = false;
		}
		u32 a = genrand_int32();
		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}
		bounded(map, &x, &y);
	}
}

// turn the cell into a ground cell.
void ground(cell_t *cell) {
	unsigned int a = genrand_int32();
	cell->type = T_GROUND;
	unsigned int b = a & 3; // top two bits of a
	a >>= 2; // get rid of the used random bits
	switch (b) {
		case 0:
			cell->ch = '.'; break;
		case 1:
			cell->ch = ','; break;
		case 2:
			cell->ch = '\''; break;
		case 3:
			cell->ch = '`'; break;
	}
	b = a & 3; // next two bits of a (0..3)
	a >>= 2;
	u8 g = a & 7; // three bits (0..7)
	a >>= 3;
	u8 r = a & 7; // (0..7)
	a >>= 3;
	cell->col = RGB15(17+r,9+g,6+b); // more randomness in red/green than in blue
	cell->opaque = false;
}


void random_map(map_t *map) {
	s32 x,y;
	reset_map(map);
	map->objtypes = objects;

	// clear out the map to a backdrop of trees
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
			cell->opaque = true;
			cell->type = T_TREE;
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	// start in the centre
	x = map->w/2; y = map->h/2;

	node_t *something = request_node(map->object_pool);
	object_t *obj = node_data(something);
	obj->type = 0;
	insert_object(map, something, x, y);


	// place some fires
	u32 light1 = genrand_int32() >> 21, // between 0 and 2047
			light2 = light1 + 40;

	u32 lakepos = (genrand_int32()>>21) + 4096; // hopefully away from the fires

	u32 i;
	for (i = 8192; i > 0; i--) { // 8192 steps of the drunkard's walk
		cell_t *cell = cell_at(map, x, y);
		if (i == light1 || i == light2) { // place a fire here
			// fires go on the ground
			if (cell->type != T_GROUND) ground(cell);
			new_obj_fire(map, x, y, 9<<12);
		} else if (i == lakepos) {
			lake(map, x, y);
		} else if (cell->type == T_TREE) { // clear away some tree
			ground(cell);
		}

		u32 a = genrand_int32();
		if (a & 1) { // pick some bits off the number
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}

		// don't run off the edge of the map
		bounded(map, &x, &y);
	}

	// place the stairs at the end
	cell_t *cell = cell_at(map, x, y);
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);

	// and the player at the beginning
	game(map)->pX = map->w/2;
	game(map)->pY = map->h/2;

	// update opacity information
	refresh_blockmap(map);
}

void load_map(map_t *map, size_t len, const char *desc) {
	s32 x,y;
	reset_map(map);
	map->objtypes = objects;

	// clear the map out to ground
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++)
			ground(cell_at(map, x, y));

	// read the map from the string
	for (x = 0, y = 0; len > 0; len--) {
		u8 c = *desc++;
		cell_t *cell = cell_at(map, x, y);
		switch (c) {
			case '*':
				cell->type = T_TREE;
				cell->ch = '*';
				cell->col = RGB15(4,31,1);
				cell->opaque = true;
				break;
			case '@':
				ground(cell);
				game(map)->pX = x;
				game(map)->pY = y;
				break;
			case 'w':
				ground(cell);
				new_obj_fire(map, x, y, 9<<12);
				break;
			case 'o':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 0.39*(1<<12), 0.05*(1<<12), 1.00*(1<<12)));
				break;
			case 'r':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 1.00*(1<<12), 0.07*(1<<12), 0.07*(1<<12)));
				break;
			case 'g':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 1.00*(1<<12), 0.07*(1<<12)));
				break;
			case 'b':
				ground(cell);
				new_obj_light(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 0.07*(1<<12), 1.00*(1<<12)));
				break;
			case '\n': // reached the end of the line, so move down
				x = -1; // gets ++'d later
				y++;
				break;
		}
		x++;
	}

	// update opacity map
	refresh_blockmap(map);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// libfov functions
bool opacity_test(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return true;
	return cell_at(map, x, y)->opaque || (game(map)->pX == x && game(map)->pY == y);
}
bool sight_opaque(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	// stop at the edge of the screen
	if (y < map->scrollY || y >= map->scrollY + 24
	    || x < map->scrollX || x >= map->scrollX + 32)
		return true;
	return cell_at(map, x, y)->opaque;
}

inline int32 calc_semicircle(int32 dist2, int32 rad2) {
	int32 val = (1<<12) - divf32(dist2, rad2);
	if (val < 0) return 0;
	return sqrtf32(val);
}

inline int32 calc_cubic(int32 dist2, int32 rad, int32 rad2) {
	int32 dist = sqrtf32(dist2);
	// 2(d/r)³ - 3(d/r)² + 1
	return mulf32(divf32(2<<12, mulf32(rad,rad2)), mulf32(dist2,dist)) -
		mulf32(divf32(3<<12, rad2), dist2) + (1<<12);
}

inline int32 calc_quadratic(int32 dist2, int32 rad2) {
	return (1<<12) - divf32(dist2,rad2);
}

inline DIRECTION seen_from(map_t *map, DIRECTION d, cell_t *cell) {
	bool opa, opb;
	switch (d) {
		case D_NORTHWEST:
		case D_NORTH_AND_WEST:
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_NORTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_WEST;
			break;
		case D_SOUTHWEST:
		case D_SOUTH_AND_WEST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_SOUTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_WEST;
			break;
		case D_NORTHEAST:
		case D_NORTH_AND_EAST:
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_EAST;
			if (opa && opb) return D_NORTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_EAST;
			break;
		case D_SOUTHEAST:
		case D_SOUTH_AND_EAST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_EAST;
			if (opa && opb) return D_SOUTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_EAST;
			break;
	}
	return d;
}

void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;
	light_t *l = (light_t*)src_;

	// don't bother calculating if we're outside the edge of the screen
	s32 scrollX = map->scrollX, scrollY = map->scrollY;
	if (x < scrollX || y < scrollY || x > scrollX + 31 || y > scrollY + 23) return;

	cell_t *cell = cell_at(map, x, y);

	DIRECTION d = D_NONE;
	if (cell->opaque)
		d = seen_from(map, direction(game(map)->pX, game(map)->pY, x, y), cell);
	cell->seen_from = d;

	if (game(map)->torch_on) {
		// the funny bit-twiddling here is to preserve a few more bits in dx/dy
		// during multiplication. mulf32 is a software multiply, and thus slow.
		int32 dx = (l->x - (x << 12)) >> 2,
		      dy = (l->y - (y << 12)) >> 2,
		      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
		int32 rad = l->radius,
		      rad2 = (rad * rad) >> 12;

		if (dist2 < rad2) {
			div_32_32_raw(dist2<<8, rad2>>4);
			cache_t *cache = cache_at(map, x, y); // load the cache while waiting for the division
			while (DIV_CR & DIV_BUSY);
			int32 intensity = (1<<12) - DIV_RESULT32;

			cell->light = intensity;

			cache->lr = l->r;
			cache->lg = l->g;
			cache->lb = l->b;
		}
	}
	cell->visible = true;
}

void apply_light(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;

	cell_t *cell = cell_at(map, x, y);

	// don't light the cell if we can't see it
	if (!cell->visible) return;

	// XXX: this function is pretty much identical to apply_sight... should
	// maybe merge them.
	light_t *l = (light_t*)src_;
	int32 dx = (l->x - (x << 12)) >> 2, // shifting is for accuracy reasons
	      dy = (l->y - (y << 12)) >> 2,
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius,
	      rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);

		DIRECTION d = D_NONE;
		if (cell->opaque)
			d = seen_from(map, direction(l->x>>12, l->y>>12, x, y), cell);
		cache_t *cache = cache_at(map, x, y);

		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

		if (d & D_BOTH || cell->seen_from & D_BOTH) {
			intensity >>= 1;
			d &= cell->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) cell->light += intensity;
			if (d & D_SOUTH) cell->light += intensity;
			if (d & D_EAST) cell->light += intensity;
			if (d & D_WEST) cell->light += intensity;
		} else if (cell->seen_from == d)
			cell->light += intensity;

		cache->lr += (l->r * intensity) >> 12;
		cache->lg += (l->g * intensity) >> 12;
		cache->lb += (l->b * intensity) >> 12;
	}
}
//---------------------------------------------------------------------------

void process_sight(process_t *process, map_t *map) {
	fov_settings_type *sight = (fov_settings_type*)process->data;
	game(map)->player_light->x = game(map)->pX << 12;
	game(map)->player_light->y = game(map)->pY << 12;
	fov_circle(sight, map, game(map)->player_light, game(map)->pX, game(map)->pY, 32);
}

void end_sight(process_t *process, map_t *map) {
	free(process->data); // the fov_settings_type that was malloc'd
}

void new_sight(map_t *map) {
	fov_settings_type *sight = malloc(sizeof(fov_settings_type));
	fov_settings_init(sight);
	fov_settings_set_shape(sight, FOV_SHAPE_SQUARE);
	fov_settings_set_opacity_test_function(sight, sight_opaque);
	fov_settings_set_apply_lighting_function(sight, apply_sight);

	push_high_process(map, process_sight, end_sight, sight);
}

void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));
	clss(); // TODO: necessary?
	random_map(map);
	reset_cache(map);

	game(map)->torch_on = true;

	new_sight(map);
}

//---------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------
	torch_init();

	map_t *map = create_map(128, 128);
	map->game = malloc(sizeof(game_t));

	// the fov_settings_type that will be used for non-player-held lights.
	fov_settings_type *light = malloc(sizeof(fov_settings_type));
	fov_settings_init(light);
	fov_settings_set_shape(light, FOV_SHAPE_OCTAGON);
	fov_settings_set_opacity_test_function(light, opacity_test);
	fov_settings_set_apply_lighting_function(light, apply_light);
	game(map)->fov_light = light;

	// this is the player's light
	game(map)->player_light = new_light(7<<12, 1.00*(1<<12), 0.90*(1<<12), 0.85*(1<<12));

	u32 x, y;
	u32 frm = 0;

	new_map(map);

	// centre the player on the screen
	map->scrollX = map->w/2 - 16;
	map->scrollY = map->h/2 - 12;

	int dirty = 2; // whole screen dirty first frame

	// profiling stuff (for counting hblanks)
	u32 vc_before;
	u32 counts[10];
	for (vc_before = 0; vc_before < 10; vc_before++) counts[vc_before] = 0;

	// for luminance window stuff
	u32 low_luminance = 0;

	// we need to keep track of where we scroll due to double-buffering.
	DIRECTION just_scrolled = 0;

	while (1) {
		u32 vc_begin = hblnks;
		if (!vblnkDirty)
			swiWaitForVBlank();
		vblnkDirty = 0;

		vc_before = hblnks;

		// TODO: is DMA actually asynchronous?
		bool copying = false;

		if (just_scrolled) {
			// update the new backbuffer
			scroll_screen(map, just_scrolled);
			copying = true;
			just_scrolled = 0;
		}

		// TODO: outsource key handling to game
		scanKeys();
		u32 down = keysDown();
		if (down & KEY_START) {
			new_map(map); // resets cache
			frm = 5;
			map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
			vblnkDirty = 0;
			dirty = 2;
			low_luminance = 0;
			//iprintf("You begin again.\n");
			continue;
		}
		if (down & KEY_SELECT) {
			load_map(map, strlen(test_map), test_map);
			new_sight(map);
			frm = 5;
			map->scrollX = 0; map->scrollY = 0;

			// TODO: split out into engine function(s?) for rescrolling
			if (game(map)->pX - map->scrollX < 8 && map->scrollX > 0)
				map->scrollX = game(map)->pX - 8;
			else if (game(map)->pX - map->scrollX > 24 && map->scrollX < map->w-32)
				map->scrollX = game(map)->pX - 24;
			if (game(map)->pY - map->scrollY < 8 && map->scrollY > 0)
				map->scrollY = game(map)->pY - 8;
			else if (game(map)->pY - map->scrollY > 16 && map->scrollY < map->h-24)
				map->scrollY = game(map)->pY - 16;

			vblnkDirty = 0;
			reset_cache(map); // cache is origin-agnostic
			clss(); // TODO: necessary?
			dirty = 2;
			low_luminance = 0;
			continue;
		}
		if (down & KEY_B)
			game(map)->torch_on = !game(map)->torch_on;

		if (frm == 0) { // we don't check these things every frame; that's way too fast.
			u32 keys = keysHeld();
			if (keys & KEY_A && cell_at(map, game(map)->pX, game(map)->pY)->type == T_STAIRS) {
				//iprintf("You fall down the stairs...\nYou are now on level %d.\n", ++level);
				new_map(map);
				game(map)->pX = map->w/2;
				game(map)->pY = map->h/2;
				map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
				vblnkDirty = 0;
				dirty = 2;
				continue;
			}

			int dpX = 0, dpY = 0;
			if (keys & KEY_RIGHT)
				dpX = 1;
			if (keys & KEY_LEFT)
				dpX = -1;
			if (keys & KEY_DOWN)
				dpY = 1;
			if (keys & KEY_UP)
				dpY = -1;
			s32 pX = game(map)->pX, pY = game(map)->pY;
			if (pX + dpX < 0 || pX + dpX >= map->w) { dpX = 0; }
			if (pY + dpY < 0 || pY + dpY >= map->h) { dpY = 0; }
			cell_t *cell = cell_at(map, pX + dpX, pY + dpY);
			if (cell->opaque) { // XXX: is opacity equivalent to solidity?
				int32 rec = max(cell->recall, (1<<11)); // Eh? What's that?! I felt something!
				if (rec != cell->recall) {
					cell->recall = rec;
					cache_at(map, pX + dpX, pY + dpY)->dirty = 2;
				}
				if (dpX && dpY) {
					if (!cell_at(map, pX + dpX, pY)->opaque) // if we could just go left or right, do that
						dpY = 0;
					else if (!cell_at(map, pX, pY + dpY)->opaque)
						dpX = 0;
					else
						dpX = dpY = 0;
				} else
					dpX = dpY = 0;
			}
			if (dpX || dpY) {
				if (dpX && dpY) frm = 7;
				else frm = 5;
				cell = cell_at(map, pX + dpX, pY + dpY);
				cache_at(map, pX, pY)->dirty = 2; // the cell we just stepped away from
				pX += dpX; game(map)->pX = pX;
				pY += dpY; game(map)->pY = pY;

				s32 dsX = 0, dsY = 0;
				// keep the screen vaguely centred on the player (gap of 8 cells)
				if (pX - map->scrollX < 8 && map->scrollX > 0) { // it's just a scroll to the left
					dsX = (pX - 8) - map->scrollX;
					map->scrollX = pX - 8;
				} else if (pX - map->scrollX > 24 && map->scrollX < map->w-32) {
					dsX = (pX - 24) - map->scrollX;
					map->scrollX = pX - 24;
				}

				if (pY - map->scrollY < 8 && map->scrollY > 0) { 
					dsY = (pY - 8) - map->scrollY;
					map->scrollY = pY - 8;
				} else if (pY - map->scrollY > 16 && map->scrollY < map->h-24) {
					dsY = (pY - 16) - map->scrollY;
					map->scrollY = pY - 16;
				}

				if (dsX || dsY) {
					// XXX: look at generalising scroll function

					//if (abs(dsX) > 1 || abs(dsY) > 1) dirty = 2;
					// the above should never be the case. if it ever is, make sure
					// scrolling doesn't happen *as well as* full-screen dirtying.

					map->cacheX += dsX;
					map->cacheY += dsY;
					// wrap the cache origin
					if (map->cacheX < 0) map->cacheX += 32;
					if (map->cacheY < 0) map->cacheY += 24;
					if (map->cacheX >= 32) map->cacheX -= 32;
					if (map->cacheY >= 24) map->cacheY -= 24;
					DIRECTION dir = direction(dsX, dsY, 0, 0);
					scroll_screen(map, dir); // DMA the map data around so we don't have to redraw
					copying = true;
					just_scrolled = dir;
				}
			}
		}
		if (frm > 0) // await the players command (we check every frame if frm == 0)
			frm--;
		counts[0] += hblnks - vc_before;

		vc_before = hblnks;

		u32 i;

		if (frames % 4 == 0) {
			// have the light lag a bit behind the player

			/*if (frames % 8 == 0) {
				player_light.dx = (genrand_gaussian32()>>21) - (1<<10);
				player_light.dy = (genrand_gaussian32()>>21) - (1<<10);
				player_light.dr = (genrand_gaussian32()>>20) - (1<<11);
			}*/
		}

		counts[1] += hblnks - vc_before;

		vc_before = hblnks;
		// run important processes first
		run_processes(map, &map->high_processes);
		// then everything else
		run_processes(map, &map->processes);
		counts[2] += hblnks - vc_before;

		// wait for DMA to finish
		if (copying)
			while (dmaBusy(3));

		//------------------------------------------------------------------------
		// draw loop

		vc_before = hblnks;
		u32 twiddling = 0;
		u32 drawing = 0;

		// adjust is a war between values above the top of the luminance window and
		// values below the bottom
		s32 adjust = 0;
		u32 max_luminance = 0;

		cell_t *cell = cell_at(map, map->scrollX, map->scrollY);
		for (y = 0; y < 24; y++) {
			for (x = 0; x < 32; x++) {
				cache_t *cache = cache_at_s(map, x, y);

				if (cell->visible && cell->light > 0) {
					start_stopwatch();
					cell->recall = min(1<<12, max(cell->light, cell->recall));
					if (cell->light > max_luminance) max_luminance = cell->light;
					if (cell->light < low_luminance) {
						cell->light = 0;
						adjust--;
					} else if (cell->light > low_luminance + (1<<12)) {
						cell->light = 1<<12;
						adjust++;
					} else
						cell->light -= low_luminance;

					u16 ch = cell->ch;
					u16 col = cell->col;

					// if there are objects in the cell, we want to draw them instead of
					// the terrain.
					if (cell->objects) {
						// we'll only draw the head of the list. since the object list is
						// maintained as sorted, this will be the most recently added most
						// important object in the cell.
						object_t *obj = node_data(cell->objects);
						objecttype_t *objtype = &map->objtypes[obj->type];

						// if the object has a custom display function, we'll ask that.
						if (objtype->display) {
							u32 disp = objtype->display(obj, map);
							// the character should be in the high bytes
							ch = disp >> 16;
							// and the colour in the low bytes
							col = disp & 0xffff;
						} else {
							// otherwise we just use what's default for the object type.
							ch = objtype->ch;
							col = objtype->col;
						}
					}

					int32 rval = cache->lr,
					      gval = cache->lg,
					      bval = cache->lb;
					// if the values have changed significantly from last time (by 7 bits
					// or more, i guess) we'll recalculate the colour. Otherwise, we won't
					// bother.
					if (rval >> 8 != cache->last_lr ||
					    gval >> 8 != cache->last_lg ||
					    bval >> 8 != cache->last_lb ||
					    cache->last_light != cell->light >> 8 ||
					    col != cache->last_col) {
						// fade out to the recalled colour (or 0 for ground)
						int32 minval = 0;
						if (cell->type != T_GROUND) minval = (cell->recall>>2);
						int32 val = max(minval, cell->light);
						int32 maxcol = max(rval,max(bval,gval));
						// scale [rgb]val by the luminance, and keep the ratio between the
						// colours the same
						rval = (div_32_32(rval,maxcol) * val) >> 12;
						gval = (div_32_32(gval,maxcol) * val) >> 12;
						bval = (div_32_32(bval,maxcol) * val) >> 12;
						rval = max(rval, minval);
						gval = max(gval, minval);
						bval = max(bval, minval);
						// eke out the colour values from the 15-bit colour
						u32 r = col & 0x001f,
								g = (col & 0x03e0) >> 5,
								b = (col & 0x7c00) >> 10;
						// multiply the colour through fixed-point 20.12 for a bit more accuracy
						r = ((r<<12) * rval) >> 24;
						g = ((g<<12) * gval) >> 24;
						b = ((b<<12) * bval) >> 24;
						twiddling += read_stopwatch();
						start_stopwatch();
						u16 col_to_draw = RGB15(r,g,b);
						u16 last_col_final = cache->last_col_final;
						if (col_to_draw != last_col_final) {
							drawcq(x*8, y*8, ch, col_to_draw);
							cache->last_col_final = col_to_draw;
							cache->last_lr = cache->lr >> 8;
							cache->last_lg = cache->lg >> 8;
							cache->last_lb = cache->lb >> 8;
							cache->last_light = cell->light >> 8;
							cache->dirty = 2;
						} else if (cache->dirty > 0 || dirty > 0) {
							drawcq(x*8, y*8, ch, col_to_draw);
							if (cache->dirty > 0)
								cache->dirty--;
						}
						drawing += read_stopwatch();
					} else {
						twiddling += read_stopwatch();
						start_stopwatch();
						if (cache->dirty > 0 || dirty > 0) {
							drawcq(x*8, y*8, ch, cache->last_col_final);
							if (cache->dirty > 0)
								cache->dirty--;
						}
						drawing += read_stopwatch();
					}
					cell->light = 0;
					cache->lr = 0;
					cache->lg = 0;
					cache->lb = 0;
					cache->was_visible = true;
				} else if (cache->dirty > 0 || dirty > 0 || cache->was_visible) {
					// dirty or it was visible last frame and now isn't.
					if (cell->recall > 0 && cell->type != T_GROUND) {
						start_stopwatch();
						u32 r = cell->col & 0x001f,
						    g = (cell->col & 0x03e0) >> 5,
						    b = (cell->col & 0x7c00) >> 10;
						int32 val = (cell->recall>>2);
						r = ((r<<12) * val) >> 24;
						g = ((g<<12) * val) >> 24;
						b = ((b<<12) * val) >> 24;
						cache->last_col_final = RGB15(r,g,b);
						cache->last_col = cache->last_col_final; // not affected by light, so they're the same
						cache->last_lr = 0;
						cache->last_lg = 0;
						cache->last_lb = 0;
						cache->last_light = 0;
						twiddling += read_stopwatch();
						start_stopwatch();
						drawcq(x*8, y*8, cell->ch, cache->last_col_final);
						drawing += read_stopwatch();
					} else {
						drawcq(x*8, y*8, ' ', 0); // clear
						cache->last_col_final = 0;
						cache->last_col = 0;
						cache->last_lr = 0;
						cache->last_lg = 0;
						cache->last_lb = 0;
						cache->last_light = 0;
					}
					if (cache->was_visible) {
						cache->was_visible = false;
						cache->dirty = 2;
					}
					if (cache->dirty > 0)
						cache->dirty--;
				}
				cell->visible = 0;

				cell++; // takes into account the size of the structure, apparently
			}
			cell += map->w - 32; // the next cell down
		}

		low_luminance += max(adjust*2, -low_luminance); // adjust to fit at twice the difference
		// drift towards having luminance values on-screen placed at maximum brightness.
		if (low_luminance > 0 && max_luminance < low_luminance + (1<<12))
			low_luminance -= min(40,low_luminance);
		iprintf("\x1b[10;8H      \x1b[10;0Hadjust: %d\nlow luminance: %04x", adjust, low_luminance);
		iprintf("\x1b[13;0Hdrawing: %05x\ntwiddling: %05x", drawing, twiddling);
		//------------------------------------------------------------------------


		drawcq((game(map)->pX-map->scrollX)*8, (game(map)->pY-map->scrollY)*8, '@', RGB15(31,31,31));
		counts[3] += hblnks - vc_before;
		swapbufs();
		if (dirty > 0) {
			dirty--;
			if (dirty > 0)
				cls();
		}
		counts[4] += hblnks - vc_begin;
		frames += 1;
		if (vblnks >= 60) {
			iprintf("\x1b[0;19H%02dfps", (frames * 64 - frames * 4) / vblnks);
			iprintf("\x1b[2;0HKeys:    %05d\nSight:   %05d\nProcess: %05d\nDrawing: %05d\n",
					counts[0], counts[1], counts[2], counts[3]);
			iprintf("         -----\nLeft:    %05d\n",
					counts[4] - counts[0] - counts[1] - counts[2] - counts[3]);
			for (i = 0; i < 10; i++) counts[i] = 0;
			vblnks = frames = 0;
		}
	}

	return 0;
}
