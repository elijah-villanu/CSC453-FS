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
        case ERR_FILE_READ_ONLY:      return "ERR_FILE_READ_ONLY";
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

    // Listing all files on disk (should show file1 and file2)
    printf("\n-Listing directory-\n");
    ret_val = tfs_readdir();
    if (ret_val != 0) {
        printf("Failed to readdir: %s\n", errToString(ret_val));
    }

    // Renaming file1 to renamed1 (pass case)
    printf("\n-Renaming file1 to renamed1-\n");
    ret_val = tfs_rename(fd1, "renamed1");
    if (ret_val == 0) {
        printf("Renamed file1 to renamed1\n");
    } else {
        printf("Failed to rename: %s\n", errToString(ret_val));
    }

    // Listing directory again (should show renamed1 and file2)
    printf("\n-Listing directory after rename-\n");
    ret_val = tfs_readdir();
    if (ret_val != 0) {
        printf("Failed to readdir: %s\n", errToString(ret_val));
    }

    // Renaming with invalid name (should fail)
    printf("\n-Renaming with too long name (should fail)-\n");
    ret_val = tfs_rename(fd1, "abcdefghijklk");
    if (ret_val == 0) {
        printf("Renamed successfully\n");
    } else {
        printf("Failed to rename: %s\n", errToString(ret_val));
    }

    // Reading file info for renamed1 (should show timestamps)
    printf("\n-Reading file info for renamed1-\n");
    ret_val = tfs_readFileInfo(fd1);
    if (ret_val != 0) {
        printf("Failed to read file info: %s\n", errToString(ret_val));
    }

    // Reading file info for file2 (should show timestamps)
    printf("\n-Reading file info for file2-\n");
    ret_val = tfs_readFileInfo(fd2);
    if (ret_val != 0) {
        printf("Failed to read file info: %s\n", errToString(ret_val));
    }

    // Reading bytes from renamed1 (pass case)
    printf("\n-Reading bytes from renamed1-\n");
    char readBuf;
    printf("Read: ");
    while (tfs_readByte(fd1, &readBuf) >= 0) {
        printf("%c", readBuf);
    }
    printf("\n");

    // Reading from empty file (should fail with EOF)
    printf("\n-Reading from empty file2 (should fail)-\n");
    ret_val = tfs_readByte(fd2, &readBuf);
    if (ret_val == 0) {
        printf("Read byte: %c\n", readBuf);
    } else {
        printf("Failed to read: %s\n", errToString(ret_val));
    }

    // Seeking to offset 3 in renamed1 and reading
    printf("\n-Seeking to offset 3 in renamed1-\n");
    ret_val = tfs_seek(fd1, 3);
    if (ret_val == 0) {
        printf("Seeked to offset 3\n");
    } else {
        printf("Failed to seek: %s\n", errToString(ret_val));
    }
    ret_val = tfs_readByte(fd1, &readBuf);
    if (ret_val == 0) {
        printf("Byte at offset 3: '%c'\n", readBuf);
    } else {
        printf("Failed to read: %s\n", errToString(ret_val));
    }

    // Seeking with negative offset (should fail)
    printf("\n-Seeking with negative offset (should fail)-\n");
    ret_val = tfs_seek(fd1, -1);
    if (ret_val == 0) {
        printf("Seeked to -1\n");
    } else {
        printf("Failed to seek: %s\n", errToString(ret_val));
    }

    // Display block map before defrag
    printf("\n-Displaying block fragments-\n");
    tfs_displayFragments();

    // Making renamed1 read-only
    printf("\n-Making renamed1 read-only-\n");
    ret_val = tfs_makeRO("renamed1");
    if (ret_val == 0) {
        printf("renamed1 is now read-only\n");
    } else {
        printf("Failed to make RO: %s\n", errToString(ret_val));
    }

    // Writing to read-only file (should fail)
    printf("\n-Writing to read-only file (should fail)-\n");
    fd1 = tfs_openFile("renamed1");
    ret_val = tfs_writeFile(fd1, data, sizeof(data));
    if (ret_val == 0) {
        printf("Wrote to renamed1\n");
    } else {
        printf("Failed to write: %s\n", errToString(ret_val));
    }

    // Deleting read-only file (should fail)
    printf("\n-Deleting read-only file (should fail)-\n");
    ret_val = tfs_deleteFile(fd1);
    if (ret_val == 0) {
        printf("Deleted renamed1\n");
    } else {
        printf("Failed to delete: %s\n", errToString(ret_val));
    }

    // Making renamed1 read-write again
    printf("\n-Making renamed1 read-write again-\n");
    ret_val = tfs_makeRW("renamed1");
    if (ret_val == 0) {
        printf("renamed1 is now read-write\n");
    } else {
        printf("Failed to make RW: %s\n", errToString(ret_val));
    }

    // Writing to read-write file (should work now)
    printf("\n-Writing to read-write file (should work)-\n");
    ret_val = tfs_writeFile(fd1, data, sizeof(data));
    if (ret_val == 0) {
        printf("Wrote '%s' to renamed1\n", data);
    } else {
        printf("Failed to write: %s\n", errToString(ret_val));
    }

    // WriteByte: overwrite byte at offset 0 with 'X'
    printf("\n-WriteByte: overwrite byte 0 with 'X'-\n");
    tfs_seek(fd1, 0);
    ret_val = tfs_writeByte(fd1, 'X');
    if (ret_val == 0) {
        printf("Wrote 'X' at offset 0\n");
    } else {
        printf("Failed to writeByte: %s\n", errToString(ret_val));
    }

    // Read back to verify writeByte worked
    printf("\n-Reading back after writeByte-\n");
    tfs_seek(fd1, 0);
    printf("Read: ");
    while (tfs_readByte(fd1, &readBuf) >= 0) {
        printf("%c", readBuf);
    }
    printf("\n");

    // Defragment the disk
    printf("\n-Defragmenting disk-\n");
    ret_val = tfs_defrag();
    if (ret_val == 0) {
        printf("Defragmentation complete\n");
    } else {
        printf("Failed to defrag: %s\n", errToString(ret_val));
    }

    // Display block map after defrag
    printf("\n-Displaying block fragments after defrag-\n");
    tfs_displayFragments();

    // Verify file still readable after defrag
    printf("\n-Reading renamed1 after defrag-\n");
    tfs_seek(fd1, 0);
    printf("Read: ");
    while (tfs_readByte(fd1, &readBuf) >= 0) {
        printf("%c", readBuf);
    }
    printf("\n");

    // Closing renamed1 (pass case)
    printf("\n-Closing renamed1-\n");
    ret_val = tfs_closeFile(fd1);
    if (ret_val == 0) {
        printf("Closed renamed1\n");
    } else {
        printf("Failed to close: %s\n", errToString(ret_val));
    }

    // Reading from closed file (should fail)
    printf("\n-Reading from closed file (should fail)-\n");
    ret_val = tfs_readByte(fd1, &readBuf);
    if (ret_val == 0) {
        printf("Read byte: %c\n", readBuf);
    } else {
        printf("Failed to read: %s\n", errToString(ret_val));
    }

    // Deleting file2 (pass case)
    printf("\n-Deleting file2-\n");
    ret_val = tfs_deleteFile(fd2);
    if (ret_val == 0) {
        printf("Deleted file2\n");
    } else {
        printf("Failed to delete: %s\n", errToString(ret_val));
    }

    // Deleting already deleted file (should fail)
    printf("\n-Deleting file2 again (should fail)-\n");
    ret_val = tfs_deleteFile(fd2);
    if (ret_val == 0) {
        printf("Deleted file2\n");
    } else {
        printf("Failed to delete: %s\n", errToString(ret_val));
    }

    // Listing directory after delete (should only show renamed1)
    printf("\n-Listing directory after delete-\n");
    ret_val = tfs_readdir();
    if (ret_val != 0) {
        printf("Failed to readdir: %s\n", errToString(ret_val));
    }

    // Unmounting (pass case)
    printf("\n-Unmounting-\n");
    ret_val = tfs_unmount();
    if (ret_val == 0) {
        printf("Unmounted successfully\n");
    } else {
        printf("Failed to unmount: %s\n", errToString(ret_val));
    }

    // Unmounting when nothing mounted (should fail)
    printf("\n-Unmounting again (should fail)-\n");
    ret_val = tfs_unmount();
    if (ret_val == 0) {
        printf("Unmounted successfully\n");
    } else {
        printf("Failed to unmount: %s\n", errToString(ret_val));
    }

    printf("\n-Demo complete!-\n");
    return 0;
}
