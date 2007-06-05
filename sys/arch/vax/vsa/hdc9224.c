/*	$OpenBSD: hdc9224.c,v 1.15 2007/06/05 00:38:19 deraadt Exp $	*/
/*	$NetBSD: hdc9224.c,v 1.16 2001/07/26 15:05:09 wiz Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
 * with much help from (in alphabetical order):
 *	Jeremy
 *	Roger Ivie
 *	Rick Macklem
 *	Mike Young
 *
 * Rewritten by Ragge 25 Jun 2000. New features:
 *	- Uses interrupts instead of polling to signal ready.
 *	- Can cooperate with the SCSI routines WRT. the DMA area.
 *
 * TODO:
 *	- Floppy support missing.
 *	- Bad block forwarding missing.
 *	- Statistics collection.
 */
#undef	HDDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h> 
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/reboot.h>

#include <uvm/uvm_extern.h>

#include <ufs/ufs/dinode.h> /* For BBSIZE */
#include <ufs/ffs/fs.h>

#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/vsbus.h>
#include <machine/rpb.h>
#include <machine/scb.h>

#include <arch/vax/mscp/mscp.h> /* For DEC disk encoding */

#include <vax/vsa/hdc9224.h>

/* 
 * on-disk geometry block 
 */
struct hdgeom {
	char mbz[10];		/* 10 bytes of zero */
	long xbn_count;		/* number of XBNs */
	long dbn_count;		/* number of DBNs */
	long lbn_count;		/* number of LBNs (Logical-Block-Numbers) */
	long rbn_count;		/* number of RBNs (Replacement-Block-Numbers) */
	short nspt;		/* number of sectors per track */
	short ntracks;		/* number of tracks */
	short ncylinders;	/* number of cylinders */
	short precomp;		/* first cylinder for write precompensation */
	short reduced;		/* first cylinder for reduced write current */
	short seek_rate;	/* seek rate or zero for buffered seeks */
	short crc_eec;		/* 0 if CRC, 1 if ECC is being used */
	short rct;		/* "replacement control table" (RCT) */
	short rct_ncopies;	/* number of copies of the RCT */
	long	media_id;	/* media identifier */
	short interleave;	/* sector-to-sector interleave */
	short headskew;		/* head-to-head skew */
	short cylskew;		/* cylinder-to-cylinder skew */
	short gap0_size;	/* size of GAP 0 in the MFM format */
	short gap1_size;	/* size of GAP 1 in the MFM format */
	short gap2_size;	/* size of GAP 2 in the MFM format */
	short gap3_size;	/* size of GAP 3 in the MFM format */
	short sync_value;	/* sync value used when formatting */
	char	reserved[32];	/* reserved for use by the RQDX formatter */
	short serial_number;	/* serial number */
#if 0	/* we don't need these 412 useless bytes ... */
	char	fill[412-2];	/* Filler bytes to the end of the block */
	short checksum;	/* checksum over the XBN */
#endif
} __packed;

/*
 * Software status
 */
struct	hdsoftc {
	struct device sc_dev;		/* must be here! (pseudo-OOP:) */
	struct disk sc_disk;		/* disklabel etc. */
	struct hdgeom sc_xbn;		/* on-disk geometry information */
	int sc_drive;			/* physical unit number */
};

struct	hdcsoftc {
	struct device sc_dev;		/* must be here (pseudo-OOP:) */
	struct evcount sc_intrcnt;
	struct vsbus_dma sc_vd;
	vaddr_t sc_regs;		/* register addresses */
	struct buf sc_buf_queue;
	struct buf *sc_active;
	struct hdc9224_UDCreg sc_creg;	/* (command) registers to be written */
	struct hdc9224_UDCreg sc_sreg;	/* (status) registers being read */
	caddr_t	sc_dmabase;		/* */
	int	sc_dmasize;
	caddr_t sc_bufaddr;		/* Current in-core address */
	int sc_diskblk;			/* Current block on disk */
	int sc_bytecnt;			/* How much left to transfer */
	int sc_xfer;			/* Current transfer size */
	int sc_retries;
	volatile u_char sc_status;	/* last status from interrupt */
	char sc_intbit;
};

