/*	$NetBSD: alpha_mmclock_gettime.c,v 1.1 1995/12/20 12:54:24 cgd Exp $	*/

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

void
alpha_mmclock_gettime(tdp)
	struct alpha_timedata *tdp;
{
	u_long npcc, pccdiff;

	do {
		tdp->td_tv.tv_usec = alpha_mmclockdata->td_tv.tv_usec;
		tdp->td_tv.tv_sec = alpha_mmclockdata->td_tv.tv_sec;
		tdp->td_cc = alpha_mmclockdata->td_cc;
	} while (tdp->td_tv.tv_usec != alpha_mmclockdata->td_tv.tv_usec ||
	    tdp->td_tv.tv_sec != alpha_mmclockdata->td_tv.tv_sec);

	npcc = rpcc() & 0x00000000ffffffffUL;
	if (npcc < tdp->td_cc)
		npcc += 0x0000000100000000UL;
	tdp->td_cc = npcc - tdp->td_cc;
}
