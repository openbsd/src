/*	$OpenBSD: rf_desc.h,v 1.6 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_desc.h,v 1.5 2000/01/09 00:00:18 oster Exp $	*/

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

#ifndef	_RF__RF_DESC_H_
#define	_RF__RF_DESC_H_

#include "rf_archs.h"
#include "rf_types.h"
#include "rf_etimer.h"
#include "rf_dag.h"

struct RF_RaidReconDesc_s {
	RF_Raid_t	*raidPtr;	/* Raid device descriptor. */
	RF_RowCol_t	 row;		/* Row of failed disk. */
	RF_RowCol_t	 col;		/* Col of failed disk. */
	int		 state;		/*
					 * How far along the reconstruction
					 * operation has gotten.
					 */
	RF_RaidDisk_t	*spareDiskPtr;	/*
					 * Describes target disk for recon.
					 * (not used in dist sparing)
					 */
	int		 numDisksDone;	/*
					 * The number of surviving disks that
					 * have completed their work.
					 */
	RF_RowCol_t	 srow;		/*
					 * Row ID of the spare disk.
					 * (not used in dist sparing)
					 */
	RF_RowCol_t	 scol;		/*
					 * Col ID of the spare disk.
					 * (not used in dist sparing)
					 */
	/*
         * Prevent recon from hogging CPU
         */
	RF_Etimer_t	 recon_exec_timer;
	RF_uint64	 reconExecTimerRunning;
	RF_uint64	 reconExecTicks;
	RF_uint64	 maxReconExecTicks;

#if	RF_RECON_STATS > 0
	RF_uint64	 hsStallCount;	/* Head sep stall count. */
	RF_uint64	 numReconExecDelays;
	RF_uint64	 numReconEventWaits;
#endif	/* RF_RECON_STATS > 0 */
	RF_RaidReconDesc_t *next;
};

struct RF_RaidAccessDesc_s {
	RF_Raid_t	 *raidPtr;	/* Raid device descriptor. */
	RF_IoType_t	  type;		/* Read or write. */
	RF_RaidAddr_t	  raidAddress;	/*
					 * Starting address in raid address
					 * space.
					 */
	RF_SectorCount_t  numBlocks;	/*
					 * Number of blocks (sectors)
					 * to transfer.
					 */
	RF_StripeCount_t  numStripes;	/*
					 * Number of stripes involved in
					 * access.
					 */
	caddr_t		  bufPtr;	/* Pointer to data buffer. */
	RF_RaidAccessFlags_t flags;	/* Flags controlling operation. */
	int		  state;	/*
					 * Index into states telling how far
					 * along the RAID operation has gotten.
					 */
	RF_AccessState_t *states;	/* Array of states to be run. */
	int		  status;	/*
					 * Pass/fail status of the last
					 * operation.
					 */
	RF_DagList_t	 *dagArray;	/*
					 * Array of DAG lists, one list
					 * per stripe.
					 */
	RF_AccessStripeMapHeader_t *asmap; /* The asm for this I/O. */
	void		 *bp;		/*
					 * Buffer pointer for this RAID acc.
					 * Ignored outside the kernel.
					 */
	RF_DagHeader_t	**paramDAG;	/*
					 * Allows the DAG to be returned to
					 * the caller after I/O completion.
					 */
	RF_AccessStripeMapHeader_t **paramASM;	/*
						 * Allows the ASM to be
						 * returned to the caller
						 * after I/O completion.
						 */
	RF_AccTraceEntry_t tracerec;	/*
					 * Perf monitoring information for a
					 * user access (not for dag stats).
					 */
	void		(*callbackFunc) (RF_CBParam_t);
					/* Callback function for this I/O. */
	void		 *callbackArg;	/* Arg to give to callback func. */

	RF_AllocListElem_t *cleanupList; /*
					  * Memory to be freed at the
					  * end of the access.
					  */

	RF_RaidAccessDesc_t *next;
	RF_RaidAccessDesc_t *head;

	int		  numPending;

	RF_DECLARE_MUTEX( mutex );	/*
					 * These are used to implement
					 * blocking I/O.
					 */
	RF_DECLARE_COND(  cond );
	int		  async_flag;

	RF_Etimer_t	  timer;	/* Used for timing this access. */
};

#endif	/* ! _RF__RF_DESC_H_ */
