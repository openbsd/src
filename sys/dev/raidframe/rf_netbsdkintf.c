/*	$OpenBSD: rf_netbsdkintf.c,v 1.10 2002/10/12 01:09:44 krw Exp $	*/
/*	$NetBSD: rf_netbsdkintf.c,v 1.93 2000/07/14 15:26:29 oster Exp $	*/
/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster; Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *      @(#)cd.c        8.2 (Berkeley) 11/16/93
 */




/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, Jim Zelenka
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

/***********************************************************
 *
 * rf_kintf.c -- the kernel interface routines for RAIDframe
 *
 ***********************************************************/

#include <sys/errno.h>
#include <sys/param.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/device.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/param.h>
#include <sys/types.h>
#include <machine/types.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/buf.h>
#include <sys/user.h>
#include <sys/reboot.h>

#include "raid.h"
#include "opt_raid_autoconfig.h"
#include "rf_raid.h"
#include "rf_raidframe.h"
#include "rf_copyback.h"
#include "rf_dag.h"
#include "rf_dagflags.h"
#include "rf_diskqueue.h"
#include "rf_acctrace.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_debugMem.h"
#include "rf_kintf.h"
#include "rf_options.h"
#include "rf_driver.h"
#include "rf_parityscan.h"
#include "rf_debugprint.h"
#include "rf_threadstuff.h"
#include "rf_configure.h"

int     rf_kdebug_level = 0;

#ifdef DEBUG
#define db1_printf(a) if (rf_kdebug_level > 0) printf a
#else				/* DEBUG */
#define db1_printf(a) { }
#endif				/* DEBUG */

static RF_Raid_t **raidPtrs;	/* global raid device descriptors */

RF_DECLARE_STATIC_MUTEX(rf_sparet_wait_mutex)

static RF_SparetWait_t *rf_sparet_wait_queue;	/* requests to install a
						 * spare table */
static RF_SparetWait_t *rf_sparet_resp_queue;	/* responses from
						 * installation process */

/* prototypes */
static void KernelWakeupFunc(struct buf * bp);
static void InitBP(struct buf * bp, struct vnode *, unsigned rw_flag, 
		   dev_t dev, RF_SectorNum_t startSect, 
		   RF_SectorCount_t numSect, caddr_t buf,
		   void (*cbFunc) (struct buf *), void *cbArg, 
		   int logBytesPerSector, struct proc * b_proc);
static void raidinit(RF_Raid_t *);

void raidattach(int);
int raidsize(dev_t);
int raidopen(dev_t, int, int, struct proc *);
int raidclose(dev_t, int, int, struct proc *);
int raidioctl(dev_t, u_long, caddr_t, int, struct proc *);
int raidwrite(dev_t, struct uio *, int);
int raidread(dev_t, struct uio *, int);
void raidstrategy(struct buf *);
int raiddump(dev_t, daddr_t, caddr_t, size_t);

/*
 * Pilfered from ccd.c
 */

struct raidbuf {
	struct buf rf_buf;	/* new I/O buf.  MUST BE FIRST!!! */
	struct buf *rf_obp;	/* ptr. to original I/O buf */
	int     rf_flags;	/* misc. flags */
	RF_DiskQueueData_t *req;/* the request that this was part of.. */
};


#define RAIDGETBUF(rs) pool_get(&(rs)->sc_cbufpool, PR_NOWAIT)
#define	RAIDPUTBUF(rs, cbp) pool_put(&(rs)->sc_cbufpool, cbp)

/* XXX Not sure if the following should be replacing the raidPtrs above,
   or if it should be used in conjunction with that... 
*/

struct raid_softc {
	int     sc_flags;	/* flags */
	int     sc_cflags;	/* configuration flags */
	size_t  sc_size;        /* size of the raid device */
	char    sc_xname[20];	/* XXX external name */
	struct disk sc_dkdev;	/* generic disk device info */
	struct pool sc_cbufpool;	/* component buffer pool */
	struct buf_queue buf_queue;	/* used for the device queue */
};
/* sc_flags */
#define RAIDF_INITED	0x01	/* unit has been initialized */
#define RAIDF_WLABEL	0x02	/* label area is writable */
#define RAIDF_LABELLING	0x04	/* unit is currently being labelled */
#define RAIDF_WANTED	0x40	/* someone is waiting to obtain a lock */
#define RAIDF_LOCKED	0x80	/* unit is locked */

#define	raidunit(x)	DISKUNIT(x)
int numraid = 0;

/* 
 * Allow RAIDOUTSTANDING number of simultaneous IO's to this RAID device. 
 * Be aware that large numbers can allow the driver to consume a lot of 
 * kernel memory, especially on writes, and in degraded mode reads.
 * 
 * For example: with a stripe width of 64 blocks (32k) and 5 disks, 
 * a single 64K write will typically require 64K for the old data, 
 * 64K for the old parity, and 64K for the new parity, for a total 
 * of 192K (if the parity buffer is not re-used immediately).
 * Even it if is used immedately, that's still 128K, which when multiplied
 * by say 10 requests, is 1280K, *on top* of the 640K of incoming data.
 * 
 * Now in degraded mode, for example, a 64K read on the above setup may
 * require data reconstruction, which will require *all* of the 4 remaining 
 * disks to participate -- 4 * 32K/disk == 128K again.
 */

#ifndef RAIDOUTSTANDING
#define RAIDOUTSTANDING   6
#endif

#define RAIDLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), raidunit((dev)), RAW_PART))

/* declared here, and made public, for the benefit of KVM stuff.. */
struct raid_softc *raid_softc;

static void raidgetdefaultlabel(RF_Raid_t *, struct raid_softc *, 
				     struct disklabel *);
static void raidgetdisklabel(dev_t);
static void raidmakedisklabel(struct raid_softc *);

static int raidlock(struct raid_softc *);
static void raidunlock(struct raid_softc *);

static void rf_markalldirty(RF_Raid_t *);
void rf_mountroot_hook(struct device *);

struct device *raidrootdev;

void rf_ReconThread(struct rf_recon_req *);
/* XXX what I want is: */
/*void rf_ReconThread(RF_Raid_t *raidPtr);  */
void rf_RewriteParityThread(RF_Raid_t *raidPtr);
void rf_CopybackThread(RF_Raid_t *raidPtr);
void rf_ReconstructInPlaceThread(struct rf_recon_req *);
void rf_buildroothack(void *);

RF_AutoConfig_t *rf_find_raid_components(void);
RF_ConfigSet_t *rf_create_auto_sets(RF_AutoConfig_t *);
static int rf_does_it_fit(RF_ConfigSet_t *,RF_AutoConfig_t *);
static int rf_reasonable_label(RF_ComponentLabel_t *);
void rf_create_configuration(RF_AutoConfig_t *,RF_Config_t *,
				  RF_Raid_t *);
int rf_set_autoconfig(RF_Raid_t *, int);
int rf_set_rootpartition(RF_Raid_t *, int);
void rf_release_all_vps(RF_ConfigSet_t *);
void rf_cleanup_config_set(RF_ConfigSet_t *);
int rf_have_enough_components(RF_ConfigSet_t *);
int rf_auto_config_set(RF_ConfigSet_t *, int *);

static int raidautoconfig = 0; /* Debugging, mostly.  Set to 0 to not
				  allow autoconfig to take place.
			          Note that this is overridden by having
			          RAID_AUTOCONFIG as an option in the 
			          kernel config file.  */

void
raidattach(num)
	int     num;
{
	int raidID;
	int i, rc;
	RF_AutoConfig_t *ac_list; /* autoconfig list */
	RF_ConfigSet_t *config_sets;

#ifdef DEBUG
	printf("raidattach: Asked for %d units\n", num);
#endif

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("raidattach: count <= 0");
#endif
		return;
	}
	/* This is where all the initialization stuff gets done. */

	numraid = num;

	/* Make some space for requested number of units... */

	RF_Calloc(raidPtrs, num, sizeof(RF_Raid_t *), (RF_Raid_t **));
	if (raidPtrs == NULL) {
		panic("raidPtrs is NULL!!");
	}
	
	rc = rf_mutex_init(&rf_sparet_wait_mutex);
	if (rc) {
		RF_PANIC();
	}

	rf_sparet_wait_queue = rf_sparet_resp_queue = NULL;

	for (i = 0; i < num; i++)
		raidPtrs[i] = NULL;
	rc = rf_BootRaidframe();
	if (rc == 0)
		printf("Kernelized RAIDframe activated\n");
	else
		panic("Serious error booting RAID!!");

	/* put together some datastructures like the CCD device does.. This
	 * lets us lock the device and what-not when it gets opened. */

	raid_softc = (struct raid_softc *)
	    malloc(num * sizeof(struct raid_softc),
		   M_RAIDFRAME, M_NOWAIT);
	if (raid_softc == NULL) {
		printf("WARNING: no memory for RAIDframe driver\n");
		return;
	}

	bzero(raid_softc, num * sizeof(struct raid_softc));
	
	raidrootdev = (struct device *)malloc(num * sizeof(struct device),
					      M_RAIDFRAME, M_NOWAIT);
	if (raidrootdev == NULL) {
		panic("No memory for RAIDframe driver!!?!?!");
	}

	for (raidID = 0; raidID < num; raidID++) {
		BUFQ_INIT(&raid_softc[raidID].buf_queue);

		raidrootdev[raidID].dv_class  = DV_DISK;
		raidrootdev[raidID].dv_cfdata = NULL;
		raidrootdev[raidID].dv_unit   = raidID;
		raidrootdev[raidID].dv_parent = NULL;
		raidrootdev[raidID].dv_flags  = 0;
		sprintf(raidrootdev[raidID].dv_xname,"raid%d",raidID);

		raid_softc[raidID].buf_queue.b_actf = NULL;
		raid_softc[raidID].buf_queue.b_actb = 
			&raid_softc[raidID].buf_queue.b_actf;
		RF_Calloc(raidPtrs[raidID], 1, sizeof(RF_Raid_t),
			  (RF_Raid_t *));
		if (raidPtrs[raidID] == NULL) {
			printf("WARNING: raidPtrs[%d] is NULL\n", raidID);
			numraid = raidID;
			return;
		}
	}

#if RAID_AUTOCONFIG
	raidautoconfig = 1;
#endif

if (raidautoconfig) {
	/* 1. locate all RAID components on the system */

#if DEBUG
	printf("Searching for raid components...\n");
#endif
	ac_list = rf_find_raid_components();

	/* 2. sort them into their respective sets */

	config_sets = rf_create_auto_sets(ac_list);

	/* 3. evaluate each set and configure the valid ones
	   This gets done in rf_buildroothack() */

	/* schedule the creation of the thread to do the 
	   "/ on RAID" stuff */

	kthread_create(rf_buildroothack,config_sets);

#if 0
	mountroothook_establish(rf_mountroot_hook, &raidrootdev[0]);
#endif
}

}

void
rf_buildroothack(arg)
	void *arg;
{
	RF_ConfigSet_t *config_sets = arg;
	RF_ConfigSet_t *cset;
	RF_ConfigSet_t *next_cset;
	int retcode;
	int raidID;
	int rootID;
	int num_root;

	num_root = 0;
	cset = config_sets;
	while(cset != NULL ) {
		next_cset = cset->next;
		if (rf_have_enough_components(cset) && 
		    cset->ac->clabel->autoconfigure==1) {
			retcode = rf_auto_config_set(cset,&raidID);
			if (!retcode) {
				if (cset->rootable) {
					rootID = raidID;
					num_root++;
				}
			} else {
				/* The autoconfig didn't work :( */
#if DEBUG
				printf("Autoconfig failed with code %d for raid%d\n", retcode, raidID);
#endif
				rf_release_all_vps(cset);
			}
		} else {
			/* we're not autoconfiguring this set...  
			   release the associated resources */
			rf_release_all_vps(cset);
		}
		/* cleanup */
		rf_cleanup_config_set(cset);
		cset = next_cset;
	}
	if (boothowto & RB_ASKNAME) {
		/* We don't auto-config... */
	} else {
		/* They didn't ask, and we found something bootable... */

		if (num_root == 1) {
			booted_device = &raidrootdev[rootID]; 
		} else if (num_root > 1) {
			/* we can't guess.. require the user to answer... */
			boothowto |= RB_ASKNAME;
		}
	}
}


