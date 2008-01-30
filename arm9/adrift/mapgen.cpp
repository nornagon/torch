#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

#include "generic.h"
#include <malloc.h>

#include <stdio.h>

#define abs(x) ((x) < 0 ? -(x) : (x))

void hline(Map *map, s32 x0, s32 x1, s32 y, void (*func)(Cell*)) {
	map->bounded(x1, y);
	if (x0 > x1) hline(map, x1, x0, y, func);
	for (; x0 <= x1; x0++)
		func(map->at(x0, y));
}

void hollowCircle(Map *map, s32 x, s32 y, s32 r, void (*func)(Cell*)) {
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
		func(map->at(x,y));
		return;
	}

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = map->w - 1;
	top = 0;
	bottom = map->h - 1;

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
				func(map->at(xmcx,ypcy));
				func(map->at(xpcx,ypcy));
				func(map->at(xmcx,ymcy));
				func(map->at(xpcx,ymcy));
			} else {
				func(map->at(xmcx,y));
				func(map->at(xpcx,y));
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				func(map->at(xmcy,ypcx));
				func(map->at(xpcy,ypcx));
				func(map->at(xmcy,ymcx));
				func(map->at(xpcy,ymcx));
			} else {
				func(map->at(xmcy,y));
				func(map->at(xpcy,y));
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
void filledCircle(Map *map, s32 x, s32 y, s32 r, void (*func)(Cell*))
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
		func(map->at(x, y));
		return;
	}

	/*
	 * Get clipping boundary
	 */
	left = 0;
	right = map->w - 1;
	top = 0;
	bottom = map->h - 1;

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
				hline(map, xmcx, xpcx, ypcy, func);
				hline(map, xmcx, xpcx, ymcy, func);
			} else {
				hline(map, xmcx, xpcx, y, func);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					hline(map, xmcy, xpcy, ymcx, func);
					hline(map, xmcy, xpcy, ypcx, func);
				} else {
					hline(map, xmcy, xpcy, y, func);
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

void bresenham(Map *map, s32 x0, s32 y0, s32 x1, s32 y1, void (*func)(Cell*)) {
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
		if (steep) func(map->at(y, x));
		else func(map->at(x, y));
		error += deltay;
		if (error >= 0) {
			y += ystep;
			error -= deltax;
		}
	}
}

void tree(Cell *cell) {
	cell->type = T_TREE;
	cell->ch = '*';
	cell->col = RGB15(4,31,1);
	cell->opaque = true;
	cell->forgettable = false;
}

void ground(Cell *cell) {
	cell->type = T_GROUND;
	cell->ch = '.';
	cell->col = RGB15(17,9,6);
	cell->opaque = false;
	cell->forgettable = true;
}

void glass(Cell *cell) {
	cell->type = T_GLASS;
	cell->ch = '/';
	cell->col = RGB15(4,12,30);
	cell->opaque = false;
	cell->forgettable = false;
}

void null(Cell *cell) {
	cell->type = T_NONE;
	cell->ch = ' ';
	cell->col = 0;
	cell->opaque = false;
	cell->forgettable = true;
}

void randwalk(s32 &x, s32 &y) {
	unsigned int a = genrand_int32();
	switch (a & 3) {
		case 0: x++; break;
		case 1: x--; break;
		case 2: y++; break;
		case 3: y--; break;
	}
}

void haunted_grove(Map *map, s32 cx, s32 cy) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		u32 a = genrand_int32();
		int r = 5 + (a & 3);
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(map, cx + ((x*r) >> 12), cy + ((y*r) >> 12), cx + ((x*(r+2)) >> 12), cy + ((y*(r+2)) >> 12), tree);
	}
	for (unsigned int t = 0; t < 0x1ff; t += 2) {
		u32 a = genrand_int32();
		int r = 15 + (a & 3); a >>= 2;
		int x = COS[t], y = SIN[t];
		if (t % 16 > (a & 3))
			bresenham(map, cx + ((x*r) >> 12), cy + ((y*r) >> 12), cx + ((x*(r+4)) >> 12), cy + ((y*(r+4)) >> 12), tree);
	}
}

void generate_terrarium(Map *map) {
	s32 cx = map->w/2, cy = map->h/2;

	map->reset();
	map->reset_cache();

	for (int y = 0; y < map->h; y++)
		for (int x = 0; x < map->w; x++)
			null(map->at(x,y));

	filledCircle(map, cx, cy, 60, ground);
	hollowCircle(map, cx, cy, 60, glass);

	haunted_grove(map, cx, cy);

	map->pX = map->w/2;
	map->pY = map->h/2;

	refresh_blockmap(map);
}
