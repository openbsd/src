/* $OpenBSD: clockintr.h,v 1.3 2023/03/09 03:50:38 cheloha Exp $ */
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
 *	o	Owned by a single CPU.
 */
struct clockintr_queue;
struct clockintr {
	uint64_t cl_expiration;				/* [o] dispatch time */
	TAILQ_ENTRY(clockintr) cl_elink;		/* [o] cq_est glue */
	TAILQ_ENTRY(clockintr) cl_plink;		/* [o] cq_pend glue */
	void (*cl_func)(struct clockintr *, void *);	/* [I] callback */
	struct clockintr_queue *cl_queue;		/* [I] parent queue */
	u_int cl_flags;					/* [o] CLST_* flags */
};

#define CLST_PENDING	0x00000001		/* scheduled to run */

/*
 * Per-CPU clock interrupt state.
 *
 * Struct member protections:
 *
 *	I	Immutable after initialization.
 *	o	Owned by a single CPU.
 */
struct clockintr_queue {
	uint64_t cq_uptime;		/* [o] cached uptime */
	TAILQ_HEAD(, clockintr) cq_est;	/* [o] established clockintr list */
	TAILQ_HEAD(, clockintr) cq_pend;/* [o] pending clockintr list */
	struct clockintr *cq_running;	/* [o] running clockintr */
	struct clockintr *cq_hardclock;	/* [o] hardclock handle */
	struct clockintr *cq_schedclock;/* [o] schedclock handle, if any */
	struct clockintr *cq_statclock;	/* [o] statclock handle */
	struct intrclock cq_intrclock;	/* [I] local interrupt clock */
	struct clockintr_stat cq_stat;	/* [o] dispatch statistics */
	volatile u_int cq_gen;		/* [o] cq_stat update generation */ 
	volatile u_int cq_dispatch;	/* [o] dispatch is running */
	u_int cq_flags;			/* [I] local state flags */
};

/* Global state flags. */
#define CL_INIT			0x00000001	/* global init done */
#define CL_STATCLOCK		0x00000002	/* statclock variables set */
#define CL_SCHEDCLOCK		0x00000004	/* run separate schedclock */
#define CL_STATE_MASK		0x00000007

/* Global behavior flags. */
#define CL_RNDSTAT		0x80000000	/* randomized statclock */
#define CL_FLAG_MASK		0x80000000

/* Per-CPU state flags. */
#define CL_CPU_INIT		0x00000001	/* CPU is ready for dispatch */
#define CL_CPU_INTRCLOCK	0x00000002	/* CPU has intrclock */
#define CL_CPU_STATE_MASK	0x00000003

void clockintr_cpu_init(const struct intrclock *);
int clockintr_dispatch(void *);
void clockintr_init(u_int);
void clockintr_setstatclockrate(int);
void clockintr_trigger(void);

/*
 * Kernel API
 */

int sysctl_clockintr(int *, u_int, void *, size_t *, void *, size_t);

#endif /* _KERNEL */

#endif /* !_SYS_CLOCKINTR_H_ */
