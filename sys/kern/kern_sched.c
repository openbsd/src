/*	$OpenBSD: kern_sched.c,v 1.9 2009/03/23 13:25:11 art Exp $	*/
/*
 * Copyright (c) 2007, 2008 Artur Grabowski <art@openbsd.org>
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
#include <machine/atomic.h>

#include <uvm/uvm_extern.h>

#include <sys/malloc.h>


void sched_kthreads_create(void *);
void sched_idle(void *);

int sched_proc_to_cpu_cost(struct cpu_info *ci, struct proc *p);
struct proc *sched_steal_proc(struct cpu_info *);

/*
 * To help choosing which cpu should run which process we keep track
 * of cpus which are currently idle and which cpus have processes
 * queued.
 */
struct cpuset sched_idle_cpus;
struct cpuset sched_queued_cpus;

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
	int i;

	for (i = 0; i < SCHED_NQS; i++)
		TAILQ_INIT(&spc->spc_qs[i]);

	spc->spc_idleproc = NULL;

	kthread_create_deferred(sched_kthreads_create, ci);

	LIST_INIT(&spc->spc_deadproc);

	/*
	 * Slight hack here until the cpuset code handles cpu_info
	 * structures.
	 */
	cpuset_init_cpu(ci);
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
	struct schedstate_percpu *spc;
	struct proc *p = curproc;
	struct cpu_info *ci = v;
	int s;

	KERNEL_PROC_UNLOCK(p);

	spc = &ci->ci_schedstate;

	/*
	 * First time we enter here, we're not supposed to idle,
	 * just go away for a while.
	 */
	SCHED_LOCK(s);
	cpuset_add(&sched_idle_cpus, ci);
	p->p_stat = SSLEEP;
	mi_switch();
	cpuset_del(&sched_idle_cpus, ci);
	SCHED_UNLOCK(s);

	KASSERT(ci == curcpu());
	KASSERT(curproc == spc->spc_idleproc);

	while (1) {
		while (!curcpu_is_idle()) {
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

		splassert(IPL_NONE);

		cpuset_add(&sched_idle_cpus, ci);
		cpu_idle_enter();
		while (spc->spc_whichqs == 0)
			cpu_idle_cycle();
		cpu_idle_leave();
		cpuset_del(&sched_idle_cpus, ci);
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
	panic("cpu_switchto returned");
}

/*
 * Run queue management.
 */
void
sched_init_runqueues(void)
{
#ifdef MULTIPROCESSOR
	__mp_lock_init(&sched_lock);
#endif
}

void
setrunqueue(struct proc *p)
{
	struct schedstate_percpu *spc;
	int queue = p->p_priority >> 2;

	SCHED_ASSERT_LOCKED();
	sched_choosecpu(p);
	spc = &p->p_cpu->ci_schedstate;
	spc->spc_nrun++;

	TAILQ_INSERT_TAIL(&spc->spc_qs[queue], p, p_runq);
	spc->spc_whichqs |= (1 << queue);
	cpuset_add(&sched_queued_cpus, p->p_cpu);

	if (p->p_cpu != curcpu())
		cpu_unidle(p->p_cpu);
}

void
remrunqueue(struct proc *p)
{
	struct schedstate_percpu *spc;
	int queue = p->p_priority >> 2;

	SCHED_ASSERT_LOCKED();
	spc = &p->p_cpu->ci_schedstate;
	spc->spc_nrun--;

	TAILQ_REMOVE(&spc->spc_qs[queue], p, p_runq);
	if (TAILQ_EMPTY(&spc->spc_qs[queue])) {
		spc->spc_whichqs &= ~(1 << queue);
		if (spc->spc_whichqs == 0)
			cpuset_del(&sched_queued_cpus, p->p_cpu);
	}
}

struct proc *
sched_chooseproc(void)
{
	struct schedstate_percpu *spc = &curcpu()->ci_schedstate;
	struct proc *p;
	int queue;

	SCHED_ASSERT_LOCKED();

again:
	if (spc->spc_whichqs) {
		queue = ffs(spc->spc_whichqs) - 1;
		p = TAILQ_FIRST(&spc->spc_qs[queue]);
		remrunqueue(p);
	} else if ((p = sched_steal_proc(curcpu())) == NULL) {
		p = spc->spc_idleproc;
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
	} 

	return (p);	
}

uint64_t sched_nmigrations;
uint64_t sched_noidle;
uint64_t sched_stolen;

uint64_t sched_choose;
uint64_t sched_wasidle;
uint64_t sched_nomigrations;

void
sched_choosecpu(struct proc *p)
{
	struct cpu_info *choice = NULL;
	int last_cost = INT_MAX;
	struct cpu_info *ci;
	struct cpuset set;

	sched_choose++;

	/*
	 * The simplest case. Our cpu of choice was idle. This happens
	 * when we were sleeping and something woke us up.
	 *
	 * We also need to check sched_queued_cpus to make sure that
	 * we're not thundering herding one cpu that hasn't managed to
	 * get out of the idle loop yet.
	 */
	if (p->p_cpu && cpuset_isset(&sched_idle_cpus, p->p_cpu) &&
	    !cpuset_isset(&sched_queued_cpus, p->p_cpu)) {
		sched_wasidle++;
		return;
	}

#if 0

		/* Most likely, this is broken. don't do it. */
	/*
	 * Second case. (shouldn't be necessary in the future)
	 * If our cpu is not idle, but has nothing else queued (which
	 * means that we are curproc and roundrobin asks us to reschedule).
	 */
	if (p->p_cpu && p->p_cpu->ci_schedstate.spc_nrun == 0)
		return;
#endif

	/*
	 * Look at all cpus that are currently idle. Pick the cheapest of
	 * those.
	 */
	cpuset_copy(&set, &sched_idle_cpus);
	while ((ci = cpuset_first(&set)) != NULL) {
		int cost = sched_proc_to_cpu_cost(ci, p);

		if (choice == NULL || cost < last_cost) {
			choice = ci;
			last_cost = cost;
		}
		cpuset_del(&set, ci);
	}

	/*
	 * All cpus are busy. Pick one.
	 */
	if (choice == NULL) {
		CPU_INFO_ITERATOR cii;

		sched_noidle++;

		/*
		 * Not curproc, pick the cpu with the lowest cost to switch to.
		 */
		CPU_INFO_FOREACH(cii, ci) {
			int cost = sched_proc_to_cpu_cost(ci, p);

			if (choice == NULL || cost < last_cost) {
				choice = ci;
				last_cost = cost;
			}
		}
	}

	KASSERT(choice);

	if (p->p_cpu && p->p_cpu != choice)
		sched_nmigrations++;
	else if (p->p_cpu != NULL)
		sched_nomigrations++;

	p->p_cpu = choice;
}

/*
 * Attempt to steal a proc from some cpu.
 */
struct proc *
sched_steal_proc(struct cpu_info *self)
{
	struct schedstate_percpu *spc;
	struct proc *best = NULL;
	int bestcost = INT_MAX;
	struct cpu_info *ci;
	struct cpuset set;

	cpuset_copy(&set, &sched_queued_cpus);

	while ((ci = cpuset_first(&set)) != NULL) {
		struct proc *p;
		int cost;

		cpuset_del(&set, ci);

		spc = &ci->ci_schedstate;

		p = TAILQ_FIRST(&spc->spc_qs[ffs(spc->spc_whichqs) - 1]);
		KASSERT(p);
		cost = sched_proc_to_cpu_cost(self, p);

		if (best == NULL || cost < bestcost) {
			best = p;
			bestcost = cost;
		}
	}
	if (best == NULL)
		return (NULL);

	spc = &best->p_cpu->ci_schedstate;
	remrunqueue(best);
	best->p_cpu = self;

	sched_stolen++;

	return (best);
}

/*
 * Base 2 logarithm of an int. returns 0 for 0 (yeye, I know).
 */
static int
log2(unsigned int i)
{
	int ret = 0;

	while (i >>= 1)
		ret++;

	return (ret);
}

/*
 * Calculate the cost of moving the proc to this cpu.
 * 
 * What we want is some guesstimate of how much "performance" it will
 * cost us to move the proc here. Not just for caches and TLBs and NUMA
 * memory, but also for the proc itself. A highly loaded cpu might not
 * be the best candidate for this proc since it won't get run.
 *
 * Just total guesstimates for now.
 */

int sched_cost_load = 1;
int sched_cost_priority = 1;
int sched_cost_runnable = 3;
int sched_cost_resident = 1;

int
sched_proc_to_cpu_cost(struct cpu_info *ci, struct proc *p)
{
	struct schedstate_percpu *spc;
	int l2resident = 0;
	int cost;

	spc = &ci->ci_schedstate;

	cost = 0;

	/*
	 * First, account for the priority of the proc we want to move.
	 * More willing to move, the lower the priority of the destination
	 * and the higher the priority of the proc.
	 */
	if (!cpuset_isset(&sched_idle_cpus, ci)) {
		cost += (p->p_priority - spc->spc_curpriority) *
		    sched_cost_priority;
		cost += sched_cost_runnable;
	}
	if (cpuset_isset(&sched_queued_cpus, ci)) {
		cost += spc->spc_nrun * sched_cost_runnable;
	}

	/*
	 * Higher load on the destination means we don't want to go there.
	 */
	cost += ((sched_cost_load * spc->spc_ldavg) >> FSHIFT);

	/*
	 * If the proc is on this cpu already, lower the cost by how much
	 * it has been running and an estimate of its footprint.
	 */
	if (p->p_cpu == ci && p->p_slptime == 0) {
		l2resident =
		    log2(pmap_resident_count(p->p_vmspace->vm_map.pmap));
		cost -= l2resident * sched_cost_resident;
	}

	return (cost);
}

/*
 * Functions to manipulate cpu sets.
 */
struct cpu_info *cpuset_infos[MAXCPUS];
static struct cpuset cpuset_all;

void
cpuset_init_cpu(struct cpu_info *ci)
{
	cpuset_add(&cpuset_all, ci);
	cpuset_infos[CPU_INFO_UNIT(ci)] = ci;
}

void
cpuset_clear(struct cpuset *cs)
{
	memset(cs, 0, sizeof(*cs));
}

/*
 * XXX - implement it on SP architectures too
 */
#ifndef CPU_INFO_UNIT
#define CPU_INFO_UNIT 0
#endif

void
cpuset_add(struct cpuset *cs, struct cpu_info *ci)
{
	unsigned int num = CPU_INFO_UNIT(ci);
	atomic_setbits_int(&cs->cs_set[num/32], (1 << (num % 32)));
}

void
cpuset_del(struct cpuset *cs, struct cpu_info *ci)
{
	unsigned int num = CPU_INFO_UNIT(ci);
	atomic_clearbits_int(&cs->cs_set[num/32], (1 << (num % 32)));
}

int
cpuset_isset(struct cpuset *cs, struct cpu_info *ci)
{
	unsigned int num = CPU_INFO_UNIT(ci);
	return (cs->cs_set[num/32] & (1 << (num % 32)));
}

void
cpuset_add_all(struct cpuset *cs)
{
	cpuset_copy(cs, &cpuset_all);
}

void
cpuset_copy(struct cpuset *to, struct cpuset *from)
{
	memcpy(to, from, sizeof(*to));
}

struct cpu_info *
cpuset_first(struct cpuset *cs)
{
	int i;

	for (i = 0; i < CPUSET_ASIZE(ncpus); i++)
		if (cs->cs_set[i])
			return (cpuset_infos[i * 32 + ffs(cs->cs_set[i]) - 1]);

	return (NULL);
}
