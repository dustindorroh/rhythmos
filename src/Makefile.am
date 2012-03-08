#       Makefile
#
#       Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
#


# Host defs
CC = gcc
CPP = cpp
LD = ld
AS = as

HOST_CC = $(CC)
HOST_CFLAGS = -Wall
HOST_CPP = $(CPP)
HOST_LD = $(LD)
HOST_AS = $(AS)

# Target defs
TARGET_CPP = cpp
TARGET_CC = gcc
TARGET_CFLAGS = -m32 -fno-stack-protector -Wall -nostdinc -fno-builtin
TARGET_AS = as
TARGET_AS_FLAGS = --32
TARGET_LD = ld
TARGET_LD_FLAGS = -m elf_i386

INCLUDE = -Iinclude

PRJ_ROOT ?= ..
KERNEL_IMG = kernel.img
FILESYSTEM_IMG = filesystem.img
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


COREUTILS = sh ls cat find pwd echo hello dsh kill halt
COREUTILS_OBJECTS = $(addsuffix .o, $(COREUTILS))

TESTS = mptest daemon
TESTS_OBJECTS = $(addsuffix .o, $(TESTS))

TEST_UTILS = fstool testbuddy
TEST_UTILS_OBJECTS = $(addsufix .o, $(TEST_UTILS))

FILE_SYSTEM_BASE = $(PRJ_ROOT)/rootfs
FILE_SYSTEM_BIN = $(FILE_SYSTEM_BASE)/bin

all: default fs boot

default: \
	fstool \
	testbuddy \
	$(COREUTILS) \
	$(TESTS) \
	$(KERNEL_IMG)

grub: |boot

grub.img: |boot

fdimage:|boot

boot: default fs
	./mk_boot_image.sh

filesystem.img: |fs

fs: default
	./mkfstree.sh
	cp $(COREUTILS) $(TESTS) $(FILE_SYSTEM_BIN)
	./mk_filesystem_image.sh


VPATH = ./include

fstool: fstool.c filesystem.c filesystem.h
	$(HOST_CC) -DUSE_UNIXPROC -DUSERLAND $(INCLUDE) $(HOST_CFLAGS) -o fstool fstool.c filesystem.c

testbuddy: testbuddy.c buddy.c buddy.h
	$(HOST_CC) -DUSERLAND $(INCLUDE) -m32 $(HOST_CFLAGS) -o testbuddy testbuddy.c buddy.c

%.o: %.c
	$(TARGET_CC) $(INCLUDE) $(TARGET_CFLAGS) -c $<

%.o: %.s
	$(TARGET_CPP) $(INCLUDE) $< | $(TARGET_AS) $(TARGET_AS_FLAGS) -o $*.o


# Link using `link.ld'
$(KERNEL_IMG): $(KERNEL_OBJECTS)
	$(TARGET_LD) $(TARGET_LD_FLAGS) -T link.ld -o $(KERNEL_IMG) $(KERNEL_OBJECTS)

# Link using `link-user.ld'
$(COREUTILS): %: %.o $(USER_OBJECTS)
	$(TARGET_LD) $(TARGET_LD_FLAGS) -T link-user.ld -o $@ $(USER_OBJECTS) $@.o

$(TESTS): %: %.o $(USER_OBJECTS)
	$(TARGET_LD) $(TARGET_LD_FLAGS) -T link-user.ld -o $@ $(USER_OBJECTS) $@.o

# Misc targets
.PHONY : run run-qemu install clean
run: |run-qemu

run-qemu: boot
	qemu -daemonize -fda $(GRUB_IMG)


clean-local:
	-rm --force $(KERNEL_IMG) $(KERNEL_OBJECTS) $(FILESYSTEM_IMG) \
	$(USER_OBJECTS) \
	$(COREUTILS) $(COREUTILS_OBJECTS) \
	$(TESTS) $(TESTS_OBJECTS) \
	$(TEST_UTILS) $(TEST_UTILS_OBJECTS) \
	$(PROGRAMS) $(PROGRAMS_OBJECTS)
