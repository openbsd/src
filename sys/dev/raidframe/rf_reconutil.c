/*	$OpenBSD: rf_reconutil.c,v 1.1 1999/01/11 14:29:47 niklas Exp $	*/
/*	$NetBSD: rf_reconutil.c,v 1.1 1998/11/13 04:20:34 oster Exp $	*/
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

/********************************************
 * rf_reconutil.c -- reconstruction utilities
 ********************************************/

/* :  
 * Log: rf_reconutil.c,v 
 * Revision 1.32  1996/07/29 14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.31  1996/07/15  05:40:41  jimz
 * some recon datastructure cleanup
 * better handling of multiple failures
 * added undocumented double-recon test
 *
 * Revision 1.30  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.29  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.28  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.27  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.26  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.25  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.24  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.23  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.22  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.21  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.20  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.19  1996/05/20  16:14:55  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.18  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.17  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.16  1995/12/06  15:05:31  root
 * added copyright info
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_desc.h"
#include "rf_reconutil.h"
#include "rf_reconbuffer.h"
#include "rf_general.h"
#include "rf_decluster.h"
#include "rf_raid5_rotatedspare.h"
#include "rf_interdecluster.h"
#include "rf_chaindecluster.h"

/*******************************************************************
 * allocates/frees the reconstruction control information structures
 *******************************************************************/
RF_ReconCtrl_t *rf_MakeReconControl(reconDesc, frow, fcol, srow, scol)
  RF_RaidReconDesc_t  *reconDesc;
  RF_RowCol_t          frow;    /* failed row and column */
  RF_RowCol_t          fcol;
  RF_RowCol_t          srow;    /* identifies which spare we're using */
  RF_RowCol_t          scol;
{
  RF_Raid_t *raidPtr = reconDesc->raidPtr;
  RF_RaidLayout_t  *layoutPtr = &raidPtr->Layout;
  RF_ReconUnitCount_t RUsPerPU = layoutPtr->SUsPerPU / layoutPtr->SUsPerRU;
  RF_ReconUnitCount_t numSpareRUs;
  RF_ReconCtrl_t *reconCtrlPtr;
  RF_ReconBuffer_t *rbuf;
  RF_LayoutSW_t *lp;
  int retcode, rc;
  RF_RowCol_t i;

  lp = raidPtr->Layout.map;

  /* make and zero the global reconstruction structure and the per-disk structure */
  RF_Calloc(reconCtrlPtr, 1, sizeof(RF_ReconCtrl_t), (RF_ReconCtrl_t *));
  RF_Calloc(reconCtrlPtr->perDiskInfo, raidPtr->numCol, sizeof(RF_PerDiskReconCtrl_t), (RF_PerDiskReconCtrl_t *));  /* this zeros it */
  reconCtrlPtr->reconDesc = reconDesc;
  reconCtrlPtr->fcol = fcol;
  reconCtrlPtr->spareRow = srow;
  reconCtrlPtr->spareCol = scol;
  reconCtrlPtr->lastPSID = layoutPtr->numStripe/layoutPtr->SUsPerPU;
  reconCtrlPtr->percentComplete = 0;
  
  /* initialize each per-disk recon information structure */
  for (i=0; i<raidPtr->numCol; i++) {
    reconCtrlPtr->perDiskInfo[i].reconCtrl = reconCtrlPtr;
    reconCtrlPtr->perDiskInfo[i].row      = frow;
    reconCtrlPtr->perDiskInfo[i].col      = i;
    reconCtrlPtr->perDiskInfo[i].curPSID  = -1;          /* make it appear as if we just finished an RU */
    reconCtrlPtr->perDiskInfo[i].ru_count = RUsPerPU-1;
  }

 /* Get the number of spare units per disk and the sparemap in case spare is distributed  */

  if (lp->GetNumSpareRUs) {
    numSpareRUs = lp->GetNumSpareRUs(raidPtr);
  }
  else {
    numSpareRUs = 0;
  }

  /*
   * Not all distributed sparing archs need dynamic mappings
   */
  if (lp->InstallSpareTable) {
    retcode = rf_InstallSpareTable(raidPtr, frow, fcol);
    if (retcode) {
      RF_PANIC(); /* XXX fix this*/
    }
  }

  /* make the reconstruction map */
  reconCtrlPtr->reconMap = rf_MakeReconMap(raidPtr, (int) (layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit),
					raidPtr->sectorsPerDisk, numSpareRUs);

  /* make the per-disk reconstruction buffers */
  for (i=0; i<raidPtr->numCol; i++) {
    reconCtrlPtr->perDiskInfo[i].rbuf = (i==fcol) ? NULL : rf_MakeReconBuffer(raidPtr, frow, i, RF_RBUF_TYPE_EXCLUSIVE);
  }

  /* initialize the event queue */
  rc = rf_mutex_init(&reconCtrlPtr->eq_mutex);
  if (rc) {
    /* XXX deallocate, cleanup */
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(NULL);
  }
  rc = rf_cond_init(&reconCtrlPtr->eq_cond);
  if (rc) {
    /* XXX deallocate, cleanup */
    RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(NULL);
  }
  reconCtrlPtr->eventQueue = NULL;
  reconCtrlPtr->eq_count = 0;

  /* make the floating recon buffers and append them to the free list */
  rc = rf_mutex_init(&reconCtrlPtr->rb_mutex);
  if (rc) {
    /* XXX deallocate, cleanup */
    RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
      __LINE__, rc);
    return(NULL);
  }
  reconCtrlPtr->fullBufferList= NULL;
  reconCtrlPtr->priorityList  = NULL;
  reconCtrlPtr->floatingRbufs = NULL;
  reconCtrlPtr->committedRbufs= NULL;
  for (i=0; i<raidPtr->numFloatingReconBufs; i++) {
    rbuf = rf_MakeReconBuffer(raidPtr, frow, fcol, RF_RBUF_TYPE_FLOATING);
    rbuf->next = reconCtrlPtr->floatingRbufs;
    reconCtrlPtr->floatingRbufs = rbuf;
  }

  /* create the parity stripe status table */
  reconCtrlPtr->pssTable = rf_MakeParityStripeStatusTable(raidPtr);

  /* set the initial min head sep counter val */
  reconCtrlPtr->minHeadSepCounter = 0;
  
  return(reconCtrlPtr);
}

