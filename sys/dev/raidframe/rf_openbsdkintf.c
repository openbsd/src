/*	$OpenBSD: rf_openbsdkintf.c,v 1.9 2000/01/11 18:02:22 peter Exp $	*/
/*	$NetBSD: rf_netbsdkintf.c,v 1.46 2000/01/09 03:39:13 oster Exp $	*/
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
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
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
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
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
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/disk.h>
#include <sys/device.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/buf.h>
#include <sys/user.h>

#include "raid.h"
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

int	rf_kdebug_level = 0;

#ifdef RAIDDEBUG
#define db1_printf(a) do if (rf_kdebug_level > 0) printf a; while(0)
#else	/* RAIDDEBUG */
#define db1_printf(a) (void)0
#endif	/* RAIDDEBUG */

static RF_Raid_t **raidPtrs;	/* global raid device descriptors */

RF_DECLARE_STATIC_MUTEX(rf_sparet_wait_mutex)

/* requests to install a spare table */
static RF_SparetWait_t *rf_sparet_wait_queue;

/* responses from installation process */
static RF_SparetWait_t *rf_sparet_resp_queue;

/* prototypes */
void	rf_KernelWakeupFunc __P((struct buf *));
void	rf_InitBP __P((struct buf *, struct vnode *, unsigned, dev_t,
	    RF_SectorNum_t, RF_SectorCount_t, caddr_t, void (*)(struct buf *),
	    void *, int, struct proc *));
static int raidinit __P((dev_t, RF_Raid_t *, int));

void	raidattach __P((int));
int	raidsize __P((dev_t));
int	raidopen __P((dev_t, int, int, struct proc *));
int	raidclose __P((dev_t, int, int, struct proc *));
int	raidioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	raidwrite __P((dev_t, struct uio *, int));
int	raidread __P((dev_t, struct uio *, int));
void	raidstrategy __P((struct buf *));
int	raiddump __P((dev_t, daddr_t, caddr_t, size_t));

/*
 * Pilfered from ccd.c
 */
struct raidbuf {
	struct buf rf_buf;	/* new I/O buf.	 MUST BE FIRST!!! */
	struct buf *rf_obp;	/* ptr. to original I/O buf */
	int	rf_flags;	/* misc. flags */
	RF_DiskQueueData_t *req;/* the request that this was part of.. */
};

#define RAIDGETBUF() malloc(sizeof (struct raidbuf), M_RAIDFRAME, M_NOWAIT)
#define	RAIDPUTBUF(buf) free(buf, M_RAIDFRAME)

/*
 * XXX Not sure if the following should be replacing the raidPtrs above,
 * or if it should be used in conjunction with that...
 */
struct raid_softc {
	int	 sc_flags;		/* flags */
	int	 sc_cflags;		/* configuration flags */
	size_t	 sc_size;		/* size of the raid device */
	dev_t	 sc_dev;		/* our device..*/
	char	 sc_xname[20];		/* XXX external name */
	struct disk sc_dkdev;		/* generic disk device info */
	struct buf buf_queue;		/* used for the device queue */
};

/* sc_flags */
#define RAIDF_INITED	0x01	/* unit has been initialized */
#define RAIDF_WLABEL	0x02	/* label area is writable */
#define RAIDF_LABELLING	0x04	/* unit is currently being labelled */
#define RAIDF_WANTED	0x40	/* someone is waiting to obtain a lock */
#define RAIDF_LOCKED	0x80	/* unit is locked */

#define	raidunit(x)	DISKUNIT(x)
static int numraid = 0;

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

void	raidgetdefaultlabel
	     __P((RF_Raid_t *, struct raid_softc *, struct disklabel *));
void	raidgetdisklabel __P((dev_t));
void	raidmakedisklabel __P((struct raid_softc *));

int	raidlock __P((struct raid_softc *));
void	raidunlock __P((struct raid_softc *));

void	rf_markalldirty __P((RF_Raid_t *));

void rf_ReconThread __P((struct rf_recon_req *));
/* XXX what I want is: */
/*void rf_ReconThread __P((RF_Raid_t *raidPtr));  */
void rf_RewriteParityThread __P((RF_Raid_t *raidPtr));
void rf_CopybackThread __P((RF_Raid_t *raidPtr));
void rf_ReconstructInPlaceThread __P((struct rf_recon_req *));