int
raidsize(dev)
	dev_t   dev;
{
	struct raid_softc *rs;
	struct disklabel *lp;
	int     part, unit, omask, size;

	unit = raidunit(dev);
	if (unit >= numraid)
		return (-1);
	rs = &raid_softc[unit];

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (-1);

	part = DISKPART(dev);
	omask = rs->sc_dkdev.dk_openmask & (1 << part);
	lp = rs->sc_dkdev.dk_label;

	if (omask == 0 && raidopen(dev, 0, S_IFBLK, curproc))
		return (-1);

	if (lp->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = lp->d_partitions[part].p_size *
		    (lp->d_secsize / DEV_BSIZE);

	if (omask == 0 && raidclose(dev, 0, S_IFBLK, curproc))
		return (-1);

	return (size);

}

int
raiddump(dev, blkno, va, size)
	dev_t   dev;
	daddr_t blkno;
	caddr_t va;
	size_t  size;
{
	/* Not implemented. */
	return ENXIO;
}
/* ARGSUSED */
int
raidopen(dev, flags, fmt, p)
	dev_t   dev;
	int     flags, fmt;
	struct proc *p;
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	struct disklabel *lp;
	int     part, pmask;
	int     error = 0;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((error = raidlock(rs)) != 0)
		return (error);
	lp = rs->sc_dkdev.dk_label;

	part = DISKPART(dev);
	pmask = (1 << part);

	db1_printf(("Opening raid device number: %d partition: %d\n",
		unit, part));


	if ((rs->sc_flags & RAIDF_INITED) &&
	    (rs->sc_dkdev.dk_openmask == 0))
		raidgetdisklabel(dev);

	/* make sure that this partition exists */

	if (part != RAW_PART) {
		db1_printf(("Not a raw partition..\n"));
		if (((rs->sc_flags & RAIDF_INITED) == 0) ||
		    ((part >= lp->d_npartitions) ||
			(lp->d_partitions[part].p_fstype == FS_UNUSED))) {
			error = ENXIO;
			raidunlock(rs);
			db1_printf(("Bailing out...\n"));
			return (error);
		}
	}
	/* Prevent this unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		rs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		rs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}

	if ((rs->sc_dkdev.dk_openmask == 0) && 
	    ((rs->sc_flags & RAIDF_INITED) != 0)) {
		/* First one... mark things as dirty... Note that we *MUST*
		 have done a configure before this.  I DO NOT WANT TO BE
		 SCRIBBLING TO RANDOM COMPONENTS UNTIL IT'S BEEN DETERMINED
		 THAT THEY BELONG TOGETHER!!!!! */
		/* XXX should check to see if we're only open for reading
		   here... If so, we needn't do this, but then need some
		   other way of keeping track of what's happened.. */

		rf_markalldirty( raidPtrs[unit] );
	}


	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;

	raidunlock(rs);

	return (error);


}
/* ARGSUSED */
int
raidclose(dev, flags, fmt, p)
	dev_t   dev;
	int     flags, fmt;
	struct proc *p;
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	int     error = 0;
	int     part;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((error = raidlock(rs)) != 0)
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		rs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		rs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;
	
	if ((rs->sc_dkdev.dk_openmask == 0) &&
	    ((rs->sc_flags & RAIDF_INITED) != 0)) {
		/* Last one... device is not unconfigured yet.  
		   Device shutdown has taken care of setting the 
		   clean bits if RAIDF_INITED is not set 
		   mark things as clean... */
#if 0
		printf("Last one on raid%d.  Updating status.\n",unit);
#endif
		rf_update_component_labels(raidPtrs[unit],
						 RF_FINAL_COMPONENT_UPDATE);
	}

	raidunlock(rs);
	return (0);

}

void
raidstrategy(bp)
	struct buf *bp;
{
	int s;

	unsigned int raidID = raidunit(bp->b_dev);
	RF_Raid_t *raidPtr;
	struct raid_softc *rs = &raid_softc[raidID];
	struct disklabel *lp;
	int     wlabel;

	if ((rs->sc_flags & RAIDF_INITED) ==0) {
		bp->b_error = ENXIO;
		bp->b_flags = B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	}
	if (raidID >= numraid || !raidPtrs[raidID]) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	}
	raidPtr = raidPtrs[raidID];
	if (!raidPtr->valid) {
		bp->b_error = ENODEV;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	}
	if (bp->b_bcount == 0) {
		db1_printf(("b_bcount is zero..\n"));
		biodone(bp);
		return;
	}
	lp = rs->sc_dkdev.dk_label;

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.
	 */

	wlabel = rs->sc_flags & (RAIDF_WLABEL | RAIDF_LABELLING);
	if (DISKPART(bp->b_dev) != RAW_PART)
		if (bounds_check_with_label(bp, lp, wlabel) <= 0) {
			db1_printf(("Bounds check failed!!:%d %d\n",
				(int) bp->b_blkno, (int) wlabel));
			biodone(bp);
			return;
		}
	s = splbio();

	bp->b_resid = 0;

	/* stuff it onto our queue */
	BUFQ_INSERT_TAIL(&rs->buf_queue, bp);

	raidstart(raidPtrs[raidID]);

	splx(s);
}
/* ARGSUSED */
int
raidread(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int     flags;
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;
	int     part;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (ENXIO);
	part = DISKPART(dev);

	db1_printf(("raidread: unit: %d partition: %d\n", unit, part));

	return (physio(raidstrategy, NULL, dev, B_READ, minphys, uio));

}
/* ARGSUSED */
int
raidwrite(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int     flags;
{
	int     unit = raidunit(dev);
	struct raid_softc *rs;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (ENXIO);
	db1_printf(("raidwrite\n"));
	return (physio(raidstrategy, NULL, dev, B_WRITE, minphys, uio));

}

int
raidioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	u_long  cmd;
	caddr_t data;
	int     flag;
	struct proc *p;
{
	int     unit = raidunit(dev);
	int     error = 0;
	int     part, pmask;
	struct raid_softc *rs;
	RF_Config_t *k_cfg, *u_cfg;
	RF_Raid_t *raidPtr;
	RF_RaidDisk_t *diskPtr;
	RF_AccTotals_t *totals;
	RF_DeviceConfig_t *d_cfg, **ucfgp;
	u_char *specific_buf;
	int retcode = 0;
	int row;
	int column;
	struct rf_recon_req *rrcopy, *rr;
	RF_ComponentLabel_t *clabel;
	RF_ComponentLabel_t ci_label;
	RF_ComponentLabel_t **clabel_ptr;
	RF_SingleComponent_t *sparePtr,*componentPtr;
	RF_SingleComponent_t hot_spare;
	RF_SingleComponent_t component;
	RF_ProgressInfo_t progressInfo, **progressInfoPtr;
	int i, j, d;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];
	raidPtr = raidPtrs[unit];

	db1_printf(("raidioctl: %d %d %d %d\n", (int) dev,
		(int) DISKPART(dev), (int) unit, (int) cmd));

	/* Must be open for writes for these commands... */
	switch (cmd) {
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	/* Must be initialized for these... */
	switch (cmd) {
	case DIOCGDINFO:
	case DIOCSDINFO:
	case DIOCWDINFO:
	case DIOCGPART:
	case DIOCWLABEL:
	case DIOCGDEFLABEL:
	case RAIDFRAME_SHUTDOWN:
	case RAIDFRAME_REWRITEPARITY:
	case RAIDFRAME_GET_INFO:
	case RAIDFRAME_RESET_ACCTOTALS:
	case RAIDFRAME_GET_ACCTOTALS:
	case RAIDFRAME_KEEP_ACCTOTALS:
	case RAIDFRAME_GET_SIZE:
	case RAIDFRAME_FAIL_DISK:
	case RAIDFRAME_COPYBACK:
	case RAIDFRAME_CHECK_RECON_STATUS:
	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
	case RAIDFRAME_GET_COMPONENT_LABEL:
	case RAIDFRAME_SET_COMPONENT_LABEL:
	case RAIDFRAME_ADD_HOT_SPARE:
	case RAIDFRAME_REMOVE_HOT_SPARE:
	case RAIDFRAME_INIT_LABELS:
	case RAIDFRAME_REBUILD_IN_PLACE:
	case RAIDFRAME_CHECK_PARITY:
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT:
	case RAIDFRAME_CHECK_COPYBACK_STATUS:
	case RAIDFRAME_CHECK_COPYBACK_STATUS_EXT:
	case RAIDFRAME_SET_AUTOCONFIG:
	case RAIDFRAME_SET_ROOT:
	case RAIDFRAME_DELETE_COMPONENT:
	case RAIDFRAME_INCORPORATE_HOT_SPARE:
		if ((rs->sc_flags & RAIDF_INITED) == 0)
			return (ENXIO);
	}

	switch (cmd) {

		/* configure the system */
	case RAIDFRAME_CONFIGURE:

		if (raidPtr->valid) {
			/* There is a valid RAID set running on this unit! */
			printf("raid%d: Device already configured!\n",unit);
			return(EINVAL);
		}

		/* copy-in the configuration information */
		/* data points to a pointer to the configuration structure */

		u_cfg = *((RF_Config_t **) data);
		RF_Malloc(k_cfg, sizeof(RF_Config_t), (RF_Config_t *));
		if (k_cfg == NULL) {
			return (ENOMEM);
		}
		retcode = copyin((caddr_t) u_cfg, (caddr_t) k_cfg,
		    sizeof(RF_Config_t));
		if (retcode) {
			RF_Free(k_cfg, sizeof(RF_Config_t));
			db1_printf(("rf_ioctl: retcode=%d copyin.1\n",
				retcode));
			return (retcode);
		}
		/* allocate a buffer for the layout-specific data, and copy it
		 * in */
		if (k_cfg->layoutSpecificSize) {
			if (k_cfg->layoutSpecificSize > 10000) {
				/* sanity check */
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (EINVAL);
			}
			RF_Malloc(specific_buf, k_cfg->layoutSpecificSize,
			    (u_char *));
			if (specific_buf == NULL) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (ENOMEM);
			}
			retcode = copyin(k_cfg->layoutSpecific,
			    (caddr_t) specific_buf,
			    k_cfg->layoutSpecificSize);
			if (retcode) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				RF_Free(specific_buf, 
					k_cfg->layoutSpecificSize);
				db1_printf(("rf_ioctl: retcode=%d copyin.2\n",
					retcode));
				return (retcode);
			}
		} else
			specific_buf = NULL;
		k_cfg->layoutSpecific = specific_buf;

		/* should do some kind of sanity check on the configuration.
		 * Store the sum of all the bytes in the last byte? */

		/* configure the system */

		/*
		 * Clear the entire RAID descriptor, just to make sure
		 *  there is no stale data left in the case of a 
		 *  reconfiguration 
		 */
		bzero((char *) raidPtr, sizeof(RF_Raid_t));
		raidPtr->raidid = unit;

		retcode = rf_Configure(raidPtr, k_cfg, NULL);

		if (retcode == 0) {

			/* allow this many simultaneous IO's to 
			   this RAID device */
			raidPtr->openings = RAIDOUTSTANDING;
					
			raidinit(raidPtr);
			rf_markalldirty(raidPtr);
		}
		/* free the buffers.  No return code here. */
		if (k_cfg->layoutSpecificSize) {
			RF_Free(specific_buf, k_cfg->layoutSpecificSize);
		}
		RF_Free(k_cfg, sizeof(RF_Config_t));

		return (retcode);

		/* shutdown the system */
	case RAIDFRAME_SHUTDOWN:

		if ((error = raidlock(rs)) != 0)
			return (error);

		/*
		 * If somebody has a partition mounted, we shouldn't
		 * shutdown.
		 */

		part = DISKPART(dev);
		pmask = (1 << part);
		if ((rs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((rs->sc_dkdev.dk_bopenmask & pmask) &&
			(rs->sc_dkdev.dk_copenmask & pmask))) {
			raidunlock(rs);
			return (EBUSY);
		}

		retcode = rf_Shutdown(raidPtr);

		pool_destroy(&rs->sc_cbufpool);

		/* It's no longer initialized... */
		rs->sc_flags &= ~RAIDF_INITED;

		/* Detach the disk. */
		disk_detach(&rs->sc_dkdev);

		raidunlock(rs);

		return (retcode);
	case RAIDFRAME_GET_COMPONENT_LABEL:
		clabel_ptr = (RF_ComponentLabel_t **) data;
		/* need to read the component label for the disk indicated
		   by row,column in clabel */

		/* For practice, let's get it directly fromdisk, rather 
		   than from the in-core copy */
		RF_Malloc( clabel, sizeof( RF_ComponentLabel_t ),
			   (RF_ComponentLabel_t *));
		if (clabel == NULL)
			return (ENOMEM);

		bzero((char *) clabel, sizeof(RF_ComponentLabel_t));
		
		retcode = copyin( *clabel_ptr, clabel, 
				  sizeof(RF_ComponentLabel_t));

		if (retcode) {
			RF_Free( clabel, sizeof(RF_ComponentLabel_t));
			return(retcode);
		}

		row = clabel->row;
		column = clabel->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol +
				     raidPtr->numSpare)) {
			RF_Free( clabel, sizeof(RF_ComponentLabel_t));
			return(EINVAL);
		}

		raidread_component_label(raidPtr->Disks[row][column].dev, 
				raidPtr->raid_cinfo[row][column].ci_vp, 
				clabel );

		retcode = copyout((caddr_t) clabel, 
				  (caddr_t) *clabel_ptr,
				  sizeof(RF_ComponentLabel_t));
		RF_Free( clabel, sizeof(RF_ComponentLabel_t));
		return (retcode);

	case RAIDFRAME_SET_COMPONENT_LABEL:
		clabel = (RF_ComponentLabel_t *) data;

		/* XXX check the label for valid stuff... */
		/* Note that some things *should not* get modified --
		   the user should be re-initing the labels instead of 
		   trying to patch things.
		   */

		printf("Got component label:\n");
		printf("Version: %d\n",clabel->version);
		printf("Serial Number: %d\n",clabel->serial_number);
		printf("Mod counter: %d\n",clabel->mod_counter);
		printf("Row: %d\n", clabel->row);
		printf("Column: %d\n", clabel->column);
		printf("Num Rows: %d\n", clabel->num_rows);
		printf("Num Columns: %d\n", clabel->num_columns);
		printf("Clean: %d\n", clabel->clean);
		printf("Status: %d\n", clabel->status);

		row = clabel->row;
		column = clabel->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
		}

		/* XXX this isn't allowed to do anything for now :-) */

		/* XXX and before it is, we need to fill in the rest
		   of the fields!?!?!?! */
