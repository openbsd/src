/*	$OpenBSD: rf_netbsdkintf.c,v 1.1 1999/01/11 14:29:30 niklas Exp $	*/
/*	$NetBSD: rf_netbsdkintf.c,v 1.5 1998/12/22 20:03:14 oster Exp $	*/
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
/*
 * :  
 * Log: rf_kintf.c,v 
 * Revision 1.57  1996/07/19 16:12:20  jimz
 * remove addition of protectedSectors in InitBP- it's already
 * done in the diskqueue code
 *
 * Revision 1.56  1996/07/17  21:00:58  jimz
 * clean up timer interface, tracing
 *
 * Revision 1.55  1996/06/17  03:00:54  jimz
 * Change RAIDFRAME_GET_INFO interface to do its own copyout()
 * (because size of device config structure now exceeds 8k)
 *
 * Revision 1.54  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.53  1996/06/07  21:33:04  jimz
 * begin using consistent types for sector numbers,
 * stripe numbers, row+col numbers, recon unit numbers
 *
 * Revision 1.52  1996/06/06  17:28:08  jimz
 * track sector number of last I/O dequeued
 *
 * Revision 1.51  1996/06/05  18:06:02  jimz
 * Major code cleanup. The Great Renaming is now done.
 * Better modularity. Better typing. Fixed a bunch of
 * synchronization bugs. Made a lot of global stuff
 * per-desc or per-array. Removed dead code.
 *
 * Revision 1.50  1996/06/03  23:28:26  jimz
 * more bugfixes
 * check in tree to sync for IPDS runs with current bugfixes
 * there still may be a problem with threads in the script test
 * getting I/Os stuck- not trivially reproducible (runs ~50 times
 * in a row without getting stuck)
 *
 * Revision 1.49  1996/06/02  17:31:48  jimz
 * Moved a lot of global stuff into array structure, where it belongs.
 * Fixed up paritylogging, pss modules in this manner. Some general
 * code cleanup. Removed lots of dead code, some dead files.
 *
 * Revision 1.48  1996/05/31  22:26:54  jimz
 * fix a lot of mapping problems, memory allocation problems
 * found some weird lock issues, fixed 'em
 * more code cleanup
 *
 * Revision 1.47  1996/05/30  12:59:18  jimz
 * make etimer happier, more portable
 *
 * Revision 1.46  1996/05/30  11:29:41  jimz
 * Numerous bug fixes. Stripe lock release code disagreed with the taking code
 * about when stripes should be locked (I made it consistent: no parity, no lock)
 * There was a lot of extra serialization of I/Os which I've removed- a lot of
 * it was to calculate values for the cache code, which is no longer with us.
 * More types, function, macro cleanup. Added code to properly quiesce the array
 * on shutdown. Made a lot of stuff array-specific which was (bogusly) general
 * before. Fixed memory allocation, freeing bugs.
 *
 * Revision 1.45  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.44  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.43  1996/05/24  01:59:45  jimz
 * another checkpoint in code cleanup for release
 * time to sync kernel tree
 *
 * Revision 1.42  1996/05/23  22:17:54  jimz
 * fix sector size hardcoding problems
 *
 * Revision 1.41  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.40  1996/05/23  13:18:07  jimz
 * tracing_mutex -> rf_tracing_mutex
 *
 * Revision 1.39  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.38  1996/05/20  16:15:32  jimz
 * switch to rf_{mutex,cond}_{init,destroy}
 *
 * Revision 1.37  1996/05/10  16:23:47  jimz
 * RF_offset -> RF_Offset
 *
 * Revision 1.36  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.35  1996/05/03  19:10:48  jimz
 * change sanity checking for bogus I/Os to return more appropriate
 * values (to make some user-level utilities happer with RAIDframe)
 *
 * Revision 1.34  1996/05/02  22:17:00  jimz
 * When using DKUSAGE, send a bogus IO after configuring to let DKUSAGE know
 * that we exist. This will let user-level programs doing group stats on the
 * RF device function without error before RF gets its first IO
 *
 * Changed rf_device_config devs and spares fields to RF_RaidDisk_t
 *
 * Inc numOutstanding for the disk queue in rf_DispatchKernelIO if
 * type is IO_TYPE_NOP. I'm not sure this is right, but it seems to be,
 * because the disk IO completion routine wants to dec it, and doesn't
 * care if there was no such IO.
 *
 * Revision 1.33  1996/05/02  15:05:44  jimz
 * for now, rf_DoAccessKernel will reject non-sector-sized I/Os
 * eventually, it should do something more clever...
 * (and do it in DoAccess(), not just DoAccessKernel())
 *
 * Revision 1.32  1996/05/01  16:28:39  jimz
 * get rid of uses of ccmn_ functions
 *
 * Revision 1.31  1996/05/01  15:42:17  jimz
 * ccmn_* memory management is on the way out. This is an archival checkpoint-
 * both the old and new code are in place (all the ccmn_ calls are #if 0). After
 * this, the ccmn_ code will no longer appear.
 *
 * Revision 1.30  1996/04/22  15:53:13  jimz
 * MAX_RAIDS -> NRAIDFRAME
 *
 * Revision 1.29  1995/12/12  18:10:06  jimz
 * MIN -> RF_MIN, MAX -> RF_MAX, ASSERT -> RF_ASSERT
 * fix 80-column brain damage in comments
 *
 * Revision 1.28  1995/12/01  19:11:01  root
 * added copyright info
 *
 * Revision 1.27  1995/11/28  18:56:40  wvcii
 * disabled buffer copy in rf_write
 *
 * Revision 1.26  1995/10/06  16:37:08  jimz
 * get struct bufs from ubc, not cam
 * copy all write data, and operate on copy
 * (temporary hack to get around dags in PQ that want
 * to Xor into user write buffers)
 *
 * Revision 1.25  1995/09/30  22:23:08  jimz
 * do not require raid to be active to perform ACCTOTAL ioctl
 *
 * Revision 1.24  1995/09/30  20:39:08  jimz
 * added new ioctls:
 *   RAIDFRAME_RESET_ACCTOTALS
 *   RAIDFRAME_GET_ACCTOTALS
 *   RAIDFRAME_KEEP_ACCTOTALS
 *
 * Revision 1.23  1995/09/20  21:11:59  jimz
 * include dfstrace.h in KERNEL block
 * (even though it's a kernel-only file, this makes the depend process
 * at user-level happy. Why the user-level Makefile wants to depend
 * kintf.c is less clear, but this is a workaround).
 *
 * Revision 1.22  1995/09/19  23:19:03  jimz
 * added DKUSAGE support
 *
 */




