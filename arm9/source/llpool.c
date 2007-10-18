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
