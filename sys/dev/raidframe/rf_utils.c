/*	$OpenBSD: rf_utils.c,v 1.5 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_utils.c,v 1.5 2000/01/07 03:41:03 oster Exp $	*/

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

/******************************************
 *
 * rf_utils.c -- Various support routines.
 *
 ******************************************/


#include "rf_threadstuff.h"

#include <sys/time.h>

#include "rf_utils.h"
#include "rf_debugMem.h"
#include "rf_alloclist.h"
#include "rf_general.h"

/* Creates & zeros 2-d array with b rows and k columns. (MCH) */
RF_RowCol_t **
rf_make_2d_array(int b, int k, RF_AllocListElem_t *allocList)
{
	RF_RowCol_t **retval, i;

	RF_MallocAndAdd(retval, b * sizeof(RF_RowCol_t *), (RF_RowCol_t **),
	    allocList);
	for (i = 0; i < b; i++) {
		RF_MallocAndAdd(retval[i], k * sizeof(RF_RowCol_t),
		    (RF_RowCol_t *), allocList);
		bzero((char *) retval[i], k * sizeof(RF_RowCol_t));
	}
	return (retval);
}

void
rf_free_2d_array(RF_RowCol_t **a, int b, int k)
{
	RF_RowCol_t i;

	for (i = 0; i < b; i++)
		RF_Free(a[i], k * sizeof(RF_RowCol_t));
	RF_Free(a, b * sizeof(RF_RowCol_t));
}

/* Creates & zeroes a 1-d array with c columns. */
RF_RowCol_t *
rf_make_1d_array(int c, RF_AllocListElem_t *allocList)
{
	RF_RowCol_t *retval;

	RF_MallocAndAdd(retval, c * sizeof(RF_RowCol_t), (RF_RowCol_t *),
	    allocList);
	bzero((char *) retval, c * sizeof(RF_RowCol_t));
	return (retval);
}

void
rf_free_1d_array(RF_RowCol_t *a, int n)
{
	RF_Free(a, n * sizeof(RF_RowCol_t));
}

/*
 * Euclid's algorithm: Finds and returns the greatest common divisor
 * between a and b. (MCH)
 */
int
rf_gcd(int m, int n)
{
	int t;

	while (m > 0) {
		t = n % m;
		n = m;
		m = t;
	}
	return (n);
}

/*
 * These convert between text and integer. Apparently the regular C macros
 * for doing this are not available in the kernel.
 */

#define	ISDIGIT(x)	((x) >= '0' && (x) <= '9')
#define	ISHEXCHAR(x)	(((x) >= 'a' && (x) <= 'f') ||			\
			 ((x) >= 'A' && (x) <= 'F'))
#define	ISHEX(x)	(ISDIGIT(x) || ISHEXCHAR(x))
#define	HC2INT(x)	(((x) >= 'a' && (x) <= 'f') ?			\
			 (x) - 'a' + 10 :				\
			 (((x) >= 'A' && (x) <= 'F') ?			\
			   (x) - 'A' + 10 : (x - '0')))

int
rf_atoi(char *p)
{
	int val = 0, negate = 0;

	if (*p == '-') {
		negate = 1;
		p++;
	}
	for (; ISDIGIT(*p); p++)
		val = 10 * val + (*p - '0');
	return ((negate) ? -val : val);
}

int
rf_htoi(char *p)
{
	int val = 0;
	for (; ISHEXCHAR(*p); p++)
		val = 16 * val + HC2INT(*p);
	return (val);
}
