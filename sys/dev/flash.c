/*	$OpenBSD: flash.c,v 1.8 2007/06/20 18:15:46 deraadt Exp $	*/

/*
 * Copyright (c) 2005 Uwe Stuehler <uwe@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <dev/flashvar.h>

#include <ufs/ffs/fs.h>		/* XXX */

/* Samsung command set */
#define SAMSUNG_CMD_PTRLO	0x00
#define SAMSUNG_CMD_PTRHI	0x01
#define SAMSUNG_CMD_PTROOB	0x50
#define SAMSUNG_CMD_READ	0x30
#define SAMSUNG_CMD_SEQIN	0x80
#define SAMSUNG_CMD_WRITE	0x10
#define SAMSUNG_CMD_ERASE0	0x60
#define SAMSUNG_CMD_ERASE1	0xd0
#define SAMSUNG_CMD_STATUS	0x70
#define  STATUS_FAIL		(1<<0)
#define  STATUS_READY		(1<<6)
#define  STATUS_NWP		(1<<7)
#define SAMSUNG_CMD_READID	0x90
#define SAMSUNG_CMD_RESET	0xff

int	 flash_wait_ready(struct flash_softc *);
int	 flash_wait_complete(struct flash_softc *);

/* XXX: these should go elsewhere */
cdev_decl(flash);
bdev_decl(flash);

#define flashlock(sc) disk_lock(&(sc)->sc_dk)
#define flashunlock(sc) disk_unlock(&(sc)->sc_dk)
#define flashlookup(unit) \
	(struct flash_softc *)device_lookup(&flash_cd, (unit))

void	flashminphys(struct buf *);
void	flashstart(struct flash_softc *);
void	_flashstart(struct flash_softc *, struct buf *);
void	flashdone(void *);

int	flashsafestrategy(struct flash_softc *, struct buf *);
void	flashgetdefaultlabel(dev_t, struct flash_softc *,
    struct disklabel *);
void	flashgetdisklabel(dev_t, struct flash_softc *, struct disklabel *, int);

/*
 * Driver attachment glue
 */

struct flashvendor {
	u_int8_t	 vendor;
	const char	*name;
};

static const struct flashvendor flashvendors[] = {
	{ FLASH_VENDOR_SAMSUNG, "Samsung" }
};
#define	FLASH_NVENDORS (sizeof(flashvendors) / sizeof(flashvendors[0]))

static const struct flashdev flashdevs[] = {
	{ FLASH_DEVICE_SAMSUNG_K9F2808U0C, "K9F2808U0C 16Mx8 3.3V",
	   512, 16, 32, 32768 },
	{ FLASH_DEVICE_SAMSUNG_K9F1G08U0A, "K9F1G08U0A 128Mx8 3.3V",
	  2048, 64, 64, 65536 },
};
#define	FLASH_NDEVS (sizeof(flashdevs) / sizeof(flashdevs[0]))

struct cfdriver flash_cd = {
	NULL, "flash", DV_DISK
};

struct dkdriver flashdkdriver = { flashstrategy };

void
flashattach(struct flash_softc *sc, struct flash_ctl_tag *tag,
    void *cookie)
{
	u_int8_t vendor, device;
	u_int16_t id;
	int i;

	sc->sc_tag = tag;
	sc->sc_cookie = cookie;

	if (sc->sc_maxwaitready <= 0)
		sc->sc_maxwaitready = 1000;      /* 1ms */
	if (sc->sc_maxwaitcomplete <= 0)
		sc->sc_maxwaitcomplete = 200000; /* 200ms */

	flash_chip_enable(sc);

	/* Identify the flash device. */
	if (flash_chip_identify(sc, &vendor, &device) != 0) {
		printf(": identification failed\n");
		flash_chip_disable(sc);
		return;
	}
	id = (vendor << 8) | device;

	/* Look up device characteristics, abort if not recognized. */
	for (i = 0; i < FLASH_NVENDORS; i++) {
		if (flashvendors[i].vendor == vendor) {
			printf(": %s", flashvendors[i].name);
			break;
		}
	}
	if (i == FLASH_NVENDORS)
		printf(": vendor 0x%02x", vendor);
	for (i = 0; i < FLASH_NDEVS; i++) {
		if (flashdevs[i].id == id) {
			printf(" %s\n", flashdevs[i].longname);
			break;
		}
	}
	if (i == FLASH_NDEVS) {
		/* Need to add this device to flashdevs first. */
		printf(" device 0x%02x\n", device);
		flash_chip_disable(sc);
		return;
	}
	sc->sc_flashdev = &flashdevs[i];

	/* Check if the device really works or fail early. */
	if (flash_chip_reset(sc) != 0) {
		printf("%s: reset failed\n", sc->sc_dev.dv_xname);
		flash_chip_disable(sc);
		return;
	}

	flash_chip_disable(sc);

	/*
	 * Initialize and attach the disk structure.
	 */
	sc->sc_dk.dk_driver = &flashdkdriver;
	sc->sc_dk.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_dk);

	/* XXX establish shutdown hook to finish any commands. */
}

