/*	$OpenBSD: rf_reconstruct.c,v 1.14 2003/01/19 14:27:01 tdeval Exp $	*/
/*	$NetBSD: rf_reconstruct.c,v 1.26 2000/06/04 02:05:13 oster Exp $	*/

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

/**************************************************************
 *
 * rf_reconstruct.c -- Code to perform on-line reconstruction.
 *
 **************************************************************/

#include "rf_types.h"
#include <sys/time.h>
#include <sys/buf.h>
#include <sys/errno.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#if	__NETBSD__
#include <sys/vnode.h>
#endif

#include "rf_raid.h"
#include "rf_reconutil.h"
#include "rf_revent.h"
#include "rf_reconbuffer.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_dag.h"
#include "rf_desc.h"
#include "rf_general.h"
#include "rf_freelist.h"
#include "rf_debugprint.h"
#include "rf_driver.h"
#include "rf_utils.h"
#include "rf_shutdown.h"

#include "rf_kintf.h"

/*
 * Setting these to -1 causes them to be set to their default values if not set
 * by debug options.
 */

#define	Dprintf(s)							\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);	\
} while (0)
#define	Dprintf1(s,a)							\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    NULL, NULL, NULL, NULL, NULL, NULL, NULL);		\
} while (0)
#define	Dprintf2(s,a,b)							\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    NULL, NULL, NULL, NULL, NULL, NULL);		\
} while (0)
#define	Dprintf3(s,a,b,c)						\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    (void *)((unsigned long)c),				\
		    NULL, NULL, NULL, NULL, NULL);			\
} while (0)
#define	Dprintf4(s,a,b,c,d)						\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    (void *)((unsigned long)c),				\
		    (void *)((unsigned long)d),				\
		    NULL, NULL, NULL, NULL);				\
} while (0)
#define	Dprintf5(s,a,b,c,d,e)						\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    (void *)((unsigned long)c),				\
		    (void *)((unsigned long)d),				\
		    (void *)((unsigned long)e),				\
		    NULL, NULL, NULL);					\
} while (0)
#define	Dprintf6(s,a,b,c,d,e,f)						\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    (void *)((unsigned long)c),				\
		    (void *)((unsigned long)d),				\
		    (void *)((unsigned long)e),				\
		    (void *)((unsigned long)f),				\
		    NULL, NULL);					\
} while (0)
#define	Dprintf7(s,a,b,c,d,e,f,g)					\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    (void *)((unsigned long)c),				\
		    (void *)((unsigned long)d),				\
		    (void *)((unsigned long)e),				\
		    (void *)((unsigned long)f),				\
		    (void *)((unsigned long)g),				\
		    NULL);						\
} while (0)

#define	DDprintf1(s,a)							\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    NULL, NULL, NULL, NULL, NULL, NULL, NULL);		\
} while (0)
#define	DDprintf2(s,a,b)						\
do {									\
	if (rf_reconDebug)						\
		rf_debug_printf(s,					\
		    (void *)((unsigned long)a),				\
		    (void *)((unsigned long)b),				\
		    NULL, NULL, NULL, NULL, NULL, NULL);		\
} while (0)

static RF_FreeList_t *rf_recond_freelist;
#define	RF_MAX_FREE_RECOND	4
#define	RF_RECOND_INC		1

RF_RaidReconDesc_t *rf_AllocRaidReconDesc(RF_Raid_t *,
	RF_RowCol_t, RF_RowCol_t, RF_RaidDisk_t *, int,
	RF_RowCol_t, RF_RowCol_t);
int  rf_ProcessReconEvent(RF_Raid_t *, RF_RowCol_t, RF_ReconEvent_t *);
int  rf_IssueNextReadRequest(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_TryToRead(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);
int  rf_ComputePSDiskOffsets(RF_Raid_t *, RF_StripeNum_t,
	RF_RowCol_t, RF_RowCol_t, RF_SectorNum_t *, RF_SectorNum_t *,
	RF_RowCol_t *, RF_RowCol_t *, RF_SectorNum_t *);
int  rf_ReconReadDoneProc(void *, int);
int  rf_ReconWriteDoneProc(void *, int);
void rf_CheckForNewMinHeadSep(RF_Raid_t *, RF_RowCol_t, RF_HeadSepLimit_t);
int  rf_CheckHeadSeparation(RF_Raid_t *, RF_PerDiskReconCtrl_t *,
	RF_RowCol_t, RF_RowCol_t, RF_HeadSepLimit_t, RF_ReconUnitNum_t);
void rf_ForceReconReadDoneProc(void *, int);
void rf_ShutdownReconstruction(void *);

/*
 * These functions are inlined on gcc. If they are used more than
 * once, it is strongly advised to un-line them.
 */
void rf_FreeReconDesc(RF_RaidReconDesc_t *);
int  rf_IssueNextWriteRequest(RF_Raid_t *, RF_RowCol_t);
int  rf_CheckForcedOrBlockedReconstruction(RF_Raid_t *,
	RF_ReconParityStripeStatus_t *, RF_PerDiskReconCtrl_t *,
	RF_RowCol_t, RF_RowCol_t, RF_StripeNum_t, RF_ReconUnitNum_t);
void rf_SignalReconDone(RF_Raid_t *);

struct RF_ReconDoneProc_s {
	void			(*proc) (RF_Raid_t *, void *);
	void			 *arg;
	RF_ReconDoneProc_t	 *next;
};

static RF_FreeList_t *rf_rdp_freelist;
#define	RF_MAX_FREE_RDP		4
#define	RF_RDP_INC		1

void
rf_SignalReconDone(RF_Raid_t *raidPtr)
{
	RF_ReconDoneProc_t *p;

	RF_LOCK_MUTEX(raidPtr->recon_done_proc_mutex);
	for (p = raidPtr->recon_done_procs; p; p = p->next) {
		p->proc(raidPtr, p->arg);
	}
	RF_UNLOCK_MUTEX(raidPtr->recon_done_proc_mutex);
}

int
rf_RegisterReconDoneProc(RF_Raid_t *raidPtr, void (*proc) (RF_Raid_t *, void *),
    void *arg, RF_ReconDoneProc_t **handlep)
{
	RF_ReconDoneProc_t *p;

	RF_FREELIST_GET(rf_rdp_freelist, p, next, (RF_ReconDoneProc_t *));
	if (p == NULL)
		return (ENOMEM);
	p->proc = proc;
	p->arg = arg;
	RF_LOCK_MUTEX(raidPtr->recon_done_proc_mutex);
	p->next = raidPtr->recon_done_procs;
	raidPtr->recon_done_procs = p;
	RF_UNLOCK_MUTEX(raidPtr->recon_done_proc_mutex);
	if (handlep)
		*handlep = p;
	return (0);
}

/*****************************************************************************
 *
 * Sets up the parameters that will be used by the reconstruction process.
 * Currently there are none, except for those that the layout-specific
 * configuration (e.g. rf_ConfigureDeclustered) routine sets up.
 *
 * In the kernel, we fire off the recon thread.
 *
 *****************************************************************************/
void
rf_ShutdownReconstruction(void *ignored)
{
	RF_FREELIST_DESTROY(rf_recond_freelist, next, (RF_RaidReconDesc_t *));
	RF_FREELIST_DESTROY(rf_rdp_freelist, next, (RF_ReconDoneProc_t *));
}

int
rf_ConfigureReconstruction(RF_ShutdownList_t **listp)
{
	int rc;

	RF_FREELIST_CREATE(rf_recond_freelist, RF_MAX_FREE_RECOND,
	    RF_RECOND_INC, sizeof(RF_RaidReconDesc_t));
	if (rf_recond_freelist == NULL)
		return (ENOMEM);
	RF_FREELIST_CREATE(rf_rdp_freelist, RF_MAX_FREE_RDP,
	    RF_RDP_INC, sizeof(RF_ReconDoneProc_t));
	if (rf_rdp_freelist == NULL) {
		RF_FREELIST_DESTROY(rf_recond_freelist, next,
		    (RF_RaidReconDesc_t *));
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownReconstruction, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d"
		    " rc=%d.\n", __FILE__, __LINE__, rc);
		rf_ShutdownReconstruction(NULL);
		return (rc);
	}
	return (0);
}

RF_RaidReconDesc_t *
rf_AllocRaidReconDesc(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col,
    RF_RaidDisk_t *spareDiskPtr, int numDisksDone, RF_RowCol_t srow,
    RF_RowCol_t scol)
{

	RF_RaidReconDesc_t *reconDesc;

	RF_FREELIST_GET(rf_recond_freelist, reconDesc, next,
	    (RF_RaidReconDesc_t *));

	reconDesc->raidPtr = raidPtr;
	reconDesc->row = row;
	reconDesc->col = col;
	reconDesc->spareDiskPtr = spareDiskPtr;
	reconDesc->numDisksDone = numDisksDone;
	reconDesc->srow = srow;
	reconDesc->scol = scol;
	reconDesc->state = 0;
	reconDesc->next = NULL;

	return (reconDesc);
}

