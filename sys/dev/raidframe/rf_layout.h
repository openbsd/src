/*	$OpenBSD: rf_layout.h,v 1.1 1999/01/11 14:29:28 niklas Exp $	*/
/*	$NetBSD: rf_layout.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
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

/* rf_layout.h -- header file defining layout data structures
 */

/*
 * :  
 * Log: rf_layout.h,v 
 * Revision 1.50  1996/11/05 21:10:40  jimz
 * failed pda generalization
 *
 * Revision 1.49  1996/07/29  14:05:12  jimz
 * fix numPUs/numRUs confusion (everything is now numRUs)
 * clean up some commenting, return values
 *
 * Revision 1.48  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.47  1996/07/18  22:57:14  jimz
 * port simulator to AIX
 *
 * Revision 1.46  1996/07/13  00:00:59  jimz
 * sanitized generalized reconstruction architecture
 * cleaned up head sep, rbuf problems
 *
 * Revision 1.45  1996/07/11  19:08:00  jimz
 * generalize reconstruction mechanism
 * allow raid1 reconstructs via copyback (done with array
 * quiesced, not online, therefore not disk-directed)
 *
 * Revision 1.44  1996/06/19  22:23:01  jimz
 * parity verification is now a layout-configurable thing
 * not all layouts currently support it (correctly, anyway)
 *
 * Revision 1.43  1996/06/19  17:53:48  jimz
 * move GetNumSparePUs, InstallSpareTable ops into layout switch
 *
 * Revision 1.42  1996/06/19  14:56:48  jimz
 * move layout-specific config parsing hooks into RF_LayoutSW_t
 * table in rf_layout.c
 *
 * Revision 1.41  1996/06/10  11:55:47  jimz
 * Straightened out some per-array/not-per-array distinctions, fixed
 * a couple bugs related to confusion. Added shutdown lists. Removed
 * layout shutdown function (now subsumed by shutdown lists).
 *
 * Revision 1.40  1996/06/07  22:26:27  jimz
 * type-ify which_ru (RF_ReconUnitNum_t)
 *
 * Revision 1.39  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.38  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.37  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.36  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.35  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.34  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.33  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.32  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.31  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.30  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.29  1995/12/01  19:16:19  root
 * added copyright info
 *
 * Revision 1.28  1995/11/28  21:26:49  amiri
 * defined a declustering flag RF_BD_DECLUSTERED
 *
 * Revision 1.27  1995/11/17  19:00:59  wvcii
 * created MapQ entry in switch table
 * added prototyping to MapParity
 *
 * Revision 1.26  1995/11/07  15:40:27  wvcii
 * changed prototype of SeclectionFunc in mapsw
 * function no longer returns numHdrSucc, numTermAnt
 *
 * Revision 1.25  1995/10/12  20:57:08  arw
 * added lots of comments
 *
 * Revision 1.24  1995/10/12  16:04:08  jimz
 * added config name to mapsw
 *
 * Revision 1.23  1995/07/26  03:28:31  robby
 * intermediary checkin
 *
 * Revision 1.22  1995/07/10  20:51:08  robby
 * added to the asm info for the virtual striping locks
 *
 * Revision 1.21  1995/07/10  16:57:47  robby
 * updated alloclistelem struct to the correct struct name
 *
 * Revision 1.20  1995/07/08  20:06:11  rachad
 * *** empty log message ***
 *
 * Revision 1.19  1995/07/08  18:05:39  rachad
 * Linked up Claudsons code with the real cache
 *
 * Revision 1.18  1995/07/06  14:29:36  robby
 * added defaults states list to the layout switch
 *
 * Revision 1.17  1995/06/23  13:40:14  robby
 * updeated to prototypes in rf_layout.h
 *
 * Revision 1.16  1995/06/08  22:11:03  holland
 * bug fixes related to mutiple-row arrays
 *
 * Revision 1.15  1995/05/24  21:43:23  wvcii
 * added field numParityLogCol to RaidLayout
 *
 * Revision 1.14  95/05/02  22:46:53  holland
 * minor code cleanups.
 * 
 * Revision 1.13  1995/05/02  12:48:01  holland
 * eliminated some unused code.
 *
 * Revision 1.12  1995/05/01  13:28:00  holland
 * parity range locks, locking disk requests, recon+parityscan in kernel, etc.
 *
 * Revision 1.11  1995/03/15  20:01:17  holland
 * added REMAP and DONT_REMAP
 *
 * Revision 1.10  1995/03/09  19:54:11  rachad
 * Added suport for threadless simulator
 *
 * Revision 1.9  1995/03/03  21:48:58  holland
 * minor changes.
 *
 * Revision 1.8  1995/03/01  20:25:48  holland
 * kernelization changes
 *
 * Revision 1.7  1995/02/03  22:31:36  holland
 * many changes related to kernelization
 *
 * Revision 1.6  1995/01/30  14:53:46  holland
 * extensive changes related to making DoIO non-blocking
 *
 * Revision 1.5  1995/01/24  23:58:46  holland
 * multi-way recon XOR, plus various small changes
 *
 * Revision 1.4  1995/01/04  19:28:35  holland
 * corrected comments around mapsw
 *
 * Revision 1.3  1994/11/28  22:15:45  danner
 * Added type field to the physdiskaddr struct.
 *
 */

