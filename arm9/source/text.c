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
			offset[k] = i + 1;
		}
	}
	width[k] = i - offset[k];
}

int yoffset;

void text_clear() {
	int i;

	/* zap background */
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	for (i = 0; i < 256 * 256; i++)
		vram[i] = 0;

	yoffset = 1;
}

void text_render_blob(int xoffset, int yoffset, char *text, int textlen) {
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	
	int i, o = 0;
	for (i = 0; i < textlen; i++) {
		int y, c;
		c = text[i] - ' ';
		for (y = 0; y < CHAR_HEIGHT; y++) {
			int x, s = (y + 1) * bitmapwidth;
			u8*dest = (u8*)vram + o + xoffset + (256 * (y + yoffset));
			u8*src = (u8*)propfontBitmap + s + offset[c];
			int count = width[c];
			
			// XXX: alignment doom
			if ((int)dest & 1) {
				if ((int)src & 1) {
					// fix alignment by just copying the blank column
					src--;
					dest--;
					count++;
				} else dest--;
			} else if ((int)src & 1) src--;
			for (x = 0; x < (count + 2) / 2; x++) {
				*(u16*)dest = *(u16*)src;
				dest += 2; src += 2;
			}
		}

		/* move onward, remembering the space */
		/* XXX: two spaces due to alignment issues for now */
		o += width[c] + 2;
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

		xoffset += width[c] + 2;
	}
	
	text_render_blob(1, yoffset, text, strlen(text));
}

