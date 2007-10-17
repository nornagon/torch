#include "nds.h"
#include <nds/arm9/console.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "mt19937ar.h"
#include "fov.h"
#include "mem.h"
#include "test_map.h"
#include "draw.h"
#include "map.h"


//---------------------------------------------------------------------------
// timers
inline void start_stopwatch() {
	TIMER_CR(0) = TIMER_DIV_1 | TIMER_ENABLE;
}

inline u16 read_stopwatch() {
	TIMER_CR(0) = 0;
	return TIMER_DATA(0);
}
//---------------------------------------------------------------------------


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

// raw divide, you'll have to check DIV_RESULT32 and DIV_BUSY yourself.
static inline void div_32_32_raw(int32 num, int32 den) {
	DIV_CR = DIV_32_32;

	while (DIV_CR & DIV_BUSY);

	DIV_NUMERATOR32 = num;
	DIV_DENOMINATOR32 = den;
}
// beware, if your numerator can't deal with being shifted left 12, you will
// lose bits off the left-hand side!
static inline int32 div_32_32(int32 num, int32 den) {
	div_32_32_raw(num << 12, den);
	while (DIV_CR & DIV_BUSY);
	return DIV_RESULT32;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// map stuff
void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds + IPC->time.rtc.minutes*60+IPC->time.rtc.hours*60*60 + IPC->time.rtc.weekday*7*24*60*60));
	clss();
	random_map(map);
	reset_cache(map);
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


//---------------------------------------------------------------------------
// libfov functions
bool opacity_test(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	if (y < 0 || y >= map->h || x < 0 || x >= map->w) return true;
	return opaque(cell_at(map, x, y)) || (map->pX == x && map->pY == y);
}
bool sight_opaque(void *map_, int x, int y) {
	map_t *map = (map_t*)map_;
	// stop at the edge of the screen
	if (y < map->scrollY || y >= map->scrollY + 24
	    || x < map->scrollX || x >= map->scrollX + 32)
		return true;
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
	return (1<<12) - divf32(dist2,rad2);
}

inline DIRECTION seen_from(map_t *map, DIRECTION d, cell_t *cell) {
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
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_NORTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_WEST;
			break;
		case D_SOUTHWEST:
		case D_SOUTH_AND_WEST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_WEST;
			if (opa && opb) return D_SOUTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_WEST;
			break;
		case D_NORTHEAST:
		case D_NORTH_AND_EAST:
			opa = cell->blocked_from & D_NORTH;
			opb = cell->blocked_from & D_EAST;
			if (opa && opb) return D_NORTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_EAST;
			break;
		case D_SOUTHEAST:
		case D_SOUTH_AND_EAST:
			opa = cell->blocked_from & D_SOUTH;
			opb = cell->blocked_from & D_EAST;
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
	light_t *l = (light_t*)src_;

	// don't bother calculating if we're outside the edge of the screen
	s32 scrollX = map->scrollX, scrollY = map->scrollY;
	if (x < scrollX || y < scrollY || x > scrollX + 31 || y > scrollY + 23) return;

	cell_t *cell = cell_at(map, x, y);

	DIRECTION d = D_NONE;
	if (opaque(cell))
		d = seen_from(map, direction(map->pX, map->pY, x, y), cell);
	cell->seen_from = d;

	if (map->torch_on) {
		// the funny bit-twiddling here is to preserve a few more bits in dx/dy
		// during multiplication. mulf32 is a software multiply, and thus slow.
		int32 dx = ((l->x << 12) - (x << 12)) >> 2,
		      dy = ((l->y << 12) - (y << 12)) >> 2,
		      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
		int32 rad = (l->radius << 12) + l->dr,
		      rad2 = (rad * rad) >> 12;

		if (dist2 < rad2) {
			div_32_32_raw(dist2<<8, rad2>>4);
			cache_t *cache = cache_at(map, x, y); // load the cache while waiting for the division
			while (DIV_CR & DIV_BUSY);
			int32 intensity = (1<<12) - DIV_RESULT32;

			cell->light = intensity;

			cache->lr = l->r;
			cache->lg = l->g;
			cache->lb = l->b;
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
	light_t *l = (light_t*)src_;
	int32 dx = ((l->x << 12) - (x << 12)) >> 2, // shifting is for accuracy reasons
	      dy = ((l->y << 12) - (y << 12)) >> 2,
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = (l->radius << 12) + l->dr,
	      rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		div_32_32_raw(dist2<<8, rad2>>4);

		DIRECTION d = D_NONE;
		if (opaque(cell))
			d = seen_from(map, direction(l->x, l->y, x, y), cell);
		cache_t *cache = cache_at(map, x, y);

		while (DIV_CR & DIV_BUSY);
		int32 intensity = (1<<12) - DIV_RESULT32;

		if (d & D_BOTH || cell->seen_from & D_BOTH) {
			intensity >>= 1;
			d &= cell->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) cell->light += intensity;
			if (d & D_SOUTH) cell->light += intensity;
			if (d & D_EAST) cell->light += intensity;
			if (d & D_WEST) cell->light += intensity;
		} else if (cell->seen_from == d)
			cell->light += intensity;

		cache->lr += (l->r * intensity) >> 12;
		cache->lg += (l->g * intensity) >> 12;
		cache->lb += (l->b * intensity) >> 12;
	}
}
//---------------------------------------------------------------------------


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


