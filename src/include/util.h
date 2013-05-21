/* Copyright 2013 Kyle Miller
 * util.h
 * Utility functions
 */

#ifndef clangor_util_h
#define clangor_util_h

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>

#ifdef DEBUG
#define debug(...) {                             \
    printf("DEBUG %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__);                         \
    printf("\n");                                \
  }
#define probe(v, s) {                                     \
    printf("PROBE %s:%d: %s = ", __FILE__, __LINE__, #v); \
    printf(s, v);                                         \
    printf("\n");                                         \
  }
#define assert(test, expectation) {                                     \
    if (!(test)) {                                                      \
      error("Assertion error: %s is not true. %s", #test, expectation); \
    }                                                                   \
  }
#else
#define debug(...) {}
#define probe(v, s) {}
#define assert(test, ...) {}
#endif

// Halts the program, printing an error message.
#define error(...) {                                      \
    if (errno) { perror("ERROR"); }                       \
    fprintf(stderr, "ERROR %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                         \
    fprintf(stderr, "\n");                                \
    exit(22);                                             \
  }

// Next power of two.  This should only be used in the pre-processor!
#define _npo2_b2(x) ((x) | ((x) >> 1))
#define _npo2_b4(x) (_npo2_b2(x) | (_npo2_b2(x) >> 2))
#define _npo2_b8(x) (_npo2_b4(x) | (_npo2_b4(x) >> 4))
#define _npo2_b16(x) (_npo2_b8(x) | (_npo2_b8(x) >> 8))
#define _npo2_b32(x) (_npo2_b16(x) | (_npo2_b16(x) >> 16))
#define next_power_of_2(x) (_npo2_b32((x)-1) + 1)

typedef uintptr_t word;

#endif
