/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Shell commands for UART config, generator control, scenarios, and stats.
 *
 * Top-level commands:
 *   uart      — list / <id> baud|enable|disable
 *   gen       — start|stop|interval|size|random
 *   scenario  — list|run|stop
 *   stats     — [<id>]
 */

#include "shell_commands.h"

#include "uart_manager.h"
#include "generator.h"
#include "statistics.h"
#include "scenarios.h"

#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Forward declarations for APIs not yet provided by sibling headers.
 * These match the contracts the StatisticsModule and ScenariosModule
 * will implement.
 * ------------------------------------------------------------------ */

/* statistics.c */
void stats_print_all(void);
void stats_print(enum uart_id id);

/* scenarios.c */
const char * const *scenario_list(int *count);
int  scenario_run(const char *name);
int  scenario_stop(void);

/* ------------------------------------------------------------------
 * ID-string helpers
 * ------------------------------------------------------------------ */

/**
 * @brief Convert a shell UART name to an enum uart_id.
 *
 * Accepts "uart1", "uart3", "uart4".  Returns -1 on invalid input.
 */
static int str_to_uart_id(const char *str)
{
	if (strcmp(str, "uart1") == 0) {
		return UART_GEN1;
	}
	if (strcmp(str, "uart3") == 0) {
		return UART_GEN2;
	}
	if (strcmp(str, "uart4") == 0) {
		return UART_GEN3;
	}
	return -1;
}

/**
 * @brief Convert a shell UART name to an enum gen_id.
 *
 * Accepts "uart1", "uart3", "uart4".  Returns -1 on invalid input.
 */
static int str_to_gen_id(const char *str)
{
	if (strcmp(str, "uart1") == 0) {
		return GEN1;
	}
	if (strcmp(str, "uart3") == 0) {
		return GEN2;
	}
	if (strcmp(str, "uart4") == 0) {
		return GEN3;
	}
	return -1;
}

/**
 * @brief Return the user-facing name for a uart_id.
 */
static const char *uart_id_name(enum uart_id id)
{
	switch (id) {
	case UART_GEN1: return "uart1";
	case UART_GEN2: return "uart3";
	case UART_GEN3: return "uart4";
	default:        return "?";
	}
}

/* ==================================================================
 * uart command
 * ================================================================== */

static int cmd_uart_list(const struct shell *sh, size_t argc, char **argv)
{
	(void)argc;
	(void)argv;

	shell_print(sh, "%-6s %-7s %-12s %s", "ID", "Status", "Baud", "Device");
	shell_print(sh, "------ ------- ------------ -------");

	for (enum uart_id id = UART_GEN1; id < UART_COUNT; id++) {
		const struct uart_port *port = uart_get(id);
		if (port == NULL) {
			continue;
		}
		shell_print(sh, "%-6s %-7s %-12u %s",
			    uart_id_name(id),
			    port->enabled ? "ENABLED" : "DISABLED",
			    (unsigned int)port->baudrate,
			    port->label);
	}

	return 0;
}

