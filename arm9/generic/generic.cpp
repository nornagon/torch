#include "generic.h"
#include "engine.h"
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

fov_settings_type *build_fov_settings(
    bool (*opaque)(void *map, int x, int y),
    void (*apply)(void *map, int x, int y, int dx, int dy, void *src),
    fov_shape_type shape) {
  fov_settings_type *settings = new fov_settings_type;
  fov_settings_init(settings);
  fov_settings_set_shape(settings, shape);
  fov_settings_set_opacity_test_function(settings, opaque);
  fov_settings_set_apply_lighting_function(settings, apply);
  return settings;
}

#include <stdio.h>

void draw_light(fov_settings_type *settings, blockmap *map, light_t *l) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < torch.buf.scroll.x ||
	    ((l->x - l->radius) >> 12) >= torch.buf.scroll.x + 32 ||
	    ((l->y + l->radius) >> 12) < torch.buf.scroll.y ||
	    ((l->y - l->radius) >> 12) >= torch.buf.scroll.y + 24 ||
	    l->radius == 0 || (l->r == 0 && l->g == 0 && l->b == 0)) return;

	// calculate lighting values
	fov_circle(settings, map, l,
			l->x>>12, l->y>>12, (l->radius>>12) + 1);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	blockel *b = map->at(l->x>>12, l->y>>12);
	if (b->visible) {
		luxel *e = torch.buf.luxat(l->x>>12, l->y>>12);
		e->lval += (1<<12);
		e->lr = l->r;
		e->lg = l->g;
		e->lb = l->b;
		torch.buf.at(l->x>>12, l->y>>12)->recall = 1<<12;
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

	light_t *l = (light_t*)src;
	int32 dx = (l->x - (x << 12)) >> 2, // shifting is for accuracy reasons
	      dy = (l->y - (y << 12)) >> 2,
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius,
	      rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);

		DIRECTION d = D_NONE;
		if (b->opaque)
			d = seen_from(direction(l->x>>12, l->y>>12, x, y), b);
		luxel *e = torch.buf.luxat(x, y);

		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

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
#ifdef __cplusplus
}
#endif
