/*	$OpenBSD: hdc9224.c,v 1.12 2005/02/17 23:49:25 miod Exp $ */
/*	$NetBSD: hdc9224.c,v 1.6 1997/03/15 16:32:22 ragge Exp $ */
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
 */

/* #define DEBUG	/* */
/* #define TRACE	/* */
static int haveLock = 0;
static int keepLock = 0;

#define F_READ	11
#define F_WRITE 12

#define trace(x)
#define debug(x)

#include "hdc.h"
#if NHDC > 0

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
#include <sys/dkstat.h> 
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/reboot.h>

#include <machine/pte.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/uvax.h>
#include <machine/ka410.h>
#include <machine/vsbus.h>
#include <machine/rpb.h>

#include <vax/vsa/hdc9224.h>


/*
 * some definitions 
 */
#define CTLRNAME  "hdc"
#define UNITNAME  "hd"
#define HDC_PRI	  LOG_INFO

/* Bits in minor device */ 
#define HDCUNIT(dev)	DISKUNIT(dev)
#define HDCPART(dev)	DISKPART(dev)
#define HDCCTLR(dev)	0
#define HDCLABELDEV(dev)	(MAKEDISKDEV(major(dev),HDCUNIT(dev),RAW_PART))

#define MAX_WAIT	(1000*1000)	/* # of loop-instructions in seconds */


/* 
 * on-disk geometry block 
 */
#define _aP	__attribute__ ((packed))	/* force byte-alignment */
struct hdgeom {
  char mbz[10];		/* 10 bytes of zero */
  long xbn_count _aP;	/* number of XBNs */
  long dbn_count _aP;	/* number of DBNs */
  long lbn_count _aP;	/* number of LBNs (Logical-Block-Numbers) */
  long rbn_count _aP;	/* number of RBNs (Replacement-Block-Numbers) */
  short nspt;		/* number of sectors per track */
  short ntracks;	/* number of tracks */
  short ncylinders;	/* number of cylinders */
  short precomp;	/* first cylinder for write precompensation */
  short reduced;	/* first cylinder for reduced write current */
  short seek_rate;	/* seek rate or zero for buffered seeks */
  short crc_eec;	/* 0 if CRC is being used or 1 if ECC is being used */
  short rct;		/* "replacement control table" (RCT) */
  short rct_ncopies;	/* number of copies of the RCT */
  long	media_id _aP;	/* media identifier */
  short interleave;	/* sector-to-sector interleave */
  short headskew;	/* head-to-head skew */
  short cylskew;	/* cylinder-to-cylinder skew */
  short gap0_size;	/* size of GAP 0 in the MFM format */
  short gap1_size;	/* size of GAP 1 in the MFM format */
  short gap2_size;	/* size of GAP 2 in the MFM format */
  short gap3_size;	/* size of GAP 3 in the MFM format */
  short sync_value;	/* sync value used to start a track when formatting */
  char	reserved[32];	/* reserved for use by the RQDX1/2/3 formatter */
  short serial_number;	/* serial number */
#if 0	/* we don't need these 412 useless bytes ... */
  char	fill[412-2];	/* Filler bytes to the end of the block */
  short checksum;	/* checksum over the XBN */
#endif
};

/*
 * Software status
 */
struct	hdsoftc {
	struct device	sc_dev;		/* must be here! (pseudo-OOP:) */
	struct disk	sc_dk;		/* disklabel etc. */
	struct hdgeom	sc_xbn;		/* on-disk geometry information */
	struct hdparams {
		u_short cylinders;	/* number of cylinders */
		u_char	heads;		/* number of heads (tracks) */
		u_char	sectors;	/* number of sectors/track */
		u_long	diskblks;	/* number of sectors/disk */
		u_long	disklbns;	/* number of available sectors */
		u_long	blksize;	/* number of bytes/sector */
		u_long	diskbytes;	/* number of bytes/disk */
		char	diskname[8];
	} sc_param;
	int	sc_drive;		/* physical unit number */
	int	sc_flags;
	int	sc_state;
	int	sc_mode;
};

