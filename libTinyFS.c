#include "tinyFS.h"
#include "libTinyFS.h"
#include "libDisk.h"
#include "tinyFS_errno.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static OpenFileEntry openFileTable[MAX_FILES];
static int currentMount = -1;

/* HELPERS */
// Scans bitmap in superblock and returns block number of first free block
static int findFreeBlock() {
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;

    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

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

    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

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
    if (nameLen < 1 || nameLen > MAX_NAME_LEN) return ERR_INVALID_NAME;
    
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

    // File exists, update access timestamp
    if (inodeBlock != -1) {
        time_t now = time(NULL);
        memcpy(&block[INODE_ACCESS_TS], &now, sizeof(time_t));
        writeBlock(currentMount, inodeBlock, block);
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
        strncpy(&newInode[INODE_NAME_OFFSET], name, MAX_NAME_LEN);
        newInode[INODE_SIZE_OFFSET]  = 0; // empty file

        // Setting creation modification and access timestamps
        time_t now = time(NULL);
        memcpy(&newInode[INODE_CREATION_TS], &now, sizeof(time_t));
        memcpy(&newInode[INODE_MODIFICATION_TS], &now, sizeof(time_t));
        memcpy(&newInode[INODE_ACCESS_TS], &now, sizeof(time_t));

        // Write inode to disk
        if (writeBlock(currentMount, inodeBlock, newInode) < 0) return ERR_BLOCK_WRITE;
        // Mark block as used in bitmap
        if (setBitmapBit(inodeBlock, 0) < 0) return ERR_BLOCK_WRITE;
    }
    
    // Update open file table
    openFileTable[fd].inUse = 1;
    openFileTable[fd].inodeBlock = inodeBlock;
    openFileTable[fd].filePointer = 0;
    strncpy(openFileTable[fd].name, name, MAX_NAME_LEN);
    // Null terminate filename
    openFileTable[fd].name[MAX_NAME_LEN] = '\0';

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


int tfs_writeFile(fileDescriptor FD, char *buffer, int size) {
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Making sure file descriptor in correct range
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;
    if (buffer == NULL) return ERR_INVALID_PARAM;
    if (size < 0) return ERR_INVALID_PARAM;
    
    // Read inode block
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;

    // Check read only flag
    if (inode[INODE_RO_FLAG] == 1) return ERR_FILE_READ_ONLY;

    // Free any existing extent blocks before writing new data
    int extentBlock = (unsigned char)inode[INODE_FIRST_EXTENT];
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
        freeBlock[MAGIC_NUM_OFFSET]  = MAGIC_NUMBER;
        if (writeBlock(currentMount, extentBlock, freeBlock) < 0) return ERR_BLOCK_WRITE;

        extentBlock = nextExtent;
    }

    // Reset inode state
    inode[INODE_FIRST_EXTENT] = 0;
    inode[INODE_SIZE_OFFSET] = 0;
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;

    // Write buffer to file extent blocks   
    int bytesWritten = 0;
    int prevExtentBlock = -1;

    while (bytesWritten < size) {
        // Find a free block for this extent
        int newExtent = findFreeBlock();
        if (newExtent < 0) return ERR_NO_FREE_BLOCKS;
        // Mark new extent block as used in bitmap
        if (setBitmapBit(newExtent, 0) < 0) return ERR_BLOCK_WRITE;

        // Build file extent block
        char extent[BLOCKSIZE];
        memset(extent, 0x00, BLOCKSIZE);
        extent[BLOCK_TYPE_OFFSET] = FILE_EXTENT;
        extent[MAGIC_NUM_OFFSET] = MAGIC_NUMBER;
        extent[FILE_EXTENT_NEXT] = 0;

        // Copy bytes from buffer that fit into a block
        int bytesToCopy = size - bytesWritten;
        if (bytesToCopy > EXTENT_DATA_SIZE) bytesToCopy = EXTENT_DATA_SIZE;
        memcpy(&extent[EXTENT_DATA_OFFSET], buffer + bytesWritten, bytesToCopy);
        bytesWritten += bytesToCopy;
        if (writeBlock(currentMount, newExtent, extent) < 0) return ERR_BLOCK_WRITE;
        
        // If first file extent, update inode
        if (prevExtentBlock == -1) {
            inode[INODE_FIRST_EXTENT] = (char)newExtent;
            if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;
        } else {
            char prevExtent[BLOCKSIZE];
            if (readBlock(currentMount, prevExtentBlock, prevExtent) < 0) return ERR_DISK_READ;
            // Need to get prev extent block to set next extent pointer to new extent
            prevExtent[FILE_EXTENT_NEXT] = (char)newExtent;
            if (writeBlock(currentMount, prevExtentBlock, prevExtent) < 0) return ERR_BLOCK_WRITE;
        }
        // Set this extent to prev for next iteration
        prevExtentBlock = newExtent;   
    }

    // Update inode state once file write successful
    // Store file size as 4 bytes
    memcpy(&inode[INODE_SIZE_OFFSET], &size, sizeof(int));
    // Update modification timestamp
    time_t now = time(NULL);
    memcpy(&inode[INODE_MODIFICATION_TS], &now, sizeof(time_t));
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;
    openFileTable[FD].filePointer = 0;

    return TFS_SUCCESS;
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

    // Check read-only flag
    if (inode[INODE_RO_FLAG] == 1) return ERR_FILE_READ_ONLY;

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
    // Making sure a file system exists first
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;   
    // Making sure file descriptor in correct range   
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    // Make sure file is actually in use
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;
    // BUFFER ERROR CODE NEEDED
    if (buffer == NULL) return ERR_INVALID_PARAM;

    // Get file size from inode
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;
    // Read file size as 4 bytes
    int fileSize;
    memcpy(&fileSize, &inode[INODE_SIZE_OFFSET], sizeof(int));

    int fp = openFileTable[FD].filePointer;

    // Check if fp is past EOF
    if (fp >= fileSize) return ERR_EOF;

    // Find block that fp points to
    int extentIndex = fp / EXTENT_DATA_SIZE;
    int byteInExtent = fp % EXTENT_DATA_SIZE;
    
    // Traverse through each block until target block
    int extentBlock = (unsigned char)inode[INODE_FIRST_EXTENT];
    for (int i = 0; i < extentIndex; i++) {
        if (extentBlock == 0) return ERR_EOF;
        char extent[BLOCKSIZE];
        if (readBlock(currentMount, extentBlock, extent) < 0) return ERR_DISK_READ;
        extentBlock = (unsigned char)extent[FILE_EXTENT_NEXT];
    }

    // Read the target extent and copy the byte to buffer
    char extent[BLOCKSIZE];
    if (readBlock(currentMount, extentBlock, extent) < 0) return ERR_DISK_READ;
    *buffer = extent[EXTENT_DATA_OFFSET + byteInExtent];

    // Update access timestamp
    time_t now = time(NULL);
    memcpy(&inode[INODE_ACCESS_TS], &now, sizeof(time_t));
    writeBlock(currentMount, inodeBlock, inode);

    // Increment file pointer
    openFileTable[FD].filePointer++;
    return TFS_SUCCESS;
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


// Feature b: Directory listing and file renaming
int tfs_readdir(void) {
    // Make sure a file system is mounted
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;

    // Read superblock to get total blocks
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;
    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

    // Scan all blocks for inodes and print their names
    printf("Files on disk:\n");
    char block[BLOCKSIZE];
    for (int i = 1; i < numBlocks; i++) {
        if (readBlock(currentMount, i, block) < 0) continue;
        if (block[BLOCK_TYPE_OFFSET] != INODE) continue;
        if (block[MAGIC_NUM_OFFSET] != MAGIC_NUMBER) continue;
        printf("- %s\n", &block[INODE_NAME_OFFSET]);
    }
    return TFS_SUCCESS;
}

int tfs_rename(fileDescriptor FD, char *newName) {
    // Make sure a file system is mounted
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Validate file descriptor
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;
    // Validate new name
    if (newName == NULL) return ERR_INVALID_NAME;
    int nameLen = strlen(newName);
    if (nameLen < 1 || nameLen > MAX_NAME_LEN) return ERR_INVALID_NAME;

    // Read the inode block
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;

    // Overwrite the name in the inode
    memset(&inode[INODE_NAME_OFFSET], 0x00, MAX_NAME_LEN + 1);
    strncpy(&inode[INODE_NAME_OFFSET], newName, MAX_NAME_LEN);

    // Write updated inode back to disk
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;

    // Update open file table entry
    strncpy(openFileTable[FD].name, newName, MAX_NAME_LEN);
    openFileTable[FD].name[MAX_NAME_LEN] = '\0';

    return TFS_SUCCESS;
}

// Feature e: Timestamps
int tfs_readFileInfo(fileDescriptor FD) {
    // Make sure a file system is mounted
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    // Validate file descriptor
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;

    // Read inode block
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;

    // Extract timestamps from inode
    time_t creation, modification, access;
    memcpy(&creation, &inode[INODE_CREATION_TS], sizeof(time_t));
    memcpy(&modification, &inode[INODE_MODIFICATION_TS], sizeof(time_t));
    memcpy(&access, &inode[INODE_ACCESS_TS], sizeof(time_t));

    // Print file info with timestamps
    printf("File: %s\n", openFileTable[FD].name);
    printf("- Created: %s", ctime(&creation));
    printf("- Modified: %s", ctime(&modification));
    printf("- Accessed: %s", ctime(&access));

    return TFS_SUCCESS;
}

// Helper: find inode block number by filename, returns -1 if not found
static int findInodeByName(char *name) {
    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return -1;
    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

    char block[BLOCKSIZE];
    for (int i = 1; i < numBlocks; i++) {
        if (readBlock(currentMount, i, block) < 0) continue;
        if (block[BLOCK_TYPE_OFFSET] != INODE) continue;
        if (block[MAGIC_NUM_OFFSET] != MAGIC_NUMBER) continue;
        if (strcmp(&block[INODE_NAME_OFFSET], name) == 0) return i;
    }
    return -1;
}

// Feature d: Read only and writeByte
int tfs_makeRO(char *name) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    if (name == NULL) return ERR_INVALID_NAME;

    int inodeBlock = findInodeByName(name);
    if (inodeBlock < 0) return ERR_FILE_NOT_FOUND;

    // Read inode and set RO flag
    char inode[BLOCKSIZE];
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;
    inode[INODE_RO_FLAG] = 1;
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;

    return TFS_SUCCESS;
}

