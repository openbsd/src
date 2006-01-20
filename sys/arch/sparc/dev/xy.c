/*	$OpenBSD: xy.c,v 1.27 2006/01/20 23:27:25 miod Exp $	*/
/*	$NetBSD: xy.c,v 1.26 1997/07/19 21:43:56 pk Exp $	*/

/*
 *
 * Copyright (c) 1995 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * x y . c   x y l o g i c s   4 5 0 / 4 5 1   s m d   d r i v e r
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * id: $NetBSD: xy.c,v 1.26 1997/07/19 21:43:56 pk Exp $
 * started: 14-Sep-95
 * references: [1] Xylogics Model 753 User's Manual
 *                 part number: 166-753-001, Revision B, May 21, 1988.
 *                 "Your Partner For Performance"
 *             [2] other NetBSD disk device drivers
 *	       [3] Xylogics Model 450 User's Manual
 *		   part number: 166-017-001, Revision B, 1983.
 *	       [4] Addendum to Xylogics Model 450 Disk Controller User's
 *			Manual, Jan. 1985.
 *	       [5] The 451 Controller, Rev. B3, September 2, 1986.
 *	       [6] David Jones <dej@achilles.net>'s unfinished 450/451 driver
 *
 */

#undef XYC_DEBUG		/* full debug */
#undef XYC_DIAG			/* extra sanity checks */
#if defined(DIAGNOSTIC) && !defined(XYC_DIAG)
#define XYC_DIAG		/* link in with master DIAG option */
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/dkbad.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <dev/sun/disklabel.h>
#include <machine/conf.h>

#include <sparc/dev/xyreg.h>
#include <sparc/dev/xyvar.h>
#include <sparc/dev/xio.h>
#include <sparc/sparc/vaddrs.h>

/*
 * macros
 */

/*
 * XYC_GO: start iopb ADDR (DVMA addr in a u_long) on XYC
 */
#define XYC_GO(XYC, ADDR) { \
	(XYC)->xyc_addr_lo = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XYC)->xyc_addr_hi = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XYC)->xyc_reloc_lo = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XYC)->xyc_reloc_hi = (ADDR); \
	(XYC)->xyc_csr = XYC_GBSY; /* go! */ \
}

/*
 * XYC_DONE: don't need IORQ, get error code and free (done after xyc_cmd)
 */

#define XYC_DONE(SC,ER) { \
	if ((ER) == XY_ERR_AOK) { \
		(ER) = (SC)->ciorq->errno; \
		(SC)->ciorq->mode = XY_SUB_FREE; \
		wakeup((SC)->ciorq); \
	} \
	}

/*
 * XYC_ADVANCE: advance iorq's pointers by a number of sectors
 */

#define XYC_ADVANCE(IORQ, N) { \
	if (N) { \
		(IORQ)->sectcnt -= (N); \
		(IORQ)->blockno += (N); \
		(IORQ)->dbuf += ((N)*XYFM_BPS); \
	} \
}

/*
 * note - addresses you can sleep on:
 *   [1] & of xy_softc's "state" (waiting for a chance to attach a drive)
 *   [2] & an iorq (waiting for an XY_SUB_WAIT iorq to finish)
 */


/*
 * function prototypes
 * "xyc_*" functions are internal, all others are external interfaces
 */

extern int pil_to_vme[];	/* from obio.c */

/* internals */
struct xy_iopb *xyc_chain(struct xyc_softc *, struct xy_iorq *);
int	xyc_cmd(struct xyc_softc *, int, int, int, int, int, char *, int);
char   *xyc_e2str(int);
int	xyc_entoact(int);
int	xyc_error(struct xyc_softc *, struct xy_iorq *,
		   struct xy_iopb *, int);
int	xyc_ioctlcmd(struct xy_softc *, dev_t dev, struct xd_iocmd *);
void	xyc_perror(struct xy_iorq *, struct xy_iopb *, int);
int	xyc_piodriver(struct xyc_softc *, struct xy_iorq *);
int	xyc_remove_iorq(struct xyc_softc *);
int	xyc_reset(struct xyc_softc *, int, struct xy_iorq *, int,
			struct xy_softc *);
inline void xyc_rqinit(struct xy_iorq *, struct xyc_softc *,
			    struct xy_softc *, int, u_long, int,
			    caddr_t, struct buf *);
void	xyc_rqtopb(struct xy_iorq *, struct xy_iopb *, int, int);
void	xyc_start(struct xyc_softc *, struct xy_iorq *);
int	xyc_startbuf(struct xyc_softc *, struct xy_softc *, struct buf *);
int	xyc_submit_iorq(struct xyc_softc *, struct xy_iorq *, int);
void	xyc_tick(void *);
int	xyc_unbusy(struct xyc *, int);
void	xyc_xyreset(struct xyc_softc *, struct xy_softc *);

/* machine interrupt hook */
int	xycintr(void *);

/* autoconf */
int	xycmatch(struct device *, void *, void *);
void	xycattach(struct device *, struct device *, void *);
int	xymatch(struct device *, void *, void *);
void	xyattach(struct device *, struct device *, void *);

static	void xydummystrat(struct buf *);
int	xygetdisklabel(struct xy_softc *, void *);

/*
 * cfdrivers: device driver interface to autoconfig
 */

struct cfattach xyc_ca = {
	sizeof(struct xyc_softc), xycmatch, xycattach
};

struct cfdriver xyc_cd = {
	NULL, "xyc", DV_DULL
};

struct cfattach xy_ca = {
	sizeof(struct xy_softc), xymatch, xyattach
};

struct cfdriver xy_cd = {
	NULL, "xy", DV_DISK
};

struct xyc_attach_args {	/* this is the "aux" args to xyattach */
	int	driveno;	/* unit number */
	char	*buf;		/* scratch buffer for reading disk label */
	char	*dvmabuf;	/* DVMA address of above */
	int	fullmode;	/* submit mode */
	int	booting;	/* are we booting or not? */
};

/*
 * dkdriver
 */

struct dkdriver xydkdriver = { xystrategy };

/*
 * start: disk label fix code (XXX)
 */

static void *xy_labeldata;

static void
xydummystrat(bp)
	struct buf *bp;
{
	if (bp->b_bcount != XYFM_BPS)
		panic("xydummystrat");
	bcopy(xy_labeldata, bp->b_data, XYFM_BPS);
	bp->b_flags |= B_DONE;
	bp->b_flags &= ~B_BUSY;
}

int
xygetdisklabel(xy, b)
	struct xy_softc *xy;
	void *b;
{
	char *err;
	struct sun_disklabel *sdl;

	/* We already have the label data in `b'; setup for dummy strategy */
	xy_labeldata = b;

	/* Required parameter for readdisklabel() */
	xy->sc_dk.dk_label->d_secsize = XYFM_BPS;

	err = readdisklabel(MAKEDISKDEV(0, xy->sc_dev.dv_unit, RAW_PART),
					xydummystrat,
				xy->sc_dk.dk_label, xy->sc_dk.dk_cpulabel, 0);
	if (err) {
		printf("%s: %s\n", xy->sc_dev.dv_xname, err);
		return(XY_ERR_FAIL);
	}

	/* Ok, we have the label; fill in `pcyl' if there's SunOS magic */
	sdl = (struct sun_disklabel *)xy->sc_dk.dk_cpulabel->cd_block;
	if (sdl->sl_magic == SUN_DKMAGIC)
		xy->pcyl = sdl->sl_pcylinders;
	else {
		printf("%s: WARNING: no `pcyl' in disk label.\n",
			xy->sc_dev.dv_xname);
		xy->pcyl = xy->sc_dk.dk_label->d_ncylinders +
			xy->sc_dk.dk_label->d_acylinders;
		printf("%s: WARNING: guessing pcyl=%d (ncyl+acyl)\n",
		xy->sc_dev.dv_xname, xy->pcyl);
	}

	xy->ncyl = xy->sc_dk.dk_label->d_ncylinders;
	xy->acyl = xy->sc_dk.dk_label->d_acylinders;
	xy->nhead = xy->sc_dk.dk_label->d_ntracks;
	xy->nsect = xy->sc_dk.dk_label->d_nsectors;
	xy->sectpercyl = xy->nhead * xy->nsect;
	xy->sc_dk.dk_label->d_secsize = XYFM_BPS; /* not handled by
                                          	  * sun->bsd */
	return(XY_ERR_AOK);
}

/*
 * end: disk label fix code (XXX)
 */

/*
 * a u t o c o n f i g   f u n c t i o n s
 */

/*
 * xycmatch: determine if xyc is present or not.   we do a
 * soft reset to detect the xyc.
 */

int xycmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct xyc *xyc;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	switch (ca->ca_bustype) {
	case BUS_OBIO:
	case BUS_SBUS:
	case BUS_VME32:
	default:
		return (0);
	case BUS_VME16:
		xyc = (struct xyc *) ra->ra_vaddr;
		if (probeget((caddr_t) &xyc->xyc_rsetup, 1) == -1)
			return (0);
		if (xyc_unbusy(xyc, XYC_RESETUSEC) == XY_ERR_FAIL)
			return(0);
		return (1);
	}
}

/*
 * xycattach: attach controller
 */
void
xycattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct xyc_softc *xyc = (void *) self;
	struct confargs *ca = aux;
	struct xyc_attach_args xa;
	int     lcv, err, pri, res, pbsz;
	void	*tmp, *tmp2;
	void	*dtmp, *dtmp2;
	u_long	ultmp;

	/* get addressing and intr level stuff from autoconfig and load it
	 * into our xyc_softc. */

	ca->ca_ra.ra_vaddr = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct xyc));

	xyc->xyc = (struct xyc *) ca->ca_ra.ra_vaddr;
	pri = ca->ca_ra.ra_intr[0].int_pri;
	xyc->ipl = pil_to_vme[pri];
	xyc->vector = ca->ca_ra.ra_intr[0].int_vec;
	printf(" pri %d", pri);
	xyc->no_ols = 0; /* XXX should be from config */

	for (lcv = 0; lcv < XYC_MAXDEV; lcv++)
		xyc->sc_drives[lcv] = (struct xy_softc *) 0;

	/*
	 * allocate and zero buffers
	 * check boundaries of the KVA's ... all IOPBs must reside in
 	 * the same 64K region.
	 */

	pbsz = XYC_MAXIOPB * sizeof(struct xy_iopb);
	dtmp = dtmp2 = (struct xy_iopb *)dvma_malloc(pbsz, &tmp, M_NOWAIT);
	tmp2 = tmp;
	ultmp = (u_long) dtmp;
	if ((ultmp & 0xffff0000) != ((ultmp + pbsz) & 0xffff0000)) {
		dtmp = (struct xy_iopb *)
		    dvma_malloc(pbsz, &tmp, M_NOWAIT); /* retry! */
		dvma_free(dtmp2, pbsz, &tmp2);
		ultmp = (u_long) dtmp;
		if ((ultmp & 0xffff0000) != ((ultmp + pbsz) & 0xffff0000)) {
			printf("%s: can't alloc IOPB mem in 64K\n",
				xyc->sc_dev.dv_xname);
			return;
		}
	}
	bzero(tmp, pbsz);
	xyc->iopbase = tmp;
	xyc->iopbase = dtmp; /* XXX TMP HACK */
	xyc->dvmaiopb = (struct xy_iopb *) ((u_long)dtmp - DVMA_BASE);
	xyc->reqs = (struct xy_iorq *)
	    malloc(XYC_MAXIOPB * sizeof(struct xy_iorq), M_DEVBUF, M_NOWAIT);
	if (xyc->reqs == NULL)
		panic("xyc malloc");
	bzero(xyc->reqs, XYC_MAXIOPB * sizeof(struct xy_iorq));

	/*
	 * init iorq to iopb pointers, and non-zero fields in the
	 * iopb which never change.
	 */

	for (lcv = 0; lcv < XYC_MAXIOPB; lcv++) {
		xyc->xy_chain[lcv] = NULL;
		xyc->reqs[lcv].iopb = &xyc->iopbase[lcv];
		xyc->iopbase[lcv].asr = 1;	/* always the same */
		xyc->iopbase[lcv].eef = 1;	/* always the same */
		xyc->iopbase[lcv].ecm = XY_ECM;	/* always the same */
		xyc->iopbase[lcv].aud = 1;	/* always the same */
		xyc->iopbase[lcv].relo = 1;	/* always the same */
		xyc->iopbase[lcv].thro = XY_THRO;/* always the same */
	}
	xyc->ciorq = &xyc->reqs[XYC_CTLIOPB];    /* short hand name */
	xyc->ciopb = &xyc->iopbase[XYC_CTLIOPB]; /* short hand name */
	xyc->xy_hand = 0;

	/* read controller parameters and insure we have a 450/451 */

	err = xyc_cmd(xyc, XYCMD_ST, 0, 0, 0, 0, 0, XY_SUB_POLL);
	res = xyc->ciopb->ctyp;
	XYC_DONE(xyc, err);
	if (res != XYCT_450) {
		if (err)
			printf(": %s: ", xyc_e2str(err));
		printf(": doesn't identify as a 450/451\n");
		return;
	}
	printf(": Xylogics 450/451");
	if (xyc->no_ols)
		printf(" [OLS disabled]"); /* 450 doesn't overlap seek right */
	printf("\n");
	if (err) {
		printf("%s: error: %s\n", xyc->sc_dev.dv_xname,
				xyc_e2str(err));
		return;
	}
	if ((xyc->xyc->xyc_csr & XYC_ADRM) == 0) {
		printf("%s: 24 bit addressing turned off\n",
			xyc->sc_dev.dv_xname);
		printf("please set hardware jumpers JM1-JM2=in, JM3-JM4=out\n");
		printf("to enable 24 bit mode and this driver\n");
		return;
	}

	/* link in interrupt with higher level software */

	xyc->sc_ih.ih_fun = xycintr;
	xyc->sc_ih.ih_arg = xyc;
	vmeintr_establish(ca->ca_ra.ra_intr[0].int_vec,
	    ca->ca_ra.ra_intr[0].int_pri, &xyc->sc_ih, IPL_BIO,
	    self->dv_xname);

	/* now we must look for disks using autoconfig */
	xa.dvmabuf = (char *)dvma_malloc(XYFM_BPS, &xa.buf, M_NOWAIT);
	xa.fullmode = XY_SUB_POLL;
	xa.booting = 1;

	if (ca->ca_ra.ra_bp && ca->ca_ra.ra_bp->val[0] == -1 &&
	    ca->ca_ra.ra_bp->val[1] == xyc->sc_dev.dv_unit) {
		bootpath_store(1, ca->ca_ra.ra_bp + 1); /* advance bootpath */
	}

	for (xa.driveno = 0; xa.driveno < XYC_MAXDEV; xa.driveno++)
		(void) config_found(self, (void *) &xa, NULL);

	dvma_free(xa.dvmabuf, XYFM_BPS, &xa.buf);
	bootpath_store(1, NULL);

	/* start the watchdog clock */
	timeout_set(&xyc->xyc_tick_tmo, xyc_tick, xyc);
	timeout_add(&xyc->xyc_tick_tmo, XYC_TICKCNT);

}

/*
 * xymatch: probe for disk.
 *
 * note: we almost always say disk is present.   this allows us to
 * spin up and configure a disk after the system is booted (we can
 * call xyattach!).
 */
int
xymatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct xyc_attach_args *xa = aux;

	/* looking for autoconf wildcard or exact match */

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != xa->driveno)
		return 0;

	return 1;

}

/*
 * xyattach: attach a disk.   this can be called from autoconf and also
 * from xyopen/xystrategy.
 */
