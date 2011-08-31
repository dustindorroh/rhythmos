#       mkfstree.sh
#       
#       Copyright 2011 Dustin Dorroh <dustindorroh@gmail.com>
#       

#!/bin/bash
ROOTFS=$(readlink -f $PWD/../rootfs)
echo "Creating rootfs" $'\n'

# Create rootfs if not already created.
if [ ! -d "$ROOTFS" ]; then
	mkdir -v -p $ROOTFS
	mkdir -v -p $ROOTFS/bin
	mkdir -v -p $ROOTFS/boot
	mkdir -v -p $ROOTFS/dev
	mkdir -v -p $ROOTFS/etc
	mkdir -v -p $ROOTFS/usr
	mkdir -v -p $ROOTFS/root
	mkdir -v -p $ROOTFS/tmp
else
    	mkdir -v -p $ROOTFS/bin
	mkdir -v -p $ROOTFS/boot
	mkdir -v -p $ROOTFS/dev
	mkdir -v -p $ROOTFS/etc
	mkdir -v -p $ROOTFS/usr
	mkdir -v -p $ROOTFS/root
	mkdir -v -p $ROOTFS/tmp
fi
