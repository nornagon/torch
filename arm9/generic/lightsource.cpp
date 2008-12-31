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
	flicker(this);
}

void FLICKER_NONE(lightsource*) {}
void FLICKER_LIGHT(lightsource *l) {
	if (l->frame > 0) {
		l->frame--;
		if (l->frame == 0) {
			l->frame = -1-rand4();
			l->intensity = l->orig_intensity >> 1;
		}
	} else {
		l->frame++;
		if (l->frame == 0) {
			l->frame = 1+(rand8()%128);
			l->r = l->orig_r;
			l->g = l->orig_g;
			l->b = l->orig_b;
			l->intensity = l->orig_intensity;
		}
	}
}
void FLICKER_RADIUS(lightsource *l) {
	if (l->frame > 0) {
		l->frame--;
		if (l->frame == 0) {
			l->frame = -1-(rand32()%5);
			int32 factor = 2*rand8();
			l->radius = l->orig_radius - ((l->orig_radius*factor)>>12);
			l->intensity = l->orig_intensity - ((l->orig_intensity*factor)>>12);
		}
	} else {
		l->frame++;
		if (l->frame == 0) {
			l->frame = 1+(rand4()&0x7);
			l->radius = l->orig_radius;
		}
	}
}
