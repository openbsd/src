/*	$OpenBSD: rf_reconstruct.h,v 1.5 2002/12/16 07:01:05 tdeval Exp $	*/
/*	$NetBSD: rf_reconstruct.h,v 1.5 2000/05/28 00:48:30 oster Exp $	*/

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

/***********************************************************
 * rf_reconstruct.h -- Header file for reconstruction code.
 ***********************************************************/

#ifndef _RF__RF_RECONSTRUCT_H_
#define _RF__RF_RECONSTRUCT_H_

#include "rf_types.h"
#include <sys/time.h>
#include "rf_reconmap.h"
#include "rf_psstatus.h"

/* Reconstruction configuration information. */
struct RF_ReconConfig_s {
	unsigned		numFloatingReconBufs;
						/*
						 * Number of floating recon
						 * bufs to use.
						 */
	RF_HeadSepLimit_t	headSepLimit;	/*
						 * How far apart the heads are
						 * allowed to become, in parity
						 * stripes.
						 */
};

/* A reconstruction buffer. */
struct RF_ReconBuffer_s {
	RF_Raid_t		*raidPtr;	/*
						 * (void *) to avoid recursive
						 * includes.
						 */
	caddr_t			 buffer;	/* Points to the data. */
	RF_StripeNum_t		 parityStripeID;/*
						 * The parity stripe that this
						 * data relates to.
						 */
	int			 which_ru;	/*
						 * Which reconstruction unit
						 * within the PSS.
						 */
	RF_SectorNum_t		 failedDiskSectorOffset;
						/*
						 * The offset into the failed
						 * disk.
						 */
	RF_RowCol_t		 row, col;	/*
						 * Which disk this buffer
						 * belongs to or is targeted at.
						 */
	RF_StripeCount_t	 count;		/*
						 * Counts the # of SUs
						 * installed so far.
						 */
	int			 priority;	/*
						 * Used to force high priority
						 * recon.
						 */
	RF_RbufType_t		 type;		/* FORCED or FLOATING. */
	char			*arrived;	/*
						 * [x] = 1/0 if SU from disk x
						 * has/hasn't arrived.
						 */
	RF_ReconBuffer_t	*next;		/*
						 * Used for buffer management.
						 */
	void			*arg;		/*
						 * Generic field for general
						 * use.
						 */
	RF_RowCol_t		 spRow, spCol;	/*
						 * Spare disk to which this buf
						 * should be written.
						 */
	/* If dist sparing off, always identifies the replacement disk */
	RF_SectorNum_t		 spOffset;	/*
						 * Offset into the spare disk.
						 */
	/* If dist sparing off, identical to failedDiskSectorOffset */
	RF_ReconParityStripeStatus_t *pssPtr;	/*
						 * Debug pss associated with
						 * issue-pending write.
						 */
};

/*
 * A reconstruction event descriptor. The event types currently are:
 *    RF_REVENT_READDONE	-- A read operation has completed.
 *    RF_REVENT_WRITEDONE	-- A write operation has completed.
 *    RF_REVENT_BUFREADY	-- The buffer manager has produced a
 *				   full buffer.
 *    RF_REVENT_BLOCKCLEAR	-- A reconstruction blockage has been cleared.
 *    RF_REVENT_BUFCLEAR	-- The buffer manager has released a process
 *				   blocked on submission.
 *    RF_REVENT_SKIP		-- We need to skip the current RU and go on
 *				   to the next one, typ. b/c we found recon
 *				   forced.
 *    RF_REVENT_FORCEDREADONE	-- A forced-reconstructoin read operation has
 *				   completed.
 */
typedef enum RF_Revent_e {
	RF_REVENT_READDONE,
	RF_REVENT_WRITEDONE,
	RF_REVENT_BUFREADY,
	RF_REVENT_BLOCKCLEAR,
	RF_REVENT_BUFCLEAR,
	RF_REVENT_HEADSEPCLEAR,
	RF_REVENT_SKIP,
	RF_REVENT_FORCEDREADDONE
} RF_Revent_t;

struct RF_ReconEvent_s {
	RF_Revent_t		 type;	/* What kind of event has occurred. */
	RF_RowCol_t		 col;	/*
					 * Row ID is implicit in the queue in
					 * which the event is placed.
					 */
	void			*arg;	/* A generic argument. */
	RF_ReconEvent_t		*next;
};

/*
 * Reconstruction control information maintained per-disk.
 * (for surviving disks)
 */
