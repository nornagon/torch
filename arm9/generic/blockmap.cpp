#include "blockmap.h"

blockmap::blockmap() {
	els = 0;
	w = h = 0;
	pX = pY = 0;
}

blockmap::blockmap(int w, int h) {
	resize(w, h);
}

void blockmap::resize(int _w, int _h) {
	if (els) delete [] els;

	w = _w; h = _h;

	els = new blockel[w*h];
}

void blockmap::reset() {
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			at(x,y)->reset();
}

void blockmap::refresh_blocked_from() {
	// each cell needs to know which cells around them are opaque for purposes of
	// direction-aware lighting.

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			unsigned int blocked_from = 0;
			if (y != 0 && at(x, y-1)->opaque) blocked_from |= D_NORTH;
			if (y != h - 1 && at(x, y+1)->opaque) blocked_from |= D_SOUTH;
			if (x != w - 1 && at(x+1, y)->opaque) blocked_from |= D_EAST;
			if (x != 0 && at(x-1, y)->opaque) blocked_from |= D_WEST;
			at(x, y)->blocked_from = blocked_from;
		}
}
