/*	$OpenBSD: ccd.c,v 1.74 2007/06/05 00:38:20 deraadt Exp $	*/
/*	$NetBSD: ccd.c,v 1.33 1996/05/05 04:21:14 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * Copyright (c) 1997 Niklas Hallqvist.
 * Copyright (c) 2005 Michael Shalayeff.
 * All rights reserved.
 * 
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
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
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 */

/*
 * "Concatenated" disk driver.
 *
 * Dynamic configuration and disklabel support by:
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 *	Numerical Aerodynamic Simulation Facility
 *	Mail Stop 258-6
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 *
 * Mirroring support based on code written by Satoshi Asami
 * and Nisha Talagala.
 */
/* #define	CCDDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/namei.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <sys/rwlock.h>

#include <dev/ccdvar.h>

#ifdef __GNUC__
#define INLINE static __inline
#else
#define INLINE
#endif

/*
 * A concatenated disk is described after initialization by this structure.
 */
struct ccd_softc {
	struct disk	sc_dkdev;		/* generic disk device info */
	struct ccdgeom	sc_geom;		/* pseudo geometry info */
	struct ccdcinfo	*sc_cinfo;		/* component info */
	struct ccdiinfo	*sc_itable;		/* interleave table */
	char		sc_xname[8];		/* XXX external name */
	size_t		sc_size;		/* size of ccd */
	int		sc_flags;		/* flags */
	int		sc_cflags;		/* copy of ccd_flags */
	int		sc_ileave;		/* interleave */
	u_int		sc_nccdisks;		/* # of components */
	u_int		sc_nccunits;		/* # of components for data */
	struct rwlock	sc_rwlock;		/* lock */

};

/* sc_flags */
#define CCDF_INITED	0x01	/* unit has been initialized */
#define CCDF_WLABEL	0x02	/* label area is writable */
#define CCDF_LABELLING	0x04	/* unit is currently being labelled */

#ifdef CCDDEBUG
#define CCD_DCALL(m,c)		if (ccddebug & (m)) c
#define CCD_DPRINTF(m,a)	CCD_DCALL(m, printf a)
#define CCDB_FOLLOW	0x01
#define CCDB_INIT	0x02
#define CCDB_IO		0x04
#define CCDB_LABEL	0x08
#define CCDB_VNODE	0x10
int ccddebug = 0x00;
#else
#define CCD_DCALL(m,c)		/* m, c */
#define CCD_DPRINTF(m,a)	/* m, a */
#endif

struct ccdbuf {
	struct buf	cb_buf;		/* new I/O buf */
	struct buf	*cb_obp;	/* ptr. to original I/O buf */
	struct ccd_softc*cb_sc;		/* point back to the device */
	struct ccdbuf	*cb_dep;	/* mutual ptrs for mirror part */
	int		cb_comp;	/* target component */
	int		cb_flags;	/* misc. flags */
#define CBF_MIRROR	0x01		/* we're for a mirror component */
#define CBF_DONE	0x02		/* this buffer is done */
};

/* called by main() at boot time */
void	ccdattach(int);

/* called by biodone() at interrupt time */
void	ccdiodone(struct buf *);
int	ccdsize(dev_t);

void	ccdstart(struct ccd_softc *, struct buf *);
void	ccdinterleave(struct ccd_softc *);
void	ccdintr(struct ccd_softc *, struct buf *);
int	ccdinit(struct ccddevice *, char **, struct proc *);
int	ccdlookup(char *, struct proc *p, struct vnode **);
long	ccdbuffer(struct ccd_softc *, struct buf *, daddr_t, caddr_t,
    long, struct ccdbuf **);
void	ccdgetdisklabel(dev_t, struct ccd_softc *, struct disklabel *,
    struct cpu_disklabel *, int);
INLINE struct ccdbuf *getccdbuf(void);
INLINE void putccdbuf(struct ccdbuf *);

#define ccdlock(sc) rw_enter(&sc->sc_rwlock, RW_WRITE|RW_INTR)
#define ccdunlock(sc) rw_exit_write(&sc->sc_rwlock)

#ifdef CCDDEBUG
void	printiinfo(struct ccdiinfo *);
#endif

/* Non-private for the benefit of libkvm. */
struct	ccd_softc *ccd_softc;
struct	ccddevice *ccddevs;
int	numccd = 0;

/*
 * struct ccdbuf allocator
 */
struct pool	ccdbufpl;

/*
 * Manage the ccd buffer structures.
 */
INLINE struct ccdbuf *
getccdbuf(void)
{
	struct ccdbuf *cbp;

	if ((cbp = pool_get(&ccdbufpl, PR_WAITOK)))
		bzero(cbp, sizeof(struct ccdbuf));
	return (cbp);
}

INLINE void
putccdbuf(struct ccdbuf *cbp)
{
	pool_put(&ccdbufpl, cbp);
}

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later.
 */
