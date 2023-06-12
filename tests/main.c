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
  assert(7 == s1->len);
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

  TASSERT_EQ(0x7,  bitClr(0x1F, 0x18));
  TASSERT_EQ(0x19, bitSet(0x1F, 1, 7));

  EXPECT_ERR(SET_ERR(SLC("expected 1")), " 1");
  EXPECT_ERR(SET_ERR(SLC("expected 2")), " 2");
END_TEST

TEST(slc)
  Slc a = SLC("aaa");
  Slc b = Slc_lit('a', 'b', 'b', 'd');
  Slc c = SLC("abc");

  TASSERT_EQ(3, a.len); TASSERT_EQ('a', a.dat[0]);
  TASSERT_EQ(0,  Slc_cmp(a, a));
  TASSERT_EQ(-1, Slc_cmp(a, b));
  TASSERT_EQ(-1, Slc_cmp(a, c));
  TASSERT_EQ(1,  Slc_cmp(b, a));
  TASSERT_EQ(1,  Slc_cmp(c, b));

  Slc c0 = SLC("abc");
  TASSERT_EQ(0, Slc_cmp(c, c0));

  TASSERT_EQ(0, Slc_find(SLC("121412"), SLC("12")));
  TASSERT_EQ(6, Slc_find(SLC("121412"), SLC("45")));
  TASSERT_EQ(2, Slc_find(SLC("  abc"),  SLC("abc")));
  TASSERT_EQ(6, Slc_find(SLC("  dabc"), SLC("abcd")));

END_TEST

TEST(buf)
  Buf_var(b, 10);
  TASSERT_EQ(0, b.len); TASSERT_EQ(10, b.cap);
  Buf_extend(&b, SLC("hello"));
  TASSERT_EQ(5, b.len);

  Buf_clear(&b); Buf_addBE2(&b, 0x1234);
  TASSERT_EQ(2, b.len);
  TASSERT_SLC_EQ("\x12\x34", *Buf_asSlc(&b));

  Buf_clear(&b); Buf_addBE4(&b, 0x1234ABCD);
  TASSERT_EQ(4, b.len);
  TASSERT_SLC_EQ("\x12\x34\xAB\xCD", *Buf_asSlc(&b));
END_TEST

TEST(plcBuf)
  char dat[] = "foo bar baz";
  PlcBuf pb = (PlcBuf){.dat = dat, .len = 11, .cap = 11};
  TASSERT_EQ(0, Slc_cmp(SLC("foo bar baz"), *PlcBuf_asSlc(&pb)));
  TASSERT_EQ(0, pb.plc); TASSERT_EQ(11, pb.cap);
  pb.plc = 4; PlcBuf_shift(&pb);
  TASSERT_EQ(0, Slc_cmp(*PlcBuf_asSlc(&pb), SLC("bar baz")));

END_TEST

TEST(stk)
  S dat[3];
  Stk s = Stk_init(dat, 3);
  TASSERT_EQ((S*)dat, s.dat); TASSERT_EQ(3, s.sp); TASSERT_EQ(3, s.cap);
  EXPECT_ERR(Stk_pop(&s), "Stk underflow");
  Stk_add(&s, 3);
  TASSERT_EQ(3, dat[2]); TASSERT_EQ(2, s.sp);
  TASSERT_EQ(3, Stk_pop(&s));
  Stk_add3(&s, 1, 2, 42);
  EXPECT_ERR(Stk_add(&s, 0xFF), "Stk overflow");
  TASSERT_STK(42, &s); TASSERT_STK(2, &s); TASSERT_STK(1, &s);
  S a = 0, b = 0, c = 0;
  Stk_add3(&s, 4, 5, 99); Stk_pop3(&s, a, b, c);
  TASSERT_EQ(4, a); TASSERT_EQ(5, b); TASSERT_EQ(99, c);


END_TEST

