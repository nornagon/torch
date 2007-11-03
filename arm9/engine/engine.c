#include "engine.h"
#include "draw.h"
#include "mersenne.h"
#include "util.h"
#include "text.h"

DIRECTION just_scrolled = 0;
int dirty;
u32 low_luminance = 0; // for luminance window stuff

vu32 vblnks = 0;
void vblank_counter() {
	vblnks += 1;
}

void torch_init() {
	// Set up IRQs to call our stuff when we need it
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqEnable(IRQ_VBLANK);

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

	// some text test stuff
	text_init();
	text_console_render("This is \1\xDD\xDDTorch\1\xFF\xFF, an engine from \1\xAA\xAAnornagon\1\xFF\xFF. Starting up..\n");

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

	dirty = 2;
}

void run_processes(map_t *map, node_t **processes) {
	node_t *node = *processes;
	node_t *prev = NULL;
	while (node) {
		process_t *proc = node_data(node);
		if (proc->process) {
			proc->process(proc, map);
			prev = node;
			node = node->next;
		} else { // a NULL process callback means free the process
			if (proc->end)
				proc->end(proc, map);
			if (prev) // heal the list
				prev->next = node->next;
			else // there's a new head
				*processes = node->next;
			// add the dead process to the free pool
			node_t *k = node->next;
			free_node(map->process_pool, node);
			node = k;
		}
	}
}

// we copy data *away* from dir
void move_port(map_t *map, DIRECTION dir) {
	u32 i;
	// TODO: generalise?
	if (dir & D_NORTH) {
		// mark the top squares dirty
		// TODO: slower than not going through cache_at?
		for (i = 0; i < 32; i++)
			cache_at_s(map, i, 0)->dirty = 2;

		if (dir & D_EAST) {
			for (i = 1; i < 24; i++)
				cache_at_s(map, 31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1-8];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 1; i < 24; i++)
				cache_at_s(map, 0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-8-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8-8)>>1);
		} else {
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8)>>1);
		}
	} else if (dir & D_SOUTH) {
		// mark the southern squares dirty
		for (i = 0; i < 32; i++)
			cache_at_s(map, i, 23)->dirty = 2;
		if (dir & D_EAST) {
			for (i = 0; i < 23; i++)
				cache_at_s(map, 31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*8+8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 0; i < 23; i++)
				cache_at_s(map, 0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*8];
			DMA_DEST(3) = (uint32)&backbuf[8];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8-8)>>1);
		} else {
			DMA_SRC(3) = (uint32)&backbuf[256*8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8)>>1);
		}
	} else {
		if (dir & D_EAST) {
			for (i = 0; i < 24; i++)
				cache_at_s(map, 31, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 0; i < 24; i++)
				cache_at_s(map, 0, i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-8)>>1);
		}
	}
}

void scroll_screen(map_t *map, int dsX, int dsY) {
	map->cacheX += dsX;
	map->cacheY += dsY;
	// wrap the cache origin
	if (map->cacheX < 0) map->cacheX += 32;
	if (map->cacheY < 0) map->cacheY += 24;
	if (map->cacheX >= 32) map->cacheX -= 32;
	if (map->cacheY >= 24) map->cacheY -= 24;

	if (abs(dsX) > 1 || abs(dsY) > 1)
		dirty_screen();
	else {
		DIRECTION dir = direction(dsX, dsY, 0, 0);
		move_port(map, dir);
		just_scrolled = dir;
	}
}

void dirty_screen() {
	dirty = 2;
}

void reset_luminance() {
	low_luminance = 0;
}

