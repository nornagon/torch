#ifndef APPEARANCE_GENERIC_H
#define APPEARANCE_GENERIC_H 1

#include <nds/jtypes.h>

bool isforgettable(s16 x, s16 y);
void visible_appearance(s16 x, s16 y, u16 *ch, u16 *col);
void recalled_appearance(s16 x, s16 y, u16 *ch, u16 *col);

#endif /* APPEARANCE_GENERIC_H */
