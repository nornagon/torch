#ifndef LIGHT_H
#define LIGHT_H 1

#include <nds.h>

// light_t holds information about a specific light source, as well as
// intermediate information used by processes that alter the light source (such
// as flickering)
typedef struct {
	int32 x,y; // position in the map
	int32 r,g,b;
	int32 radius;
} light_t;

// fill the properties of a light structure
void set_light(light_t *light, int32 radius, int32 r, int32 g, int32 b);

// allocate some space for a new light structure. You will be responsible for
// freeing the light.
light_t *new_light(int32 radius, int32 r, int32 g, int32 b);

#endif /* LIGHT_H */
