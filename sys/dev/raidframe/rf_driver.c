/*	$OpenBSD: rf_driver.c,v 1.11 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_driver.c,v 1.37 2000/06/04 02:05:13 oster Exp $	*/

/*
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author:	Mark Holland, Khalil Amiri, Claudson Bornstein,
 *		William V. Courtright II, Robby Findler, Daniel Stodolsky,
 *		Rachad Youssef, Jim Zelenka
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

/*****************************************************************************
 *
 * rf_driver.c -- Main setup, teardown, and access routines for the RAID
 *		  driver
 *
 * All routines are prefixed with rf_ (RAIDframe), to avoid conficts.
 *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#ifdef	__NetBSD__
#include <sys/vnode.h>
#endif


#include "rf_archs.h"
#include "rf_threadstuff.h"


#include <sys/errno.h>

#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_aselect.h"
#include "rf_diskqueue.h"
#include "rf_parityscan.h"
#include "rf_alloclist.h"
#include "rf_dagutils.h"
#include "rf_utils.h"
#include "rf_etimer.h"
#include "rf_acctrace.h"
#include "rf_configure.h"
#include "rf_general.h"
#include "rf_desc.h"
#include "rf_states.h"
#include "rf_freelist.h"
#include "rf_decluster.h"
#include "rf_map.h"
#include "rf_revent.h"
#include "rf_callback.h"
#include "rf_engine.h"
#include "rf_memchunk.h"
#include "rf_mcpair.h"
#include "rf_nwayxor.h"
#include "rf_debugprint.h"
#include "rf_copyback.h"
#include "rf_driver.h"
#include "rf_options.h"
#include "rf_shutdown.h"
#include "rf_kintf.h"

#include <sys/buf.h>

/* rad == RF_RaidAccessDesc_t */
static RF_FreeList_t *rf_rad_freelist;
#define	RF_MAX_FREE_RAD		128
#define	RF_RAD_INC		 16
#define	RF_RAD_INITIAL		 32

/* Debug variables. */
char	rf_panicbuf[2048];	/*
				 * A buffer to hold an error msg when we panic.
				 */

/* Main configuration routines. */
static int raidframe_booted = 0;

void rf_ConfigureDebug(RF_Config_t *);
void rf_set_debug_option(char *, long);
void rf_UnconfigureArray(void);
int  rf_init_rad(RF_RaidAccessDesc_t *);
void rf_clean_rad(RF_RaidAccessDesc_t *);
void rf_ShutdownRDFreeList(void *);
int  rf_ConfigureRDFreeList(RF_ShutdownList_t **);

RF_DECLARE_MUTEX(rf_printf_mutex);	/*
					 * Debug only: Avoids interleaved
					 * printfs by different stripes.
					 */

#define	SIGNAL_QUIESCENT_COND(_raid_)	wakeup(&((_raid_)->accesses_suspended))
#define	WAIT_FOR_QUIESCENCE(_raid_)					\
	tsleep(&((_raid_)->accesses_suspended), PRIBIO, "RAIDframe quiesce", 0);

#define	IO_BUF_ERR(bp, err)						\
do {									\
	bp->b_flags |= B_ERROR;						\
	bp->b_resid = bp->b_bcount;					\
	bp->b_error = err;						\
	biodone(bp);							\
} while (0)

static int configureCount = 0;	/* Number of active configurations. */
static int isconfigged = 0;	/*
				 * Is basic RAIDframe (non per-array)
				 * stuff configured ?
				 */
RF_DECLARE_STATIC_MUTEX(configureMutex);	/*
						 * Used to lock the
						 * configuration stuff.
						 */
static RF_ShutdownList_t *globalShutdown;	/* Non array-specific stuff. */
int  rf_ConfigureRDFreeList(RF_ShutdownList_t **);


/* Called at system boot time. */
int
rf_BootRaidframe(void)
{
	int rc;

	if (raidframe_booted)
		return (EBUSY);
	raidframe_booted = 1;

	rc = rf_mutex_init(&configureMutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d.\n",
		    __FILE__, __LINE__, rc);
		RF_PANIC();
	}
	configureCount = 0;
	isconfigged = 0;
	globalShutdown = NULL;
	return (0);
}


