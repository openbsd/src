/*	$OpenBSD: clock.c,v 1.10 1998/03/04 19:19:28 johns Exp $	*/
/*	$NetBSD: clock.c,v 1.52 1997/05/24 20:16:05 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1996 Paul Kranenburg
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Paul Kranenburg.
 *	This product includes software developed by Harvard University.
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
 *
 */

/*
 * Clock driver.  This is the id prom and eeprom driver as well
 * and includes the timer register functions too.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif

#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/eeprom.h>
#include <machine/cpu.h>

#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/clockreg.h>
#include <sparc/sparc/intreg.h>
#include <sparc/sparc/timerreg.h>

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 1024 would
 * give us offsets in [0..1023].  Instead, we take offsets in [1..1023].
 * This is symmetric about the point 512, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
/* XXX fix comment to match value */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */
int timerok;

#include <dev/ic/intersil7170.h>

extern struct idprom idprom;

#define intersil_command(run, interrupt) \
    (run | interrupt | INTERSIL_CMD_FREQ_32K | INTERSIL_CMD_24HR_MODE | \
     INTERSIL_CMD_NORMAL_MODE)

#define intersil_disable(CLOCK) \
    CLOCK->clk_cmd_reg = \
    intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IDISABLE)

#define intersil_enable(CLOCK) \
    CLOCK->clk_cmd_reg = \
    intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE)

#define intersil_clear(CLOCK) CLOCK->clk_intr_reg

#if defined(SUN4)
/*
 * OCLOCK support: 4/100's and 4/200's have the old clock.
 */
static int oldclk = 0;
struct intersil7170 *i7;

long	oclk_get_secs __P((void));
void	oclk_get_dt __P((struct intersil_dt *));
void	dt_to_gmt __P((struct intersil_dt *, long *));
void	oclk_set_dt __P((struct intersil_dt *));
void	oclk_set_secs __P((long));
void	gmt_to_dt __P((long *, struct intersil_dt *));
#endif

int	oclockmatch __P((struct device *, void *, void *));
void	oclockattach __P((struct device *, struct device *, void *));

struct cfattach oclock_ca = {
	sizeof(struct device), oclockmatch, oclockattach
};

struct cfdriver oclock_cd = {
	NULL, "oclock", DV_DULL
};

/*
 * Sun 4 machines use the old-style (a'la Sun 3) EEPROM.  On the
 * 4/100's and 4/200's, this is at a separate obio space.  On the
 * 4/300's and 4/400's, however, it is the cl_nvram[] chunk of the
 * Mostek chip.  Therefore, eeprom_match will only return true on
 * the 100/200 models, and the eeprom will be attached separately.
 * On the 300/400 models, the eeprom will be dealt with when the clock is
 * attached.
 */
char		*eeprom_va = NULL;
#if defined(SUN4)
static int	eeprom_busy = 0;
static int	eeprom_wanted = 0;
static int	eeprom_nvram = 0;	/* non-zero if eeprom is on Mostek */
int	eeprom_take __P((void));
void	eeprom_give __P((void));
int	eeprom_update __P((char *, int, int));
#endif

int	eeprom_match __P((struct device *, void *, void *));
void	eeprom_attach __P((struct device *, struct device *, void *));

struct cfattach eeprom_ca = {
	sizeof(struct device), eeprom_match, eeprom_attach
};

struct	cfdriver eeprom_cd = {
	NULL, "eeprom", DV_DULL
};

int	clockmatch __P((struct device *, void *, void *));
void	clockattach __P((struct device *, struct device *, void *));

struct cfattach clock_ca = {
	sizeof(struct device), clockmatch, clockattach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

int	timermatch __P((struct device *, void *, void *));
void	timerattach __P((struct device *, struct device *, void *));

struct timer_4m	*timerreg_4m;	/* XXX - need more cleanup */
struct counter_4m	*counterreg_4m;
#define	timerreg4		((struct timerreg_4 *)TIMERREG_VA)

struct cfattach timer_ca = {
	sizeof(struct device), timermatch, timerattach
};

struct cfdriver timer_cd = {
	NULL, "timer", DV_DULL
};

struct chiptime;
void clk_wenable __P((int));
void myetheraddr __P((u_char *));
int chiptotime __P((int, int, int, int, int, int));
void timetochip __P((struct chiptime *));

int timerblurb = 10; /* Guess a value; used before clock is attached */

/*
 * old clock match routine
 */
int
oclockmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct confargs *ca = aux;

