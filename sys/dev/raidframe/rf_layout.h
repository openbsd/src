/*	$OpenBSD: rf_layout.h,v 1.5 2002/12/16 07:01:04 tdeval Exp $	*/
/*	$NetBSD: rf_layout.h,v 1.4 2000/05/23 00:44:38 thorpej Exp $	*/

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
 * rf_layout.h -- Header file defining layout data structures.
 */

#ifndef	_RF__RF_LAYOUT_H_
#define	_RF__RF_LAYOUT_H_

#include "rf_types.h"
#include "rf_archs.h"
#include "rf_alloclist.h"

#ifndef	_KERNEL
#include <stdio.h>
#endif

/*****************************************************************************
 *
 * This structure identifies all layout-specific operations and parameters.
 *
 *****************************************************************************/

typedef struct RF_LayoutSW_s {
	RF_ParityConfig_t	  parityConfig;
	const char		 *configName;

#ifndef	_KERNEL
	/* Layout-specific parsing. */
	int			(*MakeLayoutSpecific)
				(FILE *, RF_Config_t *, void *);
	void			 *makeLayoutSpecificArg;
#endif	/* !_KERNEL */

#if	RF_UTILITY == 0
	/* Initialization routine. */
	int			(*Configure)
				(RF_ShutdownList_t **, RF_Raid_t *,
				    RF_Config_t *);

	/* Routine to map RAID sector address -> physical (row, col, offset). */
	void			(*MapSector)
				(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
				    RF_RowCol_t *, RF_SectorNum_t *, int);

	/*
	 * Routine to map RAID sector address -> physical (r,c,o) of parity
	 * unit.
	 */
	void			(*MapParity)
				(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
				    RF_RowCol_t *, RF_SectorNum_t *, int);

	/* Routine to map RAID sector address -> physical (r,c,o) of Q unit. */
	void			(*MapQ)
				(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t *,
				    RF_RowCol_t *, RF_SectorNum_t *, int);

	/* Routine to identify the disks comprising a stripe. */
	void			(*IdentifyStripe)
				(RF_Raid_t *, RF_RaidAddr_t, RF_RowCol_t **,
				    RF_RowCol_t *);

	/* Routine to select a dag. */
	void			(*SelectionFunc)
				(RF_Raid_t *, RF_IoType_t,
				    RF_AccessStripeMap_t *, RF_VoidFuncPtr *);
#if 0
	void			(**createFunc)
				(RF_Raid_t *, RF_AccessStripeMap_t *,
				    RF_DagHeader_t *, void *,
				    RF_RaidAccessFlags_t, RF_AllocListElem_t *);
#endif

	/*
	 * Map a stripe ID to a parity stripe ID. This is typically the
	 * identity mapping.
	 */
	void			(*MapSIDToPSID)
				(RF_RaidLayout_t *, RF_StripeNum_t,
				    RF_StripeNum_t *, RF_ReconUnitNum_t *);

	/* Get default head separation limit (may be NULL). */
	RF_HeadSepLimit_t	(*GetDefaultHeadSepLimit) (RF_Raid_t *);

	/* Get default num recon buffers (may be NULL). */
	int			(*GetDefaultNumFloatingReconBuffers)
				(RF_Raid_t *);

	/* Get number of spare recon units (may be NULL). */
	RF_ReconUnitCount_t	(*GetNumSpareRUs) (RF_Raid_t *);

	/* Spare table installation (may be NULL). */
	int			(*InstallSpareTable)
				(RF_Raid_t *, RF_RowCol_t, RF_RowCol_t);

	/* Recon buffer submission function. */
	int			(*SubmitReconBuffer)
				(RF_ReconBuffer_t *, int, int);

	/*
	 * Verify that parity information for a stripe is correct.
	 * See rf_parityscan.h for return vals.
	 */
	int			(*VerifyParity)
				(RF_Raid_t *, RF_RaidAddr_t,
				    RF_PhysDiskAddr_t *, int,
				    RF_RaidAccessFlags_t);

	/* Number of faults tolerated by this mapping. */
	int			  faultsTolerated;

	/*
	 * States to step through in an access. Must end with "LastState". The
	 * default is DefaultStates in rf_layout.c .
	 */
	RF_AccessState_t	 *states;

	RF_AccessStripeMapFlags_t flags;
#endif	/* RF_UTILITY == 0 */
}	RF_LayoutSW_t;

