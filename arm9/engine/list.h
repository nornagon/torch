#ifndef LIST_H
#define LIST_H 1

// These singly-linked lists are as memory-light and call malloc as little as I
// could make them do without re-implementing malloc. They are designed such
// that they can fit to any size of data, and hopefully with little overhead.
//
// The layout of nodes is like this:
//                   | next_ptr |  (data data data)  |
// next_ptr is a single word: a pointer to the next node in the list, or NULL if
// this is the tail of the list. The data can be anything and any size, with one
// restriction: all the elements in a particular list must have the same size.
// llpool_t will deal with memory allocation for you as much as possible. When
// you create a new llpool ('linked list pool'), you must specify the size of
// the elements it will deal with.
//
// llpool_t keeps track of a list of "free nodes". These nodes are allocated,
// but unused. When you request a node from a pool, you will simply be given the
// pointer to the head of the "free" list, unless it is empty -- in which case,
// space for 32 more nodes will be allocated and the nodes allocated will be
// added to the free pool. Note that the space allocated *includes* enough space
// after each node pointer for whatever kind of data the pool is storing.
//
// (TODO: make the number of nodes allocated in a block-allocation configurable)
//
// Note that an llpool will never free() any memory on its own. Calling
// flush_free will walk the free list and free() each node (and its data with
// it.)

#include <cstddef> // for size_t
#include <cstdlib>

template <class T> struct NodeV;
template <class T> struct List;

template <class T>
struct Pool {
	Pool<T>(): free_nodes() {}

	List<T> free_nodes;

	void alloc_space(unsigned int n);
	void flush_free();
	NodeV<T> *request_node();
};

template <class T>
class Node {
	protected:
		NodeV<T> *v;

	public:
		Node<T>(NodeV<T>* v_): v(v_) {}
		Node<T>(): v(0) {}

		T* operator->() {
			return &v->data;
		}

		NodeV<T>* data() { return v; }
		operator T*() { return &v->data; }
		T operator*() { return v->data; }
		operator bool() { return !!v; }
		Node<T> next() { return v->next; }
		void free() { v->free(); }
};

template <class T>
struct NodeV {
	NodeV(): next(0) {}
	NodeV<T> *next;
	T data;
	static Pool<T> pool;
	void free() {
		pool.free_nodes.push(this);
	}
	operator T* () { return &data; }
	void* operator new (size_t sz) { return (void*) NodeV<T>::pool.request_node(); }
	void operator delete (void* thing) { ((NodeV<T>*)thing)->free(); }
	operator Node<T>() { return Node<T>(this); }
};
template <class T> Pool<T> NodeV<T>::pool = Pool<T>();

template <class T>
struct List {
	protected:
		NodeV<T> *head;

	public:
		List(): head(0) {}

		Node<T> top() {
			return Node<T>(head);
		}

		// returns true if the node is in the list (and thus was removed), false if
		// it wasn't (and thus wasn't removed)
		bool remove(NodeV<T> *node) {
			if (head == 0) return false;
			if (node == head) { head = head->next; return true; }
			NodeV<T>* prev = head;
			NodeV<T>* k = prev->next;
			while (k && k != node) {
				prev = k;
				k = k->next;
			}
			if (k) // didn't hit the end
				prev->next = k->next;
			return !!k;
		}
		inline bool remove(Node<T> node) {
			return remove(node.data());
		}
		/*bool remove(T x) {
			if (head == 0) return false;
			if (x == head->data) { head = head->next; return true; }
			NodeV<T>* prev = head;
			NodeV<T>* k = prev->next;
			while (k && k->data != x) {
				prev = k;
				k = k->next;
			}
			if (k) // didn't hit the end
				prev->next = k->next;
			return !!k;
		}*/
		inline void push(T x) { // TODO: pass by reference?
			NodeV<T>* n = new NodeV<T>;
			n->data = x;
			push(n);
		}
		inline void push(Node<T> node) {
			push(node.data());
		}
		void push(NodeV<T> *node) {
			node->next = head;
			head = node;
		}
		NodeV<T> *pop() {
			if (head == 0) return 0;
			NodeV<T> *ret = head;
			head = head->next;
			return ret;
		}

		unsigned int length() {
			unsigned int len = 0;
			for (NodeV<T>* k = head; k; k = k->next)
				len++;
			return len;
		}

		bool empty() {
			return !head;
		}

		void clear() {
			while (NodeV<T>* k = pop())
				k->free();
			NodeV<T>::pool.flush_free();
		}
};

template <class T> void Pool<T>::alloc_space(unsigned int n) {
	for (; n > 0; n--) {
		NodeV<T> *node = (NodeV<T>*)malloc(sizeof(NodeV<T>));
		free_nodes.push(node);
	}
}

template <class T> void Pool<T>::flush_free() {
	NodeV<T> *p;
	while ((p = free_nodes.pop())) {
		free(p);
	}
}

// request a node from the pool. will alloc more space if there is none.
template <class T>
NodeV<T>* Pool<T>::request_node() {
	if (!free_nodes.top())
		alloc_space(32); // TODO: allow customisation
	return free_nodes.pop();
}

#endif /* LIST_H */
