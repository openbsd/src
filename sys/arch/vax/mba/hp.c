/*	$OpenBSD: hp.c,v 1.9 1998/10/03 21:18:59 millert Exp $ */
/*	$NetBSD: hp.c,v 1.15 1997/06/24 01:09:37 thorpej Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Simple device driver routine for massbuss disks.
 * TODO:
 *  Fix support for Standard DEC BAD144 bad block forwarding.
 *  Be able to to handle soft/hard transfer errors.
 *  Handle non-data transfer interrupts.
 *  Autoconfiguration of disk drives 'on the fly'.
 *  Handle disk media changes.
 *  Dual-port operations should be supported.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/dkio.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/ioccom.h>
#include <sys/fcntl.h>
#include <sys/syslog.h>
#include <sys/reboot.h>

#include <machine/trap.h>
#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>
#include <machine/rpb.h>

#include <vax/mba/mbavar.h>
#include <vax/mba/mbareg.h>
#include <vax/mba/hpreg.h>

#define	HPMASK 0xffff

struct	hp_softc {
	struct	device	sc_dev;
	struct	disk sc_disk;
	struct	mba_device sc_md;	/* Common struct used by mbaqueue. */
	int	sc_wlabel;		/* Disklabel area is writable */
	int	sc_physnr;		/* Physical disk number */
};

int     hpmatch __P((struct device *, void *, void *));
void    hpattach __P((struct device *, struct device *, void *));
void	hpstrategy __P((struct buf *));
void	hpstart __P((struct mba_device *));
int	hpattn __P((struct mba_device *));
enum	xfer_action hpfinish __P((struct mba_device *, int, int *));
int	hpopen __P((dev_t, int, int));
int	hpclose __P((dev_t, int, int));
int	hpioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	hpdump __P((dev_t, caddr_t, caddr_t, size_t));
int	hpread __P((dev_t, struct uio *));
int	hpwrite __P((dev_t, struct uio *));
int	hpsize __P((dev_t));

struct	cfdriver hp_cd = {
	NULL, "hp", DV_DISK
};

struct	cfattach hp_ca = {
	sizeof(struct hp_softc), hpmatch, hpattach
};

/*
 * Check if this is a disk drive; done by checking type from mbaattach.
 */
int
hpmatch(parent, match, aux)
	struct	device *parent;
	void	*match, *aux;
{
	struct	cfdata *cf = match;
	struct	mba_attach_args *ma = aux;

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ma->unit)
		return 0;

	if (ma->devtyp != MB_RP)
		return 0;

	return 1;
}

/*
 * Disk drive found; fake a disklabel and try to read the real one.
 * If the on-disk label can't be read; we lose.
 */
void
hpattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct	hp_softc *sc = (void *)self;
	struct	mba_softc *ms = (void *)parent;
	struct	disklabel *dl;
	struct  mba_attach_args *ma = aux;
	char	*msg;

	/*
	 * Init the common struct for both the adapter and its slaves.
	 */
	sc->sc_md.md_softc = (void *)sc;	/* Pointer to this softc */
	sc->sc_md.md_mba = (void *)parent;	/* Pointer to parent softc */
	sc->sc_md.md_start = hpstart;		/* Disk start routine */
	sc->sc_md.md_attn = hpattn;		/* Disk attention routine */
	sc->sc_md.md_finish = hpfinish;		/* Disk xfer finish routine */

	ms->sc_md[ma->unit] = &sc->sc_md;	/* Per-unit backpointer */

	sc->sc_physnr = ma->unit;
	/*
	 * Init and attach the disk structure.
	 */
	sc->sc_disk.dk_name = sc->sc_dev.dv_xname;
	disk_attach(&sc->sc_disk);

	/*
	 * Fake a disklabel to be able to read in the real label.
	 */
	dl = sc->sc_disk.dk_label;

	dl->d_secsize = DEV_BSIZE;
	dl->d_ntracks = 1;
	dl->d_nsectors = 32;
	dl->d_secpercyl = 32;

	/*
	 * Read in label.
	 */
	if ((msg = readdisklabel(makedev(0, self->dv_unit * 8), hpstrategy,
	    dl, NULL, 0)) != NULL)
		printf(": %s", msg);
	printf(": %s, size = %d sectors\n", dl->d_typename, dl->d_secperunit);
	/*
	 * check if this was what we booted from.
	 */
	if ((B_TYPE(bootdev) == BDEV_HP) && (ma->unit == B_UNIT(bootdev)) &&
	    (ms->sc_physnr == B_ADAPTOR(bootdev)))
		booted_from = self;
}


