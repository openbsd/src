/*	$OpenBSD: clock.c,v 1.1 1998/12/29 21:35:42 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

struct timeval time;

void startrtclock __P((void));

/*
 * Return the best possible estimate of the current time.
 */
void
microtime(tvp)
	struct timeval *tvp;
{

}

void
cpu_initclocks()
{
	u_int time_inval;
#ifdef USELEDS
	static u_int hbcnt = 0;

	if (!(hbcnt % 50)) {
		register u_int r = (hbcnt / 50) % 6;

		heartbeat(r < 4 && !(r % 2));
	}
#endif
	/* Start the interval timer. */
	mfctl(CR_ITMR, time_inval);
	mtctl(time_inval + cpu_hzticks, CR_ITMR);
}


/*
 * initialize the system time from the time of day clock
 */
void
inittodr(t)
	time_t t;
{
	static struct pdc_tod tod;

	pdc_call((iodcio_t)PAGE0->mem_pdc, 1, PDC_TOD, PDC_TOD_READ,
		&tod, 0, 0, 0, 0, 0);

	time = *(struct timeval *)&tod;

	if ((long)time.tv_sec < 0) {
		time.tv_sec = SECYR * (1990 - 1970);
		printf("WARNING: clock not initialized -- check and reset\n");
	}
}

/*
 * reset the time of day clock to the value in time
 */
void
resettodr()
{
	static struct pdc_tod tod;

	tod.sec = time.tv_sec;
	tod.usec = time.tv_usec;

	pdc_call((iodcio_t)PAGE0->mem_pdc, 1, PDC_TOD, PDC_TOD_WRITE, &tod);
}

void
setstatclockrate(newhz)
	int newhz;
{
	/* nothing we can do */
}

