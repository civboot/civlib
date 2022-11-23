#include  "civ_unix.h"

TEST(basic)
  uint64_t direct                     = 1ULL << 63;
  uint64_t calculated = 1; calculated = calculated << 63;
  assert(1 == (direct     >> 63));
  assert(1 == (calculated >> 63));

  assert(sizeof("hello") == 6); // 1 byte for null terminator
  static const char s0[1 + sizeof("hello")] = "\x05" "hello";
  assert(5 == s0[0]);
  CStr_ntVar(s1, "\x07", "goodbye");
  assert(7 == s1->count);
  assert(0 == strcmp(s1->dat, "goodbye"));

  Slc s2 = Slc_lit(0, 1, 2, 3, 4);
  TASSERT_EQ(5, s2.len);
  TASSERT_EQ(3, s2.dat[3]);

  U1 dat[16] = {0};
  srBE(dat,      1, 0x01);
  srBE(dat + 1,  2, 0x2345);
  srBE(dat + 3,  4, 0x6789ABCD);
  TASSERT_EQ(0x01,         ftBE(dat, 1));
  TASSERT_EQ(0x2345,       ftBE(dat + 1, 2));
  TASSERT_EQ(0x6789ABCD,   ftBE(dat + 3, 4));

  TASSERT_EQ(0, align(0, 4));
  TASSERT_EQ(4, align(1, 4));
  TASSERT_EQ(4, align(2, 4));
  TASSERT_EQ(4, align(4, 4));
END_TEST

TEST(slc)
  Slc a = Slc_ntLit("aaa");
  Slc b = Slc_lit('a', 'b', 'b', 'd');
  Slc c = Slc_ntLit("abc");

  TASSERT_EQ(3, a.len); TASSERT_EQ('a', a.dat[0]);
  TASSERT_EQ(0,  Slc_cmp(a, a));
  TASSERT_EQ(-1, Slc_cmp(a, b));
  TASSERT_EQ(-1, Slc_cmp(a, c));
  TASSERT_EQ(1,  Slc_cmp(b, a));
  TASSERT_EQ(1,  Slc_cmp(c, b));

  Slc c0 = Slc_ntLit("abc");
  TASSERT_EQ(0, Slc_cmp(c, c0));
END_TEST

TEST(sll)
  // create b -> a and then assert.
  Sll* root = NULL;
  Sll a = {0}; Sll b = {0};
  Sll_add(&root, &a);  TASSERT_EQ(root, &a);
  TASSERT_EQ(&a, Sll_pop(&root)); TASSERT_EQ(root, NULL);
  Sll_add(&root, &a);              Sll_add(&root, &b);
  TASSERT_EQ(root, &b);            TASSERT_EQ(b.next, &a);
  TASSERT_EQ(&b, Sll_pop(&root));  TASSERT_EQ(&a, Sll_pop(&root));
  TASSERT_EQ(NULL, Sll_pop(&root));
END_TEST

TEST(dll)
  // create a -> b and then assert
  Dll node = {.dat = (void*)0x10};
  Dll a = {.dat = (void*)0x11}; Dll b = {.dat = (void*)0x12};
  Dll_add(&node, &a);  TASSERT_EQ(node.next, &a);
  TASSERT_EQ(&a, Dll_pop(&node));  TASSERT_EQ(node.next, NULL);
  Dll_add(&node, &a);              Dll_add(&node, &b);
  TASSERT_EQ(node.next, &b);       TASSERT_EQ(b.next, &a);
  TASSERT_EQ(&b, Dll_pop(&node));  TASSERT_EQ(&a, Dll_pop(&node));
  TASSERT_EQ(NULL, Dll_pop(&node));

  // Create a -> node -> b and remove node.
  Dll_add(&a, &node); Dll_add(&node, &b);
  Dll_remove(&node);
  TASSERT_EQ(a.next, &b);  TASSERT_EQ(b.prev, &a);

  // Now treat like a root in a struct
  // root -> b -> a;
  DllRoot root = {0};
  DllRoot_add(&root, &a); TASSERT_EQ(root.start, &a);
  TASSERT_EQ(a.next, NULL); TASSERT_EQ(a.prev, NULL);

  DllRoot_add(&root, &b); TASSERT_EQ(b.next,     &a);
  TASSERT_EQ(a.prev, &b); TASSERT_EQ(b.prev, NULL);

  TASSERT_EQ(0x12, (Slot)(DllRoot_pop(&root)->dat));
  TASSERT_EQ(a.prev, NULL); TASSERT_EQ(a.next, NULL);

  TASSERT_EQ(0x11, (Slot)(DllRoot_pop(&root)->dat));
  TASSERT_EQ(NULL, DllRoot_pop(&root));
END_TEST

