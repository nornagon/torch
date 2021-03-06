#include "engine.h"
#include "mersenne.h"
#include "util.h"
#include "text.h"

#include "nocash.h"

#ifdef NATIVE
#include "native.h"
#include <sys/time.h>
#define TRIP_DMA(n) trip_dma(n)
#else
#define TRIP_DMA(n) ((void)0)
#include "draw.h"
#endif

engine torch;

engine::engine() {
	just_scrolled = 0;
	dirty = 0;
	low_luminance = 0;
}

vu32 vblnks = 0;
void vblank_counter() {
	vblnks += 1;
}

void engine::init() {
	// Set up IRQs to call our stuff when we need it
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqEnable(IRQ_VBLANK);
#ifndef NATIVE

	// a bunch of other stuff (draw.c, from memory) relies on the below being the
	// case (i.e, VRAM banks being mapped like that and the BG modes set thus), so
	// be careful if you want to change it.
	videoSetMode( MODE_3_2D | DISPLAY_BG3_ACTIVE );
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	// the important BG modes I was referring to.
	BGCTRL[3] = BG_BMP16_256x256 | BG_BMP_BASE(6);
	BG3_XDY = 0;
	BG3_XDX = 1 << 8;
	BG3_YDX = 0;
	BG3_YDY = 1 << 8;
	BG_PALETTE[0] = 0;

	// not sure if this is necessary, but we don't want any surprises. TIMER_DATA
	// is what the timer resets to when you start it (or it overflows)
	TIMER_DATA(0) = 0;

	// initialize the Twister
	swiWaitForVBlank(); // sync with arm7, which should be filling the IPC struct with RTC info
	u32 seed = IPC->time.rtc.seconds;
	seed += IPC->time.rtc.minutes*60;
	seed += IPC->time.rtc.hours*60*60;
	seed += IPC->time.rtc.weekday*7*24*60*60;
	init_genrand(seed);

	// some text test stuff
	text_init();
	text_console_render("This is \1\x9E\x02Torch\1\xFF\xFF, an engine from \1\xE0\x7Dnornagon\1\xFF\xFF. Starting up...\n");
#else
	native_init();
	{
		struct timeval tv;
		gettimeofday(&tv, NULL);
		init_genrand(tv.tv_usec + tv.tv_sec*1000000);
	}
#endif

	dirty = 0;
}

// we copy data *away* from dir
void engine::move_port(DIRECTION dir) {
	u32 i;
	// TODO: generalise?
	if (dir & D_NORTH) {
		// mark the top squares dirty
		// TODO: slower than not going through cache_at?
		for (i = 0; i < 32; i++)
			buf.cache.at(i, 0)->dirty = 2;

		if (dir & D_EAST) {
			for (i = 1; i < 24; i++)
				buf.cache.at(31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1-8];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8-8)>>1);
			TRIP_DMA(3);
		} else if (dir & D_WEST) {
			for (i = 1; i < 24; i++)
				buf.cache.at(0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-8-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8-8)>>1);
			TRIP_DMA(3);
		} else {
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8)>>1);
			TRIP_DMA(3);
		}
	} else if (dir & D_SOUTH) {
		// mark the southern squares dirty
		for (i = 0; i < 32; i++)
			buf.cache.at(i, 23)->dirty = 2;
		if (dir & D_EAST) {
			for (i = 0; i < 23; i++)
				buf.cache.at(31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*8+8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8-8)>>1);
			TRIP_DMA(3);
		} else if (dir & D_WEST) {
			for (i = 0; i < 23; i++)
				buf.cache.at(0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*8];
			DMA_DEST(3) = (uint32)&backbuf[8];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8-8)>>1);
			TRIP_DMA(3);
		} else {
			DMA_SRC(3) = (uint32)&backbuf[256*8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8)>>1);
			TRIP_DMA(3);
		}
	} else {
		if (dir & D_EAST) {
			for (i = 0; i < 24; i++)
				buf.cache.at(31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-8)>>1);
			TRIP_DMA(3);
		} else if (dir & D_WEST) {
			for (i = 0; i < 24; i++)
				buf.cache.at(0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-8)>>1);
			TRIP_DMA(3);
		}
	}
}

