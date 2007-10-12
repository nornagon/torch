#include "nds.h"
#include <nds/arm9/console.h>
#include <stdio.h>
#include <malloc.h>
#include "mt19937ar.h"
#include "fov.h"
#include "mem.h"

#include "font.h"

#include "test_map.h"

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
	memset32((void*)backbuf, 0, (256*192*2)/4);
}
void clss() {
	memset32((void*)BG_BMP_RAM(0), 0, (256*192*4)/4);
}

void drawc(u16 x, u16 y, u8 c, u16 color) {
	c -= ' '; // XXX: might want to remove this
	color |= BIT(15);
	u8 px,py;
	for (py = 0; py < 8; py++)
		for (px = 0; px < 8; px++) {
			if (fontTiles[8*8*c+8*py+px])
				backbuf[(y+py)*256+x+px] = color;
		}
}
void drawcq(u32 x, u32 y, u32 c, u32 color) { // OPAQUE version (clobbers)
	asm (
			"adr r7, 2f\n"
			"bx r7\n"
			".align 4\n"
			".arm\n"
			"2:\n"
			"sub %2, %2, #32\n"         // c -= ' '
			"add %1, %0, %1, lsl #8\n"  // y = x+y*256
			"orr r8, %3, #32768\n"      // r8 <- col | BIT(15)
			"add r6, %5, %2, lsl #6\n"  // r6 <- &fontTiles[8*8*c]
			"add r7, %4, %1, lsl #1\n"  // r7 <- &backbuf[y*256+x]

			"mov r9, #8\n"       // end vram address

			"loop:\n"
			"ldmia r6!, {r4, r5}\n"   // grab 8 bytes from the font

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
			"movs r5, r5, lsr #1\n"
			"movcs r2, r8\n"
			"movs r5, r5, lsr #8\n"
			"orrcs r2, r8, lsl #16\n"

			"eor r3, r3, r3\n"
			"movs r5, r5, lsr #8\n"
			"movcs r3, r8\n"
			"movs r5, r5, lsr #8\n"
			"orrcs r3, r8, lsl #16\n"

			"stm r7, {r0, r1, r2, r3}\n"
			"add r7, r7, #512\n"
			"subs r9, r9, #1\n"
			"bpl loop\n"

			"adr r3, 3f + 1\n"
			"bx r3\n"
			".thumb\n"
			"3:\n"

			: /* no output */
			: "r"(x), "r"(y), "r"(c), "r"(color), "r"(backbuf), "r"(fontTiles)
			: "r6", "r7", "r8", "r9"
			);
	/*c -= ' '; // XXX: might want to remove this
	color |= BIT(15);
	u8 px,py;
	for (py = 0; py < 8; py++) {
		memset32(&backbuf[(y+py)*256+x], 0, 4);
		for (px = 0; px < 8; px++) {
			if (fontTiles[8*8*c+8*py+px])
				backbuf[(y+py)*256+x+px] = color;
		}
	}*/
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// map stuff

#define MAX_LIGHTS 32

typedef enum {
	T_TREE,
	T_GROUND,
	T_STAIRS,
	T_FIRE,
	NO_TYPE
} CELL_TYPE;

typedef enum {
	L_FIRE,
	NO_LIGHT
} LIGHT_TYPE;

#define D_NONE  0
#define D_NORTH 1
#define D_SOUTH 2
#define D_EAST  4
#define D_WEST  8
#define D_NORTHEAST (D_NORTH|D_EAST)
#define D_SOUTHEAST (D_SOUTH|D_EAST)
#define D_NORTHWEST (D_NORTH|D_WEST)
#define D_SOUTHWEST (D_SOUTH|D_WEST)

#define D_BOTH 0x10
#define D_NORTH_AND_EAST (D_NORTHEAST|D_BOTH)
#define D_SOUTH_AND_EAST (D_SOUTHEAST|D_BOTH)
#define D_NORTH_AND_WEST (D_NORTHWEST|D_BOTH)
#define D_SOUTH_AND_WEST (D_SOUTHWEST|D_BOTH)
typedef unsigned int DIRECTION;

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
	u8 ch;
	u16 col;
	int32 light;
	int32 recall;
	CELL_TYPE type : 8;
	u8 dirty : 2;
	bool visible : 1;
	DIRECTION seen_from : 5;
} cell_t;
/*    ~~    */

/*** map ***/
typedef struct {
	u32 w,h;
	cell_t* cells;
	light_t lights[MAX_LIGHTS];
	u8 num_lights;
	s32 pX,pY, scrollX, scrollY;
	bool torch_on : 1;
} map_t;
/*    ~    */

static inline cell_t *cell_at(map_t *map, int x, int y) {
	return &map->cells[y*map->w+x];
}

