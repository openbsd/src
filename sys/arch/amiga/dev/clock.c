/*	$OpenBSD: clock.c,v 1.16 2002/11/06 00:17:27 art Exp $	*/
/*	$NetBSD: clock.c,v 1.25 1997/01/02 20:59:42 is Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	7.6 (Berkeley) 5/7/91
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/cia.h>
#ifdef DRACO
#include <amiga/amiga/drcustom.h>
#endif
#include <amiga/amiga/isr.h>
#include <amiga/dev/rtc.h>
#include <amiga/dev/zbusvar.h>

#if defined(PROF) && defined(PROFTIMER)
#include <sys/PROF.h>
#endif

/* the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz. 
   We're using a 100 Hz clock. */

#define CLK_INTERVAL amiga_clk_interval
int amiga_clk_interval;
int eclockfreq;
struct CIA *clockcia;

#if defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
/*
 * The INT6 handler copies the clockframe from the stack in here as hardclock
 * may be delayed by the IPL-remapping code.  At that time the original stack
 * location will no longer be valid.
 */
struct clockframe hardclock_frame;
#endif

/*
 * Machine-dependent clock routines.
 *
 * Startrtclock restarts the real-time clock, which provides
 * hardclock interrupts to kern_clock.c.
 *
 * Inittodr initializes the time of day hardware which provides
 * date functions.
 *
 * Resettodr restores the time of day hardware after a time change.
 *
 * A note on the real-time clock:
 * We actually load the clock with CLK_INTERVAL-1 instead of CLK_INTERVAL.
 * This is because the counter decrements to zero after N+1 enabled clock
 * periods where N is the value loaded into the counter.
 */

int clockmatch(struct device *, void *, void *);
void clockattach(struct device *, struct device *, void *);
void cpu_initclocks(void);
void calibrate_delay(struct device *);
int clockintr(void *);

struct cfattach clock_ca = {
	sizeof(struct device), clockmatch, clockattach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL, NULL, 0 };

int
clockmatch(pdp, match, auxp)
	struct device *pdp;
	void *match, *auxp;
{

	if (matchname("clock", auxp))
		return(1);
	return(0);
}

/*
 * Start the real-time clock.
 */
void
clockattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	char *clockchip;
	unsigned short interval;
#ifdef DRACO
	u_char dracorev;
#endif

	if (eclockfreq == 0)
		eclockfreq = 715909;	/* guess NTSC */
		
	CLK_INTERVAL = (eclockfreq / 100);

#ifdef DRACO
	dracorev = is_draco();
	if (dracorev >= 4) {
		CLK_INTERVAL = (eclockfreq / 700);
		clockchip = "QuickLogic";
	} else if (dracorev) {
		clockcia = (struct CIA *)CIAAbase;
		clockchip = "CIA A";
	} else 
#endif
	{
		clockcia = (struct CIA *)CIABbase;
		clockchip = "CIA B";
	}

	if (dp)
		printf(": %s system hz %d hardware hz %d\n", clockchip, hz,
#ifdef DRACO
		dracorev >= 4 ? eclockfreq / 7 : eclockfreq);
#else
		eclockfreq);
#endif

#ifdef DRACO
	if (dracorev >= 4) {
		/* 
		 * can't preload anything beforehand, timer is free_running;
		 * but need this for delay calibration.
		 */

		draco_ioct->io_timerlo = CLK_INTERVAL & 0xff;
		draco_ioct->io_timerhi = CLK_INTERVAL >> 8;

		calibrate_delay(dp);

		return;
	}
#endif

	/*
	 * stop timer A 
	 */
	clockcia->cra = clockcia->cra & 0xc0;
	clockcia->icr = 1 << 0;		/* disable timer A interrupt */
	interval = clockcia->icr;		/* and make sure it's clear */

	/*
	 * load interval into registers.
         * the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz
	 * supprort for PAL WHEN?!?! XXX
	 */
	interval = CLK_INTERVAL - 1;

	/*
	 * order of setting is important !
	 */
	clockcia->talo = interval & 0xff;
	clockcia->tahi = interval >> 8;

	/*
	 * start timer A in continuous mode
	 */
	clockcia->cra = (clockcia->cra & 0xc0) | 1;

	calibrate_delay(dp);
}

#if defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
int
clockintr (arg)
	void *arg;
{
	/* Is it a timer A interrupt? */
	if (ciab.icr & 1) {
		hardclock(&hardclock_frame);
		return 1;
	}
	return 0;
}
#endif

