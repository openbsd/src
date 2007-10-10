/*	$OpenBSD: kern_sched.c,v 1.1 2007/10/10 15:53:53 art Exp $	*/
/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
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

#include <sys/sched.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/mutex.h>

#include <uvm/uvm_extern.h>

struct proc *sched_chooseproc(void);
void sched_kthreads_create(void *);
void sched_idle(void *);

/*
 * A few notes about cpu_switchto that is implemented in MD code.
 *
 * cpu_switchto takes two arguments, the old proc and the proc
 * it should switch to. The new proc will never be NULL, so we always have
 * a saved state that we need to switch to. The old proc however can
 * be NULL if the process is exiting. NULL for the old proc simply
 * means "don't bother saving old state".
 *
 * cpu_switchto is supposed to atomically load the new state of the process
 * including the pcb, pmap and setting curproc, the p_cpu pointer in the
 * proc and p_stat to SONPROC. Atomically with respect to interrupts, other
 * cpus in the system must not depend on this state being consistent.
 * Therefore no locking is necessary in cpu_switchto other than blocking
 * interrupts during the context switch.
 */

/*
 * sched_init_cpu is called from main() for the boot cpu, then it's the
 * responsibility of the MD code to call it for all other cpus.
 */
void
sched_init_cpu(struct cpu_info *ci)
{
	struct schedstate_percpu *spc = &ci->ci_schedstate;

	spc->spc_idleproc = NULL;

	kthread_create_deferred(sched_kthreads_create, ci);

	LIST_INIT(&spc->spc_deadproc);
}

void
sched_kthreads_create(void *v)
{
	struct cpu_info *ci = v;
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	static int num;

	if (kthread_create(sched_idle, ci, &spc->spc_idleproc, "idle%d", num))
		panic("fork idle");

	num++;
}

void
sched_idle(void *v)
{
	struct proc *p = curproc;
	struct cpu_info *ci = v;
	int s;

	KERNEL_PROC_UNLOCK(p);

	/*
	 * First time we enter here, we're not supposed to idle,
	 * just go away for a while.
	 */
	SCHED_LOCK(s);
	p->p_stat = SSLEEP;
	mi_switch();
	SCHED_UNLOCK(s);

	while (1) {
		KASSERT(ci == curcpu());
		KASSERT(curproc == ci->ci_schedstate.spc_idleproc);

		while (!sched_is_idle()) {
			struct schedstate_percpu *spc = &ci->ci_schedstate;
			struct proc *dead;

			SCHED_LOCK(s);
			p->p_stat = SSLEEP;
			mi_switch();
			SCHED_UNLOCK(s);

			while ((dead = LIST_FIRST(&spc->spc_deadproc))) {
				LIST_REMOVE(dead, p_hash);
				exit2(dead);
			}
		}

		cpu_idle_enter();
		while (sched_is_idle())
			cpu_idle_cycle();
		cpu_idle_leave();
	}
}

/*
 * To free our address space we have to jump through a few hoops.
 * The freeing is done by the reaper, but until we have one reaper
 * per cpu, we have no way of putting this proc on the deadproc list
 * and waking up the reaper without risking having our address space and
 * stack torn from under us before we manage to switch to another proc.
 * Therefore we have a per-cpu list of dead processes where we put this
 * proc and have idle clean up that list and move it to the reaper list.
 * All this will be unnecessary once we can bind the reaper this cpu
 * and not risk having it switch to another in case it sleeps.
 */
void
sched_exit(struct proc *p)
{
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;
	struct timeval tv;
	struct proc *idle;
	int s;

	microuptime(&tv);
	timersub(&tv, &spc->spc_runtime, &tv);
	timeradd(&p->p_rtime, &tv, &p->p_rtime);

	LIST_INSERT_HEAD(&spc->spc_deadproc, p, p_hash);

#ifdef MULTIPROCESSOR
	KASSERT(__mp_lock_held(&kernel_lock) == 0);
#endif

	SCHED_LOCK(s);
	idle = spc->spc_idleproc;
	idle->p_stat = SRUN;
	cpu_switchto(NULL, idle);
}

/*
 * Run queue management.
 *
 * The run queue management is just like before, except that it's with
 * a bit more modern queue handling.
 */

TAILQ_HEAD(prochead, proc) sched_qs[NQS];
volatile int sched_whichqs;

void
sched_init_runqueues(void)
{
	int i;

	for (i = 0; i < NQS; i++)
		TAILQ_INIT(&sched_qs[i]);

#ifdef MULTIPROCESSOR
	SIMPLE_LOCK_INIT(&sched_lock);
#endif
}

void
setrunqueue(struct proc *p)
{
	int queue = p->p_priority >> 2;

	SCHED_ASSERT_LOCKED();

	TAILQ_INSERT_TAIL(&sched_qs[queue], p, p_runq);
	sched_whichqs |= (1 << queue);
}

void
remrunqueue(struct proc *p)
{
	int queue = p->p_priority >> 2;

	SCHED_ASSERT_LOCKED();

	TAILQ_REMOVE(&sched_qs[queue], p, p_runq);
	if (TAILQ_EMPTY(&sched_qs[queue]))
		sched_whichqs &= ~(1 << queue);
}

struct proc *
sched_chooseproc(void)
{
	struct proc *p;
	int queue;

	SCHED_ASSERT_LOCKED();

again:
	if (sched_whichqs == 0) {
		p = curcpu()->ci_schedstate.spc_idleproc;
		if (p == NULL) {
                        int s;
			/*
			 * We get here if someone decides to switch during
			 * boot before forking kthreads, bleh.
			 * This is kind of like a stupid idle loop.
			 */
#ifdef MULTIPROCESSOR
			__mp_unlock(&sched_lock);
#endif
			spl0();
			delay(10);
			SCHED_LOCK(s);
			goto again;
                }
		KASSERT(p);
		p->p_stat = SRUN;
	} else {
		queue = ffs(sched_whichqs) - 1;
		p = TAILQ_FIRST(&sched_qs[queue]);
		TAILQ_REMOVE(&sched_qs[queue], p, p_runq);
		if (TAILQ_EMPTY(&sched_qs[queue]))
			sched_whichqs &= ~(1 << queue);
	}

	return (p);	
}
