#include <string.h>
#include <assert.h>

#include "./civ.h"

/*extern*/ jmp_buf* err_jmp = NULL;
/*extern*/ U2 civErr        = 0;
/*extern*/ Civ civ          = (Civ) {};

#define MIN_DEF { if(a < b) return a; return b; }
U4  minU4 (U4  a, U4  b) MIN_DEF
Ref minRef(Ref a, Ref b) MIN_DEF

#define MAX_DEF { if(a < b) return a; return b; }
U4  maxU4 (U4  a, U4  b) MAX_DEF
Ref maxRef(Ref a, Ref b) MAX_DEF

DEFINE_AS(Buf,    /*as*/Slc);
DEFINE_AS(PlcBuf, /*as*/Slc);
DEFINE_AS(PlcBuf, /*as*/Buf);

Slc Slc_from(U1* s) { return (Slc) { .dat = s, .len = strlen(s) }; }

void Buf_copy(Buf* b, U1* s) {
  b->len = strlen(s); assert(b->cap >= b->len); memcpy(b->dat, s, b->len);
}

// #################################
// # BA: Block Allocator

#define BA_index(BA, BLOCK)   (((Ref)(BLOCK) - (Ref)(BA).blocks) >> BLOCK_PO2)

void BA_init(BA* ba) {
  if(ba->cap == 0) return; ASM_ASSERT(ba->cap < BLOCK_END, 0x0C00);
  BANode* nodes = ba->nodes;
  ba->rooti = 0;
  U1 i, previ = BLOCK_END;
  for (i = 0; i < ba->cap; i += 1) {
    nodes[i].previ = previ; nodes[i].nexti = i + 1;
    previ = i;
  }
  nodes[i - 1].nexti = BLOCK_END;
}

Block* BA_alloc(BA* ba, U1* clientRooti) {
  uint8_t di = ba->rooti; // index of "d"
  if(di == BLOCK_END) return 0;

  BANode* nodes = ba->nodes;
  BANode* d = &nodes[di]; // node "d"
  ba->rooti = d->nexti;  // baRoot -> e
  if (d->nexti != BLOCK_END) nodes[d->nexti].previ = BLOCK_END; // baRoot <- e

  ASM_ASSERT(d->previ == BLOCK_END, 0x6039); // "d" is already root node
  d->nexti = *clientRooti; // d -> c
  if(*clientRooti != BLOCK_END) {
    nodes[*clientRooti].previ = di;  // d <- c
  }
  *clientRooti = di; // clientRooti -> d
  return &ba->blocks[di]; // return block 'd'
}

void BA_free(BA* ba, uint8_t* clientRooti, Block* b) {
  // Assert block is within blocks memory region
  ASM_ASSERT(    (b >= ba->blocks)
             and (b <= &ba->blocks[ba->cap + 1])
           , E_oob);
  uint8_t ci = BA_index(*ba, b);

  BANode* nodes = ba->nodes;
  BANode* c = &nodes[ci]; // node 'c'
  if(ci == *clientRooti) {
    ASM_ASSERT(c->previ == BLOCK_END, 0x6042);
    *clientRooti = c->nexti; // clientRoot -> b
    if(c->nexti != BLOCK_END) {
      nodes[c->nexti].previ = BLOCK_END; // clientRoot <- b
    }
  } else { // i.e. b -> c -> d  ===>  b -> d
    nodes[c->previ].nexti = c->nexti;
    nodes[c->nexti].previ = c->previ;
  }

  c->nexti               = ba->rooti; // c -> d
  nodes[ba->rooti].previ = ci;        // c <- d
  ba->rooti              = ci;        // baRoot -> c
  c->previ               = BLOCK_END; // baRoot <- c
}

void BA_freeAll(BA* ba, U1* clientRooti) {
  while(BLOCK_END != *clientRooti) {
    BA_free(ba, clientRooti, &ba->blocks[*clientRooti]);
  }
}

// #################################
// # BBA: Block Bump Arena


BBA BBA_new(BA* ba) { return (BBA) { .ba = ba, .rooti = BLOCK_END}; }

bool _BBA_reserveIfSmall(BBA* bba, U2 size) {
  if((bba->cap) < (bba->len) + size) {
    if(0 == BA_alloc(bba->ba, &bba->rooti)) return false;
    bba->len = 0;
    bba->cap = BLOCK_SIZE;
  }
  return true;
}

// Allocate "aligned" data from the top of the block.
//
// WARNING: It is the caller's job to ensure that size is suitably alligned to
// their system width.
U1* BBA_alloc(BBA* bba, Ref size) {
  if(!_BBA_reserveIfSmall(bba, size)) return 0;
  bba->cap -= size;
  U1* out = ((U1*)&bba->ba->blocks[bba->rooti]) + bba->cap;
  return out;
}

// Allocate "unaligned" data from the bottom of the block.
U1* BBA_allocUnaligned(BBA* bba, Ref size) {
  if(!_BBA_reserveIfSmall(bba, size)) return 0;
  U1* out = ((U1*)&bba->ba->blocks[bba->rooti]) + bba->len;
  bba->len += size;
  return out;
}

void BBA_drop(BBA* bba) {
  BA* ba = bba->ba;
  while(bba->rooti != BLOCK_END)
    BA_free(ba, &bba->rooti, &ba->blocks[bba->rooti]);
  assert(BLOCK_END == bba->rooti);
  bba->len = 0; bba->cap = 0;
}
