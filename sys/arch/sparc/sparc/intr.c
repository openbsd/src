/*	$OpenBSD: intr.c,v 1.31 2007/05/29 18:10:43 miod Exp $ */
/*	$NetBSD: intr.c,v 1.20 1997/07/29 09:42:03 fair Exp $ */

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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
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
#include <sys/socket.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <net/netisr.h>
#include <net/if.h>

#include <machine/atomic.h>
#include <machine/cpu.h>
#include <machine/ctlreg.h>
#include <machine/instr.h>
#include <machine/trap.h>

#include <sparc/sparc/cpuvar.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#endif

#ifdef INET6
# ifndef INET
#  include <netinet/in.h>
# endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

void	strayintr(struct clockframe *);
int	soft01intr(void *);

/*
 * Stray interrupt handler.  Clear it if possible.
 * If not, and if we get 10 interrupts in 10 seconds, panic.
 */
void
strayintr(fp)
	struct clockframe *fp;
{
	static int straytime, nstray;
	int timesince;

	printf("stray interrupt ipl 0x%x pc=0x%x npc=0x%x psr=%b\n",
		fp->ipl, fp->pc, fp->npc, fp->psr, PSR_BITS);
	timesince = time.tv_sec - straytime;
	if (timesince <= 10) {
		if (++nstray > 9)
			panic("crazy interrupts");
	} else {
		straytime = time.tv_sec;
		nstray = 1;
	}
}

static struct intrhand level10 = { clockintr, NULL, (IPL_CLOCK << 8) };
static struct intrhand level14 = { statintr, NULL, (IPL_STATCLOCK << 8) };
union sir sir;
int netisr;

/*
 * Level 1 software interrupt (could also be SBus level 1 interrupt).
 * Three possible reasons:
 *	ROM console input needed
 *	Network software interrupt
 *	Soft clock interrupt
 */
int
soft01intr(fp)
	void *fp;
{
	if (sir.sir_any) {
		if (sir.sir_which[SIR_NET]) {
			int n;

			sir.sir_which[SIR_NET] = 0;
			while ((n = netisr) != 0) {
				atomic_clearbits_int(&netisr, n);

#define DONETISR(bit, fn)						\
				do {					\
					if (n & (1 << bit))		\
						fn();			\
				} while (0)

#include <net/netisr_dispatch.h>

#undef DONETISR
			}
		}
		if (sir.sir_which[SIR_CLOCK]) {
			sir.sir_which[SIR_CLOCK] = 0;
			softclock();
		}
	}
	return (1);
}

#if defined(SUN4M)
void	nmi_hard(void);
void
nmi_hard()
{
	/*
         * A level 15 hard interrupt.
         */
#ifdef noyet
	int fatal = 0;
#endif
	u_int32_t si;
	u_int afsr, afva;

	afsr = afva = 0;
	if ((*cpuinfo.get_asyncflt)(&afsr, &afva) == 0) {
		printf("Async registers (mid %d): afsr=%b; afva=0x%x%x\n",
		       cpuinfo.mid, afsr, AFSR_BITS,
		       (afsr & AFSR_AFA) >> AFSR_AFA_RSHIFT, afva);
	}

	if (cpuinfo.master == 0) {
		/*
		 * For now, just return.
		 * Should wait on damage analysis done by the master.
		 */
		return;
	}

	/*
	 * Examine pending system interrupts.
	 */
	si = *((u_int32_t *)ICR_SI_PEND);
	printf("NMI: system interrupts: %b\n", si, SINTR_BITS);

#ifdef notyet
	if ((si & SINTR_M) != 0) {
		/* ECC memory error */
                if (memerr_handler != NULL)
                        fatal |= (*memerr_handler)();
        }
        if ((si & SINTR_I) != 0) {
                /* MBus/SBus async error */
                if (sbuserr_handler != NULL)
                        fatal |= (*sbuserr_handler)();
        }
        if ((si & SINTR_V) != 0) {
                /* VME async error */
                if (vmeerr_handler != NULL)
                        fatal |= (*vmeerr_handler)();
        }
        if ((si & SINTR_ME) != 0) {
                /* Module async error */
                if (moduleerr_handler != NULL)
                        fatal |= (*moduleerr_handler)();
        }

        if (fatal)
#endif
                panic("nmi");
}
#endif

static struct intrhand level01 = { soft01intr, NULL, (IPL_SOFTINT << 8) };

