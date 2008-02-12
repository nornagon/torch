#ifndef MAP_H
#define MAP_H 1

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
	u16 last_col, last_col_final;
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

/*struct Cell {
	u16 type;
	u16 col;
	u8 ch;

	// can light pass through the cell?
	bool opaque : 1;

	bool forgettable : 1;

	// blocked_from is cache for working out which sides of an opaque cell we
	// should light
	unsigned int blocked_from : 4;

	// visible is true if the cell is on-screen *and* in the player's LOS
	bool visible : 1;

	int16 recall; // how much the player remembers this cell. TODO: could be 5 bits, i think.

	// what things are here? A ball of string and some sealing wax.
	List<Object> objects;
};*/

#if 0
// map_t holds map information as well as game state.
class Map {
	private:
	s32 w,h;

	public:
	s32 getWidth() { return w; }
	s32 getHeight() { return h; }

	List<Process> processes;

	Cell* cells;

	Cache* cache;
	int cacheX, cacheY; // top-left corner of cache. Should be kept positive.

	public:
	Map();
	// resize the map by resetting, then freeing and reallocating all the cells.
	void resize(u32 w, u32 h);

	void reset();
	void reset_cache();

	void (*handler)();

	s32 scrollX, scrollY; // top-left corner of screen. Should be kept positive.

	s32 pX, pY;

	inline Cell *at(s32 x, s32 y) {
		return &cells[y*w+x];
	}

	/*inline Cell *operator()(s32 x, s32 y) const {
		return at(x,y);
	}*/

	// cache for *screen* coordinates (x,y).
	inline Cache *cache_at_s(s32 x, s32 y) {
		// Because we don't want to move data in the cache around every time the
		// screen scrolls, we're going to move the origin instead. The cache at
		// (cacheX, cacheY) corresponds to the cell in the top-left corner of the
		// screen. When we want to scroll one tile to the right, we move the origin of
		// the cache one tile to the right, and mark all the cells on the right edge
		// as dirty. If (cacheX, cacheY) were (0, 0) before the scroll, afterwards
		// they will be (1, 0). Note that this means the right edge of the screen
		// would normally be outside of the cache memory -- so we 'wrap' around the
		// right edge of cache memory and the column cacheX = 0 now represents the
		// right edge of the screen. Similarly for the left edges and scrolling left,
		// and for the top and bottom edges.
		x += cacheX;
		y += cacheY;
		if (x >= 32) x -= 32;
		if (y >= 24) y -= 24;
		return &cache[y*32+x];
	}
	inline Cache *cache_at(s32 x, s32 y) {
		return cache_at_s(x - scrollX, y - scrollY);
	}

	inline void bounded(s32 &x, s32 &y) {
		if (x < 0) x = 0;
		else if (x >= w) x = w-1;
		if (y < 0) y = 0;
		else if (y >= h) y = h-1;
	}

	inline bool is_outside(s32 x, s32 y) {
		return y < 0 || y >= h || x < 0 || x >= h;
	}

	// push a new process on the process stack, returning the new process node.
	Node<Process> *push_process(process_func process, process_func end, void* data);

	// remove the process proc from the process list and add it to the free pool.
	inline void free_process(Node<Process> *proc) {
		processes.remove(proc);
		proc->free();
	}

	// free all processes
	void free_processes(Node<Process> *procs[], unsigned int num);

	// free all processes that aren't the one specified.
	// procs should be a pointer to an *array* of nodes, not the head of a list. num
	// should be the number of nodes in the array.
	//void free_other_processes(Map *map, Process *this_proc, Node<Process> *procs[], unsigned int num);

	// create a new object. this doesn't add the object to any lists, so you'd
	// better do it yourself. returns the object node created.
	Node<Object> *new_object(ObjType *type, void* data);

	// remove the object from its owning cell, and add the node to the free pool
	void free_object(Node<Object> *obj_node);

	// perform an insert of obj into the map cell at (x,y) sorted by importance.
	// insert_object walks the list, so it's O(n), but it's better in the case of
	// inserting a high-importance object (because fewer comparisons of importance
	// have to be made)
	void insert_object(Node<Object> *obj, s32 x, s32 y);

	// remove obj from its cell and add it to the cell at (x,y).
	void move_object(Node<Object> *obj, s32 x, s32 y);

	// move an object by (dX,dY). Will pay attention to opaque cells, map edges,
	// etc.
	void displace_object(Node<Object> *obj_node, int dX, int dY);

	// refresh cells' awareness of their surroundings
	void refresh_blockmap();
};

#endif

//extern Map map;


// will return the first instance of an object of type objtype it comes across
// in the given cell, or NULL if there are none.
//Node<Object> *has_objtype(Cell *cell, ObjType *objtype);

#endif /* MAP_H */
