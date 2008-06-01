#include <nds/jtypes.h>
#include "bresenstate.h"
#include "util.h"

bresenstate::bresenstate(s16 x0_, s16 y0_, s16 x1_, s16 y1_) {
	reset(x0_,y0_,x1_,y1_);
}

bresenstate::bresenstate() {
	x0 = x1 = y0 = y1 = deltax = deltay = error = ystep = x = y = 0;
	steep = reversed = false;
}

void bresenstate::reset(s16 x0_, s16 y0_, s16 x1_, s16 y1_) {
	steep = abs(y1_-y0_) > abs(x1_-x0_);
	if (steep) {
		x0 = y0_; x1 = y1_;
		y0 = x0_; y1 = x1_;
	} else {
		x0 = x0_; x1 = x1_;
		y0 = y0_; y1 = y1_;
	}
	reversed = x0 > x1;
	if (reversed) {
		s16 tmp = x0; x0 = x1; x1 = tmp;
		tmp = y0; y0 = y1; y1 = tmp;
	}
	deltax = x1 - x0;
	deltay = abs(y1 - y0);
	error = -(deltax+1)/2;
	ystep = y0 < y1 ? 1 : -1;
	y = y0;
	x = x0;
}

void bresenstate::step() {
	error += deltay;
	if (error >= 0) {
		y += ystep;
		error -= deltax;
	}
	x++;
}

s16 bresenstate::posx() {
	if (reversed) {
		return destx() - (steep?y:x) + (steep?y1:x1);
	} else {
		if (steep) return y; return x;
	}
}

s16 bresenstate::posy() {
	if (reversed) {
		return desty() - (steep?x:y) + (steep?x1:y1);
	} else {
		if (steep) return x; return y;
	}
}

s16 bresenstate::destx() {
	if (reversed) {
		if (steep) return y0; return x0;
	} else {
		if (steep) return y1; return x1;
	}
}

s16 bresenstate::desty() {
	if (reversed) {
		if (steep) return x0; return y0;
	} else {
		if (steep) return x1; return y1;
	}
}
