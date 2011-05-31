/*	$OpenBSD: ramdisk.c,v 1.51 2011/05/31 17:35:35 matthew Exp $	*/
/*	$NetBSD: ramdisk.c,v 1.8 1996/04/12 08:30:09 leo Exp $	*/

/*
 * Copyright (c) 1995 Gordon W. Ross, Leo Weppelman.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *			Gordon W. Ross and Leo Weppelman.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This implements a general-purpose RAM-disk.
 * See ramdisk.h for notes on the config types.
 *
 * Note that this driver provides the same functionality
 * as the MFS filesystem hack, but this is better because
 * you can use this for any filesystem type you'd like!
 *
 * Credit for most of the kmem ramdisk code goes to:
 *   Leo Weppelman (atari) and Phil Nelson (pc532)
 * Credit for the ideas behind the "user space RAM" code goes
 * to the authors of the MFS implementation.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/file.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>

#include <uvm/uvm_extern.h>

#include <dev/ramdisk.h>

/*
 * By default, include the user-space functionality.
 * Use:  option RAMDISK_SERVER=0 to turn it off.
 */
#if !defined(RAMDISK_SERVER) && !defined(SMALL_KERNEL)
#define	RAMDISK_SERVER 1
#endif

/*
 * XXX: the "control" unit is (base unit + 16).
 * We should just use the cdev as the "control", but
 * that interferes with the security stuff preventing
 * simultaneous use of raw and block devices.
 *
 * XXX Assumption: 16 RAM-disks are enough!
 */
#define RD_MAX_UNITS	0x10
#define RD_IS_CTRL(dev) (DISKPART(dev) == RAW_PART)

/* autoconfig stuff... */

struct rd_softc {
	struct device sc_dev;	/* REQUIRED first entry */
	struct disk sc_dk;	/* hook for generic disk handling */
	struct rd_conf sc_rd;
#if RAMDISK_SERVER
	struct buf *sc_buflist;
#endif
};
/* shorthand for fields in sc_rd: */
#define sc_addr sc_rd.rd_addr
#define sc_size sc_rd.rd_size
#define sc_type sc_rd.rd_type

void rdattach(int);
void rd_attach(struct device *, struct device *, void *);
void rdgetdisklabel(dev_t, struct rd_softc *, struct disklabel *, int);

/*
 * Some ports (like i386) use a swapgeneric that wants to
 * snoop around in this rd_cd structure.  It is preserved
 * (for now) to remain compatible with such practice.
 * XXX - that practice is questionable...
 */
struct cfdriver rd_cd = {
	NULL, "rd", DV_DISK
};

/*
 * Here we define a cfattach structure for inserting any new rd device into the
 * device tree. This is needed by some archs that look for bootable devices in
 * there.
 */
int  rd_probe(struct device *, void *, void *);
int  rd_detach(struct device *, int);

struct cfattach rd_ca = {
	sizeof(struct rd_softc), rd_probe, rd_attach,
	rd_detach
};

void rdstrategy(struct buf *bp);

int   ramdisk_ndevs;
void *ramdisk_devs[RD_MAX_UNITS];

/*
 * This is called if we are configured as a pseudo-device
 */
void
rdattach(int n)
{
	struct rd_softc *sc;
	struct cfdata *cf;
	int i;

#ifdef	DIAGNOSTIC
	if (ramdisk_ndevs) {
		printf("ramdisk: multiple attach calls?\n");
		return;
	}
#endif

	/* XXX:  Are we supposed to provide a default? */
	if (n < 1)
		n = 1;
	if (n > RD_MAX_UNITS)
		n = RD_MAX_UNITS;
	ramdisk_ndevs = n;

	/* XXX: Fake-up rd_cd (see above) */
	rd_cd.cd_ndevs = ramdisk_ndevs;
	rd_cd.cd_devs  = ramdisk_devs;

	/* Attach as if by autoconfig. */
	cf = malloc(sizeof(struct cfdata), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cf == NULL) {
		printf("WARNING: no memory for cfdata struct\n");
		return;
	}
	cf->cf_attach = &rd_ca;
	cf->cf_driver = &rd_cd;

	for (i = 0; i < n; i++) {
		sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
		if (snprintf(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname),
		    "rd%d", i) >= sizeof(sc->sc_dev.dv_xname)) {
			printf("rdattach: device name too long\n");
			free(sc, M_DEVBUF);
			return;
		}
		ramdisk_devs[i] = sc;
		sc->sc_dev.dv_unit = i;
		sc->sc_dev.dv_class = DV_DISK;
		sc->sc_dev.dv_parent = NULL;
		sc->sc_dev.dv_cfdata = cf;
		TAILQ_INSERT_TAIL(&alldevs, &sc->sc_dev, dv_list);
		device_ref(&sc->sc_dev);
		rd_attach(NULL, &sc->sc_dev, NULL);
	}
}

