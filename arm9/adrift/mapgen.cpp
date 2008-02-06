#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

#include "generic.h"
#include <malloc.h>

#include <stdio.h>

#define abs(x) ((x) < 0 ? -(x) : (x))

void hline(Map *m, s32 x0, s32 x1, s32 y, void (*func)(Cell*)) {
	m->bounded(x1, y);
	if (x0 > x1) hline(m, x1, x0, y, func);
	for (; x0 <= x1; x0++)
		func(m->at(x0, y));
}

void hollowCircle(Map *m, s32 x, s32 y, s32 r, void (*func)(Cell*)) {
	s32 left, right, top, bottom;
	s32 x1, y1, x2, y2;
	s32 cx = 0;
	s32 cy = r;
	s32 ocx = (s32) 0xffff;
	s32 ocy = (s32) 0xffff;
	s32 df = 1 - r;
	s32 d_e = 3;
	s32 d_se = -2 * r + 5;
	s32 xpcx, xmcx, xpcy, xmcy;
	s32 ypcy, ymcy, ypcx, ymcx;

	/*
	 * Sanity check radius
	 */
	if (r < 0)
		return;

	/*
	 * Special case for r=0 - draw a point
	 */
	if (r == 0) {
		func(m->at(x,y));
		return;
	}

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = m->getWidth() - 1;
	top = 0;
	bottom = m->getHeight() - 1;

	/*
	 * Test if bounding box of circle is visible
	 */
	x1 = x - r;
	x2 = x + r;
	y1 = y - r;
	y2 = y + r;
	if ((x1 < left) && (x2 < left)) return;
	if ((x1 > right) && (x2 > right)) return;
	if ((y1 < top) && (y2 < top)) return;
	if ((y1 > bottom) && (y2 > bottom)) return;

	/*
	 * Draw
	 */
	do {
		if ((ocy != cy) || (ocx != cx)) {
			xpcx = x + cx;
			xmcx = x - cx;
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				func(m->at(xmcx,ypcy));
				func(m->at(xpcx,ypcy));
				func(m->at(xmcx,ymcy));
				func(m->at(xpcx,ymcy));
			} else {
				func(m->at(xmcx,y));
				func(m->at(xpcx,y));
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				func(m->at(xmcy,ypcx));
				func(m->at(xpcy,ypcx));
				func(m->at(xmcy,ymcx));
				func(m->at(xpcy,ymcx));
			} else {
				func(m->at(xmcy,y));
				func(m->at(xpcy,y));
			}
			ocx = cx;
		}
		/*
		 * Update
		 */
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

// stolen from SDL_gfxPrimitives
void filledCircle(Map *m, s32 x, s32 y, s32 r, void (*func)(Cell*))
{
	s32 left, right, top, bottom;
	int result;
	s32 x1, y1, x2, y2;
	s32 cx = 0;
	s32 cy = r;
	s32 ocx = (s32) 0xffff;
	s32 ocy = (s32) 0xffff;
	s32 df = 1 - r;
	s32 d_e = 3;
	s32 d_se = -2 * r + 5;
	s32 xpcx, xmcx, xpcy, xmcy;
	s32 ypcy, ymcy, ypcx, ymcx;

	/*
	 * Sanity check radius
	 */
	if (r < 0)
		return;

	/*
	 * Special case for r=0 - draw a point
	 */
	if (r == 0) {
		func(m->at(x, y));
		return;
	}

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = m->getWidth() - 1;
	top = 0;
	bottom = m->getHeight() - 1;

	/*
	 * Test if bounding box of circle is visible
	 */
	x1 = x - r;
	x2 = x + r;
	y1 = y - r;
	y2 = y + r;
	if ((x1 < left) && (x2 < left)) return;
	if ((x1 > right) && (x2 > right)) return;
	if ((y1 < top) && (y2 < top)) return;
	if ((y1 > bottom) && (y2 > bottom)) return;

	/*
	 * Draw
	 */
	result = 0;
	do {
		xpcx = x + cx;
		xmcx = x - cx;
		xpcy = x + cy;
		xmcy = x - cy;
		if (ocy != cy) {
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				hline(m, xmcx, xpcx, ypcy, func);
				hline(m, xmcx, xpcx, ymcy, func);
			} else {
				hline(m, xmcx, xpcx, y, func);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					hline(m, xmcy, xpcy, ymcx, func);
					hline(m, xmcy, xpcy, ypcx, func);
				} else {
					hline(m, xmcy, xpcy, y, func);
				}
			}
			ocx = cx;
		}
		/*
		 * Update
		 */
		if (df < 0) {
			df += d_e;
			d_e += 2;
			d_se += 2;
		} else {
			df += d_se;
			d_e += 2;
			d_se += 4;
			cy--;
		}
		cx++;
	} while (cx <= cy);
}

