#include "globe.h"
#include "generic.h"
#include "testworld.h"

objecttype_t ot_globe = {
	.ch = 'o',
	.col = 0,
	.importance = 64,
	.display = obj_globe_display,
	.end = obj_globe_obj_end
};
objecttype_t *OT_GLOBE = &ot_globe;

void new_obj_globe(map_t *map, s32 x, s32 y, light_t *light) {
	obj_globe_t *globe = malloc(sizeof(obj_globe_t));
	globe->light = light;
	light->x = x<<12; light->y = y<<12;
	// we cache the display characteristics of the light for performance reasons.
	globe->display = ('o'<<16) |
		RGB15((light->r * (31<<12))>>24,
		      (light->g * (31<<12))>>24,
		      (light->b * (31<<12))>>24);
	// create the lighting process and store the node for cleanup purposes
	globe->light_node = push_process(map,
			obj_globe_process, obj_globe_proc_end, globe);
	// create the worldly presence of the light
	globe->obj_node = new_object(map, OT_GLOBE, globe);
	// and add it to the map
	insert_object(map, globe->obj_node, x, y);
}

// boilerplate entity-freeing functions. TODO: how to make this easier?
void obj_globe_proc_end(process_t *proc, map_t *map) {
	obj_globe_t *globe = (obj_globe_t*)proc->data;
	free_object(map, globe->obj_node);
	free(globe);
}

void obj_globe_obj_end(object_t *obj, map_t *map) {
	obj_globe_t *globe = (obj_globe_t*)obj->data;
	free_process(map, globe->light_node);
	free(globe);
}

void obj_globe_process(process_t *proc, map_t *map) {
	obj_globe_t *globe = (obj_globe_t*)proc->data;
	draw_light(map, game(map)->fov_light, globe->light);
}

u32 obj_globe_display(object_t *obj, map_t *map) {
	obj_globe_t *globe = (obj_globe_t*)obj->data;
	return globe->display;
}
