# aVisor Hypervisor

**[aVisor](https://github.com/calinyara/avisor)**  is a bare-metal hypervisor that runs on the Raspberry Pi 3. It can be used to learn about the basic concepts of ARM virtualization and the principles of hypervisors and operating systems.

<br>

**Compilation and QEMU Simulation**

```
./scripts/demo.sh		// Compile and run the demo
./scripts/clean.sh		// Clean the project
```

**Operation In the Console**

The above demo will run 4 Guest VMs on the Hypervisor. After startup, press ***Enter*** to go to the hypervisor's console.

- **[echo](https://github.com/calinyara/avisor/tree/main/guests/echo)**:  A baremetal binary that echoes keyboard input.
- **[lrtos](https://github.com/calinyara/avisor/tree/main/guests/lrtos)**:  A miniature operating system that runs two user mode processes after startup, one prints "12345" and the other prints "abcde". The lrtos kernel supports the process scheduling.
- **[uboot](https://github.com/u-boot/u-boot)**: The standard Das U-Boot Bootloader.
- **[FreeRTOS](https://github.com/hacker-jie/freertos-raspi3)**: The FreeRTOS VM runs two tasks, one prints "12345" and the other prints "ABCDE". The tasks are scheduled by FreeRTOS.

<br>

```
help			// Print the help
vml			// Display the current Guest VMs info
vmc <vm id>		// Switch from the hypervisor's console to a Guest VM's console
@+0			// Switch back to the hypervisor's console from a Guest VM's console
ls			// List all files (VM images)
vmld <images> <load addr> <entry addr>		// Load a VM image and run it
```

