#include "testworld.h"
#include "object.h"
#include "mersenne.h"
#include "util.h"
#include "engine.h"
#include "test_map.h"
#include "generic.h"

void load_map(map_t *map, size_t len, const char *desc);
void random_map(map_t *map);

//----------{{{ libfov functions---------------------------------------------
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
//------------------}}}------------------------------------------------------

//---------{{{ game processes------------------------------------------------
void process_sight(process_t *process, map_t *map) {
	fov_settings_type *sight = (fov_settings_type*)process->data;
	game(map)->player_light->x = game(map)->pX << 12;
	game(map)->player_light->y = game(map)->pY << 12;
	fov_circle(sight, map, game(map)->player_light, game(map)->pX, game(map)->pY, 32);
	cell_t *cell = cell_at(map, game(map)->pX, game(map)->pY);
	cell->light = (1<<12);
	cell->visible = true;
	cache_t *cache = cache_at(map, game(map)->pX, game(map)->pY);
	cache->lr = game(map)->player_light->r;
	cache->lg = game(map)->player_light->g;
	cache->lb = game(map)->player_light->b;
	cell->recall = 1<<12;
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

node_t *new_obj_player(map_t *map);
void new_map(map_t *map);

void process_keys(process_t *process, map_t *map) {
	scanKeys();
	u32 down = keysDown();
	if (down & KEY_START) {
		// creating a new map will remove this process from the list and free its
		// parent node (calling any ending handlers), so be sure that there are no
		// mixups.
		new_map(map);
		process->data = new_obj_player(map);
		game(map)->frm = 5;
		map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
		dirty_screen();
		reset_luminance();
		//iprintf("You begin again.\n");
		return;
	}
	if (down & KEY_SELECT) {
		load_map(map, strlen(test_map), test_map);
		reset_cache(map); // cache is origin-agnostic
		process->data = new_obj_player(map);
		game(map)->frm = 5;
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

		dirty_screen();
		reset_luminance();
		return;
	}
	if (down & KEY_B)
		game(map)->torch_on = !game(map)->torch_on;

	if (game(map)->frm == 0) { // we don't check these things every frame; that's way too fast.
		u32 keys = keysHeld();
		if (keys & KEY_A && cell_at(map, game(map)->pX, game(map)->pY)->type == T_STAIRS) {
			//iprintf("You fall down the stairs...\nYou are now on level %d.\n", ++level);
			new_map(map);
			process->data = new_obj_player(map);
			game(map)->pX = map->w/2;
			game(map)->pY = map->h/2;
			map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
			dirty_screen();
			return;
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
		if (!dpX && !dpY) return;
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
			if (dpX && dpY) game(map)->frm = 7; // moving diagonally takes longer
			else game(map)->frm = 5;
			cell = cell_at(map, pX + dpX, pY + dpY);
			cache_at(map, pX, pY)->dirty = 2; // the cell we just stepped away from
			move_object(map, cell_at(map, pX, pY), process->data, pX + dpX, pY + dpY); // move the player object
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

			if (dsX || dsY)
				scroll_screen(map, dsX, dsY);
		}
	}
	if (game(map)->frm > 0) // await the players command (we check every frame if frm == 0)
		game(map)->frm--;
}

objecttype_t ot_player = {
	.ch = '@',
	.col = RGB15(31,31,31),
	.importance = 254,
	.display = NULL,
	.end = NULL
};
objecttype_t *OT_PLAYER = &ot_player;

node_t *new_obj_player(map_t *map) {
	node_t *node = new_object(map, OT_PLAYER, NULL);
	insert_object(map, node, game(map)->pX, game(map)->pY);
	return node;
}
void new_player(map_t *map) {
	node_t *proc_node = push_high_process(map, process_keys, NULL, NULL);
	process_t *proc = node_data(proc_node);
	proc->data = new_obj_player(map);
}

void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));
	random_map(map);
	reset_cache(map);

	game(map)->torch_on = true;
}
//---------}}}---------------------------------------------------------------

// {{{ ot_unknown
u32 random_colour(object_t *obj, map_t *map) {
	return ((obj->type->ch)<<16) | (genrand_int32()&0xffff);
}
objecttype_t ot_unknown = {
	.ch = '?',
	.col = RGB15(31,31,31),
	.importance = 3,
	.display = random_colour,
	.end = NULL
};
objecttype_t *OT_UNKNOWN = &ot_unknown;
// }}}

// {{{ lighting

// {{{ fire
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

	draw_light(map, game(map)->fov_light, l);
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