void
rf_FreeReconDesc(RF_RaidReconDesc_t *reconDesc)
{
#if	RF_RECON_STATS > 0
	printf("RAIDframe: %qu recon event waits, %qu recon delays.\n",
	    reconDesc->numReconEventWaits, reconDesc->numReconExecDelays);
#endif	/* RF_RECON_STATS > 0 */

	printf("RAIDframe: %qu max exec ticks.\n",
	    reconDesc->maxReconExecTicks);

#if	(RF_RECON_STATS > 0) || defined(_KERNEL)
	printf("\n");
#endif	/* (RF_RECON_STATS > 0) || _KERNEL */
	RF_FREELIST_FREE(rf_recond_freelist, reconDesc, next);
}


/*****************************************************************************
 *
 * Primary routine to reconstruct a failed disk. This should be called from
 * within its own thread. It won't return until reconstruction completes,
 * fails, or is aborted.
 *
 *****************************************************************************/
int
rf_ReconstructFailedDisk(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col)
{
	RF_LayoutSW_t *lp;
	int rc;

	lp = raidPtr->Layout.map;
	if (lp->SubmitReconBuffer) {
		/*
		 * The current infrastructure only supports reconstructing one
		 * disk at a time for each array.
		 */
		RF_LOCK_MUTEX(raidPtr->mutex);
		while (raidPtr->reconInProgress) {
			RF_WAIT_COND(raidPtr->waitForReconCond, raidPtr->mutex);
		}
		raidPtr->reconInProgress++;
		RF_UNLOCK_MUTEX(raidPtr->mutex);
		rc = rf_ReconstructFailedDiskBasic(raidPtr, row, col);
		RF_LOCK_MUTEX(raidPtr->mutex);
		raidPtr->reconInProgress--;
		RF_UNLOCK_MUTEX(raidPtr->mutex);
	} else {
		RF_ERRORMSG1("RECON: no way to reconstruct failed disk for"
		    " arch %c.\n", lp->parityConfig);
		rc = EIO;
	}
	RF_SIGNAL_COND(raidPtr->waitForReconCond);
	wakeup(&raidPtr->waitForReconCond);	/*
						 * XXX Methinks this will be
						 * needed at some point... GO
						 */
	return (rc);
}

int
rf_ReconstructFailedDiskBasic(RF_Raid_t *raidPtr, RF_RowCol_t row,
    RF_RowCol_t col)
{
	RF_ComponentLabel_t c_label;
	RF_RaidDisk_t *spareDiskPtr = NULL;
	RF_RaidReconDesc_t *reconDesc;
	RF_RowCol_t srow, scol;
	int numDisksDone = 0, rc;

	/* First look for a spare drive onto which to reconstruct the data. */
	/*
	 * Spare disk descriptors are stored in row 0. This may have to
	 * change eventually.
	 */

	RF_LOCK_MUTEX(raidPtr->mutex);
	RF_ASSERT(raidPtr->Disks[row][col].status == rf_ds_failed);

	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		if (raidPtr->status[row] != rf_rs_degraded) {
			RF_ERRORMSG2("Unable to reconstruct disk at row %d"
			    " col %d because status not degraded.\n", row, col);
			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return (EINVAL);
		}
		srow = row;
		scol = (-1);
	} else {
		srow = 0;
		for (scol = raidPtr->numCol;
		     scol < raidPtr->numCol + raidPtr->numSpare; scol++) {
			if (raidPtr->Disks[srow][scol].status == rf_ds_spare) {
				spareDiskPtr = &raidPtr->Disks[srow][scol];
				spareDiskPtr->status = rf_ds_used_spare;
				break;
			}
		}
		if (!spareDiskPtr) {
			RF_ERRORMSG2("Unable to reconstruct disk at row %d"
			    " col %d because no spares are available.\n",
			    row, col);
			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return (ENOSPC);
		}
		printf("RECON: initiating reconstruction on row %d col %d"
		    " -> spare at row %d col %d.\n", row, col, srow, scol);
	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);

	reconDesc = rf_AllocRaidReconDesc((void *) raidPtr, row, col,
	    spareDiskPtr, numDisksDone, srow, scol);
	raidPtr->reconDesc = (void *) reconDesc;
#if	RF_RECON_STATS > 0
	reconDesc->hsStallCount = 0;
	reconDesc->numReconExecDelays = 0;
	reconDesc->numReconEventWaits = 0;
#endif	/* RF_RECON_STATS > 0 */
	reconDesc->reconExecTimerRunning = 0;
	reconDesc->reconExecTicks = 0;
	reconDesc->maxReconExecTicks = 0;
	rc = rf_ContinueReconstructFailedDisk(reconDesc);

	if (!rc) {
		/* Fix up the component label. */
		/* Don't actually need the read here... */
		raidread_component_label(
		    raidPtr->raid_cinfo[srow][scol].ci_dev,
		    raidPtr->raid_cinfo[srow][scol].ci_vp,
		    &c_label);

		raid_init_component_label(raidPtr, &c_label);
		c_label.row = row;
		c_label.column = col;
		c_label.clean = RF_RAID_DIRTY;
		c_label.status = rf_ds_optimal;

		/* XXXX MORE NEEDED HERE. */

		raidwrite_component_label(
		    raidPtr->raid_cinfo[srow][scol].ci_dev,
		    raidPtr->raid_cinfo[srow][scol].ci_vp,
		    &c_label);

	}
	return (rc);
}

/*
 *
 * Allow reconstructing a disk in-place -- i.e. component /dev/sd2e goes AWOL,
 * and you don't get a spare until the next Monday. With this function
 * (and hot-swappable drives) you can now put your new disk containing
 * /dev/sd2e on the bus, scsictl it alive, and then use raidctl(8) to
 * rebuild the data "on the spot".
 *
 */

