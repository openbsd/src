/*	$OpenBSD: interrupt.c,v 1.4 2004/08/10 20:15:47 deraadt Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/malloc.h>
#include <sys/device.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <net/netisr.h>

#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/regnum.h>

#include <machine/rm7000.h>

#include <mips64/archtype.h>

#ifdef DDB
#include <mips64/db_machdep.h>
#include <ddb/db_sym.h>
#endif

int netisr;

/*
 *  Modern versions of MIPS processors have extended interrupt
 *  capabilites. How these are handeled differs from implementation
 *  to implementation. This code tries to hide away some of these
 *  in "higher level" interrupt code.
 *
 *  Basically there are <n> interrupt inputs to the processor and
 *  typically the HW designer ties these interrupts to various
 *  sources in the HW. The low level code does not deal with interrupts
 *  in more than it dispatches handling to the code that has registred
 *  an interrupt handler for that particular interrupt. More than one
 *  handler can register to an interrupt input and one handler may register
 *  for more than one interrupt input. A handler is only called once even
 *  if it register for more than one interrupt input.
 *
 *  The interrupt mechanism in this port uses a delayed masking model
 *  where interrupts are not really masked when doing an spl(). Instead
 *  a masked interrupt will be taken and validiated in the various
 *  handlers. If the handler finds that an interrupt is masked it will
 *  register this interrupt as pending and return a new mask to this
 *  code that will turn off the interrupt hardware wise. Later when
 *  the pending interrupt is unmasked it will be processed as usual
 *  and the hardware mask will be restored.
 */

/*
 *  Interrupt mapping is as follows:
 *
 *  irq can be between 1 and 10. This maps to CPU IPL2..IPL11.
 *  The two software interrupts IPL0 and IPL1 are reserved for
 *  kernel functions. IPL13 is used for the performance counters
 *  in the RM7000. IPL12 extra timer is currently not used.
 *
 *  irq's maps into the software spl register to the bit corresponding
 *  to it's status/mask bit in the cause/sr register shifted right eight
 *  places.
 *
 *  A well designed system uses the CPUs interrupt inputs in a way, such
 *  that masking can be done according to the IPL in the CPU status and
 *  interrupt vontrol register. However support for an external masking
 *  register is provided but will case a slightly higher overhead when
 *  used. When an external masking register is used, no masking in the
 *  CPU is done. Instead a fixed mask is set and used throughout.
 */

void interrupt (struct trap_frame *);
void softintr (void);

/*
 * Handle an interrupt. Both kernel and user mode is handeled here.
 *
 * The interrupt handler is called with the CR_INT bits set that
 * was given when the handlers was registred that needs servicing.
 * The handler should return a similar word with a mask indicating
 * which CR_INT bits that has been served.
 */

void
interrupt(struct trap_frame *trapframe)
{
	u_int32_t pending;
	u_int32_t cause;
	int i;
	intrmask_t pcpl;

	/*
	 *  Paranoic? Perhaps. But if we got here with the enable
	 *  bit reset a mtc0 COP_0_STATUS_REG may have been interrupted.
	 *  If this was a disable and the pipleine had advanced long
	 *  enough... i don't know but better safe than sorry...
	 *  The main effect is not the interrupts but the spl mechanism.
	 */
	if (!(trapframe->sr & SR_INT_ENAB)) {
		return;
	}

	uvmexp.intrs++;
	pcpl = splhigh() ;	/* Turn off all and get current SW mask */

#ifdef DEBUG_INTERRUPT
	trapdebug_enter(trapframe, 0);
#endif

	pending = trapframe->cause & CR_IPEND;
#ifdef IMASK_EXTERNAL
	pending &= idle_mask << 8;
#else
	ipending |= (pending >> 8) & pcpl;
	pending &= ~(pcpl << 8);
#endif
	cause = pending;

	if (cause & CR_INT_PERF) {
		rm7k_perfintr(trapframe);
		cause &= ~CR_INT_PERF;
	}

	for (i = 0; i <= last_low_int; i++) {
		intrmask_t active;
		active = cpu_int_tab[i].int_mask & pending;
		if (active) {
			cause &= ~(*cpu_int_tab[i].int_hand)(active, trapframe);
		}
	}
#if 0
if ((pending & cause & ~(SOFT_INT_MASK_1|SOFT_INT_MASK_0)) != 0) {
printf("Unhandled interrupt %x:%x\n", cause, pending);
//Debugger();
}
#endif
	/*
	 *  Reenable all non served hardware levels.
	 */
#if 0
	splx((trapframe->sr & ~cause & SR_INT_MASK) | SR_INT_ENAB);
#endif

	if (pending & SOFT_INT_MASK_0) {
		clearsoftintr0();
		uvmexp.softs++;
	}

#ifndef IMASK_EXTERNAL
	trapframe->sr &= ~((pcpl << 8) & SR_INT_MASK);
	trapframe->ic &= ~(pcpl & IC_INT_MASK);
#endif
	splx(pcpl);	/* Process pendings. */
}


