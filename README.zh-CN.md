# aVisor Hypervisor

**[aVisor](https://calinyara.github.io/technology/2023/02/25/aVisor.html)** 是一个可运行在树莓派3上的Hypervisor。用以帮助理解ARM虚拟化的基本概念，学习Hypervisor和操作系统的实现原理。

参考  [**Armv8架构虚拟化介绍**](https://calinyara.github.io/technology/2019/11/03/armv8-virtualization.html)

**编译及运行**

```
./scripts/demo.sh		// 编译并运行demo
./scripts/clean.sh		// 清理
```

**控制台操作**

运行上述demo，将在aVisor运行4个虚拟机
- **[echo](https://github.com/calinyara/avisor/tree/main/guests/echo)**:  一个baremetal二进制程序，用以回显键盘输入
- **[lrtos](https://github.com/calinyara/avisor/tree/main/guests/lrtos)**: 一个微型操作系统，启动后运行两个用户态程序，一个打印“12345”， 另一个打印"abcde", 其内核支持简单调度
- **uboot**: 标准的 Das U-Boot Bootloader
- **[FreeRTOS](https://github.com/hacker-jie/freertos-raspi3)**: 该DEMO运行两个Task, 一个打印“12345”, 另一个打印"ABCDE"，由FreeRTOS调度运行 

启动后按回车进入aVisor控制台

```
help			// 打印帮助
vml			// 显示当前虚拟机信息
vmc <vm id>		// 从Hypervisor控制台切换到VM控制台，例如：vmc 3，切换到uboot控制台
@+0			// 从VM控制台切换回Hypervisor控制台
ls                      // 显示当前目录下文件(虚拟机镜像文件)
vmld <images> <load addr> <entry addr>          // 加载一个虚拟机镜像文件并运行
```

