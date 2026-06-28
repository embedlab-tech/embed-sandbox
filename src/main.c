/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * embed-sandbox — STM32 UART log generator
 *
 * Generates configurable UART log traffic on multiple interfaces
 * to exercise a desktop log collection tool.
 */

#include "uart_manager.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);


int main(void)
{
	LOG_INF("Embed-sandbox starting (board: %s)", CONFIG_BOARD);

	printk("\n=== embed-sandbox ===\n");
	printk("Board: %s\n", CONFIG_BOARD);
	printk("Type 'help' for available commands.\n\n");

	int err = uart_manager_init();
	if (err) {
		LOG_WRN("%d generator UART(s) not available", err);
	}

	/* Brief TX test on each enabled generator UART */
	for (enum uart_id id = UART_GEN1; id < UART_COUNT; id++) {
		const struct uart_port *port = uart_get(id);
		if (!port || !port->enabled) {
			continue;
		}
		char msg[64];
		int len = snprintk(msg, sizeof(msg),
				  "[%s] Generator online\r\n",
				  port->label);
		if (len > 0) {
			uart_send(id, (const uint8_t *)msg, len);
		}
	}

	while (1) {
		k_sleep(K_SECONDS(1));
	}

	return 0;
}
