/*	$OpenBSD: rf_layout.c,v 1.1 1999/01/11 14:29:27 niklas Exp $	*/
/*	$NetBSD: rf_layout.c,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* rf_layout.c -- driver code dealing with layout and mapping issues
 */

/*
 * :  
 * Log: rf_layout.c,v 
 * Revision 1.71  1996/08/20 22:41:30  jimz
 * add declustered evenodd
 *
 * Revision 1.70  1996/07/31  16:56:18  jimz
 * dataBytesPerStripe, sectorsPerDisk init arch-indep.
 *
 * Revision 1.69  1996/07/31  15:34:46  jimz
 * add EvenOdd
 *
 * Revision 1.68  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.67  1996/07/27  23:36:08  jimz
 * Solaris port of simulator
 *
 * Revision 1.66  1996/07/27  18:40:24  jimz
 * cleanup sweep
 *
 * Revision 1.65  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.64  1996/07/15  17:22:18  jimz
 * nit-pick code cleanup
 * resolve stdlib problems on DEC OSF
 *
 * Revision 1.63  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.62  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.61  1996/06/19  22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.60  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.59  1996/06/19  14:57:58  jimz
 * move layout-specific config parsing hooks into RF_LayoutSW_t
 * table in rf_layout.c
 *
 * Revision 1.58  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.57  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.56  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.55  1996/06/06  18:41:35  jimz
 * change interleaved declustering dag selection to an
 * interleaved-declustering-specific routine (so we can
 * use the partitioned mirror node)
 *
 * Revision 1.54  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.53  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.52  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.51  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.50  1996/05/30  23:22:16  jimz
 * bugfixes of serialization, timing problems
 * more cleanup
 *
 * Revision 1.49  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.48  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.47  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.46  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.45  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.44  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.43  1996/02/22  16:46:35  amiri
 * modified chained declustering to use a seperate DAG selection routine
 *
 * Revision 1.42  1995/12/01  19:16:11  root
 * added copyright info
 *
 * Revision 1.41  1995/11/28  21:31:02  amiri
 * added Interleaved Declustering to switch table
 *
 * Revision 1.40  1995/11/20  14:35:17  arw
 * moved rf_StartThroughputStats in DefaultWrite and DefaultRead
 *
 * Revision 1.39  1995/11/19  16:28:46  wvcii
 * replaced LaunchDAGState with CreateDAGState, ExecuteDAGState
 *
 * Revision 1.38  1995/11/17  19:00:41  wvcii
 * added MapQ entries to switch table
 *
 * Revision 1.37  1995/11/17  16:58:13  amiri
 * Added the Chained Declustering architecture ('C'),
 * essentially a variant of mirroring.
 *
 * Revision 1.36  1995/11/16  16:16:10  amiri
 * Added RAID5 with rotated sparing ('R' configuration)
 *
 * Revision 1.35  1995/11/07  15:41:17  wvcii
 * modified state lists: DefaultStates, VSReadStates
 * necessary to support new states (LaunchDAGState, ProcessDAGState)
 *
 * Revision 1.34  1995/10/18  01:23:20  amiri
 * added ifndef SIMULATE wrapper around rf_StartThroughputStats()
 *
 * Revision 1.33  1995/10/13  15:05:46  arw
 * added rf_StartThroughputStats to DefaultRead and DefaultWrite
 *
 * Revision 1.32  1995/10/12  16:04:23  jimz
 * added config names to mapsw entires
 *
 * Revision 1.31  1995/10/04  03:57:48  wvcii
 * added raid level 1 to mapsw
 *
 * Revision 1.30  1995/09/07  01:26:55  jimz
 * Achive basic compilation in kernel. Kernel functionality
 * is not guaranteed at all, but it'll compile. Mostly. I hope.
 *
 * Revision 1.29  1995/07/28  21:43:42  robby
 * checkin after leaving for Rice. Bye
 *
 * Revision 1.28  1995/07/26  03:26:14  robby
 * *** empty log message ***
 *
 * Revision 1.27  1995/07/21  19:47:52  rachad
 * Added raid 0 /5 with caching architectures
 *
 * Revision 1.26  1995/07/21  19:29:27  robby
 * added virtual striping states
 *
 * Revision 1.25  1995/07/10  21:41:47  robby
 * switched to have my own virtual stripng write function from the cache
 *
 * Revision 1.24  1995/07/10  20:51:59  robby
 * added virtual striping states
 *
 * Revision 1.23  1995/07/10  16:57:42  robby
 * updated alloclistelem struct to the correct struct name
 *
 * Revision 1.22  1995/07/08  20:06:11  rachad
 * *** empty log message ***
 *
 * Revision 1.21  1995/07/08  19:43:16  cfb
 * *** empty log message ***
 *
 * Revision 1.20  1995/07/08  18:05:39  rachad
 * Linked up Claudsons code with the real cache
 *
 * Revision 1.19  1995/07/06  14:29:36  robby
 * added defaults states list to the layout switch
 *
 * Revision 1.18  1995/06/23  13:40:34  robby
 * updeated to prototypes in rf_layout.h
 *
 */

