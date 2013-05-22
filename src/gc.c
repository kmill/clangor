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


// A generation of the garbage collector
typedef struct Generation_s {
  uint16_t num; // generation number
  Blockinfo_t *blocks; // blocks in this generation
  word n_blocks;
  word n_words;
  Blockinfo_t *remembered; // blocks containing lists of pointers to earlier generations
  struct Generation_s *to_gen; // destination generation for live objects
  Blockinfo_t *old_blocks;
  word old_n_blocks;
} Generation_t;




int main() {
  //  test();
  printf("sizeof(Blockinfo_t) = %d\n", sizeof(Blockinfo_t));
  printf("sizeof(struct Blockinfo_aligned_s) = %d\n", sizeof(struct Blockinfo_aligned_s));
  printf("sizeof(Block_t) = %d\n", sizeof(Block_t));
  printf("sizeof(Megablock_t) = %d\n", sizeof(Megablock_t));
  printf("BLOCKINFO_SIZE = %d\n", BLOCKINFO_SIZE);
  printf("NUM_USABLE_BLOCKS = %d\n", NUM_USABLE_BLOCKS);
  printf("NUM_BLOCKS = %d\n", NUM_BLOCKS);
  printf("FIRST_USABLE_BLOCK = %d\n", FIRST_USABLE_BLOCK);

  init_free_lists();

/*   for (int i = 0; i <= 1; i++) { */
/*     probe(b[i], "%p"); */
/*     probe(b[i]->blocks, "%d"); */
/*     probe(b[i]->start, "%p"); */
/*   } */
/*   printf("%d\n", ((word)(b[1])-(word)(b[0]))/BLOCKINFO_SIZE); */
  //  probe(TO_MEGABLOCK(b[0]), "%p");
  //  probe(FIRST_BLOCKINFO(b[0]), "%p");
  //  free_group(b[1]);
  //  free_group(b[0]);
  debug_print_free_block_list();
  debug_print_free_megablock_list();
  
/*   void* mem = alloc_megablocks(1); */
/*   for (int i = 0; i < 1*MEGABLOCK_SIZE; i++) { */
/*     *((uint8_t*)mem + i) = 22; */
/*   } */

/*   for (int i = 5; i < 15; i++) { */
/*     printf("%d, %ld\n", i, get_blockinfo((void*)(0x1000*i + 1024*1024))); */
/*   } */

/*   printf("%d\n", BLOCKS_TO_MEGABLOCKS(NUM_USABLE_BLOCKS+NUM_BLOCKS*1+1)); */
/*   printf("alloc_megagroup(5) = %p\n", alloc_megagroup(5)); */

/*   for (int i = 1; i < 7; i++) { */
/*     probe(i, "%d"); */
/*     Blockinfo_t *bg = alloc_megagroup(i); */
/*     assert(MEGABLOCKS_TO_BLOCKS(i) == bg->blocks, "mb->b"); */
/*     assert(i == BLOCKS_TO_MEGABLOCKS(bg->blocks), "b->mb"); */
/*     free_megagroup(bg); */
/*     print_free_megablock_list(); */
/*   } */


/*   printf("No frees:\n"); */
/*   print_free_megablock_list(); */
/*   Blockinfo_t *bg = alloc_megagroup(1); */
/*   print_free_megablock_list(); */
/*   free_megagroup(bg); */
/*   printf("One free:\n"); */
/*   print_free_megablock_list(); */
/*   free_megagroup(alloc_megagroup(1)); */
/*   printf("Two frees, alternating allocs:\n"); */
/*   print_free_megablock_list(); */
/*   Blockinfo_t *bg1 = alloc_megagroup(1), *bg2 = alloc_megagroup(1); */
/*   free_megagroup(bg1); */
/*   free_megagroup(bg2); */
/*   printf("Two frees after two allocs:\n"); */
/*   print_free_megablock_list(); */
/*   alloc_megagroup(2); */
/*   printf("Alloced 2-megagroup\n"); */
/*   print_free_megablock_list(); */

/*   for (int i = 1; i < 40; i++) { */
/*     printf("log2_ceil(%d) = %d\tlog2_floor(%d) = %d\n", i, log2_ceil(i), i, log2_floor(i)); */
/*   } */

  return 0;
}