/* Enables remapping to spare location under dist sparing. */
#define	RF_REMAP			1
#define	RF_DONT_REMAP			0

/*
 * Flags values for RF_AccessStripeMapFlags_t.
 */
#define	RF_NO_STRIPE_LOCKS	   0x0001	/* Suppress stripe locks. */
#define	RF_DISTRIBUTE_SPARE	   0x0002	/*
						 * Distribute spare space in
						 * archs that support it.
						 */
#define	RF_BD_DECLUSTERED	   0x0004	/*
						 * Declustering uses block
						 * designs.
						 */

/*************************************************************************
 *
 * This structure forms the layout component of the main Raid
 * structure. It describes everything needed to define and perform
 * the mapping of logical RAID addresses <-> physical disk addresses.
 *
 *************************************************************************/
struct RF_RaidLayout_s {
	/* Configuration parameters. */
	RF_SectorCount_t	 sectorsPerStripeUnit;
						/*
						 * Number of sectors in one
						 * stripe unit.
						 */
	RF_StripeCount_t	 SUsPerPU;	/*
						 * Stripe units per parity unit.
						 */
	RF_StripeCount_t	 SUsPerRU;	/*
						 * Stripe units per
						 * reconstruction unit.
						 */

	/*
	 * Redundant-but-useful info computed from the above, used in all
	 * layouts.
	 */
	RF_StripeCount_t	 numStripe;	/*
						 * Total number of stripes
						 * in the array.
						 */
	RF_SectorCount_t	 dataSectorsPerStripe;
	RF_StripeCount_t	 dataStripeUnitsPerDisk;
	u_int			 bytesPerStripeUnit;
	u_int			 dataBytesPerStripe;
	RF_StripeCount_t	 numDataCol;	/*
						 * Number of SUs of data per
						 * stripe.
						 * (name here is a la RAID4)
						 */
	RF_StripeCount_t	 numParityCol;	/*
						 * Number of SUs of parity
						 * per stripe.
						 * Always 1 for now.
						 */
	RF_StripeCount_t	 numParityLogCol;
						/*
						 * Number of SUs of parity log
						 * per stripe.
						 * Always 1 for now.
						 */
	RF_StripeCount_t	 stripeUnitsPerDisk;

	RF_LayoutSW_t		*map;		/*
						 * Pointer to struct holding
						 * mapping fns and information.
						 */
	void			*layoutSpecificInfo;
						/* Pointer to a struct holding
						 * layout-specific params.
						 */
};

/*****************************************************************************
 *
 * The mapping code returns a pointer to a list of AccessStripeMap
 * structures, which describes all the mapping information about an access.
 * The list contains one AccessStripeMap structure per stripe touched by
 * the access. Each element in the list contains a stripe identifier and
 * a pointer to a list of PhysDiskAddr structuress. Each element in this
 * latter list describes the physical location of a stripe unit accessed
 * within the corresponding stripe.
 *
 *****************************************************************************/

#define	RF_PDA_TYPE_DATA		0
#define	RF_PDA_TYPE_PARITY		1
#define	RF_PDA_TYPE_Q			2

struct RF_PhysDiskAddr_s {
	RF_RowCol_t		 row, col;	/* Disk identifier. */
	RF_SectorNum_t		 startSector;	/*
						 * Sector offset into the disk.
						 */
	RF_SectorCount_t	 numSector;	/*
						 * Number of sectors accessed.
						 */
	int			 type;		/*
						 * Used by higher levels:
						 * currently data, parity,
						 * or q.
						 */
	caddr_t			 bufPtr;	/*
						 * Pointer to buffer
						 * supplying/receiving data.
						 */
	RF_RaidAddr_t		 raidAddress;	/*
						 * Raid address corresponding
						 * to this physical disk
						 * address.
						 */
	RF_PhysDiskAddr_t	*next;
};
#define	RF_MAX_FAILED_PDA		RF_MAXCOL

