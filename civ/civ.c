#include <string.h>
#include <assert.h>

#include "./civ.h"

/*extern*/ jmp_buf* err_jmp = NULL;
/*extern*/ U2 civErr        = 0;
/*extern*/ Civ civ          = (Civ) {};

// ####
// # Core methods

// ##
// # Big Endian (unaligned) Fetch/Store
U4 ftBE(U1* p, Slot size) { // fetch Big Endian
  switch(size) {
    case 1: return *p;                  case 2: return (*p<<8) + *(p + 1);
    case 4: return (*p << 24) + (*(p + 1)<<16) + (*(p + 2)<<8) + *(p + 3);
    default: SET_ERR(Slc_ntLit("ftBE: invalid sz"));
  }
}

void srBE(U1* p, Slot size, U4 value) { // store Big Endian
  switch(size) {
    case 1: *p = value; break;
    case 2: *p = value>>8; *(p+1) = value; break;
    case 4: *p = value>>24; *(p+1) = value>>16; *(p+2) = value>>8; *(p+3) = value;
            break;
    default: SET_ERR(Slc_ntLit("srBE: invalid sz"));
  }
}

// ##
// # min/max
#define MIN_DEF { if(a < b) return a; return b; }
U4  U4_min (U4  a, U4  b) MIN_DEF
Ref Ref_min(Ref a, Ref b) MIN_DEF

#define MAX_DEF { if(a < b) return a; return b; }
U4  U4_max (U4  a, U4  b) MAX_DEF
Ref Ref_max(Ref a, Ref b) MAX_DEF

// ##
// # Slc
Slc Slc_frNt(U1* s)     { return (Slc) { .dat = s,      .len = strlen(s) }; }
Slc Slc_frCStr(CStr* c) { return (Slc) { .dat = c->dat, .len = c->count  }; }

I4 Slc_cmp(Slc l, Slc r) { // return -1 if l<r, 1 if l>r, 0 if eq
  U2 len; if(l.len < r.len) len = l.len;  else len = r.len;
  U1 *lp = l.dat; U1 *rp = r.dat;
  for(U2 i = 0; i < len; i += 1) {
    if(*lp < *rp) return -1;
    if(*lp > *rp) return 1;
    lp += 1, rp += 1;
  }
  if(l.len < r.len) return -1;
  if(l.len > r.len) return 1;
  return 0;
}

// ##
// # Buf + PlcBuf
DEFINE_AS(Buf,    /*as*/Slc);
DEFINE_AS(PlcBuf, /*as*/Slc);
DEFINE_AS(PlcBuf, /*as*/Buf);

void Buf_ntCopy(Buf* b, U1* s) {
  b->len = strlen(s);
  ASSERT(b->cap >= b->len, "Buf_ntCopy: copy too large");
  memcpy(b->dat, s, b->len);
}

// #################################
// # BA: Block Allocator
#define BA_index(BA, BLOCK)   (((Ref)(BLOCK) - (Ref)(BA).blocks) >> BLOCK_PO2)

void BA_init(BA* ba) {
  if(ba->cap == 0) return;
  ASSERT(ba->cap < BLOCK_END, "bad BA init");
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

  ASSERT(d->previ == BLOCK_END, "BA is corrupt"); // "d" is already root node
  d->nexti = *clientRooti; // d -> c
  if(*clientRooti != BLOCK_END) {
    nodes[*clientRooti].previ = di;  // d <- c
  }
  *clientRooti = di; // clientRooti -> d
  return &ba->blocks[di]; // return block 'd'
}

