#!/bin/bash

# In the below syntax The drive letter : (colon) has a special meaning . It is 
# used to access image files which are directly specified on the command line 
# using the -i options. See sect 2.2 in the mtools documentation for more info. 

# Delete old kernel and filesystem data from the GRUB image.
mdel -i grub.img ::kernel.img || true
mdel -i grub.img ::filesystem.img || true

echo -n "Copying files to disk image... "

# Write kernel and filesystem data to the GRUB image.
mcopy -i grub.img kernel.img :: || exit 1
mcopy -i grub.img filesystem.img :: || exit 1
echo "finished" $'\n'$'\n'