/*
 * Calibrate delay loop.
 * We use two iterations because we don't have enough bits to do a factor of
 * 8 with better than 1%.
 *
 * XXX Note that we MUST stay below 1 tick if using clkread(), even for 
 * underestimated values of delaydivisor. 
 *
 * XXX the "ns" below is only correct for a shift of 10 bits, and even then
 * off by 2.4%
 */

void calibrate_delay(dp)
	struct device *dp;
{
	unsigned long t1, t2;
	extern u_int32_t delaydivisor;
		/* XXX this should be defined elsewhere */

	if (dp)
		printf("Calibrating delay loop... "); 

	do {
		t1 = clkread();
		delay(1024);
		t2 = clkread();
	} while (t2 <= t1);
	t2 -= t1;
	delaydivisor = (delaydivisor * t2 + 1023) >> 10;
#ifdef DIAGNOSTIC
	if (dp)
		printf("\ndiff %ld us, new divisor %u/1024 us\n", t2,
		    delaydivisor); 
	do {
		t1 = clkread();
		delay(1024);
		t2 = clkread();
	} while (t2 <= t1);
	t2 -= t1;
	delaydivisor = (delaydivisor * t2 + 1023) >> 10;
	if (dp)
		printf("diff %ld us, new divisor %u/1024 us\n", t2,
		    delaydivisor); 
#endif
	do {
		t1 = clkread();
		delay(1024);
		t2 = clkread();
	} while (t2 <= t1);
	t2 -= t1;
	delaydivisor = (delaydivisor * t2 + 1023) >> 10;
#ifdef DIAGNOSTIC
	if (dp)
		printf("diff %ld us, new divisor ", t2);
#endif
	if (dp)
		printf("%u/1024 us\n", delaydivisor); 
}

void
cpu_initclocks()
{
#if defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
	static struct isr isr;
#endif

#ifdef DRACO
	unsigned char dracorev;

	dracorev = is_draco();
	if (dracorev >= 4) {
		draco_ioct->io_timerlo = CLK_INTERVAL & 0xFF;
		draco_ioct->io_timerhi = CLK_INTERVAL >> 8;
		draco_ioct->io_timerrst = 0;	/* any value resets */
		draco_ioct->io_status2 |= DRSTAT2_TMRINTENA;

		return;
	}
#endif

	/*
	 * enable interrupts for timer A
	 */
	clockcia->icr = (1<<7) | (1<<0);

	/*
	 * start timer A in continuous shot mode
	 */
	clockcia->cra = (clockcia->cra & 0xc0) | 1;
  
#if defined(IPL_REMAP_1) || defined(IPL_REMAP_2)
	isr.isr_intr = clockintr;
	isr.isr_ipl = 6;
	isr.isr_mapped_ipl = IPL_CLOCK;
	add_isr(&isr);
#else
	/*
	 * and globally enable interrupts for ciab
	 */
#ifdef DRACO
	if (dracorev)		/* we use cia a on DraCo */
		*draco_intena |= DRIRQ_INT2;
	else
#endif
		custom.intena = INTF_SETCLR | INTF_EXTER;
#endif
}

void
setstatclockrate(hz)
	int hz;
{
}

/*
 * Returns number of usec since last recorded clock "tick"
 * (i.e. clock interrupt).
 */
u_long
clkread()
{
	u_int interval;
	u_char hi, hi2, lo;

#ifdef DRACO
	if (is_draco() >= 4) {
		hi2 = draco_ioct->io_chiprev;	/* latch timer */
		hi = draco_ioct->io_timerhi;
		lo = draco_ioct->io_timerlo;
		interval = ((hi<<8) | lo);
		if (interval > CLK_INTERVAL)	/* timer underflow */
			interval = 65536 + CLK_INTERVAL - interval;
		else
			interval = CLK_INTERVAL - interval;

	} else
#endif
	{
		hi  = clockcia->tahi;
		lo  = clockcia->talo;
		hi2 = clockcia->tahi;
		if (hi != hi2) {
			lo = clockcia->talo;
			hi = hi2;
		}

		interval = (CLK_INTERVAL - 1) - ((hi<<8) | lo);
   
		/*
		 * should read ICR and if there's an int pending, adjust
		 * interval. However, since reading ICR clears the interrupt,
		 * we'd lose a hardclock int, and this is not tolerable.
		 */
	}

	return((interval * tick) / CLK_INTERVAL);
}

#if notyet

/* implement this later. I'd suggest using both timers in CIA-A, they're
   not yet used. */

