/*	$OpenBSD: rf_utils.c,v 1.1 1999/01/11 14:29:54 niklas Exp $	*/
/*	$NetBSD: rf_utils.c,v 1.1 1998/11/13 04:20:35 oster Exp $	*/
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
 * rf_utils.c -- various support routines
 *
 ****************************************/

/* :  
 * Log: rf_utils.c,v 
 * Revision 1.20  1996/07/27 23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.19  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.18  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.17  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.16  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.15  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.14  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.13  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.12  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.11  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.10  1995/12/06  15:17:44  root
 * added copyright info
 *
 */

#include "rf_threadstuff.h"

#ifdef _KERNEL
#define KERNEL
#endif

#ifndef KERNEL
#include <stdio.h>
#endif /* !KERNEL */
#include <sys/time.h>

#include "rf_threadid.h"
#include "rf_utils.h"
#include "rf_debugMem.h"
#include "rf_alloclist.h"
#include "rf_general.h"
#include "rf_sys.h"

#ifndef KERNEL
#include "rf_randmacros.h"
#endif /* !KERNEL */

/* creates & zeros 2-d array with b rows and k columns (MCH) */
RF_RowCol_t **rf_make_2d_array(b, k, allocList)
  int                  b;
  int                  k;
  RF_AllocListElem_t  *allocList;
{
    RF_RowCol_t **retval, i;

    RF_MallocAndAdd(retval, b * sizeof(RF_RowCol_t *), (RF_RowCol_t **), allocList);
    for (i=0; i<b; i++) {
      RF_MallocAndAdd(retval[i], k * sizeof(RF_RowCol_t), (RF_RowCol_t *), allocList);
      (void) bzero((char *) retval[i], k*sizeof(RF_RowCol_t));
    }
    return(retval);
}

void rf_free_2d_array(a, b, k)
  RF_RowCol_t  **a;
  int            b;
  int            k;
{
  RF_RowCol_t i;

  for (i=0; i<b; i++)
    RF_Free(a[i], k*sizeof(RF_RowCol_t));
  RF_Free(a, b*sizeof(RF_RowCol_t));
}


/* creates & zeros a 1-d array with c columns */
RF_RowCol_t *rf_make_1d_array(c, allocList)
  int                  c;
  RF_AllocListElem_t  *allocList;
{
  RF_RowCol_t *retval;

  RF_MallocAndAdd(retval, c * sizeof(RF_RowCol_t), (RF_RowCol_t *), allocList);
  (void) bzero((char *) retval, c*sizeof(RF_RowCol_t));
  return(retval);
}

void rf_free_1d_array(a, n)
  RF_RowCol_t  *a;
  int           n;
{
  RF_Free(a, n * sizeof(RF_RowCol_t));
}

/* Euclid's algorithm:  finds and returns the greatest common divisor
 * between a and b.     (MCH)
 */
int rf_gcd(m, n)
  int  m;
  int  n;
{
    int t;

    while (m>0) {
        t = n % m;
        n = m;
        m = t;
    }
    return(n);
}

#if !defined(KERNEL) && !defined(SIMULATE) && defined(__osf__)
/* this is used to generate a random number when _FASTRANDOM is off
 * in randmacros.h
 */
long rf_do_random(rval, rdata)
  long                *rval;
  struct random_data  *rdata;
{
  int a, b;
  long c;
  /*
   * random_r() generates random 32-bit values. OR them together.
   */
  if (random_r(&a, rdata)!=0) {
    fprintf(stderr,"Yikes!  call to random_r failed\n");
    exit(1);
  }
  if (random_r(&b, rdata)!=0) {
    fprintf(stderr,"Yikes!  call to random_r failed\n");
    exit(1);
  }
  c = ((long)a)<<32;
  *rval = c|b;
  return(*rval);
}
#endif /* !KERNEL && !SIMULATE && __osf__ */

/* these convert between text and integer.  Apparently the regular C macros
 * for doing this are not available in the kernel
 */

#define ISDIGIT(x)   ( (x) >= '0' && (x) <= '9' )
#define ISHEXCHAR(x) ( ((x) >= 'a' && (x) <= 'f') || ((x) >= 'A' && (x) <= 'F') )
#define ISHEX(x)     ( ISDIGIT(x) || ISHEXCHAR(x) )
#define HC2INT(x)    ( ((x) >= 'a' && (x) <= 'f') ? (x) - 'a' + 10 :                    \
		       ( ((x) >= 'A' && (x) <= 'F') ? (x) - 'A' + 10 : (x - '0') ) )

int rf_atoi(p)
 char  *p;
{
  int val = 0, negate = 0;
  
  if (*p == '-') {negate=1; p++;}
  for ( ; ISDIGIT(*p); p++) val = 10 * val + (*p - '0');
  return((negate) ? -val : val);
}

int rf_htoi(p)
  char  *p;
{
  int val = 0;
  for ( ; ISHEXCHAR(*p); p++) val = 16 * val + HC2INT(*p);
  return(val);
}
