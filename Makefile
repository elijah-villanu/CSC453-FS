CC = gcc
CFLAGS = -Wall -g -std=c99
PROG = tinyFSDemo
OBJS = tinyFSDemo.o libTinyFS.o libDisk.o

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJS)

tinyFSDemo.o: tinyFSDemo.c libTinyFS.h tinyFS.h tinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libTinyFS.o: libTinyFS.c libTinyFS.h tinyFS.h libDisk.h libDisk.o tinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

libDisk.o: libDisk.c libDisk.h tinyFS.h
	$(CC) $(CFLAGS) -c -o $@ $<

tfsTest: tfsTest.o libTinyFS.o libDisk.o
	$(CC) $(CFLAGS) -o tfsTest tfsTest.o libTinyFS.o libDisk.o

tfsTest.o: tfsTest.c libTinyFS.h tinyFS.h tinyFS_errno.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(PROG) tfsTest tfsTest.o