#ifdef _KERNEL
#define KERNEL
#endif



#ifdef KERNEL

#include <sys/errno.h>

#ifdef __NetBSD__
#include "raid.h"
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
#endif

#include <sys/param.h>
#include <sys/types.h>

#include <machine/types.h>

#include <sys/disklabel.h>

#include <sys/conf.h>


#ifdef __NetBSD__
#include <sys/lock.h>
#endif /* __NetBSD__ */


#include <sys/buf.h>
#include <sys/user.h>
#include "rf_raid.h"
#include "rf_raidframe.h"
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

int rf_kdebug_level = 0;

#define RFK_BOOT_NONE 0
#define RFK_BOOT_GOOD 1
#define RFK_BOOT_BAD  2
static int rf_kbooted = RFK_BOOT_NONE;

#ifdef DEBUG
#define db0_printf(a) printf a
#define db_printf(a) if (rf_kdebug_level > 0) printf a
#define db1_printf(a) if (rf_kdebug_level > 0) printf a
#define db2_printf(a) if (rf_kdebug_level > 1) printf a
#define db3_printf(a) if (rf_kdebug_level > 2) printf a
#define db4_printf(a) if (rf_kdebug_level > 3) printf a
#define db5_printf(a) if (rf_kdebug_level > 4) printf a
#else /* DEBUG */
#define db0_printf(a) printf a
#define db1_printf(a) { }
#define db2_printf(a) { }
#define db3_printf(a) { }
#define db4_printf(a) { }
#define db5_printf(a) { }
#endif /* DEBUG */

static RF_Raid_t **raidPtrs; /* global raid device descriptors */

static int rf_pending_testaccs;

RF_DECLARE_STATIC_MUTEX(rf_sparet_wait_mutex)
RF_DECLARE_STATIC_MUTEX(rf_async_done_q_mutex)
static RF_SparetWait_t *rf_sparet_wait_queue;      /* requests to install a spare table */
static RF_SparetWait_t *rf_sparet_resp_queue;      /* responses from installation process */
static struct rf_test_acc    *rf_async_done_qh, *rf_async_done_qt;

static struct rf_recon_req *recon_queue = NULL;          /* used to communicate reconstruction requests */


decl_simple_lock_data(,recon_queue_mutex)


#define LOCK_RECON_Q_MUTEX() simple_lock(&recon_queue_mutex)
#define UNLOCK_RECON_Q_MUTEX() simple_unlock(&recon_queue_mutex)

/* prototypes */
static void KernelWakeupFunc(struct buf  *bp);
static void InitBP(struct buf *bp, struct vnode *, unsigned rw_flag, dev_t dev,
	RF_SectorNum_t startSect, RF_SectorCount_t numSect, caddr_t buf,
	void (*cbFunc)(struct buf *), void *cbArg, int logBytesPerSector,
	struct proc *b_proc);

#define Dprintf0(s)       if (rf_queueDebug) rf_debug_printf(s,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf1(s,a)     if (rf_queueDebug) rf_debug_printf(s,a,NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)   if (rf_queueDebug) rf_debug_printf(s,a,b,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c) if (rf_queueDebug) rf_debug_printf(s,a,b,c,NULL,NULL,NULL,NULL,NULL)


/* this is so that we can compile under 2.0 as well as 3.2 */
#ifndef proc_to_task
#define proc_to_task(x) ((x)->task)
#endif /* !proc_to_task */

void	raidattach __P((int));
int	raidsize __P((dev_t));

void	rf_DiskIOComplete(RF_DiskQueue_t *, RF_DiskQueueData_t *, int);
void	rf_CopybackReconstructedData(RF_Raid_t *raidPtr);
static	int raidinit __P((dev_t,RF_Raid_t *,int));

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
        struct buf	rf_buf;		/* new I/O buf.  MUST BE FIRST!!! */
	struct buf	*rf_obp;	/* ptr. to original I/O buf */
	int		rf_flags;	/* misc. flags */
	RF_DiskQueueData_t  *req;       /* the request that this was part of.. */
};


#define RAIDGETBUF(rs) pool_get(&(rs)->sc_cbufpool, PR_NOWAIT)
#define	RAIDPUTBUF(rs, cbp) pool_put(&(rs)->sc_cbufpool, cbp)

/* XXX Not sure if the following should be replacing the raidPtrs above, 
or if it should be used in conjunction with that... */

struct raid_softc {
	int		 sc_unit;		/* logical unit number */
	int		 sc_flags;		/* flags */
	int		 sc_cflags;		/* configuration flags */
	size_t		 sc_size;		/* size of the raid device */
	dev_t            sc_dev;                /* our device..*/
	char		 sc_xname[20];		/* XXX external name */
	struct disk	 sc_dkdev;		/* generic disk device info */
	struct pool      sc_cbufpool;           /* component buffer pool */
};

/* sc_flags */
#define RAIDF_INITED	0x01	/* unit has been initialized */
#define RAIDF_WLABEL	0x02	/* label area is writable */
#define RAIDF_LABELLING	0x04	/* unit is currently being labelled */
#define RAIDF_WANTED	0x40	/* someone is waiting to obtain a lock */
#define RAIDF_LOCKED	0x80	/* unit is locked */

#define	raidunit(x)	DISKUNIT(x)
static int numraid=0;

#define RAIDLABELDEV(dev)	\
	(MAKEDISKDEV(major((dev)), raidunit((dev)), RAW_PART))

/* declared here, and made public, for the benefit of KVM stuff.. */
struct raid_softc *raid_softc;

static	void raidgetdefaultlabel __P((RF_Raid_t *, struct raid_softc *, struct disklabel *));
static	void raidgetdisklabel __P((dev_t));
static	void raidmakedisklabel __P((struct raid_softc *));

