#include "light.h"
#include "engine.h"
#include "util.h"

#ifdef NATIVE
#include "native.h"
#endif

#include "assert.h"
#include <stdio.h>

fov_direction_type to_fov_dir(DIRECTION dir) {
	switch (dir) {
		case D_NORTH: return FOV_NORTH;
		case D_SOUTH: return FOV_SOUTH;
		case D_EAST: return FOV_EAST;
		case D_WEST: return FOV_WEST;
		case D_NORTHEAST: return FOV_NORTHEAST;
		case D_NORTHWEST: return FOV_NORTHWEST;
		case D_SOUTHEAST: return FOV_SOUTHEAST;
		case D_SOUTHWEST: return FOV_SOUTHWEST;
	}
	assert(!"nonexistent direction");
	return FOV_EAST;
}

void draw_lights(fov_settings_type *settings, blockmap *map, List<lightsource> lights) {
	lightsource *k = lights.head();
	for (; k; k = k->next())
		draw_light(settings, map, k);
}

void draw_light(fov_settings_type *settings, blockmap *map, lightsource *l) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < torch.buf.scroll.x ||
	    ((l->x - l->radius) >> 12) >= torch.buf.scroll.x + 32 ||
	    ((l->y + l->radius) >> 12) < torch.buf.scroll.y ||
	    ((l->y - l->radius) >> 12) >= torch.buf.scroll.y + 24 ||
	    l->radius == 0 || (l->r == 0 && l->g == 0 && l->b == 0)) return;

	// calculate lighting values
	if (l->type == LIGHT_POINT) {
		fov_circle(settings, map, l,
				l->x>>12, l->y>>12, (l->radius>>12) + 1);
	} else {
		fov_beam(settings, map, l,
				l->x>>12, l->y>>12, (l->radius>>12) + 1,
				to_fov_dir(l->direction), l->angle);
	}

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	blockel *b = map->at(l->x>>12, l->y>>12);
	if (b->visible) {
		luxel *e = torch.buf.luxat(l->x>>12, l->y>>12);
		e->lval += l->intensity;
		e->lr += l->r;
		e->lg += l->g;
		e->lb += l->b;
		torch.buf.at(l->x>>12, l->y>>12)->recall = l->intensity;
	}
}

bool opacity_test(void *map_, int x, int y) {
	blockmap *map = (blockmap*)map_;
	if (map->is_outside(x,y)) return true;
	return map->at(x, y)->opaque || (map->pX == x && map->pY == y);
}

void apply_light(void *map_, int x, int y, int dxblah, int dyblah, void *src) {
	blockmap *map = (blockmap*)map_;
	if (map->is_outside(x,y)) return;

	blockel *b = map->at(x, y);

	// don't light the cell if we can't see it
	if (!b->visible) return;

	lightsource *l = (lightsource*)src;
	int32 dx = ((l->x>>4) - (x << 8)),
	      dy = ((l->y>>4) - (y << 8)),
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius >> 4,
	      rad2 = (rad * rad) >> 8;

	if (dist2 < rad2) {
		div_32_32_raw(rad2*(l->intensity>>4),rad2+(((9<<8)*dist2)>>8));

		DIRECTION d = D_NONE;
		if (b->opaque)
			d = seen_from(direction(l->x>>12, l->y>>12, x, y), b);
		luxel *e = torch.buf.luxat(x, y);

		while (DIV_CR & DIV_BUSY);
		int32 intensity = DIV_RESULT32 << 4;

		if (d & D_BOTH || b->seen_from & D_BOTH) {
			intensity >>= 1;
			d &= b->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) e->lval += intensity;
			else if (d & D_SOUTH) e->lval += intensity;
			if (d & D_EAST) e->lval += intensity;
			else if (d & D_WEST) e->lval += intensity;
		} else if (b->seen_from == d)
			e->lval += intensity;

		e->lr += (l->r * intensity) >> 12;
		e->lg += (l->g * intensity) >> 12;
		e->lb += (l->b * intensity) >> 12;
	}
}