struct RF_AccessStripeMap_s {
	RF_StripeNum_t		 stripeID;	/* The stripe index. */
	RF_RaidAddr_t		 raidAddress;	/*
						 * The starting raid address
						 * within this stripe.
						 */
	RF_RaidAddr_t		 endRaidAddress;/*
						 * Raid address one sector past
						 * the end of the access.
						 */
	RF_SectorCount_t	 totalSectorsAccessed;
						/*
						 * Total num sectors
						 * identified in physInfo list.
						 */
	RF_StripeCount_t	 numStripeUnitsAccessed;
						/*
						 * Total num elements in
						 * physInfo list.
						 */
	int			 numDataFailed;	/*
						 * Number of failed data disks
						 * accessed.
						 */
	int			 numParityFailed;
						/*
						 * Number of failed parity
						 * disks accessed (0 or 1).
						 */
	int			 numQFailed;	/*
						 * Number of failed Q units
						 * accessed (0 or 1).
						 */
	RF_AccessStripeMapFlags_t flags;	/* Various flags. */
#if 0
	RF_PhysDiskAddr_t	*failedPDA;	/*
						 * Points to the PDA that
						 * has failed.
						 */
	RF_PhysDiskAddr_t	*failedPDAtwo;	/*
						 * Points to the second PDA
						 * that has failed, if any.
						 */
#else
	int			 numFailedPDAs;	/*
						 * Number of failed phys addrs.
						 */
	RF_PhysDiskAddr_t	*failedPDAs[RF_MAX_FAILED_PDA];
						/*
						 * Array of failed phys addrs.
						 */
#endif
	RF_PhysDiskAddr_t	*physInfo;	/*
						 * A list of PhysDiskAddr
						 * structs.
						 */
	RF_PhysDiskAddr_t	*parityInfo;	/*
						 * List of physical addrs for
						 * the parity (P of P + Q).
						 */
	RF_PhysDiskAddr_t	*qInfo;		/*
						 * List of physical addrs for
						 * the Q of P + Q.
						 */
	RF_LockReqDesc_t	 lockReqDesc;	/* Used for stripe locking. */
	RF_RowCol_t		 origRow;	/*
						 * The original row:  we may
						 * redirect the acc to a
						 * different row.
						 */
	RF_AccessStripeMap_t	*next;
};
/* Flag values. */
#define	RF_ASM_REDIR_LARGE_WRITE	0x00000001	/*
							 * Allows large-write
							 * creation code to
							 * redirect failed
							 * accs.
							 */
#define	RF_ASM_BAILOUT_DAG_USED		0x00000002	/*
							 * Allows us to detect
							 * recursive calls to
							 * the bailout write
							 * dag.
							 */
#define	RF_ASM_FLAGS_LOCK_TRIED		0x00000004	/*
							 * We've acquired the
							 * lock on the first
							 * parity range in
							 * this parity stripe.
							 */
#define	RF_ASM_FLAGS_LOCK_TRIED2	0x00000008	/*
							 * we've acquired the
							 * lock on the 2nd
							 * parity range in this
							 * parity stripe.
							 */
#define	RF_ASM_FLAGS_FORCE_TRIED	0x00000010	/*
							 * We've done the
							 * force-recon call on
							 * this parity stripe.
							 */
#define	RF_ASM_FLAGS_RECON_BLOCKED	0x00000020	/*
							 * We blocked recon
							 * => we must unblock
							 * it later.
							 */

struct RF_AccessStripeMapHeader_s {
	RF_StripeCount_t	 numStripes;	/*
						 * Total number of stripes
						 * touched by this access.
						 */
	RF_AccessStripeMap_t	*stripeMap;	/*
						 * Pointer to the actual map.
						 * Also used for making lists.
						 */
	RF_AccessStripeMapHeader_t *next;
};


/*****************************************************************************
 *
 * Various routines mapping addresses in the RAID address space. These work
 * across all layouts. DON'T PUT ANY LAYOUT-SPECIFIC CODE HERE.
 *
 *****************************************************************************/

