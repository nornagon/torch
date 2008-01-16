#include "world.h"
#include "object.h"
#include "mersenne.h"
#include "util.h"
#include "engine.h"
#include "test_map.h"
#include "generic.h"
#include "text.h"

#include "creature.h"

#include <nds/arm9/console.h>
#include <stdio.h>

#include "sight.h"

#include "randmap.h"
#include "loadmap.h"

bool does_obstruct(object_t *obj, map_t *map) {
	return true;
}

void process_sight(process_t *process, map_t *map) {
	fov_settings_type *sight = (fov_settings_type*)process->data;
	game(map)->player->light->x = map->pX << 12;
	game(map)->player->light->y = map->pY << 12;
	fov_circle(sight, map, game(map)->player->light, map->pX, map->pY, 32);
	cell_t *cell = cell_at(map, map->pX, map->pY);
	cell->light = (1<<12);
	cell->visible = true;
	cache_t *cache = cache_at(map, map->pX, map->pY);
	cache->lr = game(map)->player->light->r;
	cache->lg = game(map)->player->light->g;
	cache->lb = game(map)->player->light->b;
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

void manage_inventory(map_t *map) {
	// XXX: hax here, non-portable, etc.
	lcdMainOnTop();
	u16* vram = (u16*)BG_BMP_RAM_SUB(0);
	u32 sel = 0;
	u32 begin = 0;
	char buf[128];
	u32 invsz = listlen(game(map)->player->bag);
	while (1) {
		text_display_clear();
		u32 i;
		for (i = 3; i < 252; i++) {
			vram[i+3*256] = 0xffff;
			vram[i+(192-5)*256] = 0xffff;
		}
		for (i = 4; i < 192-5; i++) {
			vram[i*256+3] = 0xffff;
			vram[i*256+256-5] = 0xffff;
		}
		text_render_raw(6, 0, "Inventory", 9, 0xffff);
		node_t *inv = game(map)->player->bag;
		node_t *k = inv;
		for (i = 0; k && i < begin + 19; i++, k = k->next) {
			if (i < begin) continue;
			u32 j = i - begin;
			object_t *obj = node_data(k);
			int len = 0;
			if (obj->quantity == 1) {
				len = sniprintf(buf, 128, "a %s", gobjt(obj)->singular);
			} else {
				if (gobjt(obj)->plural)
					len = sniprintf(buf, 128, "%d %s", obj->quantity, gobjt(obj)->plural);
				else
					len = sniprintf(buf, 128, "%d %ss", obj->quantity, gobjt(obj)->singular);
			}
			if (i == sel)
				text_render_raw(7, 10+j*8+j+2, "*", 1, 0xffff);
			text_render_raw(7+widthof('*')+3, 10+j*8+j+2, buf, len, i == sel ? 0xaaaa:0xffff);
		}
		bool changed = false;
		while (!changed) {
			swiWaitForVBlank();
			scanKeys();
			u32 down = keysDown();
			if (down & KEY_DOWN && sel < invsz-1) {
				sel++;
				if (sel > begin + 15 && begin < invsz - 19) begin++;
				changed = true;
			}
			if (down & KEY_UP && sel > 0) {
				sel--;
				if (sel < begin + 3 && begin > 0) begin--;
				changed = true;
			}
			if (down & KEY_R) {
				sel += min(10, invsz-1 - sel);
				if (invsz > 19)
					if (sel > begin + 15) begin = min(invsz - 19, sel - 15);
				changed = true;
			}
			if (down & KEY_L) {
				sel -= min(sel, 10);
				if (invsz > 19)
					if (sel < begin + 3) begin = sel - min(sel, 3);
				changed = true;
			}
			if (down & KEY_B)
				goto done;
		}
		continue;
done:
		break;
	}
	lcdMainOnBottom();
	text_console_rerender();
}

void get_item(map_t *map) {
	cell_t *cell = cell_at(map, map->pX, map->pY);
	if (cell->objects->next) { // skip over the player object
		node_t *node = cell->objects->next;
		while (node) {
			object_t *obj = node_data(node);
			node_t *next = node->next;
			if (gobjt(obj)->obtainable) {
				// TODO: hrmph. unwieldy, this is.
				if (obj->quantity == 1) {
					iprintf("You pick up a %s.\n", gobjt(obj)->singular);
				} else {
					if (gobjt(obj)->plural)
						iprintf("You pick up %d %s.\n", obj->quantity, gobjt(obj)->plural);
					else
						iprintf("You pick up %d %ss.\n", obj->quantity, gobjt(obj)->singular);
				}
				game(map)->frm += 5;
				cell->objects = remove_node(cell->objects, node);
				node_t *k = game(map)->player->bag;
				if (gobjt(obj)->combinable) {
					for (; k; k = k->next) {
						object_t *other = node_data(k);
						if (gobjt(obj)->combinable(obj, other)) {
							int q = (u32)obj->quantity + (u32)other->quantity;
							if (q > 0xff) {
								obj->quantity -= 0xff - other->quantity;
								other->quantity = 0xff;
								push_node(&game(map)->player->bag, node);
							} else {
								other->quantity = q;
								free_object(map, node);
							}
							break;
						}
					}
					if (!k) push_node(&game(map)->player->bag, node);
				} else
					push_node(&game(map)->player->bag, node);
			}
			node = next;
		}
	}
}

bool solid(map_t *map, cell_t *cell) {
	if (cell->type == T_TREE) return true;
	return false;
}

// TODO: This is ugly and could get very expensive very fast. Should cache the
// result.
bool obstructs(map_t *map, cell_t *cell) {
	if (solid(map, cell)) return true;
	node_t *k = cell->objects;
	for (; k; k = k->next) {
		object_t *obj = node_data(k);
		gameobjtype_t *gt = gobjt(obj);
		if (gt->obstructs && gt->obstructs(obj, map))
			return true;
	}
	return false;
}

// XXX: hax. This assumes that creatures will always be on the top of the object
// list.
node_t *creature(map_t *map, cell_t *cell) {
	if (gobjt(node_data(cell->objects))->creature)
		return cell->objects;
	return NULL;
}

void move_player(map_t *map, DIRECTION dir) {
	s32 pX = map->pX, pY = map->pY;

	int dpX = D_DX[dir],
	    dpY = D_DY[dir];

	if (pX + dpX < 0 || pX + dpX >= map->w) { dpX = 0; }
	if (pY + dpY < 0 || pY + dpY >= map->h) { dpY = 0; }

	cell_t *cell = cell_at(map, pX + dpX, pY + dpY);

	node_t *cr_node = creature(map, cell);

	if (cr_node) {
		object_t *cr_obj = node_data(cr_node);
		// attack the creature in the target cell
		creature_t *cr = cr_obj->data;
		int power = 13;
		if (rand0(power) >= cr->ac) {
			int dam = rand0(10)+4;
			// triggers
			cr->hp -= dam;
			iprintf("You hit the %s. ", gobjt(cr_obj)->singular);
			if (cr->hp < 0) {
				iprintf("The %s dies. ", gobjt(cr_obj)->singular);
				free_object(map, cr_node);
			}
			iprintf("\n");
		} else {
			iprintf("You miss.\n");
		}
		game(map)->frm += 7;
		return;
	}
	else if (obstructs(map, cell)) {
		if (dpX && dpY) {
			// if we could just go left or right, do that. This results in 'sliding'
			// along walls when moving diagonally
			if (!obstructs(map, cell_at(map, pX + dpX, pY)))
				dpY = 0;
			else if (!obstructs(map, cell_at(map, pX, pY + dpY)))
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

		// trigger any objects in the cell.
		node_t *node = cell->objects;
		for (; node; node = node->next) {
			if (node == game(map)->player->obj) continue;
			object_t *obj = node_data(node);
			if (gobjt(obj)->obtainable) {
				if (obj->quantity == 1) {
					iprintf("You see here a %s.\n", gobjt(obj)->singular);
				} else {
					if (gobjt(obj)->plural)
						iprintf("You see here %d %s.\n", obj->quantity, gobjt(obj)->plural);
					else
						iprintf("You see here %d %ss.\n", obj->quantity, gobjt(obj)->singular);
				}
			}
			if (gobjt(obj)->entered)
				gobjt(obj)->entered(obj, node_data(game(map)->player->obj), map);
		}


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

node_t *new_obj_player(map_t *map);
void new_map(map_t *map);

void process_keys(process_t *process, map_t *map) {
	player_t *player = process->data;
	scanKeys();
	u32 down = keysDown();
	if (down & KEY_START) {
		// creating a new map will remove this process from the list and free its
		// parent node (calling any ending handlers), so be sure that there are no
		// mixups.
		new_map(map);
		player->obj = new_obj_player(map);
		game(map)->frm = 5;
		map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
		dirty_screen();
		reset_luminance();
		iprintf("You begin again.\n");
		return;
	}
	if (down & KEY_SELECT) {
		load_map(map, strlen(test_map), test_map);
		player->obj = new_obj_player(map);
		reset_cache(map); // cache is origin-agnostic
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

	if (down & KEY_X)
		manage_inventory(map);
	if (down & KEY_A)
		get_item(map);

	if (game(map)->frm == 0) { // we don't check these things every frame; that's way too fast.
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
	if (game(map)->frm > 0) // await the player's command (we check keys every frame if frm == 0)
		game(map)->frm--;
}

gameobjtype_t go_player = {
	.obtainable = false,
};
objecttype_t ot_player = {
	.ch = '@',
	.col = RGB15(31,31,31),
	.importance = 255,
	.display = NULL,
	.data = &go_player,
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
	push_high_process(map, process_keys, NULL, player);

	player->obj = new_obj_player(map);
	player->bag = NULL;
	player->light = new_light(7<<12, 1.00*(1<<12), 0.90*(1<<12), 0.85*(1<<12));

	game(map)->player = player;
}

void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds +
				IPC->time.rtc.minutes*60 + IPC->time.rtc.hours*60*60 +
				IPC->time.rtc.weekday*7*24*60*60));
	random_map(map);
	reset_cache(map);
}


u32 random_colour(object_t *obj, map_t *map) {
	return ((obj->type->ch)<<16) | (genrand_int32()&0xffff);
}

gameobjtype_t go_unknown = {
	.obtainable = false,
	.creature = false,
	.singular = "???",
};
objecttype_t ot_unknown = {
	.ch = '?',
	.col = RGB15(31,31,31),
	.importance = 3,
	.display = random_colour,
	.data = &go_unknown,
	.end = NULL
};
objecttype_t *OT_UNKNOWN = &ot_unknown;




map_t *init_world() {
	lcdMainOnBottom();

	map_t *map = create_map(128, 128);
	map->game = malloc(sizeof(game_t));

	// the fov_settings_type that will be used for non-player-held lights.
	fov_settings_type *light = malloc(sizeof(fov_settings_type));
	fov_settings_init(light);
	fov_settings_set_shape(light, FOV_SHAPE_OCTAGON);
	fov_settings_set_opacity_test_function(light, opacity_test);
	fov_settings_set_apply_lighting_function(light, apply_light);
	game(map)->fov_light = light;

	new_map(map);
	new_sight(map);
	new_player(map);

	// centre the player on the screen
	map->scrollX = map->w/2 - 16;
	map->scrollY = map->h/2 - 12;

	return map;
}
