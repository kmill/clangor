/* Copyright 2013 Kyle Miller
 * gc.c
 * Garbage collector/memory allocator.  Loosely based on GHC's block-based allocator.
 */

#include <sys/mman.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gc.h"
#include "util.h"
#include <stdint.h>
#include "constants.h"


#define GC_MEGABLOCK_SIZE (1<<GC_MEGABLOCK_SIZE_LG)
#define GC_MEGABLOCK_MASK (~(GC_MEGABLOCK_SIZE-1))
#define GC_BLOCK_SIZE (1<<GC_BLOCK_SIZE_LG)
#define GC_BLOCK_MASK (~(GC_BLOCK_SIZE-1))
#define GC_BLOCKINFO_SIZE (next_power_of_2(sizeof(GC_Blockinfo_t)))
#define GC_NUM_USABLE_BLOCKS                                        \
  ((word)(GC_MEGABLOCK_SIZE / (GC_BLOCK_SIZE + GC_BLOCKINFO_SIZE)))
#define GC_NUM_BLOCKS ((word)GC_MEGABLOCK_SIZE / GC_BLOCK_SIZE)
#define GC_FIRST_USABLE_BLOCK (GC_NUM_BLOCKS - GC_NUM_USABLE_BLOCKS)

// Takes a number of blocks and gives the minimum number of megablocks
// required to store those blocks.  Assumes the only Blockinfos are
// the ones at the beginning of the first megablock.
#define GC_BLOCKS_TO_MEGABLOCKS(n)                                         \
  (1 + ((GC_MEGABLOCK_MASK & (word)((n) + GC_FIRST_USABLE_BLOCK - 1)*GC_BLOCK_SIZE) >> GC_MEGABLOCK_SIZE_LG))
// Takes a number of megablocks and gives the number of blocks
// therein, assuming beginning of the first megablock is being
// utilized for blockinfos.  Assumes n is at least 1.
#define GC_MEGABLOCKS_TO_BLOCKS(n)                    \
  (GC_NUM_USABLE_BLOCKS + ((n) - 1) * GC_NUM_BLOCKS)

#define GC_TO_MEGABLOCK(n) \
  ((GC_Megablock_t *)(GC_MEGABLOCK_MASK & (word)(n)))

#define GC_LAST_BLOCKINFO(n) \
  ((void *)((struct GC_Blockinfo_aligned_s *)GC_TO_MEGABLOCK(n) + (word)(GC_NUM_BLOCKS - 1)))
#define GC_FIRST_BLOCKINFO(n) \
  ((void *)((struct GC_Blockinfo_aligned_s *)GC_TO_MEGABLOCK(n) + (word)GC_FIRST_USABLE_BLOCK))

// Descriptor for a block.
typedef struct GC_Blockinfo_s {
  void *start; // start address of block [constant] (for convenience)
  void *free_ptr; // first byte of free memory, zero if this is not
                  // the head of the group, or -1 if this the group
                  // head is free
  word blocks; // number of blocks in group, or zero if this is not the
              // head of the group
  struct GC_Blockinfo_s *link; // for chaining blocks into an area, or
                               // links the last block of a group to
                               // the head of its group
  struct GC_Blockinfo_s *back; // for a doubly-linked free list
  struct GC_Generation_s *gen; // generation
  uint16_t flags; // block flags (see BF_*)
} GC_Blockinfo_t;

// Block contains objects evacuated during this GC
#define BF_EVACUATED 1
// Block is a large object
#define BF_LARGE     2
// Block is pinned
#define BF_PINNED    4
// Block is to be marked, not copied
#define BF_MARKED   16

// power-of-two aligned for easier computations
struct GC_Blockinfo_aligned_s {
  GC_Blockinfo_t blockinfo;
  uint8_t __padding[GC_BLOCKINFO_SIZE - sizeof(GC_Blockinfo_t)];
};

typedef struct GC_Block_s {
  uint8_t data[GC_BLOCK_SIZE];
} GC_Block_t;