void
raidattach(num)
	int num;
{
	int raidID;
	int i, rc;

	db1_printf(("raidattach: Asked for %d units\n", num));

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("raidattach: count <= 0");
#endif
		return;
	}

	/* This is where all the initialization stuff gets done. */

	/* Make some space for requested number of units... */
	RF_Calloc(raidPtrs, num, sizeof(RF_Raid_t *), (RF_Raid_t **));
	if (raidPtrs == NULL) {
		panic("raidPtrs is NULL!!\n");
	}

	rc = rf_mutex_init(&rf_sparet_wait_mutex);
	if (rc) {
	        RF_PANIC();
        }

	rf_sparet_wait_queue = rf_sparet_resp_queue = NULL;

	for (i = 0; i < numraid; i++)
	        raidPtrs[i] = NULL;
	rc = rf_BootRaidframe();
	if (rc == 0)
	        printf("Kernelized RAIDframe activated\n");
	else
	        panic("Serious error booting RAID!!\n");
	
	/*
	 * Put together some datastructures like the CCD device does..
	 * This lets us lock the device and what-not when it gets opened.
	 */
	
	raid_softc = (struct raid_softc *)
	    malloc(num * sizeof (struct raid_softc), M_RAIDFRAME, M_NOWAIT);
	if (raid_softc == NULL) {
		printf("WARNING: no memory for RAIDframe driver\n");
		return;
	}
	numraid = num;
	bzero(raid_softc, num * sizeof (struct raid_softc));

	for (raidID = 0; raidID < num; raidID++) {
		raid_softc[raidID].buf_queue.b_actf = NULL;
		raid_softc[raidID].buf_queue.b_actb = 
			&raid_softc[raidID].buf_queue.b_actf;
		RF_Calloc(raidPtrs[raidID], 1, sizeof (RF_Raid_t),
		    (RF_Raid_t *));
		if (raidPtrs[raidID] == NULL) {
			printf("WARNING: raidPtrs[%d] is NULL\n", raidID);
			numraid = raidID;
			return;
		}
	}
}

int
raidsize(dev)
	dev_t dev;
{
	struct raid_softc *rs;
	struct disklabel *lp;
	int part, unit, omask, size;

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
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	/* Not implemented. */
	return (ENXIO);
}

/* ARGSUSED */
int
raidopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = raidunit(dev);
	struct raid_softc *rs;
	struct disklabel *lp;
	int part,pmask;
	int error = 0;
	
	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((error = raidlock(rs)) != 0)
		return (error);
	lp = rs->sc_dkdev.dk_label;

	part = DISKPART(dev);
	pmask = (1 << part);

	db1_printf(
	    ("Opening raid device number: %d partition: %d\n", unit, part));


	if ((rs->sc_flags & RAIDF_INITED) && (rs->sc_dkdev.dk_openmask == 0))
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
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = raidunit(dev);
	struct raid_softc *rs;
	int error = 0;
	int part;

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
	        rf_update_component_labels( raidPtrs[unit] );
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
	struct buf *dp;
	int wlabel;

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
		if (bounds_check_with_label(bp, lp, rs->sc_dkdev.dk_cpulabel,
		    wlabel) <= 0) {
			db1_printf(("Bounds check failed!!:%d %d\n",
			    (int)bp->b_blkno, (int)wlabel));
			biodone(bp);
			return;
		}

	s = splbio();

	bp->b_resid = 0;

	/* stuff it onto our queue */

	dp = &rs->buf_queue;
	bp->b_actf = NULL;
	bp->b_actb = dp->b_actb;
	*dp->b_actb = bp;
	dp->b_actb = &bp->b_actf;
	
	raidstart(raidPtrs[raidID]);

	splx(s);
}

