/*	$OpenBSD: intr.c,v 1.42 2011/06/27 17:04:46 jsing Exp $	*/
/*	$NetBSD: intr.c,v 1.39 2001/07/19 23:38:11 eeh Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT OT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)intr.c	8.3 (Berkeley) 11/11/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/cons.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/instr.h>
#include <machine/trap.h>

/* Grab interrupt map stuff (what is it doing there???) */
#include <sparc64/dev/iommureg.h>

/*
 * The following array is to used by locore.s to map interrupt packets
 * to the proper IPL to send ourselves a softint.  It should be filled
 * in as the devices are probed.  We should eventually change this to a
 * vector table and call these things directly.
 */
struct intrhand *intrlev[MAXINTNUM];

void	strayintr(const struct trapframe64 *, int);
int	softintr(void *);
int	intr_list_handler(void *);
void	intr_ack(struct intrhand *);

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 */
int ignore_stray = 1;
int straycnt[16];

void
strayintr(fp, vectored)
	const struct trapframe64 *fp;
	int vectored;
{
	static int straytime, nstray;
	int timesince;
#if 0
	extern int swallow_zsintrs;
#endif

	if (fp->tf_pil < 16)
		straycnt[(int)fp->tf_pil]++;

	if (ignore_stray)
		return;

	/* If we're in polled mode ignore spurious interrupts */
	if ((fp->tf_pil == PIL_SER) /* && swallow_zsintrs */) return;

	printf("stray interrupt ipl %u pc=%llx npc=%llx pstate=%b "
	    "vectored=%d\n", fp->tf_pil, (unsigned long long)fp->tf_pc,
	    (unsigned long long)fp->tf_npc, fp->tf_tstate>>TSTATE_PSTATE_SHIFT,
	    PSTATE_BITS, vectored);

	timesince = time_second - straytime;
	if (timesince <= 10) {
		if (++nstray > 500)
			panic("crazy interrupts");
	} else {
		straytime = time_second;
		nstray = 1;
	}
#ifdef DDB
	Debugger();
#endif
}

/*
 * Level 1 software interrupt (could also be SBus level 1 interrupt).
 * Three possible reasons:
 *	Network software interrupt
 *	Soft clock interrupt
 */

/*
 * PCI devices can share interrupts so we need to have
 * a handler to hand out interrupts.
 */
int
intr_list_handler(void *arg)
{
	struct cpu_info *ci = curcpu();
	struct intrhand *ih = arg;
	int claimed = 0, ipl = ci->ci_handled_intr_level;

	while (ih) {
		sparc_wrpr(pil, ih->ih_pil, 0);
		ci->ci_handled_intr_level = ih->ih_pil;

		if (ih->ih_fun(ih->ih_arg)) {
			ih->ih_count.ec_count++;
			claimed = 1;
		}

		ih = ih->ih_next;
	}
	sparc_wrpr(pil, ipl, 0);
	ci->ci_handled_intr_level = ipl;

	return (claimed);
}

void
intr_ack(struct intrhand *ih)
{
	*ih->ih_clr = INTCLR_IDLE;
}

/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This is not possible if it has been taken away as a fast vector.
 */
