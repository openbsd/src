/*	$OpenBSD: rf_utils.h,v 1.5 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_utils.h,v 1.4 1999/08/13 03:26:55 oster Exp $	*/

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

/****************************************
 *
 * rf_utils.c -- Header file for utils.c
 *
 ****************************************/


#ifndef	_RF__RF_UTILS_H_
#define	_RF__RF_UTILS_H_

#include "rf_types.h"
#include "rf_alloclist.h"
#include "rf_threadstuff.h"

char *rf_find_non_white(char *);
char *rf_find_white(char *);
RF_RowCol_t **rf_make_2d_array(int, int, RF_AllocListElem_t *);
RF_RowCol_t *rf_make_1d_array(int, RF_AllocListElem_t *);
void  rf_free_2d_array(RF_RowCol_t **, int, int);
void  rf_free_1d_array(RF_RowCol_t *, int);
int   rf_gcd(int, int);
int   rf_atoi(char *);
int   rf_htoi(char *);

#define	RF_USEC_PER_SEC			1000000
#define	RF_TIMEVAL_TO_US(_t_)						\
	(((_t_).tv_sec) * RF_USEC_PER_SEC + (_t_).tv_usec)

#define	RF_TIMEVAL_DIFF(_start_,_end_,_diff_)				\
do {									\
	if ((_end_)->tv_usec < (_start_)->tv_usec) {			\
		(_diff_)->tv_usec = ((_end_)->tv_usec +			\
		    RF_USEC_PER_SEC) - (_start_)->tv_usec;		\
		(_diff_)->tv_sec = ((_end_)->tv_sec-1) -		\
		    (_start_)->tv_sec;					\
	}								\
	else {								\
		(_diff_)->tv_usec = (_end_)->tv_usec -			\
		    (_start_)->tv_usec;					\
		(_diff_)->tv_sec  = (_end_)->tv_sec  -			\
		    (_start_)->tv_sec;					\
	}								\
} while (0)

#endif	/* !_RF__RF_UTILS_H_ */
