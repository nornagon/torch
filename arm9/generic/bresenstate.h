#ifndef BRESENSTATE_H
#define BRESENSTATE_H 1

#include <nds/jtypes.h>

class bresenstate {
	public:
		bresenstate(s16 x0_, s16 y0_, s16 x1_, s16 y1_);
		bresenstate();
		void reset(s16 x0_, s16 y0_, s16 x1_, s16 y1_);
		void step();
		s16 posx();
		s16 posy();
		s16 destx();
		s16 desty();

	private:
		bool steep, reversed;
		s16 deltax, deltay, error, ystep;
		s16 x0, y0, x1, y1;
		s16 x, y;
};

#endif /* BRESENSTATE_H */
