/*	$OpenBSD: rf_sys.c,v 1.2 1999/02/16 00:03:29 niklas Exp $	*/
/*	$NetBSD: rf_sys.c,v 1.3 1999/02/05 00:06:18 oster Exp $	*/
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

#include "rf_types.h"
#include "rf_sys.h"
#include <sys/param.h>
#include <sys/time.h>
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_threadstuff.h"

extern struct rpb *rpb;

/* timer stuff */
long    rf_timer_max_val;
long    rf_timer_ticks_per_second;
unsigned long rf_timer_ticks_per_usec;


int 
rf_ConfigureEtimer(listp)
	RF_ShutdownList_t **listp;
{
	/* XXX just picking some random values to keep things happy... without
	 * these set, stuff will panic on division by zero errors!! */
	rf_timer_ticks_per_second = 233100233;
	rf_timer_max_val = RF_DEF_TIMER_MAX_VAL;
	rf_timer_ticks_per_usec = rf_timer_ticks_per_second / 1000000;

	return (0);
}
