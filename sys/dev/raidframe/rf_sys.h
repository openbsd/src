/*	$OpenBSD: rf_sys.h,v 1.1 1999/01/11 14:29:53 niklas Exp $	*/
/*	$NetBSD: rf_sys.h,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
/*
 * rf_sys.h
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

#ifndef _RF__RF_SYS_H_
#define _RF__RF_SYS_H_

#include "rf_types.h"

int rf_ConfigureEtimer(RF_ShutdownList_t **listp);

#if defined(__osf__) && !defined(KERNEL)
int rf_get_cpu_ticks_per_sec(long *ticksp);
#endif /* __osf__ && !KERNEL */

#ifdef AIX
#include <nlist.h>
#include <sys/time.h>
#if RF_AIXVers == 3
int gettimeofday(struct timeval *tp, struct timezone *tzp);
#endif /* RF_AIXVers == 3 */
int knlist(struct nlist *namelist, int nel, int size);
int ffs(int index);
#endif /* AIX */

#ifdef sun
#define bcopy(a,b,n) memcpy(b,a,n)
#define bzero(b,n)   memset(b,0,n)
#define bcmp(a,b,n)  memcmp(a,b,n)
#endif /* sun */

#ifdef __GNUC__
/* we use gcc -Wall to check our anal-retentiveness level, occasionally */
#if defined(DEC_OSF) && !defined(KERNEL)
extern int ioctl(int fd, int req, ...);
#endif /* DEC_OSF && !KERNEL */
#endif /* __GNUC__ */

#endif /* !_RF__RF_SYS_H_ */
