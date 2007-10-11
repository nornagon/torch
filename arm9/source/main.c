#include "nds.h"
#include <nds/arm9/console.h>
#include <stdio.h>
#include <malloc.h>
#include "mt19937ar.h"
#include "fov.h"

#include "font.h"

//---------------------------------------------------------------------------
// numbers

inline int min(int a, int b) {
	if (a < b) return a;
	return b;
}
inline int max(int a, int b) {
	if (a > b) return a;
	return b;
}

// a convolution of a uniform distribution with itself is close to Gaussian
u32 genrand_gaussian32() {
	return (u32)((genrand_int32()>>2) + (genrand_int32()>>2) +
			(genrand_int32()>>2) + (genrand_int32()>>2));
}

//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// drawing
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
	c -= ' '; // XXX: might want to remove this
	u8 px,py;
	for (py = 0; py < 8; py++)
		for (px = 0; px < 8; px++) {
			if (fontTiles[8*8*c+8*py+px])
				backbuf[(y+py)*256+x+px] = color|BIT(15);
		}
}
void drawcq(u16 x, u16 y, u8 c, u16 color) { // OPAQUE version (clobbers)
	c -= ' '; // XXX: might want to remove this
	u8 px,py;
	for (py = 0; py < 8; py++)
		for (px = 0; px < 8; px++) {
			backbuf[(y+py)*256+x+px] = fontTiles[8*8*c+8*py+px] ? color|BIT(15) : 0;
		}
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// map stuff

#define MAX_LIGHTS 32

typedef enum {
	T_TREE = 0,
	T_GROUND,
	T_STAIRS,
	T_FIRE,
	NO_TYPE
} CELL_TYPE;

typedef enum {
	L_FIRE = 0,
	NO_LIGHT
} LIGHT_TYPE;

/*** light ***/
typedef struct {
	s32 x,y;
	u16 col;
	u8 radius;
	LIGHT_TYPE type : 8;
} light_t;
/*     ~     */

/*** cell ***/
typedef struct {
	CELL_TYPE type : 8;
	u8 ch;
	u16 col;
	int32 lit, recall;
	u8 dirty : 2;
	bool visible : 1;
} __attribute__((aligned(4))) cell_t;
/*    ~~    */

/*** map ***/
typedef struct {
	u32 w,h;
	cell_t* cells;
	light_t lights[MAX_LIGHTS];
	u8 num_lights;
	s32 pX,pY;
} map_t;
/*    ~    */

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
	map->num_lights = 0;
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
	reset_map(map, T_TREE);
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = &map->cells[y*map->w+x];
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	x = map->w/2; y = map->h/2;
	u32 i = 8192;
	u32 light = genrand_int32() >> 21; // between 0 and 2047
	for (; i > 0; i--) {
		cell_t *cell = &map->cells[y*map->w+x];
		u32 a = genrand_int32();
		if (i == light) {
			cell->type = T_FIRE;
			cell->ch = 'w';
			cell->col = RGB15(31,12,0);
			light_t *l = &map->lights[map->num_lights];
			l->x = x;
			l->y = y;
			l->col = RGB15(31,20,8);
			l->radius = 9;
			l->type = L_FIRE;
			map->num_lights++;
		} else if (cell->type == T_TREE) {
			cell->type = T_GROUND;
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
		}

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
	cell_t *cell = &map->cells[y*map->w+x];
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);
}

void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds + IPC->time.rtc.minutes*60+IPC->time.rtc.hours*60*60 + IPC->time.rtc.weekday*7*24*60*60));
	clss();
	random_map(map);
}

