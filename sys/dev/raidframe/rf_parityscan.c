/*	$OpenBSD: rf_parityscan.c,v 1.1 1999/01/11 14:29:37 niklas Exp $	*/
/*	$NetBSD: rf_parityscan.c,v 1.1 1998/11/13 04:20:32 oster Exp $	*/
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

/*****************************************************************************
 *
 * rf_parityscan.c -- misc utilities related to parity verification
 *
 *****************************************************************************/

/*
 * :  
 * Log: rf_parityscan.c,v 
 * Revision 1.47  1996/08/20 20:35:01  jimz
 * change diagnostic string in rewrite
 *
 * Revision 1.46  1996/08/20  20:03:19  jimz
 * fixed parity rewrite to actually use arch-specific parity stuff
 * (this ever worked... how?)
 *
 * Revision 1.45  1996/08/16  17:41:25  jimz
 * allow rewrite parity on any fault-tolerant arch
 *
 * Revision 1.44  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.43  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.42  1996/07/22  21:12:01  jimz
 * clean up parity scan status printing
 *
 * Revision 1.41  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.40  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.39  1996/07/09  21:44:26  jimz
 * fix bogus return code in VerifyParityBasic when a stripe can't be corrected
 *
 * Revision 1.38  1996/06/20  17:56:57  jimz
 * update VerifyParity to check complete AccessStripeMaps
 *
 * Revision 1.37  1996/06/19  22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.36  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.35  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.34  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.33  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.32  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.31  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.30  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.29  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.28  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.27  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.26  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.25  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.24  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.23  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.22  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.21  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.20  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.19  1995/11/30  16:16:49  wvcii
 * added copyright info
 *
 * Revision 1.18  1995/11/19  16:32:19  wvcii
 * eliminated initialization of dag header fields which no longer exist
 * (numDags, numDagsDone, firstHdr)
 *
 * Revision 1.17  1995/11/07  16:23:36  wvcii
 * added comments, asserts, and prototypes
 * encoded commit point nodes, barrier, and antecedents types into dags
 *
 */

#include "rf_types.h"
#include "rf_raid.h"
#include "rf_dag.h"
#include "rf_dagfuncs.h"
#include "rf_dagutils.h"
#include "rf_mcpair.h"
#include "rf_general.h"
#include "rf_engine.h"
#include "rf_parityscan.h"
#include "rf_map.h"
#include "rf_sys.h"

/*****************************************************************************************
 *
 * walk through the entire arry and write new parity.
 * This works by creating two DAGs, one to read a stripe of data and one to
 * write new parity.  The first is executed, the data is xored together, and
 * then the second is executed.  To avoid constantly building and tearing down
 * the DAGs, we create them a priori and fill them in with the mapping
 * information as we go along.
 *
 * there should never be more than one thread running this.
 *
 ****************************************************************************************/

int rf_RewriteParity(raidPtr)
  RF_Raid_t  *raidPtr;
{
  RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
  RF_AccessStripeMapHeader_t *asm_h;
  int old_pctg, new_pctg, rc;
  RF_PhysDiskAddr_t pda;
  RF_SectorNum_t i;

  pda.startSector = 0;
  pda.numSector   = raidPtr->Layout.sectorsPerStripeUnit;
  old_pctg = -1;

/* rf_verifyParityDebug=1; */
  for (i=0; i<raidPtr->totalSectors; i+=layoutPtr->dataSectorsPerStripe) {
    asm_h = rf_MapAccess(raidPtr, i, layoutPtr->dataSectorsPerStripe, NULL, RF_DONT_REMAP);
    rc = rf_VerifyParity(raidPtr, asm_h->stripeMap, 1, 0);
    /*     printf("Parity verified: rc=%d\n",rc); */
    switch (rc) {
      case RF_PARITY_OKAY:
      case RF_PARITY_CORRECTED:
        break;
      case RF_PARITY_BAD:
        printf("Parity bad during correction\n");
        RF_PANIC();
        break;
      case RF_PARITY_COULD_NOT_CORRECT:
        printf("Could not correct bad parity\n");
        RF_PANIC();
        break;
      case RF_PARITY_COULD_NOT_VERIFY:
        printf("Could not verify parity\n");
        RF_PANIC();
        break;
      default:
        printf("Bad rc=%d from VerifyParity in RewriteParity\n", rc);
        RF_PANIC();
    }
    rf_FreeAccessStripeMap(asm_h);
    new_pctg = i*1000/raidPtr->totalSectors;
    if (new_pctg != old_pctg) {
#ifndef KERNEL
      fprintf(stderr,"\rParity rewrite: %d.%d%% complete",
        new_pctg/10, new_pctg%10);
      fflush(stderr);
#endif /* !KERNEL */
    }
    old_pctg = new_pctg;
  }
#ifndef KERNEL
  fprintf(stderr,"\rParity rewrite: 100.0%% complete\n");
#endif /* !KERNEL */
#if 1
  return(0); /* XXX nothing was here.. GO */
#endif
}

