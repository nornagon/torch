#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"
#include "engine.h"
#include "recalc.h"
#include "lightsource.h"
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "gfxPrimitives.h"
#include "assert.h"

#include "entities/creature.h"

static inline void set_tile(s16 x, s16 y, int type) {
	game.map.at(x,y)->type = type;
	mapel *m = torch.buf.at(x,y);
	m->recall = 0; m->col = terraindesc[type].color; m->ch = terraindesc[type].ch;
	blockel *b = game.map.block.at(x,y);
	b->opaque = terraindesc[type].opaque;
}
// helper function for passing to the gfxPrimitives generics
static void set_tile_i(s16 x, s16 y, void* type) {
	set_tile(x,y,(int)type);
}

// The algorithm here is to start at a middle point and throw out radial lines
// of random length. I think some Angband variant used it to draw lakes; this
// function uses it to draw two circles of trees (with some hacks for ensuring
// it's possible to get in and out of them).
/*void haunted_grove(s16 cx, s16 cy) {
	int r0 = 7, r1 = 15;
	int w0 = 3, w1 = 4;

	unsigned int playerpos = ((rand32() & 0x1ff) / 8) * 8;
	for (unsigned int t = 0; t < 0x1ff; t += 8) {
		int r = rand32() % 5;
		int x = COS[t], y = SIN[t];
		bresenham(cx, cy, cx + ((x*r) >> 12), cy + ((y*r) >> 12), set_tile, (void*)WATER);
		if (t == playerpos) {
			unsigned int extra = rand8() % 3;
			game.player.x = cx + ((x*(5+extra)) >> 12);
			game.player.y = cy + ((y*(5+extra)) >> 12);
		}
	}

	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = (rand8() & 3) + r0;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w0)) >> 12), cy + ((y*(r+w0)) >> 12), set_tile, (void*)TREE);
	}
	unsigned int firepos = ((rand32() & 0x1ff) / 4) * 4;
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int x = COS[t], y = SIN[t];
		if (t == firepos) {
			int px = cx + ((x*(r0+w0+1)) >> 12),
			    py = cy + ((y*(r0+w0+1)) >> 12);
			set_tile(px, py, (void*)FIRE);
			lightsource *l = new_light(8<<12, (int)(0.9*(1<<12)), (int)(0.3*(1<<12)), (int)(0.1*(1<<12)));
			l->x = px<<12; l->y = py<<12;
			game.map.lights.push(l);
		}
	}
	for (unsigned int t = 0; t < 0x1ff; t += 2) {
		int a = rand8();
		int r = (a & 3) + r1; a >>= 2;
		int x = COS[t], y = SIN[t];
		if (t % 16 > (a & 3))
			bresenham(cx + ((x*r) >> 12), cy + ((y*r) >> 12),
			               cx + ((x*(r+w1)) >> 12), cy + ((y*(r+w1)) >> 12), set_tile, (void*)TREE);
	}
	u16 k = rand16() & 0x1ff;
	int x = COS[k], y = SIN[k];
	bresenham(cx + ((x*r0) >> 12), cy + ((y*r0) >> 12),
	               cx + ((x*(r0+w0)) >> 12), cy + ((y*(r0+w0)) >> 12), set_tile, (void*)GROUND);
	k = rand16() & 0x1ff;
	x = COS[k]; y = SIN[k];
	bresenham(cx + ((x*r1) >> 12), cy + ((y*r1) >> 12),
	               cx + ((x*(r1+w1)) >> 12), cy + ((y*(r1+w1)) >> 12), set_tile, (void*)GROUND);

	int nTraps = 4 + (rand16() % 10);
	for (int i = 0; i < nTraps; i++) {
		int x, y;
		do {
			int theta = rand16() & 0x1ff;
			int r = r0 + w0 + (rand16() % 6);
			x = cx + ((COS[theta]*r) >> 12), y = cy + ((SIN[theta]*r) >> 12);
		} while (!(game.map.at(x,y)->type == GROUND && !game.map.occupied(x,y)));
		Node<Creature> trap(new NodeV<Creature>);
		trap->type = VENUS_FLY_TRAP;
		trap->hp = creaturedesc[trap->type].max_hp;
		trap->cooldown = 0;
		trap->setPos(x,y);
		game.map.at(x,y)->creature = trap;
		game.monsters.push(trap);
	}
}*/

void drop_rock(s16 x, s16 y, void *info) {
	Cell *l = game.map.at(x,y);
	if (rand32() % 10 == 0 && !game.map.solid(x,y)) {
		Node<Object> on(new NodeV<Object>);
		on->type = ROCK;
		stack_item_push(l->objects, on);
	}
}

