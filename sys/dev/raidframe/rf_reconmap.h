/*	$OpenBSD: rf_reconmap.h,v 1.3 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_reconmap.h,v 1.3 1999/02/05 00:06:16 oster Exp $	*/

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
 * rf_reconMap.h
 *
 * -- Header file describing reconstruction status data structure.
 ******************************************************************************/

#ifndef	_RF__RF_RECONMAP_H_
#define	_RF__RF_RECONMAP_H_

#include "rf_types.h"
#include "rf_threadstuff.h"

/*
 * Main reconstruction status descriptor; size and maxsize are used for
 * monitoring only: they have no function for reconstruction.
 */
struct RF_ReconMap_s {
	RF_SectorCount_t	  sectorsPerReconUnit;
						/*
						 * Sectors per reconstruct
						 * unit.
						 */
	RF_SectorCount_t	  sectorsInDisk;/* Total sectors in disk. */
	RF_SectorCount_t	  unitsLeft;	/* Recon units left to recon. */
	RF_ReconUnitCount_t	  totalRUs;	/* Total recon units on disk. */
	RF_ReconUnitCount_t	  spareRUs;	/*
						 * Total number of spare RUs on
						 * failed disk.
						 */
	RF_StripeCount_t	  totalParityStripes;
						/*
						 * Total number of parity
						 * stripes in array.
						 */
	u_int			  size;		/*
						 * Overall size of this
						 * structure.
						 */
	u_int			  maxSize;	/* Maximum size so far. */
	RF_ReconMapListElem_t	**status;	/*
						 * Array of ptrs to list
						 * elements.
						 */
	RF_DECLARE_MUTEX	 (mutex);
};

/* A list element. */
struct RF_ReconMapListElem_s {
	/* Bounding sect nums on this block. */
	RF_SectorNum_t		 startSector;
	RF_SectorNum_t		 stopSector;
	RF_ReconMapListElem_t	*next;		/* Next element in list. */
};

RF_ReconMap_t *rf_MakeReconMap(RF_Raid_t *,
	RF_SectorCount_t, RF_SectorCount_t, RF_ReconUnitCount_t);

void rf_ReconMapUpdate(RF_Raid_t *, RF_ReconMap_t *,
	RF_SectorNum_t, RF_SectorNum_t);

void rf_FreeReconMap(RF_ReconMap_t *);

int  rf_CheckRUReconstructed(RF_ReconMap_t *, RF_SectorNum_t);

RF_ReconUnitCount_t rf_UnitsLeftToReconstruct(RF_ReconMap_t *);

void rf_PrintReconMap(RF_Raid_t *, RF_ReconMap_t *, RF_RowCol_t, RF_RowCol_t);

void rf_PrintReconSchedule(RF_ReconMap_t *, struct timeval *);

#endif	/* !_RF__RF_RECONMAP_H_ */