int
rf_ReconstructInPlace(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col)
{
	RF_RaidDisk_t *spareDiskPtr = NULL;
	RF_RaidReconDesc_t *reconDesc;
	RF_LayoutSW_t *lp;
	RF_RaidDisk_t *badDisk;
	RF_ComponentLabel_t c_label;
	int numDisksDone = 0, rc;
	struct partinfo dpart;
	struct vnode *vp;
	struct vattr va;
	struct proc *proc;
	int retcode;
	int ac;

	lp = raidPtr->Layout.map;
	if (lp->SubmitReconBuffer) {
		/*
		 * The current infrastructure only supports reconstructing one
		 * disk at a time for each array.
		 */
		RF_LOCK_MUTEX(raidPtr->mutex);
		if ((raidPtr->Disks[row][col].status == rf_ds_optimal) &&
		    (raidPtr->numFailures > 0)) {
			/* XXX 0 above shouldn't be constant !!! */
			/*
			 * Some component other than this has failed.
			 * Let's not make things worse than they already
			 * are...
			 */
#ifdef	RAIDDEBUG
			printf("RAIDFRAME: Unable to reconstruct to disk at:\n"
			    "      Row: %d Col: %d   Too many failures.\n",
			    row, col);
#endif	/* RAIDDEBUG */
			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return (EINVAL);
		}
		if (raidPtr->Disks[row][col].status == rf_ds_reconstructing) {
#ifdef	RAIDDEBUG
			printf("RAIDFRAME: Unable to reconstruct to disk at:\n"
			    "      Row: %d Col: %d   Reconstruction already"
			    " occuring !\n", row, col);
#endif	/* RAIDDEBUG */

			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return (EINVAL);
		}


		if (raidPtr->Disks[row][col].status != rf_ds_failed) {
			/* "It's gone..." */
			raidPtr->numFailures++;
			raidPtr->Disks[row][col].status = rf_ds_failed;
			raidPtr->status[row] = rf_rs_degraded;
			rf_update_component_labels(raidPtr,
			    RF_NORMAL_COMPONENT_UPDATE);
		}

		while (raidPtr->reconInProgress) {
			RF_WAIT_COND(raidPtr->waitForReconCond, raidPtr->mutex);
		}

		raidPtr->reconInProgress++;

		/*
		 * First look for a spare drive onto which to reconstruct
		 * the data. Spare disk descriptors are stored in row 0.
		 * This may have to change eventually.
		 */

		/*
		 * Actually, we don't care if it's failed or not...
		 * On a RAID set with correct parity, this function
		 * should be callable on any component without ill effects.
		 */
		/*
		 * RF_ASSERT(raidPtr->Disks[row][col].status == rf_ds_failed);
		 */

		if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
			RF_ERRORMSG2("Unable to reconstruct to disk at row %d"
			    " col %d: operation not supported for"
			    " RF_DISTRIBUTE_SPARE.\n", row, col);

			raidPtr->reconInProgress--;
			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return (EINVAL);
		}

		/*
		 * XXX Need goop here to see if the disk is alive,
		 * and, if not, make it so...
		 */

		badDisk = &raidPtr->Disks[row][col];

		proc = raidPtr->recon_thread;

		/*
		 * This device may have been opened successfully the
		 * first time. Close it before trying to open it again...
		 */

		if (raidPtr->raid_cinfo[row][col].ci_vp != NULL) {
			printf("Closing the opened device: %s\n",
			    raidPtr->Disks[row][col].devname);
			vp = raidPtr->raid_cinfo[row][col].ci_vp;
			ac = raidPtr->Disks[row][col].auto_configured;
			rf_close_component(raidPtr, vp, ac);
			raidPtr->raid_cinfo[row][col].ci_vp = NULL;
		}
		/*
		 * Note that this disk was *not* auto_configured (any longer).
		 */
		raidPtr->Disks[row][col].auto_configured = 0;

		printf("About to (re-)open the device for rebuilding: %s\n",
		    raidPtr->Disks[row][col].devname);

		retcode = raidlookup(raidPtr->Disks[row][col].devname,
		    proc, &vp);

		if (retcode) {
			printf("raid%d: rebuilding: raidlookup on device: %s"
			    " failed: %d !\n", raidPtr->raidid,
			    raidPtr->Disks[row][col].devname, retcode);

			/*
			 * XXX the component isn't responding properly...
			 * Must still be dead :-(
			 */
			raidPtr->reconInProgress--;
			RF_UNLOCK_MUTEX(raidPtr->mutex);
			return(retcode);

		} else {

			/*
			 * Ok, so we can at least do a lookup...
			 * How about actually getting a vp for it ?
			 */

			if ((retcode =
			     VOP_GETATTR(vp, &va, proc->p_ucred, proc)) != 0) {
				raidPtr->reconInProgress--;
				RF_UNLOCK_MUTEX(raidPtr->mutex);
				return(retcode);
			}
			retcode = VOP_IOCTL(vp, DIOCGPART, (caddr_t) & dpart,
			    FREAD, proc->p_ucred, proc);
			if (retcode) {
				raidPtr->reconInProgress--;
				RF_UNLOCK_MUTEX(raidPtr->mutex);
				return(retcode);
			}
			raidPtr->Disks[row][col].blockSize =
			    dpart.disklab->d_secsize;

			raidPtr->Disks[row][col].numBlocks =
			    dpart.part->p_size - rf_protectedSectors;

			raidPtr->raid_cinfo[row][col].ci_vp = vp;
			raidPtr->raid_cinfo[row][col].ci_dev = va.va_rdev;

			raidPtr->Disks[row][col].dev = va.va_rdev;

			/*
			 * We allow the user to specify that only a
			 * fraction of the disks should be used this is
			 * just for debug:  it speeds up the parity scan.
			 */
			raidPtr->Disks[row][col].numBlocks =
			    raidPtr->Disks[row][col].numBlocks *
			    rf_sizePercentage / 100;
		}

		spareDiskPtr = &raidPtr->Disks[row][col];
		spareDiskPtr->status = rf_ds_used_spare;

		printf("RECON: Initiating in-place reconstruction on\n");
		printf("       row %d col %d -> spare at row %d col %d.\n",
		    row, col, row, col);

		RF_UNLOCK_MUTEX(raidPtr->mutex);

		reconDesc = rf_AllocRaidReconDesc((void *) raidPtr, row, col,
		    spareDiskPtr, numDisksDone, row, col);
		raidPtr->reconDesc = (void *) reconDesc;
#if	RF_RECON_STATS > 0
		reconDesc->hsStallCount = 0;
		reconDesc->numReconExecDelays = 0;
		reconDesc->numReconEventWaits = 0;
#endif	/* RF_RECON_STATS > 0 */
		reconDesc->reconExecTimerRunning = 0;
		reconDesc->reconExecTicks = 0;
		reconDesc->maxReconExecTicks = 0;
		rc = rf_ContinueReconstructFailedDisk(reconDesc);

		RF_LOCK_MUTEX(raidPtr->mutex);
		raidPtr->reconInProgress--;
		RF_UNLOCK_MUTEX(raidPtr->mutex);

	} else {
		RF_ERRORMSG1("RECON: no way to reconstruct failed disk for"
		    " arch %c.\n", lp->parityConfig);
		rc = EIO;
	}
	RF_LOCK_MUTEX(raidPtr->mutex);

	if (!rc) {
		/*
		 * Need to set these here, as at this point it'll be claiming
		 * that the disk is in rf_ds_spared !  But we know better :-)
		 */

		raidPtr->Disks[row][col].status = rf_ds_optimal;
		raidPtr->status[row] = rf_rs_optimal;

		/* Fix up the component label. */
		/* Don't actually need the read here... */
		raidread_component_label(
		    raidPtr->raid_cinfo[row][col].ci_dev,
		    raidPtr->raid_cinfo[row][col].ci_vp,
		    &c_label);

		raid_init_component_label(raidPtr, &c_label);

		c_label.row = row;
		c_label.column = col;

		raidwrite_component_label(raidPtr->raid_cinfo[row][col].ci_dev,
		    raidPtr->raid_cinfo[row][col].ci_vp, &c_label);

	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	RF_SIGNAL_COND(raidPtr->waitForReconCond);
	wakeup(&raidPtr->waitForReconCond);
	return (rc);
}


int
rf_ContinueReconstructFailedDisk(RF_RaidReconDesc_t *reconDesc)
{
	RF_Raid_t *raidPtr = reconDesc->raidPtr;
	RF_RowCol_t row = reconDesc->row;
	RF_RowCol_t col = reconDesc->col;
	RF_RowCol_t srow = reconDesc->srow;
	RF_RowCol_t scol = reconDesc->scol;
	RF_ReconMap_t *mapPtr;

	RF_ReconEvent_t *event;
	struct timeval etime, elpsd;
	unsigned long xor_s, xor_resid_us;
	int retcode, i, ds;

	switch (reconDesc->state) {
	case 0:
		raidPtr->accumXorTimeUs = 0;

		/* Create one trace record per physical disk. */
		RF_Malloc(raidPtr->recon_tracerecs, raidPtr->numCol *
		    sizeof(RF_AccTraceEntry_t), (RF_AccTraceEntry_t *));

		/*
		 * Quiesce the array prior to starting recon. This is needed
		 * to assure no nasty interactions with pending user writes.
		 * We need to do this before we change the disk or row status.
		 */
		reconDesc->state = 1;

		Dprintf("RECON: begin request suspend.\n");
		retcode = rf_SuspendNewRequestsAndWait(raidPtr);
		Dprintf("RECON: end request suspend.\n");
		rf_StartUserStats(raidPtr);	/*
						 * Zero out the stats kept on
						 * user accs.
						 */
		/* Fall through to state 1. */
	case 1:
		RF_LOCK_MUTEX(raidPtr->mutex);

		/*
		 * Create the reconstruction control pointer and install it in
		 * the right slot.
		 */
		raidPtr->reconControl[row] =
		    rf_MakeReconControl(reconDesc, row, col, srow, scol);
		mapPtr = raidPtr->reconControl[row]->reconMap;
		raidPtr->status[row] = rf_rs_reconstructing;
		raidPtr->Disks[row][col].status = rf_ds_reconstructing;
		raidPtr->Disks[row][col].spareRow = srow;
		raidPtr->Disks[row][col].spareCol = scol;

		RF_UNLOCK_MUTEX(raidPtr->mutex);

		RF_GETTIME(raidPtr->reconControl[row]->starttime);

		/*
		 * Now start up the actual reconstruction: issue a read for
		 * each surviving disk.
		 */

		reconDesc->numDisksDone = 0;
		for (i = 0; i < raidPtr->numCol; i++) {
			if (i != col) {
				/*
				 * Find and issue the next I/O on the
				 * indicated disk.
				 */
				if (rf_IssueNextReadRequest(raidPtr, row, i)) {
					Dprintf2("RECON: done issuing for r%d"
					    " c%d.\n", row, i);
					reconDesc->numDisksDone++;
				}
			}
		}

		reconDesc->state = 2;

	case 2:
		Dprintf("RECON: resume requests.\n");
		rf_ResumeNewRequests(raidPtr);

		reconDesc->state = 3;

	case 3:

		/*
		 * Process reconstruction events until all disks report that
		 * they've completed all work.
		 */
		mapPtr = raidPtr->reconControl[row]->reconMap;

		while (reconDesc->numDisksDone < raidPtr->numCol - 1) {

			event = rf_GetNextReconEvent(reconDesc, row,
			   (void (*) (void *)) rf_ContinueReconstructFailedDisk,
			    reconDesc);
			RF_ASSERT(event);

			if (rf_ProcessReconEvent(raidPtr, row, event))
				reconDesc->numDisksDone++;
			raidPtr->reconControl[row]->numRUsTotal =
				mapPtr->totalRUs;
			raidPtr->reconControl[row]->numRUsComplete =
				mapPtr->totalRUs -
				rf_UnitsLeftToReconstruct(mapPtr);

			raidPtr->reconControl[row]->percentComplete =
			    (raidPtr->reconControl[row]->numRUsComplete * 100 /
			     raidPtr->reconControl[row]->numRUsTotal);
			if (rf_prReconSched) {
				rf_PrintReconSchedule(
				    raidPtr->reconControl[row]->reconMap,
				    &(raidPtr->reconControl[row]->starttime));
			}
		}

		reconDesc->state = 4;

	case 4:
		mapPtr = raidPtr->reconControl[row]->reconMap;
		if (rf_reconDebug) {
			printf("RECON: all reads completed.\n");
		}
		/*
		 * At this point all the reads have completed. We now wait
		 * for any pending writes to complete, and then we're done.
		 */

		while (rf_UnitsLeftToReconstruct(
		    raidPtr->reconControl[row]->reconMap) > 0) {

			event = rf_GetNextReconEvent(reconDesc, row,
			   (void (*) (void *)) rf_ContinueReconstructFailedDisk,
			    reconDesc);
			RF_ASSERT(event);

			/* Ignore return code. */
			(void) rf_ProcessReconEvent(raidPtr, row, event);
			raidPtr->reconControl[row]->percentComplete =
			    100 - (rf_UnitsLeftToReconstruct(mapPtr) * 100 /
			    mapPtr->totalRUs);
			if (rf_prReconSched) {
				rf_PrintReconSchedule(
				    raidPtr->reconControl[row]->reconMap,
				    &(raidPtr->reconControl[row]->starttime));
			}
		}
		reconDesc->state = 5;

	case 5:
		/*
		 * Success:  mark the dead disk as reconstructed. We quiesce
		 * the array here to assure no nasty interactions with pending
		 * user accesses, when we free up the psstatus structure as
		 * part of FreeReconControl().
		 */

		reconDesc->state = 6;

		retcode = rf_SuspendNewRequestsAndWait(raidPtr);
		rf_StopUserStats(raidPtr);
		rf_PrintUserStats(raidPtr);	/*
						 * Print out the stats on user
						 * accs accumulated during
						 * recon.
						 */

		/* Fall through to state 6. */
	case 6:
		RF_LOCK_MUTEX(raidPtr->mutex);
		raidPtr->numFailures--;
		ds = (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE);
		raidPtr->Disks[row][col].status = (ds) ? rf_ds_dist_spared :
							 rf_ds_spared;
		raidPtr->status[row] = (ds) ? rf_rs_reconfigured :
					      rf_rs_optimal;
		RF_UNLOCK_MUTEX(raidPtr->mutex);
		RF_GETTIME(etime);
		RF_TIMEVAL_DIFF(&(raidPtr->reconControl[row]->starttime),
		    &etime, &elpsd);

		/*
		 * XXX -- Why is state 7 different from state 6 if there is no
		 * return() here ? -- XXX Note that I set elpsd above & use it
		 * below, so if you put a return here you'll have to fix this.
		 * (also, FreeReconControl is called below).
		 */

	case 7:

		rf_ResumeNewRequests(raidPtr);

		printf("Reconstruction of disk at row %d col %d completed.\n",
		    row, col);
		xor_s = raidPtr->accumXorTimeUs / 1000000;
		xor_resid_us = raidPtr->accumXorTimeUs % 1000000;
		printf("Recon time was %d.%06d seconds, accumulated XOR time"
		    " was %ld us (%ld.%06ld).\n", (int) elpsd.tv_sec,
		    (int) elpsd.tv_usec, raidPtr->accumXorTimeUs, xor_s,
		    xor_resid_us);
		printf("  (start time %d sec %d usec, end time %d sec %d"
		    " usec)\n",
		    (int) raidPtr->reconControl[row]->starttime.tv_sec,
		    (int) raidPtr->reconControl[row]->starttime.tv_usec,
		    (int) etime.tv_sec, (int) etime.tv_usec);

#if	RF_RECON_STATS > 0
		printf("Total head-sep stall count was %d.\n",
		    (int) reconDesc->hsStallCount);
#endif	/* RF_RECON_STATS > 0 */
		rf_FreeReconControl(raidPtr, row);
		RF_Free(raidPtr->recon_tracerecs, raidPtr->numCol *
		    sizeof(RF_AccTraceEntry_t));
		rf_FreeReconDesc(reconDesc);

	}

	rf_SignalReconDone(raidPtr);
	return (0);
}


