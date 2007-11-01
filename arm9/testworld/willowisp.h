#ifndef WILLOWISP_H
#define WILLOWISP_H 1

#include "torch.h"

#include "light.h"

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
	light_t light;

	// the wisp will try to stay close to its home
	s32 homeX, homeY;

	// the wisp shouldn't move too fast, so we keep a counter.
	// TODO: maybe allow processes to be called at longer intervals?
	u32 counter;
} mon_WillOWisp_t;

void mon_WillOWisp_entered(object_t *object, object_t *incoming, map_t *map);
void mon_WillOWisp_held(process_t *process, map_t *map);

void mon_WillOWisp_light(process_t *process, map_t *map);

void mon_WillOWisp_wander(process_t *process, map_t *map);
void mon_WillOWisp_follow(process_t *process, map_t *map);

void mon_WillOWisp_obj_end(object_t *object, map_t *map);
void mon_WillOWisp_proc_end(process_t *process, map_t *map);

void new_mon_WillOWisp(map_t *map, s32 x, s32 y);

objecttype_t ot_wisp;
objecttype_t* OT_WISP;

#endif /* WILLOWISP_H */
