#include "draw.h"
#include <nds/arm9/video.h>
#include "mem.h"

#include "font.h"

u16* backbuf = (u16*)BG_BMP_RAM(0);
void swapbufs() {
	if (backbuf == (u16*)BG_BMP_RAM(0)) {
		BGCTRL[3] = BG_BMP16_256x256 | BG_BMP_BASE(0);
		backbuf = (u16*)BG_BMP_RAM(6);
	} else {
		BGCTRL[3] = BG_BMP16_256x256 | BG_BMP_BASE(6);
		backbuf = (u16*)BG_BMP_RAM(0);
	}
}

void cls() {
	memset32((void*)backbuf, 0, (256*192*2)/4);
}
void clss() {
	memset32((void*)BG_BMP_RAM(0), 0, (256*192*4)/4);
}

void drawcq(u32 x, u32 y, u32 c, u32 color) {
	drawch(backbuf, x, y, c, color);
}

void drawch(u16* mem, u32 x, u32 y, u32 c, u32 color) { // OPAQUE version (clobbers)
	asm (
			"adr r7, 2f\n"
			"bx r7\n"
			".align 4\n"
			".arm\n"
			"2:\n"
			"sub %2, %2, #32\n"         // c -= ' '
			"add %1, %0, %1, lsl #8\n"  // y = x+y*256
			"orr r8, %3, #32768\n"      // r8 <- col | BIT(15)
			"add r6, %5, %2, lsl #6\n"  // r6 <- &fontTiles[8*8*c]
			"add r7, %4, %1, lsl #1\n"  // r7 <- &backbuf[y*256+x]

			"mov r9, #7\n"            // we want to draw 8 lines

			"loop:\n"
			"ldmia r6!, {r4, r5}\n"   // grab 8 bytes from the font

			"eor r0, r0, r0\n"
			"movs r4, r4, lsr #1\n"   // r4 >> 1, C flag set if that bit was 1
			"movcs r0, r8\n"          // if C flag was set, colour low r0
			"movs r4, r4, lsr #8\n"   // the next possible 1 is 8 bits to the left
			"orrcs r0, r8, lsl #16\n" // if C flag was set, colour high r0

			"eor r1, r1, r1\n"
			"movs r4, r4, lsr #8\n"
			"movcs r1, r8\n"
			"movs r4, r4, lsr #8\n"
			"orrcs r1, r8, lsl #16\n"

			"eor r2, r2, r2\n"
			"movs r5, r5, lsr #1\n"
			"movcs r2, r8\n"
			"movs r5, r5, lsr #8\n"
			"orrcs r2, r8, lsl #16\n"

			"eor r3, r3, r3\n"
			"movs r5, r5, lsr #8\n"
			"movcs r3, r8\n"
			"movs r5, r5, lsr #8\n"
			"orrcs r3, r8, lsl #16\n"

			"stm r7, {r0, r1, r2, r3}\n"
			"add r7, r7, #512\n"
			"subs r9, r9, #1\n"
			"bpl loop\n"

			"adr r3, 3f + 1\n"
			"bx r3\n"
			".thumb\n"
			"3:\n"

			: /* no output */
			: "r"(x), "r"(y), "r"(c), "r"(color), "r"(mem), "r"(fontTiles)
			: "r0", "r1", "r5", "r6", "r7", "r8", "r9"
			);
}
