#ifndef LIBTINYFS_H
#define LIBTINYFS_H

#include "tinyFS.h"

// Magic number for our tinyFS
#define MAGIC_NUMBER 0x44

// Block types and their codes
#define SUPERBLOCK 1
#define INODE 2
#define FILE_EXTENT 3
#define FREE_BLOCK 4

// General block storage byte contents
#define BLOCK_TYPE_OFFSET 0 // block type = 1|2|3|4
#define MAGIC_NUM_OFFSET 1 // 0x44
#define NEXT_BLOCK_PTR_OFFSET 2 // Pointer to address of another block
#define EMPTY_BYTE_OFFSET 3 // This will be empty
#define DATA_START_OFFSET 4 // Start of data, following bytes also data

// Superblock specific layout
#define SUPERBLOCK_ROOT_INODE_OFFSET 4 // Block number of root inode
#define SUPERBLOCK_BITMAP_OFFSET 5 // Start of the free block bitmap

// Inode specific layout
#define INODE_FIRST_EXTENT 2 // First extent block num
#define INODE_NAME_OFFSET 4 // Start of name, up to 9 total bytes
#define INODE_SIZE_OFFSET 13 // File size

// File extent specific layout
#define FILE_EXTENT_NEXT 2 // Block num of next extent (set to zero if last)
#define EXTENT_DATA_OFFSET 4 // Start of file extent data
#define EXTENT_DATA_SIZE (BLOCKSIZE - DATA_START_OFFSET) // Data bytes available after default bytes set (252)

// Representing an entry in open file table
typedef struct {
    char name[9]; // max 8 chars + null term
    int  inUse;
    int  inodeBlock;
    int  filePointer;
} OpenFileEntry;

#endif
