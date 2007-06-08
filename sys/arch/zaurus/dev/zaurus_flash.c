/*	$OpenBSD: zaurus_flash.c,v 1.3 2007/06/08 05:27:58 deraadt Exp $	*/

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

/*
 * Samsung NAND flash controlled by some unspecified CPLD device.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/flashvar.h>
#include <dev/rndvar.h>

#include <machine/zaurus_var.h>

#include <arch/arm/xscale/pxa2x0var.h>

#define DEBUG
#ifdef DEBUG
#define DPRINTF(x) printf x
#else
#define DPRINTF(x)
#endif

/* CPLD register definitions */
#define CPLD_REG_ECCLPLB	0x00
#define CPLD_REG_ECCLPUB	0x04
#define CPLD_REG_ECCCP		0x08
#define CPLD_REG_ECCCNTR	0x0c
#define CPLD_REG_ECCCLRR	0x10
#define CPLD_REG_FLASHIO	0x14
#define CPLD_REG_FLASHCTL	0x18
#define  FLASHCTL_NCE0		(1<<0)
#define  FLASHCTL_CLE		(1<<1)
#define  FLASHCTL_ALE		(1<<2)
#define  FLASHCTL_NWP		(1<<3)
#define  FLASHCTL_NCE1		(1<<4)
#define  FLASHCTL_RYBY		(1<<5)
#define  FLASHCTL_NCE		(FLASHCTL_NCE0|FLASHCTL_NCE1)

/* CPLD register accesses */
#define CPLD_READ(sc, r)						\
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (r))
#define CPLD_WRITE(sc, r, v)						\
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (r), (v))
#define CPLD_SET(sc, r, v)						\
	CPLD_WRITE((sc), (r), CPLD_READ((sc), (r)) | (v))
#define CPLD_CLR(sc, r, v)						\
	CPLD_WRITE((sc), (r), CPLD_READ((sc), (r)) & ~(v))
#define CPLD_SETORCLR(sc, r, m, v)					\
	((v) ? CPLD_SET((sc), (r), (m)) : CPLD_CLR((sc), (r), (m)))

/* Offsets into OOB data. */
#define OOB_JFFS2_ECC0		0
#define OOB_JFFS2_ECC1		1
#define OOB_JFFS2_ECC2		2
#define OOB_JFFS2_ECC3		3
#define OOB_JFFS2_ECC4		6
#define OOB_JFFS2_ECC5		7
#define OOB_LOGADDR_0_LO	8
#define OOB_LOGADDR_0_HI	9
#define OOB_LOGADDR_1_LO	10
#define OOB_LOGADDR_1_HI	11
#define OOB_LOGADDR_2_LO	12
#define OOB_LOGADDR_2_HI	13

/*
 * Structure for managing logical blocks in a partition; allocated on
 * first use of each partition on a "safe" flash device.
 */
struct zflash_safe {
	dev_t			 sp_dev;
	u_long			 sp_pblks;	/* physical block count */
	u_long			 sp_lblks;	/* logical block count */
	u_int16_t		*sp_phyuse;	/* physical block usage */
	u_int			*sp_logmap;	/* logical to physical */
	u_int			 sp_pnext;	/* next physical block */
};

struct zflash_softc {
	struct flash_softc	 sc_flash;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	int			 sc_ioobbadblk;
	int			 sc_ioobpostbadblk;
	struct zflash_safe	*sc_safe[MAXPARTITIONS];
};

int	 zflashmatch(struct device *, void *, void *);
void	 zflashattach(struct device *, struct device *, void *);
int	 zflashdetach(struct device *, int);

u_int8_t zflash_reg8_read(void *, int);
int	 zflash_regx_read_page(void *, caddr_t, caddr_t);
void	 zflash_reg8_write(void *, int, u_int8_t);
int	 zflash_regx_write_page(void *, caddr_t, caddr_t);
void	 zflash_default_disklabel(void *, dev_t, struct disklabel *,
	     struct cpu_disklabel *);
int	 zflash_safe_strategy(void *, struct buf *);

int	 zflash_safe_start(struct zflash_softc *, dev_t);
void	 zflash_safe_stop(struct zflash_softc *, dev_t);

struct cfattach flash_pxaip_ca = {
	sizeof(struct zflash_softc), zflashmatch, zflashattach,
	zflashdetach, flashactivate
};