void rf_FreeReconControl(raidPtr, row)
  RF_Raid_t    *raidPtr;
  RF_RowCol_t   row;
{
  RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl[row];
  RF_ReconBuffer_t *t;
  RF_ReconUnitNum_t i;
  
  RF_ASSERT(reconCtrlPtr);
  for (i=0; i<raidPtr->numCol; i++) if (reconCtrlPtr->perDiskInfo[i].rbuf) rf_FreeReconBuffer(reconCtrlPtr->perDiskInfo[i].rbuf);
  for (i=0; i<raidPtr->numFloatingReconBufs; i++) {
    t = reconCtrlPtr->floatingRbufs;
    RF_ASSERT(t);
    reconCtrlPtr->floatingRbufs = t->next;
    rf_FreeReconBuffer(t);
  }
  rf_mutex_destroy(&reconCtrlPtr->rb_mutex);
  rf_mutex_destroy(&reconCtrlPtr->eq_mutex);
  rf_cond_destroy(&reconCtrlPtr->eq_cond);
  rf_FreeReconMap(reconCtrlPtr->reconMap);
  rf_FreeParityStripeStatusTable(raidPtr, reconCtrlPtr->pssTable);
  RF_Free(reconCtrlPtr->perDiskInfo, raidPtr->numCol * sizeof(RF_PerDiskReconCtrl_t));
  RF_Free(reconCtrlPtr, sizeof(*reconCtrlPtr));
}


/******************************************************************************
 * computes the default head separation limit
 *****************************************************************************/
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimit(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_HeadSepLimit_t hsl;
  RF_LayoutSW_t *lp;

  lp = raidPtr->Layout.map;
  if (lp->GetDefaultHeadSepLimit == NULL)
    return(-1);
  hsl = lp->GetDefaultHeadSepLimit(raidPtr);
  return(hsl);
}


/******************************************************************************
 * computes the default number of floating recon buffers
 *****************************************************************************/
int rf_GetDefaultNumFloatingReconBuffers(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_LayoutSW_t *lp;
  int nrb;

  lp = raidPtr->Layout.map;
  if (lp->GetDefaultNumFloatingReconBuffers == NULL)
    return(3 * raidPtr->numCol);
  nrb = lp->GetDefaultNumFloatingReconBuffers(raidPtr);
  return(nrb);
}


