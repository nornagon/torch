#include "appearance_generic.h"
#include "adrift.h"

#include "entities/terrain.h"
#include "entities/creature.h"

bool isforgettable(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (!terraindesc[l->type].forgettable) return false;
	if (l->objects.top()) return false;
	return true;
}

void visible_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->creature) {
		*ch = creaturedesc[l->creature->type].ch;
		*col = creaturedesc[l->creature->type].color;
	} else if (l->objects.top()) {
		if (l->objects.top()->desc().animation) {
			*ch = l->objects.top()->desc().animation[l->objects.top()->quantity];
			*col = l->objects.top()->desc().color;
		} else {
			*ch = l->objects.top()->desc().ch;
			*col = l->objects.top()->desc().color;
		}
	} else {
		*ch = terraindesc[l->type].ch;
		*col = terraindesc[l->type].color;
	}
}

void recalled_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->objects.top())
		*ch = objectdesc[l->objects.top()->type].ch,
		*col = objectdesc[l->objects.top()->type].color;
	else if (!terraindesc[l->type].forgettable)
		*ch = terraindesc[l->type].ch,
		*col = terraindesc[l->type].color;
	else
		*ch = ' ',
		*col = 0;
}