#include "rf_types.h"
#include "rf_archs.h"
#include "rf_raid.h"
#include "rf_configure.h"
#include "rf_dag.h"
#include "rf_desc.h"
#include "rf_decluster.h"
#include "rf_pq.h"
#include "rf_declusterPQ.h"
#include "rf_raid0.h"
#include "rf_raid1.h"
#include "rf_raid4.h"
#include "rf_raid5.h"
#include "rf_states.h"
#if RF_INCLUDE_RAID5_RS > 0
#include "rf_raid5_rotatedspare.h"
#endif /* RF_INCLUDE_RAID5_RS > 0 */
#if RF_INCLUDE_CHAINDECLUSTER > 0
#include "rf_chaindecluster.h"
#endif /* RF_INCLUDE_CHAINDECLUSTER > 0 */
#if RF_INCLUDE_INTERDECLUSTER > 0
#include "rf_interdecluster.h"
#endif /* RF_INCLUDE_INTERDECLUSTER > 0 */
#if RF_INCLUDE_PARITYLOGGING > 0
#include "rf_paritylogging.h"
#endif /* RF_INCLUDE_PARITYLOGGING > 0 */
#if RF_INCLUDE_EVENODD > 0
#include "rf_evenodd.h"
#endif /* RF_INCLUDE_EVENODD > 0 */
#include "rf_general.h"
#include "rf_driver.h"
#include "rf_parityscan.h"
#include "rf_reconbuffer.h"
#include "rf_reconutil.h"

/***********************************************************************
 *
 * the layout switch defines all the layouts that are supported.
 *    fields are: layout ID, init routine, shutdown routine, map
 *    sector, map parity, identify stripe, dag selection, map stripeid
 *    to parity stripe id (optional), num faults tolerated, special
 *    flags.
 *
 ***********************************************************************/

static RF_AccessState_t DefaultStates[] = {rf_QuiesceState,
	rf_IncrAccessesCountState, rf_MapState, rf_LockState, rf_CreateDAGState,
	rf_ExecuteDAGState, rf_ProcessDAGState, rf_DecrAccessesCountState,
	rf_CleanupState, rf_LastState};

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && !defined(_KERNEL)
/* XXX Gross hack to shutup gcc -- it complains that DefaultStates is not 
used when compiling this in userland..  I hate to burst it's bubble, but
DefaultStates is used all over the place here in the initialization of
lots of data structures.  GO */
RF_AccessState_t *NothingAtAll = DefaultStates;
#endif

#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
/* XXX Remove static so GCC doesn't complain about these being unused! */
int distSpareYes = 1;
int distSpareNo  = 0;
#else
static int distSpareYes = 1;
static int distSpareNo  = 0;
#endif
#ifdef KERNEL
#define RF_NK2(a,b)
#else /* KERNEL */
#define RF_NK2(a,b) a,b,
#endif /* KERNEL */

#if RF_UTILITY > 0
#define RF_NU(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)
#else /* RF_UTILITY > 0 */
#define RF_NU(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#endif /* RF_UTILITY > 0 */

