/*	$OpenBSD: subr_disk.c,v 1.47 2007/06/05 00:38:23 deraadt Exp $	*/
/*	$NetBSD: subr_disk.c,v 1.17 1996/03/16 23:17:08 christos Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/time.h>
#include <sys/disklabel.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/disk.h>
#include <sys/reboot.h>
#include <sys/dkio.h>
#include <sys/dkstat.h>		/* XXX */
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

#include <dev/rndvar.h>
#include <dev/cons.h>

/*
 * A global list of all disks attached to the system.  May grow or
 * shrink over time.
 */
struct	disklist_head disklist;	/* TAILQ_HEAD */
int	disk_count;		/* number of drives in global disklist */
int	disk_change;		/* set if a disk has been attached/detached
				 * since last we looked at this variable. This
				 * is reset by hw_sysctl()
				 */

/*
 * Seek sort for disks.  We depend on the driver which calls us using b_resid
 * as the current cylinder number.
 *
 * The argument ap structure holds a b_actf activity chain pointer on which we
 * keep two queues, sorted in ascending cylinder order.  The first queue holds
 * those requests which are positioned after the current cylinder (in the first
 * request); the second holds requests which came in after their cylinder number
 * was passed.  Thus we implement a one way scan, retracting after reaching the
 * end of the drive to the first request on the second queue, at which time it
 * becomes the first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

void
disksort(struct buf *ap, struct buf *bp)
{
	struct buf *bq;

	/* If the queue is empty, then it's easy. */
	if (ap->b_actf == NULL) {
		bp->b_actf = NULL;
		ap->b_actf = bp;
		return;
	}

	/*
	 * If we lie after the first (currently active) request, then we
	 * must locate the second request list and add ourselves to it.
	 */
	bq = ap->b_actf;
	if (bp->b_cylinder < bq->b_cylinder) {
		while (bq->b_actf) {
			/*
			 * Check for an ``inversion'' in the normally ascending
			 * cylinder numbers, indicating the start of the second
			 * request list.
			 */
			if (bq->b_actf->b_cylinder < bq->b_cylinder) {
				/*
				 * Search the second request list for the first
				 * request at a larger cylinder number.  We go
				 * before that; if there is no such request, we
				 * go at end.
				 */
				do {
					if (bp->b_cylinder <
					    bq->b_actf->b_cylinder)
						goto insert;
					if (bp->b_cylinder ==
					    bq->b_actf->b_cylinder &&
					    bp->b_blkno < bq->b_actf->b_blkno)
						goto insert;
					bq = bq->b_actf;
				} while (bq->b_actf);
				goto insert;		/* after last */
			}
			bq = bq->b_actf;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}
	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while (bq->b_actf) {
		/*
		 * We want to go after the current request if there is an
		 * inversion after it (i.e. it is the end of the first
		 * request list), or if the next request is a larger cylinder
		 * than our request.
		 */
		if (bq->b_actf->b_cylinder < bq->b_cylinder ||
		    bp->b_cylinder < bq->b_actf->b_cylinder ||
		    (bp->b_cylinder == bq->b_actf->b_cylinder &&
		    bp->b_blkno < bq->b_actf->b_blkno))
			goto insert;
		bq = bq->b_actf;
	}
	/*
	 * Neither a second list nor a larger request... we go at the end of
	 * the first list, which is the same as the end of the whole schebang.
	 */
insert:	bp->b_actf = bq->b_actf;
	bq->b_actf = bp;
}

/*
 * Compute checksum for disk label.
 */
