// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/shell.h"
#include "common/errno.h"
#include "common/mini_uart.h"
#include "common/mm.h"
#include "common/printf.h"
#include "common/task.h"
#include "common/utils.h"
#include "common/loader.h"
#include "shell_priv.h"

#define MAX_STR_SIZE		    256U
#define SHELL_PROMPT_STR	    "avisor:/# "
#define CODE_AVOID_SHELL_PROMPT_STR 0xFFFF

#define SHELL_LOG_BUF_SIZE (PAGE_SIZE * 2)

/* Input Line Other - Switch to the "other" input line (there are only two
 * input lines total).
 */
#define SHELL_INPUT_LINE_OTHER(v) (((v) + 1U) & 0x1U)

static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv);
static int32_t shell_cmd_vml(__unused int32_t argc, __unused char **argv);
static int32_t shell_cmd_vmc(int32_t argc, char **argv);
static int32_t shell_cmd_vmld(int32_t argc, char **argv);

static struct shell_cmd shell_cmds[] = {
	{
		.str = SHELL_CMD_HELP,
		.cmd_param = SHELL_CMD_HELP_PARAM,
		.help_str = SHELL_CMD_HELP_HELP,
		.fcn = shell_cmd_help,
	},
	{
		.str = SHELL_CMD_VML,
		.cmd_param = SHELL_CMD_VML_PARAM,
		.help_str = SHELL_CMD_VML_HELP,
		.fcn = shell_cmd_vml,
	},
	{
		.str = SHELL_CMD_VMC,
		.cmd_param = SHELL_CMD_VMC_PARAM,
		.help_str = SHELL_CMD_VMC_HELP,
		.fcn = shell_cmd_vmc,
	},
	{
		.str = SHELL_CMD_VMLD,
		.cmd_param = SHELL_CMD_VMLD_PARAM,
		.help_str = SHELL_CMD_VMLD_HELP,
		.fcn = shell_cmd_vmld,
	},
};

static struct shell hv_shell;
static struct shell *p_shell = &hv_shell;

static int32_t string_to_argv(char *argv_str, void *p_argv_mem,
			      __unused uint32_t argv_mem_size, uint32_t *p_argc,
			      char ***p_argv)
{
	uint32_t argc;
	char **argv;
	char *p_ch;

	/* Setup initial argument values. */
	argc = 0U;
	argv = NULL;

	/* Ensure there are arguments to be processed. */
	if (argv_str == NULL) {
		*p_argc = argc;
		*p_argv = argv;
		return -EINVAL;
	}

	/* Process the argument string (there is at least one element). */
	argv = (char **)p_argv_mem;
	p_ch = argv_str;

	/* Remove all spaces at the beginning of cmd*/
	while (*p_ch == ' ')
		p_ch++;

	while (*p_ch != 0) {
		/* Add argument (string) pointer to the vector. */
		argv[argc] = p_ch;

		/* Move past the vector entry argument string (in the
		 * argument string).
		 */
		while ((*p_ch != ' ') && (*p_ch != ',') && (*p_ch != 0))
			p_ch++;

		/* Count the argument just processed. */
		argc++;

		/* Check for the end of the argument string. */
		if (*p_ch != 0) {
			/* Terminate the vector entry argument string
			 * and move to the next.
			 */
			*p_ch = 0;
			/* Remove all space in middile of cmdline */
			p_ch++;
			while (*p_ch == ' ')
				p_ch++;
		}
	}

	/* Update return parameters */
	*p_argc = argc;
	*p_argv = argv;

	return 0;
}

static struct shell_cmd *shell_find_cmd(const char *cmd_str)
{
	uint32_t i;

	struct shell_cmd *p_cmd = NULL;

	for (i = 0U; i < p_shell->cmd_count; i++) {
		p_cmd = &p_shell->cmds[i];
		if (strcmp(p_cmd->str, cmd_str) == 0)
			return p_cmd;
	}

	return NULL;
}

static char shell_getc(void)
{
	return uart_recv();
}

static void shell_putc(char ch)
{
	uart_send(ch);
}

static void shell_puts(const char *string_ptr)
{
	/* Output the string */
	printf(string_ptr);
}

static void shell_handle_special_char(char ch)
{
	switch (ch) {
	/* Escape character */
	case 0x1b:
		/* Consume the next 2 characters */
		(void)shell_getc();
		(void)shell_getc();
		break;
	default:
		/*
		 * Only the Escape character is treated as special character.
		 * All the other characters have been handled properly in
		 * shell_input_line, so they will not be handled in this API.
		 * Gracefully return if prior case clauses have not been met.
		 */
		break;
	}
}

