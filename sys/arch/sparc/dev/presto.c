/*	$OpenBSD: presto.c,v 1.26 2015/03/28 19:07:07 miod Exp $	*/
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

struct presto_softc {
	struct	device	sc_dev;
	struct	disk	sc_dk;

	vsize_t		sc_offset;	/* reserved memory offset */
	vsize_t		sc_memsize;	/* total NVRAM size */
	caddr_t		sc_mem;		/* NVRAM kva */
	caddr_t		sc_status;	/* status register kva (MBus) */
	char		sc_model[16];	/* Prestoserve model */
};

/*
 * The beginning of the NVRAM contains a few control and status values.
 * On SBus boards, there are two distinct four-bit status values;
 * On MBus boards, these are provided in the second register mapping.
 */

#define	PSERVE_BATTERYSTATUS	0x07
#define	PSBAT_CHARGING			0x10
#define	PSBAT_CONNECTED			0x20
#define	PSBAT_FAULT			0x40

#define	PSERVE_DATASTATUS	0x0b
#define	PSDATA_EMPTY			0x00
#define	PSDATA_SAVED			0x01

/*
 * Reserved area size on SBus flavours, needs to be rounded to a sector
 * size for i/o.
 */
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

#define prestolookup(unit) (struct presto_softc *)device_lookup(&presto_cd, (unit))

int
presto_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, "MMI,prestoserve") != 0 &&
	    strcmp(ra->ra_name, "SUNW,nvone") != 0)
		return 0;

	if (ra->ra_nreg < 1)
		return 0;

	/* no usable memory ? */
	if (ra->ra_nreg == 1 && ra->ra_len < PSERVE_OFFSET)
		return 0;

	return 1;
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
	if (ca->ca_ra.ra_nreg == 1) {
		sc->sc_status = NULL;
	} else {
		sc->sc_status = (void *)mapiodev(&ca->ca_ra.ra_reg[1], 0,
		    ca->ca_ra.ra_reg[1].rr_len);
	}

	/*
	 * Clear the ``disconnect battery'' bit.
	 */
	if (sc->sc_status == NULL)
		*(u_int8_t *)(sc->sc_mem + PSERVE_BATTERYSTATUS) = 0x00;
	else
		*(u_int8_t *)sc->sc_status &= 0x0f;

	/*
	 * Clear the ``unflushed data'' status. This way, if the card is
	 * reused under SunOS, the system will not try to flush whatever
	 * data the user put in the nvram...
	 */
	if (sc->sc_status == NULL)
		*(u_int8_t *)(sc->sc_mem + PSERVE_DATASTATUS) = 0x00;
	else
		*(u_int8_t *)sc->sc_status &= 0xf0;

	/*
	 * Decode battery status
	 */
	if (sc->sc_status == NULL)
		status = *(u_int8_t *)(sc->sc_mem + PSERVE_BATTERYSTATUS);
	else
		status = *(u_int8_t *)sc->sc_status;
	printf("battery status %02x ", status);
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
	if (sc->sc_status == NULL) {
		printf("%s: status codes %02.2x, %02.2x, %02.2x, %02.2x\n",
		    sc->sc_dev.dv_xname,
		    *(u_int8_t *)(sc->sc_mem + 0x03),
		    *(u_int8_t *)(sc->sc_mem + 0x07),
		    *(u_int8_t *)(sc->sc_mem + 0x0b),
		    *(u_int8_t *)(sc->sc_mem + 0x0f));
	}
#endif

	sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
	if (sc->sc_status == NULL)
		sc->sc_offset = PSERVE_OFFSET;
	else
		sc->sc_offset = 0;
	disk_attach(&sc->sc_dev, &sc->sc_dk);
}

/*
 * Block device i/o operations
 */

int
prestodump(dev_t dev, daddr_t blkno, caddr_t va, size_t size)
{
	/*
	 * A dump to nvram is theoretically possible, but its size is
	 * very likely to be WAY too small.
	 */
	return (ENXIO);
}

daddr_t
prestosize(dev_t dev)
{
	struct presto_softc *sc;
	daddr_t size;
	int part;

	sc = prestolookup(DISKUNIT(dev));
	if (sc == NULL)
		return (-1);

	part = DISKPART(dev);
	if (part >= sc->sc_dk.dk_label->d_npartitions)
		size = -1;
	else
		size = DL_SECTOBLK(sc->sc_dk.dk_label,
		    DL_GETPSIZE(&sc->sc_dk.dk_label->d_partitions[part]));

	device_unref(&sc->sc_dev);
	return (size);
}

int
prestoopen(dev_t dev, int flag, int fmt, struct proc *proc)
{
	struct presto_softc *sc;
	int error;

	sc = prestolookup(DISKUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	/* read the disk label */
	presto_getdisklabel(dev, sc, sc->sc_dk.dk_label, 0);

	/* only allow valid partitions */
	error = disk_openpart(&sc->sc_dk, DISKPART(dev), fmt, 1);

	device_unref(&sc->sc_dev);
	return (error);
}

int
prestoclose(dev_t dev, int flag, int fmt, struct proc *proc)
{
	struct presto_softc *sc;

	sc = prestolookup(DISKUNIT(dev));
	if (sc == NULL)
		return (ENXIO);

	disk_closepart(&sc->sc_dk, DISKPART(dev), fmt);

	device_unref(&sc->sc_dev);
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
	struct presto_softc *sc;
	size_t offset, count;
	int s;

	sc = prestolookup(DISKUNIT(bp->b_dev));

	/* Sort rogue requests out */
	if (sc == NULL) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* Validate the request. */
	if (bounds_check_with_label(bp, sc->sc_dk.dk_label) == -1)
		goto done;

	/* Bound the request size, then move data between buf and nvram */
	offset = (bp->b_blkno << DEV_BSHIFT) + sc->sc_offset;
	count = bp->b_bcount;
	if (count > (sc->sc_memsize - offset))
		count = (sc->sc_memsize - offset);
	if (ISSET(bp->b_flags, B_READ))
		bcopy(sc->sc_mem + offset, bp->b_data, count);
	else
		bcopy(bp->b_data, sc->sc_mem + offset, count);
	bp->b_resid = bp->b_bcount - count;
	goto done;

 bad:
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
 done:
	s = splbio();
	biodone(bp);
	splx(s);
	if (sc != NULL)
		device_unref(&sc->sc_dev);
}

int
prestoioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *proc)
{
	struct presto_softc *sc;
	int error = 0;

	sc = prestolookup(DISKUNIT(dev));

	switch (cmd) {
	case DIOCGPDINFO:
		presto_getdisklabel(dev, sc, (struct disklabel *)data, 1);
		break;

	case DIOCGDINFO:
		*(struct disklabel *)data = *sc->sc_dk.dk_label;
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}

		error = setdisklabel(sc->sc_dk.dk_label,
		    (struct disklabel *)data, sc->sc_dk.dk_openmask);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    prestostrategy, sc->sc_dk.dk_label);
		}
		break;

	default:
		error = EINVAL;
		break;
	}

	device_unref(&sc->sc_dev);
	return (error);
}

/*
 * Read the disklabel. If none is present, use a fictitious one instead.
 */
void
presto_getdisklabel(dev_t dev, struct presto_softc *sc, struct disklabel *lp,
    int spoofonly)
{
	bzero(sc->sc_dk.dk_label, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = 1;
	lp->d_nsectors = 32;
	DL_SETDSIZE(lp, (sc->sc_memsize - sc->sc_offset) >> DEV_BSHIFT);
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
