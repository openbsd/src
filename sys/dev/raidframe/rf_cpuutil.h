/*	$OpenBSD: rf_cpuutil.h,v 1.1 1999/01/11 14:29:03 niklas Exp $	*/
/*	$NetBSD: rf_cpuutil.h,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
/*
 * rf_cpuutil.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Jim Zelenka
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
 * :  
 * Log: rf_cpuutil.h,v 
 * Revision 1.3  1996/07/18 22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.2  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.1  1996/05/18  19:55:29  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_CPUUTIL_H_
#define _RF__RF_CPUUTIL_H_

#include "rf_types.h"

int  rf_ConfigureCpuMonitor(RF_ShutdownList_t **listp);
void rf_start_cpu_monitor(void);
void rf_stop_cpu_monitor(void);
void rf_print_cpu_util(char *s);

#endif /* !_RF__RF_CPUUTIL_H_ */
