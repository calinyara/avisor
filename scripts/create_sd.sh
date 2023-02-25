#############################################################
# SPDX-License-Identifier: GPL-2.0-or-later
#
#  aVisor Hypervisor
#
#  A Tiny Hypervisor for IoT Development
#
#  Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
#############################################################

#!/bin/bash

# sdcard img name, hv (not needed for qemu) and one VM at least
if [ "$#" -lt 2 ]; then
    echo "Usage: "$0" sdimage [hv] guest0, guest1 ..."
    exit 1
fi

SDNAME="$1"
shift
PAYLOAD="$@"

command -v qemu-img >/dev/null || { echo "qemu-img not installed"; exit 1; }
command -v qemu-nbd >/dev/null || { echo "qemu-nbd not installed"; exit 1; }

qemu-img create "$SDNAME" 64M
sudo qemu-nbd -c /dev/nbd0 "$SDNAME"

(echo o;
echo n; echo p
echo 1
echo ; echo
echo w; echo q) | sudo fdisk /dev/nbd0
sudo mkfs.fat -F32 /dev/nbd0p1

mkdir tmp || true
sudo mount -o user /dev/nbd0p1 tmp/
sudo cp $PAYLOAD tmp/
sleep 1s
sudo umount tmp/
rmdir tmp || true

sudo qemu-nbd -d /dev/nbd0

(echo t; echo c
echo w; echo q) | sudo fdisk "$SDNAME"
