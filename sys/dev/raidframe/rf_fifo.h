/*	$OpenBSD: rf_fifo.h,v 1.1 1999/01/11 14:29:23 niklas Exp $	*/
/*	$NetBSD: rf_fifo.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/*
 * rf_fifo.h --  prioritized FIFO queue code.
 *
 * 4-9-93 Created (MCH)
 */

/*
 * :  
 * Log: rf_fifo.h,v 
 * Revision 1.12  1996/06/18 20:53:11  jimz
 * fix up disk queueing (remove configure routine,
 * add shutdown list arg to create routines)
 *
 * Revision 1.11  1996/06/13  20:41:28  jimz
 * add random queueing
 *
 * Revision 1.10  1996/06/13  20:38:28  jimz
 * add random dequeue, peek
 *
 * Revision 1.9  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.8  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.7  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.6  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.5  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.4  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.3  1995/12/01  18:22:26  root
 * added copyright info
 *
 * Revision 1.2  1995/11/07  15:31:57  wvcii
 * added Peek() function
 *
 */

#ifndef _RF__RF_FIFO_H_
#define _RF__RF_FIFO_H_

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_diskqueue.h"

typedef struct RF_FifoHeader_s {
    RF_DiskQueueData_t *hq_head, *hq_tail;	/* high priority requests */
    RF_DiskQueueData_t *lq_head, *lq_tail;	/* low priority requests */
    int                 hq_count, lq_count; /* debug only */
#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
    long                rval;               /* next random number (random qpolicy) */
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */
} RF_FifoHeader_t;

extern void *rf_FifoCreate(RF_SectorCount_t sectPerDisk,
	RF_AllocListElem_t *clList, RF_ShutdownList_t **listp);
extern void rf_FifoEnqueue(void *q_in, RF_DiskQueueData_t *elem,
	int priority);
extern RF_DiskQueueData_t *rf_FifoDequeue(void *q_in);
extern RF_DiskQueueData_t *rf_FifoPeek(void *q_in);
extern int rf_FifoPromote(void *q_in, RF_StripeNum_t parityStripeID,
	RF_ReconUnitNum_t which_ru);
#if !defined(KERNEL) && RF_INCLUDE_QUEUE_RANDOM > 0
extern RF_DiskQueueData_t *rf_RandomDequeue(void *q_in);
extern RF_DiskQueueData_t *rf_RandomPeek(void *q_in);
#endif /* !KERNEL && RF_INCLUDE_QUEUE_RANDOM > 0 */

#endif /* !_RF__RF_FIFO_H_ */
