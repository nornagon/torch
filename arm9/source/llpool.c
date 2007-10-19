#include "llpool.h"
#include <stdlib.h>
#include <malloc.h>

llpool_t *new_llpool(unsigned int size) {
	llpool_t *ret = malloc(sizeof(llpool_t));
	ret->size = size;
	ret->free = NULL;
	return ret;
}

void alloc_space(llpool_t *pool, unsigned int n) {
	while (n > 0) {
		node_t *node = malloc(pool->size + sizeof(node_t));
		free_node(pool, node);
		n--;
	}
}

void flush_free(llpool_t *pool) {
	while (pool->free) {
		node_t *next = pool->free->next;
		free(pool->free);
		pool->free = next;
	}
}

node_t *remove_node(node_t *list, node_t *node) {
	// walk the list
	node_t *prev = NULL;
	node_t *k = list;
	while (k != node) {
		prev = k;
		k = k->next;
	}
	// update the pointer of the node before the target if necessary
	if (prev)
		prev->next = node->next;
	else
		return k->next;
	return list;
	// don't free it; the caller might want to add it to another list.
	// if not, it's their responsibility to free the node.
}