/*****************************************************************************************
 *
 * verify that the parity in a particular stripe is correct.
 * we validate only the range of parity defined by parityPDA, since
 * this is all we have locked.  The way we do this is to create an asm
 * that maps the whole stripe and then range-restrict it to the parity
 * region defined by the parityPDA.
 *
 ****************************************************************************************/
int rf_VerifyParity(raidPtr, aasm, correct_it, flags)
  RF_Raid_t             *raidPtr;
  RF_AccessStripeMap_t  *aasm;
  int                    correct_it;
  RF_RaidAccessFlags_t   flags;
{
  RF_PhysDiskAddr_t *parityPDA;
  RF_AccessStripeMap_t *doasm;
  RF_LayoutSW_t *lp;
  int lrc, rc;

  lp = raidPtr->Layout.map;
  if (lp->faultsTolerated == 0) {
    /*
     * There isn't any parity. Call it "okay."
     */
    return(RF_PARITY_OKAY);
  }
  rc = RF_PARITY_OKAY;
  if (lp->VerifyParity) {
    for(doasm=aasm;doasm;doasm=doasm->next) {
      for(parityPDA=doasm->parityInfo;parityPDA;parityPDA=parityPDA->next) {
        lrc = lp->VerifyParity(raidPtr, doasm->raidAddress, parityPDA,
          correct_it, flags);
        if (lrc > rc) {
          /* see rf_parityscan.h for why this works */
          rc = lrc;
        }
      }
    }
  }
  else {
    rc = RF_PARITY_COULD_NOT_VERIFY;
  }
  return(rc);
}

