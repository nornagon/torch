#ifndef RECALC_H
#define RECALC_H 1

#include "appearance_generic.h"
#include "blockmap.h"

// recalculates the appearance of the given cell, based on the implementations
// of appearance_visible and appearance_recalled.
void recalc(blockmap *block, s16 x, s16 y);
void refresh(blockmap *block);

#endif /* RECALC_H */