/* ARGSUSED */
int
raidread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = raidunit(dev);
	struct raid_softc *rs;
	int part;

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
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = raidunit(dev);
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
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = raidunit(dev);
	int error = 0;
	int part, pmask;
	struct raid_softc *rs;
	RF_Config_t *k_cfg, *u_cfg;
	RF_Raid_t *raidPtr;
	RF_AccTotals_t *totals;
	RF_DeviceConfig_t *d_cfg, **ucfgp;
	u_char *specific_buf;
	int retcode = 0;
	int row;
	int column;
	struct rf_recon_req *rrcopy, *rr;
	RF_ComponentLabel_t *component_label;
	RF_ComponentLabel_t ci_label;
	RF_ComponentLabel_t **c_label_ptr;
	RF_SingleComponent_t *sparePtr,*componentPtr;
	RF_SingleComponent_t hot_spare;
	RF_SingleComponent_t component;
	int i, j, d;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];
	raidPtr = raidPtrs[unit];

	db1_printf(("raidioctl: %d %d %d %d\n", (int)dev, (int)DISKPART(dev),
	    (int)unit, (int)cmd));

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
	case RAIDFRAME_GET_COMPONENT_LABEL:
	case RAIDFRAME_SET_COMPONENT_LABEL:
	case RAIDFRAME_ADD_HOT_SPARE:
	case RAIDFRAME_REMOVE_HOT_SPARE:
	case RAIDFRAME_INIT_LABELS:
	case RAIDFRAME_REBUILD_IN_PLACE:
	case RAIDFRAME_CHECK_PARITY:
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
	case RAIDFRAME_CHECK_COPYBACK_STATUS:
		if ((rs->sc_flags & RAIDF_INITED) == 0)
			return (ENXIO);
	}
	
	switch (cmd) {
		/* Configure the system */
	case RAIDFRAME_CONFIGURE:
		/*
		 * Copy-in the configuration information
		 * data points to a pointer to the configuration structure.
		 */
		u_cfg = *((RF_Config_t **)data);
		RF_Malloc(k_cfg, sizeof (RF_Config_t), (RF_Config_t *));
		if (k_cfg == NULL) {
			return (ENOMEM);
		}
		retcode = copyin((caddr_t)u_cfg, (caddr_t)k_cfg,
		    sizeof (RF_Config_t));
		if (retcode) {
			RF_Free(k_cfg, sizeof(RF_Config_t));
			return (retcode);
		}

		/*
		 * Allocate a buffer for the layout-specific data,
		 * and copy it in.
		 */
		if (k_cfg->layoutSpecificSize) {
			if (k_cfg->layoutSpecificSize > 10000) {
				/* sanity check */
				RF_Free(k_cfg, sizeof(RF_Config_t));
				return (EINVAL);
			}
			RF_Malloc(specific_buf, k_cfg->layoutSpecificSize,
			    (u_char *));
			if (specific_buf == NULL) {
				RF_Free(k_cfg, sizeof (RF_Config_t));
				return (ENOMEM);
			}
			retcode = copyin(k_cfg->layoutSpecific,
			    (caddr_t)specific_buf, k_cfg->layoutSpecificSize);
			if (retcode) {
				RF_Free(k_cfg, sizeof(RF_Config_t));
				RF_Free(specific_buf, 
					k_cfg->layoutSpecificSize);
				return (retcode);
			}
		} else
			specific_buf = NULL;
		k_cfg->layoutSpecific = specific_buf;
		
		/*
		 * We should do some kind of sanity check on the
		 * configuration.
		 * Store the sum of all the bytes in the last byte?
		 */

		/* configure the system */
		raidPtr->raidid = unit;

		retcode = rf_Configure(raidPtr, k_cfg);

		if (retcode == 0) {

			/* allow this many simultaneous IO's to
			   this RAID device */
			raidPtr->openings = RAIDOUTSTANDING;

			/* XXX should be moved to rf_Configure() */

			raidPtr->copyback_in_progress = 0;
			raidPtr->parity_rewrite_in_progress = 0;
			raidPtr->recon_in_progress = 0;
		
			retcode = raidinit(dev, raidPtr, unit);
			rf_markalldirty( raidPtr );
		}

		/* Free the buffers.  No return code here. */
		if (k_cfg->layoutSpecificSize) {
			RF_Free(specific_buf, k_cfg->layoutSpecificSize);
		}
		RF_Free(k_cfg, sizeof (RF_Config_t));
		
		return (retcode);
		
	case RAIDFRAME_SHUTDOWN:
		/* Shutdown the system */
		
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

		/* It's no longer initialized... */
		rs->sc_flags &= ~RAIDF_INITED;

		/* Detach the disk. */
		disk_detach(&rs->sc_dkdev);

		raidunlock(rs);

		return (retcode);
		
	case RAIDFRAME_GET_COMPONENT_LABEL:
		c_label_ptr = (RF_ComponentLabel_t **) data;
		/* need to read the component label for the disk indicated
		   by row,column in component_label */

		/* For practice, let's get it directly fromdisk, rather 
		   than from the in-core copy */
		RF_Malloc( component_label, sizeof( RF_ComponentLabel_t ),
			   (RF_ComponentLabel_t *));
		if (component_label == NULL)
			return (ENOMEM);

		bzero((char *) component_label, sizeof(RF_ComponentLabel_t));

		retcode = copyin( *c_label_ptr, component_label, 
				  sizeof(RF_ComponentLabel_t));

		if (retcode) {
			RF_Free( component_label, sizeof(RF_ComponentLabel_t));
			return(retcode);
		}
 
 		row = component_label->row;
		column = component_label->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			return(EINVAL);
  		}

		raidread_component_label( 
                              raidPtr->Disks[row][column].dev, 
			      raidPtr->raid_cinfo[row][column].ci_vp, 
			      component_label );

		retcode = copyout((caddr_t) component_label, 
				  (caddr_t) *c_label_ptr,
				  sizeof(RF_ComponentLabel_t));
		RF_Free( component_label, sizeof(RF_ComponentLabel_t));
		return (retcode);

	case RAIDFRAME_SET_COMPONENT_LABEL:
		component_label = (RF_ComponentLabel_t *) data;

		/* XXX check the label for valid stuff... */
		/* Note that some things *should not* get modified --
		   the user should be re-initing the labels instead of 
		   trying to patch things.
		   */

		printf("Got component label:\n");
		printf("Version: %d\n",component_label->version);
		printf("Serial Number: %d\n",component_label->serial_number);
		printf("Mod counter: %d\n",component_label->mod_counter);
		printf("Row: %d\n", component_label->row);
		printf("Column: %d\n", component_label->column);
		printf("Num Rows: %d\n", component_label->num_rows);
		printf("Num Columns: %d\n", component_label->num_columns);
		printf("Clean: %d\n", component_label->clean);
		printf("Status: %d\n", component_label->status);

		row = component_label->row;
		column = component_label->column;

		if ((row < 0) || (row >= raidPtr->numRow) ||
		    (column < 0) || (column >= raidPtr->numCol)) {
			RF_Free( component_label, sizeof(RF_ComponentLabel_t));
			return(EINVAL);
		}

 		/* XXX this isn't allowed to do anything for now :-) */
#if 0
		raidwrite_component_label( 
                            raidPtr->Disks[row][column].dev, 
			    raidPtr->raid_cinfo[row][column].ci_vp, 
			    component_label );
