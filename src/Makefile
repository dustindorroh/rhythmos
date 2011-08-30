#       Makefile
#
#       Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
#
CC = gcc
CPP = cpp
LD = ld

PRJ_ROOT ?= ..
KERNEL_IMG = kernel.bin
GRUB_IMG = grub.img
KERNEL_OBJECTS = \
	start.o \
	process.o \
	page.o \
	libc.o \
	syscall.o \
	calls.o \
	buddy.o \
	pipe.o \
	filedesc.o \
	filesystem.o \
	fscalls.o \
	unixproc.o \
	main.o \
	segmentation.o \
	interrupts.o
USER_OBJECTS = crtso.o libc.o calls.o buddy.o


COREUTILS = sh ls cat find pwd echo hello dsh kill
COREUTILS_OBJECTS = $(addsuffix .o, $(COREUTILS))

TESTS = mptest daemon
TESTS_OBJECTS = $(addsuffix .o, $(TESTS))

TEST_UTILS = fstool testbuddy
TEST_UTILS_OBJECTS = $(addsufix .o, $(TEST_UTILS))

PROGRAMS =
PROGRAMS_OBJECTS = $(addsufix .o, $(PROGRAMS))

FILE_SYSTEM_BASE = $(PRJ_ROOT)/rootfs
FILE_SYSTEM_BIN = $(FILE_SYSTEM_BASE)/bin

all: default fs boot

default: \
	fstool \
	testbuddy \
	$(COREUTILS) \
	$(TESTS) \
	$(KERNEL_IMG)\

grub: |boot

boot: default fs
	./mk_boot_image.sh

fs: default
	./mkfstree.sh
	cp $(COREUTILS) $(TESTS) $(FILE_SYSTEM_BIN)
	./mk_filesystem_image.sh

fstool: fstool.c filesystem.c filesystem.h
	$(CC) -m32 -DUSE_UNIXPROC -DUSERLAND -Wall -o fstool fstool.c filesystem.c

testbuddy: testbuddy.c buddy.c buddy.h
	$(CC) -m32 -DUSERLAND -Wall -o testbuddy testbuddy.c buddy.c

%.o: %.c *.h
	$(CC) -m32 -fno-stack-protector -Wall -nostdinc -fno-builtin -c $<

%.o: %.s
	$(CPP) $< | as -o $*.o


# Link using `link.ld'
$(KERNEL_IMG): $(KERNEL_OBJECTS)
	$(LD) -T link.ld -o $(KERNEL_IMG) $(KERNEL_OBJECTS)

# Link using `link-user.ld'
$(COREUTILS): %: %.o $(USER_OBJECTS)
	$(LD) -T link-user.ld -o $@ $(USER_OBJECTS) $@.o

$(TESTS): %: %.o $(USER_OBJECTS)
	$(LD) -T link-user.ld -o $@ $(USER_OBJECTS) $@.o

# Misc targets
.PHONY : run run-qemu install clean
run: |run-qemu

run-qemu: boot
	qemu -daemonize -fda $(GRUB_IMG)

install:

clean:
	-rm --force $(KERNEL_IMG) $(KERNEL_OBJECTS) \
	$(USER_OBJECTS) \
	$(COREUTILS) $(COREUTILS_OBJECTS) \
	$(TESTS) $(TESTS_OBJECTS) \
	$(TEST_UTILS) $(TEST_UTILS_OBJECTS) \
	$(PROGRAMS) $(PROGRAMS_OBJECTS)