struct	hdcsoftc {
	struct device sc_dev;		/* must be here (pseudo-OOP:) */
	struct hdc9224_DKCreg *sc_dkc;	/* I/O address of the controller */
	struct hdc9224_UDCreg sc_creg;	/* (command) registers to be written */
	struct hdc9224_UDCreg sc_sreg;	/* (status) registers being read */
	struct confargs *sc_cfargs;	/* remember args being probed with */
	char	*sc_dmabase;		/* */
	long	sc_dmasize;		/* */
	long	sc_ioaddr;		/* unmapped I/O address */
	long	sc_ivec;		/* interrupt vector address */
	short	sc_ibit;		/* bit-value in interrupt register */
	short	sc_status;		/* copy of status register */
	short	sc_state;
	short	sc_flags;
	short	sc_errors;
};

/*
 * Device definition for (new) autoconfiguration.
 */
int	hdcmatch(struct device *parent, void *cfdata, void *aux);
void	hdcattach(struct device *parent, struct device *self, void *aux);
int	hdcprint(void *aux, const char *name);

struct	cfdriver hdc_cd = {
	NULL, "hdc", DV_DULL
};
struct	cfattach hdc_ca = {
	sizeof(struct hdcsoftc), hdcmatch, hdcattach
};

int	hdmatch(struct device *parent, void *cfdata, void *aux);
void	hdattach(struct device *parent, struct device *self, void *aux);
int	hdprint(void *aux, const char *name);
void	hdstrategy(struct buf *bp);

struct	cfdriver hd_cd = {
	NULL, "hd", DV_DISK
};
struct	cfattach hd_ca = {
	sizeof(struct hdsoftc), hdmatch, hdattach
};

struct dkdriver hddkdriver = { hdstrategy };

/*
 * prototypes for (almost) all the internal routines
 */
int hdc_reset(struct hdcsoftc *sc);
int hdc_select(struct hdcsoftc *sc, int drive);
int hdc_command(struct hdcsoftc *sc, int cmd);

int hdc_getdata(struct hdcsoftc *hdc, struct hdsoftc *hd, int drive);
int hdc_getlabel(struct hdcsoftc *hdc, struct hdsoftc *hd, int drive);

void hdgetlabel(struct hdsoftc *sc);

/*
 * new-config's hdcmatch() is similar to old-config's hdcprobe(), 
 * thus we probe for the existence of the controller and reset it.
 * NB: we can't initialize the controller yet, since space for hdcsoftc 
 *     is not yet allocated. Thus we do this in hdcattach()...
 */
int
hdcmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct confargs *ca = aux;

	trace(("hdcmatch(0x%x, %d, %s)\n", parent, cf->cf_unit, ca->ca_name));

	if (strcmp(ca->ca_name, "hdc") &&
	    strcmp(ca->ca_name, "hdc9224") &&
	    strcmp(ca->ca_name, "HDC9224"))
		return (0);

	/*
	 * only(?) VS2000/KA410 has exactly one HDC9224 controller
	 */
	if (vax_boardtype != VAX_BTYP_410) {
		printf("unexpected boardtype 0x%x in hdcmatch()\n", 
		    vax_boardtype);
		return (0);
	}
	if (cf->cf_unit != 0)
		return (0);

	return (1);
}

struct hdc_attach_args {
	int ha_drive;
};

int
hdprint(aux, name)
	void *aux;
	const char *name;
{
	struct hdc_attach_args *ha = aux;

	trace(("hdprint(%d, %s)\n", ha->ha_drive, name));

	if (!name)
		printf(" drive %d", ha->ha_drive);
	return (QUIET);
}

/*
 * hdc_attach() probes for all possible devices
 */
void 
hdcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hdcsoftc *sc = (void *)self;
	struct confargs *ca = aux;
	struct hdc_attach_args ha;

	trace(("hdcattach(0x%x, 0x%x, %s)\n", parent, self, ca->ca_name));

	printf("\n");
	/*
	 * first reset/initialize the controller
	 */
	sc->sc_cfargs = ca;

	sc->sc_ioaddr = ca->ca_ioaddr;
	sc->sc_dkc = (void *)uvax_phys2virt(sc->sc_ioaddr);
	sc->sc_ibit = ca->ca_intbit;
	sc->sc_ivec = ca->ca_intvec;
	sc->sc_status = 0;
	sc->sc_state = 0;
	sc->sc_flags = 0;
	sc->sc_errors = 0;

	sc->sc_dkc     = (void *)uvax_phys2virt(KA410_DKC_BASE);
	sc->sc_dmabase = (void *)uvax_phys2virt(KA410_DMA_BASE);
	sc->sc_dmasize = KA410_DMA_SIZE;
					      
	if (hdc_reset(sc) != 0) {
		delay(500*1000);	/* wait .5 seconds */
		if (hdc_reset(sc) != 0)
			printf("problems with hdc_reset()...\n");
	}

	/*
	 * now probe for all possible disks
	 */
	for (ha.ha_drive=0; ha.ha_drive<3; ha.ha_drive++)
		(void)config_found(self, (void *)&ha, hdprint);