objecttype_t ot_fire = {
	.ch = 'w',
	.col = RGB15(31,12,0),
	.importance = 64,
	.display = NULL,
	.end = obj_fire_obj_end
};
objecttype_t *OT_FIRE = &ot_fire;

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
// }}}

// {{{  a glowing, coloured light
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
	draw_light(map, game(map)->fov_light, obj_light->light);
}

u32 obj_light_display(object_t *obj, map_t *map) {
	obj_light_t *obj_light = (obj_light_t*)obj->data;
	return obj_light->display;
}

objecttype_t ot_light = {
	.ch = 'o',
	.col = 0,
	.importance = 64,
	.display = obj_light_display,
	.end = obj_light_obj_end
};
objecttype_t *OT_LIGHT = &ot_light;

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
// }}}

// }}}

// {{{ Will-O'-Wisp

// {{{ Will-O'-Wisp state structure
typedef struct {
	// light_node and thought_node are next to each other so we can free them
	// later if we need to using free_processes or free_other_processes (which
	// take an *array* of processes)
	node_t *light_node;
	node_t *thought_node;
	// we need to keep track of the object in case we need to free it (from the
	// end process handler)
	node_t *obj_node;

	// the wisp will emit a glow
	light_t *light;

	// the wisp will try to stay close to its home
	s32 homeX, homeY;

	// the wisp shouldn't move too fast, so we keep a counter.
	// TODO: maybe allow processes to be called at longer intervals?
	u32 counter;
} mon_WillOWisp_t;
// }}}

void mon_WillOWisp_light(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	draw_light(map, game(map)->fov_light, wisp->light);
}

void mon_WillOWisp_wander(process_t *process, map_t *map);
void mon_WillOWisp_follow(process_t *process, map_t *map);

// {{{ thought processes
// return (dX, dY) as a position delta in the direction of dir. Will wander
// around a bit, not just straight in that direction.
void mon_WillOWisp_randdir(DIRECTION dir, int *dX, int *dY) {
	u32 a = genrand_int32();
	switch (dir) {
		case D_NORTH: // the player is to the north
			*dY = -1;
			if (a&1) *dX = -1;
			else if (a&2) *dX = 1;
			break;
		case D_SOUTH:
			if (a&1) *dX = -1;
			else if (a&2) *dX = 1;
			*dY = 1; break;
		case D_WEST:
			if (a&1) *dY = -1;
			else if (a&2) *dY = 1;
			*dX = -1; break;
		case D_EAST:
			if (a&1)      *dY = -1;
			else if (a&2) *dY = 1;
			*dX = 1; break;
		case D_NORTHEAST:
			if (a&1)      *dX = 1;
			else if (a&2) *dY = -1;
			else          { *dX = 1; *dY = -1; }
			break;
		case D_NORTHWEST:
			if (a&1)      *dX = -1;
			else if (a&2) *dY = -1;
			else          { *dX = -1; *dY = -1; }
			break;
		case D_SOUTHEAST:
			if (a&1)      *dX = 1;
			else if (a&2) *dY = 1;
			else          { *dX = 1; *dY = 1; }
			break;
		case D_SOUTHWEST:
			if (a&1)      *dX = -1;
			else if (a&2) *dY = 1;
			else          { *dX = -1; *dY = 1; }
			break;
	}
}

void mon_WillOWisp_wander(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	object_t *obj = node_data(wisp->obj_node);
	cell_t *cell = cell_at(map, obj->x, obj->y);
	if (cell->visible) { // if they can see us, we can see them...
		process->process = mon_WillOWisp_follow;
		wisp->counter = 0;
	} else if (wisp->counter == 0) {
		int dir = genrand_int32() % 4;
		int dX = 0, dY = 0;
		switch (dir) {
			case 0: dX = 1; dY = 0; break;
			case 1: dX = -1; dY = 0; break;
			case 2: dX = 0; dY = 1; break;
			case 3: dX = 0; dY = -1; break;
		}

		object_t *obj = node_data(wisp->obj_node);

		// try to stay close to the home
		if (dX < 0 && obj->x + dX < wisp->homeX - 20) // too far west
			dX = 1;
		else if (dX > 0 && obj->x + dX > wisp->homeX + 20) // too far east
			dX = -1;
		else if (dY < 0 && obj->y + dY < wisp->homeY - 20) // too far north
			dY = 1;
		else if (dY > 0 && obj->y + dY > wisp->homeY + 20) // too far south
			dY = -1;

		displace_object(wisp->obj_node, map, dX, dY);

		wisp->light->x = obj->x << 12;
		wisp->light->y = obj->y << 12;
		wisp->counter = 40;
	} else
		wisp->counter--;
}

