/* Copyright 2013 Kyle Miller
 * test_blocks.c
 * testing block-structured heap
 */

#include "test.h"
#include "blocks.h"
#include "util.h"
#include <stdint.h>

// Some sanity checks on the constants related to block sizes.
void TEST_SUCCEEDS test_constants(void) {
  assert(BLOCKINFO_SIZE == sizeof(struct Blockinfo_aligned_s),
         "Blockinfo aligned struct not right size.");
  assert(BLOCKINFO_SIZE >= sizeof(Blockinfo_t),
         "Aligned blockinfo is not big enough fo blockinfo.");
  assert(sizeof(Block_t) == BLOCK_SIZE,
         "Block size does not correspond to constant.");
  assert(NUM_BLOCKS*BLOCK_SIZE >= NUM_USABLE_BLOCKS * (BLOCK_SIZE + BLOCKINFO_SIZE),
         "Blockinfos overlap with blocks.");
  assert(FIRST_USABLE_BLOCK*BLOCK_SIZE >= NUM_USABLE_BLOCKS * BLOCKINFO_SIZE,
         "Blockinfos ovelap with blocks.");
}

// Tests various orders of allocation and deallocation of two block
// groups which make up a megablock.  Some of this is redundant.
void TEST_SUCCEEDS test_allocation_deallocation(void) {
  Blockinfo_t *b1, *b2;
  init_free_lists();
  verify_free_block_list();
  verify_free_megablock_list();
  debug("test allocating/deallocating one block with no megablocks in the megablock free list");
  b1 = alloc_group(1);
  verify_free_block_list();
  verify_free_megablock_list();
  free_group(b1);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty();
  debug("test allocating group with a megablock in the megablock free list");
  b1 = alloc_group(NUM_USABLE_BLOCKS - 1);
  verify_free_block_list();
  verify_free_megablock_list();
  debug("test allocating the rest of the free list");
  b2 = alloc_group(1);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty(); // should be empty now
  debug("test deallocating this memory in both orders.  Order 1:");
  free_group(b1);
  free_group(b2);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty();
  debug("Order 2:");
  b1 = alloc_group(1);
  b2 = alloc_group(NUM_USABLE_BLOCKS - 1);
  free_group(b2);
  free_group(b1);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty();
  debug("Now again in the opposite order of allocation. Order 1:");
  b2 = alloc_group(NUM_USABLE_BLOCKS - 1);
  b1 = alloc_group(1);
  assert_free_block_list_empty();
  free_group(b1);
  free_group(b2);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty();
  debug("Order 2:");
  b2 = alloc_group(NUM_USABLE_BLOCKS - 1);
  b1 = alloc_group(1);
  assert_free_block_list_empty();
  free_group(b2);
  free_group(b1);
  verify_free_block_list();
  verify_free_megablock_list();
  assert_free_block_list_empty();
}

// Deallocate a block twice.
void TEST_FAILS test_deallocating_deallocated(void) {
  init_free_lists();
  Blockinfo_t *b = alloc_group(1);
  free_group(b);
  free_group(b);
}

// Make sure assert_free_block_list_empty actually fails.
void TEST_FAILS test_not_deallocating(void) {
  init_free_lists();
  alloc_group(1); // should leave the remainder of a megablock around.
  assert_free_block_list_empty();
}

// Pseudorandomly allocate and deallocate some blocks.
void TEST_SUCCEEDS test_big_allocation_deallocation(void) {
  init_free_lists();
  Blockinfo_t *b[101];
  for (int i = 1; i < 101; i++) {
    int j = 1 + 3*(i-1) % 100;
    b[j] = alloc_group(j);
    assert(b[j]->blocks == j, "Group not the right size.");
    verify_free_block_list();
    verify_free_megablock_list();
  }
  for (int i = 1; i < 101; i++) {
    int j = 1 + 7*(i-1) % 100;
    free_group(b[j]);
    verify_free_block_list();
    verify_free_megablock_list();
  }
  assert_free_block_list_empty();
}

// Can we write to a block which is given to us?
void TEST_SUCCEEDS test_writing_to_block(void) {
  Blockinfo_t *b = alloc_group(5);
  assert(b->blocks == 5, "Block not the right size.");
  for (word i = 0; i < BLOCK_SIZE * b->blocks; i++) {
    *((uint8_t *)b->start + i) = 22;
  }
  for (word i = 0; i < BLOCK_SIZE * b->blocks; i++) {
    assert(*((uint8_t *)b->start + i) == 22, "Didn't set the memory.");
  }
  // in case we wrote over something
  verify_free_block_list();
  verify_free_megablock_list();
  free_group(b);
  verify_free_block_list();
  verify_free_megablock_list();
}

// Ruin data for blockinfo right before freeing
void TEST_FAILS test_writing_junk_into_blockinfo1(void) {
  Blockinfo_t *b = alloc_group(1);
  for (word i = 0; i < BLOCKINFO_SIZE; i++) {
    *((uint8_t *)b + i) = 22;
  }
  free_group(b);
}

// Ruin data for blockinfo right before freeing
void TEST_FAILS test_writing_junk_into_blockinfo2(void) {
  // First we need the allocator to allocate a megablock
  Blockinfo_t *b = alloc_group(1);
  free_group(b);
  uint8_t *mb = (void *)TO_MEGABLOCK(b);
  for (word i = 0; i < NUM_BLOCKS*BLOCKINFO_SIZE; i++) {
    *(mb + i) = 22;
  }
  alloc_group(1);
}

// Ruin data for free megablock, try verify_free_megablock_list
void TEST_FAILS test_writing_junk_into_blockinfo3(void) {
  // First we need the allocator to allocate a megablock
  Blockinfo_t *b = alloc_group(1);
  free_group(b);
  uint8_t *mb = (void *)TO_MEGABLOCK(b);
  for (word i = 0; i < NUM_BLOCKS*BLOCKINFO_SIZE; i++) {
    *(mb + i) = 22;
  }
  verify_free_megablock_list();
}
