#ifndef LIGHTSOURCE_H
#define LIGHTSOURCE_H 1

#include <nds.h>

// lightsource holds information about a specific light source, as well as
// intermediate information used by processes that alter the light source (such
// as flickering)
struct lightsource {
	int32 x,y; // position in the map
	int32 r,g,b;
	int32 radius;
};

// fill the properties of a light structure
void set_light(lightsource *light, int32 radius, int32 r, int32 g, int32 b);

// allocate some space for a new light structure. You will be responsible for
// freeing the light.
lightsource *new_light(int32 radius, int32 r, int32 g, int32 b);

#endif /* LIGHTSOURCE_H */
