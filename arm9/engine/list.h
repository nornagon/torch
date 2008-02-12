#ifndef LLPOOL_H
#define LLPOOL_H 1

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

template <class T> struct Node;
template <class T> struct List;

template <class T>
struct Pool {
	Pool<T>(): free_nodes() {}

	List<T> free_nodes;

	void alloc_space(unsigned int n);
	void flush_free();
	Node<T> *request_node();
};

template <class T>
struct Node {
	Node(): next(0) {}
	Node<T> *next;
	T data;
	static Pool<T> pool;
	void free() {
		pool.free_nodes.push(this);
	}
	operator T* () { return &data; }
};
template <class T> Pool<T> Node<T>::pool = Pool<T>();

template <class T>
struct List {
	List(): head(0) {}

	Node<T> *head;

	void remove(Node<T> *node) {
		if (head == 0) return;
		if (node == head) { head = head->next; return; }
		Node<T>* prev = head;
		Node<T>* k = prev->next;
		while (k && k != node) {
			prev = k;
			k = k->next;
		}
		if (k) // didn't hit the end
			prev->next = k->next;
	}
	void push(Node<T> *node) {
		node->next = head;
		head = node;
	}
	Node<T> *pop() {
		if (head == 0) return 0;
		Node<T> *ret = head;
		head = head->next;
		return ret;
	}

	unsigned int length() {
		unsigned int len = 0;
		for (Node<T>* k = head; k; k = k->next)
			len++;
		return len;
	}
};

template <class T> void Pool<T>::alloc_space(unsigned int n) {
	for (; n > 0; n--) {
		Node<T> *node = new Node<T>;
		free_nodes.push(node);
	}
}

template <class T> void Pool<T>::flush_free() {
	while (free_nodes.head) {
		Node<T> *next = free_nodes.head->next;
		delete free_nodes.head;
		free_nodes.head = next;
	}
}

// request a node from the pool. will alloc more space if there is none.
template <class T>
Node<T>* Pool<T>::request_node() {
	if (!free_nodes.head)
		alloc_space(32); // TODO: allow customisation
	return free_nodes.pop();
}

#endif /* LLPOOL_H */