// drop rocks in the vicinity around a specified point
/*void drop_rocks(s16 ax, s16 ay, int r) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(ax + ((x*r) >> 12), ay + ((y*r) >> 12),
			               ax + ((x*(r+4)) >> 12), ay + ((y*(r+4)) >> 12), drop_rock, NULL);
	}
}*/

struct Point { int x, y; };

// true if there's a path of non-solid tiles from any point on the map to every
// other point on the map.
// TODO: write a checkPath function that takes two specific points and checks
// connectivity between just those points. Useful for, eg, checking whether
// it's possible for the player to reach the stairs.
bool checkConnected() {
	int x = 0, y = 0;
	int w = torch.buf.getw(), h = torch.buf.geth();
	bool done = false;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (!game.map.solid(x,y)) { done = true; break; }
		}
		if (done) break;
	}
	assert(x < w && y < h);

	bool *marks = new bool[w*h];
	memset(marks, 0, w*h*sizeof(bool));

	List<Point> queue;
	queue.push((Point){ x, y });

	while (!queue.empty()) {
		NodeV<Point>* Np = queue.pop();
		Point p = Np->data;
		for (int dx = -1; dx <= 1; dx++) {
			for (int dy = -1; dy <= 1; dy++) {
				if (!marks[(p.y+dy)*w+p.x+dx] && !game.map.solid(p.x+dx,p.y+dy)) {
					marks[p.x + dx + w*(p.y+dy)] = true;
					Point pp = { p.x+dx, p.y+dy };
					queue.push(pp);
				}
			}
		}
		Np->free();
	}

	NodeV<Point>::pool.flush_free();

	done = false;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (!game.map.solid(x,y) && !marks[y*w+x]) {
				done = true; break;
			}
		}
		if (done) break;
	}

	delete [] marks;

	if (x < w && y < h) {
		return false;
	} else
		return true;
}



// These next few functions are for a very flexible cellular automata
// generation method described by
// http://roguebasin.roguelikedevelopment.org/index.php?title=Cellular_Automata_Method_for_Generating_Random_Cave-Like_Levels
// and http://pixelenvy.ca/wa/ca_cave.html
// Essentially, you run one or more cycles in each of which you add or remove
// some kind of tile according to some rule involving the nearby tiles. An
// example would be the 4-5 rule, which means that tiles with < 4 nearby
// similar tiles 'die' and become ground tiles, and tiles with > 5 friends
// become that type. If the tile has 4 or 5 similar neighbours, leave it as is.

// count tiles of type ty in the (square) vicinity
int countFoo(s16 x, s16 y, s16 r, int ty) {
	int count = 0;
	int w = game.map.getw(), h = game.map.geth();
	int dxstart = (x-r<0?0:x-r), dystart = (y-r<0?0:y-r);
	int dxend = (x+r>w-1?w-1:x+r), dyend = (y+r>h-1?h-1:y+r);
	for (int dx = dxstart; dx <= dxend; dx++)
		for (int dy = dystart; dy <= dyend; dy++)
			if (game.map.at(dx,dy)->type == ty)
				count++;
	return count;
}

int countTrees(s16 x, s16 y, s16 r) {
	return countFoo(x, y, r, TREE);
}

void AddStars() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.at(x,y)->type == TERRAIN_NONE && rand8() < 10) {
				// light
				Node<lightsource> li(new NodeV<lightsource>);
				li->set(1, 1<<12, 1<<12, 1<<12);
				li->x = x<<12; li->y = y<<12;
				game.map.lights.push(li);

				// object
				Node<Object> on = addObject(x,y,STAR);
				on->quantity = 0;

				// animation
				Node<Animation> ani(new NodeV<Animation>);
				ani->obj = on;
				ani->frame = 0;
				ani->x = x; ani->y = y;
				game.map.animations.push(ani);
			}
		}
	}
}

void CATrees() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (!game.map.solid(x,y) && (rand4() < 6)) set_tile(x,y, TREE);
		}
	}
	bool *next = new bool[torch.buf.geth()*torch.buf.getw()];
	for (int i = 0; i < 4; i++) {
		memset(next, 0, torch.buf.geth()*torch.buf.getw()*sizeof(bool));
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (countTrees(x,y,2) <= 3 || countTrees(x,y,1) >= 5) next[y*torch.buf.getw()+x] = true;
				else next[y*torch.buf.getw()+x] = false;
			}
		}
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (!game.map.solid(x,y) || game.map.at(x,y)->type == TREE) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, TREE);
					else
						set_tile(x,y, GROUND);
				}
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		memset(next, 0, torch.buf.geth()*torch.buf.getw()*sizeof(bool));
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (countTrees(x,y,1) >= 5) next[y*torch.buf.getw()+x] = true;
				else next[y*torch.buf.getw()+x] = false;
			}
		}
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (!game.map.solid(x,y) || game.map.at(x,y)->type == TREE) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, TREE);
					else
						set_tile(x,y, GROUND);
				}
			}
		}
	}
	delete [] next;
}