static int cmd_uart_id(const struct shell *sh, size_t argc, char **argv)
{
	/* argv[-1] is the subcommand name (one of "uart1", "uart3", "uart4") */
	const char *id_str = argv[-1];
	int id = str_to_uart_id(id_str);
	if (id < 0) {
		shell_error(sh, "Internal error: unknown subcommand '%s'", id_str);
		return -EINVAL;
	}

	if (argc < 1) {
		shell_error(sh, "Missing action (baud <val>, enable, disable)");
		return -EINVAL;
	}

	const char *action = argv[0];

	if (strcmp(action, "baud") == 0) {
		if (argc < 2) {
			shell_error(sh, "Missing baud rate value");
			return -EINVAL;
		}

		unsigned long baud = strtoul(argv[1], NULL, 10);
		if (baud < 9600 || baud > 921600) {
			shell_error(sh, "Baud rate %lu out of range (9600-921600)",
				    baud);
			return -EINVAL;
		}

		int ret = uart_set_baud((enum uart_id)id, (uint32_t)baud);
		if (ret < 0) {
			shell_error(sh, "Failed to set baud rate: %d", ret);
			return ret;
		}

		shell_info(sh, "%s baud rate set to %lu", id_str, baud);
		return 0;
	}

	if (strcmp(action, "enable") == 0) {
		int ret = uart_enable((enum uart_id)id);
		if (ret < 0) {
			shell_error(sh, "Failed to enable %s: %d", id_str, ret);
			return ret;
		}
		shell_info(sh, "%s enabled", id_str);
		return 0;
	}

	if (strcmp(action, "disable") == 0) {
		int ret = uart_disable((enum uart_id)id);
		if (ret < 0) {
			shell_error(sh, "Failed to disable %s: %d", id_str, ret);
			return ret;
		}
		shell_info(sh, "%s disabled", id_str);
		return 0;
	}

	shell_error(sh, "Unknown action: '%s' (expected baud <val>, enable, or disable)",
		    action);
	return -EINVAL;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_uart,
	SHELL_CMD(list, NULL, "List all generator UARTs with status and baud rate",
		  cmd_uart_list),
	SHELL_CMD_ARG(uart1, NULL,
		      "Configure UART1: baud <val>, enable, disable",
		      cmd_uart_id, 1, 2),
	SHELL_CMD_ARG(uart3, NULL,
		      "Configure UART3: baud <val>, enable, disable",
		      cmd_uart_id, 1, 2),
	SHELL_CMD_ARG(uart4, NULL,
		      "Configure UART4: baud <val>, enable, disable",
		      cmd_uart_id, 1, 2),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(uart, &sub_uart, "Generator UART configuration", NULL);

/* ==================================================================
 * gen command
 * ================================================================== */

static int cmd_gen_uart(const struct shell *sh, size_t argc, char **argv)
{
	const char *id_str = argv[-1];
	int id = str_to_gen_id(id_str);
	if (id < 0) {
		shell_error(sh, "Internal error: unknown subcommand '%s'", id_str);
		return -EINVAL;
	}

	if (argc < 1) {
		shell_error(sh, "Missing action (start, stop, interval, size, random)");
		return -EINVAL;
	}

	const char *action = argv[0];

	if (strcmp(action, "start") == 0) {
		int ret = gen_start((enum gen_id)id);
		if (ret < 0) {
			shell_error(sh, "Failed to start generator on %s: %d",
				    id_str, ret);
			return ret;
		}
		shell_info(sh, "Generator started on %s", id_str);
		return 0;
	}

	if (strcmp(action, "stop") == 0) {
		int ret = gen_stop((enum gen_id)id);
		if (ret < 0) {
			shell_error(sh, "Failed to stop generator on %s: %d",
				    id_str, ret);
			return ret;
		}
		shell_info(sh, "Generator stopped on %s", id_str);
		return 0;
	}

	if (strcmp(action, "interval") == 0) {
		if (argc < 2) {
			shell_error(sh, "Missing interval value (ms)");
			return -EINVAL;
		}

		unsigned long ms = strtoul(argv[1], NULL, 10);
		if (ms < 10) {
			shell_error(sh, "Interval %lu ms too low (minimum 10)", ms);
			return -EINVAL;
		}

		int ret = gen_set_interval((enum gen_id)id, (uint32_t)ms);
		if (ret < 0) {
			shell_error(sh, "Failed to set interval on %s: %d",
				    id_str, ret);
			return ret;
		}

		shell_info(sh, "%s interval set to %lu ms", id_str, ms);
		return 0;
	}

	if (strcmp(action, "size") == 0) {
		if (argc < 2) {
			shell_error(sh, "Missing size value (bytes)");
			return -EINVAL;
		}

		unsigned long bytes = strtoul(argv[1], NULL, 10);
		if (bytes < 16 || bytes > 512) {
			shell_error(sh, "Size %lu out of range (16-512)", bytes);
			return -EINVAL;
		}

		int ret = gen_set_size((enum gen_id)id, (uint16_t)bytes);
		if (ret < 0) {
			shell_error(sh, "Failed to set size on %s: %d",
				    id_str, ret);
			return ret;
		}

		shell_info(sh, "%s message size set to %lu bytes", id_str, bytes);
		return 0;
	}

	if (strcmp(action, "random") == 0) {
		if (argc < 2) {
			shell_error(sh, "Missing value: on or off");
			return -EINVAL;
		}

		bool on;
		if (strcmp(argv[1], "on") == 0) {
			on = true;
		} else if (strcmp(argv[1], "off") == 0) {
			on = false;
		} else {
			shell_error(sh, "Invalid random value '%s' (expected on or off)",
				    argv[1]);
			return -EINVAL;
		}

		int ret = gen_set_random((enum gen_id)id, on);
		if (ret < 0) {
			shell_error(sh, "Failed to set random mode on %s: %d",
				    id_str, ret);
			return ret;
		}

		shell_info(sh, "%s random mode %s", id_str, on ? "on" : "off");
		return 0;
	}

	shell_error(sh, "Unknown action: '%s' (expected start, stop, interval, "
		    "size, or random)", action);
	return -EINVAL;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_gen,
	SHELL_CMD_ARG(uart1, NULL,
		      "Control generator on UART1: start, stop, interval <ms>, "
		      "size <bytes>, random on|off",
		      cmd_gen_uart, 1, 2),
	SHELL_CMD_ARG(uart3, NULL,
		      "Control generator on UART3: start, stop, interval <ms>, "
		      "size <bytes>, random on|off",
		      cmd_gen_uart, 1, 2),
	SHELL_CMD_ARG(uart4, NULL,
		      "Control generator on UART4: start, stop, interval <ms>, "
		      "size <bytes>, random on|off",
		      cmd_gen_uart, 1, 2),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(gen, &sub_gen, "Log generator control", NULL);

/* ==================================================================
 * scenario command
 * ================================================================== */

static int cmd_scenario_list(const struct shell *sh, size_t argc, char **argv)
{
	(void)argc;
	(void)argv;

	int count;
	const char * const *names = scenario_list(&count);

	shell_print(sh, "Available scenarios (%d):", count);
	for (int i = 0; i < count; i++) {
		shell_print(sh, "  %s", names[i]);
	}
	return 0;
}

static int cmd_scenario_run(const struct shell *sh, size_t argc, char **argv)
{
	(void)argc;

	const char *name = argv[0];
	int ret = scenario_run(name);
	if (ret < 0) {
		shell_error(sh, "Failed to run scenario '%s': %d", name, ret);
		return ret;
	}

	shell_info(sh, "Scenario '%s' started", name);
	return 0;
}

static int cmd_scenario_stop(const struct shell *sh, size_t argc, char **argv)
{
	(void)argc;
	(void)argv;

	int ret = scenario_stop();
	if (ret < 0) {
		shell_error(sh, "Failed to stop scenario: %d", ret);
		return ret;
	}

	shell_info(sh, "Scenario stopped");
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_scenario,
	SHELL_CMD(list, NULL, "List available test scenarios",
		  cmd_scenario_list),
	SHELL_CMD_ARG(run, NULL, "Run a named scenario", cmd_scenario_run, 1, 1),
	SHELL_CMD(stop, NULL, "Stop the currently running scenario",
		  cmd_scenario_stop),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(scenario, &sub_scenario, "Test scenario control", NULL);

/* ==================================================================
 * stats command
 * ================================================================== */

static int cmd_stats(const struct shell *sh, size_t argc, char **argv)
{
	if (argc == 0) {
		stats_print_all();
		return 0;
	}

	int id = str_to_uart_id(argv[0]);
	if (id < 0) {
		shell_error(sh, "Unknown UART: '%s' (expected uart1, uart3, or uart4)",
			    argv[0]);
		return -EINVAL;
	}

	stats_print((enum uart_id)id);
	return 0;
}

SHELL_CMD_ARG_REGISTER(stats, NULL, "Show UART statistics", cmd_stats, 0, 1);
