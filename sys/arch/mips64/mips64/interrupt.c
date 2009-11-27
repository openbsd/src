/*	$OpenBSD: interrupt.c,v 1.55 2009/11/27 00:08:27 syuu Exp $ */

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
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/frame.h>

#include <mips64/rm7000.h>

#ifdef DDB
#include <mips64/db_machdep.h>
#include <ddb/db_sym.h>
#endif

void	dummy_splx(int);
void	interrupt(struct trap_frame *);

static struct evcount soft_count;
static int soft_irq = 0;

uint32_t idle_mask;
int	last_low_int;

struct {
	uint32_t int_mask;
	uint32_t (*int_hand)(uint32_t, struct trap_frame *);
} cpu_int_tab[NLOWINT];

int_f	*splx_hand = &dummy_splx;

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
 *  if it registers for more than one interrupt input.
 *
 *  The interrupt mechanism in this port uses a delayed masking model
 *  where interrupts are not really masked when doing an spl(). Instead
 *  a masked interrupt will be taken and validated in the various
 *  handlers. If the handler finds that an interrupt is masked it will
 *  register this interrupt as pending and return a new mask to this
 *  code that will turn off the interrupt hardware wise. Later when
 *  the pending interrupt is unmasked it will be processed as usual
 *  and the regular hardware mask will be restored.
 */

/*
 * Handle an interrupt. Both kernel and user mode are handled here.
 *
 * The interrupt handler is called with the CR_INT bits set that
 * were given when the handler was registered.
 * The handler should return a similar word with a mask indicating
 * which CR_INT bits have been handled.
 */

void
interrupt(struct trap_frame *trapframe)
{
	struct cpu_info *ci = curcpu();
	u_int32_t pending;
	int i, s;

	/*
	 *  Paranoic? Perhaps. But if we got here with the enable
	 *  bit reset a mtc0 COP_0_STATUS_REG may have been interrupted.
	 *  If this was a disable and the pipeline had advanced long
	 *  enough... i don't know but better safe than sorry...
	 *  The main effect is not the interrupts but the spl mechanism.
	 */
	if (!(trapframe->sr & SR_INT_ENAB))
		return;

#ifdef DEBUG_INTERRUPT
	trapdebug_enter(trapframe, 0);
#endif
	atomic_add_int(&uvmexp.intrs, 1);

	/* Mask out interrupts from cause that are unmasked */
	pending = trapframe->cause & CR_IPEND & trapframe->sr;

	if (pending & SOFT_INT_MASK_0) {
		clearsoftintr0();
		atomic_add_uint64(&soft_count.ec_count, 1);
	}

#ifdef RM7K_PERFCNTR
	if (pending & CR_INT_PERF)
		rm7k_perfintr(trapframe);
#endif

	for (i = 0; i <= last_low_int; i++) {
		uint32_t active;
		active = cpu_int_tab[i].int_mask & pending;
		if (active != 0)
			(*cpu_int_tab[i].int_hand)(active, trapframe);
	}

	/*
	 * Dispatch soft interrupts if current ipl allows them.
	 */
	if (ci->ci_ipl < IPL_SOFTINT && ci->ci_softpending != 0) {
		s = splsoft();
		dosoftint();
		__asm__ (".set noreorder\n");
		ci->ci_ipl = s;	/* no-overhead splx */
		__asm__ ("sync\n\t.set reorder\n");
	}
}


/*
 * Set up handler for external interrupt events.
 * Use CR_INT_<n> to select the proper interrupt condition to dispatch on.
 * We also enable the software ints here since they are always on.
 */
void
set_intr(int pri, uint32_t mask,
    uint32_t (*int_hand)(uint32_t, struct trap_frame *))
{
	if ((idle_mask & SOFT_INT_MASK) == 0)
		evcount_attach(&soft_count, "soft", (void *)&soft_irq,
		    &evcount_intr);
	if (pri < 0 || pri >= NLOWINT)
		panic("set_intr: too high priority (%d), increase NLOWINT",
		    pri);

	if (pri > last_low_int)
		last_low_int = pri;

	if ((mask & ~CR_IPEND) != 0)
		panic("set_intr: invalid mask 0x%x", mask);

	if (cpu_int_tab[pri].int_mask != 0 &&
	   (cpu_int_tab[pri].int_mask != mask ||
	    cpu_int_tab[pri].int_hand != int_hand))
		panic("set_intr: int already set at pri %d", pri);

	cpu_int_tab[pri].int_hand = int_hand;
	cpu_int_tab[pri].int_mask = mask;
	idle_mask |= mask | SOFT_INT_MASK;
}

void
dummy_splx(int newcpl)
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

	/*
	 * Update proc0 pcb to contain proper values.
	 */
	pcb->pcb_context.val[13] = IPL_NONE;
#ifdef RM7000_ICR
	pcb->pcb_context.val[12] = (idle_mask << 8) & IC_INT_MASK;
#endif
	pcb->pcb_context.val[11] = (pcb->pcb_regs.sr & ~SR_INT_MASK) |
	    (idle_mask & SR_INT_MASK);

	spl0();
	(void)updateimask(0);
}

int
splraise(int newipl)
{
	struct cpu_info *ci = curcpu();
        int oldipl;

	__asm__ (".set noreorder\n");
	oldipl = ci->ci_ipl;
	if (oldipl < newipl) {
		/* XXX to kill warning about dla being used in a delay slot */
		__asm__("nop");
		ci->ci_ipl = newipl;
	}
	__asm__ ("sync\n\t.set reorder\n");
	return oldipl;
}

void
splx(int newipl)
{
	(*splx_hand)(newipl);
}

int
spllower(int newipl)
{
	struct cpu_info *ci = curcpu();
	int oldipl;

	oldipl = ci->ci_ipl;
	splx(newipl);
	return oldipl;
}