#endif
		return (0);

	case RAIDFRAME_INIT_LABELS:	
		component_label = (RF_ComponentLabel_t *) data;
		/* 
		   we only want the serial number from
		   the above.  We get all the rest of the information
		   from the config that was used to create this RAID
		   set. 
		   */

		raidPtr->serial_number = component_label->serial_number;
		/* current version number */
		ci_label.version = RF_COMPONENT_LABEL_VERSION; 
		ci_label.serial_number = component_label->serial_number;
		ci_label.mod_counter = raidPtr->mod_counter;
		ci_label.num_rows = raidPtr->numRow;
		ci_label.num_columns = raidPtr->numCol;
		ci_label.clean = RF_RAID_DIRTY; /* not clean */
		ci_label.status = rf_ds_optimal; /* "It's good!" */

		for(row=0;row<raidPtr->numRow;row++) {
			ci_label.row = row;
			for(column=0;column<raidPtr->numCol;column++) {
				ci_label.column = column;
				raidwrite_component_label( 
				  raidPtr->Disks[row][column].dev, 
				  raidPtr->raid_cinfo[row][column].ci_vp, 
				  &ci_label );
			}
		}

		return (retcode);
  
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
		printf("Adding spare\n");
		retcode = rf_add_hot_spare(raidPtr, &hot_spare);
		return(retcode);

	case RAIDFRAME_REMOVE_HOT_SPARE:
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
		rr = (struct rf_recon_req *)data;
		
		if (rr->row < 0 || rr->row >= raidPtr->numRow ||
		    rr->col < 0 || rr->col >= raidPtr->numCol)
			return (EINVAL);

		printf("raid%d: Failing the disk: row: %d col: %d\n",
		       unit, rr->row, rr->col);
		
		/*
		 * Make a copy of the recon request so that we don't
		 * rely on the user's buffer
		 */
		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		if (rrcopy == NULL)
			return(ENOMEM);
		bcopy(rr, rrcopy, sizeof(*rr));
		rrcopy->raidPtr = (void *)raidPtr;

		retcode = RF_CREATE_THREAD(raidPtr->recon_thread,
					   rf_ReconThread,
					   rrcopy,"raid_recon");
		return (0);
		
		/*
		 * Invoke a copyback operation after recon on whatever
		 * disk needs it, if any.
		 */
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
		
		/* Return the percentage completion of reconstruction */
	case RAIDFRAME_CHECK_RECON_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			return(EINVAL);
		}
		row = 0; /* XXX we only consider a single row... */
		if (raidPtr->status[row] != rf_rs_reconstructing)
			*(int *)data = 100;
		else
			*(int *)data =
			    raidPtr->reconControl[row]->percentComplete;
		return (0);
	case RAIDFRAME_CHECK_PARITYREWRITE_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			return(EINVAL);
		}
		if (raidPtr->parity_rewrite_in_progress == 1) {
			*(int *) data = 100 * raidPtr->parity_rewrite_stripes_done / raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

	case RAIDFRAME_CHECK_COPYBACK_STATUS:
		if (raidPtr->Layout.map->faultsTolerated == 0) {
			/* This makes no sense on a RAID 0 */
			return(EINVAL);
		}
		if (raidPtr->copyback_in_progress == 1) {
			*(int *) data = 100 * raidPtr->copyback_stripes_done / raidPtr->Layout.numStripe;
		} else {
			*(int *) data = 100;
		}
		return (0);

#if 0
	case RAIDFRAME_SPARET_WAIT:
		/*
		 * The sparetable daemon calls this to wait for the
		 * kernel to need a spare table.
		 * This ioctl does not return until a spare table is needed.
		 * XXX -- Calling mpsleep here in the ioctl code is almost
		 * certainly wrong and evil. -- XXX
		 * XXX -- I should either compute the spare table in the
		 * kernel, or have a different. -- XXX
		 * XXX -- Interface (a different character device) for
		 * delivering the table. -- XXX
		 */
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		while (!rf_sparet_wait_queue)
			mpsleep(&rf_sparet_wait_queue, (PZERO + 1) | PCATCH,
			    "sparet wait", 0,
			    (void *)simple_lock_addr(rf_sparet_wait_mutex),
			    MS_LOCK_SIMPLE);
		waitreq = rf_sparet_wait_queue;
		rf_sparet_wait_queue = rf_sparet_wait_queue->next;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		
		*((RF_SparetWait_t *)data) = *waitreq;
		
		RF_Free(waitreq, sizeof *waitreq);
		return (0);
		
	case RAIDFRAME_ABORT_SPARET_WAIT:
		/*
		 * Wakes up a process waiting on SPARET_WAIT and puts an
		 * error code in it that will cause the dameon to exit.
		 */
		RF_Malloc(waitreq, sizeof (*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = -1;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_wait_queue;
		rf_sparet_wait_queue = waitreq;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		wakeup(&rf_sparet_wait_queue);
		return (0);

	case RAIDFRAME_SEND_SPARET:
		/*
		 * Used by the spare table daemon to deliver a spare table
		 * into the kernel
		 */
		
		/* Install the spare table */
		retcode = rf_SetSpareTable(raidPtr,*(void **)data);
		
		/*
		 * Respond to the requestor.  the return status of the
		 * spare table installation is passed in the "fcol" field
		 */
		RF_Malloc(waitreq, sizeof *waitreq, (RF_SparetWait_t *));
		waitreq->fcol = retcode;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_resp_queue;
		rf_sparet_resp_queue = waitreq;
		wakeup(&rf_sparet_resp_queue);
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		
		return (retcode);
#endif
	default:
		/* fall through to the os-specific code below */
		break;
	}
	
	if (!raidPtr->valid)
		return (EINVAL);
	
	/*
	 * Add support for "regular" device ioctls here.
	 */
	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)data = *(rs->sc_dkdev.dk_label);
		break;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = rs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &rs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((error = raidlock(rs)) != 0)
			return (error);

		rs->sc_flags |= RAIDF_LABELLING;

		error = setdisklabel(rs->sc_dkdev.dk_label,
		    (struct disklabel *)data, 0, rs->sc_dkdev.dk_cpulabel);
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
		if (*(int *)data != 0)
			rs->sc_flags |= RAIDF_WLABEL;
		else
			rs->sc_flags &= ~RAIDF_WLABEL;
		break;

#if 0
  	case DIOCGDEFLABEL:
  		raidgetdefaultlabel(raidPtr, rs,
  		    (struct disklabel *) data);
  		break;
#endif
  

	default:
		retcode = ENOTTY;
	}
	return (retcode);
}

