/*	$OpenBSD: mfm.c,v 1.4 2003/08/15 23:16:30 deraadt Exp $	*/
/*	$NetBSD: mfm.c,v 1.4 2001/07/26 22:55:13 wiz Exp $	*/
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by
 * Bertram Barth.
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
 * ToDo:
 *
 * - insert appropriate delays for diskette-drive where needed
 * - allow more than one sector per diskette-read
 * - check for and handle bad sectors
 * - ???
 */

#include "sys/param.h"
#include "sys/reboot.h"
#include "sys/disklabel.h"

#include "lib/libsa/stand.h"
#include "lib/libsa/ufs.h"

#include "../include/pte.h"
#include "../include/sid.h"
#include "../include/mtpr.h"
#include "../include/reg.h"
#include "../include/rpb.h"

#include "ka410.h"
#include "../vsa/hdc9224.h"

#include "data.h"
#include "vaxstand.h"

#define MAX_WAIT	(1000*1000)	/* # of loop-instructions in seconds */

struct mfm_softc {
	int		part;
	int		unit;
};

static struct disklabel mfmlabel;
static struct mfm_softc mfm_softc;
static char io_buf[DEV_BSIZE];

/*
 * These should probably be somewhere else, but ka410 is the only
 * one with mfm disks anyway...
 */
volatile unsigned char *ka410_intreq = (void*)0x2008000f;
volatile unsigned char *ka410_intclr = (void*)0x2008000f;
volatile unsigned char *ka410_intmsk = (void*)0x2008000c;

static volatile struct hdc9224_DKCreg *dkc = (void *) 0x200c0000;
static volatile struct hdc9224_UDCreg sreg;	/* input */
static volatile struct hdc9224_UDCreg creg;	/* output */

static void sreg_read(void);
static void creg_write(void);
static int mfm_rxprepare(void);
static int mfm_command(int cmd);
static int mfm_rxselect(int unit);
static int mfm_rdselect(int unit);
static int mfm_rxstrategy(void *f, int func, daddr_t dblk, size_t size, void *buf, size_t *rsize);
static int mfm_rdstrategy(void *f, int func, daddr_t dblk, size_t size, void *buf, size_t *rsize);
/*
 * we have to wait 0.7 usec between two accesses to any of the
 * dkc-registers, on a VS2000 with 1 MIPS, this is roughly one
 * instruction. Thus the loop-overhead will be enough...
 */
static void
sreg_read(void)
{
	int	i;
	char    *p;

	dkc->dkc_cmd = 0x40;	/* set internal counter to zero */
	p = (void *) &sreg;
	for (i = 0; i < 10; i++)
		*p++ = dkc->dkc_reg;	/* dkc_reg auto-increments */
}

static void
creg_write(void)
{
	int	i;
	char    *p;

	dkc->dkc_cmd = 0x40;	/* set internal counter to zero */
	p = (void *) &creg;
	for (i = 0; i < 10; i++)
		dkc->dkc_reg = *p++;	/* dkc_reg auto-increments */
}

/*
 * floppies are handled in a quite strange way by this controller...
 *
 * before reading/writing a sector from/to floppy, we use the SEEK/READ_ID
 * command to place the head at the desired location. Then we wait some
 * time before issuing the real command in order to let the drive become
 * ready...
 */
int
mfm_rxprepare(void)
{
	int	error;

	error = mfm_command(DKC_CMD_SEEKREADID | 0x04); /* step=1, verify=0 */
	if (error) {
		printf("error while stepping to position %d/%d/%x. Retry...\n",
		    creg.udc_dsect, creg.udc_dhead, creg.udc_dcyl);
		error = mfm_command(DKC_CMD_SEEKREADID | 0x04);
	}
	return error;
}