struct hdc_attach_args {
	int ha_drive;
};

/*
 * prototypes for (almost) all the internal routines
 */
int hdcmatch(struct device *, void *, void *);
void hdcattach(struct device *, struct device *, void *);
int hdcprint(void *, const char *);
int hdmatch(struct device *, void *, void *);
void hdattach(struct device *, struct device *, void *);
void hdcintr(void *);
int hdc_command(struct hdcsoftc *, int);
void hd_readgeom(struct hdcsoftc *, struct hdsoftc *);
#ifdef HDDEBUG
void hdc_printgeom( struct hdgeom *);
#endif
void hdc_writeregs(struct hdcsoftc *);
void hdcstart(struct hdcsoftc *, struct buf *);
int hdc_hdselect(struct hdcsoftc *, int);
void hdmakelabel(struct disklabel *, struct hdgeom *);
void hdc_writeregs(struct hdcsoftc *);
void hdc_readregs(struct hdcsoftc *);
void hdc_qstart(void *);
 
bdev_decl(hd);
cdev_decl(hd);

const struct	cfattach hdc_ca = {
	sizeof(struct hdcsoftc), hdcmatch, hdcattach
};

struct cfdriver hdc_cd = {
	NULL, "hdc", DV_DULL
};

const struct	cfattach hd_ca = {
	sizeof(struct hdsoftc), hdmatch, hdattach
};

struct cfdriver hd_cd = {
	NULL, "hd", DV_DISK
};

/* At least 0.7 uS between register accesses */
static int hd_dmasize, inq = 0;	/* XXX should be in softc... but only 1 ctrl */
static int u;
#define	WAIT	asm("movl _u,_u;movl _u,_u;movl _u,_u; movl _u,_u")

#define	HDC_WREG(x)	*(volatile char *)(sc->sc_regs) = (x)
#define	HDC_RREG	*(volatile char *)(sc->sc_regs)
#define	HDC_WCMD(x)	*(volatile char *)(sc->sc_regs + 4) = (x)
#define	HDC_RSTAT	*(volatile char *)(sc->sc_regs + 4)

/*
 * new-config's hdcmatch() is similar to old-config's hdcprobe(), 
 * thus we probe for the existence of the controller and reset it.
 * NB: we can't initialize the controller yet, since space for hdcsoftc 
 *     is not yet allocated. Thus we do this in hdcattach()...
 */
int
hdcmatch(struct device *parent, void *vcf, void *aux)
{
	static int matched = 0;
	struct vsbus_attach_args *va = aux;
	volatile char *hdc_csr = (char *)va->va_addr;
	int i;

	if (vax_boardtype == VAX_BTYP_49 || vax_boardtype == VAX_BTYP_46 ||
	    vax_boardtype == VAX_BTYP_48 || vax_boardtype == VAX_BTYP_1303)
		return (0);

	/* Can only match once due to DMA setup. This should not be an issue. */
	if (matched != 0)
		return (0);

	hdc_csr[4] = DKC_CMD_RESET; /* reset chip */
	for (i = 0; i < 1000; i++) {
		DELAY(1000);
		if (hdc_csr[4] & DKC_ST_DONE)
			break;
	}
	if (i == 100)
		return 0; /* No response to reset */

	hdc_csr[4] = DKC_CMD_SETREGPTR|UDC_TERM;
	WAIT;
	hdc_csr[0] = UDC_TC_CRCPRE|UDC_TC_INTDONE;
	WAIT;
	hdc_csr[4] = DKC_CMD_DRDESELECT; /* Should be harmless */
	DELAY(1000);
	return (matched = 1);
}

int
hdcprint(void *aux, const char *pnp)
{
	struct hdc_attach_args *ha = aux;

	if (pnp != NULL)
		printf("%s at %s drive %d",
		    ha->ha_drive == 2 ? "ry" : "hd", pnp, ha->ha_drive);

	return (UNCONF);
}

