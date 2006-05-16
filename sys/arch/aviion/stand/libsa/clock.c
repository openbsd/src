/*	$OpenBSD: clock.c,v 1.1 2006/05/16 22:48:18 miod Exp $	*/


#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"

#include "nvramreg.h"

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
const int dayyr[12] =
{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

u_long
chiptotime(sec, min, hour, day, mon, year)
	int sec, min, hour, day, mon, year;
{
	int days, yr;

	sec = FROMBCD(sec);
	min = FROMBCD(min);
	hour = FROMBCD(hour);
	day = FROMBCD(day);
	mon = FROMBCD(mon);
	year = FROMBCD(year) + YEAR0;

	/* simple sanity checks */
	if (year > 164 || mon < 1 || mon > 12 || day < 1 || day > 31)
		return (0);
	yr = 70;
	days = 0;

	if (year < 70) {
		for (; yr < year; yr++)
			days += LEAPYEAR(yr) ? 366 : 365;
		yr = 0;
	}

	for (; yr < year; yr++)
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
	int sec, min, hour, day, mon, year;
#define	TOD_BASE	(0xfff80000 + AV400_NVRAM_TOD_OFF)

	*(volatile u_int32_t *)(TOD_BASE + (CLK_CSR << 2)) = CLK_READ |
	    *(volatile u_int32_t *)(TOD_BASE + (CLK_CSR << 2));
	sec = *(volatile u_int32_t *)(TOD_BASE + (CLK_SEC << 2)) & 0xff;
	min = *(volatile u_int32_t *)(TOD_BASE + (CLK_MIN << 2)) & 0xff;
	hour = *(volatile u_int32_t *)(TOD_BASE + (CLK_HOUR << 2)) & 0xff;
	day = *(volatile u_int32_t *)(TOD_BASE + (CLK_DAY << 2)) & 0xff;
	mon = *(volatile u_int32_t *)(TOD_BASE + (CLK_MONTH << 2)) & 0xff;
	year = *(volatile u_int32_t *)(TOD_BASE + (CLK_YEAR << 2)) & 0xff;
	*(volatile u_int32_t *)(TOD_BASE + (CLK_CSR << 2)) = ~CLK_READ &
	    *(volatile u_int32_t *)(TOD_BASE + (CLK_CSR << 2));

	return (chiptotime(sec, min, hour, day, mon, year));
}
