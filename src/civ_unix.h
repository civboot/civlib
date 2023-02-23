#ifndef __CIV_UNIX_H
#define __CIV_UNIX_H

#include <fcntl.h>  // create, open
#include <execinfo.h>
#include <signal.h>
#include "civ.h"

#define TEST_UNIX(NAME, numBlocks) \
  TEST(NAME)                       \
  CivUnix_init(numBlocks);


#define END_TEST_UNIX \
    CivUnix_drop(); END_TEST

#define SETUP_SIG(HANDLE_SIG)  \
  struct sigaction _sa;        \
  _sa.sa_handler = HANDLE_SIG; \
  sigemptyset(&_sa.sa_mask);   \
  _sa.sa_flags = SA_RESTART;   \
  sigaction(SIGSEGV, &_sa, NULL); \
  sigaction(SIGFPE, &_sa, NULL);

  /* ... add your own signals */


typedef struct {
  DllRoot mallocs;
} CivUnix;

Trace Trace_newSig(int cap, int sig, struct sigcontext* ctx);
void  Trace_handleSig(int sig, struct sigcontext* ctx);
void defaultHandleSig(int sig, struct sigcontext ctx);

extern CivUnix civUnix;;

void CivUnix_init(S numBlocks);
void CivUnix_drop();
void CivUnix_allocBlocks(S numBlocks);

// File
typedef struct {
  Ring      ring;     // buffer for reading or writing data
  U2        code;     // status or error (File_*)
  Sll*      nextResource; // resource SLL
  S         fid;      // file id or reference
} UFile;

#define File_RDWR      O_RDWR
#define File_RDONLY    O_RDONLY
#define File_WRONLY    O_WRONLY
#define File_TRUNC     O_TRUNC
#define File_CREATE    O_CREAT

MFile* UFile_mFile();
UFile UFile_malloc(U4 bufCap); // only use in tests
UFile UFile_new(Ring ring);
void  UFile_readAll(UFile* f);

int UFile_handleErr(UFile* f, int res);
DECLARE_METHOD(void,      UFile,drop, Arena a);
DECLARE_METHOD(Sll*,      UFile,resourceLL);
DECLARE_METHOD(BaseFile*, UFile,asBase);
DECLARE_METHOD(void,      UFile,open, Slc path, S options);
DECLARE_METHOD(void,      UFile,close);
DECLARE_METHOD(void,      UFile,stop);
DECLARE_METHOD(void,      UFile,seek, ISlot offset, U1 whence);
DECLARE_METHOD(void,      UFile,read);
DECLARE_METHOD(void,      UFile,write);
File UFile_asFile(UFile* d);

#endif // __CIV_UNIX_H
