#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

#include "engine.h"

#include "lightsource.h"
#include <malloc.h>

#include <stdio.h>
#include <nds.h>

#define abs(x) ((x) < 0 ? -(x) : (x))

void hline(s16 x0, s16 x1, s16 y, void (*func)(s16 x, s16 y)) {
	torch.buf.bounded(x1, y);
	if (x0 > x1) hline(x1, x0, y, func);
	for (; x0 <= x1; x0++)
		func(x0, y);
}

void hollowCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y)) {
	s16 left, right, top, bottom;
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
		func(x,y);
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
	do {
		if ((ocy != cy) || (ocx != cx)) {
			xpcx = x + cx;
			xmcx = x - cx;
			if (cy > 0) {
				ypcy = y + cy;
				ymcy = y - cy;
				func(xmcx,ypcy);
				func(xpcx,ypcy);
				func(xmcx,ymcy);
				func(xpcx,ymcy);
			} else {
				func(xmcx,y);
				func(xpcx,y);
			}
			ocy = cy;
			xpcy = x + cy;
			xmcy = x - cy;
			if (cx > 0) {
				ypcx = y + cx;
				ymcx = y - cx;
				func(xmcy,ypcx);
				func(xpcy,ypcx);
				func(xmcy,ymcx);
				func(xpcy,ymcx);
			} else {
				func(xmcy,y);
				func(xpcy,y);
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
void filledCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y))
{
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
		func(x, y);
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
				hline(xmcx, xpcx, ypcy, func);
				hline(xmcx, xpcx, ymcy, func);
			} else {
				hline(xmcx, xpcx, y, func);
			}
			ocy = cy;
		}
		if (ocx != cx) {
			if (cx != cy) {
				if (cx > 0) {
					ypcx = y + cx;
					ymcx = y - cx;
					hline(xmcy, xpcy, ymcx, func);
					hline(xmcy, xpcy, ypcx, func);
				} else {
					hline(xmcy, xpcy, y, func);
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

void bresenham(s16 x0, s16 y0, s16 x1, s16 y1, void (*func)(s16 x, s16 y)) {
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
		if (steep) func(y, x);
		else func(x, y);
		error += deltay;
		if (error >= 0) {
			y += ystep;
			error -= deltax;
		}
	}
}

#define DEF(type_) \
	static inline void SET_##type_(s16 x, s16 y) { \
		game.map.at(x,y)->type = T_##type_; \
		mapel *m = torch.buf.at(x,y); \
		m->recall = 0; m->col = celldesc[T_##type_].col; m->ch = celldesc[T_##type_].ch; \
		blockel *b = game.map.block.at(x,y); \
		b->opaque = celldesc[T_##type_].opaque; \
	}
DEF(TREE);
DEF(GROUND);
DEF(NONE);
DEF(GLASS);
DEF(WATER);
DEF(FIRE);
#undef DEF

void randwalk(s16 &x, s16 &y) {
	unsigned int a = rand4();
	switch (a & 3) {
		case 0: x++; break;
		case 1: x--; break;
		case 2: y++; break;
		case 3: y--; break;
	}
}

void haunted_grove(s16 cx, s16 cy) {
	int r0 = 7, r1 = 15;
	int w0 = 3, w1 = 4;

	unsigned int playerpos = ((rand32() & 0x1ff) / 8) * 8;
	for (unsigned int t = 0; t < 0x1ff; t += 8) {
		int r = rand32() % 5;
		int x = COS[t], y = SIN[t];
		bresenham(cx, cy, cx + ((x*r) >> 12), cy + ((y*r) >> 12), SET_WATER);
		if (t == playerpos) {
			unsigned int extra = rand8() % 3;
			game.player.x = cx + ((x*(5+extra)) >> 12);
			game.player.y = cy + ((y*(5+extra)) >> 12);
		}
	}

	unsigned int firepos = ((rand32() & 0x1ff) / 4) * 4;
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = (rand8() & 3) + r0;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w0)) >> 12), cy + ((y*(r+w0)) >> 12), SET_TREE);
		if (t == firepos) {
			int px = cx + ((x*(r0+w0+1)) >> 12),
			    py = cy + ((y*(r0+w0+1)) >> 12);
			SET_FIRE(px, py);
			lightsource *l = new_light(8<<12, (int)(0.9*(1<<12)), (int)(0.3*(1<<12)), (int)(0.1*(1<<12)));
			l->x = px<<12; l->y = py<<12;
			game.map.lights.push(l);
		}
	}
	for (unsigned int t = 0; t < 0x1ff; t += 2) {
		int a = rand8();
		int r = (a & 3) + r1; a >>= 2;
		int x = COS[t], y = SIN[t];
		if (t % 16 > (a & 3))
			bresenham(cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w1)) >> 12), cy + ((y*(r+w1)) >> 12), SET_TREE);
	}
	u16 k = rand16() & 0x1ff;
	int x = COS[k], y = SIN[k];
	bresenham(cx + ((x*r0) >> 12), cy + ((y*r0) >> 12),
	               cx + ((x*(r0+w0)) >> 12), cy + ((y*(r0+w0)) >> 12), SET_GROUND);
	k = rand16() & 0x1ff;
	x = COS[k]; y = SIN[k];
	bresenham(cx + ((x*r1) >> 12), cy + ((y*r1) >> 12),
	               cx + ((x*(r1+w1)) >> 12), cy + ((y*(r1+w1)) >> 12), SET_GROUND);
}

void drop_rock(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (rand32() % 10 == 0 && l->type == T_GROUND) {
		Node<Object> *on = Node<Object>::pool.request_node();
		on->data.type = J_ROCK;
		l->objects.push(on);
	}
}

void drop_rocks(s16 ax, s16 ay) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = 4;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(ax + ((x*r) >> 12), ay + ((y*r) >> 12),
			               ax + ((x*(r+4)) >> 12), ay + ((y*(r+4)) >> 12), drop_rock);
	}
}

#include "assert.h"

void generate_terrarium() {
	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	torch.buf.reset();
	torch.buf.cache.reset();

	for (int y = 0; y < torch.buf.getw(); y++)
		for (int x = 0; x < torch.buf.geth(); x++)
			SET_NONE(x,y);

	filledCircle(cx, cy, 60, SET_GROUND);
	hollowCircle(cx, cy, 60, SET_GLASS);

	haunted_grove(cx, cy);

	drop_rocks(cx, cy);

	Cell *l = game.map.at(game.player.x+1, game.player.y);
	Node<Creature> *cn = Node<Creature>::pool.request_node();
	cn->data.type = 0;
	l->creatures.push(cn);

	l = game.map.at(game.player.x-1, game.player.y);
	Node<Object> *on = Node<Object>::pool.request_node();
	on->data.type = 0;
	l->objects.push(on);

	game.map.block.refresh();
}
