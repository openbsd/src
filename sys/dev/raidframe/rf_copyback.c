/*	$OpenBSD: rf_copyback.c,v 1.1 1999/01/11 14:29:02 niklas Exp $	*/
/*	$NetBSD: rf_copyback.c,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
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

/*****************************************************************************************
 *
 * copyback.c -- code to copy reconstructed data back from spare space to
 *               the replaced disk.
 *
 * the code operates using callbacks on the I/Os to continue with the next 
 * unit to be copied back.  We do this because a simple loop containing blocking I/Os
 * will not work in the simulator.
 *
 ****************************************************************************************/

/*
 * :  
 * Log: rf_copyback.c,v 
 * Revision 1.26  1996/08/06 22:26:00  jimz
 * don't include sys/buf.h on linux
 *
 * Revision 1.25  1996/07/30  03:30:40  jimz
 * include rf_types.h first
 *
 * Revision 1.24  1996/07/27  18:39:52  jimz
 * cleanup sweep
 *
 * Revision 1.23  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.22  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.21  1996/07/11  16:03:47  jimz
 * fixed hanging bug in rf_CopybackWriteDoneProc()
 *
 * Revision 1.20  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.19  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.18  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.17  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.16  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.15  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.14  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.13  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.12  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.11  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.10  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.9  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.8  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.7  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.6  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.5  1995/12/01  15:15:31  root
 * added copyright info
 *
 * Revision 1.4  1995/06/23  13:41:36  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#include "rf_types.h"
#include <sys/time.h>
#ifndef LINUX
#include <sys/buf.h>
#endif /* !LINUX */
#include "rf_raid.h"
#include "rf_threadid.h"
#include "rf_mcpair.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_utils.h"
#include "rf_copyback.h"
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#include "rf_camlayer.h"
#endif
#include "rf_decluster.h"
#include "rf_driver.h"
#include "rf_shutdown.h"
#include "rf_sys.h"

#define RF_COPYBACK_DATA   0
#define RF_COPYBACK_PARITY 1

int rf_copyback_in_progress;

static int  rf_CopybackReadDoneProc(RF_CopybackDesc_t *desc, int status);
static int  rf_CopybackWriteDoneProc(RF_CopybackDesc_t *desc, int status);
static void rf_CopybackOne(RF_CopybackDesc_t *desc, int typ,
	RF_RaidAddr_t addr, RF_RowCol_t testRow, RF_RowCol_t testCol,
	RF_SectorNum_t testOffs);
static void rf_CopybackComplete(RF_CopybackDesc_t *desc, int status);

int rf_ConfigureCopyback(listp)
  RF_ShutdownList_t  **listp;
{
  rf_copyback_in_progress = 0;
  return(0);
}

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#ifdef __NETBSD__
#include <sys/vnode.h>
#endif

int raidlookup __P((char *, struct proc *, struct vnode **));
#endif

/* do a complete copyback */
void rf_CopybackReconstructedData(raidPtr)
  RF_Raid_t  *raidPtr;
{
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
  int done,retcode;
  RF_CopybackDesc_t *desc;
  RF_RowCol_t frow, fcol;
  RF_RaidDisk_t *badDisk;
  char *databuf;

  struct partinfo dpart;
  struct vnode *vp;
  struct vattr va;
  struct proc *proc;

#else
  int bus, targ, lun, done, retcode;
  RF_CopybackDesc_t *desc;
  RF_RowCol_t frow, fcol;
  RF_RaidDisk_t *badDisk;
  RF_DiskOp_t *tur_op;
  char *databuf;
#endif

  done = 0;
  fcol = 0;
  for (frow=0; frow<raidPtr->numRow; frow++) {
    for (fcol=0; fcol<raidPtr->numCol; fcol++) {
      if (raidPtr->Disks[frow][fcol].status == rf_ds_dist_spared
        || raidPtr->Disks[frow][fcol].status == rf_ds_spared)
      {
        done = 1;
        break;
      }
    }
    if (done)
      break;
  }

  if (frow == raidPtr->numRow) {
    printf("COPYBACK:  no disks need copyback\n");
    return;
  }

  badDisk = &raidPtr->Disks[frow][fcol];
#ifndef SIMULATE
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)

  proc = raidPtr->proc;  /* XXX Yes, this is not nice.. */