void mon_WillOWisp_follow(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	object_t *obj = node_data(wisp->obj_node);
	cell_t *cell = cell_at(map, obj->x, obj->y);
	if (!cell->visible) {
		// if we can't see the player, go back to wandering
		process->process = mon_WillOWisp_wander;
	} else if (wisp->counter == 0) { // time to do something
		unsigned int mdist = manhdist(obj->x, obj->y, game(map)->pX, game(map)->pY);
		if (mdist < 4) { // don't get too close
			DIRECTION dir = direction(game(map)->pX, game(map)->pY, obj->x, obj->y);
			// the player is in the direction dir (direction from obj to player)
			int dX = 0, dY = 0;
			mon_WillOWisp_randdir(dir, &dX, &dY);
			// randdir returns a position delta *towards* the player, so invert it
			dX = -dX;
			dY = -dY;
			displace_object(wisp->obj_node, map, dX, dY);
			wisp->counter = 10;
		} else if (mdist > 5) { // don't get too far away either
			DIRECTION dir = direction(game(map)->pX, game(map)->pY, obj->x, obj->y);
			int dX = 0, dY = 0;
			mon_WillOWisp_randdir(dir, &dX, &dY);
			displace_object(wisp->obj_node, map, dX, dY);
			wisp->counter = 10;
		} else { // meander around
			u32 a = genrand_int32();
			int dX = 0, dY = 0;
			// move randomly in one of four directions.
			switch (a&3) {
				case 0: dX = 1; dY = 0; break;
				case 1: dX = -1; dY = 0; break;
				case 2: dX = 0; dY = 1; break;
				case 3: dX = 0; dY = -1; break;
			}
			displace_object(wisp->obj_node, map, dX, dY);
			wisp->counter = 20;
		}
		wisp->light->x = obj->x << 12;
		wisp->light->y = obj->y << 12;
	} else
		wisp->counter--;
}
// }}}

// {{{ housekeeping
// these ending functions are complicated because we want to free all the bits
// of the wisp any time a single one of them is freed.
void mon_WillOWisp_obj_end(object_t *object, map_t *map) {
	mon_WillOWisp_t *wisp = object->data;
	free_processes(map, &wisp->light_node, 2);
	// free the state data
	free(wisp->light);
	free(wisp);
}

void mon_WillOWisp_proc_end(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	// the below use of wisp->light_node as an array is sort of hackish, but it
	// works. free_other_processes will free whichever process isn't this one.
	free_other_processes(map, process, &wisp->light_node, 2);
	free_object(map, wisp->obj_node);
	// all of those things above refer to the same data (the wisp state), so we
	// just free the state.
	free(wisp->light);
	free(wisp);
}
// }}}

objecttype_t ot_wisp = {
	.ch = 'o',
	.col = RGB15(7,31,27),
	.importance = 128,
	.display = NULL,
	.end = mon_WillOWisp_obj_end
};
objecttype_t *OT_WISP = &ot_wisp;

void new_mon_WillOWisp(map_t *map, s32 x, s32 y) {
	mon_WillOWisp_t *wisp = malloc(sizeof(mon_WillOWisp_t));

	wisp->homeX = x;
	wisp->homeY = y;
	wisp->counter = 0;

	// set up lighting and AI processes
	wisp->light_node = push_process(map,
			mon_WillOWisp_light, mon_WillOWisp_proc_end, wisp);
	wisp->thought_node = push_process(map,
			mon_WillOWisp_wander, mon_WillOWisp_proc_end, wisp);

	// a light cyan glow
	light_t *light = new_light(4<<12, 0.23*(1<<12), 0.87*(1<<12), 1.00*(1<<12));
	light->x = x << 12;
	light->y = y << 12;

	wisp->light = light;

	// the object to represent the wisp on the map
  wisp->obj_node = new_object(map, OT_WISP, wisp);
  insert_object(map, wisp->obj_node, x, y);
}
// }}}

// {{{ map generation
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

	node_t *something = new_object(map, OT_UNKNOWN, NULL);
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
// }}}

map_t *init_test() {
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

	new_map(map);
	new_player(map);
	new_sight(map);

	// centre the player on the screen
	map->scrollX = map->w/2 - 16;
	map->scrollY = map->h/2 - 12;

	return map;
}
