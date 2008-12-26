#include "light.h"
#include "mem.h"
#include "mersenne.h"
#include <malloc.h>
#include <string.h>

void set_light(lightsource *light, int32 radius, int32 r, int32 g, int32 b) {
	light->type = LIGHT_POINT;
	light->radius = light->orig_radius = radius;
	light->angle = 0;
	light->direction = D_NONE;
	light->r = light->orig_r = r;
	light->g = light->orig_g = g;
	light->b = light->orig_b = b;
	light->intensity = light->orig_intensity = (1<<12);

	light->flicker = FLICKER_NONE;
	light->frame = 0;
}

void set_light_beam(lightsource *light, DIRECTION dir, int32 radius, int32 angle, int32 r, int32 g, int32 b) {
	light->type = LIGHT_BEAM;
	light->radius = light->orig_radius = radius;
	light->angle = angle;
	light->direction = dir;
	light->r = light->orig_r = r;
	light->g = light->orig_g = g;
	light->b = light->orig_b = b;
	light->intensity = light->orig_intensity = (1<<12);

	light->flicker = FLICKER_NONE;
	light->frame = 0;
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

void lightsource::update_flicker() {
	switch (flicker) {
		case FLICKER_LIGHT:
			if (frame > 0) {
				frame--;
				if (frame == 0) {
					frame = -1-rand4();
					intensity = orig_intensity >> 1;
				}
			} else {
				frame++;
				if (frame == 0) {
					frame = 1+(rand8()%128);
					r = orig_r;
					g = orig_g;
					b = orig_b;
					intensity = orig_intensity;
				}
			}
			break;
		case FLICKER_RADIUS:
			if (frame > 0) {
				frame--;
				if (frame == 0) {
					frame = -1-(rand32()%5);
					int32 factor = 2*rand8();
					radius = orig_radius - ((orig_radius*factor)>>12);
					intensity = orig_intensity - ((orig_intensity*factor)>>12);
				}
			} else {
				frame++;
				if (frame == 0) {
					frame = 1+(rand4()&0x7);
					radius = orig_radius;
				}
			}
			break;
		case FLICKER_DARK:
		case FLICKER_NONE:
			return;
	}
}
