/*	$OpenBSD: rf_sys.c,v 1.1 1999/01/11 14:29:53 niklas Exp $	*/
/*	$NetBSD: rf_sys.c,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
/*
 * rf_sys.c
 *
 * Jim Zelenka, CMU/SCS, 14 June 1996
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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

#ifdef _KERNEL
#define KERNEL
#endif

#include "rf_types.h"
#include "rf_sys.h"
#ifndef KERNEL
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdio.h>
#include <unistd.h>
#endif /* !KERNEL */
#include <sys/param.h>
#if !defined(sun) && !defined(__NetBSD__) && !defined(__OpenBSD__) && !defined(LINUX) && (!defined(MACH) || defined(__osf__))
#include <sys/sysinfo.h>
#endif /* !sun && !__NetBSD__ && !__OpenBSD__ && !LINUX && (!MACH || __osf__) */
#include <sys/time.h>
#ifdef __osf__
#include <machine/rpb.h>
#include <machine/hal/hal_sysinfo.h>
#endif /* __osf__ */
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_threadstuff.h"

#ifdef KERNEL
extern struct rpb *rpb;
#endif /* KERNEL */

/* timer stuff */
#ifdef __alpha
long rf_timer_max_val;
long rf_timer_ticks_per_second;
unsigned long rf_timer_ticks_per_usec;
#endif /* __alpha */


#if defined(__NetBSD__) || defined(__OpenBSD__)
long rf_timer_max_val;
long rf_timer_ticks_per_second;
unsigned long rf_timer_ticks_per_usec;
#endif /* __NetBSD__ || __OpenBSD__ */

#if !defined(KERNEL) && !defined(SIMULATE) && (RF_UTILITY == 0)
pthread_attr_t raidframe_attr_default;

int rf_thread_create(
  RF_Thread_t      *thread,
  pthread_attr_t    attr,
  void            (*func)(),
  RF_ThreadArg_t    arg)
{
  int rc;

#ifdef __osf__
  rc = pthread_create(thread, attr, (pthread_startroutine_t)func, arg); 
#endif /* __osf__ */
#ifdef AIX
  rc = pthread_create(thread, &attr, (void *(*)(void *))func, arg); 
#endif /* AIX */
 if (rc)
    return(errno);
  rc = pthread_detach(thread);
  if (rc) {
    /* don't return error, because the thread exists, and must be cleaned up */
    RF_ERRORMSG1("RAIDFRAME WARNING: failed detaching thread %lx\n", thread);
  }
  return(0);
}
#endif /* !KERNEL && !SIMULATE && (RF_UTILITY == 0) */

