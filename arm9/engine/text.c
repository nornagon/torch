#include "propfont.h"
#include "text.h"
#include <string.h>
#include <nds.h>
#include <sys/iosupport.h>
#include <stdio.h>

/* font data */
#define NO_CHARS 94
#define CHAR_HEIGHT 8

unsigned short offset[NO_CHARS];
unsigned char width[NO_CHARS];
unsigned short bitmapwidth;

/* state for renderer */
unsigned int xoffset, yoffset;

void text_render_str(const char *str, int len);

/* this is infrasturcture for hooking ourselves up to fds */
int text_write(struct _reent *r, int fd, const char *ptr, int len) {
	text_render_str(ptr, len);
	return len;
}
const devoptab_t dotab_textout = { "con", 0, NULL, NULL, text_write, NULL, NULL, NULL };

void text_init() {
	unsigned int i, k;

	/* hook ourselves up to stdout, for convenience */
	devoptab_list[STD_OUT] = &dotab_textout;
	setvbuf(stdout, NULL , _IONBF, 0);

	/* video setup is done elsewhere */
	text_clear();

	/* work out the offset/width of each character */
	offset[0] = 0;
	k = 0;
	bitmapwidth = propfontBitmapLen / (CHAR_HEIGHT + 1);
	for (i = 0; i < bitmapwidth; i++) {
		if (propfontBitmap[i]) {
			width[k] = i - offset[k];
			k++;
			if (k == NO_CHARS) return;
			offset[k] = i + 1;
		}
	}
	width[k] = i - offset[k];
}

void text_clear() {
	int i;

	/* zap background */
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	for (i = 0; i < (256 * 256) / 2; i++)
		vram[i] = 0;

	xoffset = 0;
	yoffset = 1;
}

inline u16 colourPixel(u8 data, u8 fgcolor) {
	if (data) return fgcolor;
	else return 0;
}

void text_render_raw(int xoffset, int yoffset, const char *text, int textlen, u8 fgcolor) {
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
				*(u16*)dest = (*(u16*)dest & 0xFF) + (colourPixel(*src, fgcolor) << 8);
				dest+=2;
				src++;
				count--;
			}

			// src might be unaligned, so we read two u8s and merge them :-(
			for (x = 0; x < count / 2; x++) {
				*(u16*)dest = colourPixel(*src, fgcolor) + (colourPixel(*(src + 1), fgcolor) << 8);
				dest += 2; src += 2;
			}

			// if we have one last, lonely byte, copy that too
			if (x != (count + 1) / 2) {
				// no need to preserve the rest of the contents!
				*(u16*)dest = colourPixel(*src, fgcolor);
			}
		}

		/* move onward, remembering the space */
		o += width[c];
	}
}

void text_scroll() {
	int i;
	u16 *vram = (u16 *)BG_BMP_RAM_SUB(0);
	
	yoffset -= CHAR_HEIGHT + 2;

	/* fuzzie is lazy. DMA is easy. whoo! */
	/* XXX: this is probably dumb */
	DMA_SRC(3) = (unsigned int)vram + (256 * (CHAR_HEIGHT + 2));
	DMA_DEST(3) = (unsigned int)vram;
	DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | (256 * (192 - (CHAR_HEIGHT + 2))) << 1;
	while (dmaBusy(3));

	/* wipe rest of screen */
	for (i = 256 * (192 - (CHAR_HEIGHT + 2)) << 1; i < (256 * 256) / 2; i++)
		vram[i] = 0;
}

void text_render(const char *text) {
	text_render_str(text, strlen(text));
}

void text_render_str(const char *text, int len) {
	int i, xusage = xoffset, lastgoodlen = 0;
	u8 fgcolor = 1;

	for (i = 0; i < len; i++) {
		int c = text[i] - ' ';
		if (c < 0 || c >= NO_CHARS) c = '?' - ' ';

		/* if we're encountering a word break (space or newline), change the location of the 'last word' */
		if (c == 0 || text[i] == '\n') lastgoodlen = i;
		
		/* if there's a colour change here, render the text so far and change colour */
		if (text[i] == '\1') {
			lastgoodlen = i;
			text_render_raw(xoffset, yoffset, text, lastgoodlen, fgcolor);
			fgcolor = text[i + 1];
			
			text = text + lastgoodlen + 2;
			len -= lastgoodlen + 2;
			i = -1;

			xoffset = xusage;
			continue;
		}

		/* if there's a newline or we ran out of space here, render the text up to the last word and move to the next line */
		if (text[i] == '\n' || xusage + width[c] + 2 > 256) {
			while (yoffset + CHAR_HEIGHT + 2 > 192) text_scroll();
			if (lastgoodlen == 0) lastgoodlen = i; /* if there's some crazy-long word which takes up a whole line, render the whole thing */
			text_render_raw(xoffset, yoffset, text, lastgoodlen, fgcolor);
			
			if (text[i] == '\n') { lastgoodlen += 1; }
			if (text[lastgoodlen] == ' ') lastgoodlen += 1;
			text = text + lastgoodlen;
			len -= lastgoodlen;
		
			xoffset = 0;
			xusage = 0;
			yoffset += CHAR_HEIGHT + 2;
			
			i = -1;
			continue;
		}

		xusage += width[c];
	}
	
	/* render any leftover text */
	text_render_raw(xoffset, yoffset, text, len, fgcolor);
	xoffset = xusage;
}

