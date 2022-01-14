/*	$OpenBSD: irq_work.h,v 1.6 2022/01/14 06:53:14 jsg Exp $	*/
/*
 * Copyright (c) 2015 Mark Kettenis
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _LINUX_IRQ_WORK_H
#define _LINUX_IRQ_WORK_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <machine/cpu.h>	/* for CPU_BUSY_CYCLE() */

struct irq_work {
	struct timeout to;
};

typedef void (*irq_work_func_t)(struct irq_work *);

static inline void
init_irq_work(struct irq_work *work, irq_work_func_t func)
{
	timeout_set(&work->to, (void (*)(void *))func, work);
}

static inline bool
irq_work_queue(struct irq_work *work)
{
	return timeout_add(&work->to, 1);
}

static inline void
irq_work_sync(struct irq_work *work)
{
	timeout_del_barrier(&work->to);
}

#endif