/******************************************************************************
 * creates and initializes a reconstruction buffer
 *****************************************************************************/
RF_ReconBuffer_t *rf_MakeReconBuffer(
  RF_Raid_t      *raidPtr,
  RF_RowCol_t     row,
  RF_RowCol_t     col,
  RF_RbufType_t   type)
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_ReconBuffer_t *t;
  u_int recon_buffer_size = rf_RaidAddressToByte(raidPtr, layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit);

  RF_Malloc(t, sizeof(RF_ReconBuffer_t), (RF_ReconBuffer_t *));
  RF_Malloc(t->buffer, recon_buffer_size, (caddr_t));
  RF_Malloc(t->arrived, raidPtr->numCol * sizeof(char), (char *));
  t->raidPtr = raidPtr;
  t->row = row; t->col = col;
  t->priority = RF_IO_RECON_PRIORITY;
  t->type = type;
  t->pssPtr = NULL;
  t->next = NULL;
  return(t);
}

/******************************************************************************
 * frees a reconstruction buffer
 *****************************************************************************/
void rf_FreeReconBuffer(rbuf)
  RF_ReconBuffer_t  *rbuf;
{
  RF_Raid_t *raidPtr = rbuf->raidPtr;
  u_int recon_buffer_size = rf_RaidAddressToByte(raidPtr, raidPtr->Layout.SUsPerRU * raidPtr->Layout.sectorsPerStripeUnit);
  
  RF_Free(rbuf->arrived, raidPtr->numCol * sizeof(char));
  RF_Free(rbuf->buffer, recon_buffer_size);
  RF_Free(rbuf, sizeof(*rbuf));
}


/******************************************************************************
 * debug only:  sanity check the number of floating recon bufs in use
 *****************************************************************************/
void rf_CheckFloatingRbufCount(raidPtr, dolock)
  RF_Raid_t  *raidPtr;
  int         dolock;
{
  RF_ReconParityStripeStatus_t *p;
  RF_PSStatusHeader_t *pssTable;
  RF_ReconBuffer_t *rbuf;
  int i, j, sum = 0;
  RF_RowCol_t frow=0;

  for (i=0; i<raidPtr->numRow; i++)
    if (raidPtr->reconControl[i]) {
      frow = i;
      break;
     }
  RF_ASSERT(frow >= 0);

  if (dolock)
    RF_LOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
  pssTable = raidPtr->reconControl[frow]->pssTable;

  for (i=0; i<raidPtr->pssTableSize; i++) {
    RF_LOCK_MUTEX(pssTable[i].mutex);
    for (p = pssTable[i].chain; p; p=p->next) {
      rbuf = (RF_ReconBuffer_t *) p->rbuf;
      if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
        sum++;

      rbuf = (RF_ReconBuffer_t *) p->writeRbuf;
      if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
        sum++;

      for (j=0; j<p->xorBufCount; j++) {
        rbuf = (RF_ReconBuffer_t *) p->rbufsForXor[j];
        RF_ASSERT(rbuf);
        if (rbuf->type == RF_RBUF_TYPE_FLOATING)
          sum++;
      }
    }
    RF_UNLOCK_MUTEX(pssTable[i].mutex);
  }

  for (rbuf = raidPtr->reconControl[frow]->floatingRbufs;  rbuf; rbuf = rbuf->next) {
    if (rbuf->type == RF_RBUF_TYPE_FLOATING)
      sum++;
  }
  for (rbuf = raidPtr->reconControl[frow]->committedRbufs; rbuf; rbuf = rbuf->next) {
    if (rbuf->type == RF_RBUF_TYPE_FLOATING)
      sum++;
  }
  for (rbuf = raidPtr->reconControl[frow]->fullBufferList; rbuf; rbuf = rbuf->next) {
    if (rbuf->type == RF_RBUF_TYPE_FLOATING)
      sum++;
  }
  for (rbuf = raidPtr->reconControl[frow]->priorityList;   rbuf; rbuf = rbuf->next) {
    if (rbuf->type == RF_RBUF_TYPE_FLOATING)
      sum++;
  }

  RF_ASSERT(sum == raidPtr->numFloatingReconBufs);

  if (dolock)
    RF_UNLOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
}
