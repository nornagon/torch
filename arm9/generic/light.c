#include "light.h"
#include "mem.h"
#include <malloc.h>

void set_light(light_t *light, int32 radius, int32 r, int32 g, int32 b) {
	light->radius = radius;
	light->r = r;
	light->g = g;
	light->b = b;
}

light_t *new_light(int32 radius, int32 r, int32 g, int32 b) {
	light_t *light = malloc(sizeof(light_t));
	memset32(light, 0, sizeof(light_t)/4);
	set_light(light, radius, r, g, b);
	return light;
};