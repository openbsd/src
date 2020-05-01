/*	$OpenBSD: clock.c,v 1.30 2020/05/01 19:56:11 kettenis Exp $	*/

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
#include <sys/timetc.h>

#include <dev/clock_subr.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/intr.h>
#include <machine/reg.h>
#include <machine/cpufunc.h>
#include <machine/autoconf.h>

u_long	cpu_hzticks;

int	cpu_hardclock(void *);
u_int	itmr_get_timecount(struct timecounter *);

struct timecounter itmr_timecounter = {
	itmr_get_timecount, NULL, 0xffffffff, 0, "itmr", 0, NULL
};

extern todr_chip_handle_t todr_handle;
struct todr_chip_handle pdc_todr;

int
pdc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct pdc_tod tod PDC_ALIGNMENT;
	int error;

	if ((error = pdc_call((iodcio_t)pdc, 1, PDC_TOD, PDC_TOD_READ,
	    &tod, 0, 0, 0, 0, 0))) {
		printf("clock: failed to fetch (%d)\n", error);
		return EIO;
	}

	tv->tv_sec = tod.sec;
	tv->tv_usec = tod.usec;
	return 0;
}

int
pdc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	int error;

	if ((error = pdc_call((iodcio_t)pdc, 1, PDC_TOD, PDC_TOD_WRITE,
	    tv->tv_sec, tv->tv_usec))) {
		printf("clock: failed to save (%d)\n", error);
		return EIO;
	}

	return 0;
}

void
cpu_initclocks(void)
{
	struct cpu_info *ci = curcpu();
	u_long __itmr;

	pdc_todr.todr_gettime = pdc_gettime;
	pdc_todr.todr_settime = pdc_settime;
	todr_handle = &pdc_todr;

	cpu_hzticks = (PAGE0->mem_10msec * 100) / hz;

	itmr_timecounter.tc_frequency = PAGE0->mem_10msec * 100;
	tc_init(&itmr_timecounter);

	mfctl(CR_ITMR, __itmr);
	ci->ci_itmr = __itmr;
	__itmr += cpu_hzticks;
	mtctl(__itmr, CR_ITMR);
}

int
cpu_hardclock(void *v)
{
	struct cpu_info *ci = curcpu();
	u_long __itmr, delta, eta;
	int wrap;
	register_t eiem;

	/*
	 * Invoke hardclock as many times as there has been cpu_hzticks
	 * ticks since the last interrupt.
	 */
	for (;;) {
		mfctl(CR_ITMR, __itmr);
		delta = __itmr - ci->ci_itmr;
		if (delta >= cpu_hzticks) {
			hardclock(v);
			ci->ci_itmr += cpu_hzticks;
		} else
			break;
	}

	/*
	 * Program the next clock interrupt, making sure it will
	 * indeed happen in the future. This is done with interrupts
	 * disabled to avoid a possible race.
	 */
	eta = ci->ci_itmr + cpu_hzticks;
	wrap = eta < ci->ci_itmr;	/* watch out for a wraparound */
	__asm volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm volatile("mtctl	%r0, %cr15");
	mtctl(eta, CR_ITMR);
	mfctl(CR_ITMR, __itmr);
	/*
	 * If we were close enough to the next tick interrupt
	 * value, by the time we have programmed itmr, it might
	 * have passed the value, which would cause a complete
	 * cycle until the next interrupt occurs. On slow
	 * models, this would be a disaster (a complete cycle
	 * taking over two minutes on a 715/33).
	 *
	 * We expect that it will only be necessary to postpone
	 * the interrupt once. Thus, there are two cases:
	 * - We are expecting a wraparound: eta < cpu_itmr.
	 *   itmr is in tracks if either >= cpu_itmr or < eta.
	 * - We are not wrapping: eta > cpu_itmr.
	 *   itmr is in tracks if >= cpu_itmr and < eta (we need
	 *   to keep the >= cpu_itmr test because itmr might wrap
	 *   before eta does).
	 */
	if ((wrap && !(eta > __itmr || __itmr >= ci->ci_itmr)) ||
	    (!wrap && !(eta > __itmr && __itmr >= ci->ci_itmr))) {
		eta += cpu_hzticks;
		mtctl(eta, CR_ITMR);
	}
	__asm volatile("mtctl	%0, %%cr15":: "r" (eiem));

	return (1);
}

void
setstatclockrate(int newhz)
{
	/* nothing we can do */
}

u_int
itmr_get_timecount(struct timecounter *tc)
{
	u_long __itmr;

	mfctl(CR_ITMR, __itmr);
	return (__itmr);
}