#if 0
  printf("Pretending the disk is happy...\n");
  retcode = 0;  /* XXX this should be set to something more realistic. */
#endif

  /* This device may have been opened successfully the first time. 
     Close it before trying to open it again.. */

  if (raidPtr->raid_cinfo[frow][fcol].ci_vp != NULL) {
	  printf("Closed the open device: %s\n",
		 raidPtr->Disks[frow][fcol].devname);
	  (void)vn_close(raidPtr->raid_cinfo[frow][fcol].ci_vp, 
			 FREAD|FWRITE,  proc->p_ucred, proc); 
  }

  printf("About to (re-)open the device: %s\n",
		 raidPtr->Disks[frow][fcol].devname);

  retcode = raidlookup(raidPtr->Disks[frow][fcol].devname, proc, &vp);

  if (retcode) {
	  printf("COPYBACK: raidlookup on device: %s failed: %d!\n",
		 raidPtr->Disks[frow][fcol].devname, retcode);

	  /* XXX the component isn't responding properly...
	     must be still dead :-( */
	  return;

  } else {
	  
	  /* Ok, so we can at least do a lookup... How about actually 
	     getting a vp for it? */

	  if ((retcode = VOP_GETATTR(vp, &va, proc->p_ucred, proc)) != 0) {
		  return;
	  } 

	  retcode = VOP_IOCTL(vp, DIOCGPART, (caddr_t)&dpart, 
			    FREAD, proc->p_ucred, proc);
	  if (retcode) {
		  return;
	  }
	  raidPtr->Disks[frow][fcol].blockSize = dpart.disklab->d_secsize;
		  
	  raidPtr->Disks[frow][fcol].numBlocks = dpart.part->p_size - 
			  rf_protectedSectors;
	  
	  raidPtr->raid_cinfo[frow][fcol].ci_vp = vp;
	  raidPtr->raid_cinfo[frow][fcol].ci_dev = va.va_rdev;

	  raidPtr->Disks[frow][fcol].dev = va.va_rdev; /* XXX or the above? */

	  /* we allow the user to specify that only a fraction of the 
	   * disks should be used this is just for debug:  it speeds up 
	   * the parity scan
	   */
	  raidPtr->Disks[frow][fcol].numBlocks = 
		  raidPtr->Disks[frow][fcol].numBlocks * 
		  rf_sizePercentage / 100;
  }
#else
  if (rf_extract_ids(badDisk->devname, &bus, &targ, &lun)) {
    printf("COPYBACK: unable to extract bus, target, lun from devname %s\n",
      badDisk->devname);
    return;
  }

  /* TUR the disk that's marked as bad to be sure that it's actually alive */
  rf_SCSI_AllocTUR(&tur_op);
  retcode = rf_SCSI_DoTUR(tur_op, bus, targ, lun, badDisk->dev);
  rf_SCSI_FreeDiskOp(tur_op, 0);
#endif

  if (retcode) {
    printf("COPYBACK: target disk failed TUR\n");
    return;
  }
#endif /* !SIMULATE */

  /* get a buffer to hold one SU  */
  RF_Malloc(databuf, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit), (char *));

  /* create a descriptor */
  RF_Malloc(desc, sizeof(*desc), (RF_CopybackDesc_t *));
  desc->raidPtr       = raidPtr;
  desc->status        = 0;
  desc->frow          = frow;
  desc->fcol          = fcol;
  desc->spRow         = badDisk->spareRow;
  desc->spCol         = badDisk->spareCol;
  desc->stripeAddr    = 0;
  desc->sectPerSU     = raidPtr->Layout.sectorsPerStripeUnit;
  desc->sectPerStripe = raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.numDataCol;
  desc->databuf       = databuf;
#ifndef SIMULATE
  desc->mcpair        = rf_AllocMCPair();
#endif /* !SIMULATE */

  printf("COPYBACK: Quiescing the array\n");
  /* quiesce the array, since we don't want to code support for user accs here */
  rf_SuspendNewRequestsAndWait(raidPtr);

  /* adjust state of the array and of the disks */
  RF_LOCK_MUTEX(raidPtr->mutex);
  raidPtr->Disks[desc->frow][desc->fcol].status = rf_ds_optimal;
  raidPtr->status[desc->frow] = rf_rs_optimal;
  rf_copyback_in_progress = 1;                   /* debug only */
  RF_UNLOCK_MUTEX(raidPtr->mutex);
  
  printf("COPYBACK: Beginning\n");
  RF_GETTIME(desc->starttime);
  rf_ContinueCopyback(desc);
}


