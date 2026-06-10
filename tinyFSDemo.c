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
        case ERR_INVALID_PARAM:        return "ERR_INVALID_PARAM";
        default:                       return "UNKNOWN_ERROR";
    }
}

int main() {
    // Creating file system
    printf("-Creating tiny file system-\n");
    int fs = tfs_mkfs(DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);
    if (fs == 0) {
        printf("%s created with size %d\n", DEFAULT_DISK_NAME, DEFAULT_DISK_SIZE);
    } else {
        printf("Failed to create tinyFS: %s\n", errToString(fs));
    }

    // Mounting file system (pass case)
    printf("\n-Mounting file system-\n");
    int ret_val = tfs_mount(DEFAULT_DISK_NAME);
    if (ret_val == 0) {
        printf("%s mounted successfully\n", DEFAULT_DISK_NAME);
    } else {
        printf("Failed to mount: %s\n", errToString(ret_val));
    }

    // Mounting file system (fail case)
    printf("\n-Mounting already mounted disk (should fail)-\n");
    ret_val = tfs_mount(DEFAULT_DISK_NAME);
    if (ret_val == 0) {
        printf("%s mounted successfully\n", DEFAULT_DISK_NAME);
    } else {
        printf("Failed to mount: %s\n", errToString(ret_val));
    }

    // Opening first file (pass case)
    printf("\n-Opening files-\n");
    int fd1 = tfs_openFile("file1");
    if (fd1 >= 0) {
        printf("Opened 'file1' with fd = %d\n", fd1);
    } else {
        printf("Failed to open file1: %s\n", errToString(fd1));
    }

    // Opening second file (pass case)
    int fd2 = tfs_openFile("file2");
    if (fd2 >= 0) {
        printf("Opened 'file2' with fd = %d\n", fd2);
    } else {
        printf("Failed to open file2: %s\n", errToString(fd2));
    }

    // Opening file that already exists (should return same fd)
    printf("\n-Opening file1 again\n");
    int fd1_dup = tfs_openFile("file1");
    if (fd1_dup >= 0) {
        printf("Opened 'file1' with fd = %d (expecting %d)\n", fd1_dup, fd1);
    } else {
        printf("Failed to open file1: %s\n", errToString(fd1_dup));
    }

    // Opening file with invalid name (too long: should fail)
    printf("\n-Opening file with invalid name (should fail)-\n");
    ret_val = tfs_openFile("aaaaaaaaaaaa");
    if (ret_val >= 0) {
        printf("Opened file with fd = %d\n", ret_val);
    } else {
        printf("Failed to open: %s\n", errToString(ret_val));
    }

    // Writing to file1 (pass case)
    printf("\n-Writing to file1-\n");
    char data[] = "Im writing this into the file wooo";
    ret_val = tfs_writeFile(fd1, data, sizeof(data));
    if (ret_val == 0) {
        printf("Wrote '%s' to file1\n", data);
    } else {
        printf("Failed to write to file1: %s\n", errToString(ret_val));
    }

    // Writing to closed file (should fail)
    printf("\n-Writing to closed file (should fail)-\n");
    // FD outside of allowed range
    fileDescriptor out_of_range_fd = MAX_FILES + 1;
    ret_val = tfs_writeFile(out_of_range_fd, data, sizeof(data));
    if (ret_val == 0) {
        printf("Wrote to fd = %d\n", out_of_range_fd);
    } else {
        printf("Failed to write: %s\n", errToString(ret_val));
    }

    printf("\n-Demo complete!-\n");
    return 0;
}