void
xyattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct xy_softc *xy = (void *) self, *oxy;
	struct xyc_softc *xyc = (void *) parent;
	struct xyc_attach_args *xa = aux;
	int     err, spt, mb, blk, lcv, fmode, s = 0, newstate;
	struct dkbad *dkb;
	struct bootpath *bp;

	/*
	 * Always re-initialize the disk structure.  We want statistics
	 * to start with a clean slate.
	 */
	bzero(&xy->sc_dk, sizeof(xy->sc_dk));
	xy->sc_dk.dk_driver = &xydkdriver;
	xy->sc_dk.dk_name = xy->sc_dev.dv_xname;

	/* if booting, init the xy_softc */

	if (xa->booting) {
		xy->state = XY_DRIVE_UNKNOWN;	/* to start */
		xy->flags = 0;
		xy->parent = xyc;

		/* init queue of waiting bufs */

		xy->xyq.b_active = 0;
		xy->xyq.b_actf = 0;
		xy->xyq.b_actb = &xy->xyq.b_actf; /* XXX b_actb: not used? */

		xy->xyrq = &xyc->reqs[xa->driveno];

	}
	xy->xy_drive = xa->driveno;
	fmode = xa->fullmode;
	xyc->sc_drives[xa->driveno] = xy;

	/* if not booting, make sure we are the only process in the attach for
	 * this drive.   if locked out, sleep on it. */

	if (!xa->booting) {
		s = splbio();
		while (xy->state == XY_DRIVE_ATTACHING) {
			if (tsleep(&xy->state, PRIBIO, "xyattach", 0)) {
				splx(s);
				return;
			}
		}
		printf("%s at %s",
			xy->sc_dev.dv_xname, xy->parent->sc_dev.dv_xname);
	}
	/* we now have control */

	xy->state = XY_DRIVE_ATTACHING;
	newstate = XY_DRIVE_UNKNOWN;

	/* first try and reset the drive */

	err = xyc_cmd(xyc, XYCMD_RST, 0, xy->xy_drive, 0, 0, 0, fmode);
	XYC_DONE(xyc, err);
	if (err == XY_ERR_DNRY) {
		printf(" drive %d: off-line\n", xa->driveno);
		goto done;
	}
	if (err) {
		printf(": ERROR 0x%02x (%s)\n", err, xyc_e2str(err));
		goto done;
	}
	printf(" drive %d: ready", xa->driveno);

	/*
	 * now set drive parameters (to semi-bogus values) so we can read the
	 * disk label.
	 */
	xy->pcyl = xy->ncyl = 1;
	xy->acyl = 0;
	xy->nhead = 1;
	xy->nsect = 1;
	xy->sectpercyl = 1;
	for (lcv = 0; lcv < 126; lcv++)	/* init empty bad144 table */
		xy->dkb.bt_bad[lcv].bt_cyl =
			xy->dkb.bt_bad[lcv].bt_trksec = 0xffff;

	/* read disk label */
	for (xy->drive_type = 0 ; xy->drive_type <= XYC_MAXDT ;
						xy->drive_type++) {
		err = xyc_cmd(xyc, XYCMD_RD, 0, xy->xy_drive, 0, 1,
						xa->dvmabuf, fmode);
		XYC_DONE(xyc, err);
		if (err == XY_ERR_AOK) break;
	}

	if (err != XY_ERR_AOK) {
		printf("\n%s: reading disk label failed: %s\n",
			xy->sc_dev.dv_xname, xyc_e2str(err));
		goto done;
	}
	printf(" (drive type %d)\n", xy->drive_type);

	newstate = XY_DRIVE_NOLABEL;

	xy->hw_spt = spt = 0; /* XXX needed ? */
	/* Attach the disk: must be before getdisklabel to malloc label */
	disk_attach(&xy->sc_dk);

	if (xygetdisklabel(xy, xa->buf) != XY_ERR_AOK)
		goto done;

	/* inform the user of what is up */
	printf("%s: <%s>, pcyl %d\n", xy->sc_dev.dv_xname,
		xa->buf, xy->pcyl);
	mb = xy->ncyl * (xy->nhead * xy->nsect) / (1048576 / XYFM_BPS);
	printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec\n",
		xy->sc_dev.dv_xname, mb, xy->ncyl, xy->nhead, xy->nsect,
		XYFM_BPS);

	/*
	 * 450/451 stupidity: the drive type is encoded into the format
	 * of the disk.   the drive type in the IOPB must match the drive
	 * type in the format, or you will not be able to do I/O to the
	 * disk (you get header not found errors).  if you have two drives
	 * of different sizes that have the same drive type in their
	 * formatting then you are out of luck.
	 *
	 * this problem was corrected in the 753/7053.
	 */

	for (lcv = 0 ; lcv < XYC_MAXDEV ; lcv++) {
		oxy = xyc->sc_drives[lcv];
		if (oxy == NULL || oxy == xy) continue;
		if (oxy->drive_type != xy->drive_type) continue;
		if (xy->nsect != oxy->nsect || xy->pcyl != oxy->pcyl ||
			xy->nhead != oxy->nhead) {
			printf("%s: %s and %s must be the same size!\n",
				xyc->sc_dev.dv_xname, xy->sc_dev.dv_xname,
				oxy->sc_dev.dv_xname);
			panic("xy drive size mismatch");
		}
	}


	/* now set the real drive parameters! */

	blk = (xy->nsect - 1) +
		((xy->nhead - 1) * xy->nsect) +
		((xy->pcyl - 1) * xy->nsect * xy->nhead);
	err = xyc_cmd(xyc, XYCMD_SDS, 0, xy->xy_drive, blk, 0, 0, fmode);
	XYC_DONE(xyc, err);
	if (err) {
		printf("%s: write drive size failed: %s\n",
			xy->sc_dev.dv_xname, xyc_e2str(err));
		goto done;
	}
	newstate = XY_DRIVE_ONLINE;

	/*
	 * read bad144 table. this table resides on the first sector of the
	 * last track of the disk (i.e. second cyl of "acyl" area).
	 */

	blk = (xy->ncyl + xy->acyl - 1) * (xy->nhead * xy->nsect) +
								/* last cyl */
	    (xy->nhead - 1) * xy->nsect;	/* last head */
	err = xyc_cmd(xyc, XYCMD_RD, 0, xy->xy_drive, blk, 1,
						xa->dvmabuf, fmode);
	XYC_DONE(xyc, err);
	if (err) {
		printf("%s: reading bad144 failed: %s\n",
			xy->sc_dev.dv_xname, xyc_e2str(err));
		goto done;
	}

	/* check dkbad for sanity */
	dkb = (struct dkbad *) xa->buf;
	for (lcv = 0; lcv < 126; lcv++) {
		if ((dkb->bt_bad[lcv].bt_cyl == 0xffff ||
				dkb->bt_bad[lcv].bt_cyl == 0) &&
		     dkb->bt_bad[lcv].bt_trksec == 0xffff)
			continue;	/* blank */
		if (dkb->bt_bad[lcv].bt_cyl >= xy->ncyl)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec >> 8) >= xy->nhead)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec & 0xff) >= xy->nsect)
			break;
	}
	if (lcv != 126) {
		printf("%s: warning: invalid bad144 sector!\n",
			xy->sc_dev.dv_xname);
	} else {
		bcopy(xa->buf, &xy->dkb, XYFM_BPS);
	}

	if (xa->booting) {
		/* restore bootpath! (do this via attach_args again?)*/
		bp = bootpath_store(0, NULL);
		if (bp && strcmp("xy", bp->name) == 0 &&
					xy->xy_drive == bp->val[0])
			bp->dev = &xy->sc_dev;
	}

	dk_establish(&xy->sc_dk, &xy->sc_dev);		/* XXX */

done:
	xy->state = newstate;
	if (!xa->booting) {
		wakeup(&xy->state);
		splx(s);
	}
}

/*
 * end of autoconfig functions
 */

/*
 * { b , c } d e v s w   f u n c t i o n s
 */

/*
 * xyclose: close device
 */
int
xyclose(dev, flag, fmt, p)
	dev_t   dev;
	int     flag, fmt;
	struct proc *p;

{
	struct xy_softc *xy = xy_cd.cd_devs[DISKUNIT(dev)];
	int     part = DISKPART(dev);

	/* clear mask bits */

	switch (fmt) {
	case S_IFCHR:
		xy->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		xy->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	xy->sc_dk.dk_openmask = xy->sc_dk.dk_copenmask | xy->sc_dk.dk_bopenmask;

	return 0;
}

/*
 * xydump: crash dump system
 */
int
xydump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	int     unit, part;
	struct xy_softc *xy;

	unit = DISKUNIT(dev);
	if (unit >= xy_cd.cd_ndevs)
		return ENXIO;
	part = DISKPART(dev);

	xy = xy_cd.cd_devs[unit];

	printf("%s%c: crash dump not supported (yet)\n", xy->sc_dev.dv_xname,
	    'a' + part);

	return ENXIO;

	/* outline: globals: "dumplo" == sector number of partition to start
	 * dump at (convert to physical sector with partition table)
	 * "dumpsize" == size of dump in clicks "physmem" == size of physical
	 * memory (clicks, ctob() to get bytes) (normal case: dumpsize ==
	 * physmem)
	 *
	 * dump a copy of physical memory to the dump device starting at sector
	 * "dumplo" in the swap partition (make sure > 0).   map in pages as
	 * we go.   use polled I/O.
	 *
	 * XXX how to handle NON_CONTIG? */

}

/*
 * xyioctl: ioctls on XY drives.   based on ioctl's of other netbsd disks.
 */
int
xyioctl(dev, command, addr, flag, p)
	dev_t   dev;
	u_long  command;
	caddr_t addr;
	int     flag;
	struct proc *p;

{
	struct xy_softc *xy;
	struct xd_iocmd *xio;
	int     error, s, unit;

	unit = DISKUNIT(dev);