void
intr_init()
{
	level01.ih_vec = level01.ih_ipl >> 8;
	evcount_attach(&level01.ih_count, "softintr", &level01.ih_vec,
	    &evcount_intr);
	level10.ih_vec = level10.ih_ipl >> 8;
	evcount_attach(&level10.ih_count, "clock", &level10.ih_vec,
	    &evcount_intr);
	level14.ih_vec = level14.ih_ipl >> 8;
	evcount_attach(&level14.ih_count, "prof", &level14.ih_vec,
	    &evcount_intr);
}

/*
 * Level 15 interrupts are special, and not vectored here.
 * Only `prewired' interrupts appear here; boot-time configured devices
 * are attached via intr_establish() below.
 */
struct intrhand *intrhand[15] = {
	NULL,			/*  0 = error */
	&level01,		/*  1 = software level 1 + SBus */
	NULL,	 		/*  2 = SBus level 2 (4m: SBus L1) */
	NULL,			/*  3 = SCSI + DMA + SBus level 3 (4m: L2,lpt)*/
	NULL,			/*  4 = software level 4 (tty softint) (scsi) */
	NULL,			/*  5 = Ethernet + SBus level 4 (4m: SBus L3) */
	NULL,			/*  6 = software level 6 (not used) (4m: enet)*/
	NULL,			/*  7 = video + SBus level 5 */
	NULL,			/*  8 = SBus level 6 */
	NULL,			/*  9 = SBus level 7 */
	&level10,		/* 10 = counter 0 = clock */
	NULL,			/* 11 = floppy */
	NULL,			/* 12 = zs hardware interrupt */
	NULL,			/* 13 = audio chip */
	&level14,		/* 14 = counter 1 = profiling timer */
};

static int fastvec;		/* marks fast vectors (see below) */
static struct {
	int (*cb)(void *);
	void *data;
} fastvec_share[15];

extern int sparc_interrupt4m[];
extern int sparc_interrupt44c[];

/*
 * Attach an interrupt handler to the vector chain for the given level.
 * This may not be possible if it has been taken away as a fast vector.
 */
void
intr_establish(level, ih, ipl_block, name)
	int level;
	struct intrhand *ih;
	int ipl_block;
	const char *name;
{
	struct intrhand **p, *q;
#ifdef DIAGNOSTIC
	struct trapvec *tv;
	int displ;
#endif
	int s;

	if (ipl_block == -1)
		ipl_block = level;

#ifdef DIAGNOSTIC
	/*
	 * If the level we're supposed to block is lower than this interrupts
	 * level someone is doing something very wrong. Most likely it
	 * means that some IPL_ constant in machine/psl.h is preconfigured too
	 * low.
	 */
	if (ipl_block < level)
		panic("intr_establish: level (%d) > block (%d)", level,
		    ipl_block);
	if (ipl_block > 15)
		panic("intr_establish: strange block level: %d", ipl_block);
#endif

	/*
	 * We store the ipl pre-shifted so that we can avoid one instruction
	 * in the interrupt handlers.
	 */
	ih->ih_vec = ipl_block;
	ih->ih_ipl = (ipl_block << 8);
	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_vec, &evcount_intr);

	s = splhigh();

	/*
	 * Check if this interrupt is already being handled by a fast trap.
	 * If so, attempt to change it back to a regular (thus) shareable
	 * trap.
	 */
	if (fastvec & (1 << level)) {
		if (fastvec_share[level].cb == NULL ||
		    (*fastvec_share[level].cb)(fastvec_share[level].data) != 0)
			panic("intr_establish: level %d interrupt tied to fast vector",
			    level);
	}

#ifdef DIAGNOSTIC
	/* double check for legal hardware interrupt */
	if ((level != 1 && level != 4 && level != 6) || CPU_ISSUN4M ) {
		tv = &trapbase[T_L1INT - 1 + level];
		displ = (CPU_ISSUN4M)
			? &sparc_interrupt4m[0] - &tv->tv_instr[1]
			: &sparc_interrupt44c[0] - &tv->tv_instr[1];

		/* has to be `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
		if (tv->tv_instr[0] != I_MOVi(I_L3, level) ||
		    tv->tv_instr[1] != I_BA(0, displ) ||
		    tv->tv_instr[2] != I_RDPSR(I_L0))
			panic("intr_establish(%d, %p)\n0x%x 0x%x 0x%x != 0x%x 0x%x 0x%x",
			    level, ih,
			    tv->tv_instr[0], tv->tv_instr[1], tv->tv_instr[2],
			    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
	}
#endif
	/*
	 * This is O(N^2) for long chains, but chains are never long
	 * and we do want to preserve order.
	 */
	for (p = &intrhand[level]; (q = *p) != NULL; p = &q->ih_next)
		continue;
	*p = ih;
	ih->ih_next = NULL;
	splx(s);
}

