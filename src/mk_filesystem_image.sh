#!/bin/bash
ROOTFS=../rootfs

if [ ! -f grub.img ]; then
	echo "File grub.img not found - cannot build boot disk image"
	exit 1
fi

if [ -f fstool ]; then
	if [ ! -d $ROOTFS ]; then
		echo $ROOTFS "does not exist; cannot build filesystem image"
		exit 1
	fi

	echo -n "Building filesystem image... "

	if [ -f sh ]; then
		cp -f sh ls cat find mptest pwd $ROOTFS/bin || exit 1
	fi

	./fstool -build filesystem.img $ROOTFS >/dev/null || exit 1
	echo "finished" $'\n' $'\n'
else
	rm -f filesystem.img
	touch filesystem.img
fi