#if 0
		raidwrite_component_label( 
                            raidPtr->Disks[row][column].dev, 
			    raidPtr->raid_cinfo[row][column].ci_vp, 
			    clabel );
#endif
		return (0);

	case RAIDFRAME_INIT_LABELS:	
		clabel = (RF_ComponentLabel_t *) data;
		/* 
		   we only want the serial number from
		   the above.  We get all the rest of the information
		   from the config that was used to create this RAID
		   set. 
		   */

		raidPtr->serial_number = clabel->serial_number;
		
		raid_init_component_label(raidPtr, &ci_label);
		ci_label.serial_number = clabel->serial_number;

		for(row=0;row<raidPtr->numRow;row++) {
			ci_label.row = row;
			for(column=0;column<raidPtr->numCol;column++) {
				diskPtr = &raidPtr->Disks[row][column];
				ci_label.partitionSize = diskPtr->partitionSize;
				ci_label.column = column;
				raidwrite_component_label( 
				  raidPtr->Disks[row][column].dev, 
				  raidPtr->raid_cinfo[row][column].ci_vp, 
				  &ci_label );
			}
		}

		return (retcode);
	case RAIDFRAME_SET_AUTOCONFIG:
		d = rf_set_autoconfig(raidPtr, *(int *) data);
		printf("New autoconfig value is: %d\n", d);
		*(int *) data = d;
		return (retcode);

	case RAIDFRAME_SET_ROOT:
		d = rf_set_rootpartition(raidPtr, *(int *) data);
		printf("New rootpartition value is: %d\n", d);
		*(int *) data = d;
		return (retcode);

		/* initialize all parity */
	case RAIDFRAME_REWRITEPARITY:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Parity for RAID 0 is trivially correct */
			raidPtr->parity_good = RF_RAID_CLEAN;
			return(0);
		}

		if (raidPtr->parity_rewrite_in_progress == 1) {
			/* Re-write is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->parity_rewrite_thread,
					   rf_RewriteParityThread,
					   raidPtr,"raid_parity");
		return (retcode);


	case RAIDFRAME_ADD_HOT_SPARE:
		sparePtr = (RF_SingleComponent_t *) data;
		memcpy( &hot_spare, sparePtr, sizeof(RF_SingleComponent_t));
		retcode = rf_add_hot_spare(raidPtr, &hot_spare);
		return(retcode);

	case RAIDFRAME_REMOVE_HOT_SPARE:
		return(retcode);

	case RAIDFRAME_DELETE_COMPONENT:
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( &component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		retcode = rf_delete_component(raidPtr, &component);
		return(retcode);

	case RAIDFRAME_INCORPORATE_HOT_SPARE:
		componentPtr = (RF_SingleComponent_t *)data;
		memcpy( &component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		retcode = rf_incorporate_hot_spare(raidPtr, &component);
		return(retcode);

	case RAIDFRAME_REBUILD_IN_PLACE:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			return(EINVAL);
		}

		if (raidPtr->recon_in_progress == 1) {
			/* a reconstruct is already in progress! */
			return(EINVAL);
		}

		componentPtr = (RF_SingleComponent_t *) data;
		memcpy( &component, componentPtr, 
			sizeof(RF_SingleComponent_t));
		row = component.row;
		column = component.column;
		printf("Rebuild: %d %d\n",row, column);
		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
		}

		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);

		rrcopy->raidPtr = (void *) raidPtr;
		rrcopy->row = row;
		rrcopy->col = column;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconstructInPlaceThread,
					   rrcopy,"raid_reconip");
		return (retcode);

	case RAIDFRAME_GET_INFO:
		if (!raidPtr->valid)
			return (ENODEV);
		ucfgp = (RF_DeviceConfig_t **) data;
		RF_Malloc(d_cfg, sizeof(RF_DeviceConfig_t),
			  (RF_DeviceConfig_t *));
		if (d_cfg == NULL)
			return (ENOMEM);
		bzero((char *) d_cfg, sizeof(RF_DeviceConfig_t));
		d_cfg->rows = raidPtr->numRow;
		d_cfg->cols = raidPtr->numCol;
		d_cfg->ndevs = raidPtr->numRow * raidPtr->numCol;
		if (d_cfg->ndevs >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->nspares = raidPtr->numSpare;
		if (d_cfg->nspares >= RF_MAX_DISKS) {
			RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));
			return (ENOMEM);
		}
		d_cfg->maxqdepth = raidPtr->maxQueueDepth;
		d = 0;
		for (i = 0; i < d_cfg->rows; i++) {
			for (j = 0; j < d_cfg->cols; j++) {
				d_cfg->devs[d] = raidPtr->Disks[i][j];
				d++;
			}
		}
		for (j = d_cfg->cols, i = 0; i < d_cfg->nspares; i++, j++) {
			d_cfg->spares[i] = raidPtr->Disks[0][j];
		}
		retcode = copyout((caddr_t) d_cfg, (caddr_t) * ucfgp,
				  sizeof(RF_DeviceConfig_t));
		RF_Free(d_cfg, sizeof(RF_DeviceConfig_t));

		return (retcode);

	case RAIDFRAME_CHECK_PARITY:
		*(int *) data = raidPtr->parity_good;
		return (0);

	case RAIDFRAME_RESET_ACCTOTALS:
		bzero(&raidPtr->acc_totals, sizeof(raidPtr->acc_totals));
		return (0);

	case RAIDFRAME_GET_ACCTOTALS:
		totals = (RF_AccTotals_t *) data;
		*totals = raidPtr->acc_totals;
		return (0);

	case RAIDFRAME_KEEP_ACCTOTALS:
		raidPtr->keep_acc_totals = *(int *)data;
		return (0);

	case RAIDFRAME_GET_SIZE:
		*(int *) data = raidPtr->totalSectors;
		return (0);

		/* fail a disk & optionally start reconstruction */
	case RAIDFRAME_FAIL_DISK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* Can't do this on a RAID 0!! */
			return(EINVAL);
		}

		rr = (struct rf_recon_req *) data;

		if (rr->row < 0 || rr->row >= raidPtr->numRow
		    || rr->col < 0 || rr->col >= raidPtr->numCol)
			return (EINVAL);

		printf("raid%d: Failing the disk: row: %d col: %d\n",
		       unit, rr->row, rr->col);

		/* make a copy of the recon request so that we don't rely on
		 * the user's buffer */
		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);
		bcopy(rr, rrcopy, sizeof(*rr));
		rrcopy->raidPtr = (void *) raidPtr;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconThread,
					   rrcopy,"raid_recon");
		return (0);

		/* invoke a copyback operation after recon on whatever disk
		 * needs it, if any */
	case RAIDFRAME_COPYBACK:

		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0!! */
			return(EINVAL);
		}

		if (raidPtr->copyback_in_progress == 1) {
			/* Copyback is already in progress! */
			return(EINVAL);
		}

		retcode = RF_CREATE_THREAD(raidPtr->copyback_thread,
					   rf_CopybackThread,
					   raidPtr,"raid_copyback");
		return (retcode);

		/* return the percentage completion of reconstruction */
	case RAIDFRAME_CHECK_RECON_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			return (EINVAL);
		}
		row = 0; /* XXX we only consider a single row... */
		if (raidPtr->status[row] != rf_rs_reconstructing)
			*(int *) data = 100;
		else
			*(int *) data = raidPtr->reconControl[row]->percentComplete;
		return (0);

	case RAIDFRAME_CHECK_RECON_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		row = 0; /* XXX we only consider a single row... */
		if (raidPtr->status[row] != rf_rs_reconstructing) {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		} else {
			progressInfo.total = 
				raidPtr->reconControl[row]->numRUsTotal;
			progressInfo.completed = 
				raidPtr->reconControl[row]->numRUsComplete;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		}
		retcode = copyout((caddr_t) &progressInfo,
				  (caddr_t) *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0, so tell the
			   user it's done. */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->parity_rewrite_in_progress == 1) {
			*(int *) data = 100 * 
				raidPtr->parity_rewrite_stripes_done / 
				raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		if (raidPtr->parity_rewrite_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed = 
				raidPtr->parity_rewrite_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		retcode = copyout((caddr_t) &progressInfo,
				  (caddr_t) *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

	case RAIDFRAME_CHECK_COPYBACK_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			*(int *) data = 100;
			return(0);
		}
		if (raidPtr->copyback_in_progress == 1) {
			*(int *) data = 100 * raidPtr->copyback_stripes_done / raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_COPYBACK_STATUS_EXT:
		progressInfoPtr = (RF_ProgressInfo_t **) data;
		if (raidPtr->copyback_in_progress == 1) {
			progressInfo.total = raidPtr->Layout.numStripe;
			progressInfo.completed = 
				raidPtr->copyback_stripes_done;
			progressInfo.remaining = progressInfo.total -
				progressInfo.completed;
		} else {
			progressInfo.remaining = 0;
			progressInfo.completed = 100;
			progressInfo.total = 100;
		}
		retcode = copyout((caddr_t) &progressInfo,
				  (caddr_t) *progressInfoPtr,
				  sizeof(RF_ProgressInfo_t));
		return (retcode);

		/* the sparetable daemon calls this to wait for the kernel to
		 * need a spare table. this ioctl does not return until a
		 * spare table is needed. XXX -- calling mpsleep here in the
		 * ioctl code is almost certainly wrong and evil. -- XXX XXX
		 * -- I should either compute the spare table in the kernel,
		 * or have a different -- XXX XXX -- interface (a different
		 * character device) for delivering the table     -- XXX */
#if 0
	case RAIDFRAME_SPARET_WAIT:
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		while (!rf_sparet_wait_queue)
			mpsleep(&rf_sparet_wait_queue, (PZERO + 1) | PCATCH, "sparet wait", 0, (void *) simple_lock_addr(rf_sparet_wait_mutex), MS_LOCK_SIMPLE);
		waitreq = rf_sparet_wait_queue;
		rf_sparet_wait_queue = rf_sparet_wait_queue->next;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

		/* structure assignment */
		*((RF_SparetWait_t *) data) = *waitreq;	

		RF_Free(waitreq, sizeof(*waitreq));
		return (0);

		/* wakes up a process waiting on SPARET_WAIT and puts an error
		 * code in it that will cause the dameon to exit */
	case RAIDFRAME_ABORT_SPARET_WAIT:
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = -1;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_wait_queue;
		rf_sparet_wait_queue = waitreq;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		wakeup(&rf_sparet_wait_queue);
		return (0);

		/* used by the spare table daemon to deliver a spare table
		 * into the kernel */
	case RAIDFRAME_SEND_SPARET:

		/* install the spare table */
		retcode = rf_SetSpareTable(raidPtr, *(void **) data);

		/* respond to the requestor.  the return status of the spare
		 * table installation is passed in the "fcol" field */
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = retcode;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_resp_queue;
		rf_sparet_resp_queue = waitreq;
		wakeup(&rf_sparet_resp_queue);
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

		return (retcode);
#endif

	default:
		break;		/* fall through to the os-specific code below */

	}

	if (!raidPtr->valid)
		return (EINVAL);

	/*
	 * Add support for "regular" device ioctls here.
	 */

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *) data = *(rs->sc_dkdev.dk_label);
		break;

	case DIOCGPART:
		((struct partinfo *) data)->disklab = rs->sc_dkdev.dk_label;
		((struct partinfo *) data)->part =
		    &rs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((error = raidlock(rs)) != 0)
			return (error);

		rs->sc_flags |= RAIDF_LABELLING;

		error = setdisklabel(rs->sc_dkdev.dk_label,
		    (struct disklabel *) data, 0, rs->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(RAIDLABELDEV(dev),
				    raidstrategy, rs->sc_dkdev.dk_label,
				    rs->sc_dkdev.dk_cpulabel);
		}
		rs->sc_flags &= ~RAIDF_LABELLING;

		raidunlock(rs);

		if (error)
			return (error);
		break;

	case DIOCWLABEL:
		if (*(int *) data != 0)
			rs->sc_flags |= RAIDF_WLABEL;
		else
			rs->sc_flags &= ~RAIDF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		raidgetdefaultlabel(raidPtr, rs,
		    (struct disklabel *) data);
		break;

	default:
		retcode = ENOTTY;
	}
	return (retcode);

}


