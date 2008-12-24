#include "recalc.h"
#include "torch.h"

void recalc(blockmap *block, s16 x, s16 y) {
	blockel *b = block->at(x, y);
	mapel *m = torch.buf.at(x,y);
	if (b->visible && torch.buf.luxat(x,y)->lval > 0) {
		visible_appearance(x, y, &m->ch, &m->col);
	} else if (x < torch.buf.scroll.x || x-32 > torch.buf.scroll.x ||
	           y < torch.buf.scroll.y || y-32 > torch.buf.scroll.y ||
	           torch.buf.cacheat(x,y)->was_visible) {
	  // recalculating either something off the screen or something that has
	  // become invisible
		recalled_appearance(x, y, &m->ch, &m->col);
	}
}

void refresh(blockmap *block) {
	for (int y = torch.buf.scroll.y; y < torch.buf.scroll.y + 24; y++)
		for (int x = torch.buf.scroll.x; x < torch.buf.scroll.x + 32; x++) {
			int32 v = torch.buf.luxat(x, y)->lval;
			mapel *m = torch.buf.at(x, y);
			if (isforgettable(x,y))
				m->recall = 0;
			else
				m->recall = min(1<<12, max(v, m->recall));

			recalc(block, x, y);
		}
}