	/* Only these sun4s have oclock */
	if (!CPU_ISSUN4 ||
	    (cpuinfo.cpu_type != CPUTYP_4_100 &&
	     cpuinfo.cpu_type != CPUTYP_4_200))
		return (0);

	/* Check configuration name */
	if (strcmp(oclock_cd.cd_name, ca->ca_ra.ra_name) != 0)
		return (0);

	/* Make sure there is something there */
	if (probeget(ca->ca_ra.ra_vaddr, 1) == -1)
		return (0);

	return (1);
}

/* ARGSUSED */
void
oclockattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#if defined(SUN4)
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct idprom *idp;
	register int h;

	oldclk = 1;  /* we've got an oldie! */

	i7 = (struct intersil7170 *) mapiodev(ra->ra_reg, 0, sizeof(*i7));

	idp = &idprom;
	h = idp->id_machine << 24;
	h |= idp->id_hostid[0] << 16;
	h |= idp->id_hostid[1] << 8;
	h |= idp->id_hostid[2];
	hostid = h;

	/* 
	 * calibrate delay() 
	 */
	ienab_bic(IE_L14 | IE_L10);	/* disable all clock intrs */
	for (timerblurb = 1; ; timerblurb++) {
		volatile register char *ireg = &i7->clk_intr_reg;
		register int ival;
		*ireg = INTERSIL_INTER_CSECONDS; /* 1/100 sec */
		intersil_enable(i7);		 /* enable clock */
		while ((*ireg & INTERSIL_INTER_PENDING) == 0)
			/* sync with interrupt */;
		while ((*ireg & INTERSIL_INTER_PENDING) == 0)
			/* XXX: do it again, seems to need it */;
		delay(10000);			/* Probe 1/100 sec delay */
		ival = *ireg;			/* clear, save value */
		intersil_disable(i7);		/* disable clock */
		if (ival & INTERSIL_INTER_PENDING) {
			printf(" delay constant %d%s\n", timerblurb,
				(timerblurb == 1) ? " [TOO SMALL?]" : "");
			break;
		}
		if (timerblurb > 10) {
			printf("\noclock: calibration failing; clamped at %d\n",
			       timerblurb);
			break;
		}
	}
#endif /* SUN4 */
}

/*
 * Sun 4/100, 4/200 EEPROM match routine.
 */
int
eeprom_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;

	if (!CPU_ISSUN4)
		return (0);

	if (cf->cf_unit != 0)
		return (0);

	if (cpuinfo.cpu_type != CPUTYP_4_100 &&
	    cpuinfo.cpu_type != CPUTYP_4_200)
		return (0);

	if (strcmp(eeprom_cd.cd_name, ca->ca_ra.ra_name) != 0)
		return (0);

	/*
	 * Make sure there's something there...
	 * This is especially important if we want to
	 * use the same kernel on a 4/100 as a 4/200.
	 */
	if (probeget(ca->ca_ra.ra_vaddr, 1) == -1)
		return (0);

	/* Passed all tests */
	return (1);
}

void
eeprom_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#if defined(SUN4)
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	printf("\n");

	eeprom_va = (char *)mapiodev(ra->ra_reg, 0, EEPROM_SIZE);

	eeprom_nvram = 0;
#endif /* SUN4 */
}

/*
 * The OPENPROM calls the clock the "eeprom", so we have to have our
 * own special match function to call it the "clock".
 */
int
clockmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct confargs *ca = aux;

	if (CPU_ISSUN4) {
		/* Only these sun4s have "clock" (others have "oclock") */
		if (cpuinfo.cpu_type != CPUTYP_4_300 &&
		    cpuinfo.cpu_type != CPUTYP_4_400)
			return (0);

		if (strcmp(clock_cd.cd_name, ca->ca_ra.ra_name) != 0)
			return (0);

		/* Make sure there is something there */
		if (probeget(ca->ca_ra.ra_vaddr, 1) == -1)
			return (0);

		return (1);
	}

	return (strcmp("eeprom", ca->ca_ra.ra_name) == 0);
}

