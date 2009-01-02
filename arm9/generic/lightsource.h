#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H 1

#include <nds/jtypes.h>
#include "direction.h"
#include "list.h"
#include "datastream.h"

enum LIGHT_TYPE {
	LIGHT_POINT = 0,
	LIGHT_BEAM,
};

typedef void(*FLICKER_TYPE)(struct lightsource*);

// if you define a custom flicker type, it must be registered with
// addFlickerType for serialisation *BEFORE LOADING* to work correctly
void addFlickerType(FLICKER_TYPE);

// lightsource holds information about a specific light source, as well as
// intermediate information used by processes that alter the light source (such
// as flickering)
struct lightsource : public listable<lightsource> {
	lightsource();
	lightsource(int32 radius, int32 r, int32 g, int32 b);
	lightsource(DIRECTION direction, int32 radius, int32 angle, int32 r, int32 g, int32 b);

	// fill the properties of a light structure
	void set(int32 radius, int32 r, int32 g, int32 b);
	void set(DIRECTION direction, int32 radius, int32 angle, int32 r, int32 g, int32 b);

	int32 x,y; // position in the map
	int32 orig_r,orig_g,orig_b, orig_intensity; // original color
	int32 r,g,b, intensity; // current color
	LIGHT_TYPE type;
	int32 radius, orig_radius;
	int32 angle; // angle of beam
	DIRECTION direction; // direction of beam

	FLICKER_TYPE flicker;
	s32 frame; // animation state

	void update_flicker();
	friend DataStream& operator <<(DataStream&, lightsource&);
	friend DataStream& operator >>(DataStream&, lightsource&);
};
DataStream& operator <<(DataStream&, lightsource&);
DataStream& operator >>(DataStream&, lightsource&);

void FLICKER_NONE(lightsource*);
void FLICKER_RADIUS(lightsource*);
void FLICKER_LIGHT(lightsource*);
void FLICKER_DARK(lightsource*);

#endif /* LIGHTSOURCE_H */
