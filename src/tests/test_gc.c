/* Copyright 2013 Kyle Miller
 * test_gc.c
 * Testing the garbage collector
 */

#include "test.h"
#include <stdio.h>
//#include "objects.h"
#include "gc.h"

// Empty test
void TEST_SUCCEEDS test_alloc_obj(void) {
	init_free_lists();
	init_generations();
	init_nurseries(1);
	Nursery_t *nursery = get_nursery(0);
	Obj_t *o = alloc_obj(nursery, 5);
	assert((word)nursery->alloc_block->free_ptr - (word)nursery->alloc_block->start >= 5,
				 "Didn't move free pointer far enough.");
	assert(((word)nursery->alloc_block->free_ptr & (sizeof(void *) - 1)) == 0,
				 "Not correctly aligned.");
	verify_free_block_list();
	word ptr = (word)nursery->alloc_block->free_ptr;
	o = alloc_obj(nursery, sizeof(void *));
	assert((word)nursery->alloc_block->free_ptr - ptr == sizeof(void *),
				 "Didn't move free pointer exactly the right amount.");
}

void TEST_FAILS test_alloc_a_lot(void) {
	init_free_lists();
	init_generations();
	init_nurseries(1);
	Nursery_t *nursery = get_nursery(0);
	word mem = 0;
	for (int i = 0; ; i++) {
		printf("%p\n", alloc_obj(nursery, 4096));
		mem += 4096;
		printf("%d\n", mem);
	}
}
