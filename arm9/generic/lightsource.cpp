#include "light.h"
#include "mem.h"
#include <malloc.h>

void set_light(lightsource *light, int32 radius, int32 r, int32 g, int32 b) {
	light->radius = radius;
	light->r = r;
	light->g = g;
	light->b = b;
}

lightsource *new_light(int32 radius, int32 r, int32 g, int32 b) {
	lightsource *light = new lightsource;
	memset32(light, 0, sizeof(lightsource)/4);
	set_light(light, radius, r, g, b);
	return light;
};
