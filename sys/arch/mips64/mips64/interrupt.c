/*	$OpenBSD: interrupt.c,v 1.39 2009/05/27 18:58:15 miod Exp $ */

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

#include <machine/trap.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/regnum.h>
#include <machine/atomic.h>

#include <mips64/rm7000.h>

#include <mips64/archtype.h>

#ifdef DDB
#include <mips64/db_machdep.h>
#include <ddb/db_sym.h>
#endif


static struct evcount soft_count;
static int soft_irq = 0;

volatile intrmask_t cpl;
volatile intrmask_t ipending, astpending;

intrmask_t imask[NIPLS];

intrmask_t idle_mask;
int	last_low_int;

struct {
	intrmask_t int_mask;
	intrmask_t (*int_hand)(intrmask_t, struct trap_frame *);
} cpu_int_tab[NLOWINT];

void dummy_do_pending_int(int);

int_f *pending_hand = &dummy_do_pending_int;

/*
 *  Modern versions of MIPS processors have extended interrupt
 *  capabilities. How these are handled differs from implementation
 *  to implementation. This code tries to hide away some of these
 *  in "higher level" interrupt code.
 *
 *  Basically there are <n> interrupt inputs to the processor and
 *  typically the HW designer ties these interrupts to various
 *  sources in the HW. The low level code does not deal with interrupts
 *  in more than it dispatches handling to the code that has registered
 *  an interrupt handler for that particular interrupt. More than one
 *  handler can register to an interrupt input and one handler may register
 *  for more than one interrupt input. A handler is only called once even
 *  if it register for more than one interrupt input.
 *
 *  The interrupt mechanism in this port uses a delayed masking model
 *  where interrupts are not really masked when doing an spl(). Instead
 *  a masked interrupt will be taken and validated in the various
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
 *  to its status/mask bit in the cause/sr register shifted right eight
 *  places.
 *
 *  A well designed system uses the CPUs interrupt inputs in a way, such
 *  that masking can be done according to the IPL in the CPU status and
 *  interrupt control register. However support for an external masking
 *  register is provided but will cause a slightly higher overhead when
 *  used. When an external masking register is used, no masking in the
 *  CPU is done. Instead a fixed mask is set and used throughout.
 */

void interrupt(struct trap_frame *);
void ast(void);

/*
 * Handle an interrupt. Both kernel and user mode is handled here.
 *
 * The interrupt handler is called with the CR_INT bits set that
 * was given when the handlers was registered that needs servicing.
 * The handler should return a similar word with a mask indicating
 * which CR_INT bits that has been served.
 */

void
interrupt(struct trap_frame *trapframe)
{
	u_int32_t pending;
	u_int32_t cause;
	int i;
	intrmask_t xcpl;

	/*
	 *  Paranoic? Perhaps. But if we got here with the enable
	 *  bit reset a mtc0 COP_0_STATUS_REG may have been interrupted.
	 *  If this was a disable and the pipeline had advanced long
	 *  enough... i don't know but better safe than sorry...
	 *  The main effect is not the interrupts but the spl mechanism.
	 */
	if (!(trapframe->sr & SR_INT_ENAB)) {
		return;
	}

#ifdef DEBUG_INTERRUPT
	trapdebug_enter(trapframe, 0);
#endif

	uvmexp.intrs++;

	/* Mask out interrupts from cause that are unmasked */
	pending = trapframe->cause & CR_IPEND & trapframe->sr;
	cause = pending;

	if (cause & SOFT_INT_MASK_0) {
		clearsoftintr0();
		soft_count.ec_count++;
	}

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

	/*
	 *  Reenable all non served hardware levels.
	 */
#if 0
	/* XXX the following should, when req., change the IC reg as well */
	setsr((trapframe->sr & ~pending) | SR_INT_ENAB);
#endif

	xcpl = splsoft();
	if ((ipending & SINT_ALLMASK) & ~xcpl) {
		dosoftint(xcpl);
	}

	__asm__ (" .set noreorder\n");
	cpl = xcpl;
	__asm__ (" sync\n .set reorder\n");
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
	if ((idle_mask & SOFT_INT_MASK) == 0)
		evcount_attach(&soft_count, "soft", (void *)&soft_irq, &evcount_intr);
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
	idle_mask |= mask | SOFT_INT_MASK;
}

/*
 * This is called from MipsUserIntr() if astpending is set.
 */
void
ast()
{
	struct proc *p = curproc;
	int sig;

	uvmexp.softs++;

	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		ADDUPROF(p);
	}
	if (want_resched)
		preempt(NULL);

	/* inline userret(p) */

	while ((sig = CURSIG(p)) != 0)		/* take pending signals */
		postsig(sig);
	p->p_cpu->ci_schedstate.spc_curpriority = p->p_priority = p->p_usrpri;
}

struct intrhand *intrhand[INTMASKSIZE];

void
dummy_do_pending_int(int newcpl)
{
	/* Dummy handler */
}

/*
 *  splinit() is special in that sense that it require us to update
 *  the interrupt mask in the CPU since it may be the first time we arm
 *  the interrupt system. This function is called right after
 *  autoconfiguration has completed in autoconf.c.
 *  We enable everything in idle_mask.
 */
void
splinit()
{
	struct proc *p = curproc;
	struct pcb *pcb = &p->p_addr->u_pcb;
	u_int32_t sr;

	/*
	 * Update proc0 pcb to contain proper values.
	 */
	pcb->pcb_context.val[13] = 0;	/* IPL_NONE */
	pcb->pcb_context.val[12] = (idle_mask << 8) & IC_INT_MASK;
	pcb->pcb_context.val[11] = (pcb->pcb_regs.sr & ~SR_INT_MASK) |
	    (idle_mask & SR_INT_MASK);

	spl0();
	sr = updateimask(0);
	sr |= SR_INT_ENAB;
	setsr(sr);
#ifdef IMASK_EXTERNAL
	hw_setintrmask(0);
#endif
}

int
splraise(int newcpl)
{
        int oldcpl;

	__asm__ (" .set noreorder\n");
	oldcpl = cpl;
	cpl = oldcpl | newcpl;
	__asm__ (" sync\n .set reorder\n");
	return (oldcpl);
}

void
splx(int newcpl)
{
	if (ipending & ~newcpl)
		(*pending_hand)(newcpl);
	else {
		__asm__ (" .set noreorder\n");
		cpl = newcpl;
		__asm__ (" sync\n .set reorder\n");
#ifdef IMASK_EXTERNAL
		hw_setintrmask(newcpl);
#endif
	}
}

int
spllower(int newcpl)
{
	int oldcpl;

	oldcpl = cpl;
	splx(newcpl);
	return (oldcpl);
}

