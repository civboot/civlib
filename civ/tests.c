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

TEST(dict)
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


TEST(file)
  File f = (File) {
    .buf = (PlcBuf) { .dat = malloc(20), .cap = 20 },
    .code = File_CLOSED,
  };
  File_open(&f, Slc_ntLit("data/UFile_test.txt"), File_RDONLY);
  assert(f.code == File_DONE);
  File_readAll(&f);
  assert(f.buf.len == 20); assert(f.code == File_DONE);
  assert(0 == memcmp(f.buf.dat, "easy to test text\nwr", 20));

  File_readAll(&f);
  assert(f.buf.len == 20); assert(f.code == File_DONE);
  assert(0 == memcmp(f.buf.dat, "iting a simple haiku", 20));

  File_readAll(&f);
  assert(f.buf.len == 20); assert(f.code == File_DONE);
  assert(0 == memcmp(f.buf.dat, "\nand the job is done", 20));

  File_readAll(&f);
  assert(f.buf.len == 2); assert(f.code == File_EOF);
  assert(0 == memcmp(f.buf.dat, "\n\n", 2));
  File_close(&f);
  free(f.buf.dat);
END_TEST

TEST_UNIX(baNew, 5)
  BANode* nodes = civ.ba.nodes;

  TASSERT_EQ(4, civ.ba.cap);
  // Check start node: root <-> a
  TASSERT_EQ(BLOCK_END, nodes[0].previ);
  TASSERT_EQ(1        , nodes[0].nexti);

  // Check end node
  TASSERT_EQ(2        , nodes[3].previ);
  TASSERT_EQ(BLOCK_END, nodes[3].nexti);
END_TEST_UNIX

TEST_UNIX(allocFree, 5)
  BANode* nodes = civ.ba.nodes;
  U1 crooti = BLOCK_END; // clientRoot

  Block* a = BA_alloc(&civ.ba, &crooti);
  TASSERT_EQ(civ.ba.blocks, a); // first block

  // // clientroot -> a
  TASSERT_EQ(0         , crooti);
  TASSERT_EQ(BLOCK_END , nodes[0].previ);
  TASSERT_EQ(BLOCK_END , nodes[0].nexti);

  // // baRoot -> b -> c
  TASSERT_EQ(1         , civ.ba.rooti);
  TASSERT_EQ(BLOCK_END , nodes[1].previ);
  TASSERT_EQ(2         , nodes[1].nexti);

  BA_free(&civ.ba, &crooti, a);
  TASSERT_EQ(BLOCK_END , crooti);
  TASSERT_EQ(BLOCK_END , nodes[0].previ);
  TASSERT_EQ(1         , nodes[0].nexti);
  TASSERT_EQ(0         , nodes[1].previ);
END_TEST_UNIX

TEST_UNIX(alloc2FreeFirst, 5)
  BANode* nodes = civ.ba.nodes;
  uint8_t crooti = BLOCK_END; // clientRoot

  Block* a = BA_alloc(&civ.ba, &crooti);
  Block* b = BA_alloc(&civ.ba, &crooti); // clientRoot -> b -> a;  baRoot -> c -> d
  TASSERT_EQ(a, civ.ba.blocks);        // first block
  TASSERT_EQ(b, &civ.ba.blocks[1]);  // second block
  BA_free(&civ.ba, &crooti, a); // clientroot -> b -> END;  baRoot -> a -> c -> d

  // clientroot -> b -> END
  TASSERT_EQ(1         , crooti);
  TASSERT_EQ(BLOCK_END , nodes[1].previ);
  TASSERT_EQ(BLOCK_END , nodes[1].nexti);

  // baRoot -> a -> c ...
  TASSERT_EQ(0         , civ.ba.rooti);
  TASSERT_EQ(BLOCK_END , nodes[0].previ);
  TASSERT_EQ(2         , nodes[0].nexti);
END_TEST_UNIX

TEST_UNIX(bba, 5)
  BBA bba = BBA_new(&civ.ba);

  BANode* nodes = civ.ba.nodes;
  TASSERT_EQ((U1*) civ.ba.blocks + BLOCK_SIZE - 12  , BBA_alloc(&bba, 12));
  TASSERT_EQ((U1*)&civ.ba.blocks[1]      , BBA_alloc(&bba, BLOCK_SIZE));
  TASSERT_EQ((U1*)&civ.ba.blocks[2]      , BBA_allocUnaligned(&bba, 13));
  TASSERT_EQ((U1*)&civ.ba.blocks[2] + 13 , BBA_allocUnaligned(&bba, 25));
  TASSERT_EQ((U1*)&civ.ba.blocks[3]      , BBA_allocUnaligned(&bba, BLOCK_SIZE - 20));
  TASSERT_EQ(NULL                        , BBA_alloc(&bba, BLOCK_SIZE));
  BBA_drop(&bba);
  TASSERT_EQ((U1*) civ.ba.blocks         , BBA_allocUnaligned(&bba, 12));
END_TEST_UNIX

int main() {
  eprintf("# Starting Tests\n");
  test_basic();
  test_slc();
  test_dict();
  test_file();
  test_baNew();
  test_allocFree();
  test_alloc2FreeFirst();
  test_bba();
  eprintf("# Tests All Pass\n");
  return 0;
}