void bresenham(Map *m, s32 x0, s32 y0, s32 x1, s32 y1, void (*func)(Cell*)) {
	bool steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		s32 tmp = x0; x0 = y0; y0 = tmp;
		tmp = x1; x1 = y1; y1 = tmp;
	}
	if (x0 > x1) {
		s32 tmp = x0; x0 = x1; x1 = tmp;
		tmp = y0; y0 = y1; y1 = tmp;
	}
	s32 deltax = x1 - x0;
	s32 deltay = abs(y1-y0);
	s32 error = -(deltax + 1) / 2;
	s32 ystep = y0 < y1 ? 1 : -1;
	s32 y = y0;
	for (int x = x0; x < x1; x++) {
		if (steep) func(m->at(y, x));
		else func(m->at(x, y));
		error += deltay;
		if (error >= 0) {
			y += ystep;
			error -= deltax;
		}
	}
}

#define DEF(type, ch, col, opaque, forget) \
	static inline void SET_##type(Cell *c) { *c = (Cell){ T_##type, (col), (ch), (opaque), (forget) }; }
DEF(TREE, '*', RGB15(4,31,1), true, false);
DEF(GROUND, '.', RGB15(17,9,6), false, true);
DEF(NONE, ' ', RGB15(31,31,31), false, true);
DEF(GLASS, '/', RGB15(4,12,30), false, false);
#undef DEF

void randwalk(s32 &x, s32 &y) {
	unsigned int a = genrand_int32();
	switch (a & 3) {
		case 0: x++; break;
		case 1: x--; break;
		case 2: y++; break;
		case 3: y--; break;
	}
}

void haunted_grove(s32 cx, s32 cy) {
	int r0 = 5, r1 = 15;
	int w0 = 2, w1 = 4;
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = (rand8() & 3) + r0;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(&map, cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w0)) >> 12), cy + ((y*(r+w0)) >> 12), SET_TREE);
	}
	for (unsigned int t = 0; t < 0x1ff; t += 2) {
		int a = rand8();
		int r = (a & 3) + r1; a >>= 2;
		int x = COS[t], y = SIN[t];
		if (t % 16 > (a & 3))
			bresenham(&map, cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w1)) >> 12), cy + ((y*(r+w1)) >> 12), SET_TREE);
	}
	u16 k = rand16() & 0x1ff;
	int x = COS[k], y = SIN[k];
	bresenham(&map, cx + ((x*r0) >> 12), cy + ((y*r0) >> 12),
	               cx + ((x*(r0+w0)) >> 12), cy + ((y*(r0+w0)) >> 12), SET_GROUND);
	k = rand16() & 0x1ff;
	x = COS[k]; y = SIN[k];
	bresenham(&map, cx + ((x*r1) >> 12), cy + ((y*r1) >> 12),
	               cx + ((x*(r1+w1)) >> 12), cy + ((y*(r1+w1)) >> 12), SET_GROUND);
}

void generate_terrarium() {
	s32 cx = map.getWidth()/2, cy = map.getHeight()/2;

	map.pX = cx;
	map.pY = cy;

	map.reset();
	map.reset_cache();

	for (int y = 0; y < map.getHeight(); y++)
		for (int x = 0; x < map.getWidth(); x++)
			SET_NONE(map.at(x,y));

	filledCircle(&map, cx, cy, 60, SET_GROUND);
	hollowCircle(&map, cx, cy, 60, SET_GLASS);

	haunted_grove(cx, cy);

	map.refresh_blockmap();
}
