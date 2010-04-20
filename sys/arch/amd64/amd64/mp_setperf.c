/* $OpenBSD: mp_setperf.c,v 1.3 2010/04/20 22:05:41 tedu Exp $ */
/*
 * Copyright (c) 2007 Gordon Willem Klok <gwk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/mutex.h>

#include <machine/cpu.h>

#include <machine/intr.h>

struct mutex setperf_mp_mutex = MUTEX_INITIALIZER(IPL_HIGH);

/* underlying setperf mechanism e.g. k8_powernow_setperf() */
void (*ul_setperf)(int);

#define MP_SETPERF_STEADY 	0	/* steady state - normal operation */
#define MP_SETPERF_INTRANSIT 	1	/* in transition */
#define MP_SETPERF_PROCEED 	2	/* proceed with transition */
#define MP_SETPERF_FINISH 	3	/* return from IPI */


/* protected by setperf_mp_mutex */
volatile int mp_setperf_state = MP_SETPERF_STEADY;
volatile int mp_perflevel;

void mp_setperf(int);

void
mp_setperf(int level)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	int notready, s;

	if (mp_setperf_state == MP_SETPERF_STEADY) {
		mtx_enter(&setperf_mp_mutex);
		disable_intr();
		mp_perflevel = level;

		curcpu()->ci_setperf_state = CI_SETPERF_INTRANSIT;
		/* ask all other processors to drop what they are doing */
		CPU_INFO_FOREACH(cii, ci) {
			if (ci->ci_setperf_state != CI_SETPERF_INTRANSIT) {
				ci->ci_setperf_state =
				    CI_SETPERF_SHOULDSTOP;
				x86_send_ipi(ci, X86_IPI_SETPERF);
			}
		}


		/* Loop until all processors report ready */
		do {
			CPU_INFO_FOREACH(cii, ci) {
				if ((notready = (ci->ci_setperf_state
				    != CI_SETPERF_INTRANSIT)))
					break;
			}
		} while (notready);

		mp_setperf_state = MP_SETPERF_PROCEED; /* release the hounds */

		s = splipi();

		ul_setperf(mp_perflevel);

		splx(s);

		curcpu()->ci_setperf_state = CI_SETPERF_DONE;
		/* Loop until all processors report done */
		do {
			CPU_INFO_FOREACH(cii, ci) {
				if ((notready = (ci->ci_setperf_state
				    != CI_SETPERF_DONE)))
					break;
			}
		} while (notready);

		mp_setperf_state = MP_SETPERF_FINISH;
		/* delay a little for potential straglers */
		DELAY(2);
		curcpu()->ci_setperf_state = CI_SETPERF_READY;
		mp_setperf_state = MP_SETPERF_STEADY; /* restore normallity */
		enable_intr();
		mtx_leave(&setperf_mp_mutex);
	}

}

void
x86_setperf_ipi(struct cpu_info *ci)
{

	disable_intr();

	if (ci->ci_setperf_state == CI_SETPERF_SHOULDSTOP)
		ci->ci_setperf_state = CI_SETPERF_INTRANSIT;

	while (mp_setperf_state != MP_SETPERF_PROCEED)
		;

	ul_setperf(mp_perflevel);

	ci->ci_setperf_state = CI_SETPERF_DONE;

	while (mp_setperf_state != MP_SETPERF_FINISH)
		;
	ci->ci_setperf_state = CI_SETPERF_READY;

	enable_intr();
}

void
mp_setperf_init()
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;

	if (!cpu_setperf)
		return;
	ul_setperf = cpu_setperf;

	cpu_setperf = mp_setperf;

	CPU_INFO_FOREACH(cii, ci) {
		ci->ci_setperf_state = CI_SETPERF_READY;
	}
	mtx_init(&setperf_mp_mutex, IPL_HIGH);
}