int
flashdetach(struct device *self, int flags)
{
	struct flash_softc *sc = (struct flash_softc *)self;

	/* Detach disk. */
	disk_detach(&sc->sc_dk);

	/* XXX more resources need to be freed here. */
	return 0;
}

int
flashactivate(struct device *self, enum devact act)
{
	/* XXX anything to be done here? */
	return 0;
}

/*
 * Flash controller and chip functions
 */

u_int8_t
flash_reg8_read(struct flash_softc *sc, int reg)
{
	return sc->sc_tag->reg8_read(sc->sc_cookie, reg);
}

void
flash_reg8_read_page(struct flash_softc *sc, caddr_t data, caddr_t oob)
{
	int i;

	for (i = 0; i < sc->sc_flashdev->pagesize; i++)
		data[i] = flash_reg8_read(sc, FLASH_REG_DATA);

	if (oob != NULL)
		for (i = 0; i < sc->sc_flashdev->oobsize; i++)
			oob[i] = flash_reg8_read(sc, FLASH_REG_DATA);
}

void
flash_reg8_write(struct flash_softc *sc, int reg, u_int8_t value)
{
	sc->sc_tag->reg8_write(sc->sc_cookie, reg, value);
}

void
flash_reg8_write_page(struct flash_softc *sc, caddr_t data, caddr_t oob)
{
	int i;

	for (i = 0; i < sc->sc_flashdev->pagesize; i++)
		flash_reg8_write(sc, FLASH_REG_DATA, data[i]);

	if (oob != NULL)
		for (i = 0; i < sc->sc_flashdev->oobsize; i++)
			flash_reg8_write(sc, FLASH_REG_DATA, oob[i]);
}

/*
 * Wait for the "Ready/Busy" signal to go high, indicating that the
 * device is ready to accept another command.
 */
int
flash_wait_ready(struct flash_softc *sc)
{
	int timo = sc->sc_maxwaitready;
	u_int8_t ready;

	ready = flash_reg8_read(sc, FLASH_REG_READY);
	while (ready == 0 && timo-- > 0) {
		delay(1);
		ready = flash_reg8_read(sc, FLASH_REG_READY);
	}
	return (ready == 0 ? EIO : 0);
}

/*
 * Similar to flash_wait_ready() but looks at IO 6 and IO 0 signals
 * besides R/B to decide whether the last operation was successful.
 */
int
flash_wait_complete(struct flash_softc *sc)
{
	int timo = sc->sc_maxwaitcomplete;
	u_int8_t status;

	(void)flash_wait_ready(sc);

	flash_reg8_write(sc, FLASH_REG_CLE, 1);
	flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_STATUS);
	flash_reg8_write(sc, FLASH_REG_CLE, 0);

	status = flash_reg8_read(sc, FLASH_REG_DATA);
	while ((status & STATUS_READY) == 0 && timo-- > 0) {
		if (flash_reg8_read(sc, FLASH_REG_READY))
			break;
		delay(1);
		status = flash_reg8_read(sc, FLASH_REG_DATA);
	}

	status = flash_reg8_read(sc, FLASH_REG_DATA);
	return ((status & STATUS_FAIL) != 0 ? EIO : 0);
}

void
flash_chip_enable(struct flash_softc *sc)
{
	/* XXX aquire the lock. */
	flash_reg8_write(sc, FLASH_REG_CE, 1);
}