/*
 * This function is really just for debugging user-level stuff: It
 * frees up all memory, other RAIDframe resources that might otherwise
 * be kept around. This is used with systems like "sentinel" to detect
 * memory leaks.
 */
int
rf_UnbootRaidframe(void)
{
	int rc;

	RF_LOCK_MUTEX(configureMutex);
	if (configureCount) {
		RF_UNLOCK_MUTEX(configureMutex);
		return (EBUSY);
	}
	raidframe_booted = 0;
	RF_UNLOCK_MUTEX(configureMutex);
	rc = rf_mutex_destroy(&configureMutex);
	if (rc) {
		RF_ERRORMSG3("Unable to destroy mutex file %s line %d"
		    " rc=%d.\n", __FILE__, __LINE__, rc);
		RF_PANIC();
	}
	return (0);
}


/*
 * Called whenever an array is shutdown.
 */
void
rf_UnconfigureArray(void)
{
	int rc;

	RF_LOCK_MUTEX(configureMutex);
	if (--configureCount == 0) {	/*
					 * If no active configurations, shut
					 * everything down.
					 */
		isconfigged = 0;

		rc = rf_ShutdownList(&globalShutdown);
		if (rc) {
			RF_ERRORMSG1("RAIDFRAME: unable to do global shutdown,"
			    " rc=%d.\n", rc);
		}

		/*
		 * We must wait until now, because the AllocList module
		 * uses the DebugMem module.
		 */
		if (rf_memDebug)
			rf_print_unfreed();
	}
	RF_UNLOCK_MUTEX(configureMutex);
}


/*
 * Called to shut down an array.
 */
int
rf_Shutdown(RF_Raid_t *raidPtr)
{
	if (!raidPtr->valid) {
		RF_ERRORMSG("Attempt to shut down unconfigured RAIDframe"
		    " driver. Aborting shutdown.\n");
		return (EINVAL);
	}
	/*
	 * Wait for outstanding IOs to land.
	 * As described in rf_raid.h, we use the rad_freelist lock
	 * to protect the per-array info about outstanding descs,
	 * since we need to do freelist locking anyway, and this
	 * cuts down on the amount of serialization we've got going
	 * on.
	 */
	RF_FREELIST_DO_LOCK(rf_rad_freelist);
	if (raidPtr->waitShutdown) {
		RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
		return (EBUSY);
	}
	raidPtr->waitShutdown = 1;
	while (raidPtr->nAccOutstanding) {
		RF_WAIT_COND(raidPtr->outstandingCond, RF_FREELIST_MUTEX_OF(rf_rad_freelist));
	}
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);

	/* Wait for any parity re-writes to stop... */
	while (raidPtr->parity_rewrite_in_progress) {
		printf("Waiting for parity re-write to exit...\n");
		tsleep(&raidPtr->parity_rewrite_in_progress, PRIBIO,
		       "rfprwshutdown", 0);
	}

	raidPtr->valid = 0;

	rf_update_component_labels(raidPtr, RF_FINAL_COMPONENT_UPDATE);

	rf_UnconfigureVnodes(raidPtr);

	rf_ShutdownList(&raidPtr->shutdownList);

	rf_UnconfigureArray();

	return (0);
}