void
intr_establish(int level, struct intrhand *ih)
{
	struct intrhand *q;
	u_int64_t m, id;
	int s;

	s = splhigh();

	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	ih->ih_pil = level; /* XXXX caller should have done this before */
	ih->ih_pending = 0; /* XXXX caller should have done this before */
	ih->ih_next = NULL;
	if (ih->ih_clr)
		ih->ih_ack = intr_ack;
	else
		ih->ih_ack = NULL;

	/*
	 * Store in fast lookup table
	 */
#ifdef NOT_DEBUG
	if (!ih->ih_number) {
		printf("\nintr_establish: NULL vector fun %p arg %p pil %p",
			  ih->ih_fun, ih->ih_arg, ih->ih_number, ih->ih_pil);
		Debugger();
	}
#endif

	if (ih->ih_number <= 0 || ih->ih_number >= MAXINTNUM)
		panic("intr_establish: bad intr number %x", ih->ih_number);

	if (strlen(ih->ih_name) == 0)
		evcount_attach(&ih->ih_count, "unknown", NULL);
	else
		evcount_attach(&ih->ih_count, ih->ih_name, NULL);

	q = intrlev[ih->ih_number];
	if (q == NULL) {
		/* No interrupt already there, just put handler in place. */
		intrlev[ih->ih_number] = ih;
	} else {
		struct intrhand *nih, *pih;
		int ipl;

		/*
		 * Interrupt is already there.  We need to create a
		 * new interrupt handler and interpose it.
		 */
#ifdef DEBUG
		printf("intr_establish: intr reused %x\n", ih->ih_number);
#endif
		if (q->ih_fun != intr_list_handler) {
			nih = malloc(sizeof(struct intrhand),
			    M_DEVBUF, M_NOWAIT | M_ZERO);
			if (nih == NULL)
				panic("intr_establish");

			nih->ih_fun = intr_list_handler;
			nih->ih_arg = q;
			nih->ih_number = q->ih_number;
			nih->ih_pil = min(q->ih_pil, ih->ih_pil);
			nih->ih_map = q->ih_map;
			nih->ih_clr = q->ih_clr;
			nih->ih_ack = q->ih_ack;
			q->ih_ack = NULL;

			intrlev[ih->ih_number] = q = nih;
		} else
			q->ih_pil = min(q->ih_pil, ih->ih_pil);

		ih->ih_ack = NULL;

		/* Add ih to list in priority order. */
		pih = q;
		nih = pih->ih_arg;
		ipl = nih->ih_pil;
		while (nih && ih->ih_pil <= nih->ih_pil) {
			ipl = nih->ih_pil;
			pih = nih;
			nih = nih->ih_next;
		}
#if DEBUG
		printf("intr_establish: inserting pri %i after %i\n",
		    ih->ih_pil, ipl);
#endif
		if (pih == q) {
			ih->ih_next = pih->ih_arg;
			pih->ih_arg = ih;
		} else {
			ih->ih_next = pih->ih_next;
			pih->ih_next = ih;
		}
	}

	if (ih->ih_clr != NULL)			/* Set interrupt to idle */
		*ih->ih_clr = INTCLR_IDLE;

	if (ih->ih_map) {
		id = CPU_UPAID;
		m = *ih->ih_map;
		if (INTTID(m) != id) {
#ifdef DEBUG
			printf("\nintr_establish: changing map 0x%llx -> ", m);
#endif
			m = (m & ~INTMAP_TID) | (id << INTTID_SHIFT);
#ifdef DEBUG
			printf("0x%llx (id=%llx) ", m, id);
#endif
		}
		m |= INTMAP_V;
		*ih->ih_map = m;
	}

#ifdef DEBUG
	printf("\nintr_establish: vector %x pil %x mapintr %p "
	    "clrintr %p fun %p arg %p target %d",
	    ih->ih_number, ih->ih_pil, (void *)ih->ih_map,
	    (void *)ih->ih_clr, (void *)ih->ih_fun,
	    (void *)ih->ih_arg, (int)(ih->ih_map ? INTTID(*ih->ih_map) : -1));
#endif

	splx(s);
}

void *
softintr_establish(level, fun, arg)
	int level; 
	void (*fun)(void *);
	void *arg;
{
	struct intrhand *ih;

	if (level == IPL_TTY)
		level = IPL_SOFTTTY;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK | M_ZERO);
	ih->ih_fun = (int (*)(void *))fun;	/* XXX */
	ih->ih_arg = arg;
	ih->ih_pil = level;
	ih->ih_pending = 0;
	ih->ih_ack = NULL;
	ih->ih_clr = NULL;
	return (void *)ih;
}

void
softintr_disestablish(cookie)
	void *cookie;
{
	free(cookie, M_DEVBUF);
}

void
softintr_schedule(cookie)
	void *cookie;
{
	struct intrhand *ih = (struct intrhand *)cookie;

	send_softint(-1, ih->ih_pil, ih);
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	struct cpu_info *ci = curcpu();
	int oldipl;

	__asm __volatile("rdpr %%pil,%0" : "=r" (oldipl));

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
	}

	if (ci->ci_handled_intr_level > wantipl) {
		/*
		 * XXX - need to show difference between what's blocked and
		 * what's running.
		 */
		splassert_fail(wantipl, ci->ci_handled_intr_level, func);
	}
}
#endif

#ifdef MULTIPROCESSOR

void sparc64_intlock(struct trapframe64 *);
void sparc64_intunlock(struct trapframe64 *);

void
sparc64_intlock(struct trapframe64 *tf)
{
	if (tf->tf_pil < PIL_SCHED && tf->tf_pil != PIL_CLOCK)
		__mp_lock(&kernel_lock);
}

void
sparc64_intunlock(struct trapframe64 *tf)
{
	if (tf->tf_pil < PIL_SCHED && tf->tf_pil != PIL_CLOCK)
		__mp_unlock(&kernel_lock);
}

#endif
