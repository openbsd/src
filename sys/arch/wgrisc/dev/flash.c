/*	$OpenBSD: flash.c,v 1.1 1997/02/23 21:59:27 pefo Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pio.h>

#include <wgrisc/riscbus/riscbus.h>
#include <wgrisc/dev/flashreg.h>
#include <wgrisc/wgrisc/wgrisctype.h>

extern int cputype;

void flashattach __P((struct device *, struct device *, void *));
int  flashmatch __P((struct device *, void *, void *));

struct flashtype {
	char	*fl_name;
	int	fl_size;
	int	fl_blksz;
	u_char	fl_manf;
	u_char	fl_type;
};
struct flashtype flashlist[] = {
	{ "Samsung KM29N16000",	2097152, 4096, 0xec, 0x64 },
	{ NULL },
};

struct flashsoftc {
	struct device	sc_dev;
	int		sc_prot;
	size_t		sc_size;
	int		sc_present;
	struct flashtype *sc_ftype;
};

struct cfattach flash_ca = {
	sizeof(struct flashsoftc), flashmatch, flashattach
};
struct cfdriver flash_cd = {
	NULL, "flash", DV_DISK, 0	/* Yes! We want is as root device */
};

int
flashmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;

	if(!BUS_MATCHNAME(ca, "flash"))
		return(0);
	return(1);
}

void
flashattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct flashsoftc *sc = (struct flashsoftc *)self;
	struct flashtype *flist;
	int i, manf, type;

	switch(cputype) {
	case WGRISC9100:	/* WGRISC9100 can have 4 chips */
		sc->sc_present = 0;
		sc->sc_ftype = NULL;
		OUT_FL_CTRL(0, 0);	/* All CS lines high */
		for(i = 0; i < 4; i++) {
			OUT_FL_CLE(FL_READID, (1 << i));
			OUT_FL_ALE1(0, (1 << i));
			manf = IN_FL_DATA;
			type = IN_FL_DATA;
			flist = flashlist;
			while(flist->fl_name != 0) {
				if(flist->fl_manf == manf &&
				   flist->fl_type == type) {
					sc->sc_present |= 1 << i;
					sc->sc_size += flist->fl_size;
					if(sc->sc_ftype == NULL) {
						sc->sc_ftype = flist;
					}
					else if(sc->sc_ftype == flist) {
					}
/* XXX Protection test type dependent ? */
					OUT_FL_CLE(FL_READSTAT, (1 << i));
					if(!(IN_FL_DATA & FLST_UNPROT)) {
						sc->sc_prot = 1;
					}
					break;
				}
				flist++;
			}
		}
		break;

	default:
		printf("flash: Unknown cputype '%d'", cputype);
	}
	if(sc->sc_ftype != NULL) {
		printf(" %s, %d*%d bytes%s.", sc->sc_ftype->fl_name,
				sc->sc_size / sc->sc_ftype->fl_size,
				sc->sc_ftype->fl_size,
				sc->sc_prot ? " Write protected" : "");
	}
	else {
		printf("WARNING! Flash type not identified!");
	}
	printf("\n");
}

static int
flashgetblk(sc, blk, offs, cnt)
	struct flashsoftc *sc;
	char *blk;
	size_t offs;
	size_t cnt;
{
	int chip;
	int blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_READ1, chip);
	OUT_FL_ALE3(blkadr, chip);
	WAIT_FL_RDY;
	while(cnt--) {
		*blk++ = IN_FL_DATA;
	}
	return(0);
}

static int
flashputblk(sc, blk, offs, cnt)
	struct flashsoftc *sc;
	char *blk;
	size_t offs;
	size_t cnt;
{
	int chip;
	int blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_SEQDI, chip);
	OUT_FL_ALE3(blkadr, chip);
	while(cnt--) {
		OUT_FL_DATA(*blk);
		blk++;
	}
	OUT_FL_CLE(FL_PGPROG, chip);
	WAIT_FL_RDY;
	OUT_FL_CLE(FL_READSTAT, chip);
	if(IN_FL_DATA & FLST_ERROR) {
		return(-1);
	}
	return(0);
}

static int
flasheraseblk(sc, offs)
	struct flashsoftc *sc;
	size_t offs;
{
	int chip;
	int blkadr;

	chip = 1 << (offs / sc->sc_ftype->fl_size);
	blkadr = offs % sc->sc_ftype->fl_size;

	OUT_FL_CLE(FL_BLERASE, chip);
	OUT_FL_ALE2(blkadr, chip);
	OUT_FL_CLE(FL_REERASE, chip);
	WAIT_FL_RDY;
	OUT_FL_CLE(FL_READSTAT, chip);
	if(IN_FL_DATA & FLST_ERROR) {
		return(-1);
	}
	return(0);
}

/*ARGSUSED*/
int
flashopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= flash_cd.cd_ndevs || flash_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return(0);
}

/*ARGSUSED*/
int
flashclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	return(0);
}

/*ARGSUSED*/
int
flashioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	u_char *data;
	int     cmd, flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];
	int error = 0;

	switch (cmd) {
	default:
		error = ENOTTY;
		break;
	}
	return(error);
}

void
flashstrategy(bp)
	struct buf *bp;
{
	int unit = minor(bp->b_dev);
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];
	int error = 0;
	size_t offs, xfer, cnt;
	caddr_t buf;

	offs = bp->b_blkno << DEV_BSHIFT;	/* Start address */
	buf = bp->b_data;
	bp->b_resid = bp->b_bcount;
	if(offs < sc->sc_size) {
		xfer = bp->b_resid;
		if(offs + xfer > sc->sc_size) {
			xfer = sc->sc_size - offs;
		}
		if(bp->b_flags & B_READ) {
			bp->b_resid -= xfer;
			while(xfer > 0) {
				cnt = (xfer > 256) ? 256 : xfer;
				flashgetblk(sc, buf, offs, cnt);
				xfer -= cnt;
				offs += cnt;
				buf += cnt;
			}
		}
		else {
			while(xfer > 0) {
				if((offs & (sc->sc_ftype->fl_blksz - 1)) == 0 &&
				   (xfer >= sc->sc_ftype->fl_blksz)) {
					if(flasheraseblk(sc, offs))
						error = EIO;
				}
				cnt = (xfer > 256) ? 256 : xfer;
				if(flashputblk(sc, buf, offs, cnt))
					error = EIO;
				xfer -= cnt;
				buf += cnt;
				offs += cnt;
				bp->b_resid -= cnt;
			}
		}
	}
	else if(!(bp->b_flags & B_READ)) {	/* No space for write */
		error = EIO;
	}
	if(error) {
		bp->b_error = error;
		bp->b_flags |= B_ERROR;
	}

	biodone(bp);
}

/*ARGSUSED*/
int
flashread(dev, uio, ioflag)
        dev_t dev;
        struct uio *uio;
	int ioflag;
{
	return (physio(flashstrategy, NULL, dev, B_READ, minphys, uio));
}

/*ARGSUSED*/
int
flashwrite(dev, uio, ioflag)
        dev_t dev;
        struct uio *uio;
        int ioflag;
{
	return (physio(flashstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
flashdump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	return(ENODEV);
}

int
flashsize(dev)
	dev_t dev;
{
	return(0);
}
