#ifndef CACHE_H
#define CACHE_H 1

#include "direction.h"
#include <nds.h>

// a cache_t corresponds to a tile currently being shown on the screen. It's
// used for holding information about what pixels are already there on the
// screen, for purposes of not having to draw it again.
struct Cache {
	int32 lr,lg,lb, // light colour
	      last_lr, last_lg, last_lb;
	int32 last_light;
	u16 last_col_final; // the last colour we *drew* (including lights)
	u16 last_col; // the last colour we received in as the colour of the cell
	u8 dirty : 2; // if a cell is marked dirty, it gets drawn. (and dirty gets decremented.)
	bool was_visible : 1;

	// which direction is the player seeing this cell from?
	DIRECTION seen_from : 5;

	int16 light; // total intensity of the light falling on this cell
};

#endif /* CACHE_H */
