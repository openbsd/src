/*	$OpenBSD: vnd.c,v 1.53 2005/01/05 06:38:15 tedu Exp $	*/
/*	$NetBSD: vnd.c,v 1.26 1996/03/30 23:06:11 christos Exp $	*/

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
 * from: Utah $Hdr: vn.c 1.13 94/04/02$
 *
 *	@(#)vn.c	8.6 (Berkeley) 4/1/94
 */

/*
 * Vnode disk driver.
 *
 * Block/character interface to a vnode.  Allows one to treat a file
 * as a disk (e.g. build a filesystem in it, mount it, etc.).
 *
 * NOTE 1: This uses either the VOP_BMAP/VOP_STRATEGY interface to the
 * vnode or simple VOP_READ/VOP_WRITE.  The former is suitable for swapping
 * as it doesn't distort the local buffer cache.  The latter is good for
 * building disk images as it keeps the cache consistent after the block
 * device is closed.
 *
 * NOTE 2: There is a security issue involved with this driver.
 * Once mounted all access to the contents of the "mapped" file via
 * the special file is controlled by the permissions on the special
 * file, the protection of the mapped file is ignored (effectively,
 * by using root credentials in all transactions).
 *
 * NOTE 3: Doesn't interact with leases, should it?
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>

#include <crypto/blf.h>

#include <miscfs/specfs/specdev.h>

#include <dev/vndioctl.h>

#ifdef DEBUG
int dovndcluster = 1;
int vnddebug = 0x00;
#define	VDB_FOLLOW	0x01
#define	VDB_INIT	0x02
#define	VDB_IO		0x04
#endif

#define	b_cylin	b_resid

/*
 * vndunit is a bit weird.  have to reconstitute the dev_t for
 * DISKUNIT(), but with the minor masked off.
 */
#define	vndunit(x)	DISKUNIT(makedev(major(x), minor(x) & 0x7ff))
#define	vndsimple(x)	(minor(x) & 0x800)

/* same as MAKEDISKDEV, preserving the vndsimple() property */
#define	VNDLABELDEV(dev)	\
	makedev(major(dev), DISKMINOR(vndunit(dev), RAW_PART) | \
	    (vndsimple(dev) ? 0x800 : 0))

struct vndbuf {
	struct buf	vb_buf;
	struct buf	*vb_obp;
};

#define	getvndbuf()	\
	((struct vndbuf *)malloc(sizeof(struct vndbuf), M_DEVBUF, M_WAITOK))
#define	putvndbuf(vbp)	\
	free((caddr_t)(vbp), M_DEVBUF)

struct vnd_softc {
	struct device	 sc_dev;
	struct disk	 sc_dk;

	char		 sc_file[VNDNLEN];	/* file we're covering */
	int		 sc_flags;		/* flags */
	size_t		 sc_size;		/* size of vnd in blocks */
	struct vnode	*sc_vp;			/* vnode */
	struct ucred	*sc_cred;		/* credentials */
	int		 sc_maxactive;		/* max # of active requests */
	struct buf	 sc_tab;		/* transfer queue */
	void		*sc_keyctx;		/* key context */
};

/* sc_flags */
#define	VNF_ALIVE	0x0001
#define	VNF_INITED	0x0002
#define	VNF_WANTED	0x0040
#define	VNF_LOCKED	0x0080
#define	VNF_LABELLING	0x0100
#define	VNF_WLABEL	0x0200
#define	VNF_HAVELABEL	0x0400
#define	VNF_BUSY	0x0800
#define	VNF_SIMPLE	0x1000

struct vnd_softc *vnd_softc;
int numvnd = 0;

struct dkdriver vnddkdriver = { vndstrategy };

/* called by main() at boot time */
void	vndattach(int);

void	vndclear(struct vnd_softc *);
void	vndstart(struct vnd_softc *);
int	vndsetcred(struct vnd_softc *, struct ucred *);
void	vndthrottle(struct vnd_softc *, struct vnode *);
void	vndiodone(struct buf *);
void	vndshutdown(void);
void	vndgetdisklabel(dev_t, struct vnd_softc *);
void	vndencrypt(struct vnd_softc *, caddr_t, size_t, daddr_t, int);

int	vndlock(struct vnd_softc *);
void	vndunlock(struct vnd_softc *);

void
vndencrypt(vnd, addr, size, off, encrypt)
	struct vnd_softc *vnd;
	caddr_t addr;
	size_t size;
	daddr_t off;
	int encrypt;
{
	int i, bsize;
	u_char iv[8];

	bsize = dbtob(1);
	for (i = 0; i < size/bsize; i++) {
		bzero(iv, sizeof(iv));
		bcopy((u_char *)&off, iv, sizeof(off));
		blf_ecb_encrypt(vnd->sc_keyctx, iv, sizeof(iv));
		if (encrypt)
			blf_cbc_encrypt(vnd->sc_keyctx, iv, addr, bsize);
		else
			blf_cbc_decrypt(vnd->sc_keyctx, iv, addr, bsize);

		addr += bsize;
		off++;
	}
}

void
vndattach(num)
	int num;
{
	char *mem;
	u_long size;

	if (num <= 0)
		return;
	size = num * sizeof(struct vnd_softc);
	mem = malloc(size, M_DEVBUF, M_NOWAIT);
	if (mem == NULL) {
		printf("WARNING: no memory for vnode disks\n");
		return;
	}
	bzero(mem, size);
	vnd_softc = (struct vnd_softc *)mem;
	numvnd = num;
}

int
vndopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = vndunit(dev);
	struct vnd_softc *sc;
	int error = 0, part, pmask;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndopen(%x, %x, %x, %p)\n", dev, flags, mode, p);
#endif
	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((error = vndlock(sc)) != 0)
		return (error);

	if ((sc->sc_flags & VNF_INITED) &&
	    (sc->sc_flags & VNF_HAVELABEL) == 0) {
		sc->sc_flags |= VNF_HAVELABEL;
		vndgetdisklabel(dev, sc);
	}

	part = DISKPART(dev);
	pmask = 1 << part;

	/*
	 * If any partition is open, all succeeding openings must be of the
	 * same type.
	 */
	if (sc->sc_dk.dk_openmask) {
		if (((sc->sc_flags & VNF_SIMPLE) != 0) !=
		    (vndsimple(dev) != 0)) {
			error = EBUSY;
			goto bad;
		}
	} else if (vndsimple(dev))
		sc->sc_flags |= VNF_SIMPLE;
	else
		sc->sc_flags &= ~VNF_SIMPLE;

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    ((sc->sc_flags & VNF_HAVELABEL) == 0 ||
	    part >= sc->sc_dk.dk_label->d_npartitions ||
	    sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Prevent our unit from being unconfigured while open. */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= pmask;
		break;
	}
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	vndunlock(sc);
	return (0);
bad:
	vndunlock(sc);
	return (error);
}

