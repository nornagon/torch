#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H 1

#include <nds/jtypes.h>
#include "direction.h"

enum LIGHT_TYPE {
	LIGHT_POINT,
	LIGHT_BEAM,
};

enum FLICKER_TYPE {
	FLICKER_NONE,
	FLICKER_LIGHT,
	FLICKER_DARK,
	FLICKER_RADIUS,
};

// lightsource holds information about a specific light source, as well as
// intermediate information used by processes that alter the light source (such
// as flickering)
struct lightsource {
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

// fill the properties of a light structure
void set_light(lightsource *light, int32 radius, int32 r, int32 g, int32 b);
void set_light_beam(lightsource *light, DIRECTION direction, int32 radius, int32 angle,
                    int32 r, int32 g, int32 b);

// allocate some space for a new light structure. You will be responsible for
// freeing the light.
lightsource *new_light(int32 radius, int32 r, int32 g, int32 b);
lightsource *new_light_beam(DIRECTION direction, int32 radius, int32 angle,
                            int32 r, int32 g, int32 b);

#endif /* LIGHTSOURCE_H */