#define	DO_INIT_CONFIGURE(f)						\
do {									\
	rc = f (&globalShutdown);					\
	if (rc) {							\
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d.\n",		\
		    RF_STRING(f), rc);					\
		rf_ShutdownList(&globalShutdown);			\
		configureCount--;					\
		RF_UNLOCK_MUTEX(configureMutex);			\
		return(rc);						\
	}								\
} while (0)

#define	DO_RAID_FAIL()							\
do {									\
	rf_UnconfigureVnodes(raidPtr);					\
	rf_ShutdownList(&raidPtr->shutdownList);			\
	rf_UnconfigureArray();						\
} while (0)

#define	DO_RAID_INIT_CONFIGURE(f)					\
do {									\
	rc = (f)(&raidPtr->shutdownList, raidPtr, cfgPtr);		\
	if (rc) {							\
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d.\n",		\
		    RF_STRING(f), rc);					\
		DO_RAID_FAIL();						\
		return(rc);						\
	}								\
} while (0)

#define	DO_RAID_MUTEX(_m_)						\
do {									\
	rc = rf_create_managed_mutex(&raidPtr->shutdownList, (_m_));	\
	if (rc) {							\
		RF_ERRORMSG3("Unable to init mutex file %s line %d"	\
		    " rc=%d.\n", __FILE__, __LINE__, rc);		\
		DO_RAID_FAIL();						\
		return(rc);						\
	}								\
} while (0)

#define	DO_RAID_COND(_c_)						\
do {									\
	rc = rf_create_managed_cond(&raidPtr->shutdownList, (_c_));	\
	if (rc) {							\
		RF_ERRORMSG3("Unable to init cond file %s line %d"	\
		    " rc=%d.\n", __FILE__, __LINE__, rc);		\
		DO_RAID_FAIL();						\
		return(rc);						\
	}								\
} while (0)

int
rf_Configure(RF_Raid_t *raidPtr, RF_Config_t *cfgPtr, RF_AutoConfig_t *ac)
{
	RF_RowCol_t row, col;
	int i, rc;

	/*
	 * XXX This check can probably be removed now, since
	 * RAIDFRAME_CONFIGURE now checks to make sure that the
	 * RAID set is not already valid.
	 */
	if (raidPtr->valid) {
		RF_ERRORMSG("RAIDframe configuration not shut down."
		    " Aborting configure.\n");
		return (EINVAL);
	}
	RF_LOCK_MUTEX(configureMutex);
	configureCount++;
	if (isconfigged == 0) {
		rc = rf_create_managed_mutex(&globalShutdown, &rf_printf_mutex);
		if (rc) {
			RF_ERRORMSG3("Unable to init mutex file %s line %d"
			    " rc=%d.\n", __FILE__, __LINE__, rc);
			rf_ShutdownList(&globalShutdown);
			return (rc);
		}
		/* Initialize globals. */
#ifdef	RAIDDEBUG
		printf("RAIDFRAME: protectedSectors is %ld.\n",
		       rf_protectedSectors);
#endif	/* RAIDDEBUG */

		rf_clear_debug_print_buffer();

		DO_INIT_CONFIGURE(rf_ConfigureAllocList);

		/*
		 * Yes, this does make debugging general to the whole
		 * system instead of being array specific. Bummer, drag.
		 */
		rf_ConfigureDebug(cfgPtr);
		DO_INIT_CONFIGURE(rf_ConfigureDebugMem);
		DO_INIT_CONFIGURE(rf_ConfigureAccessTrace);
		DO_INIT_CONFIGURE(rf_ConfigureMapModule);
		DO_INIT_CONFIGURE(rf_ConfigureReconEvent);
		DO_INIT_CONFIGURE(rf_ConfigureCallback);
		DO_INIT_CONFIGURE(rf_ConfigureMemChunk);
		DO_INIT_CONFIGURE(rf_ConfigureRDFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureNWayXor);
		DO_INIT_CONFIGURE(rf_ConfigureStripeLockFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureMCPair);
		DO_INIT_CONFIGURE(rf_ConfigureDAGs);
		DO_INIT_CONFIGURE(rf_ConfigureDAGFuncs);
		DO_INIT_CONFIGURE(rf_ConfigureDebugPrint);
		DO_INIT_CONFIGURE(rf_ConfigureReconstruction);
		DO_INIT_CONFIGURE(rf_ConfigureCopyback);
		DO_INIT_CONFIGURE(rf_ConfigureDiskQueueSystem);
		isconfigged = 1;
	}
	RF_UNLOCK_MUTEX(configureMutex);

	DO_RAID_MUTEX(&raidPtr->mutex);
	/*
	 * Set up the cleanup list. Do this after ConfigureDebug so that
	 * value of memDebug will be set.
	 */

	rf_MakeAllocList(raidPtr->cleanupList);
	if (raidPtr->cleanupList == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(&raidPtr->shutdownList,
	    (void (*) (void *)) rf_FreeAllocList, raidPtr->cleanupList);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d"
		    " rc=%d.\n", __FILE__, __LINE__, rc);
		DO_RAID_FAIL();
		return (rc);
	}
	raidPtr->numRow = cfgPtr->numRow;
	raidPtr->numCol = cfgPtr->numCol;
	raidPtr->numSpare = cfgPtr->numSpare;

	/*
	 * XXX We don't even pretend to support more than one row in the
	 * kernel...
	 */
	if (raidPtr->numRow != 1) {
		RF_ERRORMSG("Only one row supported in kernel.\n");
		DO_RAID_FAIL();
		return (EINVAL);
	}
	RF_CallocAndAdd(raidPtr->status, raidPtr->numRow,
	    sizeof(RF_RowStatus_t), (RF_RowStatus_t *), raidPtr->cleanupList);
	if (raidPtr->status == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	RF_CallocAndAdd(raidPtr->reconControl, raidPtr->numRow,
	    sizeof(RF_ReconCtrl_t *), (RF_ReconCtrl_t **), raidPtr->cleanupList);
	if (raidPtr->reconControl == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	for (i = 0; i < raidPtr->numRow; i++) {
		raidPtr->status[i] = rf_rs_optimal;
		raidPtr->reconControl[i] = NULL;
	}

	DO_RAID_INIT_CONFIGURE(rf_ConfigureEngine);
	DO_RAID_INIT_CONFIGURE(rf_ConfigureStripeLocks);

	DO_RAID_COND(&raidPtr->outstandingCond);

	raidPtr->nAccOutstanding = 0;
	raidPtr->waitShutdown = 0;

	DO_RAID_MUTEX(&raidPtr->access_suspend_mutex);
	DO_RAID_COND(&raidPtr->quiescent_cond);

	DO_RAID_COND(&raidPtr->waitForReconCond);

	DO_RAID_MUTEX(&raidPtr->recon_done_proc_mutex);

	if (ac != NULL) {
		/*
		 * We have an AutoConfig structure... Don't do the
		 * normal disk configuration... call the auto config
		 * stuff.
		 */
		rf_AutoConfigureDisks(raidPtr, cfgPtr, ac);
	} else {
		DO_RAID_INIT_CONFIGURE(rf_ConfigureDisks);
		DO_RAID_INIT_CONFIGURE(rf_ConfigureSpareDisks);
	}
	/*
	 * Do this after ConfigureDisks & ConfigureSpareDisks to be sure
	 * devno is set.
	 */
	DO_RAID_INIT_CONFIGURE(rf_ConfigureDiskQueues);

	DO_RAID_INIT_CONFIGURE(rf_ConfigureLayout);

	DO_RAID_INIT_CONFIGURE(rf_ConfigurePSStatus);

	for (row = 0; row < raidPtr->numRow; row++) {
		for (col = 0; col < raidPtr->numCol; col++) {
			/*
			 * XXX Better distribution.
			 */
			raidPtr->hist_diskreq[row][col] = 0;
		}
	}

	raidPtr->numNewFailures = 0;
	raidPtr->copyback_in_progress = 0;
	raidPtr->parity_rewrite_in_progress = 0;
	raidPtr->recon_in_progress = 0;
	raidPtr->maxOutstanding = cfgPtr->maxOutstandingDiskReqs;

	/*
	 * Autoconfigure and root_partition will actually get filled in
	 * after the config is done.
	 */
	raidPtr->autoconfigure = 0;
	raidPtr->root_partition = 0;
	raidPtr->last_unit = raidPtr->raidid;
	raidPtr->config_order = 0;

	if (rf_keepAccTotals) {
		raidPtr->keep_acc_totals = 1;
	}
	rf_StartUserStats(raidPtr);

	raidPtr->valid = 1;
	return (0);
}

int
rf_init_rad(RF_RaidAccessDesc_t *desc)
{
	int rc;

	rc = rf_mutex_init(&desc->mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d.\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	rc = rf_cond_init(&desc->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d.\n", __FILE__,
		    __LINE__, rc);
		rf_mutex_destroy(&desc->mutex);
		return (rc);
	}
	return (0);
}

void
rf_clean_rad(RF_RaidAccessDesc_t *desc)
{
	rf_mutex_destroy(&desc->mutex);
	rf_cond_destroy(&desc->cond);
}

void
rf_ShutdownRDFreeList(void *ignored)
{
	RF_FREELIST_DESTROY_CLEAN(rf_rad_freelist, next,
	    (RF_RaidAccessDesc_t *), rf_clean_rad);
}

int
rf_ConfigureRDFreeList(RF_ShutdownList_t **listp)
{
	int rc;

	RF_FREELIST_CREATE(rf_rad_freelist, RF_MAX_FREE_RAD,
	    RF_RAD_INC, sizeof(RF_RaidAccessDesc_t));
	if (rf_rad_freelist == NULL) {
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownRDFreeList, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d.\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownRDFreeList(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME_INIT(rf_rad_freelist, RF_RAD_INITIAL, next,
	    (RF_RaidAccessDesc_t *), rf_init_rad);
	return (0);
}

RF_RaidAccessDesc_t *
rf_AllocRaidAccDesc(
    RF_Raid_t			 *raidPtr,
    RF_IoType_t			  type,
    RF_RaidAddr_t		  raidAddress,
    RF_SectorCount_t		  numBlocks,
    caddr_t			  bufPtr,
    void			 *bp,
    RF_DagHeader_t		**paramDAG,
    RF_AccessStripeMapHeader_t	**paramASM,
    RF_RaidAccessFlags_t	  flags,
    void			(*cbF) (struct buf *),
    void			 *cbA,
    RF_AccessState_t		 *states
)
{
	RF_RaidAccessDesc_t *desc;

	RF_FREELIST_GET_INIT_NOUNLOCK(rf_rad_freelist, desc, next,
	    (RF_RaidAccessDesc_t *), rf_init_rad);
	if (raidPtr->waitShutdown) {
		/*
		 * Actually, we're shutting the array down. Free the desc
		 * and return NULL.
		 */
		RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
		RF_FREELIST_FREE_CLEAN(rf_rad_freelist, desc, next,
		    rf_clean_rad);
		return (NULL);
	}
	raidPtr->nAccOutstanding++;
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);

	desc->raidPtr = (void *) raidPtr;
	desc->type = type;
	desc->raidAddress = raidAddress;
	desc->numBlocks = numBlocks;
	desc->bufPtr = bufPtr;
	desc->bp = bp;
	desc->paramDAG = paramDAG;
	desc->paramASM = paramASM;
	desc->flags = flags;
	desc->states = states;
	desc->state = 0;

	desc->status = 0;
	bzero((char *) &desc->tracerec, sizeof(RF_AccTraceEntry_t));
	desc->callbackFunc = (void (*) (RF_CBParam_t)) cbF;	/* XXX */
	desc->callbackArg = cbA;
	desc->next = NULL;
	desc->head = desc;
	desc->numPending = 0;
	desc->cleanupList = NULL;
	rf_MakeAllocList(desc->cleanupList);
	return (desc);
}

void
rf_FreeRaidAccDesc(RF_RaidAccessDesc_t * desc)
{
	RF_Raid_t *raidPtr = desc->raidPtr;

	RF_ASSERT(desc);

	rf_FreeAllocList(desc->cleanupList);
	RF_FREELIST_FREE_CLEAN_NOUNLOCK(rf_rad_freelist, desc, next, rf_clean_rad);
	raidPtr->nAccOutstanding--;
	if (raidPtr->waitShutdown) {
		RF_SIGNAL_COND(raidPtr->outstandingCond);
	}
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
}


/********************************************************************
 * Main routine for performing an access.
 * Accesses are retried until a DAG can not be selected. This occurs
 * when either the DAG library is incomplete or there are too many
 * failures in a parity group.
 ********************************************************************/
int
rf_DoAccess(
    RF_Raid_t			 *raidPtr,
    RF_IoType_t			  type,		/* Should be read or write. */
    int				  async_flag,	/*
						 * Should be RF_TRUE
						 * or RF_FALSE.
						 */
    RF_RaidAddr_t		  raidAddress,
    RF_SectorCount_t		  numBlocks,
    caddr_t			  bufPtr,
    void			 *bp_in,	/*
						 * It's a buf pointer.
						 * void * to facilitate
						 * ignoring it outside
						 * the kernel.
						 */
    RF_DagHeader_t		**paramDAG,
    RF_AccessStripeMapHeader_t	**paramASM,
    RF_RaidAccessFlags_t	  flags,
    RF_RaidAccessDesc_t		**paramDesc,
    void			(*cbF) (struct buf *),
    void			 *cbA
)
{
	RF_RaidAccessDesc_t *desc;
	caddr_t lbufPtr = bufPtr;
	struct buf *bp = (struct buf *) bp_in;

	raidAddress += rf_raidSectorOffset;

	if (!raidPtr->valid) {
		RF_ERRORMSG("RAIDframe driver not successfully configured."
		    " Rejecting access.\n");
		IO_BUF_ERR(bp, EINVAL);
		return (EINVAL);
	}

	if (rf_accessDebug) {

		printf("logBytes is: %d %d %d.\n", raidPtr->raidid,
		    raidPtr->logBytesPerSector,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks));
		printf("raid%d: %s raidAddr %d (stripeid %d-%d) numBlocks %d (%d bytes) buf 0x%lx.\n", raidPtr->raidid,
		    (type == RF_IO_TYPE_READ) ? "READ" : "WRITE", (int) raidAddress,
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress),
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress + numBlocks - 1),
		    (int) numBlocks,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks),
		    (long) bufPtr);
	}
	if (raidAddress + numBlocks > raidPtr->totalSectors) {

		printf("DoAccess: raid addr %lu too large to access %lu sectors. Max legal addr is %lu.\n",
		    (u_long) raidAddress, (u_long) numBlocks, (u_long) raidPtr->totalSectors);

			IO_BUF_ERR(bp, ENOSPC);
			return (ENOSPC);
	}
	desc = rf_AllocRaidAccDesc(raidPtr, type, raidAddress,
	    numBlocks, lbufPtr, bp, paramDAG, paramASM,
	    flags, cbF, cbA, raidPtr->Layout.map->states);

	if (desc == NULL) {
		return (ENOMEM);
	}
	RF_ETIMER_START(desc->tracerec.tot_timer);

	desc->async_flag = async_flag;

	rf_ContinueRaidAccess(desc);

	return (0);
}


/* Force the array into reconfigured mode without doing reconstruction. */
int
rf_SetReconfiguredMode(RF_Raid_t *raidPtr, int row, int col)
{
	if (!(raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		printf("Can't set reconfigured mode in dedicated-spare"
		    " array.\n");
		RF_PANIC();
	}
	RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->numFailures++;
	raidPtr->Disks[row][col].status = rf_ds_dist_spared;
	raidPtr->status[row] = rf_rs_reconfigured;
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	/*
	 * Install spare table only if declustering + distributed sparing
	 * architecture.
	 */
	if (raidPtr->Layout.map->flags & RF_BD_DECLUSTERED)
		rf_InstallSpareTable(raidPtr, row, col);
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	return (0);
}

extern int fail_row, fail_col, fail_time;
extern int delayed_recon;

int
rf_FailDisk(RF_Raid_t *raidPtr, int frow, int fcol, int initRecon)
{
	printf("raid%d: Failing disk r%d c%d.\n", raidPtr->raidid, frow, fcol);
	RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->numFailures++;
	raidPtr->Disks[frow][fcol].status = rf_ds_failed;
	raidPtr->status[frow] = rf_rs_degraded;
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	if (initRecon)
		rf_ReconstructFailedDisk(raidPtr, frow, fcol);
	return (0);
}


/*
 * Releases a thread that is waiting for the array to become quiesced.
 * access_suspend_mutex should be locked upon calling this.
 */
void
rf_SignalQuiescenceLock(RF_Raid_t *raidPtr, RF_RaidReconDesc_t *reconDesc)
{
	if (rf_quiesceDebug) {
		printf("raid%d: Signalling quiescence lock.\n",
		       raidPtr->raidid);
	}
	raidPtr->access_suspend_release = 1;

	if (raidPtr->waiting_for_quiescence) {
		SIGNAL_QUIESCENT_COND(raidPtr);
	}
}


/*
 * Suspends all new requests to the array. No effect on accesses that are
 * in flight.
 */
int
rf_SuspendNewRequestsAndWait(RF_Raid_t *raidPtr)
{
	if (rf_quiesceDebug)
		printf("Suspending new reqs.\n");

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended++;
	raidPtr->waiting_for_quiescence = (raidPtr->accs_in_flight == 0) ? 0 : 1;

	if (raidPtr->waiting_for_quiescence) {
		raidPtr->access_suspend_release = 0;
		while (!raidPtr->access_suspend_release) {
			printf("Suspending: Waiting for Quiescence.\n");
			WAIT_FOR_QUIESCENCE(raidPtr);
			raidPtr->waiting_for_quiescence = 0;
		}
	}
	printf("Quiescence reached...\n");

	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);
	return (raidPtr->waiting_for_quiescence);
}


/* Wake up everyone waiting for quiescence to be released. */
void
rf_ResumeNewRequests(RF_Raid_t *raidPtr)
{
	RF_CallbackDesc_t *t, *cb;

	if (rf_quiesceDebug)
		printf("Resuming new reqs.\n");

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended--;
	if (raidPtr->accesses_suspended == 0)
		cb = raidPtr->quiesce_wait_list;
	else
		cb = NULL;
	raidPtr->quiesce_wait_list = NULL;
	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);

	while (cb) {
		t = cb;
		cb = cb->next;
		(t->callbackFunc) (t->callbackArg);
		rf_FreeCallbackDesc(t);
	}
}


/*****************************************************************************
 *
 * Debug routines.
 *
 *****************************************************************************/

void
rf_set_debug_option(char *name, long val)
{
	RF_DebugName_t *p;

	for (p = rf_debugNames; p->name; p++) {
		if (!strcmp(p->name, name)) {
			*(p->ptr) = val;
			printf("[Set debug variable %s to %ld]\n", name, val);
			return;
		}
	}
	RF_ERRORMSG1("Unknown debug string \"%s\"\n", name);
}


/* Would like to use sscanf here, but apparently not available in kernel. */
/*ARGSUSED*/
void
rf_ConfigureDebug(RF_Config_t *cfgPtr)
{
	char *val_p, *name_p, *white_p;
	long val;
	int i;

	rf_ResetDebugOptions();
	for (i = 0; cfgPtr->debugVars[i][0] && i < RF_MAXDBGV; i++) {
		name_p = rf_find_non_white(&cfgPtr->debugVars[i][0]);
		white_p = rf_find_white(name_p);	/*
							 * Skip to start of 2nd
							 * word.
							 */
		val_p = rf_find_non_white(white_p);
		if (*val_p == '0' && *(val_p + 1) == 'x')
			val = rf_htoi(val_p + 2);
		else
			val = rf_atoi(val_p);
		*white_p = '\0';
		rf_set_debug_option(name_p, val);
	}
}


/* Performance monitoring stuff. */

#if	!defined(_KERNEL) && !defined(SIMULATE)

/*
 * Throughput stats currently only used in user-level RAIDframe.
 */

int
rf_InitThroughputStats(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
    RF_Config_t *cfgPtr)
{
	int rc;

	/* These used by user-level RAIDframe only. */
	rc = rf_create_managed_mutex(listp, &raidPtr->throughputstats.mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d.\n",
		    __FILE__, __LINE__, rc);
		return (rc);
	}
	raidPtr->throughputstats.sum_io_us = 0;
	raidPtr->throughputstats.num_ios = 0;
	raidPtr->throughputstats.num_out_ios = 0;
	return (0);
}