#ifdef notyet
	/*
	 * now that probing is done, we can register and enable interrupts
	 */
	vsbus_intr_register(XXX);
	vsbus_intr_enable(XXX);
#endif
}

/*
 * hdmatch() probes for the existence of a RD-type disk/floppy
 */
int
hdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct hdcsoftc *hdc = (void *)parent;
	struct cfdata *cf = match;
	struct hdc_attach_args *ha = aux;
	int drive = ha->ha_drive;
	int res;

	trace(("hdmatch(%d, %d)\n", cf->cf_unit, drive));

	if (cf->cf_unit != ha->ha_drive)
		return (0);

	switch (drive) {
	case 0:
	case 1:
	case 2:
		res = hdc_select(hdc, drive);
		break;
	default:
		printf("hdmatch: invalid unit-number %d\n", drive);
		return (0);
	}

	debug (("cstat: %x dstat: %x\n", hdc->sc_sreg.udc_cstat,
		hdc->sc_sreg.udc_dstat));
	if (drive == 1) 
	  return (0);	/* XXX */

	return (1);
}

void
hdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct hdcsoftc *hdc = (void *)parent;
	struct hdsoftc *hd = (void *)self;
	struct hdc_attach_args *ha = aux;
	struct hdparams *hp = &hd->sc_param;

	trace(("hdattach(%d)\n", ha->ha_drive));

	hd->sc_drive = ha->ha_drive;
	/*
	 * Initialize and attach the disk structure.
	 */
	hd->sc_dk.dk_driver = &hddkdriver;
	hd->sc_dk.dk_name = hd->sc_dev.dv_xname;
	disk_attach(&hd->sc_dk);
	/*
	 * if it's not a floppy then evaluate the on-disk geometry.
	 * if necessary correct the label...
	 */
	printf("\n%s: ", hd->sc_dev.dv_xname);
	if (hd->sc_drive == 2) {
		printf("floppy (RX33)\n");
	}
	else {
		hdc_getdata(hdc, hd, hd->sc_drive);
		printf("%s, %d MB, %d LBN, %d cyl, %d head, %d sect/track\n",
		    hp->diskname, hp->diskblks / 2048, hp->disklbns, 
		    hp->cylinders, hp->heads, hp->sectors);
	}
	/*
	 * Know where we booted from.
	 */
	if ((B_TYPE(bootdev) == BDEV_RD) && (hd->sc_drive == B_UNIT(bootdev)))
		booted_from = self;
}

/*
 * Read/write routine for a buffer.  For now we poll the controller, 
 * thus this routine waits for the transfer to complete.
 */
void
hdstrategy(bp)
	struct buf *bp;
{
	struct hdsoftc *hd = hd_cd.cd_devs[HDCUNIT(bp->b_dev)];
	struct hdcsoftc *hdc = (void *)hd->sc_dev.dv_parent;
	struct partition *p;
	int blkno, i, s;

	trace (("hdstrategy(#%d/%d)\n", bp->b_blkno, bp->b_bcount));

	/* XXX		should make some checks... */

	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;
	
