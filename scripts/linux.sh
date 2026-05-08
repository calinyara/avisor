#############################################################
# SPDX-License-Identifier: GPL-2.0-or-later
#
#  aVisor Hypervisor
#
#  A Tiny Hypervisor for IoT Development
#
#  Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
#############################################################

#!/bin/bash

# build the aVisor hpervisor
make clean
make qemu 

# build the lrtos
cd guests/lrtos
make clean
make

# build the echo
cd ../echo
make clean
make

# build the Qemu boot img
cd ../../
cp guests/lrtos/lrtos.bin ./bin
cp guests/echo/echo.bin ./bin
cp guests/uboot/uboot.bin ./bin
cp guests/freertos/freertos.bin ./bin
# cp ~/work/tiny_raspi/build/images/Image ./bin
# cp ~/work/tiny_raspi/build/images/bcm2710-rpi-3-b.dtb ./bin/rasp3b.dtb
cp misc/Image ./bin
cp misc/bcm2710-rpi-3-b.dtb ./bin/rasp3b.dtb
cp misc/rootfs.cpio.gz ./bin/rootfs.gz
sudo modprobe nbd max_part=8
./scripts/create_sd.sh ./bin/avisor.img ./bin/rootfs.gz ./bin/uboot.bin ./bin/freertos.bin ./bin/Image ./bin/rasp3b.dtb

# Run the Demo
qemu-system-aarch64 -M raspi3b -nographic -serial null -serial mon:stdio -m 1024 -kernel ./bin/kernel8.img -drive file=./bin/avisor.img,if=sd,format=raw

