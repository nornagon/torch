#include "sight.h"
#include "torch.h"
#include "light.h"
#include "generic.h"

bool sight_opaque(void *map_, int x, int y) {
	Map *map = (Map*)map_;
	// stop at the edge of the screen
	if (y < map->scrollY || y >= map->scrollY + 24
	    || x < map->scrollX || x >= map->scrollX + 32)
		return true;
	return map->at(x, y)->opaque;
}

void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	Map *map = (Map*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;
	light_t *l = (light_t*)src_;

	// don't bother calculating if we're outside the edge of the screen
	s32 scrollX = map->scrollX, scrollY = map->scrollY;
	if (x < scrollX || y < scrollY || x > scrollX + 31 || y > scrollY + 23) return;

	Cell *cell = map->at(x, y);
	cache_t *cache = cache_at(map, x, y);

	DIRECTION d = D_NONE;
	if (cell->opaque)
		d = seen_from(map, direction(map->pX, map->pY, x, y), cell);
	cache->seen_from = d;

	// the funny bit-twiddling here is to preserve a few more bits in dx/dy
	// during multiplication. mulf32 is a software multiply, and thus slow.
	int32 dx = (l->x - (x << 12)) >> 2,
				dy = (l->y - (y << 12)) >> 2,
				dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius,
				rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);
		cache_t *cache = cache_at(map, x, y); // load the cache while waiting for the division
		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

		cache->light = intensity;

		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
	}
	cell->visible = true;
}
