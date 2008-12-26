#include "sight.h"
#include "torch.h"
#include "light.h"

#include "nocash.h"

#include "blockmap.h"

#ifdef NATIVE
#include "native.h"
#endif

void cast_sight(fov_settings_type *settings, blockmap *block, lightsource *l) {
	fov_circle(settings, block, l, l->x>>12, l->y>>12, 32);
	luxel *e = torch.buf.luxat(l->x>>12, l->y>>12);
	e->lval = l->intensity;
	e->lr = l->r;
	e->lg = l->g;
	e->lb = l->b;
	torch.buf.at(l->x>>12, l->y>>12)->recall = l->intensity;
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
	// don't bother calculating if we're outside the edge of the screen
	s32 scrollX = torch.buf.scroll.x, scrollY = torch.buf.scroll.y;
	if (x < scrollX || y < scrollY || x >= scrollX + 32 || y >= scrollY + 24) return;

	lightsource *l = (lightsource*)src;

	blockel *b = map->at(x, y);

	DIRECTION d = D_NONE;
	if (b->opaque)
		d = seen_from(direction(map->pX, map->pY, x, y), b);
	b->seen_from = d;

	// shift through 24.8
	int32 dx = ((l->x >> 4) - (x << 8)),
				dy = ((l->y >> 4) - (y << 8)),
				dist2 = ((dx * dx)>>8) + ((dy * dy)>>8);
	int32 rad = l->radius >> 4,
				rad2 = (rad * rad) >> 8;

	if (dist2 < rad2) {
		div_32_32_raw(rad2*(l->intensity>>4),(rad2+(((9<<8)*dist2)>>8)));
		luxel *e = torch.buf.luxat(x,y);
		while (DIV_CR & DIV_BUSY);
		int32 intensity = DIV_RESULT32 << 4;

		e->lval = intensity;

		e->lr = (l->r * intensity) >> 12;
		e->lg = (l->g * intensity) >> 12;
		e->lb = (l->b * intensity) >> 12;
	}
	b->visible = true;
}