static	int raidlock __P((struct raid_softc *));
static	void raidunlock __P((struct raid_softc *));
int raidlookup __P((char *, struct proc *p, struct vnode **));


void
raidattach(num)
	int num;
{
	int raidID;

#ifdef DEBUG
	printf("raidattach: Asked for %d units\n",num);
#endif

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("raidattach: count <= 0");
#endif
		return;
	}
	/* 
	   This is where all the initialization stuff gets done.
	 */

	/* Make some space for requested number of units... */

	RF_Calloc(raidPtrs, num, sizeof(RF_Raid_t *), (RF_Raid_t **));
	if (raidPtrs == NULL) {
		panic("raidPtrs is NULL!!\n");
	}



	rf_kbooted = rf_boot();
	if (rf_kbooted) {
		panic("Serious error booting RAID!!\n");
	}
	
	rf_kbooted = RFK_BOOT_GOOD; 

	/* 
	   put together some datastructures like the CCD device does.. 
	   This lets us lock the device and what-not when it gets opened.
        */
	
	raid_softc = (struct raid_softc *)
		malloc(num * sizeof(struct raid_softc),
		       M_DEVBUF, M_NOWAIT);
	if (raid_softc == NULL) {
		printf("WARNING: no memory for RAIDframe driver\n");
		return;
	}
	numraid = num;
	bzero(raid_softc, num * sizeof(struct raid_softc));

	for(raidID=0;raidID < num;raidID++) {
		RF_Calloc(raidPtrs[raidID], 1, sizeof(RF_Raid_t), 
			  (RF_Raid_t *));
		if (raidPtrs[raidID]==NULL) {
			printf("raidPtrs[%d] is NULL\n",raidID);
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
	return ENXIO;
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
	unsigned int raidID;
	int rc;
	int error = 0;
	
	/* This whole next chunk of code is somewhat suspect... Not sure
	   it's needed here at all... XXX */

	if (rf_kbooted == RFK_BOOT_NONE) { 
		printf("Doing restart on raidopen.\n");
		rf_kbooted = RFK_BOOT_GOOD;
		rc = rf_boot();
		if (rc) {
			rf_kbooted = RFK_BOOT_BAD;
			printf("Someone is unhappy...\n");
			return(rc);
		}
	}

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((error = raidlock(rs)) != 0)
		return(error);
	lp = rs->sc_dkdev.dk_label;

	raidID = raidunit(dev);

	part = DISKPART(dev);
	pmask = (1 << part);

	db1_printf(("Opening raid device number: %d partition: %d\n",
		    raidID,part));


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
			return(error);
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
	rs->sc_dkdev.dk_openmask =
	    rs->sc_dkdev.dk_copenmask | rs->sc_dkdev.dk_bopenmask;

	raidunlock(rs);

	return(error);


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

	raidunlock(rs);
	return (0);

}

void
raidstrategy(bp)
	register struct buf *bp;
{
	register int s;

	unsigned int raidID = raidunit(bp->b_dev);
	RF_Raid_t *raidPtr;
	struct raid_softc *rs = &raid_softc[raidID];
	struct disklabel *lp;
	int wlabel;

#if 0
	db1_printf(("Strategy: 0x%x 0x%x\n",bp,bp->b_data));
	db1_printf(("Strategy(2): bp->b_bufsize%d\n", (int)bp->b_bufsize));
	db1_printf(("bp->b_count=%d\n",(int)bp->b_bcount));
	db1_printf(("bp->b_resid=%d\n",(int)bp->b_resid));
	db1_printf(("bp->b_blkno=%d\n",(int)bp->b_blkno));

	if (bp->b_flags&B_READ) 
		db1_printf(("READ\n"));
	else
		db1_printf(("WRITE\n"));
#endif
	if (rf_kbooted != RFK_BOOT_GOOD)
		return;
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

	wlabel = rs->sc_flags & (RAIDF_WLABEL|RAIDF_LABELLING);
	if (DISKPART(bp->b_dev) != RAW_PART)
		if (bounds_check_with_label(bp, lp, wlabel) <= 0) {
			db1_printf(("Bounds check failed!!:%d %d\n",
			       (int)bp->b_blkno,(int)wlabel));
			biodone(bp);
			return;
		}

	s = splbio(); /* XXX Needed? */
	db1_printf(("Beginning strategy...\n"));
	
	bp->b_resid = 0;
	bp->b_error = rf_DoAccessKernel(raidPtrs[raidID], bp, 
					NULL, NULL, NULL);
	if (bp->b_error) {
		bp->b_flags |= B_ERROR;
		db1_printf(("bp->b_flags HAS B_ERROR SET!!!: %d\n",
			    bp->b_error));
	}
	splx(s);
#if 0
	db1_printf(("Strategy exiting: 0x%x 0x%x %d %d\n",
		    bp,bp->b_data, 
		    (int)bp->b_bcount,(int)bp->b_resid));
#endif
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
	int result;
	int part;

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	if ((rs->sc_flags & RAIDF_INITED) == 0)
		return (ENXIO);
	part = DISKPART(dev);

	db1_printf(("raidread: unit: %d partition: %d\n",unit,part));

#if 0
	return (physio(raidstrategy, NULL, dev, B_READ, minphys, uio));
#endif
	result=physio(raidstrategy, NULL, dev, B_READ, minphys, uio);
	db1_printf(("raidread done.  Result is %d %d\n",
		    result,uio->uio_resid));
	return(result);

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
#if 0
	int r,c;
#endif
	/* 	struct raid_ioctl *ccio = (struct ccd_ioctl *)data; */

	/* 	struct ccdbuf *cbp; */
	/* 	struct raidbuf *raidbp; */
	RF_Config_t *k_cfg, *u_cfg;
	u_char *specific_buf;
	int retcode = 0;

	int row;
	struct rf_recon_req *rrcopy, *rr;
#if 0
	int nbytes, spl, rw, row;
	struct rf_test_acc *ta;
	struct buf *bp;
	RF_SparetWait_t *waitreq;
	struct rf_test_acc *ta_p, *ta_copy;
#endif

	if (unit >= numraid)
		return (ENXIO);
	rs = &raid_softc[unit];

	db1_printf(("raidioctl: %d %d %d %d\n",(int)dev,
		    (int)DISKPART(dev),(int)unit,(int)cmd));

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
	case RAIDFRAME_CHECKRECON:
		if ((rs->sc_flags & RAIDF_INITED) == 0)
			return (ENXIO);
	}
	
	switch (cmd) {


		/* configure the system */
	case RAIDFRAME_CONFIGURE:

		db3_printf(("rf_ioctl: RAIDFRAME_CONFIGURE\n"));
		/* copy-in the configuration information */
		/* data points to a pointer to the configuration structure */
		u_cfg = *((RF_Config_t **) data);   
		RF_Malloc(k_cfg,sizeof(RF_Config_t),(RF_Config_t *));
		if (k_cfg == NULL) {
			db3_printf(("rf_ioctl: ENOMEM for config. Code is %d\n", retcode));
			return(ENOMEM);
		}
		retcode = copyin((caddr_t) u_cfg, (caddr_t) k_cfg, 
				 sizeof(RF_Config_t));
		if (retcode) {
			db3_printf(("rf_ioctl: retcode=%d copyin.1\n", 
				    retcode));
			return(retcode);
		}

		/* allocate a buffer for the layout-specific data, 
		   and copy it in */
		if (k_cfg->layoutSpecificSize) {
			if (k_cfg->layoutSpecificSize > 10000) { 
				/* sanity check */
				db3_printf(("rf_ioctl: EINVAL %d\n", retcode));
				return(EINVAL);
			}
			RF_Malloc(specific_buf,k_cfg->layoutSpecificSize,
				  (u_char *));
			if (specific_buf == NULL) {
				RF_Free(k_cfg,sizeof(RF_Config_t));
				db3_printf(("rf_ioctl: ENOMEM %d\n", retcode));
				return(ENOMEM);
			}
			retcode = copyin(k_cfg->layoutSpecific, 
					 (caddr_t) specific_buf, 
					 k_cfg->layoutSpecificSize);
			if (retcode) {
				db3_printf(("rf_ioctl: retcode=%d copyin.2\n",
					    retcode));
				return(retcode);
			}
		} else specific_buf = NULL;
		k_cfg->layoutSpecific = specific_buf;
		
		/* should do some kind of sanity check on the configuration. 
		   Store the sum of all the bytes in the last byte?
		   */

#if 0
		db1_printf(("Considering configuring the system.:%d 0x%x\n",
			    unit,p));
#endif

		/* We need the pointer to this a little deeper, so
		   stash it here... */

		raidPtrs[unit]->proc = p;

		/* configure the system */
		rf_pending_testaccs = 0;


		raidPtrs[unit]->raidid = unit;
		retcode = rf_Configure(raidPtrs[unit], k_cfg);

		
		if (retcode == 0) {
			retcode = raidinit(dev, raidPtrs[unit],unit);
		} 

		/* free the buffers.  No return code here. */
		if (k_cfg->layoutSpecificSize) {
			RF_Free(specific_buf,k_cfg->layoutSpecificSize);
		}
		RF_Free(k_cfg,sizeof(RF_Config_t));
		
		db3_printf(("rf_ioctl: retcode=%d RAIDFRAME_CONFIGURE\n", 
			    retcode));
		return(retcode);
		
		/* shutdown the system */    
	case RAIDFRAME_SHUTDOWN:
		
		if ((error = raidlock(rs)) != 0) 
			return(error);

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
		
		/* the intention here was to disallow shutdowns while
		   raidframe is mounted, but it doesn't work because the 
		   shutdown ioctl calls rf_open
		   */
		if (rf_pending_testaccs > 0) {
			printf("RAIDFRAME:  Can't shutdown because there are %d pending test accs\n",
			       rf_pending_testaccs);
			return(EINVAL);
		}
		if (rf_debugKernelAccess) {
			printf("call shutdown\n");
		}
		raidPtrs[unit]->proc = p; /* XXX  necessary evil */
		retcode = rf_Shutdown(raidPtrs[unit]);

		db1_printf(("Done main shutdown\n"));

		pool_destroy(&rs->sc_cbufpool);
		db1_printf(("Done freeing component buffer freelist\n"));

		/* It's no longer initialized... */
		rs->sc_flags &= ~RAIDF_INITED;

                /* Detach the disk. */
                disk_detach(&rs->sc_dkdev);

		raidunlock(rs);

		return(retcode);
		
		/* initialize all parity */
	case RAIDFRAME_REWRITEPARITY:

		if (raidPtrs[unit]->Layout.map->faultsTolerated == 0) 
			return(EINVAL);
		/* borrow the thread of the requesting process */
		raidPtrs[unit]->proc = p; /* Blah... :-p GO */
		retcode = rf_RewriteParity(raidPtrs[unit]);
		/* return I/O Error if the parity rewrite fails */ 

		if (retcode) 
			retcode = EIO;           
		return(retcode);

		/* issue a test-unit-ready through raidframe to the 
		   indicated device */
#if 0 /* XXX not supported yet (ever?) */
	case RAIDFRAME_TUR:
		/* debug only */
		retcode = rf_SCSI_DoTUR(0, 0, 0, 0, *(dev_t *) data);  
		return(retcode);
#endif
	case RAIDFRAME_GET_INFO:
		{
			RF_Raid_t *raid = raidPtrs[unit];
			RF_DeviceConfig_t *cfg, **ucfgp;
			int i, j, d;
			
			if (!raid->valid)
				return(ENODEV);
			ucfgp = (RF_DeviceConfig_t **)data;
			RF_Malloc(cfg,sizeof(RF_DeviceConfig_t),
				  (RF_DeviceConfig_t *));
			if (cfg == NULL)
				return(ENOMEM);
			bzero((char *)cfg, sizeof(RF_DeviceConfig_t));
			cfg->rows = raid->numRow;
			cfg->cols = raid->numCol;
			cfg->ndevs = raid->numRow * raid->numCol;
			if (cfg->ndevs >= RF_MAX_DISKS) {
				cfg->ndevs = 0;
				return(ENOMEM);
			}
			cfg->nspares = raid->numSpare;
			if (cfg->nspares >= RF_MAX_DISKS) {
				cfg->nspares = 0;
				return(ENOMEM);
			}
			cfg->maxqdepth = raid->maxQueueDepth;
			d = 0;
			for(i=0;i<cfg->rows;i++) {
				for(j=0;j<cfg->cols;j++) {
					cfg->devs[d] = raid->Disks[i][j];
					d++;
				}
			}
			for(j=cfg->cols,i=0;i<cfg->nspares;i++,j++) {
				cfg->spares[i] = raid->Disks[0][j];
			}
			retcode = copyout((caddr_t)cfg, (caddr_t)*ucfgp, 
					  sizeof(RF_DeviceConfig_t));
			RF_Free(cfg,sizeof(RF_DeviceConfig_t));
			
			return(retcode);
		}
	break;
	
	case RAIDFRAME_RESET_ACCTOTALS:
		{
			RF_Raid_t *raid = raidPtrs[unit];
			
			bzero(&raid->acc_totals, sizeof(raid->acc_totals));
			return(0);
		}
	break;
	
	case RAIDFRAME_GET_ACCTOTALS:
		{
			RF_AccTotals_t *totals = (RF_AccTotals_t *)data;
			RF_Raid_t *raid = raidPtrs[unit];
			
			*totals = raid->acc_totals;
			return(0);
		}
	break;
	
	case RAIDFRAME_KEEP_ACCTOTALS:
		{
			RF_Raid_t *raid = raidPtrs[unit];
			int *keep = (int *)data;
			
			raid->keep_acc_totals = *keep;
			return(0);
		}
	break;
	
	case RAIDFRAME_GET_SIZE:
		*(int *) data = raidPtrs[unit]->totalSectors;
		return(0);

#define RAIDFRAME_RECON 1
		/* XXX The above should probably be set somewhere else!! GO */
#if RAIDFRAME_RECON > 0

		/* fail a disk & optionally start reconstruction */
	case RAIDFRAME_FAIL_DISK:
		rr = (struct rf_recon_req *) data;
		
		if (rr->row < 0 || rr->row >= raidPtrs[unit]->numRow 
		    || rr->col < 0 || rr->col >= raidPtrs[unit]->numCol)
			return(EINVAL);

		printf("Failing the disk: row: %d col: %d\n",rr->row,rr->col);
		
		/* make a copy of the recon request so that we don't 
		   rely on the user's buffer */
		RF_Malloc(rrcopy, sizeof(*rrcopy), (struct rf_recon_req *));
		bcopy(rr, rrcopy, sizeof(*rr));
		rrcopy->raidPtr = (void *) raidPtrs[unit];

		LOCK_RECON_Q_MUTEX();
		rrcopy->next = recon_queue;
		recon_queue = rrcopy;
		wakeup(&recon_queue);
		UNLOCK_RECON_Q_MUTEX();
		
		return(0);
		
		/* invoke a copyback operation after recon on whatever 
		   disk needs it, if any */
	case RAIDFRAME_COPYBACK:		
		/* borrow the current thread to get this done */
		raidPtrs[unit]->proc = p; /* ICK.. but needed :-p  GO */
		rf_CopybackReconstructedData(raidPtrs[unit]);
		return(0);
		
		/* return the percentage completion of reconstruction */
	case RAIDFRAME_CHECKRECON:
		row = *(int *) data;
		if (row < 0 || row >= raidPtrs[unit]->numRow)
			return(EINVAL);
		if (raidPtrs[unit]->status[row] != rf_rs_reconstructing) 
			*(int *) data = 100;
		else 
			*(int *) data = raidPtrs[unit]->reconControl[row]->percentComplete;
		return(0);
		
		/* the sparetable daemon calls this to wait for the 
		   kernel to need a spare table.
		   * this ioctl does not return until a spare table is needed.
		   * XXX -- calling mpsleep here in the ioctl code is almost 
		   certainly wrong and evil. -- XXX
		   * XXX -- I should either compute the spare table in the 
		   kernel, or have a different -- XXX
		   * XXX -- interface (a different character device) for
		   delivering the table          -- XXX
		  */
#if 0
	case RAIDFRAME_SPARET_WAIT:
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		while (!rf_sparet_wait_queue) mpsleep(&rf_sparet_wait_queue, (PZERO+1)|PCATCH, "sparet wait", 0, (void *) simple_lock_addr(rf_sparet_wait_mutex), MS_LOCK_SIMPLE);
		waitreq = rf_sparet_wait_queue;
		rf_sparet_wait_queue = rf_sparet_wait_queue->next;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		
		*((RF_SparetWait_t *) data) = *waitreq;              /* structure assignment */
		
		RF_Free(waitreq, sizeof(*waitreq));
		return(0);
		
		
		/* wakes up a process waiting on SPARET_WAIT and puts an 
		   error code in it that will cause the dameon to exit */
	case RAIDFRAME_ABORT_SPARET_WAIT:
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = -1;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_wait_queue;
		rf_sparet_wait_queue = waitreq;
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		wakeup(&rf_sparet_wait_queue);
		return(0);

		/* used by the spare table daemon to deliver a spare table 
		   into the kernel */
	case RAIDFRAME_SEND_SPARET:
		
		/* install the spare table */
		retcode = rf_SetSpareTable(raidPtrs[unit],*(void **) data);
		
		/* respond to the requestor.  the return status of the 
		   spare table installation is passed in the "fcol" field */
		RF_Malloc(waitreq, sizeof(*waitreq), (RF_SparetWait_t *));
		waitreq->fcol = retcode;
		RF_LOCK_MUTEX(rf_sparet_wait_mutex);
		waitreq->next = rf_sparet_resp_queue;
		rf_sparet_resp_queue = waitreq;
		wakeup(&rf_sparet_resp_queue);
		RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);
		
		return(retcode);
#endif


#endif   /* RAIDFRAME_RECON > 0 */

	default:  break;               /* fall through to the os-specific code below */
		
	}
	
	if (!raidPtrs[unit]->valid)
		return(EINVAL);
	
	/*
	 * Add support for "regular" device ioctls here.
	 */
	
	switch (cmd) {
	case DIOCGDINFO:
		db1_printf(("DIOCGDINFO %d %d\n",(int)dev,(int)DISKPART(dev)));
		*(struct disklabel *)data = *(rs->sc_dkdev.dk_label);
		break;

	case DIOCGPART:
		db1_printf(("DIOCGPART: %d %d\n",(int)dev,(int)DISKPART(dev)));
		((struct partinfo *)data)->disklab = rs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &rs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
		db1_printf(("DIOCWDINFO\n"));
	case DIOCSDINFO:
		db1_printf(("DIOCSDINFO\n"));
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
		db1_printf(("DIOCWLABEL\n"));
		if (*(int *)data != 0)
			rs->sc_flags |= RAIDF_WLABEL;
		else
			rs->sc_flags &= ~RAIDF_WLABEL;
		break;

	case DIOCGDEFLABEL:
		db1_printf(("DIOCGDEFLABEL\n"));
		raidgetdefaultlabel(raidPtrs[unit], rs,
					 (struct disklabel *)data);
		break;

	default:
		retcode = ENOTTY; /* XXXX ?? OR EINVAL ? */
	}
	return(retcode);

}


