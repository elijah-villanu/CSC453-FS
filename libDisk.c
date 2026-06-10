#define _GNU_SOURCE
#include "libDisk.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    int fd;
    int diskNum;
    int diskSize;
    char* filename;
} Disk;

Disk diskList[MAX_DISKS];
int diskCounter = 0;

/* HELPERS */
// Validates disk number, block number, and buffer
static int validateDiskBlock(int disk, int bNum, void *block) {
    // Input validation on passed in disk
    if (disk < 0 || disk >= MAX_DISKS) return -1;
    // If disk is closed
    if (diskList[disk].fd == -1) return -1;
    if (block == NULL) return -1;
    // Input validation on passed in block
    if (bNum < 0 || bNum >= diskList[disk].diskSize / BLOCKSIZE) return -1;

    return 0;
}

// Repositions file pointer to the start of a block
static int seekToBlock(int disk, int bNum) {
    // Reposition file pointer by offset
    off_t offset = (off_t) bNum * BLOCKSIZE;
    int fd = diskList[disk].fd;
    if (lseek(fd, offset, SEEK_SET) < 0) return -1;

    return fd;
}

// Adds an opened disk into diskList
static int addDiskEntry(char *filename, int fd, int diskSize) {
    diskList[diskCounter].fd = fd;
    diskList[diskCounter].diskNum = diskCounter;
    diskList[diskCounter].diskSize = diskSize;
    diskList[diskCounter].filename = strdup(filename);
    diskCounter++;

    return (diskCounter - 1);
}

// Initializes all diskList entries
void initDiskList() {
     for (int i = 0; i < MAX_DISKS; i++) {
        diskList[i].fd = -1;
        diskList[i].filename = NULL;
    }
}

int openDisk(char *filename, int nBytes) {
    int diskSize;
    
    // Open existing disk, don't overwrite
    if (nBytes == 0) {
        int diskCount = sizeof(diskList)/sizeof(Disk);
        for (int i = 0; i < diskCount ; i++) {
            if (diskList[i].filename != NULL && strcmp(diskList[i].filename, filename) == 0) {
                 return diskList[i].diskNum;
            }
        }

        // Check if list has space
        if (diskCounter >= MAX_DISKS) return -1;

        // No existing disk open, open from file without overwriting (new fd)
        int fd = open(filename, O_RDWR);
        if (fd < 0) {
            return -1;
        }
        // Repositions file pointer to end of file and returns the size of file in bytes
        off_t size = lseek(fd, 0, SEEK_END);
        if (size < BLOCKSIZE) {
            close(fd);
            return -1;
        }

        // Update the existing disk entry in diskList
        return addDiskEntry(filename, fd, (int)size);
    }

    // Invalid nBytes passed (creating new disk requires at least BLOCKSIZE)
    if (nBytes < BLOCKSIZE) return -1;

    // Check if list has space
    if (diskCounter >= MAX_DISKS) return -1;

    // Make disk size a factor of block size
    diskSize = (int)(nBytes / BLOCKSIZE) * BLOCKSIZE;    

    // Create UNIX file   
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    // File error, return -1
    if (fd < 0) {
        return -1;
    }

    // Create new disk
    return addDiskEntry(filename, fd, diskSize);
}

// Returns 0 on successful disk closure, -1 on failure
int closeDisk(int disk) {
    // Invalid disk passed in
    if (disk < 0 || disk >= MAX_DISKS) return -1;
    // If disk is already closed
    if (diskList[disk].fd == -1) return -1;

    // Close file descriptor and update disklist
    close(diskList[disk].fd);
    diskList[disk].fd = -1;
    diskList[disk].diskSize = 0;
    free(diskList[disk].filename);
    diskList[disk].filename = NULL;
    return 0;
}


int readBlock(int disk, int bNum, void *block) {
    if (validateDiskBlock(disk, bNum, block) < 0) return -1;

    int fd = seekToBlock(disk, bNum);
    if (fd < 0) return -1;

    // Successful read
    if (read(fd, block, BLOCKSIZE) != BLOCKSIZE) return -1;
    return 0;

}

int writeBlock(int disk, int bNum, void *block) {
    if (validateDiskBlock(disk, bNum, block) < 0) return -1;

    int fd = seekToBlock(disk, bNum);
    if (fd < 0) return -1;
    
    // Successful write
    if (write(fd, block, BLOCKSIZE) != BLOCKSIZE) return -1;
    return 0;
}