void draw(map_t *map) {
	int x, y;

	u32 twiddling = 0;
	u32 drawing = 0;

	// adjust is a war between values above the top of the luminance window and
	// values below the bottom
	s32 adjust = 0;
	u32 max_luminance = 0;
	//iprintf("vbs:%02d\n", vblnks);

	cell_t *cell = cell_at(map, map->scrollX, map->scrollY);
	for (y = 0; y < 24; y++) {
		for (x = 0; x < 32; x++) {
			cache_t *cache = cache_at_s(map, x, y);

			if (cell->visible && cell->light > 0) {
				start_stopwatch();
				cell->recall = min(1<<12, max(cell->light, cell->recall));
				if (cell->light > max_luminance) max_luminance = cell->light;
				if (cell->light < low_luminance) {
					cell->light = 0;
					adjust--;
				} else if (cell->light > low_luminance + (1<<12)) {
					cell->light = 1<<12;
					adjust++;
				} else
					cell->light -= low_luminance;

				u16 ch = cell->ch;
				u16 col = cell->col;

				// if there are objects in the cell, we want to draw them instead of
				// the terrain.
				if (cell->objects) {
					// we'll only draw the head of the list. since the object list is
					// maintained as sorted, this will be the most recently added most
					// important object in the cell.
					object_t *obj = node_data(cell->objects);
					objecttype_t *objtype = obj->type;

					// if the object has a custom display function, we'll ask that.
					if (objtype->display) {
						u32 disp = objtype->display(obj, map);
						// the character should be in the high bytes
						ch = disp >> 16;
						// and the colour in the low bytes
						col = disp & 0xffff;
					} else {
						// otherwise we just use what's default for the object type.
						ch = objtype->ch;
						col = objtype->col;
					}
					cell->recalled_ch = ch;
					cell->recalled_col = col;
				} else
					cell->recalled_ch = cell->recalled_col = 0;
				int32 rval = cache->lr,
							gval = cache->lg,
							bval = cache->lb;

				bool foo = rval >> 8 != cache->last_lr ||
						gval >> 8 != cache->last_lg ||
						bval >> 8 != cache->last_lb ||
						cache->last_light != cell->light >> 8 ||
						col != cache->last_col;

				twiddling += read_stopwatch();

				// if the values have changed significantly from last time (by 7 bits
				// or more, i guess) we'll recalculate the colour. Otherwise, we won't
				// bother.
				if (foo) {
					cache->last_col = col;
					// fade out to the recalled colour (or 0 for ground)
					int32 minval = 0;
					if (!cell->forgettable) minval = (cell->recall>>2);
					int32 val = max(minval, cell->light);
					int32 maxcol = max(rval,max(bval,gval));
					// scale [rgb]val by the luminance, and keep the ratio between the
					// colours the same
					rval = (div_32_32(rval,maxcol) * val) >> 12;
					gval = (div_32_32(gval,maxcol) * val) >> 12;
					bval = (div_32_32(bval,maxcol) * val) >> 12;
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
					start_stopwatch();
					u16 col_to_draw = RGB15(r,g,b);
					u16 last_col_final = cache->last_col_final;
					if (col_to_draw != last_col_final) {
						drawcq(x*8, y*8, ch, col_to_draw);
						cache->last_col_final = col_to_draw;
						cache->last_lr = cache->lr >> 8;
						cache->last_lg = cache->lg >> 8;
						cache->last_lb = cache->lb >> 8;
						cache->last_light = cell->light >> 8;
						cache->dirty = 2;
					} else if (cache->dirty > 0 || dirty > 0) {
						drawcq(x*8, y*8, ch, col_to_draw);
						if (cache->dirty > 0)
							cache->dirty--;
					}
					drawing += read_stopwatch();
				} else {
					start_stopwatch();
					if (cache->dirty > 0 || dirty > 0) {
						drawcq(x*8, y*8, ch, cache->last_col_final);
						if (cache->dirty > 0)
							cache->dirty--;
					}
					drawing += read_stopwatch();
				}
				cell->light = 0;
				cache->lr = 0;
				cache->lg = 0;
				cache->lb = 0;
				cache->was_visible = true;
			} else if (cache->dirty > 0 || dirty > 0 || cache->was_visible) {
				// dirty or it was visible last frame and now isn't.
				if (cell->recall > 0 && (!cell->forgettable || cell->recalled_ch)) {
					u16 col;
					u8 ch;
					if (cell->recalled_ch) {
						col = cell->recalled_col;
						ch = cell->recalled_ch;
					} else {
						col = cell->col;
						ch = cell->ch;
					}
					u32 r = col & 0x001f,
							g = (col & 0x03e0) >> 5,
							b = (col & 0x7c00) >> 10;
					int32 val = (cell->recall>>2);
					r = ((r<<12) * val) >> 24;
					g = ((g<<12) * val) >> 24;
					b = ((b<<12) * val) >> 24;
					cache->last_col_final = RGB15(r,g,b);
					cache->last_col = cache->last_col_final; // not affected by light, so they're the same
					cache->last_lr = 0;
					cache->last_lg = 0;
					cache->last_lb = 0;
					cache->last_light = 0;
					start_stopwatch();
					drawcq(x*8, y*8, ch, cache->last_col_final);
					drawing += read_stopwatch();
				} else {
					drawcq(x*8, y*8, ' ', 0); // clear
					cache->last_col_final = 0;
					cache->last_col = 0;
					cache->last_lr = 0;
					cache->last_lg = 0;
					cache->last_lb = 0;
					cache->last_light = 0;
				}
				if (cache->was_visible) {
					cache->was_visible = false;
					cache->dirty = 2;
				}
				if (cache->dirty > 0)
					cache->dirty--;
			}
			cell->visible = 0;

			cell++; // takes into account the size of the structure, apparently
		}
		cell += map->w - 32; // the next row down
	}

	low_luminance += max(adjust*2, -low_luminance); // adjust to fit at twice the difference
	// drift towards having luminance values on-screen placed at maximum brightness.
	if (low_luminance > 0 && max_luminance < low_luminance + (1<<12))
		low_luminance -= min(40,low_luminance);

	/*iprintf("\x1b[10;8H      \x1b[10;0Hadjust: %d\nlow luminance: %04x", adjust, low_luminance);
	iprintf("\x1b[13;0Hdrawing: %05x\ntwiddling: %05x", drawing, twiddling);*/
	//iprintf("vbe:%02d\n", vblnks);

	if (dirty > 0) {
		dirty--;
		if (dirty > 0)
			cls();
	}
}

void run(map_t *map) {
	while (1) {
		// TODO: is DMA actually asynchronous?
		bool copying = false;

		if (just_scrolled) {
			// update the new backbuffer
			move_port(map, just_scrolled);
			copying = true;
			just_scrolled = 0;
		}

		// run important processes first
		run_processes(map, &map->high_processes);
		// then everything else
		run_processes(map, &map->processes);

		// wait for DMA to finish
		if (copying)
			while (dmaBusy(3));

		// draw loop
		draw(map);

		while (vblnks < 2) swiWaitForVBlank();
		vblnks = 0;
		swapbufs();
	}
}