/* raidinit -- complete the rest of the initialization for the 
   RAIDframe device.  */


static int
raidinit(dev, raidPtr,unit)
	dev_t dev;
	RF_Raid_t *raidPtr;
	int unit;
{
	int retcode;
	/* 	int ix; */
	/* 	struct raidbuf *raidbp; */
	struct raid_softc *rs;

	retcode = 0;

	rs = &raid_softc[unit];
	pool_init(&rs->sc_cbufpool, sizeof(struct raidbuf), 0,
		  0, 0, "raidpl", 0, NULL, NULL, M_DEVBUF);

	
	/* XXX should check return code first... */
	rs->sc_flags |= RAIDF_INITED;

	sprintf(rs->sc_xname, "raid%d", unit); /* XXX doesn't check bounds.*/

	rs->sc_dkdev.dk_name = rs->sc_xname;	
	/* disk_attach actually creates space for the CPU disklabel, among
	   other things, so it's critical to call this *BEFORE* we
	   try putzing with disklabels. */
	disk_attach(&rs->sc_dkdev);

	/* XXX There may be a weird interaction here between this, and
	   protectedSectors, as used in RAIDframe.  */
	rs->sc_size = raidPtr->totalSectors; 
	rs->sc_dev = dev;
	return(retcode);
}