/*
 * invoked via callback after a copyback I/O has completed to
 * continue on with the next one
 */
void rf_ContinueCopyback(desc)
  RF_CopybackDesc_t  *desc;
{
  RF_SectorNum_t testOffs, stripeAddr;
  RF_Raid_t *raidPtr = desc->raidPtr;
  RF_RaidAddr_t addr;
  RF_RowCol_t testRow, testCol; 
  int old_pctg, new_pctg, done;
  struct timeval t, diff;

  old_pctg = (-1);
  while (1) {
    stripeAddr = desc->stripeAddr;
    if (rf_prReconSched) {
      old_pctg = 100 * desc->stripeAddr / raidPtr->totalSectors;
    }
    desc->stripeAddr += desc->sectPerStripe;
    if (rf_prReconSched) {
      new_pctg = 100 * desc->stripeAddr / raidPtr->totalSectors;
      if (new_pctg != old_pctg) {
        RF_GETTIME(t);
        RF_TIMEVAL_DIFF(&desc->starttime, &t, &diff);
        printf("%d %d.%06d\n",new_pctg, (int)diff.tv_sec, (int)diff.tv_usec);
      }
    }
    
    if (stripeAddr >= raidPtr->totalSectors) {
      rf_CopybackComplete(desc, 0);
      return;
    }
    
    /* walk through the current stripe, su-by-su */
    for (done=0, addr = stripeAddr; addr < stripeAddr+desc->sectPerStripe; addr += desc->sectPerSU) {
      
      /* map the SU, disallowing remap to spare space */
      (raidPtr->Layout.map->MapSector)(raidPtr, addr, &testRow, &testCol, &testOffs, RF_DONT_REMAP);
      
      if (testRow == desc->frow && testCol == desc->fcol) {
        rf_CopybackOne(desc, RF_COPYBACK_DATA, addr, testRow, testCol, testOffs);
#ifdef SIMULATE
        return;
#else /* SIMULATE */
        done = 1;
        break;
#endif /* SIMULATE */
      }
    }

    if (!done) {
      /* we didn't find the failed disk in the data part.  check parity. */
    
      /* map the parity for this stripe, disallowing remap to spare space */
      (raidPtr->Layout.map->MapParity)(raidPtr, stripeAddr, &testRow, &testCol, &testOffs, RF_DONT_REMAP);
    
      if (testRow == desc->frow && testCol == desc->fcol) {
        rf_CopybackOne(desc, RF_COPYBACK_PARITY, stripeAddr, testRow, testCol, testOffs);
#ifdef SIMULATE
        return;
#endif /* SIMULATE */
      }
    }

    /* check to see if the last read/write pair failed */
    if (desc->status) {
      rf_CopybackComplete(desc, 1);
      return;
    }
    
    /* we didn't find any units to copy back in this stripe.  Continue with the next one */
  }
}


/* copyback one unit */
static void rf_CopybackOne(desc, typ, addr, testRow, testCol, testOffs)
  RF_CopybackDesc_t  *desc;
  int                 typ;
  RF_RaidAddr_t       addr;
  RF_RowCol_t         testRow;
  RF_RowCol_t         testCol;
  RF_SectorNum_t      testOffs;
{
  RF_SectorCount_t sectPerSU = desc->sectPerSU;
  RF_Raid_t *raidPtr = desc->raidPtr;
  RF_RowCol_t spRow     = desc->spRow;
  RF_RowCol_t spCol     = desc->spCol;
  RF_SectorNum_t spOffs;

  /* find the spare spare location for this SU */
  if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
    if (typ == RF_COPYBACK_DATA)
      raidPtr->Layout.map->MapSector(raidPtr, addr, &spRow, &spCol, &spOffs, RF_REMAP);
    else
      raidPtr->Layout.map->MapParity(raidPtr, addr, &spRow, &spCol, &spOffs, RF_REMAP);      
  } else {
    spOffs = testOffs;
  }

  /* create reqs to read the old location & write the new */
  desc->readreq  = rf_CreateDiskQueueData(RF_IO_TYPE_READ, spOffs,
    sectPerSU, desc->databuf, 0L, 0, 
    (int (*)(void *,int)) rf_CopybackReadDoneProc, desc,
    NULL, NULL, (void *) raidPtr, RF_DISKQUEUE_DATA_FLAGS_NONE, NULL);
  desc->writereq = rf_CreateDiskQueueData(RF_IO_TYPE_WRITE, testOffs,
    sectPerSU, desc->databuf, 0L, 0, 
    (int (*)(void *,int)) rf_CopybackWriteDoneProc, desc,
    NULL, NULL, (void *) raidPtr, RF_DISKQUEUE_DATA_FLAGS_NONE, NULL);
  desc->frow = testRow;
  desc->fcol = testCol;
  
  /* enqueue the read.  the write will go out as part of the callback on the read.
   * at user-level & in the kernel, wait for the read-write pair to complete.
   * in the simulator, just return, since everything will happen as callbacks 
   */
