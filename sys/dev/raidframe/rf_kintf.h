/*	$OpenBSD: rf_kintf.h,v 1.1 1999/01/11 14:29:27 niklas Exp $	*/
/*	$NetBSD: rf_kintf.h,v 1.1 1998/11/13 04:20:30 oster Exp $	*/
/*
 * rf_kintf.h
 *
 * RAIDframe exported kernel interface
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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
 * :  
 * Log: rf_kintf.h,v 
 * Revision 1.2  1996/06/03 23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.1  1996/05/31  18:59:14  jimz
 * Initial revision
 *
 */

#ifndef _RF__RF_KINTF_H_
#define _RF__RF_KINTF_H_

#include "rf_types.h"

int rf_boot(void);
int rf_open(dev_t dev, int flag, int fmt);
int rf_close(dev_t dev, int flag, int fmt);
void rf_strategy(struct buf *bp);
void rf_minphys(struct buf *bp);
int rf_read(dev_t dev, struct uio *uio);
int rf_write(dev_t dev, struct uio *uio);
int rf_size(dev_t dev);
int rf_ioctl(dev_t dev, int cmd, caddr_t data, int flag);
void rf_ReconKernelThread(void);
int rf_GetSpareTableFromDaemon(RF_SparetWait_t *req);
caddr_t rf_MapToKernelSpace(struct buf *bp, caddr_t addr);
int rf_BzeroWithRemap(struct buf *bp, char *databuf, int len);
int rf_DoAccessKernel(RF_Raid_t *raidPtr, struct buf *bp,
	RF_RaidAccessFlags_t flags, void (*cbFunc)(struct buf *), void *cbArg);
int rf_DispatchKernelIO(RF_DiskQueue_t *queue, RF_DiskQueueData_t *req);

#endif /* _RF__RF_KINTF_H_ */
