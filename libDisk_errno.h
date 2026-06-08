#ifndef LIBDISK_ERRNO_H
#define LIBDISK_ERRNO_H

// Error codes for openDisk
#define ERR_DISK_LIST_FULL -1
#define ERR_NBYTES_TOO_SMALL -2
#define ERR_FILE_OPEN_FAILED -3
#define ERR_DISK_NOT_FOUND -4

// Codes for closeDisk
#define ERR_DISK_ALREADY_CLOSED -5
#define ERR_INVALID_DISK_NUM -6

// Codes for read and write disk
#define ERR_DISK_NOT_OPEN -7
#define ERR_NULL_BUFFER -8
#define ERR_BLOCK_OUT_OF_RANGE -9
#define ERR_SEEK_FAILED -10
#define ERR_READ_FAILED -11
#define ERR_WRITE_FAILED -12

#endif
