#include <string.h>
#include <assert.h>

#include "civ.h"

/*extern*/ char** ARGV       = NULL;
/*extern*/ const U1* emptyNt = "";
/*extern*/ jmp_buf* err_jmp  = NULL;
/*extern*/ U2 civErr         = 0;
/*extern*/ Civ civ           = (Civ) {0};

void Civ_init(Fiber* fb) {
  civ = (Civ) {.fb = fb};
}

void runErrPrinter() {
  if(civ.errPrinter) civ.errPrinter();
  else               defaultErrPrinter();
}

// ####
// # Core methods

// Most APIs only align on 4 bytes (or are unaligned)
#define FIX_ALIGN(A) ((A == 1) ? 1 : 4)

S align(S ptr, U2 alignment) {
  U2 need = alignment - (ptr % alignment);
  return (need == alignment) ? ptr : (ptr + need);
}

// ##
// # Big Endian (unaligned) Fetch/Store
U4 ftBE(U1* p, S size) { // fetch Big Endian
  switch(size) {
    case 1: return *p;                  case 2: return ftBE2(p);
    case 4: return ftBE4(p);
    default: SET_ERR(SLC("ftBE: invalid sz"));
  }
}

void srBE(U1* p, S size, U4 value) { // store Big Endian
  switch(size) {
    case 1: *p = value;      break;
    case 2: srBE2(p, value); break;
    case 4: srBE4(p, value); break;
    default: SET_ERR(SLC("srBE: invalid sz"));
  }
}

// ##
// # Slc
Slc Slc_frNt(U1* s)     { return (Slc) { .dat = s,      .len = strlen(s) }; }
Slc Slc_frCStr(CStr* c) { return (Slc) { .dat = c->dat, .len = c->len    }; }

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

U2 Slc_move(Slc to, Slc from) {
  if(to.len > from.len) {
    memmove(to.dat, from.dat, from.len);
    return from.len;
  }
  memmove(to.dat, from.dat, to.len);
  return to.len;
}

// ##
// # Buf + PlcBuf
void Buf_add(Buf* b, U1 v) {
  ASSERT(b->len < b->cap, "Buf add OOB");
  _Buf_add(b, v);
}

void Buf_addBE2(Buf* b, U2 v) {
  ASSERT(b->len + 1 < b->cap, "Buf addBE2 OOB");
  srBE2(b->dat + b->len, v);
  b->len += 2;
}

void Buf_addBE4(Buf* b, U4 v) {
  U2 len = b->len;
  ASSERT(len + 3 < b->cap, "Buf addBE4 OOB");
  srBE4(b->dat + b->len, v);
  b->len += 4;
}

void Buf_extend(Buf* b, Slc s) {
  ASSERT(b->cap >= b->len + s.len, "Buf extend OOB");
  memcpy(b->dat + b->len, s.dat, s.len);
  b->len += s.len;
}

void Buf_extendNt(Buf* b, U1* s) {
  return Buf_extend(b, Slc_frNt(s));
}

