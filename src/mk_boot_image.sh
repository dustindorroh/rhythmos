#!/bin/bash

KERNEL_IMG=$1
FILESYSTEM_IMG=$2

# In the below syntax The drive letter : (colon) has a special meaning . It is 
# used to access image files which are directly specified on the command line 
# using the -i options. See sect 2.2 in the mtools documentation for more info. 

# Delete old kernel and filesystem data from the GRUB image.
mdel -i grub.img ::${KERNEL_IMG} || true
mdel -i grub.img ::${FILESYSTEM_IMG} || true

echo -n "Copying files to disk image... "

# Write kernel and filesystem data to the GRUB image.
mcopy -i grub.img ${KERNEL_IMG} :: || exit 1
mcopy -i grub.img ${FILESYSTEM_IMG} :: || exit 1
echo "finished" $'\n'$'\n'
