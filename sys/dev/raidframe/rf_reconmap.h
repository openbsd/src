/*	$OpenBSD: rf_reconmap.h,v 1.1 1999/01/11 14:29:46 niklas Exp $	*/
/*	$NetBSD: rf_reconmap.h,v 1.1 1998/11/13 04:20:33 oster Exp $	*/
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

/******************************************************************************
 * rf_reconMap.h -- Header file describing reconstruction status data structure
 ******************************************************************************/

/* :  
 * Log: rf_reconmap.h,v 
 * Revision 1.10  1996/08/01 15:59:25  jimz
 * minor cleanup
 *
 * Revision 1.9  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.8  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.7  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.6  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.5  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.4  1995/12/06  15:04:01  root
 * added copyright info
 *
 */

#ifndef _RF__RF_RECONMAP_H_
#define _RF__RF_RECONMAP_H_

#include "rf_types.h"
#include "rf_threadstuff.h"

/*
 * Main reconstruction status descriptor. size and maxsize are used for
 * monitoring only:  they have no function for reconstruction.
 */
struct RF_ReconMap_s {
  RF_SectorCount_t         sectorsPerReconUnit; /* sectors per reconstruct unit */
  RF_SectorCount_t         sectorsInDisk;       /* total sectors in disk */
  RF_SectorCount_t         unitsLeft;           /* recon units left to recon */
  RF_ReconUnitCount_t      totalRUs;            /* total recon units on disk */
  RF_ReconUnitCount_t      spareRUs;            /* total number of spare RUs on failed disk */
  RF_StripeCount_t         totalParityStripes;  /* total number of parity stripes in array */
  u_int                    size;				/* overall size of this structure */
  u_int                    maxSize;			    /* maximum size so far */
  RF_ReconMapListElem_t  **status;              /* array of ptrs to list elements */
  RF_DECLARE_MUTEX(mutex)
};

/* a list element */
struct RF_ReconMapListElem_s {
  RF_SectorNum_t          startSector; /* bounding sect nums on this block */
  RF_SectorNum_t          stopSector;
  RF_ReconMapListElem_t  *next;        /* next element in list */
};

RF_ReconMap_t *rf_MakeReconMap(RF_Raid_t *raidPtr, RF_SectorCount_t ru_sectors,
	RF_SectorCount_t disk_sectors, RF_ReconUnitCount_t spareUnitsPerDisk);

void rf_ReconMapUpdate(RF_Raid_t *raidPtr, RF_ReconMap_t *mapPtr,
	RF_SectorNum_t startSector, RF_SectorNum_t stopSector);

void rf_FreeReconMap(RF_ReconMap_t *mapPtr);

int rf_CheckRUReconstructed(RF_ReconMap_t *mapPtr, RF_SectorNum_t startSector);

RF_ReconUnitCount_t rf_UnitsLeftToReconstruct(RF_ReconMap_t *mapPtr);

void rf_PrintReconMap(RF_Raid_t *raidPtr, RF_ReconMap_t *mapPtr,
	RF_RowCol_t frow, RF_RowCol_t fcol);

void rf_PrintReconSchedule(RF_ReconMap_t *mapPtr, struct timeval *starttime);

#endif /* !_RF__RF_RECONMAP_H_ */
