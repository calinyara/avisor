// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

#include "common/types.h"

#define SHELL_CMD_MAX_LEN    100U
#define SHELL_STRING_MAX_LEN (PAGE_SIZE << 2U)

/* Shell Command Function */
typedef int32_t (*shell_cmd_fn_t)(int32_t argc, char **argv);

/* Shell Command */
struct shell_cmd {
	char *str; /* Command string */
	char *cmd_param; /* Command parameter string */
	char *help_str; /* Help text associated with the command */
	shell_cmd_fn_t fcn; /* Command call-back function */
};

/* Shell Control Block */
struct shell {
	char input_line[2][SHELL_CMD_MAX_LEN + 1U]; /* current & last */
	uint32_t input_line_len; /* Length of current input line */
	uint32_t input_line_active; /* Active input line index */
	struct shell_cmd *cmds; /* cmds supported */
	uint32_t cmd_count; /* Count of cmds supported */
};

/* Shell Command list with parameters and help description */
#define SHELL_CMD_HELP	     "help"
#define SHELL_CMD_HELP_PARAM NULL
#define SHELL_CMD_HELP_HELP  "Supported hypervisor shell commands"

#define SHELL_CMD_VML	    "vml"
#define SHELL_CMD_VML_PARAM NULL
#define SHELL_CMD_VML_HELP  "List all VMs"

#define SHELL_CMD_VMC	    "vmc"
#define SHELL_CMD_VMC_PARAM "<vm id>"
#define SHELL_CMD_VMC_HELP \
	"Switch to the VM's console. Use [@0] to return to the aVisor console"