/*
 * hdc_attach() probes for all possible devices
 */
void 
hdcattach(struct device *parent, struct device *self, void *aux)
{
	struct vsbus_attach_args *va = aux;
	struct hdcsoftc *sc = (void *)self;
	struct hdc_attach_args ha;
	int status, i;

	u = 0; /* !!! - GCC */

	printf("\n");

	/*
	 * Get interrupt vector, enable instrumentation.
	 */
	scb_vecalloc(va->va_cvec, hdcintr, sc, SCB_ISTACK, &sc->sc_intrcnt);
	evcount_attach(&sc->sc_intrcnt, self->dv_xname, (void *)va->va_cvec,
	    &evcount_intr);

	sc->sc_regs = vax_map_physmem(va->va_paddr, 1);
	sc->sc_dmabase = (caddr_t)va->va_dmaaddr;
	sc->sc_dmasize = va->va_dmasize;
	sc->sc_intbit = va->va_maskno;
	hd_dmasize = min(MAXPHYS, sc->sc_dmasize); /* Used in hd_minphys */

	sc->sc_vd.vd_go = hdc_qstart;
	sc->sc_vd.vd_arg = sc;

	/*
	 * Reset controller.
	 */
	HDC_WCMD(DKC_CMD_RESET);
	DELAY(1000);
	status = HDC_RSTAT;
	if (status != (DKC_ST_DONE|DKC_TC_SUCCESS)) {
		printf("%s: RESET failed,  status 0x%x\n",
		    sc->sc_dev.dv_xname, status);
		return;
	}

	/*
	 * now probe for all possible hard drives
	 */
	for (i = 0; i < 4; i++) {
		if (i == 2) /* Floppy, needs special handling */
			continue;
		HDC_WCMD(DKC_CMD_DRSELECT | i);
		DELAY(1000);
		status = HDC_RSTAT;
		ha.ha_drive = i;
		if ((status & DKC_ST_TERMCOD) == DKC_TC_SUCCESS)
			config_found(self, (void *)&ha, hdcprint);
	}
}

/*
 * hdmatch() probes for the existence of a HD-type disk/floppy
 */
int
hdmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf;
	void *aux;
{
	struct hdc_attach_args *ha = aux;
	struct cfdata *cf = vcf;

	if (cf->cf_loc[0] != -1 &&
	    cf->cf_loc[0] != ha->ha_drive)
		return 0;

	if (ha->ha_drive == 2) /* Always floppy, not supported */
		return 0;

	return 1;
}

#define	HDMAJOR 19

void
hdattach(struct device *parent, struct device *self, void *aux)
{
	struct hdcsoftc *sc = (void*)parent;
	struct hdsoftc *hd = (void*)self;
	struct hdc_attach_args *ha = aux;
	struct disklabel *dl;
	char *msg;

	hd->sc_drive = ha->ha_drive;
	/*
	 * Initialize and attach the disk structure.
	 */
	hd->sc_disk.dk_name = hd->sc_dev.dv_xname;
	disk_attach(&hd->sc_disk);

	/*
	 * if it's not a floppy then evaluate the on-disk geometry.
	 * if necessary correct the label...
	 */
	hd_readgeom(sc, hd);
	disk_printtype(hd->sc_drive, hd->sc_xbn.media_id);
	dl = hd->sc_disk.dk_label;
	hdmakelabel(dl, &hd->sc_xbn);
	msg = readdisklabel(MAKEDISKDEV(HDMAJOR, hd->sc_dev.dv_unit, RAW_PART),
	    hdstrategy, dl, NULL, 0);
	printf("%s: %luMB, %lu sectors\n",
	    hd->sc_dev.dv_xname, DL_GETDSIZE(dl) / (1048576 / DEV_BSIZE),
	    DL_GETDSIZE(dl));
	if (msg) {
		/*printf("%s: %s\n", hd->sc_dev.dv_xname, msg);*/
	}
#ifdef HDDEBUG
	hdc_printgeom(&hd->sc_xbn);
#endif
}