static bool shell_input_line(void)
{
	bool done = false;
	char ch;

	ch = shell_getc();

	/* Check character */
	switch (ch) {
	/* Backspace, \b is 8, but when press Backspace, get ch = 127 */
	case '\b':
	case 127:
		/* Ensure length is not 0 */
		if (p_shell->input_line_len > 0U) {
			/* Reduce the length of the string by one */
			p_shell->input_line_len--;

			/* Null terminate the last character to erase it */
			p_shell->input_line[p_shell->input_line_active]
					   [p_shell->input_line_len] = 0;

			/* Echo backspace */
			shell_putc('\b');

			/* Send a space + backspace sequence to delete
			 * character
			 */
			shell_puts(" \b");
		}
		break;

	/* Carriage-return */
	case '\r':
		/* Echo carriage return / line feed */
		shell_puts("\r\n");

		/* Set flag showing line input done */
		done = true;

		/* Reset command length for next command processing */
		p_shell->input_line_len = 0U;
		break;

	/* Line feed */
	case '\n':
		/* Do nothing */
		break;

	/* All other characters */
	default:
		/* Ensure data doesn't exceed full terminal width */
		if (p_shell->input_line_len < SHELL_CMD_MAX_LEN) {
			/* See if a "standard" prINTable ASCII character received */
			if ((ch >= 32) && (ch <= 126)) {
				/* Add character to string */
				p_shell->input_line[p_shell->input_line_active]
						   [p_shell->input_line_len] =
					ch;
				/* Echo back the input */
				shell_puts(&p_shell->input_line
						    [p_shell->input_line_active]
						    [p_shell->input_line_len]);

				/* Move to next character in string */
				p_shell->input_line_len++;
			} else {
				/* call special character handler */
				shell_handle_special_char(ch);
			}
		} else {
			/* Echo carriage return / line feed */
			shell_puts("\r\n");

			/* Set flag showing line input done */
			done = true;

			/* Reset command length for next command processing */
			p_shell->input_line_len = 0U;
		}
		break;
	}

	return done;
}

static int32_t shell_process_cmd(const char *p_input_line)
{
	int32_t status = -EINVAL;
	struct shell_cmd *p_cmd;
	char cmd_argv_str[SHELL_CMD_MAX_LEN + 1U];
	int32_t cmd_argv_mem[sizeof(char *) * ((SHELL_CMD_MAX_LEN + 1U) >> 1U)];
	int32_t cmd_argc;
	char **cmd_argv;

	/* Copy the input line INTo an argument string to become part of the
	 * argument vector.
	 */
	(void)strncpy(&cmd_argv_str[0], p_input_line, SHELL_CMD_MAX_LEN);
	cmd_argv_str[SHELL_CMD_MAX_LEN] = 0;

	/* Build the argv vector from the string. The first argument in the
	 * resulting vector will be the command string itself.
	 */

	/* NOTE: This process is destructive to the argument string! */

	(void)string_to_argv(&cmd_argv_str[0], (void *)&cmd_argv_mem[0],
			     sizeof(cmd_argv_mem), (void *)&cmd_argc,
			     &cmd_argv);

	/* Determine if there is a command to process. */
	if (cmd_argc != 0) {
		/* See if command is in cmds supported */
		p_cmd = shell_find_cmd(cmd_argv[0]);
		if (p_cmd == NULL) {
			shell_puts("Error: Invalid command.\r\n");
			return -EINVAL;
		}

		status = p_cmd->fcn(cmd_argc, &cmd_argv[0]);
		if (status == -EINVAL) {
			shell_puts("Error: Invalid parameters.\r\n");
		} else if (status == CODE_AVOID_SHELL_PROMPT_STR) {
			/* do nothing */
		} else if (status != 0) {
			shell_puts("Command launch failed.\r\n");
		} else {
			/* No other state currently, do nothing */
		}
	}

	return status;
}

static int32_t shell_process(void)
{
	int32_t status;
	char *p_input_line;

	/* Check for the repeat command character in active input line.
	 */
	if (p_shell->input_line[p_shell->input_line_active][0] == '.') {
		/* Repeat the last command (using inactive input line).
		 */
		p_input_line = &p_shell->input_line[SHELL_INPUT_LINE_OTHER(
			p_shell->input_line_active)][0];
	} else {
		/* Process current command (using active input line). */
		p_input_line =
			&p_shell->input_line[p_shell->input_line_active][0];

		/* Switch active input line. */
		p_shell->input_line_active =
			SHELL_INPUT_LINE_OTHER(p_shell->input_line_active);
	}

	/* Process command */
	status = shell_process_cmd(p_input_line);

	/* Now that the command is processed, zero fill the input buffer */
	(void)memset((void *)p_shell->input_line[p_shell->input_line_active], 0,
		     SHELL_CMD_MAX_LEN + 1U);

	/* Process command and return result to caller */
	return status;
}

void shell_kick(void)
{
	static bool is_cmd_cmplt = false;
	int32_t status = 0;

	/* Get user's input */
	is_cmd_cmplt = shell_input_line();

	/* If user has pressed the ENTER then process
	 * the command
	 */
	if (is_cmd_cmplt) {
		/* Process current input line. */
		status = shell_process();
	}

	if (is_cmd_cmplt && status != CODE_AVOID_SHELL_PROMPT_STR)
		shell_puts(SHELL_PROMPT_STR);
}