/*********************************************************
 *
 * initialization code called at boot time (startup.c)
 *
 ********************************************************/
int rf_boot()
{
  int i, rc;

  rc = rf_mutex_init(&rf_sparet_wait_mutex);
  if (rc) {
    RF_PANIC();
  }
  rc = rf_mutex_init(&rf_async_done_q_mutex);
  if (rc) {
    RF_PANIC();
  }
  rf_sparet_wait_queue = rf_sparet_resp_queue = NULL;
  recon_queue = NULL;
  rf_async_done_qh = rf_async_done_qt = NULL;
  for (i=0; i<numraid; i++)
    raidPtrs[i] = NULL;
  rc = rf_BootRaidframe();
  if (rc == 0)
    printf("Kernelized RAIDframe activated\n");
  else
    rf_kbooted = RFK_BOOT_BAD;
  return(rc);
}

/*
 * This kernel thread never exits.  It is created once, and persists
 * until the system reboots.
 */
void rf_ReconKernelThread()
{
  struct rf_recon_req *req;
  int s;

  /* XXX not sure what spl() level we should be at here... probably splbio() */
  s=splbio();

  while (1) {
    /* grab the next reconstruction request from the queue */
    LOCK_RECON_Q_MUTEX();
    while (!recon_queue) {
	    UNLOCK_RECON_Q_MUTEX();
	    tsleep(&recon_queue, PRIBIO | PCATCH, "raidframe recon", 0);
	    LOCK_RECON_Q_MUTEX();
    }
    req = recon_queue;
    recon_queue = recon_queue->next;
    UNLOCK_RECON_Q_MUTEX();

    /*
     * If flags specifies that we should start recon, this call
     * will not return until reconstruction completes, fails, or is aborted.
     */
    rf_FailDisk((RF_Raid_t *) req->raidPtr, req->row, req->col,
        ((req->flags&RF_FDFLAGS_RECON) ? 1 : 0));

    RF_Free(req, sizeof(*req));
  }
}
/* wake up the daemon & tell it to get us a spare table
 * XXX
 * the entries in the queues should be tagged with the raidPtr 
 * so that in the extremely rare case that two recons happen at once, we know for
 * which device were requesting a spare table
 * XXX
 */
