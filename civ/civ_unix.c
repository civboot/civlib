#include <unistd.h> // read, write, lseek

#include "./civ_unix.h"

/*extern*/ CivUnix civUnix          = (CivUnix) {};

#include <execinfo.h>

#define BT_SIZE   128
void printTrace() {
  void*    array[BT_SIZE];
  int      size    = backtrace(array, BT_SIZE);
  char**   strings =  backtrace_symbols(array, size);
  assert(strings); // No debug symbols found
  for(int i = 2; i < size; i++) printf("  %s\n", strings[i]);
  free(strings);
}

void defaultErrPrinter() {
  eprintf("!! Encountered error. Stack trace:\n");
  printTrace();
  eprintf("!! Error: %.*s\n", civ.fb->err.len, civ.fb->err.dat);
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

void CivUnix_init(Slot numBlocks) {
  CivUnix_allocBlocks(numBlocks);
}


void CivUnix_drop() {
  for(Dll* dll; (dll = DllRoot_pop(&civUnix.mallocs));) free(dll->dat);
  assert(NULL == civUnix.mallocs.start);
  civ.ba = (BA) {0};
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

DEFINE_METHOD(void, UFile,drop, Arena a) {
  if(this->code != File_CLOSED) UFile_close(this);
  Xr(a, free, this->ring.dat, this->ring._cap, 1);
}

DEFINE_METHOD(Sll*, UFile,resourceLL) {
  return (Sll*)&this->nextResource;
}

DEFINE_METHOD(BaseFile*, UFile,asBase) { return (BaseFile*) this; }

DEFINE_METHOD(void, UFile,open, Slc path, Slot options) {
  assert(this->code == File_CLOSED);
  assert(path.len < 255);
  uint8_t pathname[256];
  memcpy(pathname, path.dat, path.len);
  pathname[path.len] = 0;
  int fd = UFile_handleErr(this, open(pathname, O_NONBLOCK | options, 0666));
  if(fd < 0) return;
  this->fid = fd;
  this->ring.head = 0; this->ring.tail = 0; this->code = File_DONE;
}

DEFINE_METHOD(void, UFile,close) {
  assert(this->code >= File_DONE);
  if(close(this->fid)) this->code = File_ERROR;
  else                 this->code = File_CLOSED;
}

DEFINE_METHOD(void, UFile,stop) {
  fsync(this->fid);
  this->code = File_DONE;
}

DEFINE_METHOD(void, UFile,seek, ISlot offset, U1 whence) {
  assert(this->code >= File_DONE);
  UFile_handleErr(this, lseek(this->fid, offset, whence));
}

DEFINE_METHOD(void, UFile,read) {
  assert(this->code == File_READING || this->code >= File_DONE);
  int len = 0;
  Ring* r = &this->ring;
  this->code = File_READING;
  Slc avail = Ring_avail(r);
  if(avail.len) {
    len = read(this->fid, avail.dat, avail.len);
    len = UFile_handleErr(this, len);
    Ring_incTail(r, len);
  }
  if(Ring_isFull(r)) { this->code = File_DONE; }
  else if (0 == len) { this->code = File_EOF;  }
}

DEFINE_METHOD(void, UFile,write) {
  assert(this->code == File_WRITING || this->code >= File_DONE);
  this->code = File_WRITING;
  Ring* r = &this->ring;
  Slc first = Ring_1st(r);
  this->code = File_WRITING;
  int len = write(this->fid, first.dat, first.len);
  len     = UFile_handleErr(this, len);
  Ring_incHead(r, len);
  if(Ring_isEmpty(r)) this->code = File_DONE;
}

// Read until the buffer is full or EOF.
// If the file-code is a DONE code then also clear the ring.
void UFile_readAll(UFile* f) {
  do { UFile_read(f); } while (f->code < File_DONE);
}

DEFINE_METHODS(MFile, UFile_mFile,
  .drop       = M_UFile_drop,
  .resourceLL = M_UFile_resourceLL,
  .asBase     = M_UFile_asBase,
  .open       = M_UFile_open,
  .close      = M_UFile_close,
  .stop       = M_UFile_stop,
  .seek       = M_UFile_seek,
  .read       = M_UFile_read,
  .write      = M_UFile_write,
)

File UFile_asFile(UFile* d) {
  return (File) { .m = UFile_mFile(), .d = d };
}
