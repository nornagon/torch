#include "propfont.h"
#include "text.h"
#include <string.h>
#include <nds.h>
#include <sys/iosupport.h>
#include <stdio.h>
#include "mem.h"

/* font data */
#define NO_CHARS 94
#define CHAR_HEIGHT 8
unsigned short offset[NO_CHARS];
unsigned char width[NO_CHARS];
unsigned short bitmapwidth;

#define TEXT_END (192-9)

/* state for renderer */
#define TEXT_BUFFER_SIZE (100 * 40) /* ought to be about two screens worth */
#define DEFAULT_COLOR 0xFFFF
unsigned int xoffset, yoffset;
u16 fgcolor;
char text_buffer[TEXT_BUFFER_SIZE];
int renderconsole, keepstate;

void text_display_setup();
void sub_clear();

/* this is infrasturcture for hooking ourselves up to fds */
void text_render_str(const char *str, int len);
int text_write(struct _reent *r, int fd, const char *ptr, int len) {
	text_render_str(ptr, len);
	return len;
}
const devoptab_t dotab_textout = { "con", 0, NULL, NULL, text_write, NULL, NULL, NULL };

unsigned char widthof(int c) {
	return width[c];
}

int textwidth(const char *str) {
	int totalwidth = 0;
	while (*str) {
		totalwidth += width[*str - ' '];
		str++;
	}
	return totalwidth;
}

