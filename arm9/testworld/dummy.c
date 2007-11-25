#include "dummy.h"
#include "creature.h"
#include "world.h"

void dummy_end(object_t *obj, map_t *map);

gameobjtype_t go_dummy = {
	.obtainable = false,
	.creature = true,
	.singular = "training dummy",
	.plural = "training dummies",
	.obstructs = does_obstruct,
};

objecttype_t ot_dummy = {
	.ch = 'd',
	.col = RGB15(27,20,8),
	.importance = 128,
	.display = NULL,
	.data = &go_dummy,
	.end = dummy_end,
};

objecttype_t *OT_DUMMY = &ot_dummy;

typedef struct {
	creature_t creature;
} dummy_t;

void new_dummy(map_t *map, s32 x, s32 y) {
	dummy_t *dummy = malloc(sizeof(dummy_t));
	dummy->creature.hp = 40;
	dummy->creature.ac = 5;

	node_t *obj = new_object(map, OT_DUMMY, dummy);
	insert_object(map, obj, x, y);
}

void dummy_end(object_t *obj, map_t *map) {
	free(obj->data); // TODO: make this the default behaviour
}
