/*	$OpenBSD: rtc_rd.c,v 1.3 2003/09/07 21:35:35 miod Exp $	*/

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
	asm volatile ("or r2,r0,%0": : "r" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
