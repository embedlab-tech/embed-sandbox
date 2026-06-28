/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * UART Manager — logical abstraction for generator UARTs
 *
 * Manages three generator UARTs (USART1, USART3, USART4).
 * USART2 (shell/console) is excluded.
 */

#ifndef UART_MANAGER_H
#define UART_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Logical UART identifiers for the three generator ports. */
enum uart_id {
	UART_GEN1 = 0,	/**< USART1 */
	UART_GEN2,	/**< USART3 */
	UART_GEN3,	/**< USART4 */
	UART_COUNT	/**< Number of generator UARTs */
};

/** Runtime state for a single generator UART port. */
struct uart_port {
	const struct device *dev;	/**< Zephyr device pointer */
	const char *label;		/**< Human-readable label (e.g. "USART1") */
	uint32_t baudrate;		/**< Current baud rate in bps */
	bool enabled;			/**< Port is enabled and usable */
};

/**
 * @brief Initialise the UART manager.
 *
 * Resolves all three generator UARTs from the devicetree, validates
 * readiness, records default baud rates, and marks ports as enabled.
 * Prints per-port status via printk().
 *
 * @retval 0 on success.
 * @retval negative if one or more UARTs could not be resolved or are
 *         not ready (the manager still initialises the working subset).
 */
int uart_manager_init(void);

/**
 * @brief Get a pointer to the port descriptor for a logical UART.
 *
 * @param id  Logical UART identifier.
 *
 * @return Pointer to the @ref uart_port descriptor, or NULL on invalid id.
 */
const struct uart_port *uart_get(enum uart_id id);

/**
 * @brief Change the baud rate of a UART port.
 *
 * Reads the current UART configuration, updates the baud rate, and
 * applies the new configuration via uart_configure().
 *
 * @param id    Logical UART identifier.
 * @param baud  Desired baud rate in bps.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p id is out of range.
 * @retval -ENODEV if the port is disabled or the device is not ready.
 * @retval negative errno from uart_configure() on failure.
 */
int uart_set_baud(enum uart_id id, uint32_t baud);

/**
 * @brief Enable a UART port.
 *
 * Marks the port as enabled so that uart_send() will accept data.
 *
 * @param id  Logical UART identifier.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p id is out of range.
 */
int uart_enable(enum uart_id id);

/**
 * @brief Disable a UART port.
 *
 * Marks the port as disabled; uart_send() will return -ENODEV.
 *
 * @param id  Logical UART identifier.
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p id is out of range.
 */
int uart_disable(enum uart_id id);

/**
 * @brief Send data on a UART port.
 *
 * Transmits @p len bytes from @p buf via the UART hardware.
 *
 * @param id  Logical UART identifier.
 * @param buf Pointer to data buffer.
 * @param len Number of bytes to transmit.
 *
 * @retval 0 on success (or positive number of bytes sent).
 * @retval -EINVAL if @p id is out of range.
 * @retval -ENODEV if the port is disabled or device not ready.
 * @retval -EIO if uart_tx() fails.
 */
int uart_send(enum uart_id id, const uint8_t *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UART_MANAGER_H */
