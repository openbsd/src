/*	$OpenBSD: rtc_rd.c,v 1.2 1996/04/28 10:48:47 deraadt Exp $ */

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
	asm volatile ("movel %0,sp@-" :  :"a" (ptime));
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