void
hpstrategy(bp)
	struct buf *bp;
{
	struct	hp_softc *sc;
	struct	buf *gp;
	int	unit, s;

	unit = DISKUNIT(bp->b_dev);
	sc = hp_cd.cd_devs[unit];

	if (bounds_check_with_label(bp, sc->sc_disk.dk_label,
	    sc->sc_disk.dk_cpulabel, sc->sc_wlabel) <= 0)
		goto done;
	s = splbio();

	gp = sc->sc_md.md_q.b_actf;
	disksort(&sc->sc_md.md_q, bp);
	if (gp == 0)
		mbaqueue(&sc->sc_md);

	splx(s);
	return;

done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Start transfer on given disk. Called from mbastart().
 */
void
hpstart(md)
	struct	mba_device *md;
{
	struct	hp_softc *sc = md->md_softc;
	struct	mba_regs *mr = md->md_mba->sc_mbareg;
	volatile struct	hp_regs *hr;
	struct	disklabel *lp = sc->sc_disk.dk_label;
	struct	buf *bp = md->md_q.b_actf;
	unsigned bn, cn, sn, tn;
	int	part = DISKPART(bp->b_dev);

	/*
	 * Collect statistics.
	 */
	disk_busy(&sc->sc_disk);
	sc->sc_disk.dk_seek++;

	hr = (void *)&mr->mba_md[DISKUNIT(bp->b_dev)];

	bn = bp->b_blkno + lp->d_partitions[part].p_offset;
	if (bn) {
		cn = bn / lp->d_secpercyl;
		sn = bn % lp->d_secpercyl;
		tn = sn / lp->d_nsectors;
		sn = sn % lp->d_nsectors;
	} else
		cn = sn = tn = 0;

