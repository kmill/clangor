/* Copyright 2013 Kyle Miller
 * blocks.c
 * Functions for allocating/freeing blocks (basically an alternative to malloc).
 */

#include <sys/mman.h>
#include "blocks.h"

// Free lists

static Blockinfo_t *free_megablock_list;

#define FREE_LIST_SIZE  (MEGABLOCK_SIZE_LG - BLOCK_SIZE_LG + 1)
// free_block_list[i] holds blocks of size 2^i to 2^{i+1}-1.
static Blockinfo_t *free_block_list[FREE_LIST_SIZE];

// Initialize the megablock and block free lists
void init_free_lists(void) {
  free_megablock_list = NULL;
  for (int i = 0; i < FREE_LIST_SIZE; i++) {
    free_block_list[i] = NULL;
  }
}

// Remove a block from a list, double-linked.
static
void list_unlink_blockinfo(Blockinfo_t *removed, Blockinfo_t **list) {
  if (removed->back != NULL) {
    removed->back->link = removed->link;
  } else {
    // otherwise 'removed' was the beginning of the list
    *list = removed->link;
  }
  if (removed->link != NULL) {
    removed->link->back = removed->back;
  }
}

// Add a block to the front of a list, double-linked.
static
void list_link_blockinfo(Blockinfo_t *added, Blockinfo_t **list) {
  added->link = *list;
  added->back = NULL;
  if (*list != NULL) {
    (*list)->back = added;
  }
  *list = added;
}

// Compute floor(log2(n)).  Used for finding in which free list to
// store a block.
static inline
word log2_floor(word n) {
  word i;
  for (i = -1; n != 0; n >>= 1, i++)
    ;
  return i;
}
// Compute ceil(log2(n)). Used for finding in which free list to place
// a free block.
static inline
word log2_ceil(word n) {
  word i, x;
  for (x = 1, i = 0; x < n; x <<= 1, i++)
    ;
  return i;
}


