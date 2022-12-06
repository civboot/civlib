// civc header file.
//
// version: 0.0.2

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


#if UINTPTR_MAX == 0xFFFFFFFF
#define RSIZE   4
#else
#define RSIZE   8
#endif

#define ALIGN1      1
#define ALIGN_SLOT  RSIZE

#define not  !
#define and  &&
#define or   ||
#define msk  &
#define jn   |

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
typedef size_t               Slot;

typedef int8_t               I1;
typedef int16_t              I2;
typedef int32_t              I4;
typedef int64_t              I8;
#if RSIZE == 4
typedef I4                   ISlot;
#else
typedef I8                   ISlot;
#endif

extern const U1* emptyNt; // empty null-terminated string

// ####
// # Core Structs
typedef struct { U1*   dat;   U2 len;                    } Slc;
typedef struct { U1*   dat;   U2 len;  U2 cap;           } Buf;
typedef struct { U1*   dat;   U2 len;  U2 cap; U2 plc;   } PlcBuf;
typedef struct { Slot* dat;   U2 sp;   U2 cap;           } Stk;
typedef struct { U1    count; U1 dat[];                  } CStr;
typedef struct { U1*   dat;   U2 head; U2 tail; U2 _cap; } Ring;

typedef struct _Sll {
  struct _Sll* next;
  void* dat;
} Sll;

typedef struct _Dll {
  struct _Dll* next;
  struct _Dll* prev;
  void* dat;
} Dll;

typedef struct { Dll* start; } DllRoot;

// ####
// # Core functions

void defaultErrPrinter();

// Get the required addition/subtraction to ptr to achieve alignment
Slot align(Slot ptr, U2 alignment);

// Clear bits in a mask.
#define bitClr(V, MASK)      ((V) & (~(MASK)))
#define bitSet(V, SET, MASK) (bitClr(V, MASK) | (SET))

// ##
// # Big Endian (unaligned) Fetch/Store
// Big endian store the largest values first. They are frequently used for
// networking code or where bytes are packed together without being aligned.
//
// The naming is confusing, since "big endian" makes you think the "big" value
// would be at the "end". This is not the case. The name was originally derived
// from Gulliver's travels where the "Liliputians", aka the Little
// Putians(Endians), break the little part of the egg on their "head". For an
// array, the start is also called the head. The Liliputians went to war with
// their neighbors (who broke the big side of the egg on their head) due to the
// difference of this custom.  Similarily, hardware architects at the dawn of
// computing were going to war over big/little endianness.
U4   ftBE(U1* p, Slot size);
void srBE(U1* p, Slot size, U4 value);

// ##
// # min/max
#define MIN_DEF { if(a < b) return a; return b; }
static inline U4   U4_min (U4  a, U4  b) MIN_DEF
static inline Slot Slot_min(Slot a, Slot b) MIN_DEF

#define MAX_DEF { if(a < b) return a; return b; }
static inline U4   U4_max (U4  a, U4  b) MAX_DEF
static inline Slot Slot_max(Slot a, Slot b) MAX_DEF

// ##
// # div
static inline U4 U4_ceil(U4 a, U4 b) { return (a / b) + (a % b != 0); }

// #################################
// # Slc: data slice of up to 64KiB indexes (0x10,000)
// Slice is just a data pointer and a len.
Slc  Slc_frNt(U1* s); // from null-terminated str
Slc  Slc_frCStr(CStr* c);
I4   Slc_cmp(Slc a, Slc b);

#define _ntLit(STR)        .dat = STR, .len = sizeof(STR) - 1
#define Slc_ntLit(STR)     ((Slc){ _ntLit(STR) })
#define Buf_ntLit(STR)     ((PlcBuf) { _ntLit(STR), .cap = sizeof(STR) - 1 })
#define PlcBuf_ntLit(STR)  ((PlcBuf) { _ntLit(STR), .cap = sizeof(STR) - 1 })
#define Slc_lit(...)  (Slc){           \
    .dat = (U1[]){__VA_ARGS__},        \
    .len = sizeof((U1[]){__VA_ARGS__}) \
  }

// Use this with printf using the '%.*s' format, like so:
//
//   printf("Printing my slice: %.*s\n", Dat_printf(slc));
//
// This is safe to use with CStr, Buf and PlcBuf.
#define Dat_printf(DAT)   (DAT).len, (DAT).dat

// #################################
// # Buf + PlcBuf: buffers of up to 64KiB indexes (0x10,000)
// Buffers are a data pointer, a length (used data) and a capacity
// PlcBuf has an additional field "plc" to keep place while processing a buffer.
Slc* Buf_asSlc(Buf*);
Slc* PlcBuf_asSlc(PlcBuf*);
Buf* PlcBuf_asBuf(PlcBuf*);

