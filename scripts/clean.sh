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

make clean

cd guests/lrtos
make clean

cd ../echo
make clean
