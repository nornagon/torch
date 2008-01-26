#ifndef DRAW_H
#define DRAW_H 1

#include <nds.h>

#ifdef __cplusplus
extern "C" {
#endif

// pointer to vram that is not being drawn currently
extern u16* backbuf;

// swap the back buffer out to be displayed next drawing time
// NB: you shouldn't call this during VDRAW, otherwise you'll get tearing
void swapbufs();

// clear the backbuf
void cls();

// clear the backbuf and the displayed screen. VDRAW-unfriendly.
void clss();

void drawch(u16 *mem, u32 x, u32 y, u32 c, u32 color);

void drawcq(u32 x, u32 y, u32 c, u32 color);

#ifdef __cplusplus
}
#endif

#endif /* DRAW_H */