/*
 * Like intr_establish, but wires a fast trap vector.  Only one such fast
 * trap is legal for any interrupt, and it must be a hardware interrupt.
 * In case some other device wants to share the interrupt, we also register
 * a callback which will be able to revert this and register a slower, but
 * shareable trap vector if necessary (for example, to share int 13 between
 * audioamd and stp).
 */
int
intr_fasttrap(int level, void (*vec)(void), int (*share)(void *), void *cbdata)
{
	struct trapvec *tv;
	u_long hi22, lo10;
#ifdef DIAGNOSTIC
	int displ;	/* suspenders, belt, and buttons too */
#endif
	int s, i;
	int instr[3];
	char *instrp;
	char *tvp;

	tv = &trapbase[T_L1INT - 1 + level];
	hi22 = ((u_long)vec) >> 10;
	lo10 = ((u_long)vec) & 0x3ff;
	s = splhigh();

	/*
	 * If this interrupt is already being handled, fail; the caller will
	 * either panic or try to register a slow (shareable) trap.
	 */
	if ((fastvec & (1 << level)) != 0 || intrhand[level] != NULL) {
		splx(s);
		return (EBUSY);
	}

#ifdef DIAGNOSTIC
	displ = (CPU_ISSUN4M)
		? &sparc_interrupt4m[0] - &tv->tv_instr[1]
		: &sparc_interrupt44c[0] - &tv->tv_instr[1];

	/* has to be `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
	if (tv->tv_instr[0] != I_MOVi(I_L3, level) ||
	    tv->tv_instr[1] != I_BA(0, displ) ||
	    tv->tv_instr[2] != I_RDPSR(I_L0))
		panic("intr_fasttrap(%d, %p)\n0x%x 0x%x 0x%x != 0x%x 0x%x 0x%x",
		    level, vec,
		    tv->tv_instr[0], tv->tv_instr[1], tv->tv_instr[2],
		    I_MOVi(I_L3, level), I_BA(0, displ), I_RDPSR(I_L0));
#endif
	
	instr[0] = I_SETHI(I_L3, hi22);		/* sethi %hi(vec),%l3 */
	instr[1] = I_JMPLri(I_G0, I_L3, lo10);	/* jmpl %l3+%lo(vec),%g0 */
	instr[2] = I_RDPSR(I_L0);		/* mov %psr, %l0 */

	fastvec_share[level].cb = share;
	fastvec_share[level].data = cbdata;

	tvp = (char *)tv->tv_instr;
	instrp = (char *)instr;
	for (i = 0; i < sizeof(int) * 3; i++, instrp++, tvp++)
		pmap_writetext(tvp, *instrp);
	fastvec |= 1 << level;
	splx(s);

	return (0);
}

void
intr_fastuntrap(int level)
{
	struct trapvec *tv;
	int i, s;
	int displ;
	int instr[3];
	char *instrp;
	char *tvp;

	tv = &trapbase[T_L1INT - 1 + level];

	/* restore to `mov level,%l3; ba _sparc_interrupt; rdpsr %l0' */
	displ = (CPU_ISSUN4M)
		? &sparc_interrupt4m[0] - &tv->tv_instr[1]
		: &sparc_interrupt44c[0] - &tv->tv_instr[1];
	instr[0] = I_MOVi(I_L3, level);
	instr[1] = I_BA(0, displ);
	instr[2] = I_RDPSR(I_L0);

	s = splhigh();

#ifdef DIAGNOSTIC
	if ((fastvec & (1 << level)) == 0) {
		splx(s);
		return;
	}
#endif

	tvp = (char *)tv->tv_instr;
	instrp = (char *)instr;
	for (i = 0; i < sizeof(int) * 3; i++, instrp++, tvp++)
		pmap_writetext(tvp, *instrp);
	fastvec &= ~(1 << level);
	fastvec_share[level].cb = NULL;

	splx(s);
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl = (getpsr() & PSR_PIL) >> 8;

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		setpsr((getpsr() & ~PSR_PIL) | wantipl << 8);
	}
}
#endif

