#ifndef ADRIFT_H
#define ADRIFT_H 1

#ifdef NATIVE
#include "native.h"
#endif

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
#include "bresenstate.h"

#include "entities/terrain.h"

class Map {
	private:
		Cell *cells;
		s16 w, h;

	public:
		blockmap block;
		List<lightsource*> lights;

		Map() { cells = 0; w = h = 0; }
		Map(s16 w_, s16 h_) { resize(w_,h_); }

		void resize(s16 _w, s16 _h) {
			if (cells) delete [] cells;
			w = _w; h = _h;
			cells = new Cell[w*h];
		}
		void reset();

		inline Cell *at(s16 x, s16 y) {
			assert(x >= 0 && y >= 0 && x < w && y < w);
			return &cells[y*w+x];
		}

		inline bool solid(s16 x, s16 y) {
			Node<Object> o = at(x,y)->objects.top();
			while (o) {
				if (o->desc()->obstruction) return true;
				o = o.next();
			}
			return at(x,y)->desc()->solid;
		}

		inline bool occupied(s16 x, s16 y) {
			return at(x,y)->creature;
		}

		inline bool walkable(s16 x, s16 y) {
			return !(solid(x,y) || occupied(x,y));
		}

		inline bool contains(s16 x, s16 y) {
			return x >= 0 && x < w && y >= 0 && y < h;
		}

		inline s16 getw() { return w; }
		inline s16 geth() { return h; }
};

struct Projectile {
	Node<Object> obj;
	bresenstate st;
};

struct Animation {
	Node<Object> obj;
	s16 x, y;
	s16 frame;
};

struct Adrift {
	Adrift();
	Player player;
	Map map;

	List<Creature> monsters;
	List<Projectile> projectiles;
	List<Animation> animations;

	fov_settings_type *fov_light, *fov_sight;

	void spawn(u16 type, s16 x, s16 y);

	int cooldown;
};

extern Adrift game;

void recalc_visible(s16 x, s16 y);
void recalc_recall(s16 x, s16 y);

#endif /* ADRIFT_H */