TEST(ring)
  U1 dat[10];
  Ring r = Ring_init(dat, 10);
  TASSERT_EQ(9, Ring_cap(&r));
  TASSERT_EQ(true,  Ring_isEmpty(&r));
  TASSERT_EQ(false, Ring_isFull(&r));
  TASSERT_EQ(0, Ring_len(&r));
  Slc avail = Ring_avail(&r);
  TASSERT_EQ(9, avail.len);
  assert(dat == avail.dat);

  Ring_push(&r, 'a');
  TASSERT_EQ(false, Ring_isEmpty(&r));
  TASSERT_EQ(false, Ring_isFull(&r));
  TASSERT_EQ(1, Ring_len(&r));
  TASSERT_EQ(0, r.head); TASSERT_EQ(1, r.tail);
  TASSERT_EQ('a', dat[0]);
  avail = Ring_avail(&r);
  TASSERT_EQ(8, avail.len); assert(dat + 1 == avail.dat);
  TASSERT_EQ('a', Ring_get(&r, 0));

  Ring_extend(&r, SLC("bcde"));
  TASSERT_EQ(0, r.head); TASSERT_EQ(5, r.tail);
  TASSERT_EQ(0, memcmp(dat, "abcde", 5));
  TASSERT_EQ('e', Ring_get(&r, 4));

  assert(dat == Ring_next(&r));
  TASSERT_EQ(1, r.head); TASSERT_EQ(5, r.tail);
  TASSERT_EQ(4, Ring_len(&r));

  Ring_extend(&r, SLC("ABCD"));
  TASSERT_EQ(9, r.tail);  TASSERT_EQ(8, Ring_len(&r));
  TASSERT_EQ(0, memcmp(dat + 1, "bcdeABCD", 8));
  TASSERT_SLC_EQ("bcdeABCD", Ring_1st(&r));
  TASSERT_EQ(0, Ring_2nd(&r).len);
  TASSERT_EQ(0, Ring_cmpSlc(&r, SLC("bcdeABCD")));
  avail = Ring_avail(&r);
  TASSERT_EQ(1, avail.len); assert(dat + 9 == avail.dat);

  // Cannot add len 3
  EXPECT_ERR(Ring_extend(&r, SLC("WXY")), "too full");
  TASSERT_EQ(8, Ring_len(&r));

  // Fill up
  Ring_push(&r, 'e');
  TASSERT_EQ(false, Ring_isEmpty(&r));
  TASSERT_EQ(true, Ring_isFull(&r));

  // Wrap around write
  assert(dat + 1 == Ring_next(&r));
  TASSERT_EQ(2, r.head);
  r.head = 4;
  TASSERT_EQ(6, Ring_len(&r));
  Ring_extend(&r, SLC("fgh"));
  TASSERT_EQ(9, Ring_len(&r));
  TASSERT_EQ(0, memcmp(dat + 4, "eABCDe", 6));
  TASSERT_EQ(0, memcmp(dat, "fgh", 3));
  TASSERT_EQ('f', Ring_get(&r, 6)); TASSERT_EQ('g', Ring_get(&r, 7));
  avail = Ring_avail(&r);
  TASSERT_EQ(0, avail.len); assert(dat + 3 == avail.dat);

  TASSERT_EQ(0, Slc_cmp(SLC("eABCDe"), Ring_1st(&r)));
  TASSERT_EQ(0, Slc_cmp(SLC("fgh"), Ring_2nd(&r)));
  TASSERT_EQ(0, Ring_cmpSlc(&r, SLC("eABCDefgh")));
  TASSERT_EQ(1, Ring_cmpSlc(&r, SLC("aABCDefgh")));

  // Wrap around read
  r.head = 8;
  TASSERT_EQ(5, Ring_len(&r));
  assert(dat + 8 == Ring_next(&r));
  assert(dat + 9 == Ring_next(&r));
  TASSERT_EQ('f', Ring_pop(&r));
  TASSERT_EQ(2, Ring_len(&r));

  // Test an already full Ring
  r.head = 0; r.tail = r._cap - 1;
  TASSERT_EQ(true, Ring_isFull(&r));
END_TEST

TEST_UNIX(sll, 2)
  // create b -> a and then assert.
  Sll* root = NULL;
  Sll a = {0}; Sll b = {0};
  Sll_add(&root, &a);  TASSERT_EQ(root, &a);
  TASSERT_EQ(&a, Sll_pop(&root)); TASSERT_EQ(root, NULL);
  Sll_add(&root, &a);              Sll_add(&root, &b);
  TASSERT_EQ(root, &b);            TASSERT_EQ(b.next, &a);
  TASSERT_EQ(&b, Sll_pop(&root));  TASSERT_EQ(&a, Sll_pop(&root));
  TASSERT_EQ(NULL, Sll_pop(&root));

  // Clone
  Sll_add(&root, &b); Sll_add(&root, &a);
  a.dat = 42; b.dat = 43;

  // root -> a -> b
  root = Sll_reverse(root);
  TASSERT_EQ(root,   &b)
  TASSERT_EQ(b.next, &a);
  TASSERT_EQ(43, root->dat);
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

  TASSERT_EQ(0x12, (S)(DllRoot_pop(&root)->dat));
  TASSERT_EQ(a.prev, NULL); TASSERT_EQ(a.next, NULL);

  TASSERT_EQ(0x11, (S)(DllRoot_pop(&root)->dat));
  TASSERT_EQ(NULL, DllRoot_pop(&root));