/*
 * Load the label information on the named device
 */
void
vndgetdisklabel(dev, sc)
	dev_t dev;
	struct vnd_softc *sc;
{
	struct disklabel *lp = sc->sc_dk.dk_label;
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(sc->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = 512;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = sc->sc_size / 100;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	strncpy(lp->d_typename, "vnd device", sizeof(lp->d_typename));
	lp->d_type = DTYPE_SCSI;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_secperunit = sc->sc_size;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(VNDLABELDEV(dev), vndstrategy, lp,
	    sc->sc_dk.dk_cpulabel, 0);
	if (errstring) {
		/*printf("%s: %s\n", sc->sc_dev.dv_xname, errstring);*/
		return;
	}
}

int
vndclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = vndunit(dev);
	struct vnd_softc *sc;
	int error = 0, part;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndclose(%x, %x, %x, %p)\n", dev, flags, mode, p);
#endif

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((error = vndlock(sc)) != 0)
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		sc->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	vndunlock(sc);
	return (0);
}

/*
 * Two methods are used, the traditional buffercache bypassing and the
 * newer, cache-coherent on unmount, one.
 *
 * Former method:
 * Break the request into bsize pieces and submit using VOP_BMAP/VOP_STRATEGY.
 * Note that this driver can only be used for swapping over NFS on the hp
 * since nfs_strategy on the vax cannot handle u-areas and page tables.
 *
 * Latter method:
 * Repack the buffer into an uio structure and use VOP_READ/VOP_WRITE to
 * access the underlying file.  Things are complicated by the fact that we
 * might get recursively called due to buffer flushes.  In those cases we
 * queue one write.
 */
