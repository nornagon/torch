#include "buf.h"

torchbuf::torchbuf() {
	w = 0; h = 0;
	map = 0;
	light = 0;
}

torchbuf::torchbuf(s16 w, s16 h) {
	resize(w,h);
}

void torchbuf::resize(s16 _w, s16 _h) {
	w = _w;
	h = _h;

	if (map) delete [] map;
	if (light) delete [] light;

	map = new mapel[w*h];
	light = new luxel[32*24]; // TODO: remove constants

	scroll.reset();
	cache.reset();
}

void torchbuf::reset() {
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			at(x,y)->reset();

	for (int y = 0; y < 24; y++)
		for (int x = 0; x < 32; x++)
			light[y*32+x].reset();
}