/*
 *  Set up handler for external interrupt events.
 *  Use CR_INT_<n> to select the proper interrupt
 *  condition to dispatch on. We also enable the
 *  software ints here since they are always on.
 */
void
set_intr(int pri, intrmask_t mask,
	intrmask_t (*int_hand)(intrmask_t, struct trap_frame *))
{
	if (pri < 0 || pri >= NLOWINT) {
		panic("set_intr: to high priority");
	}

	if (pri > last_low_int)
		last_low_int = pri;

	if ((mask & ~CR_IPEND) != 0) {
		panic("set_intr: invalid mask 0x%x", mask);
	}

	if (cpu_int_tab[pri].int_mask != 0 &&
	   (cpu_int_tab[pri].int_mask != mask ||
	    cpu_int_tab[pri].int_hand != int_hand)) {
		panic("set_intr: int already set at pri %d", pri);
	}

	cpu_int_tab[pri].int_hand = int_hand;
	cpu_int_tab[pri].int_mask = mask;
	idle_mask |= (mask | SOFT_INT_MASK) >> 8;
}

/*
 * This is called from MipsUserIntr() if astpending is set.
 * This is very similar to the tail of trap().
 */
void
softintr()
{
	struct proc *p = curproc;
	int sig;

	uvmexp.softs++;
	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}
	if (want_resched) {
		int s;

		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}
	curpriority = p->p_priority;
}



intrmask_t intem = 0x0;
intrmask_t intrtype[INTMASKSIZE], intrmask[INTMASKSIZE], intrlevel[INTMASKSIZE];
struct intrhand *intrhand[INTMASKSIZE];

/*======================================================================*/

/*
 *	Generic interrupt handling code.
 *      ================================
 *
 *  This code can be used for interrupt models where only the
 *  processor status register has to be changed to mask/unmask.
 *  HW specific setup can be done in a MD function that can then
 *  call this function to use the generic interrupt code.
 */
static int fakeintr(void *);
static int fakeintr(void *a) {return 0;}

/*
 *  Establish an interrupt handler called from the dispatcher.
 *  The interrupt function established should return zero if
 *  there was nothing to serve (no int) and non zero when an
 *  interrupt was serviced.
 *  Interrupts are numbered from 1 and up where 1 maps to HW int 0.
 */