/*****************************************************************************
 * Do the right thing upon each reconstruction event.
 * Returns nonzero if and only if there is nothing left unread on the
 * indicated disk.
 *****************************************************************************/
int
rf_ProcessReconEvent(RF_Raid_t *raidPtr, RF_RowCol_t frow,
    RF_ReconEvent_t *event)
{
	int retcode = 0, submitblocked;
	RF_ReconBuffer_t *rbuf;
	RF_SectorCount_t sectorsPerRU;

	Dprintf1("RECON: rf_ProcessReconEvent type %d.\n", event->type);

	switch (event->type) {

		/* A read I/O has completed. */
	case RF_REVENT_READDONE:
		rbuf = raidPtr->reconControl[frow]
		    ->perDiskInfo[event->col].rbuf;
		Dprintf3("RECON: READDONE EVENT: row %d col %d psid %ld.\n",
		    frow, event->col, rbuf->parityStripeID);
		Dprintf7("RECON: done read  psid %ld buf %lx  %02x %02x %02x"
		    " %02x %02x.\n", rbuf->parityStripeID, rbuf->buffer,
		    rbuf->buffer[0] & 0xff, rbuf->buffer[1] & 0xff,
		    rbuf->buffer[2] & 0xff, rbuf->buffer[3] & 0xff,
		    rbuf->buffer[4] & 0xff);
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		submitblocked = rf_SubmitReconBuffer(rbuf, 0, 0);
		Dprintf1("RECON: submitblocked=%d.\n", submitblocked);
		if (!submitblocked)
			retcode = rf_IssueNextReadRequest(raidPtr, frow,
			    event->col);
		break;

		/* A write I/O has completed. */
	case RF_REVENT_WRITEDONE:
		if (rf_floatingRbufDebug) {
			rf_CheckFloatingRbufCount(raidPtr, 1);
		}
		sectorsPerRU = raidPtr->Layout.sectorsPerStripeUnit *
		    raidPtr->Layout.SUsPerRU;
		rbuf = (RF_ReconBuffer_t *) event->arg;
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		Dprintf3("RECON: WRITEDONE EVENT: psid %d ru %d"
		    " (%d %% complete).\n",
		    rbuf->parityStripeID, rbuf->which_ru,
		    raidPtr->reconControl[frow]->percentComplete);
		rf_ReconMapUpdate(raidPtr, raidPtr->reconControl[frow]
		    ->reconMap, rbuf->failedDiskSectorOffset,
		    rbuf->failedDiskSectorOffset + sectorsPerRU - 1);
		rf_RemoveFromActiveReconTable(raidPtr, frow,
		    rbuf->parityStripeID, rbuf->which_ru);

		if (rbuf->type == RF_RBUF_TYPE_FLOATING) {
			RF_LOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
			raidPtr->numFullReconBuffers--;
			rf_ReleaseFloatingReconBuffer(raidPtr, frow, rbuf);
			RF_UNLOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
		} else
			if (rbuf->type == RF_RBUF_TYPE_FORCED)
				rf_FreeReconBuffer(rbuf);
			else
				RF_ASSERT(0);
		break;

		/* A buffer-stall condition has been cleared. */
	case RF_REVENT_BUFCLEAR:
		Dprintf2("RECON: BUFCLEAR EVENT: row %d col %d.\n", frow,
		    event->col);
		submitblocked = rf_SubmitReconBuffer(raidPtr
		    ->reconControl[frow]->perDiskInfo[event->col].rbuf, 0,
		    (int) (long) event->arg);
		RF_ASSERT(!submitblocked);	/*
						 * We wouldn't have gotten the
						 * BUFCLEAR event if we
						 * couldn't submit.
						 */
		retcode = rf_IssueNextReadRequest(raidPtr, frow, event->col);
		break;

		/* A user-write reconstruction blockage has been cleared. */
	case RF_REVENT_BLOCKCLEAR:
		DDprintf2("RECON: BLOCKCLEAR EVENT: row %d col %d.\n",
		    frow, event->col);
		retcode = rf_TryToRead(raidPtr, frow, event->col);
		break;

		/*
		 * A max-head-separation reconstruction blockage has been
		 * cleared.
		 */
	case RF_REVENT_HEADSEPCLEAR:
		Dprintf2("RECON: HEADSEPCLEAR EVENT: row %d col %d.\n",
		    frow, event->col);
		retcode = rf_TryToRead(raidPtr, frow, event->col);
		break;

		/* A buffer has become ready to write. */
	case RF_REVENT_BUFREADY:
		Dprintf2("RECON: BUFREADY EVENT: row %d col %d.\n",
		    frow, event->col);
		retcode = rf_IssueNextWriteRequest(raidPtr, frow);
		if (rf_floatingRbufDebug) {
			rf_CheckFloatingRbufCount(raidPtr, 1);
		}
		break;

		/*
		 * We need to skip the current RU entirely because it got
		 * recon'd while we were waiting for something else to happen.
		 */
	case RF_REVENT_SKIP:
		DDprintf2("RECON: SKIP EVENT: row %d col %d.\n",
		    frow, event->col);
		retcode = rf_IssueNextReadRequest(raidPtr, frow, event->col);
		break;

		/*
		 * A forced-reconstruction read access has completed. Just
		 * submit the buffer.
		 */
	case RF_REVENT_FORCEDREADDONE:
		rbuf = (RF_ReconBuffer_t *) event->arg;
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) rbuf->arg);
		DDprintf2("RECON: FORCEDREADDONE EVENT: row %d col %d.\n",
		    frow, event->col);
		submitblocked = rf_SubmitReconBuffer(rbuf, 1, 0);
		RF_ASSERT(!submitblocked);
		break;

	default:
		RF_PANIC();
	}
	rf_FreeReconEventDesc(event);
	return (retcode);
}

