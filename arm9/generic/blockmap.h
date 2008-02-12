#ifndef BLOCKMAP_H
#define BLOCKMAP_H 1

#include "direction.h"

struct blockel {
	blockel() { reset(); }

	unsigned int blocked_from : 4;
	DIRECTION seen_from : 5;
	bool opaque : 1;
	bool visible : 1;

	void reset() {
		blocked_from = seen_from = opaque = 0;
	}
};

class blockmap {
	private:
		blockel *els;
		int w, h;

	public:
		int pX, pY;

		blockmap(int w, int h);
		blockmap();
		void resize(int w, int h);
		void reset();

		inline bool is_outside(int x, int y) {
			return x < 0 || y < 0 || x >= w || y >= h;
		}

		inline blockel *at(int x, int y) {
			return &els[y*w+x];
		}
};

#endif /* BLOCKMAP_H */