void
ccdattach(int num)
{
	int i;

	if (num <= 0) {
#ifdef DIAGNOSTIC
		panic("ccdattach: count <= 0");
#endif
		return;
	}

	ccd_softc = (struct ccd_softc *)malloc(num * sizeof(struct ccd_softc),
	    M_DEVBUF, M_NOWAIT);
	ccddevs = (struct ccddevice *)malloc(num * sizeof(struct ccddevice),
	    M_DEVBUF, M_NOWAIT);
	if ((ccd_softc == NULL) || (ccddevs == NULL)) {
		printf("WARNING: no memory for concatenated disks\n");
		if (ccd_softc != NULL)
			free(ccd_softc, M_DEVBUF);
		if (ccddevs != NULL)
			free(ccddevs, M_DEVBUF);
		return;
	}
	for (i = 0; i < num; i++) {
		rw_init(&ccd_softc[i].sc_rwlock, "ccdlock");
	}
	numccd = num;
	bzero(ccd_softc, num * sizeof(struct ccd_softc));
	bzero(ccddevs, num * sizeof(struct ccddevice));

	pool_init(&ccdbufpl, sizeof(struct ccdbuf), 0, 0, 0, "ccdbufpl", NULL);
	pool_setlowat(&ccdbufpl, 16);
	pool_sethiwat(&ccdbufpl, 1024);
}

int
ccdinit(struct ccddevice *ccd, char **cpaths, struct proc *p)
{
	struct ccd_softc *cs = &ccd_softc[ccd->ccd_unit];
	struct ccdcinfo *ci = NULL;
	size_t size;
	int ix, rpm;
	struct vnode *vp;
	struct vattr va;
	size_t minsize;
	int maxsecsize;
	struct partinfo dpart;
	struct ccdgeom *ccg = &cs->sc_geom;
	char tmppath[MAXPATHLEN];
	int error;

	CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT, ("ccdinit: unit %d cflags %b\n",
	    ccd->ccd_unit, ccd->ccd_flags, CCDF_BITS));

	cs->sc_size = 0;
	cs->sc_ileave = ccd->ccd_interleave;
	cs->sc_nccdisks = ccd->ccd_ndev;
	if (snprintf(cs->sc_xname, sizeof(cs->sc_xname), "ccd%d",
	    ccd->ccd_unit) >= sizeof(cs->sc_xname)) {
		printf("ccdinit: device name too long.\n");
		return(ENXIO);
	}

	/* Allocate space for the component info. */
	cs->sc_cinfo = malloc(cs->sc_nccdisks * sizeof(struct ccdcinfo),
	    M_DEVBUF, M_WAITOK);
	bzero(cs->sc_cinfo, cs->sc_nccdisks * sizeof(struct ccdcinfo));

	/*
	 * Verify that each component piece exists and record
	 * relevant information about it.
	 */
	maxsecsize = 0;
	minsize = 0;
	rpm = 0;
	for (ix = 0; ix < cs->sc_nccdisks; ix++) {
		vp = ccd->ccd_vpp[ix];
		ci = &cs->sc_cinfo[ix];
		ci->ci_vp = vp;

		/*
		 * Copy in the pathname of the component.
		 */
		bzero(tmppath, sizeof(tmppath));	/* sanity */
		error = copyinstr(cpaths[ix], tmppath,
		    MAXPATHLEN, &ci->ci_pathlen);
		if (error) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: can't copy path, error = %d\n",
			    cs->sc_xname, error));
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_path = malloc(ci->ci_pathlen, M_DEVBUF, M_WAITOK);
		bcopy(tmppath, ci->ci_path, ci->ci_pathlen);

		/*
		 * XXX: Cache the component's dev_t.
		 */
		if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: %s: getattr failed error = %d\n",
			    cs->sc_xname, ci->ci_path, error));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_dev = va.va_rdev;

		/*
		 * Get partition information for the component.
		 */
		error = VOP_IOCTL(vp, DIOCGPART, (caddr_t)&dpart,
		    FREAD, p->p_ucred, p);
		if (error) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: %s: ioctl failed, error = %d\n",
			    cs->sc_xname, ci->ci_path, error));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		if (dpart.part->p_fstype == FS_CCD ||
		    dpart.part->p_fstype == FS_BSDFFS) {
			maxsecsize =
			    ((dpart.disklab->d_secsize > maxsecsize) ?
			    dpart.disklab->d_secsize : maxsecsize);
			size = DL_GETPSIZE(dpart.part);
		} else {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: %s: incorrect partition type\n",
			    cs->sc_xname, ci->ci_path));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EFTYPE);
		}

		/*
		 * Calculate the size, truncating to an interleave
		 * boundary if necessary.
		 */
		if (cs->sc_ileave > 1)
			size -= size % cs->sc_ileave;

		if (size == 0) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: %s: size == 0\n", cs->sc_xname, ci->ci_path));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (ENODEV);
		}

		if (minsize == 0 || size < minsize)
			minsize = size;
		ci->ci_size = size;
		cs->sc_size += size;
		rpm += dpart.disklab->d_rpm;
	}
	ccg->ccg_rpm = rpm / cs->sc_nccdisks;

	/*
	 * Don't allow the interleave to be smaller than
	 * the biggest component sector.
	 */
	if ((cs->sc_ileave > 0) &&
	    (cs->sc_ileave < (maxsecsize / DEV_BSIZE))) {
		CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
		    ("%s: interleave must be at least %d\n",
		    cs->sc_xname, (maxsecsize / DEV_BSIZE)));
		free(ci->ci_path, M_DEVBUF);
		free(cs->sc_cinfo, M_DEVBUF);
		return (EINVAL);
	}

	/*
	 * Mirroring support requires uniform interleave and
	 * and even number of components.
	 */
	if (ccd->ccd_flags & CCDF_MIRROR) {
		ccd->ccd_flags |= CCDF_UNIFORM;
		if (cs->sc_ileave == 0) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: mirroring requires interleave\n",
			    cs->sc_xname));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EINVAL);
		}
		if (cs->sc_nccdisks % 2) {
			CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
			    ("%s: mirroring requires even # of components\n",
			    cs->sc_xname));
			free(ci->ci_path, M_DEVBUF);
			free(cs->sc_cinfo, M_DEVBUF);
			return (EINVAL);
		}
	}

	/*
	 * If uniform interleave is desired set all sizes to that of
	 * the smallest component.
	 */
	ccg->ccg_ntracks = cs->sc_nccunits = cs->sc_nccdisks;
	if (ccd->ccd_flags & CCDF_UNIFORM) {
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			ci->ci_size = minsize;

		if (ccd->ccd_flags & CCDF_MIRROR)
			cs->sc_nccunits = ccg->ccg_ntracks /= 2;
		cs->sc_size = ccg->ccg_ntracks * minsize;
	}

	cs->sc_cflags = ccd->ccd_flags;	/* So we can find out later... */

	/*
	 * Construct the interleave table.
	 */
	ccdinterleave(cs);

	/*
	 * Create pseudo-geometry based on 1MB cylinders.  It's
	 * pretty close.
	 */
	ccg->ccg_secsize = DEV_BSIZE;
	ccg->ccg_nsectors = cs->sc_ileave? cs->sc_ileave :
	    1024 * (1024 / ccg->ccg_secsize);
	ccg->ccg_ncylinders = cs->sc_size / ccg->ccg_ntracks /
	    ccg->ccg_nsectors;

	cs->sc_flags |= CCDF_INITED;

	return (0);
}