/*****************************************************************************
 *
 * Find the next thing that's needed on the indicated disk, and issue
 * a read request for it. We assume that the reconstruction buffer
 * associated with this process is free to receive the data. If
 * reconstruction is blocked on the indicated RU, we issue a
 * blockage-release request instead of a physical disk read request.
 * If the current disk gets too far ahead of the others, we issue a
 * head-separation wait request and return.
 *
 * ctrl->{ru_count, curPSID, diskOffset} and
 * rbuf->failedDiskSectorOffset are maintained to point to the unit
 * we're currently accessing. Note that this deviates from the
 * standard C idiom of having counters point to the next thing to be
 * accessed. This allows us to easily retry when we're blocked by
 * head separation or reconstruction-blockage events.
 *
 * Returns nonzero if and only if there is nothing left unread on the
 * indicated disk.
 *
 *****************************************************************************/
int
rf_IssueNextReadRequest(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col)
{
	RF_PerDiskReconCtrl_t *ctrl =
	    &raidPtr->reconControl[row]->perDiskInfo[col];
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconBuffer_t *rbuf = ctrl->rbuf;
	RF_ReconUnitCount_t RUsPerPU =
	    layoutPtr->SUsPerPU / layoutPtr->SUsPerRU;
	RF_SectorCount_t sectorsPerRU =
	    layoutPtr->sectorsPerStripeUnit * layoutPtr->SUsPerRU;
	int do_new_check = 0, retcode = 0, status;

	/*
	 * If we are currently the slowest disk, mark that we have to do a new
	 * check.
	 */
	if (ctrl->headSepCounter <=
	    raidPtr->reconControl[row]->minHeadSepCounter)
		do_new_check = 1;

	while (1) {

		ctrl->ru_count++;
		if (ctrl->ru_count < RUsPerPU) {
			ctrl->diskOffset += sectorsPerRU;
			rbuf->failedDiskSectorOffset += sectorsPerRU;
		} else {
			ctrl->curPSID++;
			ctrl->ru_count = 0;
			/* code left over from when head-sep was based on
			 * parity stripe id */
			if (ctrl->curPSID >=
			    raidPtr->reconControl[row]->lastPSID) {
				rf_CheckForNewMinHeadSep(raidPtr, row,
				    ++(ctrl->headSepCounter));
				return (1);	/* Finito ! */
			}
			/*
			 * Find the disk offsets of the start of the parity
			 * stripe on both the current disk and the failed
			 * disk. Skip this entire parity stripe if either disk
			 * does not appear in the indicated PS.
			 */
			status = rf_ComputePSDiskOffsets(raidPtr,
			    ctrl->curPSID, row, col, &ctrl->diskOffset,
			    &rbuf->failedDiskSectorOffset, &rbuf->spRow,
			    &rbuf->spCol, &rbuf->spOffset);
			if (status) {
				ctrl->ru_count = RUsPerPU - 1;
				continue;
			}
		}
		rbuf->which_ru = ctrl->ru_count;

		/* Skip this RU if it's already been reconstructed. */
		if (rf_CheckRUReconstructed(raidPtr->reconControl[row]
		    ->reconMap, rbuf->failedDiskSectorOffset)) {
			Dprintf2("Skipping psid %ld ru %d: already"
			    " reconstructed.\n", ctrl->curPSID, ctrl->ru_count);
			continue;
		}
		break;
	}
	ctrl->headSepCounter++;
	if (do_new_check)	/* Update min if needed. */
		rf_CheckForNewMinHeadSep(raidPtr, row, ctrl->headSepCounter);


	/*
	 * At this point, we have definitely decided what to do, and we have
	 * only to see if we can actually do it now.
	 */
	rbuf->parityStripeID = ctrl->curPSID;
	rbuf->which_ru = ctrl->ru_count;
	bzero((char *) &raidPtr->recon_tracerecs[col],
	    sizeof(raidPtr->recon_tracerecs[col]));
	raidPtr->recon_tracerecs[col].reconacc = 1;
	RF_ETIMER_START(raidPtr->recon_tracerecs[col].recon_timer);
	retcode = rf_TryToRead(raidPtr, row, col);
	return (retcode);
}

/*
 * Tries to issue the next read on the indicated disk. We may be
 * blocked by (a) the heads being too far apart, or (b) recon on the
 * indicated RU being blocked due to a write by a user thread. In
 * this case, we issue a head-sep or blockage wait request, which will
 * cause this same routine to be invoked again later when the blockage
 * has cleared.
 */

int
rf_TryToRead(RF_Raid_t *raidPtr, RF_RowCol_t row, RF_RowCol_t col)
{
	RF_PerDiskReconCtrl_t *ctrl =
	    &raidPtr->reconControl[row]->perDiskInfo[col];
	RF_SectorCount_t sectorsPerRU =
	    raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.SUsPerRU;
	RF_StripeNum_t psid = ctrl->curPSID;
	RF_ReconUnitNum_t which_ru = ctrl->ru_count;
	RF_DiskQueueData_t *req;
	int status, created = 0;
	RF_ReconParityStripeStatus_t *pssPtr;

	/*
	 * If the current disk is too far ahead of the others, issue a
	 * head-separation wait and return.
	 */
	if (rf_CheckHeadSeparation(raidPtr, ctrl, row, col,
	    ctrl->headSepCounter, which_ru))
		return (0);
	RF_LOCK_PSS_MUTEX(raidPtr, row, psid);
	pssPtr = rf_LookupRUStatus(raidPtr, raidPtr->reconControl[row]
	    ->pssTable, psid, which_ru, RF_PSS_CREATE, &created);

	/*
	 * If recon is blocked on the indicated parity stripe, issue a
	 * block-wait request and return. This also must mark the indicated RU
	 * in the stripe as under reconstruction if not blocked.
	 */
	status = rf_CheckForcedOrBlockedReconstruction(raidPtr, pssPtr, ctrl,
	    row, col, psid, which_ru);
	if (status == RF_PSS_RECON_BLOCKED) {
		Dprintf2("RECON: Stalling psid %ld ru %d: recon blocked.\n",
		    psid, which_ru);
		goto out;
	} else
		if (status == RF_PSS_FORCED_ON_WRITE) {
			rf_CauseReconEvent(raidPtr, row, col, NULL,
			    RF_REVENT_SKIP);
			goto out;
		}
	/*
	 * Make one last check to be sure that the indicated RU didn't get
	 * reconstructed while we were waiting for something else to happen.
	 * This is unfortunate in that it causes us to make this check twice
	 * in the normal case. Might want to make some attempt to re-work
	 * this so that we only do this check if we've definitely blocked on
	 * one of the above checks. When this condition is detected, we may
	 * have just created a bogus status entry, which we need to delete.
	 */
	if (rf_CheckRUReconstructed(raidPtr->reconControl[row]->reconMap,
	    ctrl->rbuf->failedDiskSectorOffset)) {
		Dprintf2("RECON: Skipping psid %ld ru %d: prior recon after"
		    " stall.\n", psid, which_ru);
		if (created)
			rf_PSStatusDelete(raidPtr,
			    raidPtr->reconControl[row]->pssTable, pssPtr);
		rf_CauseReconEvent(raidPtr, row, col, NULL, RF_REVENT_SKIP);
		goto out;
	}
	/* Found something to read. Issue the I/O. */
	Dprintf5("RECON: Read for psid %ld on row %d col %d offset %ld"
	    " buf %lx.\n", psid, row, col, ctrl->diskOffset,
	    ctrl->rbuf->buffer);
	RF_ETIMER_STOP(raidPtr->recon_tracerecs[col].recon_timer);
	RF_ETIMER_EVAL(raidPtr->recon_tracerecs[col].recon_timer);
	raidPtr->recon_tracerecs[col].specific.recon.recon_start_to_fetch_us =
	    RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[col].recon_timer);
	RF_ETIMER_START(raidPtr->recon_tracerecs[col].recon_timer);

	/*
	 * Should be ok to use a NULL proc pointer here, all the bufs we use
	 * should be in kernel space.
	 */
	req = rf_CreateDiskQueueData(RF_IO_TYPE_READ, ctrl->diskOffset,
	    sectorsPerRU, ctrl->rbuf->buffer, psid, which_ru,
	    rf_ReconReadDoneProc, (void *) ctrl, NULL,
	    &raidPtr->recon_tracerecs[col], (void *) raidPtr, 0, NULL);

	RF_ASSERT(req);		/* XXX -- Fix this. -- XXX */

	ctrl->rbuf->arg = (void *) req;
	rf_DiskIOEnqueue(&raidPtr->Queues[row][col], req, RF_IO_RECON_PRIORITY);
	pssPtr->issued[col] = 1;

out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, row, psid);
	return (0);
}


/*
 * Given a parity stripe ID, we want to find out whether both the
 * current disk and the failed disk exist in that parity stripe. If
 * not, we want to skip this whole PS. If so, we want to find the
 * disk offset of the start of the PS on both the current disk and the
 * failed disk.
 *
 * This works by getting a list of disks comprising the indicated
 * parity stripe, and searching the list for the current and failed
 * disks. Once we've decided they both exist in the parity stripe, we
 * need to decide whether each is data or parity, so that we'll know
 * which mapping function to call to get the corresponding disk
 * offsets.
 *
 * This is kind of unpleasant, but doing it this way allows the
 * reconstruction code to use parity stripe IDs rather than physical
 * disks address to march through the failed disk, which greatly
 * simplifies a lot of code, as well as eliminating the need for a
 * reverse-mapping function. I also think it will execute faster,
 * since the calls to the mapping module are kept to a minimum.
 *
 * ASSUMES THAT THE STRIPE IDENTIFIER IDENTIFIES THE DISKS COMPRISING
 * THE STRIPE IN THE CORRECT ORDER.
 */

