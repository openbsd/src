/* $OpenBSD: clockintr.h,v 1.10 2023/08/21 17:22:04 cheloha Exp $ */
/*
 * Copyright (c) 2020-2022 Scott Cheloha <cheloha@openbsd.org>
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

#ifndef _SYS_CLOCKINTR_H_
#define _SYS_CLOCKINTR_H_

#include <sys/stdint.h>

struct clockintr_stat {
	uint64_t cs_dispatched;		/* total time in dispatch (ns) */
	uint64_t cs_early;		/* number of early dispatch calls */
	uint64_t cs_earliness;		/* total earliness (ns) */
	uint64_t cs_lateness;		/* total lateness (ns) */
	uint64_t cs_prompt;		/* number of prompt dispatch calls */
	uint64_t cs_run;		/* number of events dispatched */
	uint64_t cs_spurious;		/* number of spurious dispatch calls */
};

#ifdef _KERNEL

#include <sys/mutex.h>
#include <sys/queue.h>

/*
 * Platform API
 */

struct intrclock {
	void *ic_cookie;
	void (*ic_rearm)(void *, uint64_t);
	void (*ic_trigger)(void *);
};

static inline void
intrclock_rearm(struct intrclock *ic, uint64_t nsecs)
{
	ic->ic_rearm(ic->ic_cookie, nsecs);
}

static inline void
intrclock_trigger(struct intrclock *ic)
{
	ic->ic_trigger(ic->ic_cookie);
}

/*
 * Schedulable clock interrupt callback.
 *
 * Struct member protections:
 *
 *	I	Immutable after initialization.
 *	m	Parent queue mutex (cl_queue->cq_mtx).
 */
struct clockintr_queue;
struct clockintr {
	uint64_t cl_expiration;				/* [m] dispatch time */
	TAILQ_ENTRY(clockintr) cl_elink;		/* [m] cq_est glue */
	TAILQ_ENTRY(clockintr) cl_plink;		/* [m] cq_pend glue */
	void (*cl_func)(struct clockintr *, void *);	/* [I] callback */
	struct clockintr_queue *cl_queue;		/* [I] parent queue */
	u_int cl_flags;					/* [m] CLST_* flags */
};

#define CLST_PENDING		0x00000001	/* scheduled to run */
#define CLST_SHADOW_PENDING	0x00000002	/* shadow is scheduled to run */
#define CLST_IGNORE_SHADOW	0x00000004	/* ignore shadow copy */

/*
 * Per-CPU clock interrupt state.
 *
 * Struct member protections:
 *
 *	a	Modified atomically.
 *	I	Immutable after initialization.
 *	m	Per-queue mutex (cq_mtx).
 *	o	Owned by a single CPU.
 */
struct clockintr_queue {
	struct clockintr cq_shadow;	/* [o] copy of running clockintr */
	struct mutex cq_mtx;		/* [a] per-queue mutex */
	uint64_t cq_uptime;		/* [o] cached uptime */
	TAILQ_HEAD(, clockintr) cq_est;	/* [m] established clockintr list */
	TAILQ_HEAD(, clockintr) cq_pend;/* [m] pending clockintr list */
	struct clockintr *cq_running;	/* [m] running clockintr */
	struct clockintr *cq_hardclock;	/* [o] hardclock handle */
	struct clockintr *cq_statclock;	/* [o] statclock handle */
	struct intrclock cq_intrclock;	/* [I] local interrupt clock */
	struct clockintr_stat cq_stat;	/* [o] dispatch statistics */
	volatile u_int cq_gen;		/* [o] cq_stat update generation */ 
	volatile u_int cq_dispatch;	/* [o] dispatch is running */
	u_int cq_flags;			/* [I] CQ_* flags; see below */
};

#define CQ_INIT			0x00000001	/* clockintr_cpu_init() done */
#define CQ_INTRCLOCK		0x00000002	/* intrclock installed */
#define CQ_STATE_MASK		0x00000003

/* Global state flags. */
#define CL_INIT			0x00000001	/* global init done */
#define CL_STATE_MASK		0x00000001

/* Global behavior flags. */
#define CL_RNDSTAT		0x80000000	/* randomized statclock */
#define CL_FLAG_MASK		0x80000000

void clockintr_cpu_init(const struct intrclock *);
int clockintr_dispatch(void *);
void clockintr_init(u_int);
void clockintr_trigger(void);

/*
 * Kernel API
 */

uint64_t clockintr_advance(struct clockintr *, uint64_t);
void clockintr_cancel(struct clockintr *);
struct clockintr *clockintr_establish(struct clockintr_queue *,
    void (*)(struct clockintr *, void *));
void clockintr_stagger(struct clockintr *, uint64_t, u_int, u_int);
void clockqueue_init(struct clockintr_queue *);
int sysctl_clockintr(int *, u_int, void *, size_t *, void *, size_t);

#endif /* _KERNEL */

#endif /* !_SYS_CLOCKINTR_H_ */