/* ARGSUSED */
void
clockattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register int h;
	register struct clockreg *cl;
	register struct idprom *idp;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	char *prop = NULL;

	if (CPU_ISSUN4)
		prop = "mk48t02";

	if (CPU_ISSUN4COR4M)
		prop = getpropstring(ra->ra_node, "model");

#ifdef DIAGNOSTIC
	if (prop == NULL)
		panic("no prop");
#endif
	printf(": %s (eeprom)\n", prop);

	/*
	 * We ignore any existing virtual address as we need to map
	 * this read-only and make it read-write only temporarily,
	 * whenever we read or write the clock chip.  The clock also
	 * contains the ID ``PROM'', and I have already had the pleasure
	 * of reloading the cpu type, Ethernet address, etc, by hand from
	 * the console FORTH interpreter.  I intend not to enjoy it again.
	 */
	if (strcmp(prop, "mk48t08") == 0) {
		/*
		 * the MK48T08 is 8K
		 */
		cl = (struct clockreg *)mapiodev(ra->ra_reg, 0, 8192);
		pmap_changeprot(pmap_kernel(), (vm_offset_t)cl, VM_PROT_READ, 1);
		pmap_changeprot(pmap_kernel(), (vm_offset_t)cl + 4096,
				VM_PROT_READ, 1);
		cl = (struct clockreg *)((int)cl + CLK_MK48T08_OFF);
	} else {
		/*
		 * the MK48T02 is 2K
		 */
		cl = (struct clockreg *)mapiodev(ra->ra_reg, 0,
						 sizeof *clockreg);
		pmap_changeprot(pmap_kernel(), (vm_offset_t)cl, VM_PROT_READ, 1);
	}
	idp = &cl->cl_idprom;

#if defined(SUN4)
	if (CPU_ISSUN4) {
		idp = &idprom;

		if (cpuinfo.cpu_type == CPUTYP_4_300 ||
		    cpuinfo.cpu_type == CPUTYP_4_400) {
			eeprom_va = (char *)cl->cl_nvram;
			eeprom_nvram = 1;
		}
	}
#endif

	h = idp->id_machine << 24;
	h |= idp->id_hostid[0] << 16;
	h |= idp->id_hostid[1] << 8;
	h |= idp->id_hostid[2];
	hostid = h;
	clockreg = cl;
}

/*
 * The OPENPROM calls the timer the "counter-timer".
 */
int
timermatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct confargs *ca = aux;

	if (CPU_ISSUN4) {
		if (cpuinfo.cpu_type != CPUTYP_4_300 &&
		    cpuinfo.cpu_type != CPUTYP_4_400)
			return (0);

		if (strcmp("timer", ca->ca_ra.ra_name) != 0)
			return (0);

		/* Make sure there is something there */
		if (probeget(ca->ca_ra.ra_vaddr, 4) == -1)
			return (0);

		return (1);
	}

	if (CPU_ISSUN4C) {
		return (strcmp("counter-timer", ca->ca_ra.ra_name) == 0);
	}

	if (CPU_ISSUN4M) {
		return (strcmp("counter", ca->ca_ra.ra_name) == 0);
	}

	return (0);
}