int
rf_ComputePSDiskOffsets(
    RF_Raid_t		*raidPtr,	/* RAID descriptor. */
    RF_StripeNum_t	 psid,		/* Parity stripe identifier. */
    RF_RowCol_t		 row,		/*
					 * Row and column of disk to find
					 * the offsets for.
					 */
    RF_RowCol_t		 col,
    RF_SectorNum_t	*outDiskOffset,
    RF_SectorNum_t	*outFailedDiskSectorOffset,
    RF_RowCol_t		*spRow,		/*
					 * OUT: Row,col of spare unit for
					 * failed unit.
					 */
    RF_RowCol_t		*spCol,
    RF_SectorNum_t	*spOffset	/*
					 * OUT: Offset into disk containing
					 * spare unit.
					 */
)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_RowCol_t fcol = raidPtr->reconControl[row]->fcol;
	RF_RaidAddr_t sosRaidAddress;	/* start-of-stripe */
	RF_RowCol_t *diskids;
	u_int i, j, k, i_offset, j_offset;
	RF_RowCol_t prow, pcol;
	int testcol, testrow;
	RF_RowCol_t stripe;
	RF_SectorNum_t poffset;
	char i_is_parity = 0, j_is_parity = 0;
	RF_RowCol_t stripeWidth =
	    layoutPtr->numDataCol + layoutPtr->numParityCol;

	/* Get a listing of the disks comprising that stripe. */
	sosRaidAddress = rf_ParityStripeIDToRaidAddress(layoutPtr, psid);
	(layoutPtr->map->IdentifyStripe) (raidPtr, sosRaidAddress, &diskids,
	    &stripe);
	RF_ASSERT(diskids);

	/*
	 * Reject this entire parity stripe if it does not contain the
	 * indicated disk or it does not contain the failed disk.
	 */
	if (row != stripe)
		goto skipit;
	for (i = 0; i < stripeWidth; i++) {
		if (col == diskids[i])
			break;
	}
	if (i == stripeWidth)
		goto skipit;
	for (j = 0; j < stripeWidth; j++) {
		if (fcol == diskids[j])
			break;
	}
	if (j == stripeWidth) {
		goto skipit;
	}
	/* Find out which disk the parity is on. */
	(layoutPtr->map->MapParity) (raidPtr, sosRaidAddress, &prow, &pcol,
	    &poffset, RF_DONT_REMAP);

	/* Find out if either the current RU or the failed RU is parity. */
	/*
	 * Also, if the parity occurs in this stripe prior to the data and/or
	 * failed col, we need to decrement i and/or j.
	 */
	for (k = 0; k < stripeWidth; k++)
		if (diskids[k] == pcol)
			break;
	RF_ASSERT(k < stripeWidth);
	i_offset = i;
	j_offset = j;
	if (k < i)
		i_offset--;
	else
		if (k == i) {
			i_is_parity = 1;
			i_offset = 0;
		}		/*
				 * Set offsets to zero to disable multiply
				 * below.
				 */
	if (k < j)
		j_offset--;
	else
		if (k == j) {
			j_is_parity = 1;
			j_offset = 0;
		}
	/*
	 * At this point, [ij]_is_parity tells us whether the [current,failed]
	 * disk is parity at the start of this RU, and, if data, "[ij]_offset"
	 * tells us how far into the stripe the [current,failed] disk is.
	 */

	/*
	 * Call the mapping routine to get the offset into the current disk,
	 * repeat for failed disk.
	 */
	if (i_is_parity)
		layoutPtr->map->MapParity(raidPtr, sosRaidAddress + i_offset *
		    layoutPtr->sectorsPerStripeUnit, &testrow, &testcol,
		    outDiskOffset, RF_DONT_REMAP);
	else
		layoutPtr->map->MapSector(raidPtr, sosRaidAddress + i_offset *
		    layoutPtr->sectorsPerStripeUnit, &testrow, &testcol,
		    outDiskOffset, RF_DONT_REMAP);

	RF_ASSERT(row == testrow && col == testcol);

	if (j_is_parity)
		layoutPtr->map->MapParity(raidPtr, sosRaidAddress + j_offset *
		    layoutPtr->sectorsPerStripeUnit, &testrow, &testcol,
		    outFailedDiskSectorOffset, RF_DONT_REMAP);
	else
		layoutPtr->map->MapSector(raidPtr, sosRaidAddress + j_offset *
		    layoutPtr->sectorsPerStripeUnit, &testrow, &testcol,
		    outFailedDiskSectorOffset, RF_DONT_REMAP);
	RF_ASSERT(row == testrow && fcol == testcol);

	/* Now locate the spare unit for the failed unit. */
	if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {
		if (j_is_parity)
			layoutPtr->map->MapParity(raidPtr, sosRaidAddress +
			    j_offset * layoutPtr->sectorsPerStripeUnit, spRow,
			    spCol, spOffset, RF_REMAP);
		else
			layoutPtr->map->MapSector(raidPtr, sosRaidAddress +
			    j_offset * layoutPtr->sectorsPerStripeUnit, spRow,
			    spCol, spOffset, RF_REMAP);
	} else {
		*spRow = raidPtr->reconControl[row]->spareRow;
		*spCol = raidPtr->reconControl[row]->spareCol;
		*spOffset = *outFailedDiskSectorOffset;
	}

	return (0);

skipit:
	Dprintf3("RECON: Skipping psid %ld: nothing needed from r%d c%d.\n",
	    psid, row, col);
	return (1);
}


/*
 * This is called when a buffer has become ready to write to the replacement
 * disk.
 */
int
rf_IssueNextWriteRequest(RF_Raid_t *raidPtr, RF_RowCol_t row)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_SectorCount_t sectorsPerRU =
	    layoutPtr->sectorsPerStripeUnit * layoutPtr->SUsPerRU;
	RF_RowCol_t fcol = raidPtr->reconControl[row]->fcol;
	RF_ReconBuffer_t *rbuf;
	RF_DiskQueueData_t *req;

	rbuf = rf_GetFullReconBuffer(raidPtr->reconControl[row]);
	RF_ASSERT(rbuf);	/*
				 * There must be one available, or we wouldn't
				 * have gotten the event that sent us here.
				 */
	RF_ASSERT(rbuf->pssPtr);

	rbuf->pssPtr->writeRbuf = rbuf;
	rbuf->pssPtr = NULL;

	Dprintf7("RECON: New write (r %d c %d offs %d) for psid %ld ru %d"
	    " (failed disk offset %ld) buf %lx.\n",
	    rbuf->spRow, rbuf->spCol, rbuf->spOffset, rbuf->parityStripeID,
	    rbuf->which_ru, rbuf->failedDiskSectorOffset, rbuf->buffer);
	Dprintf6("RECON: new write psid %ld   %02x %02x %02x %02x %02x.\n",
	    rbuf->parityStripeID, rbuf->buffer[0] & 0xff,
	    rbuf->buffer[1] & 0xff, rbuf->buffer[2] & 0xff,
	    rbuf->buffer[3] & 0xff, rbuf->buffer[4] & 0xff);

	/*
	 * Should be ok to use a NULL b_proc here b/c all addrs should be in
	 * kernel space.
	 */
	req = rf_CreateDiskQueueData(RF_IO_TYPE_WRITE, rbuf->spOffset,
	    sectorsPerRU, rbuf->buffer, rbuf->parityStripeID, rbuf->which_ru,
	    rf_ReconWriteDoneProc, (void *) rbuf, NULL,
	    &raidPtr->recon_tracerecs[fcol], (void *) raidPtr, 0, NULL);

	RF_ASSERT(req);		/* XXX -- Fix this. -- XXX */

	rbuf->arg = (void *) req;
	rf_DiskIOEnqueue(&raidPtr->Queues[rbuf->spRow][rbuf->spCol], req,
	    RF_IO_RECON_PRIORITY);

	return (0);
}

/*
 * This gets called upon the completion of a reconstruction read
 * operation. The arg is a pointer to the per-disk reconstruction
 * control structure for the process that just finished a read.
 *
 * Called at interrupt context in the kernel, so don't do anything
 * illegal here.
 */
