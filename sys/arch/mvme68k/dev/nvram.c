/*	$OpenBSD: nvram.c,v 1.21 2010/12/26 15:40:59 miod Exp $ */

/*
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
 */

/* 
 * 8/22/2000 BH Cleaned up year 2000 problems with calendar hardware.
 * This code will break again in 2068 or so - come dance on my grave.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>
#include <machine/psl.h>

#include <uvm/uvm_extern.h>

#include <mvme68k/dev/memdevs.h>
#include <mvme68k/dev/nvramreg.h>

#if defined(GPROF)
#include <sys/gmon.h>
#endif

struct nvramsoftc {
	struct device	sc_dev;
	paddr_t		sc_paddr;
	vaddr_t		sc_vaddr;
	int             sc_len;
	void		*sc_clockregs;
};

void    nvramattach(struct device *, struct device *, void *);
int     nvrammatch(struct device *, void *, void *);

struct cfattach nvram_ca = {
	sizeof(struct nvramsoftc), nvrammatch, nvramattach
};

struct cfdriver nvram_cd = {
	NULL, "nvram", DV_DULL
};

int
nvrammatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;   

/*X*/	if (ca->ca_vaddr == (vaddr_t)-1)
/*X*/		return (1);
	return (!badvaddr(ca->ca_vaddr, 1));
}

void
nvramattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct nvramsoftc *sc = (struct nvramsoftc *)self;
	vsize_t maplen;

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = (vaddr_t)ca->ca_vaddr;

	switch (cputyp) {
	default:
		sc->sc_len = MK48T08_SIZE;
		break;
#ifdef MVME141
	case CPU_141:
		sc->sc_len = MK48T02_SIZE;
		break;
#endif
#ifdef MVME147
	case CPU_147:
		sc->sc_len = MK48T02_SIZE;
		break;
#endif
	}

	/*
	 * On the MVME165, the MK48T08 is mapped as one byte per longword,
	 * thus spans four time as much address space.
	 */
#ifdef MVME165
	if (cputyp == CPU_165)
		maplen = 4 * sc->sc_len;
	else
#endif
		maplen = sc->sc_len;

/*X*/	if (sc->sc_vaddr == -1)
/*X*/		sc->sc_vaddr = mapiodev(sc->sc_paddr, maplen);
/*X*/	if (sc->sc_vaddr == 0)
/*X*/		panic("failed to map!");

#ifdef MVME165
	if (cputyp == CPU_165)
		sc->sc_clockregs = (void *)(sc->sc_vaddr + maplen -
		    sizeof(struct clockreg_165));
	else
#endif
		sc->sc_clockregs = (void *)(sc->sc_vaddr + sc->sc_len -
		    sizeof(struct clockreg));

	printf(": MK48T0%d len %d\n", sc->sc_len / 1024, sc->sc_len);
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt (clock.c:clkread).
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for fun,
 * we guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

/*
 * BCD to decimal and decimal to BCD.
 */
#define	FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define	TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

#define	SECYR		(SECDAY * 365)
#define	LEAPYEAR(y)	(((y) & 3) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

struct chiptime {
	int     sec;
	int     min;
	int     hour;
	int     wday;
	int     day;
	int     mon;
	int     year;
};

u_long chiptotime(int, int, int, int, int, int);
void timetochip(struct chiptime *);

u_long
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
	if (year>164 || mon<1 || mon>12 || day<1 || day>31)
		return (0);
	yr = 70;
	days = 0;

	if (year < 70) {		/* 2000 <= year */
		for (; yr < 100; yr++)	/* deal with first 30 years */
			days += LEAPYEAR(yr) ? 366 : 365;
		yr = 0;
	}

	for (; yr < year; yr++)		/* deal with years left */
		days += LEAPYEAR(yr) ? 366 : 365;

	days += dayyr[mon - 1] + day - 1;

	if (LEAPYEAR(yr) && mon > 2)
		days++;

	/* now have days since Jan 1, 1970; the rest is easy... */
	return (days * SECDAY + hour * 3600 + min * 60 + sec);
}

void
timetochip(c)
	struct chiptime *c;
{
	int t, t2, t3, now = time.tv_sec;

	/* January 1 1970 was a Thursday (4 in unix wdays) */
	/* compute the days since the epoch */
	t2 = now / SECDAY;

