#ifndef ADRIFT_H
#define ADRIFT_H 1

#include "light.h"
#include "fov.h"
#include "map.h"
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
		List<light_t*> lights; // TODO: hrm, should this be private? how should process_lights work?

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

		void add_light(light_t *l);
		void remove_light(light_t *l);
};

struct Player {
	List<Object> bag;
	Node<Creature> *obj;
	light_t *light;

	s16 x, y;
};

struct Adrift {
	Adrift();
	Player player;
	Map map;

	fov_settings_type *fov_light, *fov_sight;

	unsigned int frm;
};

extern Adrift game;

#endif /* ADRIFT_H */