int
mfm_rxselect(int unit)
{
	int	error;

	/*
	 * bring "creg" in some known-to-work state and
	 * select the drive with the DRIVE SELECT command.
	 */
	creg.udc_dma7 = 0;
	creg.udc_dma15 = 0;
	creg.udc_dma23 = 0;
	creg.udc_dsect = 1;	/* sectors are numbered 1..15 !!! */
	creg.udc_dhead = 0;
	creg.udc_dcyl = 0;
	creg.udc_scnt = 0;

	creg.udc_rtcnt = UDC_RC_RX33READ;
	creg.udc_mode = UDC_MD_RX33;
	creg.udc_term = UDC_TC_FDD;

	/*
	 * this is ...
	 */
	error = mfm_command(DKC_CMD_DRSEL_RX33 | unit);

	if ((error != 0) || ((sreg.udc_dstat & UDC_DS_READY) == 0)) {
		printf("\nfloppy-drive not ready (new floppy inserted?)\n\n");

		creg.udc_rtcnt &= ~UDC_RC_INVRDY;	/* clear INVRDY-flag */
		error = mfm_command(DKC_CMD_DRSEL_RX33 | unit);
		if ((error != 0) || ((sreg.udc_dstat & UDC_DS_READY) == 0)) {
			printf("diskette not ready(1): %x/%x\n",
			       error, sreg.udc_dstat);
			printf("floppy-drive offline?\n");
			return (-1);
		}
		if (sreg.udc_dstat & UDC_DS_TRK00)
			error = mfm_command(DKC_CMD_STEPIN_FDD);
		else
			error = mfm_command(DKC_CMD_STEPOUT_FDD);

		/*
		 * now ready should be 0, cause INVRDY is not set
		 * (retrying a command makes this fail...)
		 */
		if ((error != 0) || ((sreg.udc_dstat & UDC_DS_READY) == 1)) {
			printf("diskette not ready(2): %x/%x\n",
			       error, sreg.udc_dstat);
		}
		creg.udc_rtcnt |= UDC_RC_INVRDY;
		error = mfm_command(DKC_CMD_DRSEL_RX33 | unit);

		if ((error != 0) || ((sreg.udc_dstat & UDC_DS_READY) == 0)) {
			printf("diskette not ready(3): %x/%x\n",
			       error, sreg.udc_dstat);
			printf("no floppy inserted or floppy-door open\n");
			return (-1);
		}
		printf("floppy-drive reselected.\n");
	}
	return (error);
}

int
mfm_rdselect(int unit)
{
	int	error;

	/*
	 * bring "creg" in some known-to-work state and
	 * select the drive with the DRIVE SELECT command.
	 */
	creg.udc_dma7 = 0;
	creg.udc_dma15 = 0;
	creg.udc_dma23 = 0;
	creg.udc_dsect = 0;	/* sectors are numbered 0..16 */
	creg.udc_dhead = 0;
	creg.udc_dcyl = 0;
	creg.udc_scnt = 0;

	creg.udc_rtcnt = UDC_RC_HDD_READ;
	creg.udc_mode = UDC_MD_HDD;
	creg.udc_term = UDC_TC_HDD;

	error = mfm_command(DKC_CMD_DRSEL_HDD | unit);

	return (error);
}

static int	mfm_retry = 0;

int
mfm_command(int	cmd)
{
	int	termcode, ready, i;

	creg_write();		/* write command-registers */
	*ka410_intclr = INTR_DC;
	dkc->dkc_cmd = cmd;	/* issue command */
	for (i = 0; i < MAX_WAIT; i++) {
		if (*ka410_intreq & INTR_DC)	/* wait for interrupt */
			break;
	}
	if ((*ka410_intreq & INTR_DC) == 0)
		printf("timeout in mfm_command...\n");

	sreg_read();		/* read status-registers */

	if (dkc->dkc_stat == (DKC_ST_DONE | DKC_TC_SUCCESS))
		return (0);

	if (sreg.udc_cstat & UDC_CS_ECCERR) {
		printf(
"\nspurious(?) ECC/CRC error at s%d/t%d/c%d [s%d/t%d/c%d(%d)]\n",
		   sreg.udc_csect, sreg.udc_chead, sreg.udc_ccyl,
		   creg.udc_dsect, creg.udc_dhead, creg.udc_dcyl,creg.udc_scnt);
		if (sreg.udc_csect != creg.udc_dsect + creg.udc_scnt - 1) {
			printf("DMA: %x %x %x [%x]\n",
			    sreg.udc_dma23, sreg.udc_dma15,
			    sreg.udc_dma7, 512 * (sreg.udc_csect -
			    creg.udc_dsect));
			creg.udc_scnt = creg.udc_scnt -
			    (sreg.udc_csect - creg.udc_dsect) - 1;
			creg.udc_dsect = sreg.udc_csect + 1;
			creg.udc_dma23 = sreg.udc_dma23;
			creg.udc_dma15 = sreg.udc_dma15 + 2;
			creg.udc_dma7 = 0;
			printf("Retry starting from s%d/t%d/c%d (%d). ",
			    creg.udc_dsect, creg.udc_dhead, creg.udc_dcyl,
			    creg.udc_scnt);
		}
		goto retry;
	}
	termcode = (dkc->dkc_stat & DKC_ST_TERMCOD) >> 3;
	ready = sreg.udc_dstat & UDC_DS_READY;

	printf("cmd:0x%x: termcode=0x%x, status=0x%x, cstat=0x%x, dstat=0x%x\n",
	       cmd, termcode, dkc->dkc_stat, sreg.udc_cstat, sreg.udc_dstat);

	if (dkc->dkc_stat & DKC_ST_BADSECT)
		printf("bad sector found: s%d/t%d/c%d\n", creg.udc_dsect,
		       creg.udc_dhead, creg.udc_dcyl);
retry:
	if ((mfm_retry == 0) && (sreg.udc_cstat & UDC_CS_RETREQ)) {
		mfm_retry = 1;
		printf("Retrying... ");
		mfm_command(cmd);
		printf("Retry done.\n");
		mfm_retry = 0;
	}
	return ((dkc->dkc_stat & DKC_ST_TERMCOD) >> 3);
}

