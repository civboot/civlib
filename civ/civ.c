#include <string.h>
#include <assert.h>

#include "./civ.h"

/*extern*/ const U1* emptyNt = "";
/*extern*/ jmp_buf* err_jmp = NULL;
/*extern*/ U2 civErr        = 0;
/*extern*/ Civ civ          = (Civ) {0};

void Civ_init() {
  civ = (Civ) {0};
  civ.fb = &civ.rootFiber;
}

// ####
// # Core methods

// Most APIs only align on 4 bytes (or are unaligned)
#define FIX_ALIGN(A) ((A == 1) ? 1 : 4)

void defaultErrPrinter() {
  eprintf("!! Error: %.*s\n", civ.fb->err.len, civ.fb->err.dat);
}

Slot align(Slot ptr, U2 alignment) {
  U2 need = alignment - (ptr % alignment);
  return (need == alignment) ? ptr : (ptr + need);
}

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
U4   U4_min (U4  a, U4  b) MIN_DEF
Slot Slot_min(Slot a, Slot b) MIN_DEF

#define MAX_DEF { if(a < b) return a; return b; }
U4   U4_max (U4  a, U4  b) MAX_DEF
Slot Slot_max(Slot a, Slot b) MAX_DEF

// ##
// # Slc
Slc Slc_frNt(U1* s)     { return (Slc) { .dat = s,      .len = strlen(s) }; }
Slc Slc_frCStr(CStr* c) { return (Slc) { .dat = c->dat, .len = c->count  }; }

