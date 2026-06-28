/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * UART Manager — logical abstraction for generator UARTs
 */

#include "uart_manager.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

/* ------------------------------------------------------------------
 * Port table — statically allocated, indexed by enum uart_id
 * ------------------------------------------------------------------ */
static struct uart_port ports[UART_COUNT];

static const char * const port_labels[UART_COUNT] = {
	[UART_GEN1] = "USART1",
	[UART_GEN2] = "USART3",
	[UART_GEN3] = "USART4",
};

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */
static bool id_valid(enum uart_id id)
{
	return (int)id >= 0 && id < UART_COUNT;
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

int uart_manager_init(void)
{
	/* Resolve devicetree nodes */
	const struct device *devs[UART_COUNT];

	devs[UART_GEN1] = DEVICE_DT_GET(DT_NODELABEL(usart1));
	devs[UART_GEN2] = DEVICE_DT_GET(DT_NODELABEL(usart3));
	devs[UART_GEN3] = DEVICE_DT_GET(DT_NODELABEL(usart4));

	int failures = 0;
	int ret;

	for (enum uart_id id = UART_GEN1; id < UART_COUNT; id++) {
		const struct device *dev = devs[id];

		ports[id].dev = dev;
		ports[id].label = port_labels[id];
		ports[id].enabled = false;
		ports[id].baudrate = 0;

		if (!device_is_ready(dev)) {
			printk("UART %s: NOT READY\n", port_labels[id]);
			failures++;
			continue;
		}

		/* Read initial configuration to record the baud rate */
		struct uart_config cfg;
		ret = uart_config_get(dev, &cfg);
		if (ret == 0) {
			ports[id].baudrate = cfg.baudrate;
		} else {
			/* Default to 115200 if we can't read config */
			ports[id].baudrate = 115200;
		}

		ports[id].enabled = true;

		printk("UART %s: ready, baud=%u\n",
		       port_labels[id], (unsigned int)ports[id].baudrate);
	}

	return failures ? -EIO : 0;
}

const struct uart_port *uart_get(enum uart_id id)
{
	if (!id_valid(id)) {
		return NULL;
	}
	return &ports[id];
}

int uart_set_baud(enum uart_id id, uint32_t baud)
{
	if (!id_valid(id)) {
		return -EINVAL;
	}

	struct uart_port *port = &ports[id];

	if (!port->enabled || !device_is_ready(port->dev)) {
		return -ENODEV;
	}

	/* Read current config, update baud, write back */
	struct uart_config cfg;
	int ret = uart_config_get(port->dev, &cfg);
	if (ret != 0) {
		return ret;
	}

	cfg.baudrate = baud;

	ret = uart_configure(port->dev, &cfg);
	if (ret != 0) {
		return ret;
	}

	port->baudrate = baud;
	return 0;
}

int uart_enable(enum uart_id id)
{
	if (!id_valid(id)) {
		return -EINVAL;
	}

	ports[id].enabled = device_is_ready(ports[id].dev);
	return 0;
}

int uart_disable(enum uart_id id)
{
	if (!id_valid(id)) {
		return -EINVAL;
	}

	ports[id].enabled = false;
	return 0;
}

int uart_send(enum uart_id id, const uint8_t *buf, size_t len)
{
	if (!id_valid(id)) {
		return -EINVAL;
	}

	const struct uart_port *port = &ports[id];

	if (!port->enabled || !device_is_ready(port->dev)) {
		return -ENODEV;
	}

	int ret = uart_tx(port->dev, buf, len, SYS_FOREVER_MS);
	if (ret != 0) {
		return -EIO;
	}

	return 0;
}