/*
 * on-disk geometry block
 */
#define _aP	__attribute__ ((packed))	/* force byte-alignment */

volatile struct mfm_xbn {
	char		mbz[10];/* 10 bytes of zero */
	long xbn_count	_aP;	/* number of XBNs */
	long dbn_count	_aP;	/* number of DBNs */
	long lbn_count	_aP;	/* number of LBNs (Logical-Block-Numbers) */
	long rbn_count	_aP;	/* number of RBNs (Replacement-Block-Numbers) */
	short		nspt;	/* number of sectors per track */
	short		ntracks;/* number of tracks */
	short		ncylinders;	/* number of cylinders */
	short		precomp;/* first cylinder for write precompensation */
	short		reduced;/* first cylinder for reduced write current */
	short		seek_rate;	/* seek rate or zero for buffered
					 * seeks */
	short		crc_eec;/* 0 if CRC is being used or 1 if ECC is
				 * being used */
	short		rct;	/* "replacement control table" (RCT) */
	short		rct_ncopies;	/* number of copies of the RCT */
	long media_id	_aP;	/* media identifier */
	short		interleave;	/* sector-to-sector interleave */
	short		headskew;	/* head-to-head skew */
	short		cylskew;/* cylinder-to-cylinder skew */
	short		gap0_size;	/* size of GAP 0 in the MFM format */
	short		gap1_size;	/* size of GAP 1 in the MFM format */
	short		gap2_size;	/* size of GAP 2 in the MFM format */
	short		gap3_size;	/* size of GAP 3 in the MFM format */
	short		sync_value;	/* sync value used to start a track
					 * when formatting */
	char		reserved[32];	/* reserved for use by the RQDX1/2/3
					 * formatter */
	short		serial_number;	/* serial number */
	char		fill[412];	/* Filler bytes to the end of the
					 * block */
	short		checksum;	/* checksum over the XBN */
} mfm_xbn;

#ifdef verbose
display_xbn(struct mfm_xbn *p)
{
	printf("**DiskData**	XBNs: %d, DBNs: %d, LBNs: %d, RBNs: %d\n",
	    p->xbn_count, p->dbn_count, p->lbn_count, p->rbn_count);
	printf("sect/track: %d, tracks: %d, cyl: %d, precomp/reduced: %d/%d\n",
	    p->nspt, p->ntracks, p->ncylinders, p->precomp, p->reduced);
	printf("seek-rate: %d, crc/eec: %s, RCT: %d, RCT-copies: %d\n",
	    p->seek_rate, p->crc_eec ? "EEC" : "CRC", p->rct, p->rct_ncopies);
	printf("media-ID: 0x%x, interleave: %d, headskew: %d, cylskew: %d\n",
	    &p->media_id, p->interleave, p->headskew, p->cylskew);
	printf("gap0: %d, gap1: %d, gap2: %d, gap3: %d, sync-value: %d\n",
	    p->gap0_size, p->gap1_size, p->gap2_size, p->gap3_size,
	    p->sync_value);
	printf("serial: %d, checksum: %d, size: %d, reserved: %32c\n",
	    p->serial_number, p->checksum, sizeof(*p), p->reserved);
}
#endif

