/*	$OpenBSD: rf_configure.h,v 1.5 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_configure.h,v 1.4 1999/03/02 03:18:49 oster Exp $	*/

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
 * rf_configure.h
 *
 * Header file for RAIDframe configuration in the kernel version only.
 * Configuration is invoked via ioctl rather than at boot time.
 *
 *****************************************************************************/


#ifndef	_RF__RF_CONFIGURE_H_
#define	_RF__RF_CONFIGURE_H_

#include "rf_archs.h"
#include "rf_types.h"

#include <sys/param.h>
#include <sys/proc.h>

#include <sys/ioctl.h>

/*
 * The RAIDframe configuration, passed down through an ioctl.
 * The driver can be reconfigured (with total loss of data) at any time,
 * but it must be shut down first.
 */
struct RF_Config_s {
	/* Number of rows, columns, and spare disks. */
	RF_RowCol_t		 numRow, numCol, numSpare;

	/* Device numbers for disks comprising array. */
	dev_t			 devs[RF_MAXROW][RF_MAXCOL];

	/* Device names. */
	char			 devnames[RF_MAXROW][RF_MAXCOL][50];

	/* Device numbers for spare disks. */
	dev_t			 spare_devs[RF_MAXSPARE];

	/* Device names. */
	char			 spare_names[RF_MAXSPARE][50];

	/* Sectors per stripe unit. */
	RF_SectorNum_t		 sectPerSU;

	/* Stripe units per parity unit. */
	RF_StripeNum_t		 SUsPerPU;

	/* Stripe units per reconstruction unit. */
	RF_StripeNum_t		 SUsPerRU;

	/* Identifies the RAID architecture to be used. */
	RF_ParityConfig_t	 parityConfig;

	/* 'f' = fifo, 'c' = cvscan, not used in kernel. */
	RF_DiskQueueType_t	 diskQueueType;

	/* # concurrent reqs to be sent to a disk.  Not used in kernel. */
	char			 maxOutstandingDiskReqs;

	/* Space for specifying debug variables & their values. */
	char			 debugVars[RF_MAXDBGV][RF_MAXDBGVLEN];

	/* Size in bytes of layout-specific info. */
	unsigned int		 layoutSpecificSize;

	/* A pointer to a layout-specific structure to be copied in. */
	void			*layoutSpecific;

	/* If !0, ignore many fatal configuration conditions. */
	int			 force;
	/*
	 * "force" is used to override cases where the component labels
	 * would indicate that configuration should not proceed without
	 * user intervention.
	*/
};

#ifndef	_KERNEL
int   rf_MakeConfig(char *, RF_Config_t *);
int   rf_MakeLayoutSpecificNULL(FILE *, RF_Config_t *, void *);
int   rf_MakeLayoutSpecificDeclustered(FILE *, RF_Config_t *, void *);
void *rf_ReadSpareTable(RF_SparetWait_t *, char *);
#endif	/* !_KERNEL */

#endif	/* !_RF__RF_CONFIGURE_H_ */
