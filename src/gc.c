/* Copyright 2013 Kyle Miller
 * gc.c
 * Garbage collector/memory allocator.  Loosely based on GHC's block-based allocator.
 */

#include <stdlib.h>
#include <stdbool.h>
#include "gc.h"
#include "util.h"
#include <stdint.h>
#include "constants.h"
#include "blocks.h"
#include "objects.h"

Generation_t generations[MAX_GENERATIONS];
Nursery_t nurseries[MAX_GC_THREADS];

// Number of steps per generation; 0 to terminate.
int default_generation_config[] = {2, 2, 1, 0};

// generation_config is a zero-terminated array of integers, each of
// which gives the number of steps for the given generation.
void init_generations(int generation_config[]) {
	int k = 0; // index into generations[]
	for (int n = 0; n < MAX_GENERATIONS && generation_config[n] != 0; n++) {
		for (int i = 0; i < generation_config[n]; i++, k++) {
			Generation_t *gen = &generations[k];
			gen->num = n;
			gen->blocks = NULL;
			gen->n_blocks = 0;
			gen->large = NULL;
			gen->remembered = (void *)-1;
			if (generation_config[n + 1] == 0) {
				gen->to_gen = NULL;
				break;
			} else {
				gen->to_gen = &generations[k + 1];
			}
		}
	}
	if (k == 0) {
		generations[0].num = -1; // sentinel for testing
	}
}

void init_nurseries(int num_threads) {
	guard(num_threads <= MAX_GC_THREADS,
				"Number of threads exceeds MAX_GC_THREADS");
	for (int i = 0; i < num_threads; i++) {
		Nursery_t *nursery = &nurseries[i];
		Blockinfo_t *free = NULL;
		for (int j = 0; j < NURSERY_BLOCKS; j++) {
			Blockinfo_t *block = alloc_group(1);
			assert(block != NULL, "Got bad block");
			assert(block->start != NULL, "block has bad start");
			block->link = free;
			block->gen = &generations[0];
			free = block;
		}
		nursery->blocks = free;
		nursery->alloc_block = free;
		nursery->alloc_block->free_ptr = nursery->alloc_block->start;
		assert(nursery->alloc_block->free_ptr != NULL, "Bad free pointer");
	}
}

Nursery_t *get_nursery(int i) {
	return &nurseries[i];
}

Obj_t *alloc_obj(Nursery_t *nursery, word size) {
	assert(((word)nursery - (word)nurseries) % sizeof(nursery) == 0,
				 "Bad nursery pointer");
	if (size > BLOCK_SIZE) {
		// The object is kind of big; allocate a group for it.
		word blocks = (size + BLOCK_SIZE - 1) >> BLOCK_SIZE_LG;
		assert(blocks * BLOCK_SIZE >= size,
					 "Not getting enough blocks for given size.");
		Blockinfo_t *block = alloc_group(blocks);
		block->link = generations[0].large;
		generations[0].large = block;
		return (Obj_t *)block->start;
	}
	if (nursery->alloc_block != NULL && BLOCK_SIZE - ((word)nursery->alloc_block->free_ptr - (word)nursery->alloc_block->start) < size) {
		// The object won't fit in the free space of the current
		// allocation block.  Just go on to the next allocation block.
		nursery->alloc_block = nursery->alloc_block->link;
		if (nursery->alloc_block != NULL) {
			nursery->alloc_block->free_ptr = nursery->alloc_block->start;
		}
	}
	if (nursery->alloc_block == NULL) {
		garbage_collect();
		assert(nursery->alloc_block != NULL,
					 "Garbage collection didn't free up block");
	}
	Obj_t *obj = nursery->alloc_block->free_ptr;
	nursery->alloc_block->free_ptr = (void *)NEXT_PTR_ALIGNED((word)nursery->alloc_block->free_ptr + size);
	return obj;
}

void garbage_collect(void) {
	Generation_t *gen;
	uint16_t num = -1;
	for (gen = generations[0]; gen != NULL; gen = gen->to_gen) {
		if (gen->n_blocks + gen->n_large_blocks > n_max_blocks) {
			num = gen->num;
		}
	}
	error("Need to implement GC trigger here.");
}

void gc_evacuate(Obj_t **ptr) {
	
}

void gc_scavange(Obj_t *obj) {
	Blockinfo_t *blockinfo = get_blockinfo(obj);
}
