#ifndef __CIV_H
#define __CIV_H
#include <stdint.h>

#if UINTPTR_MAX == 0xFFFF
#define RSIZE   4
#else
#define RSIZE   8
#endif


// TO asTO(FROM);
#define DECLARE_AS(TO, FROM)  TO* as##TO(FROM* f)
#define DEFINE_AS(TO, FROM) \
   DECLARE_AS(TO, FROM) { return (TO*) f; }

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

typedef struct { U1* dat; U2 len;                 } CSlc;
typedef struct { U1* dat; U2 len; U2 cap;         } CBuf;
typedef struct { U1* dat; U2 len; U2 cap; U2 plc; } CPlcBuf;

DECLARE_AS(CSlc, CBuf);     // CSlc asCSlc(CBuf)
DECLARE_AS(CBuf, CPlcBuf);  // CBuf asCBuf(CPlcBuf)

CSlc CSlc_from(U1* s);
void CBuf_copy(CBuf* b, U1* s);

U4 minU4(U4 a, U4 b);
U4 maxU4(U4 a, U4 b);

// #################################
// # Roles

// Role Execute. Expands:
//   REX(Role, method, arg1, arg2)
// to
//   Role.m.method(Role.d, arg1, arg2)
#define REX(ROLE, METHOD, ...) \
  (ROLE).m.METHOD(&(ROLE).d __VA_OPT__(,) __VA_ARGS__)

// For declaring role methods. This Expands:
//   Role_METHOD(myFunc, U1, U2)
// to
//   void (*)(void*, U1, U2) myFunc
#define Role_METHOD(M, ...)  ((void (*)(void* __VA_OPT__(,) __VA_ARGS__)) M)




// #################################
// # File Role

// File Methods
typedef struct {
  void (*open)  (void* d, CSlc path);
  void (*close) (void* d);
  void (*stop)  (void* d);
  void (*seek)  (void* d, long int offset, U1 whence); // 1=SET, 2=CUR, 3=END
  void (*clear) (void* d);
  void (*read)  (void* d);
  void (*insert)(void* d);
} M_File;

typedef struct {
  Ref      pos;   // current position in file. If seek: desired position.
  Ref      fid;   // file id or reference
  CPlcBuf  buf;   // buffer for reading or writing data
  U2       code;  // status or error (File_*)
} File;

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