void
flash_chip_disable(struct flash_softc *sc)
{
	flash_reg8_write(sc, FLASH_REG_CE, 0);
	/* XXX release the lock. */
}

int
flash_chip_reset(struct flash_softc *sc)
{
	flash_reg8_write(sc, FLASH_REG_CLE, 1);
	flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_RESET);
	flash_reg8_write(sc, FLASH_REG_CLE, 0);

	return flash_wait_ready(sc);
}

int
flash_chip_identify(struct flash_softc *sc, u_int8_t *vendor,
    u_int8_t *device)
{
	int error;

	(void)flash_wait_ready(sc);

	flash_reg8_write(sc, FLASH_REG_CLE, 1);
	flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_READID);
	flash_reg8_write(sc, FLASH_REG_CLE, 0);

	error = flash_wait_ready(sc);
	if (error == 0) {
		*vendor = flash_reg8_read(sc, FLASH_REG_DATA);
		*device = flash_reg8_read(sc, FLASH_REG_DATA);
	}
	return error;
}

int
flash_chip_erase_block(struct flash_softc *sc, long blkno)
{
	long pageno = blkno * sc->sc_flashdev->blkpages;
	int error;

	(void)flash_wait_ready(sc);

	/* Disable write-protection. */
	flash_reg8_write(sc, FLASH_REG_WP, 0);

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_ERASE0);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno >> 8);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_ERASE1);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	error = flash_wait_complete(sc);

	/* Re-enable write-protection. */
	flash_reg8_write(sc, FLASH_REG_WP, 1);

	return error;
}

int
flash_chip_read_block(struct flash_softc *sc, long blkno, caddr_t data)
{
	long pageno;
	long blkend;
	int error;

	pageno = blkno * sc->sc_flashdev->blkpages;
	blkend = pageno + sc->sc_flashdev->blkpages;

	while (pageno < blkend) {
		error = flash_chip_read_page(sc, pageno, data, NULL);
		if (error != 0)
			return error;
		data += sc->sc_flashdev->pagesize;
		pageno++;
	}
	return 0;
}

int
flash_chip_read_page(struct flash_softc *sc, long pageno, caddr_t data,
    caddr_t oob)
{
	int error;

	(void)flash_wait_ready(sc);

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_PTRLO);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno >> 8);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_READ);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	if ((error = flash_wait_ready(sc)) != 0)
		return error;

	/* Support hardware ECC calculation. */
	if (sc->sc_tag->regx_read_page) {
		error = sc->sc_tag->regx_read_page(sc->sc_cookie, data,
		    oob);
		if (error != 0)
			return error;
	} else
		flash_reg8_read_page(sc, data, oob);

	return 0;
}

int
flash_chip_read_oob(struct flash_softc *sc, long pageno, caddr_t oob)
{
	u_int8_t *p = (u_int8_t *)oob;
	int error;
	int i;

	(void)flash_wait_ready(sc);

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_PTROOB);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_PTRLO);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_COL, 0x08);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno >> 8);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_READ);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	if ((error = flash_wait_ready(sc)) != 0)
		return error;

	for (i = 0; i < sc->sc_flashdev->oobsize; i++)
		p[i] = flash_reg8_read(sc, FLASH_REG_DATA);

	return 0;
}

int
flash_chip_write_block(struct flash_softc *sc, long blkno, caddr_t data,
    caddr_t oob)
{
	long pageno;
	long blkend;
	caddr_t p;
	int error;

	pageno = blkno * sc->sc_flashdev->blkpages;
	blkend = pageno + sc->sc_flashdev->blkpages;

	p = data;
	while (pageno < blkend) {
		error = flash_chip_write_page(sc, pageno, p, oob);
		if (error != 0)
			return error;
		p += sc->sc_flashdev->pagesize;
		pageno++;
	}

	/* Verify the newly written block. */
	return flash_chip_verify_block(sc, blkno, data, oob);
}

