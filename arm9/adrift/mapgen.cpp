#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

#include "engine.h"

#include "lightsource.h"
#include <malloc.h>
#include <string.h>

#include <stdio.h>
#include <nds.h>

#include "gfxPrimitives.h"

#include "entities/creature.h"

#include "assert.h"

// helper function for passing to the gfxPrimitives generics
static inline void set_tile(s16 x, s16 y, void *info) {
	int type_ = (int)info;
	game.map.at(x,y)->type = type_;
	mapel *m = torch.buf.at(x,y);
	m->recall = 0; m->col = terraindesc[type_].color; m->ch = terraindesc[type_].ch;
	blockel *b = game.map.block.at(x,y);
	b->opaque = terraindesc[type_].opaque;
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
	if (rand32() % 10 == 0 && !l->desc()->solid) {
		Node<Object> on(new NodeV<Object>);
		on->type = ROCK;
		stack_item_push(l->objects, on);
	}
}

// drop rocks in the vicinity around a specified point
void drop_rocks(s16 ax, s16 ay, int r) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(ax + ((x*r) >> 12), ay + ((y*r) >> 12),
			               ax + ((x*(r+4)) >> 12), ay + ((y*(r+4)) >> 12), drop_rock, NULL);
	}
}

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
			if (!game.map.at(x,y)->desc()->solid) { done = true; break; }
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
				if (!marks[(p.y+dy)*w+p.x+dx] && !game.map.at(p.x+dx,p.y+dy)->desc()->solid) {
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
			if (!game.map.at(x,y)->desc()->solid && !marks[y*w+x]) {
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
	for (int dx = -r; dx <= r; dx++)
		for (int dy = -r; dy <= r; dy++)
			if (game.map.at(x+dx,y+dy)->type == ty)
				count++;
	return count;
}

int countTrees(s16 x, s16 y, s16 r) {
	return countFoo(x, y, r, TREE);
}

void CATrees() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (!game.map.at(x,y)->desc()->solid && (rand4() < 6)) set_tile(x,y, (void*)TREE);
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
				if (!game.map.at(x,y)->desc()->solid || game.map.at(x,y)->type == TREE) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, (void*)TREE);
					else
						set_tile(x,y, (void*)GROUND);
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
				if (!game.map.at(x,y)->desc()->solid || game.map.at(x,y)->type == TREE) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, (void*)TREE);
					else
						set_tile(x,y, (void*)GROUND);
				}
			}
		}
	}
}

void CALakes() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (!game.map.at(x,y)->desc()->solid && (rand8() < 23) && countFoo(x,y,6,GLASS)) set_tile(x,y, (void*)WATER);
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
						set_tile(x,y, (void*)WATER);
					else if (game.map.at(x,y)->type == WATER)
						set_tile(x,y, (void*)GROUND);
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
				if (!game.map.at(x,y)->desc()->solid || game.map.at(x,y)->type == WATER) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, (void*)WATER);
					else
						set_tile(x,y, (void*)GROUND);
				}
			}
		}
	}
	int w = 0;
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.at(x,y)->type == WATER) w++;
		}
	}
	printf("%d water cells. ", w);
}

void generate_terrarium() {
	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	torch.buf.reset();
	torch.buf.cache.reset();
	game.map.reset();

	// glass on the rim
	filledCircle(cx, cy, 60, set_tile, (void*)GROUND);
	hollowCircle(cx, cy, 60, set_tile, (void*)GLASS);
	printf("Growing trees... ");
	CATrees();
	printf("done\n");
	printf("Filling lakes... ");
	CALakes();
	printf("done\n");
	printf("Checking connectivity... ");
	if (checkConnected()) {
		printf("connected.\n");
	} else {
		printf("\1\x1f\x01unconnected\1\xff\xff\n");
	}
	printf("Spawning creatures... ");
	for (int i = 0; i < 60; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.at(x,y)->desc()->solid) {
			Node<Creature> fly(new NodeV<Creature>);
			fly->type = BLOWFLY;
			fly->hp = creaturedesc[fly->type].max_hp;
			fly->cooldown = 0;
			fly->setPos(x,y);
			game.map.at(x,y)->creature = fly;
			game.monsters.push(fly);
		} else i--;
	}
	printf("done\n");
	printf("Dropping items... ");
	for (int i = 0; i < 60; i++) {
		s16 x = rand32() % torch.buf.getw(), y = rand32() % torch.buf.geth();
		if (!game.map.at(x,y)->desc()->solid) {
			Node<Object> on(new NodeV<Object>);
			on->type = ROCK;
			stack_item_push(game.map.at(x,y)->objects, on);
		} else i--;
	}
	printf("done\n");

	Cell *l;
	s16 x = cx, y = cy;
	while (game.map.at(x, y)->desc()->solid)
		randwalk(x, y);
	game.player.x = x;
	game.player.y = y;

	x = cx, y = cy;
	while ((l = game.map.at(x, y)) && l->desc()->solid)
		randwalk(x, y);
	Node<Creature> cn(new NodeV<Creature>);
	cn->type = 0;
	cn->cooldown = 0;
	cn->setPos(x,y);
	l->creature = cn;

	x = cx; y = cy;

	while ((l = game.map.at(x, y)) && l->desc()->solid)
		randwalk(x, y);
	Node<Object> on(new NodeV<Object>);
	on->type = 0;
	l->objects.push(on);

	set_tile(cx+40, cy, (void*)FIRE);
	lightsource *li = new_light(12<<12, (int)(0.1*(1<<12)), (int)(1.0*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+40)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	set_tile(cx+30, cy, (void*)FIRE);
	li = new_light(12<<12, (int)(1.0*(1<<12)), (int)(0.1*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+30)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	set_tile(cx+35, cy-7, (void*)FIRE);
	li = new_light(12<<12, (int)(0.1*(1<<12)), (int)(0.1*(1<<12)), (int)(1.0*(1<<12)));
	li->x = (cx+35)<<12; li->y = (cy-7)<<12;
	game.map.lights.push(li);

	game.map.block.refresh_blocked_from();
}
