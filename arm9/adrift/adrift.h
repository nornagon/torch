#ifndef ADRIFT_H
#define ADRIFT_H 1

#include "lightsource.h"
#include "fov.h"
#include "buf.h"
#include "list.h"
#include "blockmap.h"
#include "creature.h"
#include "cell.h"
#include "object.h"
#include "player.h"
#include "util.h"

class Map {
	private:
		Cell *cells;
		s16 w, h;

	public:
		blockmap block;
		List<lightsource*> lights;

		Map() { cells = 0; w = h = 0; }
		Map(s16 w, s16 h) { resize(w,h); }

		void resize(s16 _w, s16 _h) {
			if (cells) delete [] cells;
			w = _w; h = _h;
			cells = new Cell[w*h];
		}
		inline Cell *at(s16 x, s16 y) {
			return &cells[y*w+x];
		}

		inline bool solid(s16 x, s16 y) {
			return celldesc[at(x,y)->type].solid;
		}

		inline bool occupied(s16 x, s16 y) {
			return !at(x,y)->creatures.empty();
		}

		inline bool walkable(s16 x, s16 y) {
			return !(solid(x,y) || occupied(x,y));
		}
};

struct bresenstate {
	bool steep, reversed;
	s16 deltax, deltay, error, ystep;
	s16 x0, y0, x1, y1;
	s16 x, y;
	bresenstate(s16 x0_, s16 y0_, s16 x1_, s16 y1_) {
		reset(x0_,y0_,x1_,y1_);
	}
	bresenstate() {}
	void reset(s16 x0_, s16 y0_, s16 x1_, s16 y1_) {
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
	void step() {
		error += deltay;
		if (error >= 0) {
			y += ystep;
			error -= deltax;
		}
		x++;
	}
	s16 posx() {
		if (reversed) {
			return destx() - (steep?y:x) + (steep?y1:x1);
		} else {
			if (steep) return y; return x;
		}
	}
	s16 posy() {
		if (reversed) {
			return desty() - (steep?x:y) + (steep?x1:y1);
		} else {
			if (steep) return x; return y;
		}
	}
	s16 destx() {
		if (reversed) {
			if (steep) return y0; return x0;
		} else {
			if (steep) return y1; return x1;
		}
	}
	s16 desty() {
		if (reversed) {
			if (steep) return x0; return y0;
		} else {
			if (steep) return x1; return y1;
		}
	}
};

struct Projectile {
	Node<Object> *obj;
	Object *object() { return &obj->data; }
	bresenstate st;
};

struct Adrift {
	Adrift();
	Player player;
	Map map;

	List<Projectile> projectiles;

	fov_settings_type *fov_light, *fov_sight;

	int cooldown;
};

extern Adrift game;

void recalc_visible(s16 x, s16 y);
void recalc_recall(s16 x, s16 y);

#endif /* ADRIFT_H */
