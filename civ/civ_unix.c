#include <unistd.h> // read, write, lseek
#include <fcntl.h>  // creat, open

#include "./civ_unix.h"

#define UFile_FD(F)      ((~File_INDEX) & (F).fid)

void civErrPrinter() { eprintf("!! Error #%X\n", civ.civErr); }

void initCivUnix(BANode* nodes, Block* blocks, U1 numBlocks) {
  civ.ba = (BA) {
    .nodes = nodes, .blocks = blocks,
    .rooti  = BLOCK_END, .cap = numBlocks,
  };
  civ.civErrPrinter = civErrPrinter;
  civ.civErr = 0;
}

// #################################
// # File
File UFile_malloc(U4 bufCap) {
  return (File) {
    .buf = (PlcBuf) { .dat = malloc(bufCap), .cap = bufCap },
    .code = File_DONE,
  };
}

// Re-use a file object
void File_clear(File* f) {
  *f = (File) {.buf = (PlcBuf) { .dat = f->buf.dat, .cap = f->buf.cap } };
}

void UFile_drop(File* f) { free(f->buf.dat); }

int UFile_handleErr(File* f, int res) {
  if(errno == EWOULDBLOCK) { errno = 0; return res; }
  if(res < 0) { f->code = File_EIO; }
  return res;
}

void UFile_open(File* f, Slc path) {
  assert(path.len < 255);
  uint8_t pathname[256];
  memcpy(pathname, path.dat, path.len);
  pathname[path.len] = 0;
  int fd = UFile_handleErr(f, open(pathname, O_NONBLOCK, O_RDWR));
  if(fd < 0) return;
  f->pos = 0; f->fid = File_INDEX | fd;
  f->buf.len = 0; f->buf.plc = 0; f->code = File_DONE;
}

void UFile_close(File* f) {
  assert(f->code >= File_DONE);
  if(close(UFile_FD(*f))) f->code = File_ERROR;
  else                    f->code = File_CLOSED;
}

void UFile_stop(File* f) { f->code = File_STOPPED; }
void UFile_seek(File* f, long int offset, U1 whence) {
  assert(f->code == File_READING || f->code >= File_DONE);
  UFile_handleErr(f, lseek(UFile_FD(*f), offset, whence));
}

void UFile_read(File* f) {
  assert(f->code == File_READING || f->code >= File_DONE);
  int len;
  if(!(File_INDEX & f->fid)) { // mocked file.
    PlcBuf* p = (PlcBuf*) f->fid;
    len = minU4(p->len - p->plc, f->buf.cap - f->buf.len);
    memmove(f->buf.dat, p->dat + p->plc, len); p->plc += len;
  } else {
    f->code = File_READING;
    len = read(UFile_FD(*f), f->buf.dat + f->buf.len, f->buf.cap - f->buf.len);
    len = UFile_handleErr(f, len);  assert(len >= 0);
  }
  f->buf.len += len; f->pos += len;
  if(f->buf.len == f->buf.cap) f->code = File_DONE;
  else if (0 == len)           f->code = File_EOF;
}

// Read until the buffer is full or EOF.
// If the file-code is a DONE code then also clear the buf.len
void UFile_readAll(File* f) {
  if(f->code >= File_DONE) f->buf.len = 0;
  do { UFile_read(f); } while (f->code < File_DONE);
}

M_File M_UFile = (M_File) {
  .open  = Role_METHOD(UFile_open, Slc path),
  .close = Role_METHOD(UFile_close),
  .stop = Role_METHOD(UFile_stop),
  .seek = Role_METHOD(UFile_seek, long int, U1),
  .clear = NULL,
  .read = Role_METHOD(UFile_read),
  .insert = NULL, // not supported
};

