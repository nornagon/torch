#ifndef FIRE_H
#define FIRE_H 1

#include "torch.h"
#include "light.h"

typedef struct {
	node_t *light_node;
	node_t *obj_node;
	unsigned int flickered;
	int32 radius;
	light_t *light;
} obj_fire_t;

void new_obj_fire(map_t *map, s32 x, s32 y, int32 radius);

void obj_fire_process(process_t *process, map_t *map);

void obj_fire_proc_end(process_t *process, map_t *map);
void obj_fire_obj_end(object_t *object, map_t *map);

objecttype_t ot_fire;
objecttype_t *OT_FIRE;

#endif /* FIRE_H */
