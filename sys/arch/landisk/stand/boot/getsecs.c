/*	$NetBSD: getsecs.c,v 1.2 2006/09/11 13:48:57 nonaka Exp $	*/

#include <sys/param.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>

#include <sh/devreg.h>
#include <arch/sh/dev/scireg.h>

#include <arch/landisk/dev/rs5c313reg.h>

/**
 * RICOH RS5C313
 *
 * Web page: http://www.ricoh.co.jp/LSI/product_rtc/3wire/5c313/
 *
 * How to control RS5C313 on LANDISK
 *   see http://www.mizore.jp/wiki/index.php?LANDISK/rtc
 */

uint8_t rtc_read(uint32_t addr);
void rtc_write(uint32_t addr, uint8_t data);

static void
rtc_init(void)
{

	SHREG_SCSPTR = SCSPTR_SPB1IO | SCSPTR_SPB1DT
		       | SCSPTR_SPB0IO | SCSPTR_SPB0DT;
	delay(1);
}

/* control RTC chip enable */
static void
rtc_ce(int onoff)
{

	if (onoff) {
		_reg_write_1(0xb0000003, (1 << 1));
	} else {
		_reg_write_1(0xb0000003, (0 << 1));
	}
	delay(1);
}

static inline void
rtc_clk(int onoff)
{

	if (onoff) {
		SHREG_SCSPTR |= SCSPTR_SPB0DT;
	} else {
		SHREG_SCSPTR &= ~SCSPTR_SPB0DT;
	}
	delay(1);
}

static void
rtc_dir(int output)
{

	if (output) {
		SHREG_SCSPTR |= SCSPTR_SPB1IO;
	} else {
		SHREG_SCSPTR &= ~SCSPTR_SPB1IO;
	}
	delay(1);
}

/* data-out */
static void
rtc_do(int onoff)
{

	if (onoff) {
		SHREG_SCSPTR |= SCSPTR_SPB1DT;
	} else {
		SHREG_SCSPTR &= ~SCSPTR_SPB1DT;
	}
	delay(1);

	rtc_clk(0);
	rtc_clk(1);
}

/* data-in */
static int
rtc_di(void)
{
	int d;

	d = (SHREG_SCSPTR & SCSPTR_SPB1DT) ? 1 : 0;

	rtc_clk(0);
	rtc_clk(1);

	return d;
}

uint8_t
rtc_read(uint32_t addr)
{
	uint8_t data;

	rtc_init();
	rtc_ce(1);

	rtc_dir(1);
	rtc_do(1);		/* Don't care */
	rtc_do(1);		/* R/#W = 1(READ) */
	rtc_do(1);		/* AD = 1 */
	rtc_do(0);		/* DT = 0 */
	rtc_do(addr & 0x8);	/* A3 */
	rtc_do(addr & 0x4);	/* A2 */
	rtc_do(addr & 0x2);	/* A1 */
	rtc_do(addr & 0x1);	/* A0 */

	rtc_dir(0);
	(void)rtc_di();
	(void)rtc_di();
	(void)rtc_di();
	(void)rtc_di();
	data = rtc_di();	/* D3 */
	data <<= 1;
	data |= rtc_di();	/* D2 */
	data <<= 1;
	data |= rtc_di();	/* D1 */
	data <<= 1;
	data |= rtc_di();	/* D0 */

	rtc_ce(0);

	return data & 0xf;
}

void
rtc_write(uint32_t addr, uint8_t data)
{

	rtc_init();
	rtc_ce(1);

	rtc_dir(1);
	rtc_do(1);		/* Don't care */
	rtc_do(0);		/* R/#W = 0(WRITE) */
	rtc_do(1);		/* AD = 1 */
	rtc_do(0);		/* DT = 0 */
	rtc_do(addr & 0x8);	/* A3 */
	rtc_do(addr & 0x4);	/* A2 */
	rtc_do(addr & 0x2);	/* A1 */
	rtc_do(addr & 0x1);	/* A0 */

	rtc_do(1);		/* Don't care */
	rtc_do(0);		/* R/#W = 0(WRITE) */
	rtc_do(0);		/* AD = 0 */
	rtc_do(1);		/* DT = 1 */
	rtc_do(data & 0x8);	/* D3 */
	rtc_do(data & 0x4);	/* D2 */
	rtc_do(data & 0x2);	/* D1 */
	rtc_do(data & 0x1);	/* D0 */

	rtc_ce(0);
}

time_t
getsecs(void)
{
	uint32_t sec, min, hour, day;
#if 0
	uint32_t mon, year;
#endif
	time_t secs;

	sec = rtc_read(RS5C313_SEC1);
	sec += rtc_read(RS5C313_SEC10) * 10;
	min = rtc_read(RS5C313_MIN1);
	min += rtc_read(RS5C313_MIN10) * 10;
	hour = rtc_read(RS5C313_HOUR1);
	hour += rtc_read(RS5C313_HOUR10) * 10;
	day = rtc_read(RS5C313_DAY1);
	day += rtc_read(RS5C313_DAY10) * 10;
#if 0
	mon = rtc_read(RS5C313_MON1);
	mon += rtc_read(RS5C313_MON10) * 10;
	year = rtc_read(RS5C313_YEAR1);
	year += rtc_read(RS5C313_YEAR10) * 10;
#endif

	secs = sec;
	secs += min * 60;
	secs += hour * 60 * 60;
	secs += day * 60 * 60 * 24;
#if 0
	/* XXX mon, year */
#endif

#if defined(DEBUG)
	printf("getsecs: secs = %d\n", (uint32_t)secs);
#endif

	return secs;
}