u_int
dkcksum(struct disklabel *lp)
{
	u_int16_t *start, *end;
	u_int16_t sum = 0;

	start = (u_int16_t *)lp;
	end = (u_int16_t *)&lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

/*
 * Convert an on-disk disklabel to a kernel disklabel, converting versions
 * as required and applying constraints that kernel disklabels are guaranteed
 * to satisfy.
 */
void
disklabeltokernlabel(struct disklabel *lp)
{
	struct __partitionv0 *v0pp = (struct __partitionv0 *)lp->d_partitions;
	struct partition *pp = lp->d_partitions;
	int i, oversion = lp->d_version;

	if (oversion == 0) {
		lp->d_version = 1;
		lp->d_secperunith = 0;
	}

	for (i = 0; i < MAXPARTITIONS; i++, pp++, v0pp++) {
		if (oversion == 0) {
			pp->p_fragblock = DISKLABELV1_FFS_FRAGBLOCK(v0pp->
			    p_fsize, v0pp->p_frag);
			pp->p_offseth = 0;
			pp->p_sizeh = 0;
		}

		/* In a V1 label no partition extends past DL_GETSIZE(lp) - 1. */
		if (DL_GETPOFFSET(pp) > DL_GETDSIZE(lp)) {
			pp->p_fstype = FS_UNUSED;
			DL_SETPSIZE(pp, 0);
			DL_SETPOFFSET(pp, 0);
#ifdef notyet  /* older broken sparc/sparc64 fake cyl-based disklabels fool this */
		} else if (DL_GETPOFFSET(pp) + DL_GETPSIZE(pp) > DL_GETDSIZE(lp)) {
			daddr64_t sz;

			printf("%lld %lld %lld\n",
			    DL_GETPOFFSET(pp), DL_GETPSIZE(pp),
			    DL_GETPOFFSET(pp) + DL_GETPSIZE(pp),
			    DL_GETDSIZE(lp));
			pp->p_fstype = FS_UNUSED;
			sz = DL_GETDSIZE(lp) - DL_GETPOFFSET(pp);
			DL_SETPSIZE(pp, sz);
#endif
		}
	}
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf
 * if pri is LOG_PRINTF, otherwise it uses log at the specified priority.
 * The message should be completed (with at least a newline) with printf
 * or addlog, respectively.  There is no trailing space.
 */
void
diskerr(struct buf *bp, char *dname, char *what, int pri, int blkdone,
    struct disklabel *lp)
{
	int unit = DISKUNIT(bp->b_dev), part = DISKPART(bp->b_dev);
	int (*pr)(const char *, ...);
	char partname = 'a' + part;
	int sn;

	if (pri != LOG_PRINTF) {
		static const char fmt[] = "";
		log(pri, fmt);
		pr = addlog;
	} else
		pr = printf;
	(*pr)("%s%d%c: %s %sing fsbn ", dname, unit, partname, what,
	    bp->b_flags & B_READ ? "read" : "writ");
	sn = bp->b_blkno;
	if (bp->b_bcount <= DEV_BSIZE)
		(*pr)("%d", sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			(*pr)("%d of ", sn);
		}
		(*pr)("%d-%d", bp->b_blkno,
		    bp->b_blkno + (bp->b_bcount - 1) / DEV_BSIZE);
	}
	if (lp && (blkdone >= 0 || bp->b_bcount <= lp->d_secsize)) {
		sn += DL_GETPOFFSET(&lp->d_partitions[part]);
		(*pr)(" (%s%d bn %d; cn %d", dname, unit, sn,
		    sn / lp->d_secpercyl);
		sn %= lp->d_secpercyl;
		(*pr)(" tn %d sn %d)", sn / lp->d_nsectors, sn % lp->d_nsectors);
	}
}

/*
 * Initialize the disklist.  Called by main() before autoconfiguration.
 */
void
disk_init(void)
{

	TAILQ_INIT(&disklist);
	disk_count = disk_change = 0;
}

int
disk_construct(struct disk *diskp, char *lockname)
{
	rw_init(&diskp->dk_lock, lockname);
	
	diskp->dk_flags |= DKF_CONSTRUCTED;
	    
	return (0);
}

/*
 * Attach a disk.
 */
void
disk_attach(struct disk *diskp)
{

	if (!ISSET(diskp->dk_flags, DKF_CONSTRUCTED))
		disk_construct(diskp, diskp->dk_name);

	/*
	 * Allocate and initialize the disklabel structures.  Note that
	 * it's not safe to sleep here, since we're probably going to be
	 * called during autoconfiguration.
	 */
	diskp->dk_label = malloc(sizeof(struct disklabel), M_DEVBUF, M_NOWAIT);
	diskp->dk_cpulabel = malloc(sizeof(struct cpu_disklabel), M_DEVBUF,
	    M_NOWAIT);
	if ((diskp->dk_label == NULL) || (diskp->dk_cpulabel == NULL))
		panic("disk_attach: can't allocate storage for disklabel");

	bzero(diskp->dk_label, sizeof(struct disklabel));
	bzero(diskp->dk_cpulabel, sizeof(struct cpu_disklabel));

	/*
	 * Set the attached timestamp.
	 */
	microuptime(&diskp->dk_attachtime);

	/*
	 * Link into the disklist.
	 */
	TAILQ_INSERT_TAIL(&disklist, diskp, dk_link);
	++disk_count;
	disk_change = 1;
}

/*
 * Detach a disk.
 */
void
disk_detach(struct disk *diskp)
{

	/*
	 * Free the space used by the disklabel structures.
	 */
	free(diskp->dk_label, M_DEVBUF);
	free(diskp->dk_cpulabel, M_DEVBUF);

	/*
	 * Remove from the disklist.
	 */
	TAILQ_REMOVE(&disklist, diskp, dk_link);
	disk_change = 1;
	if (--disk_count < 0)
		panic("disk_detach: disk_count < 0");
}

/*
 * Increment a disk's busy counter.  If the counter is going from
 * 0 to 1, set the timestamp.
 */
void
disk_busy(struct disk *diskp)
{

	/*
	 * XXX We'd like to use something as accurate as microtime(),
	 * but that doesn't depend on the system TOD clock.
	 */
	if (diskp->dk_busy++ == 0) {
		microuptime(&diskp->dk_timestamp);
	}
}

/*
 * Decrement a disk's busy counter, increment the byte count, total busy
 * time, and reset the timestamp.
 */
void
disk_unbusy(struct disk *diskp, long bcount, int read)
{
	struct timeval dv_time, diff_time;

	if (diskp->dk_busy-- == 0)
		printf("disk_unbusy: %s: dk_busy < 0\n", diskp->dk_name);

	microuptime(&dv_time);

	timersub(&dv_time, &diskp->dk_timestamp, &diff_time);
	timeradd(&diskp->dk_time, &diff_time, &diskp->dk_time);

	diskp->dk_timestamp = dv_time;
	if (bcount > 0) {
		if (read) {
			diskp->dk_rbytes += bcount;
			diskp->dk_rxfer++;
		} else {
			diskp->dk_wbytes += bcount;
			diskp->dk_wxfer++;
		}
	} else
		diskp->dk_seek++;

	add_disk_randomness(bcount ^ diff_time.tv_usec);
}

int
disk_lock(struct disk *dk)
{
	int error;

	error = rw_enter(&dk->dk_lock, RW_WRITE|RW_INTR);

	return (error);
}

void
disk_unlock(struct disk *dk)
{
	rw_exit(&dk->dk_lock);
}

int
dk_mountroot(void)
{
	dev_t rawdev, rrootdev;
	int part = DISKPART(rootdev);
	int (*mountrootfn)(void);
	struct disklabel dl;
	int error;

	rrootdev = blktochr(rootdev);
	rawdev = MAKEDISKDEV(major(rrootdev), DISKUNIT(rootdev), RAW_PART);
#ifdef DEBUG
	printf("rootdev=0x%x rrootdev=0x%x rawdev=0x%x\n", rootdev,
	    rrootdev, rawdev);
#endif

	/*
	 * open device, ioctl for the disklabel, and close it.
	 */
	error = (cdevsw[major(rrootdev)].d_open)(rawdev, FREAD,
	    S_IFCHR, curproc);
	if (error)
		panic("cannot open disk, 0x%x/0x%x, error %d",
		    rootdev, rrootdev, error);
	error = (cdevsw[major(rrootdev)].d_ioctl)(rawdev, DIOCGDINFO,
	    (caddr_t)&dl, FREAD, curproc);
	if (error)
		panic("cannot read disk label, 0x%x/0x%x, error %d",
		    rootdev, rrootdev, error);
	(void) (cdevsw[major(rrootdev)].d_close)(rawdev, FREAD,
	    S_IFCHR, curproc);

	if (DL_GETPSIZE(&dl.d_partitions[part]) == 0)
		panic("root filesystem has size 0");
	switch (dl.d_partitions[part].p_fstype) {
#ifdef EXT2FS
	case FS_EXT2FS:
		{
		extern int ext2fs_mountroot(void);
		mountrootfn = ext2fs_mountroot;
		}
		break;
#endif
#ifdef FFS
	case FS_BSDFFS:
		{
		extern int ffs_mountroot(void);
		mountrootfn = ffs_mountroot;
		}
		break;
#endif
#ifdef CD9660
	case FS_ISO9660:
		{
		extern int cd9660_mountroot(void);
		mountrootfn = cd9660_mountroot;
		}
		break;
#endif
	default:
#ifdef FFS
		{ 
		extern int ffs_mountroot(void);

		printf("filesystem type %d not known.. assuming ffs\n",
		    dl.d_partitions[part].p_fstype);
		mountrootfn = ffs_mountroot;
		}
#else
		panic("disk 0x%x/0x%x filesystem type %d not known", 
		    rootdev, rrootdev, dl.d_partitions[part].p_fstype);
#endif
	}
	return (*mountrootfn)();
}

struct bufq *
bufq_default_alloc(void)
{
	struct bufq_default *bq;

	bq = malloc(sizeof(*bq), M_DEVBUF, M_NOWAIT);
	if (bq == NULL)
		panic("bufq_default_alloc: no memory");

	memset(bq, 0, sizeof(*bq));
	bq->bufq.bufq_free = bufq_default_free;
	bq->bufq.bufq_add = bufq_default_add;
	bq->bufq.bufq_get = bufq_default_get;

	return ((struct bufq *)bq);
}

void
bufq_default_free(struct bufq *bq)
{
	free(bq, M_DEVBUF);
}

void
bufq_default_add(struct bufq *bq, struct buf *bp)
{
	struct bufq_default *bufq = (struct bufq_default *)bq;
	struct proc *p = bp->b_proc;
	struct buf *head;

	if (p == NULL || p->p_nice < NZERO)
		head = &bufq->bufq_head[0];
	else if (p->p_nice == NZERO)
		head = &bufq->bufq_head[1];
	else
		head = &bufq->bufq_head[2];

	disksort(head, bp);
}

struct buf *
bufq_default_get(struct bufq *bq)
{
	struct bufq_default *bufq = (struct bufq_default *)bq;
	struct buf *bp, *head;
	int i;

	for (i = 0; i < 3; i++) {
		head = &bufq->bufq_head[i];
		if ((bp = head->b_actf))
			break;
	}
	if (bp == NULL)
		return (NULL);
	head->b_actf = bp->b_actf;
	return (bp);
}

#ifdef RAMDISK_HOOKS
static struct device fakerdrootdev = { DV_DISK, {}, NULL, 0, "rd0", NULL };
#endif

struct device *
getdisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;

	if ((dv = parsedisk(str, len, defpart, devp)) == NULL) {
		printf("use one of: exit");
#ifdef RAMDISK_HOOKS
		printf(" %s[a-p]", fakerdrootdev.dv_xname);
#endif
		TAILQ_FOREACH(dv, &alldevs, dv_list) {
			if (dv->dv_class == DV_DISK)
				printf(" %s[a-p]", dv->dv_xname);
#if defined(NFSCLIENT)
			if (dv->dv_class == DV_IFNET)
				printf(" %s", dv->dv_xname);
#endif
		}
		printf("\n");
	}
	return (dv);
}