void
ccdinterleave(struct ccd_softc *cs)
{
	struct ccdcinfo *ci, *smallci;
	struct ccdiinfo *ii;
	daddr_t bn, lbn;
	int ix;
	u_long size;

	CCD_DPRINTF(CCDB_INIT,
	    ("ccdinterleave(%p): ileave %d\n", cs, cs->sc_ileave));

	/*
	 * Allocate an interleave table.
	 * Chances are this is too big, but we don't care.
	 */
	size = (cs->sc_nccdisks + 1) * sizeof(struct ccdiinfo);
	cs->sc_itable = (struct ccdiinfo *)malloc(size, M_DEVBUF, M_WAITOK);
	bzero((caddr_t)cs->sc_itable, size);

	/*
	 * Trivial case: no interleave (actually interleave of disk size).
	 * Each table entry represents a single component in its entirety.
	 */
	if (cs->sc_ileave == 0) {
		bn = 0;
		ii = cs->sc_itable;

		for (ix = 0; ix < cs->sc_nccdisks; ix++) {
			/* Allocate space for ii_index. */
			ii->ii_index = malloc(sizeof(int), M_DEVBUF, M_WAITOK);
			ii->ii_ndisk = 1;
			ii->ii_startblk = bn;
			ii->ii_startoff = 0;
			ii->ii_index[0] = ix;
			bn += cs->sc_cinfo[ix].ci_size;
			ii++;
		}
		ii->ii_ndisk = 0;

		CCD_DCALL(CCDB_INIT, printiinfo(cs->sc_itable));
		return;
	}

	/*
	 * The following isn't fast or pretty; it doesn't have to be.
	 */
	size = 0;
	bn = lbn = 0;
	for (ii = cs->sc_itable; ; ii++) {
		/* Allocate space for ii_index. */
		ii->ii_index = malloc((sizeof(int) * cs->sc_nccdisks),
		    M_DEVBUF, M_WAITOK);

		/*
		 * Locate the smallest of the remaining components
		 */
		smallci = NULL;
		for (ci = cs->sc_cinfo;
		    ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size > size &&
			    (smallci == NULL ||
			    ci->ci_size < smallci->ci_size))
				smallci = ci;

		/*
		 * Nobody left, all done
		 */
		if (smallci == NULL) {
			ii->ii_ndisk = 0;
			break;
		}

		/*
		 * Record starting logical block and component offset
		 */
		ii->ii_startblk = bn / cs->sc_ileave;
		ii->ii_startoff = lbn;

		/*
		 * Determine how many disks take part in this interleave
		 * and record their indices.
		 */
		ix = 0;
		for (ci = cs->sc_cinfo;
		    ci < &cs->sc_cinfo[cs->sc_nccunits]; ci++)
			if (ci->ci_size >= smallci->ci_size)
				ii->ii_index[ix++] = ci - cs->sc_cinfo;
		ii->ii_ndisk = ix;
		bn += ix * (smallci->ci_size - size);
		lbn = smallci->ci_size / cs->sc_ileave;
		size = smallci->ci_size;
	}

	CCD_DCALL(CCDB_INIT, printiinfo(cs->sc_itable));
}

