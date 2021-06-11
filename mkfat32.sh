#!/bin/bash

if [ ! -f "/tmp/aex2/aex.fat32.img" ]
then
    dd if=/dev/zero of=/tmp/aex2/aex.fat32.img count=65536 bs=512
    sed -e 's/\s*\([\+0-9a-zA-Z]*\).*/\1/' << EOF | fdisk /tmp/aex2/aex.fat32.img
        o
        n # fat32
        p

        

        w # Finish
EOF

    sudo losetup --partscan /dev/loop0 /tmp/aex2/aex.fat32.img 
    sudo mkfs.vfat -F 32 /dev/loop0p1
    sudo dosfsck -r -l -a -v -t /dev/loop0p1
    sudo losetup -d /dev/loop0 
fi

mkdir target

sudo losetup --partscan /dev/loop0 /tmp/aex2/aex.fat32.img 
sudo mount /dev/loop0p1 target

sudo cp -r iso/* target
sudo grub-install /dev/loop0 --boot-directory=/home/tymk/Development/aex2/target/boot \
    --target=i386-pc --modules="part_msdos fat normal"

sudo umount /dev/loop0p1
sudo dosfsck -r -l -a -v -t /dev/loop0p1

sudo losetup -d /dev/loop0 