/* Return the identifier of the stripe containing the given address. */
#define	rf_RaidAddressToStripeID(_layoutPtr_,_addr_)			\
	(((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) /		\
	 (_layoutPtr_)->numDataCol)

/* Return the raid address of the start of the indicates stripe ID. */
#define	rf_StripeIDToRaidAddress(_layoutPtr_,_sid_)			\
	(((_sid_) * (_layoutPtr_)->sectorsPerStripeUnit) *		\
	 (_layoutPtr_)->numDataCol)

/* Return the identifier of the stripe containing the given stripe unit ID. */
#define	rf_StripeUnitIDToStripeID(_layoutPtr_,_addr_)			\
	((_addr_) / (_layoutPtr_)->numDataCol)

/* Return the identifier of the stripe unit containing the given address. */
#define	rf_RaidAddressToStripeUnitID(_layoutPtr_,_addr_)		\
	(((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit))

/* Return the RAID address of next stripe boundary beyond the given address. */
#define	rf_RaidAddressOfNextStripeBoundary(_layoutPtr_,_addr_)		\
	((((_addr_) / (_layoutPtr_)->dataSectorsPerStripe) + 1) *	\
	 (_layoutPtr_)->dataSectorsPerStripe)

/*
 * Return the RAID address of the start of the stripe containing the
 * given address.
 */
#define	rf_RaidAddressOfPrevStripeBoundary(_layoutPtr_,_addr_)		\
	((((_addr_) / (_layoutPtr_)->dataSectorsPerStripe) + 0) *	\
	 (_layoutPtr_)->dataSectorsPerStripe)

/*
 * Return the RAID address of next stripe unit boundary beyond the
 * given address.
 */
#define	rf_RaidAddressOfNextStripeUnitBoundary(_layoutPtr_,_addr_)	\
	((((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) + 1L) *	\
	 (_layoutPtr_)->sectorsPerStripeUnit)

/*
 * Return the RAID address of the start of the stripe unit containing
 * RAID address _addr_.
 */
#define	rf_RaidAddressOfPrevStripeUnitBoundary(_layoutPtr_,_addr_)	\
	((((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) + 0) *	\
	 (_layoutPtr_)->sectorsPerStripeUnit)

/* Returns the offset into the stripe. Used by RaidAddressStripeAligned. */
#define	rf_RaidAddressStripeOffset(_layoutPtr_,_addr_)			\
	((_addr_) % (_layoutPtr_)->dataSectorsPerStripe)

/* Returns the offset into the stripe unit. */
#define	rf_StripeUnitOffset(_layoutPtr_,_addr_)				\
	((_addr_) % (_layoutPtr_)->sectorsPerStripeUnit)

/* Returns nonzero if the given RAID address is stripe-aligned. */
#define	rf_RaidAddressStripeAligned(__layoutPtr__,__addr__)		\
	(rf_RaidAddressStripeOffset(__layoutPtr__, __addr__) == 0)

/* Returns nonzero if the given address is stripe-unit aligned. */
#define	rf_StripeUnitAligned(__layoutPtr__,__addr__)			\
	(rf_StripeUnitOffset(__layoutPtr__, __addr__) == 0)

/*
 * Convert an address expressed in RAID blocks to/from an addr expressed
 * in bytes.
 */
#define	rf_RaidAddressToByte(_raidPtr_,_addr_)				\
	((_addr_) << (_raidPtr_)->logBytesPerSector)

#define	rf_ByteToRaidAddress(_raidPtr_,_addr_)				\
	((_addr_) >> (_raidPtr_)->logBytesPerSector)

/*
 * Convert a raid address to/from a parity stripe ID. Conversion to raid
 * address is easy, since we're asking for the address of the first sector
 * in the parity stripe. Conversion to a parity stripe ID is more complex,
 * since stripes are not contiguously allocated in parity stripes.
 */
#define	rf_RaidAddressToParityStripeID(_layoutPtr_,_addr_,_ru_num_)	\
	rf_MapStripeIDToParityStripeID((_layoutPtr_),			\
	    rf_RaidAddressToStripeID((_layoutPtr_), (_addr_)), (_ru_num_))

#define	rf_ParityStripeIDToRaidAddress(_layoutPtr_,_psid_)		\
	((_psid_) * (_layoutPtr_)->SUsPerPU *				\
	 (_layoutPtr_)->numDataCol * (_layoutPtr_)->sectorsPerStripeUnit)

RF_LayoutSW_t *rf_GetLayout(RF_ParityConfig_t);
int  rf_ConfigureLayout(RF_ShutdownList_t **, RF_Raid_t *, RF_Config_t *);
RF_StripeNum_t rf_MapStripeIDToParityStripeID(RF_RaidLayout_t *,
	 RF_StripeNum_t, RF_ReconUnitNum_t *);

#endif	/* !_RF__RF_LAYOUT_H_ */