typedef union {
  struct GC_Blockinfo_aligned_s blockinfos[GC_NUM_BLOCKS];
  GC_Block_t blocks[GC_NUM_BLOCKS];
} GC_Megablock_t;

// Get a blockinfo from a pointer
static inline
GC_Blockinfo_t *gc_get_blockinfo(void *ptr) {
  word block = (word)ptr & GC_BLOCK_MASK;
  GC_Megablock_t *megablock = (GC_Megablock_t *)GC_TO_MEGABLOCK(block);
  word blockinfo_num = (~GC_MEGABLOCK_MASK & block) >> GC_BLOCK_SIZE_LG;
  assert(blockinfo_num >= GC_NUM_BLOCKS - GC_NUM_USABLE_BLOCKS,
         "blockinfo_num is within blockinfos!");
  return &megablock->blockinfos[blockinfo_num].blockinfo;
}

// A generation of the garbage collector
typedef struct GC_Generation_s {
  uint16_t num; // generation number
  GC_Blockinfo_t *blocks; // blocks in this generation
  word n_blocks;
  word n_words;
  GC_Blockinfo_t *remembered; // blocks containing lists of pointers to earlier generations
  struct GC_Generation_s *to_gen; // destination generation for live objects
  GC_Blockinfo_t *old_blocks;
  word old_n_blocks;
} GC_Generation_t;


// Allocate some number of megablocks at the megabyte boundary.  The
// technique is to allocate one more block than required and then
// munmap-ing the slop.
static
GC_Megablock_t *gc_alloc_megablocks(word n_megablocks) {
  word size = GC_MEGABLOCK_SIZE * n_megablocks;
  // Allocate one more megablock than expected so we can ensure alignment
  void *ptr = mmap(NULL, size + GC_MEGABLOCK_SIZE,
                   PROT_READ | PROT_WRITE, MAP_PRIVATE | GC_MMAP_MAPPING,
                   -1, 0);
  if (ptr == MAP_FAILED) {
    error("gc_alloc_megablocks unable to allocate blocks using mmap");
  }
  word slop = (word)ptr & ~GC_MEGABLOCK_MASK;
  if (slop == 0) {
    slop += GC_MEGABLOCK_SIZE;
  }
  if (GC_MEGABLOCK_SIZE - slop > 0 && munmap(ptr, GC_MEGABLOCK_SIZE - slop) == -1) {
    error("gc_alloc_megablocks unable to unmap pre-slop");
  }
  if (munmap(ptr + size + GC_MEGABLOCK_SIZE - slop, slop) == -1) {
    error("gc_alloc_megablocks unable to unmap post-slop");
  }
  void *res = ptr + GC_MEGABLOCK_SIZE - slop;
  assert(0 == ((word)res & ~GC_MEGABLOCK_MASK),
         "gc_alloc_megablocks made misaligned megablock.");
  return res;
}



// Initialize the Blockinfo.start's of the megablock.
static inline
void gc_init_megablock(GC_Megablock_t *megablock) {
  for (word i = GC_FIRST_USABLE_BLOCK; i < GC_NUM_BLOCKS; i++) {
    megablock->blockinfos[i].blockinfo.start = &megablock->blocks[i];
  }
}

// Initializes a group of blocks, assuming blockinfo->blocks is set to
// the number of blocks in the group.
static inline
void gc_init_group(GC_Blockinfo_t *blockinfo) {
  GC_Blockinfo_t *curr_bi;
  word i;
  blockinfo->free_ptr = blockinfo->start;
  blockinfo->link = NULL;
  for (i = 1, curr_bi = blockinfo + 1; i < blockinfo->blocks; i++, curr_bi++) {
    curr_bi->free_ptr = 0;
    curr_bi->blocks = 0;
    curr_bi->link = blockinfo;
  }
}

static inline
void gc_fix_group_tail(GC_Blockinfo_t *blockinfo) {
  GC_Blockinfo_t *tail = blockinfo + blockinfo->blocks - 1;
  if (tail != blockinfo) {
    tail->blocks = 0;
    tail->free_ptr = 0;
    tail->link = blockinfo;
  }
}

