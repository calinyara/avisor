// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

#include "common/sched.h"
#include "common/types.h"
#include <stdint.h>

#define MBOX_REQUEST 0

typedef enum {
        CLK_EMMC_ID  = 0x1, // Mailbox Tag Channel EMMC clock ID
        CLK_UART_ID  = 0x2, // Mailbox Tag Channel uart clock ID
        CLK_ARM_ID   = 0x3, // Mailbox Tag Channel ARM clock ID
        CLK_CORE_ID  = 0x4, // Mailbox Tag Channel SOC core clock ID
        CLK_V3D_ID   = 0x5, // Mailbox Tag Channel V3D clock ID
        CLK_H264_ID  = 0x6, // Mailbox Tag Channel H264 clock ID
        CLK_ISP_ID   = 0x7, // Mailbox Tag Channel ISP clock ID
        CLK_SDRAM_ID = 0x8, // Mailbox Tag Channel SDRAM clock ID
        CLK_PIXEL_ID = 0x9, // Mailbox Tag Channel PIXEL clock ID
        CLK_PWM_ID   = 0xA, // Mailbox Tag Channel PWM clock ID
} MB_CLOCK_ID;

typedef enum {
        MBOX_CHAN_POWER = 0x0, // Mailbox Channel 0: Power Management Interface
        MBOX_CHAN_FB    = 0x1, // Mailbox Channel 1: Frame Buffer
        MBOX_CHAN_VUART = 0x2, // Mailbox Channel 2: Virtual UART
        MBOX_CHAN_VCHIQ = 0x3, // Mailbox Channel 3: VCHIQ Interface
        MBOX_CHAN_LEDS  = 0x4, // Mailbox Channel 4: LEDs Interface
        MBOX_CHAN_BUTTONS = 0x5, // Mailbox Channel 5: Buttons Interface
        MBOX_CHAN_TOUCH   = 0x6, // Mailbox Channel 6: Touchscreen Interface
        MBOX_CHAN_COUNT   = 0x7, // Mailbox Channel 7: Counter
        MBOX_CHAN_TAGS    = 0x8, // Mailbox Channel 8: Tags (ARM to VC)
        MBOX_CHAN_GPU     = 0x9, // Mailbox Channel 9: GPU (VC to ARM)
} MBOX_CHAN;

