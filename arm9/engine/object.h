#ifndef OBJECT_H
#define OBJECT_H 1

struct map_s;
struct objecttype_s;

// object_t is for things that need to be drawn on the map, e.g. NPCs, dynamic
// bits of landscape, items
typedef struct {
	s32 x,y;
	struct objecttype_s* type; // index into array of objecttype_ts
	void* data;
	u8 quantity;
} object_t;

// objecttype_ts are kept in an array, and define various properties of
// particular objects.
typedef struct objecttype_s {
	u8 ch;
	u16 col;

	// when we draw stuff on the map, we need to know which character should be
	// drawn on top. Stuff with a higher importance is drawn over stuff with less
	// importance.
	u8 importance;

	// if non-NULL, this will be called prior to drawing the object. It should
	// return both a colour and a character, with the character in the high bytes.
	// i.e: return (ch << 16) | col;
	u32 (*display)(object_t *obj, struct map_s *map);

	// called when an object of this type is about to be destroyed.
	void (*end)(object_t *obj, struct map_s *map);

	void *data;
} objecttype_t;

#endif /* OBJECT_H */
