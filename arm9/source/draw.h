#ifndef DRAW_H
#define DRAW_H 1

#include <nds.h>

extern u16* backbuf;

// swap the back buffer out to be displayed next drawing time
// NB: you shouldn't call this during VDRAW, otherwise you'll get tearing
void swapbufs();

// clear the backbuf
void cls();

// clear the backbuf and the displayed screen. VDRAW-unfriendly.
void clss();

// Draw a character at the given (x,y) pixel location in the given colour.
void drawc(u32 x, u32 y, u8 c, u16 color);

// Same as above, but faster and doesn't consider transparency.
void drawcq(u32 x, u32 y, u32 c, u32 color);

#endif /* DRAW_H */

