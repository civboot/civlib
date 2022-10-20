#ifndef __CIV_UNIX_H
#define __CIV_UNIX_H

#include "./civ.h"

#define UFile_FD(F)      ((~File_INDEX) & (F).fid)

int UFile_handleErr(File* f, int res);
void UFile_open(File* f, CSlc s);
void UFile_close(File* f);
void UFile_read(File* f);
extern M_File M_UFile;

#endif // __CIV_UNIX_H
