#include "mapgen.h"
#include "adrift.h"
#include "mersenne.h"

#include "engine.h"

#include "lightsource.h"
#include <malloc.h>

#include <stdio.h>
#include <nds.h>

#include "gfxPrimitives.h"

#define DEF(type_) \
	static inline void SET_##type_(s16 x, s16 y) { \
		game.map.at(x,y)->type = type_; \
		mapel *m = torch.buf.at(x,y); \
		m->recall = 0; m->col = terraindesc[type_].color; m->ch = terraindesc[type_].ch; \
		blockel *b = game.map.block.at(x,y); \
		b->opaque = terraindesc[type_].opaque; \
	}
DEF(TREE);
DEF(GROUND);
DEF(TERRAIN_NONE);
DEF(GLASS);
DEF(WATER);
DEF(FIRE);
#undef DEF

void haunted_grove(s16 cx, s16 cy) {
	int r0 = 7, r1 = 15;
	int w0 = 3, w1 = 4;

	unsigned int playerpos = ((rand32() & 0x1ff) / 8) * 8;
	for (unsigned int t = 0; t < 0x1ff; t += 8) {
		int r = rand32() % 5;
		int x = COS[t], y = SIN[t];
		bresenham(cx, cy, cx + ((x*r) >> 12), cy + ((y*r) >> 12), SET_WATER);
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
			               cx + ((x*(r+w0)) >> 12), cy + ((y*(r+w0)) >> 12), SET_TREE);
	}
	unsigned int firepos = ((rand32() & 0x1ff) / 4) * 4;
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int x = COS[t], y = SIN[t];
		if (t == firepos) {
			int px = cx + ((x*(r0+w0+1)) >> 12),
			    py = cy + ((y*(r0+w0+1)) >> 12);
			SET_FIRE(px, py);
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
			               cx + ((x*(r+w1)) >> 12), cy + ((y*(r+w1)) >> 12), SET_TREE);
	}
	u16 k = rand16() & 0x1ff;
	int x = COS[k], y = SIN[k];
	bresenham(cx + ((x*r0) >> 12), cy + ((y*r0) >> 12),
	               cx + ((x*(r0+w0)) >> 12), cy + ((y*(r0+w0)) >> 12), SET_GROUND);
	k = rand16() & 0x1ff;
	x = COS[k]; y = SIN[k];
	bresenham(cx + ((x*r1) >> 12), cy + ((y*r1) >> 12),
	               cx + ((x*(r1+w1)) >> 12), cy + ((y*(r1+w1)) >> 12), SET_GROUND);

	int nTraps = 4 + (rand16() % 10);
	for (int i = 0; i < nTraps; i++) {
		int x, y;
		do {
			int theta = rand16() & 0x1ff;
			int r = r0 + w0 + (rand16() % 6);
			x = cx + ((COS[theta]*r) >> 12), y = cy + ((SIN[theta]*r) >> 12);
		} while (!(game.map.at(x,y)->type == GROUND && !game.map.occupied(x,y)));
		Node<Creature> trap(new NodeV<Creature>);
		trap->type = C_FLYTRAP;
		trap->hp = creaturedesc[trap->type].maxhp;
		trap->cooldown = 0;
		trap->setPos(x,y);
		game.map.at(x,y)->creature = trap;
		game.monsters.push(trap);
	}
}

void drop_rock(s16 x, s16 y) {
	Cell *l = game.map.at(x,y);
	if (rand32() % 10 == 0 && l->type == GROUND) {
		Node<Object> on(new NodeV<Object>);
		on->type = J_ROCK;
		stack_item_push(l->objects, on);
	}
}

void drop_rocks(s16 ax, s16 ay) {
	for (unsigned int t = 0; t < 0x1ff; t += 4) {
		int r = 4;
		int x = COS[t], y = SIN[t];
		if (t % 16 != 0)
			bresenham(ax + ((x*r) >> 12), ay + ((y*r) >> 12),
			               ax + ((x*(r+4)) >> 12), ay + ((y*(r+4)) >> 12), drop_rock);
	}
}

#include "assert.h"

void generate_terrarium() {
	s16 cx = torch.buf.getw()/2, cy = torch.buf.geth()/2;

	torch.buf.reset();
	torch.buf.cache.reset();
	game.map.reset();

	filledCircle(cx, cy, 60, SET_GROUND);
	hollowCircle(cx, cy, 60, SET_GLASS);

	haunted_grove(cx, cy);

	drop_rocks(cx, cy);

	s16 x = cx, y = cy;
	Cell *l;
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

	SET_FIRE(cx+40, cy);
	lightsource *li = new_light(12<<12, (int)(0.1*(1<<12)), (int)(1.0*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+40)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	SET_FIRE(cx+30, cy);
	li = new_light(12<<12, (int)(1.0*(1<<12)), (int)(0.1*(1<<12)), (int)(0.1*(1<<12)));
	li->x = (cx+30)<<12; li->y = cy<<12;
	game.map.lights.push(li);
	SET_FIRE(cx+35, cy-7);
	li = new_light(12<<12, (int)(0.1*(1<<12)), (int)(0.1*(1<<12)), (int)(1.0*(1<<12)));
	li->x = (cx+35)<<12; li->y = (cy-7)<<12;
	game.map.lights.push(li);

	game.map.block.refresh_blocked_from();
}
