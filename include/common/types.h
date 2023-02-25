// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define __aligned(x)	__attribute__((aligned(x)))
#define __packed	__attribute__((packed))
#define __unused	__attribute__((unused))

#ifndef ASSEMBLER

/* Define standard data types.	These definitions allow software components
 * to perform in the same manner on different target platforms.
 */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef signed long int64_t;
// NOW #include <stdint.h>
// typedef unsigned int		size_t;
typedef __builtin_va_list va_list;

typedef _Bool bool;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef true
#define true  ((_Bool)1)
#define false ((_Bool)0)
#endif

#ifndef UINT64_MAX
#define UINT64_MAX (0xffffffffffffffffUL)
#endif

#ifndef UINT32_MAX
#define UINT32_MAX (0xffffffffU)
#endif

#endif /* ASSEMBLER */
