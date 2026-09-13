#!/bin/bash
set -e

make -j$(nproc)

echo "Building Filesystem"
./build_fs.sh

echo "Copy kernel to iso/boot"
./copy_kernel.sh

echo "Please create iso/boot/grub if you have not and please put grub.cfg in iso/boot/grub"
./make_iso.sh

echo "Kernel build script completed run ./run.sh to try it out"