int
rf_ReconReadDoneProc(void *arg, int status)
{
	RF_PerDiskReconCtrl_t *ctrl = (RF_PerDiskReconCtrl_t *) arg;
	RF_Raid_t *raidPtr = ctrl->reconCtrl->reconDesc->raidPtr;

	if (status) {
		/*
		 * XXX
		 */
		printf("Recon read failed !\n");
		RF_PANIC();
	}
	RF_ETIMER_STOP(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	RF_ETIMER_EVAL(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	raidPtr->recon_tracerecs[ctrl->col].specific.recon.
	   recon_fetch_to_return_us =
	     RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[ctrl->col].recon_timer);
	RF_ETIMER_START(raidPtr->recon_tracerecs[ctrl->col].recon_timer);

	rf_CauseReconEvent(raidPtr, ctrl->row, ctrl->col, NULL,
	    RF_REVENT_READDONE);
	return (0);
}


/*
 * This gets called upon the completion of a reconstruction write operation.
 * The arg is a pointer to the rbuf that was just written.
 *
 * Called at interrupt context in the kernel, so don't do anything illegal here.
 */
int
rf_ReconWriteDoneProc(void *arg, int status)
{
	RF_ReconBuffer_t *rbuf = (RF_ReconBuffer_t *) arg;

	Dprintf2("Reconstruction completed on psid %ld ru %d.\n",
	    rbuf->parityStripeID, rbuf->which_ru);
	if (status) {
		/* fprintf(stderr, "Recon write failed !\n"); */
		printf("Recon write failed !\n");
		RF_PANIC();
	}
	rf_CauseReconEvent((RF_Raid_t *) rbuf->raidPtr, rbuf->row, rbuf->col,
	    arg, RF_REVENT_WRITEDONE);
	return (0);
}


/*
 * Computes a new minimum head sep, and wakes up anyone who needs to
 * be woken as a result.
 */
void
rf_CheckForNewMinHeadSep(RF_Raid_t *raidPtr, RF_RowCol_t row,
    RF_HeadSepLimit_t hsCtr)
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl[row];
	RF_HeadSepLimit_t new_min;
	RF_RowCol_t i;
	RF_CallbackDesc_t *p;
	/* From the definition of a minimum. */
	RF_ASSERT(hsCtr >= reconCtrlPtr->minHeadSepCounter);


	RF_LOCK_MUTEX(reconCtrlPtr->rb_mutex);

	new_min = ~(1L << (8 * sizeof(long) - 1));	/* 0x7FFF....FFF */
	for (i = 0; i < raidPtr->numCol; i++)
		if (i != reconCtrlPtr->fcol) {
			if (reconCtrlPtr->perDiskInfo[i].headSepCounter <
			    new_min)
				new_min =
				    reconCtrlPtr->perDiskInfo[i].headSepCounter;
		}
	/* Set the new minimum and wake up anyone who can now run again. */
	if (new_min != reconCtrlPtr->minHeadSepCounter) {
		reconCtrlPtr->minHeadSepCounter = new_min;
		Dprintf1("RECON:  new min head pos counter val is %ld.\n",
		    new_min);
		while (reconCtrlPtr->headSepCBList) {
			if (reconCtrlPtr->headSepCBList->callbackArg.v >
			    new_min)
				break;
			p = reconCtrlPtr->headSepCBList;
			reconCtrlPtr->headSepCBList = p->next;
			p->next = NULL;
			rf_CauseReconEvent(raidPtr, p->row, p->col, NULL,
			    RF_REVENT_HEADSEPCLEAR);
			rf_FreeCallbackDesc(p);
		}

	}
	RF_UNLOCK_MUTEX(reconCtrlPtr->rb_mutex);
}

/*
 * Checks to see that the maximum head separation will not be violated
 * if we initiate a reconstruction I/O on the indicated disk.
 * Limiting the maximum head separation between two disks eliminates
 * the nasty buffer-stall conditions that occur when one disk races
 * ahead of the others and consumes all of the floating recon buffers.
 * This code is complex and unpleasant but it's necessary to avoid
 * some very nasty, albeit fairly rare, reconstruction behavior.
 *
 * Returns non-zero if and only if we have to stop working on the
 * indicated disk due to a head-separation delay.
 */
int
rf_CheckHeadSeparation(
    RF_Raid_t			*raidPtr,
    RF_PerDiskReconCtrl_t	*ctrl,
    RF_RowCol_t			 row,
    RF_RowCol_t			 col,
    RF_HeadSepLimit_t		 hsCtr,
    RF_ReconUnitNum_t		 which_ru
)
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl[row];
	RF_CallbackDesc_t *cb, *p, *pt;
	int retval = 0;

	/*
	 * If we're too far ahead of the slowest disk, stop working on this
	 * disk until the slower ones catch up. We do this by scheduling a
	 * wakeup callback for the time when the slowest disk has caught up.
	 * We define "caught up" with 20% hysteresis, i.e. the head separation
	 * must have fallen to at most 80% of the max allowable head
	 * separation before we'll wake up.
	 */
	RF_LOCK_MUTEX(reconCtrlPtr->rb_mutex);
	if ((raidPtr->headSepLimit >= 0) &&
	    ((ctrl->headSepCounter - reconCtrlPtr->minHeadSepCounter) >
	     raidPtr->headSepLimit)) {
		Dprintf6("raid%d: RECON: head sep stall: row %d col %d hsCtr"
		    " %ld minHSCtr %ld limit %ld.\n",
		    raidPtr->raidid, row, col, ctrl->headSepCounter,
		    reconCtrlPtr->minHeadSepCounter, raidPtr->headSepLimit);
		cb = rf_AllocCallbackDesc();
		/*
		 * The minHeadSepCounter value we have to get to before we'll
		 * wake up. Build in 20% hysteresis.
		 */
		cb->callbackArg.v = (ctrl->headSepCounter -
		    raidPtr->headSepLimit + raidPtr->headSepLimit / 5);
		cb->row = row;
		cb->col = col;
		cb->next = NULL;

		/*
		 * Insert this callback descriptor into the sorted list of
		 * pending head-sep callbacks.
		 */
		p = reconCtrlPtr->headSepCBList;
		if (!p)
			reconCtrlPtr->headSepCBList = cb;
		else
			if (cb->callbackArg.v < p->callbackArg.v) {
				cb->next = reconCtrlPtr->headSepCBList;
				reconCtrlPtr->headSepCBList = cb;
			} else {
				for (pt = p, p = p->next;
				    p && (p->callbackArg.v < cb->callbackArg.v);
				    pt = p, p = p->next);
				cb->next = p;
				pt->next = cb;
			}
		retval = 1;
#if	RF_RECON_STATS > 0
		ctrl->reconCtrl->reconDesc->hsStallCount++;
#endif	/* RF_RECON_STATS > 0 */
	}
	RF_UNLOCK_MUTEX(reconCtrlPtr->rb_mutex);

	return (retval);
}



/*
 * Checks to see if reconstruction has been either forced or blocked
 * by a user operation. If forced, we skip this RU entirely. Else if
 * blocked, put ourselves on the wait list. Else return 0.
 *
 * ASSUMES THE PSS MUTEX IS LOCKED UPON ENTRY.
 */
int
rf_CheckForcedOrBlockedReconstruction(
    RF_Raid_t			 *raidPtr,
    RF_ReconParityStripeStatus_t *pssPtr,
    RF_PerDiskReconCtrl_t	 *ctrl,
    RF_RowCol_t			  row,
    RF_RowCol_t			  col,
    RF_StripeNum_t		  psid,
    RF_ReconUnitNum_t		  which_ru
)
{
	RF_CallbackDesc_t *cb;
	int retcode = 0;

	if ((pssPtr->flags & RF_PSS_FORCED_ON_READ) ||
	    (pssPtr->flags & RF_PSS_FORCED_ON_WRITE))
		retcode = RF_PSS_FORCED_ON_WRITE;
	else
		if (pssPtr->flags & RF_PSS_RECON_BLOCKED) {
			Dprintf4("RECON: row %d col %d blocked at psid %ld"
			    " ru %d.\n", row, col, psid, which_ru);
			cb = rf_AllocCallbackDesc();	/*
							 * Append ourselves to
							 * the blockage-wait
							 * list.
							 */
			cb->row = row;
			cb->col = col;
			cb->next = pssPtr->blockWaitList;
			pssPtr->blockWaitList = cb;
			retcode = RF_PSS_RECON_BLOCKED;
		}
	if (!retcode)
		pssPtr->flags |= RF_PSS_UNDER_RECON;	/*
							 * Mark this RU as under
							 * reconstruction.
							 */

	return (retcode);
}


/*
 * If reconstruction is currently ongoing for the indicated stripeID,
 * reconstruction is forced to completion and we return non-zero to
 * indicate that the caller must wait. If not, then reconstruction is
 * blocked on the indicated stripe and the routine returns zero. If
 * and only if we return non-zero, we'll cause the cbFunc to get
 * invoked with the cbArg when the reconstruction has completed.
 */
