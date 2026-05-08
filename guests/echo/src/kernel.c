#include "mini_uart.h"
#include "printf.h"
#include "utils.h"

void kernel_main(void)
{
	uart_init();
	init_printf(0, putc);

	printf("ECHO OS: echo your input...\n");

	while (1) {
		char c = uart_recv();
		if (c == '\n' || c == '\r') {
			uart_send('\r');
			uart_send('\n');
		} else {
			uart_send(c);
		}
	}
}