void
rf_StartThroughputStats(RF_Raid_t *raidPtr)
{
	RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
	raidPtr->throughputstats.num_ios++;
	raidPtr->throughputstats.num_out_ios++;
	if (raidPtr->throughputstats.num_out_ios == 1)
		RF_GETTIME(raidPtr->throughputstats.start);
	RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);
}

void
rf_StopThroughputStats(RF_Raid_t *raidPtr)
{
	struct timeval diff;

	RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
	raidPtr->throughputstats.num_out_ios--;
	if (raidPtr->throughputstats.num_out_ios == 0) {
		RF_GETTIME(raidPtr->throughputstats.stop);
		RF_TIMEVAL_DIFF(&raidPtr->throughputstats.start,
		    &raidPtr->throughputstats.stop, &diff);
		raidPtr->throughputstats.sum_io_us += RF_TIMEVAL_TO_US(diff);
	}
	RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);
}

void
rf_PrintThroughputStats(RF_Raid_t *raidPtr)
{
	RF_ASSERT(raidPtr->throughputstats.num_out_ios == 0);
	if (raidPtr->throughputstats.sum_io_us != 0) {
		printf("[Througphut: %8.2f IOs/second]\n",
		    raidPtr->throughputstats.num_ios /
		    (raidPtr->throughputstats.sum_io_us / 1000000.0));
	}
}

