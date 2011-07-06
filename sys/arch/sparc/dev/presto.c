/*	$OpenBSD: presto.c,v 1.22 2011/07/06 04:49:35 matthew Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include <machine/autoconf.h>
#include <machine/conf.h>

#include <sparc/dev/sbusvar.h>

struct presto_softc {
	struct	device	sc_dev;
	struct	disk	sc_dk;

	vsize_t		sc_memsize;	/* total NVRAM size */
	caddr_t		sc_mem;		/* NVRAM kva */
	char		sc_model[16];	/* Prestoserve model */
};

/*
 * The beginning of the NVRAM contains a few control and status values
 */

#define	PSERVE_BATTERYSTATUS	0x07
#define	PSBAT_CHARGING			0x10
#define	PSBAT_CONNECTED			0x20
#define	PSBAT_FAULT			0x40

#define	PSERVE_DATASTATUS	0x0b
#define	PSDATA_EMPTY			0x00
#define	PSDATA_SAVED			0x01

/* reserved area size - needs to be rounded to a sector size for i/o */
#define	PSERVE_RESERVED		0x0010
#define	PSERVE_OFFSET		roundup(PSERVE_RESERVED, DEV_BSIZE)

void	prestostrategy(struct buf *);
void	presto_attach(struct device *, struct device *, void *);
void	presto_getdisklabel(dev_t, struct presto_softc *, struct disklabel *, int);
int	presto_match(struct device *, void *, void *);

struct cfattach presto_ca = {
	sizeof(struct presto_softc), presto_match, presto_attach
};

struct cfdriver presto_cd = {
	NULL, "presto", DV_DULL
};

int
presto_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "MMI,prestoserve") != 0)
		return (0);

	if (ra->ra_len < PSERVE_OFFSET)	/* no usable memory ? */
		return (0);

	return (1);
}

void
presto_attach(struct device *parent, struct device *self, void *args)
{
	struct presto_softc *sc = (struct presto_softc *)self;
	struct confargs *ca = args;
	char *model, *submodel;
	u_int8_t status;

	/* Get card parameters */
	model = getpropstring(ca->ca_ra.ra_node, "model");
	if (*model == '\0')
		submodel = "fictitious";
	else {
		submodel = memchr(model, ',', strlen(model));
		if (submodel != NULL)
			submodel++;
		else
			submodel = model;
	}
	strncpy(sc->sc_model, submodel, 16);
	sc->sc_memsize = ca->ca_ra.ra_len;

	printf(": %s\n%s: %d MB NVRAM, ", model,
	    sc->sc_dev.dv_xname, sc->sc_memsize >> 20);

	/* Map memory */
	sc->sc_mem = (void *)mapiodev(ca->ca_ra.ra_reg, 0, sc->sc_memsize);

	/*
	 * Clear the ``disconnect battery'' bit.
	 */
	*(u_int8_t *)(sc->sc_mem + PSERVE_BATTERYSTATUS) = 0x00;

	/*
	 * Clear the ``unflushed data'' status. This way, if the card is
	 * reused under SunOS, the system will not try to flush whatever
	 * data the user put in the nvram...
	 */
	*(u_int8_t *)(sc->sc_mem + PSERVE_DATASTATUS) = 0x00;

	/*
	 * Decode battery status
	 */
	status = *(u_int8_t *)(sc->sc_mem + PSERVE_BATTERYSTATUS);
	printf("battery status %x ", status);
	if (ISSET(status, PSBAT_FAULT)) {
		printf("(non-working)");
	} else if (ISSET(status, PSBAT_CONNECTED)) {
		if (ISSET(status, PSBAT_CHARGING))
			printf("(charging)");
		else
			printf("(ok)");
	} else
		printf("(disabled)");
	printf("\n");

#ifdef DEBUG
	printf("%s: status codes %02.2x, %02.2x, %02.2x, %02.2x\n",
	    sc->sc_dev.dv_xname,
	    *(u_int8_t *)(sc->sc_mem + 0x03), *(u_int8_t *)(sc->sc_mem + 0x07),
	    *(u_int8_t *)(sc->sc_mem + 0x0b), *(u_int8_t *)(sc->sc_mem + 0x0f));
#endif

	sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dev, &sc->sc_dk);
}

/*
 * Block device i/o operations
 */

int
prestodump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{
	/*
	 * A dump to nvram is theoretically possible, but its size is
	 * very likely to be WAY too small.
	 */
	return (ENXIO);
}

