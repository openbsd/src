/*	$NetBSD: vnd.c,v 1.23 1996/01/07 22:03:33 thorpej Exp $	*/

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
 * NOTE 1: This uses the VOP_BMAP/VOP_STRATEGY interface to the vnode
 * instead of a simple VOP_RDWR.  We do this to avoid distorting the
 * local buffer cache.
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
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>

#include <miscfs/specfs/specdev.h>

#include <dev/vndioctl.h>

#ifdef DEBUG
int dovndcluster = 1;
int vnddebug = 0x00;
#define VDB_FOLLOW	0x01
#define VDB_INIT	0x02
#define VDB_IO		0x04
#endif

#define b_cylin	b_resid

#define	vndunit(x)	DISKUNIT(x)

struct vndbuf {
	struct buf	vb_buf;
	struct buf	*vb_obp;
};

#define	getvndbuf()	\
	((struct vndbuf *)malloc(sizeof(struct vndbuf), M_DEVBUF, M_WAITOK))
#define putvndbuf(vbp)	\
	free((caddr_t)(vbp), M_DEVBUF)

struct vnd_softc {
	int		 sc_flags;	/* flags */
	size_t		 sc_size;	/* size of vnd */
	struct vnode	*sc_vp;		/* vnode */
	struct ucred	*sc_cred;	/* credentials */
	int		 sc_maxactive;	/* max # of active requests */
	struct buf	 sc_tab;	/* transfer queue */
	char		 sc_xname[8];	/* XXX external name */
	struct disk	 sc_dkdev;	/* generic disk device info */
};

/* sc_flags */
#define	VNF_ALIVE	0x01
#define VNF_INITED	0x02
#define VNF_WANTED	0x40
#define VNF_LOCKED	0x80

struct vnd_softc *vnd_softc;
int numvnd = 0;

/* {b,c}devsw[] function prototypes */
dev_type_open(vndopen);
dev_type_close(vndclose);
dev_type_strategy(vndstrategy);
dev_type_ioctl(vndioctl);
dev_type_read(vndread);
dev_type_write(vndwrite);

/* called by main() at boot time */
void	vndattach __P((int));

void	vndclear __P((struct vnd_softc *));
void	vndstart __P((struct vnd_softc *));
int	vndsetcred __P((struct vnd_softc *, struct ucred *));
void	vndthrottle __P((struct vnd_softc *, struct vnode *));

static	int vndlock __P((struct vnd_softc *));
static	void vndunlock __P((struct vnd_softc *));

void
vndattach(num)
	int num;
{
	char *mem;
	register u_long size;

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

	/*
	 * XXX Should support disklabels.
	 */

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndopen(%x, %x, %x, %x)\n", dev, flags, mode, p);
#endif
	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if (error = vndlock(sc))
		return (error);

	part = DISKPART(dev);
	pmask = (1 << part);

	/* Prevent our unit from being unconfigured while open. */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		sc->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	sc->sc_dkdev.dk_openmask =
	    sc->sc_dkdev.dk_copenmask | sc->sc_dkdev.dk_bopenmask;

	vndunlock(sc);
	return (0);
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
		printf("vndclose(%x, %x, %x, %x)\n", dev, flags, mode, p);
#endif

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if (error = vndlock(sc))
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		sc->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dkdev.dk_openmask =
	    sc->sc_dkdev.dk_copenmask | sc->sc_dkdev.dk_bopenmask;

	vndunlock(sc);
	return (0);
}

/*
 * Break the request into bsize pieces and submit using VOP_BMAP/VOP_STRATEGY.
 * Note that this driver can only be used for swapping over NFS on the hp
 * since nfs_strategy on the vax cannot handle u-areas and page tables.
 */
