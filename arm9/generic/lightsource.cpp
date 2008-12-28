#include "light.h"
#include "mem.h"
#include "mersenne.h"
#include <malloc.h>
#include <string.h>

lightsource::lightsource() {
	memset(this, 0, sizeof(lightsource));
}

lightsource::lightsource(int32 radius, int32 r, int32 g, int32 b) {
	memset(this, 0, sizeof(lightsource));
	set(radius, r, g, b);
};

lightsource::lightsource(DIRECTION dir, int32 radius, int32 angle, int32 r, int32 g, int32 b) {
	memset(this, 0, sizeof(lightsource));
	set(dir, radius, angle, r, g, b);
};

void lightsource::set(int32 radius_, int32 r_, int32 g_, int32 b_) {
	type = LIGHT_POINT;
	radius = orig_radius = radius_;
	angle = 0;
	direction = D_NONE;
	r = orig_r = r_;
	g = orig_g = g_;
	b = orig_b = b_;
	intensity = orig_intensity = (1<<12);

	flicker = FLICKER_NONE;
	frame = 0;
}

void lightsource::set(DIRECTION dir, int32 radius_, int32 angle_, int32 r_, int32 g_, int32 b_) {
	type = LIGHT_BEAM;
	radius = orig_radius = radius_;
	angle = angle_;
	direction = dir;
	r = orig_r = r_;
	g = orig_g = g_;
	b = orig_b = b_;
	intensity = orig_intensity = (1<<12);

	flicker = FLICKER_NONE;
	frame = 0;
}

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