#define _slc(TY) { \
  ASSERT(end >= start, #TY "_slc end < start"); \
  ASSERT(end <= d->len, #TY "_slc OOB"); \
  return (Slc){.dat = d->dat + start, .len = end - start}; \
}
Slc Slc_slc(Slc* d, U2 start, U2 end) _slc(Slc)
Slc Buf_slc(Buf* d, U2 start, U2 end) _slc(Buf)
#undef _slc

void PlcBuf_shift(PlcBuf* buf) {
  I4 len = (I4)buf->len - buf->plc;
  ASSERT(len >= 0, "PlcBuf_shift: invalid plc");
  memmove(buf->dat, buf->dat + buf->plc, (U4)len);
  buf->len = (U2)len; buf->plc = 0;
}

CStr* CStr_init(CStr* this, Slc s) {
  ASSERT(s.len <= 0xFF, "CStr max len = 255");
  this->len = s.len;
  memcpy(this->dat, s.dat, s.len);
  return this;
}

CStr* CStr_new(Arena a, Slc s) {
  ASSERT(s.len <= 0xFF, "CStr max len = 255");
  CStr* c = (CStr*) Xr(a, alloc, s.len + 1, /*align*/1);
  if(not c) return NULL;
  return CStr_init(c, s);
}


// #################################
// # Stk: efficient first-in last-out buffer.

S Stk_pop(Stk* stk) {
  ASSERT(stk->sp < stk->cap, "Stk underflow");
  return stk->dat[stk->sp ++];
}

S Stk_top(Stk* stk) {
  ASSERT(stk->sp < stk->cap, "Stk_top OOB");
  return stk->dat[stk->sp];
}

S* Stk_topRef(Stk* stk) {
  ASSERT(stk->sp < stk->cap, "Stk_topRef OOB");
  return &stk->dat[stk->sp];
}

void Stk_add(Stk* stk, S value) {
  ASSERT(stk->sp, "Stk overflow");
  stk->dat[-- stk->sp] = value;
}

// #################################
// # Ring: a lock-free ring buffer.

U2 Ring_len(Ring* r) {
  if(r->tail >= r->head) return r->tail - r->head;
  else                   return r->tail + r->_cap - r->head;
}

U1 Ring_get(Ring* r, U2 i) {
  ASSERT(i < Ring_len(r), "Ring_get OOB");
  return r->dat[(r->head + i) % r->_cap];
}

static inline void Ring_wrapHead(Ring* r) {
  r->head = (r->head + 1) % r->_cap;
}

static inline void Ring_wrapTail(Ring* r) {
  r->tail = (r->tail + 1) % r->_cap;
}

U1* Ring_next(Ring* r) {
  if(r->head == r->tail) return NULL;
  U1* out = r->dat + r->head;
  Ring_wrapHead(r);
  return out;
}

U1 Ring_pop(Ring* r) {
  ASSERT(not Ring_isEmpty(r), "Ring pop: empty");
  U1 c = r->dat[r->head];
  Ring_wrapHead(r);
  return c;
}

void Ring_push(Ring* r, U1 c) {
  ASSERT(not Ring_isFull(r), "Ring push: already full");
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

// Move as much data as possible into ring. Return the amount moved.
U2 Ring_move(Ring* r, Slc s) { return Slc_move(Ring_avail(r), s); }

Slc Ring_1st(Ring* r) {
  if(r->tail >= r->head) {
    return (Slc) { .dat = r->dat + r->head, .len = r->tail - r->head };
  }
  return (Slc) { .dat = r->dat + r->head, .len = r->_cap - r->head };
}

Slc Ring_2nd(Ring* r) {
  if(r->tail >= r->head) return (Slc){0};
  return (Slc) { .dat = r->dat, .len = r->tail };
}

I4 Ring_cmpSlc(Ring* r, Slc s) {
  Slc first = Ring_1st(r);
  I4 cmp = Slc_cmp(first, (Slc){s.dat, first.len});
  if(cmp) return cmp;
  return Slc_cmp(Ring_2nd(r), (Slc){s.dat + first.len, s.len - first.len});
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

S Sll_len(Sll* node) {
  S len = 0; for(; node; node = node->next) len += 1;
  return len;
}

Sll* Sll_reverse(Sll* node) {
  Sll* prev = NULL;
  while(node) {
    Sll* next = node->next;
    node->next = prev;
    prev = node;
    node = next;
  }
  return prev;
}

Slc* Sll_free(Sll* node, U2 nodeSz, Arena a) {
  while(node) {
    Sll* next = node->next;
    Slc* err = Xr(a, free, node, nodeSz, alignment(nodeSz));
    if(err) return err;
    node = next;
  }
  return NULL;
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
//   I4 cmp = Bst_find(&node, SLC("myNode"));
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
//
// WARNING: this does not modify add.left or add.right.
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

void BA_freeArray(BA* ba, S len, BANode nodes[], Block blocks[]) {
  for(S i = 0; i < len; i++) {
    BANode* node = nodes + i;
    node->block  = blocks + i;
    BA_free(&civ.ba, node);
  }
}

// #################################
// # BBA: Block Bump Arena
DllRoot* BBA_asDllRoot(BBA* bba) { return (DllRoot*)&bba->dat; }

DEFINE_METHOD(void, BBA,drop) {
  BA_freeAll(this->ba, this->dat);
  this->dat = NULL;
}

// Get spare bytes
DEFINE_METHOD(S, BBA,spare) {
  Block* b = BBA_block(this);
  return b->top - b->bot;
}

static Block* BBA_allocBlock(BBA* bba) {
  BANode* node = BA_alloc(bba->ba);
  if(not node) return NULL;
  Block* b = node->block;
  b->bot = 0;
  b->top = BLOCK_AVAIL;
  DllRoot_add(BBA_asDllRoot(bba), BANode_asDll(node));
  return node->block;
}

// Return block that can handle the growth or NULL
static Block* _allocBlockIfRequired(BBA* bba, S grow) {
  if(not bba->dat)
    return BBA_allocBlock(bba);
  Block* b = BBA_block(bba);
  if(b->bot + grow > b->top)
      return BBA_allocBlock(bba);
  return b;
}

DEFINE_METHOD(void*, BBA,alloc, S sz, U2 alignment) {
  ASSERT(sz <= BLOCK_AVAIL, "allocation sz too large");
  if(1 == alignment) {
    // Grow up
    Block* b = _allocBlockIfRequired(this, sz);
    if(not b) return NULL;
    U1* out = (U1*)b + b->bot;
    b->bot += sz;
    return out;
  }
  // Else grow down (aligned)
  sz = align(sz, FIX_ALIGN(alignment));
  Block* b = _allocBlockIfRequired(this, sz);
  if(not b) return NULL;
  U2* top = &(b->top);
  *top -= sz;
  return (U1*)b + (*top);
}

Slc BBA_free_empty = SLC("Free empty BBA");
Slc BBA_free_below = SLC("Data below block");
Slc BBA_free_above = SLC("Data above block");
Slc BBA_unorderedSz  = SLC("unordered free: sz");

DEFINE_METHOD(Slc*, BBA,free, void* data, S sz, U2 alignment) {
  if(not data) return NULL;
  if(not this->dat) return &BBA_free_empty;
  Block* b = BBA_block(this);
  if((U1*)b > (U1*)data) return &BBA_free_below;
  if((U1*)data + sz > (U1*)b + BLOCK_AVAIL) return &BBA_free_above;

  U2 plc = (U2)((U1*)data - (U1*)b);
  if(1 == alignment) {
    if(plc != b->bot - sz) return &BBA_unorderedSz;
    b->bot = plc;
  } else {
    sz = align(sz, FIX_ALIGN(alignment));
    if(plc > b->top) return &BBA_unorderedSz;
    b->top = plc + sz;
  }

  if(b->top - b->bot == BLOCK_AVAIL) {
    BA_free(this->ba, (BANode*)DllRoot_pop(BBA_asDllRoot(this)));
  }
  return NULL;
}

DEFINE_METHOD(S, BBA,maxAlloc) { return BLOCK_AVAIL; }

DEFINE_METHODS(MArena, BBA_mArena,
  .drop      = M_BBA_drop,
  .alloc     = M_BBA_alloc,
  .free      = M_BBA_free,
  .maxAlloc  = M_BBA_maxAlloc,
)

Arena BBA_asArena(BBA* d) { return (Arena) { .m = BBA_mArena(), .d = d }; }

void writeAll(Writer w, Slc s) {
  BaseFile* b = Xr(w, asBase);
  while(s.len) {
    U2 moved = Ring_move(&b->ring, s);
    s = (Slc){s.dat + moved, s.len - moved};
    Xr(w, write);
  }
}

#define FEXTEND { \
  BaseFile* b = Xr(f, asBase);            \
  while(s.len) {                          \
    U2 moved = Ring_move(&b->ring, s);    \
    s = (Slc){s.dat + moved, s.len - moved}; \
    if(s.len) Xr(f, write); \
  } \
}


void File_extend(File f, Slc s)      FEXTEND
void Writter_extend(Writer f, Slc s) FEXTEND

U1* Reader_get(Reader f, U2 i) {
  BaseFile* b = Xr(f, asBase);
  Ring* r = &b->ring;
  if(i < Ring_len(r)) return &r->dat[(r->head + i) % r->_cap];
  ASSERT(i < Ring_cap(r), "index larger than Ring");
  do {
    Xr(f, read);
    if(i < Ring_len(r)) return &r->dat[(r->head + i) % r->_cap];
  } while(b->code <= File_DONE);
  return NULL;
}

// #################################
// # BufFile
void File_panicOpen(void* d, Slc path, S options) {
  SET_ERR(SLC("Open not supported."));
}
void File_panic(void* d) { SET_ERR(SLC("Unsuported file method.")); }
void File_noop(void* d)  {}

DEFINE_METHOD(void      , BufFile,drop, Arena a) {
  PlcBuf_free(&this->b, a);
}

DEFINE_METHOD(Sll*      , BufFile,resourceLL) {
  return (Sll*)&this->nextResource;
}

DEFINE_METHOD(void      , BufFile,seek, ISlot offset, U1 whence) {
  if(File_seek_SET == whence) {
    ASSERT(offset >= 0, "SET: offset must be >=0");
    ASSERT(offset < this->b.len, "SET: offset must be < buf.len");
    this->b.plc = offset;
  } else {
    assert(false); // TODO: not implemented
  }
}

DEFINE_METHOD(void      , BufFile,read) {
  ASSERT(this->code == File_READING || this->code >= File_DONE, "File operation out of order");
  Ring* r = &this->ring;
  PlcBuf* b = &this->b;
  Slc avail = Ring_avail(r);
  if(avail.len) {
    U2 moved = Slc_move(avail, PlcBuf_plcAsSlc(b));
    Ring_incTail(r, moved);
    b->plc += moved;
  }
  if     (b->plc >= b->len) this->code = File_EOF;
  else if(Ring_isFull(r))   this->code = File_DONE;
}

DEFINE_METHOD(BaseFile* , BufFile,asBase) {
  return (BaseFile*) &this->ring;
}

DEFINE_METHOD(void      , BufFile,write) {
  ASSERT(this->code == File_WRITING || this->code >= File_DONE, "File operation out of order");
  this->code = File_WRITING;
  Ring* r = &this->ring; PlcBuf* b = &this->b;
  Slc s = Ring_1st(r);
  Buf_extend(PlcBuf_asBuf(b), s);
  Ring_incHead(r, s.len);
  if(Ring_isEmpty(r)) this->code = File_DONE;
}

DEFINE_METHODS(MFile, BufFile_mFile,
  .drop       = M_BufFile_drop,
  .resourceLL = M_BufFile_resourceLL,
  .asBase     = M_BufFile_asBase,
  .open       = File_panicOpen,
  .close      = File_noop,
  .stop       = File_noop,
  .seek       = M_BufFile_seek,
  .read       = M_BufFile_read,
  .write      = M_BufFile_write,
)

File BufFile_asFile(BufFile* d) {
  return (File) { .m = BufFile_mFile(), .d = d };
}