// Attempt to extend Buf. Return true if there is not enough space.
bool Buf_extend(Buf* b, Slc s);
bool Buf_extendNt(Buf* b, U1* s);

// #################################
// # Stk: efficient first-in last-out buffer.
// Stacks "grow down" so that indexes can be accessed using positive offsets.

// Initialize the stack. The cap must be the total number of Slots
// (NOT the size in bytes)
#define Stk_init(DAT, CAP) (Stk) (Stk) {.dat = DAT, .sp = CAP, .cap = CAP}
#define Stk_clear(STK)     ((STK).sp = (STK).cap)

// Get the number of slots in use.
#define Stk_len(STK)             ((STK).cap - (STK).sp)

Slot Stk_pop(Stk* stk); // pop a value from the stack, reducing it's len
void Stk_add(Stk* stk, Slot value); // add a value to the stack

// #################################
// # Ring: a lock-free ring buffer.
// Data is written to the tail and read from the head.
#define Ring_init(dat, datLen)   (Ring){.dat = dat, ._cap = datLen}
#define Ring_drop(RING, ARENA)   Xr(ARENA, free, (RING)->dat, (RING)->_cap, 1)

#define Ring_isEmpty(R)     ((R)->tail     == (R)->head)
#define Ring_isFull(R)      ((R)->tail + 1 == (R)->head)

U2   Ring_len(Ring* r);

// The capacity of the Ring is one less than the buffer capacity.
#define Ring_cap(RING)    ((RING)->_cap - 1)
#define Ring_remain(RING) (Ring_cap(RING) - Ring_len(RING))

static inline void Ring_clear(Ring* r) { r->head = 0; r->tail = 0; }

// Get the current character and advance the head.
// Returns NULL if empty.
// Typically this is used like:  while(( c = Ring_next(r) )) { ... }
U1*  Ring_next(Ring* r);

// Warning: Panics if length not sufficient
U1     Ring_pop(Ring* r); 
void   Ring_push(Ring* r, U1 c);
void   Ring_extend(Ring* r, Slc s);

// This API is for:
// 1. Get an available contiguous slice of memory and store some data in it.
//    Note: Ring_avail MAY NOT be the entire amount of available memory. It is
//    only the contiguous memory. To fill the Ring you may need to call this
//    twice.
// 2. Record how much was used with incTail.
Slc  Ring_avail(Ring* r);
void Ring_incTail(Ring* r, U2 inc);


// This API is for:
// 1. Get the first "chunk" of data and use some amount of it.
// 2. incHead by the amount used.
Slc Ring_first(Ring* r);
Slc Ring_second(Ring* r);
void Ring_incHead(Ring* r, U2 inc);

I4  Ring_cmpSlc(Ring* r, Slc s);


// Remove dat[:plc], shifting data[plc:len] to the left.
//
// This is extremely useful when reading files: a few bytes (i.e. a word, a
// line) can be processed at a time, then File_read called again.
void PlcBuf_shift(PlcBuf*);

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
// # Sll: Singly Linked List
void Sll_add(Sll** root, Sll* node);
Sll* Sll_pop(Sll** root);

// #################################
// # Dll: Doubly Linked List

// Add to next: to -> a ==> to -> b -> a
void Dll_add(Dll* to, Dll* node);

// Pop from next: from -> b -> a ==> from -> a (return b)
//
// Note: from->prev is never used, so the following is safe:
//
//   Dll* root = ...;
//   Dll* b = Dll_pop((Dll*)&root);
Dll* Dll_pop(Dll* from);

// Remove from chain: a -> node -> b ==> a -> b
Dll* Dll_remove(Dll* node);

// Add to root, preserving prev/next nodes.
//
//     root              root
//       v      ==>        v
// a <-> b           a <-> node <-> b
void DllRoot_add(DllRoot* root, Dll* node);

Dll* DllRoot_pop(DllRoot* root);

// #################################
// # Binary Search Tree
typedef struct _Bst {
  struct _Bst* l; struct _Bst* r;
  CStr* key;
} Bst;

I4   Bst_find(Bst** node, Slc slc);
Bst* Bst_add(Bst** root, Bst* add);

// #################################
// # Error Handling and Testing

