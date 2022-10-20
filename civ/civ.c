#include <string.h>
#include <assert.h>

#include "./civ.h"

U4 minU4(U4 a, U4 b) { if(a < b) return a; return b; }
U4 maxU4(U4 a, U4 b) { if(a < b) return b; return a; }

DEFINE_AS(CSlc, CBuf);     // CSlc asCSlc(CBuf)
DEFINE_AS(CBuf, CPlcBuf);  // CBuf asCBuf(CPlcBuf)

CSlc CSlc_from(U1* s) { return (CSlc) { .dat = s, .len = strlen(s) }; }

void CBuf_copy(CBuf* b, U1* s) {
  b->len = strlen(s); assert(b->cap >= b->len); memcpy(b->dat, s, b->len);
}