//---------------------------------------------------------------------------
int main(void) {
//---------------------------------------------------------------------------
	irqInit();
	irqSet(IRQ_VBLANK, vblank_counter);
	irqSet(IRQ_HBLANK, hblank_counter);
	irqEnable(IRQ_VBLANK | IRQ_HBLANK);

	//touchPosition touchXY;

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

	TIMER_DATA(0) = 0;

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
	init_genrand(seed);

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

	light_t player_light = {
		.x = map->pX,
		.y = map->pY,
		.dx = 0,
		.dy = 0,
		.r = 1<<12,
		.g = 1<<12,
		.b = 1<<12,
		.radius = 7,
		.dr = 0,
		.type = T_FIRE
	};

	u32 vc_before;
	u32 counts[10];
	for (vc_before = 0; vc_before < 10; vc_before++) counts[vc_before] = 0;

	u32 low_luminance = 0;

	DIRECTION just_scrolled = 0;

	while (1) {
		u32 vc_begin = hblnks;
		if (!vblnkDirty)
			swiWaitForVBlank();
		vblnkDirty = 0;

		vc_before = hblnks;

		// TODO: is DMA actually asynchronous?
		bool copying = false;

		if (just_scrolled) {
			scroll_screen(map, just_scrolled);
			copying = true;
			just_scrolled = 0;
		}

		scanKeys();
		u32 down = keysDown();
		if (down & KEY_START) {
			new_map(map);
			map->pX = map->w/2;
			map->pY = map->h/2;
			frm = 5;
			map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
			vblnkDirty = 0;
			reset_cache(map);
			dirty = 2;
			level = 0;
			low_luminance = 0;
			//iprintf("You begin again.\n");
			continue;
		}
		if (down & KEY_SELECT) {
			load_map(map, strlen(test_map), test_map);
			frm = 5;
			map->scrollX = 0; map->scrollY = 0;
			if (map->pX - map->scrollX < 8 && map->scrollX > 0)
				map->scrollX = map->pX - 8;
			else if (map->pX - map->scrollX > 24 && map->scrollX < map->w-32)
				map->scrollX = map->pX - 24;
			if (map->pY - map->scrollY < 8 && map->scrollY > 0)
				map->scrollY = map->pY - 8;
			else if (map->pY - map->scrollY > 16 && map->scrollY < map->h-24)
				map->scrollY = map->pY - 16;
			vblnkDirty = 0;
			reset_cache(map); // cache is origin-agnostic
			dirty = 2;
			level = 0;
			low_luminance = 0;
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
				map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
				vblnkDirty = 0;
				reset_cache(map);
				dirty = 2;
				continue;
			}
			s32 pX = map->pX, pY = map->pY;
			// bumping into a wall takes some time
			if (pX + dpX < 0) { dpX = dpY = 0; frm = 2; }
			if (pY + dpY < 0) { dpY = dpX = 0; frm = 2; }
			if (pX + dpX >= map->w) { dpX = dpY = 0; frm = 2; }
			if (pY + dpY >= map->h) { dpY = dpX = 0; frm = 2; }
			if (dpX || dpY) {
				frm = 5;
				cell_t *cell = cell_at(map, pX + dpX, pY + dpY);
				if (opaque(cell)) { // XXX: is opacity equivalent to solidity?
					int32 rec = max(cell->recall, (1<<11)); // Eh? What's that?! I felt something!
					if (rec != cell->recall) {
						cell->recall = rec;
						cache_at(map, pX + dpX, pY + dpY)->dirty = 2;
					}
					dpX = dpY = 0;
					frm = 2;
				} else {
					cache_at(map, pX, pY)->dirty = 2;
					map->pX += dpX; pX += dpX;
					map->pY += dpY; pY += dpY;

					s32 dsX = 0, dsY = 0;
					if (pX - map->scrollX < 8 && map->scrollX > 0) { // it's just a scroll to the left
						dsX = (pX - 8) - map->scrollX;
						map->scrollX = pX - 8;
					} else if (pX - map->scrollX > 24 && map->scrollX < map->w-32) {
						dsX = (pX - 24) - map->scrollX;
						map->scrollX = pX - 24;
					}

					if (pY - map->scrollY < 8 && map->scrollY > 0) { 
						dsY = (pY - 8) - map->scrollY;
						map->scrollY = pY - 8;
					} else if (pY - map->scrollY > 16 && map->scrollY < map->h-24) {
						dsY = (pY - 16) - map->scrollY;
						map->scrollY = pY - 16;
					}

					if (dsX || dsY) {
						// XXX: look at generalising scroll function

						//if (abs(dsX) > 1 || abs(dsY) > 1) dirty = 2;
						// the above should never be the case. if it ever is, make sure
						// scrolling doesn't happen *as well as* full-screen dirtying.

						map->cacheX += dsX;
						map->cacheY += dsY;
						if (map->cacheX < 0) map->cacheX += 32;
						if (map->cacheY < 0) map->cacheY += 24;
						DIRECTION dir = direction(dsX, dsY, 0, 0);
						scroll_screen(map, dir);
						copying = true;
						just_scrolled = dir;
					}
				}
			}
		}
		if (frm > 0)
			frm--;
		counts[0] += hblnks - vc_before;

		vc_before = hblnks;

		u32 i;

		if (frames % 4 == 0) {
			// have the light lag a bit behind the player
			player_light.x = map->pX;
			player_light.y = map->pY;

			player_light.dx = genrand_gaussian32()>>20;
			player_light.dy = genrand_gaussian32()>>20;
			player_light.dr = genrand_gaussian32()>>20;

			u32 m = frames % 8 == 0;
			for (i = 0; i < map->num_lights; i++) {
				light_t *l = &map->lights[i];
				if (flickers(l)) {
					l->dr = (genrand_gaussian32() >> 19) - 4096;
					if (m) {
						m = genrand_int32();
						if (m < (u32)(0xffffffff*0.6)) l->dx = l->dy = 0;
						else {
							if (m < (u32)(0xffffffff*0.7)) {
								l->dx = 0; l->dy = 1;
							} else if (m < (u32)(0xffffffff*0.8)) {
								l->dx = 0; l->dy = -1;
							} else if (m < (u32)(0xffffffff*0.9)) {
								l->dx = 1; l->dy = 0;
							} else {
								l->dx = -1; l->dy = 0;
							}
							if (opaque(cell_at(map, l->x + l->dx, l->y + l->dy)))
								l->dx = l->dy = 0;
						}
					}
				}
			}
		}

		fov_circle(sight, (void*)map, (void*)&player_light, map->pX, map->pY, 32);
		counts[1] += hblnks - vc_before;

		vc_before = hblnks;
		for (i = 0; i < map->num_lights; i++) {
			light_t *l = &map->lights[i];

			// don't bother if it's completely outside the screen.
			if (l->x + l->radius + (l->dr >> 12) < map->scrollX ||
			    l->x - l->radius - (l->dr >> 12) > map->scrollX + 32 ||
			    l->y + l->radius + (l->dr >> 12) < map->scrollY ||
			    l->y - l->radius - (l->dr >> 12) > map->scrollY + 24) continue;

			fov_circle(light, (void*)map, (void*)l, l->x + l->dx, l->y + l->dy, l->radius + 2);
			cell_t *cell = cell_at(map, l->x + l->dx, l->y + l->dy);
			if (cell->visible) {
				cell->light = low_luminance + (1<<12);
				cache_t *cache = cache_at(map, l->x + l->dx, l->y + l->dy);
				cache->lr = l->r;
				cache->lg = l->g;
				cache->lb = l->b;
				cell->recall = 1<<12;
			}
		}
		counts[2] += hblnks - vc_before;

		if (copying)
			while (dmaBusy(3));

		//------------------------------------------------------------------------
		// draw loop

		vc_before = hblnks;
		u32 twiddling = 0;
		u32 drawing = 0;

		s32 adjust = 0;
		u32 max_luminance = 0;

		cell_t *cell = cell_at(map, map->scrollX, map->scrollY);
		for (y = 0; y < 24; y++) {
			for (x = 0; x < 32; x++) {
				cache_t *cache = cache_at(map, map->scrollX + x, map->scrollY + y);

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

					u32 r = cell->col & 0x001f,
					    g = (cell->col & 0x03e0) >> 5,
					    b = (cell->col & 0x7c00) >> 10;
					int32 minval = cell->type == T_GROUND ? 0 : (cell->recall>>2);
					int32 val = max(minval, cell->light);
					int32 rval = cache->lr,
					      gval = cache->lg,
					      bval = cache->lb;
					if (rval >> 8 != cache->last_lr ||
					    gval >> 8 != cache->last_lg ||
					    bval >> 8 != cache->last_lb ||
					    cache->last_light != cell->light >> 8) {
						int32 maxcol = max(rval,max(bval,gval));
						// cap [rgb]val at 1<<12, then scale by val
						rval = (div_32_32(rval,maxcol) * val) >> 12;
						gval = (div_32_32(gval,maxcol) * val) >> 12;
						bval = (div_32_32(bval,maxcol) * val) >> 12;
						rval = max(rval, minval);
						gval = max(gval, minval);
						bval = max(bval, minval);
						// multiply the colour through fixed-point 20.12 for a bit more accuracy
						r = ((r<<12) * rval) >> 24;
						g = ((g<<12) * gval) >> 24;
						b = ((b<<12) * bval) >> 24;
						twiddling += read_stopwatch();
						start_stopwatch();
						u16 col_to_draw = RGB15(r,g,b);
						u16 last_col = cache->last_col;
						if (col_to_draw != last_col) {
							drawcq(x*8, y*8, cell->ch, col_to_draw);
							cache->last_col = col_to_draw;
							cache->last_lr = cache->lr >> 8;
							cache->last_lg = cache->lg >> 8;
							cache->last_lb = cache->lb >> 8;
							cache->last_light = cell->light >> 8;
							cache->dirty = 2;
						} else if (cache->dirty > 0 || dirty > 0) {
							drawcq(x*8, y*8, cell->ch, col_to_draw);
							if (cache->dirty > 0)
								cache->dirty--;
						}
						drawing += read_stopwatch();
					} else {
						twiddling += read_stopwatch();
						start_stopwatch();
						if (cache->dirty > 0 || dirty > 0) {
							drawcq(x*8, y*8, cell->ch, cache->last_col);
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
					if (cell->recall > 0 && cell->type != T_GROUND) {
						start_stopwatch();
						u32 r = cell->col & 0x001f,
						    g = (cell->col & 0x03e0) >> 5,
						    b = (cell->col & 0x7c00) >> 10;
						int32 val = (cell->recall>>2);
						r = ((r<<12) * val) >> 24;
						g = ((g<<12) * val) >> 24;
						b = ((b<<12) * val) >> 24;
						cache->last_col = RGB15(r,g,b);
						cache->last_lr = 0;
						cache->last_lg = 0;
						cache->last_lb = 0;
						cache->last_light = 0;
						twiddling += read_stopwatch();
						start_stopwatch();
						drawcq(x*8, y*8, cell->ch, RGB15(r,g,b));
						drawing += read_stopwatch();
					} else {
						drawcq(x*8, y*8, ' ', 0); // clear
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
			cell += map->w - 32;
		}

		low_luminance += max(adjust*2, -low_luminance);
		if (low_luminance > 0 && max_luminance < low_luminance + (1<<12))
			low_luminance -= min(40,low_luminance);
		iprintf("\x1b[10;8H      \x1b[10;0Hadjust: %d\nlow luminance: %04x", adjust, low_luminance);
		iprintf("\x1b[13;0Hdrawing: %05x\ntwiddling: %05x", drawing, twiddling);
		//------------------------------------------------------------------------


		drawcq((map->pX-map->scrollX)*8, (map->pY-map->scrollY)*8, '@', RGB15(31,31,31));
		counts[3] += hblnks - vc_before;
		swapbufs();
		if (dirty > 0) {
			dirty--;
			if (dirty > 0)
				cls();
		}
		counts[4] += hblnks - vc_begin;
		frames += 1;
		if (vblnks >= 60) {
			iprintf("\x1b[0;19H%02dfps", (frames * 64 - frames * 4) / vblnks);
			iprintf("\x1b[2;0HKeys:    %05d\nSight:   %05d\nLights:  %05d\nDrawing: %05d\n",
					counts[0], counts[1], counts[2], counts[3]);
			iprintf("         -----\nLeft:    %05d\n", counts[4] - counts[0] - counts[1] - counts[2] - counts[3]);
			for (i = 0; i < 10; i++) counts[i] = 0;
			vblnks = frames = 0;
		}
	}

	return 0;
}
