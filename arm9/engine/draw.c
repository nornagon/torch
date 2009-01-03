#include "draw.h"
#include <nds/arm9/video.h>
#include "mem.h"
#include <string.h>

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
	memset((void*)backbuf, 0, (256*192*2));
}
void clss() {
	memset((void*)BG_BMP_RAM(0), 0, (256*192*4));
}

void drawcq(u32 x, u32 y, u32 c, u32 color) {
	drawch(backbuf, x, y, c, color);
}

void drawch(u16* mem, u32 x, u32 y, u32 c, u32 color) { // OPAQUE version (clobbers)
	// FIXME: I'm not quite sure how to tell gcc that I want it to put the
	// arguments to this in r{2..7} specifically. I assume that's what it'll do,
	// since when I compiled with -Os it complained that there were no low
	// registers in which it could put its arguments until I removed r{2..7} from
	// the clobber list. I'd like to specify those registers in the clobber list
	// (since I do in fact clobber them), but then gcc refuses to put the
	// arguments in those registers. So here's hoping gcc doesn't do anything
	// funny...
	asm (
			"adr r0, 2f\n"
			"bx r0\n"
			".align 4\n"
			".arm\n"
			"2:\n"
			"sub %2, %2, #32\n"         // c -= ' '
			"add %1, %0, %1, lsl #8\n"  // y = x+y*256
			"orr r8, %3, #32768\n"      // r8 <- col | BIT(15)
			"add r11, %5, %2, lsl #6\n"  // r11 <- &fontTiles[8*8*c]
			"add r12, %4, %1, lsl #1\n"  // r12 <- &backbuf[y*256+x]

			"mov r9, #7\n"            // we want to draw 8 lines

			"1:\n"
			"ldmia r11!, {r4, r10}\n"   // grab 8 bytes from the font

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
			"movs r10, r10, lsr #1\n"
			"movcs r2, r8\n"
			"movs r10, r10, lsr #8\n"
			"orrcs r2, r8, lsl #16\n"

			"eor r3, r3, r3\n"
			"movs r10, r10, lsr #8\n"
			"movcs r3, r8\n"
			"movs r10, r10, lsr #8\n"
			"orrcs r3, r8, lsl #16\n"

			"stm r12, {r0, r1, r2, r3}\n"
			"add r12, r12, #512\n"
			"subs r9, r9, #1\n"
			"bpl 1b\n"

			"adr r3, 3f + 1\n"
			"bx r3\n"
			".thumb\n"
			"3:\n"

			: /* no output */
			: "r"(x), "r"(y), "r"(c), "r"(color), "r"(mem), "r"(fontTiles)
			: "r0", "r1", "r10", "r11", "r12", "r8", "r9"
			);
}
