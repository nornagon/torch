#include "fire.h"
#include "mersenne.h"
#include "generic.h"
#include "world.h"

gameobjtype_t go_fire = {
	.obtainable = false,
	.singular = "fire",
};

objecttype_t ot_fire = {
	.ch = 'w',
	.col = RGB15(31,12,0),
	.importance = 64,
	.display = NULL,
	.data = &go_fire,
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
