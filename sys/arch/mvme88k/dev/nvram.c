#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>

#include <mvme88k/dev/nvramreg.h>
#include <mvme88k/dev/pcctworeg.h>

struct nvramsoftc {
	struct device		sc_dev;
	caddr_t			sc_paddr;
	caddr_t			sc_vaddr;
	int			sc_size;
	volatile struct clockreg *sc_clockreg;
};

int	nvrammatch	__P((struct device *, void *, void *));
void	nvramattach	__P((struct device *, struct device *, void *));

struct cfattach nvram_ca = { 
        sizeof(struct nvramsoftc), nvrammatch, nvramattach
}; 
 
struct cfdriver nvram_cd = {
        NULL, "nvram", DV_DULL, 0
};

/* ARGSUSED */
int
nvrammatch(struct device *parent, void *self, void *aux)
{
	struct confargs *ca = aux;
	struct cfdata *cf = self;
	caddr_t base;
	int ret;
	
#if 0
	if (cputyp != CPU_167 && cputyp != CPU_166
#ifdef CPU_187
		&& cputyp != CPU_187
#endif
		)
	{
		return 0;
	}
#endif /* 0 */
	
	if (cputyp != CPU_187) {
		return 0;
	}
	
	/* 
	 * If bus or name do not match, fail.
	 */
	if (ca->ca_bustype != BUS_PCCTWO ||
		strcmp(cf->cf_driver->cd_name, "nvram")) {
		return 0;
	}


	/* 3 locators base, size, ipl */
	base = (caddr_t)cf->cf_loc[0];

	if (badpaddr(base, 1) == -1) {
		return 0;
	}

	/*
	 * Tell our parent our requirements.
	 */
	ca->ca_paddr = (caddr_t)cf->cf_loc[0];
	ca->ca_size  = NVRAMSIZE;
	ca->ca_ipl   = 0;

	return 1;
}

/* ARGSUSED */
void
nvramattach(struct device *parent, struct device *self, void *aux)
{
	struct nvramsoftc	*sc = (struct nvramsoftc *)self;
	struct confargs		*ca = aux;

	sc->sc_clockreg = (struct clockreg *)(ca->ca_vaddr + NVRAM_TOD_OFF);

	printf(": MK48T08\n");
}

#if 0
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
		pmap_changeprot(pmap_kernel(), (vm_offset_t)clockreg, prot, 1);
}
#endif /* 0 */

/*
 * BCD to hex and hex to BCD.
 */
#define	FROMBCD(x)	(((x) >> 4) * 10 + ((x) & 0xf))
#define	TOBCD(x)	(((x) / 10 * 16) + ((x) % 10))

#define	SECDAY		(24 * 60 * 60)
#define	SECYR		(SECDAY * 365)
#define LEAPYEAR(y) 	(((y) % 4 == 0) && ((y) % 100) != 0 || ((y) % 400) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

chiptotime(int sec, int min, int hour, int day, int mon, int year)
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

timetochip(struct chiptime *c)
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
inittodr(time_t base)
{
	register struct nvramsoftc *sc;
	register volatile struct clockreg *cl;
	int sec, min, hour, day, mon, year;
	int badbase = 0, waszero = base == 0;
	
	sc = (struct nvramsoftc *)nvram_cd.cd_devs[0];
	cl = sc->sc_clockreg;

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
	
	cl->cl_csr |= CLK_READ;		/* enable read (stop time) */
	sec = cl->cl_sec;
	min = cl->cl_min;
	hour = cl->cl_hour;
	day = cl->cl_mday;
	mon = cl->cl_month;
	year = cl->cl_year;
	cl->cl_csr &= ~CLK_READ;	/* time wears on */

	time.tv_sec = chiptotime(sec, min, hour, day, mon, year);

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
	register struct nvramsoftc *sc;
	register volatile struct clockreg *cl;
	struct chiptime c;

	sc = (struct nvramsoftc *)nvram_cd.cd_devs[0];

	if (!time.tv_sec || (cl = sc->sc_clockreg) == NULL)
		return;
	timetochip(&c);

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
