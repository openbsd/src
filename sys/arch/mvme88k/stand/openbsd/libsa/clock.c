#include <sys/types.h>

#include "clockreg.h"
#include "config.h"
#include "clock.h"

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)      (((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)        (((x) / 10 * 16) + ((x) % 10))

#define SECDAY          (24 * 60 * 60)
#define SECYR           (SECDAY * 365)
#define LEAPYEAR(y)     (((y) & 3) == 0)

/*
 * This code is defunct after 2068.
 * Will Unix still be here then??
 */
const short dayyr[12] =
{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static  u_long
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
	if (year < 70)
		year = 70;

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

time_t
getsecs()
{
	extern int cputyp;
	register struct clockreg *cl;
	int     sec, min, hour, day, mon, year;

	if (cputyp == CPU_147)
		cl = (struct clockreg *) CLOCK_ADDR_147;
	else
		cl = (struct clockreg *) CLOCK_ADDR_16x;

	cl->cl_csr |= CLK_READ; /* enable read (stop time) */
	sec = cl->cl_sec;
	min = cl->cl_min;
	hour = cl->cl_hour;
	day = cl->cl_mday;
	mon = cl->cl_month;
	year = cl->cl_year;
	cl->cl_csr &= ~CLK_READ;/* time wears on */
	return (chiptotime(sec, min, hour, day, mon, year));
}

int
getticks()
{
	return getsecs() * 100;
}