static RF_LayoutSW_t mapsw[] = {
	/* parity declustering */
	{'T', "Parity declustering",
	RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareNo)
	RF_NU(
	rf_ConfigureDeclustered,
	rf_MapSectorDeclustered, rf_MapParityDeclustered, NULL,
	rf_IdentifyStripeDeclustered,
	rf_RaidFiveDagSelect,
	rf_MapSIDToPSIDDeclustered,
	rf_GetDefaultHeadSepLimitDeclustered,
	rf_GetDefaultNumFloatingReconBuffersDeclustered,
	NULL, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	0)
	},

	/* parity declustering with distributed sparing */
	{'D', "Distributed sparing parity declustering",
	RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareYes)
	RF_NU(
	rf_ConfigureDeclusteredDS,
	rf_MapSectorDeclustered, rf_MapParityDeclustered, NULL,
	rf_IdentifyStripeDeclustered,
	rf_RaidFiveDagSelect,
	rf_MapSIDToPSIDDeclustered,
	rf_GetDefaultHeadSepLimitDeclustered,
	rf_GetDefaultNumFloatingReconBuffersDeclustered,
	rf_GetNumSpareRUsDeclustered, rf_InstallSpareTable,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	RF_DISTRIBUTE_SPARE|RF_BD_DECLUSTERED)
	},

#if RF_INCLUDE_DECL_PQ > 0
	/* declustered P+Q */
	{'Q', "Declustered P+Q",
	RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareNo)
	RF_NU(
	rf_ConfigureDeclusteredPQ,
	rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ, rf_MapQDeclusteredPQ,
	rf_IdentifyStripeDeclusteredPQ,
	rf_PQDagSelect,
	rf_MapSIDToPSIDDeclustered,
	rf_GetDefaultHeadSepLimitDeclustered,
	rf_GetDefaultNumFloatingReconBuffersPQ,
	NULL, NULL,
	NULL,
	rf_VerifyParityBasic,
	2,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_DECL_PQ > 0 */

#if RF_INCLUDE_RAID5_RS > 0
	/* RAID 5 with rotated sparing */
	{'R', "RAID Level 5 rotated sparing",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureRAID5_RS,
	rf_MapSectorRAID5_RS, rf_MapParityRAID5_RS, NULL,
	rf_IdentifyStripeRAID5_RS,
	rf_RaidFiveDagSelect,
	rf_MapSIDToPSIDRAID5_RS,
	rf_GetDefaultHeadSepLimitRAID5,
	rf_GetDefaultNumFloatingReconBuffersRAID5,
	rf_GetNumSpareRUsRAID5_RS, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	RF_DISTRIBUTE_SPARE)
	},
#endif /* RF_INCLUDE_RAID5_RS > 0 */

#if RF_INCLUDE_CHAINDECLUSTER > 0
	/* Chained Declustering */
	{'C', "Chained Declustering",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureChainDecluster,
	rf_MapSectorChainDecluster, rf_MapParityChainDecluster, NULL,
	rf_IdentifyStripeChainDecluster,
	rf_RAIDCDagSelect,
	rf_MapSIDToPSIDChainDecluster,
	NULL,
	NULL,
	rf_GetNumSpareRUsChainDecluster, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_CHAINDECLUSTER > 0 */

#if RF_INCLUDE_INTERDECLUSTER > 0
	/* Interleaved Declustering */
	{'I', "Interleaved Declustering",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureInterDecluster,
	rf_MapSectorInterDecluster, rf_MapParityInterDecluster, NULL,
	rf_IdentifyStripeInterDecluster,
	rf_RAIDIDagSelect,
	rf_MapSIDToPSIDInterDecluster,
	rf_GetDefaultHeadSepLimitInterDecluster,
	rf_GetDefaultNumFloatingReconBuffersInterDecluster,
	rf_GetNumSpareRUsInterDecluster, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	RF_DISTRIBUTE_SPARE)
	},
#endif /* RF_INCLUDE_INTERDECLUSTER > 0 */

