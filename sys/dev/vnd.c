/*	$OpenBSD: vnd.c,v 1.136 2011/07/03 18:08:14 matthew Exp $	*/
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
 */

/*
 * There is a security issue involved with this driver.
 *
 * Once mounted all access to the contents of the "mapped" file via
 * the special file is controlled by the permissions on the special
 * file, the protection of the mapped file is ignored (effectively,
 * by using root credentials in all transactions).
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/pool.h>
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
#include <sys/dkio.h>

#include <crypto/blf.h>

#include <miscfs/specfs/specdev.h>

#include <dev/vndioctl.h>

#ifdef VNDDEBUG
int vnddebug = 0x00;
#define	VDB_FOLLOW	0x01
#define	VDB_INIT	0x02
#define	VDB_IO		0x04
#define	DNPRINTF(f, p...)	do { if ((f) & vnddebug) printf(p); } while (0)
#else
#define	DNPRINTF(f, p...)	/* nothing */
#endif	/* VNDDEBUG */

struct vnd_softc {
	struct device	 sc_dev;
	struct disk	 sc_dk;

	char		 sc_file[VNDNLEN];	/* file we're covering */
	int		 sc_flags;		/* flags */
	size_t		 sc_size;		/* size of vnd in sectors */
	size_t		 sc_secsize;		/* sector size in bytes */
	size_t		 sc_nsectors;		/* # of sectors per track */
	size_t		 sc_ntracks;		/* # of tracks per cylinder */
	struct vnode	*sc_vp;			/* vnode */
	struct ucred	*sc_cred;		/* credentials */
	blf_ctx		*sc_keyctx;		/* key context */
};

/* sc_flags */
#define	VNF_INITED	0x0002
#define	VNF_HAVELABEL	0x0400
#define	VNF_READONLY	0x2000

#define	VNDRW(v)	((v)->sc_flags & VNF_READONLY ? FREAD : FREAD|FWRITE)

struct vnd_softc *vnd_softc;
int numvnd = 0;

/* called by main() at boot time */
void	vndattach(int);

void	vndclear(struct vnd_softc *);
int	vndsetcred(struct vnd_softc *, struct ucred *);
int	vndgetdisklabel(dev_t, struct vnd_softc *, struct disklabel *, int);
void	vndencrypt(struct vnd_softc *, caddr_t, size_t, daddr64_t, int);
size_t	vndbdevsize(struct vnode *, struct proc *);

void
vndencrypt(struct vnd_softc *sc, caddr_t addr, size_t size, daddr64_t off,
    int encrypt)
{
	int i, bsize;
	u_char iv[8];

	bsize = dbtob(1);
	for (i = 0; i < size/bsize; i++) {
		bzero(iv, sizeof(iv));
		bcopy(&off, iv, sizeof(off));
		blf_ecb_encrypt(sc->sc_keyctx, iv, sizeof(iv));
		if (encrypt)
			blf_cbc_encrypt(sc->sc_keyctx, iv, addr, bsize);
		else
			blf_cbc_decrypt(sc->sc_keyctx, iv, addr, bsize);

		addr += bsize;
		off++;
	}
}

void
vndattach(int num)
{
	char *mem;
	u_long size;
	int i;

	if (num <= 0)
		return;
	size = num * sizeof(struct vnd_softc);
	mem = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mem == NULL) {
		printf("WARNING: no memory for vnode disks\n");
		return;
	}
	vnd_softc = (struct vnd_softc *)mem;
	for (i = 0; i < num; i++) {
		struct vnd_softc *sc = &vnd_softc[i];

		sc->sc_dev.dv_unit = i;
		snprintf(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname),
		    "vnd%d", i);
		disk_construct(&sc->sc_dk);
		device_ref(&sc->sc_dev);
	}
	numvnd = num;
}

