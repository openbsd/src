/*	$OpenBSD: rf_layout.c,v 1.7 2007/04/10 17:47:55 miod Exp $	*/
/*	$NetBSD: rf_layout.c,v 1.6 2000/04/17 19:35:12 oster Exp $	*/

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

/*
 * rf_layout.c -- Driver code dealing with layout and mapping issues.
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
#if	RF_INCLUDE_RAID5_RS > 0
#include "rf_raid5_rotatedspare.h"
#endif	/* RF_INCLUDE_RAID5_RS > 0 */
#if	RF_INCLUDE_CHAINDECLUSTER > 0
#include "rf_chaindecluster.h"
#endif	/* RF_INCLUDE_CHAINDECLUSTER > 0 */
#if	RF_INCLUDE_INTERDECLUSTER > 0
#include "rf_interdecluster.h"
#endif	/* RF_INCLUDE_INTERDECLUSTER > 0 */
#if	RF_INCLUDE_PARITYLOGGING > 0
#include "rf_paritylogging.h"
#endif	/* RF_INCLUDE_PARITYLOGGING > 0 */
#if	RF_INCLUDE_EVENODD > 0
#include "rf_evenodd.h"
#endif	/* RF_INCLUDE_EVENODD > 0 */
#include "rf_general.h"
#include "rf_driver.h"
#include "rf_parityscan.h"
#include "rf_reconbuffer.h"
#include "rf_reconutil.h"

/*****************************************************************************
 *
 * The layout switch defines all the layouts that are supported.
 * Fields are:	layout ID, init routine, shutdown routine, map sector,
 *		map parity, identify stripe, dag selection, map stripeid
 *		to parity stripe id (optional), num faults tolerated,
 *		special flags.
 *
 *****************************************************************************/

static RF_AccessState_t DefaultStates[] = {
	rf_QuiesceState, rf_IncrAccessesCountState, rf_MapState,
	rf_LockState, rf_CreateDAGState, rf_ExecuteDAGState,
	rf_ProcessDAGState, rf_DecrAccessesCountState,
	rf_CleanupState, rf_LastState
};

#if	(defined(__NetBSD__) || defined(__OpenBSD__)) && !defined(_KERNEL)
/*
 * XXX Gross hack to shutup gcc -- It complains that DefaultStates is not
 * used when compiling this in userland... I hate to burst its bubble, but
 * DefaultStates is used all over the place here in the initialization of
 * lots of data structures. GO
 */
RF_AccessState_t *NothingAtAll = DefaultStates;
#endif

#if	(defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
/* XXX Remove static so GCC doesn't complain about these being unused ! */
int distSpareYes = 1;
int distSpareNo = 0;
#else
static int distSpareYes = 1;
static int distSpareNo = 0;
#endif

#ifdef	_KERNEL
#define	RF_NK2(a,b)
#else	/* _KERNEL */
#define	RF_NK2(a,b)	a,b,
#endif	/* !_KERNEL */

#if	RF_UTILITY > 0
#define	RF_NU(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)
#else	/* RF_UTILITY > 0 */
#define	RF_NU(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)	a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p
#endif	/* RF_UTILITY > 0 */

static RF_LayoutSW_t mapsw[] = {
	/* Parity declustering. */
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

	/* Parity declustering with distributed sparing. */
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
		    RF_DISTRIBUTE_SPARE | RF_BD_DECLUSTERED)
	},