void CALakes() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (!game.map.solid(x,y) && (rand8() < 23) && countFoo(x,y,6,GLASS)) set_tile(x,y, WATER);
		}
	}
	bool *next = new bool[torch.buf.geth()*torch.buf.getw()];
	for (int i = 0; i < 4; i++) {
		memset(next, 0, torch.buf.geth()*torch.buf.getw()*sizeof(bool));
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (countFoo(x,y,1,WATER) >= 2) next[y*torch.buf.getw()+x] = true;
				else next[y*torch.buf.getw()+x] = false;
			}
		}
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				//if (!game.map.at(x,y)->desc()->solid || game.map.at(x,y)->type == WATER) {
				if (game.map.at(x,y)->type != GLASS && game.map.at(x,y)->type != 0) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, WATER);
					else if (game.map.at(x,y)->type == WATER)
						set_tile(x,y, GROUND);
				}
			}
		}
	}
	for (int i = 0; i < 3; i++) {
		memset(next, 0, torch.buf.geth()*torch.buf.getw()*sizeof(bool));
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (countFoo(x,y,1,WATER) >= 4 - countFoo(x,y,1,GLASS) &&
						countFoo(x,y,2,WATER) >= 16 - countFoo(x,y,2,GLASS)) next[y*torch.buf.getw()+x] = true;
				else next[y*torch.buf.getw()+x] = false;
			}
		}
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (!game.map.solid(x,y) || game.map.at(x,y)->type == WATER) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, WATER);
					else
						set_tile(x,y, GROUND);
				}
			}
		}
	}
	delete [] next;
	int w = 0;
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.at(x,y)->type == WATER) w++;
		}
	}
	printf("%d water cells. ", w);
}

void AddGrass() {
	for (int i = 0; i < 4; i++) {
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (game.map.at(x,y)->type != GROUND) continue;
				unsigned int nWater1 = countFoo(x,y,1,WATER),
				             nWater2 = countFoo(x,y,2,WATER),
				             nGrass1 = countFoo(x,y,1,GRASS),
				             nGrass2 = countFoo(x,y,2,GRASS);
				if (rand4() < nWater1 || (rand8() & 0x1f) < nWater2 ||
				    rand4() < nGrass1/2 || (rand8() & 0x1f) < nGrass2/2 ) {
					set_tile(x,y, GRASS);
				}
			}
		}
	}
}

void Inhabit() {
	int ntries = 0;
	for (unsigned int i = 0; i < 4+(rand4()&7); i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.solid(x,y) && countFoo(x,y,1,GROUND) > 4) {
			Node<Object> on = addObject(x, y, VENDING_MACHINE);
			on->quantity = rand4() + 10;
			// don't let it face into a tree
			DIRECTION orientation = D_EAST;
			do {
				switch (rand4() & 0x3) {
					case 0: orientation = D_NORTH; break;
					case 1: orientation = D_SOUTH; break;
					case 2: orientation = D_EAST; break;
					case 3: orientation = D_WEST; break;
				}
			} while (game.map.solid(x+D_DX[orientation],y+D_DY[orientation]));
			on->orientation = orientation;
			Node<lightsource> li(new NodeV<lightsource>);
			li->set(orientation, 4<<12, 150<<12, 1<<12, 1<<11, 1<<11);
			li->x = x<<12; li->y = y<<12;
			li->flicker = FLICKER_LIGHT;
			game.map.lights.push(li);
		} else {
			i--;
			ntries++;
			if (ntries > 10000) assert(!"too many tries...");
		}
	}
}

void SpawnCreatures() {
	for (int i = 0, n = 40 + (rand8() % 32); i < n; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.solid(x,y)) {
			game.map.spawn(BLOWFLY, x, y);
		} else i--;
	}
	for (int i = 0, n = 20 + rand4(); i < n; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (game.map.at(x,y)->type == TREE && countFoo(x,y,1,GROUND) >= 1) {
			game.map.spawn(VENUS_FLY_TRAP, x, y);
			set_tile(x,y, GROUND);
		} else i--;
	}

	for (int i = 0, n = 15 + rand4(); i < n; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.solid(x,y)) {
			game.map.spawn(LABRADOR, x, y);
		} else i--;
	}
}

static inline void startTimer() {
#ifndef NATIVE
	TIMER_CR(1) = TIMER_CASCADE;
	TIMER_CR(0) = TIMER_DIV_1024 | TIMER_ENABLE;
#endif
}
static inline u32 stopTimer() {
#ifndef NATIVE
	TIMER_CR(0) = 0;
	return TIMER_DATA(0) | (TIMER_DATA(1) << 16);
#else
	return 0;
#endif
}