#include "clock.h"
#if NCLOCK > 0
/*
 * /dev/clock: mappable high resolution timer.
 *
 * This code implements a 32-bit recycling counter (with a 4 usec period)
 * using timers 2 & 3 on the 6840 clock chip.  The counter can be mapped
 * RO into a user's address space to achieve low overhead (no system calls),
 * high-precision timing.
 *
 * Note that timer 3 is also used for the high precision profiling timer
 * (PROFTIMER code above).  Care should be taken when both uses are
 * configured as only a token effort is made to avoid conflicting use.
 */
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <uvm/uvm_extern.h>
#include <amiga/amiga/clockioctl.h>
#include <sys/specdev.h>
#include <sys/vnode.h>
#include <sys/mman.h>

int clockon = 0;		/* non-zero if high-res timer enabled */
#ifdef PROFTIMER
int  profprocs = 0;		/* # of procs using profiling timer */
#endif
#ifdef DEBUG
int clockdebug = 0;
#endif

/*ARGSUSED*/
clockopen(dev, flags)
	dev_t dev;
{
#ifdef PROFTIMER
#ifdef PROF
	/*
	 * Kernel profiling enabled, give up.
	 */
	if (profiling)
		return(EBUSY);
#endif
	/*
	 * If any user processes are profiling, give up.
	 */
	if (profprocs)
		return(EBUSY);
#endif
	if (!clockon) {
		startclock();
		clockon++;
	}
	return(0);
}

/*ARGSUSED*/
clockclose(dev, flags)
	dev_t dev;
{
	(void) clockunmmap(dev, (caddr_t)0, curproc);	/* XXX */
	stopclock();
	clockon = 0;
	return(0);
}

/*ARGSUSED*/
clockioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	struct proc *p;
{
	int error = 0;
	
	switch (cmd) {

	case CLOCKMAP:
		error = clockmmap(dev, (caddr_t *)data, p);
		break;

	case CLOCKUNMAP:
		error = clockunmmap(dev, *(caddr_t *)data, p);
		break;

	case CLOCKGETRES:
		*(int *)data = CLK_RESOLUTION;
		break;

	default:
		error = EINVAL;
		break;
	}
	return(error);
}

/*ARGSUSED*/
clockmap(dev, off, prot)
	dev_t dev;
{
	return((off + (INTIOBASE+CLKBASE+CLKSR-1)) >> PGSHIFT);
}

clockmmap(dev, addrp, p)
	dev_t dev;
	caddr_t *addrp;
	struct proc *p;
{
	int error;
	struct vnode vn;
	struct specinfo si;
	int flags;

	flags = MAP_FILE|MAP_SHARED;
	if (*addrp)
		flags |= MAP_FIXED;
	else
		*addrp = (caddr_t)0x1000000;	/* XXX */
	vn.v_type = VCHR;			/* XXX */
	vn.v_specinfo = &si;			/* XXX */
	vn.v_rdev = dev;			/* XXX */
	error = vm_mmap(&p->p_vmspace->vm_map, (vm_offset_t *)addrp,
			PAGE_SIZE, VM_PROT_ALL, flags, (caddr_t)&vn, 0);
	return(error);
}

clockunmmap(dev, addr, p)
	dev_t dev;
	caddr_t addr;
	struct proc *p;
{
	int rv;

	if (addr == 0)
		return(EINVAL);		/* XXX: how do we deal with this? */
	uvm_deallocate(p->p_vmspace->vm_map, (vm_offset_t)addr, PAGE_SIZE);
	return (0);
}

startclock()
{
	register struct clkreg *clk = (struct clkreg *)clkstd[0];

	clk->clk_msb2 = -1; clk->clk_lsb2 = -1;
	clk->clk_msb3 = -1; clk->clk_lsb3 = -1;

	clk->clk_cr2 = CLK_CR3;
	clk->clk_cr3 = CLK_OENAB|CLK_8BIT;
	clk->clk_cr2 = CLK_CR1;
	clk->clk_cr1 = CLK_IENAB;
}

stopclock()
{
	register struct clkreg *clk = (struct clkreg *)clkstd[0];

	clk->clk_cr2 = CLK_CR3;
	clk->clk_cr3 = 0;
	clk->clk_cr2 = CLK_CR1;
	clk->clk_cr1 = CLK_IENAB;
}
#endif

#endif