daddr64_t
prestosize(dev_t dev)
{
	struct presto_softc *sc;
	int unit, part;

	unit = DISKUNIT(dev);
	sc = (struct presto_softc *)device_lookup(&presto_cd, unit);
	if (sc == NULL)
		return (0);

	part = DISKPART(dev);
	if (part >= sc->sc_dk.dk_label->d_npartitions)
		return (0);
	else
		return (DL_GETPSIZE(&sc->sc_dk.dk_label->d_partitions[part]) *
		    (sc->sc_dk.dk_label->d_secsize / DEV_BSIZE));
}

int
prestoopen(dev_t dev, int flag, int fmt, struct proc *proc)
{
	int unit, part;
	struct presto_softc *sc;

	unit = DISKUNIT(dev);
	sc = (struct presto_softc *)device_lookup(&presto_cd, unit);
	if (sc == NULL)
		return (ENXIO);

	/* read the disk label */
	presto_getdisklabel(dev, sc, sc->sc_dk.dk_label, 0);

	/* only allow valid partitions */
	part = DISKPART(dev);
	if (part != RAW_PART &&
	    (part >= sc->sc_dk.dk_label->d_npartitions ||
	    sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED))
		return (ENXIO);

	/* update open masks */
	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	return (0);
}

int
prestoclose(dev_t dev, int flag, int fmt, struct proc *proc)
{
	int unit, part;
	struct presto_softc *sc;

	unit = DISKUNIT(dev);
	sc = (struct presto_softc *)device_lookup(&presto_cd, unit);

	/* update open masks */
	part = DISKPART(dev);
	switch (fmt) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dk.dk_openmask = sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	return (0);
}

int
prestoread(dev_t dev, struct uio *uio, int flags)
{
	return (physio(prestostrategy, dev, B_READ, minphys, uio));
}

int
prestowrite(dev_t dev, struct uio *uio, int flags)
{
	return (physio(prestostrategy, dev, B_WRITE, minphys, uio));
}

void
prestostrategy(struct buf *bp)
{
	int unit;
	struct presto_softc *sc;
	size_t offset, count;
	int s;

	unit = DISKUNIT(bp->b_dev);
	sc = (struct presto_softc *)device_lookup(&presto_cd, unit);

	/* Sort rogue requests out */
	if (sc == NULL) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* Validate the request. */
	if (bounds_check_with_label(bp, sc->sc_dk.dk_label) == -1)
		goto done;

	/* Bound the request size, then move data between buf and nvram */
	bp->b_resid = bp->b_bcount;
	offset = (bp->b_blkno << DEV_BSHIFT) + PSERVE_OFFSET;
	count = bp->b_bcount;
	if (count > (sc->sc_memsize - offset))
		count = (sc->sc_memsize - offset);
	if (ISSET(bp->b_flags, B_READ))
		bcopy(sc->sc_mem + offset, bp->b_data, count);
	else
		bcopy(bp->b_data, sc->sc_mem + offset, count);
	bp->b_resid -= count;
	goto done;

 bad:
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
 done:
	s = splbio();
	biodone(bp);
	splx(s);
}

int
prestoioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *proc)
{
	struct presto_softc *sc;
	int unit;
	int error;

	unit = DISKUNIT(dev);
	sc = (struct presto_softc *)device_lookup(&presto_cd, unit);

	switch (cmd) {
	case DIOCGPDINFO:
		presto_getdisklabel(dev, sc, (struct disklabel *)data, 1);
		break;

	case DIOCGDINFO:
		bcopy(sc->sc_dk.dk_label, data, sizeof(struct disklabel));
		return (0);

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);

		error = setdisklabel(sc->sc_dk.dk_label,
		    (struct disklabel *)data, /*sd->sc_dk.dk_openmask : */0);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    prestostrategy, sc->sc_dk.dk_label);
		}
		return (error);

	default:
		return (EINVAL);
	}
}

/*
 * Read the disklabel. If none is present, use a fictitious one instead.
 */
void
presto_getdisklabel(dev_t dev, struct presto_softc *sc, struct disklabel *lp,
    int spoofonly);
{
	bzero(sc->sc_dk.dk_label, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = 1;
	lp->d_nsectors = 32;
	DL_SETDSIZE(lp, (sc->sc_memsize - PSERVE_OFFSET) >> DEV_BSHIFT);
	lp->d_ncylinders = DL_GETDSIZE(lp) / lp->d_nsectors;
	lp->d_secpercyl = lp->d_nsectors;

	strncpy(lp->d_typename, "Prestoserve", 16);
	lp->d_type = DTYPE_SCSI;	/* what better to put here? */
	strncpy(lp->d_packname, sc->sc_model, 16);
	lp->d_version = 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	readdisklabel(DISKLABELDEV(dev), prestostrategy, lp, spoofonly);
}
