#include "tinyFS.h"
#include "libTinyFS.h"
#include "libDisk.h"
#include "tinyFS_errno.h"
#include <string.h>
#include <stdlib.h>

static OpenFileEntry openFileTable[MAX_FILES];
static int fileCounter = 0;
static int currentMount = -1;

int tfs_mkfs(char *filename, int nBytes) {
    // Create File System and track disk
    int diskNum = openDisk(filename, nBytes);

    if (diskNum < 0) {
        return ERR_DISK_OPEN;
    }

    // Int division always floors
    int numBlocks = (int)nBytes / BLOCKSIZE;

    // Initialize blocks as 0x00
    char initBlock[BLOCKSIZE];
    memset(initBlock, 0x00, BLOCKSIZE);
    for (int i = 0; i < numBlocks; i++) {
        if(writeBlock(diskNum, i, initBlock) < 0) return -1;
    }

    // Initialize superblock
    char superblock[BLOCKSIZE];
    memset(superblock, 0x00, BLOCKSIZE);
    superblock[BLOCK_TYPE_OFFSET] = SUPERBLOCK;
    superblock[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
    superblock[NEXT_BLOCK_PTR_OFFSET] = 0; //unsure
    superblock[SUPERBLOCK_ROOT_INODE_OFFSET] = 0;

    // Blocks tracked as bitmap
    int bitmapSize = (numBlocks + 7) / 8;
    for (int i = 0; i < bitmapSize; i++) {
        superblock[SUPERBLOCK_BITMAP_OFFSET + i] = 0xFF;
    }
    
    // FROM CLAUDE
    // Mark block 0 (superblock) as used in bitmap
    superblock[SUPERBLOCK_BITMAP_OFFSET] &= ~(1 << 7); // clear bit 7 of first byte
    
    if (writeBlock(diskNum, 0, superblock) < 0) return -1;

    // Initialize rest of free blocks
    char freeBlock[BLOCKSIZE];
    memset(freeBlock, 0x00, BLOCKSIZE);
    freeBlock[BLOCK_TYPE_OFFSET] = FREE_BLOCK;
    freeBlock[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
    // Skip superblock
    for (int i = 1; i < numBlocks; i++) {
        if (writeBlock(diskNum, i, freeBlock) < 0) return ERR_BLOCK_WRITE;
    }

    closeDisk(diskNum);
    return 0;
}


int tfs_mount(char *diskname) {
    // Check if any disk is mounted
    if (currentMount != -1) return ERR_DISK_ALREADY_MOUNTED;    
    
    // Open existing disk
    int diskNum = openDisk(diskname, 0);
    if (diskNum < 0) return ERR_DISK_NOT_MOUNTED;

    // Verify correct file type via magic num
    char block[BLOCKSIZE];
    if (readBlock(diskNum, 0, block) < 0) {
        return ERR_DISK_READ;
    }
    if (block[BLOCK_TYPE_OFFSET] != SUPERBLOCK) { 
        return ERR_INVALID_FS;
    }
    if (block[MAGIC_NUM_OFFSET] != MAGIC_NUMBER) {
        return ERR_INVALID_MAGIC_NUM;
    }

    // Set as new currently mounted disk
    currentMount = diskNum;
    return 0;
}


int tfs_unmount(void) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;

    // Close open files
    for (int i = 0; i < MAX_FILES; i++) {
        if (openFileTable[i].inUse) {
            openFileTable[i].inUse = 0;
            openFileTable[i].inodeBlock = -1;
            openFileTable[i].filePointer = 0;
        }
    }

    // Update mount state
    closeDisk(currentMount);
    currentMount = -1;
    return 0;
}


fileDescriptor tfs_openFile(char *name);


int tfs_closeFile(fileDescriptor FD) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Making sure file descriptor in correct range
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;

    // TODO: May need to also free anything from openFile (if we end up doing any malloc)

    // Clearing entry of provided file descriptor
    openFileTable[FD].inUse = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = 0;
    return TFS_SUCCESS;
}


int tfs_writeFile(fileDescriptor FD,char *buffer, int size);


int tfs_deleteFile(fileDescriptor FD);


int tfs_readByte(fileDescriptor FD, char *buffer);


int tfs_seek(fileDescriptor FD, int offset) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Making sure file descriptor in correct range
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;
    // Offset can't be negative 
    if (offset < 0) return ERR_EOF;

    // Set file pointer to provided offset
    openFileTable[FD].filePointer = offset;
    return TFS_SUCCESS;
}