void
vndstrategy(bp)
	struct buf *bp;
{
	int unit = vndunit(bp->b_dev);
	struct vnd_softc *vnd = &vnd_softc[unit];
	struct vndbuf *nbp;
	int bsize;
	off_t bn;
	caddr_t addr;
	size_t resid;
	int sz, flags, error, s;
	struct iovec aiov;
	struct uio auio;
	struct proc *p = curproc;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndstrategy(%p): unit %d\n", bp, unit);
#endif
	if ((vnd->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		s = splbio();
		biodone(bp);
		splx(s);
		return;
	}

	bn = bp->b_blkno;
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	bp->b_resid = bp->b_bcount;
	if (bn < 0) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		s = splbio();
		biodone(bp);
		splx(s);
		return;
	}
	if (DISKPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, vnd->sc_dk.dk_label,
	    vnd->sc_dk.dk_cpulabel, 1) <= 0) {
		s = splbio();
		biodone(bp);
		splx(s);
		return;
	}

	/* No bypassing of buffer cache?  */
	if (vndsimple(bp->b_dev)) {
		/*
		 * In order to avoid "locking against myself" panics, we
		 * must be prepared to queue operations during another I/O
		 * operation.  This situation comes up where a dirty cache
		 * buffer needs to be flushed in order to provide the current
		 * operation with a fresh buffer.
		 *
		 * XXX do we really need to protect stuff relating to this with
		 * splbio?
		 */
		if (vnd->sc_flags & VNF_BUSY) {
			s = splbio();
			bp->b_actf = vnd->sc_tab.b_actf;
			vnd->sc_tab.b_actf = bp;
			vnd->sc_tab.b_active++;
			splx(s);
			return;
		}

		/* Loop until all queued requests are handled.  */
		for (;;) {
			int part = DISKPART(bp->b_dev);
			int off = vnd->sc_dk.dk_label->d_partitions[part].p_offset;

			aiov.iov_base = bp->b_data;
			auio.uio_resid = aiov.iov_len = bp->b_bcount;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_offset = dbtob((off_t)(bp->b_blkno + off));
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_procp = p;

			vn_lock(vnd->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
			vnd->sc_flags |= VNF_BUSY;
			if (bp->b_flags & B_READ) {
				auio.uio_rw = UIO_READ;
				bp->b_error = VOP_READ(vnd->sc_vp, &auio, 0,
				    vnd->sc_cred);
				if (vnd->sc_keyctx)
					vndencrypt(vnd,	bp->b_data,
					   bp->b_bcount, bp->b_blkno, 0);
			} else {
				if (vnd->sc_keyctx)
					vndencrypt(vnd, bp->b_data,
					   bp->b_bcount, bp->b_blkno, 1);
				auio.uio_rw = UIO_WRITE;
				bp->b_error = VOP_WRITE(vnd->sc_vp, &auio, 0,
				    vnd->sc_cred);
				/* Data in buffer cache needs to be in clear */
				if (vnd->sc_keyctx)
					vndencrypt(vnd, bp->b_data,
					   bp->b_bcount, bp->b_blkno, 0);
			}
			vnd->sc_flags &= ~VNF_BUSY;
			VOP_UNLOCK(vnd->sc_vp, 0, p);
			if (bp->b_error)
				bp->b_flags |= B_ERROR;
			bp->b_resid = auio.uio_resid;
			s = splbio();
			biodone(bp);
			splx(s);

			/* If nothing more is queued, we are done.  */
			if (!vnd->sc_tab.b_active)
				return;

			/*
			 * Dequeue now since lower level strategy
			 * routine might queue using same links.
			 */
			s = splbio();
			bp = vnd->sc_tab.b_actf;
			vnd->sc_tab.b_actf = bp->b_actf;
			vnd->sc_tab.b_active--;
			splx(s);
		}
	}

	/* The old-style buffercache bypassing method.  */
	bn += vnd->sc_dk.dk_label->d_partitions[DISKPART(bp->b_dev)].p_offset;
	bn = dbtob(bn);
	bsize = vnd->sc_vp->v_mount->mnt_stat.f_iosize;
	addr = bp->b_data;
	flags = bp->b_flags | B_CALL;
	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vnode *vp;
		daddr_t nbn;
		int off, s, nra;

		nra = 0;
		vn_lock(vnd->sc_vp, LK_RETRY | LK_EXCLUSIVE, p);
		error = VOP_BMAP(vnd->sc_vp, bn / bsize, &vp, &nbn, &nra);
		VOP_UNLOCK(vnd->sc_vp, 0, p);
		if (error == 0 && (long)nbn == -1)
			error = EIO;
#ifdef DEBUG
		if (!dovndcluster)
			nra = 0;
#endif

		if ((off = bn % bsize) != 0)
			sz = bsize - off;
		else
			sz = (1 + nra) * bsize;
		if (resid < sz)
			sz = resid;
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndstrategy: vp %p/%p bn %x/%x sz %x\n",
			    vnd->sc_vp, vp, bn, nbn, sz);
