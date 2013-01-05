/*	$OpenBSD: rtc_rd.c,v 1.4 2013/01/05 11:20:56 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>
#include "prom.h"

void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	asm volatile ("or %%r2,%%r0,%0": : "r" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
