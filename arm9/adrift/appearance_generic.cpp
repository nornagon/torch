#include "appearance_generic.h"
#include "adrift.h"

#include "entities/terrain.h"
#include "entities/creature.h"

bool isforgettable(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (!l->desc()->forgettable) return false;
	if (l->objects.head()) return false;
	return true;
}

void visible_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->creature) {
		*ch = l->creature->desc()->ch;
		*col = l->creature->desc()->color;
	} else if (l->objects.head()) {
		if (l->objects.head()->desc()->animation) {
			*ch = l->objects.head()->desc()->animation[l->objects.head()->quantity];
			*col = l->objects.head()->desc()->color;
		} else {
			*ch = l->objects.head()->desc()->ch;
			*col = l->objects.head()->desc()->color;
		}
	} else {
		*ch = l->desc()->ch;
		*col = l->desc()->color;
	}
}

void recalled_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->objects.head())
		*ch = l->objects.head()->desc()->ch,
		*col = l->objects.head()->desc()->color;
	else if (!terraindesc[l->type].forgettable)
		*ch = l->desc()->ch,
		*col = l->desc()->color;
	else
		*ch = ' ',
		*col = 0;
}