int rf_VerifyParityBasic(raidPtr, raidAddr, parityPDA, correct_it, flags)
  RF_Raid_t             *raidPtr;
  RF_RaidAddr_t          raidAddr;
  RF_PhysDiskAddr_t     *parityPDA;
  int                    correct_it;
  RF_RaidAccessFlags_t   flags;
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_RaidAddr_t startAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, raidAddr);
  RF_SectorCount_t numsector = parityPDA->numSector;
  int numbytes  = rf_RaidAddressToByte(raidPtr, numsector);
  int bytesPerStripe = numbytes * layoutPtr->numDataCol;
  RF_DagHeader_t *rd_dag_h, *wr_dag_h;          /* read, write dag */
  RF_DagNode_t *blockNode, *unblockNode, *wrBlock, *wrUnblock;
  RF_AccessStripeMapHeader_t *asm_h;
  RF_AccessStripeMap_t *asmap;
  RF_AllocListElem_t *alloclist;
  RF_PhysDiskAddr_t *pda;
  char *pbuf, *buf, *end_p, *p;
  int i, retcode;
  RF_ReconUnitNum_t which_ru;
  RF_StripeNum_t psID = rf_RaidAddressToParityStripeID(layoutPtr, raidAddr, &which_ru);
  int stripeWidth = layoutPtr->numDataCol + layoutPtr->numParityCol;
  RF_AccTraceEntry_t tracerec;
  RF_MCPair_t *mcpair;

  retcode = RF_PARITY_OKAY;

  mcpair = rf_AllocMCPair();
  rf_MakeAllocList(alloclist);
  RF_MallocAndAdd(buf, numbytes * (layoutPtr->numDataCol + layoutPtr->numParityCol), (char *), alloclist);
  RF_CallocAndAdd(pbuf, 1, numbytes, (char *), alloclist);     /* use calloc to make sure buffer is zeroed */
  end_p = buf + bytesPerStripe;

  rd_dag_h = rf_MakeSimpleDAG(raidPtr, stripeWidth, numbytes, buf, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			   "Rod", alloclist, flags, RF_IO_NORMAL_PRIORITY);
  blockNode = rd_dag_h->succedents[0];
  unblockNode = blockNode->succedents[0]->succedents[0];

  /* map the stripe and fill in the PDAs in the dag */
  asm_h = rf_MapAccess(raidPtr, startAddr, layoutPtr->dataSectorsPerStripe, buf, RF_DONT_REMAP);
  asmap = asm_h->stripeMap;
  
  for (pda=asmap->physInfo,i=0; i<layoutPtr->numDataCol; i++,pda=pda->next) {
    RF_ASSERT(pda);
    rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);
    RF_ASSERT(pda->numSector != 0);
    if (rf_TryToRedirectPDA(raidPtr, pda, 0)) goto out;   /* no way to verify parity if disk is dead.  return w/ good status */
    blockNode->succedents[i]->params[0].p = pda;
    blockNode->succedents[i]->params[2].v = psID;
    blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
  }

  RF_ASSERT(!asmap->parityInfo->next);
  rf_RangeRestrictPDA(raidPtr, parityPDA, asmap->parityInfo, 0, 1);
  RF_ASSERT(asmap->parityInfo->numSector != 0);
  if (rf_TryToRedirectPDA(raidPtr, asmap->parityInfo, 1))
    goto out;
  blockNode->succedents[layoutPtr->numDataCol]->params[0].p = asmap->parityInfo;

  /* fire off the DAG */
  bzero((char *)&tracerec,sizeof(tracerec));
  rd_dag_h->tracerec = &tracerec;

  if (rf_verifyParityDebug) {
    printf("Parity verify read dag:\n");
    rf_PrintDAGList(rd_dag_h);
  }

  RF_LOCK_MUTEX(mcpair->mutex);
  mcpair->flag = 0;
  rf_DispatchDAG(rd_dag_h, (void (*)(void *))rf_MCPairWakeupFunc, 
		 (void *) mcpair);
  while (!mcpair->flag) 
	  RF_WAIT_COND(mcpair->cond, mcpair->mutex);
  RF_UNLOCK_MUTEX(mcpair->mutex);
  if (rd_dag_h->status != rf_enable) {
    RF_ERRORMSG("Unable to verify parity:  can't read the stripe\n");
    retcode = RF_PARITY_COULD_NOT_VERIFY;
    goto out;
  }

  for (p=buf; p<end_p; p+=numbytes) {
    rf_bxor(p, pbuf, numbytes, NULL);
  }
  for (i=0; i<numbytes; i++) {
#if 0
	  if (pbuf[i]!=0 || buf[bytesPerStripe+i]!=0) {
	  printf("Bytes: %d %d %d\n",i,pbuf[i],buf[bytesPerStripe+i]);
	  }
#endif
	  if (pbuf[i] != buf[bytesPerStripe+i]) {
		  if (!correct_it) 
			  RF_ERRORMSG3("Parity verify error: byte %d of parity is 0x%x should be 0x%x\n",
			       i,(u_char) buf[bytesPerStripe+i],(u_char) pbuf[i]);
		  retcode = RF_PARITY_BAD;
		  break;
	  }
  }

  if (retcode && correct_it) {
    wr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, numbytes, pbuf, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			     "Wnp", alloclist, flags, RF_IO_NORMAL_PRIORITY);
    wrBlock = wr_dag_h->succedents[0]; wrUnblock = wrBlock->succedents[0]->succedents[0];
    wrBlock->succedents[0]->params[0].p = asmap->parityInfo;
    wrBlock->succedents[0]->params[2].v = psID;
    wrBlock->succedents[0]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
    bzero((char *)&tracerec,sizeof(tracerec));
    wr_dag_h->tracerec = &tracerec;
    if (rf_verifyParityDebug) {
      printf("Parity verify write dag:\n");
      rf_PrintDAGList(wr_dag_h);
    }
    RF_LOCK_MUTEX(mcpair->mutex);
    mcpair->flag = 0;
    rf_DispatchDAG(wr_dag_h, (void (*)(void *))rf_MCPairWakeupFunc, 
		   (void *) mcpair);
    while (!mcpair->flag)
      RF_WAIT_COND(mcpair->cond, mcpair->mutex);
    RF_UNLOCK_MUTEX(mcpair->mutex);
    if (wr_dag_h->status != rf_enable) {
      RF_ERRORMSG("Unable to correct parity in VerifyParity:  can't write the stripe\n");
      retcode = RF_PARITY_COULD_NOT_CORRECT;
    }
    rf_FreeDAG(wr_dag_h);
    if (retcode == RF_PARITY_BAD)
      retcode = RF_PARITY_CORRECTED;
  }

out:
  rf_FreeAccessStripeMap(asm_h);
  rf_FreeAllocList(alloclist);
  rf_FreeDAG(rd_dag_h);
  rf_FreeMCPair(mcpair);
  return(retcode);
}

