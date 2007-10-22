#include "light.h"
#include "mem.h"
#include <malloc.h>

light_t *new_light(int32 radius, int32 r, int32 g, int32 b) {
	light_t *light = malloc(sizeof(light_t));
	memset32(light, 0, sizeof(light_t)/4);
	light->radius = radius;
	light->r = r;
	light->g = g;
	light->b = b;
	return light;
};
