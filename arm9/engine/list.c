#include "list.h"
#include <stdlib.h>
#include <malloc.h>

llpool_t *new_llpool(unsigned int size) {
	llpool_t *ret = malloc(sizeof(llpool_t));
	ret->size = size;
	ret->free = NULL;
	return ret;
}

void alloc_space(llpool_t *pool, unsigned int n) {
	for (;n > 0; n--) {
		node_t *node = malloc(pool->size + sizeof(node_t));
		free_node(pool, node);
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
	if (list == NULL) return NULL;
	if (node == list) return list->next;
	node_t *prev = list;
	node_t *k = prev->next;
	while (k && k != node) {
		prev = k;
		k = k->next;
	}
	if (k) // we didn't hit the end of the list
		prev->next = k->next;
	return list;
}
