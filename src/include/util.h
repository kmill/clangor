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

#define STRINGIFY(x) #x

#ifdef DEBUG
#define debug(...) do {                                     \
    printf("DEBUG %s:%d: ", __FILE__, __LINE__);            \
    printf(__VA_ARGS__);                                    \
    printf("\n");                                           \
    fflush(stdout);                                         \
  } while (0)
#define probe(v, s) do {                                            \
    printf("PROBE %s:%d: %s = ", __FILE__, __LINE__, #v);           \
    printf(s, v);                                                   \
    printf("\n");                                                   \
  } while (0)
#define assert(test, expectation) do {                                  \
    if (!(test)) {                                                      \
      error("Assertion error: %s is not true. %s", #test, expectation); \
    }                                                                   \
  } while (0)
#else
#define debug(...) do {} while(0)
#define probe(v, s) do {} while(0)
#define assert(test, ...) do {} while(0)
#endif

// Halts the program, printing an error message.
#define error(...) do {                                              \
    if (errno) { perror("ERROR"); }                                  \
    fprintf(stderr, "ERROR %s:%d: ", __FILE__, __LINE__);            \
    fprintf(stderr, __VA_ARGS__);                                    \
    fprintf(stderr, "\n");                                           \
    exit(22);                                                        \
  } while(0)

// Next power of two.  This should only be for computations in the
// pre-processor!
#define _npo2_b2(x) ((x) | ((x) >> 1))
#define _npo2_b4(x) (_npo2_b2(x) | (_npo2_b2(x) >> 2))
#define _npo2_b8(x) (_npo2_b4(x) | (_npo2_b4(x) >> 4))
#define _npo2_b16(x) (_npo2_b8(x) | (_npo2_b8(x) >> 8))
#define _npo2_b32(x) (_npo2_b16(x) | (_npo2_b16(x) >> 16))
#define next_power_of_2(x) (_npo2_b32((x)-1) + 1)

// Types

typedef uintptr_t   word;

#endif