u32 vblnks = 0, frames = 0;
int vblnkDirty = 0;
void vblank_counter() {
	vblnkDirty = 1;
	vblnks += 1;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// libfov functions
bool opacity_test(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return true;
	return map->cells[y*map->w+x].type == T_TREE;
}

inline int32 calc_semicircle(int32 dist2, int32 rad2) {
	int32 val = (1<<12) - divf32(dist2, rad2);
	if (val < 0) return 0;
	return sqrtf32(val);
}

inline int32 calc_cubic(int32 dist2, int32 rad, int32 rad2) {
	int32 dist = sqrtf32(dist2);
	// 2(d/r)³ - 3(d/r)² + 1
	return mulf32(divf32(2<<12, mulf32(rad,rad2)), mulf32(dist2,dist)) -
		mulf32(divf32(3<<12, rad2), dist2) + (1<<12);
}

void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;
	int32 *src = (int32*)src_;

	// XXX: super ick, magical fonting numbers here
	s32 scrollX = src[3], scrollY = src[4];
	if (x < scrollX || y < scrollY || x > scrollX + 31 || y > scrollY + 23) return;

	int32 dx = src[0]-(x<<12),
				dy = src[1]-(y<<12),
				dist2 = mulf32(dx,dx)+mulf32(dy,dy);
	int32 rad = src[2],
				rad2 = mulf32(rad,rad);

	cell_t *cell = &map->cells[y*map->w+x];
	if (dist2 < rad2) {
		cell->lit = calc_semicircle(dist2, rad2); // NB: no addition or capping here
		cell->recall = max(cell->lit, cell->recall);
		cell->dirty = 2;
	}
	cell->visible = true;
}

