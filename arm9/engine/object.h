#ifndef OBJECT_H
#define OBJECT_H 1

struct ObjType;

// object_t is for things that need to be drawn on the map, e.g. NPCs, dynamic
// bits of landscape, items
struct Object {
	s32 x,y;
	struct ObjType* type;
	void* data;
	u8 quantity;
};

// objecttype_ts are kept in an array, and define various properties of
// particular objects.
struct ObjType {
	u8 ch;
	u16 col;

	// when we draw stuff on the map, we need to know which character should be
	// drawn on top. Stuff with a higher importance is drawn over stuff with less
	// importance.
	u8 importance;

	// if non-NULL, this will be called prior to drawing the object. It should
	// return both a colour and a character, with the character in the high bytes.
	// i.e: return (ch << 16) | col;
	u32 (*display)(Object *obj);

	// called when an object of this type is about to be destroyed.
	void (*end)(Object *obj);

	void *data;
};

#endif /* OBJECT_H */
