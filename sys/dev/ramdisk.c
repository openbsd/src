/*	$OpenBSD: ramdisk.c,v 1.9 1999/02/26 01:44:33 art Exp $	*/
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
 * This implements a general-puspose RAM-disk.
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

#include <vm/vm.h>
#include <vm/vm_kern.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#else
/* Don't want all those other VM headers... */
extern vm_offset_t	 kmem_alloc __P((vm_map_t, vm_size_t));
#endif

#include <dev/ramdisk.h>

/*
 * By default, include the user-space functionality.
 * Use:  option RAMDISK_SERVER=0 to turn it off.
 */
#ifndef RAMDISK_SERVER
#define	RAMDISK_SERVER 1
#endif

/*
 * XXX: the "control" unit is (base unit + 16).
 * We should just use the cdev as the "control", but
 * that interferes with the security stuff preventing
 * simulatneous use of raw and block devices.
 *
 * XXX Assumption: 16 RAM-disks are enough!
 */
#define RD_MAX_UNITS	0x10
#define RD_UNIT(unit)    (unit / MAXPARTITIONS)
#define RD_PART(unit)    (unit % MAXPARTITIONS)
#define RD_IS_CTRL(unit) (RD_PART(unit) == RAW_PART)
#define	MAKERDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	RDLABELDEV(dev)	(MAKERDDEV(major(dev), RD_UNIT(dev), RAW_PART))

/* autoconfig stuff... */

struct rd_softc {
	struct device sc_dev;	/* REQUIRED first entry */
	struct disk sc_dkdev;	/* hook for generic disk handling */
	struct rd_conf sc_rd;
	struct buf *sc_buflist;
	int sc_flags;
};
/* shorthand for fields in sc_rd: */
#define sc_addr sc_rd.rd_addr
#define sc_size sc_rd.rd_size
#define sc_type sc_rd.rd_type
/* flags */
#define RD_ISOPEN	0x01
#define RD_SERVED	0x02

void rdattach __P((int));
static void rd_attach __P((struct device *, struct device *, void *));
struct disklabel *rdgetdisklabel __P((dev_t dev, struct rd_softc *sc));

/*
 * Some ports (like i386) use a swapgeneric that wants to
 * snoop around in this rd_cd structure.  It is preserved
 * (for now) to remain compatible with such practice.
 * XXX - that practice is questionable...
 */
struct cfdriver rd_cd = {
	NULL, "rd", DV_DULL, NULL, 0
};

void rdstrategy __P((struct buf *bp));
struct dkdriver rddkdriver = { rdstrategy };

static int   ramdisk_ndevs;
static void *ramdisk_devs[RD_MAX_UNITS];

/*
 * This is called if we are configured as a pseudo-device
 */
void
rdattach(n)
	int n;
{
	struct rd_softc *sc;
	int i;

#ifdef	DIAGNOSTIC
	if (ramdisk_ndevs) {
		printf("ramdisk: multiple attach calls?\n");
		return;
	}
#endif

	/* XXX:  Are we supposed to provide a default? */
	if (n <= 1)
		n = 1;
	if (n > RD_MAX_UNITS)
		n = RD_MAX_UNITS;
	ramdisk_ndevs = n;

	/* XXX: Fake-up rd_cd (see above) */
	rd_cd.cd_ndevs = ramdisk_ndevs;
	rd_cd.cd_devs  = ramdisk_devs;

	/* Attach as if by autoconfig. */
	for (i = 0; i < n; i++) {

		sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK);
		if (!sc) {
			printf("ramdisk: malloc for attach failed!\n");
			return;
		}
		bzero((caddr_t)sc, sizeof(*sc));
		ramdisk_devs[i] = sc;
		sc->sc_dev.dv_unit = i;
		sprintf(sc->sc_dev.dv_xname, "rd%d", i);
		rd_attach(NULL, &sc->sc_dev, NULL);
	}
}

static void
rd_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
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
	sc->sc_dkdev.dk_driver = &rddkdriver;
	sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dkdev);
}

/*
 * operational routines:
 * open, close, read, write, strategy,
 * ioctl, dump, size
 */

#if RAMDISK_SERVER
static int rd_server_loop __P((struct rd_softc *sc));
static int rd_ioctl_server __P((struct rd_softc *sc,
		struct rd_conf *urd, struct proc *proc));
#endif
static int rd_ioctl_kalloc __P((struct rd_softc *sc,
		struct rd_conf *urd, struct proc *proc));