int
mfmopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	char *msg;
	struct disklabel *lp = &mfmlabel;
	struct mfm_softc *msc = &mfm_softc;
	int err;
	size_t i;

	bzero(lp, sizeof(struct disklabel));
	msc->unit = unit;
	msc->part = part;

	err = mfmstrategy(msc, F_READ, LABELSECTOR, DEV_BSIZE, io_buf, &i);
	if (err) {
		printf("reading disklabel: %s\n", strerror(err));
		return 0;
	}
	msg = getdisklabel(io_buf + LABELOFFSET, lp);
	if (msg)
		printf("getdisklabel: %s\n", msg);

	f->f_devdata = (void *) msc;

	{
#ifdef verbose
		int		k;
		unsigned char  *ucp;
		struct mfm_xbn *xp;
#endif

		/* mfmstrategy(msc, F_READ, -16, 8192, io_buf, &i); */
		mfmstrategy(msc, F_READ, -16, 512, io_buf, &i);
#ifdef verbose
		printf("dumping raw disk-block #0:\n");
		ucp = io_buf;
		for (k = 0; k < 128; k++) {
			if (ucp[k] < 0x10)
				printf("0");
			printf("%x ", ucp[k]);
			if (k % 8 == 7)
				printf("  ");
			if (k % 16 == 15)
				printf("\n");
		}
		printf("\n");

		xp = (void *) io_buf;
		display_xbn(xp);
		printf("\n");
#endif
	}

	if (unit == 2) {	/* floppy! */
		if (lp->d_ntracks != 2) {
#ifdef verbose
			printf("changing number of tracks from %d to %d.\n",
			       lp->d_ntracks, 2);
#endif
			lp->d_ntracks = 2;
		}
	} else {		/* hard-disk */
		unsigned short *usp = (void *) io_buf;
#ifdef verbose
		printf("label says: s/t/c = %d/%d/%d\n",
		       lp->d_nsectors, lp->d_ntracks, lp->d_ncylinders);
#endif
		if (lp->d_nsectors != usp[13]) {
#ifdef verbose
			printf("changing number of sectors from %d to %d.\n",
			       lp->d_nsectors, usp[13]);
#endif
			lp->d_nsectors = usp[13];
		}
		if (lp->d_ntracks != usp[14]) {
#ifdef verbose
			printf("changing number of heads/tracks from %d to %d.\n",
			       lp->d_ntracks, usp[14]);
#endif
			lp->d_ntracks = usp[14];
		}
		if (lp->d_ncylinders != usp[15]) {
#ifdef verbose
			printf("changing number of cylinders from %d to %d.\n",
			       lp->d_ncylinders, usp[15]);
#endif
			lp->d_ncylinders = usp[15];
		}
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
	}

	return (0);
}

int
mfm_rxstrategy(void *f, int func, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
	struct mfm_softc *msc = f;
	struct disklabel *lp;
	int	block, sect, head, cyl, scount, res;

	lp = &mfmlabel;
	block = (dblk < 0 ? 0 : dblk + lp->d_partitions[msc->part].p_offset);

	mfm_rxselect(msc->unit);

	/*
	 * if label is empty, assume RX33
	 */
	if (lp->d_nsectors == 0)
		lp->d_nsectors = 15;
	if (lp->d_ntracks == 0)
		lp->d_ntracks = 2;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 30;

	bzero((void *) 0x200D0000, size);
	scount = size / 512;

	while (scount) {
		/*
		 * prepare drive/operation parameter
		 */
		cyl = block / lp->d_secpercyl;
		sect = block % lp->d_secpercyl;
		head = sect / lp->d_nsectors;
		sect = sect % lp->d_nsectors;

		/*
		 * *rsize = 512;		one sector after the other
		 * ...
		 */
		*rsize = 512 * min(scount, lp->d_nsectors - sect);

		/*
		 * now initialize the register values ...
		 */
		creg.udc_dma7 = 0;
		creg.udc_dma15 = 0;
		creg.udc_dma23 = 0;

		creg.udc_dsect = sect + 1;	/* sectors are numbered 1..15
						 * !!! */
		head |= (cyl >> 4) & 0x70;
		creg.udc_dhead = head;
		creg.udc_dcyl = cyl;

		creg.udc_scnt = *rsize / 512;

		if (func == F_WRITE) {
			creg.udc_rtcnt = UDC_RC_RX33WRT;
			creg.udc_mode = UDC_MD_RX33;
			creg.udc_term = UDC_TC_FDD;

			mfm_rxprepare();
			/* copy from buf */
			bcopy(buf, (void *) 0x200D0000, *rsize);
			res = mfm_command(DKC_CMD_WRITE_RX33);
		} else {
			creg.udc_rtcnt = UDC_RC_RX33READ;
			creg.udc_mode = UDC_MD_RX33;
			creg.udc_term = UDC_TC_FDD;

			mfm_rxprepare();
			/* clear disk buffer */
			bzero((void *) 0x200D0000, *rsize);
			res = mfm_command(DKC_CMD_READ_RX33);
			/* copy to buf */
			bcopy((void *) 0x200D0000, buf, *rsize);
		}

		scount -= *rsize / 512;
		block += *rsize / 512;
		(char *)buf += *rsize;
	}

	*rsize = size;
	return 0;
}

