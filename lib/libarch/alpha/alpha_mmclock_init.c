/*	$NetBSD: alpha_mmclock_init.c,v 1.1 1995/12/20 12:55:21 cgd Exp $	*/

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
#include <machine/sysarch.h>

#include "libalpha.h"

volatile struct alpha_timedata *alpha_mmclockdata;
u_long alpha_cycles_per_second;

int __alpha_mmclock_init_failed;

volatile struct alpha_timedata *
alpha_mmclock_init()
{
	int fd;

	if (alpha_mmclockdata != NULL)
		return (alpha_mmclockdata);

	fd = open("/dev/mmclock", O_RDONLY, 0);
	if (fd == -1)
		goto fail;

	alpha_mmclockdata = (struct alpha_timedata *)
	    mmap(NULL, getpagesize(), PROT_READ, MAP_FILE, fd, 0);
	close(fd);
	if (alpha_mmclockdata == NULL)
		goto fail;

	alpha_cycles_per_second = alpha_mmclockdata->td_cpc;

	__alpha_mmclock_init_failed = 0;
	return (alpha_mmclockdata);

fail:
	__alpha_mmclock_init_failed = 1;
	return (NULL);
}
