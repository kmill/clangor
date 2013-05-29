/* Copyright 2013 Kyle Miller
 * objects.h
 *
 * Definition of heap objects.
 */

#ifndef clangor_objects_h
#define clangor_objects_h

#include <stdint.h>
#include "util.h"

// A standard object
#define OBJ_TYPE_STD 1
// An array object
#define OBJ_TYPE_ARRAY 2

typedef struct ObjDef_s {
  // Since an objdef is an obj, too:
  struct ObjDef_s *def;
  word type;
  // The number of entries in the Obj (if it's not an array type).
  word length;
  // Bitmap of which entries in an object are Objs.  bitmap & (1 <<
  // i) is true iff payload.obj[i] is an Obj.  If type is
  // OBJ_TYPE_ARRAY, then bitmap != 0 iff it is made entirely of Objs.
  uint64_t bitmap;
  // Pointer to code to execute
} ObjDef_t;

typedef struct Obj_s {
  ObjDef_t *def;
	// Makes a linked list of remembered-set objects in a generation.  0
	// marks not being in the remembered set, (void *)-1 marks the end
	// of the linked list.
	struct Obj_s *link;
  union {
    struct {
      word length;
      struct Obj_s *data[0];
    } array;
    struct {
      struct Obj_s *data[0];
    } obj;
  } payload;
} Obj_t;

#endif
