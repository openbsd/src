/*	$OpenBSD: rtc_rd.c,v 1.1 1996/05/07 11:25:12 deraadt Exp $ */

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
	MVMEPROM_ARG1(ptime);
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
