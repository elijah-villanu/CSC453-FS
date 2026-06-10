#include "tinyFS.h"
#include "libTinyFS.h"
#include "libDisk.h"
#include "tinyFS_errno.h"
#include <string.h>
#include <stdlib.h>

static OpenFileEntry openFileTable[MAX_FILES];
// static int fileCounter = 0; Temp comment out for warning
static int currentMount = -1;

/* HELPERS */
// Scans bitmap in superblock and returns block number of first free block
static int findFreeBlock() {
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;

    int numBlocks = superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];
    
    // Iterate through bitmap from offset for free blocks
    for (int byte = 0; byte < (numBlocks + 7) / 8; byte++) {
        unsigned char bitmapByte = superblock[SUPERBLOCK_BITMAP_OFFSET + byte];
        for (int bit = 7; bit >= 0; bit--) {
            int blockNum = (byte * 8) + (7 - bit);
            // Make sure we don't go past actual disk size
            if (blockNum >= numBlocks) return ERR_NO_FREE_BLOCKS;
            if (bitmapByte & (1 << bit)) return blockNum;
        }
    }
    // None found
    return ERR_NO_FREE_BLOCKS;
}

// Sets block in use or free in superblock bitmap
static int setBitmapBit(int blockNum, int isFree) {
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;

    int numBlocks = superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];
    
    // Validate block number
    if (blockNum < 0 || blockNum >= numBlocks) return ERR_BLOCK_OUT_OF_RANGE;

    // Position to correct bit in bitmap
    int byteIndex = blockNum / 8;
    int bitIndex  = 7 - (blockNum % 8);
    
    // Set correct bits
    if (isFree) {
        superblock[SUPERBLOCK_BITMAP_OFFSET + byteIndex] |=  (1 << bitIndex); // set bit
    } else {
        superblock[SUPERBLOCK_BITMAP_OFFSET + byteIndex] &= ~(1 << bitIndex); // clear bit
    }

    // Update superblock with new bitmap
    if (writeBlock(currentMount, 0, superblock) < 0) return ERR_BLOCK_WRITE;
    return 0;
}

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
        if(writeBlock(diskNum, i, initBlock) < 0) return ERR_BLOCK_WRITE;
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
    
    // Mark block 0 (superblock) as used in bitmap
    superblock[SUPERBLOCK_BITMAP_OFFSET] &= ~(1 << 7); // clear bit 7 of first byte
    // Store total block count in superblock
    superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET] = (char)numBlocks;
    
    if (writeBlock(diskNum, 0, superblock) < 0) return ERR_BLOCK_WRITE;

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
    return TFS_SUCCESS;
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


fileDescriptor tfs_openFile(char *name) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    
    // Filename validation
    if (name == NULL) return ERR_INVALID_NAME;
    int nameLen = strlen(name);
    if (nameLen < 1 || nameLen > 8) return ERR_INVALID_NAME;
    
    // TODO: Filename check for alphanumeric

    // If file is already open, return existing fd
    for (int i = 0; i < MAX_FILES; i++) {
        if (openFileTable[i].inUse && strcmp(openFileTable[i].name, name) == 0) {
            return i;
        }
    }

    // Find free slot in open file table
    int fd = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (!openFileTable[i].inUse) {
            fd = i; 
            break;
        }
    }

    if (fd == -1) return ERR_FILE_TABLE_FULL;

    // Read superblock to get disk size from bitmap
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_NO_FREE_BLOCKS;
    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

    // Scan all blocks for existing inode
    char block[BLOCKSIZE];
    int inodeBlock = -1;
    
    for (int i = 1; i < numBlocks; i++) {
        if (readBlock(currentMount, i, block) < 0) continue;
        if (block[BLOCK_TYPE_OFFSET] != INODE) continue;
        if (block[MAGIC_NUM_OFFSET]  != MAGIC_NUMBER) continue;
        // Compare inode name
        if (strcmp(&block[INODE_NAME_OFFSET], name) == 0) {
            inodeBlock = i;
            break;
        }
    }

    // File doesn't exist (no inode found) create file
    if (inodeBlock == -1) {
        inodeBlock = findFreeBlock();
        if (inodeBlock < 0) return ERR_NO_FREE_BLOCKS;
        
        // Build inode block
        char newInode[BLOCKSIZE];
        memset(newInode, 0x00, BLOCKSIZE);
        newInode[BLOCK_TYPE_OFFSET] = INODE;
        newInode[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
        newInode[INODE_FIRST_EXTENT] = 0; // no extents yet
        strncpy(&newInode[INODE_NAME_OFFSET], name, 8);
        newInode[INODE_SIZE_OFFSET]  = 0; // empty file

        // Write inode to disk
        if (writeBlock(currentMount, inodeBlock, newInode) < 0) return ERR_BLOCK_WRITE;
        // Mark block as used in bitmap
        if (setBitmapBit(inodeBlock, 0) < 0) return ERR_BLOCK_WRITE;

    }
    
    // Update open file table
    openFileTable[fd].inUse = 1;
    openFileTable[fd].inodeBlock = inodeBlock;
    openFileTable[fd].filePointer = 0;
    strncpy(openFileTable[fd].name, name, 8);
    // Null terminate filename
    openFileTable[fd].name[8] = '\0';

    return fd;
}


int tfs_closeFile(fileDescriptor FD) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Making sure file descriptor in correct range
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;

    // Clearing entry of provided file descriptor
    openFileTable[FD].inUse = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = 0;
    return TFS_SUCCESS;
}


int tfs_writeFile(fileDescriptor FD,char *buffer, int size) {
     // Making sure a file system exists first
     if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;

     // TODO: implement
     return -1;
}


int tfs_deleteFile(fileDescriptor FD) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Making sure file descriptor in correct range
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;

    // Read inode block
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;   

    // Get first file extent block 
    int extentBlock = (unsigned char)inode[INODE_FIRST_EXTENT];
    // Iterate through all file extents
    while (extentBlock != 0) {
        char extent[BLOCKSIZE];
        if (readBlock(currentMount, extentBlock, extent) < 0) return ERR_DISK_READ;

        int nextExtent = (unsigned char)extent[FILE_EXTENT_NEXT];
        
        // Update bitmap
        if (setBitmapBit(extentBlock, 1) < 0) return ERR_BLOCK_WRITE;

        // Update disk as free block
        char freeBlock[BLOCKSIZE];
        memset(freeBlock, 0x00, BLOCKSIZE);
        freeBlock[BLOCK_TYPE_OFFSET] = FREE_BLOCK;
        freeBlock[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
        if (writeBlock(currentMount, extentBlock, freeBlock) < 0) return ERR_BLOCK_WRITE;

        extentBlock = nextExtent;
    }

    // Free inode itself
    if (setBitmapBit(inodeBlock, 1) < 0) return ERR_BLOCK_WRITE;
    char freeBlock[BLOCKSIZE];
    memset(freeBlock, 0x00, BLOCKSIZE);
    freeBlock[BLOCK_TYPE_OFFSET] = FREE_BLOCK;
    freeBlock[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
    if (writeBlock(currentMount, inodeBlock, freeBlock) < 0) return ERR_BLOCK_WRITE;

    // Update open file table
    openFileTable[FD].inUse = 0;
    openFileTable[FD].inodeBlock = -1;
    openFileTable[FD].filePointer = 0;   
    openFileTable[FD].name[0] = '\0';
    
    return TFS_SUCCESS;
}


int tfs_readByte(fileDescriptor FD, char *buffer) {
    // TODO: implement
    return -1;
}


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