typedef enum {
        /* Videocore info commands */
        MBOX_TAG_GET_VERSION = 0x00000001, // Get firmware revision

        /* Hardware info commands */
        MBOX_TAG_GET_BOARD_MODEL       = 0x00010001, // Get board model
        MBOX_TAG_GET_BOARD_REVISION    = 0x00010002, // Get board revision
        MBOX_TAG_GET_BOARD_MAC_ADDRESS = 0x00010003, // Get board MAC address
        MBOX_TAG_GET_BOARD_SERIAL      = 0x00010004, // Get board serial
        MBOX_TAG_GET_ARM_MEMORY        = 0x00010005, // Get ARM memory
        MBOX_TAG_GET_VC_MEMORY         = 0x00010006, // Get VC memory
        MBOX_TAG_GET_CLOCKS            = 0x00010007, // Get clocks

        /* Power commands */
        MBOX_TAG_GET_POWER_STATE = 0x00020001, // Get power state
        MBOX_TAG_GET_TIMING      = 0x00020002, // Get timing
        MBOX_TAG_SET_POWER_STATE = 0x00028001, // Set power state

        /* GPIO commands */
        MBOX_TAG_GET_GET_GPIO_STATE = 0x00030041, // Get GPIO state
        MBOX_TAG_SET_GPIO_STATE     = 0x00038041, // Set GPIO state

        /* Clock commands */
        MBOX_TAG_GET_CLOCK_STATE    = 0x00030001, // Get clock state
        MBOX_TAG_GET_CLOCK_RATE     = 0x00030002, // Get clock rate
        MBOX_TAG_GET_MAX_CLOCK_RATE = 0x00030004, // Get max clock rate
        MBOX_TAG_GET_MIN_CLOCK_RATE = 0x00030007, // Get min clock rate
        MBOX_TAG_GET_TURBO          = 0x00030009, // Get turbo

        MBOX_TAG_SET_CLOCK_STATE = 0x00038001, // Set clock state
        MBOX_TAG_SET_CLOCK_RATE  = 0x00038002, // Set clock rate
        MBOX_TAG_SET_TURBO       = 0x00038009, // Set turbo

        /* Voltage commands */
        MBOX_TAG_GET_VOLTAGE     = 0x00030003, // Get voltage
        MBOX_TAG_GET_MAX_VOLTAGE = 0x00030005, // Get max voltage
        MBOX_TAG_GET_MIN_VOLTAGE = 0x00030008, // Get min voltage

        MBOX_TAG_SET_VOLTAGE = 0x00038003, // Set voltage

        /* Temperature commands */
        MBOX_TAG_GET_TEMPERATURE     = 0x00030006, // Get temperature
        MBOX_TAG_GET_MAX_TEMPERATURE = 0x0003000A, // Get max temperature

        /* Memory commands */
        MBOX_TAG_ALLOCATE_MEMORY = 0x0003000C, // Allocate Memory
        MBOX_TAG_LOCK_MEMORY     = 0x0003000D, // Lock memory
        MBOX_TAG_UNLOCK_MEMORY   = 0x0003000E, // Unlock memory
        MBOX_TAG_RELEASE_MEMORY  = 0x0003000F, // Release Memory

        /* Execute code commands */
        MBOX_TAG_EXECUTE_CODE = 0x00030010, // Execute code

        /* QPU control commands */
        MBOX_TAG_EXECUTE_QPU = 0x00030011, // Execute code on QPU
        MBOX_TAG_ENABLE_QPU  = 0x00030012, // QPU enable

        /* Displaymax commands */
        MBOX_TAG_GET_DISPMANX_HANDLE = 0x00030014, // Get displaymax handle
        MBOX_TAG_GET_EDID_BLOCK      = 0x00030020, // Get HDMI EDID block

        /* SD Card commands */
        MBOX_GET_SDHOST_CLOCK = 0x00030042, // Get SD Card EMCC clock
        MBOX_SET_SDHOST_CLOCK = 0x00038042, // Set SD Card EMCC clock

        /* Framebuffer commands */
        MBOX_TAG_ALLOCATE_FRAMEBUFFER =
                0x00040001, // Allocate Framebuffer address
        MBOX_TAG_BLANK_SCREEN = 0x00040002, // Blank screen
        MBOX_TAG_GET_PHYSICAL_WIDTH_HEIGHT =
                0x00040003, // Get physical screen width/height
        MBOX_TAG_GET_VIRTUAL_WIDTH_HEIGHT =
                0x00040004, // Get virtual screen width/height
        MBOX_TAG_GET_COLOUR_DEPTH = 0x00040005, // Get screen colour depth
        MBOX_TAG_GET_PIXEL_ORDER  = 0x00040006, // Get screen pixel order
        MBOX_TAG_GET_ALPHA_MODE   = 0x00040007, // Get screen alpha mode
        MBOX_TAG_GET_PITCH        = 0x00040008, // Get screen line to line pitch
        MBOX_TAG_GET_VIRTUAL_OFFSET = 0x00040009, // Get screen virtual offset
        MBOX_TAG_GET_OVERSCAN       = 0x0004000A, // Get screen overscan value
        MBOX_TAG_GET_PALETTE        = 0x0004000B, // Get screen palette

        MBOX_TAG_RELEASE_FRAMEBUFFER =
                0x00048001, // Release Framebuffer address
        MBOX_TAG_SET_PHYSICAL_WIDTH_HEIGHT =
                0x00048003, // Set physical screen width/heigh
        MBOX_TAG_SET_VIRTUAL_WIDTH_HEIGHT =
                0x00048004, // Set virtual screen width/height
        MBOX_TAG_SET_COLOUR_DEPTH   = 0x00048005, // Set screen colour depth
        MBOX_TAG_SET_PIXEL_ORDER    = 0x00048006, // Set screen pixel order
        MBOX_TAG_SET_ALPHA_MODE     = 0x00048007, // Set screen alpha mode
        MBOX_TAG_SET_VIRTUAL_OFFSET = 0x00048009, // Set screen virtual offset
        MBOX_TAG_SET_OVERSCAN       = 0x0004800A, // Set screen overscan value
        MBOX_TAG_SET_PALETTE        = 0x0004800B, // Set screen palette
        MBOX_TAG_SET_VSYNC          = 0x0004800E, // Set screen VSync
        MBOX_TAG_SET_BACKLIGHT      = 0x0004800F, // Set screen backlight

        /* VCHIQ commands */
        MBOX_TAG_VCHIQ_INIT = 0x00048010, // Enable VCHIQ

        /* Config commands */
        MBOX_TAG_GET_COMMAND_LINE = 0x00050001, // Get command line

        /* Shared resource management commands */
        MBOX_TAG_GET_DMA_CHANNELS = 0x00060001, // Get DMA channels

        /* Cursor commands */
        MBOX_TAG_SET_CURSOR_INFO  = 0x00008010, // Set cursor info
        MBOX_TAG_SET_CURSOR_STATE = 0x00008011, // Set cursor state

        MBOX_TAG_LAST = 0x0, // Tag for the end
} MBOX_TAG;

bool is_mbox_addr(uint64_t addr);
uint64_t handle_mbox_read(struct task_struct *tsk, uint64_t addr);
uint64_t handle_mbox_write(struct task_struct *tsk, uint64_t addr, uint64_t val);

