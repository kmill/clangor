/* Copyright 2013 Kyle Miller
 * gc.h
 * Garbage collector/memory allocator
 */

#ifndef clangor_gc_h
#define clangor_gc_h

#ifndef MAP_ANONYMOUS
# define GC_MMAP_MAPPING MAP_ANON
#else
# define GC_MMAP_MAPPING MAP_ANONYMOUS
#endif

// Object definitions

typedef struct DataDef_s {
  
} DataDef_t;

typedef struct Obj_s {
  DataDef_t* def; // If NULL then it's a primitive array
  union {
    struct {
      long length;
      void* data[0];
    } array;
    struct {
      void* data[0];
    } obj;
  } data;
} Obj_t;

// Garbage collector definitions

typedef struct GC_s GC_t;

//GC_t* gc_new_manager(void);
//void* gc_alloc(GC_t* gc

#endif
