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
