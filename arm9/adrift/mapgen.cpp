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

static inline void set_tile(s16 x, s16 y, void *info) {
	int type_ = (int)info;
	game.map.at(x,y)->type = type_;
	mapel *m = torch.buf.at(x,y);
	m->recall = 0; m->col = terraindesc[type_].color; m->ch = terraindesc[type_].ch;
	blockel *b = game.map.block.at(x,y);
	b->opaque = terraindesc[type_].opaque;
}

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
	if (rand32() % 10 == 0 && l->type == GROUND) {
		Node<Object> on(new NodeV<Object>);
		on->type = ROCK;
		stack_item_push(l->objects, on);
	}
}

void drop_rocks(s16 ax, s16 ay) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = 4;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(ax + ((x*r) >> 12), ay + ((y*r) >> 12),
			               ax + ((x*(r+4)) >> 12), ay + ((y*(r+4)) >> 12), drop_rock, NULL);
	}
}

/*void killTrees() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			Cell *l = game.map.at(x,y);
			if (l->type == TREE && rand32() % 64 == 0) {
				set_tile(x,y, (void*)GROUND);
			}
		}
	}
}

void growTrees() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.at(x,y)->type == GROUND && countTrees(x,y) > 3 && rand32() % 32 == 0)
				set_tile(x,y, (void*)TREE);
		}
	}
}
*/

int countTrees(s16 x, s16 y) {
	int count = 0;
	for (int dx = -1; dx <= 1; dx++)
		for (int dy = -1; dy <= 1; dy++)
			if (game.map.at(x+dx,y+dy)->type == TREE)
				count++;
	return count;
}

void CATrees() {
	for (int y = 0; y < torch.buf.geth(); y++) {
		for (int x = 0; x < torch.buf.getw(); x++) {
			if (game.map.at(x,y)->type == GROUND && (rand4() < 8)) set_tile(x,y, (void*)TREE);
		}
	}
	bool *next = new bool[torch.buf.geth()*torch.buf.getw()];
	for (int i = 0; i < 5; i++) {
		memset(next, 0, torch.buf.geth()*torch.buf.getw()*sizeof(bool));
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (countTrees(x,y) >= 5) next[y*torch.buf.getw()+x] = true;
				else next[y*torch.buf.getw()+x] = false;
			}
		}
		for (int y = 0; y < torch.buf.geth(); y++) {
			for (int x = 0; x < torch.buf.getw(); x++) {
				if (game.map.at(x,y)->type == GROUND || game.map.at(x,y)->type == TREE) {
					if (next[y*torch.buf.getw()+x])
						set_tile(x,y, (void*)TREE);
					else
						set_tile(x,y, (void*)GROUND);
				}
			}
		}
	}
}

#include "assert.h"

void generate_terrarium() {
	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	torch.buf.reset();
	torch.buf.cache.reset();
	game.map.reset();

	// glass on the rim
	filledCircle(cx, cy, 60, set_tile, (void*)GROUND);
	hollowCircle(cx, cy, 60, set_tile, (void*)GLASS);
	CATrees();

	//haunted_grove(cx, cy);
	/*hollowCircle(cx, cy, 30, set_tile, (void*)TREE);
	hollowCircle(cx+1, cy, 30, set_tile, (void*)TREE);
	hollowCircle(cx, cy+1, 30, set_tile, (void*)TREE);
	hollowCircle(cx-1, cy, 30, set_tile, (void*)TREE);
	hollowCircle(cx, cy-1, 30, set_tile, (void*)TREE);*/

	/*for (int i = 0; i < 5; i++) {
		killTrees();
		growTrees();
	}*/

	/*bresenham(cx,cy,cx+(((40)*COS[30]) >> 12),cy+(((40)*SIN[30]) >> 12),set_tile,(void*)GROUND);*/
	Cell *l;
	s16 x = cx, y = cy;
	while (game.map.at(x, y)->type != GROUND)
		randwalk(x, y);
	game.player.x = x;
	game.player.y = y;

	drop_rocks(cx, cy);

	x = cx, y = cy;
	while ((l = game.map.at(x, y)) && l->type != GROUND)
		randwalk(x, y);
	Node<Creature> cn(new NodeV<Creature>);
	cn->type = 0;
	cn->cooldown = 0;
	cn->setPos(x,y);
	l->creature = cn;

	x = cx; y = cy;

	while ((l = game.map.at(x, y)) && l->type != GROUND)
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
