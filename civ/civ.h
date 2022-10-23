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

typedef uint8_t              U1;
typedef uint16_t             U2;
typedef uint32_t             U4;
typedef uint64_t             U8;
typedef size_t               Ref;

typedef int8_t               I1;
typedef int16_t              I2;
typedef int32_t              I4;
typedef int64_t              I8;
#if RSIZE == 4
typedef I4                   IRef;
#else
typedef I8                   IRef;
#endif

typedef struct { U1* dat; U2 len;                 } Slc;
typedef struct { U1* dat; U2 len; U2 cap;         } Buf;
typedef struct { U1* dat; U2 len; U2 cap; U2 plc; } PlcBuf;

Slc* Buf_asSlc(Buf*);
Slc* PlcBuf_asSlc(PlcBuf*);
Buf* PlcBuf_asBuf(PlcBuf*);

Slc Slc_from(U1* s);
void Buf_copy(Buf* b, U1* s);

U4  minU4(U4 a, U4 b);
Ref minRef(Ref a, Ref b);

U4  maxU4(U4 a, U4 b);
Ref maxRef(Ref a, Ref b);

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
  assert(E); civ.fb->err = E; \
  longjmp(*civ.fb->errJmp, 1); }
#define ASM_ASSERT(C, E)   if(!(C)) { SET_ERR(E); }
#define ASSERT_NO_ERR()    assert(!civ.fb->err)
#define ASSERT_EQ(E, CODE) if(1) { \
  typeof(E) __result = CODE; \
  if((E) != __result) eprintf("!!! Assertion failed: 0x%X == 0x%X\n", E, __result); \
  assert((E) == __result); }


// #################################
// # Methods and Roles

// Method Execute: Xm(myTy, meth, a, b) -> MyTy_meth(myTy, a, b)
#define Xm(D, M, ...)    typeof(*D) ## _ ## M(D __VA_OPT__(,) __VA_ARGS__)

// Role Execute: Xr(myRole, meth, a, b) -> myRole.m.meth(myRole.d, a, b)
#define Xr(R, M, ...)    (R).m.M(&(R).d __VA_OPT__(,) __VA_ARGS__)

// Declare role method:
//   Role_METHOD(myFunc, U1, U2) -> void (*)(void*, U1, U2) myFunc
#define Role_METHOD(M, ...)  ((void (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)

// #################################
// # BA: Block Allocator
#define BLOCK_PO2  12
#define BLOCK_SIZE (1<<BLOCK_PO2)
#define BLOCK_END  0xFF
typedef struct { U1 dat[BLOCK_SIZE];                             } Block;
typedef struct { U1 previ; U1 nexti;                             } BANode;
typedef struct { BANode* nodes; Block* blocks; U1 rooti; U1 cap; } BA;
typedef struct { BA* ba; U1 rooti; U2 len; U2 cap;               } BBA;

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
  void (*drop)            (void* d);
  void (*alloc)           (void* d, Ref size);
  void (*allocUnaligned)  (void* d, Ref size);
  void (*free)            (void* d, Ref size);
} M_Arena;


// #################################
// # BBA: Block Bump Arena
// For storing code and dictionary entries which reference code, fngi uses a
// block bump arena. This "bumps" memory from the top (for aligned) or bottom of
// a 4k block, but does not allow freeing it. However, the entire arena can be
// dropped to recover all the memory without fragmentation.

BBA BBA_new(BA* ba);

// Allocate "aligned" data from the top of the block.
//
// WARNING: It is the caller's job to ensure that size is suitably alligned to
// their system width.
U1* BBA_alloc(BBA* bba, Ref size);

// Allocate "unaligned" data from the bottom of the block.
U1* BBA_allocUnaligned(BBA* bba, Ref size);

void BBA_drop(BBA* bba);


// #################################
// # Civ Global Environment

typedef struct _Fiber {
  struct _Fiber* next;
  struct _Fiber* prev;
  jmp_buf*   errJmp;
  U2 err;
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
// # File
typedef struct {
  void (*open)  (void* d, Slc path);
  void (*close) (void* d);
  void (*stop)  (void* d);
  void (*seek)  (void* d, long int offset, U1 whence); // 1=SET, 2=CUR, 3=END
  void (*clear) (void* d);
  void (*read)  (void* d);
  void (*insert)(void* d);
} M_File;

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