int
flash_chip_write_page(struct flash_softc *sc, long pageno, caddr_t data,
    caddr_t oob)
{
	int error;

	(void)flash_wait_ready(sc);

	/* Disable write-protection. */
	flash_reg8_write(sc, FLASH_REG_WP, 0);

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_PTRLO);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_SEQIN);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_COL, 0x00);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_ALE, 1);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno);
		flash_reg8_write(sc, FLASH_REG_ROW, pageno >> 8);
		flash_reg8_write(sc, FLASH_REG_ALE, 0);
		break;
	}

	/* Support hardware ECC calculation. */
	if (sc->sc_tag->regx_write_page) {
		error = sc->sc_tag->regx_write_page(sc->sc_cookie, data,
		    oob);
		if (error != 0)
			return error;
	} else
		flash_reg8_write_page(sc, data, oob);

	switch (sc->sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		flash_reg8_write(sc, FLASH_REG_CLE, 1);
		flash_reg8_write(sc, FLASH_REG_CMD, SAMSUNG_CMD_WRITE);
		flash_reg8_write(sc, FLASH_REG_CLE, 0);
		break;
	}

	/*
	 * Wait for the write operation to complete although this can
	 * take up to 700 us for the K9F1G08U0A flash type.
	 */
	error = flash_wait_complete(sc);

	/* Re-enable write-protection. */
	flash_reg8_write(sc, FLASH_REG_WP, 1);

	return error;
}

int
flash_chip_verify_block(struct flash_softc *sc, long blkno, caddr_t data,
    caddr_t oob)
{
	long pageno;
	long blkend;
	int error;

	pageno = blkno * sc->sc_flashdev->blkpages;
	blkend = pageno + sc->sc_flashdev->blkpages;

	while (pageno < blkend) {
		error = flash_chip_verify_page(sc, pageno, data, oob);
		if (error != 0) {
			printf("block %d page %d verify failed\n",
			    blkno, pageno);
			return error;
		}
		data += sc->sc_flashdev->pagesize;
		pageno++;
	}
	return 0;
}

int
flash_chip_verify_page(struct flash_softc *sc, long pageno, caddr_t data,
    caddr_t oob)
{
	static u_char rbuf[FLASH_MAXPAGESIZE];
	static u_char roob[FLASH_MAXOOBSIZE];
	int error;

	error = flash_chip_read_page(sc, pageno, rbuf,
	    oob == NULL ? NULL : roob);
	if (error != 0)
		return error;

	if (memcmp((const void *)&rbuf[0], (const void *)data,
	    sc->sc_flashdev->pagesize) != 0)
		return EIO;

	if (oob != NULL && memcmp((const void *)&roob[0],
	    (const void *)oob, sc->sc_flashdev->oobsize) != 0)
		return EIO;

	return 0;
}

/*
 * Block device functions
 */

