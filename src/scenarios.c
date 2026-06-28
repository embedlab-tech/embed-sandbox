/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Predefined test scenarios for UART log generation.
 */

#include "scenarios.h"
#include "generator.h"
#include "uart_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/sys/util.h>
#include <errno.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------ */

static bool scenario_running;

/* ------------------------------------------------------------------
 * Baud-switching thread resources
 * ------------------------------------------------------------------ */

#define BAUD_SWITCH_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(baud_switch_stack, BAUD_SWITCH_STACK_SIZE);
static struct k_thread baud_switch_thread;

/* ------------------------------------------------------------------
 * Scenario name table
 * ------------------------------------------------------------------ */

static const char * const scenario_names[] = {
	"continuous",
	"burst",
	"random",
	"high-throughput",
	"mixed",
	"baud-switching",
};

#define NUM_SCENARIOS ARRAY_SIZE(scenario_names)

/* ------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------ */

static void start_all_generators(void)
{
	for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
		gen_start(id);
	}
}

static void stop_all_generators(void)
{
	for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
		gen_stop(id);
	}
}

/* ------------------------------------------------------------------
 * Baud-switching thread
 * ------------------------------------------------------------------ */

static void baud_switch_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	static const uint32_t baud_rates[] = {
		9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600,
	};

	while (scenario_running) {
		for (size_t i = 0; i < ARRAY_SIZE(baud_rates) && scenario_running; i++) {
			for (enum uart_id id = UART_GEN1; id < UART_COUNT; id++) {
				uart_set_baud(id, baud_rates[i]);
			}
			k_sleep(K_SECONDS(3));
		}
	}
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

int scenario_run(const char *name)
{
	if (!name) {
		return -EINVAL;
	}

	/* Stop any scenario that is already running */
	if (scenario_running) {
		scenario_stop();
	}

	if (strcmp(name, "continuous") == 0) {
		/* All generators: interval=1000ms, size=64, random=off */
		for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
			gen_set_interval(id, 1000);
			gen_set_size(id, 64);
			gen_set_random(id, false);
		}
		scenario_running = true;
		start_all_generators();

	} else if (strcmp(name, "burst") == 0) {
		/* All generators: interval=10ms, size=128, run 5s then stop */
		for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
			gen_set_interval(id, 10);
			gen_set_size(id, 128);
		}
		scenario_running = true;
		start_all_generators();

		k_sleep(K_SECONDS(5));

		stop_all_generators();
		scenario_running = false;

	} else if (strcmp(name, "random") == 0) {
		/* Each generator: random interval 50..549ms, size 32..287, random=on */
		for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
			uint32_t r = sys_rand32_get();
			uint32_t interval = (r % 500U) + 50;
			uint16_t size = (uint16_t)((r >> 16) % 256U) + 32;

			gen_set_interval(id, interval);
			gen_set_size(id, size);
			gen_set_random(id, true);
		}
		scenario_running = true;
		start_all_generators();

	} else if (strcmp(name, "high-throughput") == 0) {
		/* All generators: interval=10ms, size=256 */
		for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
			gen_set_interval(id, 10);
			gen_set_size(id, 256);
		}
		scenario_running = true;
		start_all_generators();

	} else if (strcmp(name, "mixed") == 0) {
		/* Per-UART custom settings */
		uart_set_baud(UART_GEN1, 9600);
		gen_set_interval(GEN1, 2000);
		gen_set_size(GEN1, 32);

		uart_set_baud(UART_GEN2, 115200);
		gen_set_interval(GEN2, 500);
		gen_set_size(GEN2, 64);

		uart_set_baud(UART_GEN3, 921600);
		gen_set_interval(GEN3, 100);
		gen_set_size(GEN3, 128);

		scenario_running = true;
		start_all_generators();

	} else if (strcmp(name, "baud-switching") == 0) {
		/* Moderate settings, then cycle baud rates in a thread */
		for (enum gen_id id = GEN1; id < GEN_COUNT; id++) {
			gen_set_interval(id, 100);
			gen_set_size(id, 64);
		}

		scenario_running = true;
		start_all_generators();

		k_thread_create(&baud_switch_thread, baud_switch_stack,
				K_THREAD_STACK_SIZEOF(baud_switch_stack),
				baud_switch_fn, NULL, NULL, NULL,
				5, 0, K_NO_WAIT);

	} else {
		return -EINVAL;
	}

	return 0;
}

int scenario_stop(void)
{
	scenario_running = false;
	stop_all_generators();
	return 0;
}

const char * const *scenario_list(int *count)
{
	if (count) {
		*count = NUM_SCENARIOS;
	}
	return scenario_names;
}