#endif

		nbp = getvndbuf();
		nbp->vb_buf.b_flags = flags;
		nbp->vb_buf.b_bcount = sz;
		nbp->vb_buf.b_bufsize = bp->b_bufsize;
		nbp->vb_buf.b_error = 0;
		if (vp->v_type == VBLK || vp->v_type == VCHR)
			nbp->vb_buf.b_dev = vp->v_rdev;
		else
			nbp->vb_buf.b_dev = NODEV;
		nbp->vb_buf.b_data = addr;
		nbp->vb_buf.b_blkno = nbn + btodb(off);
		nbp->vb_buf.b_proc = bp->b_proc;
		nbp->vb_buf.b_iodone = vndiodone;
		nbp->vb_buf.b_vp = vp;
		nbp->vb_buf.b_dirtyoff = bp->b_dirtyoff;
		nbp->vb_buf.b_dirtyend = bp->b_dirtyend;
		nbp->vb_buf.b_validoff = bp->b_validoff;
		nbp->vb_buf.b_validend = bp->b_validend;
		LIST_INIT(&nbp->vb_buf.b_dep);

		/* save a reference to the old buffer */
		nbp->vb_obp = bp;

		/*
		 * If there was an error or a hole in the file...punt.
		 * Note that we deal with this after the nbp allocation.
		 * This ensures that we properly clean up any operations
		 * that we have already fired off.
		 *
		 * XXX we could deal with holes here but it would be
		 * a hassle (in the write case).
		 */
		if (error) {
			nbp->vb_buf.b_error = error;
			nbp->vb_buf.b_flags |= B_ERROR;
			bp->b_resid -= (resid - sz);
			s = splbio();
			biodone(&nbp->vb_buf);
			splx(s);
			return;
		}
		/*
		 * Just sort by block number
		 */
		nbp->vb_buf.b_cylin = nbp->vb_buf.b_blkno;
		s = splbio();
		disksort(&vnd->sc_tab, &nbp->vb_buf);
		if (vnd->sc_tab.b_active < vnd->sc_maxactive) {
			vnd->sc_tab.b_active++;
			vndstart(vnd);
		}
		splx(s);

		bn += sz;
		addr += sz;
	}
}

/*
 * Feed requests sequentially.
 * We do it this way to keep from flooding NFS servers if we are connected
 * to an NFS file.  This places the burden on the client rather than the
 * server.
 */
void
vndstart(vnd)
	struct vnd_softc *vnd;
{
	struct buf *bp;

	/*
	 * Dequeue now since lower level strategy routine might
	 * queue using same links
	 */
	bp = vnd->sc_tab.b_actf;
	vnd->sc_tab.b_actf = bp->b_actf;
#ifdef DEBUG
	if (vnddebug & VDB_IO)
		printf("vndstart(%d): bp %p vp %p blkno %x addr %p cnt %lx\n",
		    vnd-vnd_softc, bp, bp->b_vp, bp->b_blkno, bp->b_data,
		    bp->b_bcount);
#endif

	/* Instrumentation. */
	disk_busy(&vnd->sc_dk);

	if ((bp->b_flags & B_READ) == 0)
		bp->b_vp->v_numoutput++;
	VOP_STRATEGY(bp);
}

