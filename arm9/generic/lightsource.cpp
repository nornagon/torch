#include "light.h"
#include "mem.h"
#include <malloc.h>
#include <string.h>

void set_light(lightsource *light, int32 radius, int32 r, int32 g, int32 b) {
	light->type = LIGHT_POINT;
	light->radius = radius;
	light->angle = 0;
	light->direction = D_NONE;
	light->r = r;
	light->g = g;
	light->b = b;
}

void set_light_beam(lightsource *light, DIRECTION dir, int32 radius, int32 angle, int32 r, int32 g, int32 b) {
	light->type = LIGHT_BEAM;
	light->radius = radius;
	light->angle = angle;
	light->direction = dir;
	light->r = r;
	light->g = g;
	light->b = b;
}

lightsource *new_light(int32 radius, int32 r, int32 g, int32 b) {
	lightsource *light = new lightsource;
	memset(light, 0, sizeof(lightsource));
	set_light(light, radius, r, g, b);
	return light;
};

lightsource *new_light_beam(DIRECTION dir, int32 radius, int32 angle, int32 r, int32 g, int32 b) {
	lightsource *light = new lightsource;
	memset(light, 0, sizeof(lightsource));
	set_light_beam(light, dir, radius, angle, r, g, b);
	return light;
};