struct device *
parsedisk(char *str, int len, int defpart, dev_t *devp)
{
	struct device *dv;
	char c;
	int majdev, part;

	if (len == 0)
		return (NULL);
	c = str[len-1];
	if (c >= 'a' && (c - 'a') < MAXPARTITIONS) {
		part = c - 'a';
		len -= 1;
	} else
		part = defpart;

#ifdef RAMDISK_HOOKS
	if (strcmp(str, fakerdrootdev.dv_xname) == 0) {
		dv = &fakerdrootdev;
		goto gotdisk;
	}
#endif

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class == DV_DISK &&
		    strncmp(str, dv->dv_xname, len) == 0 &&
		    dv->dv_xname[len] == '\0') {
#ifdef RAMDISK_HOOKS
gotdisk:
#endif
			majdev = findblkmajor(dv);
			if (majdev < 0)
				panic("parsedisk");
			*devp = MAKEDISKDEV(majdev, dv->dv_unit, part);
			break;
		}
#if defined(NFSCLIENT)
		if (dv->dv_class == DV_IFNET &&
		    strncmp(str, dv->dv_xname, len) == 0 &&
		    dv->dv_xname[len] == '\0') {
			*devp = NODEV;
			break;
		}
#endif
	}

	return (dv);
}

void
setroot(struct device *bootdv, int part, int exitflags)
{
	int majdev, unit, len, s;
	struct swdevt *swp;
	struct device *rootdv, *dv;
	dev_t nrootdev, nswapdev = NODEV, temp = NODEV;
	char buf[128];
#if defined(NFSCLIENT)
	extern char *nfsbootdevname;
#endif

	if (boothowto & RB_DFLTROOT)
		return;

#ifdef RAMDISK_HOOKS
	bootdv = &fakerdrootdev;
	mountroot = NULL;
	part = 0;
#endif

	/*
	 * If `swap generic' and we couldn't determine boot device,
	 * ask the user.
	 */
	if (mountroot == NULL && bootdv == NULL)
		boothowto |= RB_ASKNAME;
	if (boothowto & RB_ASKNAME) {
		while (1) {
			printf("root device");
			if (bootdv != NULL) {
				printf(" (default %s", bootdv->dv_xname);
				if (bootdv->dv_class == DV_DISK)
					printf("%c", 'a' + part);
				printf(")");
			}
			printf(": ");
			s = splhigh();
			cnpollc(TRUE);
			len = getsn(buf, sizeof(buf));
			cnpollc(FALSE);
			splx(s);
			if (strcmp(buf, "exit") == 0)
				boot(exitflags);
			if (len == 0 && bootdv != NULL) {
				strlcpy(buf, bootdv->dv_xname, sizeof buf);
				len = strlen(buf);
			}
			if (len > 0 && buf[len - 1] == '*') {
				buf[--len] = '\0';
				dv = getdisk(buf, len, part, &nrootdev);
				if (dv != NULL) {
					rootdv = dv;
					nswapdev = nrootdev;
					goto gotswap;
				}
			}
			dv = getdisk(buf, len, part, &nrootdev);
			if (dv != NULL) {
				rootdv = dv;
				break;
			}
		}

		if (rootdv->dv_class == DV_IFNET)
			goto gotswap;

		/* try to build swap device out of new root device */
		while (1) {
			printf("swap device");
			if (rootdv != NULL)
				printf(" (default %s%s)", rootdv->dv_xname,
				    rootdv->dv_class == DV_DISK ? "b" : "");
			printf(": ");
			s = splhigh();
			cnpollc(TRUE);
			len = getsn(buf, sizeof(buf));
			cnpollc(FALSE);
			splx(s);
			if (strcmp(buf, "exit") == 0)
				boot(exitflags);
			if (len == 0 && rootdv != NULL) {
				switch (rootdv->dv_class) {
				case DV_IFNET:
					nswapdev = NODEV;
					break;
				case DV_DISK:
					nswapdev = MAKEDISKDEV(major(nrootdev),
					    DISKUNIT(nrootdev), 1);
					if (nswapdev == nrootdev)
						continue;
					break;
				default:
					break;
				}
				break;
			}
			dv = getdisk(buf, len, 1, &nswapdev);
			if (dv) {
				if (dv->dv_class == DV_IFNET)
					nswapdev = NODEV;
				if (nswapdev == nrootdev)
					continue;
				break;
			}
		}
gotswap:
		rootdev = nrootdev;
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		swdevt[1].sw_dev = NODEV;
#if defined(NFSCLIENT)
	} else if (mountroot == nfs_mountroot) {
		rootdv = bootdv;
		rootdev = dumpdev = swapdev = NODEV;
#endif
	} else if (mountroot == NULL) {
		/* `swap generic': Use the device the ROM told us to use */
		rootdv = bootdv;
		majdev = findblkmajor(rootdv);
		if (majdev >= 0) {
			/*
			 * Root and swap are on the disk.
			 * Assume swap is on partition b.
			 */
			rootdev = MAKEDISKDEV(majdev, rootdv->dv_unit, part);
			nswapdev = MAKEDISKDEV(majdev, rootdv->dv_unit, 1);
		} else {
			/*
			 * Root and swap are on a net.
			 */
			nswapdev = NODEV;
		}
		dumpdev = nswapdev;
		swdevt[0].sw_dev = nswapdev;
		/* swdevt[1].sw_dev = NODEV; */
	} else {
		/* Completely pre-configured, but we want rootdv .. */
		majdev = major(rootdev);
		if (findblkname(majdev) == NULL)
			return;
		unit = DISKUNIT(rootdev);
		part = DISKPART(rootdev);
		snprintf(buf, sizeof buf, "%s%d%c",
		    findblkname(majdev), unit, 'a' + part);
		rootdv = parsedisk(buf, strlen(buf), 0, &nrootdev);
	}

	switch (rootdv->dv_class) {
#if defined(NFSCLIENT)
	case DV_IFNET:
		mountroot = nfs_mountroot;
		nfsbootdevname = rootdv->dv_xname;
		return;
#endif
	case DV_DISK:
		mountroot = dk_mountroot;
		part = DISKPART(rootdev);
		break;
	default:
		printf("can't figure root, hope your kernel is right\n");
		return;
	}

	printf("root on %s%c", rootdv->dv_xname, 'a' + part);

	/*
	 * Make the swap partition on the root drive the primary swap.
	 */
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		if (major(rootdev) == major(swp->sw_dev) &&
		    DISKUNIT(rootdev) == DISKUNIT(swp->sw_dev)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev != NODEV) {
		/*
		 * If dumpdev was the same as the old primary swap device,
		 * move it to the new primary swap device.
		 */
		if (temp == dumpdev)
			dumpdev = swdevt[0].sw_dev;
	}
	if (swdevt[0].sw_dev != NODEV)
		printf(" swap on %s%d%c", findblkname(major(swdevt[0].sw_dev)),
		    DISKUNIT(swdevt[0].sw_dev),
		    'a' + DISKPART(swdevt[0].sw_dev));
	if (dumpdev != NODEV)
		printf(" dump on %s%d%c", findblkname(major(dumpdev)),
		    DISKUNIT(dumpdev), 'a' + DISKPART(dumpdev));
	printf("\n");
}

extern struct nam2blk nam2blk[];

int
findblkmajor(struct device *dv)
{
	char *name = dv->dv_xname;
	int i;

	for (i = 0; nam2blk[i].name; i++)
		if (!strncmp(name, nam2blk[i].name, strlen(nam2blk[i].name)))
			return (nam2blk[i].maj);
	return (-1);
}

char *
findblkname(int maj)
{
	int i;

	for (i = 0; nam2blk[i].name; i++)
		if (nam2blk[i].maj == maj)
			return (nam2blk[i].name);
	return (NULL);
}
