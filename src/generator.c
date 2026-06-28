/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * embed-sandbox — Log generator implementation
 *
 * Each configured USART (USART1, USART3, USART4) gets a dedicated
 * thread that periodically formats and transmits a log message via
 * the UART manager.
 */

#include "generator.h"
#include "uart_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* ------------------------------------------------------------------
 * UART label strings — must match the names in the devicetree overlay.
 * ------------------------------------------------------------------ */
static const char *const gen_labels[GEN_COUNT] = {
	"USART1",
	"USART3",
	"USART4",
};

/* ------------------------------------------------------------------
 * Thread stacks — one per generator, statically allocated.
 * ------------------------------------------------------------------ */
#define GEN_STACK_SIZE 1024

K_THREAD_STACK_DEFINE(gen_stack_0, GEN_STACK_SIZE);
K_THREAD_STACK_DEFINE(gen_stack_1, GEN_STACK_SIZE);
K_THREAD_STACK_DEFINE(gen_stack_2, GEN_STACK_SIZE);

/* Stack pointer + size per generator */
static const struct {
	k_thread_stack_t *buf;
	size_t            size;
} gen_stacks[GEN_COUNT] = {
	{ .buf = gen_stack_0, .size = K_THREAD_STACK_SIZEOF(gen_stack_0) },
	{ .buf = gen_stack_1, .size = K_THREAD_STACK_SIZEOF(gen_stack_1) },
	{ .buf = gen_stack_2, .size = K_THREAD_STACK_SIZEOF(gen_stack_2) },
};

/* Per-generator state array */
static struct gen_state gen_states[GEN_COUNT];

/* Thread priority (low preemptible — shell/main runs at higher prio) */
#define GEN_THREAD_PRIO 5

/* ------------------------------------------------------------------
 * Generator thread
 * ------------------------------------------------------------------ */

static void gen_thread_fn(void *arg1, void *arg2, void *arg3)
{
	struct gen_state *state = (struct gen_state *)arg1;
	enum gen_id id = (enum gen_id)(state - gen_states);
	char buf[256];

	while (state->cfg.enabled) {
		/* Block for interval or until woken by gen_stop */
		k_sem_take(&state->wake, K_MSEC(state->cfg.interval_ms));

		if (!state->cfg.enabled) {
			break;
		}

		/* Format message according to template */
		int len;

		switch (state->cfg.template_id) {
		case 1:
			len = snprintk(buf, sizeof(buf),
				      "[%s] DATA Packet %llu\r\n",
				      gen_labels[id],
				      (unsigned long long)state->counter);
			break;
		case 2:
			len = snprintk(buf, sizeof(buf),
				      "[%s] DEBUG Value=%llu\r\n",
				      gen_labels[id],
				      (unsigned long long)state->counter);
			break;
		case 0:
		default:
			len = snprintk(buf, sizeof(buf),
				      "[%s] INFO Counter=%llu\r\n",
				      gen_labels[id],
				      (unsigned long long)state->counter);
			break;
		}

		/* Clamp to configured msg_size if set */
		if (state->cfg.msg_size > 0 &&
		    len > (int)state->cfg.msg_size) {
			len = state->cfg.msg_size;
		}

		/* Transmit via UART manager */
		uart_send((enum uart_id)id,
			  (const uint8_t *)buf, (size_t)len);

		state->counter++;
	}
}

/* ------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------ */

int gen_init(void)
{
	for (int i = 0; i < GEN_COUNT; i++) {
		struct gen_state *s = &gen_states[i];

		s->counter = 0;
		s->cfg.enabled      = false;
		s->cfg.interval_ms  = 1000;
		s->cfg.msg_size     = 0;
		s->cfg.template_id  = 0;
		s->cfg.random_mode  = false;

		k_sem_init(&s->wake, 0, 1);
	}

	return 0;
}

int gen_start(enum gen_id id)
{
	if ((int)id < 0 || id >= GEN_COUNT) {
		return -EINVAL;
	}

	struct gen_state *s = &gen_states[id];

	if (s->cfg.enabled) {
		return -EALREADY;
	}

	s->counter = 0;
	s->cfg.enabled = true;

	k_thread_create(&s->thread,
			gen_stacks[id].buf,
			gen_stacks[id].size,
			(k_thread_entry_t)gen_thread_fn,
			(void *)s,
			NULL,
			NULL,
			GEN_THREAD_PRIO,
			0,
			K_NO_WAIT);

	return 0;
}

int gen_stop(enum gen_id id)
{
	if ((int)id < 0 || id >= GEN_COUNT) {
		return -EINVAL;
	}

	struct gen_state *s = &gen_states[id];

	if (!s->cfg.enabled) {
		return -EALREADY;
	}

	s->cfg.enabled = false;
	k_sem_give(&s->wake);

	k_thread_join(&s->thread, K_FOREVER);

	return 0;
}

int gen_set_interval(enum gen_id id, uint32_t ms)
{
	if ((int)id < 0 || id >= GEN_COUNT) {
		return -EINVAL;
	}
	if (ms == 0) {
		return -EINVAL;
	}

	gen_states[id].cfg.interval_ms = ms;
	return 0;
}

int gen_set_size(enum gen_id id, uint16_t bytes)
{
	if ((int)id < 0 || id >= GEN_COUNT) {
		return -EINVAL;
	}

	gen_states[id].cfg.msg_size = bytes;
	return 0;
}

int gen_set_random(enum gen_id id, bool on)
{
	if ((int)id < 0 || id >= GEN_COUNT) {
		return -EINVAL;
	}

	gen_states[id].cfg.random_mode = on;
	return 0;
}
