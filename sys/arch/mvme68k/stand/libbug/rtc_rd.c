/*	$OpenBSD: rtc_rd.c,v 1.2 1996/05/16 02:25:43 chuck Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"

void
mvmeprom_rtc_rd(ptime)
	struct mvmeprom_time *ptime;
{
	MVMEPROM_ARG1(ptime);
	MVMEPROM_CALL(MVMEPROM_RTC_RD);
}