void BA_free(BA* ba, uint8_t* clientRooti, Block* b) {
  // Assert block is within blocks memory region
  ASSERT((b >= ba->blocks)
         and (b <= &ba->blocks[ba->cap + 1])
         , "BA free OOB");
  uint8_t ci = BA_index(*ba, b);

  BANode* nodes = ba->nodes;
  BANode* c = &nodes[ci]; // node 'c'
  if(ci == *clientRooti) {
    ASSERT(c->previ == BLOCK_END, "BA_free corrupt");
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
DEFINE_AS(Arena,  /*as*/Resource);

BBA BBA_new(BA* ba) { return (BBA) { .ba = ba, .rooti = BLOCK_END}; }

bool _BBA_reserveIfSmall(BBA* bba, Slot sz) {
  if((bba->cap) < (bba->len) + sz) {
    if(0 == BA_alloc(bba->ba, &bba->rooti)) return false;
    bba->len = 0;
    bba->cap = BLOCK_SIZE;
  }
  return true;
}

// Allocate "aligned" data from the top of the block.
//
// WARNING: It is the caller's job to ensure that sz is suitably alligned to
// their system width.
U1* BBA_alloc(BBA* bba, Slot sz) {
  if(!_BBA_reserveIfSmall(bba, sz)) return 0;
  bba->cap -= sz;
  U1* out = ((U1*)&bba->ba->blocks[bba->rooti]) + bba->cap;
  return out;
}

// Allocate "unaligned" data from the bottom of the block.
U1* BBA_allocUnaligned(BBA* bba, Slot sz) {
  if(!_BBA_reserveIfSmall(bba, sz)) return 0;
  U1* out = ((U1*)&bba->ba->blocks[bba->rooti]) + bba->len;
  bba->len += sz;
  return out;
}

void BBA_drop(BBA* bba) {
  BA* ba = bba->ba;
  while(bba->rooti != BLOCK_END)
    BA_free(ba, &bba->rooti, &ba->blocks[bba->rooti]);
  assert(BLOCK_END == bba->rooti);
  bba->len = 0; bba->cap = 0;
}

void BBA_free(BBA* bba, void* data, Slot sz) {} // noop

MArena mBBA = (MArena) {
  .drop           = Role_METHOD(BBA_drop),
  .alloc          = Role_METHOD(BBA_alloc, Slot),
  .allocUnaligned = Role_METHOD(BBA_alloc, Slot),
  .free           = Role_METHOD(BBA_free, void*, Slot),
};

Arena BBA_asArena(BBA* bba) { return (Arena) { .m = mBBA, .d = bba }; }

// #################################
// # Binary Search Tree

// Find slice in Bst, starting at `*node`. Set result to `*node`
// Else, the return value is the result of `Slc_cmp(node.ckey, slc)`
//
// This can be used like this:
//   Bst* node = NULL;
//   I4 cmp = Bst_find(&node, Slc_ntLit("myNode"));
//   // if   not node    : *node was null (Bst is empty)
//   // elif cmp == 0    : *node key == "myNode"
//   // elif cmp < 0     : *node key <  "myNode"
//   // else cmp > 0     : *node key >  "myNode"
I4 Bst_find(Bst** node, Slc slc) {
  if(!*node) return 0;
  while(true) {
    I4 cmp = Slc_cmp(slc, Slc_frCStr((*node)->key));
    if(cmp == 0) return 0; // found exact match
    if(cmp < 0) {
      if((*node)->l)  *node = (*node)->l; // search left
      else            return cmp; // not found
    } else /* cmp > 0 */ {
      if((*node)->r)  *node = (*node)->r; // search right
      else            return cmp; // not found
    }
  }
}

// Add a node to the tree, modifying *root if the node becomes root.
//
// Returns NULL if `add.key` does not exist in the tree. Else returns the
// existing node.
Bst* Bst_add(Bst** root, Bst* add) {
  if(!*root) { *root = add; return NULL; } // new root
  Bst* node = *root; // prevent modification to root
  I4 cmp = Bst_find(&node, Slc_frCStr(add->key));
  if(cmp == 0) return node;
  if(cmp < 0) node->l = add;
  else        node->r = add;
  add->l = 0, add->r = 0;
  return NULL;
}

// #################################
// # File
DEFINE_AS(RFile,  /*as*/Resource);