void
hdcintr(void *arg)
{
	struct hdcsoftc *sc = arg;
	struct buf *bp;

	sc->sc_status = HDC_RSTAT;
	if (sc->sc_active == 0)
		return; /* Complain? */

	if ((sc->sc_status & (DKC_ST_INTPEND | DKC_ST_DONE)) !=
	    (DKC_ST_INTPEND | DKC_ST_DONE))
		return; /* Why spurious ints sometimes??? */

	bp = sc->sc_active;
	sc->sc_active = 0;
	if ((sc->sc_status & DKC_ST_TERMCOD) != DKC_TC_SUCCESS) {
		int i;
		u_char *g = (u_char *)&sc->sc_sreg;

		if (sc->sc_retries++ < 3) { /* Allow 3 retries */
			hdcstart(sc, bp);
			return;
		}
		printf("%s: failed, status 0x%x\n",
		    sc->sc_dev.dv_xname, sc->sc_status);
		hdc_readregs(sc);
		for (i = 0; i < 10; i++)
			printf("%i: %x\n", i, g[i]);
		bp->b_flags |= B_ERROR;
		bp->b_error = ENXIO;
		bp->b_resid = bp->b_bcount;
		biodone(bp);
		vsbus_dma_intr();
		return;
	}

	if (bp->b_flags & B_READ) {
		vsbus_copytoproc(bp->b_proc, sc->sc_dmabase, sc->sc_bufaddr,
		    sc->sc_xfer);
	}
	sc->sc_diskblk += (sc->sc_xfer/DEV_BSIZE);
	sc->sc_bytecnt -= sc->sc_xfer;
	sc->sc_bufaddr += sc->sc_xfer;

	if (sc->sc_bytecnt == 0) { /* Finished transfer */
		biodone(bp);
		vsbus_dma_intr();
	} else
		hdcstart(sc, bp);
}

/*
 *
 */
void
hdstrategy(struct buf *bp)
{
	struct hdsoftc *hd;
	struct hdcsoftc *sc;
	struct disklabel *lp;
	int unit, s;
	daddr_t bn;

	unit = DISKUNIT(bp->b_dev);
	if (unit > hd_cd.cd_ndevs || (hd = hd_cd.cd_devs[unit]) == NULL) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}
	sc = (void *)hd->sc_dev.dv_parent;

	lp = hd->sc_disk.dk_label;
	if ((bounds_check_with_label(bp, hd->sc_disk.dk_label,
	    hd->sc_disk.dk_cpulabel, 1)) <= 0)
		goto done;

	if (bp->b_bcount == 0)
		goto done;

	/*
	 * XXX Since we need to know blkno in hdcstart() and do not have a
	 * b_rawblkno field in struct buf (yet), abuse b_cylinder to store
	 * the block number instead of the cylinder number.
	 * This will be suboptimal in disksort(), but not harmful. Of course,
	 * this also truncates the block number at 4G, but there shouldn't be
	 * any MFM disk that large.
	 */
	bn = bp->b_blkno + DL_GETPOFFSET(&lp->d_partitions[DISKPART(bp->b_dev)]);
	bp->b_cylinder = bn;

	s = splbio();
	disksort(&sc->sc_buf_queue, bp);
	if (inq == 0) {
		inq = 1;
		vsbus_dma_start(&sc->sc_vd);
	}
	splx(s);
	return;

done:
	s = splbio();
	biodone(bp);
	splx(s);
}

void
hdc_qstart(void *arg)
{
	struct hdcsoftc *sc = arg;
	struct buf *dp;

	inq = 0;

	hdcstart(sc, NULL);
	dp = &sc->sc_buf_queue;
	if (dp->b_actf != NULL) {
		vsbus_dma_start(&sc->sc_vd); /* More to go */
		inq = 1;
	}
}

