/*	$OpenBSD: rtc_rd.c,v 1.2 2001/07/04 08:09:29 niklas Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	asm volatile ("or r2,r0,%0": : "r" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
