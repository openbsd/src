/*	$OpenBSD: kern_smr.c,v 1.8 2020/04/03 03:36:56 visa Exp $	*/

/*
 * Copyright (c) 2019 Visa Hankala
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mutex.h>
#include <sys/percpu.h>
#include <sys/proc.h>
#include <sys/smr.h>
#include <sys/time.h>
#include <sys/witness.h>

#include <machine/cpu.h>

#define SMR_PAUSE	100		/* pause between rounds in msec */

void	smr_dispatch(struct schedstate_percpu *);
void	smr_grace_wait(void);
void	smr_thread(void *);
void	smr_wakeup(void *);

struct mutex		smr_lock = MUTEX_INITIALIZER(IPL_HIGH);
struct smr_entry_list	smr_deferred;
struct timeout		smr_wakeup_tmo;
unsigned int		smr_expedite;
unsigned int		smr_ndeferred;

#ifdef WITNESS
static const char smr_lock_name[] = "smr";
struct lock_object smr_lock_obj = {
	.lo_name = smr_lock_name,
	.lo_flags = LO_WITNESS | LO_INITIALIZED | LO_SLEEPABLE |
	    (LO_CLASS_RWLOCK << LO_CLASSSHIFT)
};
struct lock_type smr_lock_type = {
	.lt_name = smr_lock_name
};
#endif

static inline int
smr_cpu_is_idle(struct cpu_info *ci)
{
	return ci->ci_curproc == ci->ci_schedstate.spc_idleproc;
}

void
smr_startup(void)
{
	SIMPLEQ_INIT(&smr_deferred);
	WITNESS_INIT(&smr_lock_obj, &smr_lock_type);
	timeout_set(&smr_wakeup_tmo, smr_wakeup, NULL);
}

void
smr_startup_thread(void)
{
	if (kthread_create(smr_thread, NULL, NULL, "smr") != 0)
		panic("could not create smr thread");
}

struct timeval smr_logintvl = { 300, 0 };

void
smr_thread(void *arg)
{
	struct timeval elapsed, end, loglast, start;
	struct smr_entry_list deferred;
	struct smr_entry *smr;

	KERNEL_ASSERT_LOCKED();
	KERNEL_UNLOCK();

	memset(&loglast, 0, sizeof(loglast));
	SIMPLEQ_INIT(&deferred);

	for (;;) {
		mtx_enter(&smr_lock);
		if (smr_ndeferred == 0) {
			while (smr_ndeferred == 0)
				msleep_nsec(&smr_ndeferred, &smr_lock, PVM,
				    "bored", INFSLP);
		} else {
			if (smr_expedite == 0)
				msleep_nsec(&smr_ndeferred, &smr_lock, PVM,
				    "pause", MSEC_TO_NSEC(SMR_PAUSE));
		}

		SIMPLEQ_CONCAT(&deferred, &smr_deferred);
		smr_ndeferred = 0;
		smr_expedite = 0;
		mtx_leave(&smr_lock);

		getmicrouptime(&start);

		smr_grace_wait();

		WITNESS_CHECKORDER(&smr_lock_obj, LOP_NEWORDER, NULL);
		WITNESS_LOCK(&smr_lock_obj, 0);

		while ((smr = SIMPLEQ_FIRST(&deferred)) != NULL) {
			SIMPLEQ_REMOVE_HEAD(&deferred, smr_list);
			smr->smr_func(smr->smr_arg);
		}

		WITNESS_UNLOCK(&smr_lock_obj, 0);

		getmicrouptime(&end);
		timersub(&end, &start, &elapsed);
		if (elapsed.tv_sec >= 5 &&
		    ratecheck(&loglast, &smr_logintvl))
			printf("smr: dispatch took %ld seconds\n",
			    (long)elapsed.tv_sec);
	}
}

/*
 * Block until all CPUs have crossed quiescent state.
 */
void
smr_grace_wait(void)
{
#ifdef MULTIPROCESSOR
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci, *ci_start;

	ci_start = curcpu();
	CPU_INFO_FOREACH(cii, ci) {
		if (ci == ci_start)
			continue;
		sched_peg_curproc(ci);
	}
	atomic_clearbits_int(&curproc->p_flag, P_CPUPEG);
#endif /* MULTIPROCESSOR */
}

void
smr_wakeup(void *arg)
{
	wakeup(&smr_ndeferred);
}

void
smr_read_enter(void)
{
#ifdef DIAGNOSTIC
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;

	spc->spc_smrdepth++;
#endif
}

void
smr_read_leave(void)
{
#ifdef DIAGNOSTIC
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;

	KASSERT(spc->spc_smrdepth > 0);
	spc->spc_smrdepth--;
#endif
}

/*
 * Move SMR entries from the local queue to the system-wide queue.
 */
void
smr_dispatch(struct schedstate_percpu *spc)
{
	int expedite = 0, wake = 0;

	mtx_enter(&smr_lock);
	if (smr_ndeferred == 0)
		wake = 1;
	SIMPLEQ_CONCAT(&smr_deferred, &spc->spc_deferred);
	smr_ndeferred += spc->spc_ndeferred;
	spc->spc_ndeferred = 0;
	smr_expedite |= spc->spc_smrexpedite;
	spc->spc_smrexpedite = 0;
	expedite = smr_expedite;
	mtx_leave(&smr_lock);

	if (expedite)
		wakeup(&smr_ndeferred);
	else if (wake)
		timeout_add_msec(&smr_wakeup_tmo, SMR_PAUSE);
}

/*
 * Signal that the current CPU is in quiescent state.
 */
void
smr_idle(void)
{
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;

	SMR_ASSERT_NONCRITICAL();

	if (spc->spc_ndeferred > 0)
		smr_dispatch(spc);
}

void
smr_call_impl(struct smr_entry *smr, void (*func)(void *), void *arg,
    int expedite)
{
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	int s;

	KASSERT(smr->smr_func == NULL);

	smr->smr_func = func;
	smr->smr_arg = arg;

	s = splhigh();
	SIMPLEQ_INSERT_TAIL(&spc->spc_deferred, smr, smr_list);
	spc->spc_ndeferred++;
	spc->spc_smrexpedite |= expedite;
	splx(s);

	/*
	 * If this call was made from an interrupt context that
	 * preempted idle state, dispatch the local queue to the shared
	 * queue immediately.
	 * The entries would linger in the local queue long if the CPU
	 * went to sleep without calling smr_idle().
	 */
	if (smr_cpu_is_idle(ci))
		smr_dispatch(spc);
}

void
smr_barrier_func(void *arg)
{
	struct cond *c = arg;

	cond_signal(c);
}

void
smr_barrier_impl(int expedite)
{
	struct cond c = COND_INITIALIZER();
	struct smr_entry smr;

	if (panicstr != NULL || db_active)
		return;

	WITNESS_CHECKORDER(&smr_lock_obj, LOP_NEWORDER, NULL);

	smr_init(&smr);
	smr_call_impl(&smr, smr_barrier_func, &c, expedite);
	cond_wait(&c, "smrbar");
}
