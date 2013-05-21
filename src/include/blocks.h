/* Copyright 2013 Kyle Miller
 * blocks.h
 *
 * Definitions of memory management structures
 */

#ifndef clangor_blocks_h
#define clangor_blocks_h

#include <stdint.h>
#include "constants.h"
#include "util.h"

// For Mac OS X (since that environment doesn't have MAP_ANONYMOUS)
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

// Definitions for MEGABLOCK_SIZE_LG and BLOCK_SIZE_LG are in constants.h

// The actual size of a megablock in bytes
#define MEGABLOCK_SIZE (1<<MEGABLOCK_SIZE_LG)
// A mask to get the megablock of a pointer
#define MEGABLOCK_MASK (~(MEGABLOCK_SIZE-1))
// The size of a block in bytes
#define BLOCK_SIZE (1<<BLOCK_SIZE_LG)
// A mask to get the block of a pointer
#define BLOCK_MASK (~(BLOCK_SIZE-1))
// The effective size of a blockinfo (which is a power of two for bit convenience)
#define BLOCKINFO_SIZE (sizeof(struct Blockinfo_aligned_s))
// The number of blocks which are useable in a megablock, since the
// beginning of a megablock is used by the blockinfos
#define NUM_USABLE_BLOCKS ((word)(MEGABLOCK_SIZE / (BLOCK_SIZE + BLOCKINFO_SIZE)))
// The number of blocks there would be if there weren't blockinfos 
#define NUM_BLOCKS ((word)MEGABLOCK_SIZE / BLOCK_SIZE)
// The index of the first usable block (the first block after the
// blockinfos)
#define FIRST_USABLE_BLOCK (NUM_BLOCKS - NUM_USABLE_BLOCKS)

// Takes a number of blocks and gives the minimum number of megablocks
// required to store those blocks.  Assumes the only blockinfos are
// the ones at the beginning of the first megablock.
#define BLOCKS_TO_MEGABLOCKS(n)                                         \
  (1 + ((MEGABLOCK_MASK & (word)((n) + FIRST_USABLE_BLOCK - 1)*BLOCK_SIZE) >> MEGABLOCK_SIZE_LG))
// Takes a number of megablocks and gives the number of blocks
// therein, assuming beginning of the first megablock is being
// utilized for blockinfos.  Assumes n is at least 1.
#define MEGABLOCKS_TO_BLOCKS(n)                    \
  (NUM_USABLE_BLOCKS + ((n) - 1) * NUM_BLOCKS)

// Takes an arbitrary pointer and gives the megablock it is in.
#define TO_MEGABLOCK(n) \
  ((Megablock_t *)(MEGABLOCK_MASK & (word)(n)))

// Gives the first useable blockinfo of the megablock a pointer is in.
#define FIRST_BLOCKINFO(n) \
  ((void *)((struct Blockinfo_aligned_s *)TO_MEGABLOCK(n) + (word)FIRST_USABLE_BLOCK))
// Gives the last blockinfo of the megablock a pointer is in.
#define LAST_BLOCKINFO(n) \
  ((void *)((struct Blockinfo_aligned_s *)TO_MEGABLOCK(n) + (word)(NUM_BLOCKS - 1)))


// Descriptor for a block.
typedef struct Blockinfo_s {
  void *start; // start address of block [constant] (for convenience)
  void *free_ptr; // first byte of free memory, zero if this is not
                  // the head of the group, or -1 if this the group
                  // head is free
  word blocks; // number of blocks in group, or zero if this is not the
              // head of the group
  struct Blockinfo_s *link; // for chaining blocks into an area, or
                               // links the last block of a group to
                               // the head of its group
  struct Blockinfo_s *back; // for a doubly-linked free list
  struct Generation_s *gen; // generation
  uint16_t flags; // block flags (see BF_*)
} Blockinfo_t;

// Block contains objects evacuated during this GC
#define BF_EVACUATED 1
// Block is a large object
#define BF_LARGE     2
// Block is pinned
#define BF_PINNED    4
// Block is to be marked, not copied
#define BF_MARKED   16

// This is a power-of-two aligned version of Blockinfo_t so we can
// easily find a blockinfo for a corresponding pointer in a block
struct Blockinfo_aligned_s {
  Blockinfo_t blockinfo;
  uint8_t __padding[next_power_of_2(sizeof(Blockinfo_t)) - sizeof(Blockinfo_t)];
};

// A block of data
typedef struct Block_s {
  uint8_t data[BLOCK_SIZE];
} Block_t;

// A megablock.  Set up so that blockinfo[i] is the blockinfo for
// blocks[i] (so long as FIRST_USABLE_BLOCK <= i < NUM_BLOCKS)
typedef union {
  struct Blockinfo_aligned_s blockinfos[NUM_BLOCKS];
  Block_t blocks[NUM_BLOCKS];
} Megablock_t;


// API

void init_free_lists(void);

Blockinfo_t *alloc_group(word blocks);

void free_group(Blockinfo_t *blockinfo);


// Useful inline functions

// Get a blockinfo for a block which contains the given pointer
inline
Blockinfo_t *get_blockinfo(void *ptr) {
  word block = (word)ptr & BLOCK_MASK;
  Megablock_t *megablock = (Megablock_t *)TO_MEGABLOCK(block);
  word blockinfo_num = (~MEGABLOCK_MASK & block) >> BLOCK_SIZE_LG;
  assert(blockinfo_num >= NUM_BLOCKS - NUM_USABLE_BLOCKS,
         "blockinfo_num is within blockinfos!");
  return &megablock->blockinfos[blockinfo_num].blockinfo;
}

// Debug

void verify_free_megablock_list(void);
void verify_free_block_list(void);
void debug_print_free_megablock_list(void);
void debug_print_free_block_list(void);
void assert_free_block_list_empty(void);

#endif
