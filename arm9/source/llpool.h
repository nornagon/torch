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

struct node_s;
typedef struct node_s node_t;

struct node_s {
	node_t *next;
};

typedef struct {
	unsigned int size; // size of the kind of structure we manage
	node_t *free; // head of the free list
} llpool_t;

// create a new llpool of objects of the given size (in bytes)
llpool_t *new_llpool(unsigned int size);

// given a node, returns a pointer to the data at that node.
static inline void* node_data(node_t *node) {
	return (void*)(node + 1);
}

// add the node to the free list (doesn't actually free())
static inline void free_node(llpool_t *pool, node_t *node) {
	node->next = pool->free;
	pool->free = node;
}

// allocate space for n objects and add them to the free list
void alloc_space(llpool_t *pool, unsigned int n); 

// frees all the nodes in the free list
void flush_free(llpool_t *pool);

// request a node from the pool. will alloc more space if there is none.
static inline node_t *request_node(llpool_t *pool) {
	if (!pool->free)
		alloc_space(pool, 32);
	node_t *ret = pool->free;
	pool->free = pool->free->next;
	return ret;
}

// walks the list and updates pointers in order to remove the node from the
// list. This does *not* add the node to the free pool, you must do that
// yourself if you want it.
// returns the new head of the list (in the case that the object removed was the
// first one, the head will be different.)
node_t *remove_node(node_t *list, node_t *node);

static inline node_t *push_node(node_t *head, node_t *node) {
	node->next = head;
	return node;
}

#endif /* LLPOOL_H */
