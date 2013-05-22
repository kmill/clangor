/* Copyright 2013 Kyle Miller
 * test_test.c
 * Testing the testing framework
 */

#include "test.h"
#include <stdio.h>

// Empty test
void TEST_SUCCEEDS test_empty(void) {
}

// Deliberate failure
void TEST_FAILS test_exit_22(void) {
  exit(22);
}

// Test match output
//> Hello, world!
void TEST_SUCCEEDS test_output(void) {
  printf("Hello, world!\n");
}