/* initialise the text renderer */
void text_init() {
	unsigned int i, k;

	renderconsole = 1;
	keepstate = 1;

	fgcolor = DEFAULT_COLOR;

	/* hook ourselves up to stdout, for convenience */
	devoptab_list[STD_OUT] = &dotab_textout;
	setvbuf(stdout, NULL , _IONBF, 0);

	text_display_setup();

	/* work out the offset/width of each character */
	/* XXX: this could be precalculated with a clever build system */
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

u16 *subfrontbuf;
void text_display_setup() {
	// setup the sub screen for our text output code
	videoSetModeSub( MODE_3_2D | DISPLAY_BG3_ACTIVE );
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	// bitmap mode
	SUB_BG3_CR = BG_BMP16_256x256 | BG_BMP_BASE(0);
	subfrontbuf = (u16*)BG_BMP_RAM_SUB(0);
	// no rotation, no scale
	SUB_BG3_XDY = 0;
	SUB_BG3_XDX = 1 << 8;
	SUB_BG3_YDX = 0;
	SUB_BG3_YDY = 1 << 8;

	text_console_clear();
}

/* blank the screen that the text renderer is using */
void text_display_clear() {
	sub_clear();

	xoffset = 0;
	yoffset = 1;
}

/* disable the text renderer console (it will remember state, but not render) */
void text_console_disable() {
	renderconsole = 0;
}

/* enable the text renderer console, re-rendering */
void text_console_enable() {
	renderconsole = 1;
	text_console_rerender();
}

/* reset the text renderer, making it forget all text */
void text_console_clear() {
	text_display_clear();

	text_buffer[0] = '\n';
	text_buffer[1] = 0;
}

/* re-render the text renderer's state, for example after using the text for something else, or scrolling around */
void text_console_rerender() {
	text_display_clear();

	char *s = strchr(text_buffer, '\n');
	if (!s) return; /* no renderable data */

	/* XXX: this doesn't account for scrolling */
	keepstate = 0;
	int newlines = 0;
	int k;
	for (k = strlen(s)-1; k > 0 && newlines < 20; k--) {
		if (s[k] == '\n' && s[k-2] != '\1' && s[k-1] != '\1') newlines++;
	}
	s = &s[k];
	s = s + 1;
	text_render_str(s, strlen(s));
	keepstate = 1;
}

/* render a piece of text onto the text renderer console, storing it in state for future use */
void text_console_render(const char *text) {
	text_render_str(text, strlen(text));
}

/*** internal functions ***/

void sub_clear() {
	memset((void*)subfrontbuf, 0, (256*192*2));
}

void append_console_state(const char *text, int len) {
	/* XXX: meh, this isn't great */
	int bs = strlen(text_buffer);
	char *b;
	if (bs + len + 1 > TEXT_BUFFER_SIZE) {
		b = text_buffer + bs - (len + 1);
		memmove(text_buffer, text_buffer + (len + 1), bs - (len + 1));
	} else {
		b = text_buffer + bs;
	}
	memmove(b, text, len);
	b[len] = 0;
}

inline u16 colourPixel(u8 data, u16 fgcolor) {
	if (data) return fgcolor;
	else return 0;
}

void text_render_raw(int xoffset, int yoffset, const char *text, int textlen, u16 fgcolor) {
	int i, o = 0;
	for (i = 0; i < textlen; i++) {
		int y, c;
		c = text[i] - ' ';
		if (c < 0 || c >= NO_CHARS) c = '?' - ' ';

		/* render a single character */
		for (y = 0; y < CHAR_HEIGHT; y++) {
			int x, s = (y + 1) * bitmapwidth;
			/* dest points into vram, it's where we're going to write this row to */
			u16*dest = subfrontbuf + o + xoffset + (256 * (y + yoffset));
			/* src is the location of the row of bitmap data */
			u8*src = (u8*)propfontBitmap + s + offset[c];
			/* this is how many bytes we're going to copy - the width of the bitmap data */
			int count = width[c];
			
			for (x = 0; x < count; x++) {
				*dest = colourPixel(*src, fgcolor);
				dest++; src++;
			}
		}

		/* move onward, remembering the space */
		o += width[c];
	}
}

void tprintf(int x, int y, u16 color, const char *format, ...) {
	va_list ap;
	char foo[100];
	va_start(ap, format);
	int len = vsnprintf(foo, 100, format, ap);
	va_end(ap);
	text_render_raw(x, y, foo, len, color | BIT(15));
}

void text_scroll() {
	int i;
	
	yoffset -= CHAR_HEIGHT + 2;

	/* fuzzie is lazy. DMA is easy. whoo! */
	/* XXX: this is probably dumb */
	DMA_SRC(3) = (uint32)&subfrontbuf[256 * (CHAR_HEIGHT + 2)];
	DMA_DEST(3) = (uint32)&subfrontbuf[0];
	DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | (256 * (TEXT_END - (CHAR_HEIGHT + 2))) >> 1;
	while (dmaBusy(3));

	/* wipe rest of screen */
	/* TODO: use memset32? */
	for (i = 256 * (TEXT_END - (CHAR_HEIGHT + 2)); i < (256 * TEXT_END); i++)
		subfrontbuf[i] = 0;
}

inline char calculateDisplayChar(char c) {
	int k = c - ' ';
	if (k < 0 || k >= NO_CHARS) k = '?' - ' ';
	return k;
}

void text_render_str(const char *text, int len) {
	int i, xusage = xoffset, lastgoodlen = 0;

	for (i = 0; i < len; i++) {
		int c = calculateDisplayChar(text[i]);
	
		/* if we're encountering a word break (space or newline), change the location of the 'last word' */
		if (c == 0 || text[i] == '\n') lastgoodlen = i;
		
		/* if there's a colour change here, render the text so far and change colour */
		if (text[i] == '\1') {
			while (yoffset + CHAR_HEIGHT + 2 > TEXT_END) text_scroll();
			lastgoodlen = i;
			if (renderconsole) text_render_raw(xoffset, yoffset, text, lastgoodlen, fgcolor);
			if (keepstate) append_console_state(text, lastgoodlen + 3);
			fgcolor = (uint16)text[i + 1] | ((uint16)text[i + 2] << 8) | BIT(15);

			text = text + lastgoodlen + 3;
			len -= lastgoodlen + 3;
			lastgoodlen = 0;
			i = -1;

			xoffset = xusage;
			continue;
		}
		if (text[i] == '\2') { // return to default colour
			/* TODO: copy/pasted from above; should remove to a seperate function */
			while (yoffset + CHAR_HEIGHT + 2 > TEXT_END) text_scroll();
			lastgoodlen = i;
			if (renderconsole) text_render_raw(xoffset, yoffset, text, lastgoodlen, fgcolor);
			if (keepstate) append_console_state(text, lastgoodlen + 1);
			fgcolor = DEFAULT_COLOR;

			text = text + lastgoodlen + 1;
			len -= lastgoodlen + 1;
			lastgoodlen = 0;
			i = -1;

			xoffset = xusage;
			continue;
		}

		/* if there's a newline or we ran out of space here, render the text up to the last word and move to the next line */
		if (text[i] == '\n' || xusage + width[c] + 2 > 256) {
			while (yoffset + CHAR_HEIGHT + 2 > TEXT_END) text_scroll();
			if (xoffset == 0 && lastgoodlen == 0) lastgoodlen = i; /* if there's some crazy-long word which takes up a whole line, render the whole thing */
			if (renderconsole) text_render_raw(xoffset, yoffset, text, lastgoodlen, fgcolor);
			if (keepstate) append_console_state(text, lastgoodlen);
			if (keepstate) append_console_state("\n", 1);
			
			if (text[i] == '\n') { lastgoodlen += 1; }
			else if (text[lastgoodlen] == ' ') lastgoodlen += 1;
			text = text + lastgoodlen;
			len -= lastgoodlen;
			lastgoodlen = 0;
		
			xoffset = 0;
			xusage = 0;
			yoffset += CHAR_HEIGHT + 2;
			
			i = -1;
			continue;
		}

		xusage += width[c];
	}
	
	if (len > 0) {
		/* render any leftover text */
		while (yoffset + CHAR_HEIGHT + 2 > TEXT_END) text_scroll();
		if (renderconsole) text_render_raw(xoffset, yoffset, text, len, fgcolor);
		if (keepstate) append_console_state(text, len);
		xoffset = xusage;
	}
}

