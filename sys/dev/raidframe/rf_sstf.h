/*	$OpenBSD: rf_sstf.h,v 1.3 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_sstf.h,v 1.3 1999/02/05 00:06:17 oster Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef	_RF__RF_SSTF_H_
#define	_RF__RF_SSTF_H_

#include "rf_diskqueue.h"

typedef struct RF_SstfQ_s {
	RF_DiskQueueData_t	*queue;
	RF_DiskQueueData_t	*qtail;
	int			 qlen;
} RF_SstfQ_t;

typedef struct RF_Sstf_s {
	RF_SstfQ_t		 left;
	RF_SstfQ_t		 right;
	RF_SstfQ_t		 lopri;
	RF_SectorNum_t		 last_sector;
	int			 dir;
	int			 allow_reverse;
} RF_Sstf_t;

void *rf_SstfCreate(RF_SectorCount_t, RF_AllocListElem_t *,
	RF_ShutdownList_t **);
void *rf_ScanCreate(RF_SectorCount_t, RF_AllocListElem_t *,
	RF_ShutdownList_t **);
void *rf_CscanCreate(RF_SectorCount_t, RF_AllocListElem_t *,
	RF_ShutdownList_t **);
void  rf_SstfEnqueue(void *, RF_DiskQueueData_t *, int);
RF_DiskQueueData_t *rf_SstfDequeue(void *);
RF_DiskQueueData_t *rf_SstfPeek(void *);
int   rf_SstfPromote(void *, RF_StripeNum_t, RF_ReconUnitNum_t);
RF_DiskQueueData_t *rf_ScanDequeue(void *);
RF_DiskQueueData_t *rf_ScanPeek(void *);
RF_DiskQueueData_t *rf_CscanDequeue(void *);
RF_DiskQueueData_t *rf_CscanPeek(void *);

#endif	/* !_RF__RF_SSTF_H_ */
