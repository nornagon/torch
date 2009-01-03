#include "map.h"

Map::~Map() {
	monsters.delete_all();
	lights.delete_all();
	animations.delete_all();
	projectiles.delete_all();
	if (cells) delete [] cells;
}

void Map::spawn(u16 type, s16 x, s16 y) {
	Creature* c = new Creature(type);
	c->setPos(x,y);
	assert(!occupied(x,y));
	at(x,y)->creature = c;
	monsters.push(c);
}

void Map::reset() {
	for (s16 y = 0; y < h; y++)
		for (s16 x = 0; x < w; x++)
			at(x,y)->reset();
	monsters.delete_all();
	lights.delete_all();
	animations.delete_all();
	projectiles.delete_all();
}

void Map::resize(s16 _w, s16 _h) {
	if (cells) delete [] cells;
	w = _w; h = _h;
	cells = new Cell[w*h];
	block.resize(w,h);
	reset();
}

DataStream& operator <<(DataStream& s, Map &m) {
	s << m.w;
	s << m.h;
	s << m.lights;
	s << m.monsters;
	for (int y = 0; y < m.h; y++) {
		for (int x = 0; x < m.w; x++) {
			s << *m.at(x,y);
		}
	}
	return s;
}
DataStream& operator >>(DataStream& s, Map &m) {
	s >> m.w;
	s >> m.h;
	m.resize(m.w, m.h);
	s >> m.lights;
	s >> m.monsters;
	for (int y = 0; y < m.h; y++) {
		for (int x = 0; x < m.w; x++) {
			s >> *m.at(x,y);
			if (m.at(x,y)->desc()->opaque)
				m.block.at(x,y)->opaque = true;
		}
	}
	for (Creature *k = m.monsters.head(); k; k = k->next()) {
		m.at(k->x, k->y)->creature = k;
	}
	m.block.refresh_blocked_from();
	return s;
}