static GC_Blockinfo_t *free_megablock_list;

#define GC_FREE_LIST_SIZE  (GC_MEGABLOCK_SIZE_LG - GC_BLOCK_SIZE_LG + 1)
// free_block_list[i] holds blocks of size 2^i to 2^{i+1}-1.
static GC_Blockinfo_t *free_block_list[GC_FREE_LIST_SIZE];

// Basic data consistency checks on free_megablock_list
void gc_verify_free_megablock_list(void) {
  GC_Blockinfo_t *curr;
  for (curr = free_megablock_list; curr != NULL; curr = curr->link) {
    if (curr->link != NULL) {
      assert(curr->start < curr->link->start, "Order invariant broken");
      assert((word)GC_TO_MEGABLOCK(curr->link) - (word)GC_TO_MEGABLOCK(curr) > curr->blocks * (word)GC_BLOCK_SIZE,
             "Not enough distance between disconnected blocks.");
    }
  }
}

// Basic data consistency checks on free_block_list
void gc_verify_free_block_list(void) {
  for (int i = 0; i < GC_FREE_LIST_SIZE; i++) {
    for (GC_Blockinfo_t *b = free_block_list[i]; b != NULL; b = b->link) {
      assert(b->blocks >= (1 << i), "Not-big-enough group free list");
      assert(b->blocks < (1 << i + 1), "Too-big group in free list");
      assert(b->free_ptr == (void *)-1, "Not marked as free");
      GC_Blockinfo_t *tail = b + b->blocks - 1;
      if (tail != b) {
        assert(tail->blocks == 0, "Tail not marked as free");
        assert(tail->free_ptr == 0, "Tail not marked as free");
        assert(tail->link == b, "Tail not pointing to group head");
      }
    }
  } 
}

void gc_print_free_megablock_list(void) {
  printf("free_megablock_list:\n");
  for (GC_Blockinfo_t *curr = free_megablock_list; curr != NULL; curr = curr->link) {
    printf("  Megablock %p: megablocks=%d (blocks=%d)\n",
           curr, GC_BLOCKS_TO_MEGABLOCKS(curr->blocks), curr->blocks);
    assert(GC_MEGABLOCKS_TO_BLOCKS(GC_BLOCKS_TO_MEGABLOCKS(curr->blocks)) == curr->blocks,
           "These aren't full megablock groups.");
  }
  printf("  (end free_megablock_list)\n");
}

void gc_print_free_block_list(void) {
  printf("free lists ...\n");
  for (int i = 0; i < GC_FREE_LIST_SIZE; i++) {
    if (free_block_list[i] == NULL) {
      printf("  for 2^%d (>=%d) is empty\n", i, 1<<i);
    } else {
      printf("  for 2^%d (>=%d)\n", i, 1<<i);
      for (GC_Blockinfo_t *b = free_block_list[i]; b != NULL; b = b->link) {
        printf("    Block %p: blocks=%d\n",
               b, b->blocks);
      }
    }
  }
}

void gc_init_free_lists(void) {
  free_megablock_list = NULL;
  for (int i = 0; i < GC_FREE_LIST_SIZE; i++) {
    free_block_list[i] = NULL;
  }
}

static inline
word log2_floor(word n) {
  word i;
  for (i = -1; n != 0; n >>= 1, i++)
    ;
  return i;
}
static inline
word log2_ceil(word n) {
  word i, x;
  for (x = 1, i = 0; x < n; x <<= 1, i++)
    ;
  return i;
}