void
rd_attach(struct device *parent, struct device *self, void *aux)
{
	struct rd_softc *sc = (struct rd_softc *)self;

	/* XXX - Could accept aux info here to set the config. */
#ifdef	RAMDISK_HOOKS
	/*
	 * This external function might setup a pre-loaded disk.
	 * All it would need to do is setup the rd_conf struct.
	 * See sys/arch/sun3/dev/rd_root.c for an example.
	 */
	rd_attach_hook(sc->sc_dev.dv_unit, &sc->sc_rd);
#endif

	/*
	 * Initialize and attach the disk structure.
	 */
	sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dev, &sc->sc_dk);
}

/*
 * operational routines:
 * open, close, read, write, strategy,
 * ioctl, dump, size
 */

#if RAMDISK_SERVER
int rd_server_loop(struct rd_softc *sc);
int rd_ioctl_server(struct rd_softc *sc,
		struct rd_conf *urd, struct proc *proc);
#endif
int rd_ioctl_kalloc(struct rd_softc *sc,
		struct rd_conf *urd, struct proc *proc);

dev_type_open(rdopen);
dev_type_close(rdclose);
dev_type_read(rdread);
dev_type_write(rdwrite);
dev_type_ioctl(rdioctl);
dev_type_size(rdsize);
dev_type_dump(rddump);

int
rddump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{
	return (ENODEV);
}

daddr64_t
rdsize(dev_t dev)
{
	int part, unit;
	struct rd_softc *sc;

	/* Disallow control units. */
	unit = DISKUNIT(dev);
	if (unit >= ramdisk_ndevs)
		return (0);
	sc = ramdisk_devs[unit];
	if (sc == NULL)
		return (0);

	if (sc->sc_type == RD_UNCONFIGURED)
		return (0);

	rdgetdisklabel(dev, sc, sc->sc_dk.dk_label, 0);
	part = DISKPART(dev);
	if (part >= sc->sc_dk.dk_label->d_npartitions)
		return (0);
	return (DL_GETPSIZE(&sc->sc_dk.dk_label->d_partitions[part]) *
	    (sc->sc_dk.dk_label->d_secsize / DEV_BSIZE));
}

int
rdopen(dev_t dev, int flag, int fmt, struct proc *proc)
{
	int unit;
	struct rd_softc *sc;

	unit = DISKUNIT(dev);
	if (unit >= ramdisk_ndevs)
		return (ENXIO);
	sc = ramdisk_devs[unit];
	if (sc == NULL)
		return (ENXIO);

	/*
	 * The control device is not exclusive, and can
	 * open uninitialized units (so you can setconf).
	 */
	if (RD_IS_CTRL(dev))
		return (0);

#ifdef	RAMDISK_HOOKS
	/* Call the open hook to allow loading the device. */
	rd_open_hook(unit, &sc->sc_rd);
#endif

	/*
	 * This is a normal, "slave" device, so
	 * enforce initialized, exclusive open.
	 */
	if (sc->sc_type == RD_UNCONFIGURED)
		return (ENXIO);

	/*
	 * Make sure we have read the disklabel.
	 */
	rdgetdisklabel(dev, sc, sc->sc_dk.dk_label, 0);
	return (0);
}

int
rdclose(dev_t dev, int flag, int fmt, struct proc *proc)
{
	return (0);
}

int
rdread(dev_t dev, struct uio *uio, int flags)
{
	return (physio(rdstrategy, dev, B_READ, minphys, uio));
}

int
rdwrite(dev_t dev, struct uio *uio, int flags)
{
	return (physio(rdstrategy, dev, B_WRITE, minphys, uio));
}