#if RF_INCLUDE_RAID0 > 0
	/* RAID level 0 */
	{'0', "RAID Level 0",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureRAID0,
	rf_MapSectorRAID0, rf_MapParityRAID0, NULL,
	rf_IdentifyStripeRAID0,
	rf_RAID0DagSelect,
	rf_MapSIDToPSIDRAID0,
	NULL,
	NULL,
	NULL, NULL,
	NULL,
	rf_VerifyParityRAID0,
	0,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_RAID0 > 0 */

#if RF_INCLUDE_RAID1 > 0
	/* RAID level 1 */
	{'1', "RAID Level 1",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureRAID1,
	rf_MapSectorRAID1, rf_MapParityRAID1, NULL,
	rf_IdentifyStripeRAID1,
	rf_RAID1DagSelect,
	rf_MapSIDToPSIDRAID1,
	NULL,
	NULL,
	NULL, NULL,
	rf_SubmitReconBufferRAID1,
	rf_VerifyParityRAID1,
	1,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_RAID1 > 0 */

#if RF_INCLUDE_RAID4 > 0
	/* RAID level 4 */
	{'4', "RAID Level 4",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureRAID4,
	rf_MapSectorRAID4, rf_MapParityRAID4, NULL,
	rf_IdentifyStripeRAID4,
	rf_RaidFiveDagSelect,
	rf_MapSIDToPSIDRAID4,
	rf_GetDefaultHeadSepLimitRAID4,
	rf_GetDefaultNumFloatingReconBuffersRAID4,
	NULL, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_RAID4 > 0 */

#if RF_INCLUDE_RAID5 > 0
	/* RAID level 5 */
	{'5', "RAID Level 5",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureRAID5,
	rf_MapSectorRAID5, rf_MapParityRAID5, NULL,
	rf_IdentifyStripeRAID5,
	rf_RaidFiveDagSelect,
	rf_MapSIDToPSIDRAID5,
	rf_GetDefaultHeadSepLimitRAID5,
	rf_GetDefaultNumFloatingReconBuffersRAID5,
	NULL, NULL,
	rf_SubmitReconBufferBasic,
	rf_VerifyParityBasic,
	1,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_RAID5 > 0 */

#if RF_INCLUDE_EVENODD > 0
	/* Evenodd */
	{'E', "EvenOdd",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureEvenOdd,
	rf_MapSectorRAID5, rf_MapParityEvenOdd, rf_MapEEvenOdd, 
	rf_IdentifyStripeEvenOdd,
	rf_EODagSelect,
	rf_MapSIDToPSIDRAID5,
	NULL,
	NULL,
	NULL, NULL,
	NULL, /* no reconstruction, yet */
	rf_VerifyParityEvenOdd,
	2,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_EVENODD > 0 */

#if RF_INCLUDE_EVENODD > 0
	/* Declustered Evenodd */
	{'e', "Declustered EvenOdd",
	RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareNo)
	RF_NU(
	rf_ConfigureDeclusteredPQ,
	rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ, rf_MapQDeclusteredPQ,
	rf_IdentifyStripeDeclusteredPQ,
	rf_EODagSelect,
	rf_MapSIDToPSIDRAID5,
	rf_GetDefaultHeadSepLimitDeclustered,
	rf_GetDefaultNumFloatingReconBuffersPQ,
	NULL, NULL,
	NULL, /* no reconstruction, yet */
	rf_VerifyParityEvenOdd,
	2,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_EVENODD > 0 */

#if RF_INCLUDE_PARITYLOGGING > 0
	/* parity logging */
	{'L', "Parity logging",
	RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
	RF_NU(
	rf_ConfigureParityLogging,
	rf_MapSectorParityLogging, rf_MapParityParityLogging, NULL,
	rf_IdentifyStripeParityLogging,
	rf_ParityLoggingDagSelect,
	rf_MapSIDToPSIDParityLogging,
	rf_GetDefaultHeadSepLimitParityLogging,
	rf_GetDefaultNumFloatingReconBuffersParityLogging,
	NULL, NULL,
	rf_SubmitReconBufferBasic,
	NULL,
	1,
	DefaultStates,
	0)
	},
#endif /* RF_INCLUDE_PARITYLOGGING > 0 */

	/* end-of-list marker */
	{ '\0', NULL,
	RF_NK2(NULL, NULL)
	RF_NU(
	NULL,
	NULL, NULL, NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, NULL,
	NULL,
	NULL,
	0,
	NULL,
	0)
	}
};

RF_LayoutSW_t *rf_GetLayout(RF_ParityConfig_t parityConfig)
{
  RF_LayoutSW_t *p;

  /* look up the specific layout */
  for (p=&mapsw[0]; p->parityConfig; p++) 
    if (p->parityConfig == parityConfig)
      break;
  if (!p->parityConfig)
    return(NULL);
  RF_ASSERT(p->parityConfig == parityConfig);
  return(p);
}

#if RF_UTILITY == 0
/*****************************************************************************************
 *
 * ConfigureLayout -- 
 *
 * read the configuration file and set up the RAID layout parameters.  After reading
 * common params, invokes the layout-specific configuration routine to finish
 * the configuration.
 *
 ****************************************************************************************/
int rf_ConfigureLayout(
  RF_ShutdownList_t  **listp,
  RF_Raid_t           *raidPtr,
  RF_Config_t         *cfgPtr)
{
  RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
  RF_ParityConfig_t parityConfig;
  RF_LayoutSW_t *p;
  int retval;

  layoutPtr->sectorsPerStripeUnit = cfgPtr->sectPerSU;
  layoutPtr->SUsPerPU             = cfgPtr->SUsPerPU;
  layoutPtr->SUsPerRU             = cfgPtr->SUsPerRU;
  parityConfig                    = cfgPtr->parityConfig;
    
  layoutPtr->stripeUnitsPerDisk = raidPtr->sectorsPerDisk / layoutPtr->sectorsPerStripeUnit;

  p = rf_GetLayout(parityConfig);
  if (p == NULL) {
    RF_ERRORMSG1("Unknown parity configuration '%c'", parityConfig);
    return(EINVAL);
  }
  RF_ASSERT(p->parityConfig == parityConfig);
  layoutPtr->map = p;

  /* initialize the specific layout */

  retval = (p->Configure)(listp, raidPtr, cfgPtr);

  if (retval)
    return(retval);

  layoutPtr->dataBytesPerStripe = layoutPtr->dataSectorsPerStripe << raidPtr->logBytesPerSector;
  raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

  if (rf_forceNumFloatingReconBufs >= 0) {
    raidPtr->numFloatingReconBufs = rf_forceNumFloatingReconBufs;
  }
  else {
    raidPtr->numFloatingReconBufs = rf_GetDefaultNumFloatingReconBuffers(raidPtr);
  }

  if (rf_forceHeadSepLimit >= 0) {
    raidPtr->headSepLimit = rf_forceHeadSepLimit;
  }
  else {
    raidPtr->headSepLimit = rf_GetDefaultHeadSepLimit(raidPtr);
  }

  printf("RAIDFRAME: Configure (%s): total number of sectors is %lu (%lu MB)\n",
     layoutPtr->map->configName,
     (unsigned long)raidPtr->totalSectors,
     (unsigned long)(raidPtr->totalSectors / 1024 * (1<<raidPtr->logBytesPerSector) / 1024));
  if (raidPtr->headSepLimit >= 0) {
    printf("RAIDFRAME(%s): Using %ld floating recon bufs with head sep limit %ld\n",
      layoutPtr->map->configName, (long)raidPtr->numFloatingReconBufs, (long)raidPtr->headSepLimit);
  }
  else {
    printf("RAIDFRAME(%s): Using %ld floating recon bufs with no head sep limit\n",
      layoutPtr->map->configName, (long)raidPtr->numFloatingReconBufs);
  }

  return(0);
}

/* typically there is a 1-1 mapping between stripes and parity stripes.
 * however, the declustering code supports packing multiple stripes into
 * a single parity stripe, so as to increase the size of the reconstruction
 * unit without affecting the size of the stripe unit.  This routine finds
 * the parity stripe identifier associated with a stripe ID.  There is also
 * a RaidAddressToParityStripeID macro in layout.h
 */
RF_StripeNum_t rf_MapStripeIDToParityStripeID(layoutPtr, stripeID, which_ru)
  RF_RaidLayout_t    *layoutPtr;
  RF_StripeNum_t      stripeID;
  RF_ReconUnitNum_t  *which_ru;
{
  RF_StripeNum_t parityStripeID;

  /* quick exit in the common case of SUsPerPU==1 */
  if ((layoutPtr->SUsPerPU == 1) || !layoutPtr->map->MapSIDToPSID) {
    *which_ru = 0;
    return(stripeID);
  }
  else {
    (layoutPtr->map->MapSIDToPSID)(layoutPtr, stripeID, &parityStripeID, which_ru);
  }
  return(parityStripeID);
}
#endif /* RF_UTILITY == 0 */