int
mfm_rdstrategy(void *f, int func, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
	struct mfm_softc *msc = f;
	struct disklabel *lp;
	int	block, sect, head, cyl, scount, cmd, res;

	lp = &mfmlabel;
	block = (dblk < 0 ? 0 : dblk + lp->d_partitions[msc->part].p_offset);

	/*
	 * if label is empty, assume RD32 (XXX this must go away!!!)
	 */
	if (lp->d_nsectors == 0)
		lp->d_nsectors = 17;
	if (lp->d_ntracks == 0)
		lp->d_ntracks = 6;
	if (lp->d_secpercyl == 0)
		lp->d_secpercyl = 102;

	mfm_rdselect(msc->unit);

	bzero((void *) 0x200D0000, size);
	scount = size / 512;

	while (scount) {
		/*
		 * prepare drive/operation parameter
		 */
		cyl = block / lp->d_secpercyl;
		sect = block % lp->d_secpercyl;
		head = sect / lp->d_nsectors;
		sect = sect % lp->d_nsectors;

		if (dblk < 0) {
#ifdef verbose
			printf("using raw diskblock-data!\n");
			printf("block %d, dblk %d ==> cyl %d, head %d, sect %d\n",
			       block, dblk, cyl, sect, head);
#endif
		} else
			cyl += 1;	/* first cylinder is reserved for
					 * controller! */

		*rsize = 512 * min(scount, lp->d_nsectors - sect);
		/*
		 * now re-initialize the register values ...
		 */
		creg.udc_dma7 = 0;
		creg.udc_dma15 = 0;
		creg.udc_dma23 = 0;

		creg.udc_dsect = sect;
		head |= (cyl >> 4) & 0x70;
		creg.udc_dhead = head;
		creg.udc_dcyl = cyl;

		creg.udc_scnt = *rsize / 512;

		if (func == F_WRITE) {
			creg.udc_rtcnt = UDC_RC_HDD_WRT;
			creg.udc_mode = UDC_MD_HDD;
			creg.udc_term = UDC_TC_HDD;
			cmd = DKC_CMD_WRITE_HDD;

			bcopy(buf, (void *) 0x200D0000, *rsize);
			res = mfm_command(cmd);
		} else {
			creg.udc_rtcnt = UDC_RC_HDD_READ;
			creg.udc_mode = UDC_MD_HDD;
			creg.udc_term = UDC_TC_HDD;
			cmd = DKC_CMD_READ_HDD;

			bzero((void *) 0x200D0000, *rsize);
			res = mfm_command(cmd);
			bcopy((void *) 0x200D0000, buf, *rsize);
		}

		scount -= *rsize / 512;
		block += *rsize / 512;
		(char *)buf += *rsize;
	}

	/*
	 * unselect the drive ...
	 */
	mfm_command(DKC_CMD_DRDESELECT);

	*rsize = size;
	return 0;
}

int
mfmstrategy(void *f, int func, daddr_t dblk, size_t size, void *buf, size_t *rsize)
{
	struct mfm_softc *msc = f;
	int	res = -1;

	switch (msc->unit) {
	case 0:
	case 1:
		res = mfm_rdstrategy(f, func, dblk, size, buf, rsize);
		break;
	case 2:
		res = mfm_rxstrategy(f, func, dblk, size, buf, rsize);
		break;
	default:
		printf("invalid unit %d in mfmstrategy()\n", msc->unit);
	}
	return (res);
}