/* ARGSUSED */
int
ccdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct ccd_softc *cs;
	struct disklabel *lp;
	int error = 0, part, pmask;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdopen(%x, %x)\n", dev, flags));

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((error = ccdlock(cs)) != 0)
		return (error);

	lp = cs->sc_dkdev.dk_label;

	part = DISKPART(dev);
	pmask = (1 << part);

	/*
	 * If we're initialized, check to see if there are any other
	 * open partitions.  If not, then it's safe to update
	 * the in-core disklabel.
	 */
	if ((cs->sc_flags & CCDF_INITED) && (cs->sc_dkdev.dk_openmask == 0))
		ccdgetdisklabel(dev, cs, lp, cs->sc_dkdev.dk_cpulabel, 0);

	/* Check that the partition exists. */
	if (part != RAW_PART) {
		if (((cs->sc_flags & CCDF_INITED) == 0) ||
		    ((part >= lp->d_npartitions) ||
		    (lp->d_partitions[part].p_fstype == FS_UNUSED))) {
			error = ENXIO;
			goto done;
		}
	}

	/* Prevent our unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

 done:
	ccdunlock(cs);
	return (error);
}

/* ARGSUSED */
int
ccdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct ccd_softc *cs;
	int error = 0, part;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdclose(%x, %x)\n", dev, flags));

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((error = ccdlock(cs)) != 0)
		return (error);

	part = DISKPART(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

	ccdunlock(cs);
	return (0);
}

void
ccdstrategy(struct buf *bp)
{
	int unit = DISKUNIT(bp->b_dev);
	struct ccd_softc *cs = &ccd_softc[unit];
	int s;
	int wlabel;
	struct disklabel *lp;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdstrategy(%p): unit %d\n", bp, unit));

	if ((cs->sc_flags & CCDF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_resid = bp->b_bcount;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* If it's a nil transfer, wake up the top half now. */
	if (bp->b_bcount == 0)
		goto done;

	lp = cs->sc_dkdev.dk_label;

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.
	 */
	wlabel = cs->sc_flags & (CCDF_WLABEL|CCDF_LABELLING);
	if (DISKPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, lp, cs->sc_dkdev.dk_cpulabel,
	    wlabel) <= 0)
		goto done;

	bp->b_resid = bp->b_bcount;

	/*
	 * "Start" the unit.
	 */
	s = splbio();
	ccdstart(cs, bp);
	splx(s);
	return;
done:
	s = splbio();
	biodone(bp);
	splx(s);
}

void
ccdstart(struct ccd_softc *cs, struct buf *bp)
{
	long bcount, rcount;
	struct ccdbuf **cbpp;
	caddr_t addr;
	daddr_t bn;
	struct partition *pp;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdstart(%p, %p, %s)\n", cs, bp,
	    bp->b_flags & B_READ? "read" : "write"));

	/* Instrumentation. */
	disk_busy(&cs->sc_dkdev);

	/*
	 * Translate the partition-relative block number to an absolute.
	 */
	bn = bp->b_blkno;
	if (DISKPART(bp->b_dev) != RAW_PART) {
		pp = &cs->sc_dkdev.dk_label->d_partitions[DISKPART(bp->b_dev)];
		bn += DL_GETPOFFSET(pp);
	}

	/*
	 * Allocate component buffers
	 */
	cbpp = malloc(2 * cs->sc_nccdisks * sizeof(struct ccdbuf *), M_DEVBUF,
	    M_WAITOK);
	bzero(cbpp, 2 * cs->sc_nccdisks * sizeof(struct ccdbuf *));
	addr = bp->b_data;
	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		rcount = ccdbuffer(cs, bp, bn, addr, bcount, cbpp);
		
		/*
		 * This is the old, slower, but less restrictive, mode of
		 * operation.  It allows interleaves which are not multiples
		 * of PAGE_SIZE and mirroring.
		 */
		if ((cbpp[0]->cb_buf.b_flags & B_READ) == 0)
			cbpp[0]->cb_buf.b_vp->v_numoutput++;
		VOP_STRATEGY(&cbpp[0]->cb_buf);

		if ((cs->sc_cflags & CCDF_MIRROR) &&
		    ((cbpp[0]->cb_buf.b_flags & B_READ) == 0)) {
			cbpp[1]->cb_buf.b_vp->v_numoutput++;
			VOP_STRATEGY(&cbpp[1]->cb_buf);
		}

		bn += btodb(rcount);
		addr += rcount;
	}

	free(cbpp, M_DEVBUF);
}

