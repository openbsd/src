/*	$OpenBSD: clock.c,v 1.21 2004/04/07 18:24:19 mickey Exp $	*/

/*
 * Copyright (c) 1998-2003 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>

#include <dev/clock_subr.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/intr.h>
#include <machine/reg.h>
#include <machine/cpufunc.h>
#include <machine/autoconf.h>

#if defined(DDB)
#include <uvm/uvm_extern.h>
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

void
cpu_initclocks()
{
	extern volatile u_long cpu_itmr;
	extern u_long cpu_hzticks;
	u_long __itmr;

	mfctl(CR_ITMR, __itmr);
	cpu_itmr = __itmr;
	__itmr += cpu_hzticks;
	mtctl(__itmr, CR_ITMR);
}

/*
 * initialize the system time from the time of day clock
 */
void
inittodr(t)
	time_t t;
{
	struct pdc_tod tod PDC_ALIGNMENT;
	int 	error, tbad = 0;

	if (t < 12*SECYR) {
		printf ("WARNING: preposterous time in file system");
		t = 6*SECYR + 186*SECDAY + SECDAY/2;
		tbad = 1;
	}

	if ((error = pdc_call((iodcio_t)pdc,
	    1, PDC_TOD, PDC_TOD_READ, &tod, 0, 0, 0, 0, 0)))
		printf("clock: failed to fetch (%d)\n", error);

	time.tv_sec = tod.sec;
	time.tv_usec = tod.usec;

	if (!tbad) {
		u_long	dt;

		dt = (time.tv_sec < t)?  t - time.tv_sec : time.tv_sec - t;

		if (dt < 2 * SECDAY)
			return;
		printf("WARNING: clock %s %ld days",
		    time.tv_sec < t? "lost" : "gained", dt / SECDAY);
	}

	printf (" -- CHECK AND RESET THE DATE!\n");
}

/*
 * reset the time of day clock to the value in time
 */
void
resettodr()
{
	int error;

	if ((error = pdc_call((iodcio_t)pdc, 1, PDC_TOD, PDC_TOD_WRITE,
	    time.tv_sec, time.tv_usec)))
		printf("clock: failed to save (%d)\n", error);
}

void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing we can do */
}
