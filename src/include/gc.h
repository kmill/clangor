/* Copyright 2013 Kyle Miller
 * gc.h
 * Garbage collector/memory allocator
 */

#ifndef clangor_gc_h
#define clangor_gc_h

// Garbage collector definitions

// typedef struct GC_s GC_t;

//GC_t* gc_new_manager(void);
//void* gc_alloc(GC_t* gc

#include <blocks.h>
#include <objects.h>

#define MAX_GENERATIONS 16
#define MAX_GC_THREADS 1
#define NURSERY_BLOCKS 128

// Aligns a pointer to a void * multiple.
#define NEXT_PTR_ALIGNED(x)																\
	(((word)(x) + sizeof(void *) - 1) & ~(sizeof(void *) - 1))

// A generation of the garbage collector
typedef struct Generation_s {
  uint16_t num; // generation number
  Blockinfo_t *blocks; // blocks in this generation
  word n_blocks;
	Blockinfo_t *large; // large objects, doubly linked
	//  word n_words;
  Obj_t *remembered; // linked list of objects in the remembered set
  struct Generation_s *to_gen; // destination generation for live objects
  Blockinfo_t *old_blocks;
  word old_n_blocks;
	Blockinfo_t *old_large;
} Generation_t;

typedef struct Nursery_s {
	Blockinfo_t *blocks;
	Blockinfo_t *alloc_block;
} Nursery_t;

// API

void init_generations(void);
void init_nurseries(int num_threads);
Obj_t *alloc_obj(Nursery_t *nursery, word size);

Nursery_t *get_nursery(int i);

#endif