struct flash_ctl_tag zflash_ctl_tag = {
	zflash_reg8_read,
	zflash_regx_read_page,
	zflash_reg8_write,
	zflash_regx_write_page,
	zflash_default_disklabel,
	zflash_safe_strategy
};

int
zflashmatch(struct device *parent, void *match, void *aux)
{
	/* XXX call flashprobe(), yet to be implemented */
	return ZAURUS_ISC3000;
}

void
zflashattach(struct device *parent, struct device *self, void *aux)
{
	struct zflash_softc *sc = (struct zflash_softc *)self;
	struct pxaip_attach_args *pxa = aux;
	bus_addr_t addr = pxa->pxa_addr;
	bus_size_t size = pxa->pxa_size;

	sc->sc_iot = pxa->pxa_iot;

	if ((int)addr == -1 || (int)size == 0) {
		addr = 0x0c000000;
		size = 0x00001000;
	}

	if (bus_space_map(sc->sc_iot, addr, size, 0, &sc->sc_ioh) != 0) {
		printf(": failed to map controller\n");
		return;
	}

	/* Disable and write-protect the chip. */
	CPLD_WRITE(sc, CPLD_REG_FLASHCTL, FLASHCTL_NCE);

	flashattach(&sc->sc_flash, &zflash_ctl_tag, sc);

	switch (sc->sc_flash.sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C: /* C3000 */
		sc->sc_ioobpostbadblk = 4;
		sc->sc_ioobbadblk = 5;
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A: /* C3100 */
		sc->sc_ioobpostbadblk = 4;
		sc->sc_ioobbadblk = 0;
		break;
	}
}

int
zflashdetach(struct device *self, int flags)
{
	struct zflash_softc *sc = (struct zflash_softc *)self;
	int part;

	for (part = 0; part < MAXPARTITIONS; part++)
		zflash_safe_stop(sc, part);

	return (flashdetach(self, flags));
}

u_int8_t
zflash_reg8_read(void *arg, int reg)
{
	struct zflash_softc *sc = arg;
	u_int8_t value;

	switch (reg) {
	case FLASH_REG_DATA:
		value = CPLD_READ(sc, CPLD_REG_FLASHIO);
		break;
	case FLASH_REG_READY:
		value = (CPLD_READ(sc, CPLD_REG_FLASHCTL) &
		    FLASHCTL_RYBY) != 0;
		break;
	default:
#ifdef DIAGNOSTIC
		printf("%s: read from pseudo-register %02x\n",
		    sc->sc_flash.sc_dev.dv_xname, reg);
#endif
		value = 0;
		break;
	}
	return value;
}

void
zflash_reg8_write(void *arg, int reg, u_int8_t value)
{
	struct zflash_softc *sc = arg;

	switch (reg) {
	case FLASH_REG_DATA:
	case FLASH_REG_COL:
	case FLASH_REG_ROW:
	case FLASH_REG_CMD:
		CPLD_WRITE(sc, CPLD_REG_FLASHIO, value);
		break;
	case FLASH_REG_ALE:
		CPLD_SETORCLR(sc, CPLD_REG_FLASHCTL, FLASHCTL_ALE, value);
		break;
	case FLASH_REG_CLE:
		CPLD_SETORCLR(sc, CPLD_REG_FLASHCTL, FLASHCTL_CLE, value);
		break;
	case FLASH_REG_CE:
		CPLD_SETORCLR(sc, CPLD_REG_FLASHCTL, FLASHCTL_NCE, !value);
		break;
	case FLASH_REG_WP:
		CPLD_SETORCLR(sc, CPLD_REG_FLASHCTL, FLASHCTL_NWP, !value);
		break;
#ifdef DIAGNOSTIC
	default:
		printf("%s: write to pseudo-register %02x\n",
		    sc->sc_flash.sc_dev.dv_xname, reg);
#endif
	}
}