/* raidinit -- complete the rest of the initialization for the
   RAIDframe device.  */


static void
raidinit(raidPtr)
	RF_Raid_t *raidPtr;
{
	struct raid_softc *rs;
	int     unit;

	unit = raidPtr->raidid;

	rs = &raid_softc[unit];
	pool_init(&rs->sc_cbufpool, sizeof(struct raidbuf), 0,
		  0, 0, "raidpl", 0, NULL, NULL, M_RAIDFRAME);


	/* XXX should check return code first... */
	rs->sc_flags |= RAIDF_INITED;

	sprintf(rs->sc_xname, "raid%d", unit);	/* XXX doesn't check bounds. */

	rs->sc_dkdev.dk_name = rs->sc_xname;

	/* disk_attach actually creates space for the CPU disklabel, among
	 * other things, so it's critical to call this *BEFORE* we try putzing
	 * with disklabels. */

	disk_attach(&rs->sc_dkdev);

	/* XXX There may be a weird interaction here between this, and
	 * protectedSectors, as used in RAIDframe.  */

	rs->sc_size = raidPtr->totalSectors;

}

/* wake up the daemon & tell it to get us a spare table
 * XXX
 * the entries in the queues should be tagged with the raidPtr
 * so that in the extremely rare case that two recons happen at once, 
 * we know for which device were requesting a spare table
 * XXX
 * 
 * XXX This code is not currently used. GO
 */
int 
rf_GetSpareTableFromDaemon(req)
	RF_SparetWait_t *req;
{
	int     retcode;

	RF_LOCK_MUTEX(rf_sparet_wait_mutex);
	req->next = rf_sparet_wait_queue;
	rf_sparet_wait_queue = req;
	wakeup(&rf_sparet_wait_queue);

	/* mpsleep unlocks the mutex */
	while (!rf_sparet_resp_queue) {
		tsleep(&rf_sparet_resp_queue, PRIBIO,
		    "raidframe getsparetable", 0);
	}
	req = rf_sparet_resp_queue;
	rf_sparet_resp_queue = req->next;
	RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

	retcode = req->fcol;
	RF_Free(req, sizeof(*req));	/* this is not the same req as we
					 * alloc'd */
	return (retcode);
}

/* a wrapper around rf_DoAccess that extracts appropriate info from the 
 * bp & passes it down.
 * any calls originating in the kernel must use non-blocking I/O
 * do some extra sanity checking to return "appropriate" error values for
 * certain conditions (to make some standard utilities work)
 * 
 * Formerly known as: rf_DoAccessKernel
 */
void
raidstart(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_SectorCount_t num_blocks, pb, sum;
	RF_RaidAddr_t raid_addr;
	int     retcode;
	struct partition *pp;
	daddr_t blocknum;
	int     unit;
	struct raid_softc *rs;
	int     do_async;
	struct buf *bp;

	unit = raidPtr->raidid;
	rs = &raid_softc[unit];

	/* quick check to see if anything has died recently */
	RF_LOCK_MUTEX(raidPtr->mutex);
	if (raidPtr->numNewFailures > 0) {
		rf_update_component_labels(raidPtr, 
					   RF_NORMAL_COMPONENT_UPDATE);
		raidPtr->numNewFailures--;
	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);

	/* Check to see if we're at the limit... */
	RF_LOCK_MUTEX(raidPtr->mutex);
	while (raidPtr->openings > 0) {
		RF_UNLOCK_MUTEX(raidPtr->mutex);

		/* get the next item, if any, from the queue */
		if ((bp = BUFQ_FIRST(&rs->buf_queue)) == NULL) {
			/* nothing more to do */
			return;
		}
		BUFQ_REMOVE(&rs->buf_queue, bp);

	/* Ok, for the bp we have here, bp->b_blkno is relative to the
		 * partition.. Need to make it absolute to the underlying 
		 * device.. */

	blocknum = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &rs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)];
		blocknum += pp->p_offset;
	}

		db1_printf(("Blocks: %d, %d\n", (int) bp->b_blkno, 
			    (int) blocknum));

	db1_printf(("bp->b_bcount = %d\n", (int) bp->b_bcount));
	db1_printf(("bp->b_resid = %d\n", (int) bp->b_resid));

		/* *THIS* is where we adjust what block we're going to... 
		 * but DO NOT TOUCH bp->b_blkno!!! */
	raid_addr = blocknum;

	num_blocks = bp->b_bcount >> raidPtr->logBytesPerSector;
	pb = (bp->b_bcount & raidPtr->sectorMask) ? 1 : 0;
	sum = raid_addr + num_blocks + pb;
	if (1 || rf_debugKernelAccess) {
		db1_printf(("raid_addr=%d sum=%d num_blocks=%d(+%d) (%d)\n",
			(int) raid_addr, (int) sum, (int) num_blocks,
			(int) pb, (int) bp->b_resid));
	}
	if ((sum > raidPtr->totalSectors) || (sum < raid_addr)
	    || (sum < num_blocks) || (sum < pb)) {
		bp->b_error = ENOSPC;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
			RF_LOCK_MUTEX(raidPtr->mutex);
			continue;
	}
	/*
	 * XXX rf_DoAccess() should do this, not just DoAccessKernel()
	 */

	if (bp->b_bcount & raidPtr->sectorMask) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
			RF_LOCK_MUTEX(raidPtr->mutex);
			continue;
			
	}
	db1_printf(("Calling DoAccess..\n"));


		RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->openings--;
	RF_UNLOCK_MUTEX(raidPtr->mutex);

	/*
	 * Everything is async.
	 */
	do_async = 1;
		
		/* don't ever condition on bp->b_flags & B_WRITE.  
		 * always condition on B_READ instead */

		/* XXX we're still at splbio() here... do we *really* 
		   need to be? */

		
  		retcode = rf_DoAccess(raidPtr, (bp->b_flags & B_READ) ?
  				      RF_IO_TYPE_READ : RF_IO_TYPE_WRITE,
  				      do_async, raid_addr, num_blocks,
 				      bp->b_data, bp, NULL, NULL, 
  				      RF_DAG_NONBLOCKING_IO, NULL, NULL, NULL);
  
		RF_LOCK_MUTEX(raidPtr->mutex);
	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);
}




/* invoke an I/O from kernel mode.  Disk queue should be locked upon entry */

