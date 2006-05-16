/*	$OpenBSD: crt.c,v 1.1 2006/05/16 22:48:18 miod Exp $ */

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"

extern void netboot(const char *, int, int, int);

void
start(const char *args, int dev, int unit, int part)
{
	extern int edata, end;

	/* 
	 * This code enables the SFU1 and is used for single stage 
	 * bootstraps or the first stage of a two stage bootstrap.
	 * Do not use any low register to enable the SFU1. This wipes out
	 * the args.  Not cool at all... r25 seems free. 
	 */
	asm("|	enable SFU1");
	asm("	ldcr	r25,cr1" ::: "r25");
	asm("	clr	r25,r25,1<3>"); /* bit 3 is SFU1D */
	asm("	stcr	r25,cr1");

	memset(&edata, 0, ((int)&end - (int)&edata));

	netboot(args, dev, unit, part);
	_rtt();
	/* NOTREACHED */
}

void
__main()
{
}