/*
 * raidinit -- complete the rest of the initialization for the
 * RAIDframe device.
 */
int
raidinit(dev, raidPtr, unit)
	dev_t dev;
	RF_Raid_t *raidPtr;
	int unit;
{
	int retcode;
	struct raid_softc *rs;

	retcode = 0;

	rs = &raid_softc[unit];
	
	/* XXX should check return code first... */
	rs->sc_flags |= RAIDF_INITED;

	/* XXX doesn't check bounds.*/
	sprintf(rs->sc_xname, "raid%d", unit);

	rs->sc_dkdev.dk_name = rs->sc_xname;	

	/*
	 * disk_attach actually creates space for the CPU disklabel, among
	 * other things, so it's critical to call this *BEFORE* we
	 * try putzing with disklabels.
	 */
	disk_attach(&rs->sc_dkdev);

	/*
	 * XXX There may be a weird interaction here between this, and
	 * protectedSectors, as used in RAIDframe.
	 */
	rs->sc_size = raidPtr->totalSectors;
	rs->sc_dev = dev;
	return (retcode);
}

/*
 * Wake up the daemon & tell it to get us a spare table
 * XXX
 * The entries in the queues should be tagged with the raidPtr so that in the
 * extremely rare case that two recons happen at once, we know for
 * which device were requesting a spare table.
 * XXX
 * 
 * XXX This code is not currently used. GO
 */
int
rf_GetSpareTableFromDaemon(req)
	RF_SparetWait_t	 *req;
{
	int retcode;

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
	/* this is not the same req as we alloc'd */
	RF_Free(req, sizeof *req);
	return (retcode);
}

/*
 * A wrapper around rf_DoAccess that extracts appropriate info from the
 * bp & passes it down.
 * Any calls originating in the kernel must use non-blocking I/O
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
	int retcode;
	struct partition *pp;
	daddr_t blocknum;	
	int unit;
	struct raid_softc *rs;
	int	do_async;
	struct buf *bp;
	struct buf *dp;

	unit = raidPtr->raidid;
	rs = &raid_softc[unit];

	
	/* Check to see if we're at the limit... */
	RF_LOCK_MUTEX(raidPtr->mutex);
	while (raidPtr->openings > 0) {
		RF_UNLOCK_MUTEX(raidPtr->mutex);
  
		/* get the next item, if any, from the queue */
		dp = &rs->buf_queue;
		bp = dp->b_actf;
		if (bp == NULL) {
			/* nothing more to do */
			return;
		}
  
		/* update structures */
		dp = bp->b_actf;
		if (dp != NULL) {
			dp->b_actb = bp->b_actb;
		} else {
			rs->buf_queue.b_actb = bp->b_actb;
		}
		*bp->b_actb = dp;
  
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
				      bp->b_un.b_addr, bp, NULL, NULL, 
				      RF_DAG_NONBLOCKING_IO, NULL, NULL, NULL);


		RF_LOCK_MUTEX(raidPtr->mutex);
	}
	RF_UNLOCK_MUTEX(raidPtr->mutex);
}

/* Invoke an I/O from kernel mode.  Disk queue should be locked upon entry */

