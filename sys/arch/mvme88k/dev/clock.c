/*	$OpenBSD: clock.c,v 1.17 2001/12/22 09:49:39 smurph Exp $ */
/*
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995 Nivas Madhur
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)clock.c	8.1 (Berkeley) 6/11/93
 */

/*
 * interval clock driver.
 */

#include <sys/param.h>
#include <sys/simplelock.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif

#include <machine/asm.h>
#include <machine/asm_macro.h>	/* for stack_pointer() */
#include <machine/board.h>	/* for register defines */
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/cpu.h>
#include <machine/cmmu.h>	/* DMA_CACHE_SYNC, etc... */
#include "pcctwo.h"
#if NPCCTWO > 0 
#include <mvme88k/dev/pcctwofunc.h>
#include <mvme88k/dev/pcctworeg.h>
#include "bugtty.h"
#if NBUGTTY > 0
#include <mvme88k/dev/bugttyfunc.h>
#endif
#endif
#include "syscon.h"
#if NSYSCON > 0 
#include <mvme88k/dev/sysconfunc.h>
#include <mvme88k/dev/sysconreg.h>
#endif
#include <mvme88k/dev/vme.h>

extern struct vme2reg *sys_vme2;
int timerok = 0;

u_long delay_factor = 1;

static int	clockmatch	__P((struct device *, void *, void *));
static void	clockattach	__P((struct device *, struct device *, void *));

void	sbc_initclock(void);
void	m188_initclock(void);
void	m188_timer_init	__P((unsigned));

struct clocksoftc {
	struct device	sc_dev;
	struct intrhand	sc_profih;
	struct intrhand	sc_statih;
};

struct cfattach clock_ca = {
        sizeof(struct clocksoftc), clockmatch, clockattach
}; 
 
struct cfdriver clock_cd = { 
        NULL, "clock", DV_DULL, 0
}; 

int	sbc_clockintr	__P((void *));
int	m188_clockintr	__P((void *));

int	clockbus;
u_char	prof_reset;

/*
 * Every machine must have a clock tick device of some sort; for this
 * platform this file manages it, no matter what form it takes.
 */
int
clockmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	register struct confargs *ca = args;
	register struct cfdata *cf = vcf;

	if (strcmp(cf->cf_driver->cd_name, "clock")) {
		return (0);
	}

	/*
	 * clock has to be at ipl 5
	 * We return the ipl here so that the parent can print
	 * a message if it is different from what ioconf.c says.
	 */
	ca->ca_ipl   = IPL_CLOCK;
	/* set size to 0 - see pcctwo.c:match for details */
	ca->ca_len  = 0;
	return (1);
}

void
clockattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct clocksoftc *sc = (struct clocksoftc *)self;

	clockbus = ca->ca_bustype;

	switch (clockbus) {
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sc->sc_profih.ih_fn = sbc_clockintr;
		sc->sc_profih.ih_arg = 0;
		sc->sc_profih.ih_wantframe = 1;
		sc->sc_profih.ih_ipl = ca->ca_ipl;
		prof_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER1, &sc->sc_profih);
		md.clock_init_func = sbc_initclock;
		printf(": VME1x7");
		break;
#endif /* NPCCTWO */
#if NSYSCON > 0 && defined(MVME188)
	case BUS_SYSCON:
		sc->sc_profih.ih_fn = m188_clockintr;
		sc->sc_profih.ih_arg = 0;
		sc->sc_profih.ih_wantframe = 1;
		sc->sc_profih.ih_ipl = ca->ca_ipl;
		sysconintr_establish(SYSCV_TIMER1, &sc->sc_profih);
		md.clock_init_func = m188_initclock;
		printf(": VME188");
		break;
#endif /* NSYSCON */
	}         
	printf("\n");
}

#if NPCCTWO > 0
void
sbc_initclock(void)
{
#ifdef CLOCK_DEBUG
	printf("SBC clock init\n");
#endif 
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}

	/* profclock */
	sys_pcc2->pcc2_t1ctl = 0;
	sys_pcc2->pcc2_t1cmp = pcc2_timer_us2lim(tick);
	sys_pcc2->pcc2_t1count = 0;
	sys_pcc2->pcc2_t1ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
			       PCC2_TCTL_COVF;
	sys_pcc2->pcc2_t1irq = prof_reset;

}

/*
 * clockintr: ack intr and call hardclock
 */