	/*
	 * what follows now should not be here but in hdstart...
	 */
	/*------------------------------*/
	blkno = bp->b_blkno / (hd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	p = &hd->sc_dk.dk_label->d_partitions[HDCPART(bp->b_dev)];
	blkno += p->p_offset;

	/* nblks = howmany(bp->b_bcount, sd->sc_dk.dk_label->d_secsize); */

	if (hdc_strategy(hdc, hd, HDCUNIT(bp->b_dev), 
			 ((bp->b_flags & B_READ) ? F_READ : F_WRITE),
			 blkno, bp->b_bcount, bp->b_data) == 0)
		goto done;
	/*------------------------------*/
bad:
	bp->b_flags |= B_ERROR;
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = 0;	/* ??? bertram */
	s = splbio();
	biodone(bp);
	splx(s);
}

int
hdc_strategy(hdc, hd, unit, func, dblk, size, buf)
	struct hdcsoftc *hdc;
	struct hdsoftc *hd;
	int unit;
	int func;
	int dblk;
	int size;
	char *buf;
{
	struct hdc9224_UDCreg *p = &hdc->sc_creg;
	struct disklabel *lp = hd->sc_dk.dk_label;
	int sect, head, cyl;
	int scount;
	int cmd, res = 0;

	trace (("hdc_strategy(%d, %d, %d, %d, 0x%x)\n",
		unit, func, dblk, size, buf));

	hdc_select(hdc, unit);		/* select drive right now */

	if (unit != 2 && dblk == -1) {	/* read the on-disk geometry */

	  p->udc_dma7  = 0;
	  p->udc_dma15 = 0;
	  p->udc_dma23 = 0;

	  p->udc_dsect = 0;
	  p->udc_dhead = 0;
	  p->udc_dcyl  = 0;

	  p->udc_scnt  = size/512; 
	  p->udc_rtcnt = 0xF0;
	  p->udc_mode  = 0xC0;
	  p->udc_term  = 0xB4;

	  vsbus_lockDMA(hdc->sc_cfargs);		/* bertram XXX */
	  haveLock = 1;
	  keepLock = 1;

#ifdef PARANOID
	  bzero (hdc->sc_dmabase, size);	/* clear disk buffer */
#endif
	  cmd = 0x5C | 0x03;			/* bypass bad sectors */
	  cmd = 0x5C | 0x01;			/* terminate if bad sector */

	  res = hdc_command (hdc, cmd);
	  /* hold the locking ! */
	  bcopy (hdc->sc_dmabase, buf, size);	/* copy to buf */
	  /* now release the locking */

	  vsbus_unlockDMA(hdc->sc_cfargs);
	  haveLock = 0;
	  keepLock = 0;

	  return (res);
	}
	
	scount = size / 512;
	while (scount) {
	  /*
	   * prepare drive/operation parameter
	   */
	  cyl  = dblk / lp->d_secpercyl;
	  sect = dblk % lp->d_secpercyl;
	  head = sect / lp->d_nsectors;
	  sect = sect % lp->d_nsectors;
	  if (unit == 2) 
		sect++;
	  else
		cyl++;		/* first cylinder is reserved */

	  size = 512 * min(scount, lp->d_nsectors - sect);

	  debug (("hdc_strategy: block #%d ==> s/t/c=%d/%d/%d (%d/%d)\n",
		  dblk, sect, head, cyl, scount, size));

	  /*
	   * now initialize the register values ...
	   */
	  p->udc_dma7  = 0;
	  p->udc_dma15 = 0;
	  p->udc_dma23 = 0;

	  p->udc_dsect = sect;
	  head |= (cyl >> 4) & 0x70;
	  p->udc_dhead = head;
	  p->udc_dcyl  = cyl;

	  p->udc_scnt  = size/512; 

	  if (unit == 2) {	/* floppy */
	    p->udc_rtcnt = 0xF2;
	    p->udc_mode	 = 0x81;	/* RX33 with RX50 media */
	    p->udc_mode	 = 0x82;	/* RX33 with RX33 media */
	    p->udc_term	 = 0xB4;	
	  } else {		 /* disk */
	    p->udc_rtcnt = 0xF0;
	    p->udc_mode	 = 0xC0;
	    p->udc_term	 = 0xB4;
	  }
	  
	  vsbus_lockDMA(hdc->sc_cfargs);
	  haveLock = 1;
	  keepLock = 1;

	  if (func == F_WRITE) {
	    bcopy (buf, hdc->sc_dmabase, size); /* copy from buf */
	    cmd = 0xA0 | (unit==2 ? 1 : 0);
	    res = hdc_command (hdc, cmd);
	  }
	  else {
#ifdef PARANOID
	    bzero (hdc->sc_dmabase, size);		/* clear disk buffer */
#endif
	    cmd = 0x5C | 0x03;	/* bypass bad sectors */
	    cmd = 0x5C | 0x01;	/* terminate if bad sector */
	    res = hdc_command (hdc, cmd);
	    bcopy (hdc->sc_dmabase, buf, size); /* copy to buf */
	  }

	  vsbus_unlockDMA(hdc->sc_cfargs);
	  haveLock = 0;
	  keepLock = 0;

	  scount -= size/512;
	  dblk += size/512;
	  buf += size;
	}

	if (unit != 2)		/* deselect drive, if not floppy */
	  hdc_command (hdc, DKC_CMD_DRDESELECT);

	return 0;
}

char hdc_iobuf[17*512];		/* we won't need more */

#ifdef DEBUG
/*
 * display the contents of the on-disk geometry structure
 */
int
hdc_printgeom(p)
	struct hdgeom *p;
{
	char dname[8];
	hdc_mid2str(p->media_id, dname, sizeof dname);