void
vndstrategy(bp)
	register struct buf *bp;
{
	int unit = vndunit(bp->b_dev);
	register struct vnd_softc *vnd = &vnd_softc[unit];
	register struct vndbuf *nbp;
	register int bn, bsize, resid;
	register caddr_t addr;
	int sz, flags, error;
	extern void vndiodone();

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndstrategy(%x): unit %d\n", bp, unit);
#endif
	if ((vnd->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}
	bn = bp->b_blkno;
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	bp->b_resid = bp->b_bcount;
	if (bn < 0 || bn + sz > vnd->sc_size) {
		if (bn != vnd->sc_size) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
		}
		biodone(bp);
		return;
	}
	bn = dbtob(bn);
 	bsize = vnd->sc_vp->v_mount->mnt_stat.f_iosize;
	addr = bp->b_data;
	flags = bp->b_flags | B_CALL;
	for (resid = bp->b_resid; resid; resid -= sz) {
		struct vnode *vp;
		daddr_t nbn;
		int off, s, nra;

		nra = 0;
		VOP_LOCK(vnd->sc_vp);
		error = VOP_BMAP(vnd->sc_vp, bn / bsize, &vp, &nbn, &nra);
		VOP_UNLOCK(vnd->sc_vp);
		if (error == 0 && (long)nbn == -1)
			error = EIO;
#ifdef DEBUG
		if (!dovndcluster)
			nra = 0;
#endif

		if (off = bn % bsize)
			sz = bsize - off;
		else
			sz = (1 + nra) * bsize;
		if (resid < sz)
			sz = resid;
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndstrategy: vp %x/%x bn %x/%x sz %x\n",
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
		nbp->vb_buf.b_rcred = vnd->sc_cred;	/* XXX crdup? */
		nbp->vb_buf.b_wcred = vnd->sc_cred;	/* XXX crdup? */
		nbp->vb_buf.b_dirtyoff = bp->b_dirtyoff;
		nbp->vb_buf.b_dirtyend = bp->b_dirtyend;
		nbp->vb_buf.b_validoff = bp->b_validoff;
		nbp->vb_buf.b_validend = bp->b_validend;

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
			biodone(&nbp->vb_buf);
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
	register struct vnd_softc *vnd;
{
	register struct buf *bp;

	/*
	 * Dequeue now since lower level strategy routine might
	 * queue using same links
	 */
	bp = vnd->sc_tab.b_actf;
	vnd->sc_tab.b_actf = bp->b_actf;
#ifdef DEBUG
	if (vnddebug & VDB_IO)
		printf("vndstart(%d): bp %x vp %x blkno %x addr %x cnt %x\n",
		    vnd-vnd_softc, bp, bp->b_vp, bp->b_blkno, bp->b_data,
		    bp->b_bcount);
#endif

	/* Instrumentation. */
	disk_busy(&vnd->sc_dkdev);

	if ((bp->b_flags & B_READ) == 0)
		bp->b_vp->v_numoutput++;
	VOP_STRATEGY(bp);
}

void
vndiodone(vbp)
	register struct vndbuf *vbp;
{
	register struct buf *pbp = vbp->vb_obp;
	register struct vnd_softc *vnd = &vnd_softc[vndunit(pbp->b_dev)];
	int s;

	s = splbio();
#ifdef DEBUG
	if (vnddebug & VDB_IO)
		printf("vndiodone(%d): vbp %x vp %x blkno %x addr %x cnt %x\n",
		    vnd-vnd_softc, vbp, vbp->vb_buf.b_vp, vbp->vb_buf.b_blkno,
		    vbp->vb_buf.b_data, vbp->vb_buf.b_bcount);
#endif

	if (vbp->vb_buf.b_error) {
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndiodone: vbp %x error %d\n", vbp,
			    vbp->vb_buf.b_error);
#endif
		pbp->b_flags |= B_ERROR;
		pbp->b_error = biowait(&vbp->vb_buf);
	}
	pbp->b_resid -= vbp->vb_buf.b_bcount;
	putvndbuf(vbp);
	disk_unbusy(&vnd->sc_dkdev, (pbp->b_bcount - pbp->b_resid));
	if (pbp->b_resid == 0) {
#ifdef DEBUG
		if (vnddebug & VDB_IO)
			printf("vndiodone: pbp %x iodone\n", pbp);
#endif
		biodone(pbp);
	}
	if (vnd->sc_tab.b_actf)
		vndstart(vnd);
	else
		vnd->sc_tab.b_active--;
	splx(s);
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
		printf("vndread(%x, %x)\n", dev, uio);
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
		printf("vndwrite(%x, %x)\n", dev, uio);
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
vndioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = vndunit(dev);
	register struct vnd_softc *vnd;
	struct vnd_ioctl *vio;
	struct vattr vattr;
	struct nameidata nd;
	int error, part, pmask, s;

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndioctl(%x, %lx, %x, %x, %x): unit %d\n",
		    dev, cmd, data, flag, p, unit);
#endif
	error = suser(p->p_ucred, &p->p_acflag);
	if (error)
		return (error);
	if (unit >= numvnd)
		return (ENXIO);

	vnd = &vnd_softc[unit];
	vio = (struct vnd_ioctl *)data;
	switch (cmd) {

	case VNDIOCSET:
		if (vnd->sc_flags & VNF_INITED)
			return (EBUSY);

		if (error = vndlock(vnd))
			return (error);

		/*
		 * Always open for read and write.
		 * This is probably bogus, but it lets vn_open()
		 * weed out directories, sockets, etc. so we don't
		 * have to worry about them.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vnd_file, p);
		if (error = vn_open(&nd, FREAD|FWRITE, 0)) {
			vndunlock(vnd);
			return(error);
		}
		if (error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p)) {
			VOP_UNLOCK(nd.ni_vp);
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			vndunlock(vnd);
			return(error);
		}
		VOP_UNLOCK(nd.ni_vp);
		vnd->sc_vp = nd.ni_vp;
		vnd->sc_size = btodb(vattr.va_size);	/* note truncation */
		if (error = vndsetcred(vnd, p->p_ucred)) {
			(void) vn_close(nd.ni_vp, FREAD|FWRITE, p->p_ucred, p);
			vndunlock(vnd);
			return(error);
		}
		vndthrottle(vnd, vnd->sc_vp);
		vio->vnd_size = dbtob(vnd->sc_size);
		vnd->sc_flags |= VNF_INITED;
#ifdef DEBUG
		if (vnddebug & VDB_INIT)
			printf("vndioctl: SET vp %x size %x\n",
			    vnd->sc_vp, vnd->sc_size);
#endif

		/* Attach the disk. */
		bzero(vnd->sc_xname, sizeof(vnd->sc_xname));	/* XXX */
		sprintf(vnd->sc_xname, "vnd%d", unit);		/* XXX */
		vnd->sc_dkdev.dk_name = vnd->sc_xname;
		disk_attach(&vnd->sc_dkdev);

		vndunlock(vnd);

		break;

	case VNDIOCCLR:
		if ((vnd->sc_flags & VNF_INITED) == 0)
			return (ENXIO);

		if (error = vndlock(vnd))
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((vnd->sc_dkdev.dk_openmask & ~pmask) ||
		    ((vnd->sc_dkdev.dk_bopenmask & pmask) &&
		    (vnd->sc_dkdev.dk_copenmask & pmask))) {
			vndunlock(vnd);
			return (EBUSY);
		}

		vndclear(vnd);
#ifdef DEBUG
		if (vnddebug & VDB_INIT)
			printf("vndioctl: CLRed\n");
#endif

		/* Detatch the disk. */
		disk_detatch(&vnd->sc_dkdev);

		/* This must be atomic. */
		s = splhigh();
		vndunlock(vnd);
		bzero(vnd, sizeof(struct vnd_softc));
		splx(s);

		break;

	/*
	 * XXX Should support disklabels.
	 */

	default:
		return(ENOTTY);
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
	register struct vnd_softc *vnd;
	struct ucred *cred;
{
	struct uio auio;
	struct iovec aiov;
	char *tmpbuf;
	int error;

	vnd->sc_cred = crdup(cred);
	tmpbuf = malloc(DEV_BSIZE, M_TEMP, M_WAITOK);

	/* XXX: Horrible kludge to establish credentials for NFS */
	aiov.iov_base = tmpbuf;
	aiov.iov_len = min(DEV_BSIZE, dbtob(vnd->sc_size));
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = aiov.iov_len;
	VOP_LOCK(vnd->sc_vp);
	error = VOP_READ(vnd->sc_vp, &auio, 0, vnd->sc_cred);
	VOP_UNLOCK(vnd->sc_vp);

	free(tmpbuf, M_TEMP);
	return (error);
}

/*
 * Set maxactive based on FS type
 */
void
vndthrottle(vnd, vp)
	register struct vnd_softc *vnd;
	struct vnode *vp;
{
#ifdef NFSCLIENT
	extern int (**nfsv2_vnodeop_p)();

	if (vp->v_op == nfsv2_vnodeop_p)
		vnd->sc_maxactive = 2;
	else
#endif
		vnd->sc_maxactive = 8;

	if (vnd->sc_maxactive < 1)
		vnd->sc_maxactive = 1;
}

void
vndshutdown()
{
	register struct vnd_softc *vnd;

	for (vnd = &vnd_softc[0]; vnd < &vnd_softc[numvnd]; vnd++)
		if (vnd->sc_flags & VNF_INITED)
			vndclear(vnd);
}

void
vndclear(vnd)
	register struct vnd_softc *vnd;
{
	register struct vnode *vp = vnd->sc_vp;
	struct proc *p = curproc;		/* XXX */

#ifdef DEBUG
	if (vnddebug & VDB_FOLLOW)
		printf("vndclear(%x): vp %x\n", vp);
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
	register struct vnd_softc *vnd = &vnd_softc[unit];

	if (unit >= numvnd || (vnd->sc_flags & VNF_INITED) == 0)
		return(-1);
	return(vnd->sc_size);
}

int
vnddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
static int
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
static void
vndunlock(sc)
	struct vnd_softc *sc;
{

	sc->sc_flags &= ~VNF_LOCKED;
	if ((sc->sc_flags & VNF_WANTED) != 0) {
		sc->sc_flags &= ~VNF_WANTED;
		wakeup(sc);
	}
}
