#ifndef LLPOOL_H
#define LLPOOL_H 1

struct node_s;
typedef struct node_s node_t;

struct node_s {
	node_t *next;
};

typedef struct {
	unsigned int size;
	node_t *free; // head of the free list
} llpool_t;

// create a new llpool of objects of the given size
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
