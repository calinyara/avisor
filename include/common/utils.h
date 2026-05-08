// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/types.h"
#include <stddef.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* MACRO related to string */
#define ULONG_MAX ((uint64_t)(~0UL)) /* 0xFFFFFFFF */
#define LONG_MAX  (ULONG_MAX >> 1U) /* 0x7FFFFFFF */
#define LONG_MIN  (~LONG_MAX) /* 0x80000000 */

struct cpu_sysregs;

void memzero(void *, size_t);
void memcpy(void *, const void *, size_t);

extern void delay(unsigned long);
extern void put32(unsigned long, unsigned int);
extern unsigned int get32(unsigned long);
extern unsigned long get_el(void);
extern void set_stage2_pgd(unsigned long, unsigned long);
extern uint64_t get_current_cpu_id(void);
extern void restore_sysregs(struct cpu_sysregs *);
extern void save_sysregs(struct cpu_sysregs *);
extern void get_all_sysregs(struct cpu_sysregs *);
extern void assert_vfiq(void);
extern void assert_virq(void);
extern void assert_vserror(void);
extern void clear_vfiq(void);
extern void clear_virq(void);
extern void clear_vserror(void);
extern unsigned long translate_el1(unsigned long);
extern void dcache_invalidate_range(unsigned long addr, unsigned long size);
extern void dcache_clean_range(unsigned long addr, unsigned long size);
extern void dcache_clean_invalidate_range(unsigned long addr, unsigned long size);

int abs(int);
char *strncpy(char *, const char *, size_t);
size_t strnlen(const char *, size_t);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t len);
char *strdup(const char *);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memchr(const void *, int, size_t);
char *strchr(const char *, int);
char *strcpy(char *, const char *);
char *strncat(char *, const char *, size_t);
char *strcat(char *, const char *);
int isdigit(int);
int isspace(int);
int toupper(int);
int tolower(int);
int64_t strtol_deci(const char *nptr);
unsigned long strtoul(const char *str, char **endptr, int base);

static inline void mmio_write32(uint64_t reg, uint32_t data)
{
    *(volatile uint32_t *) reg = data;
}

static inline uint32_t mmio_read32(uint64_t reg)
{
    return *(volatile uint32_t *) reg;
}

