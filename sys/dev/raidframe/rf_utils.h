/*	$OpenBSD: rf_utils.h,v 1.1 1999/01/11 14:29:55 niklas Exp $	*/
/*	$NetBSD: rf_utils.h,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
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

/***************************************
 *
 * rf_utils.c -- header file for utils.c
 *
 ***************************************/

/* :  
 * Log: rf_utils.h,v 
 * Revision 1.7  1996/06/07 21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.6  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.5  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.4  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.3  1995/12/06  15:17:53  root
 * added copyright info
 *
 */

#ifndef _RF__RF_UTILS_H_
#define _RF__RF_UTILS_H_

#include "rf_types.h"
#include "rf_alloclist.h"
#include "rf_threadstuff.h"

char *rf_find_non_white(char *p);
char *rf_find_white(char *p);
RF_RowCol_t **rf_make_2d_array(int b, int k, RF_AllocListElem_t *allocList);
RF_RowCol_t *rf_make_1d_array(int c, RF_AllocListElem_t *allocList);
void rf_free_2d_array(RF_RowCol_t **a, int b, int k);
void rf_free_1d_array(RF_RowCol_t *a, int n);
int rf_gcd(int m, int n);
int rf_atoi(char *p);
int rf_htoi(char *p);

#define RF_USEC_PER_SEC 1000000
#define RF_TIMEVAL_DIFF(_start_,_end_,_diff_) { \
	if ((_end_)->tv_usec < (_start_)->tv_usec) { \
		(_diff_)->tv_usec = ((_end_)->tv_usec + RF_USEC_PER_SEC) \
				- (_start_)->tv_usec; \
		(_diff_)->tv_sec = ((_end_)->tv_sec-1) - (_start_)->tv_sec; \
	} \
	else { \
		(_diff_)->tv_usec = (_end_)->tv_usec - (_start_)->tv_usec; \
		(_diff_)->tv_sec  = (_end_)->tv_sec  - (_start_)->tv_sec; \
	} \
}

#endif /* !_RF__RF_UTILS_H_ */