int
zflash_regx_read_page(void *arg, caddr_t data, caddr_t oob)
{
	struct zflash_softc *sc = arg;

	if (oob == NULL || sc->sc_flash.sc_flashdev->pagesize != 512) {
		flash_reg8_read_page(&sc->sc_flash, data, oob);
		return 0;
	}

	flash_reg8_read_page(&sc->sc_flash, data, oob);

	oob[OOB_JFFS2_ECC0] = 0xff;
	oob[OOB_JFFS2_ECC1] = 0xff;
	oob[OOB_JFFS2_ECC2] = 0xff;
	oob[OOB_JFFS2_ECC3] = 0xff;
	oob[OOB_JFFS2_ECC4] = 0xff;
	oob[OOB_JFFS2_ECC5] = 0xff;
	return 0;
}

int
zflash_regx_write_page(void *arg, caddr_t data, caddr_t oob)
{
	struct zflash_softc *sc = arg;
	int i;

	if (oob == NULL || sc->sc_flash.sc_flashdev->pagesize != 512) {
		flash_reg8_write_page(&sc->sc_flash, data, oob);
		return 0;
	}

	if (oob[OOB_JFFS2_ECC0] != 0xff || oob[OOB_JFFS2_ECC1] != 0xff ||
	    oob[OOB_JFFS2_ECC2] != 0xff || oob[OOB_JFFS2_ECC3] != 0xff ||
	    oob[OOB_JFFS2_ECC4] != 0xff || oob[OOB_JFFS2_ECC5] != 0xff) {
#ifdef DIAGNOSTIC
		printf("%s: non-FF ECC bytes in OOB data\n",
		    sc->sc_flash.sc_dev.dv_xname);
#endif
		return EINVAL;
	}

	CPLD_WRITE(sc, CPLD_REG_ECCCLRR, 0x00);
	for (i = 0; i < sc->sc_flash.sc_flashdev->pagesize / 2; i++)
		flash_reg8_write(&sc->sc_flash, FLASH_REG_DATA, data[i]);

	oob[OOB_JFFS2_ECC0] = ~CPLD_READ(sc, CPLD_REG_ECCLPUB);
	oob[OOB_JFFS2_ECC1] = ~CPLD_READ(sc, CPLD_REG_ECCLPLB);
	oob[OOB_JFFS2_ECC2] = (~CPLD_READ(sc, CPLD_REG_ECCCP) << 2) | 0x03;

	if (CPLD_READ(sc, CPLD_REG_ECCCNTR) != 0) {
		printf("%s: ECC failed\n", sc->sc_flash.sc_dev.dv_xname);
		oob[OOB_JFFS2_ECC0] = 0xff;
		oob[OOB_JFFS2_ECC1] = 0xff;
		oob[OOB_JFFS2_ECC2] = 0xff;
		return EIO;
	}

	CPLD_WRITE(sc, CPLD_REG_ECCCLRR, 0x00);
	for (; i < sc->sc_flash.sc_flashdev->pagesize; i++)
		flash_reg8_write(&sc->sc_flash, FLASH_REG_DATA, data[i]);

	oob[OOB_JFFS2_ECC3] = ~CPLD_READ(sc, CPLD_REG_ECCLPUB);
	oob[OOB_JFFS2_ECC4] = ~CPLD_READ(sc, CPLD_REG_ECCLPLB);
	oob[OOB_JFFS2_ECC5] = (~CPLD_READ(sc, CPLD_REG_ECCCP) << 2) | 0x03;

	if (CPLD_READ(sc, CPLD_REG_ECCCNTR) != 0) {
		printf("%s: ECC failed\n", sc->sc_flash.sc_dev.dv_xname);
		oob[OOB_JFFS2_ECC0] = 0xff;
		oob[OOB_JFFS2_ECC1] = 0xff;
		oob[OOB_JFFS2_ECC2] = 0xff;
		oob[OOB_JFFS2_ECC3] = 0xff;
		oob[OOB_JFFS2_ECC4] = 0xff;
		oob[OOB_JFFS2_ECC5] = 0xff;
		return EIO;
	}

	for (i = 0; i < sc->sc_flash.sc_flashdev->oobsize; i++)
		flash_reg8_write(&sc->sc_flash, FLASH_REG_DATA, oob[i]);

	oob[OOB_JFFS2_ECC0] = 0xff;
	oob[OOB_JFFS2_ECC1] = 0xff;
	oob[OOB_JFFS2_ECC2] = 0xff;
	oob[OOB_JFFS2_ECC3] = 0xff;
	oob[OOB_JFFS2_ECC4] = 0xff;
	oob[OOB_JFFS2_ECC5] = 0xff;
	return 0;
}