void
hdcstart(struct hdcsoftc *sc, struct buf *ob)
{
	struct hdc9224_UDCreg *p = &sc->sc_creg;
	struct disklabel *lp;
	struct hdsoftc *hd;
	struct buf *dp, *bp;
	int cn, sn, tn, bn, blks;
	volatile char ch;

	splassert(IPL_BIO);

	if (sc->sc_active)
		return; /* Already doing something */

	if (ob == 0) {
		dp = &sc->sc_buf_queue;
		if ((bp = dp->b_actf) == NULL)
			return; /* Nothing to do */
		dp->b_actf = bp->b_actf;
		sc->sc_bufaddr = bp->b_data;
		/* XXX see hdstrategy() comments regarding b_cylinder usage */
		sc->sc_diskblk = bp->b_cylinder;
		sc->sc_bytecnt = bp->b_bcount;
		sc->sc_retries = 0;
		bp->b_resid = 0;
	} else
		bp = ob;

	hd = hd_cd.cd_devs[DISKUNIT(bp->b_dev)];
	hdc_hdselect(sc, hd->sc_drive);
	sc->sc_active = bp;

	bn = sc->sc_diskblk;
	lp = hd->sc_disk.dk_label;
        if (bn) {
                cn = bn / lp->d_secpercyl;
                sn = bn % lp->d_secpercyl;
                tn = sn / lp->d_nsectors;
                sn = sn % lp->d_nsectors;
        } else
                cn = sn = tn = 0;

	cn++; /* first cylinder is reserved */

	bzero(p, sizeof(struct hdc9224_UDCreg));

	/*
	 * Tricky thing: the controller itself only increases the sector
	 * number, not the track or cylinder number. Therefore the driver
	 * is not allowed to have transfers that crosses track boundaries.
	 */
	blks = sc->sc_bytecnt / DEV_BSIZE;
	if ((sn + blks) > lp->d_nsectors)
		blks = lp->d_nsectors - sn;

	p->udc_dsect = sn;
	p->udc_dcyl = cn & 0xff;
	p->udc_dhead = ((cn >> 4) & 0x70) | tn;
	p->udc_scnt = blks;

	p->udc_rtcnt = UDC_RC_RTRYCNT;
	p->udc_mode = UDC_MD_HDD;
	p->udc_term = UDC_TC_CRCPRE|UDC_TC_INTDONE|UDC_TC_TDELDAT|UDC_TC_TWRFLT;
	hdc_writeregs(sc);

	/* Count up vars */
	sc->sc_xfer = blks * DEV_BSIZE;

	ch = HDC_RSTAT; /* Avoid pending interrupts */
	WAIT;
	vsbus_clrintr(sc->sc_intbit); /* Clear pending int's */

	if (bp->b_flags & B_READ) {
		HDC_WCMD(DKC_CMD_READ_HDD);
	} else {
		vsbus_copyfromproc(bp->b_proc, sc->sc_bufaddr, sc->sc_dmabase,
		    sc->sc_xfer);
		HDC_WCMD(DKC_CMD_WRITE_HDD);
	}
}

void
hd_readgeom(struct hdcsoftc *sc, struct hdsoftc *hd)
{
	struct hdc9224_UDCreg *p = &sc->sc_creg;

	hdc_hdselect(sc, hd->sc_drive);		/* select drive right now */

	bzero(p, sizeof(struct hdc9224_UDCreg));

	p->udc_scnt  = 1;
	p->udc_rtcnt = UDC_RC_RTRYCNT;
	p->udc_mode  = UDC_MD_HDD;
	p->udc_term  = UDC_TC_CRCPRE|UDC_TC_INTDONE|UDC_TC_TDELDAT|UDC_TC_TWPROT;
	hdc_writeregs(sc);
	sc->sc_status = 0;
	HDC_WCMD(DKC_CMD_READ_HDD|2);
	while ((sc->sc_status & DKC_ST_INTPEND) == 0)
		;
	bcopy(sc->sc_dmabase, &hd->sc_xbn, sizeof(struct hdgeom));
}

