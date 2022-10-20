#ifndef __CIV_UNIX_H
#define __CIV_UNIX_H

#include "./civ.h"

#define UFile_FD(F)      ((~File_INDEX) & (F).fid)

#define TEST_UNIX(NAME, numBlocks)  TEST(NAME)  \
    Block* __blocks = malloc(numBlocks << BLOCK_PO2); \
    initCivUnix(&((BANode*)__blocks)[1], &__blocks[1], numBlocks - 1);

#define END_TEST_UNIX \
    free(__blocks); END_TEST

void initCivUnix(BANode* nodes, Block* blocks, U1 numBlocks);

void UFile_drop(File* f);
File UFile_malloc(U4 bufCap);
void UFile_readAll(File* f);

int UFile_handleErr(File* f, int res);
void UFile_open(File* f, Slc s);
void UFile_close(File* f);
void UFile_read(File* f);
extern M_File M_UFile;

#endif // __CIV_UNIX_H