void *
generic_intr_establish(icp, irq, type, level, ih_fun, ih_arg, ih_what)
	void *icp;
        u_long irq;	/* XXX pci_intr_handle_t compatible XXX */
        int type;
        int level;
        int (*ih_fun) __P((void *));
        void *ih_arg;
        char *ih_what;
{
	struct intrhand **p, *q, *ih;
	static struct intrhand fakehand = {NULL, fakeintr};
	int edge;
extern int cold;

static int initialized = 0;

	if (!initialized) {
/*INIT CODE HERE*/
		initialized = 1;
	}

	if (irq > 62 || irq < 1) {
		panic("intr_establish: illegal irq %d\n", irq);
	}
	irq += 1;	/* Adjust for softint 1 and 0 */

	/* no point in sleeping unless someone can free memory. */
	ih = malloc(sizeof *ih, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");

	if (type == IST_NONE || type == IST_PULSE)
		panic("intr_establish: bogus type");

	switch (intrtype[irq]) {
	case IST_EDGE:
	case IST_LEVEL:
		if (type == intrtype[irq])
			break;
	}

	switch (type) {
	case IST_EDGE:
		edge |= 1 << irq;
		break;
	case IST_LEVEL:
		edge &= ~(1 << irq);
		break;
	}

	/*
	 * Figure out where to put the handler.
	 * This is O(N^2), but we want to preserve the order, and N is
	 * generally small.
	 */
	for (p = &intrhand[irq]; (q = *p) != NULL; p = &q->ih_next)
		;

	/*
	 * Actually install a fake handler momentarily, since we might be doing
	 * this with interrupts enabled and don't want the real routine called
	 * until masking is set up.
	 */
	fakehand.ih_level = level;
	*p = &fakehand;

	generic_intr_makemasks();

	/*
	 * Poke the real handler in now.
	 */
	ih->ih_fun = ih_fun;
	ih->ih_arg = ih_arg;
	ih->ih_count = 0;
	ih->ih_next = NULL;
	ih->ih_level = level;
	ih->ih_irq = irq;
	ih->ih_what = ih_what;
	*p = ih;

	return (ih);
}

void
generic_intr_disestablish(void *p1, void *p2)
{
}

/*
 *  Regenerate interrupt masks to reflect reality.
 */
void
generic_intr_makemasks()
{
	int irq, level;
	struct intrhand *q;

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < INTMASKSIZE; irq++) {
		int levels = 0;
		for (q = intrhand[irq]; q; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/* Then figure out which IRQs use each level. */
	for (level = 0; level < 5; level++) {
		register int irqs = 0;
		for (irq = 0; irq < INTMASKSIZE; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1 << irq;
		imask[level] = irqs | SINT_ALLMASK;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so imp > (tty | net | bio).
	 */
	imask[IPL_VM] |= imask[IPL_TTY] | imask[IPL_NET] | imask[IPL_BIO];

	/*
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	imask[IPL_TTY] |= imask[IPL_NET] | imask[IPL_BIO];
	imask[IPL_NET] |= imask[IPL_BIO];

	/*
	 * These are pseudo-levels.
	 */
	imask[IPL_NONE] = 0;
	imask[IPL_HIGH] = -1;

	/* And eventually calculate the complete masks. */
	for (irq = 0; irq < INTMASKSIZE; irq++) {
		register int irqs = 1 << irq;
		for (q = intrhand[irq]; q; q = q->ih_next)
			irqs |= imask[q->ih_level];
		intrmask[irq] = irqs | SINT_ALLMASK;
	}

	/* Lastly, determine which IRQs are actually in use. */
	irq = 0;
	for (level = 0; level < INTMASKSIZE; level++) {
		if (intrhand[level]) {
			irq |= 1 << level;
		}
	}
	intem = irq;
}

void
generic_do_pending_int()
{
	struct intrhand *ih;
	int vector;
	intrmask_t pcpl;
	intrmask_t hwpend;
	struct trap_frame cf;
static volatile int processing;

	/* Don't recurse... */
	if (processing)
		return;
	processing = 1;

/* XXX interrupt vulnerable when changing ipending */
	pcpl = splhigh();		/* Turn off all */

	/* XXX Fake a trapframe for clock pendings... */
	cf.pc = (int)&generic_do_pending_int;
	cf.sr = 0;
	cf.cpl = pcpl;

	hwpend = ipending & ~pcpl;	/* Do now unmasked pendings */
	hwpend &= ~(SINT_ALLMASK);
	ipending &= ~hwpend;
	intem |= hwpend;
	while (hwpend) {
		vector = ffs(hwpend) - 1;
		hwpend &= ~(1L << vector);
		ih = intrhand[vector];
		while (ih) {
			ih->frame = &cf;
			if ((*ih->ih_fun)(ih->ih_arg)) {
				ih->ih_count++;
			}
			ih = ih->ih_next;
		}
	}
	if ((ipending & SINT_CLOCKMASK) & ~pcpl) {
		ipending &= ~SINT_CLOCKMASK;
		softclock();
	}
	if ((ipending & SINT_NETMASK) & ~pcpl) {
		int isr = netisr;
		netisr = 0;
		ipending &= ~SINT_NETMASK;
#define	DONETISR(b,f)	if (isr & (1 << (b)))   f();
#include <net/netisr_dispatch.h>
	}

#ifdef NOTYET
	if ((ipending & SINT_TTYMASK) & ~pcpl) {
		ipending &= ~SINT_TTYMASK;
		compoll(NULL);
	}
#endif

	cpl = pcpl;		/* Don't use splx... we are here already! */
	updateimask(pcpl);	/* Update CPU mask ins SR register */
	processing = 0;
}

/*
 *  splinit() is special in that sense that it require us to update
 *  the interrupt mask in the CPU since it may be the first time we arm
 *  the interrupt system. This function is called right after
 *  autoconfiguration has compleeted in autoconf.c.
 *  We enable everything in idle_mask.
 */
void
splinit()
{
	u_int32_t sr;

	spl0();
	sr = updateimask(0);
	sr |= SR_INT_ENAB;
	setsr(sr);
#ifdef IMASK_EXTERNAL
	hw_setintrmask(0);
#endif
}

/*
 *  Process interrupts. The parameter pending has non-masked interrupts.
 */
intrmask_t
generic_iointr(intrmask_t pending, struct trap_frame *cf)
{
	struct intrhand *ih;
	intrmask_t catched, vm;
	int v;

	catched = 0;

	for (v = 2, vm = 0x400; pending != 0 && v < 16 ; v++, vm <<= 1) {
		if (pending & vm) {
			ih = intrhand[v];

			while (ih) {
				ih->frame = cf;
				if ((*ih->ih_fun)(ih->ih_arg)) {
					catched |= vm;
					ih->ih_count++;
				}
				ih = ih->ih_next;
			}
		}
	}
	return catched;
}