/* ARGSUSED */
void
timerattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;
	volatile int *cnt = NULL, *lim = NULL;
		/* XXX: must init to NULL to avoid stupid gcc -Wall warning */

	if (CPU_ISSUN4M) {
		(void)mapdev(&ra->ra_reg[ra->ra_nreg-1], TIMERREG_VA, 0,
			     sizeof(struct timer_4m));
		(void)mapdev(&ra->ra_reg[0], COUNTERREG_VA, 0,
			     sizeof(struct counter_4m));
		timerreg_4m = (struct timer_4m *)TIMERREG_VA;
		counterreg_4m = (struct counter_4m *)COUNTERREG_VA;

		/* Put processor counter in "timer" mode */
		timerreg_4m->t_cfg = 0;

		cnt = &counterreg_4m->t_counter;
		lim = &counterreg_4m->t_limit;
	}

	if (CPU_ISSUN4OR4C) {
		/*
		 * This time, we ignore any existing virtual address because
		 * we have a fixed virtual address for the timer, to make
		 * microtime() faster (in SUN4/SUN4C kernel only).
		 */
		(void)mapdev(ra->ra_reg, TIMERREG_VA, 0,
			     sizeof(struct timerreg_4));

		cnt = &timerreg4->t_c14.t_counter;
		lim = &timerreg4->t_c14.t_limit;
	}

	timerok = 1;

	/*
	 * Calibrate delay() by tweaking the magic constant
	 * until a delay(100) actually reads (at least) 100 us on the clock.
	 * Note: sun4m clocks tick with 500ns periods.
	 */

	for (timerblurb = 1; ; timerblurb++) {
		volatile int discard;
		register int t0, t1;

		/* Reset counter register by writing some large limit value */
		discard = *lim;
		*lim = tmr_ustolim(TMR_MASK-1);

		t0 = *cnt;
		delay(100);
		t1 = *cnt;

		if (t1 & TMR_LIMIT)
			panic("delay calibration");

		t0 = (t0 >> TMR_SHIFT) & TMR_MASK;
		t1 = (t1 >> TMR_SHIFT) & TMR_MASK;

		if (t1 >= t0 + 100)
			break;

	}

	printf(" delay constant %d\n", timerblurb);

	/* should link interrupt handlers here, rather than compiled-in? */
}

/*
 * Write en/dis-able clock registers.  We coordinate so that several
 * writers can run simultaneously.
 */
void
clk_wenable(onoff)
	int onoff;
{
	register int s;
	register vm_prot_t prot;/* nonzero => change prot */
	static int writers;

	s = splhigh();
	if (onoff)
		prot = writers++ == 0 ? VM_PROT_READ|VM_PROT_WRITE : 0;
	else
		prot = --writers == 0 ? VM_PROT_READ : 0;
	splx(s);
	if (prot)
		pmap_changeprot(pmap_kernel(),
				(vm_offset_t)clockreg & ~(NBPG-1),
				prot, 1);
}

/*
 * XXX this belongs elsewhere
 */
void
myetheraddr(cp)
	u_char *cp;
{
	register struct clockreg *cl = clockreg;
	register struct idprom *idp = &cl->cl_idprom;

#if defined(SUN4)
	if (CPU_ISSUN4)
		idp = &idprom;
#endif

	cp[0] = idp->id_ether[0];
	cp[1] = idp->id_ether[1];
	cp[2] = idp->id_ether[2];
	cp[3] = idp->id_ether[3];
	cp[4] = idp->id_ether[4];
	cp[5] = idp->id_ether[5];
}

/*
 * Set up the real-time and statistics clocks.  Leave stathz 0 only if
 * no alternative timer is available.
 *
 * The frequencies of these clocks must be an even number of microseconds.
 */