dev_type_open(rdopen);
dev_type_close(rdclose);
dev_type_read(rdread);
dev_type_write(rdwrite);
dev_type_ioctl(rdioctl);
dev_type_size(rdsize);
dev_type_dump(rddump);

int rddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	return ENODEV;
}

int rdsize(dev_t dev)
{
	int unit;
	struct rd_softc *sc;

	/* Disallow control units. */
	unit = minor(dev);
	if (unit >= ramdisk_ndevs)
		return 0;
	sc = ramdisk_devs[unit];
	if (sc == NULL)
		return 0;

	if (sc->sc_type == RD_UNCONFIGURED)
		return 0;

	return (sc->sc_size >> DEV_BSHIFT);
}

int
rdopen(dev, flag, fmt, proc)
	dev_t   dev;
	int     flag, fmt;
	struct proc *proc;
{
	int md, unit;
	struct rd_softc *sc;

	md = minor(dev);
	unit = RD_UNIT(md);
	if (unit >= ramdisk_ndevs)
		return ENXIO;
	sc = ramdisk_devs[unit];
	if (sc == NULL)
		return ENXIO;

	/*
	 * The control device is not exclusive, and can
	 * open uninitialized units (so you can setconf).
	 */
	if (RD_IS_CTRL(md))
		return 0;

#ifdef	RAMDISK_HOOKS
	/* Call the open hook to allow loading the device. */
	rd_open_hook(unit, &sc->sc_rd);
#endif

	/*
	 * This is a normal, "slave" device, so
	 * enforce initialized, exclusive open.
	 */
	if (sc->sc_type == RD_UNCONFIGURED)
		return ENXIO;
	if (sc->sc_flags & RD_ISOPEN)
		return EBUSY;

	return 0;
}

int
rdclose(dev, flag, fmt, proc)
	dev_t   dev;
	int     flag, fmt;
	struct proc *proc;
{
	int md, unit;
	struct rd_softc *sc;

	md = minor(dev);
	unit = RD_UNIT(md);
	sc = ramdisk_devs[unit];

	if (RD_IS_CTRL(md))
		return 0;

	/* Normal device. */
	sc->sc_flags = 0;

	return 0;
}