// Allocate some number of megablocks from the freelist (if possible)
static
GC_Blockinfo_t *gc_alloc_megagroup(word megablocks) {
  GC_Blockinfo_t *blockinfo, *best, *prev;

  word blocks = GC_MEGABLOCKS_TO_BLOCKS(megablocks);
  assert(GC_BLOCKS_TO_MEGABLOCKS(blocks) == megablocks,
         "Something is wrong with GC_MEGABLOCKS_TO_BLOCKS");
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

  GC_Megablock_t *megablock;
  if (best) {
    // Take a chunk off the end.
    word best_megablocks = GC_BLOCKS_TO_MEGABLOCKS(best->blocks);
    megablock = GC_TO_MEGABLOCK(best) + (best_megablocks - megablocks);
    best->blocks = GC_MEGABLOCKS_TO_BLOCKS(best_megablocks - megablocks);
  } else {
    // Nothing was suitable.  Allocate it fresh
    megablock = gc_alloc_megablocks(megablocks);
  }
  blockinfo = &megablock->blockinfos[GC_FIRST_USABLE_BLOCK].blockinfo;
  gc_init_megablock(megablock);
  blockinfo->blocks = blocks;
  return blockinfo;
}

// Coalesces a block to block->link as appropriate.  Returns the last
// of {block, block->link} modulo coalescing so coalescing can be
// chained.
static inline
GC_Blockinfo_t *gc_coalesce_megablocks(GC_Blockinfo_t *blockinfo) {
  GC_Blockinfo_t *next = blockinfo->link;
  if (next != NULL) {
    word megablocks = GC_BLOCKS_TO_MEGABLOCKS(blockinfo->blocks);
    if (GC_TO_MEGABLOCK(blockinfo) == GC_TO_MEGABLOCK(next) - megablocks) {
      blockinfo->link = next->link;
      blockinfo->blocks = GC_MEGABLOCKS_TO_BLOCKS(megablocks + GC_BLOCKS_TO_MEGABLOCKS(next->blocks));
      next = blockinfo;
    }
  }
  return next;
}

// Add a megagroup to the free_megablock_list, coalescing as appropriate.
static
void gc_free_megagroup(GC_Blockinfo_t *blockinfo) {
  GC_Blockinfo_t *prev, *curr;
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
    blockinfo = gc_coalesce_megablocks(prev);
  } else {
    // we're at the front
    blockinfo->link = free_megablock_list;
    free_megablock_list = blockinfo;
  }
  // coalesce forwards
  gc_coalesce_megablocks(blockinfo);
#ifdef DEBUG
  gc_verify_free_megablock_list();
#endif
}

static
void gc_free_group(GC_Blockinfo_t *blockinfo);