int 
rf_DispatchKernelIO(queue, req)
	RF_DiskQueue_t *queue;
	RF_DiskQueueData_t *req;
{
	int     op = (req->type == RF_IO_TYPE_READ) ? B_READ : B_WRITE;
	struct buf *bp;
	struct raidbuf *raidbp = NULL;
	struct raid_softc *rs;
	int     unit;
	int s;

	s=0;
	/* s = splbio();*/ /* want to test this */
	/* XXX along with the vnode, we also need the softc associated with
	 * this device.. */

	req->queue = queue;

	unit = queue->raidPtr->raidid;

	db1_printf(("DispatchKernelIO unit: %d\n", unit));

	if (unit >= numraid) {
		printf("Invalid unit number: %d %d\n", unit, numraid);
		panic("Invalid Unit number in rf_DispatchKernelIO");
	}
	rs = &raid_softc[unit];

	/* XXX is this the right place? */
	disk_busy(&rs->sc_dkdev);

	bp = req->bp;
#if 1
	/* XXX when there is a physical disk failure, someone is passing us a
	 * buffer that contains old stuff!!  Attempt to deal with this problem
	 * without taking a performance hit... (not sure where the real bug
	 * is.  It's buried in RAIDframe somewhere) :-(  GO ) */

	if (bp->b_flags & B_ERROR) {
		bp->b_flags &= ~B_ERROR;
	}
	if (bp->b_error != 0) {
		bp->b_error = 0;
	}
#endif
	raidbp = RAIDGETBUF(rs);

	raidbp->rf_flags = 0;	/* XXX not really used anywhere... */

	/*
	 * context for raidiodone
	 */
	raidbp->rf_obp = bp;
	raidbp->req = req;

	LIST_INIT(&raidbp->rf_buf.b_dep);

	switch (req->type) {
	case RF_IO_TYPE_NOP:	/* used primarily to unlock a locked queue */
		/* XXX need to do something extra here.. */
		/* I'm leaving this in, as I've never actually seen it used,
		 * and I'd like folks to report it... GO */
		printf(("WAKEUP CALLED\n"));
		queue->numOutstanding++;

		/* XXX need to glue the original buffer into this??  */

		KernelWakeupFunc(&raidbp->rf_buf);
		break;

	case RF_IO_TYPE_READ:
	case RF_IO_TYPE_WRITE:

		if (req->tracerec) {
			RF_ETIMER_START(req->tracerec->timer);
		}
		InitBP(&raidbp->rf_buf, queue->rf_cinfo->ci_vp,
		    op | bp->b_flags, queue->rf_cinfo->ci_dev,
		    req->sectorOffset, req->numSector,
		    req->buf, KernelWakeupFunc, (void *) req,
		    queue->raidPtr->logBytesPerSector, req->b_proc);

		if (rf_debugKernelAccess) {
			db1_printf(("dispatch: bp->b_blkno = %ld\n",
				(long) bp->b_blkno));
		}
		queue->numOutstanding++;
		queue->last_deq_sector = req->sectorOffset;
		/* acc wouldn't have been let in if there were any pending
		 * reqs at any other priority */
		queue->curPriority = req->priority;

		db1_printf(("Going for %c to unit %d row %d col %d\n",
			req->type, unit, queue->row, queue->col));
		db1_printf(("sector %d count %d (%d bytes) %d\n",
			(int) req->sectorOffset, (int) req->numSector,
			(int) (req->numSector <<
			    queue->raidPtr->logBytesPerSector),
			(int) queue->raidPtr->logBytesPerSector));
		if ((raidbp->rf_buf.b_flags & B_READ) == 0) {
			raidbp->rf_buf.b_vp->v_numoutput++;
		}
		VOP_STRATEGY(&raidbp->rf_buf);

		break;

	default:
		panic("bad req->type in rf_DispatchKernelIO");
	}
	db1_printf(("Exiting from DispatchKernelIO\n"));
	/* splx(s); */ /* want to test this */
	return (0);
}
/* this is the callback function associated with a I/O invoked from
   kernel code.
 */
static void 
KernelWakeupFunc(vbp)
	struct buf *vbp;
{
	RF_DiskQueueData_t *req = NULL;
	RF_DiskQueue_t *queue;
	struct raidbuf *raidbp = (struct raidbuf *) vbp;
	struct buf *bp;
	struct raid_softc *rs;
	int     unit;
	int s;

	s = splbio();
	db1_printf(("recovering the request queue:\n"));
	req = raidbp->req;

	bp = raidbp->rf_obp;

	queue = (RF_DiskQueue_t *) req->queue;

	if (raidbp->rf_buf.b_flags & B_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = raidbp->rf_buf.b_error ?
		    raidbp->rf_buf.b_error : EIO;
	}

	/* XXX methinks this could be wrong... */
#if 1
	bp->b_resid = raidbp->rf_buf.b_resid;
#endif

	if (req->tracerec) {
		RF_ETIMER_STOP(req->tracerec->timer);
		RF_ETIMER_EVAL(req->tracerec->timer);
		RF_LOCK_MUTEX(rf_tracing_mutex);
		req->tracerec->diskwait_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->phys_io_us += RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->num_phys_ios++;
		RF_UNLOCK_MUTEX(rf_tracing_mutex);
	}
	bp->b_bcount = raidbp->rf_buf.b_bcount;	/* XXXX ?? */

	unit = queue->raidPtr->raidid;	/* *Much* simpler :-> */


	/* XXX Ok, let's get aggressive... If B_ERROR is set, let's go
	 * ballistic, and mark the component as hosed... */

	if (bp->b_flags & B_ERROR) {
		/* Mark the disk as dead */
		/* but only mark it once... */
		if (queue->raidPtr->Disks[queue->row][queue->col].status ==
		    rf_ds_optimal) {
			printf("raid%d: IO Error.  Marking %s as failed.\n",
			    unit, queue->raidPtr->Disks[queue->row][queue->col].devname);
			queue->raidPtr->Disks[queue->row][queue->col].status =
			    rf_ds_failed;
			queue->raidPtr->status[queue->row] = rf_rs_degraded;
			queue->raidPtr->numFailures++;
			queue->raidPtr->numNewFailures++;
		} else {	/* Disk is already dead... */
			/* printf("Disk already marked as dead!\n"); */
		}

	}

	rs = &raid_softc[unit];
	RAIDPUTBUF(rs, raidbp);


	if (bp->b_resid == 0) {
		/* XXX is this the right place for a disk_unbusy()??!??!?!? */
		disk_unbusy(&rs->sc_dkdev, (bp->b_bcount - bp->b_resid));
	}

	rf_DiskIOComplete(queue, req, (bp->b_flags & B_ERROR) ? 1 : 0);
	(req->CompleteFunc) (req->argument, (bp->b_flags & B_ERROR) ? 1 : 0);

	splx(s);
}



/*
 * initialize a buf structure for doing an I/O in the kernel.
 */
static void 
InitBP(bp, b_vp, rw_flag, dev, startSect, numSect, buf, cbFunc, cbArg,
       logBytesPerSector, b_proc)
	struct buf *bp;
	struct vnode *b_vp;
	unsigned rw_flag;
	dev_t dev;
	RF_SectorNum_t startSect;
	RF_SectorCount_t numSect;
	caddr_t buf;
	void (*cbFunc) (struct buf *);
	void *cbArg;
	int logBytesPerSector;
	struct proc *b_proc;
{
	/* bp->b_flags       = B_PHYS | rw_flag; */
	bp->b_flags = B_CALL | rw_flag;	/* XXX need B_PHYS here too??? */
	bp->b_bcount = numSect << logBytesPerSector;
	bp->b_bufsize = bp->b_bcount;
	bp->b_error = 0;
	bp->b_dev = dev;
	bp->b_data = buf;
	bp->b_blkno = startSect;
	bp->b_resid = bp->b_bcount;	/* XXX is this right!??!?!! */
	if (bp->b_bcount == 0) {
		panic("bp->b_bcount is zero in InitBP!!");
	}
	bp->b_proc = b_proc;
	bp->b_iodone = cbFunc;
	bp->b_vp = b_vp;

}

static void
raidgetdefaultlabel(raidPtr, rs, lp)
	RF_Raid_t *raidPtr;
	struct raid_softc *rs;
	struct disklabel *lp;
{
	db1_printf(("Building a default label...\n"));
	bzero(lp, sizeof(*lp));

	/* fabricate a label... */
	lp->d_secperunit = raidPtr->totalSectors;
	lp->d_secsize = raidPtr->bytesPerSector;
	lp->d_nsectors = raidPtr->Layout.dataSectorsPerStripe;
	lp->d_ntracks = 1;
	lp->d_ncylinders = raidPtr->totalSectors / 
		(lp->d_nsectors * lp->d_ntracks);
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "raid", sizeof(lp->d_typename));
	lp->d_type = DTYPE_RAID;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = raidPtr->totalSectors;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(rs->sc_dkdev.dk_label);

}
/*
 * Read the disklabel from the raid device.  If one is not present, fake one
 * up.
 */
static void
raidgetdisklabel(dev)
	dev_t   dev;
{
	int     unit = raidunit(dev);
	struct raid_softc *rs = &raid_softc[unit];
	char   *errstring;
	struct disklabel *lp = rs->sc_dkdev.dk_label;
	struct cpu_disklabel *clp = rs->sc_dkdev.dk_cpulabel;
	RF_Raid_t *raidPtr;

	db1_printf(("Getting the disklabel...\n"));

	bzero(clp, sizeof(*clp));

	raidPtr = raidPtrs[unit];

	raidgetdefaultlabel(raidPtr, rs, lp);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	errstring = readdisklabel(RAIDLABELDEV(dev), raidstrategy,
	    rs->sc_dkdev.dk_label, rs->sc_dkdev.dk_cpulabel);
	if (errstring)
		raidmakedisklabel(rs);
	else {
		int     i;
		struct partition *pp;

		/*
		 * Sanity check whether the found disklabel is valid.
		 *
		 * This is necessary since total size of the raid device
		 * may vary when an interleave is changed even though exactly
		 * same componets are used, and old disklabel may used
		 * if that is found.
		 */
		if (lp->d_secperunit != rs->sc_size)
			printf("WARNING: %s: "
			    "total sector size in disklabel (%d) != "
			    "the size of raid (%ld)\n", rs->sc_xname,
			    lp->d_secperunit, (long) rs->sc_size);
		for (i = 0; i < lp->d_npartitions; i++) {
			pp = &lp->d_partitions[i];
			if (pp->p_offset + pp->p_size > rs->sc_size)
				printf("WARNING: %s: end of partition `%c' "
				    "exceeds the size of raid (%ld)\n",
				    rs->sc_xname, 'a' + i, (long) rs->sc_size);
		}
	}

}
/*
 * Take care of things one might want to take care of in the event
 * that a disklabel isn't present.
 */
static void
raidmakedisklabel(rs)
	struct raid_softc *rs;
{
	struct disklabel *lp = rs->sc_dkdev.dk_label;
	db1_printf(("Making a label..\n"));

	/*
	 * For historical reasons, if there's no disklabel present
	 * the raw partition must be marked FS_BSDFFS.
	 */

	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;

	strncpy(lp->d_packname, "default label", sizeof(lp->d_packname));

	lp->d_checksum = dkcksum(lp);
}
/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 * You'll find the original of this in ccd.c
 */
int
raidlookup(path, p, vpp)
	char   *path;
	struct proc *p;
	struct vnode **vpp;	/* result */
{
	struct nameidata nd;
	struct vnode *vp;
	struct vattr va;
	int     error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, p);
	if ((error = vn_open(&nd, FREAD | FWRITE, 0)) != 0) {
#ifdef DEBUG
		printf("RAIDframe: vn_open returned %d\n", error);
#endif
		return (error);
	}
	vp = nd.ni_vp;
	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp, 0);
		(void) vn_close(vp, FREAD | FWRITE, p->p_ucred, p);
		return (EBUSY);
	}
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
		VOP_UNLOCK(vp, 0);
		(void) vn_close(vp, FREAD | FWRITE, p->p_ucred, p);
		return (error);
	}
	/* XXX: eventually we should handle VREG, too. */
	if (va.va_type != VBLK) {
		VOP_UNLOCK(vp, 0);
		(void) vn_close(vp, FREAD | FWRITE, p->p_ucred, p);
		return (ENOTBLK);
	}
	VOP_UNLOCK(vp, 0);
	*vpp = vp;
	return (0);
}
/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 * (Hmm... where have we seen this warning before :->  GO )
 */
static int
raidlock(rs)
	struct raid_softc *rs;
{
	int     error;

