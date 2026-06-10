#include "libTinyFS.h"
#include "tinyFS_errno.h"
#include <stdio.h>

// Maps the error returned from tinyFS functions to its string version of the error
const char *errToString(int err) {
    switch (err) {
        case TFS_SUCCESS:              return "TFS_SUCCESS";
        case ERR_DISK_NOT_FOUND:       return "ERR_DISK_NOT_FOUND";
        case ERR_FILE_NOT_FOUND:       return "ERR_FILE_NOT_FOUND";
        case ERR_DISK_OPEN:            return "ERR_DISK_OPEN";
        case ERR_DISK_READ:            return "ERR_DISK_READ";
        case ERR_DISK_NOT_MOUNTED:     return "ERR_DISK_NOT_MOUNTED";
        case ERR_INVALID_MAGIC_NUM:    return "ERR_INVALID_MAGIC_NUM";
        case ERR_NAME_TOO_LONG:        return "ERR_NAME_TOO_LONG";
        case ERR_INVALID_FS:           return "ERR_INVALID_FS";
        case ERR_EOF:                  return "ERR_EOF";
        case ERR_NO_FREE_BLOCKS:       return "ERR_NO_FREE_BLOCKS";
        case ERR_BLOCK_WRITE:          return "ERR_BLOCK_WRITE";
        case ERR_DISK_ALREADY_MOUNTED: return "ERR_DISK_ALREADY_MOUNTED";
        case ERR_FILE_TABLE_FULL:      return "ERR_FILE_TABLE_FULL";
        case ERR_BLOCK_OUT_OF_RANGE:   return "ERR_BLOCK_OUT_OF_RANGE";
        case ERR_INVALID_NAME:         return "ERR_INVALID_NAME";
        default:                       return "UNKNOWN_ERROR";
    }
}

int main() {
    printf("Creating tiny file system\n");
    int fs = tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);
    if (fs == 0) {
        printf("%s created with size %d\n", DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);
    } else {
        printf("Failed to create tinyFS: %s\n", errToString(fs));
    }

    printf("Demo complete!\n");
    return 0;
}