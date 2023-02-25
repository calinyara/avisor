// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

void uart_init(void);
void handle_uart_irq(void);
char uart_recv(void);
void uart_send(char c);
void putc(void *p, char c);
