#include "gfxPrimitives.h"
#include "mersenne.h"
#include "torch.h"

void hline(s16 x0, s16 x1, s16 y, void (*func)(s16 x, s16 y, void *info), void *info) {
	torch.buf.bounded(x1, y);
	if (x0 > x1) hline(x1, x0, y, func, info);
	for (; x0 <= x1; x0++)
		func(x0, y, info);
}

void randwalk(s16 &x, s16 &y) {
	unsigned int a = rand4();
	switch (a & 3) {
		case 0: x++; break;
		case 1: x--; break;
		case 2: y++; break;
		case 3: y--; break;
	}
}

void hollowCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y, void *info), void *info) {
	s16 left, right, top, bottom;
	s16 x1, y1, x2, y2;

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = torch.buf.getw() - 1;
	top = 0;
	bottom = torch.buf.geth() - 1;

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

	hollowCircleNoClip(x,y,r,func,info);
}

void hollowCircleNoClip(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y, void *info), void *info) {
	s16 x1, y1, x2, y2;
	s16 cx = 0;
	s16 cy = r;
	s16 ocx = (s32) 0xffff;
	s16 ocy = (s32) 0xffff;
	s16 df = 1 - r;
	s16 d_e = 3;
	s16 d_se = -2 * r + 5;
	s16 xpcx, xmcx, xpcy, xmcy;
	s16 ypcy, ymcy, ypcx, ymcx;

	/*
	 * Sanity check radius
	 */
	if (r < 0)
		return;

	/*
	 * Special case for r=0 - draw a point
	 */
	if (r == 0) {
		func(x,y, info);
		return;
	}

	x1 = x - r;
	x2 = x + r;
	y1 = y - r;
	y2 = y + r;

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
				func(xmcx,ypcy, info);
				func(xpcx,ypcy, info);
				func(xmcx,ymcy, info);
				func(xpcx,ymcy, info);
			} else {
				func(xmcx,y, info);
				func(xpcx,y, info);
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				func(xmcy,ypcx, info);
				func(xpcy,ypcx, info);
				func(xmcy,ymcx, info);
				func(xpcy,ymcx, info);
			} else {
				func(xmcy,y, info);
				func(xpcy,y, info);
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
void filledCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y, void *info), void *info) {
	s16 left, right, top, bottom;
	int result;
	s16 x1, y1, x2, y2;
	s16 cx = 0;
	s16 cy = r;
	s16 ocx = (s32) 0xffff;
	s16 ocy = (s32) 0xffff;
	s16 df = 1 - r;
	s16 d_e = 3;
	s16 d_se = -2 * r + 5;
	s16 xpcx, xmcx, xpcy, xmcy;
	s16 ypcy, ymcy, ypcx, ymcx;

	/*
	 * Sanity check radius
	 */
	if (r < 0)
		return;

	/*
	 * Special case for r=0 - draw a point
	 */
	if (r == 0) {
		func(x, y, info);
		return;
	}

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = torch.buf.getw() - 1;
	top = 0;
	bottom = torch.buf.geth() - 1;

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
				hline(xmcx, xpcx, ypcy, func, info);
				hline(xmcx, xpcx, ymcy, func, info);
			} else {
				hline(xmcx, xpcx, y, func, info);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					hline(xmcy, xpcy, ymcx, func, info);
					hline(xmcy, xpcy, ypcx, func, info);
				} else {
					hline(xmcy, xpcy, y, func, info);
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

void bresenham(s16 x0, s16 y0, s16 x1, s16 y1, void (*func)(s16 x, s16 y, void *info), void *info) {
	bool steep = abs(y1 - y0) > abs(x1 - x0);
	if (steep) {
		s16 tmp = x0; x0 = y0; y0 = tmp;
		tmp = x1; x1 = y1; y1 = tmp;
	}
	if (x0 > x1) {
		s16 tmp = x0; x0 = x1; x1 = tmp;
		tmp = y0; y0 = y1; y1 = tmp;
	}
	s16 deltax = x1 - x0;
	s16 deltay = abs(y1-y0);
	s16 error = -(deltax + 1) / 2;
	s16 ystep = y0 < y1 ? 1 : -1;
	s16 y = y0;
	for (int x = x0; x < x1; x++) {
		if (steep) func(y, x, info);
		else func(x, y, info);
		error += deltay;
		if (error >= 0) {
			y += ystep;
			error -= deltax;
		}
	}
}

void extractComponents(u16 color, int &r, int &g, int &b) {
	r = color & 0x1f;
	g = (color & 0x3e0) >> 5;
	b = (color & 0x7c00) >> 10;
}
