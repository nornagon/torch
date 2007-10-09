#include "nds.h"
#include <nds/arm9/console.h>
#include <stdio.h>
#include <malloc.h>
#include "mt19937ar.h"
#include "fov.h"

#include "font.h"

inline int min(int a, int b) {
	if (a < b) return a;
	return b;
}
inline int max(int a, int b) {
	if (a > b) return a;
	return b;
}

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
	memset((void*)backbuf, 0, 256*192*2);
}
void clss() {
	memset((void*)BG_BMP_RAM(0), 0, 256*192*4);
}

void drawc(u16 x, u16 y, u8 c, u16 color) {
	c -= ' ';
	u8 px,py;
	for (py = 0; py < 8; py++)
		for (px = 0; px < 8; px++) {
			if (fontTiles[8*8*c+8*py+px])
				backbuf[(y+py)*256+x+px] = color|BIT(15);
		}
}
void drawcq(u16 x, u16 y, u8 c, u16 color) { // OPAQUE version (clobbers)
	c -= ' ';
	u8 px,py;
	for (py = 0; py < 8; py++)
		for (px = 0; px < 8; px++) {
			backbuf[(y+py)*256+x+px] = fontTiles[8*8*c+8*py+px] ? color|BIT(15) : 0;
		}
}

typedef enum {
	T_TREE = 0,
	T_GROUND,
	NO_TYPE
} CELL_TYPE;

typedef struct {
	CELL_TYPE type;
	u8 ch;
	u16 col;
	u8 lit;
	u8 recall;
	u8 dirty;
} cell_t;

typedef struct {
	u32 w,h;
	cell_t* cells;
} map_t;

void reset_map(map_t* map, CELL_TYPE fill) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = &map->cells[y*w+x];
			cell->type = fill;
			cell->ch = 0;
			cell->col = 0;
			cell->lit = 0;
			cell->recall = 0;
			cell->dirty = 0;
		}
}
map_t *create_map(u32 w, u32 h, CELL_TYPE fill) {
	map_t *ret = malloc(sizeof(map_t));
	ret->w = w; ret->h = h;
	ret->cells = malloc(w*h*sizeof(cell_t));
	reset_map(ret, fill);

	return ret;
}

void random_map(map_t *map) {
	s32 x,y;
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = &map->cells[y*map->w+x];
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	x = map->w/2; y = map->h/2;
	u32 i;
	for (i = 0; i < 1000; i++) {
		cell_t *cell = &map->cells[y*map->w+x];
		cell->type = T_GROUND;
		u32 a = genrand_int32();
		u8 b = a & 3; // top two bits of a
		a >>= 2; // get rid of the used random bits
		switch (b) {
			case 1:
				cell->ch = ','; break;
			case 2:
				cell->ch = '\''; break;
			case 3:
				cell->ch = '`'; break;
			default:
				cell->ch = '.'; break;
		}
		b = a & 3;
		a >>= 2;
		u8 g = a & 7;
		a >>= 3;
		u8 r = a & 7;
		a >>= 3;
		cell->col = RGB15(17+r,9+g,6+b);

		if (a & 1) {
			if (a & 2) x += 1;
			else x -= 1;
		} else {
			if (a & 2) y += 1;
			else y -= 1;
		}
		if (x < 0) x = 0;
		if (y < 0) y = 0;
		if (x >= map->w) x = map->w-1;
		if (y >= map->h) y = map->h-1;
	}
}

u32 vblnks = 0, frames = 0;
int vblnkDirty = 0;
void vblank_counter() {
	vblnkDirty = 1;
	vblnks += 1;
}

bool opacity_test(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return true;
	return map->cells[y*map->w+x].type != T_GROUND;
}

void apply_lighting(void *map_, int x, int y, int dx, int dy, void *src) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;
	cell_t *cell = &map->cells[y*map->w+x];
	cell->lit = min(8, 10 - sqrt32(dx*dx+dy*dy));
	cell->recall = max(cell->lit, cell->recall);
	cell->dirty = 2;
}