	while ((rs->sc_flags & RAIDF_LOCKED) != 0) {
		rs->sc_flags |= RAIDF_WANTED;
		if ((error =
			tsleep(rs, PRIBIO | PCATCH, "raidlck", 0)) != 0)
			return (error);
	}
	rs->sc_flags |= RAIDF_LOCKED;
	return (0);
}
/*
 * Unlock and wake up any waiters.
 */
static void
raidunlock(rs)
	struct raid_softc *rs;
{

	rs->sc_flags &= ~RAIDF_LOCKED;
	if ((rs->sc_flags & RAIDF_WANTED) != 0) {
		rs->sc_flags &= ~RAIDF_WANTED;
		wakeup(rs);
	}
}
 

#define RF_COMPONENT_INFO_OFFSET  16384 /* bytes */
#define RF_COMPONENT_INFO_SIZE     1024 /* bytes */

int 
raidmarkclean(dev_t dev, struct vnode *b_vp, int mod_counter)
{
	RF_ComponentLabel_t clabel;
	raidread_component_label(dev, b_vp, &clabel);
	clabel.mod_counter = mod_counter;
	clabel.clean = RF_RAID_CLEAN;
	raidwrite_component_label(dev, b_vp, &clabel);
	return(0);
}


int 
raidmarkdirty(dev_t dev, struct vnode *b_vp, int mod_counter)
{
	RF_ComponentLabel_t clabel;
	raidread_component_label(dev, b_vp, &clabel);
	clabel.mod_counter = mod_counter;
	clabel.clean = RF_RAID_DIRTY;
	raidwrite_component_label(dev, b_vp, &clabel);
	return(0);
}

/* ARGSUSED */
int
raidread_component_label(dev, b_vp, clabel)
	dev_t dev;
	struct vnode *b_vp;
	RF_ComponentLabel_t *clabel;
{
	struct buf *bp;
	int error;
	
	/* XXX should probably ensure that we don't try to do this if
	   someone has changed rf_protected_sectors. */ 

	/* get a block of the appropriate size... */
	bp = geteblk((int)RF_COMPONENT_INFO_SIZE);
	bp->b_dev = dev;

	/* get our ducks in a row for the read */
	bp->b_blkno = RF_COMPONENT_INFO_OFFSET / DEV_BSIZE;
	bp->b_bcount = RF_COMPONENT_INFO_SIZE;
	bp->b_flags = B_BUSY | B_READ;
 	bp->b_resid = RF_COMPONENT_INFO_SIZE / DEV_BSIZE;

	(*bdevsw[major(bp->b_dev)].d_strategy)(bp);

	error = biowait(bp); 

	if (!error) {
		memcpy(clabel, bp->b_data,
		       sizeof(RF_ComponentLabel_t));
#if 0
		rf_print_component_label( clabel );
#endif
        } else {
#if 0
		printf("Failed to read RAID component label!\n");
#endif
	}

        bp->b_flags = B_INVAL | B_AGE;
	brelse(bp); 
	return(error);
}
/* ARGSUSED */
int 
raidwrite_component_label(dev, b_vp, clabel)
	dev_t dev; 
	struct vnode *b_vp;
	RF_ComponentLabel_t *clabel;
{
	struct buf *bp;
	int error;

	/* get a block of the appropriate size... */
	bp = geteblk((int)RF_COMPONENT_INFO_SIZE);
	bp->b_dev = dev;

	/* get our ducks in a row for the write */
	bp->b_blkno = RF_COMPONENT_INFO_OFFSET / DEV_BSIZE;
	bp->b_bcount = RF_COMPONENT_INFO_SIZE;
	bp->b_flags = B_BUSY | B_WRITE;
 	bp->b_resid = RF_COMPONENT_INFO_SIZE / DEV_BSIZE;

	memset(bp->b_data, 0, RF_COMPONENT_INFO_SIZE );

	memcpy(bp->b_data, clabel, sizeof(RF_ComponentLabel_t));

	(*bdevsw[major(bp->b_dev)].d_strategy)(bp);
	error = biowait(bp); 
        bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	if (error) {
#if 1
		printf("Failed to write RAID component info!\n");
#endif
	}

	return(error);
}

void 
rf_markalldirty(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t clabel;
	int r,c;

	raidPtr->mod_counter++;
	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			if (raidPtr->Disks[r][c].status != rf_ds_failed) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&clabel);
				if (clabel.status == rf_ds_spared) {
					/* XXX do something special... 
					 but whatever you do, don't 
					 try to access it!! */
				} else {
#if 0
				clabel.status = 
					raidPtr->Disks[r][c].status;
				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&clabel);
#endif
				raidmarkdirty( 
				       raidPtr->Disks[r][c].dev, 
				       raidPtr->raid_cinfo[r][c].ci_vp,
				       raidPtr->mod_counter);
				}
			}
		} 
	}
	/* printf("Component labels marked dirty.\n"); */
#if 0
	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[r][sparecol].status == rf_ds_used_spare) {
			/* 

			   XXX this is where we get fancy and map this spare
			   into it's correct spot in the array.

			 */
			/* 
			   
			   we claim this disk is "optimal" if it's 
			   rf_ds_used_spare, as that means it should be 
			   directly substitutable for the disk it replaced. 
			   We note that too...

			 */

			for(i=0;i<raidPtr->numRow;i++) {
				for(j=0;j<raidPtr->numCol;j++) {
					if ((raidPtr->Disks[i][j].spareRow == 
					     r) &&
					    (raidPtr->Disks[i][j].spareCol ==
					     sparecol)) {
						srow = r;
						scol = sparecol;
						break;
					}
				}
			}
			
			raidread_component_label( 
				      raidPtr->Disks[r][sparecol].dev,
				      raidPtr->raid_cinfo[r][sparecol].ci_vp,
				      &clabel);
			/* make sure status is noted */
			clabel.version = RF_COMPONENT_LABEL_VERSION; 
			clabel.mod_counter = raidPtr->mod_counter;
			clabel.serial_number = raidPtr->serial_number;
			clabel.row = srow;
			clabel.column = scol;
			clabel.num_rows = raidPtr->numRow;
			clabel.num_columns = raidPtr->numCol;
			clabel.clean = RF_RAID_DIRTY; /* changed in a bit*/
			clabel.status = rf_ds_optimal;
			raidwrite_component_label(
				      raidPtr->Disks[r][sparecol].dev,
				      raidPtr->raid_cinfo[r][sparecol].ci_vp,
				      &clabel);
			raidmarkclean( raidPtr->Disks[r][sparecol].dev, 
			              raidPtr->raid_cinfo[r][sparecol].ci_vp);
		}
	}

#endif
}


void
rf_update_component_labels(raidPtr, final)
	RF_Raid_t *raidPtr;
	int final;
{
	RF_ComponentLabel_t clabel;
	int sparecol;
	int r,c;
	int i,j;
	int srow, scol;

	srow = -1;
	scol = -1;

	/* XXX should do extra checks to make sure things really are clean, 
	   rather than blindly setting the clean bit... */

	raidPtr->mod_counter++;

	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			if (raidPtr->Disks[r][c].status == rf_ds_optimal) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&clabel);
				/* make sure status is noted */
				clabel.status = rf_ds_optimal;
				/* bump the counter */
				clabel.mod_counter = raidPtr->mod_counter;

				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&clabel);
				if (final == RF_FINAL_COMPONENT_UPDATE) {
					if (raidPtr->parity_good == RF_RAID_CLEAN) {
						raidmarkclean( 
							      raidPtr->Disks[r][c].dev, 
							      raidPtr->raid_cinfo[r][c].ci_vp,
							      raidPtr->mod_counter);
					}
				}
			} 
			/* else we don't touch it.. */
		} 
	}

	for( c = 0; c < raidPtr->numSpare ; c++) {
		sparecol = raidPtr->numCol + c;
		if (raidPtr->Disks[0][sparecol].status == rf_ds_used_spare) {
			/* 
			   
			   we claim this disk is "optimal" if it's 
			   rf_ds_used_spare, as that means it should be 
			   directly substitutable for the disk it replaced. 
			   We note that too...

			 */

			for(i=0;i<raidPtr->numRow;i++) {
				for(j=0;j<raidPtr->numCol;j++) {
					if ((raidPtr->Disks[i][j].spareRow == 
					     0) &&
					    (raidPtr->Disks[i][j].spareCol ==
					     sparecol)) {
						srow = i;
						scol = j;
						break;
					}
				}
			}
			
			/* XXX shouldn't *really* need this... */
			raidread_component_label( 
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      &clabel);
			/* make sure status is noted */

			raid_init_component_label(raidPtr, &clabel);

			clabel.mod_counter = raidPtr->mod_counter;
			clabel.row = srow;
			clabel.column = scol;
			clabel.status = rf_ds_optimal;

			raidwrite_component_label(
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      &clabel);
			if (final == RF_FINAL_COMPONENT_UPDATE) {
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean( raidPtr->Disks[0][sparecol].dev,
						       raidPtr->raid_cinfo[0][sparecol].ci_vp,
						       raidPtr->mod_counter);
				}
			}
		}
	}
	/* 	printf("Component labels updated\n"); */
}

void
rf_close_component(raidPtr, vp, auto_configured)
	RF_Raid_t *raidPtr;
	struct vnode *vp;
	int auto_configured;
{
	struct proc *p;

	p = raidPtr->engine_thread;

	if (vp != NULL) {
		if (auto_configured == 1) {
			VOP_CLOSE(vp, FREAD, NOCRED, 0);
			vput(vp);
			
		} else {				
			VOP_UNLOCK(vp, 0);
			(void) vn_close(vp, FREAD | FWRITE, p->p_ucred, p);
		}
	} else {
		printf("vnode was NULL\n");
	}
}


void
rf_UnconfigureVnodes(raidPtr)
	RF_Raid_t *raidPtr;
{
	int r,c; 
	struct proc *p;
	struct vnode *vp;
	int acd;


	/* We take this opportunity to close the vnodes like we should.. */

	p = raidPtr->engine_thread;

	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			printf("Closing vnode for row: %d col: %d\n", r, c);
			vp = raidPtr->raid_cinfo[r][c].ci_vp;
			acd = raidPtr->Disks[r][c].auto_configured;
			rf_close_component(raidPtr, vp, acd);
			raidPtr->raid_cinfo[r][c].ci_vp = NULL;
			raidPtr->Disks[r][c].auto_configured = 0;
		}
	}
	for (r = 0; r < raidPtr->numSpare; r++) {
		printf("Closing vnode for spare: %d\n", r);
		vp = raidPtr->raid_cinfo[0][raidPtr->numCol + r].ci_vp;
		acd = raidPtr->Disks[0][raidPtr->numCol + r].auto_configured;
		rf_close_component(raidPtr, vp, acd);
		raidPtr->raid_cinfo[0][raidPtr->numCol + r].ci_vp = NULL;
		raidPtr->Disks[0][raidPtr->numCol + r].auto_configured = 0;
	}
}


void 
rf_ReconThread(req)
	struct rf_recon_req *req;
{
	int     s;
	RF_Raid_t *raidPtr;

	s = splbio();
	raidPtr = (RF_Raid_t *) req->raidPtr;
	raidPtr->recon_in_progress = 1;

	rf_FailDisk((RF_Raid_t *) req->raidPtr, req->row, req->col,
		    ((req->flags & RF_FDFLAGS_RECON) ? 1 : 0));

	/* XXX get rid of this! we don't need it at all.. */
	RF_Free(req, sizeof(*req));

	raidPtr->recon_in_progress = 0;
	splx(s);

	/* That's all... */
	kthread_exit(0);        /* does not return */
}