#ifndef SIMULATE
  RF_LOCK_MUTEX(desc->mcpair->mutex);
  desc->mcpair->flag = 0;
#endif /* !SIMULATE */

  rf_DiskIOEnqueue(&raidPtr->Queues[spRow][spCol], desc->readreq, RF_IO_NORMAL_PRIORITY);
  
#ifndef SIMULATE
  while (!desc->mcpair->flag) {
    RF_WAIT_MCPAIR(desc->mcpair);
  }
  RF_UNLOCK_MUTEX(desc->mcpair->mutex);
  rf_FreeDiskQueueData(desc->readreq);
  rf_FreeDiskQueueData(desc->writereq);
#endif /* !SIMULATE */
}


/* called at interrupt context when the read has completed.  just send out the write */
static int rf_CopybackReadDoneProc(desc, status)
  RF_CopybackDesc_t  *desc;
  int                 status;
{
  if (status) {                                              /* invoke the callback with bad status */
    printf("COPYBACK: copyback read failed.  Aborting.\n");
    (desc->writereq->CompleteFunc)(desc, -100);
  }
  else {
    rf_DiskIOEnqueue(&(desc->raidPtr->Queues[desc->frow][desc->fcol]), desc->writereq, RF_IO_NORMAL_PRIORITY);
  }
  return(0);
}

/* called at interrupt context when the write has completed.  
 * at user level & in the kernel, wake up the copyback thread.
 * in the simulator, invoke the next copyback directly.
 * can't free diskqueuedata structs in the kernel b/c we're at interrupt context.
 */
static int rf_CopybackWriteDoneProc(desc, status)
  RF_CopybackDesc_t  *desc;
  int                 status;
{
  if (status && status != -100) {
    printf("COPYBACK: copyback write failed.  Aborting.\n");
  }

#ifdef SIMULATE
  rf_FreeDiskQueueData(desc->readreq);
  rf_FreeDiskQueueData(desc->writereq);
  if (!status)
    rf_ContinueCopyback(desc);
  else
    rf_CopybackComplete(desc, 1);
#else /* SIMULATE */
  desc->status = status;
  rf_MCPairWakeupFunc(desc->mcpair);
#endif /* SIMULATE */
  return(0);
}  

/* invoked when the copyback has completed */
static void rf_CopybackComplete(desc, status)
  RF_CopybackDesc_t  *desc;
  int                 status;
{
  RF_Raid_t *raidPtr = desc->raidPtr;
  struct timeval t, diff;

  if (!status) {
    RF_LOCK_MUTEX(raidPtr->mutex);
    if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
      RF_ASSERT(raidPtr->Layout.map->parityConfig == 'D');
      rf_FreeSpareTable(raidPtr);
    } else {
      raidPtr->Disks[desc->spRow][desc->spCol].status = rf_ds_spare;
    }
    RF_UNLOCK_MUTEX(raidPtr->mutex);

    RF_GETTIME(t);
    RF_TIMEVAL_DIFF(&desc->starttime, &t, &diff);
    printf("Copyback time was %d.%06d seconds\n",
	   (int)diff.tv_sec, (int)diff.tv_usec);
  } else printf("COPYBACK: Failure.\n");

  RF_Free(desc->databuf, rf_RaidAddressToByte(raidPtr, desc->sectPerSU));
#ifndef SIMULATE
  rf_FreeMCPair(desc->mcpair);
#endif /* !SIMULATE */
  RF_Free(desc, sizeof(*desc));

  rf_copyback_in_progress = 0;
  rf_ResumeNewRequests(raidPtr);
}
