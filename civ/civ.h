#ifndef __CIV_H
#define __CIV_H

// Types and functions
#include <stdbool.h> // bool
#include <stdlib.h>  // size_t, NULL, malloc, exit, etc.
#include <stdint.h>  // usize16_t, etc
#include <string.h>  // (mem|str)(cmp|cpy|move|set), strlen
#include <setjmp.h>  // setjmp

// Testing and debuggin
#include <assert.h>  // assert
#include <stdio.h>   // printf
#include <errno.h>   // errno global

#include "constants.h"

#if UINTPTR_MAX == 0xFFFF
#define RSIZE   4
#else
#define RSIZE   8
#endif

#define not  !
#define and  &&
#define msk  &
#define or   ||
#define jn   |
#define R0   return 0;
#define RV   return;

#define eprintf(F, ...)   fprintf(stderr, F __VA_OPT__(,) __VA_ARGS__)

// TO asTO(FROM);
#define DEFINE_AS(FROM, TO) \
  TO* FROM ## _ ## as ## TO(FROM* f) { return (TO*) f; }

// #################################
// # Core Types and common methods

// Core Types
typedef uint8_t              U1;
typedef uint16_t             U2;
typedef uint32_t             U4;
typedef uint64_t             U8;
typedef size_t               Ref;
typedef Ref                  Slot;

typedef int8_t               I1;
typedef int16_t              I2;
typedef int32_t              I4;
typedef int64_t              I8;
#if RSIZE == 4
typedef I4                   IRef;
#else
typedef I8                   IRef;
#endif
typedef IRef                 ISlot;

// ####
// # Core Structs
typedef struct { U1* dat; U2 len;                 } Slc;
typedef struct { U1* dat; U2 len; U2 cap;         } Buf;
typedef struct { U1* dat; U2 len; U2 cap; U2 plc; } PlcBuf;
typedef struct { U1 count; U1 dat[];              } CStr;

// ####
// # Core methods

// ##
// # Big Endian (unaligned) Fetch/Store
U4   ftBE(U1* p, Slot size);
void srBE(U1* p, Slot size, U4 value);

// ##
// # min/max
U4  U4_min(U4 a, U4 b);
Ref Ref_min(Ref a, Ref b);
U4  U4_max(U4 a, U4 b);
Ref Ref_max(Ref a, Ref b);

// ##
// # Slc
Slc  Slc_frNt(U1* s); // from null-terminated str
Slc  Slc_frCStr(CStr* c);
I4   Slc_cmp(Slc a, Slc b);
#define Slc_ntLit(STR)  ((Slc){.dat = STR, .len = sizeof(STR) - 1})
#define Slc_lit(...)  (Slc){           \
    .dat = (U1[]){__VA_ARGS__},        \
    .len = sizeof((U1[]){__VA_ARGS__}) \
  }

// ##
// # Buf + PlcBuf
Slc* Buf_asSlc(Buf*);
Slc* PlcBuf_asSlc(PlcBuf*);
Buf* PlcBuf_asBuf(PlcBuf*);
void Buf_ntCopy(Buf* b, U1* s);

// CStr
bool CStr_varAssert(U4 line, U1* STR, U1* LEN);

// Declare a CStr global. It's your job to assert that the LEN is valid.
#define CStr_ntLitUnchecked(NAME, LEN, STR) \
  char _CStr_ ## NAME[1 + sizeof(STR)] = LEN STR; \
  CStr* NAME = (CStr*) _CStr_ ## NAME;

// Declare a CStr variable with auto-assert.
// The assert runs every time the function is called (not performant),
// but this is fine for one-off functions or tests.
#define CStr_ntVar(NAME, LEN, STR)      \
  static CStr_ntLitUnchecked(NAME, LEN, STR); \
  assert(CStr_varAssert(__LINE__, STR, LEN));



// #################################
// # Error Handling and Testing

#define TEST(NAME) \
  void test_ ## NAME () {              \
    jmp_buf localErrJmp, expectErrJmp; \
    civ.fb = &civ.rootFiber;           \
    civ.fb->errJmp = &localErrJmp;     \
    civ.fb->errJmp = &localErrJmp; \
    eprintf("## Testing " #NAME "...\n"); \
    if(setjmp(localErrJmp)) { civ.civErrPrinter(); exit(1); }
#define END_TEST  }

#define SET_ERR(E)  if(true) { \
  civ.fb->err = E; \
  longjmp(*civ.fb->errJmp, 1); }
#define ASSERT(C, E)   if(!(C)) { SET_ERR(Slc_ntLit(E)); }
#define ASSERT_NO_ERR()    assert(!civ.fb->err)
#define TASSERT_EQ(EXPECT, CODE) if(1) { \
  typeof(EXPECT) __result = CODE; \
  if((EXPECT) != __result) eprintf("!!! Assertion failed: 0x%X == 0x%X\n", EXPECT, __result); \
  assert((EXPECT) == __result); }


// #################################
// # Methods and Roles

// Role Execute: Xr(myRole, meth, a, b) -> myRole.m.meth(myRole.d, a, b)
#define Xr(R, M, ...)    (R).m.M(&(R).d __VA_OPT__(,) __VA_ARGS__)

// Declare role method:
//   Role_METHOD(myFunc, U1, U2) -> void (*)(void*, U1, U2) myFunc
#define Role_METHOD(M, ...)      ((void (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)
#define Role_METHODR(M, R, ...)  ((R    (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)