/*
 * Build a component buffer header.
 */
long
ccdbuffer(struct ccd_softc *cs, struct buf *bp, daddr_t bn, caddr_t addr,
    long bcount, struct ccdbuf **cbpp)
{
	struct ccdcinfo *ci, *ci2 = NULL;
	struct ccdbuf *cbp;
	daddr_t cbn, cboff, sblk;
	int ccdisk, ccdisk2, off;
	long cnt;
	struct ccdiinfo *ii;
	struct buf *nbp;

	CCD_DPRINTF(CCDB_IO, ("ccdbuffer(%p, %p, %d, %p, %ld, %p)\n",
	    cs, bp, bn, addr, bcount, cbpp));

	/*
	 * Determine which component bn falls in.
	 */
	cbn = bn;
	cboff = 0;

	if (cs->sc_ileave == 0) {
		/*
		 * Serially concatenated
		 */
		sblk = 0;
		for (ccdisk = 0, ci = &cs->sc_cinfo[ccdisk];
		    cbn >= sblk + ci->ci_size;
		    ccdisk++, ci = &cs->sc_cinfo[ccdisk])
			sblk += ci->ci_size;
		cbn -= sblk;
	} else {
		/*
		 * Interleaved
		 */
		cboff = cbn % cs->sc_ileave;
		cbn /= cs->sc_ileave;
		for (ii = cs->sc_itable; ii->ii_ndisk; ii++)
			if (ii->ii_startblk > cbn)
				break;
		ii--;
		off = cbn - ii->ii_startblk;
		if (ii->ii_ndisk == 1) {
			ccdisk = ii->ii_index[0];
			cbn = ii->ii_startoff + off;
		} else {
			ccdisk = ii->ii_index[off % ii->ii_ndisk];
			cbn = ii->ii_startoff + off / ii->ii_ndisk;
		}
		if (cs->sc_cflags & CCDF_MIRROR) {
			/* Mirrored data */
			ccdisk2 = ccdisk + ii->ii_ndisk;
			ci2 = &cs->sc_cinfo[ccdisk2];
			/* spread the read over both parts */
			if (bp->b_flags & B_READ &&
			    bcount > bp->b_bcount / 2 &&
			    (!(ci2->ci_flags & CCIF_FAILED) ||
			      ci->ci_flags & CCIF_FAILED))
				ccdisk = ccdisk2;
		}
		cbn *= cs->sc_ileave;
		ci = &cs->sc_cinfo[ccdisk];
		CCD_DPRINTF(CCDB_IO, ("ccdisk %d cbn %d ci %p ci2 %p\n",
		    ccdisk, cbn, ci, ci2));
	}

	/* Limit the operation at next component border */
	if (cs->sc_ileave == 0)
		cnt = dbtob(ci->ci_size - cbn);
	else
		cnt = dbtob(cs->sc_ileave - cboff);
	if (cnt < bcount)
		bcount = cnt;

	/*
	 * Setup new component buffer.
	 */
	cbp = cbpp[0] = getccdbuf();
	cbp->cb_flags = 0;
	nbp = &cbp->cb_buf;
	nbp->b_flags = bp->b_flags | B_CALL;
	nbp->b_iodone = ccdiodone;
	nbp->b_proc = bp->b_proc;
	nbp->b_dev = ci->ci_dev;		/* XXX */
	nbp->b_blkno = cbn + cboff;
	nbp->b_vp = ci->ci_vp;
	nbp->b_bcount = bcount;
	LIST_INIT(&nbp->b_dep);
	nbp->b_data = addr;

	/*
	 * context for ccdiodone
	 */
	cbp->cb_obp = bp;
	cbp->cb_sc = cs;
	cbp->cb_comp = ccdisk;

	/*
	 * Mirrors have an additional write operation that is nearly
	 * identical to the first.
	 */
	if ((cs->sc_cflags & CCDF_MIRROR) &&
	    !(ci2->ci_flags & CCIF_FAILED) &&
	    ((cbp->cb_buf.b_flags & B_READ) == 0)) {
		struct ccdbuf *cbp2;
		cbpp[1] = cbp2 = getccdbuf();
		*cbp2 = *cbp;
		cbp2->cb_flags = CBF_MIRROR;
		cbp2->cb_buf.b_dev = ci2->ci_dev;	/* XXX */
		cbp2->cb_buf.b_vp = ci2->ci_vp;
		LIST_INIT(&cbp2->cb_buf.b_dep);
		cbp2->cb_comp = ccdisk2;
		cbp2->cb_dep = cbp;
		cbp->cb_dep = cbp2;
	}

	CCD_DPRINTF(CCDB_IO, (" dev %x(u%d): cbp %p bn %d addr %p bcnt %ld\n",
	    ci->ci_dev, ci-cs->sc_cinfo, cbp, bp->b_blkno,
	    bp->b_data, bp->b_bcount));

	return (bcount);
}