#ifdef HDDEBUG
/*
 * display the contents of the on-disk geometry structure
 */
void
hdc_printgeom(p)
	struct hdgeom *p;
{
	printf("**DiskData**	 XBNs: %ld, DBNs: %ld, LBNs: %ld, RBNs: %ld\n",
	    p->xbn_count, p->dbn_count, p->lbn_count, p->rbn_count);
	printf("sec/track: %d, tracks: %d, cyl: %d, precomp/reduced: %d/%d\n",
	    p->nspt, p->ntracks, p->ncylinders, p->precomp, p->reduced);
	printf("seek-rate: %d, crc/eec: %s, RCT: %d, RCT-copies: %d\n",
	    p->seek_rate, p->crc_eec?"EEC":"CRC", p->rct, p->rct_ncopies);
	printf("media-ID: %lx, interleave: %d, headskew: %d, cylskew: %d\n",
	    p->media_id, p->interleave, p->headskew, p->cylskew);
	printf("gap0: %d, gap1: %d, gap2: %d, gap3: %d, sync-value: %d\n",
	    p->gap0_size, p->gap1_size, p->gap2_size, p->gap3_size, 
	    p->sync_value);
}
#endif

/*
 * Return the size of a partition, if known, or -1 if not.
 */
int
hdsize(dev_t dev)
{
	struct hdsoftc *hd;
	int unit = DISKUNIT(dev);
	int size;

	if (unit >= hd_cd.cd_ndevs || hd_cd.cd_devs[unit] == 0)
		return -1;
	hd = hd_cd.cd_devs[unit];
	size = DL_GETPSIZE(&hd->sc_disk.dk_label->d_partitions[DISKPART(dev)]) *
	    (hd->sc_disk.dk_label->d_secsize / DEV_BSIZE);

	return (size);
}

/*
 *
 */
int
hdopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct hdsoftc *hd;
	int unit, part;

	unit = DISKUNIT(dev);
	if (unit >= hd_cd.cd_ndevs)
		return ENXIO;
	hd = hd_cd.cd_devs[unit];
	if (hd == 0)
		return ENXIO;

	part = DISKPART(dev);
	if (part >= hd->sc_disk.dk_label->d_npartitions)
		return ENXIO;

	switch (fmt) {
	case S_IFCHR:
		hd->sc_disk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		hd->sc_disk.dk_bopenmask |= (1 << part);
		break;
	}
	hd->sc_disk.dk_openmask =
	    hd->sc_disk.dk_copenmask | hd->sc_disk.dk_bopenmask;

	return 0;
}

/*
 *
 */
int
hdclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct hdsoftc *hd;
	int part;

	hd = hd_cd.cd_devs[DISKUNIT(dev)];
	part = DISKPART(dev);

	switch (fmt) {
	case S_IFCHR:
		hd->sc_disk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		hd->sc_disk.dk_bopenmask &= ~(1 << part);
		break;
	}
	hd->sc_disk.dk_openmask =
	    hd->sc_disk.dk_copenmask | hd->sc_disk.dk_bopenmask;

	return (0);
}

/*
 *
 */
int
hdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct hdsoftc *hd = hd_cd.cd_devs[DISKUNIT(dev)];
	struct disklabel *lp = hd->sc_disk.dk_label;
	int err = 0;

	switch (cmd) {
	case DIOCGDINFO:
		bcopy(lp, addr, sizeof (struct disklabel));
		break;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = lp;
		((struct partinfo *)addr)->part =
		  &lp->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;
		else
			err = (cmd == DIOCSDINFO ?
			    setdisklabel(lp, (struct disklabel *)addr, 0, 0) :
			    writedisklabel(dev, hdstrategy, lp, 0));
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			err = EBADF;
		break;

	default:
		err = ENOTTY;
		break;
	}
	return err;
}

/*
 * 
 */
