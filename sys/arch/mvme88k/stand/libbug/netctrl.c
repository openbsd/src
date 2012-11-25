/*	$OpenBSD: netctrl.c,v 1.1 2012/11/25 14:10:47 miod Exp $	*/
/* public domain */

#include <sys/types.h>
#include <machine/prom.h>

#include "prom.h"

int
mvmeprom_netctrl(struct mvmeprom_netctrl *ctrl)
{
	asm volatile ("or r2,r0,%0": : "r" (ctrl));
	MVMEPROM_CALL(MVMEPROM_NETCTRL);
	return ctrl->status;
}