#ifndef _RF__RF_LAYOUT_H_
#define _RF__RF_LAYOUT_H_

#include "rf_types.h"
#include "rf_archs.h"
#include "rf_alloclist.h"

/*****************************************************************************************
 *
 * This structure identifies all layout-specific operations and parameters.
 * 
 ****************************************************************************************/

typedef struct RF_LayoutSW_s {
  RF_ParityConfig_t   parityConfig;
  char               *configName;

#ifndef KERNEL
 /* layout-specific parsing */
  int (*MakeLayoutSpecific)(FILE *fp, RF_Config_t *cfgPtr, void *arg);
  void *makeLayoutSpecificArg;
#endif /* !KERNEL */

#if RF_UTILITY == 0
 /* initialization routine */
  int (*Configure)(RF_ShutdownList_t **shutdownListp, RF_Raid_t *raidPtr, RF_Config_t *cfgPtr);

 /* routine to map RAID sector address -> physical (row, col, offset) */
  void (*MapSector)(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);

 /* routine to map RAID sector address -> physical (r,c,o) of parity unit */
  void (*MapParity)(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t *row, RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);

 /* routine to map RAID sector address -> physical (r,c,o) of Q unit */
  void (*MapQ)(RF_Raid_t *raidPtr, RF_RaidAddr_t raidSector, RF_RowCol_t *row,
    RF_RowCol_t *col, RF_SectorNum_t *diskSector, int remap);

 /* routine to identify the disks comprising a stripe */
  void (*IdentifyStripe)(RF_Raid_t *raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t **diskids, RF_RowCol_t *outRow);

 /* routine to select a dag */
  void (*SelectionFunc)(RF_Raid_t *raidPtr, RF_IoType_t type, 
			RF_AccessStripeMap_t *asmap,
			RF_VoidFuncPtr *);
#if 0
			void (**createFunc)(RF_Raid_t *, 
					    RF_AccessStripeMap_t *,
					    RF_DagHeader_t *, void *, 
					    RF_RaidAccessFlags_t,
					    RF_AllocListElem_t *));
	
#endif

 /* map a stripe ID to a parity stripe ID.  This is typically the identity mapping */
  void (*MapSIDToPSID)(RF_RaidLayout_t *layoutPtr, RF_StripeNum_t stripeID,
    RF_StripeNum_t *psID, RF_ReconUnitNum_t *which_ru);

 /* get default head separation limit (may be NULL) */
  RF_HeadSepLimit_t (*GetDefaultHeadSepLimit)(RF_Raid_t *raidPtr);

 /* get default num recon buffers (may be NULL) */
  int (*GetDefaultNumFloatingReconBuffers)(RF_Raid_t *raidPtr);

 /* get number of spare recon units (may be NULL) */
  RF_ReconUnitCount_t (*GetNumSpareRUs)(RF_Raid_t *raidPtr);

 /* spare table installation (may be NULL) */
  int (*InstallSpareTable)(RF_Raid_t *raidPtr, RF_RowCol_t frow, RF_RowCol_t fcol);

 /* recon buffer submission function */
  int (*SubmitReconBuffer)(RF_ReconBuffer_t *rbuf, int keep_it,
    int use_committed);

 /*
  * verify that parity information for a stripe is correct
  * see rf_parityscan.h for return vals
  */
  int (*VerifyParity)(RF_Raid_t *raidPtr, RF_RaidAddr_t raidAddr,
    RF_PhysDiskAddr_t *parityPDA, int correct_it, RF_RaidAccessFlags_t flags);

 /* number of faults tolerated by this mapping */
  int  faultsTolerated;

 /* states to step through in an access. Must end with "LastState".
  * The default is DefaultStates in rf_layout.c */
  RF_AccessState_t *states;

  RF_AccessStripeMapFlags_t  flags;
#endif /* RF_UTILITY == 0 */
} RF_LayoutSW_t;

/* enables remapping to spare location under dist sparing */
#define RF_REMAP       1
#define RF_DONT_REMAP  0

/*
 * Flags values for RF_AccessStripeMapFlags_t
 */
#define RF_NO_STRIPE_LOCKS   0x0001   /* suppress stripe locks */
#define RF_DISTRIBUTE_SPARE  0x0002   /* distribute spare space in archs that support it */
#define RF_BD_DECLUSTERED    0x0004   /* declustering uses block designs */

