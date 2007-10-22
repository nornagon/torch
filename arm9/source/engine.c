#include "engine.h"
#include "draw.h"
#include "mersenne.h"

u32 vblnks = 0, frames = 0, hblnks = 0;
int vblnkDirty = 0;
void vblank_counter() {
	vblnkDirty = 1;
	vblnks += 1;
}
void hblank_counter() {
	hblnks += 1;
}

void torch_init() {
	// Set up IRQs to call our stuff when we need it
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqSet(IRQ_HBLANK, hblank_counter);
	irqEnable(IRQ_VBLANK | IRQ_HBLANK);

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

	// the sub screen isn't really used very much yet except for debug output. We
	// set it up for just that here.
	videoSetModeSub( MODE_0_2D | DISPLAY_BG0_ACTIVE );
	vramSetBankC(VRAM_C_SUB_BG);
	SUB_BG0_CR = BG_MAP_BASE(31);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);

	consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);

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
}

void run_processes(map_t *map, node_t **processes) {
	node_t *node = *processes;
	node_t *prev = NULL;
	while (node) {
		process_t *proc = node_data(node);
		if (proc->process) {
			proc->process(proc, map);
			prev = node;
		} else { // a NULL process callback means free the process
			if (proc->end)
				proc->end(proc, map);
			if (prev) // heal the list
				prev->next = node->next;
			else // there's a new head
				*processes = node->next;
			// add the dead process to the free pool
			free_node(map->process_pool, node);
		}
		node = node->next;
	}
}


// we copy data *away* from dir
void scroll_screen(map_t *map, DIRECTION dir) {
	u32 i;
	// TODO: generalise?
	if (dir & D_NORTH) {
		// mark the top squares dirty
		// TODO: slower than not going through cache_at?
		for (i = 0; i < 32; i++)
			cache_at(map, i+map->scrollX, map->scrollY)->dirty = 2;

		if (dir & D_EAST) {
			for (i = 1; i < 24; i++)
				cache_at(map, map->scrollX+31, map->scrollY+i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-256*8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1-8];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-256*8-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 1; i < 24; i++)
				cache_at(map, map->scrollX, map->scrollY+i)->dirty = 2;
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
			cache_at(map, i+map->scrollX, map->scrollY+23)->dirty = 2;
		if (dir & D_EAST) {
			for (i = 0; i < 23; i++)
				cache_at(map, map->scrollX+31, map->scrollY+i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*8+8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-256*8-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 0; i < 23; i++)
				cache_at(map, map->scrollX, map->scrollY+i)->dirty = 2;
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
				cache_at(map, map->scrollX+31, map->scrollY+i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[8];
			DMA_DEST(3) = (uint32)&backbuf[0];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_INC | DMA_DST_INC | ((256*192-8)>>1);
		} else if (dir & D_WEST) {
			for (i = 0; i < 24; i++)
				cache_at(map, map->scrollX, map->scrollY+i)->dirty = 2;
			DMA_SRC(3) = (uint32)&backbuf[256*192-1-8];
			DMA_DEST(3) = (uint32)&backbuf[256*192-1];
			DMA_CR(3) = DMA_COPY_WORDS | DMA_SRC_DEC | DMA_DST_DEC | ((256*192-8)>>1);
		}
	}
}