#endif	/* !_KERNEL && !SIMULATE */

void
rf_StartUserStats(RF_Raid_t *raidPtr)
{
	RF_GETTIME(raidPtr->userstats.start);
	raidPtr->userstats.sum_io_us = 0;
	raidPtr->userstats.num_ios = 0;
	raidPtr->userstats.num_sect_moved = 0;
}

void
rf_StopUserStats(RF_Raid_t *raidPtr)
{
	RF_GETTIME(raidPtr->userstats.stop);
}

void
rf_UpdateUserStats(
    RF_Raid_t	*raidPtr,
    int		 rt,		/* Response time in us. */
    int		 numsect	/* Number of sectors for this access. */
)
{
	raidPtr->userstats.sum_io_us += rt;
	raidPtr->userstats.num_ios++;
	raidPtr->userstats.num_sect_moved += numsect;
}

void
rf_PrintUserStats(RF_Raid_t *raidPtr)
{
	long    elapsed_us, mbs, mbs_frac;
	struct timeval diff;

	RF_TIMEVAL_DIFF(&raidPtr->userstats.start, &raidPtr->userstats.stop,
	    &diff);
	elapsed_us = RF_TIMEVAL_TO_US(diff);

	/* 2000 sectors per megabyte, 10000000 microseconds per second. */
	if (elapsed_us)
		mbs = (raidPtr->userstats.num_sect_moved / 2000) /
		    (elapsed_us / 1000000);
	else
		mbs = 0;

	/* This computes only the first digit of the fractional mb/s moved. */
	if (elapsed_us) {
		mbs_frac = ((raidPtr->userstats.num_sect_moved / 200) /
		    (elapsed_us / 1000000)) - (mbs * 10);
	} else {
		mbs_frac = 0;
	}

	printf("Number of I/Os:             %ld\n",
	    raidPtr->userstats.num_ios);
	printf("Elapsed time (us):          %ld\n",
	    elapsed_us);
	printf("User I/Os per second:       %ld\n",
	    RF_DB0_CHECK(raidPtr->userstats.num_ios, (elapsed_us / 1000000)));
	printf("Average user response time: %ld us\n",
	    RF_DB0_CHECK(raidPtr->userstats.sum_io_us,
	     raidPtr->userstats.num_ios));
	printf("Total sectors moved:        %ld\n",
	    raidPtr->userstats.num_sect_moved);
	printf("Average access size (sect): %ld\n",
	    RF_DB0_CHECK(raidPtr->userstats.num_sect_moved,
	    raidPtr->userstats.num_ios));
	printf("Achieved data rate:         %ld.%ld MB/sec\n",
	    mbs, mbs_frac);
}