struct RF_PerDiskReconCtrl_s {
	RF_ReconCtrl_t		*reconCtrl;
	RF_RowCol_t		 row, col;	/*
						 * To make this structure
						 * self-identifying.
						 */
	RF_StripeNum_t		 curPSID;	/*
						 * The next parity stripe ID
						 * to check on this disk.
						 */
	RF_HeadSepLimit_t	 headSepCounter;/*
						 * Counter used to control
						 * maximum head separation.
						 */
	RF_SectorNum_t		 diskOffset;	/*
						 * The offset into the
						 * indicated disk
						 * of the current PU.
						 */
	RF_ReconUnitNum_t	 ru_count;	/*
						 * This counts off the recon
						 * units within each parity
						 * unit.
						 */
	RF_ReconBuffer_t	*rbuf;		/*
						 * The recon buffer assigned
						 * to this disk.
						 */
};

/* Main reconstruction control structure. */
struct RF_ReconCtrl_s {
	RF_RaidReconDesc_t	*reconDesc;
	RF_RowCol_t		 fcol;		/* Which column has failed. */
	RF_PerDiskReconCtrl_t	*perDiskInfo;	/*
						 * Information maintained
						 * per-disk.
						 */
	RF_ReconMap_t		*reconMap;	/*
						 * Map of what has/has not
						 * been reconstructed.
						 */
	RF_RowCol_t		 spareRow;	/*
						 * Which of the spare disks
						 * we're using.
						 */
	RF_RowCol_t		 spareCol;
	RF_StripeNum_t		 lastPSID;	/*
						 * The ID of the last
						 * parity stripe we want
						 * reconstructed.
						 */
	int			 percentComplete;
						/*
						 * Percentage completion of
						 * reconstruction.
						 */
	int			 numRUsComplete;/*
						 * Number of Reconstruction
						 * Units done.
						 */
	int			 numRUsTotal;	/*
						 * Total number of
						 * Reconstruction Units.
						 */

	/* Reconstruction event queue. */
	RF_ReconEvent_t		*eventQueue;	/*
						 * Queue of pending
						 * reconstruction events.
						 */
	RF_DECLARE_MUTEX	(eq_mutex);	/*
						 * Mutex for locking event
						 * queue.
						 */
	RF_DECLARE_COND		(eq_cond);	/*
						 * Condition variable for
						 * signalling recon events.
						 */
	int			 eq_count;	/* Debug only. */

	/* Reconstruction buffer management. */
	RF_DECLARE_MUTEX	(rb_mutex);	/*
						 * Mutex for messing around
						 * with recon buffers.
						 */
	RF_ReconBuffer_t	*floatingRbufs;	/*
						 * Available floating
						 * reconstruction buffers.
						 */
	RF_ReconBuffer_t	*committedRbufs;/*
						 * Recon buffers that have
						 * been committed to some
						 * waiting disk.
						 */
	RF_ReconBuffer_t	*fullBufferList;/*
						 * Full buffers waiting to be
						 * written out.
						 */
	RF_ReconBuffer_t	*priorityList;	/*
						 * Full buffers that have been
						 * elevated to higher priority.
						 */
	RF_CallbackDesc_t	*bufferWaitList;/*
						 * Disks that are currently
						 * blocked waiting for buffers.
						 */

	/* Parity stripe status table. */
	RF_PSStatusHeader_t	*pssTable;	/*
						 * Stores the reconstruction
						 * status of active parity
						 * stripes.
						 */

	/* Maximum-head separation control. */
	RF_HeadSepLimit_t	 minHeadSepCounter;
						/*
						 * The minimum hs counter over
						 * all disks.
						 */
	RF_CallbackDesc_t	*headSepCBList;	/*
						 * List of callbacks to be
						 * done as minPSID advances.
						 */

	/* Performance monitoring. */
	struct timeval		 starttime;	/* Recon start time. */

	void		       (*continueFunc) (void *);
						/*
						 * Function to call when io
						 * returns.
						 */
	void			*continueArg;	/* Argument for Func. */
};

/* The default priority for reconstruction accesses. */
#define RF_IO_RECON_PRIORITY RF_IO_LOW_PRIORITY

int  rf_ConfigureReconstruction(RF_ShutdownList_t **);
int  rf_ReconstructFailedDisk(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_ReconstructFailedDiskBasic(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_ReconstructInPlace(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_ContinueReconstructFailedDisk(RF_RaidReconDesc_t *);
int  rf_ForceOrBlockRecon(RF_Raid_t *, RF_AccessStripeMap_t *,
	void (*) (RF_Raid_t *, void *), void *);
int  rf_UnblockRecon(RF_Raid_t *, RF_AccessStripeMap_t *);
int  rf_RegisterReconDoneProc(RF_Raid_t *, void (*) (RF_Raid_t *, void *),
	void *, RF_ReconDoneProc_t **);

#endif	/* !_RF__RF_RECONSTRUCT_H_ */