/*************************************************************************
 *
 * this structure forms the layout component of the main Raid
 * structure.  It describes everything needed to define and perform
 * the mapping of logical RAID addresses <-> physical disk addresses.
 * 
 *************************************************************************/
struct RF_RaidLayout_s {
  /* configuration parameters */
  RF_SectorCount_t  sectorsPerStripeUnit;   /* number of sectors in one stripe unit */
  RF_StripeCount_t  SUsPerPU;               /* stripe units per parity unit */
  RF_StripeCount_t  SUsPerRU;               /* stripe units per reconstruction unit */

  /* redundant-but-useful info computed from the above, used in all layouts */
  RF_StripeCount_t  numStripe;              /* total number of stripes in the array */
  RF_SectorCount_t  dataSectorsPerStripe;
  RF_StripeCount_t  dataStripeUnitsPerDisk;
  u_int             bytesPerStripeUnit;
  u_int             dataBytesPerStripe;
  RF_StripeCount_t  numDataCol;             /* number of SUs of data per stripe (name here is a la RAID4) */
  RF_StripeCount_t  numParityCol;           /* number of SUs of parity per stripe.  Always 1 for now */
  RF_StripeCount_t  numParityLogCol;        /* number of SUs of parity log per stripe.  Always 1 for now */
  RF_StripeCount_t  stripeUnitsPerDisk;

  RF_LayoutSW_t *map;               /* ptr to struct holding mapping fns and information */
  void *layoutSpecificInfo;         /* ptr to a structure holding layout-specific params */
};

/*****************************************************************************************
 *
 * The mapping code returns a pointer to a list of AccessStripeMap structures, which
 * describes all the mapping information about an access.  The list contains one
 * AccessStripeMap structure per stripe touched by the access.  Each element in the list
 * contains a stripe identifier and a pointer to a list of PhysDiskAddr structuress.  Each
 * element in this latter list describes the physical location of a stripe unit accessed
 * within the corresponding stripe.
 * 
 ****************************************************************************************/

#define RF_PDA_TYPE_DATA   0
#define RF_PDA_TYPE_PARITY 1
#define RF_PDA_TYPE_Q      2

struct RF_PhysDiskAddr_s {
  RF_RowCol_t         row,col;      /* disk identifier */
  RF_SectorNum_t      startSector;  /* sector offset into the disk */
  RF_SectorCount_t    numSector;    /* number of sectors accessed */
  int                 type;         /* used by higher levels: currently, data, parity, or q */
  caddr_t             bufPtr;       /* pointer to buffer supplying/receiving data */
  RF_RaidAddr_t       raidAddress;  /* raid address corresponding to this physical disk address */
  RF_PhysDiskAddr_t  *next;
};

#define RF_MAX_FAILED_PDA RF_MAXCOL

struct RF_AccessStripeMap_s {
  RF_StripeNum_t             stripeID;                /* the stripe index */
  RF_RaidAddr_t              raidAddress;             /* the starting raid address within this stripe */
  RF_RaidAddr_t              endRaidAddress;          /* raid address one sector past the end of the access */
  RF_SectorCount_t           totalSectorsAccessed;    /* total num sectors identified in physInfo list */
  RF_StripeCount_t           numStripeUnitsAccessed;  /* total num elements in physInfo list */
  int                        numDataFailed;           /* number of failed data disks accessed */
  int                        numParityFailed;         /* number of failed parity disks accessed (0 or 1) */
  int                        numQFailed;              /* number of failed Q units accessed (0 or 1) */
  RF_AccessStripeMapFlags_t  flags;                   /* various flags */
#if 0
  RF_PhysDiskAddr_t         *failedPDA;               /* points to the PDA that has failed */
  RF_PhysDiskAddr_t         *failedPDAtwo;            /* points to the second PDA that has failed, if any */
#else
  int                        numFailedPDAs;           /* number of failed phys addrs */
  RF_PhysDiskAddr_t         *failedPDAs[RF_MAX_FAILED_PDA]; /* array of failed phys addrs */
#endif
  RF_PhysDiskAddr_t         *physInfo;                /* a list of PhysDiskAddr structs */
  RF_PhysDiskAddr_t         *parityInfo;              /* list of physical addrs for the parity (P of P + Q ) */
  RF_PhysDiskAddr_t         *qInfo;                   /* list of physical addrs for the Q of P + Q */
  RF_LockReqDesc_t           lockReqDesc;             /* used for stripe locking */
  RF_RowCol_t                origRow;                 /* the original row:  we may redirect the acc to a different row */
  RF_AccessStripeMap_t      *next;
};