	if (unit >= xy_cd.cd_ndevs || (xy = xy_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	/* switch on ioctl type */

	switch (command) {
	case DIOCSBAD:		/* set bad144 info */
		if ((flag & FWRITE) == 0)
			return EBADF;
		s = splbio();
		bcopy(addr, &xy->dkb, sizeof(xy->dkb));
		splx(s);
		return 0;

	case DIOCGDINFO:	/* get disk label */
		bcopy(xy->sc_dk.dk_label, addr, sizeof(struct disklabel));
		return 0;

	case DIOCGPART:	/* get partition info */
		((struct partinfo *) addr)->disklab = xy->sc_dk.dk_label;
		((struct partinfo *) addr)->part =
		    &xy->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return 0;

	case DIOCSDINFO:	/* set disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(xy->sc_dk.dk_label,
		    (struct disklabel *) addr, /* xy->sc_dk.dk_openmask : */ 0,
		    xy->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (xy->state == XY_DRIVE_NOLABEL)
				xy->state = XY_DRIVE_ONLINE;
		}
		return error;

	case DIOCWLABEL:	/* change write status of disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *) addr)
			xy->flags |= XY_WLABEL;
		else
			xy->flags &= ~XY_WLABEL;
		return 0;

	case DIOCWDINFO:	/* write disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(xy->sc_dk.dk_label,
		    (struct disklabel *) addr, /* xy->sc_dk.dk_openmask : */ 0,
		    xy->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (xy->state == XY_DRIVE_NOLABEL)
				xy->state = XY_DRIVE_ONLINE;

			/* Simulate opening partition 0 so write succeeds. */
			xy->sc_dk.dk_openmask |= (1 << 0);
			error = writedisklabel(MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART),
			    xystrategy, xy->sc_dk.dk_label,
			    xy->sc_dk.dk_cpulabel);
			xy->sc_dk.dk_openmask =
			    xy->sc_dk.dk_copenmask | xy->sc_dk.dk_bopenmask;
		}
		return error;

	case DIOSXDCMD:
		xio = (struct xd_iocmd *) addr;
		if ((error = suser(p, 0)) != 0)
			return (error);
		return (xyc_ioctlcmd(xy, dev, xio));

	default:
		return ENOTTY;
	}
}

/*
 * xyopen: open drive
 */

int
xyopen(dev, flag, fmt, p)
	dev_t   dev;
	int     flag, fmt;
	struct proc *p;
{
	int     unit, part;
	struct xy_softc *xy;
	struct xyc_attach_args xa;

	/* first, could it be a valid target? */

	unit = DISKUNIT(dev);
	if (unit >= xy_cd.cd_ndevs || (xy = xy_cd.cd_devs[unit]) == NULL)
		return (ENXIO);
	part = DISKPART(dev);

	/* do we need to attach the drive? */

	if (xy->state == XY_DRIVE_UNKNOWN) {
		xa.driveno = xy->xy_drive;
		xa.dvmabuf = (char *)dvma_malloc(XYFM_BPS, &xa.buf, M_NOWAIT);
		xa.fullmode = XY_SUB_WAIT;
		xa.booting = 0;
		xyattach((struct device *) xy->parent,
						(struct device *) xy, &xa);
		dvma_free(xa.dvmabuf, XYFM_BPS, &xa.buf);
		if (xy->state == XY_DRIVE_UNKNOWN) {
			return (EIO);
		}
	}
	/* check for partition */

