#ifndef MAP_H
#define MAP_H 1

#include "lightsource.h"
#include "list.h"
#include "blockmap.h"
#include "cell.h"
#include "object.h"
#include "creature.h"
#include "entities/terrain.h"
#include "datastream.h"
#include "bresenstate.h"

struct Projectile : public listable<Projectile> {
	Object *obj;
	bresenstate st;
};

struct Animation : public listable<Animation> {
	Object *obj;
	s16 x, y;
	s16 frame;
};

class Map {
	private:
		Cell *cells;
		s16 w, h;

	public:
		blockmap block;
		List<lightsource> lights;
		List<Creature> monsters;
		List<Projectile> projectiles;
		List<Animation> animations;

		Map() { cells = 0; w = h = 0; }
		Map(s16 w_, s16 h_) { resize(w_,h_); }
		~Map();

		void resize(s16 _w, s16 _h);
		void reset();

		inline Cell *at(s16 x, s16 y) {
			assert(x >= 0 && y >= 0 && x < w && y < h);
			return &cells[y*w+x];
		}

		inline bool solid(s16 x, s16 y) {
			Object *o = at(x,y)->objects.head();
			while (o) {
				if (o->desc()->obstruction) return true;
				o = o->next();
			}
			return at(x,y)->desc()->solid;
		}

		inline bool occupied(s16 x, s16 y) {
			return at(x,y)->creature;
		}

		inline bool walkable(s16 x, s16 y) {
			return !(solid(x,y) || occupied(x,y));
		}
		inline bool flyable(s16 x, s16 y) {
			return (!solid(x,y) || at(x,y)->desc()->flyable) && !occupied(x,y);
		}

		inline bool contains(s16 x, s16 y) {
			return x >= 0 && x < w && y >= 0 && y < h;
		}

		inline s16 getw() { return w; }
		inline s16 geth() { return h; }

		void spawn(u16 type, s16 x, s16 y);

		friend DataStream& operator <<(DataStream&, Map&);
		friend DataStream& operator >>(DataStream&, Map&);
};
DataStream& operator <<(DataStream&, Map&);
DataStream& operator >>(DataStream&, Map&);

#endif /* MAP_H */
