#include "world.h"
#include "generic.h"
#include "adrift.h"

#include "mapgen.h"

#include "torch.h"

#include <nds.h>

#include "sight.h"
#include "light.h"
#include "mersenne.h"

#include "assert.h"

void process_sight(map_t *map) {
	game(map)->player->light->x = map->pX << 12;
	game(map)->player->light->y = map->pY << 12;
	fov_circle(game(map)->fov_sight, map, game(map)->player->light, map->pX, map->pY, 32);
	cell_t *cell = cell_at(map, map->pX, map->pY);
	cell->light = (1<<12);
	cell->visible = true;
	cache_t *cache = cache_at(map, map->pX, map->pY);
	cache->lr = game(map)->player->light->r;
	cache->lg = game(map)->player->light->g;
	cache->lb = game(map)->player->light->b;
	cell->recall = 1<<12;
}

bool solid(map_t *map, cell_t *cell) {
	if (cell->type == T_TREE) return true;
	return false;
}

void move_player(map_t *map, DIRECTION dir) {
	s32 pX = map->pX, pY = map->pY;

	int dpX = D_DX[dir],
	    dpY = D_DY[dir];

	if (pX + dpX < 0 || pX + dpX >= map->w) { dpX = 0; }
	if (pY + dpY < 0 || pY + dpY >= map->h) { dpY = 0; }

	cell_t *cell = cell_at(map, pX + dpX, pY + dpY);

	if (solid(map, cell)) {
		if (dpX && dpY) {
			// if we could just go left or right, do that. This results in 'sliding'
			// along walls when moving diagonally
			if (!solid(map, cell_at(map, pX + dpX, pY)))
				dpY = 0;
			else if (!solid(map, cell_at(map, pX, pY + dpY)))
				dpX = 0;
			else
				dpX = dpY = 0;
		} else
			dpX = dpY = 0;
	}

	if (dpX || dpY) {
		// moving diagonally takes longer. 5*sqrt(2) ~= 7
		if (dpX && dpY) game(map)->frm = 7;
		else game(map)->frm = 5;

		cell = cell_at(map, pX + dpX, pY + dpY);

		// dirty the cell we just stepped away from
		cache_at(map, pX, pY)->dirty = 2;

		// move the player object
		move_object(map, game(map)->player->obj, pX + dpX, pY + dpY);

		pX += dpX; map->pX = pX;
		pY += dpY; map->pY = pY;

		// dirty the cell we just entered
		cache_at(map, pX, pY)->dirty = 2;

		// TODO: split into a separate (generic?) function
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

void process_keys(map_t *map) {
	if (game(map)->frm == 0) {
		scanKeys();
		u32 keys = keysHeld();

		DIRECTION dir = 0;
		if (keys & KEY_RIGHT)
			dir |= D_EAST;
		if (keys & KEY_LEFT)
			dir |= D_WEST;
		if (keys & KEY_DOWN)
			dir |= D_SOUTH;
		if (keys & KEY_UP)
			dir |= D_NORTH;

		if (!dir) return;

		move_player(map, dir);
	}
	if (game(map)->frm > 0)
		game(map)->frm--;
}

objecttype_t ot_player = {
	.ch = '@',
	.col = RGB15(31,31,31),
	.importance = 255,
	.display = NULL,
	.data = NULL,
	.end = NULL
};
objecttype_t *OT_PLAYER = &ot_player;

node_t *new_obj_player(map_t *map) {
	node_t *node = new_object(map, OT_PLAYER, NULL);
	insert_object(map, node, map->pX, map->pY);
	return node;
}
void new_player(map_t *map) {
	player_t *player = malloc(sizeof(player_t));
	memset(player, 0, sizeof(player_t));

	player->obj = new_obj_player(map);
	player->bag = NULL;
	player->light = new_light(7<<12, 1.00*(1<<12), 0.90*(1<<12), 0.85*(1<<12));

	game(map)->player = player;
}

void handler(map_t *map) {
	process_keys(map);
	process_sight(map);
}

void new_game() {
	map_t *map = create_map(128,128);
	map->game = malloc(sizeof(game_t));
	memset(map->game, 0, sizeof(game_t));

	// the fov_settings_type that will be used for non-player-held lights.
	game(map)->fov_light = build_fov_settings(opacity_test, apply_light, FOV_SHAPE_OCTAGON);
	game(map)->fov_sight = build_fov_settings(sight_opaque, apply_sight, FOV_SHAPE_SQUARE);

	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));

	generate_terrarium(map);
	reset_cache(map);

	new_player(map);

	map->scrollX = map->pX - 16;
	map->scrollY = map->pY - 12;
	bounded(map, &map->scrollX, &map->scrollY);

	map->handler = handler;

	run(map);
}

void init_world() {
	lcdMainOnBottom();

	new_game();
}
