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
// map stuff
void new_map(map_t *map) {
	init_genrand(genrand_int32() ^ (IPC->time.rtc.seconds + IPC->time.rtc.minutes*60+IPC->time.rtc.hours*60*60 + IPC->time.rtc.weekday*7*24*60*60));
	clss();
	random_map(map);
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

inline bool flickers(light_t *light) {
	return light->type == L_FIRE;
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
	return (1<<12) - divf32(dist2,rad2);
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
			opa = opaque(cell_at(map, x, y-1));
			opb = opaque(cell_at(map, x-1, y));
			if (opa && opb) return D_NORTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_WEST;
			break;
		case D_SOUTHWEST:
		case D_SOUTH_AND_WEST:
			opa = opaque(cell_at(map, x, y+1));
			opb = opaque(cell_at(map, x-1, y));
			if (opa && opb) return D_SOUTHWEST;
			if (opa)        return D_WEST;
			if (opb)        return D_SOUTH;
			                return D_SOUTH_AND_WEST;
			break;
		case D_NORTHEAST:
		case D_NORTH_AND_EAST:
			opa = opaque(cell_at(map, x, y-1));
			opb = opaque(cell_at(map, x+1, y));
			if (opa && opb) return D_NORTHEAST;
			if (opa)        return D_EAST;
			if (opb)        return D_NORTH;
			                return D_NORTH_AND_EAST;
			break;
		case D_SOUTHEAST:
		case D_SOUTH_AND_EAST:
			opa = opaque(cell_at(map, x, y+1));
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
		// the funny bit-twiddling here is to preserve a few more bits in dx/dy
		// during multiplication. mulf32 is a software multiply, and thus slow.
		int32 dx = (src[0] - (x << 12)) >> 2,
		      dy = (src[1] - (y << 12)) >> 2,
		      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
		int32 rad = src[2];
		int32 rad2 = (rad * rad) >> 12;

		if (dist2 < rad2) {
			int32 intensity = calc_quadratic(dist2, rad2);

			//cell->recall = max(intensity, cell->recall);

			cell->light = intensity;

			cell->lr = 1<<12;
			cell->lg = 1<<12;
			cell->lb = 1<<12;

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
	// fires light themselves
	//if (cell->type == T_FIRE) return;

	// XXX: this function is pretty much identical to apply_sight... should
	// maybe merge them.
	//int32 *src = (int32*)src_;
	light_t *l = (light_t*)src_;
	int32 dx = ((l->x << 12) - (x << 12)) >> 2, // shifting is for accuracy reasons
	      dy = ((l->y << 12) - (y << 12)) >> 2,
	      dist2 = ((dx * dx) >> 8) + ((dy * dy) >> 8);
	int32 rad = (l->radius << 12) + l->dr,
	      rad2 = (rad * rad) >> 12;

	if (dist2 < rad2) {
		int32 intensity = calc_quadratic(dist2, rad2);

		DIRECTION d = D_NONE;
		if (opaque(cell))
			d = seen_from(map, direction(l->x, l->y, x, y), x, y);

		if (d & D_BOTH || cell->seen_from & D_BOTH) {
			intensity >>= 1;
			d &= cell->seen_from;
			// only two of these should be set at maximum.
			if (d & D_NORTH) cell->light += intensity;
			if (d & D_SOUTH) cell->light += intensity;
			if (d & D_EAST) cell->light += intensity;
			if (d & D_WEST) cell->light += intensity;
			//cell->light = min(1<<12, cell->light);
		} else if (cell->seen_from == d)
			cell->light += intensity; // = min(1<<12, cell->light + intensity);

		//cell->recall = max(cell->recall, cell->light);
		cell->lr += (l->r * intensity) >> 12;
		cell->lg += (l->g * intensity) >> 12;
		cell->lb += (l->b * intensity) >> 12;

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

	u32 low_luminance = 0;

	while (1) {
		u32 vc_begin = hblnks;
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
			low_luminance = 0;
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
				// TODO: make so screen doesn't jump on down-stairs
				map->scrollX = map->w/2 - 16; map->scrollY = map->h/2 - 12;
				vblnkDirty = 0;
				dirty = 2;
				continue;
			}
			if (dpX || dpY) {
				frm = 5;
				s32 pX = map->pX, pY = map->pY;
				cell_t *cell = cell_at(map, pX + dpX, pY + dpY);
				// bumping into a wall takes some time
				if (opaque(cell)) { // XXX: is opacity equivalent to solidity?
					int32 rec = max(cell->recall, (1<<11)); // Eh? What's that?! I felt something!
					if (rec != cell->recall) {
						cell->recall = rec;
						cell->dirty = 2;
					}
					dpX = dpY = 0;
					frm = 2;
				} else {
					if (pX + dpX < 0) { dpX = dpY = 0; frm = 2; }
					if (pY + dpY < 0) { dpY = dpX = 0; frm = 2; }
					if (pX + dpX >= map->w) { dpX = dpY = 0; frm = 2; }
					if (pY + dpY >= map->h) { dpY = dpX = 0; frm = 2; }
					cell_at(map, pX, pY)->dirty = 2;
					map->pX += dpX; pX += dpX;
					map->pY += dpY; pY += dpY;

					// XXX: beware, here be font-specific values
					if (pX - map->scrollX < 8 && map->scrollX > 0) {
						map->scrollX = pX - 8; dirty = 2;
					} else if (pX - map->scrollX > 24 && map->scrollX < map->w-32) {
						map->scrollX = pX - 24; dirty = 2;
					}
					if (pY - map->scrollY < 8 && map->scrollY > 0) { 
						map->scrollY = pY - 8; dirty = 2;
					} else if (pY - map->scrollY > 16 && map->scrollY < map->h-24) {
						map->scrollY = pY - 16; dirty = 2;
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
			// XXX: ick
			src[0] = (map->pX<<12)+(genrand_gaussian32()>>20);
			src[1] = (map->pY<<12)+(genrand_gaussian32()>>20);
			src[2] = (7<<12)+(genrand_gaussian32()>>20);
			src[3] = map->scrollX;
			src[4] = map->scrollY;

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

		fov_circle(sight, (void*)map, (void*)src, map->pX, map->pY, 32);
		counts[1] += hblnks - vc_before;

		vc_before = hblnks;
		for (i = 0; i < map->num_lights; i++) {
			light_t *l = &map->lights[i];
			if (l->x + l->radius < map->scrollX || l->x - l->radius > map->scrollX + 32 ||
					l->y + l->radius < map->scrollY || l->y - l->radius > map->scrollY + 24) continue;

			//int32 source[3] = { l->x << 12, l->y << 12 , (l->radius << 12) + l->dr };
			cell_t *cell = cell_at(map, l->x, l->y);
			fov_circle(light, (void*)map, (void*)l, l->x + l->dx, l->y + l->dy, l->radius + 2);
			cell = cell_at(map, l->x + l->dx, l->y + l->dy);
			if (cell->visible) {
				cell->light = low_luminance + (1<<12); // XXX: change for when coloured lights come
				cell->recall = 1<<12;
				cell->dirty = 2;
			}
		}
		counts[2] += hblnks - vc_before;


		//------------------------------------------------------------------------
		// draw loop

		// XXX: more font-specific magical values
		vc_before = hblnks;

		s32 adjust = 0;
		u32 max_luminance = 0;
		for (y = 0; y < 24; y++)
			for (x = 0; x < 32; x++) {
				cell_t *cell = cell_at(map, x+map->scrollX, y+map->scrollY);
				if (cell->visible && cell->light > 0) {
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
					int32 minval = cell->type == T_GROUND ? 0 : ((cell->recall>>1) - (cell->recall>>3));
					int32 val = max(minval, cell->light);
					int32 rval = cell->lr,
					      gval = cell->lg,
					      bval = cell->lb;
					int32 maxcol = max(rval,max(bval,gval));
					// cap [rgb]val at 1<<12, then scale by val
					rval = (divf32(rval,maxcol) * val) >> 12;
					gval = (divf32(gval,maxcol) * val) >> 12;
					bval = (divf32(bval,maxcol) * val) >> 12;
					rval = max(rval, minval);
					gval = max(gval, minval);
					bval = max(bval, minval);
					// multiply the colour through fixed-point 20.12 for a bit more accuracy
					r = ((r<<12) * rval) >> 24;
					g = ((g<<12) * gval) >> 24;
					b = ((b<<12) * bval) >> 24;
					drawcq(x*8, y*8, cell->ch, RGB15(r,g,b));
					cell->light = 0;
				} else if (cell->dirty > 0 || dirty > 0) {
					if (cell->recall > 0 && cell->type != T_GROUND) {
						u32 r = cell->col & 0x001f,
						    g = (cell->col & 0x03e0) >> 5,
						    b = (cell->col & 0x7c00) >> 10;
						int32 val = (cell->recall>>1) - (cell->recall>>3);
						r = ((r<<12) * val) >> 24;
						g = ((g<<12) * val) >> 24;
						b = ((b<<12) * val) >> 24;
						drawcq(x*8, y*8, cell->ch, RGB15(r,g,b));
					} else
						drawcq(x*8, y*8, ' ', 0); // clear
					if (cell->dirty > 0)
						cell->dirty--;
				}
				if (cell->visible)
					cell->visible = 0;
			}

		low_luminance += max(adjust*2, -low_luminance);
		if (low_luminance > 0 && max_luminance < low_luminance + (1<<12))
			low_luminance -= min(40,low_luminance);
		iprintf("\x1b[10;8H      \x1b[10;0Hadjust: %d\nlow luminance: %04x", adjust, low_luminance);
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
			iprintf("\x1b[14;14H%02dfps", (frames * 64 - frames * 4) / vblnks);
			iprintf("\x1b[2;0HKeys:    %05d\nSight:   %05d\nLights:  %05d\nDrawing: %05d\n",
					counts[0], counts[1], counts[2], counts[3]);
			iprintf("         -----\nLeft:    %05d\n", counts[4] - counts[0] - counts[1] - counts[2] - counts[3]);
			for (i = 0; i < 10; i++) counts[i] = 0;
			vblnks = frames = 0;
		}
	}

	return 0;
}