	if (part != RAW_PART &&
	    (part >= xy->sc_dk.dk_label->d_npartitions ||
		xy->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		return (ENXIO);
	}
	/* set open masks */

	switch (fmt) {
	case S_IFCHR:
		xy->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		xy->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	xy->sc_dk.dk_openmask = xy->sc_dk.dk_copenmask | xy->sc_dk.dk_bopenmask;

	return 0;
}

int
xyread(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int flags;
{

	return (physio(xystrategy, NULL, dev, B_READ, minphys, uio));
}

int
xywrite(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int flags;
{

	return (physio(xystrategy, NULL, dev, B_WRITE, minphys, uio));
}


/*
 * xysize: return size of a partition for a dump
 */

int
xysize(dev)
	dev_t   dev;

{
	struct xy_softc *xysc;
	int     unit, part, size, omask;

	/* valid unit? */
	unit = DISKUNIT(dev);
	if (unit >= xy_cd.cd_ndevs || (xysc = xy_cd.cd_devs[unit]) == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = xysc->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && xyopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);

	/* do it */
	if (xysc->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;	/* only give valid size for swap partitions */
	else
		size = xysc->sc_dk.dk_label->d_partitions[part].p_size *
		    (xysc->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && xyclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}

/*
 * xystrategy: buffering system interface to xy.
 */

void
xystrategy(bp)
	struct buf *bp;

{
	struct xy_softc *xy;
	int     s, unit;
	struct xyc_attach_args xa;

	unit = DISKUNIT(bp->b_dev);

	/* check for live device */

	if (unit >= xy_cd.cd_ndevs || (xy = xy_cd.cd_devs[unit]) == 0 ||
	    bp->b_blkno < 0 ||
	    (bp->b_bcount % xy->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/* do we need to attach the drive? */

	if (xy->state == XY_DRIVE_UNKNOWN) {
		xa.driveno = xy->xy_drive;
		xa.dvmabuf = (char *)dvma_malloc(XYFM_BPS, &xa.buf, M_NOWAIT);
		xa.fullmode = XY_SUB_WAIT;
		xa.booting = 0;
		xyattach((struct device *)xy->parent, (struct device *)xy, &xa);
		dvma_free(xa.dvmabuf, XYFM_BPS, &xa.buf);
		if (xy->state == XY_DRIVE_UNKNOWN) {
			bp->b_error = EIO;
			goto bad;
		}
	}
	if (xy->state != XY_DRIVE_ONLINE && DISKPART(bp->b_dev) != RAW_PART) {
		/* no I/O to unlabeled disks, unless raw partition */
		bp->b_error = EIO;
		goto bad;
	}
	/* short circuit zero length request */

	if (bp->b_bcount == 0)
		goto done;

	/* check bounds with label (disksubr.c).  Determine the size of the
	 * transfer, and make sure it is within the boundaries of the
	 * partition. Adjust transfer if needed, and signal errors or early
	 * completion. */

	if (bounds_check_with_label(bp, xy->sc_dk.dk_label,
	    xy->sc_dk.dk_cpulabel, (xy->flags & XY_WLABEL) != 0) <= 0)
		goto done;

	/*
	 * now we know we have a valid buf structure that we need to do I/O
	 * on.
	 */
	s = splbio();		/* protect the queues */

	disksort(&xy->xyq, bp);

	/* start 'em up */

	xyc_start(xy->parent, NULL);

	/* done! */

	splx(s);
	return;

bad:				/* tells upper layers we have an error */
	bp->b_flags |= B_ERROR;
done:				/* tells upper layers we are done with this
				 * buf */
	bp->b_resid = bp->b_bcount;
	s = splbio();
	biodone(bp);
	splx(s);
}
/*
 * end of {b,c}devsw functions
 */

/*
 * i n t e r r u p t   f u n c t i o n
 *
 * xycintr: hardware interrupt.
 */
int
xycintr(v)
	void   *v;

{
	struct xyc_softc *xycsc = v;

	/* remove as many done IOPBs as possible */

	xyc_remove_iorq(xycsc);

	/* start any iorq's already waiting */

	xyc_start(xycsc, NULL);

	return (1);
}
/*
 * end of interrupt function
 */

/*
 * i n t e r n a l   f u n c t i o n s
 */

/*
 * xyc_rqinit: fill out the fields of an I/O request
 */

inline void
xyc_rqinit(rq, xyc, xy, md, blk, cnt, db, bp)
	struct xy_iorq *rq;
	struct xyc_softc *xyc;
	struct xy_softc *xy;
	int     md;
	u_long  blk;
	int     cnt;
	caddr_t db;
	struct buf *bp;
{
	rq->xyc = xyc;
	rq->xy = xy;
	rq->ttl = XYC_MAXTTL + 10;
	rq->mode = md;
	rq->tries = rq->errno = rq->lasterror = 0;
	rq->blockno = blk;
	rq->sectcnt = cnt;
	rq->dbuf = rq->dbufbase = db;
	rq->buf = bp;
}

/*
 * xyc_rqtopb: load up an IOPB based on an iorq
 */

void
xyc_rqtopb(iorq, iopb, cmd, subfun)
	struct xy_iorq *iorq;
	struct xy_iopb *iopb;
	int     cmd, subfun;

{
	u_long  block, dp;

	/* normal IOPB case, standard stuff */

	/* chain bit handled later */
	iopb->ien = (XY_STATE(iorq->mode) == XY_SUB_POLL) ? 0 : 1;
	iopb->com = cmd;
	iopb->errno = 0;
	iopb->errs = 0;
	iopb->done = 0;
	if (iorq->xy) {
		iopb->unit = iorq->xy->xy_drive;
		iopb->dt = iorq->xy->drive_type;
	} else {
		iopb->unit = 0;
		iopb->dt = 0;
	}
	block = iorq->blockno;
	if (iorq->xy == NULL || block == 0) {
		iopb->sect = iopb->head = iopb->cyl = 0;
	} else {
		iopb->sect = block % iorq->xy->nsect;
		block = block / iorq->xy->nsect;
		iopb->head = block % iorq->xy->nhead;
		block = block / iorq->xy->nhead;
		iopb->cyl = block;
	}
	iopb->scnt = iorq->sectcnt;
	dp = (u_long) iorq->dbuf - DVMA_BASE;
	if (iorq->dbuf == NULL) {
		iopb->dataa = 0;
		iopb->datar = 0;
	} else {
		iopb->dataa = (dp & 0xffff);
		iopb->datar = ((dp & 0xff0000) >> 16);
	}
	iopb->subfn = subfun;
}


/*
 * xyc_unbusy: wait for the xyc to go unbusy, or timeout.
 */

int
xyc_unbusy(xyc, del)

struct xyc *xyc;
int del;

{
	while (del-- > 0) {
		if ((xyc->xyc_csr & XYC_GBSY) == 0)
			break;
		DELAY(1);
	}
	return(del == 0 ? XY_ERR_FAIL : XY_ERR_AOK);
}

/*
 * xyc_cmd: front end for POLL'd and WAIT'd commands.  Returns 0 or error.
 * note that NORM requests are handled separately.
 */
int
xyc_cmd(xycsc, cmd, subfn, unit, block, scnt, dptr, fullmode)
	struct xyc_softc *xycsc;
	int     cmd, subfn, unit, block, scnt;
	char   *dptr;
	int     fullmode;

{
	int     submode = XY_STATE(fullmode);
	struct xy_iorq *iorq = xycsc->ciorq;
	struct xy_iopb *iopb = xycsc->ciopb;

	/*
	 * is someone else using the control iopq wait for it if we can
	 */
start:
	if (submode == XY_SUB_WAIT && XY_STATE(iorq->mode) != XY_SUB_FREE) {
		if (tsleep(iorq, PRIBIO, "xyc_cmd", 0))
                                return(XY_ERR_FAIL);
		goto start;
	}

	if (XY_STATE(iorq->mode) != XY_SUB_FREE) {
		DELAY(1000000);		/* XY_SUB_POLL: steal the iorq */
		iorq->mode = XY_SUB_FREE;
		printf("%s: stole control iopb\n", xycsc->sc_dev.dv_xname);
	}

	/* init iorq/iopb */

	xyc_rqinit(iorq, xycsc,
	    (unit == XYC_NOUNIT) ? NULL : xycsc->sc_drives[unit],
	    fullmode, block, scnt, dptr, NULL);

	/* load IOPB from iorq */

	xyc_rqtopb(iorq, iopb, cmd, subfn);

	/* submit it for processing */

	xyc_submit_iorq(xycsc, iorq, fullmode);	/* error code will be in iorq */

	return(XY_ERR_AOK);
}

/*
 * xyc_startbuf
 * start a buffer for running
 */

int
xyc_startbuf(xycsc, xysc, bp)
	struct xyc_softc *xycsc;
	struct xy_softc *xysc;
	struct buf *bp;

{
	int     partno;
	struct xy_iorq *iorq;
	struct xy_iopb *iopb;
	u_long  block;
	caddr_t dbuf;

	iorq = xysc->xyrq;
	iopb = iorq->iopb;

	/* get buf */

	if (bp == NULL)
		panic("xyc_startbuf null buf");

	partno = DISKPART(bp->b_dev);
#ifdef XYC_DEBUG
	printf("xyc_startbuf: %s%c: %s block %d\n", xysc->sc_dev.dv_xname,
	    'a' + partno, (bp->b_flags & B_READ) ? "read" : "write", bp->b_blkno);
	printf("xyc_startbuf: b_bcount %d, b_data 0x%x\n",
	    bp->b_bcount, bp->b_data);
#endif

	/*
	 * load request.  we have to calculate the correct block number based
	 * on partition info.
	 *
	 * note that iorq points to the buffer as mapped into DVMA space,
	 * where as the bp->b_data points to its non-DVMA mapping.
	 */

	block = bp->b_blkno + ((partno == RAW_PART) ? 0 :
	    xysc->sc_dk.dk_label->d_partitions[partno].p_offset);

	dbuf = kdvma_mapin(bp->b_data, bp->b_bcount, 0);
	if (dbuf == NULL) {	/* out of DVMA space */
		printf("%s: warning: out of DVMA space\n",
			xycsc->sc_dev.dv_xname);
		return (XY_ERR_FAIL);	/* XXX: need some sort of
					 * call-back scheme here? */
	}

	/* init iorq and load iopb from it */

	xyc_rqinit(iorq, xycsc, xysc, XY_SUB_NORM | XY_MODE_VERBO, block,
	    bp->b_bcount / XYFM_BPS, dbuf, bp);

	xyc_rqtopb(iorq, iopb, (bp->b_flags & B_READ) ? XYCMD_RD : XYCMD_WR, 0);

	/* Instrumentation. */
	disk_busy(&xysc->sc_dk);

	return (XY_ERR_AOK);
}


/*
 * xyc_submit_iorq: submit an iorq for processing.  returns XY_ERR_AOK
 * if ok.  if it fail returns an error code.  type is XY_SUB_*.
 *
 * note: caller frees iorq in all cases except NORM
 *
 * return value:
 *   NORM: XY_AOK (req pending), XY_FAIL (couldn't submit request)
 *   WAIT: XY_AOK (success), <error-code> (failed)
 *   POLL: <same as WAIT>
 *   NOQ : <same as NORM>
 *
 * there are three sources for i/o requests:
 * [1] xystrategy: normal block I/O, using "struct buf" system.
 * [2] autoconfig/crash dump: these are polled I/O requests, no interrupts.
 * [3] open/ioctl: these are I/O requests done in the context of a process,
 *                 and the process should block until they are done.
 *
 * software state is stored in the iorq structure.  each iorq has an
 * iopb structure.  the hardware understands the iopb structure.
 * every command must go through an iopb.  a 450 handles one iopb at a
 * time, where as a 451 can take them in chains.  [the 450 claims it
 * can handle chains, but is appears to be buggy...]   iopb are allocated
 * in DVMA space at boot up time.  each disk gets one iopb, and the
 * controller gets one (for POLL and WAIT commands).  what happens if
 * the iopb is busy?  for i/o type [1], the buffers are queued at the
 * "buff" layer and * picked up later by the interrupt routine.  for case
 * [2] we can only be blocked if there is a WAIT type I/O request being
 * run.   since this can only happen when we are crashing, we wait a sec
 * and then steal the IOPB.  for case [3] the process can sleep
 * on the iorq free list until some iopbs are available.
 */


int
xyc_submit_iorq(xycsc, iorq, type)
	struct xyc_softc *xycsc;
	struct xy_iorq *iorq;
	int     type;

{
	struct xy_iopb *iopb;
	u_long  iopbaddr;

#ifdef XYC_DEBUG
	printf("xyc_submit_iorq(%s, addr=0x%x, type=%d)\n",
		xycsc->sc_dev.dv_xname, iorq, type);
#endif

	/* first check and see if controller is busy */
	if ((xycsc->xyc->xyc_csr & XYC_GBSY) != 0) {
#ifdef XYC_DEBUG
		printf("xyc_submit_iorq: XYC not ready (BUSY)\n");
#endif
		if (type == XY_SUB_NOQ)
			return (XY_ERR_FAIL);	/* failed */
		switch (type) {
		case XY_SUB_NORM:
			return XY_ERR_AOK;	/* success */
		case XY_SUB_WAIT:
			while (iorq->iopb->done == 0) {
				tsleep(iorq, PRIBIO, "xyiorq", 0);
			}
			return (iorq->errno);
		case XY_SUB_POLL:		/* steal controller */
			iopbaddr = xycsc->xyc->xyc_rsetup; /* RESET */
			if (xyc_unbusy(xycsc->xyc,XYC_RESETUSEC) == XY_ERR_FAIL)
				panic("xyc_submit_iorq: stuck xyc");
			printf("%s: stole controller\n",
				xycsc->sc_dev.dv_xname);
			break;
		default:
			panic("xyc_submit_iorq adding");
		}
	}

	iopb = xyc_chain(xycsc, iorq);	 /* build chain */
	if (iopb == NULL) { /* nothing doing? */
		if (type == XY_SUB_NORM || type == XY_SUB_NOQ)
			return(XY_ERR_AOK);
		panic("xyc_submit_iorq: xyc_chain failed!");
	}
	iopbaddr = (u_long) iopb - DVMA_BASE;

	XYC_GO(xycsc->xyc, iopbaddr);

	/* command now running, wrap it up */
	switch (type) {
	case XY_SUB_NORM:
	case XY_SUB_NOQ:
		return (XY_ERR_AOK);	/* success */
	case XY_SUB_WAIT:
		while (iorq->iopb->done == 0) {
			tsleep(iorq, PRIBIO, "xyiorq", 0);
		}
		return (iorq->errno);
	case XY_SUB_POLL:
		return (xyc_piodriver(xycsc, iorq));
	default:
		panic("xyc_submit_iorq wrap up");
	}
	panic("xyc_submit_iorq");
	return 0;	/* not reached */
}


/*
 * xyc_chain: build a chain.  return dvma address of first element in
 * the chain.   iorq != NULL: means we only want that item on the chain.
 */

struct xy_iopb *
xyc_chain(xycsc, iorq)

struct xyc_softc *xycsc;
struct xy_iorq *iorq;

{
  int togo, chain, hand;
  struct xy_iopb *iopb, *prev_iopb;
  bzero(xycsc->xy_chain, sizeof(xycsc->xy_chain));

  /*
   * promote control IOPB to the top
   */
  if (iorq == NULL) {
    if ((XY_STATE(xycsc->reqs[XYC_CTLIOPB].mode) == XY_SUB_POLL ||
        XY_STATE(xycsc->reqs[XYC_CTLIOPB].mode) == XY_SUB_WAIT) &&
		xycsc->iopbase[XYC_CTLIOPB].done == 0)
    iorq = &xycsc->reqs[XYC_CTLIOPB];
  }
  /*
   * special case: if iorq != NULL then we have a POLL or WAIT request.
   * we let these take priority and do them first.
   */
  if (iorq) {
    xycsc->xy_chain[0] = iorq;
    iorq->iopb->chen = 0;
    return(iorq->iopb);
  }

  /*
   * NORM case: do round robin and maybe chain (if allowed and possible)
   */

  chain = 0;
  hand = xycsc->xy_hand;
  xycsc->xy_hand = (xycsc->xy_hand + 1) % XYC_MAXIOPB;

  for (togo = XYC_MAXIOPB ; togo > 0 ; togo--, hand = (hand + 1) % XYC_MAXIOPB){

    if (XY_STATE(xycsc->reqs[hand].mode) != XY_SUB_NORM ||
		xycsc->iopbase[hand].done)
      continue;   /* not ready-for-i/o */

    xycsc->xy_chain[chain] = &xycsc->reqs[hand];
    iopb = xycsc->xy_chain[chain]->iopb;
    iopb->chen = 0;
    if (chain != 0) {   /* adding a link to a chain? */
      prev_iopb = xycsc->xy_chain[chain-1]->iopb;
      prev_iopb->chen = 1;
      prev_iopb->nxtiopb = ((u_long) iopb - DVMA_BASE) & 0xffff;
    } else {            /* head of chain */
      iorq = xycsc->xy_chain[chain];
    }
    chain++;
    if (xycsc->no_ols) break;   /* quit if chaining dis-allowed */
  }
  return(iorq ? iorq->iopb : NULL);
}

/*
 * xyc_piodriver
 *
 * programmed i/o driver.   this function takes over the computer
 * and drains off the polled i/o request.   it returns the status of the iorq
 * the caller is interesting in.
 */
int
xyc_piodriver(xycsc, iorq)
	struct xyc_softc *xycsc;
	struct xy_iorq  *iorq;

{
	int     nreset = 0;
	int     retval = 0;
	u_long  res;
#ifdef XYC_DEBUG
	printf("xyc_piodriver(%s, 0x%x)\n", xycsc->sc_dev.dv_xname, iorq);
#endif

	while (iorq->iopb->done == 0) {

		res = xyc_unbusy(xycsc->xyc, XYC_MAXTIME);

		/* we expect some progress soon */
		if (res == XY_ERR_FAIL && nreset >= 2) {
			xyc_reset(xycsc, 0, XY_RSET_ALL, XY_ERR_FAIL, 0);
#ifdef XYC_DEBUG
			printf("xyc_piodriver: timeout\n");
#endif
			return (XY_ERR_FAIL);
		}
		if (res == XY_ERR_FAIL) {
			if (xyc_reset(xycsc, 0,
				      (nreset++ == 0) ? XY_RSET_NONE : iorq,
				      XY_ERR_FAIL,
				      0) == XY_ERR_FAIL)
				return (XY_ERR_FAIL);	/* flushes all but POLL
							 * requests, resets */
			continue;
		}

		xyc_remove_iorq(xycsc);	 /* may resubmit request */

		if (iorq->iopb->done == 0)
			xyc_start(xycsc, iorq);
	}

	/* get return value */

	retval = iorq->errno;

#ifdef XYC_DEBUG
	printf("xyc_piodriver: done, retval = 0x%x (%s)\n",
	    iorq->errno, xyc_e2str(iorq->errno));
#endif

	/* start up any bufs that have queued */

	xyc_start(xycsc, NULL);

	return (retval);
}

/*
 * xyc_xyreset: reset one drive.   NOTE: assumes xyc was just reset.
 * we steal iopb[XYC_CTLIOPB] for this, but we put it back when we are done.
 */
void
xyc_xyreset(xycsc, xysc)
	struct xyc_softc *xycsc;
	struct xy_softc *xysc;

{
	struct xy_iopb tmpiopb;
	u_long  addr;
	int     del;
	bcopy(xycsc->ciopb, &tmpiopb, sizeof(tmpiopb));
	xycsc->ciopb->chen = xycsc->ciopb->done = xycsc->ciopb->errs = 0;
	xycsc->ciopb->ien = 0;
	xycsc->ciopb->com = XYCMD_RST;
	xycsc->ciopb->unit = xysc->xy_drive;
	addr = (u_long) xycsc->ciopb - DVMA_BASE;

	XYC_GO(xycsc->xyc, addr);

	del = XYC_RESETUSEC;
	while (del > 0) {
		if ((xycsc->xyc->xyc_csr & XYC_GBSY) == 0) break;
		DELAY(1);
		del--;
	}

	if (del <= 0 || xycsc->ciopb->errs) {
		printf("%s: off-line: %s\n", xycsc->sc_dev.dv_xname,
		    xyc_e2str(xycsc->ciopb->errno));
		del = xycsc->xyc->xyc_rsetup;
		if (xyc_unbusy(xycsc->xyc, XYC_RESETUSEC) == XY_ERR_FAIL)
			panic("xyc_reset");
	} else {
		xycsc->xyc->xyc_csr = XYC_IPND;	/* clear IPND */
	}
	bcopy(&tmpiopb, xycsc->ciopb, sizeof(tmpiopb));
}


/*
 * xyc_reset: reset everything: requests are marked as errors except
 * a polled request (which is resubmitted)
 */
int
xyc_reset(xycsc, quiet, blastmode, error, xysc)
	struct xyc_softc *xycsc;
	int     quiet, error;
	struct xy_iorq *blastmode;
	struct xy_softc *xysc;

{
	int     del = 0, lcv, retval = XY_ERR_AOK;

	/* soft reset hardware */

	if (!quiet)
		printf("%s: soft reset\n", xycsc->sc_dev.dv_xname);
	del = xycsc->xyc->xyc_rsetup;
	del = xyc_unbusy(xycsc->xyc, XYC_RESETUSEC);
	if (del == XY_ERR_FAIL) {
		blastmode = XY_RSET_ALL;	/* dead, flush all requests */
		retval = XY_ERR_FAIL;
	}
	if (xysc)
		xyc_xyreset(xycsc, xysc);

	/* fix queues based on "blast-mode" */

	for (lcv = 0; lcv < XYC_MAXIOPB; lcv++) {
		struct xy_iorq *iorq = &xycsc->reqs[lcv];

		if (XY_STATE(iorq->mode) != XY_SUB_POLL &&
		    XY_STATE(iorq->mode) != XY_SUB_WAIT &&
		    XY_STATE(iorq->mode) != XY_SUB_NORM)
			/* is it active? */
			continue;

		if (blastmode == XY_RSET_ALL ||
				blastmode != iorq) {
			/* failed */
			iorq->errno = error;
			xycsc->iopbase[lcv].done = xycsc->iopbase[lcv].errs = 1;
			switch (XY_STATE(iorq->mode)) {
			case XY_SUB_NORM:
			    iorq->buf->b_error = EIO;
			    iorq->buf->b_flags |= B_ERROR;
			    iorq->buf->b_resid = iorq->sectcnt * XYFM_BPS;
			    dvma_mapout((vaddr_t)iorq->dbufbase,
					(vaddr_t)iorq->buf->b_data,
					iorq->buf->b_bcount);
			    iorq->xy->xyq.b_actf = iorq->buf->b_actf;
			    disk_unbusy(&xycsc->reqs[lcv].xy->sc_dk,
				(xycsc->reqs[lcv].buf->b_bcount -
				xycsc->reqs[lcv].buf->b_resid),
				(xycsc->reqs[lcv].buf->b_flags & B_READ));
			    biodone(iorq->buf);
			    iorq->mode = XY_SUB_FREE;
			    break;
			case XY_SUB_WAIT:
			    wakeup(iorq);
			case XY_SUB_POLL:
			    iorq->mode =
				XY_NEWSTATE(iorq->mode, XY_SUB_DONE);
			    break;
			}

		} else {

			/* resubmit, no need to do anything here */
		}
	}

	/*
	 * now, if stuff is waiting, start it.
	 * since we just reset it should go
	 */
	xyc_start(xycsc, NULL);

	return (retval);
}

/*
 * xyc_start: start waiting buffers
 */

void
xyc_start(xycsc, iorq)
	struct xyc_softc *xycsc;
	struct xy_iorq *iorq;

{
	int lcv;
	struct xy_softc *xy;

	if (iorq == NULL) {
		for (lcv = 0; lcv < XYC_MAXDEV ; lcv++) {
			if ((xy = xycsc->sc_drives[lcv]) == NULL) continue;
			if (xy->xyq.b_actf == NULL) continue;
			if (xy->xyrq->mode != XY_SUB_FREE) continue;
			xyc_startbuf(xycsc, xy, xy->xyq.b_actf);
		}
	}
	xyc_submit_iorq(xycsc, iorq, XY_SUB_NOQ);
}

/*
 * xyc_remove_iorq: remove "done" IOPB's.
 */

int
xyc_remove_iorq(xycsc)
	struct xyc_softc *xycsc;

{
	int     errno, rq, comm, errs;
	struct xyc *xyc = xycsc->xyc;
	u_long  addr;
	struct xy_iopb *iopb;
	struct xy_iorq *iorq;
	struct buf *bp;

	if (xyc->xyc_csr & XYC_DERR) {
		/*
		 * DOUBLE ERROR: should never happen under normal use. This
		 * error is so bad, you can't even tell which IOPB is bad, so
		 * we dump them all.
		 */
		errno = XY_ERR_DERR;
		printf("%s: DOUBLE ERROR!\n", xycsc->sc_dev.dv_xname);
		if (xyc_reset(xycsc, 0, XY_RSET_ALL, errno, 0) != XY_ERR_AOK) {
			printf("%s: soft reset failed!\n",
				xycsc->sc_dev.dv_xname);
			panic("xyc_remove_iorq: controller DEAD");
		}
		return (XY_ERR_AOK);
	}

	/*
	 * get iopb that is done, loop down the chain
	 */

	if (xyc->xyc_csr & XYC_ERR) {
		xyc->xyc_csr = XYC_ERR; /* clear error condition */
	}
	if (xyc->xyc_csr & XYC_IPND) {
		xyc->xyc_csr = XYC_IPND; /* clear interrupt */
	}

	for (rq = 0; rq < XYC_MAXIOPB; rq++) {
		iorq = xycsc->xy_chain[rq];
		if (iorq == NULL) break; /* done ! */
		if (iorq->mode == 0 || XY_STATE(iorq->mode) == XY_SUB_DONE)
			continue;	/* free, or done */
		iopb = iorq->iopb;
		if (iopb->done == 0)
			continue;	/* not done yet */

		comm = iopb->com;
		errs = iopb->errs;

		if (errs)
			iorq->errno = iopb->errno;
		else
			iorq->errno = 0;

		/* handle non-fatal errors */

		if (errs &&
		    xyc_error(xycsc, iorq, iopb, comm) == XY_ERR_AOK)
			continue;	/* AOK: we resubmitted it */


		/* this iorq is now done (hasn't been restarted or anything) */

		if ((iorq->mode & XY_MODE_VERBO) && iorq->lasterror)
			xyc_perror(iorq, iopb, 0);

		/* now, if read/write check to make sure we got all the data
		 * we needed. (this may not be the case if we got an error in
		 * the middle of a multisector request).   */

		if ((iorq->mode & XY_MODE_B144) != 0 && errs == 0 &&
		    (comm == XYCMD_RD || comm == XYCMD_WR)) {
			/* we just successfully processed a bad144 sector
			 * note: if we are in bad 144 mode, the pointers have
			 * been advanced already (see above) and are pointing
			 * at the bad144 sector.   to exit bad144 mode, we
			 * must advance the pointers 1 sector and issue a new
			 * request if there are still sectors left to process
			 *
			 */
			XYC_ADVANCE(iorq, 1);	/* advance 1 sector */

			/* exit b144 mode */
			iorq->mode = iorq->mode & (~XY_MODE_B144);

			if (iorq->sectcnt) {	/* more to go! */
				iorq->lasterror = iorq->errno = iopb->errno = 0;
				iopb->errs = iopb->done = 0;
				iorq->tries = 0;
				iopb->scnt = iorq->sectcnt;
				iopb->cyl = iorq->blockno /
						iorq->xy->sectpercyl;
				iopb->head =
					(iorq->blockno / iorq->xy->nhead) %
						iorq->xy->nhead;
				iopb->sect = iorq->blockno % XYFM_BPS;
				addr = (u_long) iorq->dbuf - DVMA_BASE;
				iopb->dataa = (addr & 0xffff);
				iopb->datar = ((addr & 0xff0000) >> 16);
				/* will resubit at end */
				continue;
			}
		}
		/* final cleanup, totally done with this request */

		switch (XY_STATE(iorq->mode)) {
		case XY_SUB_NORM:
			bp = iorq->buf;
			if (errs) {
				bp->b_error = EIO;
				bp->b_flags |= B_ERROR;
				bp->b_resid = iorq->sectcnt * XYFM_BPS;
			} else {
				bp->b_resid = 0;	/* done */
			}
			dvma_mapout((vaddr_t) iorq->dbufbase,
				    (vaddr_t) bp->b_data,
				    bp->b_bcount);
			iorq->xy->xyq.b_actf = bp->b_actf;
			disk_unbusy(&iorq->xy->sc_dk,
			    (bp->b_bcount - bp->b_resid),
			    (bp->b_flags & B_READ));
			iorq->mode = XY_SUB_FREE;
			biodone(bp);
			break;
		case XY_SUB_WAIT:
			iorq->mode = XY_NEWSTATE(iorq->mode, XY_SUB_DONE);
			wakeup(iorq);
			break;
		case XY_SUB_POLL:
			iorq->mode = XY_NEWSTATE(iorq->mode, XY_SUB_DONE);
			break;
		}
	}

	return (XY_ERR_AOK);
}

/*
 * xyc_perror: print error.
 * - if still_trying is true: we got an error, retried and got a
 *   different error.  in that case lasterror is the old error,
 *   and errno is the new one.
 * - if still_trying is not true, then if we ever had an error it
 *   is in lasterror. also, if iorq->errno == 0, then we recovered
 *   from that error (otherwise iorq->errno == iorq->lasterror).
 */
void
xyc_perror(iorq, iopb, still_trying)
	struct xy_iorq *iorq;
	struct xy_iopb *iopb;
	int     still_trying;

{

	int     error = iorq->lasterror;

	printf("%s", (iorq->xy) ? iorq->xy->sc_dev.dv_xname
	    : iorq->xyc->sc_dev.dv_xname);
	if (iorq->buf)
		printf("%c: ", 'a' + DISKPART(iorq->buf->b_dev));
	if (iopb->com == XYCMD_RD || iopb->com == XYCMD_WR)
		printf("%s %d/%d/%d: ",
			(iopb->com == XYCMD_RD) ? "read" : "write",
			iopb->cyl, iopb->head, iopb->sect);
	printf("%s", xyc_e2str(error));

	if (still_trying)
		printf(" [still trying, new error=%s]", xyc_e2str(iorq->errno));
	else
		if (iorq->errno == 0)
			printf(" [recovered in %d tries]", iorq->tries);

	printf("\n");
}

/*
 * xyc_error: non-fatal error encountered... recover.
 * return AOK if resubmitted, return FAIL if this iopb is done
 */
int
xyc_error(xycsc, iorq, iopb, comm)
	struct xyc_softc *xycsc;
	struct xy_iorq *iorq;
	struct xy_iopb *iopb;
	int     comm;

{
	int     errno = iorq->errno;
	int     erract = xyc_entoact(errno);
	int     oldmode, advance, i;

	if (erract == XY_ERA_RSET) {	/* some errors require a reset */
		oldmode = iorq->mode;
		iorq->mode = XY_SUB_DONE | (~XY_SUB_MASK & oldmode);
		/* make xyc_start ignore us */
		xyc_reset(xycsc, 1, XY_RSET_NONE, errno, iorq->xy);
		iorq->mode = oldmode;
	}
	/* check for read/write to a sector in bad144 table if bad: redirect
	 * request to bad144 area */

	if ((comm == XYCMD_RD || comm == XYCMD_WR) &&
	    (iorq->mode & XY_MODE_B144) == 0) {
		advance = iorq->sectcnt - iopb->scnt;
		XYC_ADVANCE(iorq, advance);
		if ((i = isbad(&iorq->xy->dkb, iorq->blockno / iorq->xy->sectpercyl,
			    (iorq->blockno / iorq->xy->nsect) % iorq->xy->nhead,
			    iorq->blockno % iorq->xy->nsect)) != -1) {
			iorq->mode |= XY_MODE_B144;	/* enter bad144 mode &
							 * redirect */
			iopb->errno = iopb->done = iopb->errs = 0;
			iopb->scnt = 1;
			iopb->cyl = (iorq->xy->ncyl + iorq->xy->acyl) - 2;
			/* second to last acyl */
			i = iorq->xy->sectpercyl - 1 - i;	/* follow bad144
								 * standard */
			iopb->head = i / iorq->xy->nhead;
			iopb->sect = i % iorq->xy->nhead;
			/* will resubmit when we come out of remove_iorq */
			return (XY_ERR_AOK);	/* recovered! */
		}
	}

	/*
	 * it isn't a bad144 sector, must be real error! see if we can retry
	 * it?
	 */
	if ((iorq->mode & XY_MODE_VERBO) && iorq->lasterror)
		xyc_perror(iorq, iopb, 1);	/* inform of error state
						 * change */
	iorq->lasterror = errno;

	if ((erract == XY_ERA_RSET || erract == XY_ERA_HARD)
	    && iorq->tries < XYC_MAXTRIES) {	/* retry? */
		iorq->tries++;
		iorq->errno = iopb->errno = iopb->done = iopb->errs = 0;
		/* will resubmit at end of remove_iorq */
		return (XY_ERR_AOK);	/* recovered! */
	}

	/* failed to recover from this error */
	return (XY_ERR_FAIL);
}

/*
 * xyc_tick: make sure xy is still alive and ticking (err, kicking).
 */
void
xyc_tick(arg)
	void   *arg;

{
	struct xyc_softc *xycsc = arg;
	int     lcv, s, reset = 0;

	/* reduce ttl for each request if one goes to zero, reset xyc */
	s = splbio();
	for (lcv = 0; lcv < XYC_MAXIOPB; lcv++) {
		if (xycsc->reqs[lcv].mode == 0 ||
		    XY_STATE(xycsc->reqs[lcv].mode) == XY_SUB_DONE)
			continue;
		xycsc->reqs[lcv].ttl--;
		if (xycsc->reqs[lcv].ttl == 0)
			reset = 1;
	}
	if (reset) {
		printf("%s: watchdog timeout\n", xycsc->sc_dev.dv_xname);
		xyc_reset(xycsc, 0, XY_RSET_NONE, XY_ERR_FAIL, NULL);
	}
	splx(s);

	/* until next time */

	timeout_add(&xycsc->xyc_tick_tmo, XYC_TICKCNT);
}

/*
 * xyc_ioctlcmd: this function provides a user level interface to the
 * controller via ioctl.   this allows "format" programs to be written
 * in user code, and is also useful for some debugging.   we return
 * an error code.   called at user priority.
 *
 * XXX missing a few commands (see the 7053 driver for ideas)
 */
int
xyc_ioctlcmd(xy, dev, xio)
	struct xy_softc *xy;
	dev_t   dev;
	struct xd_iocmd *xio;

{
	int     s, err, rqno, dummy = 0;
	caddr_t dvmabuf = NULL, buf = NULL;
	struct xyc_softc *xycsc;

	/* check sanity of requested command */

	switch (xio->cmd) {

	case XYCMD_NOP:	/* no op: everything should be zero */
		if (xio->subfn || xio->dptr || xio->dlen ||
		    xio->block || xio->sectcnt)
			return (EINVAL);
		break;

	case XYCMD_RD:		/* read / write sectors (up to XD_IOCMD_MAXS) */
	case XYCMD_WR:
		if (xio->subfn || xio->sectcnt > XD_IOCMD_MAXS ||
		    xio->sectcnt * XYFM_BPS != xio->dlen || xio->dptr == NULL)
			return (EINVAL);
		break;

	case XYCMD_SK:		/* seek: doesn't seem useful to export this */
		return (EINVAL);

		break;

	default:
		return (EINVAL);/* ??? */
	}

	/* create DVMA buffer for request if needed */

	if (xio->dlen) {
		dvmabuf = dvma_malloc(xio->dlen, &buf, M_WAITOK);
		if (xio->cmd == XYCMD_WR) {
			if ((err = copyin(xio->dptr, buf, xio->dlen)) != 0) {
				dvma_free(dvmabuf, xio->dlen, &buf);
				return (err);
			}
		}
	}
	/* do it! */

	err = 0;
	xycsc = xy->parent;
	s = splbio();
	rqno = xyc_cmd(xycsc, xio->cmd, xio->subfn, xy->xy_drive, xio->block,
	    xio->sectcnt, dvmabuf, XY_SUB_WAIT);
	if (rqno == XY_ERR_FAIL) {
		err = EIO;
		goto done;
	}
	xio->errno = xycsc->ciorq->errno;
	xio->tries = xycsc->ciorq->tries;
	XYC_DONE(xycsc, dummy);

	if (xio->cmd == XYCMD_RD)
		err = copyout(buf, xio->dptr, xio->dlen);

done:
	splx(s);
	if (dvmabuf)
		dvma_free(dvmabuf, xio->dlen, &buf);
	return (err);
}

/*
 * xyc_e2str: convert error code number into an error string
 */
char *
xyc_e2str(no)
	int     no;
{
	switch (no) {
	case XY_ERR_FAIL:
		return ("Software fatal error");
	case XY_ERR_DERR:
		return ("DOUBLE ERROR");
	case XY_ERR_AOK:
		return ("Successful completion");
	case XY_ERR_IPEN:
		return("Interrupt pending");
	case XY_ERR_BCFL:
		return("Busy conflict");
	case XY_ERR_TIMO:
		return("Operation timeout");
	case XY_ERR_NHDR:
		return("Header not found");
	case XY_ERR_HARD:
		return("Hard ECC error");
	case XY_ERR_ICYL:
		return("Illegal cylinder address");
	case XY_ERR_ISEC:
		return("Illegal sector address");
	case XY_ERR_SMAL:
		return("Last sector too small");
	case XY_ERR_SACK:
		return("Slave ACK error (non-existent memory)");
	case XY_ERR_CHER:
		return("Cylinder and head/header error");
	case XY_ERR_SRTR:
		return("Auto-seek retry successful");
	case XY_ERR_WPRO:
		return("Write-protect error");
	case XY_ERR_UIMP:
		return("Unimplemented command");
	case XY_ERR_DNRY:
		return("Drive not ready");
	case XY_ERR_SZER:
		return("Sector count zero");
	case XY_ERR_DFLT:
		return("Drive faulted");
	case XY_ERR_ISSZ:
		return("Illegal sector size");
	case XY_ERR_SLTA:
		return("Self test A");
	case XY_ERR_SLTB:
		return("Self test B");
	case XY_ERR_SLTC:
		return("Self test C");
	case XY_ERR_SOFT:
		return("Soft ECC error");
	case XY_ERR_SFOK:
		return("Soft ECC error recovered");
	case XY_ERR_IHED:
		return("Illegal head");
	case XY_ERR_DSEQ:
		return("Disk sequencer error");
	case XY_ERR_SEEK:
		return("Seek error");
	default:
		return ("Unknown error");
	}
}

int
xyc_entoact(errno)

int errno;

{
  switch (errno) {
    case XY_ERR_FAIL:	case XY_ERR_DERR:	case XY_ERR_IPEN:
    case XY_ERR_BCFL:	case XY_ERR_ICYL:	case XY_ERR_ISEC:
    case XY_ERR_UIMP:	case XY_ERR_SZER:	case XY_ERR_ISSZ:
    case XY_ERR_SLTA:	case XY_ERR_SLTB:	case XY_ERR_SLTC:
    case XY_ERR_IHED:	case XY_ERR_SACK:	case XY_ERR_SMAL:

	return(XY_ERA_PROG); /* program error ! */

    case XY_ERR_TIMO:	case XY_ERR_NHDR:	case XY_ERR_HARD:
    case XY_ERR_DNRY:	case XY_ERR_CHER:	case XY_ERR_SEEK:
    case XY_ERR_SOFT:

	return(XY_ERA_HARD); /* hard error, retry */

    case XY_ERR_DFLT:	case XY_ERR_DSEQ:

	return(XY_ERA_RSET); /* hard error reset */

    case XY_ERR_SRTR:	case XY_ERR_SFOK:	case XY_ERR_AOK:

	return(XY_ERA_SOFT); /* an FYI error */

    case XY_ERR_WPRO:

	return(XY_ERA_WPRO); /* write protect */
  }

  return(XY_ERA_PROG); /* ??? */
}
