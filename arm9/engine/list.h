#ifndef LIST_H
#define LIST_H 1
#include <stdio.h>

// These singly-linked lists are as memory-light and call malloc as little as I
// could make them do without re-implementing malloc. They are designed such
// that they can fit to any size of data, and hopefully with little overhead.
//
// The layout of nodes is like this:
//                   | next_ptr |  (data data data)  |
// next_ptr is a single word: a pointer to the next node in the list, or NULL if
// this is the tail of the list. The data can be anything and any size, with one
// restriction: all the elements in a particular list must have the same size.
//
// A Pool keeps track of a list of "free nodes". These nodes are allocated, but
// unused. When you request a node from a pool, you will simply be given the
// pointer to the head of the "free" list, unless it is empty -- in which case,
// space for more nodes will be allocated and the nodes allocated will be added
// to the free pool. Note that the space allocated *includes* enough space
// after each node pointer for whatever kind of data the pool is storing.
//
// Note that an Pool will never free() any memory on its own. Calling
// flush_free will walk the free list and free() each node (and its data with
// it.)

#include <cstddef> // for size_t
#include <cstdlib>
#include "assert.h"

#define POOLED(T) \
	private: \
	static Pool<T>& __getPool() { \
		static Pool<T> p; \
		return p; \
	} \
	public: \
	inline void *operator new(size_t sz) { \
		assert(sz <= sizeof(T)); \
		return __getPool().alloc(); \
	} \
	inline void operator delete(void *mem) { \
		__getPool().free((T*)mem); \
	}

#define UNPOOLED \
	public: \
	inline void *operator new(size_t sz) { \
		return ::operator new(sz); \
	} \
	inline void operator delete(void *mem) { \
		::operator delete(mem); \
	}


template <class T> struct List;

template <class T, unsigned int blockSize = 32>
class Pool {
	static List<T> free_nodes;

	public:
		T* alloc() {
			if (free_nodes.empty()) {
				for (unsigned int i = 0; i < blockSize; i++) {
					T* node = (T*)malloc(sizeof(T));
					free_nodes.push(node);
				}
			}
			T* ret = free_nodes.pop();
			return ret;
		}
		void free(T* t) {
			free_nodes.push(t);
		}

		static void flush_free() {
			T *n;
			while ((n = free_nodes.pop()))
				::free(n);
		}
};

template <class T, unsigned int blockSize>
List<T> Pool<T, blockSize>::free_nodes;

template <class T>
class listable {
	listable *__next;
	friend class List<T>;
	public:
		T* next() {
			return static_cast<T*>(__next);
		}
};

template <class T>
struct List {
	protected:
		listable<T> *mHead;

	public:
		List(): mHead(0) {}

		T* head() {
			return static_cast<T*>(mHead);
		}

		// returns true if the node is in the list (and thus was removed), false if
		// it wasn't (and thus wasn't removed)
		bool remove(listable<T> *ptr) {
			if (mHead == 0) return false;
			if (ptr == mHead) { mHead = mHead->__next; return true; }
			listable<T> *prev = mHead;
			listable<T> *k = prev->__next;
			while (k && k != ptr) {
				prev = k;
				k = k->__next;
			}
			if (k) // didn't hit the end
				prev->__next = k->__next;
			return !!k;
		}
		inline bool remove(T *ptr) {
			return remove(static_cast<listable<T>*>(ptr));
		}

		inline void push(listable<T> *x) {
			x->__next = mHead;
			mHead = x;
		}
		inline void push(T *x) {
			push(static_cast<listable<T>*>(x));
		}

		T *pop() {
			if (!mHead) return NULL;
			listable<T> *ret = mHead;
			mHead = mHead->__next;
			return static_cast<T*>(ret);
		}

		unsigned int length() {
			unsigned int len = 0;
			for (listable<T> *k = mHead; k; k = k->__next)
				len++;
			return len;
		}

		bool empty() {
			return !mHead;
		}

		void delete_all() {
			while (T* k = pop())
				delete k;
		}
};

#endif /* LIST_H */
