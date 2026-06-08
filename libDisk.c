#include "libDisk.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_DISKS 100 
#define BLOCKSIZE 256

typedef struct {
    int fd;
    int diskNum;
    int diskSize;
    char* filename;
} Disk;

Disk diskList[MAX_DISKS];
int diskCounter = 0;

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
        // No existing disk open
        return -1;
    }

    // Invalid nBytes passed
    if (nBytes < BLOCKSIZE) return -1;

    // Check if list has space
    if (diskCounter >= MAX_DISKS) return -1;

    // Make disk size a factor of block size
    diskSize = (int)floor(nBytes / BLOCKSIZE) * BLOCKSIZE;    

    // Create UNIX file   
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    // File error, return -1
    if (fd < 0) {
        return -1;
    }

    // Create new disk
    diskList[diskCounter].fd = fd;
    diskList[diskCounter].diskNum = diskCounter;
    diskList[diskCounter].diskSize = diskSize;
    diskList[diskCounter].filename = filename; 
    diskCounter++;
    
    return (diskCounter - 1);
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
    diskList[disk].filename = NULL;
    return 0;
}


int readBlock(int disk, int bNum, void *block) {

}

int writeBlock(int disk, int bNum, void *block) {

}