// #################################
// # BA: Block Allocator
#define BLOCK_PO2  12
#define BLOCK_SIZE (1<<BLOCK_PO2)
#define BLOCK_END  0xFF
typedef struct { U1 dat[BLOCK_SIZE];                             } Block;
typedef struct { U1 previ; U1 nexti;                             } BANode;
typedef struct { BANode* nodes; Block* blocks; U1 rooti; U1 cap; } BA;

typedef struct {
  BA* ba;
  U1 rooti;
  U2 len; U2 cap;
} BBA;

// Initialize a BA
void BA_init(BA* ba);

// Allocate a block, updating BlockAllocator and client's root indexes.
//
// Go from:
//   baRoot     -> d -> e -> f
//   clientRoot -> c -> b -> a
// To (returning block 'd'):
//   baRoot     -> e -> f
//   clientRoot -> d -> c -> b -> a
Block* BA_alloc(BA* ba, U1* clientRooti);

// Free a block, updating BlockAllocator and client's root indexes.
//
// Go from (freeing c):
//   clientRoot -> c -> b -> a
//   baRoot     -> d -> e -> f
// To:
//   clientRoot -> b -> a
//   baRoot     -> c -> d -> e -> f
void BA_free(BA* ba, uint8_t* clientRooti, Block* b);

// Free all blocks owned by the client.
//
// Go from:
//   clientRoot -> c -> b -> a
//   baRoot     -> d -> e -> f
// To:
//   clientRoot -> END
//   baRoot     -> a -> b -> c -> d -> e -> f
void BA_freeAll(BA* ba, U1* clientRooti);

// #################################
// # Arena Role
typedef struct {
  void (*drop)            (void* d);          // drop whole arena
  void (*alloc)           (void* d, Slot sz); // allocate memory of size
  void (*allocUnaligned)  (void* d, Slot sz); // ... possibly unaligned.
  void (*free)            (void* d, void* mem, Slot sz); // free memory of size
} MArena;

typedef struct { MArena m; void* d; } Arena;

// #################################
// # Resource Role
typedef struct {
  // Perform resource cleanup and drop any non-POD data (i.e. buffers, etc)
  // This must NOT free the `d` pointer itself.
  //
  // Return true if drop is done. Returning false means it will be called again
  // (although other resources may be called first).
  bool (*drop)            (void* d, Arena* a);
} MResource;

typedef struct { MResource m; void* d; } Resource;
Resource* Arena_asResource(Arena*);

// #################################
// # BBA: Block Bump Arena
// This is a bump arena. Allocations "bump" the len (unaligned) or cap (aligned)
// indexes, while frees are ignored.
//
// This is great for data that just grows and is rarely (or never) freed. It
// will cause OOM for other workloads unless they are small and the Arena is
// quickly dropped.
Arena BBA_asArena(BBA* b);

void BBA_drop(BBA* bba); // drop whole Arena
BBA BBA_new(BA* ba);

// Allocate "aligned" data from the top of the block.
//
// WARNING: It is the caller's job to ensure that size is suitably alligned to
// their system width.
U1* BBA_alloc(BBA* bba, Slot sz);

// Allocate "unaligned" data from the bottom of the block.
U1* BBA_allocUnaligned(BBA* bba, Slot sz);

// #################################
// # Civ Global Environment

typedef struct _Fiber {
  struct _Fiber* next;
  struct _Fiber* prev;
  jmp_buf*   errJmp;
  Arena*     arena;     // Global default arena
  Slc err;
} Fiber;

typedef struct {
  BA         ba;    // root block allocator
  Fiber*     fb;    // currently executing fiber

  // Misc (normally not set/read)
  Fiber rootFiber;
  void (*civErrPrinter)();
} Civ;

extern Civ civ;

// #################################
// # Binary Search Tree
typedef struct _Bst {
  struct _Bst* l; struct _Bst* r;
  CStr* key;
} Bst;

I4   Bst_find(Bst** node, Slc slc);
Bst* Bst_add(Bst** root, Bst* add);

// #################################
// # File
// Unlike many roles, the File role requires the data structure to follow the
// below. This is because interacting with files are inherently interacting with
// buffers.
//
// System-specific data can expand on this, such as storing the file name/etc.

#define File_seek_SET  1 // seek from beginning
#define File_seek_CUR  2 // seek from current position
#define File_seek_END  3 // seek from end

typedef struct {
  Ref      pos;   // current position in file. If seek: desired position.
  Ref      fid;   // file id or reference
  PlcBuf   buf;   // buffer for reading or writing data
  U2       code;  // status or error (File_*)
} File;

typedef struct {
  // Resource methods
  bool (*drop) (File* d);

  // Close a file
  void (*close) (File* d);

  // Open a file. Platform must define File_(RDWR|RDONLY|WRONLY|TRUNC)
  void (*open)  (File* d, Slc path, Slot options);

  // Stop async operations (may be noop)
  void (*stop)  (File* d);

  // Seek in the file whence=File_seek_(SET|CUR|END)
  void (*seek)  (File* d, ISlot offset, U1 whence);

  // Read from a file into d buffer.
  void (*read)  (File* d);

  // Write to a file from d buffer.
  void (*write)(File* d);
} MFile;

typedef struct { MFile* m; File* d; } RFile;  // Role
Resource* RFile_asResource(RFile*);

// If set it is a real "file index/id"
#define File_INDEX      ((Ref)1 << ((sizeof(Ref) * 8) - 1))

#define File_CLOSED   0x00

#define File_SEEKING  0x10
#define File_READING  0x11
#define File_WRITING  0x12
#define File_STOPPING 0x13

#define File_DONE     0xD0
#define File_STOPPED  0xD1
#define File_EOF      0xD2

#define File_ERROR    0xE0
#define File_EIO      0xE2

#endif // __CIV_H
