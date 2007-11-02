#include "rocks.h"
#include "testworld.h"

void obj_rock_end(object_t *obj, map_t *map) {
	free(obj->data);
}

bool obj_rock_combinable(object_t *obj, object_t *other) {
	return true;
}

gameobjtype_t go_rocks = {
	.obtainable = true,
	.singular = "rock",
	.combinable = obj_rock_combinable,
};

objecttype_t ot_rocks = {
	.ch = ':',
	.col = RGB15(27,27,27),
	.importance = 1,
	.data = &go_rocks,
	.end = obj_rock_end
};
objecttype_t *OT_ROCKS = &ot_rocks;

void new_obj_rock(map_t *map, s32 x, s32 y, u8 quantity) {
	node_t *obj_node = new_object(map, OT_ROCKS, NULL);
	object_t *obj = node_data(obj_node);
	obj->quantity = quantity;
	insert_object(map, obj_node, x, y);
}