#ifdef PROFTIMER
/*
 * This code allows the amiga kernel to use one of the extra timers on
 * the clock chip for profiling, instead of the regular system timer.
 * The advantage of this is that the profiling timer can be turned up to
 * a higher interrupt rate, giving finer resolution timing. The profclock
 * routine is called from the lev6intr in locore, and is a specialized
 * routine that calls addupc_task. The overhead then is far less than if
 * hardclock/softclock was called. Further, the context switch code in
 * locore has been changed to turn the profile clock on/off when switching
 * into/out of a process that is profiling (startprofclock/stopprofclock).
 * This reduces the impact of the profiling clock on other users, and might
 * possibly increase the accuracy of the profiling. 
 */
int  profint   = PRF_INTERVAL;	/* Clock ticks between interrupts */
int  profscale = 0;		/* Scale factor from sys clock to prof clock */
char profon    = 0;		/* Is profiling clock on? */

/* profon values - do not change, locore.s assumes these values */
#define PRF_NONE	0x00
#define	PRF_USER	0x01
#define	PRF_KERNEL	0x80

initprofclock()
{
#if NCLOCK > 0
	struct proc *p = curproc;		/* XXX */

	/*
	 * If the high-res timer is running, force profiling off.
	 * Unfortunately, this gets reflected back to the user not as
	 * an error but as a lack of results.
	 */
	if (clockon) {
		p->p_stats->p_prof.pr_scale = 0;
		return;
	}
	/*
	 * Keep track of the number of user processes that are profiling
	 * by checking the scale value.
	 *
	 * XXX: this all assumes that the profiling code is well behaved;
	 * i.e. profil() is called once per process with pcscale non-zero
	 * to turn it on, and once with pcscale zero to turn it off.
	 * Also assumes you don't do any forks or execs.  Oh well, there
	 * is always adb...
	 */
	if (p->p_stats->p_prof.pr_scale)
		profprocs++;
	else
		profprocs--;
#endif
	/*
	 * The profile interrupt interval must be an even divisor
	 * of the CLK_INTERVAL so that scaling from a system clock
	 * tick to a profile clock tick is possible using integer math.
	 */
	if (profint > CLK_INTERVAL || (CLK_INTERVAL % profint) != 0)
		profint = CLK_INTERVAL;
	profscale = CLK_INTERVAL / profint;
}

startprofclock()
{
  unsigned short interval;

  /* stop timer B */
  clockcia->crb = clockcia->crb & 0xc0;

  /* load interval into registers.
     the clocks run at NTSC: 715.909kHz or PAL: 709.379kHz */

  interval = profint - 1;

  /* order of setting is important ! */
  clockcia->tblo = interval & 0xff;
  clockcia->tbhi = interval >> 8;

  /* enable interrupts for timer B */
  clockcia->icr = (1<<7) | (1<<1);

  /* start timer B in continuous shot mode */
  clockcia->crb = (clockcia->crb & 0xc0) | 1;
}

stopprofclock()
{
  /* stop timer B */
  clockcia->crb = clockcia->crb & 0xc0;
}

#ifdef PROF
/*
 * profclock() is expanded in line in lev6intr() unless profiling kernel.
 * Assumes it is called with clock interrupts blocked.
 */
profclock(pc, ps)
	caddr_t pc;
	int ps;
{
	/*
	 * Came from user mode.
	 * If this process is being profiled record the tick.
	 */
	if (USERMODE(ps)) {
		if (p->p_stats.p_prof.pr_scale)
			addupc_task(&curproc, pc, 1);
	}
	/*
	 * Came from kernel (supervisor) mode.
	 * If we are profiling the kernel, record the tick.
	 */
	else if (profiling < 2) {
		register int s = pc - s_lowpc;

		if (s < s_textsize)
			kcount[s / (HISTFRACTION * sizeof (*kcount))]++;
	}
	/*
	 * Kernel profiling was on but has been disabled.
	 * Mark as no longer profiling kernel and if all profiling done,
	 * disable the clock.
	 */
	if (profiling && (profon & PRF_KERNEL)) {
		profon &= ~PRF_KERNEL;
		if (profon == PRF_NONE)
			stopprofclock();
	}
}
#endif
#endif

/*
 * Initialize the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
void
inittodr(base)
	time_t base;
{
	time_t timbuf = base;	/* assume no battery clock exists */
  
	if (gettod == NULL)
		printf("WARNING: no battery clock\n");
	else
		timbuf = gettod();
  
	if (timbuf < base) {
		printf("WARNING: bad date in battery clock\n");
		timbuf = base;
	}
  
	/* Battery clock does not store usec's, so forget about it. */
	time.tv_sec = timbuf;
}

void
resettodr()
{
	if (settod && settod(time.tv_sec) == 0)
		printf("Cannot set battery backed clock\n");
}
