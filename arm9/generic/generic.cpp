#include "generic.h"
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

void draw_light(fov_settings_type *settings, light_t *l) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < map.scrollX ||
	    ((l->x - l->radius) >> 12) > map.scrollX + 32 ||
	    ((l->y + l->radius) >> 12) < map.scrollY ||
	    ((l->y - l->radius) >> 12) > map.scrollY + 24) return;

	// calculate lighting values
	fov_circle(settings, &map, l,
			l->x>>12, l->y>>12, (l->radius>>12) + 1);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	Cell *cell = map.at(l->x>>12, l->y>>12);
	if (cell->visible) {
		Cache *cache = map.cache_at(l->x>>12, l->y>>12);
		cache->light += (1<<12);
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

bool opacity_test(void *map_, int x, int y) {
	Map *map = (Map*)map_;
	if (map->is_outside(x,y)) return true;
	return map->at(x, y)->opaque || (map->pX == x && map->pY == y);
}

void apply_light(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	Map *map = (Map*)map_;
	if (map->is_outside(x,y)) return;

	Cell *cell = map->at(x, y);

	// don't light the cell if we can't see it
	if (!cell->visible) return;

	light_t *l = (light_t*)src_;
	int32 dx = (l->x - (x << 12)) >> 2, // shifting is for accuracy reasons
	      dy = (l->y - (y << 12)) >> 2,
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius,
	      rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);

		DIRECTION d = D_NONE;
		if (cell->opaque)
			d = seen_from(direction(l->x>>12, l->y>>12, x, y), cell);
		Cache *cache = map->cache_at(x, y);

		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

		if (d & D_BOTH || cache->seen_from & D_BOTH) {
			intensity >>= 1;
			d &= cache->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) cache->light += intensity;
			else if (d & D_SOUTH) cache->light += intensity;
			if (d & D_EAST) cache->light += intensity;
			else if (d & D_WEST) cache->light += intensity;
		} else if (cache->seen_from == d)
			cache->light += intensity;

		cache->lr += (l->r * intensity) >> 12;
		cache->lg += (l->g * intensity) >> 12;
		cache->lb += (l->b * intensity) >> 12;
	}
}
#ifdef __cplusplus
}
#endif
