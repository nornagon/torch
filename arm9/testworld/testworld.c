#include "testworld.h"
#include "object.h"
#include "mersenne.h"
#include "util.h"
#include "engine.h"
#include "test_map.h"
#include "generic.h"

#include <nds/arm9/console.h>

#include "sight.h"

#include "randmap.h"
#include "loadmap.h"

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
			move_object(map, process->data, pX + dpX, pY + dpY); // move the player object
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

objecttype_t ot_unknown = {
	.ch = '?',
	.col = RGB15(31,31,31),
	.importance = 3,
	.display = random_colour,
	.end = NULL
};
objecttype_t *OT_UNKNOWN = &ot_unknown;

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
