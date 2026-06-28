/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Predefined test scenarios.
 */

#ifndef SCENARIOS_H
#define SCENARIOS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run a named scenario.
 *
 * Stops any currently running scenario first, configures generators and
 * UARTs per the scenario definition, and starts the generators.
 *
 * @param name  Scenario name (see @ref scenario_list).
 *
 * @retval 0 on success.
 * @retval -EINVAL if @p name is NULL or unknown.
 */
int scenario_run(const char *name);

/**
 * @brief Stop the currently running scenario.
 *
 * Sets the running flag false, stops all generators, and resets internal
 * state.  Safe to call even when no scenario is running.
 *
 * @retval 0 always.
 */
int scenario_stop(void);

/**
 * @brief Return the list of available scenario names.
 *
 * @param[out] count  Set to the number of entries in the returned array.
 *
 * @return Pointer to a NULL-terminated array of scenario name strings.
 */
const char * const *scenario_list(int *count);

#ifdef __cplusplus
}
#endif

#endif /* SCENARIOS_H */