/*
 * Handle I/O requests, either directly, or
 * by passing them to the server process.
 */
void
rdstrategy(struct buf *bp)
{
	int unit, s;
	struct rd_softc *sc;
	caddr_t addr;
	size_t off, xfer;

	unit = DISKUNIT(bp->b_dev);
	sc = ramdisk_devs[unit];

	/* Sort rogue requests out */
	if (sc == NULL || bp->b_blkno < 0 ||
	    (bp->b_bcount % sc->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* Do not write on "no trespassing" areas... */
	if (bounds_check_with_label(bp, sc->sc_dk.dk_label, 1) <= 0)
		goto bad;

	switch (sc->sc_type) {
#if RAMDISK_SERVER
	case RD_UMEM_SERVER:
		/* Just add this job to the server's queue. */
		bp->b_actf = sc->sc_buflist;
		sc->sc_buflist = bp;
		if (bp->b_actf == NULL) {
			/* server queue was empty. */
			wakeup((caddr_t)sc);
			/* see rd_server_loop() */
		}
		/* no biodone in this case */
		return;
#endif	/* RAMDISK_SERVER */

	case RD_KMEM_FIXED:
	case RD_KMEM_ALLOCATED:
		/* These are in kernel space.  Access directly. */
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		xfer = bp->b_bcount;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = sc->sc_addr + off;
		if (bp->b_flags & B_READ)
			bcopy(addr, bp->b_data, xfer);
		else
			bcopy(bp->b_data, addr, xfer);
		bp->b_resid -= xfer;
		break;

	default:
		bp->b_error = EIO;
bad:
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		break;
	}

	s = splbio();
	biodone(bp);
	splx(s);
}

int
rdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *proc)
{
	struct disklabel *lp;
	struct rd_softc *sc;
	struct rd_conf *urd;
	int unit = DISKUNIT(dev), error;

	sc = ramdisk_devs[unit];

	urd = (struct rd_conf *)data;
	switch (cmd) {
	case DIOCRLDINFO:
		if (sc->sc_type == RD_UNCONFIGURED)
			break;
		lp = malloc(sizeof(*lp), M_TEMP, M_WAITOK);
		rdgetdisklabel(dev, sc, lp, 0);
		bcopy(lp, sc->sc_dk.dk_label, sizeof(*lp));
		free(lp, M_TEMP);
		return (0);

	case DIOCGPDINFO:
		if (sc->sc_type == RD_UNCONFIGURED)
			break;
		rdgetdisklabel(dev, sc, (struct disklabel *)data, 1);
		return (0);

	case DIOCGDINFO:
		if (sc->sc_type == RD_UNCONFIGURED)
			break;
		*(struct disklabel *)data = *(sc->sc_dk.dk_label);
		return (0);

	case DIOCGPART:
		((struct partinfo *)data)->disklab = sc->sc_dk.dk_label;
		((struct partinfo *)data)->part =
		    &sc->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return (0);

	case DIOCWDINFO:
	case DIOCSDINFO:
		if (sc->sc_type == RD_UNCONFIGURED)
			break;
		if ((flag & FWRITE) == 0)
			return (EBADF);

		error = setdisklabel(sc->sc_dk.dk_label,
		    (struct disklabel *)data, sc->sc_dk.dk_openmask);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    rdstrategy, sc->sc_dk.dk_label);
		}
		return (error);

	case DIOCWLABEL:
		if (sc->sc_type == RD_UNCONFIGURED)
			break;
		if ((flag & FWRITE) == 0)
			return (EBADF);
		return (0);

	case RD_GETCONF:
		/* If this is not the control device, punt! */
		if (RD_IS_CTRL(dev) == 0)
			break;
		*urd = sc->sc_rd;
		return (0);

	case RD_SETCONF:
		/* If this is not the control device, punt! */
		if (RD_IS_CTRL(dev) == 0)
			break;
		/* Can only set it once. */
		if (sc->sc_type != RD_UNCONFIGURED)
			break;
		switch (urd->rd_type) {
		case RD_KMEM_ALLOCATED:
			return (rd_ioctl_kalloc(sc, urd, proc));
#if RAMDISK_SERVER
		case RD_UMEM_SERVER:
			return (rd_ioctl_server(sc, urd, proc));
#endif
		default:
			break;
		}
		break;

	default:
		return (ENOTTY);
	}
	return (EINVAL);
}

