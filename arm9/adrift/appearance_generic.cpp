#include "appearance_generic.h"
#include "adrift.h"

bool isforgettable(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (!celldesc[l->type].forgettable) return false;
	if (l->objects.top()) return false;
	return true;
}

void visible_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->creature)
		*ch = creaturedesc[l->creature->type].ch,
		*col = creaturedesc[l->creature->type].col;
	else if (l->objects.top())
		*ch = objdesc[l->objects.top()->type].ch,
		*col = objdesc[l->objects.top()->type].col;
	else
		*ch = celldesc[l->type].ch,
		*col = celldesc[l->type].col;
}

void recalled_appearance(s16 x, s16 y, u16 *ch, u16 *col) {
	Cell *l = game.map.at(x,y);
	if (l->objects.top())
		*ch = objdesc[l->objects.top()->type].ch,
		*col = objdesc[l->objects.top()->type].col;
	else if (!celldesc[l->type].forgettable)
		*ch = celldesc[l->type].ch,
		*col = celldesc[l->type].col;
	else
		*ch = ' ',
		*col = 0;
}
