#include "sight.h"
#include "torch.h"
#include "light.h"

#include "nocash.h"

#include "blockmap.h"

void cast_sight(fov_settings_type *settings, blockmap *block, lightsource *l) {
	fov_circle(settings, block, l, l->x>>12, l->y>>12, 32);
	luxel *e = torch.buf.luxat(l->x>>12, l->y>>12);
	e->lval = (1<<12);
	e->lr = l->r;
	e->lg = l->g;
	e->lb = l->b;
	torch.buf.at(l->x>>12, l->y>>12)->recall = 1<<12;
	block->at(l->x>>12, l->y>>12)->visible = true;
}

bool sight_opaque(void *map_, int x, int y) {
	blockmap *map = (blockmap*)map_;
	// stop at the edge of the screen
	if (y < torch.buf.scroll.y || y >= torch.buf.scroll.y + 24
	    || x < torch.buf.scroll.x || x >= torch.buf.scroll.x + 32)
		return true;
	return map->at(x, y)->opaque;
}

void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src) {
	blockmap *map = (blockmap*)map_;
	if (map->is_outside(x,y)) return;
	lightsource *l = (lightsource*)src;

	// don't bother calculating if we're outside the edge of the screen
	s32 scrollX = torch.buf.scroll.x, scrollY = torch.buf.scroll.y;
	if (x < scrollX || y < scrollY || x >= scrollX + 32 || y >= scrollY + 24) return;

	blockel *b = map->at(x, y);

	DIRECTION d = D_NONE;
	if (b->opaque)
		d = seen_from(direction(map->pX, map->pY, x, y), b);
	b->seen_from = d;

	// the funny bit-twiddling here is to preserve a few more bits in dx/dy
	// during multiplication. mulf32 is a software multiply, and thus slow.
	int32 dx = (l->x - (x << 12)) >> 2,
				dy = (l->y - (y << 12)) >> 2,
				dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = l->radius,
				rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);
		//Cache *cache = map->cache_at(x, y); // load the cache while waiting for the division
		luxel *e = torch.buf.luxat(x,y);
		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

		e->lval = intensity;

		e->lr = l->r;
		e->lg = l->g;
		e->lb = l->b;
	}
	b->visible = true;
}
