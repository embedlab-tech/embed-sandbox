/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Per-UART runtime statistics.
 */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdint.h>
#include <stddef.h>

#include "uart_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise all per-UART statistics counters to zero.
 *
 * Must be called once before any other stats function.
 */
void stats_init(void);

/**
 * @brief Record a successful TX transmission.
 *
 * Increments tx_messages and tx_bytes.  On the very first call for a
 * given UART (when tx_messages was zero) the current uptime is captured
 * as the port's start time.
 *
 * @param id    Logical UART identifier.
 * @param bytes Number of bytes transmitted.
 */
void stats_record_tx(enum uart_id id, size_t bytes);

/**
 * @brief Record a failed TX attempt.
 *
 * Increments the dropped counter for the given UART.
 *
 * @param id  Logical UART identifier.
 */
void stats_record_drop(enum uart_id id);

/**
 * @brief Print an aggregate statistics table for all UARTs.
 *
 * Output format (via printk):
 *   UART        TX bytes    TX msgs     Dropped     Baudrate
 *   USART1      1234567     12345       0           115200
 *   ...
 */
void stats_print_all(void);

/**
 * @brief Print detailed statistics for a single UART.
 *
 * Output format (via printk):
 *   USART1 statistics:
 *     TX bytes:    1234567
 *     TX messages: 12345
 *     Dropped:     0
 *     Active time: 123s
 *     Baudrate:    115200
 *
 * @param id  Logical UART identifier.
 */
void stats_print(enum uart_id id);

/**
 * @brief Update the tracked baudrate for a UART.
 *
 * Called by the shell/control layer whenever the baud rate changes.
 *
 * @param id   Logical UART identifier.
 * @param baud Current baud rate in bps.
 */
void stats_update_baud(enum uart_id id, uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif /* STATISTICS_H */
