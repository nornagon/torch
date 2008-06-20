#ifndef GFXPRIMITIVES_H
#define GFXPRIMITIVES_H 1

#include <nds/jtypes.h>

void hline(s16 x0, s16 x1, s16 y, void (*func)(s16 x, s16 y));
void randwalk(s16 &x, s16 &y);

void hollowCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y));
void filledCircle(s16 x, s16 y, s16 r, void (*func)(s16 x, s16 y));
void bresenham(s16 x0, s16 y0, s16 x1, s16 y1, void (*func)(s16 x, s16 y));

#endif /* GFXPRIMITIVES_H */