void
vndiodone(bp)
	struct buf *bp;
{
	struct vndbuf *vbp = (struct vndbuf *) bp;
	struct buf *pbp = vbp->vb_obp;
	struct vnd_softc *vnd = &vnd_softc[vndunit(pbp->b_dev)];

	splassert(IPL_BIO);

#ifdef DEBUG
	if (vnddebug & VDB_IO)
		printf("vndiodone(%d): vbp %p vp %p blkno %x addr %p cnt %lx\n",
		    vnd-vnd_softc, vbp, vbp->vb_buf.b_vp, vbp->vb_buf.b_blkno,
		    vbp->vb_buf.b_data, vbp->vb_buf.b_bcount);
#endif

	if (vbp->vb_buf.b_error) {
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndiodone: vbp %p error %d\n", vbp,
			    vbp->vb_buf.b_error);
#endif
		pbp->b_flags |= B_ERROR;
		pbp->b_error = biowait(&vbp->vb_buf);
	}
	pbp->b_resid -= vbp->vb_buf.b_bcount;
	putvndbuf(vbp);
	if (vnd->sc_tab.b_active) {
		disk_unbusy(&vnd->sc_dk, (pbp->b_bcount - pbp->b_resid),
		    (pbp->b_flags & B_READ));
		if (vnd->sc_tab.b_actf)
			vndstart(vnd);
		else
			vnd->sc_tab.b_active--;
	}
	if (pbp->b_resid == 0) {
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndiodone: pbp %p iodone\n", pbp);
#endif
		biodone(pbp);
	}

}

/* ARGSUSED */
int
vndread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = vndunit(dev);
	struct vnd_softc *sc;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndread(%x, %p)\n", dev, uio);
#endif

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vndstrategy, NULL, dev, B_READ, minphys, uio));
}

/* ARGSUSED */
int
vndwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = vndunit(dev);
	struct vnd_softc *sc;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndwrite(%x, %p)\n", dev, uio);
#endif

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vndstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/* ARGSUSED */
int
vndioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int unit = vndunit(dev);
	struct vnd_softc *vnd;
	struct vnd_ioctl *vio;
	struct vnd_user *vnu;
	struct vattr vattr;
	struct nameidata nd;
	int error, part, pmask, s;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndioctl(%x, %lx, %p, %x, %p): unit %d\n",
		    dev, cmd, addr, flag, p, unit);
