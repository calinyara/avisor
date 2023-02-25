// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

struct fifo;

int is_empty_fifo(struct fifo *);
int is_full_fifo(struct fifo *);
struct fifo *create_fifo(void);
void clear_fifo(struct fifo *fifo);
int enqueue_fifo(struct fifo *, unsigned long);
int dequeue_fifo(struct fifo *, unsigned long *);
int used_of_fifo(struct fifo *fifo);
