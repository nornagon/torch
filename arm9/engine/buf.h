#ifndef BUF_H
#define BUF_H 1

#include <nds.h>
//#include <string.h>

/*#include "list.h"
#include "process.h"
#include "cache.h"
#include "object.h"

#include "direction.h"*/

struct mapel {
	mapel() { reset(); }
	mapel(u16 ch_, u16 col_) { reset(); col = col_; ch = ch_; }
	u16 col, ch;

	int32 recall;

	void reset() { col = ch = recall = 0; }
};
struct luxel {
	luxel() { reset(); }
	int32 lr, lg, lb, lval;

	void reset() {
		lr = lg = lb = lval = 0;
		last_lr = last_lg = last_lb = last_lval = 0;
	}

	private:
		friend class engine;
		int32 last_lr, last_lg, last_lb, last_lval;
};
struct cachel {
	cachel() { reset(); }
	u16 last_col, last_col_final, last_ch;
	u8 dirty : 2;
	bool was_visible : 1;

	void reset() { last_col = last_col_final = dirty = was_visible = 0; }
};

struct coord {
	coord() { reset(); }
	s16 x, y;

	void reset() { x = y = 0; }
};

template <int W, int H>
class cachebucket {
	private:
		friend class engine;
		cachel *els;
		coord origin;

	public:
		cachebucket() {
			els = new cachel[W*H];
		}
		~cachebucket() {
			delete [] els;
		}

		inline cachel *at(s16 x, s16 y) {
			x += origin.x;
			y += origin.y;
			if (x >= W) x -= W;
			if (y >= H) y -= H;
			return &els[y*W+x];
		}

		void reset() {
			for (int x = 0; x < W; x++)
				for (int y = 0; y < H; y++)
					els[y*W+x].reset();
			origin.reset();
		}
};

class torchbuf {
	friend class engine;
	private:
		s16 w, h;
		mapel *map;
		luxel *light;

		inline luxel *luxat_s(s16 x, s16 y) { return &light[y*32+x]; }

	public:
		torchbuf();
		torchbuf(s16 w, s16 h);

		coord scroll;

		cachebucket<32,24> cache;

		void resize(s16 w, s16 h);
		void reset();

		inline mapel *at(s16 x, s16 y) { return &map[y*w+x]; }
		inline luxel *luxat(s16 x, s16 y) { return &light[(y-scroll.y)*32+(x-scroll.x)]; }
		inline cachel *cacheat(s16 x, s16 y) {
			return cache.at(x - scroll.x, y - scroll.y);
		}

		s16 getw() { return w; }
		s16 geth() { return h; }

		void bounded(s16 &x, s16 &y) {
			if (x < 0) x = 0;
			else if (x >= w) x = w-1;
			if (y < 0) y = 0;
			else if (y >= h) y = h-1;
		}
};

#endif /* BUF_H */