// Allocate some number of raw megablocks at the megablock-size
// boundary.  The technique is to allocate one more megablock than
// required and then munmap-ing the slop.  This function shouldn't be
// confused with alloc_megagroup.
static
Megablock_t *alloc_megablocks(word n_megablocks) {
  word size = MEGABLOCK_SIZE * n_megablocks;
  // Allocate one more megablock than expected so we can ensure alignment
  void *ptr = mmap(NULL, size + MEGABLOCK_SIZE,
                   PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
  if (ptr == MAP_FAILED) {
    error("alloc_megablocks unable to allocate blocks using mmap");
  }
  word slop = (word)ptr & ~MEGABLOCK_MASK;
  if (slop == 0) {
    slop += MEGABLOCK_SIZE;
  }
  if (MEGABLOCK_SIZE - slop > 0 && munmap(ptr, MEGABLOCK_SIZE - slop) == -1) {
    error("alloc_megablocks unable to unmap pre-slop");
  }
  if (munmap(ptr + size + MEGABLOCK_SIZE - slop, slop) == -1) {
    error("alloc_megablocks unable to unmap post-slop");
  }
  void *res = ptr + MEGABLOCK_SIZE - slop;
  assert(0 == ((word)res & ~MEGABLOCK_MASK),
         "alloc_megablocks made misaligned megablock.");
  return res;
}

// Initialize the Blockinfo.start's of the megablock.
static inline
void init_megablock(Megablock_t *megablock) {
  for (word i = FIRST_USABLE_BLOCK; i < NUM_BLOCKS; i++) {
    megablock->blockinfos[i].blockinfo.start = &megablock->blocks[i];
  }
}

// Fixes the invariant that the last block in a group points to the
// head of the group.
static inline
void fix_group_tail(Blockinfo_t *blockinfo) {
  Blockinfo_t *tail = blockinfo + blockinfo->blocks - 1;
  if (tail != blockinfo) {
    tail->blocks = 0;
    tail->free_ptr = 0;
    tail->link = blockinfo;
  }
}

// Initializes a group of blocks, assuming blockinfo->blocks is set to
// the number of blocks in the group.  An initialized group of blocks
// has its tail block point back to the head of the group to detect
// appropriate coalescing.
static inline
void init_group(Blockinfo_t *blockinfo) {
  blockinfo->free_ptr = blockinfo->start;
  blockinfo->link = NULL;
  fix_group_tail(blockinfo);
}

// Allocate some group of megablocks from the freelist (if possible),
// otherwise allocates fresh megablocks.
static
Blockinfo_t *alloc_megagroup(word megablocks) {
  Blockinfo_t *blockinfo, *best, *prev;

  word blocks = MEGABLOCKS_TO_BLOCKS(megablocks);
  assert(BLOCKS_TO_MEGABLOCKS(blocks) == megablocks,
         "Something is wrong with MEGABLOCKS_TO_BLOCKS");
  best = prev = NULL;

  for (blockinfo = free_megablock_list; blockinfo != NULL; prev = blockinfo, blockinfo = blockinfo->link) {
    if (blockinfo->blocks == blocks) {
      // This block looks good.  Let's take it!
      if (prev) {
        prev->link = blockinfo->link;
      } else {
        free_megablock_list = blockinfo->link;
      }
      return blockinfo;
    } else if (blockinfo->blocks > blocks) {
      // Heuristic: it is better to break up a smaller megablock group
      if (best == NULL || blockinfo->blocks < best->blocks) {
        best = blockinfo;
      }
    }
  }

  Megablock_t *megablock;
  if (best) {
    // Take a chunk off the end.
    word best_megablocks = BLOCKS_TO_MEGABLOCKS(best->blocks);
    megablock = TO_MEGABLOCK(best) + (best_megablocks - megablocks);
    best->blocks = MEGABLOCKS_TO_BLOCKS(best_megablocks - megablocks);
  } else {
    // Nothing was suitable.  Allocate it fresh
    megablock = alloc_megablocks(megablocks);
  }
  blockinfo = &megablock->blockinfos[FIRST_USABLE_BLOCK].blockinfo;
  init_megablock(megablock);
  blockinfo->blocks = blocks;
  return blockinfo;
}


// Coalesces a block to block->link as appropriate.  Returns the last
// of {block, block->link} modulo coalescing so coalescing can be
// chained.
static inline
Blockinfo_t *coalesce_megablocks(Blockinfo_t *blockinfo) {
  Blockinfo_t *next = blockinfo->link;
  if (next != NULL) {
    word megablocks = BLOCKS_TO_MEGABLOCKS(blockinfo->blocks);
    if (TO_MEGABLOCK(blockinfo) == TO_MEGABLOCK(next) - megablocks) {
      blockinfo->link = next->link;
      blockinfo->blocks = MEGABLOCKS_TO_BLOCKS(megablocks + BLOCKS_TO_MEGABLOCKS(next->blocks));
      next = blockinfo;
    }
  }
  return next;
}

// Add a megagroup to the free_megablock_list, coalescing as appropriate.
static
void free_megagroup(Blockinfo_t *blockinfo) {
  Blockinfo_t *prev, *curr;
  prev = NULL;
  curr = free_megablock_list;
  while (curr != NULL && curr->start < blockinfo->start) {
    prev = curr;
    curr = curr->link;
  }
  if (prev != NULL) {
    blockinfo->link = prev->link;
    prev->link = blockinfo;
    // coalesce backwards
    blockinfo = coalesce_megablocks(prev);
  } else {
    // we're at the front
    blockinfo->link = free_megablock_list;
    free_megablock_list = blockinfo;
  }
  // coalesce forwards
  coalesce_megablocks(blockinfo);
#ifdef DEBUG
  verify_free_megablock_list();
#endif
}

// Split a free block group into two, where the the second part will
// have the given number of blocks.
static
Blockinfo_t *split_free_group(Blockinfo_t *blockinfo, word blocks, word i) {
  assert(blockinfo->blocks > blocks, "Splitting a group which is too small.");
  // remove from free list since size is changing
  list_unlink_blockinfo(blockinfo, &free_block_list[i]);
  // Take the block off the end of this one.
  Blockinfo_t *cut = blockinfo + blockinfo->blocks - blocks;
  cut->blocks = blocks;
  blockinfo->blocks -= blocks;
  fix_group_tail(blockinfo);
  // add back to the free list
  i = log2_floor(blockinfo->blocks);
  list_link_blockinfo(blockinfo, &free_block_list[i]);
  return cut;
}

// Allocate a region of memory of a given number of blocks
Blockinfo_t *alloc_group(word blocks) {
  if (blocks == 0) {
    error("zero blocks requested in alloc_group");
  }

  Blockinfo_t *blockinfo;

  if (blocks >= NUM_USABLE_BLOCKS) {
    // What a big object!  Allocate as contiguous megablocks.  Don't
    // care about waste.
    word num_mblocks = BLOCKS_TO_MEGABLOCKS(blocks);
    blockinfo = alloc_megagroup(num_mblocks);
    init_group(blockinfo);
    return blockinfo;
  } else {
    // Fits within a single megablock.  Try to find a free block group
    // from the free list of the right size.
    word i = log2_ceil(blocks);
    assert(i < FREE_LIST_SIZE, "Megablocks should have handled this.");
    while (i < FREE_LIST_SIZE && free_block_list[i] == NULL) {
      i++;
    }
    if (i == FREE_LIST_SIZE) {
      // Didn't find a free block.  Need to allocate a megablock.
      blockinfo = alloc_megagroup(1);
      blockinfo->blocks = blocks;
      init_group(blockinfo);
      Blockinfo_t *remainder = blockinfo + blocks; // assumes blockinfos are contiguous
      remainder->blocks = NUM_USABLE_BLOCKS - blocks;
      // init_group must happen before free_group(remainder) since
      // blockinfo might get coalesced into remainder:
      init_group(blockinfo);
      init_group(remainder); // to set up the free_ptr so free_group doesn't complain
      free_group(remainder);
      return blockinfo;
    } else {
      // Found one
      blockinfo = free_block_list[i];
      if (blockinfo->blocks == blocks) {
        // right size: don't need to split
        list_unlink_blockinfo(blockinfo, &free_block_list[i]);
        init_group(blockinfo);
      } else {
        // else the block is too big
        assert(blockinfo->blocks > blocks, "Free list is corrupted.");
        blockinfo = split_free_group(blockinfo, blocks, i);
        assert(blockinfo->blocks == blocks, "Didn't split block properly.");
        init_group(blockinfo);
      }
      return blockinfo;
    }
  }
}

// Returns a group to the free list.  If there are adjacent free
// groups in memory, they are coalesced.
void free_group(Blockinfo_t *blockinfo) {
  assert(blockinfo->free_ptr != (void *)-1, "Group is already freed.");
  assert(blockinfo->blocks != 0, "Group size is zero (maybe part of a group).");
  blockinfo->free_ptr = (void *)-1;
  blockinfo->gen = NULL;
  if (blockinfo->blocks >= NUM_USABLE_BLOCKS) {
    // It's a megagroup
    word num_mblocks = BLOCKS_TO_MEGABLOCKS(blockinfo->blocks);
    assert(blockinfo->blocks == MEGABLOCKS_TO_BLOCKS(num_mblocks),
           "Number of blocks reported by megagroup does not match expected number.");
    free_megagroup(blockinfo);
    return;
  } else {
    // It's a sub-megagroup group

    // Coalesce forwards
    if (blockinfo != LAST_BLOCKINFO(blockinfo)) {
      Blockinfo_t *next = blockinfo + blockinfo->blocks;
      if (next->free_ptr == (void *)-1) {
        blockinfo->blocks += next->blocks;
        word i = log2_floor(next->blocks);
        list_unlink_blockinfo(next, &free_block_list[i]);
        if (blockinfo->blocks == NUM_USABLE_BLOCKS) {
          // hooray, we completed a tetris
          free_megagroup(blockinfo);
          return;
        }
        assert(blockinfo->blocks < NUM_USABLE_BLOCKS,
               "A small block group crosses a megablock.");
        fix_group_tail(blockinfo);
      }
    }

    // Coalesce backwards
    if (blockinfo != FIRST_BLOCKINFO(blockinfo)) {
      Blockinfo_t *prev = blockinfo - 1;
      if (prev->blocks == 0) {
        // get the head of this non-head block
        prev = prev->link;
      }
      if (prev->free_ptr == (void *)-1) {
        word i = log2_floor(prev->blocks);
        list_unlink_blockinfo(prev, &free_block_list[i]);
        prev->blocks += blockinfo->blocks;
        if (prev->blocks == NUM_USABLE_BLOCKS) {
          free_megagroup(prev);
          return;
        }
        assert(blockinfo->blocks < NUM_USABLE_BLOCKS,
               "A small block group crosses a megablock.");
        blockinfo = prev;
      }
    }
    fix_group_tail(blockinfo);
    word i = log2_floor(blockinfo->blocks);
    assert(i < FREE_LIST_SIZE, "Block group is too big for free list.");
    list_link_blockinfo(blockinfo, &free_block_list[i]);
  }
}


////// Debugging routines

// Basic data consistency checks on free_megablock_list
void verify_free_megablock_list(void) {
  Blockinfo_t *curr;
  for (curr = free_megablock_list; curr != NULL; curr = curr->link) {
    if (curr->link != NULL) {
      assert(curr->start < curr->link->start, "Order invariant broken");
      assert((word)TO_MEGABLOCK(curr->link) - (word)TO_MEGABLOCK(curr) > curr->blocks * (word)BLOCK_SIZE,
             "Not enough distance between disconnected blocks.");
    }
  }
}

// Basic data consistency checks on free_block_list
void verify_free_block_list(void) {
  for (int i = 0; i < FREE_LIST_SIZE; i++) {
    for (Blockinfo_t *b = free_block_list[i]; b != NULL; b = b->link) {
      assert(b->blocks >= (1 << i), "Not-big-enough group free list");
      assert(b->blocks < (1 << i + 1), "Too-big group in free list");
      assert(b->free_ptr == (void *)-1, "Not marked as free");
      Blockinfo_t *tail = b + b->blocks - 1;
      if (tail != b) {
        assert(tail->blocks == 0, "Tail not marked as free");
        assert(tail->free_ptr == 0, "Tail not marked as free");
        assert(tail->link == b, "Tail not pointing to group head");
      }
    }
  } 
}

void debug_print_free_megablock_list(void) {
  printf("free_megablock_list:\n");
  for (Blockinfo_t *curr = free_megablock_list; curr != NULL; curr = curr->link) {
    printf("  Megablock %p: megablocks=%d (blocks=%d)\n",
           curr, BLOCKS_TO_MEGABLOCKS(curr->blocks), curr->blocks);
    assert(MEGABLOCKS_TO_BLOCKS(BLOCKS_TO_MEGABLOCKS(curr->blocks)) == curr->blocks,
           "These aren't full megablock groups.");
  }
  printf("  (end free_megablock_list)\n");
}

void debug_print_free_block_list(void) {
  printf("free lists ...\n");
  for (int i = 0; i < FREE_LIST_SIZE; i++) {
    if (free_block_list[i] == NULL) {
      printf("  for 2^%d (blocks >= %d) is empty\n", i, 1<<i);
    } else {
      printf("  for 2^%d (blocks >= %d)\n", i, 1<<i);
      for (Blockinfo_t *b = free_block_list[i]; b != NULL; b = b->link) {
        printf("    Block %p: blocks=%d\n",
               b, b->blocks);
      }
    }
  }
}

void assert_free_block_list_empty(void) {
  for (int i = 0; i < FREE_LIST_SIZE; i++) {
    if (free_block_list[i] != NULL) {
      error("Free list %d is not empty.", i);
    }
  }
}