int
flashopen(dev_t dev, int oflags, int devtype, struct proc *p)
{
	struct flash_softc *sc;
	int error;
	int part;

	sc = flashlookup(flashunit(dev));
	if (sc == NULL)
		return ENXIO;

	if ((error = flashlock(sc)) != 0) {
		device_unref(&sc->sc_dev);
		return error;
	}

	/*
	 * If no partition is open load the partition info if it is
	 * not already valid.  If partitions are already open, allow
	 * opens only for the same kind of device.
	 */
	if (sc->sc_dk.dk_openmask == 0) {
		if ((sc->sc_flags & FDK_LOADED) == 0 ||
		    ((sc->sc_flags & FDK_SAFE) == 0) !=
		    (flashsafe(dev) == 0)) {
			sc->sc_flags &= ~FDK_SAFE;
			sc->sc_flags |= FDK_LOADED;
			if (flashsafe(dev))
				sc->sc_flags |= FDK_SAFE;
			flashgetdisklabel(dev, sc, sc->sc_dk.dk_label, 0);
		}
	} else if (((sc->sc_flags & FDK_SAFE) == 0) !=
	    (flashsafe(dev) == 0)) {
		flashunlock(sc);
		device_unref(&sc->sc_dev);
		return EBUSY;
	}

	/* Check that the partition exists. */
	part = flashpart(dev);
	if (part != RAW_PART &&
	    (part >= sc->sc_dk.dk_label->d_npartitions ||
	    sc->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		flashunlock(sc);
		device_unref(&sc->sc_dev);
		return ENXIO;
	}

	/* Prevent our unit from being deconfigured while open. */
	switch (devtype) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	flashunlock(sc);
	device_unref(&sc->sc_dev);
	return 0;
}

int
flashclose(dev_t dev, int fflag, int devtype, struct proc *p)
{
	struct flash_softc *sc;
	int error;
	int part;

	sc = flashlookup(flashunit(dev));
	if (sc == NULL)
		return ENXIO;

	if ((error = flashlock(sc)) != 0) {
		device_unref(&sc->sc_dev);
		return error;
	}

	/* Close one open partition. */
	part = flashpart(dev);
	switch (devtype) {
	case S_IFCHR:
		sc->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sc->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_dk.dk_openmask =
	    sc->sc_dk.dk_copenmask | sc->sc_dk.dk_bopenmask;

	if (sc->sc_dk.dk_openmask == 0) {
		/* XXX wait for I/O to complete? */
	}

	flashunlock(sc);
	device_unref(&sc->sc_dev);
	return 0;
}

/*
 * Queue the transfer of one or more flash pages.
 */
void
flashstrategy(struct buf *bp)
{
	struct flash_softc *sc;
	int s;

	sc = flashlookup(flashunit(bp->b_dev));
	if (sc == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

	/* Transfer only a multiple of the flash page size. */
	if ((bp->b_bcount % sc->sc_flashdev->pagesize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* If the device has been invalidated, error out. */
	if ((sc->sc_flags & FDK_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}

	/* Translate logical block numbers to physical. */
	if (flashsafe(bp->b_dev) && flashsafestrategy(sc, bp) <= 0)
		goto done;

	/* Return immediately if it is a null transfer. */
	if (bp->b_bcount == 0)
		goto done;

	/* Do bounds checking on partitions. */
	if (flashpart(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, sc->sc_dk.dk_label, 0) <= 0)
		goto done;

	/* Queue the transfer. */
	s = splbio();
	disksort(&sc->sc_q, bp);
	flashstart(sc);
	splx(s);
	device_unref(&sc->sc_dev);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	if ((bp->b_flags & B_ERROR) != 0)
		bp->b_resid = bp->b_bcount;
	s = splbio();
	biodone(bp);
	splx(s);
	if (sc != NULL)
		device_unref(&sc->sc_dev);
}

int
flashioctl(dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p)
{
	struct flash_softc *sc;
	int error = 0;

	sc = flashlookup(flashunit(dev));
	if (sc == NULL)
		return ENXIO;

	if ((sc->sc_flags & FDK_LOADED) == 0) {
		device_unref(&sc->sc_dev);
		return EIO;
	}

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)data = *sc->sc_dk.dk_label;
		break;
	default:
		error = ENOTTY;
		break;
	}

	device_unref(&sc->sc_dev);
	return error;
}

int
flashdump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{
	printf("flashdump\n");
	return ENODEV;
}

daddr64_t
flashsize(dev_t dev)
{
	printf("flashsize\n");
	return ENODEV;
}

void
flashstart(struct flash_softc *sc)
{
	struct buf *dp, *bp;

	while (1) {
		/* Remove the next buffer from the queue or stop. */
		dp = &sc->sc_q;
		bp = dp->b_actf;
		if (bp == NULL)
			return;
		dp->b_actf = bp->b_actf;

		/* Transfer this buffer now. */
		_flashstart(sc, bp);
	}
}

void
_flashstart(struct flash_softc *sc, struct buf *bp)
{
	int part;
	daddr64_t offset;
	long pgno;

	part = flashpart(bp->b_dev);
	offset = DL_GETPOFFSET(&sc->sc_dk.dk_label->d_partitions[part]) +
	    bp->b_blkno;
	pgno = offset / (sc->sc_flashdev->pagesize / DEV_BSIZE);

	/*
	 * If the requested page is exactly at the end of flash and it
	 * is an "unsafe" device, return EOF, else error out.
	 */
	if (!flashsafe(bp->b_dev) && pgno == sc->sc_flashdev->capacity) {
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		return;
	} else if (pgno >= sc->sc_flashdev->capacity) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		biodone(bp);
		return;
	}

	sc->sc_bp = bp;

	/* Instrumentation. */
	disk_busy(&sc->sc_dk);

	/* XXX this should be done asynchronously. */
	flash_chip_enable(sc);
	if ((bp->b_flags & B_READ) != 0)
		bp->b_error = flash_chip_read_page(sc, pgno, bp->b_data,
		    NULL);
	else
		bp->b_error = flash_chip_write_page(sc, pgno, bp->b_data,
		    NULL);
	if (bp->b_error == 0)
		bp->b_resid = bp->b_bcount - sc->sc_flashdev->pagesize;
	flash_chip_disable(sc);
	flashdone(sc);
}

void
flashdone(void *v)
{
	struct flash_softc *sc = v;
	struct buf *bp = sc->sc_bp;

	/* Instrumentation. */
	disk_unbusy(&sc->sc_dk, bp->b_bcount - bp->b_resid,
	    (bp->b_flags & B_READ) != 0);

	if (bp->b_error != 0)
		bp->b_flags |= B_ERROR;

	biodone(bp);
	flashstart(sc);
}

void
flashgetdefaultlabel(dev_t dev, struct flash_softc *sc,
    struct disklabel *lp)
{
	size_t len;

	bzero(lp, sizeof(struct disklabel));

	lp->d_type = 0;
	lp->d_subtype = 0;
	strncpy(lp->d_typename, "NAND flash", sizeof(lp->d_typename));

	/* Use the product name up to the first space. */
	strncpy(lp->d_packname, sc->sc_flashdev->longname,
	    sizeof(lp->d_packname));
	for (len = 0; len < sizeof(lp->d_packname); len++)
		if (lp->d_packname[len] == ' ') {
			lp->d_packname[len] = '\0';
			break;
		}

	/* Fake the disk geometry. */
	lp->d_ncylinders = 1;
	lp->d_ntracks = 16;
	lp->d_secsize = sc->sc_flashdev->pagesize;
	lp->d_nsectors = sc->sc_flashdev->capacity / lp->d_ntracks
	    / lp->d_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	DL_SETDSIZE(lp, (daddr64_t)lp->d_ncylinders * lp->d_secpercyl);

	/* Fake hardware characteristics. */
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_version = 1;

	/* XXX these values assume ffs. */
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;

	/* Wrap it up. */
	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

void
flashgetdisklabel(dev_t dev, struct flash_softc *sc,
    struct disklabel *lp, int spoofonly)
{
	char *errstring;
	dev_t labeldev;

	flashgetdefaultlabel(dev, sc, lp);

	if (sc->sc_tag->default_disklabel != NULL)
		sc->sc_tag->default_disklabel(sc->sc_cookie, dev, lp);

	/* Call the generic disklabel extraction routine. */
	labeldev = flashlabeldev(dev);
	errstring = readdisklabel(labeldev, flashstrategy, lp, spoofonly);
	if (errstring != NULL) {
		/*printf("%s: %s\n", sc->sc_dev.dv_xname, errstring);*/
	}
}

/*
 * Character device functions
 */

void
flashminphys(struct buf *bp)
{
	struct flash_softc *sc;

	sc = flashlookup(flashunit(bp->b_dev));

	if (bp->b_bcount > sc->sc_flashdev->pagesize)
		bp->b_bcount = sc->sc_flashdev->pagesize;
}

int
flashread(dev_t dev, struct uio *uio, int ioflag)
{
	return physio(flashstrategy, NULL, dev, B_READ, flashminphys, uio);
}

int
flashwrite(dev_t dev, struct uio *uio, int ioflag)
{
	return physio(flashstrategy, NULL, dev, B_WRITE, flashminphys, uio);
}

/*
 * Physical access strategy "fixup" routines for transparent bad
 * blocks management, wear-leveling, etc.
 */

/*
 * Call the machine-specific routine if there is any or use just a
 * default strategy for bad blocks management.
 */
int
flashsafestrategy(struct flash_softc *sc, struct buf *bp)
{
	if (sc->sc_tag->safe_strategy) {
		return sc->sc_tag->safe_strategy(sc->sc_cookie, bp);
	}

	/* XXX no default bad blocks management strategy yet */
	return 1;
}

void dumppage(u_char *);
void dumppage(u_char *buf)
{
	int i;
	for (i = 0; i < 512; i++) {
		if ((i % 16) == 0)
			printf("%04x: ", i);
		if ((i % 16) == 8)
			printf(" ");
		printf(" %02x", buf[i]);
		if ((i % 16) == 15)
			printf("\n");
	}
	if ((i % 16) != 0)
		printf("\n");
}