I4 Slc_cmp(Slc l, Slc r) { // return -1 if l<r, 1 if l>r, 0 if eq
  U2 len = U4_min(l.len, r.len);
  U1 *lp = l.dat; U1 *rp = r.dat;
  for(U2 i = 0; i < len; i += 1) {
    if(*lp < *rp) return -1;
    if(*lp > *rp) return 1;
    lp += 1; rp += 1;
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

Buf Buf_new(Arena arena, U2 cap) {
  return (Buf) { .dat = Xr(arena, alloc, cap, 1), .cap = cap, };
}

PlcBuf PlcBuf_new(Arena arena, U2 cap) {
  return (PlcBuf) { .dat = Xr(arena, alloc, cap, 1), .cap = cap };
}

bool Buf_extend(Buf* b, Slc s) {
  if(b->cap < b->len + s.len) return true;
  memcpy(b->dat + b->len, s.dat, s.len);
  b->len += s.len;
  return false;
}

bool Buf_extendNt(Buf* b, U1* s) {
  return Buf_extend(b, Slc_frNt(s));
}

void PlcBuf_shift(PlcBuf* buf) {
  I4 len = (I4)buf->len - buf->plc;
  ASSERT(len >= 0, "PlcBuf_shift: invalid plc");
  memmove(buf->dat, buf->dat + buf->plc, (U4)len);
  buf->len = (U2)len; buf->plc = 0;
}

// #################################
// # Ring: a lock-free ring buffer.

U2 Ring_len(Ring* r) {
  if(r->tail >= r->head) return r->tail - r->head;
  else                   return r->tail + r->_cap - r->head;
}

static inline void Ring_wrapHead(Ring* r) {
  r->head += 1;
  if(r->head >= r->_cap) r->head = 0;
}

static inline void Ring_wrapTail(Ring* r) {
  r->tail += 1;
  if(r->tail >= r->_cap) r->tail = 0;
}

U1* Ring_next(Ring* r) {
  if(r->head == r->tail) return NULL;
  U1* out = r->dat + r->head;
  Ring_wrapHead(r);
  return out;
}

void Ring_push(Ring* r, U1 c) {
  ASSERT(r->head + 1 != r->tail, "Ring push: already full");
  r->dat[r->tail] = c;
  Ring_wrapTail(r);
}

void Ring_extend(Ring* r, Slc s) {
  ASSERT(r->_cap - Ring_len(r) > s.len, "Ring extend: too full");
  U2 first = r->_cap - r->tail;
  if(first >= s.len) {
    // There is enough room between tail and cap.
    memmove(r->dat + r->tail, s.dat, s.len);
    r->tail += s.len;
    if(r->tail >= r->_cap) r->tail = 0;
  } else {
    // We need to do two moves: first between tail and cap, then the rest at
    // start of dat.
    U2 second = s.len - first;
    memmove(r->dat + r->tail, s.dat, first);
    memmove(r->dat, s.dat + first, second);
    r->tail = second;
  }
}

Slc Ring_avail(Ring* r) {
  if(r->tail >= r->head) {
    // There is data from tail to cap. Subtract 1 if head==0
    return (Slc){r->dat + r->tail, r->_cap - r->tail - (not r->head)};
  }
  return (Slc){r->dat + r->tail, r->head - r->tail - 1};
}

void Ring_incTail(Ring* r, U2 inc) {
  r->tail += inc;
  if(r->tail >= r->_cap) {
    r->tail -= r->_cap;
  }
}

void Ring_incHead(Ring* r, U2 inc) {
  r->head += inc;
  if(r->head >= r->_cap) {
    r->head -= r->_cap;
  }
}

Slc Ring_first(Ring* r) {
  if(r->tail >= r->head) {
    return (Slc) { .dat = r->dat + r->head, .len = r->tail - r->head };
  }
  return (Slc) { .dat = r->dat + r->head, .len = r->_cap - r->head };
}

Slc Ring_second(Ring* r) {
  if(r->tail >= r->head) return (Slc){0};
  return (Slc) { .dat = r->dat, .len = r->tail };
}

I4 Ring_cmpSlc(Ring* r, Slc s) {
  Slc first = Ring_first(r);
  I4 cmp = Slc_cmp(first, (Slc){s.dat, first.len});
  if(cmp) return cmp;
  return Slc_cmp(Ring_second(r), (Slc){s.dat + first.len, s.len - first.len});
}

// ##
// # Sll
void Sll_add(Sll** to, Sll* node) {
  Sll* next = *to;
  *to = node;
  node->next = next;
}

Sll* Sll_pop(Sll** from) {
  if(not *from) return NULL;
  Sll* out = *from;
  *from = out->next;
  return out;
}

// ##
// # Dll

// to -> a  ===>  to -> b -> a
void Dll_add(Dll* to, Dll* node) {
  node->next = to->next;
  to->next = node;
  node->prev = to;
}

// from -> b -> a ===> from -> a  (return b)
Dll* Dll_pop(Dll* from) {
  Dll* b = from->next;
  if(b) {
    Dll* a = b->next;
    from->next = a;
    if(a) a->prev = from;
  } else {
    from->next = NULL;
  }
  return b;
}

// a -> node -> b ==> a -> b
Dll* Dll_remove(Dll* node) {
  if(not node) return NULL;
  Dll* a = node->prev;
  Dll* b = node->next;
  if(a) a->next = b;
  if(b) b->prev = a;
  return node;
}

//     root              root
//       v      ==>        v
// a <-> b           a <-> node <-> b
void DllRoot_add(DllRoot* root, Dll* node) {
  Dll* b = root->start;
  root->start = node;
  if(b) {
    node->next = b;
    Dll* a = b->prev;
    node->prev = a;
    if(a) a->next = node;
    b->prev = node;
  } else {
    node->next = NULL;
    node->prev = NULL;
  }
}

//     root                  root       root
//       v          ==>        v   ===>   v
// a <-> node <-> b      a <-> b          a
Dll* DllRoot_pop(DllRoot* root) {
  Dll* node = root->start;
  if(not node) return NULL;
  if(node->next) root->start = node->next;
  else           root->start = node->prev;
  return Dll_remove(node);
}

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
// # BA: Block Allocator

Dll*     BANode_asDll(BANode* node) { return (Dll*) node; }
DllRoot* BA_asDllRoot(BA* node)     { return (DllRoot*) &node->free; }

BANode* BA_alloc(BA* ba) {
  BANode* out = (BANode*) DllRoot_pop(BA_asDllRoot(ba));
  if(not out) return NULL;
  ba->len -= 1;
  return out;
}

void BA_free(BA* ba, BANode* node) {
  DllRoot_add(BA_asDllRoot(ba), BANode_asDll(node));
  ba->len += 1;
}

void BA_freeAll(BA* ba, BANode* nodes) {
  while(nodes) {
    BANode* freeing = nodes;
    nodes = nodes->next;
    BA_free(ba, freeing);
  }
}

void BA_freeArray(BA* ba, Slot len, BANode nodes[], Block blocks[]) {
  for(Slot i = 0; i < len; i++) {
    BANode* node = nodes + i;
    node->block  = blocks + i;
    BA_free(&civ.ba, node);
  }
}

// #################################
// # BBA: Block Bump Arena
DllRoot* BBA_asDllRoot(BBA* bba) { return (DllRoot*)&bba->dat; }

DEFINE_AS(Arena,  /*as*/Resource);

DEFINE_METHOD(void, BBA,drop) {
  BA_freeAll(this->ba, this->dat);
  this->dat = NULL;
}

// Get spare bytes
DEFINE_METHOD(Slot, BBA,spare) {
  BlockInfo* info = &BBA_info(this);
  return info->top - info->bot;
}

static Block* BBA_allocBlock(BBA* bba) {
  BANode* node = BA_alloc(bba->ba);
  if(not node) return NULL;
  BlockInfo* info = &(node->block->info);
  info->bot = 0;
  info->top = BLOCK_AVAIL;
  DllRoot_add(BBA_asDllRoot(bba), BANode_asDll(node));
  return node->block;
}

// Return block that can handle the growth or NULL
static Block* _allocBlockIfRequired(BBA* bba, Slot grow) {
  if(not bba->dat)
    return BBA_allocBlock(bba);
  Block* block = BBA_block(bba);
  if(block->info.bot + grow > block->info.top)
      return BBA_allocBlock(bba);
  return block;
}


DEFINE_METHOD(void*, BBA,alloc, Slot sz, U2 alignment) {
  ASSERT(sz <= BLOCK_AVAIL, "allocation sz too large");
  if(1 == alignment) {
    // Grow up
    Block* block = _allocBlockIfRequired(this, sz);
    if(not block) return NULL;
    U1* out = (U1*)block + block->info.bot;
    block->info.bot += sz;
    return out;
  }
  // Else grow down (aligned)
  sz = align(sz, FIX_ALIGN(alignment));
  Block* block = _allocBlockIfRequired(this, sz);
  if(not block) return NULL;
  U2* top = &(block->info.top);
  *top -= sz;
  return (U1*)block + (*top);
}

DEFINE_METHOD(void, BBA,free, void* data, Slot sz, U2 alignment) {
  ASSERT(this->dat, "Free empty BBA");
  Block* block = BBA_block(this);
  BlockInfo* info = &block->info;
  ASSERT(( (U1*)block <= (U1*)data )
         and
         ( (U1*)data + sz <= (U1*)block + BLOCK_AVAIL ),
         "unordered free: block bounds");
  U2 plc = (U2)((U1*)data - (U1*)block);

  if(1 == alignment) {
    ASSERT(plc == info->bot - sz, "unordered free: sz");
    info->bot = plc;
  } else {
    sz = align(sz, FIX_ALIGN(alignment));
    ASSERT(plc <= info->top, "unordered free: sz");
    info->top = plc + sz;
  }

  if(info->top - info->bot == BLOCK_AVAIL) {
    BA_free(this->ba, (BANode*)DllRoot_pop(BBA_asDllRoot(this)));
  }
}

DEFINE_METHOD(Slot, BBA,maxAlloc) { return BLOCK_AVAIL; }

DEFINE_METHODS(MArena, BBA_mArena,
  .drop      = M_BBA_drop,
  .alloc     = M_BBA_alloc,
  .free      = M_BBA_free,
  .maxAlloc  = M_BBA_maxAlloc,
)

Arena BBA_asArena(BBA* d) { return (Arena) { .m = BBA_mArena(), .d = d }; }

// #################################
// # File
// DEFINE_AS(RFile,  /*as*/Resource);