// Compiler state to disable error logs when expecting an error.
#define TEST(NAME) \
  void test_ ## NAME () {                  \
    jmp_buf localErrJmp;                   \
    Civ_init();                            \
    civ.fb->errJmp = &localErrJmp;         \
    eprintf("## Testing " #NAME "...\n");  \
    if(setjmp(localErrJmp)) {              \
      if(civ.errPrinter) civ.errPrinter(); \
      else defaultErrPrinter();            \
      exit(1);                             \
    }
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
#define TASSERT_STK(EXPECT, STK)  TASSERT_EQ(EXPECT, Stk_pop(STK))

// Macro expansion shenanigans. Note that a plain foo ## __LINE__ expands to the
// literal string "foo__LINE__", when you wanted "foo362" (when line=362)
#define _JOIN(A, B) A ## B
#define JOIN(A, B) _JOIN(A, B)
#define LINED(A)    JOIN(A, __LINE__)

// Execute CODE. HANDLE will be executed if an error longjmp occurs.
#define HANDLE_ERR(CODE, HANDLE) \
  jmp_buf* LINED(prevJmp) = civ.fb->errJmp;                \
  jmp_buf LINED(newJmp); civ.fb->errJmp = &LINED(newJmp);  \
  if(setjmp(LINED(newJmp))) {             \
    civ.fb->errJmp = LINED(prevJmp);      \
    HANDLE;                               \
  } else { CODE; }

// Execute CODE and expect an error longjmp
#define EXPECT_ERR(CODE)                  \
  civ.fb->state |= Fiber_EXPECT_ERR;      \
  HANDLE_ERR(                             \
    { CODE; ASSERT(false, "expected error never happend"); } \
    , civ.fb->state &= ~Fiber_EXPECT_ERR)

// #################################
// # Methods and Roles
// C is a VERY annoying language when trying to improve the ergonomics of
// defining methods. There is no way (at compile time) to safely define a
// function as one with a different type at compile.
//
// The DECLARE/DEFINE METHOD macros below create both the MyType_method
// function and a M_MyType_method function reference which casts MyType_method
// with a "void* this" as the first parameter, so it can be used in the Role.
//
// In your .h file:
// - use DECLARE_METHOD(someReturnType, MyType,myMethod, arg1)
//   - Note: this also declares M_MyType_myMethod for you.
// - Define a MyType_mRole function which returns a method pointer for the
//   relevant Role.
// - declare a "Role MyType_asRole(MyType*)" function
//
// In your .c file:
// - use DEFINE_METHOD(someReturnType, MyType,myMethod, arg1)
//   - Note: this also defines M_MyType_myMethod for you.
// - use DEFINE_METHODS(Role, MyType_mRole, .myMethod=M_MyType_myMethod)
// - define the "Role MyType_asRole(MyType*)" function using the above method.
//
// For an example, see civ's implementation of the BBA methods.

#define DECLARE_METHOD(RETURNS, TYPE, NAME, ...)                          \
  extern RETURNS (*M_ ## TYPE ## _ ## NAME)(void* __VA_OPT__(,) __VA_ARGS__);    \
  RETURNS TYPE ## _ ## NAME(TYPE* this __VA_OPT__(,) __VA_ARGS__)

#define DEFINE_METHOD(RETURNS, TYPE, NAME, ...)                           \
  RETURNS (*M_ ## TYPE ## _ ## NAME)(void* __VA_OPT__(,) __VA_ARGS__) =   \
    ( RETURNS (*)(void* __VA_OPT__(,) __VA_ARGS__) ) TYPE ## _ ## NAME;   \
  RETURNS TYPE ## _ ## NAME(TYPE* this __VA_OPT__(,) __VA_ARGS__)

#define DEFINE_METHODS(RETURNS, NAME, ...) \
  RETURNS* NAME() { \
    static RETURNS m = {0}; \
    static bool notInit = true; \
    if(notInit) m = (RETURNS) { __VA_ARGS__ }; \
    return &m; \
  }

// Role Execute: Xr(myRole, meth, a, b) -> myRole.m.meth(myRole.d, a, b)
#define Xr(R, M, ...)    (R).m->M((R).d __VA_OPT__(,) __VA_ARGS__)

// Declare role method:
//   Role_METHOD(myFunc, U1, U2) -> void (*)(void*, U1, U2) myFunc
#define Role_METHOD(M, ...)      ((void (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)
#define Role_METHODR(M, R, ...)  ((R    (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)

// #################################
// # BA: Block Allocator
#define BLOCK_PO2  12
#define BLOCK_SIZE  (1<<BLOCK_PO2)
#define BLOCK_AVAIL (BLOCK_SIZE - (sizeof(U2) * 2))
#define BLOCK_END  0xFF

typedef struct { U2 bot; U2 top; } BlockInfo;

typedef struct {
  U1 dat[BLOCK_AVAIL];
  BlockInfo info;
} Block;

typedef struct _BANode {
  struct _BANode* next; struct _BANode* prev; // Dll
  Block* block;
} BANode;

typedef struct { BANode* free; Slot len; } BA;

Dll*     BANode_asDll(BANode* node);
DllRoot* BA_asDllRoot(BA* ba);

BANode* BA_alloc(BA* ba);
void BA_free(BA* ba, BANode* node);
void BA_freeAll(BA* ba, BANode* nodes);

// Free an array of nodes and blocks. Typically used to initialize BA.
void BA_freeArray(BA* ba, Slot len, BANode nodes[], Block blocks[]);

// #################################
// # Arena Role
typedef struct {
  void  (*drop)            (void* d);
  void* (*alloc)           (void* d, Slot sz, U2 alignment);
  Slc*  (*free)            (void* d, void* dat, Slot sz, U2 alignment);
  Slot  (*maxAlloc)        (void* d);
} MArena;


typedef struct { const MArena* m; void* d; } Arena;

// Methods that depend on arena
Buf    Buf_new(Arena arena, U2 cap); // Note: check that buf.dat != NULL
PlcBuf PlcBuf_new(Arena arena, U2 cap);

// #################################
// # Resource Role
typedef struct {
  // Perform resource cleanup and drop any non-POD data (i.e. buffers, etc)
  // This must NOT free the `d` pointer itself.
  //
  // Return true if drop is done. Returning false means it will be called again
  // after other resources are dropped.
  bool (*drop)            (void* d, Arena a);

  // Get the Sll view of the resource. This is used by Arenas to track resources
  // tied to them so they can be dropped.
  Sll* (*resourceLL)       (void* d);
} MResource;

typedef struct { const MResource* m; void* d; } Resource;
Resource* Arena_asResource(Arena*);

// #################################
// # BBA: Block Bump Arena
// This is a bump arena. Allocations "bump" the len (unaligned) or cap (aligned)
// indexes. Frees reverse the bump and MUST be done in reverse order (or skipped
// if you drop the whole arena). Free must use the exact same arguments as
// alloc.
//
// This allocator is great for data that just grows and is rarely freed or the
// free-order is very controlled. This is probably the fastest possible
// arbitrary-sized allocator.
//
// Fngi uses this allocator for "growing" the code heap. Therefore,
// unaligned allocations always grow from top to bottom.

typedef struct { BA* ba; BANode* dat; } BBA;

DllRoot* BBA_asDllRoot(BBA* bba);
Arena    BBA_asArena(BBA* b);
#define  BBA_block(BBA) ((BBA)->dat->block)
#define  BBA_info(BBA)  (BBA_block(BBA)->info)

DECLARE_METHOD(void, BBA,drop);   // BBA_drop
DECLARE_METHOD(Slot , BBA,spare); // BBA_spare
DECLARE_METHOD(void*, BBA,alloc, Slot sz, U2 alignment); // BBA_alloc

// BBA_free: return any error slc.
DECLARE_METHOD(Slc* , BBA,free , void* data, Slot sz, U2 alignment); // BBA_free
DECLARE_METHOD(Slot , BBA,maxAlloc); // BBA_maxAlloc

MArena* mBBAGet();


// #################################
// # Civ Global Environment

// fiberState bitfield
#define Fiber_EXPECT_ERR   (0x80 /*disable error logging*/)

typedef struct _Fiber {
  struct _Fiber* next;
  struct _Fiber* prev;
  jmp_buf*   errJmp;
  Arena*     arena;     // Global default arena
  Slc err;
  U2 state;
} Fiber;

typedef struct {
  BA         ba;    // root block allocator
  Fiber*     fb;    // currently executing fiber

  // Misc (normally not set/read)
  Fiber rootFiber;
  void (*errPrinter)();
} Civ;

extern Civ civ;

void Civ_init();

// #################################
// # File
// Unlike many roles, the File role requires the data structure to follow the
// below. This is because interacting with files are inherently interacting with
// buffers.
//
// System-specific data can expand on this, such as storing the file name/etc
// as a child-class of File.
//
// The file uses a PlcBuf. Typically the user will ingest some number of bytes,
// moving buf.plc and then calling PlcBuf_shift to clear data and read more
// bytes from the file.

#define File_seek_SET  1 // seek from beginning
#define File_seek_CUR  2 // seek from current position
#define File_seek_END  3 // seek from end

typedef struct {
  Sll*      nextResource; // resource SLL
  Ring      ring;         // buffer for reading or writing data
  U2        code;         // status or error (File_*)
} BaseFile;

typedef struct {
  // Resource methods
  void (*drop)            (void* d, Arena a);
  Sll* (*resourceLL)      (void* d);

  // Get the base file pointer.
  BaseFile* (*asBase) (void* d);

  // Close a file
  void (*close) (void* d);

  // Open a file. Platform must define File_(RDWR|RDONLY|WRONLY|TRUNC)
  void (*open)  (void* d, Slc path, Slot options);

  // Stop async operations (may be noop)
  void (*stop)  (void* d);

  // Seek in the file whence=File_seek_(SET|CUR|END)
  void (*seek)  (void* d, ISlot offset, U1 whence);

  // Read from a file into d buffer.
  void (*read)  (void* d);

  // Write to a file from d buffer.
  void (*write)(void* d);
} MFile;

typedef struct { const MFile* m; void* d; } File;  // Role
Resource* File_asResource(File*);

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