#endif
	error = suser(p, 0);
	if (error)
		return (error);
	if (unit >= numvnd)
		return (ENXIO);

	vnd = &vnd_softc[unit];
	vio = (struct vnd_ioctl *)addr;
	switch (cmd) {

	case VNDIOCSET:
		if (vnd->sc_flags & VNF_INITED)
			return (EBUSY);
		if (!(vnd->sc_flags & VNF_SIMPLE) && vio->vnd_keylen)
			return (EINVAL);

		if ((error = vndlock(vnd)) != 0)
			return (error);

		if ((error = copyinstr(vio->vnd_file, vnd->sc_file,
		    sizeof(vnd->sc_file), NULL))) {
			vndunlock(vnd);
			return (error);
		}

		bzero(vnd->sc_dev.dv_xname, sizeof(vnd->sc_dev.dv_xname));
		if (snprintf(vnd->sc_dev.dv_xname, sizeof(vnd->sc_dev.dv_xname),
		    "vnd%d", unit) >= sizeof(vnd->sc_dev.dv_xname)) {
			printf("VNDIOCSET: device name too long\n");
			vndunlock(vnd);
			return(ENXIO);
		}

		/*
		 * Always open for read and write.
		 * This is probably bogus, but it lets vn_open()
		 * weed out directories, sockets, etc. so we don't
		 * have to worry about them.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vnd_file, p);
		if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
			vndunlock(vnd);
			return (error);
		}
		error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p);
		if (error) {
			VOP_UNLOCK(nd.ni_vp, 0, p);
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			vndunlock(vnd);
			return (error);
		}
		VOP_UNLOCK(nd.ni_vp, 0, p);
		vnd->sc_vp = nd.ni_vp;
		vnd->sc_size = btodb(vattr.va_size);	/* note truncation */
		if ((error = vndsetcred(vnd, p->p_ucred)) != 0) {
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			vndunlock(vnd);
			return (error);
		}

		if (vio->vnd_keylen > 0) {
			char key[128];

			if (vio->vnd_keylen > sizeof(key))
				vio->vnd_keylen = sizeof(key);

			if ((error = copyin(vio->vnd_key, key,
			    vio->vnd_keylen)) != 0) {
				(void) vn_close(nd.ni_vp, FREAD|FWRITE,
				    p->p_ucred, p);
				vndunlock(vnd);
				return (error);
			}

			vnd->sc_keyctx = malloc(sizeof(blf_ctx), M_DEVBUF,
			    M_WAITOK);
			blf_key(vnd->sc_keyctx, key, vio->vnd_keylen);
			bzero(key, vio->vnd_keylen);
		} else
			vnd->sc_keyctx = NULL;

		vndthrottle(vnd, vnd->sc_vp);
		vio->vnd_size = dbtob((off_t)vnd->sc_size);
		vnd->sc_flags |= VNF_INITED;
#ifdef DEBUG
		if (vnddebug & VDB_INIT)
			printf("vndioctl: SET vp %p size %llx\n",
			    vnd->sc_vp, (unsigned long long)vnd->sc_size);
#endif

		/* Attach the disk. */
		vnd->sc_dk.dk_driver = &vnddkdriver;
		vnd->sc_dk.dk_name = vnd->sc_dev.dv_xname;
		disk_attach(&vnd->sc_dk);
		dk_establish(&vnd->sc_dk, &vnd->sc_dev);

		vndunlock(vnd);

		break;

	case VNDIOCCLR:
		if ((vnd->sc_flags & VNF_INITED) == 0)
			return (ENXIO);

		if ((error = vndlock(vnd)) != 0)
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((vnd->sc_dk.dk_openmask & ~pmask) ||
		    ((vnd->sc_dk.dk_bopenmask & pmask) &&
		    (vnd->sc_dk.dk_copenmask & pmask))) {
			vndunlock(vnd);
			return (EBUSY);
		}

		vndclear(vnd);
#ifdef DEBUG
		if (vnddebug & VDB_INIT)
			printf("vndioctl: CLRed\n");
#endif
		/* Free crypto key */
		if (vnd->sc_keyctx) {
			bzero(vnd->sc_keyctx, sizeof(*vnd->sc_keyctx));
			free(vnd->sc_keyctx, M_DEVBUF);
		}

		/* Detatch the disk. */
		disk_detach(&vnd->sc_dk);

		/* This must be atomic. */
		s = splhigh();
		vndunlock(vnd);
		bzero(vnd, sizeof(struct vnd_softc));
		splx(s);
		break;

	case VNDIOCGET:
		vnu = (struct vnd_user *)addr;

		if (vnu->vnu_unit == -1)
			vnu->vnu_unit = unit;
		if (vnu->vnu_unit >= numvnd)
			return (ENXIO);
		if (vnu->vnu_unit < 0)
			return (EINVAL);

		vnd = &vnd_softc[vnu->vnu_unit];

		if (vnd->sc_flags & VNF_INITED) {
			error = VOP_GETATTR(vnd->sc_vp, &vattr, p->p_ucred, p);
			if (error)
				return (error);

			strlcpy(vnu->vnu_file, vnd->sc_file,
			    sizeof(vnu->vnu_file));
			vnu->vnu_dev = vattr.va_fsid;
			vnu->vnu_ino = vattr.va_fileid;
		} else {
			vnu->vnu_dev = 0;
			vnu->vnu_ino = 0;
		}

		break;

	case DIOCGDINFO:
		if ((vnd->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		*(struct disklabel *)addr = *(vnd->sc_dk.dk_label);
		return (0);

	case DIOCGPART:
		if ((vnd->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		((struct partinfo *)addr)->disklab = vnd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &vnd->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return (0);

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((vnd->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		if ((flag & FWRITE) == 0)
			return (EBADF);

		if ((error = vndlock(vnd)) != 0)
			return (error);
		vnd->sc_flags |= VNF_LABELLING;

		error = setdisklabel(vnd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*vnd->sc_dk.dk_openmask : */0,
		    vnd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(MAKEDISKDEV(major(dev),
				    DISKUNIT(dev), RAW_PART),
				    vndstrategy, vnd->sc_dk.dk_label,
				    vnd->sc_dk.dk_cpulabel);
		}

		vnd->sc_flags &= ~VNF_LABELLING;
		vndunlock(vnd);
		return (error);

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		if (*(int *)addr)
			vnd->sc_flags |= VNF_WLABEL;
		else
			vnd->sc_flags &= ~VNF_WLABEL;
		return (0);

	default:
		return (ENOTTY);
	}

	return (0);
}

/*
 * Duplicate the current processes' credentials.  Since we are called only
 * as the result of a SET ioctl and only root can do that, any future access
 * to this "disk" is essentially as root.  Note that credentials may change
 * if some other uid can write directly to the mapped file (NFS).
 */
int
vndsetcred(vnd, cred)
	struct vnd_softc *vnd;
	struct ucred *cred;
{
	struct uio auio;
	struct iovec aiov;
	char *tmpbuf;
	int error;
	struct proc *p = curproc;

	vnd->sc_cred = crdup(cred);
	tmpbuf = malloc(DEV_BSIZE, M_TEMP, M_WAITOK);

	/* XXX: Horrible kludge to establish credentials for NFS */
	aiov.iov_base = tmpbuf;
	aiov.iov_len = MIN(DEV_BSIZE, dbtob((off_t)vnd->sc_size));
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = aiov.iov_len;
	vn_lock(vnd->sc_vp, LK_RETRY | LK_EXCLUSIVE, p);
	error = VOP_READ(vnd->sc_vp, &auio, 0, vnd->sc_cred);
	VOP_UNLOCK(vnd->sc_vp, 0, p);

	free(tmpbuf, M_TEMP);
	return (error);
}

/*
 * Set maxactive based on FS type
 */
void
vndthrottle(vnd, vp)
	struct vnd_softc *vnd;
	struct vnode *vp;
{
#ifdef NFSCLIENT
	extern int (**nfsv2_vnodeop_p)(void *);

	if (vp->v_op == nfsv2_vnodeop_p)
		vnd->sc_maxactive = 2;
	else
#endif
		vnd->sc_maxactive = 8;
}

void
vndshutdown()
{
	struct vnd_softc *vnd;

	for (vnd = &vnd_softc[0]; vnd < &vnd_softc[numvnd]; vnd++)
		if (vnd->sc_flags & VNF_INITED)
			vndclear(vnd);
}

void
vndclear(vnd)
	struct vnd_softc *vnd;
{
	struct vnode *vp = vnd->sc_vp;
	struct proc *p = curproc;		/* XXX */

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndclear(%p): vp %p\n", vnd, vp);
#endif
	vnd->sc_flags &= ~VNF_INITED;
	if (vp == (struct vnode *)0)
		panic("vndioctl: null vp");
	(void) vn_close(vp, FREAD|FWRITE, vnd->sc_cred, p);
	crfree(vnd->sc_cred);
	vnd->sc_vp = (struct vnode *)0;
	vnd->sc_cred = (struct ucred *)0;
	vnd->sc_size = 0;
}

int
vndsize(dev)
	dev_t dev;
{
	int unit = vndunit(dev);
	struct vnd_softc *vnd = &vnd_softc[unit];

	if (unit >= numvnd || (vnd->sc_flags & VNF_INITED) == 0)
		return (-1);
	return (vnd->sc_size);
}

int
vnddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return (ENXIO);
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
vndlock(sc)
	struct vnd_softc *sc;
{
	int error;

	while ((sc->sc_flags & VNF_LOCKED) != 0) {
		sc->sc_flags |= VNF_WANTED;
		if ((error = tsleep(sc, PRIBIO | PCATCH, "vndlck", 0)) != 0)
			return (error);
	}
	sc->sc_flags |= VNF_LOCKED;
	return (0);
}

/*
 * Unlock and wake up any waiters.
 */
void
vndunlock(sc)
	struct vnd_softc *sc;
{

	sc->sc_flags &= ~VNF_LOCKED;
	if ((sc->sc_flags & VNF_WANTED) != 0) {
		sc->sc_flags &= ~VNF_WANTED;
		wakeup(sc);
	}
}
