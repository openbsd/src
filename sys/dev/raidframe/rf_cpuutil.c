/*	$OpenBSD: rf_cpuutil.c,v 1.1 1999/01/11 14:29:03 niklas Exp $	*/
/*	$NetBSD: rf_cpuutil.c,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
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

#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_cpuutil.h"

#ifndef KERNEL
#include <errno.h>
#endif /* !KERNEL */
#include "rf_types.h"
#include "rf_general.h"
#include "rf_shutdown.h"
#include "rf_sys.h"
#ifdef __osf__
#include <sys/table.h>
#endif /* __osf__ */
#ifdef AIX
#include <nlist.h>
#include <sys/sysinfo.h>
#endif /* AIX */
#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include <sys/dk.h>
#endif /* __NetBSD__ && !__OpenBSD__ */
#else /* KERNEL */
extern int table(int id, int index, void *addr, int nel, u_int lel);
#endif /* KERNEL */

#ifdef __osf__
static struct tbl_sysinfo start, stop;
#endif /* __osf__ */

#ifdef AIX
static int kmem_fd;
static off_t sysinfo_offset;
static struct sysinfo sysinfo_start, sysinfo_stop;
static struct nlist namelist[] = {
  {{"sysinfo"}},
  {{""}},
};
#endif /* AIX */

#ifdef AIX
static void rf_ShutdownCpuMonitor(ignored)
  void  *ignored;
{
  close(kmem_fd);
}
#endif /* AIX */

int rf_ConfigureCpuMonitor(listp)
  RF_ShutdownList_t  **listp;
{
#ifdef AIX
  int rc;

  rc = knlist(namelist, 1, sizeof(struct nlist));
  if (rc) {
    RF_ERRORMSG("Could not knlist() to config CPU monitor\n");
    return(errno);
  }
  if (namelist[0].n_value == 0) {
    RF_ERRORMSG("Got bogus results from knlist() for CPU monitor\n");
    return(EIO);
  }
  sysinfo_offset = namelist[0].n_value;
  kmem_fd = open("/dev/kmem", O_RDONLY);
  if (kmem_fd < 0) {
    perror("/dev/kmem");
    return(errno);
  }
  rc = rf_ShutdownCreate(listp, rf_ShutdownCpuMonitor, NULL);
  if (rc) {
    RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
			__LINE__, rc);
    rf_ShutdownCpuMonitor(NULL);
    return(rc);
  }
#endif /* AIX */
  return(0);
}

void rf_start_cpu_monitor()
{
#ifdef __osf__
#ifndef KERNEL
  if (table(TBL_SYSINFO, 0, &start, 1, sizeof(start)) != 1) {
    printf("Unable to get sysinfo for cpu utilization monitor\n");
    perror("start_cpu_monitor");
  }
#else /* !KERNEL */
  /* start.si_user = cp_time[CP_USER];
  start.si_nice = cp_time[CP_NICE];
  start.si_sys  = cp_time[CP_SYS];
  start.si_idle = cp_time[CP_IDLE];
  start.wait    = cp_time[CP_WAIT]; */
#endif /* !KERNEL */
#endif /* __osf__ */
#ifdef AIX
  off_t off;
  int rc;

  off = lseek(kmem_fd, sysinfo_offset, SEEK_SET);
  RF_ASSERT(off == sysinfo_offset);
  rc = read(kmem_fd, &sysinfo_start, sizeof(struct sysinfo));
  if (rc != sizeof(struct sysinfo)) {
    RF_ERRORMSG2("Starting CPU monitor: rc=%d != %d\n", rc,
      sizeof(struct sysinfo));
  }
#endif /* AIX */
}

void rf_stop_cpu_monitor()
{
#ifdef __osf__
#ifndef KERNEL
  if (table(TBL_SYSINFO, 0, &stop, 1, sizeof(stop)) != 1) {
    printf("Unable to get sysinfo for cpu utilization monitor\n");
    perror("stop_cpu_monitor");
  }
#else /* !KERNEL */
  /* stop.si_user = cp_time[CP_USER];
  stop.si_nice = cp_time[CP_NICE];
  stop.si_sys  = cp_time[CP_SYS];
  stop.si_idle = cp_time[CP_IDLE];
  stop.wait    = cp_time[CP_WAIT]; */
#endif /* !KERNEL */
#endif /* __osf__ */
#ifdef AIX
  off_t off;
  int rc;

  off = lseek(kmem_fd, sysinfo_offset, SEEK_SET);
  RF_ASSERT(off == sysinfo_offset);
  rc = read(kmem_fd, &sysinfo_stop, sizeof(struct sysinfo));
  if (rc != sizeof(struct sysinfo)) {
    RF_ERRORMSG2("Stopping CPU monitor: rc=%d != %d\n", rc,
      sizeof(struct sysinfo));
  }
#endif /* AIX */
}

void rf_print_cpu_util(s)
  char  *s;
{
#ifdef __osf__
  long totalticks, idleticks;

  idleticks = stop.si_idle - start.si_idle + stop.wait - start.wait;
  totalticks = stop.si_user - start.si_user + stop.si_nice - start.si_nice +
	       stop.si_sys - start.si_sys + idleticks;
  printf("CPU utilization during %s was %d %%\n", s, 100 - 100*idleticks/totalticks);
#endif /* __osf__ */
#ifdef AIX
  long idle;

  /* XXX compute a percentage here */
  idle = (long)(sysinfo_stop.cpu[CPU_IDLE] - sysinfo_start.cpu[CPU_IDLE]);
  printf("%ld idle ticks during %s.\n", idle, s);
#endif /* AIX */
}