void
rf_RewriteParityThread(raidPtr)
	RF_Raid_t *raidPtr;
{
	int retcode;
	int s;

	raidPtr->parity_rewrite_in_progress = 1;
	s = splbio();
	retcode = rf_RewriteParity(raidPtr);
	splx(s);
	if (retcode) {
		printf("raid%d: Error re-writing parity!\n",raidPtr->raidid);
	} else {
		/* set the clean bit!  If we shutdown correctly,
		   the clean bit on each component label will get
		   set */
		raidPtr->parity_good = RF_RAID_CLEAN;
	}
	raidPtr->parity_rewrite_in_progress = 0;

	/* Anyone waiting for us to stop?  If so, inform them... */
	if (raidPtr->waitShutdown) {
		wakeup(&raidPtr->parity_rewrite_in_progress);
	}

	/* That's all... */
	kthread_exit(0);        /* does not return */
}


void
rf_CopybackThread(raidPtr)
	RF_Raid_t *raidPtr;
{
	int s;

	raidPtr->copyback_in_progress = 1;
	s = splbio();
	rf_CopybackReconstructedData(raidPtr);
	splx(s);
	raidPtr->copyback_in_progress = 0;

	/* That's all... */
	kthread_exit(0);        /* does not return */
}


void
rf_ReconstructInPlaceThread(req)
	struct rf_recon_req *req;
{
	int retcode;
	int s;
	RF_Raid_t *raidPtr;
	
	s = splbio();
	raidPtr = req->raidPtr;
	raidPtr->recon_in_progress = 1;
	retcode = rf_ReconstructInPlace(raidPtr, req->row, req->col);
	RF_Free(req, sizeof(*req));
	raidPtr->recon_in_progress = 0;
	splx(s);

	/* That's all... */
	kthread_exit(0);        /* does not return */
}

void
rf_mountroot_hook(dev)
	struct device *dev;
{

}


RF_AutoConfig_t *
rf_find_raid_components()
{
	struct devnametobdevmaj *dtobdm;
	struct vnode *vp;
	struct disklabel label;
	struct device *dv;
	char *cd_name;
	dev_t dev;
	int error;
	int i;
	int good_one;
	RF_ComponentLabel_t *clabel;
	RF_AutoConfig_t *ac_list;
	RF_AutoConfig_t *ac;


	/* initialize the AutoConfig list */
	ac_list = NULL;

if (raidautoconfig) {

	/* we begin by trolling through *all* the devices on the system */

	for (dv = alldevs.tqh_first; dv != NULL;
	     dv = dv->dv_list.tqe_next) {

		/* we are only interested in disks... */
		if (dv->dv_class != DV_DISK)
			continue;

		/* we don't care about floppies... */
		if (!strcmp(dv->dv_cfdata->cf_driver->cd_name,"fd")) {
			continue;
		}
		
		/* need to find the device_name_to_block_device_major stuff */
		cd_name = dv->dv_cfdata->cf_driver->cd_name;
		dtobdm = dev_name2blk;
		while (dtobdm->d_name && strcmp(dtobdm->d_name, cd_name)) {
			dtobdm++;
		}

		/* get a vnode for the raw partition of this disk */

		dev = MAKEDISKDEV(dtobdm->d_maj, dv->dv_unit, RAW_PART);
		if (bdevvp(dev, &vp))
			panic("RAID can't alloc vnode");

		error = VOP_OPEN(vp, FREAD, NOCRED, 0);

		if (error) {
			/* "Who cares."  Continue looking 
			   for something that exists*/
			vput(vp);
			continue;
		}

		/* Ok, the disk exists.  Go get the disklabel. */
		error = VOP_IOCTL(vp, DIOCGDINFO, (caddr_t)&label, 
				  FREAD, NOCRED, 0);
		if (error) {
			/*
			 * XXX can't happen - open() would
			 * have errored out (or faked up one)
			 */
			printf("can't get label for dev %s%c (%d)!?!?\n",
			       dv->dv_xname, 'a' + RAW_PART, error);
		}

		/* don't need this any more.  We'll allocate it again
		   a little later if we really do... */
		VOP_CLOSE(vp, FREAD, NOCRED, 0);
		vput(vp);

		for (i=0; i < label.d_npartitions; i++) {
			/* We only support partitions marked as RAID */
			if (label.d_partitions[i].p_fstype != FS_RAID)
				continue;

			dev = MAKEDISKDEV(dtobdm->d_maj, dv->dv_unit, i);
			if (bdevvp(dev, &vp))
				panic("RAID can't alloc vnode");

			error = VOP_OPEN(vp, FREAD, NOCRED, 0);
			if (error) {
				/* Whatever... */
				vput(vp);
				continue;
			}

			good_one = 0;

			clabel = (RF_ComponentLabel_t *) 
				malloc(sizeof(RF_ComponentLabel_t), 
				       M_RAIDFRAME, M_NOWAIT);
			if (clabel == NULL) {
				/* XXX CLEANUP HERE */
				printf("RAID auto config: out of memory!\n");
				return(NULL); /* XXX probably should panic? */
			}

			if (!raidread_component_label(dev, vp, clabel)) {
				/* Got the label.  Does it look reasonable? */
				if (rf_reasonable_label(clabel) &&
				    (clabel->partitionSize <= 
				     label.d_partitions[i].p_size)) {
#if DEBUG
					printf("Component on: %s%c: %d\n", 
					       dv->dv_xname, 'a'+i,
					       label.d_partitions[i].p_size);
					rf_print_component_label(clabel);
#endif
					/* if it's reasonable, add it, 
					   else ignore it. */
					ac = (RF_AutoConfig_t *)
						malloc(sizeof(RF_AutoConfig_t),
						       M_RAIDFRAME,
						       M_NOWAIT);
					if (ac == NULL) {
						/* XXX should panic?? */
						return(NULL);
					}
					
					sprintf(ac->devname, "%s%c",
						dv->dv_xname, 'a'+i);
					ac->dev = dev;
					ac->vp = vp;
					ac->clabel = clabel;
					ac->next = ac_list;
					ac_list = ac;
					good_one = 1;
				} 
			}
			if (!good_one) {
				/* cleanup */
				free(clabel, M_RAIDFRAME);
				VOP_CLOSE(vp, FREAD, NOCRED, 0);
				vput(vp);
			}
		}
	}
}
return(ac_list);
}
			
static int
rf_reasonable_label(clabel)
	RF_ComponentLabel_t *clabel;
{
	
	if (((clabel->version==RF_COMPONENT_LABEL_VERSION_1) ||
	     (clabel->version==RF_COMPONENT_LABEL_VERSION)) &&
	    ((clabel->clean == RF_RAID_CLEAN) ||
	     (clabel->clean == RF_RAID_DIRTY)) &&
	    clabel->row >=0 && 
	    clabel->column >= 0 && 
	    clabel->num_rows > 0 &&
	    clabel->num_columns > 0 &&
	    clabel->row < clabel->num_rows && 
	    clabel->column < clabel->num_columns &&
	    clabel->blockSize > 0 &&
	    clabel->numBlocks > 0) {
		/* label looks reasonable enough... */
		return(1);
	}
	return(0);
}


void
rf_print_component_label(clabel)
	RF_ComponentLabel_t *clabel;
{
	printf("   Row: %d Column: %d Num Rows: %d Num Columns: %d\n",
	       clabel->row, clabel->column, 
	       clabel->num_rows, clabel->num_columns);
	printf("   Version: %d Serial Number: %d Mod Counter: %d\n",
	       clabel->version, clabel->serial_number,
	       clabel->mod_counter);
	printf("   Clean: %s Status: %d\n",
	       clabel->clean ? "Yes" : "No", clabel->status );
	printf("   sectPerSU: %d SUsPerPU: %d SUsPerRU: %d\n",
	       clabel->sectPerSU, clabel->SUsPerPU, clabel->SUsPerRU);
	printf("   RAID Level: %c  blocksize: %d numBlocks: %d\n",
	       (char) clabel->parityConfig, clabel->blockSize, 
	       clabel->numBlocks);
	printf("   Autoconfig: %s\n", clabel->autoconfigure ? "Yes" : "No" );
	printf("   Contains root partition: %s\n",  
	       clabel->root_partition ? "Yes" : "No" );
	printf("   Last configured as: raid%d\n", clabel->last_unit );
#if 0
	   printf("   Config order: %d\n", clabel->config_order);
#endif
	       
}

RF_ConfigSet_t *
rf_create_auto_sets(ac_list)
	RF_AutoConfig_t *ac_list;
{
	RF_AutoConfig_t *ac;
	RF_ConfigSet_t *config_sets;
	RF_ConfigSet_t *cset;
	RF_AutoConfig_t *ac_next;


	config_sets = NULL;

	/* Go through the AutoConfig list, and figure out which components
	   belong to what sets.  */
	ac = ac_list;
	while(ac!=NULL) {
		/* we're going to putz with ac->next, so save it here
		   for use at the end of the loop */
		ac_next = ac->next;

		if (config_sets == NULL) {
			/* will need at least this one... */
			config_sets = (RF_ConfigSet_t *)
				malloc(sizeof(RF_ConfigSet_t), 
				       M_RAIDFRAME, M_NOWAIT);
			if (config_sets == NULL) {
				panic("rf_create_auto_sets: No memory!");
			}
			/* this one is easy :) */
			config_sets->ac = ac;
			config_sets->next = NULL;
			config_sets->rootable = 0;
			ac->next = NULL;
		} else {
			/* which set does this component fit into? */
			cset = config_sets;
			while(cset!=NULL) {
				if (rf_does_it_fit(cset, ac)) {
					/* looks like it matches... */
					ac->next = cset->ac;
					cset->ac = ac;
					break;
				}
				cset = cset->next;
			}
			if (cset==NULL) {
				/* didn't find a match above... new set..*/
				cset = (RF_ConfigSet_t *)
					malloc(sizeof(RF_ConfigSet_t), 
					       M_RAIDFRAME, M_NOWAIT);
				if (cset == NULL) {
					panic("rf_create_auto_sets: No memory!");
				}
				cset->ac = ac;
				ac->next = NULL;
				cset->next = config_sets;
				cset->rootable = 0;
				config_sets = cset;
			}
		}
		ac = ac_next;
	}


	return(config_sets);
}

static int
rf_does_it_fit(cset, ac)	
	RF_ConfigSet_t *cset;
	RF_AutoConfig_t *ac;
{
	RF_ComponentLabel_t *clabel1, *clabel2;

	/* If this one matches the *first* one in the set, that's good
	   enough, since the other members of the set would have been
	   through here too... */
	/* note that we are not checking partitionSize here..

	   Note that we are also not checking the mod_counters here.
	   If everything else matches execpt the mod_counter, that's 
	   good enough for this test.  We will deal with the mod_counters
	   a little later in the autoconfiguration process.  

	    (clabel1->mod_counter == clabel2->mod_counter) &&

	   The reason we don't check for this is that failed disks
	   will have lower modification counts.  If those disks are
	   not added to the set they used to belong to, then they will
	   form their own set, which may result in 2 different sets,
	   for example, competing to be configured at raid0, and
	   perhaps competing to be the root filesystem set.  If the
	   wrong ones get configured, or both attempt to become /,
	   weird behaviour and or serious lossage will occur.  Thus we
	   need to bring them into the fold here, and kick them out at
	   a later point.

	*/

	clabel1 = cset->ac->clabel;
	clabel2 = ac->clabel;
	if ((clabel1->version == clabel2->version) &&
	    (clabel1->serial_number == clabel2->serial_number) &&
	    (clabel1->num_rows == clabel2->num_rows) &&
	    (clabel1->num_columns == clabel2->num_columns) &&
	    (clabel1->sectPerSU == clabel2->sectPerSU) &&
	    (clabel1->SUsPerPU == clabel2->SUsPerPU) &&
	    (clabel1->SUsPerRU == clabel2->SUsPerRU) &&
	    (clabel1->parityConfig == clabel2->parityConfig) &&
	    (clabel1->maxOutstanding == clabel2->maxOutstanding) &&
	    (clabel1->blockSize == clabel2->blockSize) &&
	    (clabel1->numBlocks == clabel2->numBlocks) &&
	    (clabel1->autoconfigure == clabel2->autoconfigure) &&
	    (clabel1->root_partition == clabel2->root_partition) &&
	    (clabel1->last_unit == clabel2->last_unit) &&
	    (clabel1->config_order == clabel2->config_order)) {
		/* if it get's here, it almost *has* to be a match */
	} else {
		/* it's not consistent with somebody in the set.. 
		   punt */
		return(0);
	}
	/* all was fine.. it must fit... */
	return(1);
}

