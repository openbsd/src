/*	$NetBSD: gettimeofday.c,v 1.1 1995/12/20 12:56:06 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/types.h> 
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <machine/sysarch.h>

#include "libalpha.h"

int
gettimeofday(tvp, tzp)
	struct timeval *tvp;
	struct timezone *tzp;
{
	u_long opcc, npcc, pccdiff;
	static struct timeval lasttime;

	if (tzp == NULL && alpha_mmclockdata == NULL &&
	    !__alpha_mmclock_init_failed)
		alpha_mmclockdata = alpha_mmclock_init();
	if (tzp != NULL || alpha_mmclockdata == NULL)
		return (syscall(SYS_gettimeofday, tvp, tzp));

	if (tvp == NULL)
		return 0;

	do {
		tvp->tv_usec = alpha_mmclockdata->td_tv.tv_usec;
		tvp->tv_sec = alpha_mmclockdata->td_tv.tv_sec;
		opcc = alpha_mmclockdata->td_cc;
	} while (tvp->tv_usec != alpha_mmclockdata->td_tv.tv_usec ||
	    tvp->tv_sec != alpha_mmclockdata->td_tv.tv_sec);

	npcc = rpcc() & 0x00000000ffffffffUL;
	if (npcc < opcc)
		npcc += 0x0000000100000000UL;
	pccdiff = npcc - opcc;

	tvp->tv_usec += (pccdiff * 1000000) / alpha_cycles_per_second;
	if (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
        }

        if ((tvp->tv_sec == lasttime.tv_sec &&
             tvp->tv_usec <= lasttime.tv_usec) &&
            (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
                tvp->tv_sec++;
                tvp->tv_usec -= 1000000;
        }

        lasttime = *tvp;
	return 0;
}