	t3 = (t2 + 4) % 7;	/* day of week */
	c->wday = TOBCD(t3 + 1);

	/* compute the year */
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
		if (t3 < (dayyr[t2] + ((t && (t2 > 1)) ? 1:0)))
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
	c->year = TOBCD((c->year - YEAR0) % 100);
}

/*
 * Set up the system's time, given a `reasonable' time value.
 */
void
inittodr(base)
	time_t base;
{
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[0];
	int sec, min, hour, day, mon, year;
	int badbase = 0, waszero = base == 0;

	if (base < 5 * SECYR) {
		/*
		 * If base is 0, assume filesystem time is just unknown
		 * instead of preposterous. Don't bark.
		 */
		if (base != 0)
			printf("WARNING: preposterous time in file system\n");
		/* not going to use it anyway, if the chip is readable */
		base = 21*SECYR + 186*SECDAY + SECDAY/2;
		badbase = 1;
	}

#ifdef MVME165
	if (cputyp == CPU_165) {
		struct clockreg_165 *cl = sc->sc_clockregs;

		cl->cl_csr |= CLK_READ;	 /* enable read (stop time) */
		sec = cl->cl_sec;
		min = cl->cl_min;
		hour = cl->cl_hour;
		day = cl->cl_mday;
		mon = cl->cl_month;
		year = cl->cl_year;
		cl->cl_csr &= ~CLK_READ;	/* time wears on */
	} else
#endif
	{
		struct clockreg *cl = sc->sc_clockregs;

		cl->cl_csr |= CLK_READ;	 /* enable read (stop time) */
		sec = cl->cl_sec;
		min = cl->cl_min;
		hour = cl->cl_hour;
		day = cl->cl_mday;
		mon = cl->cl_month;
		year = cl->cl_year;
		cl->cl_csr &= ~CLK_READ;	/* time wears on */
	}

	if ((time.tv_sec = chiptotime(sec, min, hour, day, mon, year)) == 0) {
		printf("WARNING: bad date in nvram");
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
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[0];
	struct chiptime c;

	if (!time.tv_sec || sc->sc_clockregs == NULL)
		return;
	timetochip(&c);

#ifdef MVME165
	if (cputyp == CPU_165) {
		struct clockreg_165 *cl = sc->sc_clockregs;

		cl->cl_csr |= CLK_WRITE;	/* enable write */
		cl->cl_sec = c.sec;
		cl->cl_min = c.min;
		cl->cl_hour = c.hour;
		cl->cl_wday = c.wday;
		cl->cl_mday = c.day;
		cl->cl_month = c.mon;
		cl->cl_year = c.year;
		cl->cl_csr &= ~CLK_WRITE;	/* load them up */
	} else
#endif
	{
		struct clockreg *cl = sc->sc_clockregs;

		cl->cl_csr |= CLK_WRITE;	/* enable write */
		cl->cl_sec = c.sec;
		cl->cl_min = c.min;
		cl->cl_hour = c.hour;
		cl->cl_wday = c.wday;
		cl->cl_mday = c.day;
		cl->cl_month = c.mon;
		cl->cl_year = c.year;
		cl->cl_csr &= ~CLK_WRITE;	/* load them up */
	}
}

/*ARGSUSED*/
int
nvramopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= nvram_cd.cd_ndevs ||
	    nvram_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
#ifdef MVME165
	/* for now, do not allow userland to access the nvram on 165. */
	if (cputyp == CPU_165)
		return (ENXIO);
#endif
	return (0);
}

/*ARGSUSED*/
int
nvramclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
nvramioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];
	int error = 0;

	switch (cmd) {
	case MIOCGSIZ:
		*(int *)data = sc->sc_len;
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*ARGSUSED*/
int
nvramrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];

	return (memdevrw(sc->sc_vaddr, sc->sc_len, uio, flags));
}

/*
 * If the NVRAM is of the 2K variety, an extra 2K of who-knows-what
 * will also be mmap'd, due to NBPG being 4K. On the MVME147 the NVRAM
 * repeats, so userland gets two copies back-to-back.
 */
paddr_t
nvrammmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];

	if (minor(dev) != 0)
		return (-1);

	/* allow access only in RAM */
	if (off < 0 || off >= round_page(sc->sc_len))
		return (-1);
	return (sc->sc_paddr + off);
}