int
vndopen(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct vnd_softc *sc;
	int error = 0, part;

	DNPRINTF(VDB_FOLLOW, "vndopen(%x, %x, %x, %p)\n", dev, flags, mode, p);

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((error = disk_lock(&sc->sc_dk)) != 0)
		return (error);

	if ((flags & FWRITE) && (sc->sc_flags & VNF_READONLY)) {
		error = EROFS;
		goto bad;
	}

	if ((sc->sc_flags & VNF_INITED) &&
	    (sc->sc_flags & VNF_HAVELABEL) == 0) {
		sc->sc_flags |= VNF_HAVELABEL;
		vndgetdisklabel(dev, sc, sc->sc_dk.dk_label, 0);
	}

	part = DISKPART(dev);

	error = disk_openpart(&sc->sc_dk, part, mode,
	    (sc->sc_flags & VNF_HAVELABEL) != 0);

bad:
	disk_unlock(&sc->sc_dk);
	return (error);
}

/*
 * Load the label information on the named device
 */
int
vndgetdisklabel(dev_t dev, struct vnd_softc *sc, struct disklabel *lp,
    int spoofonly)
{
	bzero(lp, sizeof(struct disklabel));

	lp->d_secsize = sc->sc_secsize;
	lp->d_nsectors = sc->sc_nsectors;
	lp->d_ntracks = sc->sc_ntracks;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	lp->d_ncylinders = sc->sc_size / lp->d_secpercyl;

	strncpy(lp->d_typename, "vnd device", sizeof(lp->d_typename));
	lp->d_type = DTYPE_VND;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	DL_SETDSIZE(lp, sc->sc_size);
	lp->d_flags = 0;
	lp->d_version = 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/* Call the generic disklabel extraction routine */
	return readdisklabel(DISKLABELDEV(dev), vndstrategy, lp, spoofonly);
}

int
vndclose(dev_t dev, int flags, int mode, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct vnd_softc *sc;
	int part;

	DNPRINTF(VDB_FOLLOW, "vndclose(%x, %x, %x, %p)\n", dev, flags, mode, p);

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	disk_lock_nointr(&sc->sc_dk);

	part = DISKPART(dev);

	disk_closepart(&sc->sc_dk, part, mode);

	disk_unlock(&sc->sc_dk);
	return (0);
}

void
vndstrategy(struct buf *bp)
{
	int unit = DISKUNIT(bp->b_dev);
	struct vnd_softc *sc = &vnd_softc[unit];
	off_t bn;
	int sz, s, part;
	struct iovec aiov;
	struct uio auio;
	struct proc *p = curproc;
	daddr64_t off;

	DNPRINTF(VDB_FOLLOW, "vndstrategy(%p): unit %d\n", bp, unit);

	if ((sc->sc_flags & VNF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* Ensure that the requested block is sector aligned. */
	if (bp->b_blkno % DL_BLKSPERSEC(sc->sc_dk.dk_label) != 0) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	bn = bp->b_blkno;
	bp->b_resid = bp->b_bcount;

	if (bn < 0) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* If we have a label, do a boundary check. */
	if (sc->sc_flags & VNF_HAVELABEL) {
		if (bounds_check_with_label(bp, sc->sc_dk.dk_label) <= 0)
			goto done;

		/*
		 * bounds_check_with_label() changes bp->b_resid, reset it
		 */
		bp->b_resid = bp->b_bcount;
	}

	if (sc->sc_flags & VNF_HAVELABEL)
		sz = howmany(bp->b_bcount, sc->sc_dk.dk_label->d_secsize);
	else
		sz = howmany(bp->b_bcount, DEV_BSIZE);

	part = DISKPART(bp->b_dev);
	off = DL_SECTOBLK(sc->sc_dk.dk_label,
	    DL_GETPOFFSET(&sc->sc_dk.dk_label->d_partitions[part]));
	aiov.iov_base = bp->b_data;
	auio.uio_resid = aiov.iov_len = bp->b_bcount;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = dbtob((off_t)(bp->b_blkno + off));
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	if (bp->b_flags & B_READ) {
		auio.uio_rw = UIO_READ;
		bp->b_error = VOP_READ(sc->sc_vp, &auio, 0,
		    sc->sc_cred);
		if (sc->sc_keyctx)
			vndencrypt(sc,	bp->b_data,
			   bp->b_bcount, bp->b_blkno, 0);
	} else {
		if (sc->sc_keyctx)
			vndencrypt(sc, bp->b_data,
			   bp->b_bcount, bp->b_blkno, 1);
		auio.uio_rw = UIO_WRITE;
		/*
		 * Upper layer has already checked I/O for
		 * limits, so there is no need to do it again.
		 */
		bp->b_error = VOP_WRITE(sc->sc_vp, &auio,
		    IO_NOLIMIT, sc->sc_cred);
		/* Data in buffer cache needs to be in clear */
		if (sc->sc_keyctx)
			vndencrypt(sc, bp->b_data,
			   bp->b_bcount, bp->b_blkno, 0);
	}
	VOP_UNLOCK(sc->sc_vp, 0, p);
	if (bp->b_error)
		bp->b_flags |= B_ERROR;
	bp->b_resid = auio.uio_resid;
done:
	s = splbio();
	biodone(bp);
	splx(s);
}


/* ARGSUSED */
int
vndread(dev_t dev, struct uio *uio, int flags)
{
	int unit = DISKUNIT(dev);
	struct vnd_softc *sc;

	DNPRINTF(VDB_FOLLOW, "vndread(%x, %p)\n", dev, uio);

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vndstrategy, dev, B_READ, minphys, uio));
}