/*
 * A default disklabel with only one RAW_PART spanning the whole
 * device is passed to us. We add the partitions besides RAW_PART.
 */
void
zflash_default_disklabel(void *arg, dev_t dev, struct disklabel *lp,
    struct cpu_disklabel *clp)
{
	struct zflash_softc *sc = arg;
	long bsize = sc->sc_flash.sc_flashdev->pagesize;

	switch (sc->sc_flash.sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		DL_SETPSIZE(&lp->d_partitions[8], 7*1024*1024 / bsize);
		DL_SETPSIZE(&lp->d_partitions[9], 5*1024*1024 / bsize);
		DL_SETPSIZE(&lp->d_partitions[10], 4*1024*1024 / bsize);
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		DL_SETPSIZE(&lp->d_partitions[8], 7*1024*1024 / bsize);
		DL_SETPSIZE(&lp->d_partitions[9], 32*1024*1024 / bsize);
		DL_SETPSIZE(&lp->d_partitions[10], 89*1024*1024 / bsize);
		break;
	default:
		return;
	}

	/* The "smf" partition uses logical addressing. */
	DL_SETPOFFSET(&lp->d_partitions[8], 0);
	lp->d_partitions[8].p_fstype = FS_OTHER;

	/* The "root" partition uses physical addressing. */
	DL_SETPSIZE(&lp->d_partitions[9], DL_GETPSIZE(&lp->d_partitions[8]));
	lp->d_partitions[9].p_fstype = FS_OTHER;

	/* The "home" partition uses physical addressing. */
	DL_SETPOFFSET(&lp->d_partitions[10],
	    DL_GETPOFFSET(&lp->d_partitions[9]) + DL_GETPSIZE(&lp->d_partitions[9]));
	lp->d_partitions[10].p_fstype = FS_OTHER;
	lp->d_npartitions = 11;

	lp->d_version = 1;
	/* Re-calculate the checksum. */
	lp->d_checksum = dkcksum(lp);
}

/*
 * Sharp's access strategy for bad blocks management and wear-leveling.
 */

#define PHYUSE_STATUS(v)	((v) & 0x00ff)
#define  P_BADBLOCK		0x0000
#define  P_POSTBADBLOCK		0x00f0
#define  P_NORMALBLOCK		0x00ff
#define PHYUSE_WRITTEN(v)	((v) & 0xff00)
#define  P_DUST			0x0000
#define  P_LOGICAL		0x0100
#define  P_JFFS2		0x0300

void	  zflash_write_strategy(struct zflash_softc *, struct buf *,
    struct zflash_safe *, u_int, u_int);
u_int	  zflash_safe_next_block(struct zflash_safe *);

u_char	  zflash_oob_status_decode(u_char);
u_int16_t zflash_oob_status(struct zflash_softc *, u_char *);
u_int	  zflash_oob_logno(struct zflash_softc *, u_char *);
void	  zflash_oob_set_status(struct zflash_softc *, u_char *, u_int16_t);
void	  zflash_oob_set_logno(struct zflash_softc *, u_char *, u_int);

int
zflash_safe_strategy(void *arg, struct buf *bp)
{
	struct zflash_softc *sc = arg;
	struct zflash_safe *sp;
	u_int logno;
	u_int blkofs;
	u_int blkno;
	int error;
	int part;
	int i;

	/* Initialize logical blocks management on the fly. */
	/* XXX toss everything when the disklabel has changed. */
	if ((error = zflash_safe_start(sc, bp->b_dev)) != 0) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;
		return 0;
	}

	part = flashpart(bp->b_dev);
	sp = sc->sc_safe[part];

	logno = bp->b_blkno / (sc->sc_flash.sc_flashdev->blkpages *
	    sc->sc_flash.sc_flashdev->pagesize / DEV_BSIZE);
	blkofs = bp->b_blkno % (sc->sc_flash.sc_flashdev->blkpages *
	    sc->sc_flash.sc_flashdev->pagesize / DEV_BSIZE);

	/* If exactly at end of logical flash, return EOF, else error. */
	if (logno == sp->sp_lblks && blkofs == 0) {
		bp->b_resid = bp->b_bcount;
		return 0;
	} else if (logno >= sp->sp_lblks) {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		return 0;
	}

	/* Writing is more complicated, so handle it separately. */
	if ((bp->b_flags & B_READ) == 0) {
		flash_chip_enable(&sc->sc_flash);
		zflash_write_strategy(sc, bp, sp, logno, blkofs);
		flash_chip_disable(&sc->sc_flash);
		return 0;
	}

	/* Get the physical flash block number for this logical one. */
	blkno = sp->sp_logmap[logno];

	/* Unused logical blocks read as all 0xff. */
	if ((bp->b_flags & B_READ) != 0 && blkno == UINT_MAX) {
		for (i = 0; i < sc->sc_flash.sc_flashdev->pagesize; i++)
			((u_char *)bp->b_data)[i] = 0xff;
		bp->b_resid = bp->b_bcount -
		    sc->sc_flash.sc_flashdev->pagesize;
		return 0;
	}

	/* Update the block number in the buffer with the physical one. */
	bp->b_blkno = blkno * (sc->sc_flash.sc_flashdev->blkpages *
	    sc->sc_flash.sc_flashdev->pagesize / DEV_BSIZE) + blkofs;

	/* Process the modified transfer buffer normally. */
	return 1;
}

