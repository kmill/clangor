/* Copyright 2013 Kyle Miller
 * test.h
 * support functions for a basic test framework.
 */

#ifndef clangor_test_h
#define clangor_test_h

// This provides the empty macros TEST_SUCCEEDS and TEST_FAILS.  The
// way these are intended to be used is like
//
// void TEST_SUCCEEDS test_foo(void) { ... }
//
// The script make_test.sh looks for these TEST_* markers and creates
// a program which can evaluate the tests using a command-line
// argument.
//
// An expected output to stdout may be given by giving comments like
// //> Expected output
// before a test definition.  Be careful! All such lines between tests
// are concatenated!
//
// See test_test.c for examples.

#ifndef DEBUG
# define DEBUG
#endif
#include "util.h"

#define TEST_SUCCEEDS
#define TEST_FAILS

typedef void (*Tester)(void);

#endif
