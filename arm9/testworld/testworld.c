#include "testworld.h"
#include "object.h"
#include "mersenne.h"
#include "util.h"
#include "engine.h"
#include "test_map.h"
#include "generic.h"

#include <nds/arm9/console.h>

#include "willowisp.h"
#include "fire.h"
#include "globe.h"
#include "sight.h"

void load_map(map_t *map, size_t len, const char *desc);
void random_map(map_t *map);

//---------{{{ game processes------------------------------------------------
void process_sight(process_t *process, map_t *map) {
	fov_settings_type *sight = (fov_settings_type*)process->data;
	game(map)->player_light->x = map->pX << 12;
	game(map)->player_light->y = map->pY << 12;
	fov_circle(sight, map, game(map)->player_light, map->pX, map->pY, 32);
	cell_t *cell = cell_at(map, map->pX, map->pY);
	cell->light = (1<<12);
	cell->visible = true;
	cache_t *cache = cache_at(map, map->pX, map->pY);
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
		iprintf("You begin again.\n");
		return;
	}
	if (down & KEY_SELECT) {
		load_map(map, strlen(test_map), test_map);
		reset_cache(map); // cache is origin-agnostic
		process->data = new_obj_player(map);
		game(map)->frm = 5;
		map->scrollX = 0; map->scrollY = 0;

		// TODO: split out into engine function(s?) for rescrolling
		if (map->pX - map->scrollX < 8 && map->scrollX > 0)
			map->scrollX = map->pX - 8;
		else if (map->pX - map->scrollX > 24 && map->scrollX < map->w-32)
			map->scrollX = map->pX - 24;
		if (map->pY - map->scrollY < 8 && map->scrollY > 0)
			map->scrollY = map->pY - 8;
		else if (map->pY - map->scrollY > 16 && map->scrollY < map->h-24)
			map->scrollY = map->pY - 16;

		dirty_screen();
		reset_luminance();
		return;
	}
	if (down & KEY_B)
		game(map)->torch_on = !game(map)->torch_on;

	if (game(map)->frm == 0) { // we don't check these things every frame; that's way too fast.
		u32 keys = keysHeld();
		if (keys & KEY_A && cell_at(map, map->pX, map->pY)->type == T_STAIRS) {
			//iprintf("You fall down the stairs...\nYou are now on level %d.\n", ++level);
			new_map(map);
			process->data = new_obj_player(map);
			map->pX = map->w/2;
			map->pY = map->h/2;
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
		s32 pX = map->pX, pY = map->pY;
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
			pX += dpX; map->pX = pX;
			pY += dpY; map->pY = pY;

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
	if (game(map)->frm > 0) // await the player's command (we check keys every frame if frm == 0)
		game(map)->frm--;
}

static objecttype_t ot_player = {
	.ch = '@',
	.col = RGB15(31,31,31),
	.importance = 254,
	.display = NULL,
	.end = NULL
};
static objecttype_t *OT_PLAYER = &ot_player;

node_t *new_obj_player(map_t *map) {
	node_t *node = new_object(map, OT_PLAYER, NULL);
	insert_object(map, node, map->pX, map->pY);
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

u32 random_colour(object_t *obj, map_t *map) {
	return ((obj->type->ch)<<16) | (genrand_int32()&0xffff);
}
static objecttype_t ot_unknown = {
	.ch = '?',
	.col = RGB15(31,31,31),
	.importance = 3,
	.display = random_colour,
	.end = NULL
};
static objecttype_t *OT_UNKNOWN = &ot_unknown;

// {{{ map generation
void lake(map_t *map, s32 x, s32 y) {
	u32 wisppos = genrand_int32() & 0x3f; // between 0 and 63
	u32 i;
	// scour out a lake by random walk
	for (i = 0; i < 64; i++) {
		cell_t *cell = cell_at(map, x, y);
		if (i == wisppos) // place the wisp
			new_mon_WillOWisp(map, x, y);
		if (!has_objtype(cell, OT_FIRE)) { // if the cell doesn't have a fire in it
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
	cell->forgettable = true;
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
	map->pX = map->w/2;
	map->pY = map->h/2;

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
				map->pX = x;
				map->pY = y;
				break;
			case 'w':
				ground(cell);
				new_obj_fire(map, x, y, 9<<12);
				break;
			case 'o':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 0.39*(1<<12), 0.05*(1<<12), 1.00*(1<<12)));
				break;
			case 'r':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 1.00*(1<<12), 0.07*(1<<12), 0.07*(1<<12)));
				break;
			case 'g':
				ground(cell);
				new_obj_globe(map, x, y,
						new_light(8<<12, 0.07*(1<<12), 1.00*(1<<12), 0.07*(1<<12)));
				break;
			case 'b':
				ground(cell);
				new_obj_globe(map, x, y,
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
	new_sight(map);
	new_player(map);

	// centre the player on the screen
	map->scrollX = map->w/2 - 16;
	map->scrollY = map->h/2 - 12;

	return map;
}