void
zflash_write_strategy(struct zflash_softc *sc, struct buf *bp,
    struct zflash_safe *sp, u_int logno, u_int logofs)
{
	size_t	bufsize;
	u_char *buf = NULL;
	size_t	oobsize;
	u_char *oob = NULL;
	u_int	oblkno;
	u_int	nblkno;
	int	error;

	/* Not efficient, but we always transfer one page for now. */
	if (bp->b_bcount < sc->sc_flash.sc_flashdev->pagesize) {
		bp->b_error = EINVAL;
		goto bad;
	}

	/* Allocate a temporary buffer for one flash block. */
	bufsize = sc->sc_flash.sc_flashdev->blkpages * 
	    sc->sc_flash.sc_flashdev->pagesize;
	buf = (u_char *)malloc(bufsize, M_DEVBUF, M_NOWAIT);
	if (buf == NULL) {
		bp->b_error = ENOMEM;
		goto bad;
	}

	/* Allocate a temporary buffer for one spare area. */
	oobsize = sc->sc_flash.sc_flashdev->oobsize;
	oob = (u_char *)malloc(oobsize, M_DEVBUF, M_NOWAIT);
	if (oob == NULL) {
		bp->b_error = ENOMEM;
		goto bad;
	}

	/* Read the old logical block into the temporary buffer. */
	oblkno = sp->sp_logmap[logno];
	if (oblkno != UINT_MAX) {
		error = flash_chip_read_block(&sc->sc_flash, oblkno, buf);
		if (error != 0) {
			bp->b_error = error;
			goto bad;
		}
	} else
		/* Unused logical blocks read as all 0xff. */
		memset(buf, 0xff, bufsize);

	/* Transfer the page into the logical block buffer. */
	bcopy(bp->b_data, buf + logofs * sc->sc_flash.sc_flashdev->pagesize,
	    sc->sc_flash.sc_flashdev->pagesize);

	/* Generate OOB data for the spare area of this logical block. */
	memset(oob, 0xff, oobsize);
	zflash_oob_set_status(sc, oob, P_NORMALBLOCK);
	zflash_oob_set_logno(sc, oob, logno);

	while (1) {
		/* Search for a free physical block. */
		nblkno = zflash_safe_next_block(sp);
		if (nblkno == UINT_MAX) {
			printf("%s: no spare block, giving up on logical"
			    " block %u\n", sc->sc_flash.sc_dev.dv_xname,
			    logno);
			bp->b_error = ENOSPC;
			goto bad;
		}

#if 0
		DPRINTF(("%s: moving logical block %u from physical %u to %u\n",
		    sc->sc_flash.sc_dev.dv_xname, logno, oblkno, nblkno));
#endif

		/* Erase the free physical block. */
		if (flash_chip_erase_block(&sc->sc_flash, nblkno) != 0) {
			printf("%s: can't erase block %u, retrying\n",
			    sc->sc_flash.sc_dev.dv_xname, nblkno);
			sp->sp_phyuse[nblkno] = P_POSTBADBLOCK | P_DUST;
			continue;
		}

		/* Write the logical block to the free physical block. */
		if (flash_chip_write_block(&sc->sc_flash, nblkno, buf, oob)) {
			printf("%s: can't write block %u, retrying\n",
			    sc->sc_flash.sc_dev.dv_xname, nblkno);
			goto trynext;
		}

		/* Yeah, we re-wrote that logical block! */
		break;
	trynext:
		sp->sp_phyuse[nblkno] = P_POSTBADBLOCK | P_DUST;
		(void)flash_chip_erase_block(&sc->sc_flash, nblkno);
	}

