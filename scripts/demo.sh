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
sudo modprobe nbd max_part=8
./scripts/create_sd.sh ./bin/avisor.img ./bin/lrtos.bin ./bin/echo.bin ./bin/uboot.bin

# Run the Demo
qemu-system-aarch64 -M raspi3b -nographic -serial null -serial mon:stdio -m 1024 -kernel ./bin/kernel8.img -drive file=./bin/avisor.img,if=sd,format=raw

