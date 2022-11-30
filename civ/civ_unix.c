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
File File_malloc(U4 bufCap) {
  return (File) {
    .ring = (Ring) { .dat = malloc(bufCap), ._cap = bufCap },
    .code = File_CLOSED,
  };
}

File File_new(Ring ring) {
  return (File) {
    .ring = ring,
    .code = File_CLOSED,
  };
}

int File_handleErr(File* f, int res) {
  if(errno == EWOULDBLOCK) { errno = 0; return res; }
  if(res < 0) { eprintf("?? error: %X\n", -res); f->code = File_EIO; return 0; }
  return res;
}

void File_open(File* f, Slc path, Slot options) {
  assert(f->code == File_CLOSED);
  assert(path.len < 255);
  uint8_t pathname[256];
  memcpy(pathname, path.dat, path.len);
  pathname[path.len] = 0;
  int fd = File_handleErr(f, open(pathname, O_NONBLOCK | options, 0666));
  if(fd < 0) return;
  f->pos = 0; f->fid = File_INDEX | fd;
  f->ring.head = 0; f->ring.tail = 0; f->code = File_DONE;
}

void File_close(File* f) {
  assert(f->code >= File_DONE);
  if(close(File_FD(*f))) f->code = File_ERROR;
  else                   f->code = File_CLOSED;
}

bool File_drop(File* f) {
  if(f->code != File_CLOSED) File_close(f);
  if(f->code != File_CLOSED) return false;
  Xr(*civ.fb->arena, free, f->ring.dat, f->ring._cap, 1);
  return true;
}

void File_stop(File* f) {
  fsync(File_FD(*f));
  f->code = File_DONE;
}

void File_seek(File* f, ISlot offset, U1 whence) {
  assert(f->code >= File_DONE);
  // TODO: handle mocked file.
  File_handleErr(f, lseek(File_FD(*f), offset, whence));
}

void File_read(File* f) {
  assert(f->code == File_READING || f->code >= File_DONE);
  int len = 0;
  Ring* r = &f->ring;
  if(!(File_INDEX & f->fid)) { // mocked file.
    PlcBuf* p = (PlcBuf*) f->fid;
    len = U4_min(p->len - p->plc, Ring_cap(*r) - Ring_len(r));
    Ring_extend(r, (Slc){p->dat, len});
    p->plc += len;
  } else {
    f->code = File_READING;
    Slc avail = Ring_avail(r);
    if(avail.len) {
      len = read(File_FD(*f), avail.dat, avail.len);
      len = File_handleErr(f, len);  assert(len >= 0);
      Ring_incTail(r, len);
    }
  }
  f->pos += len;
  if(Ring_len(r) == Ring_cap(*r)) f->code = File_DONE;
  else if (0 == len)              f->code = File_EOF;
}

void File_write(File* f) {
  assert(f->code == File_WRITING || f->code >= File_DONE);
  f->code = File_WRITING;
  Ring* r = &f->ring;
  Slc first = Ring_first(r);
  int len;
  if(!(File_INDEX & f->fid)) { // mocked file.
    PlcBuf* p = (PlcBuf*) f->fid;
    len = U4_min(p->cap - p->len, first.len);
    Buf_extend(PlcBuf_asBuf(p), (Slc){first.dat, len});
  } else {
    f->code = File_WRITING;
    len = write(File_FD(*f), first.dat, first.len);
    len = File_handleErr(f, len);  assert(len >= 0);
  }
  if(len == Ring_len(r)) f->code = File_DONE;
  Ring_incHead(r, len);
}

// Read until the buffer is full or EOF.
// If the file-code is a DONE code then also clear the ring.
void File_readAll(File* f) {
  do { File_read(f); } while (f->code < File_DONE);
}

/*extern*/ const MFile mFile = (MFile) {
  .drop = &File_drop,
  .open  = &File_open,
  .close = &File_close,
  .stop =  &File_stop,
  .seek =  &File_seek,
  .read =  &File_read,
  .write = &File_write,
};

RFile File_asRFile(File* d) { return (RFile) { .m = &mFile, .d = d }; }