int tfs_makeRW(char *name) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    if (name == NULL) return ERR_INVALID_NAME;

    int inodeBlock = findInodeByName(name);
    if (inodeBlock < 0) return ERR_FILE_NOT_FOUND;

    // Read inode and clear RO flag
    char inode[BLOCKSIZE];
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;
    inode[INODE_RO_FLAG] = 0;
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;

    return TFS_SUCCESS;
}

int tfs_writeByte(fileDescriptor FD, unsigned int data) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;
    if (FD < 0 || FD >= MAX_FILES) return ERR_FILE_NOT_FOUND;
    if (!openFileTable[FD].inUse) return ERR_FILE_NOT_FOUND;

    // Read inode
    char inode[BLOCKSIZE];
    int inodeBlock = openFileTable[FD].inodeBlock;
    if (readBlock(currentMount, inodeBlock, inode) < 0) return ERR_DISK_READ;

    // Check readonly flag
    if (inode[INODE_RO_FLAG] == 1) return ERR_FILE_READ_ONLY;

    // Get file size
    int fileSize;
    memcpy(&fileSize, &inode[INODE_SIZE_OFFSET], sizeof(int));

    int fp = openFileTable[FD].filePointer;
    if (fp >= fileSize) return ERR_EOF;

    // Find the correct extent block (same traversal as readByte)
    int extentIndex = fp / EXTENT_DATA_SIZE;
    int byteInExtent = fp % EXTENT_DATA_SIZE;

    int extentBlock = (unsigned char)inode[INODE_FIRST_EXTENT];
    for (int i = 0; i < extentIndex; i++) {
        if (extentBlock == 0) return ERR_EOF;
        char extent[BLOCKSIZE];
        if (readBlock(currentMount, extentBlock, extent) < 0) return ERR_DISK_READ;
        extentBlock = (unsigned char)extent[FILE_EXTENT_NEXT];
    }

    // Read extent, overwrite the byte, write back
    char extent[BLOCKSIZE];
    if (readBlock(currentMount, extentBlock, extent) < 0) return ERR_DISK_READ;
    extent[EXTENT_DATA_OFFSET + byteInExtent] = (char)data;
    if (writeBlock(currentMount, extentBlock, extent) < 0) return ERR_BLOCK_WRITE;

    // Update modification timestamp
    time_t now = time(NULL);
    memcpy(&inode[INODE_MODIFICATION_TS], &now, sizeof(time_t));
    if (writeBlock(currentMount, inodeBlock, inode) < 0) return ERR_BLOCK_WRITE;

    // Increment file pointer
    openFileTable[FD].filePointer++;
    return TFS_SUCCESS;
}

