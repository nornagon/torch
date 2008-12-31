#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H 1

#include <nds/jtypes.h>
#include "direction.h"
#include "list.h"

enum LIGHT_TYPE {
	LIGHT_POINT,
	LIGHT_BEAM,
};

typedef void(*FLICKER_TYPE)(struct lightsource*);

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
};

void FLICKER_NONE(lightsource*);
void FLICKER_RADIUS(lightsource*);
void FLICKER_LIGHT(lightsource*);
void FLICKER_DARK(lightsource*);

#endif /* LIGHTSOURCE_H */