	/* Map the new physical block. */
	sp->sp_logmap[logno] = nblkno;
	sp->sp_phyuse[nblkno] = PHYUSE_STATUS(sp->sp_phyuse[nblkno])
	    | P_LOGICAL;

	/* Erase the old physical block. */
	if (oblkno != UINT_MAX) {
		sp->sp_phyuse[oblkno] = PHYUSE_STATUS(sp->sp_phyuse[oblkno])
		    | P_DUST;
		error = flash_chip_erase_block(&sc->sc_flash, oblkno);
		if (error != 0) {
			printf("%s: can't erase old block %u\n",
			    sc->sc_flash.sc_dev.dv_xname, oblkno);
			bp->b_error = error;
			goto bad;
		}
	}

	bp->b_resid = bp->b_bcount - sc->sc_flash.sc_flashdev->pagesize;
	free(oob, M_DEVBUF);
	free(buf, M_DEVBUF);
	return;
bad:
	bp->b_flags |= B_ERROR;
	if (oob != NULL)
		free(oob, M_DEVBUF);
	if (buf != NULL)
		free(buf, M_DEVBUF);
}

int
zflash_safe_start(struct zflash_softc *sc, dev_t dev)
{
	u_char oob[FLASH_MAXOOBSIZE];
	struct disklabel *lp = sc->sc_flash.sc_dk.dk_label;
	struct zflash_safe *sp;
	u_int16_t *phyuse;
	u_int *logmap;
	u_int blksect;
	u_int blkno;
	u_int logno;
	u_int unusable;
	int part;

	part = flashpart(dev);
	if (sc->sc_safe[part] != NULL)
		return 0;

	/* We can only handle so much OOB data here. */
	if (sc->sc_flash.sc_flashdev->oobsize > FLASH_MAXOOBSIZE)
		return EIO;

	/* Safe partitions must start on a flash block boundary. */
	blksect = (sc->sc_flash.sc_flashdev->blkpages *
	    sc->sc_flash.sc_flashdev->pagesize) / lp->d_secsize;
	if (DL_GETPOFFSET(&lp->d_partitions[part]) % blksect)
		return EIO;

	MALLOC(sp, struct zflash_safe *, sizeof(struct zflash_safe),
	    M_DEVBUF, M_NOWAIT);
	if (sp == NULL)
		return ENOMEM;

	bzero(sp, sizeof(struct zflash_safe));
	sp->sp_dev = dev;

	sp->sp_pblks = DL_GETPSIZE(&lp->d_partitions[part]) / blksect;
	sp->sp_lblks = sp->sp_pblks;

	/* Try to reserve a number of spare physical blocks. */
	switch (sc->sc_flash.sc_flashdev->id) {
	case FLASH_DEVICE_SAMSUNG_K9F2808U0C:
		sp->sp_lblks -= 24; /* C3000 */
		break;
	case FLASH_DEVICE_SAMSUNG_K9F1G08U0A:
		sp->sp_lblks -= 4;  /* C3100 */
		break;
	}

	DPRINTF(("pblks %u lblks %u\n", sp->sp_pblks, sp->sp_lblks));

	/* Next physical block to use; randomize for wear-leveling. */
	sp->sp_pnext = arc4random() % sp->sp_pblks;

	/* Allocate physical block usage map. */
	phyuse = (u_int16_t *)malloc(sp->sp_pblks * sizeof(u_int16_t),
	    M_DEVBUF, M_NOWAIT);
	if (phyuse == NULL) {
		FREE(sp, M_DEVBUF);
		return ENOMEM;
	}
	sp->sp_phyuse = phyuse;

	/* Allocate logical to physical block map. */
	logmap = (u_int *)malloc(sp->sp_lblks * sizeof(u_int),
	    M_DEVBUF, M_NOWAIT);
	if (logmap == NULL) {
		FREE(phyuse, M_DEVBUF);
		FREE(sp, M_DEVBUF);
		return ENOMEM;
	}
	sp->sp_logmap = logmap;

	/* Initialize the physical and logical block maps. */
	for (blkno = 0; blkno < sp->sp_pblks; blkno++)
		phyuse[blkno] = P_BADBLOCK | P_DUST;
	for (blkno = 0; blkno < sp->sp_lblks; blkno++)
		logmap[blkno] = UINT_MAX;

	/* Update physical block usage map with real data. */
	unusable = 0;
	flash_chip_enable(&sc->sc_flash);
	for (blkno = 0; blkno < sp->sp_pblks; blkno++) {
		long pageno;

		pageno = blkno * sc->sc_flash.sc_flashdev->blkpages;
		if (flash_chip_read_oob(&sc->sc_flash, pageno, oob) != 0) {
			DPRINTF(("blkno %u: can't read oob data\n", blkno));
			phyuse[blkno] = P_POSTBADBLOCK | P_DUST;
			unusable++;
			continue;
		}

		phyuse[blkno] = zflash_oob_status(sc, oob);
		if (PHYUSE_STATUS(phyuse[blkno]) != P_NORMALBLOCK) {
			DPRINTF(("blkno %u: badblock status %x\n", blkno,
			    PHYUSE_STATUS(phyuse[blkno])));
			phyuse[blkno] |= P_DUST;
			unusable++;
			continue;
		}

		logno = zflash_oob_logno(sc, oob);
		if (logno == UINT_MAX) {
			DPRINTF(("blkno %u: can't read logno\n", blkno));
			phyuse[blkno] |= P_JFFS2;
			unusable++;
			continue;
		}

		if (logno == USHRT_MAX) {
			phyuse[blkno] |= P_DUST;
			/* Block is usable and available. */
			continue;
		}

		if (logno >= sp->sp_lblks) {
			DPRINTF(("blkno %u: logno %u too big\n", blkno,
			    logno));
			phyuse[blkno] |= P_JFFS2;
			unusable++;
			continue;
		}

		if (logmap[logno] == UINT_MAX) {
			phyuse[blkno] |= P_LOGICAL;
			logmap[logno] = blkno;
		} else {
			/* Duplicate logical block! */
			DPRINTF(("blkno %u: duplicate logno %u\n", blkno,
			    logno));
			phyuse[blkno] |= P_DUST;
		}
	}
	flash_chip_disable(&sc->sc_flash);

	if (unusable > 0)
		printf("%s: %u unusable blocks\n",
		    sc->sc_flash.sc_dev.dv_xname, unusable);

	sc->sc_safe[part] = sp;
	return 0;
}

