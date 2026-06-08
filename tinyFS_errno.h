#ifndef TINYFS_ERRNO_H
#define TINYFS_ERRNO_H

#define TFS_SUCCESS 0 
#define ERR_DISK_NOT_FOUND -1 // Disk is not found
#define ERR_FILE_NOT_FOUND -2 // File is not found
#define ERR_DISK_OPEN -3 // openDisk failed
#define ERR_DISK_READ -4 // readBlock failed
#define ERR_DISK_NOT_MOUNTED -5 // No fs mounted
#define ERR_INVALID_MAGIC_NUM -6 // Not a tinyFS disk
#define ERR_NAME_TOO_LONG -7 // Name was greater than 8 chars
#define ERR_INVALID_FS -8 // Not a valid FS in open FD table
#define ERR_EOF -9 // readByte past EOF
#define ERR_NO_FREE_BLOCKS -10 // Disk full

#endif
