/*	$OpenBSD: rf_raidframe.h,v 1.2 1999/02/16 00:03:19 niklas Exp $	*/
/*	$NetBSD: rf_raidframe.h,v 1.3 1999/02/05 00:06:16 oster Exp $	*/
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

/*****************************************************
 *
 * rf_raidframe.h
 *
 * main header file for using raidframe in the kernel.
 *
 *****************************************************/


#ifndef _RF__RF_RAIDFRAME_H_
#define _RF__RF_RAIDFRAME_H_

#include "rf_types.h"
#include "rf_configure.h"
#include "rf_disks.h"
#include "rf_raid.h"

struct rf_test_acc {		/* used by RAIDFRAME_TEST_ACC ioctl */
	RF_SectorNum_t startSector;	/* raidAddress */
	RF_SectorCount_t numSector;	/* number of sectors to xfer */
	char   *buf;		/* data buffer */
	void   *returnBufs[10];	/* for async accs only, completed I/Os
				 * returned */
	struct rf_test_acc *next;	/* for making lists */
	RF_IoType_t type;	/* (see rf_types.h for RF_IO_TYPE_*) */
	struct rf_test_acc *myaddr;	/* user-address of this struct */
	void   *bp;		/* used in-kernel: need not be set by user */
};

typedef RF_uint32 RF_ReconReqFlags_t;

struct rf_recon_req {		/* used to tell the kernel to fail a disk */
	RF_RowCol_t row, col;
	RF_ReconReqFlags_t flags;
	void   *raidPtr;	/* used internally; need not be set at ioctl
				 * time */
	struct rf_recon_req *next;	/* used internally; need not be set at
					 * ioctl time */
};

struct RF_SparetWait_s {
	int     C, G, fcol;	/* C = # disks in row, G = # units in stripe,
				 * fcol = which disk has failed */

	RF_StripeCount_t SUsPerPU;	/* this stuff is the info required to
					 * create a spare table */
	int     TablesPerSpareRegion;
	int     BlocksPerTable;
	RF_StripeCount_t TableDepthInPUs;
	RF_StripeCount_t SpareSpaceDepthPerRegionInSUs;

	RF_SparetWait_t *next;	/* used internally; need not be set at ioctl
				 * time */
};

typedef struct RF_DeviceConfig_s {
	u_int   rows;
	u_int   cols;
	u_int   maxqdepth;
	int     ndevs;
	RF_RaidDisk_t devs[RF_MAX_DISKS];
	int     nspares;
	RF_RaidDisk_t spares[RF_MAX_DISKS];
}       RF_DeviceConfig_t;


/* flags that can be put in the rf_recon_req structure */
#define RF_FDFLAGS_NONE   0x0	/* just fail the disk */
#define RF_FDFLAGS_RECON  0x1	/* fail and initiate recon */

#define RF_SCSI_DISK_MAJOR   8	/* the device major number for disks in the
				 * system */

#define RAIDFRAME_CONFIGURE         _IOW ('r',  1, void *)	/* configure the driver */
#define RAIDFRAME_SHUTDOWN          _IO  ('r',  2)	/* shutdown the driver */
#define RAIDFRAME_TUR               _IOW ('r',  3, dev_t)	/* debug only: test unit
								 * ready */
#define RAIDFRAME_TEST_ACC          _IOWR('r',  4, struct rf_test_acc)	/* run a test access */
#define RAIDFRAME_FAIL_DISK         _IOW ('r',  5, struct rf_recon_req)	/* fail a disk &
									 * optionally start
									 * recon */
#define RAIDFRAME_CHECKRECON        _IOWR('r',  6, int)	/* get reconstruction %
							 * complete on indicated
							 * row */
#define RAIDFRAME_REWRITEPARITY     _IO  ('r',  7)	/* rewrite (initialize)
							 * all parity */
#define RAIDFRAME_COPYBACK          _IO  ('r',  8)	/* copy reconstructed
							 * data back to replaced
							 * disk */
#define RAIDFRAME_SPARET_WAIT       _IOR ('r',  9, RF_SparetWait_t)	/* does not return until
									 * kernel needs a spare
									 * table */
#define RAIDFRAME_SEND_SPARET       _IOW ('r', 10, void *)	/* used to send a spare
								 * table down into the
								 * kernel */
#define RAIDFRAME_ABORT_SPARET_WAIT _IO  ('r', 11)	/* used to wake up the
							 * sparemap daemon &
							 * tell it to exit */
#define RAIDFRAME_START_ATRACE      _IO  ('r', 12)	/* start tracing
							 * accesses */
#define RAIDFRAME_STOP_ATRACE       _IO  ('r', 13)	/* stop tracing accesses */
#define RAIDFRAME_GET_SIZE          _IOR ('r', 14, int)	/* get size (# sectors)
							 * in raid device */
#define RAIDFRAME_GET_INFO          _IOWR('r', 15, RF_DeviceConfig_t *)	/* get configuration */
#define RAIDFRAME_RESET_ACCTOTALS   _IO  ('r', 16)	/* reset AccTotals for
							 * device */
#define RAIDFRAME_GET_ACCTOTALS     _IOR ('r', 17, RF_AccTotals_t)	/* retrieve AccTotals
									 * for device */
#define RAIDFRAME_KEEP_ACCTOTALS    _IOW ('r', 18, int)	/* turn AccTotals on or
							 * off for device */

#endif				/* !_RF__RF_RAIDFRAME_H_ */