// Remove a block from a list, double-linked.
static
void gc_unlink_blockinfo(GC_Blockinfo_t *removed, GC_Blockinfo_t **list) {
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
void gc_link_blockinfo(GC_Blockinfo_t *added, GC_Blockinfo_t **list) {
  added->link = *list;
  added->back = NULL;
  if (*list != NULL) {
    (*list)->back = added;
  }
  *list = added;
}

// Split a free block group into two, where the the second part will
// have the given number of blocks.
static
GC_Blockinfo_t *gc_split_free_group(GC_Blockinfo_t *blockinfo, word blocks, word i) {
  assert(blockinfo->blocks > blocks, "Splitting a group which is too small.");
  // remove from free list since size is changing
  gc_unlink_blockinfo(blockinfo, &free_block_list[i]);
  // Take the block off the end of this one.
  GC_Blockinfo_t *cut = blockinfo + blockinfo->blocks - blocks;
  cut->blocks = blocks;
  blockinfo->blocks -= blocks;
  gc_fix_group_tail(blockinfo);
  // add back to the free list
  i = log2_floor(blockinfo->blocks);
  gc_link_blockinfo(blockinfo, &free_block_list[i]);
  return cut;
}

// Allocate a region of memory of a given number of blocks
static
GC_Blockinfo_t *gc_alloc_group(word blocks) {
  if (blocks == 0) {
    error("zero blocks requested in gc_alloc_group");
  }

  GC_Blockinfo_t *blockinfo;

  if (blocks >= GC_NUM_USABLE_BLOCKS) {
    // What a big object!  Allocate as contiguous megablocks.  Don't
    // care about waste.
    word num_mblocks = GC_BLOCKS_TO_MEGABLOCKS(blocks);
    blockinfo = gc_alloc_megagroup(num_mblocks);
    gc_init_group(blockinfo);
    return blockinfo;
  } else {
    // Fits within a single megablock.  Find a free block group from the
    // free list of the right size.
    word i = log2_ceil(blocks);
    assert(i < GC_FREE_LIST_SIZE, "Megablocks should have handled this.");
    while (i < GC_FREE_LIST_SIZE && free_block_list[i] == NULL) {
      i++;
    }
    if (i == GC_FREE_LIST_SIZE) {
      // Didn't find a free block.  Need to allocate a megablock.
      blockinfo = gc_alloc_megagroup(1);
      blockinfo->blocks = blocks;
      gc_init_group(blockinfo);
      GC_Blockinfo_t *remainder = blockinfo + blocks; // assumes blockinfos are contiguous
      remainder->blocks = GC_NUM_USABLE_BLOCKS - blocks;
      gc_init_group(blockinfo); // must happen before
                                // gc_free_group(remainder) since it
                                // might get coalesced
      gc_free_group(remainder);
      return blockinfo;
    } else {
      // Found one
      blockinfo = free_block_list[i];
      if (blockinfo->blocks == blocks) {
        // right size: don't need to split
        gc_unlink_blockinfo(blockinfo, &free_block_list[i]);
        gc_init_group(blockinfo);
      } else {
        // else the block is too big
        assert(blockinfo->blocks > blocks, "Free list is corrupted.");
        blockinfo = gc_split_free_group(blockinfo, blocks, i);
        assert(blockinfo->blocks == blocks, "Didn't split block properly.");
        gc_init_group(blockinfo);
      }
      return blockinfo;
    }
  }
}

static
void gc_free_group(GC_Blockinfo_t *blockinfo) {
  assert(blockinfo->free_ptr != (void *)-1, "Group is already freed.");
  assert(blockinfo->blocks != 0, "Group size is zero (maybe part of a group).");
  blockinfo->free_ptr = (void *)-1;
  blockinfo->gen = NULL;
  if (blockinfo->blocks >= GC_NUM_USABLE_BLOCKS) {
    // It's a megagroup
    word num_mblocks = GC_BLOCKS_TO_MEGABLOCKS(blockinfo->blocks);
    assert(blockinfo->blocks == GC_MEGABLOCKS_TO_BLOCKS(num_mblocks),
           "Number of blocks reported by megagroup does not match expected number.");
    gc_free_megagroup(blockinfo);
    return;
  } else {
    // It's a sub-megagroup group

    // Coalesce forwards
    if (blockinfo != GC_LAST_BLOCKINFO(blockinfo)) {
      GC_Blockinfo_t *next = blockinfo + blockinfo->blocks;
      if (next->free_ptr == (void *)-1) {
        blockinfo->blocks += next->blocks;
        word i = log2_floor(next->blocks);
        gc_unlink_blockinfo(next, &free_block_list[i]);
        if (blockinfo->blocks == GC_NUM_USABLE_BLOCKS) {
          // hooray, we completed a tetris
          gc_free_megagroup(blockinfo);
          return;
        }
        gc_fix_group_tail(blockinfo);
      }
    }

    // Coalesce backwards
    if (blockinfo != GC_FIRST_BLOCKINFO(blockinfo)) {
      GC_Blockinfo_t *prev = blockinfo - 1;
      if (prev->blocks == 0) {
        // get the head of this non-head block
        prev = prev->link;
      }
      if (prev->free_ptr == (void *)-1) {
        word i = log2_floor(prev->blocks);
        gc_unlink_blockinfo(prev, &free_block_list[i]);
        prev->blocks += blockinfo->blocks;
        if (prev->blocks == GC_NUM_USABLE_BLOCKS) {
          gc_free_megagroup(prev);
          return;
        }
        blockinfo = prev;
      }
    }
    gc_fix_group_tail(blockinfo);
    word i = log2_floor(blockinfo->blocks);
    assert(i < GC_NUM_USABLE_BLOCKS, "We should have already taken care of megablocks");
    gc_link_blockinfo(blockinfo, &free_block_list[i]);
  }
}


/* struct GC_s { */
/*   GC_Block_t* first_block; */
/* }; */

/* // Allocation of objects */

/* static inline bool gc_page_can_hold(GC_Page_t* page, size_t space) { */
/*   return space <= (void*)(page + 1) - (void*)page->metadata.free_ptr; */
/* } */

/* static inline void gc_page_fix_next_free(GC_Page_t* last_page, GC_Page_t* page) { */
/*   if (page->metadata.free_ptr - (void*)page == 4096) { */
/*     debug("Page full."); */
/*     // Then the page is full */
/*     if (last_page == NULL) { */
/*       page->metadata.parent_block->blockinfo.next_free = page->metadata.next_free; */
/*     } else { */
/*       last_page->metadata.next_free = page->metadata.next_free; */
/*     } */
/*   } */
/* } */

/* static void* gc_allocate_space(GC_t* manager, size_t space) { */
/*   space = (space + 3) & ~3; // round space up to next multiple of 4 */
/*   if (space > GC_PAGE_SIZE) { */
/*     error ("Need to fix allocator so it can do multi-page allocation."); */
/*   } */
/*   GC_Block_t* block = manager->first_block; */
/*   while (true) { */
/*     for (GC_Page_t* page = block->blockinfo.next_free, * last_page = NULL; page != NULL; last_page = page, page = page->metadata.next_free) { */
/*       if (page->metadata.free_ptr == NULL) { */
/*         debug("fresh page. &page->data = %p", &page->data); */
/*         // This is a fresh page.  We can just allocate here. */
/*         page->metadata.free_ptr = space + (void*)&page->data; */
/*         gc_page_fix_next_free(last_page, page); */
/*         return &page->data; */
/*       } else if (gc_page_can_hold(page, space)) { */
/*         debug("non-fresh page"); */
/*         // It fits in this page. */
/*         void* ptr = page->metadata.free_ptr; */
/*         page->metadata.free_ptr += space; */
/*         gc_page_fix_next_free(last_page, page); */
/*         return ptr; */
/*       } */
/*     } */
/*     // This block had no pages with sufficient space.  Let's allocate another block. */
/*     block = block->blockinfo.next_block = gc_allocate_block(manager); */
/*   } */
/* } */

/* GC_t* gc_new_manager(void) { */
/*   GC_t* manager = malloc(sizeof(GC_t)); */
/*   manager->first_block = gc_allocate_block(manager); */
/*   return manager; */
/* } */

/* static int gc_test() { */
/*   GC_t* manager = gc_new_manager(); */
/*   GC_Block_t* block = manager->first_block; */
/*   printf("sizeof(GC_Page_t)=%p\n", sizeof(GC_Page_t)); */
/*   printf("Got block %x\n", block); */
/*   block->pages[15].data[22] = 13; */
/*   printf("Parent block %x\n", block->pages[15].metadata.parent_block); */
/*   printf("free block %x\n", block->blockinfo.next_free); */
/*   for (int i = 0; i < 4; i++) { */
/*     printf("%d. gc_allocate_space(manager, 4084) = %p\n", i, gc_allocate_space(manager, -1+GC_PAGE_SIZE)); */
/*   } */
/*   for (int i = 0; i < 4; i++) { */
/*     printf("block->pages[i].metadata.free_ptr = %p\n", block->pages[i].metadata.free_ptr); */
/*   } */
/*   return; */
/* } */

int main() {
  //  gc_test();
  printf("sizeof(GC_Blockinfo_t) = %d\n", sizeof(GC_Blockinfo_t));
  printf("sizeof(struct GC_Blockinfo_aligned_s) = %d\n", sizeof(struct GC_Blockinfo_aligned_s));
  printf("sizeof(GC_Block_t) = %d\n", sizeof(GC_Block_t));
  printf("sizeof(GC_Megablock_t) = %d\n", sizeof(GC_Megablock_t));
  printf("GC_BLOCKINFO_SIZE = %d\n", GC_BLOCKINFO_SIZE);
  printf("GC_NUM_USABLE_BLOCKS = %d\n", GC_NUM_USABLE_BLOCKS);
  printf("GC_NUM_BLOCKS = %d\n", GC_NUM_BLOCKS);
  printf("GC_FIRST_USABLE_BLOCK = %d\n", GC_FIRST_USABLE_BLOCK);

  gc_init_free_lists();
  GC_Blockinfo_t *b[101];
  for (int i = 1; i < 101; i++) {
    int j = 1 + 3*(i-1) % 100;
    probe(j, "%d");
    b[j] = gc_alloc_group(j);
    gc_print_free_block_list();
    gc_verify_free_block_list();
    gc_verify_free_megablock_list();
  }
  for (int i = 1; i < 101; i++) {
    int j = 1 + 7*(i-1) % 100;
    probe(j, "%d");
    gc_free_group(b[j]);
    gc_print_free_block_list();
    gc_verify_free_block_list();
    gc_verify_free_megablock_list();
  }

/*   for (int i = 0; i <= 1; i++) { */
/*     probe(b[i], "%p"); */
/*     probe(b[i]->blocks, "%d"); */
/*     probe(b[i]->start, "%p"); */
/*   } */
/*   printf("%d\n", ((word)(b[1])-(word)(b[0]))/GC_BLOCKINFO_SIZE); */
  //  probe(GC_TO_MEGABLOCK(b[0]), "%p");
  //  probe(GC_FIRST_BLOCKINFO(b[0]), "%p");
  //  gc_free_group(b[1]);
  //  gc_free_group(b[0]);
  gc_print_free_block_list();
  gc_print_free_megablock_list();
  
/*   void* mem = gc_alloc_megablocks(1); */
/*   for (int i = 0; i < 1*GC_MEGABLOCK_SIZE; i++) { */
/*     *((uint8_t*)mem + i) = 22; */
/*   } */

/*   for (int i = 5; i < 15; i++) { */
/*     printf("%d, %ld\n", i, gc_get_blockinfo((void*)(0x1000*i + 1024*1024))); */
/*   } */

/*   printf("%d\n", GC_BLOCKS_TO_MEGABLOCKS(GC_NUM_USABLE_BLOCKS+GC_NUM_BLOCKS*1+1)); */
/*   printf("gc_alloc_megagroup(5) = %p\n", gc_alloc_megagroup(5)); */

/*   for (int i = 1; i < 7; i++) { */
/*     probe(i, "%d"); */
/*     GC_Blockinfo_t *bg = gc_alloc_megagroup(i); */
/*     assert(GC_MEGABLOCKS_TO_BLOCKS(i) == bg->blocks, "mb->b"); */
/*     assert(i == GC_BLOCKS_TO_MEGABLOCKS(bg->blocks), "b->mb"); */
/*     gc_free_megagroup(bg); */
/*     gc_print_free_megablock_list(); */
/*   } */


/*   printf("No frees:\n"); */
/*   gc_print_free_megablock_list(); */
/*   GC_Blockinfo_t *bg = gc_alloc_megagroup(1); */
/*   gc_print_free_megablock_list(); */
/*   gc_free_megagroup(bg); */
/*   printf("One free:\n"); */
/*   gc_print_free_megablock_list(); */
/*   gc_free_megagroup(gc_alloc_megagroup(1)); */
/*   printf("Two frees, alternating allocs:\n"); */
/*   gc_print_free_megablock_list(); */
/*   GC_Blockinfo_t *bg1 = gc_alloc_megagroup(1), *bg2 = gc_alloc_megagroup(1); */
/*   gc_free_megagroup(bg1); */
/*   gc_free_megagroup(bg2); */
/*   printf("Two frees after two allocs:\n"); */
/*   gc_print_free_megablock_list(); */
/*   gc_alloc_megagroup(2); */
/*   printf("Alloced 2-megagroup\n"); */
/*   gc_print_free_megablock_list(); */

/*   for (int i = 1; i < 40; i++) { */
/*     printf("log2_ceil(%d) = %d\tlog2_floor(%d) = %d\n", i, log2_ceil(i), i, log2_floor(i)); */
/*   } */

  return 0;
}