int
sbc_clockintr(eframe)
	void *eframe;
{
	sys_pcc2->pcc2_t1irq = prof_reset;
	
	/* increment intr counter */
	intrcnt[M88K_CLK_IRQ]++; 
	
	hardclock(eframe);
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */
	timerok = 1;
	return (1);
}
#endif /* NPCCTWO */
int
delay(us)
	register int us;
{
	volatile register int c;

	/*
	 * We use the vme system controller for the delay clock.
	 * Do not go to the real timer until vme device is present.
	 * Or, in the case of MVME188, not at all.
	 */
	if (sys_vme2 == NULL || brdtyp == BRD_188) {
		c = 3 * us;
		while (--c > 0);
		return (0);
	}
	sys_vme2->vme2_irql1 |= (0 << VME2_IRQL1_TIC1SHIFT);
	sys_vme2->vme2_t1count = 0;
	sys_vme2->vme2_tctl |= (VME2_TCTL1_CEN | VME2_TCTL1_COVF);

	while (sys_vme2->vme2_t1count < us)
		;
	sys_vme2->vme2_tctl &= ~(VME2_TCTL1_CEN | VME2_TCTL1_COVF);
	return (0);
}

#if NSYSCON > 0 
int counter = 0;
#define IST	
int
m188_clockintr(eframe)
	void *eframe;
{
	volatile int tmp;
	volatile int *dti_stop = (volatile int *)DART_STOPC;
	volatile int *dti_start = (volatile int *)DART_STARTC;
        volatile int *ist = (volatile int *)MVME188_IST;
#ifdef DEBUG
	register unsigned long sp;
#endif
	
	/* increment intr counter */
	intrcnt[M88K_CLK_IRQ]++; 
	/* acknowledge the timer interrupt */
	dma_cachectl(0xFFF82000, 0x1000, DMA_CACHE_SYNC_INVAL);
	tmp = *dti_stop;
	

#ifdef DEBUG
	/* check kernel stack for overflow */
	sp = stack_pointer();
	if (sp < UADDR + NBPG && sp > UADDR) {
		if (*ist & DTI_BIT) {
			printf("DTI not clearing!\n");
		}
		printf("kernel stack @ 0x%8x\n", sp);
		panic("stack overflow imminent!");
	}
#endif
	
#if 0
	/* clear the counter/timer output OP3 while we program the DART */
	*((volatile int *) DART_OPCR) = 0x00;

	/* do the stop counter/timer command */
	tmp = *((volatile int *) DART_STOPC);

	/* set counter/timer to counter mode, clock/16 */
	*((volatile int *) DART_ACR) = 0x30;

	*((volatile int *) DART_CTUR) = counter / 256;	     /* set counter MSB */
	*((volatile int *) DART_CTLR) = counter % 256;	     /* set counter LSB */
	*((volatile int *) DART_IVR) = SYSCV_TIMER1;	  /* set interrupt vec */
#endif 
	hardclock(eframe);
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */
	/* give the start counter/timer command */
	tmp = *dti_start;
#if 0
	*((volatile int *) DART_OPCR) = 0x04;
#endif 
	if (*ist & DTI_BIT) {
		printf("DTI not clearing!\n");
	}
	return (1);
}

void
m188_initclock(void)
{
#ifdef CLOCK_DEBUG
	printf("VME188 clock init\n");
#endif
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}
	m188_timer_init(tick);
}

void 
m188_timer_init(unsigned period)
{
	int imr;
	dma_cachectl(0xFFF82000, 0x1000, DMA_CACHE_SYNC_INVAL);

	/* make sure the counter range is proper. */
	if ( period < 9 )
		counter = 2;
	else if ( period > 284421 )
		counter = 65535;
	else
		counter	= period / 4.34;
#ifdef CLOCK_DEBUG
	printf("tick == %d, period == %d\n", tick, period);
	printf("timer will interrupt every %d usec\n", (int) (counter * 4.34));
#endif
	/* clear the counter/timer output OP3 while we program the DART */
	*((volatile int *) DART_OPCR) = 0x00;

	/* do the stop counter/timer command */
	imr = *((volatile int *) DART_STOPC);

	/* set counter/timer to counter mode, clock/16 */
	*((volatile int *) DART_ACR) = 0x30;

	*((volatile int *) DART_CTUR) = counter / 256;	    /* set counter MSB */
	*((volatile int *) DART_CTLR) = counter % 256;	    /* set counter LSB */
	*((volatile int *) DART_IVR) = SYSCV_TIMER1;	  /* set interrupt vec */
	
	/* give the start counter/timer command */
	/* (yes, this is supposed to be a read) */
	imr = *((volatile int *) DART_STARTC);

	/* set the counter/timer output OP3 */
	*((volatile int *) DART_OPCR) = 0x04;
}
#endif /* NSYSCON */

