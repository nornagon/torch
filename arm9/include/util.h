#ifndef UTIL_H
#define UTIL_H 1

#include <stdlib.h>
#ifdef NATIVE
#include "native.h"
#else
#include <nds.h>
#endif

#ifndef abs
#define abs(x) ((x) < 0 ? -(x) : (x))
#endif

//---------------------------------------------------------------------------
// timers
#ifdef NATIVE
static inline void start_stopwatch() {}
static inline u16 read_stopwatch() { return 0; }
#else
static inline void start_stopwatch() {
	TIMER_CR(0) = TIMER_DIV_1 | TIMER_ENABLE;
}

static inline u16 read_stopwatch() {
	TIMER_CR(0) = 0;
	return TIMER_DATA(0);
}
#endif
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// numbers
// TODO: are min/max defined in the stdlib somewhere?
// (nornagon, later): I think they're in <algorithm>... eugh. Template overhead
// worth it?
static inline int min(int a, int b) {
	if (a < b) return a;
	return b;
}
static inline int max(int a, int b) {
	if (a > b) return a;
	return b;
}

// raw divide, you'll have to check DIV_RESULT32 and DIV_BUSY yourself.
static inline void div_32_32_raw(int32 num, int32 den) {
#ifdef NATIVE
	native_div_32_32_raw(num,den);
#else
	DIV_CR = DIV_32_32;

	while (DIV_CR & DIV_BUSY);

	DIV_NUMERATOR32 = num;
	DIV_DENOMINATOR32 = den;
#endif
}
// beware, if your numerator can't deal with being shifted left 12, you will
// lose bits off the left-hand side!
static inline int32 div_32_32(int32 num, int32 den) {
#ifdef NATIVE
	return num / den;
#else
	div_32_32_raw(num << 12, den);
	while (DIV_CR & DIV_BUSY);
	return DIV_RESULT32;
#endif
}
//---------------------------------------------------------------------------

// the manhattan distance between two points
static inline unsigned int manhdist(int x1, int y1, int x2, int y2) {
	return abs(x1-x2)+abs(y1-y2);
}

// Adjacency of P0 and P1
static inline bool adjacent(s32 x0, s32 y0, s32 x1, s32 y1) {
	return abs(x0-x1) <= 1 && abs(y0-y1) <= 1;
}

// distance squared
static inline unsigned int dist2(int x1, int y1, int x2, int y2) {
	x1 = abs(x1-x2); y1 = abs(y1-y2);
	return x1*x1+y1*y1;
}

static inline bool within(int x1, int y1, int x2, int y2, unsigned int distance) {
	return dist2(x1,y1,x2,y2) <= distance*distance;
}

#endif /* UTIL_H */