END_TEST

TEST(bst)
  CBst a, b, c;

  CBst* node = NULL;
  CBst_find(&node, SLC("aaa"));
  assert(NULL == node);

  CStr_ntVar(key_a, "\x03", "aaa");
  CStr_ntVar(key_b, "\x04", "abbd");
  CStr_ntVar(key_c, "\x03", "abc");

  a = (CBst) { .key = key_a };
  b = (CBst) { .key = key_b };
  c = (CBst) { .key = key_c };

  Slc slc_a = Slc_frCStr(key_a);
  Slc slc_b = Slc_frCStr(key_b);
  Slc slc_c = Slc_frCStr(key_c);

  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ( 1, CBst_find(&node, slc_a));    assert(&b == node); // not found
  node = &b; TASSERT_EQ(-1, CBst_find(&node, slc_c));    assert(&b == node); // not found

  node = NULL; CBst_add(&node, &b); assert(node == &b);
  node = &b; CBst_add(&node, &a);
  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_a));    assert(&a == node); // a found
  node = &b; TASSERT_EQ(-1, CBst_find(&node, slc_c));    assert(&b == node); // not found

  node = &b; CBst_add(&node, &c);
  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_b));    assert(&b == node); // b found
  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_a));    assert(&a == node); // a found
  node = &b; TASSERT_EQ( 0, CBst_find(&node, slc_c));    assert(&c == node); // c found

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
  TASSERT_EQ(5, block->bot);
  TASSERT_EQ((U1*)block, bytes);

  U1* bytes2 = BBA_alloc(&bba, 12, ALIGN1);
  TASSERT_EQ(17, block->bot);
  TASSERT_EQ(bytes + 5, bytes2);
  BBA_free(&bba, bytes2, 12, 1);

  U4* v1 = BBA_alloc(&bba, 4, ALIGN_SLOT);
  TASSERT_EQ((U1*)block + BLOCK_AVAIL - 4, (U1*) v1);
  *v1 = 0x4444;

  TASSERT_EQ(BLOCK_AVAIL - 4, BBA_block(&bba)->top);
  Arena a = BBA_asArena(&bba); // testing out arenas
  Xr(a,free, v1, 4, ALIGN_SLOT);
  TASSERT_EQ(BLOCK_AVAIL, BBA_block(&bba)->top);

  BBA_drop(&bba);

  TASSERT_EQ(5, civ.ba.len);
END_TEST_UNIX

TEST_UNIX(CStr, 2)
  BBA bba = {.ba = &civ.ba};
  Slc expected = SLC("this is from a slice.");
  CStr* c = CStr_new(BBA_asArena(&bba), expected);
  TASSERT_SLC_EQ("this is from a slice.", CStr_asSlc(c));
END_TEST_UNIX

TEST(bufFile)
  { // reading
  BufFile_varNt(f, 16, "Civboot is the foundation of a simpler technology.");
  Ring* r = &f.ring;

  TASSERT_EQ(50, f.b.len);
  BufFile_read(&f);
  TASSERT_EQ(0, Ring_cmpSlc(r, SLC("Civboot is the f")));
  TASSERT_EQ(16, f.b.plc);

  Ring_incHead(r, 6);
  BufFile_read(&f);
  TASSERT_EQ(0, Ring_cmpSlc(r, SLC("t is the fo")));
  BufFile_read(&f);
  TASSERT_EQ(0, Ring_cmpSlc(r, SLC("t is the foundat")));
  TASSERT_EQ(22, f.b.plc);

  Ring_clear(r);
  BufFile_read(&f); TASSERT_EQ(0, Ring_cmpSlc(r, SLC("ion of a simpler")));

  Ring_clear(r);
  BufFile_read(&f); TASSERT_EQ(0, Ring_cmpSlc(r, SLC(" technology.")));
  TASSERT_EQ(File_EOF, f.code);
  EXPECT_ERR(BufFile_read(&f), "after EOF");
  EXPECT_ERR(BufFile_read(&f), "after EOF");
  } // end reading

  // Test writing
  BufFile_var(fw, 15, 256);
  Ring* rw = &fw.ring;
  Buf* bw = PlcBuf_asBuf(&fw.b);
  Slc* sw = Buf_asSlc(bw);
  Ring_extend(rw, SLC("Hello "));
  BufFile_write(&fw);
  TASSERT_EQ(File_DONE, fw.code);
  TASSERT_EQ(0, Ring_cmpSlc(rw, SLC("")));
  TASSERT_SLC_EQ("Hello ", *sw);
  File f = BufFile_asFile(&fw);
  File_extend(f, SLC("World!")); File_flush(f);
  TASSERT_SLC_EQ("Hello World!", *sw);

