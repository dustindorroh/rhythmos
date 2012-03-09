#!/bin/bash
mkdir floppy
losetup /dev/loop1 grub.img
mount /dev/loop1 floppy
losetup --detach /dev/loop1