int
rf_DispatchKernelIO(queue, req)
	RF_DiskQueue_t *queue;
	RF_DiskQueueData_t *req;
{
	int op = (req->type == RF_IO_TYPE_READ) ? B_READ : B_WRITE;
	struct buf *bp;
	struct raidbuf *raidbp = NULL;
	struct raid_softc *rs;
	int unit;
	int s;
  
	s=0;
	/* s = splbio();*/ /* want to test this */
	
	/*
	 * XXX along with the vnode, we also need the softc associated with
	 * this device..
	 */
	req->queue = queue;
	
	unit = queue->raidPtr->raidid;

	db1_printf(("DispatchKernelIO unit: %d\n", unit));

	if (unit >= numraid) {
		printf("Invalid unit number: %d %d\n", unit, numraid);
		panic("Invalid Unit number in rf_DispatchKernelIO\n");
	}

	rs = &raid_softc[unit];

	/* XXX is this the right place? */
	disk_busy(&rs->sc_dkdev);

	bp = req->bp;

#if 1
	/*
	 * XXX When there is a physical disk failure, someone is passing
	 * us a buffer that contains old stuff!!  Attempt to deal with
	 * this problem without taking a performance hit...
	 * (not sure where the real bug is.  It's buried in RAIDframe
	 * somewhere) :-(  GO )
	 */
	if (bp->b_flags & B_ERROR) {
		bp->b_flags &= ~B_ERROR;
	}
	if (bp->b_error!=0) {
		bp->b_error = 0;
	}
#endif

	raidbp = RAIDGETBUF();

	raidbp->rf_flags = 0; /* XXX not really used anywhere... */

	/*
	 * context for raidiodone
	 */
	raidbp->rf_obp = bp;
	raidbp->req = req;

	LIST_INIT(&raidbp->rf_buf.b_dep);

	switch (req->type) {
	case RF_IO_TYPE_NOP:
		/* Used primarily to unlock a locked queue. */

		db1_printf(("rf_DispatchKernelIO: NOP to r %d c %d\n",
		    queue->row, queue->col));

		/* XXX need to do something extra here.. */

		/*
		 * I'm leaving this in, as I've never actually seen it
		 * used, and I'd like folks to report it... GO
		 */
		printf(("WAKEUP CALLED\n"));
		queue->numOutstanding++;

		/* XXX need to glue the original buffer into this??  */

		rf_KernelWakeupFunc(&raidbp->rf_buf);
		break;
		
	case RF_IO_TYPE_READ:
	case RF_IO_TYPE_WRITE:
		if (req->tracerec) {
			RF_ETIMER_START(req->tracerec->timer);
		}

		rf_InitBP(&raidbp->rf_buf, queue->rf_cinfo->ci_vp,
		    op | bp->b_flags, queue->rf_cinfo->ci_dev,
		    req->sectorOffset, req->numSector,
		    req->buf, rf_KernelWakeupFunc, (void *)req,	
		    queue->raidPtr->logBytesPerSector, req->b_proc);

		if (rf_debugKernelAccess) {
			db1_printf(("dispatch: bp->b_blkno = %ld\n",
			    (long)bp->b_blkno));
		}
		queue->numOutstanding++;
		queue->last_deq_sector = req->sectorOffset;

		/*
		 * Acc wouldn't have been let in if there were any
		 * pending reqs at any other priority.
		 */
		queue->curPriority = req->priority;

		db1_printf(("Going for %c to unit %d row %d col %d\n",
		    req->type, unit, queue->row, queue->col));
		db1_printf(("sector %d count %d (%d bytes) %d\n",
		    (int)req->sectorOffset, (int)req->numSector,
		    (int)(req->numSector << queue->raidPtr->logBytesPerSector),
		    (int)queue->raidPtr->logBytesPerSector));
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

/*
 * This is the callback function associated with a I/O invoked from
 * kernel code.
 */
void
rf_KernelWakeupFunc(vbp)
	struct buf *vbp;
{
	RF_DiskQueueData_t *req = NULL;
	RF_DiskQueue_t *queue;
	struct raidbuf *raidbp = (struct raidbuf *)vbp;
	struct buf *bp;
	struct raid_softc *rs;
	int unit;
	int s;

	s = splbio();
	db1_printf(("recovering the request queue:\n"));
	req = raidbp->req;

	bp = raidbp->rf_obp;

	queue = (RF_DiskQueue_t *)req->queue;

	if (raidbp->rf_buf.b_flags & B_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error =
		    raidbp->rf_buf.b_error ? raidbp->rf_buf.b_error : EIO;
	}

#if 1
	/* XXX Methinks this could be wrong... */
	bp->b_resid = raidbp->rf_buf.b_resid;
#endif

	if (req->tracerec) {
		RF_ETIMER_STOP(req->tracerec->timer);
		RF_ETIMER_EVAL(req->tracerec->timer);
		RF_LOCK_MUTEX(rf_tracing_mutex);
		req->tracerec->diskwait_us +=
		    RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->phys_io_us +=
		    RF_ETIMER_VAL_US(req->tracerec->timer);
		req->tracerec->num_phys_ios++;
		RF_UNLOCK_MUTEX(rf_tracing_mutex);
	}

	bp->b_bcount = raidbp->rf_buf.b_bcount;/* XXXX ?? */

	unit = queue->raidPtr->raidid; /* *Much* simpler :-> */

	/*
	 * XXX Ok, let's get aggressive... If B_ERROR is set, let's go
	 * ballistic, and mark the component as hosed...
	 */
	if (bp->b_flags & B_ERROR) {
		/* Mark the disk as dead but only mark it once... */
		if (queue->raidPtr->Disks[queue->row][queue->col].status ==
		    rf_ds_optimal) {
			printf("raid%d: IO Error.  Marking %s as failed.\n",
			    unit,
			    queue->raidPtr->
			    Disks[queue->row][queue->col].devname);
			queue->raidPtr->Disks[queue->row][queue->col].status =
			    rf_ds_failed;
			queue->raidPtr->status[queue->row] = rf_rs_degraded;
			queue->raidPtr->numFailures++;
			/* XXX here we should bump the version number for each component, and write that data out */
		} else {
			/* Disk is already dead... */
			/* printf("Disk already marked as dead!\n"); */
		}
	}

	rs = &raid_softc[unit];
	RAIDPUTBUF(raidbp);

	if (bp->b_resid==0) {
		/* XXX is this the right place for a disk_unbusy()??!??!?!? */
		disk_unbusy(&rs->sc_dkdev, (bp->b_bcount - bp->b_resid));
	}

	rf_DiskIOComplete(queue, req, (bp->b_flags & B_ERROR) ? 1 : 0);
	(req->CompleteFunc)(req->argument, (bp->b_flags & B_ERROR) ? 1 : 0);

	splx(s);
}

/*
 * Initialize a buf structure for doing an I/O in the kernel.
 */
void
rf_InitBP(bp, b_vp, rw_flag, dev, startSect, numSect, buf, cbFunc, cbArg,
    logBytesPerSector, b_proc)
	struct buf *bp;
	struct vnode *b_vp;
	unsigned rw_flag;
	dev_t dev;
	RF_SectorNum_t startSect;
	RF_SectorCount_t numSect;
	caddr_t buf;
	void (*cbFunc)(struct buf *);
	void *cbArg;
	int logBytesPerSector;
	struct proc *b_proc;
{
	/* bp->b_flags = B_PHYS | rw_flag; */
	bp->b_flags = B_CALL | rw_flag; /* XXX need B_PHYS here too??? */
	bp->b_bcount = numSect << logBytesPerSector;
	bp->b_bufsize = bp->b_bcount;
	bp->b_error = 0;
	bp->b_dev = dev;
	bp->b_un.b_addr = buf;
	bp->b_blkno = startSect;
	bp->b_resid = bp->b_bcount; /* XXX is this right!??!?!! */
	if (bp->b_bcount == 0) {
		panic("bp->b_bcount is zero in rf_InitBP!!\n");
	}
	bp->b_proc = b_proc;
	bp->b_iodone = cbFunc;
	bp->b_vp = b_vp;
	LIST_INIT(&bp->b_dep);
}

void
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
void
raidgetdisklabel(dev)
	dev_t dev;
{
	int unit = raidunit(dev);
	struct raid_softc *rs = &raid_softc[unit];
	char *errstring;
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
	    rs->sc_dkdev.dk_label, rs->sc_dkdev.dk_cpulabel, 0);
	if (errstring)
		raidmakedisklabel(rs);
	else {
		int i;
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
void
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
 * Lookup the provided name in the filesystem.	If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 * You'll find the original of this in ccd.c
 */
int
raidlookup(path, p, vpp)
	char *path;
	struct proc *p;
	struct vnode **vpp;	/* result */
{
	struct nameidata nd;
	struct vnode *vp;
	struct vattr va;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, path, p);
	if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
#ifdef DEBUG
		printf("RAIDframe: vn_open returned %d\n", error);
#endif
		return (error);
	}
	vp = nd.ni_vp;
	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (error);
	}
	/* XXX: eventually we should handle VREG, too. */
	if (va.va_type != VBLK) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (ENOTBLK);
	}
	VOP_UNLOCK(vp, 0, p);
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
int
raidlock(rs)
	struct raid_softc *rs;
{
	int error;

