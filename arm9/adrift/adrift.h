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

class Map {
	private:
		Cell *cells;
		s16 w, h;

	public:
		blockmap block;
		List<lightsource*> lights; // TODO: hrm, should this be private? how should process_lights work?

		Map() { cells = 0; w = h = 0; }
		Map(s16 w, s16 h) { resize(w,h); }

		void resize(s16 _w, s16 _h) {
			if (cells) delete [] cells;
			w = _w; h = _h;
			cells = new Cell[w*h];
		}
		Cell *at(s16 x, s16 y) {
			return &cells[y*w+x];
		}
};

struct Player {
	List<Object> bag;
	Node<Creature> *obj;
	lightsource *light;

	s16 x, y;
};

struct Adrift {
	Adrift();
	Player player;
	Map map;

	fov_settings_type *fov_light, *fov_sight;

	int cooldown;
};

extern Adrift game;

#endif /* ADRIFT_H */