void
zflash_safe_stop(struct zflash_softc *sc, dev_t dev)
{
	struct zflash_safe *sp;
	int part;

	part = flashpart(dev);
	if (sc->sc_safe[part] == NULL)
		return;

	sp = sc->sc_safe[part];
	free(sp->sp_phyuse, M_DEVBUF);
	free(sp->sp_logmap, M_DEVBUF);
	FREE(sp, M_DEVBUF);
	sc->sc_safe[part] = NULL;
}

u_int
zflash_safe_next_block(struct zflash_safe *sp)
{
	u_int blkno;

	for (blkno = sp->sp_pnext; blkno < sp->sp_pblks; blkno++)
		if (sp->sp_phyuse[blkno] == (P_NORMALBLOCK|P_DUST)) {
			sp->sp_pnext = blkno + 1;
			return blkno;
		}

	for (blkno = 0; blkno < sp->sp_pnext; blkno++)
		if (sp->sp_phyuse[blkno] == (P_NORMALBLOCK|P_DUST)) {
			sp->sp_pnext = blkno + 1;
			return blkno;
		}

	return UINT_MAX;
}

/*
 * Correct single bit errors in the block's status byte.
 */
u_char
zflash_oob_status_decode(u_char status)
{
	u_char bit;
	int count;

	/* Speed-up. */
	if (status == 0xff)
		return 0xff;

	/* Count the number of bits set in the byte. */
	for (count = 0, bit = 0x01; bit != 0x00; bit <<= 1)
		if ((status & bit) != 0)
			count++;

	return (count > 6) ? 0xff : 0x00;
}

