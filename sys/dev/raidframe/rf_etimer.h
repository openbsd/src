/*	$OpenBSD: rf_etimer.h,v 1.2 1999/02/16 00:02:43 niklas Exp $	*/
/*	$NetBSD: rf_etimer.h,v 1.3 1999/02/05 00:06:11 oster Exp $	*/
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

/* rf_etimer.h -- header file for code related to accurate timing
 * This code currently assumes that the elapsed time between START_TIMER
 * and START_TIMER is less than the period of the cycle counter.  This
 * means the events you want to time must be less than:
 *        clock speed               max time
 *        ----------                --------
 *           175 MHz                 24 sec
 *           150 MHz                 28 sec
 *           125 MHz                 34 sec
 *
 *
 */


#ifndef _RF__RF_TIMER_H_
#define _RF__RF_TIMER_H_

#include "rf_options.h"

extern unsigned int rpcc(void);
#define rf_read_cycle_counter rpcc

#define RF_DEF_TIMER_MAX_VAL            0xFFFFFFFF

typedef struct RF_EtimerVal_s {
	unsigned ccnt;		/* cycle count */
}       RF_EtimerVal_t;

struct RF_Etimer_s {
	RF_EtimerVal_t st;
	RF_EtimerVal_t et;
	unsigned long ticks;	/* elapsed time in ticks */
};

extern long rf_timer_max_val;
extern long rf_timer_ticks_per_second;
extern unsigned long rf_timer_ticks_per_usec;

#define RF_ETIMER_TICKS2US(_tcks_)  ( (_tcks_) / rf_timer_ticks_per_usec )
#define RF_ETIMER_START(_t_)  { (_t_).st.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_STOP(_t_)   { (_t_).et.ccnt = rf_read_cycle_counter(); }
#define RF_ETIMER_EVAL(_t_) { \
	if ((_t_).st.ccnt < (_t_).et.ccnt) \
		(_t_).ticks = (_t_).et.ccnt - (_t_).st.ccnt; \
	else \
		(_t_).ticks = rf_timer_max_val - ((_t_).st.ccnt - (_t_).et.ccnt); \
}

#define RF_ETIMER_VAL_TICKS(_t_)  ((_t_).ticks)
#define RF_ETIMER_VAL_US(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks))
#define RF_ETIMER_VAL_MS(_t_)      (RF_ETIMER_TICKS2US((_t_).ticks)/1000)


#endif				/* !_RF__RF_TIMER_H_ */
