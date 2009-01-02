#include "light.h"
#include "mem.h"
#include "mersenne.h"
#include <malloc.h>
#include <string.h>
#include <deque>

static std::deque<FLICKER_TYPE> flickerTypes;
static bool flickerTypesInitialized = false;

static void initializeFlickerTypes() {
	if (flickerTypesInitialized) return;
	flickerTypes.push_front(FLICKER_DARK);
	flickerTypes.push_front(FLICKER_LIGHT);
	flickerTypes.push_front(FLICKER_RADIUS);
	flickerTypes.push_front(FLICKER_NONE);
	flickerTypesInitialized = true;
}

void addFlickerType(FLICKER_TYPE f) {
	flickerTypes.push_back(f);
}

static u8 flickerTypeIndex(FLICKER_TYPE f) {
	u8 i = 0;
	std::deque<FLICKER_TYPE>::iterator iter;
	for (iter = flickerTypes.begin();
	     iter != flickerTypes.end() && *iter != f;
	     iter++, i++);
	assert(iter != flickerTypes.end());
	return i;
}

lightsource::lightsource() {
	initializeFlickerTypes();
	memset(this, 0, sizeof(lightsource));
}

lightsource::lightsource(int32 radius, int32 r, int32 g, int32 b) {
	initializeFlickerTypes();
	memset(this, 0, sizeof(lightsource));
	set(radius, r, g, b);
};

lightsource::lightsource(DIRECTION dir, int32 radius, int32 angle, int32 r, int32 g, int32 b) {
	initializeFlickerTypes();
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
	if (flicker)
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
void FLICKER_DARK(lightsource*) {}
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

// for some reason C++ was automatically casting LIGHT_TYPE to something
// sensible for operator<<, but not for operator>>...
DataStream& operator <<(DataStream& s, LIGHT_TYPE l)
{
	s << (u8) l;
	return s;
}
DataStream& operator >>(DataStream& s, LIGHT_TYPE &l)
{
	u8 tmp;
	s >> tmp;
	l = (LIGHT_TYPE)tmp;
	return s;
}

DataStream& operator <<(DataStream& s, lightsource &l)
{
	s << l.x << l.y;
	s << l.orig_r << l.orig_g << l.orig_b << l.orig_intensity;
	s << l.r << l.g << l.b << l.intensity;
	s << l.type;
	s << l.radius << l.orig_radius;
	s << l.angle;
	s << l.direction;
	s << flickerTypeIndex(l.flicker);
	return s;
}

DataStream& operator >>(DataStream& s, lightsource &l)
{
	s >> l.x >> l.y;
	s >> l.orig_r >> l.orig_g >> l.orig_b >> l.orig_intensity;
	s >> l.r >> l.g >> l.b >> l.intensity;
	s >> l.type;
	s >> l.radius >> l.orig_radius;
	s >> l.angle;
	s >> l.direction;
	u8 flickerIndex;
	s >> flickerIndex;
	l.flicker = flickerTypes[flickerIndex];
	return s;
}