#if defined(__osf__) && !defined(KERNEL)
int rf_get_cpu_ticks_per_sec(long *ticksp)
{
  char *kmemdevname, buf[sizeof(struct rpb)+8];
  char *memdevname, kernel_name[MAXPATHLEN+1];
  struct nlist nl[2], *np;
  unsigned long rpb_addr;
  int kfd, rc, fd, bad;
  struct rpb rpb;
  off_t off;

  kmemdevname = "/dev/kmem";
  memdevname = "/dev/mem";

  np = &nl[0];
  bzero((char *)np, sizeof(nl));
  nl[0].n_name = "pmap_physhwrpb";
  nl[1].n_name = NULL;

  bad = 0;

  /* get running kernel name */
  bzero(kernel_name, MAXPATHLEN+1);
  kernel_name[0] = '/';
  rc = getsysinfo(GSI_BOOTEDFILE, &kernel_name[1], MAXPATHLEN, 0, 0);
  if (rc != 1) {
    RF_ERRORMSG("RAIDFRAME: cannot get booted kernel name\n");
    if (errno)
      return(errno);
    else
      return(EIO);
  }

  rc = nlist(kernel_name, np);
  if (rc) {
    RF_ERRORMSG1("RAIDFRAME: cannot nlist %s\n", kernel_name);
    return(EIO);
  }

  if (np->n_type == 0) {
    RF_ERRORMSG1("RAIDFRAME: cannot usefully nlist %s\n", kernel_name);
    return(EIO);
  }

  kfd = open(kmemdevname, O_RDONLY);
  if (kfd < 0) {
    perror(kmemdevname);
    return(errno);
  }
  fd = open(memdevname, O_RDONLY);
  if (fd < 0) {
    perror(kmemdevname);
    return(errno);
  }

  /*
   * pmap_physhwrpb is a variable in the kernel containing the physical
   * address of the hardware RPB. We'll just find that variable and
   * read it, then use that as a physical memory address to read the
   * rpb itself.
   */

  off = lseek(kfd, np->n_value, SEEK_SET);
  if (off != np->n_value) {
    RF_ERRORMSG("RAIDFRAME: cannot seek to address of hwrpb addr\n");
    return(EIO);
  }

  rc = read(kfd, &rpb_addr, sizeof(rpb_addr));
  if (rc != sizeof(rpb_addr)) {
    RF_ERRORMSG("RAIDFRAME: cannot read address of hwrpb addr\n");
    if (rc < 0)
      bad = errno;
    bad = EIO;
    goto isbad;
  }

  off = lseek(fd, rpb_addr, SEEK_SET);
  if (off != rpb_addr) {
    RF_ERRORMSG("RAIDFRAME: cannot seek to rpb addr\n");
    bad = EIO;
    goto isbad;
  }

  rc = read(fd, &rpb, sizeof(rpb));
  if (rc != sizeof(rpb)) {
    RF_ERRORMSG1("RAIDFRAME: cannot read rpb (rc=%d)\n", rc);
    if (rc < 0)
      bad = errno;
    bad = EIO;
    goto isbad;
  }

  /*
   * One extra sanity check: the RPB is self-identifying.
   * This field is guaranteed to have the value
   * 0x0000004250525748, always.
   */
  if (rpb.rpb_string != 0x0000004250525748) {
    bad = EIO;
    goto isbad;
  }

isbad:
  if (bad) {
    RF_ERRORMSG("ERROR: rpb failed validation\n");
    RF_ERRORMSG1("RAIDFRAME: perhaps %s has changed since booting?\n",
      kernel_name);
    return(bad);
  }

  *ticksp = rpb.rpb_counter;

  close(kfd);
  close(fd);

  return(0);
}
#endif /* __osf__ && !KERNEL */

int rf_ConfigureEtimer(listp)
  RF_ShutdownList_t  **listp;
{
#ifdef __osf__
  int rc;

#ifdef KERNEL
  rf_timer_ticks_per_second = rpb->rpb_counter;
#else /* KERNEL */
  rc = rf_get_cpu_ticks_per_sec(&rf_timer_ticks_per_second);
  if (rc)
    return(rc);
#endif /* KERNEL */
  rf_timer_max_val = RF_DEF_TIMER_MAX_VAL;
  rf_timer_ticks_per_usec = rf_timer_ticks_per_second/1000000;
#endif /* __osf__ */
#if defined(NETBSD_ALPHA) || defined(OPENBSD_ALPHA)
  /*
   * XXX cgd fix this
   */
  rf_timer_ticks_per_second = 233100233;
  rf_timer_max_val = RF_DEF_TIMER_MAX_VAL;
  rf_timer_ticks_per_usec = rf_timer_ticks_per_second/1000000;
#endif /* NETBSD_ALPHA || OPENBSD_ALPHA */
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  /* XXX just picking some random values to keep things happy... without these
     set, stuff will panic on division by zero errors!! */
  rf_timer_ticks_per_second = 233100233;
  rf_timer_max_val = RF_DEF_TIMER_MAX_VAL;
  rf_timer_ticks_per_usec = rf_timer_ticks_per_second/1000000;

#endif
  return(0);
}