void engine::scroll(int dsX, int dsY) {
	buf.cache.origin.x += dsX;
	buf.cache.origin.y += dsY;
	// wrap the cache origin
	if (buf.cache.origin.x < 0) buf.cache.origin.x += 32;
	if (buf.cache.origin.y < 0) buf.cache.origin.y += 24;
	if (buf.cache.origin.x >= 32) buf.cache.origin.x -= 32;
	if (buf.cache.origin.y >= 24) buf.cache.origin.y -= 24;

	if (abs(dsX) > 1 || abs(dsY) > 1)
		dirty_screen();
	else {
		DIRECTION dir = direction(dsX, dsY, 0, 0);
		move_port(dir);
		just_scrolled = dir;

		// XXX pretty sure this is flawed
		for (int y = (dir&D_NORTH ? 1 : 0); y < (dir&D_SOUTH ? 23 : 24); y++)
			for (int x = (dir&D_WEST ? 1 : 0); x < (dir&D_EAST ? 31 : 32); x++)
				*(buf.luxat_s(x,y)) = *(buf.luxat_s(x+D_DX[dir], y+D_DY[dir]));
	}
}

void engine::onscreen(int x, int y, unsigned int margin) {
	s32 dsX = 0, dsY = 0;
	// keep the screen vaguely centred on the player (gap of 8 cells)
	if (x - buf.scroll.x < 8 && buf.scroll.x > 0) { // it's just a scroll to the left
		dsX = (x - 8) - buf.scroll.x;
		buf.scroll.x = x - 8;
	} else if (x - buf.scroll.x > 24 && buf.scroll.x < buf.getw()-32) {
		dsX = (x - 24) - buf.scroll.x;
		buf.scroll.x = x - 24;
	}

	if (y - buf.scroll.y < 8 && buf.scroll.y > 0) {
		dsY = (y - 8) - buf.scroll.y;
		buf.scroll.y = y - 8;
	} else if (y - buf.scroll.y > 16 && buf.scroll.y < buf.geth()-24) {
		dsY = (y - 16) - buf.scroll.y;
		buf.scroll.y = y - 16;
	}

	if (dsX || dsY)
		scroll(dsX, dsY);
}

void engine::dirty_screen() {
	dirty = 2;
}

void engine::reset_luminance() {
	low_luminance = 0;
}