void apply_light(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;

	cell_t *cell = &map->cells[y*map->w+x];

	// don't light the cell if we can't see it
	if (!cell->visible) return;

	// XXX: this function is pretty much identical to apply_sight... should
	// maybe merge them.
	int32 *src = (int32*)src_;
	int32 dx = src[0]-(x<<12),
				dy = src[1]-(y<<12),
				dist2 = mulf32(dx,dx)+mulf32(dy,dy);
	int32 rad = src[2],
				rad2 = mulf32(rad,rad);
	if (dist2 < rad2) {
		cell_t *cell = &map->cells[y*map->w+x];

		cell->lit += calc_semicircle(dist2, rad2);
		cell->lit = min(cell->lit, 1<<12);

		cell->recall = max(cell->lit, cell->recall);
		cell->dirty = 2;
	}
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------
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

	fov_settings_type *sight = malloc(sizeof(fov_settings_type));
	fov_settings_init(sight);
	fov_settings_set_shape(sight, FOV_SHAPE_CIRCLE);
	fov_settings_set_opacity_test_function(sight, opacity_test);
	fov_settings_set_apply_lighting_function(sight, apply_sight);

	fov_settings_type *light = malloc(sizeof(fov_settings_type));
	fov_settings_init(light);
	fov_settings_set_shape(light, FOV_SHAPE_CIRCLE);
	fov_settings_set_opacity_test_function(light, opacity_test);
	fov_settings_set_apply_lighting_function(light, apply_light);
	
	swiWaitForVBlank();
	u32 seed = IPC->time.rtc.seconds;
	seed += IPC->time.rtc.minutes*60;
	seed += IPC->time.rtc.hours*60*60;
	seed += IPC->time.rtc.weekday*7*24*60*60;
	init_genrand(seed); // XXX: RTC is not really a good randomness source

	map_t *map = create_map(128,128, T_TREE);
	random_map(map);

	map->pX = map->w/2;
	map->pY = map->h/2;

	u32 x, y;
	u32 frm = 0;

	s32 scrollX=map->w/2 - 16, scrollY=map->h/2 - 12;

	int dirty = 2; // whole screen dirty first frame

	u32 level = 0;

	int32 src[7] = { 0, 0, 0, // source x, source y, radius
		0, 0  // scroll x, scroll y
	};

	while (1) {
		if (!vblnkDirty)
			swiWaitForVBlank();
		vblnkDirty = 0;
		scanKeys();
		u32 down = keysDown();
		if (down & KEY_START) {
			new_map(map);
			map->pX = map->w/2;
			map->pY = map->h/2;
			frm = 0;
			scrollX = map->w/2 - 16; scrollY = map->h/2 - 12;
			vblnkDirty = 0;
			dirty = 2;
			level = 0;
			//iprintf("You begin again.\n");
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
			if (keys & KEY_A && map->cells[map->pY*map->w+map->pX].type == T_STAIRS) {
				//iprintf("You fall down the stairs...\nYou are now on level %d.\n", ++level);
				new_map(map);
				map->pX = map->w/2;
				map->pY = map->h/2;
				// TODO: make so screen doesn't jump on down-stairs
				scrollX = map->w/2 - 16; scrollY = map->h/2 - 12;
				vblnkDirty = 0;
				dirty = 2;
				continue;
			}
			if (dpX || dpY) {
				frm = 5;
				// bumping into a wall takes some time
				if (map->cells[(map->pY+dpY)*map->w+map->pX+dpX].type == T_TREE) { // XXX generalize bump test
					dpX = dpY = 0;
					frm = 2;
				} else {
					if (map->pX + dpX < 0) { dpX = dpY = 0; frm = 2; }
					if (map->pY + dpY < 0) { dpY = dpX = 0; frm = 2; }
					if (map->pX + dpX >= map->w) { dpX = dpY = 0; frm = 2; }
					if (map->pY + dpY >= map->h) { dpY = dpX = 0; frm = 2; }
					map->pX += dpX;
					map->pY += dpY;

					// XXX: beware, here be font-specific values
					if (map->pX - scrollX < 8 && scrollX > 0) {
						scrollX = map->pX - 8; dirty = 2; cls();
					} else if (map->pX - scrollX > 24 && scrollX < map->w-32) {
						scrollX = map->pX - 24; dirty = 2; cls();
					}
					if (map->pY - scrollY < 8 && scrollY > 0) { 
						scrollY = map->pY - 8; dirty = 2; cls();
					} else if (map->pY - scrollY > 16 && scrollY < map->h-24) {
						scrollY = map->pY - 16; dirty = 2; cls();
					}
				}
			}
		}
		if (frm > 0)
			frm--;

		if (frames % 4 == 0) {
			// XXX: ick
			src[0] = (map->pX<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[1] = (map->pY<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[2] = (7<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[5] = scrollX;
			src[6] = scrollY;
		}

		fov_circle(sight, (void*)map, (void*)src, map->pX, map->pY, 32);

		u32 i;
		for (i = 0; i < map->num_lights; i++) {
			light_t *l = &map->lights[i];
			if (l->x + l->radius < scrollX || l->x - l->radius > scrollX + 32 ||
					l->y + l->radius < scrollY || l->y - l->radius > scrollY + 24) continue;
			// XXX: need to keep a structure for the fluctuations per-source
			int32 source[3] = { l->x << 12, l->y << 12, l->radius << 12 };
			fov_circle(light, (void*)map, (void*)source, l->x, l->y, l->radius);
			cell_t *cell = &map->cells[l->y*map->w+l->x];
			if (cell->visible) {
				cell->lit = 1<<12; // XXX: change for when coloured lights come
				cell->recall = 1<<12;
				cell->dirty = 2;
			}
		}

		// XXX: more font-specific magical values
		for (y = 0; y < 24; y++)
			for (x = 0; x < 32; x++) {
				cell_t *cell = &map->cells[(y+scrollY)*map->w+(x+scrollX)];
				if (cell->visible && cell->lit > 0) {
					int r = cell->col & 0x001f,
							g = (cell->col & 0x03e0) >> 5,
							b = (cell->col & 0x7c00) >> 10;
					int32 val = max(cell->lit, cell->recall>>1);
					r = mulf32(r<<12, val)>>12;
					g = mulf32(g<<12, val)>>12;
					b = mulf32(b<<12, val)>>12;
					drawcq(x*8,y*8,cell->ch, RGB15(r,g,b));
					cell->lit = 0;
				} else if (cell->recall > 0 && (cell->dirty > 0 || dirty > 0)) {
					int r = cell->col & 0x001f,
							g = (cell->col & 0x03e0) >> 5,
							b = (cell->col & 0x7c00) >> 10;
					if (cell->dirty > 0)
						cell->dirty--;
					int32 val = (cell->recall>>1) - (cell->recall>>3);
					r = mulf32(r<<12, val)>>12;
					g = mulf32(g<<12, val)>>12;
					b = mulf32(b<<12, val)>>12;
					drawcq(x*8,y*8,cell->ch, RGB15(r,g,b));
				}
				if (cell->visible)
					cell->visible = 0;
			}
		drawcq((map->pX-scrollX)*8,(map->pY-scrollY)*8,'@',RGB15(31,31,31));
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