	printf("**DiskData**	 XBNs: %d, DBNs: %d, LBNs: %d, RBNs: %d\n",
	    p->xbn_count, p->dbn_count, p->lbn_count, p->rbn_count);
	printf("sec/track: %d, tracks: %d, cyl: %d, precomp/reduced: %d/%d\n",
	    p->nspt, p->ntracks, p->ncylinders, p->precomp, p->reduced);
	printf("seek-rate: %d, crc/eec: %s, RCT: %d, RCT-copies: %d\n",
	    p->seek_rate, p->crc_eec?"EEC":"CRC", p->rct, p->rct_ncopies);
	printf("media-ID: %s, interleave: %d, headskew: %d, cylskew: %d\n",
	    dname, p->interleave, p->headskew, p->cylskew);
	printf("gap0: %d, gap1: %d, gap2: %d, gap3: %d, sync-value: %d\n",
	    p->gap0_size, p->gap1_size, p->gap2_size, p->gap3_size, 
	    p->sync_value);
}
#endif

/*
 * Convert media_id to string/name (encoding is documented in mscp.h)
 */
int
hdc_mid2str(media_id, name)
	long media_id;
	char *name;
	size_t len;
{
	struct {			/* For RD32 this struct holds: */
		u_long mt:7;		/* number in name: 0x20 == 32 */
		u_long a2:5;		/* ' ' encoded as 0x0 */
		u_long a1:5;		/* 'D' encoded with base '@' */
		u_long a0:5;		/* 'R' encoded with base '@' */
		u_long d1:5;		/* 'U' encoded with base '@' */
		u_long d0:5;		/* 'D' encoded with base '@' */
	} *p = (void *)&media_id;

#define MIDCHR(x)	(x ? x + '@' : ' ')

	snprintf(name, len, "%c%c%d", MIDCHR(p->a0), MIDCHR(p->a1), p->mt);
}

int
hdc_getdata(hdc, hd, unit)
	struct hdcsoftc *hdc;
	struct hdsoftc *hd;
	int unit;
{
	struct disklabel *lp = hd->sc_dk.dk_label;
	struct hdparams *hp = &hd->sc_param;
	int res;

	trace (("hdc_getdata(%d)\n", unit));

	bzero(hd->sc_dk.dk_label, sizeof(struct disklabel));
	bzero(hd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	if (unit == 2) {
		lp->d_secsize = DEV_BSIZE;
		lp->d_ntracks = 2;
		lp->d_nsectors = 15;
		lp->d_ncylinders = 80;
		lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

		return (0);
	}

	res = hdc_strategy(hdc, hd, unit, F_READ, -1, 4096, hdc_iobuf);
	bcopy (hdc_iobuf, &hd->sc_xbn, sizeof(struct hdgeom));
#ifdef DEBUG
	hdc_printgeom(&hd->sc_xbn);
#endif
	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = hd->sc_xbn.ntracks;
	lp->d_nsectors = hd->sc_xbn.nspt;
	lp->d_ncylinders = hd->sc_xbn.ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	hp->cylinders = hd->sc_xbn.ncylinders;
	hp->heads = hd->sc_xbn.ntracks;
	hp->sectors = hd->sc_xbn.nspt;
	hp->diskblks = hp->cylinders * hp->heads * hp->sectors;
	hp->disklbns = hd->sc_xbn.lbn_count;
	hp->blksize = DEV_BSIZE;
	hp->diskbytes = hp->disklbns * hp->blksize;
	hdc_mid2str(hd->sc_xbn.media_id, hp->diskname, sizeof hp->diskname);

	return (0);
}

int
hdc_getlabel(hdc, hd, unit)
	struct hdcsoftc *hdc;
	struct hdsoftc *hd;
	int unit;
{
	struct disklabel *lp = hd->sc_dk.dk_label;
	struct disklabel *xp = (void *)(hdc_iobuf + 64);
	int res;

	trace(("hdc_getlabel(%d)\n", unit));

#ifdef DEBUG
#define LBL_CHECK(x)					\
	if (xp->x != lp->x) {				\
		printf("%d-->%d\n", xp->x, lp->x);	\
		xp->x = lp->x;				\
	}
#else
#define LBL_CHECK(x)	xp->x = lp->x
#endif

	res = hdc_strategy(hdc, hd, unit, F_READ, 0, DEV_BSIZE, hdc_iobuf);
	LBL_CHECK(d_secsize);
	LBL_CHECK(d_ntracks);
	LBL_CHECK(d_nsectors);
	LBL_CHECK(d_ncylinders);
	LBL_CHECK(d_secpercyl);
	bcopy(xp, lp, sizeof(struct disklabel));

	return (0);
}

/*
 * Return the size of a partition, if known, or -1 if not.
 */
hdcsize(dev)
	dev_t dev;
{
	int unit = HDCUNIT(dev);
	int part = HDCPART(dev);
	struct hdsoftc *hd = hd_cd.cd_devs[unit];
	int size;

	trace (("hdcsize(%x == %d/%d)\n", dev, unit, part));

	if (hdcopen(dev, 0, S_IFBLK) != 0)
		return (-1);
#if 0
	if (hd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
#endif
		size = hd->sc_dk.dk_label->d_partitions[part].p_size;
	if (hdcclose(dev, 0, S_IFBLK) != 0)
		return (-1);
	debug (("hdcsize: size=%d\n", size));
	return (size);
}

/*
 *
 */
int
hdcopen (dev, flag, fmt)
	dev_t dev;
	int flag;
	int fmt;
{
	int unit = HDCUNIT(dev);
	int part = HDCPART(dev);
	struct hdcsoftc *hdc;
	struct hdsoftc *hd;
	int res, error;

	trace (("hdcopen(0x%x = %d/%d)\n", dev, unit, part));

	if (unit >= hd_cd.cd_ndevs) {
		printf("hdcopen: invalid unit %d\n", unit);
		return (ENXIO);
	}
	hd = hd_cd.cd_devs[unit];
	if (!hd) {
		printf("hdcopen: null-pointer in hdsoftc.\n");
		return (ENXIO);
	}
	hdc = (void *)hd->sc_dev.dv_parent;
	
	/* XXX here's much more to do! XXX */

	hdc_getdata (hdc, hd, unit);
	hdc_getlabel (hdc, hd, unit);

	return (0);
}

/*
 *
 */
int
hdcclose (dev, flag)
	dev_t dev;
	int flag;
{
	trace (("hdcclose()\n"));
	return (0);
}

/*
 *
 */
void
hdcstrategy(bp)
	register struct buf *bp;
{
	trace (("hdcstrategy()\n"));
	hdstrategy(bp);
	debug (("hdcstrategy done.\n"));
}

/*
 *
 */
int
hdcioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;	/* aka: addr */
	int flag;
	struct proc *p;
{
	struct hdsoftc *hd = hd_cd.cd_devs[HDCUNIT(dev)];
	struct hdcsoftc *hdc = (void *)hd->sc_dev.dv_parent;
	int error;

	trace (("hdcioctl(%x, %x)\n", dev, cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	/* XXX */

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)data = *(hd->sc_dk.dk_label);
		return (0);

	case DIOCGPART:
		((struct partinfo *)data)->disklab = hd->sc_dk.dk_label;
		((struct partinfo *)data)->part =
		  &hd->sc_dk.dk_label->d_partitions[HDCPART(dev)];
		return (0);

	case DIOCWDINFO:
	case DIOCSDINFO:
/* XXX
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = sdlock(sd)) != 0)
			return error;
		sd->flags |= SDF_LABELLING;
*/
		error = setdisklabel(hd->sc_dk.dk_label,
		     (struct disklabel *)data, 0, hd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(HDCLABELDEV(dev),
					hdstrategy, hd->sc_dk.dk_label,
					hd->sc_dk.dk_cpulabel);
		}
/* XXX
		sd->flags &= ~SDF_LABELLING;
		sdunlock(sd);
*/
		return (error);

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
/* XXX
		if (*(int *)data)
			sd->flags |= SDF_WLABEL;
		else
			sd->flags &= ~SDF_WLABEL;
*/
		return (0);
	      
	default:
		if (HDCPART(dev) != RAW_PART)
			return (ENOTTY);
		printf("IOCTL %x not implemented.\n", cmd);
		return (-1);
	}
}

/*
 *
 */
int 
hdcintr() 
{
	trace (("hdcintr()\n"));
}

/*
 * 
 */
int
hdcread (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	trace (("hdcread()\n"));
	return (physio (hdcstrategy, NULL, dev, B_READ, minphys, uio));
}

/*
 *
 */
int
hdcwrite (dev, uio)
	dev_t dev;
	struct uio *uio;
{
	trace (("hdcwrite()\n"));
	return (physio (hdcstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 *
 */
int
hdcdump(dev)
	dev_t dev;
{
	trace (("hdcdump (%x)\n", dev));
}

/*
 * we have to wait 0.7 usec between two accesses to any of the
 * dkc-registers, on a VS2000 with 1 MIPS, this is roughly one
 * instruction. Thus the loop-overhead will be enough...
 */
void
hdc_readregs(sc)
	struct hdcsoftc *sc;
{
	int i;
	char *p;

	trace(("hdc_readregs()\n"));

	sc->sc_dkc->dkc_cmd = 0x40;	/* set internal counter to zero */
	p = (void *)&sc->sc_sreg;	
	for (i=0; i<10; i++)
		*p++ = sc->sc_dkc->dkc_reg;	/* dkc_reg auto-increments */
}

void
hdc_writeregs(sc)
	struct hdcsoftc *sc;
{
	int i;
	char *p;

	trace(("hdc_writeregs()\n"));

	sc->sc_dkc->dkc_cmd = 0x40;	/* set internal counter to zero */
	p = (void *)&sc->sc_creg;	
	for (i=0; i<10; i++)
		sc->sc_dkc->dkc_reg = *p++;	/* dkc_reg auto-increments */
}

/*
 * hdc_command() issues a command and polls the intreq-register
 * to find when command has completed
 */
int
hdc_command(sc, cmd)
	struct hdcsoftc *sc;
	int cmd;
{
	volatile u_char *intreq = (void *)uvax_phys2virt(KA410_INTREQ);
	volatile u_char *intclr = (void *)uvax_phys2virt(KA410_INTCLR);
	volatile u_char *intmsk = (void *)uvax_phys2virt(KA410_INTMSK);
	int i, c;

	trace (("hdc_command(%x)\n", cmd));
	debug (("intr-state: %x %x %x\n", *intreq, *intclr, *intmsk));

	if (!haveLock) {
	  vsbus_lockDMA(sc->sc_cfargs);
	  haveLock = 1;
	}

	hdc_writeregs(sc);		/* write the prepared registers */
	*intclr = INTR_DC;		/* clear any old interrupt */
	sc->sc_dkc->dkc_cmd = cmd;	/* issue the command */
	for (i=0; i<MAX_WAIT; i++) {
		if ((c = *intreq) & INTR_DC)
			break;
	}
	if ((c & INTR_DC) == 0) {
		printf("hdc_command: timeout in command 0x%x\n", cmd);
	}
	hdc_readregs(sc);		/* read the status registers */
	sc->sc_status = sc->sc_dkc->dkc_stat;

	if (!keepLock) {
	  vsbus_unlockDMA(sc->sc_cfargs);
	  haveLock = 0;
	}

	if (sc->sc_status != DKC_ST_DONE|DKC_TC_SUCCESS) {
		printf("command 0x%x completed with status 0x%x\n",
			cmd, sc->sc_status);
		return (-1);
	}
	return (0);
}

/*
 * writing zero into the command-register will reset the controller.
 * This will not interrupt data-transfer commands!
 * Also no interrupt is generated, thus we don't use hdc_command()
 */
int 
hdc_reset(sc)
	struct hdcsoftc *sc;
{
	trace (("hdc_reset()\n"));
	
	sc->sc_dkc->dkc_cmd = DKC_CMD_RESET;	/* issue RESET command */
	hdc_readregs(sc);			/* read the status registers */
	sc->sc_status = sc->sc_dkc->dkc_stat;
	if (sc->sc_status != DKC_ST_DONE|DKC_TC_SUCCESS) {
		printf("RESET command completed with status 0x%x\n",
		    sc->sc_status);
		return (-1);
	}
	return (0);
}

int
hdc_rxselect(sc, unit)
	struct hdcsoftc *sc;
	int unit;
{
	register struct hdc9224_UDCreg *p = &sc->sc_creg;
	register struct hdc9224_UDCreg *q = &sc->sc_sreg;
	int error;

	/*
	 * bring command-regs in some known-to-work state and
	 * select the drive with the DRIVE SELECT command.
	 */
	p->udc_dma7  = 0;
	p->udc_dma15 = 0;
	p->udc_dma23 = 0;
	p->udc_dsect = 1;	/* sectors are numbered 1..15 !!! */
	p->udc_dhead = 0;
	p->udc_dcyl  = 0;
	p->udc_scnt  = 0;

	p->udc_rtcnt = UDC_RC_RX33READ;
	p->udc_mode  = UDC_MD_RX33;
	p->udc_term  = UDC_TC_FDD;

	/*
	 * this is ...
	 */
	error = hdc_command (sc, DKC_CMD_DRSEL_RX33 | unit);

	if ((error != 0) || ((q->udc_dstat & UDC_DS_READY) == 0)) {
		printf("\nfloppy-drive not ready (new floppy inserted?)\n\n");
		p->udc_rtcnt &= ~UDC_RC_INVRDY;	/* clear INVRDY-flag */
		error = hdc_command(sc, DKC_CMD_DRSEL_RX33 | unit);
		if ((error != 0) || ((q->udc_dstat & UDC_DS_READY) == 0)) {
			printf("diskette not ready(1): %x/%x\n", error,
			    q->udc_dstat);
			printf("floppy-drive offline?\n");
			return (-1);
		}

		if (q->udc_dstat & UDC_DS_TRK00)		/* track-0: */
			error = hdc_command(sc, DKC_CMD_STEPIN_FDD);
								/* step in */
		else
	  		error = hdc_command(sc, DKC_CMD_STEPOUT_FDD);
								/* step out */

		if ((error != 0) || ((q->udc_dstat & UDC_DS_READY) == 1)) {
			printf("diskette not ready(2): %x/%x\n", error,
			    q->udc_dstat);
			printf("No floppy inserted or drive offline\n");
			/* return (-1); */
		}

		p->udc_rtcnt |= UDC_RC_INVRDY;
		error = hdc_command(sc, DKC_CMD_DRSEL_RX33 | unit);
		if ((error != 0) || ((q->udc_dstat & UDC_DS_READY) == 0)) {
			printf("diskette not ready(3): %x/%x\n", error,
			    q->udc_dstat);
			printf("no floppy inserted or floppy-door open\n");
			return(-1);
		}
		printf("floppy-drive reselected.\n");
	}
	if (error)
		error = hdc_command (sc, DKC_CMD_DRSEL_RX33 | unit);

	return (error);
}

int
hdc_hdselect(sc, unit)
	struct hdcsoftc *sc;
	int unit;
{
	register struct hdc9224_UDCreg *p = &sc->sc_creg;
	register struct hdc9224_UDCreg *q = &sc->sc_sreg;
	int error;

	/*
	 * bring "creg" in some known-to-work state and
	 * select the drive with the DRIVE SELECT command.
	 */
	p->udc_dma7  = 0;
	p->udc_dma15 = 0;
	p->udc_dma23 = 0;
	p->udc_dsect = 0;		/* sectors are numbered 0..16 */
	p->udc_dhead = 0;
	p->udc_dcyl  = 0;
	p->udc_scnt  = 0;

	p->udc_rtcnt = UDC_RC_HDD_READ;
	p->udc_mode  = UDC_MD_HDD;
	p->udc_term  = UDC_TC_HDD;

	error = hdc_command (sc, DKC_CMD_DRSEL_HDD | unit);
	if (error)
		error = hdc_command (sc, DKC_CMD_DRSEL_HDD | unit);
	
	return (error);
}

/*
 * bring command-regs into some known-to-work state and select
 * the drive with the DRIVE SELECT command.
 */
int 
hdc_select(sc, unit)
	struct hdcsoftc *sc;
	int unit;
{
	int error;

	trace (("hdc_select(%x,%d)\n", sc, unit));

	switch (unit) {
	case 0:
	case 1:
		error = hdc_hdselect(sc, unit);
		break;
	case 2:
		error = hdc_rxselect(sc, unit);
		/* bertram: delay ??? XXX */
		break;
	default:
		printf("invalid unit %d in hdc_select()\n", unit);
		error = -1;
	}

	return (error);
}
#endif	/* NHDC > 0 */