void
cpu_initclocks()
{
	register int statint, minint;

#if defined(SUN4)
	if (oldclk) {
		int dummy;

		if (hz != 100) {
			printf("oclock0: cannot get %d Hz clock; using 100 Hz\n", hz);
		}

		profhz = hz = 100;
		tick = 1000000 / hz;

		i7->clk_intr_reg = INTERSIL_INTER_CSECONDS; /* 1/100 sec */

		ienab_bic(IE_L14 | IE_L10);	/* disable all clock intrs */
		intersil_disable(i7);		/* disable clock */
		dummy = intersil_clear(i7);	/* clear interrupts */
		ienab_bis(IE_L10);		/* enable l10 interrupt */
		intersil_enable(i7);		/* enable clock */

		return;
	}
#endif /* SUN4 */

	if (1000000 % hz) {
		printf("clock0: cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}
	if (stathz == 0)
		stathz = hz;
	if (1000000 % stathz) {
		printf("clock0: cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	if (CPU_ISSUN4M) {
		timerreg_4m->t_limit = tmr_ustolim4m(tick);
		counterreg_4m->t_limit = tmr_ustolim4m(statint);
	}

	if (CPU_ISSUN4OR4C) {
		timerreg4->t_c10.t_limit = tmr_ustolim(tick);
		timerreg4->t_c14.t_limit = tmr_ustolim(statint);
	}

	statmin = statint - (statvar >> 1);

#if defined(SUN4M)
	if (CPU_ISSUN4M)
		ienab_bic(SINTR_T);
#endif

	if (CPU_ISSUN4OR4C)
		ienab_bis(IE_L14 | IE_L10);

}

/*
 * Dummy setstatclockrate(), since we know profhz==hz.
 */
/* ARGSUSED */
void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing */
}

/*
 * Level 10 (clock) interrupts.  If we are using the FORTH PROM for
 * console input, we need to check for that here as well, and generate
 * a software interrupt to read it.
 */
int
clockintr(cap)
	void *cap;
{
	register volatile int discard;
	int s;
	extern int rom_console_input;

	/*
	 * Protect the clearing of the clock interrupt.  If we don't
	 * do this, and we're interrupted (by the zs, for example),
	 * the clock stops!
	 * XXX WHY DOES THIS HAPPEN?
	 */
	s = splhigh();

#if defined(SUN4)
	if (oldclk) {
		discard = intersil_clear(i7);
		ienab_bic(IE_L10);  /* clear interrupt */
		ienab_bis(IE_L10);  /* enable interrupt */
		goto forward;
	}
#endif
	/* read the limit register to clear the interrupt */
	if (CPU_ISSUN4M) {
		discard = timerreg_4m->t_limit;
	}

	if (CPU_ISSUN4OR4C) {
		discard = timerreg4->t_c10.t_limit;
	}
#if defined(SUN4)
forward:
#endif
	splx(s);

	hardclock((struct clockframe *)cap);
	if (rom_console_input && cnrom())
		setsoftint();

	return (1);
}

/*
 * Level 14 (stat clock) interrupt handler.
 */
int
statintr(cap)
	void *cap;
{
	register volatile int discard;
	register u_long newint, r, var;

#if defined(SUN4)
	if (oldclk) {
		panic("oldclk statintr");
		return (1);
	}
#endif

	/* read the limit register to clear the interrupt */
	if (CPU_ISSUN4M) {
		discard = counterreg_4m->t_limit;
		if (timerok == 0) {
			/* Stop the clock */
			counterreg_4m->t_limit = 0;
			counterreg_4m->t_ss = 0;
			timerreg_4m->t_cfg = TMR_CFG_USER;
			return 1;
		}
	}

	if (CPU_ISSUN4OR4C) {
		discard = timerreg4->t_c14.t_limit;
	}
	statclock((struct clockframe *)cap);

	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	if (CPU_ISSUN4M) {
		counterreg_4m->t_limit = tmr_ustolim4m(newint);
	}

	if (CPU_ISSUN4OR4C) {
		timerreg4->t_c14.t_limit = tmr_ustolim(newint);
	}
	return (1);
}

/*
 * BCD to decimal and decimal to BCD.
 */
#define	FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define	TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

#define	SECDAY		(24 * 60 * 60)
#define	SECYR		(SECDAY * 365)
/*
 * should use something like
 * #define LEAPYEAR(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
 * but it's unlikely that we'll still be around in 2100.
 */
#define	LEAPYEAR(y)	(((y) & 3) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

int
chiptotime(sec, min, hour, day, mon, year)
	register int sec, min, hour, day, mon, year;
{
	register int days, yr;

	sec = FROMBCD(sec);
	min = FROMBCD(min);
	hour = FROMBCD(hour);
	day = FROMBCD(day);
	mon = FROMBCD(mon);
	year = FROMBCD(year) + YEAR0;

	/* simple sanity checks */
	if (year < 70 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	days = 0;
	for (yr = 70; yr < year; yr++)
		days += LEAPYEAR(yr) ? 366 : 365;
	days += dayyr[mon - 1] + day - 1;
	if (LEAPYEAR(yr) && mon > 2)
		days++;
	/* now have days since Jan 1, 1970; the rest is easy... */
	return (days * SECDAY + hour * 3600 + min * 60 + sec);
}

struct chiptime {
	int	sec;
	int	min;
	int	hour;
	int	wday;
	int	day;
	int	mon;
	int	year;
};

void
timetochip(c)
	register struct chiptime *c;
{
	register int t, t2, t3, now = time.tv_sec;

	/* compute the year */
	t2 = now / SECDAY;
	t3 = (t2 + 2) % 7;	/* day of week */
	c->wday = TOBCD(t3 + 1);

	t = 69;
	while (t2 >= 0) {	/* whittle off years */
		t3 = t2;
		t++;
		t2 -= LEAPYEAR(t) ? 366 : 365;
	}
	c->year = t;

	/* t3 = month + day; separate */
	t = LEAPYEAR(t);
	for (t2 = 1; t2 < 12; t2++)
		if (t3 < dayyr[t2] + (t && t2 > 1))
			break;

	/* t2 is month */
	c->mon = t2;
	c->day = t3 - dayyr[t2 - 1] + 1;
	if (t && t2 > 2)
		c->day--;

	/* the rest is easy */
	t = now % SECDAY;
	c->hour = t / 3600;
	t %= 3600;
	c->min = t / 60;
	c->sec = t % 60;

	c->sec = TOBCD(c->sec);
	c->min = TOBCD(c->min);
	c->hour = TOBCD(c->hour);
	c->day = TOBCD(c->day);
	c->mon = TOBCD(c->mon);
	c->year = TOBCD(c->year - YEAR0);
}

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(base)
	time_t base;
{
	register struct clockreg *cl = clockreg;
	int sec, min, hour, day, mon, year;
	int badbase = 0, waszero = base == 0;

	if (base < 5 * SECYR) {
		/*
		 * If base is 0, assume filesystem time is just unknown
		 * in stead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		base = 21*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	}
#if defined(SUN4)
	if (oldclk) {
		time.tv_sec = oclk_get_secs();
		goto forward;
	}
#endif
	clk_wenable(1);
	cl->cl_csr |= CLK_READ;		/* enable read (stop time) */
	sec = cl->cl_sec;
	min = cl->cl_min;
	hour = cl->cl_hour;
	day = cl->cl_mday;
	mon = cl->cl_month;
	year = cl->cl_year;
	cl->cl_csr &= ~CLK_READ;	/* time wears on */
	clk_wenable(0);
	time.tv_sec = chiptotime(sec, min, hour, day, mon, year);

#if defined(SUN4)
forward:
#endif
	if (time.tv_sec == 0) {
		printf("WARNING: bad date in battery clock");
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the clock.
		 */
		time.tv_sec = base;
		if (!badbase)
			resettodr();
	} else {
		int deltat = time.tv_sec - base;

		if (deltat < 0)
			deltat = -deltat;
		if (waszero || deltat < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %d days",
		    time.tv_sec < base ? "lost" : "gained", deltat / SECDAY);
	}
	printf(" -- CHECK AND RESET THE DATE!\n");
}

/*
 * Reset the clock based on the current time.
 * Used when the current clock is preposterous, when the time is changed,
 * and when rebooting.  Do nothing if the time is not yet known, e.g.,
 * when crashing during autoconfig.
 */
void
resettodr()
{
	register struct clockreg *cl;
	struct chiptime c;

#if defined(SUN4)
	if (oldclk) {
		if (!time.tv_sec || i7 == NULL)
			return;
		oclk_set_secs(time.tv_sec);
		return;
	}
#endif

	if (!time.tv_sec || (cl = clockreg) == NULL)
		return;
	timetochip(&c);
	clk_wenable(1);
	cl->cl_csr |= CLK_WRITE;	/* enable write */
	cl->cl_sec = c.sec;
	cl->cl_min = c.min;
	cl->cl_hour = c.hour;
	cl->cl_wday = c.wday;
	cl->cl_mday = c.day;
	cl->cl_month = c.mon;
	cl->cl_year = c.year;
	cl->cl_csr &= ~CLK_WRITE;	/* load them up */
	clk_wenable(0);
}

#if defined(SUN4)
/*
 * Now routines to get and set clock as POSIX time.
 */
long
oclk_get_secs()
{
        struct intersil_dt dt;
        long gmt;

        oclk_get_dt(&dt);
        dt_to_gmt(&dt, &gmt);
        return (gmt);
}

void
oclk_set_secs(secs)
	long secs;
{
        struct intersil_dt dt;
        long gmt;

        gmt = secs;
        gmt_to_dt(&gmt, &dt);
        oclk_set_dt(&dt);
}

/*
 * Routine to copy state into and out of the clock.
 * The clock registers have to be read or written
 * in sequential order (or so it appears). -gwr
 */
void
oclk_get_dt(dt)
	struct intersil_dt *dt;
{
        int s;
        register volatile char *src, *dst;

        src = (char *) &i7->counters;

        s = splhigh();
        i7->clk_cmd_reg =
                intersil_command(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);

        dst = (char *) dt;
        dt++;   /* end marker */
        do {
                *dst++ = *src++;
        } while (dst < (char*)dt);

        i7->clk_cmd_reg =
                intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
        splx(s);
}

void
oclk_set_dt(dt)
	struct intersil_dt *dt;
{
        int s;
        register volatile char *src, *dst;

        dst = (char *) &i7->counters;

        s = splhigh();
        i7->clk_cmd_reg =
                intersil_command(INTERSIL_CMD_STOP, INTERSIL_CMD_IENABLE);

        src = (char *) dt;
        dt++;   /* end marker */
        do {
                *dst++ = *src++;
        } while (src < (char *)dt);

        i7->clk_cmd_reg =
                intersil_command(INTERSIL_CMD_RUN, INTERSIL_CMD_IENABLE);
        splx(s);
}


/*
 * Machine dependent base year:
 * Note: must be < 1970
 */
#define CLOCK_BASE_YEAR 1968

/* Traditional UNIX base year */
#define POSIX_BASE_YEAR 1970
#define FEBRUARY        2

#define leapyear(year)          ((year) % 4 == 0)
#define days_in_year(a)         (leapyear(a) ? 366 : 365)
#define days_in_month(a)        (month_days[(a) - 1])

static int month_days[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void
gmt_to_dt(tp, dt)
	long *tp;
	struct intersil_dt *dt;
{
        register int i;
        register long days, secs;

        days = *tp / SECDAY;
        secs = *tp % SECDAY;

        /* Hours, minutes, seconds are easy */
        dt->dt_hour = secs / 3600;
        secs = secs % 3600;
        dt->dt_min  = secs / 60;
        secs = secs % 60;
        dt->dt_sec  = secs;

        /* Day of week (Note: 1/1/1970 was a Thursday) */
        dt->dt_dow = (days + 4) % 7;

        /* Number of years in days */
        i = POSIX_BASE_YEAR;
        while (days >= days_in_year(i)) {
                days -= days_in_year(i);
                i++;
        }
        dt->dt_year = i - CLOCK_BASE_YEAR;

        /* Number of months in days left */
        if (leapyear(i))
                days_in_month(FEBRUARY) = 29;
        for (i = 1; days >= days_in_month(i); i++)
                days -= days_in_month(i);
        days_in_month(FEBRUARY) = 28;
        dt->dt_month = i;

        /* Days are what is left over (+1) from all that. */
        dt->dt_day = days + 1;
}


void
dt_to_gmt(dt, tp)
	struct intersil_dt *dt;
	long *tp;
{
        register int i;
        register long tmp;
        int year;

        /*
         * Hours are different for some reason. Makes no sense really.
         */

        tmp = 0;

        if (dt->dt_hour >= 24) goto out;
        if (dt->dt_day  >  31) goto out;
        if (dt->dt_month > 12) goto out;

        year = dt->dt_year + CLOCK_BASE_YEAR;


        /*
         * Compute days since start of time
         * First from years, then from months.
         */
        for (i = POSIX_BASE_YEAR; i < year; i++)
                tmp += days_in_year(i);
        if (leapyear(year) && dt->dt_month > FEBRUARY)
                tmp++;

        /* Months */
        for (i = 1; i < dt->dt_month; i++)
                tmp += days_in_month(i);
        tmp += (dt->dt_day - 1);

        /* Now do hours */
        tmp = tmp * 24 + dt->dt_hour;

        /* Now do minutes */
        tmp = tmp * 60 + dt->dt_min;

        /* Now do seconds */
        tmp = tmp * 60 + dt->dt_sec;

out:
        *tp = tmp;
}
#endif /* SUN4 */

#if defined(SUN4)
/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt.
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for
 * fun, we guarantee that the time will be greater than the value
 * obtained by a previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s;
	static struct timeval lasttime;
	static struct timeval oneusec = {0, 1};

	if (!oldclk) {
		lo_microtime(tvp);
		return;
	}

	s = splhigh();
	*tvp = time;
	splx(s);

	if (timercmp(tvp, &lasttime, <=))
		timeradd(&lasttime, &oneusec, tvp);

	lasttime = *tvp;
}
#endif /* SUN4 */

/*
 * XXX: these may actually belong somewhere else, but since the
 * EEPROM is so closely tied to the clock on some models, perhaps
 * it needs to stay here...
 */
int
eeprom_uio(uio)
	struct uio *uio;
{
#if defined(SUN4)
	int error;
	int off;	/* NOT off_t */
	u_int cnt, bcnt;
	caddr_t buf = NULL;

	if (!CPU_ISSUN4)
		return (ENODEV);

	off = uio->uio_offset;
	if (off > EEPROM_SIZE)
		return (EFAULT);

	cnt = uio->uio_resid;
	if (cnt > (EEPROM_SIZE - off))
		cnt = (EEPROM_SIZE - off);

	if ((error = eeprom_take()) != 0)
		return (error);

	if (eeprom_va == NULL) {
		error = ENXIO;
		goto out;
	}

	/*
	 * The EEPROM can only be accessed one byte at a time, yet
	 * uiomove() will attempt long-word access.  To circumvent
	 * this, we byte-by-byte copy the eeprom contents into a
	 * temporary buffer.
	 */
	buf = malloc(EEPROM_SIZE, M_DEVBUF, M_WAITOK);
	if (buf == NULL) {
		error = EAGAIN;
		goto out;
	}

	if (uio->uio_rw == UIO_READ)
		for (bcnt = 0; bcnt < EEPROM_SIZE; ++bcnt)
			*(char *)(buf + bcnt) = *(char *)(eeprom_va + bcnt);

	if ((error = uiomove(buf + off, (int)cnt, uio)) != 0)
		goto out;

	if (uio->uio_rw != UIO_READ)
		error = eeprom_update(buf, off, cnt);

 out:
	if (buf)
		free(buf, M_DEVBUF);
	eeprom_give();
	return (error);
#else /* ! SUN4 */
	return (ENODEV);
#endif /* SUN4 */
}

#if defined(SUN4)
/*
 * Update the EEPROM from the passed buf.
 */
int
eeprom_update(buf, off, cnt)
	char *buf;
	int off, cnt;
{
	int error = 0;
	volatile char *ep;
	char *bp;

	if (eeprom_va == NULL)
		return (ENXIO);

	ep = eeprom_va + off;
	bp = buf + off;

	if (eeprom_nvram)
		clk_wenable(1);

	while (cnt > 0) {
		/*
		 * DO NOT WRITE IT UNLESS WE HAVE TO because the
		 * EEPROM has a limited number of write cycles.
		 * After some number of writes it just fails!
		 */
		if (*ep != *bp) {
			*ep = *bp;
			/*
			 * We have written the EEPROM, so now we must
			 * sleep for at least 10 milliseconds while
			 * holding the lock to prevent all access to
			 * the EEPROM while it recovers.
			 */
			(void)tsleep(eeprom_va, PZERO - 1, "eeprom", hz/50);
		}
		/* Make sure the write worked. */
		if (*ep != *bp) {
			error = EIO;
			goto out;
		}
		++ep;
		++bp;
		--cnt;
	}
 out:
	if (eeprom_nvram)
		clk_wenable(0);

	return (error);
}

/* Take a lock on the eeprom. */
int
eeprom_take()
{
	int error = 0;

	while (eeprom_busy) {
		eeprom_wanted = 1;
		error = tsleep(&eeprom_busy, PZERO | PCATCH, "eeprom", 0);
		eeprom_wanted = 0;
		if (error)	/* interrupted */
			goto out;
	}
	eeprom_busy = 1;
 out:
	return (error);
}

/* Give a lock on the eeprom away. */
void
eeprom_give()
{

	eeprom_busy = 0;
	if (eeprom_wanted) {
		eeprom_wanted = 0;
		wakeup(&eeprom_busy);
	}
}
#endif /* SUN4 */