// Feature a: Fragmentation info and defragmentation
int tfs_displayFragments(void) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;

    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;
    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

    printf("Block map (%d blocks):\n", numBlocks);
    printf("S: Superblock, I: Inode, D: Data, F: Free\n");
    char block[BLOCKSIZE];
    for (int i = 0; i < numBlocks; i++) {
        if (readBlock(currentMount, i, block) < 0) continue;
        char type;
        switch (block[BLOCK_TYPE_OFFSET]) {
            case SUPERBLOCK: type = 'S'; break;
            case INODE: type = 'I'; break;
            case FILE_EXTENT: type = 'D'; break;
            case FREE_BLOCK: type = 'F'; break;
            default: type = '?'; break;
        }
        printf("[%d:%c] ", i, type);
    }
    printf("\n");
    return TFS_SUCCESS;
}

int tfs_defrag(void) {
    if (currentMount == -1) return ERR_DISK_NOT_MOUNTED;

    char superblock[BLOCKSIZE];
    if (readBlock(currentMount, 0, superblock) < 0) return ERR_DISK_READ;
    int numBlocks = (unsigned char)superblock[SUPERBLOCK_NUM_BLOCKS_OFFSET];

    // Read all blocks into memory
    char blocks[256][BLOCKSIZE];
    for (int i = 0; i < numBlocks; i++) {
        if (readBlock(currentMount, i, blocks[i]) < 0) return ERR_DISK_READ;
    }

    // Build old-to-new block mapping
    // Used blocks go first (skip superblock at 0), free blocks at end
    int newPos[256];
    int usedSlot = 1;
    int freeSlot = numBlocks - 1;

    // Superblock stays at 0
    newPos[0] = 0;

    // First pass assignss new positions for used blocks
    for (int i = 1; i < numBlocks; i++) {
        if (blocks[i][BLOCK_TYPE_OFFSET] != FREE_BLOCK) {
            newPos[i] = usedSlot++;
        }
    }
    // Second pass assigns new positions for free blocks from the end
    for (int i = 1; i < numBlocks; i++) {
        if (blocks[i][BLOCK_TYPE_OFFSET] == FREE_BLOCK) {
            newPos[i] = freeSlot--;
        }
    }

    // Update all internal block pointers using the mapping
    for (int i = 1; i < numBlocks; i++) {
        if (blocks[i][BLOCK_TYPE_OFFSET] == INODE) {
            // Update first extent pointer
            int oldExtent = (unsigned char)blocks[i][INODE_FIRST_EXTENT];
            if (oldExtent != 0) {
                blocks[i][INODE_FIRST_EXTENT] = (char)newPos[oldExtent];
            }
        } else if (blocks[i][BLOCK_TYPE_OFFSET] == FILE_EXTENT) {
            // Update next extent pointer
            int oldNext = (unsigned char)blocks[i][FILE_EXTENT_NEXT];
            if (oldNext != 0) {
                blocks[i][FILE_EXTENT_NEXT] = (char)newPos[oldNext];
            }
        }
    }

    // Rearrange blocks into new order
    char temp[256][BLOCKSIZE];
    memcpy(temp, blocks, sizeof(blocks));
    for (int i = 1; i < numBlocks; i++) {
        memcpy(blocks[newPos[i]], temp[i], BLOCKSIZE);
    }

    // Rebuild superblock bitmap
    int bitmapSize = (numBlocks + 7) / 8;
    memset(&blocks[0][SUPERBLOCK_BITMAP_OFFSET], 0x00, bitmapSize);
    for (int i = 0; i < numBlocks; i++) {
        int byteIndex = i / 8;
        int bitIndex = 7 - (i % 8);
        if (blocks[i][BLOCK_TYPE_OFFSET] == FREE_BLOCK) {
            blocks[0][SUPERBLOCK_BITMAP_OFFSET + byteIndex] |= (1 << bitIndex);
        }
    }

    // Write all blocks back to disk
    for (int i = 0; i < numBlocks; i++) {
        if (writeBlock(currentMount, i, blocks[i]) < 0) return ERR_BLOCK_WRITE;
    }

    // Update open file table inode block references
    for (int i = 0; i < MAX_FILES; i++) {
        if (openFileTable[i].inUse) {
            int oldInode = openFileTable[i].inodeBlock;
            openFileTable[i].inodeBlock = newPos[oldInode];
        }
    }

    return TFS_SUCCESS;
}