int
rdread(dev, uio, flags)
	dev_t		dev;
	struct uio	*uio;
	int		flags;
{
	return (physio(rdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
rdwrite(dev, uio, flags)
	dev_t		dev;
	struct uio	*uio;
	int		flags;
{
	return (physio(rdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Handle I/O requests, either directly, or
 * by passing them to the server process.
 */
void
rdstrategy(bp)
	struct buf *bp;
{
	int md, unit;
	struct rd_softc *sc;
	caddr_t addr;
	size_t  off, xfer;

	md = minor(bp->b_dev);
	unit = RD_UNIT(md);
	sc = ramdisk_devs[unit];

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
		if (off >= sc->sc_size) {
			if (bp->b_flags & B_READ)
				break;	/* EOF */
			goto set_eio;
		}
		xfer = bp->b_resid;
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
		bp->b_resid = bp->b_bcount;
	set_eio:
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		break;
	}
	biodone(bp);
}

int
rdioctl(dev, cmd, data, flag, proc)
	dev_t	dev;
	u_long	cmd;
	int		flag;
	caddr_t	data;
	struct proc	*proc;
{
	int md, unit;
	struct rd_softc *sc;
	struct rd_conf *urd;
	struct cpu_disklabel clp;
	struct disklabel lp, *lpp;
	int error;

	md = minor(dev);
	unit = RD_UNIT(md);
	sc = ramdisk_devs[unit];

	urd = (struct rd_conf *)data;
	switch (cmd) {
	case DIOCGDINFO:
		if (sc->sc_type == RD_UNCONFIGURED) {
			break;
		}
		lpp = rdgetdisklabel(dev, sc);
		if (lpp)
			*(struct disklabel *)data = *lpp;
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if (sc->sc_type == RD_UNCONFIGURED) {
			break;
		}
		if ((flag & FWRITE) == 0)
			return EBADF;

		error = setdisklabel(&lp, (struct disklabel *)data,
		    /*sd->sc_dk.dk_openmask : */0, &clp);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(RDLABELDEV(dev),
				    rdstrategy, &lp, &clp);
		}

		return error;

	case DIOCWLABEL:
		if (sc->sc_type == RD_UNCONFIGURED) {
			break;
		}
		if ((flag & FWRITE) == 0)
			return EBADF;
		return 0;

	case RD_GETCONF:
		/* If this is not the control device, punt! */
		if (RD_IS_CTRL(md) == 0) {
			break;
		}
		*urd = sc->sc_rd;
		return 0;

	case RD_SETCONF:
		/* If this is not the control device, punt! */
		if (RD_IS_CTRL(md) == 0) {
			break;
		}
		/* Can only set it once. */
		if (sc->sc_type != RD_UNCONFIGURED) {
			break;
		}
		switch (urd->rd_type) {
		case RD_KMEM_ALLOCATED:
			return rd_ioctl_kalloc(sc, urd, proc);
#if RAMDISK_SERVER
		case RD_UMEM_SERVER:
			return rd_ioctl_server(sc, urd, proc);
#endif
		default:
			break;
		}
		break;
	}
	return EINVAL;
}

struct disklabel *
rdgetdisklabel(dev, sc)
	dev_t dev;
	struct rd_softc *sc;
{
	static struct disklabel lp;
	struct cpu_disklabel clp;
	char *errstring;

	bzero(&lp, sizeof(struct disklabel));
	bzero(&clp, sizeof(struct cpu_disklabel));

	lp.d_secsize = 1 << DEV_BSHIFT;
	lp.d_ntracks = 1;
	lp.d_nsectors = sc->sc_size >> DEV_BSHIFT;
	lp.d_ncylinders = 1;
	lp.d_secpercyl = lp.d_nsectors;
	if (lp.d_secpercyl == 0) {
		lp.d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	strncpy(lp.d_typename, "RAM disk", 16);
	lp.d_type = DTYPE_SCSI;
	strncpy(lp.d_packname, "fictitious", 16);
	lp.d_secperunit = lp.d_nsectors;
	lp.d_rpm = 3600;
	lp.d_interleave = 1;
	lp.d_flags = 0;

	lp.d_partitions[RAW_PART].p_offset = 0;
	lp.d_partitions[RAW_PART].p_size =
	    lp.d_secperunit * (lp.d_secsize / DEV_BSIZE);
	lp.d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp.d_npartitions = RAW_PART + 1;

	lp.d_magic = DISKMAGIC;
	lp.d_magic2 = DISKMAGIC;
	lp.d_checksum = dkcksum(&lp);

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(RDLABELDEV(dev), rdstrategy, &lp, &clp, 0);
	if (errstring) {
		/*printf("%s: %s\n", sc->sc_dev.dv_xname, errstring);*/
		return NULL;
	}
	return &lp;
}

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_KMEM_ALLOCATED)
 * Just allocate some kernel memory and return.
 */
static int
rd_ioctl_kalloc(sc, urd, proc)
	struct rd_softc *sc;
	struct rd_conf *urd;
	struct proc	*proc;
{
	vm_offset_t addr;
	vm_size_t  size;

	/* Sanity check the size. */
	size = urd->rd_size;
#if defined(UVM)
	addr = uvm_km_zalloc(kernel_map, size);
#else
	addr = kmem_alloc(kernel_map, size);
#endif
	if (!addr)
		return ENOMEM;

	/* This unit is now configured. */
	sc->sc_addr = (caddr_t)addr; 	/* kernel space */
	sc->sc_size = (size_t)size;
	sc->sc_type = RD_KMEM_ALLOCATED;
	return 0;
}	

#if RAMDISK_SERVER

/*
 * Handle ioctl RD_SETCONF for (sc_type == RD_UMEM_SERVER)
 * Set config, then become the I/O server for this unit.
 */
static int
rd_ioctl_server(sc, urd, proc)
	struct rd_softc *sc;
	struct rd_conf *urd;
	struct proc	*proc;
{
	vm_offset_t end;
	int error;

	/* Sanity check addr, size. */
	end = (vm_offset_t) (urd->rd_addr + urd->rd_size);

	if ((end >= VM_MAXUSER_ADDRESS) ||
		(end < ((vm_offset_t) urd->rd_addr)) )
		return EINVAL;

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

static int
rd_server_loop(sc)
	struct rd_softc *sc;
{
	struct buf *bp;
	caddr_t addr;	/* user space address */
	size_t  off;	/* offset into "device" */
	size_t  xfer;	/* amount to transfer */
	int error;

	for (;;) {
		/* Wait for some work to arrive. */
		while (sc->sc_buflist == NULL) {
			error = tsleep((caddr_t)sc, rd_sleep_pri, "rd_idle", 0);
			if (error)
				return error;
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
		biodone(bp);
	}
}

#endif	/* RAMDISK_SERVER */