void generate_terrarium() {
	game.map.resize(128,128);
	torch.buf.resize(128,128);

	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	torch.buf.reset();
	torch.buf.cache.reset();
	game.map.reset();

	// glass on the rim
	filledCircle(cx, cy, 60, set_tile_i, (void*)GROUND);
	hollowCircle(cx, cy, 60, set_tile_i, (void*)GLASS);

	AddStars();

	u32 timerVal;

	printf("Growing trees... ");
	startTimer();
	CATrees();
	timerVal = stopTimer();
	printf("done (%.2fs).\n", timerVal*1024/33.513982e6);

	printf("Filling lakes... ");
	startTimer();
	CALakes();
	timerVal = stopTimer();
	printf("done (%.2fs).\n", timerVal*1024/33.513982e6);

	printf("Buying shrubberies... ");
	startTimer();
	AddGrass();
	timerVal = stopTimer();
	printf("done (%.2fs).\n", timerVal*1024/33.513982e6);

	printf("Checking connectivity... ");
	if (checkConnected()) {
		printf("connected.\n");
	} else {
		printf("\1\x1f\x01unconnected\2.\n");
	}

	printf("Simulating inhabitation... ");
	Inhabit();
	printf("done.\n");

	printf("Spawning creatures... ");
	SpawnCreatures();
	printf("done.\n");

	printf("Dropping items... ");
	for (int i = 0; i < 60; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.solid(x,y)) {
			Node<Object> on(new NodeV<Object>);
			on->type = ROCK;
			stack_item_push(game.map.at(x,y)->objects, on);
		} else i--;
	}
	printf("done.\n");

	s16 x = cx, y = cy;
	while (game.map.solid(x, y))
		randwalk(x, y);
	game.player.x = x;
	game.player.y = y;

	x = cx; y = cy;

	set_tile(cx+40, cy, FIRE);
	Node<lightsource> li(new NodeV<lightsource>);
	li->set(12<<12, (int)(0.1*(1<<12)), (int)(1.0*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+40)<<12; li->y = cy<<12;
	li->flicker = FLICKER_RADIUS;
	game.map.lights.push(li);
	set_tile(cx+30, cy, FIRE);
	li = new NodeV<lightsource>;
	li->set(12<<12, (int)(1.0*(1<<12)), (int)(0.1*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+30)<<12; li->y = cy<<12;
	li->flicker = FLICKER_RADIUS;
	game.map.lights.push(li);
	set_tile(cx+35, cy-7, FIRE);
	li = new NodeV<lightsource>;
	li->set(12<<12, (int)(0.1*(1<<12)), (int)(0.1*(1<<12)), (int)(1.0*(1<<12)));
	li->x = (cx+35)<<12; li->y = (cy-7)<<12;
	li->flicker = FLICKER_RADIUS;
	game.map.lights.push(li);

	game.map.block.refresh_blocked_from();
	torch.dirty_screen();
	torch.reset_luminance();
}

void test_map() {
	game.map.resize(64,64);
	torch.buf.resize(64,64);

	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	filledCircle(cx, cy, 30, set_tile_i, (void*)GROUND);
	hollowCircle(cx, cy, 30, set_tile_i, (void*)GLASS);

	game.player.x = cx; game.player.y = cy;
	game.map.at(cx,cy)->creature = game.player.obj;
	game.player.obj->setPos(cx,cy);

	Node<lightsource> li;
	set_tile(cx+5, cy, FIRE);
	li = new NodeV<lightsource>;
	li->set(12<<12, (int)(0.1*(1<<12)), (int)(1.0*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+5)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	set_tile(cx-5, cy, FIRE);
	li = new NodeV<lightsource>;
	li->set(12<<12, (int)(1.0*(1<<12)), (int)(0.1*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx-5)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	set_tile(cx, cy-5, FIRE);
	li = new NodeV<lightsource>;
	li->set(12<<12, (int)(0.1*(1<<12)), (int)(0.1*(1<<12)), (int)(1.0*(1<<12)));
	li->x = cx<<12; li->y = (cy-5)<<12;
	game.map.lights.push(li);

	Node<Object> on = addObject(cx+2, cy-1, VENDING_MACHINE);
	on->quantity = rand4() + 10;
	DIRECTION orientation = D_WEST;
	on->orientation = orientation;
	li = new NodeV<lightsource>;
	li->set(orientation, 4<<12, 150<<12, 1<<12, 1<<11, 1<<11);
	li->x = (cx+2)<<12; li->y = (cy-1)<<12;
	li->flicker = FLICKER_LIGHT;
	game.map.lights.push(li);

	game.map.block.refresh_blocked_from();
}