//---------------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------------
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqEnable(IRQ_VBLANK);

	touchPosition touchXY;

	videoSetMode( MODE_3_2D | DISPLAY_BG3_ACTIVE );
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);
	BGCTRL[3] = BG_BMP16_256x256 | BG_BMP_BASE(6);
	BG3_XDY = 0;
	BG3_XDX = 1 << 8;
	BG3_YDX = 0;
	BG3_YDY = 1 << 8;

	videoSetModeSub( MODE_0_2D | DISPLAY_BG0_ACTIVE );
	vramSetBankC(VRAM_C_SUB_BG);
	SUB_BG0_CR = BG_MAP_BASE(31);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);

	consoleInitDefault((u16*)SCREEN_BASE_BLOCK_SUB(31), (u16*)CHAR_BASE_BLOCK_SUB(0), 16);

	fov_settings_type *fov_settings = malloc(sizeof(fov_settings_type));
	fov_settings_init(fov_settings);
	fov_settings_set_shape(fov_settings, FOV_SHAPE_CIRCLE);
	fov_settings_set_opacity_test_function(fov_settings, opacity_test);
	fov_settings_set_apply_lighting_function(fov_settings, apply_lighting);
	
	swiWaitForVBlank();
	u32 seed = IPC->time.rtc.seconds;
	seed += IPC->time.rtc.minutes*60;
	seed += IPC->time.rtc.hours*60*60;
	seed += IPC->time.rtc.weekday*7*24*60*60;
	init_genrand(seed); // XXX: RTC is not really a good randomness source

	map_t *map = create_map(64,48, T_TREE);
	random_map(map);

	s32 pX = map->w/2, pY = map->h/2;

	iprintf("\n\n\tHello World!\n");

	u32 x, y;
	u32 frm = 0;

	s32 scrollX=map->w/2 - 16, scrollY=map->h/2 - 12;

	int dirty = 2; // whole screen dirty first frame

	while (1) {
		if (!vblnkDirty)
			swiWaitForVBlank();
		vblnkDirty = 0;
		scanKeys();
		u32 down = keysDown();
		if (down & KEY_START) {
			init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds + IPC->time.rtc.minutes*60+IPC->time.rtc.hours*60*60 + IPC->time.rtc.weekday*7*24*60*60));
			clss();
			reset_map(map, T_TREE);
			random_map(map);
			pX = map->w/2; pY = map->h/2;
			frm = 0;
			scrollX = map->w/2 - 16; scrollY = map->h/2 - 12;
			vblnkDirty = 0;
			dirty = 2;
			continue;
		}
		if (frm == 0) {
			u32 keys = keysHeld();
			int dpX = 0, dpY = 0;
			if (keys & KEY_RIGHT)
				dpX = 1;
			if (keys & KEY_LEFT)
				dpX = -1;
			if (keys & KEY_DOWN)
				dpY = 1;
			if (keys & KEY_UP)
				dpY = -1;
			if (dpX || dpY) {
				frm = 5;
				// bumping into a wall takes some time
				if (map->cells[(pY+dpY)*map->w+pX+dpX].type != T_GROUND) {
					dpX = dpY = 0;
					frm = 2;
				} else {
					pX += dpX;
					pY += dpY;
					if (pX < 0) { pX = 0; frm = 2; }
					if (pY < 0) { pY = 0; frm = 2; }
					if (pX >= map->w) { pX = map->w-1; frm = 2; }
					if (pY >= map->h) { pY = map->h-1; frm = 2; }

					// XXX: beware, here be font-specific values
					if (pX - scrollX < 8 && scrollX > 0) {
						scrollX = pX - 8; dirty = 2; cls();
					} else if (pX - scrollX > 24 && scrollX < map->w-32) {
						scrollX = pX - 24; dirty = 2; cls();
					}
					if (pY - scrollY < 8 && scrollY > 0) { 
						scrollY = pY - 8; dirty = 2; cls();
					} else if (pY - scrollY > 16 && scrollY < map->h-24) {
						scrollY = pY - 16; dirty = 2; cls();
					}
				}
			}
		}
		if (frm > 0)
			frm--;

		fov_circle(fov_settings, (void*)map, NULL, pX, pY, 8);

		// XXX: more font-specific magical values
		for (y = 0; y < 24; y++)
			for (x = 0; x < 32; x++) {
				cell_t *cell = &map->cells[(y+scrollY)*map->w+(x+scrollX)];
				if (cell->lit > 0) {
					int r = cell->col & 0x001f,
							g = (cell->col & 0x03e0) >> 5,
							b = (cell->col & 0x7c00) >> 10;
					drawcq(x*8,y*8,cell->ch,
							RGB15((r*max(cell->lit<<2,cell->recall))/32,
								    (g*max(cell->lit<<2,cell->recall))/32,
										(b*max(cell->lit<<2,cell->recall))/32));
					cell->lit = 0;
				} else if (cell->recall > 0 && (cell->dirty > 0 || dirty > 0)) {
					int r = cell->col & 0x001f,
							g = (cell->col & 0x03e0) >> 5,
							b = (cell->col & 0x7c00) >> 10;
					if (cell->dirty > 0)
						cell->dirty--;
					drawcq(x*8,y*8,cell->ch,
							RGB15((r*cell->recall)/32,
								    (g*cell->recall)/32,
										(b*cell->recall)/32));
				}
			}
		drawcq((pX-scrollX)*8,(pY-scrollY)*8,'@',RGB15(31,31,31));
		swapbufs();
		if (dirty > 0) {
			dirty--;
			if (dirty > 0)
				cls();
		}
		frames += 1;
		if (vblnks >= 60) {
			iprintf("\x1b[14;14H%2dfps", (frames * 64 - frames * 4) / vblnks);
			vblnks = frames = 0;
		}
	}

	return 0;
}