int
hdread(dev_t dev, struct uio *uio, int flag)
{
	return (physio(hdstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 *
 */
int
hdwrite(dev_t dev, struct uio *uio, int flag)
{
	return (physio(hdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 *
 */
int
hddump(dev_t dev, daddr_t daddr, caddr_t addr, size_t size)
{
	return 0;
}

/*
 * we have to wait 0.7 usec between two accesses to any of the
 * dkc-registers, on a VS2000 with 1 MIPS, this is roughly one
 * instruction. Thus the loop-overhead will be enough...
 */
void
hdc_readregs(struct hdcsoftc *sc)
{
	int i;
	char *p;

	HDC_WCMD(DKC_CMD_SETREGPTR);
	WAIT;
	p = (void *)&sc->sc_sreg;	
	for (i = 0; i < 10; i++) {
		*p++ = HDC_RREG;	/* dkc_reg auto-increments */
		WAIT;
	}
}

void
hdc_writeregs(struct hdcsoftc *sc)
{
	int i;
	char *p;

	HDC_WCMD(DKC_CMD_SETREGPTR);
	p = (void *)&sc->sc_creg;	
	for (i = 0; i < 10; i++) {
		HDC_WREG(*p++);	/* dkc_reg auto-increments */
		WAIT;
	}
}

/*
 * hdc_command() issues a command and polls the intreq-register
 * to find when command has completed
 */
int
hdc_command(struct hdcsoftc *sc, int cmd)
{
	hdc_writeregs(sc);		/* write the prepared registers */
	HDC_WCMD(cmd);
	WAIT;
	return (0);
}

int
hdc_hdselect(struct hdcsoftc *sc, int unit)
{
	struct hdc9224_UDCreg *p = &sc->sc_creg;
	int error;

	/*
	 * bring "creg" in some known-to-work state and
	 * select the drive with the DRIVE SELECT command.
	 */
	bzero(p, sizeof(struct hdc9224_UDCreg));

	p->udc_rtcnt = UDC_RC_HDD_READ;
	p->udc_mode  = UDC_MD_HDD;
	p->udc_term  = UDC_TC_HDD;

	error = hdc_command(sc, DKC_CMD_DRSEL_HDD | unit);

	return (error);
}

void
hdmakelabel(struct disklabel *dl, struct hdgeom *g)
{
	int n, p = 0;

	dl->d_bbsize = BBSIZE;
	dl->d_sbsize = SBSIZE;
	dl->d_typename[p++] = MSCP_MID_CHAR(2, g->media_id);
	dl->d_typename[p++] = MSCP_MID_CHAR(1, g->media_id);
	if (MSCP_MID_ECH(0, g->media_id))
		dl->d_typename[p++] = MSCP_MID_CHAR(0, g->media_id);
	n = MSCP_MID_NUM(g->media_id);
	if (n > 99) {
		dl->d_typename[p++] = '1';
		n -= 100;
	}
	if (n > 9) {
		dl->d_typename[p++] = (n / 10) + '0';
		n %= 10;
	}
	dl->d_typename[p++] = n + '0';
	dl->d_typename[p] = 0;
	dl->d_type = DTYPE_MSCP; /* XXX - what to use here??? */
	dl->d_rpm = 3600;
	dl->d_secsize = DEV_BSIZE;

	DL_SETDSIZE(dl, g->lbn_count);
	dl->d_nsectors = g->nspt;
	dl->d_ntracks = g->ntracks;
	dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
	dl->d_ncylinders = DL_GETDSIZE(dl) / dl->d_secpercyl;

	dl->d_npartitions = MAXPARTITIONS;
	DL_SETPSIZE(&dl->d_partitions[0], DL_GETDSIZE(dl));
	DL_SETPSIZE(&dl->d_partitions[2], DL_GETDSIZE(dl));
	    
	DL_SETPOFFSET(&dl->d_partitions[0], 0);
	DL_SETPOFFSET(&dl->d_partitions[2], 0);
	dl->d_interleave = dl->d_headswitch = 1;
	dl->d_magic = dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);
}