int
rf_ForceOrBlockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap,
	void (*cbFunc) (RF_Raid_t *, void *), void *cbArg)
{
	RF_RowCol_t row = asmap->physInfo->row;	/*
						 * Which row of the array
						 * we're working on.
						 */
	RF_StripeNum_t stripeID = asmap->stripeID;	/*
							 * The stripe ID we're
							 * forcing recon on.
							 */
	RF_SectorCount_t sectorsPerRU = raidPtr->Layout.sectorsPerStripeUnit *
	    raidPtr->Layout.SUsPerRU;		/* Num sects in one RU. */
	RF_ReconParityStripeStatus_t *pssPtr;	/*
						 * A pointer to the parity
						 * stripe status structure.
						 */
	RF_StripeNum_t psid;			/* Parity stripe id. */
	RF_SectorNum_t offset, fd_offset;	/*
						 * Disk offset, failed-disk
						 * offset.
						 */
	RF_RowCol_t *diskids;
	RF_RowCol_t stripe;
	RF_ReconUnitNum_t which_ru;	/* RU within parity stripe. */
	RF_RowCol_t fcol, diskno, i;
	RF_ReconBuffer_t *new_rbuf;	/* Ptr to newly allocated rbufs. */
	RF_DiskQueueData_t *req;	/* Disk I/O req to be enqueued. */
	RF_CallbackDesc_t *cb;
	int created = 0, nPromoted;

	psid = rf_MapStripeIDToParityStripeID(&raidPtr->Layout, stripeID,
	    &which_ru);

	RF_LOCK_PSS_MUTEX(raidPtr, row, psid);

	pssPtr = rf_LookupRUStatus(raidPtr,
	    raidPtr->reconControl[row]->pssTable, psid, which_ru,
	    RF_PSS_CREATE | RF_PSS_RECON_BLOCKED, &created);

	/* If recon is not ongoing on this PS, just return. */
	if (!(pssPtr->flags & RF_PSS_UNDER_RECON)) {
		RF_UNLOCK_PSS_MUTEX(raidPtr, row, psid);
		return (0);
	}
	/*
	 * Otherwise, we have to wait for reconstruction to complete on this
	 * RU.
	 */
	/*
	 * In order to avoid waiting for a potentially large number of
	 * low-priority accesses to complete, we force a normal-priority (i.e.
	 * not low-priority) reconstruction on this RU.
	 */
	if (!(pssPtr->flags & RF_PSS_FORCED_ON_WRITE) &&
	    !(pssPtr->flags & RF_PSS_FORCED_ON_READ)) {
		DDprintf1("Forcing recon on psid %ld.\n", psid);
		/* Mark this RU as under forced recon. */
		pssPtr->flags |= RF_PSS_FORCED_ON_WRITE;
		/* Clear the blockage that we just set. */
		pssPtr->flags &= ~RF_PSS_RECON_BLOCKED;
		fcol = raidPtr->reconControl[row]->fcol;

		/*
		 * Get a listing of the disks comprising the indicated stripe.
		 */
		(raidPtr->Layout.map->IdentifyStripe) (raidPtr,
		    asmap->raidAddress, &diskids, &stripe);
		RF_ASSERT(row == stripe);

		/*
		 * For previously issued reads, elevate them to normal
		 * priority. If the I/O has already completed, it won't be
		 * found in the queue, and hence this will be a no-op. For
		 * unissued reads, allocate buffers and issue new reads. The
		 * fact that we've set the FORCED bit means that the regular
		 * recon procs will not re-issue these reqs.
		 */
		for (i = 0; i < raidPtr->Layout.numDataCol +
		    raidPtr->Layout.numParityCol; i++)
			if ((diskno = diskids[i]) != fcol) {
				if (pssPtr->issued[diskno]) {
					nPromoted = rf_DiskIOPromote(&raidPtr
					    ->Queues[row][diskno], psid,
					    which_ru);
					if (rf_reconDebug && nPromoted)
						printf("raid%d: promoted read"
						    " from row %d col %d.\n",
						    raidPtr->raidid, row,
						    diskno);
				} else {
					/* Create new buf. */
					new_rbuf = rf_MakeReconBuffer(raidPtr,
					    row, diskno, RF_RBUF_TYPE_FORCED);
					/* Find offsets & spare locationp */
					rf_ComputePSDiskOffsets(raidPtr, psid,
					    row, diskno, &offset, &fd_offset,
					    &new_rbuf->spRow, &new_rbuf->spCol,
					    &new_rbuf->spOffset);
					new_rbuf->parityStripeID = psid;
					/* Fill in the buffer. */
					new_rbuf->which_ru = which_ru;
					new_rbuf->failedDiskSectorOffset =
					    fd_offset;
					new_rbuf->priority =
					    RF_IO_NORMAL_PRIORITY;

					/*
					 * Use NULL b_proc b/c all addrs
					 * should be in kernel space.
					 */
					req = rf_CreateDiskQueueData(
					    RF_IO_TYPE_READ, offset +
					    which_ru * sectorsPerRU,
					    sectorsPerRU, new_rbuf->buffer,
					    psid, which_ru, (int (*)
					    (void *, int))
					      rf_ForceReconReadDoneProc,
					    (void *) new_rbuf, NULL,
					    NULL, (void *) raidPtr, 0, NULL);

					RF_ASSERT(req);	/*
							 * XXX -- Fix this. --
							 * XXX
							 */

					new_rbuf->arg = req;
					/* Enqueue the I/O. */
					rf_DiskIOEnqueue(&raidPtr
					    ->Queues[row][diskno], req,
					    RF_IO_NORMAL_PRIORITY);
					Dprintf3("raid%d: Issued new read req"
					    " on row %d col %d.\n",
					    raidPtr->raidid, row, diskno);
				}
			}
		/*
		 * If the write is sitting in the disk queue, elevate its
		 * priority.
		 */
		if (rf_DiskIOPromote(&raidPtr->Queues[row][fcol],
		    psid, which_ru))
			printf("raid%d: promoted write to row %d col %d.\n",
			    raidPtr->raidid, row, fcol);
	}
	/*
	 * Install a callback descriptor to be invoked when recon completes on
	 * this parity stripe.
	 */
	cb = rf_AllocCallbackDesc();
	/*
	 * XXX The following is bogus... These functions don't really match !!!
	 * GO
	 */
	cb->callbackFunc = (void (*) (RF_CBParam_t)) cbFunc;
	cb->callbackArg.p = (void *) cbArg;
	cb->next = pssPtr->procWaitList;
	pssPtr->procWaitList = cb;
	DDprintf2("raid%d: Waiting for forced recon on psid %ld.\n",
	    raidPtr->raidid, psid);

	RF_UNLOCK_PSS_MUTEX(raidPtr, row, psid);
	return (1);
}


/*
 * Called upon the completion of a forced reconstruction read.
 * All we do is schedule the FORCEDREADONE event.
 * Called at interrupt context in the kernel, so don't do anything illegal here.
 */
void
rf_ForceReconReadDoneProc(void *arg, int status)
{
	RF_ReconBuffer_t *rbuf = arg;

	if (status) {
		/* fprintf(stderr, "Forced recon read failed !\n"); */
		printf("Forced recon read failed !\n");
		RF_PANIC();
	}
	rf_CauseReconEvent((RF_Raid_t *) rbuf->raidPtr, rbuf->row, rbuf->col,
	    (void *) rbuf, RF_REVENT_FORCEDREADDONE);
}


/* Releases a block on the reconstruction of the indicated stripe. */
int
rf_UnblockRecon(RF_Raid_t *raidPtr, RF_AccessStripeMap_t *asmap)
{
	RF_RowCol_t row = asmap->origRow;
	RF_StripeNum_t stripeID = asmap->stripeID;
	RF_ReconParityStripeStatus_t *pssPtr;
	RF_ReconUnitNum_t which_ru;
	RF_StripeNum_t psid;
	int created = 0;
	RF_CallbackDesc_t *cb;

	psid = rf_MapStripeIDToParityStripeID(&raidPtr->Layout, stripeID,
	    &which_ru);
	RF_LOCK_PSS_MUTEX(raidPtr, row, psid);
	pssPtr = rf_LookupRUStatus(raidPtr, raidPtr->reconControl[row]
	    ->pssTable, psid, which_ru, RF_PSS_NONE, &created);

	/*
	 * When recon is forced, the pss desc can get deleted before we get
	 * back to unblock recon. But, this can _only_ happen when recon is
	 * forced. It would be good to put some kind of sanity check here, but
	 * how to decide if recon was just forced or not ?
	 */
	if (!pssPtr) {
		/*
		 * printf("Warning: no pss descriptor upon unblock on psid %ld"
		 *     " RU %d.\n", psid, which_ru);
		 */
		if (rf_reconDebug || rf_pssDebug)
			printf("Warning: no pss descriptor upon unblock on"
			    " psid %ld RU %d.\n", (long) psid, which_ru);
		goto out;
	}
	pssPtr->blockCount--;
	Dprintf3("raid%d: unblocking recon on psid %ld: blockcount is %d.\n",
	    raidPtr->raidid, psid, pssPtr->blockCount);
	if (pssPtr->blockCount == 0) {
		/* If recon blockage has been released. */

		/*
		 * Unblock recon before calling CauseReconEvent in case
		 * CauseReconEvent causes us to try to issue a new read before
		 * returning here.
		 */
		pssPtr->flags &= ~RF_PSS_RECON_BLOCKED;


		while (pssPtr->blockWaitList) {
			/*
			 * Spin through the block-wait list and
			 * release all the waiters.
			 */
			cb = pssPtr->blockWaitList;
			pssPtr->blockWaitList = cb->next;
			cb->next = NULL;
			rf_CauseReconEvent(raidPtr, cb->row, cb->col, NULL,
			    RF_REVENT_BLOCKCLEAR);
			rf_FreeCallbackDesc(cb);
		}
		if (!(pssPtr->flags & RF_PSS_UNDER_RECON)) {
			/* If no recon was requested while recon was blocked. */
			rf_PSStatusDelete(raidPtr, raidPtr->reconControl[row]
			    ->pssTable, pssPtr);
		}
	}
out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, row, psid);
	return (0);
}