void shell_init(void)
{
	p_shell->cmds = shell_cmds;
	p_shell->cmd_count = ARRAY_SIZE(shell_cmds);

	/* Zero fill the input buffer */
	(void)memset((void *)p_shell->input_line[p_shell->input_line_active],
		     0U, SHELL_CMD_MAX_LEN + 1U);
}

#define SHELL_ROWS     30
#define MAX_OUTPUT_LEN 80
static int32_t shell_cmd_help(__unused int32_t argc, __unused char **argv)
{
	struct shell_cmd *p_cmd = NULL;

	char str[MAX_STR_SIZE];
	char *help_str;
	/* Print title */
	shell_puts("Registered Commands:\r\n");

	/* Proceed based on the number of registered commands. */
	if (p_shell->cmd_count == 0U) {
		/* No registered commands */
		shell_puts("NONE\r\n");
	} else {
		int32_t i = 0;
		uint32_t j;

		for (j = 0U; j < p_shell->cmd_count; j++) {
			p_cmd = &p_shell->cmds[j];

			/* Check if we've filled the screen with info */
			/* i + 1 used to avoid 0%SHELL_ROWS=0 */
			if (((i + 1) % SHELL_ROWS) == 0) {
				/* Pause before we continue on to the next
				 * page.
				 */

				/* Print message to the user. */
				shell_puts("<*** Hit any key to continue ***>");

				/* Wait for a character from user (NOT USED) */
				(void)shell_getc();

				/* Print a new line after the key is hit. */
				shell_puts("\r\n");
			}

			i++;
			if (p_cmd->cmd_param == NULL)
				p_cmd->cmd_param = " ";
			(void)memset(str, ' ', sizeof(str));
			shell_puts("\r\n");
			/* Output the command & parameter string */
			snprintf(str, MAX_OUTPUT_LEN, " %-15s%-64s", p_cmd->str,
				 p_cmd->cmd_param);
			shell_puts(str);
			shell_puts("\r\n");

			help_str = p_cmd->help_str;
			while (strnlen(help_str, MAX_OUTPUT_LEN) > 0) {
				(void)memset(str, ' ', sizeof(str));
				if (strnlen(help_str, MAX_OUTPUT_LEN) > 71) {
					snprintf(str, MAX_OUTPUT_LEN,
						 "        %-s", help_str);
					shell_puts(str);
					shell_puts("\r\n");
					help_str = help_str + 71;
				} else {
					snprintf(str, MAX_OUTPUT_LEN,
						 "        %-s", help_str);
					shell_puts(str);
					shell_puts("\r\n");
					break;
				}
			}
		}
	}

	shell_puts("\r\n");

	return 0;
}

static int32_t shell_cmd_vml(__unused int32_t argc, __unused char **argv)
{
	show_task_list();
	return 0;
}

/*
 * Return 0 for HV, 1 for Guests.
 */
static int32_t shell_cmd_vmc(int32_t argc, char **argv)
{
	char temp_str[60];
	uint16_t tsk_id = 0U;
	struct task_struct *tsk;

	if (argc == 2)
		tsk_id = (uint16_t)strtol_deci(argv[1]);

	/* This is HV. Nothing need to do, just return 0 */
	if (tsk_id == 0)
		return 0;

	if (tsk_id > nr_tasks - 1)
		return -EINVAL;

	/* Output that switching to Service VM shell */
	snprintf(temp_str, MAX_OUTPUT_LEN, "--> Entering VM %d Shell <--\r\n",
		 tsk_id);
	shell_puts(temp_str);

	uart_forwarded_task = tsk_id;
	tsk = task[uart_forwarded_task];
	flush_task_console(tsk);
	if (tsk->state == TASK_RUNNING)
		flush_task_console(tsk);

	return CODE_AVOID_SHELL_PROMPT_STR;
}

struct raw_binary_loader_args bl_args;
static int32_t shell_cmd_vmld(int32_t argc, char **argv)
{
	uint64_t load_addr;
	uint64_t entry_addr;
	char *end;

	if (argc != 4)
		return -EINVAL;

	load_addr = strtoul(argv[2], &end, 16);
	if (*end) {
		printf("Error: %s is not a pure hex number!\n", argv[2]);
		return -EINVAL;
	}

	entry_addr = strtoul(argv[3], &end, 16);
	if (*end) {
		printf("Error: %s is not a pure hex number!\n", argv[3]);
		return -EINVAL;
	}

	(void)strncpy(bl_args.filename, argv[1], 36);
	bl_args.load_addr = load_addr;
	bl_args.entry_point = entry_addr;

	if (create_task(raw_binary_loader, &bl_args) < 0) {
		printf("error while starting task\n");
		return -EFAULT;
	}

	return 0;
}

