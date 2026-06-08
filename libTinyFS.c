#include "tinyFS.h"
#include "libTinyFS.h"
#include "libDisk.h"


int tfs_mkfs(char *filename, int nBytes) {
    int diskNum = openDisk(filename, nBytes);

    if (diskNum < 0) {
        return ERR_DISK_OPEN;
    }

    // TODO
}


int tfs_mount(char *diskname);


int tfs_unmount(void);


fileDescriptor tfs_openFile(char *name);


int tfs_closeFile(fileDescriptor FD);


int tfs_writeFile(fileDescriptor FD,char *buffer, int size);


int tfs_deleteFile(fileDescriptor FD);


int tfs_readByte(fileDescriptor FD, char *buffer);


int tfs_seek(fileDescriptor FD, int offset);

