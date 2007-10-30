#ifndef GLOBE_H
#define GLOBE_H 1

#include "torch.h"
#include "light.h"

typedef struct {
	node_t *light_node; // the light-drawing process node
	node_t *obj_node; // the presence object node

	light_t *light;

	// cache the display colour/character so we don't have to recalculate every
	// frame.
	u32 display;
} obj_globe_t;

void new_obj_globe(map_t *map, s32 x, s32 y, light_t *light);
void obj_globe_proc_end(process_t *proc, map_t *map);
void obj_globe_obj_end(object_t *obj, map_t *map);

void obj_globe_process(process_t *proc, map_t *map);
u32 obj_globe_display(object_t *obj, map_t *map);

objecttype_t ot_globe;
objecttype_t *OT_GLOBE;

#endif /* GLOBE_H */
