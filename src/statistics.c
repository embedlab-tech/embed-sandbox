/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-UART runtime statistics.
 */

#include "statistics.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* ------------------------------------------------------------------
 * Per-UART statistics state
 * --------------------------------------------------------------- */

struct stats_entry {
	uint64_t tx_bytes;
	uint64_t tx_messages;
	uint64_t dropped;
	uint32_t start_time;	/* k_uptime_get() value of first TX; 0 = not started */
	uint32_t baudrate;
};

static struct stats_entry stats[UART_COUNT];

/* ------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------- */

void stats_init(void)
{
	for (enum uart_id id = 0; id < UART_COUNT; id++) {
		stats[id].tx_bytes   = 0;
		stats[id].tx_messages = 0;
		stats[id].dropped    = 0;
		stats[id].start_time = 0;
		stats[id].baudrate   = 0;
	}
}

void stats_record_tx(enum uart_id id, size_t bytes)
{
	if (id >= UART_COUNT) {
		return;
	}

	struct stats_entry *e = &stats[id];

	if (e->tx_messages == 0) {
		e->start_time = k_uptime_get();
	}

	e->tx_messages++;
	e->tx_bytes += bytes;
}

void stats_record_drop(enum uart_id id)
{
	if (id >= UART_COUNT) {
		return;
	}

	stats[id].dropped++;
}

void stats_print_all(void)
{
	printk("%-12s %-12s %-12s %-12s %-12s\n",
	       "UART", "TX bytes", "TX msgs", "Dropped", "Baudrate");

	for (enum uart_id id = 0; id < UART_COUNT; id++) {
		const struct stats_entry *e = &stats[id];
		const struct uart_port *port = uart_get(id);
		const char *label = (port != NULL) ? port->label : "???";

		printk("%-12s %-12llu %-12llu %-12llu %-12u\n",
		       label,
		       (unsigned long long)e->tx_bytes,
		       (unsigned long long)e->tx_messages,
		       (unsigned long long)e->dropped,
		       e->baudrate);
	}
}

void stats_print(enum uart_id id)
{
	if (id >= UART_COUNT) {
		return;
	}

	const struct stats_entry *e = &stats[id];
	const struct uart_port *port = uart_get(id);
	const char *label = (port != NULL) ? port->label : "???";
	uint32_t active_ms = 0;

	if (e->start_time != 0) {
		active_ms = k_uptime_get() - e->start_time;
	}

	printk("%s statistics:\n", label);
	printk("  TX bytes:    %llu\n", (unsigned long long)e->tx_bytes);
	printk("  TX messages: %llu\n", (unsigned long long)e->tx_messages);
	printk("  Dropped:     %llu\n", (unsigned long long)e->dropped);
	printk("  Active time: %us\n", active_ms / 1000U);
	printk("  Baudrate:    %u\n", e->baudrate);
}

void stats_update_baud(enum uart_id id, uint32_t baud)
{
	if (id >= UART_COUNT) {
		return;
	}

	stats[id].baudrate = baud;
}