TEST(bst)
  Bst a, b, c;

  Bst* node = NULL;
  Bst_find(&node, Slc_ntLit("aaa"));
  assert(NULL == node);

  CStr_ntVar(key_a, "\x03", "aaa");
  CStr_ntVar(key_b, "\x04", "abbd");
  CStr_ntVar(key_c, "\x03", "abc");

  a = (Bst) { .key = key_a };
  b = (Bst) { .key = key_b };
  c = (Bst) { .key = key_c };

  Slc slc_a = Slc_frCStr(key_a);
  Slc slc_b = Slc_frCStr(key_b);
  Slc slc_c = Slc_frCStr(key_c);

  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ(-1, Bst_find(&node, slc_a));    assert(&b == node); // not found
  node = &b; TASSERT_EQ( 1, Bst_find(&node, slc_c));    assert(&b == node); // not found

  node = NULL; Bst_add(&node, &b); assert(node == &b);
  node = &b; Bst_add(&node, &a);
  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_a));    assert(&a == node); // a found
  node = &b; TASSERT_EQ( 1, Bst_find(&node, slc_c));    assert(&b == node); // not found

  node = &b; Bst_add(&node, &c);
  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_a));    assert(&a == node); // a found
  node = &b; TASSERT_EQ( 0, Bst_find(&node, slc_c));    assert(&c == node); // c found

  TASSERT_EQ(b.l, &a);
  TASSERT_EQ(b.r, &c);
END_TEST

#define FIRST_BLOCK  (civUnix.mallocs.start->dat)

TEST_UNIX(ba, 5)
  TASSERT_EQ(sizeof(Block), BLOCK_SIZE);
  TASSERT_EQ(5, civ.ba.len);
  BANode* free = civ.ba.free;
  TASSERT_EQ(free->block - 4, FIRST_BLOCK);
  TASSERT_EQ(free->block - 1, free->next->block);
END_TEST_UNIX

TEST_UNIX(bba, 5)
  BBA bba = {.ba = &civ.ba};
  TASSERT_EQ(5, civ.ba.len);

  U1* bytes = BBA_alloc(&bba, 5, ALIGN1);
  bytes[0] = 'h'; bytes[1] = 'i';
  Block* block = BBA_block(&bba);
  TASSERT_EQ(5, block->info.bot);
  TASSERT_EQ((U1*)block, bytes);

  U1* bytes2 = BBA_alloc(&bba, 12, ALIGN1);
  TASSERT_EQ(17, block->info.bot);
  TASSERT_EQ(bytes + 5, bytes2);
  BBA_free(&bba, bytes2, 12, 1);

  U4* v1 = BBA_alloc(&bba, 4, ALIGN_SLOT);
  TASSERT_EQ((U1*)block + BLOCK_AVAIL - 4, (U1*) v1);
  *v1 = 0x4444;

  TASSERT_EQ(BLOCK_AVAIL - 4, BBA_info(&bba).top);
  Arena a = BBA_asArena(&bba); // testing out arenas
  eprintf("??? Align slot=%u  uintMax=%X\n", ALIGN_SLOT, UINTPTR_MAX);
  Xr(a,free, v1, 4, ALIGN_SLOT);
  TASSERT_EQ(BLOCK_AVAIL, BBA_info(&bba).top);

  BBA_drop(&bba);

  TASSERT_EQ(5, civ.ba.len);
END_TEST_UNIX

// TEST(file)
//   File f = (File) {
//     .buf = (PlcBuf) { .dat = malloc(20), .cap = 20 },
//     .code = File_CLOSED,
//   };
//   File_open(&f, Slc_ntLit("data/UFile_test.txt"), File_RDONLY);
//   assert(f.code == File_DONE);
//   File_readAll(&f);
//   assert(f.buf.len == 20); assert(f.code == File_DONE);
//   assert(0 == memcmp(f.buf.dat, "easy to test text\nwr", 20));
// 
//   File_readAll(&f);
//   assert(f.buf.len == 20); assert(f.code == File_DONE);
//   assert(0 == memcmp(f.buf.dat, "iting a simple haiku", 20));
// 
//   File_readAll(&f);
//   assert(f.buf.len == 20); assert(f.code == File_DONE);
//   assert(0 == memcmp(f.buf.dat, "\nand the job is done", 20));
// 
//   File_readAll(&f);
//   assert(f.buf.len == 2); assert(f.code == File_EOF);
//   assert(0 == memcmp(f.buf.dat, "\n\n", 2));
//   File_close(&f);
//   free(f.buf.dat);
// END_TEST


int main() {
  eprintf("# Starting Tests\n");
  test_basic();
  test_slc();
  test_sll();
  test_dll();
  test_bst();
  test_ba();
  test_bba();
  // test_file();
  eprintf("# Tests All Pass\n");
  return 0;
}