void
ccdintr(struct ccd_softc *cs, struct buf *bp)
{

	splassert(IPL_BIO);

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdintr(%p, %p)\n", cs, bp));

	/*
	 * Request is done for better or worse, wakeup the top half.
	 */
	if (bp->b_flags & B_ERROR)
		bp->b_resid = bp->b_bcount;
	disk_unbusy(&cs->sc_dkdev, (bp->b_bcount - bp->b_resid),
	    (bp->b_flags & B_READ));
	biodone(bp);
}

/*
 * Called at interrupt time.
 * Mark the component as done and if all components are done,
 * take a ccd interrupt.
 */
void
ccdiodone(struct buf *vbp)
{
	struct ccdbuf *cbp = (struct ccdbuf *)vbp;
	struct buf *bp = cbp->cb_obp;
	struct ccd_softc *cs = cbp->cb_sc;
	long count = bp->b_bcount;
	char *comptype;

	splassert(IPL_BIO);

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdiodone(%p)\n", cbp));
	CCD_DPRINTF(CCDB_IO, (cbp->cb_flags & CBF_MIRROR?
	    "ccdiodone: mirror component\n" : 
	    "ccdiodone: bp %p bcount %ld resid %ld\n",
	    bp, bp->b_bcount, bp->b_resid));
	CCD_DPRINTF(CCDB_IO, (" dev %x(u%d), cbp %p bn %d addr %p bcnt %ld\n",
	    vbp->b_dev, cbp->cb_comp, cbp, vbp->b_blkno,
	    vbp->b_data, vbp->b_bcount));

	if (vbp->b_flags & B_ERROR) {
		cs->sc_cinfo[cbp->cb_comp].ci_flags |= CCIF_FAILED;
		if (cbp->cb_flags & CBF_MIRROR)
			comptype = " (mirror)";
		else {
			bp->b_flags |= B_ERROR;
			bp->b_error = vbp->b_error ?
			    vbp->b_error : EIO;
			comptype = "";
		}

		printf("%s: error %d on component %d%s\n",
		    cs->sc_xname, bp->b_error, cbp->cb_comp, comptype);
	}
	cbp->cb_flags |= CBF_DONE;

	if (cbp->cb_dep &&
	    (cbp->cb_dep->cb_flags & CBF_DONE) != (cbp->cb_flags & CBF_DONE))
		return;

	if (cbp->cb_flags & CBF_MIRROR &&
	    !(cbp->cb_dep->cb_flags & CBF_MIRROR)) {
		cbp = cbp->cb_dep;
		vbp = (struct buf *)cbp;
	}

	count = vbp->b_bcount;

	putccdbuf(cbp);
	if (cbp->cb_dep)
		putccdbuf(cbp->cb_dep);

	/*
	 * If all done, "interrupt".
	 *
	 * Note that mirror component buffers aren't counted against
	 * the original I/O buffer.
	 */
	if (count > bp->b_resid)
		panic("ccdiodone: count");
	bp->b_resid -= count;
	if (bp->b_resid == 0)
		ccdintr(cs, bp);
}

/* ARGSUSED */
int
ccdread(dev_t dev, struct uio *uio, int flags)
{
	int unit = DISKUNIT(dev);
	struct ccd_softc *cs;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdread(%x, %p)\n", dev, uio));

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	/*
	 * XXX: It's not clear that using minphys() is completely safe,
	 * in particular, for raw I/O.  Underlying devices might have some
	 * non-obvious limits, because of the copy to user-space.
	 */
	return (physio(ccdstrategy, NULL, dev, B_READ, minphys, uio));
}