void engine::draw() {
	// adjust is a war between values above the top of the luminance window and
	// values below the bottom
	s32 adjust = 0;
	s32 max_luminance = 0;

	mapel *m = buf.at(buf.scroll.x, buf.scroll.y); // we'll ++ it later
	for (int y = 0; y < 24; y++) {
		for (int x = 0; x < 32; x++) {
			cachel *c = buf.cache.at(x, y);
			luxel *l = buf.luxat(x+buf.scroll.x, y+buf.scroll.y);

			if (l->lval > 0) {
				if (l->lval > max_luminance) max_luminance = l->lval;
				if (l->lval < low_luminance) {
					l->lval = 0;
					adjust--;
				} else if (l->lval > low_luminance + (1<<12)) {
					l->lval = 1<<12;
					adjust++;
				} else
					l->lval -= low_luminance;

				u16 ch = m->ch;
				u16 col = m->col;

				int32 rval = l->lr,
							gval = l->lg,
							bval = l->lb;

				bool changed = rval >> 8 != l->last_lr ||
						gval >> 8 != l->last_lg ||
						bval >> 8 != l->last_lb ||
						l->last_lval != l->lval >> 8 ||
						col != c->last_col ||
						ch != c->last_ch;

				// if the values have changed significantly from last time (by 7 bits
				// or more, i guess), or the cache says we should, we'll recalculate
				// the colour. Otherwise, we'll use the values we have stored.
				if (changed || c->dirty > 0 || dirty > 0) {
					l->last_lr = l->lr >> 8;
					l->last_lg = l->lg >> 8;
					l->last_lb = l->lb >> 8;
					l->last_lval = l->lval >> 8;
					c->last_col = col;
					c->last_ch = ch;
					// fade out to the recalled colour
					int32 minval = (m->recall>>2);
					int32 val = max(minval, l->lval);
					int32 maxcol = max(rval,max(bval,gval));
					// scale [rgb]val by the luminance, and keep the ratio between the
					// colours the same TODO correct?
					rval = div_32_32((rval * val) >> 12,maxcol);
					gval = div_32_32((gval * val) >> 12,maxcol);
					bval = div_32_32((bval * val) >> 12,maxcol);
					rval = max(rval, minval);
					gval = max(gval, minval);
					bval = max(bval, minval);
					// eke out the colour values from the 15-bit colour
					u32 r = col & 0x001f,
							g = (col & 0x03e0) >> 5,
							b = (col & 0x7c00) >> 10;
					// multiply the colour through fixed-point 20.12 for a bit more accuracy
					r = ((r<<12) * rval) >> 24;
					g = ((g<<12) * gval) >> 24;
					b = ((b<<12) * bval) >> 24;
					u16 col_to_draw = RGB15(r,g,b);
					u16 last_col_final = c->last_col_final;
					if (col_to_draw != last_col_final) {
						drawcq(x*8, y*8, ch, col_to_draw);
						c->last_col_final = col_to_draw;
						c->dirty = 2;
					} else if (c->dirty > 0 || dirty > 0) {
						drawcq(x*8, y*8, ch, col_to_draw);
					}
				}
				l->lval = 0;
				l->lr = 0;
				l->lg = 0;
				l->lb = 0;
				c->was_visible = true;
			} else if (c->dirty > 0 || dirty > 0 || c->was_visible) {
				// dirty or it was visible last frame and now isn't.
				if (m->recall > 0 /*&& !cell->forgettable*/) {
					u16 col;
					u8 ch;
					col = m->col;
					ch = m->ch;
					u32 r = col & 0x001f,
							g = (col & 0x03e0) >> 5,
							b = (col & 0x7c00) >> 10;
					int32 val = (m->recall>>2);
					r = ((r<<12) * val) >> 24;
					g = ((g<<12) * val) >> 24;
					b = ((b<<12) * val) >> 24;
					c->last_col_final = RGB15(r,g,b);
					c->last_col = c->last_col_final; // not affected by light, so they're the same
					l->last_lr = 0;
					l->last_lg = 0;
					l->last_lb = 0;
					l->last_lval = 0;
					drawcq(x*8, y*8, ch, c->last_col_final);
				} else {
					drawcq(x*8, y*8, ' ', 0); // clear
					c->last_col_final = 0;
					c->last_ch = ' ';
					c->last_col = 0;
					l->last_lr = 0;
					l->last_lg = 0;
					l->last_lb = 0;
					l->last_lval = 0;
				}
				if (c->was_visible) {
					c->was_visible = false;
					c->dirty = 2;
				}
			}
			if (c->dirty > 0)
				c->dirty--;
			//cell->visible = 0;

			m++; // takes into account the size of the structure, apparently
		}
		m += buf.getw() - 32; // the next row down
	}

	// if we always adjust the luminances, the HDR algo will tend to flicker
	// between two states. It isn't really visually noticeable, but it'll cause
	// extra work to happen that really doesn't need to be done. So we only
	// bother adjusting low_luminance if we have to.
	// This is probably a pretty bad metric for whether or not we have to, but it
	// seems to work.
	if (adjust > 4) {
		low_luminance += max(adjust*2, -low_luminance); // adjust to fit at twice the difference
	}
	// drift towards having luminance values on-screen placed at maximum brightness.
	if (low_luminance > 0 && max_luminance < low_luminance + (1<<12))
		low_luminance -= min(40,low_luminance);

	if (dirty > 0) {
		dirty--;
		/*if (dirty > 0)
			cls();*/ // XXX XXX XXX
	}
}

void engine::run(void (*handler)()) {
	while (1) {
		// TODO: is DMA actually asynchronous?
		bool copying = false;

		if (just_scrolled) {
			// update the new backbuffer
			move_port(just_scrolled);
			copying = true;
			just_scrolled = 0;
		}

		handler();

		// wait for DMA to finish
		if (copying)
			while (dmaBusy(3));

		// draw loop
		draw();

		// cap to 30fps
		while (vblnks < 2) swiWaitForVBlank();
		vblnks = 0;
		swapbufs();
	}
}