int rf_GetSpareTableFromDaemon(req)
  RF_SparetWait_t  *req;
{
  int retcode;

  RF_LOCK_MUTEX(rf_sparet_wait_mutex);
  req->next = rf_sparet_wait_queue;
  rf_sparet_wait_queue = req;
  wakeup(&rf_sparet_wait_queue);

  /* mpsleep unlocks the mutex */
  while (!rf_sparet_resp_queue) {
	  tsleep(&rf_sparet_resp_queue, PRIBIO | PCATCH, 
		 "raidframe getsparetable", 0);
#if 0
	  mpsleep(&rf_sparet_resp_queue, PZERO, "sparet resp", 0, (void *) simple_lock_addr(rf_sparet_wait_mutex), MS_LOCK_SIMPLE);
#endif
  }
  req = rf_sparet_resp_queue;
  rf_sparet_resp_queue = req->next;
  RF_UNLOCK_MUTEX(rf_sparet_wait_mutex);

  retcode = req->fcol;
  RF_Free(req, sizeof(*req));   /* this is not the same req as we alloc'd */
  return(retcode);
}

/* a wrapper around rf_DoAccess that extracts appropriate info from the bp & passes it down.
 * any calls originating in the kernel must use non-blocking I/O
 * do some extra sanity checking to return "appropriate" error values for
 * certain conditions (to make some standard utilities work)
 */