void
rdgetdisklabel(dev_t dev, struct rd_softc *sc, struct disklabel *lp,
    int spoofonly)
{
	bzero(lp, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = 1;
	lp->d_nsectors = sc->sc_size >> DEV_BSHIFT;
	lp->d_ncylinders = 1;
	lp->d_secpercyl = lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	strncpy(lp->d_typename, "RAM disk", sizeof(lp->d_typename));
	lp->d_type = DTYPE_SCSI;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	DL_SETDSIZE(lp, lp->d_nsectors);
	lp->d_version = 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	readdisklabel(DISKLABELDEV(dev), rdstrategy, lp, spoofonly);
}

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_KMEM_ALLOCATED)
 * Just allocate some kernel memory and return.
 */
int
rd_ioctl_kalloc(struct rd_softc *sc, struct rd_conf *urd, struct proc *proc)
{
	vaddr_t addr;
	vsize_t size;

	/* Sanity check the size. */
	size = urd->rd_size;
	addr = uvm_km_zalloc(kernel_map, size);
	if (!addr)
		return (ENOMEM);

	/* This unit is now configured. */
	sc->sc_addr = (caddr_t)addr; 	/* kernel space */
	sc->sc_size = (size_t)size;
	sc->sc_type = RD_KMEM_ALLOCATED;
	return (0);
}

int
rd_probe(struct device *parent, void *match_, void *aux)
{
	return (0);
}

int
rd_detach(struct device *self, int flags)
{
	return (0);
}

#if RAMDISK_SERVER

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_UMEM_SERVER)
 * Set config, then become the I/O server for this unit.
 */
int
rd_ioctl_server(struct rd_softc *sc, struct rd_conf *urd, struct proc *proc)
{
	vaddr_t end;
	int error;

	/* Sanity check addr, size. */
	end = (vaddr_t) (urd->rd_addr + urd->rd_size);

	if ((end >= VM_MAXUSER_ADDRESS) || (end < ((vaddr_t) urd->rd_addr)) )
		return (EINVAL);

	/* This unit is now configured. */
	sc->sc_addr = urd->rd_addr; 	/* user space */
	sc->sc_size = urd->rd_size;
	sc->sc_type = RD_UMEM_SERVER;

	/* Become the server daemon */
	error = rd_server_loop(sc);

	/* This server is now going away! */
	sc->sc_type = RD_UNCONFIGURED;
	sc->sc_addr = 0;
	sc->sc_size = 0;
	return (error);
}	

int	rd_sleep_pri = PWAIT | PCATCH;

int
rd_server_loop(struct rd_softc *sc)
{
	struct buf *bp;
	caddr_t addr;	/* user space address */
	size_t  off;	/* offset into "device" */
	size_t  xfer;	/* amount to transfer */
	int error, s;

	for (;;) {
		/* Wait for some work to arrive. */
		while (sc->sc_buflist == NULL) {
			error = tsleep((caddr_t)sc, rd_sleep_pri, "rd_idle", 0);
			if (error)
				return (error);
		}

		/* Unlink buf from head of list. */
		bp = sc->sc_buflist;
		sc->sc_buflist = bp->b_actf;
		bp->b_actf = NULL;

		/* Do the transfer to/from user space. */
		error = 0;
		bp->b_resid = bp->b_bcount;
		off = (bp->b_blkno << DEV_BSHIFT);
		if (off >= sc->sc_size) {
			if (bp->b_flags & B_READ)
				goto done;	/* EOF (not an error) */
			error = EIO;
			goto done;
		}
		xfer = bp->b_resid;
		if (xfer > (sc->sc_size - off))
			xfer = (sc->sc_size - off);
		addr = sc->sc_addr + off;
		if (bp->b_flags & B_READ)
			error = copyin(addr, bp->b_data, xfer);
		else
			error = copyout(bp->b_data, addr, xfer);
		if (!error)
			bp->b_resid -= xfer;

	done:
		if (error) {
			bp->b_error = error;
			bp->b_flags |= B_ERROR;
		}
		s = splbio();
		biodone(bp);
		splx(s);
	}
}

#endif	/* RAMDISK_SERVER */
