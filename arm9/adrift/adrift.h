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
#include "bresenstate.h"

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
			return at(x,y)->creature;
		}

		inline bool walkable(s16 x, s16 y) {
			return !(solid(x,y) || occupied(x,y));
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
