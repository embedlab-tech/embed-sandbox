/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * embed-sandbox — Log generator core API
 */

#ifndef GENERATOR_H
#define GENERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/** Generator logical IDs — mirrors uart_id from uart_manager.h */
enum gen_id {
	GEN1 = 0,  /**< USART1 */
	GEN2,      /**< USART3 */
	GEN3,      /**< USART4 */
	GEN_COUNT
};

/** Per-generator configuration parameters */
struct gen_config {
	bool     enabled;
	uint32_t interval_ms;
	uint16_t msg_size;
	uint8_t  template_id;
	bool     random_mode;
};

/** Per-generator runtime state */
struct gen_state {
	uint64_t         counter;
	struct gen_config cfg;
	struct k_thread  thread;
	struct k_sem     wake;
};

/**
 * Initialize all generators to default (disabled) state.
 *
 * Must be called once before any other generator API.  Assumes
 * uart_manager_init() has already succeeded.
 *
 * @retval 0 on success
 */
int gen_init(void);

/**
 * Start a generator thread.
 *
 * The thread runs until stopped via gen_stop().  Calling start on an
 * already-running generator is an error.
 *
 * @param id  generator logical ID
 * @retval 0 on success
 * @retval -EINVAL invalid id
 * @retval -EALREADY generator is already running
 */
int gen_start(enum gen_id id);

/**
 * Stop a generator thread and wait for it to exit.
 *
 * @param id  generator logical ID
 * @retval 0 on success
 * @retval -EINVAL invalid id
 * @retval -EALREADY generator is not running
 */
int gen_stop(enum gen_id id);

/**
 * Set the transmit interval for a generator.
 *
 * @param id  generator logical ID
 * @param ms  interval in milliseconds (must be > 0)
 * @retval 0 on success
 * @retval -EINVAL invalid id or ms == 0
 */
int gen_set_interval(enum gen_id id, uint32_t ms);

/**
 * Set the maximum message size for a generator.
 *
 * Size 0 means no limit (use the full formatted message).
 *
 * @param id    generator logical ID
 * @param bytes maximum message length in bytes
 * @retval 0 on success
 * @retval -EINVAL invalid id
 */
int gen_set_size(enum gen_id id, uint16_t bytes);

/**
 * Enable or disable random mode for a generator.
 *
 * Random mode is reserved for future scenario use; currently a no-op in
 * the thread format logic.
 *
 * @param id  generator logical ID
 * @param on  true to enable, false to disable
 * @retval 0 on success
 * @retval -EINVAL invalid id
 */
int gen_set_random(enum gen_id id, bool on);

#endif /* GENERATOR_H */
