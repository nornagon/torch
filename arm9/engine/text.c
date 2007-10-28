#include "propfont.h"
#include "text.h"
#include <string.h>
#include <nds.h>

#define NO_CHARS 94
#define CHAR_HEIGHT 8

unsigned int offset[NO_CHARS];
unsigned int width[NO_CHARS];
unsigned int bitmapwidth;

void text_init() {
	unsigned int i, k;
	/* video setup is done elsewhere */
	text_clear();

	/* work out the offset/width of each character */
	offset[0] = 0;
	k = 0;
	bitmapwidth = propfontBitmapLen / (CHAR_HEIGHT + 1);
	for (i = 0; i < bitmapwidth; i++) {
		if (propfontBitmap[i]) {
			if (offset[k] == i) { offset[k] = i + 1; continue; } /* XXX: because nornagon's input file is broken */
			width[k] = i - offset[k];
			k++;
			if (k == NO_CHARS) return;
			offset[k] = i;
		}
	}
	width[k] = i - offset[k];
}

int yoffset;

void text_clear() {
	int i;

	/* zap background */
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	for (i = 0; i < (256 * 256) / 2; i++)
		vram[i] = 0;

	yoffset = 1;
}

void text_render_blob(int xoffset, int yoffset, char *text, int textlen) {
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	
	int i, o = 0;
	for (i = 0; i < textlen; i++) {
		int y, c;
		c = text[i] - ' ';
		if (c < 0 || c >= NO_CHARS) c = '?' - ' ';

		/* render a single character */
		for (y = 0; y < CHAR_HEIGHT; y++) {
			int x, s = (y + 1) * bitmapwidth;
			/* dest points into vram, it's where we're going to write this row to */
			u8*dest = (u8*)vram + o + xoffset + (256 * (y + yoffset));
			/* src is the location of the row of bitmap data */
			u8*src = (u8*)propfontBitmap + s + offset[c];
			/* this is how many bytes we're going to copy - the width of the bitmap data */
			int count = width[c];
			
			// we can't do unaligned writes into vram, so do trickery to copy a first unaligned byte if needed
			if ((int)dest & 1) {
				dest--;
				*(u16*)dest = (*(u16*)dest & 0xFF) + ((u16)(*src) << 8);
				dest+=2;
				src++;
				count--;
			}

			// src might be unaligned, so we read two u8s and merge them :-(
			for (x = 0; x < count / 2; x++) {
				*(u16*)dest = ((u16)(*src)) + (u16)(*(src + 1) << 8);
				dest += 2; src += 2;
			}

			// if we have one last, lonely byte, copy that too
			if (x != (count + 1) / 2) {
				// no need to preserve the rest of the contents!
				*(u16*)dest = *src;
			}
		}

		/* move onward, remembering the space */
		o += width[c];
	}
}

void text_render(char *text) {
	int i, xoffset = 0, lastgoodlen = 0;

	for (i = 0; i < strlen(text); i++) {
		int c = text[i] - ' ';
		if (c < 0 || c >= NO_CHARS) c = '?' - ' ';
		if (c == 0 || text[i] == '\n') lastgoodlen = i;

		if (text[i] == '\n' || xoffset + width[c] + 2 > 256) {
			text_render_blob(1, yoffset, text, lastgoodlen);
			
			if (text[i] == '\n') { lastgoodlen += 1; }
			if (text[lastgoodlen] == ' ') lastgoodlen += 1;
			text = text + lastgoodlen;
			
			xoffset = 0;
			yoffset += CHAR_HEIGHT + 2;
			
			i = -1;
			continue;
		}

		xoffset += width[c];
	}
	
	text_render_blob(1, yoffset, text, strlen(text));
}