/* ARGSUSED */
int
ccdwrite(dev_t dev, struct uio *uio, int flags)
{
	int unit = DISKUNIT(dev);
	struct ccd_softc *cs;

	CCD_DPRINTF(CCDB_FOLLOW, ("ccdwrite(%x, %p)\n", dev, uio));

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (ENXIO);

	/*
	 * XXX: It's not clear that using minphys() is completely safe,
	 * in particular, for raw I/O.  Underlying devices might have some
	 * non-obvious limits, because of the copy to user-space.
	 */
	return (physio(ccdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
ccdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int unit = DISKUNIT(dev);
	int i, j, lookedup = 0, error = 0;
	int part, pmask, s;
	struct ccd_softc *cs;
	struct ccd_ioctl *ccio = (struct ccd_ioctl *)data;
	struct ccddevice ccd;
	char **cpp;
	struct vnode **vpp;

	if (unit >= numccd)
		return (ENXIO);

	cs = &ccd_softc[unit];
	if (cmd != CCDIOCSET && !(cs->sc_flags & CCDF_INITED))
		return (ENXIO);

	/* access control */
	switch (cmd) {
	case CCDIOCSET:
	case CCDIOCCLR:
	case DIOCWDINFO:
	case DIOCSDINFO:
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
	}

	bzero(&ccd, sizeof(ccd));
	switch (cmd) {
	case CCDIOCSET:
		if (cs->sc_flags & CCDF_INITED)
			return (EBUSY);

		if (ccio->ccio_ndisks == 0 || ccio->ccio_ndisks > INT_MAX ||
		    ccio->ccio_ileave < 0)
			return (EINVAL);

		if ((error = ccdlock(cs)) != 0)
			return (error);

		/* Fill in some important bits. */
		ccd.ccd_unit = unit;
		ccd.ccd_interleave = ccio->ccio_ileave;
		ccd.ccd_flags = ccio->ccio_flags & CCDF_USERMASK;

		/*
		 * Allocate space for and copy in the array of
		 * componet pathnames and device numbers.
		 */
		cpp = malloc(ccio->ccio_ndisks * sizeof(char *),
		    M_DEVBUF, M_WAITOK);
		vpp = malloc(ccio->ccio_ndisks * sizeof(struct vnode *),
		    M_DEVBUF, M_WAITOK);

		error = copyin((caddr_t)ccio->ccio_disks, (caddr_t)cpp,
		    ccio->ccio_ndisks * sizeof(char **));
		if (error) {
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

		for (i = 0; i < ccio->ccio_ndisks; ++i) {
			CCD_DPRINTF(CCDB_INIT,
			    ("ccdioctl: component %d: %p, lookedup = %d\n",
				i, cpp[i], lookedup));
			if ((error = ccdlookup(cpp[i], p, &vpp[i])) != 0) {
				for (j = 0; j < lookedup; ++j)
					(void)vn_close(vpp[j], FREAD|FWRITE,
					    p->p_ucred, p);
				free(vpp, M_DEVBUF);
				free(cpp, M_DEVBUF);
				ccdunlock(cs);
				return (error);
			}
			++lookedup;
		}
		ccd.ccd_cpp = cpp;
		ccd.ccd_vpp = vpp;
		ccd.ccd_ndev = ccio->ccio_ndisks;

		/*
		 * Initialize the ccd.  Fills in the softc for us.
		 */
		if ((error = ccdinit(&ccd, cpp, p)) != 0) {
			for (j = 0; j < lookedup; ++j)
				(void)vn_close(vpp[j], FREAD|FWRITE,
				    p->p_ucred, p);
			bzero(&ccd_softc[unit], sizeof(struct ccd_softc));
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

		/*
		 * The ccd has been successfully initialized, so
		 * we can place it into the array.  Don't try to
		 * read the disklabel until the disk has been attached,
		 * because space for the disklabel is allocated
		 * in disk_attach();
		 */
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));
		ccio->ccio_unit = unit;
		ccio->ccio_size = cs->sc_size;

		/* Attach the disk. */
		cs->sc_dkdev.dk_name = cs->sc_xname;
		disk_attach(&cs->sc_dkdev);

		/* Try and read the disklabel. */
		ccdgetdisklabel(dev, cs, cs->sc_dkdev.dk_label,
		    cs->sc_dkdev.dk_cpulabel, 0);

		ccdunlock(cs);
		break;

	case CCDIOCCLR:
		if ((error = ccdlock(cs)) != 0)
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = DISKPART(dev);
		pmask = (1 << part);
		if ((cs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((cs->sc_dkdev.dk_bopenmask & pmask) &&
		    (cs->sc_dkdev.dk_copenmask & pmask))) {
			ccdunlock(cs);
			return (EBUSY);
		}

		/*
		 * Free ccd_softc information and clear entry.
		 */

		/* Close the components and free their pathnames. */
		for (i = 0; i < cs->sc_nccdisks; ++i) {
			/*
			 * XXX: this close could potentially fail and
			 * cause Bad Things.  Maybe we need to force
			 * the close to happen?
			 */
#ifdef DIAGNOSTIC
			CCD_DCALL(CCDB_VNODE, vprint("CCDIOCCLR: vnode info",
			    cs->sc_cinfo[i].ci_vp));
#endif

			(void)vn_close(cs->sc_cinfo[i].ci_vp, FREAD|FWRITE,
			    p->p_ucred, p);
			free(cs->sc_cinfo[i].ci_path, M_DEVBUF);
		}

		/* Free interleave index. */
		for (i = 0; cs->sc_itable[i].ii_ndisk; ++i)
			free(cs->sc_itable[i].ii_index, M_DEVBUF);

		/* Free component info and interleave table. */
		free(cs->sc_cinfo, M_DEVBUF);
		free(cs->sc_itable, M_DEVBUF);
		cs->sc_flags &= ~CCDF_INITED;

		/*
		 * Free ccddevice information and clear entry.
		 */
		free(ccddevs[unit].ccd_cpp, M_DEVBUF);
		free(ccddevs[unit].ccd_vpp, M_DEVBUF);
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));

		/* Detatch the disk. */
		disk_detach(&cs->sc_dkdev);

		/* This must be atomic. */
		s = splhigh();
		ccdunlock(cs);
		bzero(cs, sizeof(struct ccd_softc));
		splx(s);
		break;

	case DIOCGPDINFO: {
		struct cpu_disklabel osdep;

		if ((error = ccdlock(cs)) != 0)
			return (error);

		ccdgetdisklabel(dev, cs, (struct disklabel *)data,
		    &osdep, 1);

		ccdunlock(cs);
		break;
	}

	case DIOCGDINFO:
		*(struct disklabel *)data = *(cs->sc_dkdev.dk_label);
		break;

	case DIOCGPART:
		((struct partinfo *)data)->disklab = cs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &cs->sc_dkdev.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((error = ccdlock(cs)) != 0)
			return (error);

		cs->sc_flags |= CCDF_LABELLING;

		error = setdisklabel(cs->sc_dkdev.dk_label,
		    (struct disklabel *)data, 0, cs->sc_dkdev.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    ccdstrategy, cs->sc_dkdev.dk_label,
				    cs->sc_dkdev.dk_cpulabel);
		}

		cs->sc_flags &= ~CCDF_LABELLING;

		ccdunlock(cs);

		if (error)
			return (error);
		break;

	case DIOCWLABEL:
		if (*(int *)data != 0)
			cs->sc_flags |= CCDF_WLABEL;
		else
			cs->sc_flags &= ~CCDF_WLABEL;
		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

int
ccdsize(dev_t dev)
{
	struct ccd_softc *cs;
	int part, size, unit;

	unit = DISKUNIT(dev);
	if (unit >= numccd)
		return (-1);

	cs = &ccd_softc[unit];
	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (-1);

	if (ccdopen(dev, 0, S_IFBLK, curproc))
		return (-1);

	part = DISKPART(dev);
	if (cs->sc_dkdev.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = DL_GETPSIZE(&cs->sc_dkdev.dk_label->d_partitions[part]);

	if (ccdclose(dev, 0, S_IFBLK, curproc))
		return (-1);

	return (size);
}

int
ccddump(dev_t dev, daddr_t blkno, caddr_t va, size_t size)
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 */
int
ccdlookup(char *path, struct proc *p, struct vnode **vpp)
{
	struct nameidata nd;
	struct vnode *vp;
	struct vattr va;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, path, p);
	if ((error = vn_open(&nd, FREAD|FWRITE, 0)) != 0) {
		CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
		    ("ccdlookup: vn_open error = %d\n", error));
		return (error);
	}
	vp = nd.ni_vp;

	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}

	if ((error = VOP_GETATTR(vp, &va, p->p_ucred, p)) != 0) {
		CCD_DPRINTF(CCDB_FOLLOW | CCDB_INIT,
		    ("ccdlookup: getattr error = %d\n", error));
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

#ifdef DIAGNOSTIC
	CCD_DCALL(CCDB_VNODE, vprint("ccdlookup: vnode info", vp));
#endif

	VOP_UNLOCK(vp, 0, p);
	*vpp = vp;
	return (0);
}

/*
 * Read the disklabel from the ccd.  If one is not present, fake one
 * up.
 */
void
ccdgetdisklabel(dev_t dev, struct ccd_softc *cs, struct disklabel *lp,
    struct cpu_disklabel *clp, int spoofonly)
{
	struct ccdgeom *ccg = &cs->sc_geom;
	char *errstring;

	bzero(lp, sizeof(*lp));
	bzero(clp, sizeof(*clp));

	DL_SETDSIZE(lp, cs->sc_size);
	lp->d_secsize = ccg->ccg_secsize;
	lp->d_nsectors = ccg->ccg_nsectors;
	lp->d_ntracks = ccg->ccg_ntracks;
	lp->d_ncylinders = ccg->ccg_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	lp->d_rpm = ccg->ccg_rpm;

	strncpy(lp->d_typename, "ccd", sizeof(lp->d_typename));
	lp->d_type = DTYPE_CCD;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(cs->sc_dkdev.dk_label);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	errstring = readdisklabel(DISKLABELDEV(dev), ccdstrategy,
	    cs->sc_dkdev.dk_label, cs->sc_dkdev.dk_cpulabel, spoofonly);
	/* It's actually extremely common to have unlabeled ccds. */
	if (errstring != NULL)
		CCD_DPRINTF(CCDB_LABEL, ("%s: %s\n", cs->sc_xname, errstring));
}

#ifdef CCDDEBUG
void
printiinfo(struct ccdiinfo *ii)
{
	int ix, i;

	for (ix = 0; ii->ii_ndisk; ix++, ii++) {
		printf(" itab[%d]: #dk %d sblk %d soff %d",
		       ix, ii->ii_ndisk, ii->ii_startblk, ii->ii_startoff);
		for (i = 0; i < ii->ii_ndisk; i++)
			printf(" %d", ii->ii_index[i]);
		printf("\n");
	}
}
#endif
