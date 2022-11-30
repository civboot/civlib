#include <unistd.h> // read, write, lseek

#include "./civ_unix.h"

/*extern*/ CivUnix civUnix          = (CivUnix) {};

#define File_FD(F)      ((~File_INDEX) & (F).fid)

void CivUnix_init(Slot numBlocks) {
  CivUnix_allocBlocks(numBlocks);
  civ.fb->err.len = 0;
}


void CivUnix_drop() {
  for(Dll* dll; (dll = DllRoot_pop(&civUnix.mallocs));) free(dll->dat);
}

void CivUnix_allocBlocks(Slot numBlocks) {
  void* mem = malloc(numBlocks * (BLOCK_SIZE + sizeof(BANode) + sizeof(Dll)));
  Block*  blocks = (Block*)mem;
  BANode* nodes  = (BANode*)(blocks + numBlocks);
  assert((Slot) nodes == (Slot)mem + (BLOCK_SIZE * numBlocks));
  BA_freeArray(&civ.ba, numBlocks, nodes, blocks);

  Dll* mallocDll = (Dll*)(nodes + numBlocks);
  mallocDll->dat = mem;
  DllRoot_add(&civUnix.mallocs, mallocDll);
}

// #################################
// # Core Types and common methods


bool CStr_varAssert(U4 line, U1* STR, U1* LEN) {
  if(1 != strlen(LEN)) {
    eprintf("ERROR CStr_var [line=%u]: LEN must be single byte (line=%u)");
    return false;
  }
  if(LEN[0] != strlen(STR)) {
    eprintf("ERROR CStr_var [line=%u]: Use LEN = \"\\x%.2X\"\n", line, strlen(STR));
    return false;
  }
  return true;
}

// #################################
// # File

// Should only be used in tests
UFile UFile_malloc(U4 bufCap) {
  return (UFile) {
    .ring = (Ring) { .dat = malloc(bufCap), ._cap = bufCap },
    .code = File_CLOSED,
  };
}

UFile UFile_new(Ring ring) {
  return (UFile) {
    .ring = ring,
    .code = File_CLOSED,
  };
}

int UFile_handleErr(UFile* f, int res) {
  if(errno == EWOULDBLOCK) { errno = 0; return res; }
  if(res < 0) { eprintf("?? error: %X\n", -res); f->code = File_EIO; return 0; }
  return res;
}

DEFINE_METHOD(bool, UFile,drop, Arena a) {
  if(this->code != File_CLOSED) UFile_close(this);
  if(this->code != File_CLOSED) return false;
  Xr(a, free, this->ring.dat, this->ring._cap, 1);
  return true;
}

DEFINE_METHOD(Sll*, UFile,resourceLL) {
  return (Sll*)this;
}

DEFINE_METHOD(void, UFile,open, Slc path, Slot options) {
  assert(this->code == File_CLOSED);
  assert(path.len < 255);
  uint8_t pathname[256];
  memcpy(pathname, path.dat, path.len);
  pathname[path.len] = 0;
  int fd = UFile_handleErr(this, open(pathname, O_NONBLOCK | options, 0666));
  if(fd < 0) return;
  this->pos = 0; this->fid = File_INDEX | fd;
  this->ring.head = 0; this->ring.tail = 0; this->code = File_DONE;
}

DEFINE_METHOD(void, UFile,close) {
  assert(this->code >= File_DONE);
  if(close(File_FD(*this))) this->code = File_ERROR;
  else                      this->code = File_CLOSED;
}

DEFINE_METHOD(void, UFile,stop) {
  fsync(File_FD(*this));
  this->code = File_DONE;
}

DEFINE_METHOD(void, UFile,seek, ISlot offset, U1 whence) {
  assert(this->code >= File_DONE);
  // TODO: handle mocked file.
  UFile_handleErr(this, lseek(File_FD(*this), offset, whence));
}

DEFINE_METHOD(void, UFile,read) {
  assert(this->code == File_READING || this->code >= File_DONE);
  int len = 0;
  Ring* r = &this->ring;
  if(!(File_INDEX & this->fid)) { // mocked file.
    PlcBuf* p = (PlcBuf*) this->fid;
    len = U4_min(p->len - p->plc, Ring_cap(*r) - Ring_len(r));
    Ring_extend(r, (Slc){p->dat, len});
    p->plc += len;
  } else {
    this->code = File_READING;
    Slc avail = Ring_avail(r);
    if(avail.len) {
      len = read(File_FD(*this), avail.dat, avail.len);
      len = UFile_handleErr(this, len);  assert(len >= 0);
      Ring_incTail(r, len);
    }
  }
  this->pos += len;
  if(Ring_len(r) == Ring_cap(*r)) this->code = File_DONE;
  else if (0 == len)              this->code = File_EOF;
}

DEFINE_METHOD(void, UFile,write) {
  assert(this->code == File_WRITING || this->code >= File_DONE);
  this->code = File_WRITING;
  Ring* r = &this->ring;
  Slc first = Ring_first(r);
  int len;
  if(!(File_INDEX & this->fid)) { // mocked file.
    PlcBuf* p = (PlcBuf*) this->fid;
    len = U4_min(p->cap - p->len, first.len);
    Buf_extend(PlcBuf_asBuf(p), (Slc){first.dat, len});
  } else {
    this->code = File_WRITING;
    len = write(File_FD(*this), first.dat, first.len);
    len = UFile_handleErr(this, len);  assert(len >= 0);
  }
  if(len == Ring_len(r)) this->code = File_DONE;
  Ring_incHead(r, len);
}

// Read until the buffer is full or EOF.
// If the file-code is a DONE code then also clear the ring.
void UFile_readAll(UFile* f) {
  do { UFile_read(f); } while (f->code < File_DONE);
}

DEFINE_METHODS(MFile, UFile_mFile,
  .drop      = M_UFile_drop,
  .resourceLL = M_UFile_resourceLL,
  .open      = M_UFile_open,
  .close     = M_UFile_close,
  .stop      = M_UFile_stop,
  .seek      = M_UFile_seek,
  .read      = M_UFile_read,
  .write     = M_UFile_write,
)

File File_asRFile(UFile* d) { return (File) { .m = UFile_mFile(), .d = d }; }
