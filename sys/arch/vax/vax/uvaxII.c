/*	$OpenBSD: uvaxII.c,v 1.13 2005/11/06 22:21:33 miod Exp $	*/
/*	$NetBSD: uvaxII.c,v 1.10 1996/10/13 03:36:04 christos Exp $	*/

/*-
 * Copyright (c) 1994 Gordon W. Ross 
 * Copyright (c) 1993 Adam Glass 
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1988, 1990, 1993
 * 	The Regents of the University of California.  All rights reserved.
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
 *      @(#)ka630.c     7.8 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <uvm/uvm_extern.h>

#include <machine/uvaxII.h>
#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>

struct uvaxIIcpu *uvaxIIcpu_ptr;

#if VAX630
struct	ka630clock *ka630clk_ptr;
static	time_t	ka630_clkread(int *);
static	void	ka630_clkwrite(time_t);

struct watclk {
    u_short wat_sec;
    u_short wat_min;
    u_short wat_hour;
    u_short wat_dow;
    u_short wat_day;
    u_short wat_month;
    u_short wat_year;
};

void gmt_to_wat (time_t *tp, struct watclk *wt);
void wat_to_gmt (struct watclk *wt, time_t *tp);
#endif

/*
 * uvaxII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
uvaxII_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	switch (cpu_type) {
	case VAX_630:
		strlcpy(cpu_model,"MicroVAX II", sizeof cpu_model);
		break;
	case VAX_410:
		strlcpy(cpu_model,"MicroVAX 2000", sizeof cpu_model);
		break;
	default:
		strlcpy(cpu_model, "MicroVAX 78032/78132", sizeof cpu_model);
		break;
	};
	printf(": %s\n", cpu_model);
}

int
uvaxII_clock()
{
	mtpr(0x40, PR_ICCS); /* Start clock and enable interrupt */
	return 1;
}

/* log crd errors */
void
uvaxII_memerr()
{
	printf("memory err!\n");
}

#define NMC78032 10
char *mc78032[] = {
	0,		"immcr (fsd)",	"immcr (ssd)",	"fpu err 0",
	"fpu err 7",	"mmu st(tb)",	"mmu st(m=0)",	"pte in p0",
	"pte in p1",	"un intr id",
};

struct mc78032frame {
	int	mc63_bcnt;		/* byte count == 0xc */
	int	mc63_summary;		/* summary parameter */
	int	mc63_mrvaddr;		/* most recent vad */
	int	mc63_istate;		/* internal state */
	int	mc63_pc;		/* trapped pc */
	int	mc63_psl;		/* trapped psl */
};

int
uvaxII_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc78032frame *mcf = (struct mc78032frame *)cmcf;
	register u_int type = mcf->mc63_summary;

	printf("machine check %x", type);
	if (type < NMC78032 && mc78032[type])
		printf(": %s", mc78032[type]);
	printf("\n\tvap %x istate %x pc %x psl %x\n",
	    mcf->mc63_mrvaddr, mcf->mc63_istate,
	    mcf->mc63_pc, mcf->mc63_psl);
	if (uvaxIIcpu_ptr && uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_MERR) {
		printf("\tmser=0x%x ", (int)uvaxIIcpu_ptr->uvaxII_mser);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_CPUE)
			printf("page=%d", (int)uvaxIIcpu_ptr->uvaxII_cear);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_DQPE)
			printf("page=%d", (int)uvaxIIcpu_ptr->uvaxII_dear);
		printf("\n");
	}
	return (-1);
}

/*
 * Handle the watch chip used by the ka630 and ka410 mother boards.
 */
u_long
uvaxII_gettodr(stopped_flag)
	int	*stopped_flag;
{
	register time_t year_secs;

	switch (cpu_type) {
#if VAX630
	case VAX_630:
		year_secs = ka630_clkread(stopped_flag);
		break;
#endif
	default:
		year_secs = 0;
		*stopped_flag = 1;
	};
	return (year_secs * 100);
}

void
uvaxII_settodr(year_ticks)
	time_t year_ticks;
{
	register time_t year_secs;

	year_secs = year_ticks / 100;
	switch (cpu_type) {
#if VAX630
	case VAX_630:
		ka630_clkwrite(year_secs);
		break;
#endif
	};
}

#if VAX630
/* init system time from tod clock */
/* ARGSUSED */
time_t
ka630_clkread(stopped_flag)
	int	*stopped_flag;
{
	register struct ka630clock *claddr = ka630clk_ptr;
	struct watclk wt;
	time_t year_secs;

	*stopped_flag = 0;

	claddr->csr1 = KA630CLK_SET;
	while ((claddr->csr0 & KA630CLK_UIP) != 0)
		;

	wt.wat_sec   = claddr->sec;
	wt.wat_min   = claddr->min;
	wt.wat_hour  = claddr->hr;
	wt.wat_day   = claddr->day;
	wt.wat_month = claddr->mon;
	wt.wat_year  = claddr->yr;

	/* If the clock is valid, use it. */
	if ((claddr->csr3 & KA630CLK_VRT) != 0 &&
	    (claddr->csr1 & KA630CLK_ENABLE) == KA630CLK_ENABLE) {
		/* simple sanity checks */
		if (wt.wat_month < 1 || wt.wat_month > 12 ||
		    wt.wat_day < 1 || wt.wat_day > 31) {
			printf("WARNING: preposterous clock chip time.\n");
			year_secs = 0;
		} else
			wat_to_gmt (&wt, &year_secs);

		claddr->csr0 = KA630CLK_RATE;
		claddr->csr1 = KA630CLK_ENABLE;

		return (year_secs);
	}

	printf("WARNING: TOY clock invalid.\n");
	return (0);
}

