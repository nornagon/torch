#include "willowisp.h"
#include "testworld.h"

#include "mersenne.h"
#include "generic.h"

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
	set_light(&wisp->light, 4<<12, 0.23*(1<<12), 0.87*(1<<12), 1.00*(1<<12));
	wisp->light.x = x << 12;
	wisp->light.y = y << 12;

	// the object to represent the wisp on the map
  wisp->obj_node = new_object(map, OT_WISP, wisp);
  insert_object(map, wisp->obj_node, x, y);
}

// these ending functions are complicated because we want to free all the bits
// of the wisp any time a single one of them is freed.
void mon_WillOWisp_obj_end(object_t *object, map_t *map) {
	mon_WillOWisp_t *wisp = object->data;
	free_processes(map, &wisp->light_node, 2); // it's not really an array, but it works like one.
	// free the state data
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
	free(wisp);
}

void mon_WillOWisp_light(process_t *process, map_t *map) {
	mon_WillOWisp_t *wisp = process->data;
	draw_light(map, game(map)->fov_light, &wisp->light);
}

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
		default:
			if (a&1) {
				if (a&2) { *dX = 0; *dY = 1; }
				else     { *dX = 0; *dY = -1; }
			} else {
				if (a&2) { *dX = 1; *dY = 0; }
				else     { *dX = -1; *dY = 0; }
			}
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

		wisp->light.x = obj->x << 12;
		wisp->light.y = obj->y << 12;
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
		unsigned int mdist = manhdist(obj->x, obj->y, map->pX, map->pY);
		if (mdist < 4) { // don't get too close
			DIRECTION dir = direction(map->pX, map->pY, obj->x, obj->y);
			// the player is in the direction dir (direction from obj to player)
			int dX = 0, dY = 0;
			mon_WillOWisp_randdir(dir, &dX, &dY);
			// randdir returns a position delta *towards* the player, so invert it
			dX = -dX;
			dY = -dY;
			displace_object(wisp->obj_node, map, dX, dY);
			wisp->counter = 10;
		} else if (mdist > 5) { // don't get too far away either
			DIRECTION dir = direction(map->pX, map->pY, obj->x, obj->y);
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
		wisp->light.x = obj->x << 12;
		wisp->light.y = obj->y << 12;
	} else
		wisp->counter--;
}