	while ((rs->sc_flags & RAIDF_LOCKED) != 0) {
		rs->sc_flags |= RAIDF_WANTED;
		if ((error = tsleep(rs, PRIBIO | PCATCH, "raidlck", 0)) != 0)
			return (error);
	}
	rs->sc_flags |= RAIDF_LOCKED;
	return (0);
}

/*
 * Unlock and wake up any waiters.
 */
void
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
	RF_ComponentLabel_t component_label;
	raidread_component_label(dev, b_vp, &component_label);
	component_label.mod_counter = mod_counter;
	component_label.clean = RF_RAID_CLEAN;
	raidwrite_component_label(dev, b_vp, &component_label);
	return(0);
}


int 
raidmarkdirty(dev_t dev, struct vnode *b_vp, int mod_counter)
{
	RF_ComponentLabel_t component_label;
	raidread_component_label(dev, b_vp, &component_label);
	component_label.mod_counter = mod_counter;
	component_label.clean = RF_RAID_DIRTY;
	raidwrite_component_label(dev, b_vp, &component_label);
	return(0);
}

/* ARGSUSED */
int
raidread_component_label(dev, b_vp, component_label)
	dev_t dev;
	struct vnode *b_vp;
	RF_ComponentLabel_t *component_label;
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
		memcpy(component_label, bp->b_un.b_addr,
		       sizeof(RF_ComponentLabel_t));
