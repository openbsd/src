/*	$OpenBSD: rf_etimer.h,v 1.7 2004/06/24 19:35:24 tholo Exp $	*/
/*	$NetBSD: rf_etimer.h,v 1.4 1999/08/13 03:26:55 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

#ifndef	_RF__RF_TIMER_H_
#define	_RF__RF_TIMER_H_


#include "rf_options.h"
#include "rf_utils.h"

#include <sys/time.h>

struct RF_Etimer_s {
	struct timeval st;
	struct timeval et;
	struct timeval diff;
};

#if	defined(_KERNEL)
#include <sys/kernel.h>

#define	RF_ETIMER_START(_t_) 						\
	do {								\
		bzero(&(_t_), sizeof (_t_));				\
		getmicrouptime(&(_t_).st);				\
	} while (0)

#define	RF_ETIMER_STOP(_t_) 						\
	do {								\
		getmicrouptime(&(_t_).et);				\
	} while (0)

#define	RF_ETIMER_EVAL(_t_)						\
	do {								\
		RF_TIMEVAL_DIFF(&(_t_).st, &(_t_).et, &(_t_).diff);	\
	} while (0)

#define	RF_ETIMER_VAL_US(_t_)		(RF_TIMEVAL_TO_US((_t_).diff))
#define	RF_ETIMER_VAL_MS(_t_)		(RF_TIMEVAL_TO_US((_t_).diff)/1000)

#endif	/* _KERNEL */

#endif	/* !_RF__RF_TIMER_H_ */
