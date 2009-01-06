#ifndef DIRECTION_H
#define DIRECTION_H 1

// a direction can be stored in 4 bits.
typedef unsigned char DIRECTION;
#define D_NONE  0
#define D_NORTH 1
#define D_SOUTH 2
#define D_EAST  4
#define D_WEST  8
#define D_NORTHEAST (D_NORTH|D_EAST)
#define D_SOUTHEAST (D_SOUTH|D_EAST)
#define D_NORTHWEST (D_NORTH|D_WEST)
#define D_SOUTHWEST (D_SOUTH|D_WEST)

// x/y deltas for each direction as LUTs
static const int D_DX[] = {
	 0,  0,  0,  0, // 0, N, S, N|S
	 1,  1,  1,  1, // E, N|E, S|E, N|S|E
	-1, -1, -1, -1, // W, N|W, S|W, N|S|W
	 0,  0,  0,  0  // E|W, E|W|N, E|W|S, E|W|N|S
};

static const int D_DY[] = {
	0, -1, 1, 0, // 0, N, S, N|S
	0, -1, 1, 0, // E, N|E, S|E, N|S|E
	0, -1, 1, 0, // W, N|W, S|W, N|S|W
	0, -1, 1, 0  // E|W, E|W|N, E|W|S, E|W|N|S
};

// These are for lighting purposes only. A cell being lit from the north-west is
// different from a cell being lit from the north and the west (due to opaque
// things). D_NORTHWEST in lighting refers to the northwestern *corner* of the
// cell, but D_NORTH_AND_WEST refers to the northern and the western sides being
// lit separately.
#define D_BOTH 0x10
#define D_NORTH_AND_EAST (D_NORTHEAST|D_BOTH)
#define D_SOUTH_AND_EAST (D_SOUTHEAST|D_BOTH)
#define D_NORTH_AND_WEST (D_NORTHWEST|D_BOTH)
#define D_SOUTH_AND_WEST (D_SOUTHWEST|D_BOTH)

// the direction from (tx,ty) to (sx,sy)
// or: the faces lit on (tx,ty) by a source at (sx,sy)
static inline DIRECTION direction(int sx, int sy, int tx, int ty) {
	if (sx < tx) {
		if (sy < ty) return D_NORTHWEST;
		if (sy > ty) return D_SOUTHWEST;
		return D_WEST;
	}
	if (sx > tx) {
		if (sy < ty) return D_NORTHEAST;
		if (sy > ty) return D_SOUTHEAST;
		return D_EAST;
	}
	if (sy < ty) return D_NORTH;
	if (sy > ty) return D_SOUTH;
	return D_NONE;
}

#endif /* DIRECTION_H */