void reset_map(map_t* map, CELL_TYPE fill) {
	u32 x, y, w = map->w, h = map->h;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			cell_t* cell = &map->cells[y*w+x];
			cell->type = fill;
			cell->ch = 0;
			cell->col = 0;
			cell->light = 0;
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
			cell_t *cell = cell_at(map, x, y);
			cell->ch = '*';
			cell->col = RGB15(4,31,1);
		}

	x = map->w/2; y = map->h/2;
	u32 i = 8192;
	u32 light1 = genrand_int32() >> 21, // between 0 and 2047
			light2 = light1 + 40;
	for (; i > 0; i--) {
		cell_t *cell = cell_at(map, x, y);
		u32 a = genrand_int32();
		if (i == light1 || i == light2) {
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
	cell_t *cell = cell_at(map, x, y);
	cell->type = T_STAIRS;
	cell->ch = '>';
	cell->col = RGB15(31,31,31);
}

void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds + IPC->time.rtc.minutes*60+IPC->time.rtc.hours*60*60 + IPC->time.rtc.weekday*7*24*60*60));
	clss();
	random_map(map);
}

void load_map(map_t *map, size_t len, unsigned char *desc) {
	s32 x,y;
	reset_map(map, T_GROUND);
	for (y = 0; y < map->h; y++)
		for (x = 0; x < map->w; x++) {
			cell_t *cell = cell_at(map, x, y);
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
		}

	for (x = 0, y = 0; len > 0; len--) {
		u8 c = *desc++;
		cell_t *cell = cell_at(map, x, y);
		switch (c) {
			case '*':
				cell->type = T_TREE;
				cell->ch = '*';
				cell->col = RGB15(4,31,1);
				break;
			case '@':
				map->pX = x;
				map->pY = y;
				break;
			case 'w':
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
				break;
			case '\n':
				x = -1; // gets ++'d later
				y++;
				break;
		}
		x++;
	}
}
//---------------------------------------------------------------------------

u32 vblnks = 0, frames = 0, hblnks = 0;
int vblnkDirty = 0;
void vblank_counter() {
	vblnkDirty = 1;
	vblnks += 1;
}
void hblank_counter() {
	hblnks += 1;
}


inline bool opaque(cell_t* cell) {
	return cell->type == T_TREE;
}


//---------------------------------------------------------------------------
// libfov functions
bool opacity_test(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return true;
	return opaque(cell_at(map, x, y)) || (map->pX == x && map->pY == y);
}
bool sight_opaque(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	// XXX: beware, magical fonting numbers
	if (y < map->scrollY || y >= map->scrollY + 24 || x < map->scrollX || x >= map->scrollX + 32) return true;
	return opaque(cell_at(map, x, y));
}

