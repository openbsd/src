/*	$OpenBSD: rf_cpuutil.c,v 1.3 2000/01/07 14:50:20 peter Exp $	*/
/*	$NetBSD: rf_cpuutil.c,v 1.4 1999/08/13 03:41:53 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, Jim Zelenka
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
/*
 * rf_cpuutil.c
 *
 * track cpu utilization
 */

#include "rf_cpuutil.h"

#include "rf_types.h"
#include "rf_general.h"
#include "rf_shutdown.h"


int 
rf_ConfigureCpuMonitor(listp)
	RF_ShutdownList_t **listp;
{
#ifdef AIX
	int     rc;

	rc = knlist(namelist, 1, sizeof(struct nlist));
	if (rc) {
		RF_ERRORMSG("Could not knlist() to config CPU monitor\n");
		return (errno);
	}
	if (namelist[0].n_value == 0) {
		RF_ERRORMSG("Got bogus results from knlist() for CPU monitor\n");
		return (EIO);
	}
	sysinfo_offset = namelist[0].n_value;
	kmem_fd = open("/dev/kmem", O_RDONLY);
	if (kmem_fd < 0) {
		perror("/dev/kmem");
		return (errno);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownCpuMonitor, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownCpuMonitor(NULL);
		return (rc);
	}
#endif				/* AIX */
	return (0);
}

void 
rf_start_cpu_monitor()
{
#ifdef AIX
	off_t   off;
	int     rc;

	off = lseek(kmem_fd, sysinfo_offset, SEEK_SET);
	RF_ASSERT(off == sysinfo_offset);
	rc = read(kmem_fd, &sysinfo_start, sizeof(struct sysinfo));
	if (rc != sizeof(struct sysinfo)) {
		RF_ERRORMSG2("Starting CPU monitor: rc=%d != %d\n", rc,
		    sizeof(struct sysinfo));
	}
#endif				/* AIX */
}

void 
rf_stop_cpu_monitor()
{
#ifdef AIX
	off_t   off;
	int     rc;

	off = lseek(kmem_fd, sysinfo_offset, SEEK_SET);
	RF_ASSERT(off == sysinfo_offset);
	rc = read(kmem_fd, &sysinfo_stop, sizeof(struct sysinfo));
	if (rc != sizeof(struct sysinfo)) {
		RF_ERRORMSG2("Stopping CPU monitor: rc=%d != %d\n", rc,
		    sizeof(struct sysinfo));
	}
#endif				/* AIX */
}

void 
rf_print_cpu_util(s)
	char   *s;
{
#ifdef AIX
	long    idle;

	/* XXX compute a percentage here */
	idle = (long) (sysinfo_stop.cpu[CPU_IDLE] - sysinfo_start.cpu[CPU_IDLE]);
	printf("%ld idle ticks during %s.\n", idle, s);
#endif				/* AIX */
}