int rf_DoAccessKernel(raidPtr, bp, flags, cbFunc, cbArg)
  RF_Raid_t              *raidPtr;
  struct buf             *bp;
  RF_RaidAccessFlags_t    flags;
  void                  (*cbFunc)(struct buf *);
  void                   *cbArg;
{
	RF_SectorCount_t num_blocks, pb, sum;
	RF_RaidAddr_t raid_addr;
	int retcode;
	struct partition *pp;
	daddr_t blocknum;	
	int unit;
	struct raid_softc *rs;

	/* XXX The dev_t used here should be for /dev/[r]raid* !!! */

	unit = raidPtr->raidid;
	rs = &raid_softc[unit];

	/* Ok, for the bp we have here, bp->b_blkno is relative to the
	   partition.. Need to make it absolute to the underlying
	   device.. */

	blocknum = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &rs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)];
		blocknum += pp->p_offset;
		db1_printf(("updated: %d %d\n",DISKPART(bp->b_dev),
			    pp->p_offset));
	} else {
		db1_printf(("Is raw..\n"));
	}
	db1_printf(("Blocks: %d, %d\n", (int) bp->b_blkno, (int) blocknum));

	db1_printf(("bp->b_bcount = %d\n",(int)bp->b_bcount));
	db1_printf(("bp->b_resid = %d\n",(int)bp->b_resid));

	/* *THIS* is where we adjust what block we're going to... but 
	   DO NOT TOUCH bp->b_blkno!!! */
	raid_addr = blocknum;
	
	num_blocks = bp->b_bcount >> raidPtr->logBytesPerSector;
	pb = (bp->b_bcount&raidPtr->sectorMask) ? 1 : 0;
	sum = raid_addr + num_blocks + pb;
	if (1 || rf_debugKernelAccess) {
		db1_printf(("raid_addr=%d sum=%d num_blocks=%d(+%d) (%d)\n", 
			    (int)raid_addr, (int)sum,(int)num_blocks,
			    (int)pb,(int)bp->b_resid));
	}
	

	if ((sum > raidPtr->totalSectors) || (sum < raid_addr)
			|| (sum < num_blocks) || (sum < pb))
	{
		bp->b_error = ENOSPC;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return(bp->b_error);
	}

	/*
	 * XXX rf_DoAccess() should do this, not just DoAccessKernel()
	 */

	if (bp->b_bcount & raidPtr->sectorMask) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return(bp->b_error);
	}
	db1_printf(("Calling DoAccess..\n"));

	/* don't ever condition on bp->b_flags & B_WRITE.  
	   always condition on B_READ instead */
	retcode = rf_DoAccess(raidPtr, (bp->b_flags & B_READ) ? 
			      RF_IO_TYPE_READ : RF_IO_TYPE_WRITE,
			      0, raid_addr, num_blocks, bp->b_un.b_addr, 
			      bp, NULL, NULL, RF_DAG_NONBLOCKING_IO|flags, 
			      NULL, cbFunc, cbArg);
#if 0
	db1_printf(("After call to DoAccess: 0x%x 0x%x %d\n",bp,
	       bp->b_data,(int)bp->b_resid));
#endif
	return(retcode);
}

/* invoke an I/O from kernel mode.  Disk queue should be locked upon entry */