/*
 * Decode the block's status byte into a value for the phyuse map.
 */
u_int16_t
zflash_oob_status(struct zflash_softc *sc, u_char *oob)
{
	u_char status;

	status = zflash_oob_status_decode(oob[sc->sc_ioobbadblk]);
	if (status != 0xff)
		return P_BADBLOCK;

	status = zflash_oob_status_decode(oob[sc->sc_ioobpostbadblk]);
	if (status != 0xff)
		return P_POSTBADBLOCK;

	return P_NORMALBLOCK;
}

/*
 * Extract the 16-bit logical block number corresponding to a physical
 * block from the physical block's OOB data.
 */
u_int
zflash_oob_logno(struct zflash_softc *sc, u_char *oob)
{
	int idx_lo, idx_hi;
	u_int16_t word;
	u_int16_t bit;
	int parity;

	/* Find a matching pair of high and low bytes. */
	if (oob[OOB_LOGADDR_0_LO] == oob[OOB_LOGADDR_1_LO] &&
	    oob[OOB_LOGADDR_0_HI] == oob[OOB_LOGADDR_1_HI]) {
		idx_lo = OOB_LOGADDR_0_LO;
		idx_hi = OOB_LOGADDR_0_HI;
	} else if (oob[OOB_LOGADDR_1_LO] == oob[OOB_LOGADDR_2_LO] &&
	    oob[OOB_LOGADDR_1_HI] == oob[OOB_LOGADDR_2_HI]) {
		idx_lo = OOB_LOGADDR_1_LO;
		idx_hi = OOB_LOGADDR_1_HI;
	} else if (oob[OOB_LOGADDR_2_LO] == oob[OOB_LOGADDR_0_LO] &&
	    oob[OOB_LOGADDR_2_HI] == oob[OOB_LOGADDR_0_HI]) {
		idx_lo = OOB_LOGADDR_2_LO;
		idx_hi = OOB_LOGADDR_2_HI;
	} else
		/* Block's OOB data may be invalid. */
		return UINT_MAX;

	word = ((u_int16_t)oob[idx_lo] << 0) |
	    ((u_int16_t)oob[idx_hi] << 8);

	/* Check for parity error in the logical block number. */
	for (parity = 0, bit = 0x0001; bit != 0x0000; bit <<= 1)
		if ((word & bit) != 0)
			parity++;
	if ((parity & 1) != 0)
		return UINT_MAX;

	/* No logical block number assigned to this block? */
	if (word == USHRT_MAX)
		return word;

	/* Return the validated logical block number. */
	return (word & 0x07fe) >> 1;
}

void
zflash_oob_set_status(struct zflash_softc *sc, u_char *oob, u_int16_t phyuse)
{
	switch (PHYUSE_STATUS(phyuse)) {
	case P_NORMALBLOCK:
		oob[sc->sc_ioobbadblk] = 0xff;
		oob[sc->sc_ioobpostbadblk] = 0xff;
		break;
	case P_BADBLOCK:
		oob[sc->sc_ioobbadblk] = 0x00;
		oob[sc->sc_ioobpostbadblk] = 0x00;
		break;
	case P_POSTBADBLOCK:
		oob[sc->sc_ioobbadblk] = 0xff;
		oob[sc->sc_ioobpostbadblk] = 0x00;
		break;
	}
}

void
zflash_oob_set_logno(struct zflash_softc *sc, u_char *oob, u_int logno)
{
	u_int16_t word;
	u_int16_t bit;
	u_char lo;
	u_char hi;
	int parity;

	/* Why do we set the most significant bit? */
	word = ((logno & 0x03ff) << 1) | 0x1000;

	/* Calculate the parity. */
	for (bit = 0x0001; bit != 0x0000; bit <<= 1)
		if ((word & bit) != 0)
			parity++;
	if ((parity & 1) != 0)
		word |= 0x0001;

	lo = word & 0x00ff;
	hi = (word & 0xff00) >> 8;

	oob[OOB_LOGADDR_0_LO] = lo;
	oob[OOB_LOGADDR_0_HI] = hi;
	oob[OOB_LOGADDR_1_LO] = lo;
	oob[OOB_LOGADDR_1_HI] = hi;
	oob[OOB_LOGADDR_2_LO] = lo;
	oob[OOB_LOGADDR_2_HI] = hi;
}