	hr->hp_dc = cn;
	hr->hp_da = (tn << 8) | sn;
	if (bp->b_flags & B_READ)
		hr->hp_cs1 = HPCS_READ;		/* GO */
	else
		hr->hp_cs1 = HPCS_WRITE;
}

int
hpopen(dev, flag, fmt)
	dev_t	dev;
	int	flag, fmt;
{
	struct	hp_softc *sc;
	int	unit, part;

	unit = DISKUNIT(dev);
	if (unit >= hp_cd.cd_ndevs)
		return ENXIO;
	sc = hp_cd.cd_devs[unit];
	if (sc == 0)
		return ENXIO;

	part = DISKPART(dev);

	if (part >= sc->sc_disk.dk_label->d_npartitions)
		return ENXIO;

	switch (fmt) {
	case 	S_IFCHR:
		sc->sc_disk.dk_copenmask |= (1 << part);
		break;

	case	S_IFBLK:
		sc->sc_disk.dk_bopenmask |= (1 << part);
		break;
	}
	sc->sc_disk.dk_openmask =
	    sc->sc_disk.dk_copenmask | sc->sc_disk.dk_bopenmask;

	return 0;
}

int
hpclose(dev, flag, fmt)
	dev_t	dev;
	int	flag, fmt;
{
	struct	hp_softc *sc;
	int	unit, part;

	unit = DISKUNIT(dev);
	sc = hp_cd.cd_devs[unit];

	part = DISKPART(dev);

	switch (fmt) {
	case 	S_IFCHR:
		sc->sc_disk.dk_copenmask &= ~(1 << part);
		break;

	case	S_IFBLK:
		sc->sc_disk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sc->sc_disk.dk_openmask =
	    sc->sc_disk.dk_copenmask | sc->sc_disk.dk_bopenmask;

	return 0;
}

int
hpioctl(dev, cmd, addr, flag, p)
	dev_t	dev;
	u_long	cmd;
	caddr_t	addr;
	int	flag;
	struct	proc *p;
{
	struct	hp_softc *sc = hp_cd.cd_devs[DISKUNIT(dev)];
	struct	disklabel *lp = sc->sc_disk.dk_label;
	int	error;

	switch (cmd) {
	case	DIOCGDINFO:
		bcopy(lp, addr, sizeof (struct disklabel));
		return 0;

	case	DIOCGPART:
		((struct partinfo *)addr)->disklab = lp;
		((struct partinfo *)addr)->part =
		    &lp->d_partitions[DISKPART(dev)];
		break;

	case	DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		return setdisklabel(lp, (struct disklabel *)addr, 0, 0);

	case	DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			sc->sc_wlabel = 1;
			error = writedisklabel(dev, hpstrategy, lp, 0);
			sc->sc_wlabel = 0;
		}
		return error;
	case	DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		sc->sc_wlabel = 1;
		break;

	default:
		printf("hpioctl: command %x\n", (unsigned int)cmd);
		return ENOTTY;
	}
	return 0;
}

/*
 * Called when a transfer is finished. Check if transfer went OK,
 * Return info about what-to-do-now.
 */
enum xfer_action
hpfinish(md, mbasr, attn)
	struct	mba_device *md;
	int	mbasr, *attn;
{
	struct	hp_softc *sc = md->md_softc;
	struct	buf *bp = md->md_q.b_actf;
	volatile struct  mba_regs *mr = md->md_mba->sc_mbareg;
	volatile struct	hp_regs *hr = (void *)&mr->mba_md[DISKUNIT(bp->b_dev)];
	int	er1, er2;
	volatile int bc; /* to get GCC read whole longword */
	unsigned byte;

	er1 = hr->hp_er1 & HPMASK;
	er2 = hr->hp_er2 & HPMASK;
	hr->hp_er1 = hr->hp_er2 = 0;
hper1:
	switch (ffs(er1) - 1) {
	case -1:
		hr->hp_er1 = 0;
		goto hper2;
		
	case HPER1_DCK: /* Corrected? data read. Just notice. */
		bc = mr->mba_bc;
		byte = ~(bc >> 16);
		diskerr(buf, hp_cd.cd_name, "soft ecc", LOG_PRINTF,
		    btodb(bp->b_bcount - byte), sc->sc_disk.dk_label);
		er1 &= ~(1<<HPER1_DCK);
		er1 &= HPMASK;
		break;

	default:
		printf("drive error :%s er1 %x er2 %x\n",
		    sc->sc_dev.dv_xname, er1, er2);
		hr->hp_er1 = hr->hp_er2 = 0;
		goto hper2;
	}
	goto hper1;

hper2:
	mbasr &= ~(MBASR_DTBUSY|MBASR_DTCMP|MBASR_ATTN);
	if (mbasr)
		printf("massbuss error :%s %x\n",
		    sc->sc_dev.dv_xname, mbasr);

	md->md_q.b_actf->b_resid = 0;
	disk_unbusy(&sc->sc_disk, md->md_q.b_actf->b_bcount);
	return XFER_FINISH;
}

/*
 * Non-data transfer interrupt; like volume change.
 */
int
hpattn(md)
	struct	mba_device *md;
{
	struct	hp_softc *sc = md->md_softc;
	struct	mba_softc *ms = (void *)sc->sc_dev.dv_parent;
	struct  mba_regs *mr = ms->sc_mbareg;
	struct  hp_regs *hr = (void *)&mr->mba_md[sc->sc_dev.dv_unit];
	int	er1, er2;

        er1 = hr->hp_er1 & HPMASK;
        er2 = hr->hp_er2 & HPMASK;

	printf("%s: Attention! er1 %x er2 %x\n",
		sc->sc_dev.dv_xname, er1, er2);
	return 0;
}


int
hpsize(dev)
	dev_t	dev;
{
	int	size, unit = DISKUNIT(dev);
	struct  hp_softc *sc;

	if (unit >= hp_cd.cd_ndevs || hp_cd.cd_devs[unit] == 0)
		return -1;

	sc = hp_cd.cd_devs[unit];
	size = sc->sc_disk.dk_label->d_partitions[DISKPART(dev)].p_size;

	return size;
}

int
hpdump(dev, a1, a2, size)
	dev_t	dev;
	caddr_t	a1, a2;
	size_t	size;
{
	printf("hpdump: Not implemented yet.\n");
	return 0;
}

int
hpread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	return (physio(hpstrategy, NULL, dev, B_READ, minphys, uio));
}

int
hpwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	return (physio(hpstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Convert physical adapternr and unit to the unit number used by kernel.
 */
int
hp_getdev(mbanr, unit, uname)
	int	mbanr, unit;
	char **uname;
{
	struct	mba_softc *ms;
	struct	hp_softc *sc;
	int i;

	for (i = 0; i < hp_cd.cd_ndevs; i++) {
		if (hp_cd.cd_devs[i] == 0)
			continue;

		sc = hp_cd.cd_devs[i];
		ms = (void *)sc->sc_dev.dv_parent;
		if (ms->sc_physnr == mbanr && sc->sc_physnr == unit) {
			*uname = sc->sc_dev.dv_xname;
			return i;
		}
	}
	return -1;
}