int rf_DispatchKernelIO(queue, req)
	RF_DiskQueue_t      *queue;
	RF_DiskQueueData_t  *req;
{
	int op = (req->type == RF_IO_TYPE_READ) ? B_READ : B_WRITE;
	struct buf *bp;
	struct raidbuf *raidbp=NULL;
	struct raid_softc *rs;
	int unit;
	
	/* XXX along with the vnode, we also need the softc associated with
	   this device.. */
	
	req->queue = queue;
	
	unit = queue->raidPtr->raidid;

 	db1_printf(("DispatchKernelIO unit: %d\n",unit)); 

     	if (unit >= numraid) {
		printf("Invalid unit number: %d %d\n",unit,numraid);
		panic("Invalid Unit number in rf_DispatchKernelIO\n");
	}

	rs = &raid_softc[unit];

	/* XXX is this the right place? */
	disk_busy(&rs->sc_dkdev); 

	bp = req->bp;

	/* 
	   XXX when there is a physical disk failure, someone is passing 
	   us a buffer that contains old stuff!!  Attempt to deal with
	   this problem without taking a performance hit...
	   (not sure where the real bug is.  It's buried in RAIDframe
	   somewhere) :-(  GO )
	 */

	if (bp->b_flags & B_ERROR) {
		bp->b_flags &= ~B_ERROR;
	}
	if (bp->b_error!=0) {
		bp->b_error = 0;
	}

	raidbp = RAIDGETBUF(rs);

	raidbp->rf_flags = 0; /* XXX not really used anywhere... */

	/*
	 * context for raidiodone
	 */
	raidbp->rf_obp = bp;
	raidbp->req = req;

	switch (req->type) {
	case RF_IO_TYPE_NOP:   /* used primarily to unlock a locked queue */
		/* 
		Dprintf2("rf_DispatchKernelIO: NOP to r %d c %d\n",
			 queue->row, queue->col);
			 */
		/* XXX need to do something extra here.. */
		/* I'm leaving this in, as I've never actually seen it 
		   used, and I'd like folks to report it... GO */
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
		/* acc wouldn't have been let in if there were any 
		   pending reqs at any other priority */
		queue->curPriority = req->priority;
		/* 
		Dprintf3("rf_DispatchKernelIO: %c to row %d col %d\n", 
			 req->type, queue->row, queue->col);
			 */

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
	return(0);
}

/* this is the callback function associated with a I/O invoked from 
   kernel code.
 */
static void KernelWakeupFunc(vbp)
  struct buf  *vbp;
{
  RF_DiskQueueData_t *req = NULL;
  RF_DiskQueue_t *queue; 
  struct raidbuf *raidbp = (struct raidbuf *)vbp;
  struct buf *bp;
  struct raid_softc *rs;
  int unit;
  register int s;

  s=splbio(); /* XXX */
  db1_printf(("recovering the request queue:\n"));
  req = raidbp->req;

  bp = raidbp->rf_obp;
#if 0
  db1_printf(("bp=0x%x\n",bp));
#endif

  queue = (RF_DiskQueue_t *) req->queue;

  if (raidbp->rf_buf.b_flags & B_ERROR) {
#if 0
	  printf("Setting bp->b_flags!!! %d\n",raidbp->rf_buf.b_error);
#endif
	  bp->b_flags |= B_ERROR;
	  bp->b_error = raidbp->rf_buf.b_error ? 
		  raidbp->rf_buf.b_error : EIO;
  }

#if 0
  db1_printf(("raidbp->rf_buf.b_bcount=%d\n",(int)raidbp->rf_buf.b_bcount));
  db1_printf(("raidbp->rf_buf.b_bufsize=%d\n",(int)raidbp->rf_buf.b_bufsize));
  db1_printf(("raidbp->rf_buf.b_resid=%d\n",(int)raidbp->rf_buf.b_resid));
  db1_printf(("raidbp->rf_buf.b_data=0x%x\n",raidbp->rf_buf.b_data));
#endif

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

  bp->b_bcount = raidbp->rf_buf.b_bcount;/* XXXX ?? */

  unit = queue->raidPtr->raidid; /* *Much* simpler :-> */


  /* XXX Ok, let's get aggressive... If B_ERROR is set, let's go ballistic, 
     and mark the component as hosed... */
#if 1
  if (bp->b_flags&B_ERROR) {
	  /* Mark the disk as dead */
	  /* but only mark it once... */
	  if (queue->raidPtr->Disks[queue->row][queue->col].status == 
	      rf_ds_optimal) {
		  printf("raid%d: IO Error.  Marking %s as failed.\n", 
			 unit, queue->raidPtr->Disks[queue->row][queue->col].devname );
		  queue->raidPtr->Disks[queue->row][queue->col].status = 
			  rf_ds_failed;
		  queue->raidPtr->status[queue->row] = rf_rs_degraded;
		  queue->raidPtr->numFailures++;
	  } else {  /* Disk is already dead... */
		  /*  printf("Disk already marked as dead!\n"); */
	  }
	  
  }
#endif

  rs = &raid_softc[unit];
  RAIDPUTBUF(rs,raidbp);


  if (bp->b_resid==0) {
	  db1_printf(("Disk is no longer busy for this buffer... %d %ld %ld\n",
		 unit, bp->b_resid, bp->b_bcount));
	  /* XXX is this the right place for a disk_unbusy()??!??!?!? */
	  disk_unbusy(&rs->sc_dkdev, (bp->b_bcount - bp->b_resid)); 
  } else {
	  db1_printf(("b_resid is still %ld\n",bp->b_resid));
  }

  rf_DiskIOComplete(queue, req, (bp->b_flags & B_ERROR) ? 1 : 0);
  (req->CompleteFunc)(req->argument, (bp->b_flags & B_ERROR) ? 1 : 0);
  /*   printf("Exiting KernelWakeupFunc\n"); */

  splx(s); /* XXX */
}



/*
 * initialize a buf structure for doing an I/O in the kernel.
 */
static void InitBP(
  struct buf         *bp,
  struct vnode       *b_vp,
  unsigned            rw_flag,
  dev_t               dev,
  RF_SectorNum_t      startSect,
  RF_SectorCount_t    numSect,
  caddr_t             buf,
  void              (*cbFunc)(struct buf *),
  void               *cbArg,
  int                 logBytesPerSector,
  struct proc        *b_proc)
{
	/*   bp->b_flags       = B_PHYS | rw_flag; */
	bp->b_flags       = B_CALL | rw_flag; /* XXX need B_PHYS here too??? */
	bp->b_bcount      = numSect << logBytesPerSector; 
	bp->b_bufsize     = bp->b_bcount;
	bp->b_error       = 0;
	bp->b_dev         = dev;
	db1_printf(("bp->b_dev is %d\n", dev));
	bp->b_un.b_addr   = buf;  
#if 0
	db1_printf(("bp->b_data=0x%x\n",bp->b_data));
#endif

	bp->b_blkno       = startSect;
	bp->b_resid       = bp->b_bcount; /* XXX is this right!??!?!! */
	db1_printf(("b_bcount is: %d\n",(int)bp->b_bcount));
	if (bp->b_bcount == 0) {
		panic("bp->b_bcount is zero in InitBP!!\n");
	}
	bp->b_proc        = b_proc;
	bp->b_iodone      = cbFunc;
	bp->b_vp          = b_vp; 
	
}
#endif /* KERNEL */

/* Extras... */

unsigned int rpcc()
{
	/* XXX no clue what this is supposed to do.. my guess is 
	   that it's supposed to read the CPU cycle counter... */
	/* 	db1_printf("this is supposed to do something useful too!??\n"); */
	return(0);
}

#if 0
int rf_GetSpareTableFromDaemon(req)
  RF_SparetWait_t  *req;
{
  int retcode=1;
  printf("This is supposed to do something useful!!\n"); /* XXX */
  
  return(retcode);

}
#endif

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
	lp->d_nsectors = 1024 * (1024 / raidPtr->bytesPerSector);
	lp->d_ntracks = 1;
	lp->d_ncylinders = raidPtr->totalSectors / lp->d_nsectors;
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
	    rs->sc_dkdev.dk_label, rs->sc_dkdev.dk_cpulabel);
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
			    "the size of raid (%d)\n", rs->sc_xname,
			    lp->d_secperunit, rs->sc_size);
		for (i = 0; i < lp->d_npartitions; i++) {
			pp = &lp->d_partitions[i];
			if (pp->p_offset + pp->p_size > rs->sc_size)
				printf("WARNING: %s: end of partition `%c' "
				    "exceeds the size of raid (%d)\n",
				    rs->sc_xname, 'a' + i, rs->sc_size);
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
			printf("RAIDframe: vn_open returned %d\n",error);
#endif
		return (error);
	}
	vp = nd.ni_vp;
	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp, 0);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}
	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
		VOP_UNLOCK(vp, 0);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (error);
	}
	/* XXX: eventually we should handle VREG, too. */
	if (va.va_type != VBLK) {
		VOP_UNLOCK(vp, 0);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
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
	int error;

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