int
rf_have_enough_components(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *auto_config;
	RF_ComponentLabel_t *clabel;
	int r,c;
	int num_rows;
	int num_cols;
	int num_missing;
	int mod_counter;
	int mod_counter_found;
	int even_pair_failed;
	char parity_type;
	

	/* check to see that we have enough 'live' components
	   of this set.  If so, we can configure it if necessary */

	num_rows = cset->ac->clabel->num_rows;
	num_cols = cset->ac->clabel->num_columns;
	parity_type = cset->ac->clabel->parityConfig;

	/* XXX Check for duplicate components!?!?!? */

	/* Determine what the mod_counter is supposed to be for this set. */

	mod_counter_found = 0;
	ac = cset->ac;
	while(ac!=NULL) {
		if (mod_counter_found==0) {
			mod_counter = ac->clabel->mod_counter;
			mod_counter_found = 1;
		} else {
			if (ac->clabel->mod_counter > mod_counter) {
				mod_counter = ac->clabel->mod_counter;
			}
		}
		ac = ac->next;
	}

	num_missing = 0;
	auto_config = cset->ac;

	for(r=0; r<num_rows; r++) {
		even_pair_failed = 0;
		for(c=0; c<num_cols; c++) {
			ac = auto_config;
			while(ac!=NULL) {
				if ((ac->clabel->row == r) &&
				    (ac->clabel->column == c) && 
				    (ac->clabel->mod_counter == mod_counter)) {
					/* it's this one... */
#if DEBUG
					printf("Found: %s at %d,%d\n",
					       ac->devname,r,c);
#endif
					break;
				}
				ac=ac->next;
			}
			if (ac==NULL) {
				/* Didn't find one here! */
				/* special case for RAID 1, especially
				   where there are more than 2
				   components (where RAIDframe treats
				   things a little differently :( ) */
				if (parity_type == '1') {
					if (c%2 == 0) { /* even component */
						even_pair_failed = 1;
					} else { /* odd component.  If
                                                    we're failed, and
                                                    so is the even
                                                    component, it's
                                                    "Good Night, Charlie" */
						if (even_pair_failed == 1) {
							return(0);
						}
					}
				} else {
					/* normal accounting */
					num_missing++;
				}
			}
			if ((parity_type == '1') && (c%2 == 1)) {
				/* Just did an even component, and we didn't
				   bail.. reset the even_pair_failed flag, 
				   and go on to the next component.... */
				even_pair_failed = 0;
			}
		}
	}

	clabel = cset->ac->clabel;

	if (((clabel->parityConfig == '0') && (num_missing > 0)) ||
	    ((clabel->parityConfig == '4') && (num_missing > 1)) ||
	    ((clabel->parityConfig == '5') && (num_missing > 1))) {
		/* XXX this needs to be made *much* more general */
		/* Too many failures */
		return(0);
	}
	/* otherwise, all is well, and we've got enough to take a kick
	   at autoconfiguring this set */
	return(1);
}

void
rf_create_configuration(ac,config,raidPtr)
	RF_AutoConfig_t *ac;
	RF_Config_t *config;
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t *clabel;
	int i;

	clabel = ac->clabel;

	/* 1. Fill in the common stuff */
	config->numRow = clabel->num_rows;
	config->numCol = clabel->num_columns;
	config->numSpare = 0; /* XXX should this be set here? */
	config->sectPerSU = clabel->sectPerSU;
	config->SUsPerPU = clabel->SUsPerPU;
	config->SUsPerRU = clabel->SUsPerRU;
	config->parityConfig = clabel->parityConfig;
	/* XXX... */
	strcpy(config->diskQueueType,"fifo");
	config->maxOutstandingDiskReqs = clabel->maxOutstanding;
	config->layoutSpecificSize = 0; /* XXX ?? */

	while(ac!=NULL) {
		/* row/col values will be in range due to the checks
		   in reasonable_label() */
		strcpy(config->devnames[ac->clabel->row][ac->clabel->column],
		       ac->devname);
		ac = ac->next;
	}

	for(i=0;i<RF_MAXDBGV;i++) {
		config->debugVars[i][0] = NULL;
	}
}

int
rf_set_autoconfig(raidPtr, new_value)
	RF_Raid_t *raidPtr;
	int new_value;
{
	RF_ComponentLabel_t clabel;
	struct vnode *vp;
	dev_t dev;
	int row, column;

	raidPtr->autoconfigure = new_value;
	for(row=0; row<raidPtr->numRow; row++) {
		for(column=0; column<raidPtr->numCol; column++) {
			if (raidPtr->Disks[row][column].status == 
			    rf_ds_optimal) {
				dev = raidPtr->Disks[row][column].dev;
				vp = raidPtr->raid_cinfo[row][column].ci_vp;
				raidread_component_label(dev, vp, &clabel);
				clabel.autoconfigure = new_value;
				raidwrite_component_label(dev, vp, &clabel);
			}
		}
	}
	return(new_value);
}

int
rf_set_rootpartition(raidPtr, new_value)
	RF_Raid_t *raidPtr;
	int new_value;
{
	RF_ComponentLabel_t clabel;
	struct vnode *vp;
	dev_t dev;
	int row, column;

	raidPtr->root_partition = new_value;
	for(row=0; row<raidPtr->numRow; row++) {
		for(column=0; column<raidPtr->numCol; column++) {
			if (raidPtr->Disks[row][column].status == 
			    rf_ds_optimal) {
				dev = raidPtr->Disks[row][column].dev;
				vp = raidPtr->raid_cinfo[row][column].ci_vp;
				raidread_component_label(dev, vp, &clabel);
				clabel.root_partition = new_value;
				raidwrite_component_label(dev, vp, &clabel);
			}
		}
	}
	return(new_value);
}

void
rf_release_all_vps(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	
	ac = cset->ac;
	while(ac!=NULL) {
		/* Close the vp, and give it back */
		if (ac->vp) {
			VOP_CLOSE(ac->vp, FREAD, NOCRED, 0);
			vput(ac->vp);
			ac->vp = NULL;
		}
		ac = ac->next;
	}
}


void
rf_cleanup_config_set(cset)
	RF_ConfigSet_t *cset;
{
	RF_AutoConfig_t *ac;
	RF_AutoConfig_t *next_ac;
	
	ac = cset->ac;
	while(ac!=NULL) {
		next_ac = ac->next;
		/* nuke the label */
		free(ac->clabel, M_RAIDFRAME);
		/* cleanup the config structure */
		free(ac, M_RAIDFRAME);
		/* "next.." */
		ac = next_ac;
	}
	/* and, finally, nuke the config set */
	free(cset, M_RAIDFRAME);
}


void
raid_init_component_label(raidPtr, clabel)
	RF_Raid_t *raidPtr;
	RF_ComponentLabel_t *clabel;
{
	/* current version number */
	clabel->version = RF_COMPONENT_LABEL_VERSION; 
	clabel->serial_number = raidPtr->serial_number;
	clabel->mod_counter = raidPtr->mod_counter;
	clabel->num_rows = raidPtr->numRow;
	clabel->num_columns = raidPtr->numCol;
	clabel->clean = RF_RAID_DIRTY; /* not clean */
	clabel->status = rf_ds_optimal; /* "It's good!" */
	
	clabel->sectPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	clabel->SUsPerPU = raidPtr->Layout.SUsPerPU;
	clabel->SUsPerRU = raidPtr->Layout.SUsPerRU;

	clabel->blockSize = raidPtr->bytesPerSector;
	clabel->numBlocks = raidPtr->sectorsPerDisk;

	/* XXX not portable */
	clabel->parityConfig = raidPtr->Layout.map->parityConfig;
	clabel->maxOutstanding = raidPtr->maxOutstanding;
	clabel->autoconfigure = raidPtr->autoconfigure;
	clabel->root_partition = raidPtr->root_partition;
	clabel->last_unit = raidPtr->raidid;
	clabel->config_order = raidPtr->config_order;
}

int
rf_auto_config_set(cset,unit)
	RF_ConfigSet_t *cset;
	int *unit;
{
	RF_Raid_t *raidPtr;
	RF_Config_t *config;
	int raidID;
	int retcode;

	printf("RAID autoconfigure\n");

	retcode = 0;
	*unit = -1;

	/* 1. Create a config structure */

	config = (RF_Config_t *)malloc(sizeof(RF_Config_t),
				       M_RAIDFRAME,
				       M_NOWAIT);
	if (config==NULL) {
		printf("Out of mem!?!?\n");
				/* XXX do something more intelligent here. */
		return(1);
	}

	memset(config, 0, sizeof(RF_Config_t));

	/* XXX raidID needs to be set correctly.. */

	/* 
	   2. Figure out what RAID ID this one is supposed to live at 
	   See if we can get the same RAID dev that it was configured
	   on last time.. 
	*/

	raidID = cset->ac->clabel->last_unit;
	if ((raidID < 0) || (raidID >= numraid)) {
		/* let's not wander off into lala land. */
		raidID = numraid - 1;
	}
	if (raidPtrs[raidID]->valid != 0) {

		/* 
		   Nope... Go looking for an alternative...  
		   Start high so we don't immediately use raid0 if that's
		   not taken. 
		*/

		for(raidID = numraid; raidID >= 0; raidID--) {
			if (raidPtrs[raidID]->valid == 0) {
				/* can use this one! */
				break;
			}
		}
	}

	if (raidID < 0) {
		/* punt... */
		printf("Unable to auto configure this set!\n");
		printf("(Out of RAID devs!)\n");
		return(1);
	}
	printf("Configuring raid%d:\n",raidID);
	raidPtr = raidPtrs[raidID];

	/* XXX all this stuff should be done SOMEWHERE ELSE! */
	raidPtr->raidid = raidID;
	raidPtr->openings = RAIDOUTSTANDING;

	/* 3. Build the configuration structure */
	rf_create_configuration(cset->ac, config, raidPtr);

	/* 4. Do the configuration */
	retcode = rf_Configure(raidPtr, config, cset->ac);
	
	if (retcode == 0) {

		raidinit(raidPtrs[raidID]);

		rf_markalldirty(raidPtrs[raidID]);
		raidPtrs[raidID]->autoconfigure = 1; /* XXX do this here? */
		if (cset->ac->clabel->root_partition==1) {
			/* everything configured just fine.  Make a note
			   that this set is eligible to be root. */
			cset->rootable = 1;
			/* XXX do this here? */
			raidPtrs[raidID]->root_partition = 1; 
		}
	}

	/* 5. Cleanup */
	free(config, M_RAIDFRAME);
	
	*unit = raidID;
	return(retcode);
}