/* flag values */
#define RF_ASM_REDIR_LARGE_WRITE   0x00000001 /* allows large-write creation code to redirect failed accs */
#define RF_ASM_BAILOUT_DAG_USED    0x00000002 /* allows us to detect recursive calls to the bailout write dag */
#define RF_ASM_FLAGS_LOCK_TRIED    0x00000004 /* we've acquired the lock on the first parity range in this parity stripe */
#define RF_ASM_FLAGS_LOCK_TRIED2   0x00000008 /* we've acquired the lock on the 2nd   parity range in this parity stripe */
#define RF_ASM_FLAGS_FORCE_TRIED   0x00000010 /* we've done the force-recon call on this parity stripe */
#define RF_ASM_FLAGS_RECON_BLOCKED 0x00000020 /* we blocked recon => we must unblock it later */

struct RF_AccessStripeMapHeader_s {
  RF_StripeCount_t             numStripes; /* total number of stripes touched by this acc */
  RF_AccessStripeMap_t        *stripeMap;  /* pointer to the actual map.  Also used for making lists */
  RF_AccessStripeMapHeader_t  *next;
};

/*****************************************************************************************
 *
 * various routines mapping addresses in the RAID address space.  These work across
 * all layouts.  DON'T PUT ANY LAYOUT-SPECIFIC CODE HERE.
 *
 ****************************************************************************************/

/* return the identifier of the stripe containing the given address */
#define rf_RaidAddressToStripeID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) / (_layoutPtr_)->numDataCol )

/* return the raid address of the start of the indicates stripe ID */
#define rf_StripeIDToRaidAddress(_layoutPtr_, _sid_) \
  ( ((_sid_) * (_layoutPtr_)->sectorsPerStripeUnit) * (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe containing the given stripe unit id */
#define rf_StripeUnitIDToStripeID(_layoutPtr_, _addr_) \
  ( (_addr_) / (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe unit containing the given address */
#define rf_RaidAddressToStripeUnitID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) )

/* return the RAID address of next stripe boundary beyond the given address */
#define rf_RaidAddressOfNextStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+1) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of the start of the stripe containing the given address */
#define rf_RaidAddressOfPrevStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+0) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of next stripe unit boundary beyond the given address */
#define rf_RaidAddressOfNextStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+1L)*(_layoutPtr_)->sectorsPerStripeUnit )

/* return the RAID address of the start of the stripe unit containing RAID address _addr_ */
#define rf_RaidAddressOfPrevStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+0)*(_layoutPtr_)->sectorsPerStripeUnit )

/* returns the offset into the stripe.  used by RaidAddressStripeAligned */
#define rf_RaidAddressStripeOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->dataSectorsPerStripe) )

/* returns the offset into the stripe unit.  */
#define rf_StripeUnitOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->sectorsPerStripeUnit) )

/* returns nonzero if the given RAID address is stripe-aligned */
#define rf_RaidAddressStripeAligned( __layoutPtr__, __addr__ ) \
  ( rf_RaidAddressStripeOffset(__layoutPtr__, __addr__) == 0 )

/* returns nonzero if the given address is stripe-unit aligned */
#define rf_StripeUnitAligned( __layoutPtr__, __addr__ ) \
  ( rf_StripeUnitOffset(__layoutPtr__, __addr__) == 0 )

/* convert an address expressed in RAID blocks to/from an addr expressed in bytes */
#define rf_RaidAddressToByte(_raidPtr_, _addr_) \
  ( (_addr_) << ( (_raidPtr_)->logBytesPerSector ) )

#define rf_ByteToRaidAddress(_raidPtr_, _addr_) \
  ( (_addr_) >> ( (_raidPtr_)->logBytesPerSector ) )

/* convert a raid address to/from a parity stripe ID.  Conversion to raid address is easy,
 * since we're asking for the address of the first sector in the parity stripe.  Conversion to a
 * parity stripe ID is more complex, since stripes are not contiguously allocated in
 * parity stripes.
 */
#define rf_RaidAddressToParityStripeID(_layoutPtr_, _addr_, _ru_num_) \
  rf_MapStripeIDToParityStripeID( (_layoutPtr_), rf_RaidAddressToStripeID( (_layoutPtr_), (_addr_) ), (_ru_num_) )

#define rf_ParityStripeIDToRaidAddress(_layoutPtr_, _psid_) \
  ( (_psid_) * (_layoutPtr_)->SUsPerPU * (_layoutPtr_)->numDataCol * (_layoutPtr_)->sectorsPerStripeUnit )

RF_LayoutSW_t *rf_GetLayout(RF_ParityConfig_t parityConfig);
int rf_ConfigureLayout(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
	RF_Config_t *cfgPtr);
RF_StripeNum_t rf_MapStripeIDToParityStripeID(RF_RaidLayout_t *layoutPtr,
	RF_StripeNum_t stripeID, RF_ReconUnitNum_t *which_ru);

#endif /* !_RF__RF_LAYOUT_H_ */