#if	RF_INCLUDE_DECL_PQ > 0
	/* Declustered P+Q. */
	{'Q', "Declustered P+Q",
		RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareNo)
		RF_NU(
		    rf_ConfigureDeclusteredPQ,
		    rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ,
		    rf_MapQDeclusteredPQ,
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
#endif	/* RF_INCLUDE_DECL_PQ > 0 */

#if	RF_INCLUDE_RAID5_RS > 0
	/* RAID 5 with rotated sparing. */
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
#endif	/* RF_INCLUDE_RAID5_RS > 0 */

#if	RF_INCLUDE_CHAINDECLUSTER > 0
	/* Chained Declustering. */
	{'C', "Chained Declustering",
		RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
		RF_NU(
		    rf_ConfigureChainDecluster,
		    rf_MapSectorChainDecluster, rf_MapParityChainDecluster,
		    NULL,
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
#endif	/* RF_INCLUDE_CHAINDECLUSTER > 0 */

#if	RF_INCLUDE_INTERDECLUSTER > 0
	/* Interleaved Declustering. */
	{'I', "Interleaved Declustering",
		RF_NK2(rf_MakeLayoutSpecificNULL, NULL)
		RF_NU(
		    rf_ConfigureInterDecluster,
		    rf_MapSectorInterDecluster, rf_MapParityInterDecluster,
		    NULL,
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
#endif	/* RF_INCLUDE_INTERDECLUSTER > 0 */

#if	RF_INCLUDE_RAID0 > 0
	/* RAID level 0. */
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
#endif	/* RF_INCLUDE_RAID0 > 0 */

#if	RF_INCLUDE_RAID1 > 0
	/* RAID level 1. */
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
#endif	/* RF_INCLUDE_RAID1 > 0 */

#if	RF_INCLUDE_RAID4 > 0
	/* RAID level 4. */
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
#endif	/* RF_INCLUDE_RAID4 > 0 */

#if	RF_INCLUDE_RAID5 > 0
	/* RAID level 5. */
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
#endif	/* RF_INCLUDE_RAID5 > 0 */

#if	RF_INCLUDE_EVENODD > 0
	/* Evenodd. */
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
		    NULL,	/* No reconstruction, yet. */
		    rf_VerifyParityEvenOdd,
		    2,
		    DefaultStates,
		    0)
	},
#endif	/* RF_INCLUDE_EVENODD > 0 */

#if	RF_INCLUDE_EVENODD > 0
	/* Declustered Evenodd. */
	{'e', "Declustered EvenOdd",
		RF_NK2(rf_MakeLayoutSpecificDeclustered, &distSpareNo)
		RF_NU(
		    rf_ConfigureDeclusteredPQ,
		    rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ,
		    rf_MapQDeclusteredPQ,
		    rf_IdentifyStripeDeclusteredPQ,
		    rf_EODagSelect,
		    rf_MapSIDToPSIDRAID5,
		    rf_GetDefaultHeadSepLimitDeclustered,
		    rf_GetDefaultNumFloatingReconBuffersPQ,
		    NULL, NULL,
		    NULL,	/* No reconstruction, yet. */
		    rf_VerifyParityEvenOdd,
		    2,
		    DefaultStates,
		    0)
	},
#endif	/* RF_INCLUDE_EVENODD > 0 */

#if	RF_INCLUDE_PARITYLOGGING > 0
	/* Parity logging. */
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
#endif	/* RF_INCLUDE_PARITYLOGGING > 0 */

	/* End-of-list marker. */
	{'\0', NULL,
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

RF_LayoutSW_t *
rf_GetLayout(RF_ParityConfig_t parityConfig)
{
	RF_LayoutSW_t *p;

	/* Look up the specific layout. */
	for (p = &mapsw[0]; p->parityConfig; p++)
		if (p->parityConfig == parityConfig)
			break;
	if (!p->parityConfig)
		return (NULL);
	RF_ASSERT(p->parityConfig == parityConfig);
	return (p);
}

#if	RF_UTILITY == 0
/*****************************************************************************
 *
 * ConfigureLayout
 *
 * Read the configuration file and set up the RAID layout parameters.
 * After reading common params, invokes the layout-specific configuration
 * routine to finish the configuration.
 *
 *****************************************************************************/
int
rf_ConfigureLayout(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
    RF_Config_t *cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_ParityConfig_t parityConfig;
	RF_LayoutSW_t *p;
	int retval;

	layoutPtr->sectorsPerStripeUnit = cfgPtr->sectPerSU;
	layoutPtr->SUsPerPU = cfgPtr->SUsPerPU;
	layoutPtr->SUsPerRU = cfgPtr->SUsPerRU;
	parityConfig = cfgPtr->parityConfig;

	if (layoutPtr->sectorsPerStripeUnit <= 0) {
		RF_ERRORMSG2("raid%d: Invalid sectorsPerStripeUnit: %d.\n",
		    raidPtr->raidid, (int)layoutPtr->sectorsPerStripeUnit);
		return (EINVAL);
	}

	layoutPtr->stripeUnitsPerDisk = raidPtr->sectorsPerDisk /
	    layoutPtr->sectorsPerStripeUnit;

	p = rf_GetLayout(parityConfig);
	if (p == NULL) {
		RF_ERRORMSG1("Unknown parity configuration '%c'", parityConfig);
		return (EINVAL);
	}
	RF_ASSERT(p->parityConfig == parityConfig);
	layoutPtr->map = p;

	/* Initialize the specific layout. */

	retval = (p->Configure) (listp, raidPtr, cfgPtr);

	if (retval)
		return (retval);

	layoutPtr->dataBytesPerStripe = layoutPtr->dataSectorsPerStripe <<
	    raidPtr->logBytesPerSector;
	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk *
	    layoutPtr->sectorsPerStripeUnit;

	if (rf_forceNumFloatingReconBufs >= 0) {
		raidPtr->numFloatingReconBufs = rf_forceNumFloatingReconBufs;
	} else {
		raidPtr->numFloatingReconBufs =
		    rf_GetDefaultNumFloatingReconBuffers(raidPtr);
	}

	if (rf_forceHeadSepLimit >= 0) {
		raidPtr->headSepLimit = rf_forceHeadSepLimit;
	} else {
		raidPtr->headSepLimit = rf_GetDefaultHeadSepLimit(raidPtr);
	}

#ifdef	RAIDDEBUG
	if (raidPtr->headSepLimit >= 0) {
		printf("RAIDFRAME(%s): Using %ld floating recon bufs"
		    " with head sep limit %ld.\n", layoutPtr->map->configName,
		    (long) raidPtr->numFloatingReconBufs,
		    (long) raidPtr->headSepLimit);
	} else {
		printf("RAIDFRAME(%s): Using %ld floating recon bufs"
		    " with no head sep limit.\n", layoutPtr->map->configName,
		    (long) raidPtr->numFloatingReconBufs);
	}
#endif	/* RAIDDEBUG */

	return (0);
}

/*
 * Typically there is a 1-1 mapping between stripes and parity stripes.
 * However, the declustering code supports packing multiple stripes into
 * a single parity stripe, so as to increase the size of the reconstruction
 * unit without affecting the size of the stripe unit. This routine finds
 * the parity stripe identifier associated with a stripe ID. There is also
 * a RaidAddressToParityStripeID macro in layout.h
 */
RF_StripeNum_t
rf_MapStripeIDToParityStripeID(RF_RaidLayout_t *layoutPtr,
    RF_StripeNum_t stripeID, RF_ReconUnitNum_t *which_ru)
{
	RF_StripeNum_t parityStripeID;

	/* Quick exit in the common case of SUsPerPU == 1. */
	if ((layoutPtr->SUsPerPU == 1) || !layoutPtr->map->MapSIDToPSID) {
		*which_ru = 0;
		return (stripeID);
	} else {
		(layoutPtr->map->MapSIDToPSID) (layoutPtr, stripeID,
		    &parityStripeID, which_ru);
	}
	return (parityStripeID);
}
#endif	/* RF_UTILITY == 0 */