#if 0
		printf("raidread_component_label: got component label:\n");
		printf("Version: %d\n",component_label->version);
		printf("Serial Number: %d\n",component_label->serial_number);
		printf("Mod counter: %d\n",component_label->mod_counter);
		printf("Row: %d\n", component_label->row);
		printf("Column: %d\n", component_label->column);
		printf("Num Rows: %d\n", component_label->num_rows);
		printf("Num Columns: %d\n", component_label->num_columns);
		printf("Clean: %d\n", component_label->clean);
		printf("Status: %d\n", component_label->status);
#endif
        } else {
		printf("Failed to read RAID component label!\n");
	}

        bp->b_flags = B_INVAL | B_AGE;
	brelse(bp); 
	return(error);
}
/* ARGSUSED */
int 
raidwrite_component_label(dev, b_vp, component_label)
	dev_t dev; 
	struct vnode *b_vp;
	RF_ComponentLabel_t *component_label;
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

	memset( bp->b_un.b_addr, 0, RF_COMPONENT_INFO_SIZE );

	memcpy( bp->b_un.b_addr, component_label, sizeof(RF_ComponentLabel_t));

	(*bdevsw[major(bp->b_dev)].d_strategy)(bp);
	error = biowait(bp); 
        bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	if (error) {
		printf("Failed to write RAID component info!\n");
	}

	return(error);
}

void 
rf_markalldirty( raidPtr )
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t c_label;
	int r,c;

	raidPtr->mod_counter++;
	for (r = 0; r < raidPtr->numRow; r++) {
		for (c = 0; c < raidPtr->numCol; c++) {
			if (raidPtr->Disks[r][c].status != rf_ds_failed) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&c_label);
				if (c_label.status == rf_ds_spared) {
					/* XXX do something special... 
					 but whatever you do, don't 
					 try to access it!! */
				} else {
#if 0
				c_label.status = 
					raidPtr->Disks[r][c].status;
				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&c_label);
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
				      &c_label);
			/* make sure status is noted */
			c_label.version = RF_COMPONENT_LABEL_VERSION; 
			c_label.mod_counter = raidPtr->mod_counter;
			c_label.serial_number = raidPtr->serial_number;
			c_label.row = srow;
			c_label.column = scol;
			c_label.num_rows = raidPtr->numRow;
			c_label.num_columns = raidPtr->numCol;
			c_label.clean = RF_RAID_DIRTY; /* changed in a bit*/
			c_label.status = rf_ds_optimal;
			raidwrite_component_label(
				      raidPtr->Disks[r][sparecol].dev,
				      raidPtr->raid_cinfo[r][sparecol].ci_vp,
				      &c_label);
			raidmarkclean( raidPtr->Disks[r][sparecol].dev, 
			              raidPtr->raid_cinfo[r][sparecol].ci_vp);
		}
	}

#endif
}


void
rf_update_component_labels( raidPtr )
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t c_label;
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
					&c_label);
				/* make sure status is noted */
				c_label.status = rf_ds_optimal;
				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&c_label);
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean( 
					      raidPtr->Disks[r][c].dev, 
					      raidPtr->raid_cinfo[r][c].ci_vp,
					      raidPtr->mod_counter);
				}
			} 
			/* else we don't touch it.. */
#if 0
			else if (raidPtr->Disks[r][c].status !=
				   rf_ds_failed) {
				raidread_component_label(
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&c_label);
				/* make sure status is noted */
				c_label.status = 
					raidPtr->Disks[r][c].status;
				raidwrite_component_label( 
					raidPtr->Disks[r][c].dev,
					raidPtr->raid_cinfo[r][c].ci_vp,
					&c_label);
				if (raidPtr->parity_good == RF_RAID_CLEAN) {
					raidmarkclean( 
					      raidPtr->Disks[r][c].dev, 
					      raidPtr->raid_cinfo[r][c].ci_vp,
					      raidPtr->mod_counter);
				}
			}
#endif
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
			
			raidread_component_label( 
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      &c_label);
			/* make sure status is noted */
			c_label.version = RF_COMPONENT_LABEL_VERSION; 
			c_label.mod_counter = raidPtr->mod_counter;
			c_label.serial_number = raidPtr->serial_number;
			c_label.row = srow;
			c_label.column = scol;
			c_label.num_rows = raidPtr->numRow;
			c_label.num_columns = raidPtr->numCol;
			c_label.clean = RF_RAID_DIRTY; /* changed in a bit*/
			c_label.status = rf_ds_optimal;
			raidwrite_component_label(
				      raidPtr->Disks[0][sparecol].dev,
				      raidPtr->raid_cinfo[0][sparecol].ci_vp,
				      &c_label);
			if (raidPtr->parity_good == RF_RAID_CLEAN) {
				raidmarkclean( raidPtr->Disks[0][sparecol].dev,
			              raidPtr->raid_cinfo[0][sparecol].ci_vp,
					       raidPtr->mod_counter);
			}
		}
	}
	/* 	printf("Component labels updated\n"); */
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
