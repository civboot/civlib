#include <string.h>
#include <assert.h>

#include "./civ.h"

/*extern*/ jmp_buf* err_jmp = NULL;
/*extern*/ U2 civErr        = 0;
/*extern*/ Civ civ          = (Civ) {0};

void Civ_init() {
  civ = (Civ) {0};
  civ.fb = &civ.rootFiber;
}

// ####
// # Core methods

#define FIX_ALIGN(A) ((A == 1) ? 1 : 4)

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

void BBA_drop(BBA* bba) {
  BA_freeAll(bba->ba, bba->dat);
  bba->dat = NULL;
}

// Get spare bytes
Slot BBA_spare(BBA* bba) {
  BlockInfo* info = &BBA_info(bba);
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
Block* _allocBlockIfRequired(BBA* bba, Slot grow) {
  if(not bba->dat)
    return BBA_allocBlock(bba);
  Block* block = BBA_block(bba);
  if(block->info.bot + grow > block->info.top)
      return BBA_allocBlock(bba);
  return block;
}


void* BBA_alloc(BBA* bba, Slot sz, U2 alignment) {
  ASSERT(sz <= BLOCK_AVAIL, "allocation sz too large");
  if(1 == alignment) {
    // Grow up
    Block* block = _allocBlockIfRequired(bba, sz);
    eprintf("??? got block: %X  bot=%u top=%u\n", block, block->info.bot, block->info.top);
    if(not block) return NULL;
    U1* out = (U1*)block + block->info.bot;
    block->info.bot += sz;
    return out;
  }
  // Else grow down (aligned)
  sz = align(sz, FIX_ALIGN(alignment));
  Block* block = _allocBlockIfRequired(bba, sz);
  if(not block) return NULL;
  U2* top = &(block->info.top);
  *top -= sz;
  return (U1*)block + (*top);
}

void BBA_free(BBA* bba, void* data, Slot sz, U2 alignment) {
  eprintf("??? what?\n");
  ASSERT(bba->dat, "Free empty BBA");
  eprintf("bba=%X, data=%X, sz=%u, align=%u\n", bba, data, sz, alignment);

  Block* block = BBA_block(bba);
  BlockInfo* info = &block->info;
  ASSERT(( (U1*)block <= (U1*)data )
         and
         ( (U1*)data + sz <= (U1*)block + BLOCK_AVAIL ),
         "unordered free: block bounds");
  U2 plc = (U2)((U1*)data - (U1*)block);
  eprintf("??? plc=%X\n", plc);

  if(1 == alignment) {
    eprintf("??? here\n");
    ASSERT(plc == info->bot - sz, "unordered free: sz");
    info->bot = plc;
  } else {
    eprintf("freeing it bro\n");
    sz = align(sz, FIX_ALIGN(alignment));
    ASSERT(plc <= info->top, "unordered free: sz");
    info->top = plc + sz;
  }

  if(info->top - info->bot == BLOCK_AVAIL) {
    BA_free(bba->ba, (BANode*)DllRoot_pop(BBA_asDllRoot(bba)));
  }
}

Slot BBA_maxAlloc(void* anything) { return BLOCK_AVAIL; }

/*extern*/ const MArena mBBA = (MArena) {
  .drop  = Role_METHOD(BBA_drop),
  .alloc = Role_METHODR(BBA_alloc, /*ret*/void*, Slot,  U2),
  .free  = Role_METHOD(BBA_free,                 void*, Slot, U2),
  .maxAlloc = BBA_maxAlloc,
};

Arena BBA_asArena(BBA* d) { return (Arena) { .m = &mBBA, .d = d }; }

// #################################
// # File
// DEFINE_AS(RFile,  /*as*/Resource);