/* ARGSUSED */
int
vndwrite(dev_t dev, struct uio *uio, int flags)
{
	int unit = DISKUNIT(dev);
	struct vnd_softc *sc;

	DNPRINTF(VDB_FOLLOW, "vndwrite(%x, %p)\n", dev, uio);

	if (unit >= numvnd)
		return (ENXIO);
	sc = &vnd_softc[unit];

	if ((sc->sc_flags & VNF_INITED) == 0)
		return (ENXIO);

	return (physio(vndstrategy, dev, B_WRITE, minphys, uio));
}

size_t
vndbdevsize(struct vnode *vp, struct proc *p)
{
	struct partinfo pi;
	struct bdevsw *bsw;
	dev_t dev;

	dev = vp->v_rdev;
	bsw = bdevsw_lookup(dev);
	if (bsw->d_ioctl == NULL)
		return (0);
	if (bsw->d_ioctl(dev, DIOCGPART, (caddr_t)&pi, FREAD, p))
		return (0);
	DNPRINTF(VDB_INIT, "vndbdevsize: size %li secsize %li\n",
	    (long)pi.part->p_size,(long)pi.disklab->d_secsize);
	return (pi.part->p_size);
}

/* ARGSUSED */
int
vndioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct disklabel *lp;
	struct vnd_softc *sc;
	struct vnd_ioctl *vio;
	struct vnd_user *vnu;
	struct vattr vattr;
	struct nameidata nd;
	int error, part, pmask;

	DNPRINTF(VDB_FOLLOW, "vndioctl(%x, %lx, %p, %x, %p): unit %d\n",
	    dev, cmd, addr, flag, p, unit);

	error = suser(p, 0);
	if (error)
		return (error);
	if (unit >= numvnd)
		return (ENXIO);

	sc = &vnd_softc[unit];
	vio = (struct vnd_ioctl *)addr;
	switch (cmd) {

	case VNDIOCSET:
		if (sc->sc_flags & VNF_INITED)
			return (EBUSY);

		if ((error = disk_lock(&sc->sc_dk)) != 0)
			return (error);

		if ((error = copyinstr(vio->vnd_file, sc->sc_file,
		    sizeof(sc->sc_file), NULL))) {
			disk_unlock(&sc->sc_dk);
			return (error);
		}

		/* Set geometry for device. */
		sc->sc_secsize = vio->vnd_secsize;
		sc->sc_ntracks = vio->vnd_ntracks;
		sc->sc_nsectors = vio->vnd_nsectors;

		/*
		 * Open for read and write first. This lets vn_open() weed out
		 * directories, sockets, etc. so we don't have to worry about
		 * them.
		 */
		NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, vio->vnd_file, p);
		sc->sc_flags &= ~VNF_READONLY; 
		error = vn_open(&nd, FREAD|FWRITE, 0);
		if (error == EROFS) {
			sc->sc_flags |= VNF_READONLY;
			error = vn_open(&nd, FREAD, 0);
		}
		if (error) {
			disk_unlock(&sc->sc_dk);
			return (error);
		}

		if (nd.ni_vp->v_type == VBLK)
			sc->sc_size = vndbdevsize(nd.ni_vp, p);
		else {
			error = VOP_GETATTR(nd.ni_vp, &vattr, p->p_ucred, p);
			if (error) {
				VOP_UNLOCK(nd.ni_vp, 0, p);
				vn_close(nd.ni_vp, VNDRW(sc), p->p_ucred, p);
				disk_unlock(&sc->sc_dk);
				return (error);
			}
			sc->sc_size = vattr.va_size / sc->sc_secsize;
		}
		VOP_UNLOCK(nd.ni_vp, 0, p);
		sc->sc_vp = nd.ni_vp;
		if ((error = vndsetcred(sc, p->p_ucred)) != 0) {
			(void) vn_close(nd.ni_vp, VNDRW(sc), p->p_ucred, p);
			disk_unlock(&sc->sc_dk);
			return (error);
		}

		if (vio->vnd_keylen > 0) {
			char key[BLF_MAXUTILIZED];

			if (vio->vnd_keylen > sizeof(key))
				vio->vnd_keylen = sizeof(key);

			if ((error = copyin(vio->vnd_key, key,
			    vio->vnd_keylen)) != 0) {
				(void) vn_close(nd.ni_vp, VNDRW(sc),
				    p->p_ucred, p);
				disk_unlock(&sc->sc_dk);
				return (error);
			}

			sc->sc_keyctx = malloc(sizeof(*sc->sc_keyctx), M_DEVBUF,
			    M_WAITOK);
			blf_key(sc->sc_keyctx, key, vio->vnd_keylen);
			explicit_bzero(key, vio->vnd_keylen);
		} else
			sc->sc_keyctx = NULL;

		vio->vnd_size = sc->sc_size * sc->sc_secsize;
		sc->sc_flags |= VNF_INITED;

		DNPRINTF(VDB_INIT, "vndioctl: SET vp %p size %llx\n",
		    sc->sc_vp, (unsigned long long)sc->sc_size);

		/* Attach the disk. */
		sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
		disk_attach(&sc->sc_dev, &sc->sc_dk);

		disk_unlock(&sc->sc_dk);

		break;

	case VNDIOCCLR:
		if ((sc->sc_flags & VNF_INITED) == 0)
			return (ENXIO);

		if ((error = disk_lock(&sc->sc_dk)) != 0)
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((sc->sc_dk.dk_openmask & ~pmask) ||
		    ((sc->sc_dk.dk_bopenmask & pmask) &&
		    (sc->sc_dk.dk_copenmask & pmask))) {
			disk_unlock(&sc->sc_dk);
			return (EBUSY);
		}

		vndclear(sc);
		DNPRINTF(VDB_INIT, "vndioctl: CLRed\n");

		/* Free crypto key */
		if (sc->sc_keyctx) {
			explicit_bzero(sc->sc_keyctx, sizeof(*sc->sc_keyctx));
			free(sc->sc_keyctx, M_DEVBUF);
		}

		/* Detach the disk. */
		disk_detach(&sc->sc_dk);
		disk_unlock(&sc->sc_dk);
		break;

	case VNDIOCGET:
		vnu = (struct vnd_user *)addr;

		if (vnu->vnu_unit == -1)
			vnu->vnu_unit = unit;
		if (vnu->vnu_unit >= numvnd)
			return (ENXIO);
		if (vnu->vnu_unit < 0)
			return (EINVAL);

		sc = &vnd_softc[vnu->vnu_unit];

		if (sc->sc_flags & VNF_INITED) {
			error = VOP_GETATTR(sc->sc_vp, &vattr, p->p_ucred, p);
			if (error)
				return (error);

			strlcpy(vnu->vnu_file, sc->sc_file,
			    sizeof(vnu->vnu_file));
			vnu->vnu_dev = vattr.va_fsid;
			vnu->vnu_ino = vattr.va_fileid;
		} else {
			vnu->vnu_dev = 0;
			vnu->vnu_ino = 0;
		}

		break;

	case DIOCRLDINFO:
		if ((sc->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		lp = malloc(sizeof(*lp), M_TEMP, M_WAITOK);
		vndgetdisklabel(dev, sc, lp, 0);
		*(sc->sc_dk.dk_label) = *lp;
		free(lp, M_TEMP);
		return (0);

	case DIOCGPDINFO:
		if ((sc->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		vndgetdisklabel(dev, sc, (struct disklabel *)addr, 1);
		return (0);

	case DIOCGDINFO:
		if ((sc->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		*(struct disklabel *)addr = *(sc->sc_dk.dk_label);
		return (0);

	case DIOCGPART:
		if ((sc->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		((struct partinfo *)addr)->disklab = sc->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &sc->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return (0);

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((sc->sc_flags & VNF_HAVELABEL) == 0)
			return (ENOTTY);
		if ((flag & FWRITE) == 0)
			return (EBADF);

		if ((error = disk_lock(&sc->sc_dk)) != 0)
			return (error);

		error = setdisklabel(sc->sc_dk.dk_label,
		    (struct disklabel *)addr, sc->sc_dk.dk_openmask);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    vndstrategy, sc->sc_dk.dk_label);
		}

		disk_unlock(&sc->sc_dk);
		return (error);

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
vndsetcred(struct vnd_softc *sc, struct ucred *cred)
{
	struct uio auio;
	struct iovec aiov;
	char *tmpbuf;
	int error;
	struct proc *p = curproc;

	sc->sc_cred = crdup(cred);
	tmpbuf = malloc(DEV_BSIZE, M_TEMP, M_WAITOK);

	/* XXX: Horrible kludge to establish credentials for NFS */
	aiov.iov_base = tmpbuf;
	aiov.iov_len = MIN(DEV_BSIZE, sc->sc_size * sc->sc_secsize);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = 0;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_resid = aiov.iov_len;
	vn_lock(sc->sc_vp, LK_RETRY | LK_EXCLUSIVE, p);
	error = VOP_READ(sc->sc_vp, &auio, 0, sc->sc_cred);
	VOP_UNLOCK(sc->sc_vp, 0, p);

	free(tmpbuf, M_TEMP);
	return (error);
}

void
vndclear(struct vnd_softc *sc)
{
	struct vnode *vp = sc->sc_vp;
	struct proc *p = curproc;		/* XXX */

	DNPRINTF(VDB_FOLLOW, "vndclear(%p): vp %p\n", sc, vp);

	if (vp == NULL)
		panic("vndioctl: null vp");
	(void) vn_close(vp, VNDRW(sc), sc->sc_cred, p);
	crfree(sc->sc_cred);
	sc->sc_flags = 0;
	sc->sc_vp = NULL;
	sc->sc_cred = NULL;
	sc->sc_size = 0;
	bzero(sc->sc_file, sizeof(sc->sc_file));
}

daddr64_t
vndsize(dev_t dev)
{
	int unit = DISKUNIT(dev);
	struct vnd_softc *sc = &vnd_softc[unit];

	if (unit >= numvnd || (sc->sc_flags & VNF_INITED) == 0)
		return (-1);
	return (sc->sc_size * (sc->sc_secsize / DEV_BSIZE));
}

int
vnddump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{

	/* Not implemented. */
	return (ENXIO);
}