// the direction from (tx,ty) to (sx,sy)
// or: the faces lit on (tx,ty) by a source at (sx,sy)
inline DIRECTION direction(int sx, int sy, int tx, int ty) {
	if (sx < tx) {
		if (sy < ty) return D_NORTHWEST;
		if (sy > ty) return D_SOUTHWEST;
		return D_WEST;
	}
	if (sx > tx) {
		if (sy < ty) return D_NORTHEAST;
		if (sy > ty) return D_SOUTHEAST;
		return D_EAST;
	}
	if (sy < ty) return D_NORTH;
	if (sy > ty) return D_SOUTH;
	return D_NONE;
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

inline int32 calc_quadratic(int32 dist2, int32 rad2) {
	return (1<<12) - max(0,divf32(dist2,rad2));
}

inline DIRECTION seen_from(map_t *map, DIRECTION d, int x, int y) {
	// Note to self: possible optimisation:
	//   store the 'neighbour kind' in the cell data.
	//
	// There are 2^4 = 16 possible neighbour combinations, and they don't change
	// often (or at all, currently). Could recalculate relatively quickly. Only
	// 5 bits per cell needed.
	bool opa, opb;
	switch (d) {
		case D_NORTHWEST:
		case D_NORTH_AND_WEST:
			opa = opaque(cell_at(map, x, y-1)),
			opb = opaque(cell_at(map, x-1, y));
			if (opa && opb) return D_NORTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_NORTH;
											return D_NORTH_AND_WEST;
			break;
		case D_SOUTHWEST:
		case D_SOUTH_AND_WEST:
			opa = opaque(cell_at(map, x, y+1)),
			opb = opaque(cell_at(map, x-1, y));
			if (opa && opb) return D_SOUTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_SOUTH;
											return D_SOUTH_AND_WEST;
			break;
		case D_NORTHEAST:
		case D_NORTH_AND_EAST:
			opa = opaque(cell_at(map, x, y-1)),
			opb = opaque(cell_at(map, x+1, y));
			if (opa && opb) return D_NORTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_NORTH;
											return D_NORTH_AND_EAST;
			break;
		case D_SOUTHEAST:
		case D_SOUTH_AND_EAST:
			opa = opaque(cell_at(map, x, y+1)),
			opb = opaque(cell_at(map, x+1, y));
			if (opa && opb) return D_SOUTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_SOUTH;
											return D_SOUTH_AND_EAST;
			break;
	}
	return d;
}

void apply_sight(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;
	int32 *src = (int32*)src_;

	// XXX: super ick, magical fonting numbers here
	s32 scrollX = map->scrollX, scrollY = map->scrollY;
	if (x < scrollX || y < scrollY || x > scrollX + 31 || y > scrollY + 23) return;

	cell_t *cell = cell_at(map, x, y);

	DIRECTION d = D_NONE;
	if (opaque(cell))
		d = seen_from(map, direction(map->pX, map->pY, x, y), x, y);
	cell->seen_from = d;

	if (map->torch_on) {
		int32 dx = src[0]-(x<<12),
					dy = src[1]-(y<<12),
					dist2 = mulf32(dx,dx)+mulf32(dy,dy);
		int32 rad = src[2],
					rad2 = mulf32(rad,rad);

		if (dist2 < rad2) {
			int32 intensity = calc_quadratic(dist2, rad2);

			cell->recall = max(intensity, cell->recall);

			// NOTE: here we SET the intensity, we do not add
			cell->light = intensity;

			cell->dirty = 2;
		}
	}
	cell->visible = true;
}

void apply_light(void *map_, int x, int y, int dxblah, int dyblah, void *src_) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return;

	cell_t *cell = cell_at(map, x, y);

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
		int32 intensity = calc_quadratic(dist2, rad2);

		DIRECTION d = D_NONE;
		if (opaque(cell))
			d = seen_from(map, direction(src[0]>>12, src[1]>>12, x, y), x, y);

		if (d & D_BOTH) {
			intensity >>= 1;
			d &= cell->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) cell->light += intensity;
			if (d & D_SOUTH) cell->light += intensity;
			if (d & D_EAST) cell->light += intensity;
			if (d & D_WEST) cell->light += intensity;
			cell->light = min(1<<12, cell->light);
		} else if (cell->seen_from == d) {
			cell->light = min(1<<12, cell->light + intensity);
		}

		cell->recall = max(cell->recall, cell->light);

		cell->dirty = 2;
	}
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqSet(IRQ_HBLANK, hblank_counter);
	irqEnable(IRQ_VBLANK | IRQ_HBLANK);

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
	fov_settings_set_shape(sight, FOV_SHAPE_SQUARE);
	fov_settings_set_opacity_test_function(sight, sight_opaque);
	fov_settings_set_apply_lighting_function(sight, apply_sight);

	fov_settings_type *light = malloc(sizeof(fov_settings_type));
	fov_settings_init(light);
	fov_settings_set_shape(light, FOV_SHAPE_OCTAGON);
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
	map->torch_on = true;

	u32 x, y;
	u32 frm = 0;

	map->scrollX = map->w/2 - 16;
	map->scrollY = map->h/2 - 12;

	int dirty = 2; // whole screen dirty first frame

	u32 level = 0;

	int32 src[5] = { 0, 0, 0, // source x, source y, radius
		0, 0  // scroll x, scroll y
	};

	u32 vc_before;
	u32 counts[10];
	for (vc_before = 0; vc_before < 10; vc_before++) counts[vc_before] = 0;

	while (1) {
		if (!vblnkDirty)
			swiWaitForVBlank();
		vblnkDirty = 0;

		vc_before = hblnks;

		scanKeys();
		u32 down = keysDown();
		if (down & KEY_START) {
			new_map(map);
			map->pX = map->w/2;
			map->pY = map->h/2;
			frm = 0;
			map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
			vblnkDirty = 0;
			dirty = 2;
			level = 0;
			//iprintf("You begin again.\n");
			continue;
		}
		if (down & KEY_SELECT) {
			load_map(map, strlen(test_map), test_map);
			frm = 0;
			map->scrollX = 0; map->scrollY = 0;
			// XXX: beware, here be font-specific values
			if (map->pX - map->scrollX < 8 && map->scrollX > 0)
				map->scrollX = map->pX - 8;
			else if (map->pX - map->scrollX > 24 && map->scrollX < map->w-32)
				map->scrollX = map->pX - 24;
			if (map->pY - map->scrollY < 8 && map->scrollY > 0)
				map->scrollY = map->pY - 8;
			else if (map->pY - map->scrollY > 16 && map->scrollY < map->h-24)
				map->scrollY = map->pY - 16;
			vblnkDirty = 0;
			dirty = 2;
			level = 0;
			continue;
		}
		if (down & KEY_B)
			map->torch_on = !map->torch_on;

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
				map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
				vblnkDirty = 0;
				dirty = 2;
				continue;
			}
			if (dpX || dpY) {
				frm = 5;
				s32 pX = map->pX, pY = map->pY;
				// bumping into a wall takes some time
				if (map->cells[(pY+dpY)*map->w+pX+dpX].type == T_TREE) { // XXX generalize bump test
					dpX = dpY = 0;
					frm = 2;
				} else {
					if (pX + dpX < 0) { dpX = dpY = 0; frm = 2; }
					if (pY + dpY < 0) { dpY = dpX = 0; frm = 2; }
					if (pX + dpX >= map->w) { dpX = dpY = 0; frm = 2; }
					if (pY + dpY >= map->h) { dpY = dpX = 0; frm = 2; }
					map->cells[pY*map->w+pX].dirty = 2;
					map->pX += dpX; pX += dpX;
					map->pY += dpY; pY += dpY;

					// XXX: beware, here be font-specific values
					if (pX - map->scrollX < 8 && map->scrollX > 0) {
						map->scrollX = pX - 8; dirty = 2; //cls();
					} else if (pX - map->scrollX > 24 && map->scrollX < map->w-32) {
						map->scrollX = pX - 24; dirty = 2; //cls();
					}
					if (pY - map->scrollY < 8 && map->scrollY > 0) { 
						map->scrollY = pY - 8; dirty = 2; //cls();
					} else if (pY - map->scrollY > 16 && map->scrollY < map->h-24) {
						map->scrollY = pY - 16; dirty = 2; //cls();
					}
				}
			}
		}
		if (frm > 0)
			frm--;
		counts[0] += hblnks - vc_before;

		vc_before = hblnks;

		if (frames % 4 == 0) {
			// XXX: ick
			src[0] = (map->pX<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[1] = (map->pY<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[2] = (7<<12)+((genrand_gaussian32()&0xfff00000)>>20);
			src[3] = map->scrollX;
			src[4] = map->scrollY;
		}

		fov_circle(sight, (void*)map, (void*)src, map->pX, map->pY, 32);
		counts[1] += hblnks - vc_before;

		vc_before = hblnks;
		u32 i;
		for (i = 0; i < map->num_lights; i++) {
			light_t *l = &map->lights[i];
			if (l->x + l->radius < map->scrollX || l->x - l->radius > map->scrollX + 32 ||
					l->y + l->radius < map->scrollY || l->y - l->radius > map->scrollY + 24) continue;
			// XXX: need to keep a structure for the fluctuations per-source
			int32 source[3] = { l->x << 12, l->y << 12, l->radius << 12 };
			fov_circle(light, (void*)map, (void*)source, l->x, l->y, l->radius);
			cell_t *cell = cell_at(map, l->x, l->y);
			if (cell->visible) {
				cell->light = 1<<12; // XXX: change for when coloured lights come
				cell->recall = 1<<12;
				cell->dirty = 2;
			}
		}
		counts[2] += hblnks - vc_before;

		// XXX: more font-specific magical values
		vc_before = hblnks;
		for (y = 0; y < 24; y++)
			for (x = 0; x < 32; x++) {
				cell_t *cell = &map->cells[(y+map->scrollY)*map->w+(x+map->scrollX)];
				if (cell->visible && cell->light > 0) {
					int r = cell->col & 0x001f,
							g = (cell->col & 0x03e0) >> 5,
							b = (cell->col & 0x7c00) >> 10;
					int32 val = max(cell->light, cell->recall>>1);
					r = mulf32(r<<12, val)>>12;
					g = mulf32(g<<12, val)>>12;
					b = mulf32(b<<12, val)>>12;
					drawcq(x*8,y*8,cell->ch, RGB15(r,g,b));
					cell->light = 0;
				} else if (cell->dirty > 0 || dirty > 0) {
					if (cell->recall > 0) {
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
					} else {
						if (cell->dirty > 0)
							cell->dirty--;
						drawcq(x*8,y*8, ' ', 0); // clear
					}
				}
				if (cell->visible)
					cell->visible = 0;
			}
		drawcq((map->pX-map->scrollX)*8,(map->pY-map->scrollY)*8,'@',RGB15(31,31,31));
		counts[3] += hblnks - vc_before;
		swapbufs();
		if (dirty > 0) {
			dirty--;
			if (dirty > 0)
				cls();
		}
		frames += 1;
		if (vblnks >= 60) {
			iprintf("\x1b[14;14H%02dfps", (frames * 64 - frames * 4) / vblnks);
			iprintf("\x1b[2;0HKeys:    %04d\nSight:   %04d\nLights:  %04d\nDrawing: %04d\n",
					counts[0], counts[1], counts[2], counts[3]);
			for (i = 0; i < 10; i++) counts[i] = 0;
			vblnks = frames = 0;
		}
	}

	return 0;
}