int rf_TryToRedirectPDA(raidPtr, pda, parity)
  RF_Raid_t          *raidPtr;
  RF_PhysDiskAddr_t  *pda;
  int                 parity;
{
  if (raidPtr->Disks[pda->row][pda->col].status == rf_ds_reconstructing) {
    if (rf_CheckRUReconstructed(raidPtr->reconControl[pda->row]->reconMap, pda->startSector)) {
      if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
	RF_RowCol_t or = pda->row, oc = pda->col;
	RF_SectorNum_t os = pda->startSector;
	if (parity) {
	  (raidPtr->Layout.map->MapParity)(raidPtr, pda->raidAddress, &pda->row, &pda->col, &pda->startSector, RF_REMAP);
	  if (rf_verifyParityDebug) printf("VerifyParity: Redir P r %d c %d sect %ld -> r %d c %d sect %ld\n",
					or,oc,(long)os,pda->row,pda->col,(long)pda->startSector);
	} else {
	  (raidPtr->Layout.map->MapSector)(raidPtr, pda->raidAddress, &pda->row, &pda->col, &pda->startSector, RF_REMAP);
	  if (rf_verifyParityDebug) printf("VerifyParity: Redir D r %d c %d sect %ld -> r %d c %d sect %ld\n",
					or,oc,(long)os,pda->row,pda->col,(long)pda->startSector);
	}
      } else {
	RF_RowCol_t spRow = raidPtr->Disks[pda->row][pda->col].spareRow;
	RF_RowCol_t spCol = raidPtr->Disks[pda->row][pda->col].spareCol;
	pda->row = spRow;
	pda->col = spCol;
      }
    }
  }
  if (RF_DEAD_DISK(raidPtr->Disks[pda->row][pda->col].status)) return(1);
  return(0);
}

/*****************************************************************************************
 *
 * currently a stub.
 *
 * takes as input an ASM describing a write operation and containing one failure, and
 * verifies that the parity was correctly updated to reflect the write.
 *
 * if it's a data unit that's failed, we read the other data units in the stripe and
 * the parity unit, XOR them together, and verify that we get the data intended for
 * the failed disk.  Since it's easy, we also validate that the right data got written
 * to the surviving data disks.
 *
 * If it's the parity that failed, there's really no validation we can do except the
 * above verification that the right data got written to all disks.  This is because
 * the new data intended for the failed disk is supplied in the ASM, but this is of
 * course not the case for the new parity.
 *
 ****************************************************************************************/
int rf_VerifyDegrModeWrite(raidPtr, asmh)
  RF_Raid_t                   *raidPtr;
  RF_AccessStripeMapHeader_t  *asmh;
{
  return(0);
}

/* creates a simple DAG with a header, a block-recon node at level 1,
 * nNodes nodes at level 2, an unblock-recon node at level 3, and
 * a terminator node at level 4.  The stripe address field in
 * the block and unblock nodes are not touched, nor are the pda
 * fields in the second-level nodes, so they must be filled in later.
 *
 * commit point is established at unblock node - this means that any
 * failure during dag execution causes the dag to fail
 */
RF_DagHeader_t *rf_MakeSimpleDAG(raidPtr, nNodes, bytesPerSU, databuf, doFunc, undoFunc, name, alloclist, flags, priority)
  RF_Raid_t              *raidPtr;
  int                     nNodes;
  int                     bytesPerSU;
  char                   *databuf;
  int                   (*doFunc)(RF_DagNode_t *node);
  int                   (*undoFunc)(RF_DagNode_t *node);
  char                   *name;        /* node names at the second level */
  RF_AllocListElem_t     *alloclist;
  RF_RaidAccessFlags_t    flags;
  int                     priority;
{
  RF_DagHeader_t *dag_h;
  RF_DagNode_t *nodes, *termNode, *blockNode, *unblockNode;
  int i;
  
  /* create the nodes, the block & unblock nodes, and the terminator node */
  RF_CallocAndAdd(nodes, nNodes+3, sizeof(RF_DagNode_t), (RF_DagNode_t *), alloclist);
  blockNode   = &nodes[nNodes];
  unblockNode = blockNode+1;
  termNode   = unblockNode+1;

  dag_h = rf_AllocDAGHeader();
  dag_h->raidPtr = (void *) raidPtr;
  dag_h->allocList = NULL;                               /* we won't use this alloc list */
  dag_h->status = rf_enable;
  dag_h->numSuccedents = 1;
  dag_h->creator = "SimpleDAG";

  /* this dag can not commit until the unblock node is reached
   * errors prior to the commit point imply the dag has failed
   */
  dag_h->numCommitNodes = 1;
  dag_h->numCommits = 0;

  dag_h->succedents[0] = blockNode;
  rf_InitNode(blockNode,   rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nNodes, 0, 0, 0, dag_h, "Nil", alloclist);
  rf_InitNode(unblockNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nNodes, 0, 0, dag_h, "Nil", alloclist);
  unblockNode->succedents[0] = termNode;
  for (i=0; i<nNodes; i++) {
    blockNode->succedents[i] = unblockNode->antecedents[i] = &nodes[i];
    unblockNode->antType[i] = rf_control;
    rf_InitNode(&nodes[i], rf_wait, RF_FALSE, doFunc, undoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, name, alloclist);
    nodes[i].succedents[0] =  unblockNode;
    nodes[i].antecedents[0] = blockNode;
    nodes[i].antType[0] = rf_control;
    nodes[i].params[1].p = (databuf + (i*bytesPerSU));
  }
  rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", alloclist);
  termNode->antecedents[0] = unblockNode;
  termNode->antType[0] = rf_control;
  return(dag_h);
}
