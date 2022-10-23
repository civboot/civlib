#include  "civ_unix.h"

TEST(basic)
  uint64_t direct                     = 1ULL << 63;
  uint64_t calculated = 1; calculated = calculated << 63;
  assert(1 == (direct     >> 63));
  assert(1 == (calculated >> 63));
END_TEST

TEST(file)
  File f = (File) {
    .buf = (PlcBuf) { .dat = malloc(20), .cap = 20 },
    .code = File_CLOSED,
  };
  File_open(&f, sSlc("data/UFile_test.txt"), File_RDONLY);
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

  ASSERT_EQ(4, civ.ba.cap);
  // Check start node: root <-> a
  ASSERT_EQ(BLOCK_END, nodes[0].previ);
  ASSERT_EQ(1        , nodes[0].nexti);

  // Check end node
  ASSERT_EQ(2        , nodes[3].previ);
  ASSERT_EQ(BLOCK_END, nodes[3].nexti);
END_TEST_UNIX

TEST_UNIX(allocFree, 5)
  BANode* nodes = civ.ba.nodes;
  U1 crooti = BLOCK_END; // clientRoot

  Block* a = BA_alloc(&civ.ba, &crooti);
  ASSERT_EQ(civ.ba.blocks, a); // first block

  // // clientroot -> a
  ASSERT_EQ(0         , crooti);
  ASSERT_EQ(BLOCK_END , nodes[0].previ);
  ASSERT_EQ(BLOCK_END , nodes[0].nexti);

  // // baRoot -> b -> c
  ASSERT_EQ(1         , civ.ba.rooti);
  ASSERT_EQ(BLOCK_END , nodes[1].previ);
  ASSERT_EQ(2         , nodes[1].nexti);

  BA_free(&civ.ba, &crooti, a);
  ASSERT_EQ(BLOCK_END , crooti);
  ASSERT_EQ(BLOCK_END , nodes[0].previ);
  ASSERT_EQ(1         , nodes[0].nexti);
  ASSERT_EQ(0         , nodes[1].previ);
END_TEST_UNIX

TEST_UNIX(alloc2FreeFirst, 5)
  BANode* nodes = civ.ba.nodes;
  printf("Nodes: %llx\n", (U8)((U4)nodes));
  uint8_t crooti = BLOCK_END; // clientRoot

  Block* a = BA_alloc(&civ.ba, &crooti);
  Block* b = BA_alloc(&civ.ba, &crooti); // clientRoot -> b -> a;  baRoot -> c -> d
  ASSERT_EQ(a, civ.ba.blocks);        // first block
  ASSERT_EQ(b, &civ.ba.blocks[1]);  // second block
  BA_free(&civ.ba, &crooti, a); // clientroot -> b -> END;  baRoot -> a -> c -> d

  // clientroot -> b -> END
  ASSERT_EQ(1         , crooti);
  ASSERT_EQ(BLOCK_END , nodes[1].previ);
  ASSERT_EQ(BLOCK_END , nodes[1].nexti);

  // baRoot -> a -> c ...
  ASSERT_EQ(0         , civ.ba.rooti);
  ASSERT_EQ(BLOCK_END , nodes[0].previ);
  ASSERT_EQ(2         , nodes[0].nexti);
END_TEST_UNIX

TEST_UNIX(bba, 5)
  BBA bba = BBA_new(&civ.ba);

  BANode* nodes = civ.ba.nodes;
  ASSERT_EQ((U1*) civ.ba.blocks + BLOCK_SIZE - 12  , BBA_alloc(&bba, 12));
  ASSERT_EQ((U1*)&civ.ba.blocks[1]      , BBA_alloc(&bba, BLOCK_SIZE));
  ASSERT_EQ((U1*)&civ.ba.blocks[2]      , BBA_allocUnaligned(&bba, 13));
  ASSERT_EQ((U1*)&civ.ba.blocks[2] + 13 , BBA_allocUnaligned(&bba, 25));
  ASSERT_EQ((U1*)&civ.ba.blocks[3]      , BBA_allocUnaligned(&bba, BLOCK_SIZE - 20));
  ASSERT_EQ(NULL                        , BBA_alloc(&bba, BLOCK_SIZE));
  BBA_drop(&bba);
  ASSERT_EQ((U1*) civ.ba.blocks         , BBA_allocUnaligned(&bba, 12));
END_TEST_UNIX

int main() {
  eprintf("# Starting Tests\n");
  test_basic();
  test_file();
  test_baNew();
  test_allocFree();
  test_alloc2FreeFirst();
  test_bba();
  eprintf("# Tests All Pass\n");
  return 0;
}