END_TEST

TEST(fileRead)
  UFile f = UFile_malloc(20);
  Ring* r = &f.ring;
  UFile_open(&f, SLC("data/UFile_test.txt"), File_RDONLY);
  TASSERT_EQ(File_DONE, f.code);
  TASSERT_EQ(0, Ring_len(&f.ring));
  TASSERT_EQ(0, f.ring.head);

  UFile_readAll(&f);
  TASSERT_EQ(19, Ring_len(r));
  assert(f.code == File_DONE);
  assert(0 == memcmp(r->dat, "easy to test text\nw", 19));

  Ring_clear(r); UFile_readAll(&f);
  assert(Ring_len(r) == 19); assert(f.code == File_DONE);
  assert(0 == memcmp(r->dat, "riting a simple hai", 19));

  // Now use it like a parser. Inc part of the ring.
  Ring_incHead(r, 16); UFile_readAll(&f);
  assert(Ring_len(r) == 19); assert(f.code == File_DONE);
  assert(0 == Ring_cmpSlc(r, SLC("haiku\nand the job i")));

  // Again, inc part of the ring.
  Ring_incHead(r, 18); UFile_readAll(&f);
  TASSERT_EQ(9, Ring_len(r));
  TASSERT_EQ(f.code, File_EOF);
  assert(0 == Ring_cmpSlc(r, SLC("is done\n\n")));
  UFile_close(&f);
  free(r->dat);
END_TEST

TEST(fileWrite)
  UFile f = UFile_malloc(20);
  Ring* r = &f.ring;
  Slc path = SLC("bin/UFile_test.txt");
  UFile_open(&f, path, File_WRONLY | File_CREATE | File_TRUNC);
  TASSERT_EQ(File_DONE, f.code);
  Ring_extend(r, SLC("hello there! My "));
  UFile_write(&f);
  TASSERT_EQ(true, Ring_isEmpty(r)); TASSERT_EQ(File_DONE, f.code);
  UFile_stop(&f);

  Ring_clear(r);
  Ring_extend(r, SLC("name is Joe!"));
  UFile_write(&f);
  TASSERT_EQ(File_DONE, f.code);
  TASSERT_EQ(true, Ring_isEmpty(r));

  UFile_close(&f); Ring_clear(r);
  UFile_open(&f, path, File_RDONLY); TASSERT_EQ(f.code, File_DONE);
  UFile_read(&f); TASSERT_EQ(f.code, File_DONE);
  TASSERT_EQ(0, Ring_cmpSlc(r, SLC("hello there! My nam")));
  UFile_close(&f);
  EXPECT_ERR(UFile_write(&f), "operation out of order");
END_TEST

TEST_UNIX(log, 5)
  BBA bba = {.ba = &civ.ba}; Arena a = BBA_asArena(&bba);
  BufFile_var(f, 15, 256);
  FileLogger* fl = FileLogger_new(a, BufFile_asFile(&f));
  FileLogger_start(fl, LOG_INFO);
  FileLogger_end(fl);
  TASSERT_SLC_EQ("[INFO] \n", *PlcBuf_asSlc(&f.b));

  Buf_clear(PlcBuf_asBuf(&f.b));
  Logger l = FileLogger_asLogger(fl);
  #define MSG "the ooobloo is in the poodloo"
  Xr(l,start, LOG_WARN); Xr(l,add, SLC(MSG)); Xr(l,end);
  TASSERT_SLC_EQ("[WARN] " MSG "\n", *PlcBuf_asSlc(&f.b));
  #undef MSG

  UFile_extend(&civUnix.logFile, SLC("* civUnix.logFile write works\n"));
  File_extend(civ.logFile,       SLC("* civ.logFile     write works\n"));
  File_flush(civ.logFile);
  assert(Xr(civ.log,start, LOG_INFO));
  Xr(civ.log,add, SLC("log.add"));
  Writer_extend(Logger_asWriter(civ.log), SLC(" and log.asWriter works"));
  Xr(civ.log,end);

END_TEST

int main(int argc, char *argv[]) {
  ARGV = argv;
  SETUP_SIG((void *)defaultHandleSig);

  eprintf("# Starting Tests\n");
  test_basic();
  test_slc();
  test_buf();
  test_plcBuf();
  test_stk();
  test_ring();
  test_sll();
  test_dll();
  test_bst();
  test_ba();
  test_bba();
  test_CStr();
  test_bufFile();
  test_fileRead();
  test_fileWrite();
  test_log();
  eprintf("# Tests All Pass\n");
  return 0;
}