/* Set the time of day clock, called via. stime system call.. */
void
ka630_clkwrite(year_secs)
	time_t year_secs;
{
	register struct ka630clock *claddr = ka630clk_ptr;
	struct watclk wt;
	int s;

	gmt_to_wat (&year_secs, &wt);

	s = splhigh();

	claddr->csr1 = KA630CLK_SET;
	while ((claddr->csr0 & KA630CLK_UIP) != 0)
		;
 
	claddr->sec = wt.wat_sec;
	claddr->min = wt.wat_min;
	claddr->hr  = wt.wat_hour;
	claddr->day = wt.wat_day;
	claddr->mon = wt.wat_month;
	claddr->yr  = wt.wat_year;

	claddr->csr0 = KA630CLK_RATE;
	claddr->csr1 = KA630CLK_ENABLE;

	splx(s);
}
#endif

void
uvaxII_steal_pages()
{
	extern  vaddr_t avail_start, virtual_avail, avail_end;
	int	junk;

	/*
	 * MicroVAX II: get 10 pages from top of memory,
	 * map in Qbus map registers, cpu and clock registers.
	 */
	avail_end -= 10;

	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);
	MAPVIRT(nexus, btoc(0x400000));
	pmap_map((vaddr_t)nexus, 0x20088000, 0x20090000,
	    VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(uvaxIIcpu_ptr, 1);
	pmap_map((vaddr_t)uvaxIIcpu_ptr, (paddr_t)UVAXIICPU,
	    (paddr_t)UVAXIICPU + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(ka630clk_ptr, 1);
	pmap_map((vaddr_t)ka630clk_ptr, (paddr_t)KA630CLK,
	    (paddr_t)KA630CLK + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * Clear restart and boot in progress flags
	 * in the CPMBX.
	 */
	ka630clk_ptr->cpmbx = (ka630clk_ptr->cpmbx & KA630CLK_LANG);

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	uvaxIIcpu_ptr->uvaxII_mser = (UVAXIIMSER_PEN | UVAXIIMSER_MERR |
	    UVAXIIMSER_LEB);

	/*
	 * Set up cpu_type so that we can differ between 630 and 420.
	 */
        if (cpunumber == VAX_78032)
                cpu_type = (((*UVAXIISID) >> 24) & 0xff) |
		    (cpu_type & 0xff000000);
}

#if VAX630
/*
 * Generic routines to convert to or from a POSIX date
 * (seconds since 1/1/1970) and  yr/mo/day/hr/min/sec
 * (These are derived from the sun3 clock chip code.)
 */

/*
 * Machine dependent base year:
 * Note: must be < 1970
 */
#define	CLOCK_BASE_YEAR	1900

/* Traditional UNIX base year */
#define	POSIX_BASE_YEAR	1970
#define FEBRUARY	2

#define SECDAY		86400L
#define SECYR		(SECDAY * 365)

#define	leapyear(year)		((year) % 4 == 0)
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void
gmt_to_wat(tp, wt)
	time_t	*tp;
	struct	watclk *wt;
{
	register int i;
	register long days, secs;

	days = *tp / SECDAY;
	secs = *tp % SECDAY;

	/* Hours, minutes, seconds are easy */
	wt->wat_hour = secs / 3600;
	secs = secs % 3600;
	wt->wat_min  = secs / 60;
	secs = secs % 60;
	wt->wat_sec  = secs;

	/* Day of week (Note: 1/1/1970 was a Thursday) */
	wt->wat_dow = (days + 4) % 7;

	/* Number of years in days */
	i = POSIX_BASE_YEAR;
	while (days >= days_in_year(i)) {
		days -= days_in_year(i);
		i++;
	}
	wt->wat_year = i - CLOCK_BASE_YEAR;

	/* Number of months in days left */
	if (leapyear(i))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; days >= days_in_month(i); i++)
		days -= days_in_month(i);
	days_in_month(FEBRUARY) = 28;
	wt->wat_month = i;

	/* Days are what is left over (+1) from all that. */
	wt->wat_day = days + 1;  
}

void
wat_to_gmt(wt, tp)
	time_t	*tp;
	struct	watclk *wt;
{
	register int i;
	register long tmp;
	int year;

	/*
	 * Hours are different for some reason. Makes no sense really.
	 */

	tmp = 0;

	if (wt->wat_hour >= 24) goto out;
	if (wt->wat_day  >  31) goto out;
	if (wt->wat_month > 12) goto out;

	year = wt->wat_year + CLOCK_BASE_YEAR;

	/*
	 * Compute days since start of time
	 * First from years, then from months.
	 */
	for (i = POSIX_BASE_YEAR; i < year; i++)
		tmp += days_in_year(i);
	if (leapyear(year) && wt->wat_month > FEBRUARY)
		tmp++;

	/* Months */
	for (i = 1; i < wt->wat_month; i++)
	  	tmp += days_in_month(i);
	tmp += (wt->wat_day - 1);

	/* Now do hours */
	tmp = tmp * 24 + wt->wat_hour;

	/* Now do minutes */
	tmp = tmp * 60 + wt->wat_min;

	/* Now do seconds */
	tmp = tmp * 60 + wt->wat_sec;

 out:
	*tp = tmp;
}
#endif /* VAX630 */

