/*	$OpenBSD: xd.c,v 1.48 2010/05/23 10:49:19 dlg Exp $	*/
/*	$NetBSD: xd.c,v 1.37 1997/07/29 09:58:16 fair Exp $	*/

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
 * x d . c   x y l o g i c s   7 5 3 / 7 0 5 3   v m e / s m d   d r i v e r
 *
 * author: Chuck Cranor <chuck@ccrc.wustl.edu>
 * id: $NetBSD: xd.c,v 1.37 1997/07/29 09:58:16 fair Exp $
 * started: 27-Feb-95
 * references: [1] Xylogics Model 753 User's Manual
 *                 part number: 166-753-001, Revision B, May 21, 1988.
 *                 "Your Partner For Performance"
 *             [2] other NetBSD disk device drivers
 *
 * Special thanks go to Scott E. Campbell of Xylogics, Inc. for taking
 * the time to answer some of my questions about the 753/7053.
 *
 * note: the 753 and the 7053 are programmed the same way, but are
 * different sizes.   the 753 is a 6U VME card, while the 7053 is a 9U
 * VME card (found in many VME based suns).
 */

#undef XDC_DEBUG		/* full debug */
#define XDC_DIAG		/* extra sanity checks */
#if defined(DIAGNOSTIC) && !defined(XDC_DIAG)
#define XDC_DIAG		/* link in with master DIAG option */
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
#include <sys/dkio.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <dev/sun/disklabel.h>
#include <machine/conf.h>

#include <sparc/dev/xdreg.h>
#include <sparc/dev/xdvar.h>
#include <sparc/dev/xio.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>

/*
 * macros
 */

/*
 * XDC_TWAIT: add iorq "N" to tail of SC's wait queue
 */
#define XDC_TWAIT(SC, N) { \
	(SC)->waitq[(SC)->waitend] = (N); \
	(SC)->waitend = ((SC)->waitend + 1) % XDC_MAXIOPB; \
	(SC)->nwait++; \
}

/*
 * XDC_HWAIT: add iorq "N" to head of SC's wait queue
 */
#define XDC_HWAIT(SC, N) { \
	(SC)->waithead = ((SC)->waithead == 0) ? \
		(XDC_MAXIOPB - 1) : ((SC)->waithead - 1); \
	(SC)->waitq[(SC)->waithead] = (N); \
	(SC)->nwait++; \
}

/*
 * XDC_GET_WAITER: gets the first request waiting on the waitq
 * and removes it (so it can be submitted)
 */
#define XDC_GET_WAITER(XDCSC, RQ) { \
	(RQ) = (XDCSC)->waitq[(XDCSC)->waithead]; \
	(XDCSC)->waithead = ((XDCSC)->waithead + 1) % XDC_MAXIOPB; \
	xdcsc->nwait--; \
}

/*
 * XDC_FREE: add iorq "N" to SC's free list
 */
#define XDC_FREE(SC, N) { \
	(SC)->freereq[(SC)->nfree++] = (N); \
	(SC)->reqs[N].mode = 0; \
	if ((SC)->nfree == 1) wakeup(&(SC)->nfree); \
}


/*
 * XDC_RQALLOC: allocate an iorq off the free list (assume nfree > 0).
 */
#define XDC_RQALLOC(XDCSC) (XDCSC)->freereq[--((XDCSC)->nfree)]

/*
 * XDC_GO: start iopb ADDR (DVMA addr in a u_long) on XDC
 */
#define XDC_GO(XDC, ADDR) { \
	(XDC)->xdc_iopbaddr0 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr1 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr2 = ((ADDR) & 0xff); \
	(ADDR) = ((ADDR) >> 8); \
	(XDC)->xdc_iopbaddr3 = (ADDR); \
	(XDC)->xdc_iopbamod = XDC_ADDRMOD; \
	(XDC)->xdc_csr = XDC_ADDIOPB; /* go! */ \
}

/*
 * XDC_WAIT: wait for XDC's csr "BITS" to come on in "TIME".
 *   LCV is a counter.  If it goes to zero then we timed out.
 */
#define XDC_WAIT(XDC, LCV, TIME, BITS) { \
	(LCV) = (TIME); \
	while ((LCV) > 0) { \
		if ((XDC)->xdc_csr & (BITS)) break; \
		(LCV) = (LCV) - 1; \
		DELAY(1); \
	} \
}

/*
 * XDC_DONE: don't need IORQ, get error code and free (done after xdc_cmd)
 */
#define XDC_DONE(SC,RQ,ER) { \
	if ((RQ) == XD_ERR_FAIL) { \
		(ER) = (RQ); \
	} else { \
		if ((SC)->ndone-- == XDC_SUBWAITLIM) \
		wakeup(&(SC)->ndone); \
		(ER) = (SC)->reqs[RQ].errno; \
		XDC_FREE((SC), (RQ)); \
	} \
}

/*
 * XDC_ADVANCE: advance iorq's pointers by a number of sectors
 */
#define XDC_ADVANCE(IORQ, N) { \
	if (N) { \
		(IORQ)->sectcnt -= (N); \
		(IORQ)->blockno += (N); \
		(IORQ)->dbuf += ((N)*XDFM_BPS); \
	} \
}

/*
 * note - addresses you can sleep on:
 *   [1] & of xd_softc's "state" (waiting for a chance to attach a drive)
 *   [2] & of xdc_softc's "nfree" (waiting for a free iorq/iopb)
 *   [3] & of xdc_softc's "ndone" (waiting for number of done iorq/iopb's
 *                                 to drop below XDC_SUBWAITLIM)
 *   [4] & an iorq (waiting for an XD_SUB_WAIT iorq to finish)
 */


/*
 * function prototypes
 * "xdc_*" functions are internal, all others are external interfaces
 */

extern int pil_to_vme[];	/* from obio.c */

/* internals */
int	xdc_cmd(struct xdc_softc *, int, int, int, int, int, char *, int);
char   *xdc_e2str(int);
int	xdc_error(struct xdc_softc *, struct xd_iorq *,
		   struct xd_iopb *, int, int);
int	xdc_ioctlcmd(struct xd_softc *, dev_t dev, struct xd_iocmd *);
void	xdc_perror(struct xd_iorq *, struct xd_iopb *, int);
int	xdc_piodriver(struct xdc_softc *, int, int);
int	xdc_remove_iorq(struct xdc_softc *);
int	xdc_reset(struct xdc_softc *, int, int, int, struct xd_softc *);
inline void xdc_rqinit(struct xd_iorq *, struct xdc_softc *,
			    struct xd_softc *, int, u_long, int,
			    caddr_t, struct buf *);
void	xdc_rqtopb(struct xd_iorq *, struct xd_iopb *, int, int);
void	xdc_start(struct xdc_softc *, int);
int	xdc_startbuf(struct xdc_softc *, struct xd_softc *, struct buf *);
int	xdc_submit_iorq(struct xdc_softc *, int, int);
void	xdc_tick(void *);
void	xdc_xdreset(struct xdc_softc *, struct xd_softc *);

/* machine interrupt hook */
int	xdcintr(void *);

/* autoconf */
int	xdcmatch(struct device *, void *, void *);
void	xdcattach(struct device *, struct device *, void *);
int	xdmatch(struct device *, void *, void *);
void	xdattach(struct device *, struct device *, void *);

static	void xddummystrat(struct buf *);
int	xdgetdisklabel(struct xd_softc *, void *);

/*
 * cfdrivers: device driver interface to autoconfig
 */

struct cfattach xdc_ca = {
	sizeof(struct xdc_softc), xdcmatch, xdcattach
};


struct cfdriver xdc_cd = {
	NULL, "xdc", DV_DULL
};

struct cfattach xd_ca = {
	sizeof(struct xd_softc), xdmatch, xdattach
};

struct cfdriver xd_cd = {
	NULL, "xd", DV_DISK
};

struct xdc_attach_args {	/* this is the "aux" args to xdattach */
	int	driveno;	/* unit number */
	char	*buf;		/* scratch buffer for reading disk label */
	char	*dvmabuf;	/* DVMA address of above */
	int	fullmode;	/* submit mode */
	int	booting;	/* are we booting or not? */
};

/*
 * dkdriver
 */

struct dkdriver xddkdriver = {xdstrategy};

/*
 * start: disk label fix code (XXX)
 */

static void *xd_labeldata;

static void
xddummystrat(bp)
	struct buf *bp;
{
	if (bp->b_bcount != XDFM_BPS)
		panic("xddummystrat");
	bcopy(xd_labeldata, bp->b_data, XDFM_BPS);
	bp->b_flags |= B_DONE;
}

int
xdgetdisklabel(xd, b)
	struct xd_softc *xd;
	void *b;
{
	struct disklabel *lp = xd->sc_dk.dk_label;
	struct sun_disklabel *sl = b;
	int error;

	bzero(lp, sizeof(struct disklabel));
	/* Required parameters for readdisklabel() */
	lp->d_secsize = XDFM_BPS;
	if (sl->sl_magic == SUN_DKMAGIC) {
		lp->d_secpercyl = sl->sl_nsectors * sl->sl_ntracks;
		DL_SETDSIZE(lp, (daddr64_t)lp->d_secpercyl * sl->sl_ncylinders);
	} else {
		lp->d_secpercyl = 1;
	}

	/* We already have the label data in `b'; setup for dummy strategy */
	xd_labeldata = b;

	error = readdisklabel(MAKEDISKDEV(0, xd->sc_dev.dv_unit, RAW_PART),
	    xddummystrat, lp, 0);
	if (error)
		return (error);

	/* Ok, we have the label; fill in `pcyl' if there's SunOS magic */
	sl = b;
	if (sl->sl_magic == SUN_DKMAGIC)
		xd->pcyl = sl->sl_pcylinders;
	else {
		printf("%s: WARNING: no `pcyl' in disk label.\n",
			xd->sc_dev.dv_xname);
		xd->pcyl = lp->d_ncylinders +
			lp->d_acylinders;
		printf("%s: WARNING: guessing pcyl=%d (ncyl+acyl)\n",
		xd->sc_dev.dv_xname, xd->pcyl);
	}

	xd->ncyl = lp->d_ncylinders;
	xd->acyl = lp->d_acylinders;
	xd->nhead = lp->d_ntracks;
	xd->nsect = lp->d_nsectors;
	xd->sectpercyl = lp->d_secpercyl;
	return (0);
}

/*
 * end: disk label fix code (XXX)
 */

/*
 * a u t o c o n f i g   f u n c t i o n s
 */

/*
 * xdcmatch: determine if xdc is present or not.   we do a
 * soft reset to detect the xdc.
 */

int xdcmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct xdc *xdc;
	int     del = 0;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	switch (ca->ca_bustype) {
	case BUS_OBIO:
	case BUS_SBUS:
	case BUS_VME16:
	default:
		return (0);
	case BUS_VME32:
		xdc = (struct xdc *) ra->ra_vaddr;
		if (probeget((caddr_t) &xdc->xdc_csr, 1) == -1)
			return (0);
		xdc->xdc_csr = XDC_RESET;
		XDC_WAIT(xdc, del, XDC_RESETUSEC, XDC_RESET);
		if (del <= 0)
			return (0);
		return (1);
	}
}

/*
 * xdcattach: attach controller
 */
void
xdcattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct xdc_softc *xdc = (void *) self;
	struct confargs *ca = aux;
	struct xdc_attach_args xa;
	int     lcv, rqno, err, pri;
	struct xd_iopb_ctrl *ctl;

	/* get addressing and intr level stuff from autoconfig and load it
	 * into our xdc_softc. */

	ca->ca_ra.ra_vaddr = mapiodev(ca->ca_ra.ra_reg, 0, sizeof(struct xdc));

	xdc->xdc = (struct xdc *) ca->ca_ra.ra_vaddr;
	pri = ca->ca_ra.ra_intr[0].int_pri;
	xdc->ipl = pil_to_vme[pri];
	xdc->vector = ca->ca_ra.ra_intr[0].int_vec;
	printf(" pri %d", pri);

	for (lcv = 0; lcv < XDC_MAXDEV; lcv++)
		xdc->sc_drives[lcv] = (struct xd_softc *) 0;

	/* allocate and zero buffers
	 *
	 * note: we simplify the code by allocating the max number of iopbs and
	 * iorq's up front.   thus, we avoid linked lists and the costs
	 * associated with them in exchange for wasting a little memory. */

	xdc->dvmaiopb = (struct xd_iopb *)
	    dvma_malloc(XDC_MAXIOPB * sizeof(struct xd_iopb), &xdc->iopbase,
			M_NOWAIT);
	xdc->iopbase = xdc->dvmaiopb; /* XXX TMP HACK */
	bzero(xdc->iopbase, XDC_MAXIOPB * sizeof(struct xd_iopb));
	/* Setup device view of DVMA address */
	xdc->dvmaiopb = (struct xd_iopb *) ((u_long) xdc->iopbase - DVMA_BASE);

	xdc->reqs = malloc(XDC_MAXIOPB * sizeof(struct xd_iorq), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (xdc->reqs == NULL)
		panic("xdc malloc");

	/* init free list, iorq to iopb pointers, and non-zero fields in the
	 * iopb which never change. */

	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		xdc->reqs[lcv].iopb = &xdc->iopbase[lcv];
		xdc->freereq[lcv] = lcv;
		xdc->iopbase[lcv].fixd = 1;	/* always the same */
		xdc->iopbase[lcv].naddrmod = XDC_ADDRMOD; /* always the same */
		xdc->iopbase[lcv].intr_vec = xdc->vector; /* always the same */
	}
	xdc->nfree = XDC_MAXIOPB;
	xdc->nrun = 0;
	xdc->waithead = xdc->waitend = xdc->nwait = 0;
	xdc->ndone = 0;

	/* init queue of waiting bufs */

	xdc->sc_wq.b_active = 0;
	xdc->sc_wq.b_actf = 0;
	xdc->sc_wq.b_actb = &xdc->sc_wq.b_actf;

	/*
	 * section 7 of the manual tells us how to init the controller:
	 * - read controller parameters (6/0)
	 * - write controller parameters (5/0)
	 */

	/* read controller parameters and insure we have a 753/7053 */

	rqno = xdc_cmd(xdc, XDCMD_RDP, XDFUN_CTL, 0, 0, 0, 0, XD_SUB_POLL);
	if (rqno == XD_ERR_FAIL) {
		printf(": couldn't read controller params\n");
		return;		/* shouldn't ever happen */
	}
	ctl = (struct xd_iopb_ctrl *) & xdc->iopbase[rqno];
	if (ctl->ctype != XDCT_753) {
		if (xdc->reqs[rqno].errno)
			printf(": %s: ", xdc_e2str(xdc->reqs[rqno].errno));
		printf(": doesn't identify as a 753/7053\n");
		XDC_DONE(xdc, rqno, err);
		return;
	}
	printf(": Xylogics 753/7053, PROM=0x%x.%02x.%02x\n",
	    ctl->eprom_partno, ctl->eprom_lvl, ctl->eprom_rev);
	XDC_DONE(xdc, rqno, err);

	/* now write controller parameters (xdc_cmd sets all params for us) */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_CTL, 0, 0, 0, 0, XD_SUB_POLL);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: controller config error: %s\n",
			xdc->sc_dev.dv_xname, xdc_e2str(err));
		return;
	}
	/* link in interrupt with higher level software */

	xdc->sc_ih.ih_fun = xdcintr;
	xdc->sc_ih.ih_arg = xdc;
	vmeintr_establish(ca->ca_ra.ra_intr[0].int_vec,
	    ca->ca_ra.ra_intr[0].int_pri, &xdc->sc_ih, IPL_BIO,
	    self->dv_xname);

	/* now we must look for disks using autoconfig */
	xa.dvmabuf = (char *)dvma_malloc(XDFM_BPS, &xa.buf, M_NOWAIT);
	xa.fullmode = XD_SUB_POLL;
	xa.booting = 1;

	if (ca->ca_ra.ra_bp && ca->ca_ra.ra_bp->val[0] == -1 &&
	    ca->ca_ra.ra_bp->val[1] == xdc->sc_dev.dv_unit) {
		bootpath_store(1, ca->ca_ra.ra_bp + 1); /* advance bootpath */
	}

	for (xa.driveno = 0; xa.driveno < XDC_MAXDEV; xa.driveno++)
		(void) config_found(self, (void *) &xa, NULL);

	dvma_free(xa.dvmabuf, XDFM_BPS, &xa.buf);
	bootpath_store(1, NULL);

	/* start the watchdog clock */
	timeout_set(&xdc->xdc_tick_tmo, xdc_tick, xdc);
	timeout_add(&xdc->xdc_tick_tmo, XDC_TICKCNT);

}

/*
 * xdmatch: probe for disk.
 *
 * note: we almost always say disk is present.   this allows us to
 * spin up and configure a disk after the system is booted (we can
 * call xdattach!).
 */
int
xdmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct xdc_attach_args *xa = aux;

	/* looking for autoconf wildcard or exact match */

	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != xa->driveno)
		return 0;

	return 1;

}

/*
 * xdattach: attach a disk.   this can be called from autoconf and also
 * from xdopen/xdstrategy.
 */
void
xdattach(parent, self, aux)
	struct device *parent, *self;
	void   *aux;

{
	struct xd_softc *xd = (void *) self;
	struct xdc_softc *xdc = (void *) parent;
	struct xdc_attach_args *xa = aux;
	int     rqno, err, spt = 0, mb, blk, lcv, fmode, s = 0, newstate;
	struct xd_iopb_drive *driopb;
	struct dkbad *dkb;
	struct bootpath *bp;

	/*
	 * Always re-initialize the disk structure.  We want statistics
	 * to start with a clean slate.
	 */
	bzero(&xd->sc_dk, sizeof(xd->sc_dk));
	xd->sc_dk.dk_driver = &xddkdriver;
	xd->sc_dk.dk_name = xd->sc_dev.dv_xname;

	/* if booting, init the xd_softc */

	if (xa->booting) {
		xd->state = XD_DRIVE_UNKNOWN;	/* to start */
		xd->flags = 0;
		xd->parent = xdc;
	}
	xd->xd_drive = xa->driveno;
	fmode = xa->fullmode;
	xdc->sc_drives[xa->driveno] = xd;

	/* if not booting, make sure we are the only process in the attach for
	 * this drive.   if locked out, sleep on it. */

	if (!xa->booting) {
		s = splbio();
		while (xd->state == XD_DRIVE_ATTACHING) {
			if (tsleep(&xd->state, PRIBIO, "xdattach", 0)) {
				splx(s);
				return;
			}
		}
		printf("%s at %s",
			xd->sc_dev.dv_xname, xd->parent->sc_dev.dv_xname);
	}
	/* we now have control */

	xd->state = XD_DRIVE_ATTACHING;
	newstate = XD_DRIVE_UNKNOWN;

	/* first try and reset the drive */

	rqno = xdc_cmd(xdc, XDCMD_RST, 0, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err == XD_ERR_NRDY) {
		printf(" drive %d: off-line\n", xa->driveno);
		goto done;
	}
	if (err) {
		printf(": ERROR 0x%02x (%s)\n", err, xdc_e2str(err));
		goto done;
	}
	printf(" drive %d: ready\n", xa->driveno);

	/* now set format parameters */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_FMT, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: write format parameters failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}

	/* get drive parameters */
	rqno = xdc_cmd(xdc, XDCMD_RDP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	if (rqno != XD_ERR_FAIL) {
		driopb = (struct xd_iopb_drive *) & xdc->iopbase[rqno];
		spt = driopb->sectpertrk;
	}
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: read drive parameters failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}

	/*
	 * now set drive parameters (to semi-bogus values) so we can read the
	 * disk label.
	 */
	xd->pcyl = xd->ncyl = 1;
	xd->acyl = 0;
	xd->nhead = 1;
	xd->nsect = 1;
	xd->sectpercyl = 1;
	for (lcv = 0; lcv < NBT_BAD; lcv++)	/* init empty bad144 table */
		xd->dkb.bt_bad[lcv].bt_cyl = xd->dkb.bt_bad[lcv].bt_trksec = 0xffff;
	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: write drive parameters failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}

	/* read disk label */
	rqno = xdc_cmd(xdc, XDCMD_RD, 0, xd->xd_drive, 0, 1, xa->dvmabuf, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: reading disk label failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}
	newstate = XD_DRIVE_NOLABEL;

	xd->hw_spt = spt;
	/* Attach the disk: must be before getdisklabel to malloc label */
	disk_attach(&xd->sc_dk);

	if (xdgetdisklabel(xd, xa->buf) != XD_ERR_AOK)
		goto done;

	/* inform the user of what is up */
	printf("%s: <%s>, pcyl %d, hw_spt %d\n", xd->sc_dev.dv_xname,
		xa->buf, xd->pcyl, spt);
	mb = xd->ncyl * (xd->nhead * xd->nsect) / (1048576 / XDFM_BPS);
	printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec\n",
		xd->sc_dev.dv_xname, mb, xd->ncyl, xd->nhead, xd->nsect,
		XDFM_BPS);

	/* now set the real drive parameters! */

	rqno = xdc_cmd(xdc, XDCMD_WRP, XDFUN_DRV, xd->xd_drive, 0, 0, 0, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: write real drive parameters failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}
	newstate = XD_DRIVE_ONLINE;

	/*
	 * read bad144 table. this table resides on the first sector of the
	 * last track of the disk (i.e. second cyl of "acyl" area).
	 */

	blk = (xd->ncyl + xd->acyl - 1) * (xd->nhead * xd->nsect) + /* last cyl */
	    (xd->nhead - 1) * xd->nsect;	/* last head */
	rqno = xdc_cmd(xdc, XDCMD_RD, 0, xd->xd_drive, blk, 1, xa->dvmabuf, fmode);
	XDC_DONE(xdc, rqno, err);
	if (err) {
		printf("%s: reading bad144 failed: %s\n",
			xd->sc_dev.dv_xname, xdc_e2str(err));
		goto done;
	}

	/* check dkbad for sanity */
	dkb = (struct dkbad *) xa->buf;
	for (lcv = 0; lcv < NBT_BAD; lcv++) {
		if ((dkb->bt_bad[lcv].bt_cyl == 0xffff ||
				dkb->bt_bad[lcv].bt_cyl == 0) &&
		     dkb->bt_bad[lcv].bt_trksec == 0xffff)
			continue;	/* blank */
		if (dkb->bt_bad[lcv].bt_cyl >= xd->ncyl)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec >> 8) >= xd->nhead)
			break;
		if ((dkb->bt_bad[lcv].bt_trksec & 0xff) >= xd->nsect)
			break;
	}
	if (lcv != NBT_BAD) {
		printf("%s: warning: invalid bad144 sector!\n",
			xd->sc_dev.dv_xname);
	} else {
		bcopy(xa->buf, &xd->dkb, XDFM_BPS);
	}

	if (xa->booting) {
		/* restore bootpath! (do this via attach_args again?)*/
		bp = bootpath_store(0, NULL);
		if (bp && strcmp("xd", bp->name) == 0 &&
						xd->xd_drive == bp->val[0])
			bp->dev = &xd->sc_dev;
	}

done:
	xd->state = newstate;
	if (!xa->booting) {
		wakeup(&xd->state);
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
 * xdclose: close device
 */
int
xdclose(dev, flag, fmt, p)
	dev_t   dev;
	int     flag, fmt;
	struct proc *p;
{
	struct xd_softc *xd = xd_cd.cd_devs[DISKUNIT(dev)];
	int     part = DISKPART(dev);

	/* clear mask bits */

	switch (fmt) {
	case S_IFCHR:
		xd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		xd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	xd->sc_dk.dk_openmask = xd->sc_dk.dk_copenmask | xd->sc_dk.dk_bopenmask;

	return 0;
}

/*
 * xddump: crash dump system
 */
int
xddump(dev, blkno, va, size)
	dev_t dev;
	daddr64_t blkno;
	caddr_t va;
	size_t size;
{
	int     unit, part;
	struct xd_softc *xd;

	unit = DISKUNIT(dev);
	if (unit >= xd_cd.cd_ndevs)
		return ENXIO;
	part = DISKPART(dev);

	xd = xd_cd.cd_devs[unit];

	printf("%s%c: crash dump not supported (yet)\n", xd->sc_dev.dv_xname,
	    'a' + part);

	return ENXIO;

	/* outline: globals: "dumplo" == sector number of partition to start
	 * dump at (convert to physical sector with partition table)
	 * "dumpsize" == size of dump in clicks "physmem" == size of physical
	 * memory (clicks, ptoa() to get bytes) (normal case: dumpsize ==
	 * physmem)
	 *
	 * dump a copy of physical memory to the dump device starting at sector
	 * "dumplo" in the swap partition (make sure > 0).   map in pages as
	 * we go.   use polled I/O.
	 *
	 * XXX how to handle NON_CONTIG? */

}

/*
 * xdioctl: ioctls on XD drives.   based on ioctl's of other netbsd disks.
 */
int
xdioctl(dev, command, addr, flag, p)
	dev_t   dev;
	u_long  command;
	caddr_t addr;
	int     flag;
	struct proc *p;

{
	struct xd_softc *xd;
	struct xd_iocmd *xio;
	int     error, s, unit;

	unit = DISKUNIT(dev);

	if (unit >= xd_cd.cd_ndevs || (xd = xd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	/* switch on ioctl type */

	switch (command) {
	case DIOCSBAD:		/* set bad144 info */
		if ((flag & FWRITE) == 0)
			return EBADF;
		s = splbio();
		bcopy(addr, &xd->dkb, sizeof(xd->dkb));
		splx(s);
		return 0;

	case DIOCGDINFO:	/* get disk label */
		bcopy(xd->sc_dk.dk_label, addr, sizeof(struct disklabel));
		return 0;

	case DIOCGPART:	/* get partition info */
		((struct partinfo *) addr)->disklab = xd->sc_dk.dk_label;
		((struct partinfo *) addr)->part =
		    &xd->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return 0;

	case DIOCWLABEL:	/* change write status of disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *) addr)
			xd->flags |= XD_WLABEL;
		else
			xd->flags &= ~XD_WLABEL;
		return 0;

	case DIOCWDINFO:	/* write disk label */
	case DIOCSDINFO:	/* set disk label */
		if ((flag & FWRITE) == 0)
			return EBADF;
		error = setdisklabel(xd->sc_dk.dk_label,
		    (struct disklabel *) addr, /* xd->sc_dk.dk_openmask : */ 0);
		if (error == 0) {
			if (xd->state == XD_DRIVE_NOLABEL)
				xd->state = XD_DRIVE_ONLINE;

			if (command == DIOCWDINFO) {
				/*
				 * Simulate opening partition 0 so write
				 * succeeds.
				 */
				xd->sc_dk.dk_openmask |= (1 << 0);
				error = writedisklabel(DISKLABELDEV(dev),
				    xdstrategy, xd->sc_dk.dk_label);
				xd->sc_dk.dk_openmask = xd->sc_dk.dk_copenmask |
				    xd->sc_dk.dk_bopenmask;
			}
		}
		return error;

	case DIOSXDCMD:
		xio = (struct xd_iocmd *) addr;
		if ((error = suser(p, 0)) != 0)
			return (error);
		return (xdc_ioctlcmd(xd, dev, xio));

	default:
		return ENOTTY;
	}
}
/*
 * xdopen: open drive
 */

int
xdopen(dev, flag, fmt, p)
	dev_t   dev;
	int     flag, fmt;
	struct proc *p;
{
	int     unit, part;
	struct xd_softc *xd;
	struct xdc_attach_args xa;

	/* first, could it be a valid target? */

	unit = DISKUNIT(dev);
	if (unit >= xd_cd.cd_ndevs || (xd = xd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);
	part = DISKPART(dev);

	/* do we need to attach the drive? */

	if (xd->state == XD_DRIVE_UNKNOWN) {
		xa.driveno = xd->xd_drive;
		xa.dvmabuf = (char *)dvma_malloc(XDFM_BPS, &xa.buf, M_NOWAIT);
		xa.fullmode = XD_SUB_WAIT;
		xa.booting = 0;
		xdattach((struct device *) xd->parent, (struct device *) xd, &xa);
		dvma_free(xa.dvmabuf, XDFM_BPS, &xa.buf);
		if (xd->state == XD_DRIVE_UNKNOWN) {
			return (EIO);
		}
	}
	/* check for partition */

	if (part != RAW_PART &&
	    (part >= xd->sc_dk.dk_label->d_npartitions ||
		xd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		return (ENXIO);
	}
	/* set open masks */

	switch (fmt) {
	case S_IFCHR:
		xd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		xd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	xd->sc_dk.dk_openmask = xd->sc_dk.dk_copenmask | xd->sc_dk.dk_bopenmask;

	return 0;
}

int
xdread(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int flags;
{

	return (physio(xdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
xdwrite(dev, uio, flags)
	dev_t   dev;
	struct uio *uio;
	int flags;
{

	return (physio(xdstrategy, NULL, dev, B_WRITE, minphys, uio));
}


/*
 * xdsize: return size of a partition for a dump
 */

daddr64_t
xdsize(dev)
	dev_t   dev;

{
	struct xd_softc *xdsc;
	int     unit, part, size, omask;

	/* valid unit? */
	unit = DISKUNIT(dev);
	if (unit >= xd_cd.cd_ndevs || (xdsc = xd_cd.cd_devs[unit]) == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = xdsc->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && xdopen(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);

	/* do it */
	if (xdsc->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;	/* only give valid size for swap partitions */
	else
		size = DL_GETPSIZE(&xdsc->sc_dk.dk_label->d_partitions[part]) *
		    (xdsc->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && xdclose(dev, 0, S_IFBLK, NULL) != 0)
		return (-1);
	return (size);
}
/*
 * xdstrategy: buffering system interface to xd.
 */

void
xdstrategy(bp)
	struct buf *bp;

{
	struct xd_softc *xd;
	struct xdc_softc *parent;
	struct buf *wq;
	int     s, unit;
	struct xdc_attach_args xa;

	unit = DISKUNIT(bp->b_dev);

	/* check for live device */

	if (unit >= xd_cd.cd_ndevs || (xd = xd_cd.cd_devs[unit]) == 0 ||
	    bp->b_blkno < 0 ||
	    (bp->b_bcount % xd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/* do we need to attach the drive? */

	if (xd->state == XD_DRIVE_UNKNOWN) {
		xa.driveno = xd->xd_drive;
		xa.dvmabuf = (char *)dvma_malloc(XDFM_BPS, &xa.buf, M_NOWAIT);
		xa.fullmode = XD_SUB_WAIT;
		xa.booting = 0;
		xdattach((struct device *)xd->parent, (struct device *)xd, &xa);
		dvma_free(xa.dvmabuf, XDFM_BPS, &xa.buf);
		if (xd->state == XD_DRIVE_UNKNOWN) {
			bp->b_error = EIO;
			goto bad;
		}
	}
	if (xd->state != XD_DRIVE_ONLINE && DISKPART(bp->b_dev) != RAW_PART) {
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

	if (bounds_check_with_label(bp, xd->sc_dk.dk_label,
	    (xd->flags & XD_WLABEL) != 0) <= 0)
		goto done;

	/*
	 * now we know we have a valid buf structure that we need to do I/O
	 * on.
	 *
	 * note that we don't disksort because the controller has a sorting
	 * algorithm built into the hardware.
	 */

	s = splbio();		/* protect the queues */

	/* first, give jobs in front of us a chance */
	parent = xd->parent;
	while (parent->nfree > 0 && parent->sc_wq.b_actf)
		if (xdc_startbuf(parent, NULL, NULL) != XD_ERR_AOK)
			break;

	/* if there are no free iorq's, then we just queue and return. the
	 * buffs will get picked up later by xdcintr().
	 */

	if (parent->nfree == 0) {
		wq = &xd->parent->sc_wq;
		bp->b_actf = 0;
		bp->b_actb = wq->b_actb;
		*wq->b_actb = bp;
		wq->b_actb = &bp->b_actf;
		splx(s);
		return;
	}

	/* now we have free iopb's and we are at splbio... start 'em up */
	if (xdc_startbuf(parent, xd, bp) != XD_ERR_AOK) {
		splx(s);
		return;
	}

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
 * xdcintr: hardware interrupt.
 */
int
xdcintr(v)
	void   *v;

{
	struct xdc_softc *xdcsc = v;

	/* remove as many done IOPBs as possible */

	xdc_remove_iorq(xdcsc);

	/* start any iorq's already waiting */

	xdc_start(xdcsc, XDC_MAXIOPB);

	/* fill up any remaining iorq's with queue'd buffers */

	while (xdcsc->nfree > 0 && xdcsc->sc_wq.b_actf)
		if (xdc_startbuf(xdcsc, NULL, NULL) != XD_ERR_AOK)
			break;

	return (1);
}
/*
 * end of interrupt function
 */

/*
 * i n t e r n a l   f u n c t i o n s
 */

/*
 * xdc_rqinit: fill out the fields of an I/O request
 */

inline void
xdc_rqinit(rq, xdc, xd, md, blk, cnt, db, bp)
	struct xd_iorq *rq;
	struct xdc_softc *xdc;
	struct xd_softc *xd;
	int     md;
	u_long  blk;
	int     cnt;
	caddr_t db;
	struct buf *bp;
{
	rq->xdc = xdc;
	rq->xd = xd;
	rq->ttl = XDC_MAXTTL + 10;
	rq->mode = md;
	rq->tries = rq->errno = rq->lasterror = 0;
	rq->blockno = blk;
	rq->sectcnt = cnt;
	rq->dbuf = rq->dbufbase = db;
	rq->buf = bp;
}
/*
 * xdc_rqtopb: load up an IOPB based on an iorq
 */

void
xdc_rqtopb(iorq, iopb, cmd, subfun)
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;
	int     cmd, subfun;

{
	u_long  block, dp;

	/* standard stuff */

	iopb->errs = iopb->done = 0;
	iopb->comm = cmd;
	iopb->errno = iopb->status = 0;
	iopb->subfun = subfun;
	if (iorq->xd)
		iopb->unit = iorq->xd->xd_drive;
	else
		iopb->unit = 0;

	/* check for alternate IOPB format */

	if (cmd == XDCMD_WRP) {
		switch (subfun) {
		case XDFUN_CTL:{
			struct xd_iopb_ctrl *ctrl =
				(struct xd_iopb_ctrl *) iopb;
			iopb->lll = 0;
			iopb->intl = (XD_STATE(iorq->mode) == XD_SUB_POLL)
					? 0
					: iorq->xdc->ipl;
			ctrl->param_a = XDPA_TMOD | XDPA_DACF;
			ctrl->param_b = XDPB_ROR | XDPB_TDT_3_2USEC;
			ctrl->param_c = XDPC_OVS | XDPC_COP | XDPC_ASR |
					XDPC_RBC | XDPC_ECC2;
			ctrl->throttle = XDC_THROTTLE;
#ifdef __sparc__
			if (CPU_ISSUN4 && cpuinfo.cpu_type == CPUTYP_4_300)
				ctrl->delay = XDC_DELAY_4_300;
			else
				ctrl->delay = XDC_DELAY_SPARC;
#endif
#ifdef sun3
			ctrl->delay = XDC_DELAY_SUN3;
#endif
			break;
			}
		case XDFUN_DRV:{
			struct xd_iopb_drive *drv =
				(struct xd_iopb_drive *)iopb;
			/* we assume that the disk label has the right
			 * info */
			if (XD_STATE(iorq->mode) == XD_SUB_POLL)
				drv->dparam_ipl = (XDC_DPARAM << 3);
			else
				drv->dparam_ipl = (XDC_DPARAM << 3) |
						  iorq->xdc->ipl;
			drv->maxsect = iorq->xd->nsect - 1;
			drv->maxsector = drv->maxsect;
			/* note: maxsector != maxsect only if you are
			 * doing cyl sparing */
			drv->headoff = 0;
			drv->maxcyl = iorq->xd->pcyl - 1;
			drv->maxhead = iorq->xd->nhead - 1;
			break;
			}
		case XDFUN_FMT:{
			struct xd_iopb_format *form =
					(struct xd_iopb_format *) iopb;
			if (XD_STATE(iorq->mode) == XD_SUB_POLL)
				form->interleave_ipl = (XDC_INTERLEAVE << 3);
			else
				form->interleave_ipl = (XDC_INTERLEAVE << 3) |
						       iorq->xdc->ipl;
			form->field1 = XDFM_FIELD1;
			form->field2 = XDFM_FIELD2;
			form->field3 = XDFM_FIELD3;
			form->field4 = XDFM_FIELD4;
			form->bytespersec = XDFM_BPS;
			form->field6 = XDFM_FIELD6;
			form->field7 = XDFM_FIELD7;
			break;
			}
		}
	} else {

		/* normal IOPB case (harmless to RDP command) */

		iopb->lll = 0;
		iopb->intl = (XD_STATE(iorq->mode) == XD_SUB_POLL)
				? 0
				: iorq->xdc->ipl;
		iopb->sectcnt = iorq->sectcnt;
		block = iorq->blockno;
		if (iorq->xd == NULL || block == 0) {
			iopb->sectno = iopb->headno = iopb->cylno = 0;
		} else {
			iopb->sectno = block % iorq->xd->nsect;
			block = block / iorq->xd->nsect;
			iopb->headno = block % iorq->xd->nhead;
			block = block / iorq->xd->nhead;
			iopb->cylno = block;
		}
		dp = (u_long) iorq->dbuf - DVMA_BASE;
		dp = iopb->daddr = (iorq->dbuf == NULL) ? 0 : dp;
		iopb->addrmod = ((dp + (XDFM_BPS * iorq->sectcnt)) > 0x1000000)
					? XDC_ADDRMOD32
					: XDC_ADDRMOD;
	}
}

/*
 * xdc_cmd: front end for POLL'd and WAIT'd commands.  Returns rqno.
 * If you've already got an IORQ, you can call submit directly (currently
 * there is no need to do this).    NORM requests are handled separately.
 */
int
xdc_cmd(xdcsc, cmd, subfn, unit, block, scnt, dptr, fullmode)
	struct xdc_softc *xdcsc;
	int     cmd, subfn, unit, block, scnt;
	char   *dptr;
	int     fullmode;

{
	int     rqno, submode = XD_STATE(fullmode), retry;
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;

	/* get iorq/iopb */
	switch (submode) {
	case XD_SUB_POLL:
		while (xdcsc->nfree == 0) {
			if (xdc_piodriver(xdcsc, 0, 1) != XD_ERR_AOK)
				return (XD_ERR_FAIL);
		}
		break;
	case XD_SUB_WAIT:
		retry = 1;
		while (retry) {
			while (xdcsc->nfree == 0) {
			    if (tsleep(&xdcsc->nfree, PRIBIO, "xdnfree", 0))
				return (XD_ERR_FAIL);
			}
			while (xdcsc->ndone > XDC_SUBWAITLIM) {
			    if (tsleep(&xdcsc->ndone, PRIBIO, "xdsubwait", 0))
				return (XD_ERR_FAIL);
			}
			if (xdcsc->nfree)
				retry = 0;	/* got it */
		}
		break;
	default:
		return (XD_ERR_FAIL);	/* illegal */
	}
	if (xdcsc->nfree == 0)
		panic("xdcmd nfree");
	rqno = XDC_RQALLOC(xdcsc);
	iorq = &xdcsc->reqs[rqno];
	iopb = iorq->iopb;


	/* init iorq/iopb */

	xdc_rqinit(iorq, xdcsc,
	    (unit == XDC_NOUNIT) ? NULL : xdcsc->sc_drives[unit],
	    fullmode, block, scnt, dptr, NULL);

	/* load IOPB from iorq */

	xdc_rqtopb(iorq, iopb, cmd, subfn);

	/* submit it for processing */

	xdc_submit_iorq(xdcsc, rqno, fullmode);	/* error code will be in iorq */

	return (rqno);
}
/*
 * xdc_startbuf
 * start a buffer running, assumes nfree > 0
 */

int
xdc_startbuf(xdcsc, xdsc, bp)
	struct xdc_softc *xdcsc;
	struct xd_softc *xdsc;
	struct buf *bp;

{
	int     rqno, partno;
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;
	struct buf *wq;
	u_long  block;
	caddr_t dbuf;

	if (!xdcsc->nfree)
		panic("xdc_startbuf free");
	rqno = XDC_RQALLOC(xdcsc);
	iorq = &xdcsc->reqs[rqno];
	iopb = iorq->iopb;

	/* get buf */

	if (bp == NULL) {
		bp = xdcsc->sc_wq.b_actf;
		if (!bp)
			panic("xdc_startbuf bp");
		wq = bp->b_actf;
		if (wq)
			wq->b_actb = bp->b_actb;
		else
			xdcsc->sc_wq.b_actb = bp->b_actb;
		*bp->b_actb = wq;
		xdsc = xdcsc->sc_drives[DISKUNIT(bp->b_dev)];
	}
	partno = DISKPART(bp->b_dev);
#ifdef XDC_DEBUG
	printf("xdc_startbuf: %s%c: %s block %lld\n",
	    xdsc->sc_dev.dv_xname, 'a' + partno,
	    (bp->b_flags & B_READ) ? "read" : "write", bp->b_blkno);
	printf("xdc_startbuf: b_bcount %d, b_data 0x%x\n",
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
	    DL_GETPOFFSET(&xdsc->sc_dk.dk_label->d_partitions[partno]));

	dbuf = kdvma_mapin(bp->b_data, bp->b_bcount, 0);
	if (dbuf == NULL) {	/* out of DVMA space */
		printf("%s: warning: out of DVMA space\n",
			xdcsc->sc_dev.dv_xname);
		XDC_FREE(xdcsc, rqno);
		wq = &xdcsc->sc_wq;	/* put at end of queue */
		bp->b_actf = 0;
		bp->b_actb = wq->b_actb;
		*wq->b_actb = bp;
		wq->b_actb = &bp->b_actf;
		return (XD_ERR_FAIL);	/* XXX: need some sort of
					 * call-back scheme here? */
	}

	/* init iorq and load iopb from it */

	xdc_rqinit(iorq, xdcsc, xdsc, XD_SUB_NORM | XD_MODE_VERBO, block,
	    bp->b_bcount / XDFM_BPS, dbuf, bp);

	xdc_rqtopb(iorq, iopb, (bp->b_flags & B_READ) ? XDCMD_RD : XDCMD_WR, 0);

	/* Instrumentation. */
	disk_busy(&xdsc->sc_dk);

	/* now submit [note that xdc_submit_iorq can never fail on NORM reqs] */

	xdc_submit_iorq(xdcsc, rqno, XD_SUB_NORM);
	return (XD_ERR_AOK);
}


/*
 * xdc_submit_iorq: submit an iorq for processing.  returns XD_ERR_AOK
 * if ok.  if it fail returns an error code.  type is XD_SUB_*.
 *
 * note: caller frees iorq in all cases except NORM
 *
 * return value:
 *   NORM: XD_AOK (req pending), XD_FAIL (couldn't submit request)
 *   WAIT: XD_AOK (success), <error-code> (failed)
 *   POLL: <same as WAIT>
 *   NOQ : <same as NORM>
 *
 * there are three sources for i/o requests:
 * [1] xdstrategy: normal block I/O, using "struct buf" system.
 * [2] autoconfig/crash dump: these are polled I/O requests, no interrupts.
 * [3] open/ioctl: these are I/O requests done in the context of a process,
 *                 and the process should block until they are done.
 *
 * software state is stored in the iorq structure.  each iorq has an
 * iopb structure.  the hardware understands the iopb structure.
 * every command must go through an iopb.  a 7053 can only handle
 * XDC_MAXIOPB (31) active iopbs at one time.  iopbs are allocated in
 * DVMA space at boot up time.  what happens if we run out of iopb's?
 * for i/o type [1], the buffers are queued at the "buff" layer and
 * picked up later by the interrupt routine.  for case [2] the
 * programmed i/o driver is called with a special flag that says
 * return when one iopb is free.  for case [3] the process can sleep
 * on the iorq free list until some iopbs are available.
 */


int
xdc_submit_iorq(xdcsc, iorqno, type)
	struct xdc_softc *xdcsc;
	int     iorqno;
	int     type;

{
	u_long  iopbaddr;
	struct xd_iorq *iorq = &xdcsc->reqs[iorqno];

#ifdef XDC_DEBUG
	printf("xdc_submit_iorq(%s, no=%d, type=%d)\n", xdcsc->sc_dev.dv_xname,
	    iorqno, type);
#endif

	/* first check and see if controller is busy */
	if (xdcsc->xdc->xdc_csr & XDC_ADDING) {
#ifdef XDC_DEBUG
		printf("xdc_submit_iorq: XDC not ready (ADDING)\n");
#endif
		if (type == XD_SUB_NOQ)
			return (XD_ERR_FAIL);	/* failed */
		XDC_TWAIT(xdcsc, iorqno);	/* put at end of waitq */
		switch (type) {
		case XD_SUB_NORM:
			return XD_ERR_AOK;	/* success */
		case XD_SUB_WAIT:
			while (iorq->iopb->done == 0) {
				tsleep(iorq, PRIBIO, "xdiorq", 0);
			}
			return (iorq->errno);
		case XD_SUB_POLL:
			return (xdc_piodriver(xdcsc, iorqno, 0));
		default:
			panic("xdc_submit_iorq adding");
		}
	}
#ifdef XDC_DEBUG
	{
		u_char *rio = (u_char *) iorq->iopb;
		int     sz = sizeof(struct xd_iopb), lcv;
		printf("%s: aio #%d [",
			xdcsc->sc_dev.dv_xname, iorq - xdcsc->reqs);
		for (lcv = 0; lcv < sz; lcv++)
			printf(" %02x", rio[lcv]);
		printf("]\n");
	}
#endif				/* XDC_DEBUG */

	/* controller not busy, start command */
	iopbaddr = (u_long) iorq->iopb - (u_long) DVMA_BASE;
	XDC_GO(xdcsc->xdc, iopbaddr);	/* go! */
	xdcsc->nrun++;
	/* command now running, wrap it up */
	switch (type) {
	case XD_SUB_NORM:
	case XD_SUB_NOQ:
		return (XD_ERR_AOK);	/* success */
	case XD_SUB_WAIT:
		while (iorq->iopb->done == 0) {
			tsleep(iorq, PRIBIO, "xdiorq", 0);
		}
		return (iorq->errno);
	case XD_SUB_POLL:
		return (xdc_piodriver(xdcsc, iorqno, 0));
	default:
		panic("xdc_submit_iorq wrap up");
	}
	panic("xdc_submit_iorq");
	return 0;	/* not reached */
}


/*
 * xdc_piodriver
 *
 * programmed i/o driver.   this function takes over the computer
 * and drains off all i/o requests.   it returns the status of the iorq
 * the caller is interesting in.   if freeone is true, then it returns
 * when there is a free iorq.
 */
int
xdc_piodriver(xdcsc, iorqno, freeone)
	struct xdc_softc *xdcsc;
	int    iorqno;   
	int    freeone;

{
	int     nreset = 0;
	int     retval = 0;
	u_long  count;
	struct xdc *xdc = xdcsc->xdc;
#ifdef XDC_DEBUG
	printf("xdc_piodriver(%s, %d, freeone=%d)\n", xdcsc->sc_dev.dv_xname,
	    iorqno, freeone);
#endif

	while (xdcsc->nwait || xdcsc->nrun) {
#ifdef XDC_DEBUG
		printf("xdc_piodriver: wait=%d, run=%d\n",
			xdcsc->nwait, xdcsc->nrun);
#endif
		XDC_WAIT(xdc, count, XDC_MAXTIME, (XDC_REMIOPB | XDC_F_ERROR));
#ifdef XDC_DEBUG
		printf("xdc_piodriver: done wait with count = %d\n", count);
#endif
		/* we expect some progress soon */
		if (count == 0 && nreset >= 2) {
			xdc_reset(xdcsc, 0, XD_RSET_ALL, XD_ERR_FAIL, 0);
#ifdef XDC_DEBUG
			printf("xdc_piodriver: timeout\n");
#endif
			return (XD_ERR_FAIL);
		}
		if (count == 0) {
			if (xdc_reset(xdcsc, 0,
				      (nreset++ == 0) ? XD_RSET_NONE : iorqno,
				      XD_ERR_FAIL,
				      0) == XD_ERR_FAIL)
				return (XD_ERR_FAIL);	/* flushes all but POLL
							 * requests, resets */
			continue;
		}
		xdc_remove_iorq(xdcsc);	/* could resubmit request */
		if (freeone) {
			if (xdcsc->nrun < XDC_MAXIOPB) {
#ifdef XDC_DEBUG
				printf("xdc_piodriver: done: one free\n");
#endif
				return (XD_ERR_AOK);
			}
			continue;	/* don't xdc_start */
		}
		xdc_start(xdcsc, XDC_MAXIOPB);
	}

	/* get return value */

	retval = xdcsc->reqs[iorqno].errno;

#ifdef XDC_DEBUG
	printf("xdc_piodriver: done, retval = 0x%x (%s)\n",
	    xdcsc->reqs[iorqno].errno, xdc_e2str(xdcsc->reqs[iorqno].errno));
#endif

	/* now that we've drained everything, start up any bufs that have
	 * queued */

	while (xdcsc->nfree > 0 && xdcsc->sc_wq.b_actf)
		if (xdc_startbuf(xdcsc, NULL, NULL) != XD_ERR_AOK)
			break;

	return (retval);
}

/*
 * xdc_reset: reset one drive.   NOTE: assumes xdc was just reset.
 * we steal iopb[0] for this, but we put it back when we are done.
 */
void
xdc_xdreset(xdcsc, xdsc)
	struct xdc_softc *xdcsc;
	struct xd_softc *xdsc;

{
	struct xd_iopb tmpiopb;
	u_long  addr;
	int     del;
	bcopy(xdcsc->iopbase, &tmpiopb, sizeof(tmpiopb));
	bzero(xdcsc->iopbase, sizeof(tmpiopb));
	xdcsc->iopbase->comm = XDCMD_RST;
	xdcsc->iopbase->unit = xdsc->xd_drive;
	addr = (u_long) xdcsc->dvmaiopb;
	XDC_GO(xdcsc->xdc, addr);	/* go! */
	XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_REMIOPB);
	if (del <= 0 || xdcsc->iopbase->errs) {
		printf("%s: off-line: %s\n", xdcsc->sc_dev.dv_xname,
		    xdc_e2str(xdcsc->iopbase->errno));
		xdcsc->xdc->xdc_csr = XDC_RESET;
		XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_RESET);
		if (del <= 0)
			panic("xdc_reset");
	} else {
		xdcsc->xdc->xdc_csr = XDC_CLRRIO;	/* clear RIO */
	}
	bcopy(&tmpiopb, xdcsc->iopbase, sizeof(tmpiopb));
}


/*
 * xdc_reset: reset everything: requests are marked as errors except
 * a polled request (which is resubmitted)
 */
int
xdc_reset(xdcsc, quiet, blastmode, error, xdsc)
	struct xdc_softc *xdcsc;
	int     quiet, blastmode, error;
	struct xd_softc *xdsc;

{
	int     del = 0, lcv, retval = XD_ERR_AOK;
	int     oldfree = xdcsc->nfree;

	/* soft reset hardware */

	if (!quiet)
		printf("%s: soft reset\n", xdcsc->sc_dev.dv_xname);
	xdcsc->xdc->xdc_csr = XDC_RESET;
	XDC_WAIT(xdcsc->xdc, del, XDC_RESETUSEC, XDC_RESET);
	if (del <= 0) {
		blastmode = XD_RSET_ALL;	/* dead, flush all requests */
		retval = XD_ERR_FAIL;
	}
	if (xdsc)
		xdc_xdreset(xdcsc, xdsc);

	/* fix queues based on "blast-mode" */

	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		struct xd_iorq *iorq = &xdcsc->reqs[lcv];

		if (XD_STATE(iorq->mode) != XD_SUB_POLL &&
		    XD_STATE(iorq->mode) != XD_SUB_WAIT &&
		    XD_STATE(iorq->mode) != XD_SUB_NORM)
			/* is it active? */
			continue;

		xdcsc->nrun--;	/* it isn't running any more */
		if (blastmode == XD_RSET_ALL || blastmode != lcv) {
			/* failed */
			iorq->errno = error;
			xdcsc->iopbase[lcv].done = xdcsc->iopbase[lcv].errs = 1;
			switch (XD_STATE(xdcsc->reqs[lcv].mode)) {
			case XD_SUB_NORM:
			    iorq->buf->b_error = EIO;
			    iorq->buf->b_flags |= B_ERROR;
			    iorq->buf->b_resid =
			       iorq->sectcnt * XDFM_BPS;
			    dvma_mapout((vaddr_t)iorq->dbufbase,
				    (vaddr_t)iorq->buf->b_data,
				    iorq->buf->b_bcount);
			    disk_unbusy(&xdcsc->reqs[lcv].xd->sc_dk,
				(xdcsc->reqs[lcv].buf->b_bcount -
				xdcsc->reqs[lcv].buf->b_resid),
				(xdcsc->reqs[lcv].buf->b_flags & B_READ));
			    biodone(iorq->buf);
			    XDC_FREE(xdcsc, lcv);	/* add to free list */
			    break;
			case XD_SUB_WAIT:
			    wakeup(iorq);
			case XD_SUB_POLL:
			    xdcsc->ndone++;
			    iorq->mode =
				XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			    break;
			}

		} else {

			/* resubmit, put at front of wait queue */
			XDC_HWAIT(xdcsc, lcv);
		}
	}

	/*
	 * now, if stuff is waiting, start it.
	 * since we just reset it should go
	 */
	xdc_start(xdcsc, XDC_MAXIOPB);

	/* ok, we did it */
	if (oldfree == 0 && xdcsc->nfree)
		wakeup(&xdcsc->nfree);

#ifdef XDC_DIAG
	del = xdcsc->nwait + xdcsc->nrun + xdcsc->nfree + xdcsc->ndone;
	if (del != XDC_MAXIOPB)
		printf("%s: diag: xdc_reset miscount (%d should be %d)!\n",
		    xdcsc->sc_dev.dv_xname, del, XDC_MAXIOPB);
	else
		if (xdcsc->ndone > XDC_MAXIOPB - XDC_SUBWAITLIM)
			printf("%s: diag: lots of done jobs (%d)\n",
			    xdcsc->sc_dev.dv_xname, xdcsc->ndone);
#endif
	printf("RESET DONE\n");
	return (retval);
}
/*
 * xdc_start: start all waiting buffers
 */

void
xdc_start(xdcsc, maxio)
	struct xdc_softc *xdcsc;
	int     maxio;

{
	int     rqno;
	while (maxio && xdcsc->nwait &&
		(xdcsc->xdc->xdc_csr & XDC_ADDING) == 0) {
		XDC_GET_WAITER(xdcsc, rqno);	/* note: rqno is an "out"
						 * param */
		if (xdc_submit_iorq(xdcsc, rqno, XD_SUB_NOQ) != XD_ERR_AOK)
			panic("xdc_start");	/* should never happen */
		maxio--;
	}
}
/*
 * xdc_remove_iorq: remove "done" IOPB's.
 */

int
xdc_remove_iorq(xdcsc)
	struct xdc_softc *xdcsc;

{
	int     errno, rqno, comm, errs;
	struct xdc *xdc = xdcsc->xdc;
	struct xd_iopb *iopb;
	struct xd_iorq *iorq;
	struct buf *bp;

	if (xdc->xdc_csr & XDC_F_ERROR) {
		/*
		 * FATAL ERROR: should never happen under normal use. This
		 * error is so bad, you can't even tell which IOPB is bad, so
		 * we dump them all.
		 */
		errno = xdc->xdc_f_err;
		printf("%s: fatal error 0x%02x: %s\n", xdcsc->sc_dev.dv_xname,
		    errno, xdc_e2str(errno));
		if (xdc_reset(xdcsc, 0, XD_RSET_ALL, errno, 0) != XD_ERR_AOK) {
			printf("%s: soft reset failed!\n",
				xdcsc->sc_dev.dv_xname);
			panic("xdc_remove_iorq: controller DEAD");
		}
		return (XD_ERR_AOK);
	}

	/*
	 * get iopb that is done
	 *
	 * hmm... I used to read the address of the done IOPB off the VME
	 * registers and calculate the rqno directly from that.   that worked
	 * until I started putting a load on the controller.   when loaded, i
	 * would get interrupts but neither the REMIOPB or F_ERROR bits would
	 * be set, even after DELAY'ing a while!   later on the timeout
	 * routine would detect IOPBs that were marked "running" but their
	 * "done" bit was set.   rather than dealing directly with this
	 * problem, it is just easier to look at all running IOPB's for the
	 * done bit.
	 */
	if (xdc->xdc_csr & XDC_REMIOPB) {
		xdc->xdc_csr = XDC_CLRRIO;
	}

	for (rqno = 0; rqno < XDC_MAXIOPB; rqno++) {
		iorq = &xdcsc->reqs[rqno];
		if (iorq->mode == 0 || XD_STATE(iorq->mode) == XD_SUB_DONE)
			continue;	/* free, or done */
		iopb = &xdcsc->iopbase[rqno];
		if (iopb->done == 0)
			continue;	/* not done yet */

#ifdef XDC_DEBUG
		{
			u_char *rio = (u_char *) iopb;
			int     sz = sizeof(struct xd_iopb), lcv;
			printf("%s: rio #%d [", xdcsc->sc_dev.dv_xname, rqno);
			for (lcv = 0; lcv < sz; lcv++)
				printf(" %02x", rio[lcv]);
			printf("]\n");
		}
#endif				/* XDC_DEBUG */

		xdcsc->nrun--;

		comm = iopb->comm;
		errs = iopb->errs;

		if (errs)
			iorq->errno = iopb->errno;
		else
			iorq->errno = 0;

		/* handle non-fatal errors */

		if (errs &&
		    xdc_error(xdcsc, iorq, iopb, rqno, comm) == XD_ERR_AOK)
			continue;	/* AOK: we resubmitted it */


		/* this iorq is now done (hasn't been restarted or anything) */

		if ((iorq->mode & XD_MODE_VERBO) && iorq->lasterror)
			xdc_perror(iorq, iopb, 0);

		/* now, if read/write check to make sure we got all the data
		 * we needed. (this may not be the case if we got an error in
		 * the middle of a multisector request).   */

		if ((iorq->mode & XD_MODE_B144) != 0 && errs == 0 &&
		    (comm == XDCMD_RD || comm == XDCMD_WR)) {
			/* we just successfully processed a bad144 sector
			 * note: if we are in bad 144 mode, the pointers have
			 * been advanced already (see above) and are pointing
			 * at the bad144 sector.   to exit bad144 mode, we
			 * must advance the pointers 1 sector and issue a new
			 * request if there are still sectors left to process
			 *
			 */
			XDC_ADVANCE(iorq, 1);	/* advance 1 sector */

			/* exit b144 mode */
			iorq->mode = iorq->mode & (~XD_MODE_B144);

			if (iorq->sectcnt) {	/* more to go! */
				iorq->lasterror = iorq->errno = iopb->errno = 0;
				iopb->errs = iopb->done = 0;
				iorq->tries = 0;
				iopb->sectcnt = iorq->sectcnt;
				iopb->cylno = iorq->blockno /
						iorq->xd->sectpercyl;
				iopb->headno =
					(iorq->blockno / iorq->xd->nhead) %
						iorq->xd->nhead;
				iopb->sectno = iorq->blockno % XDFM_BPS;
				iopb->daddr = (u_long) iorq->dbuf - DVMA_BASE;
				XDC_HWAIT(xdcsc, rqno);
				xdc_start(xdcsc, 1);	/* resubmit */
				continue;
			}
		}
		/* final cleanup, totally done with this request */

		switch (XD_STATE(iorq->mode)) {
		case XD_SUB_NORM:
			bp = iorq->buf;
			if (errs) {
				bp->b_error = EIO;
				bp->b_flags |= B_ERROR;
				bp->b_resid = iorq->sectcnt * XDFM_BPS;
			} else {
				bp->b_resid = 0;	/* done */
			}
			dvma_mapout((vaddr_t) iorq->dbufbase,
				    (vaddr_t) bp->b_data,
				    bp->b_bcount);
			disk_unbusy(&iorq->xd->sc_dk,
			    (bp->b_bcount - bp->b_resid),
			    (bp->b_flags & B_READ));
			XDC_FREE(xdcsc, rqno);
			biodone(bp);
			break;
		case XD_SUB_WAIT:
			iorq->mode = XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			xdcsc->ndone++;
			wakeup(iorq);
			break;
		case XD_SUB_POLL:
			iorq->mode = XD_NEWSTATE(iorq->mode, XD_SUB_DONE);
			xdcsc->ndone++;
			break;
		}
	}

	return (XD_ERR_AOK);
}

/*
 * xdc_perror: print error.
 * - if still_trying is true: we got an error, retried and got a
 *   different error.  in that case lasterror is the old error,
 *   and errno is the new one.
 * - if still_trying is not true, then if we ever had an error it
 *   is in lasterror. also, if iorq->errno == 0, then we recovered
 *   from that error (otherwise iorq->errno == iorq->lasterror).
 */
void
xdc_perror(iorq, iopb, still_trying)
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;
	int     still_trying;

{

	int     error = iorq->lasterror;

	printf("%s", (iorq->xd) ? iorq->xd->sc_dev.dv_xname
	    : iorq->xdc->sc_dev.dv_xname);
	if (iorq->buf)
		printf("%c: ", 'a' + DISKPART(iorq->buf->b_dev));
	if (iopb->comm == XDCMD_RD || iopb->comm == XDCMD_WR)
		printf("%s %d/%d/%d: ",
			(iopb->comm == XDCMD_RD) ? "read" : "write",
			iopb->cylno, iopb->headno, iopb->sectno);
	printf("%s", xdc_e2str(error));

	if (still_trying)
		printf(" [still trying, new error=%s]", xdc_e2str(iorq->errno));
	else
		if (iorq->errno == 0)
			printf(" [recovered in %d tries]", iorq->tries);

	printf("\n");
}

/*
 * xdc_error: non-fatal error encountered... recover.
 * return AOK if resubmitted, return FAIL if this iopb is done
 */
int
xdc_error(xdcsc, iorq, iopb, rqno, comm)
	struct xdc_softc *xdcsc;
	struct xd_iorq *iorq;
	struct xd_iopb *iopb;
	int     rqno, comm;

{
	int     errno = iorq->errno;
	int     erract = errno & XD_ERA_MASK;
	int     oldmode, advance, i;

	if (erract == XD_ERA_RSET) {	/* some errors require a reset */
		oldmode = iorq->mode;
		iorq->mode = XD_SUB_DONE | (~XD_SUB_MASK & oldmode);
		xdcsc->ndone++;
		/* make xdc_start ignore us */
		xdc_reset(xdcsc, 1, XD_RSET_NONE, errno, iorq->xd);
		iorq->mode = oldmode;
		xdcsc->ndone--;
	}
	/* check for read/write to a sector in bad144 table if bad: redirect
	 * request to bad144 area */

	if ((comm == XDCMD_RD || comm == XDCMD_WR) &&
	    (iorq->mode & XD_MODE_B144) == 0) {
		advance = iorq->sectcnt - iopb->sectcnt;
		XDC_ADVANCE(iorq, advance);
		if ((i = isbad(&iorq->xd->dkb, iorq->blockno / iorq->xd->sectpercyl,
			    (iorq->blockno / iorq->xd->nsect) % iorq->xd->nhead,
			    iorq->blockno % iorq->xd->nsect)) != -1) {
			iorq->mode |= XD_MODE_B144;	/* enter bad144 mode &
							 * redirect */
			iopb->errno = iopb->done = iopb->errs = 0;
			iopb->sectcnt = 1;
			iopb->cylno = (iorq->xd->ncyl + iorq->xd->acyl) - 2;
			/* second to last acyl */
			i = iorq->xd->sectpercyl - 1 - i;	/* follow bad144
								 * standard */
			iopb->headno = i / iorq->xd->nhead;
			iopb->sectno = i % iorq->xd->nhead;
			XDC_HWAIT(xdcsc, rqno);
			xdc_start(xdcsc, 1);	/* resubmit */
			return (XD_ERR_AOK);	/* recovered! */
		}
	}

	/*
	 * it isn't a bad144 sector, must be real error! see if we can retry
	 * it?
	 */
	if ((iorq->mode & XD_MODE_VERBO) && iorq->lasterror)
		xdc_perror(iorq, iopb, 1);	/* inform of error state
						 * change */
	iorq->lasterror = errno;

	if ((erract == XD_ERA_RSET || erract == XD_ERA_HARD)
	    && iorq->tries < XDC_MAXTRIES) {	/* retry? */
		iorq->tries++;
		iorq->errno = iopb->errno = iopb->done = iopb->errs = 0;
		XDC_HWAIT(xdcsc, rqno);
		xdc_start(xdcsc, 1);	/* restart */
		return (XD_ERR_AOK);	/* recovered! */
	}

	/* failed to recover from this error */
	return (XD_ERR_FAIL);
}

/*
 * xdc_tick: make sure xd is still alive and ticking (err, kicking).
 */
void
xdc_tick(arg)
	void   *arg;

{
	struct xdc_softc *xdcsc = arg;
	int     lcv, s, reset = 0;
#ifdef XDC_DIAG
	int     wait, run, free, done, whd = 0;
	u_char  fqc[XDC_MAXIOPB], wqc[XDC_MAXIOPB], mark[XDC_MAXIOPB];
	s = splbio();
	wait = xdcsc->nwait;
	run = xdcsc->nrun;
	free = xdcsc->nfree;
	done = xdcsc->ndone;
	bcopy(xdcsc->waitq, wqc, sizeof(wqc));
	bcopy(xdcsc->freereq, fqc, sizeof(fqc));
	splx(s);
	if (wait + run + free + done != XDC_MAXIOPB) {
		printf("%s: diag: IOPB miscount (got w/f/r/d %d/%d/%d/%d, wanted %d)\n",
		    xdcsc->sc_dev.dv_xname, wait, free, run, done, XDC_MAXIOPB);
		bzero(mark, sizeof(mark));
		printf("FREE: ");
		for (lcv = free; lcv > 0; lcv--) {
			printf("%d ", fqc[lcv - 1]);
			mark[fqc[lcv - 1]] = 1;
		}
		printf("\nWAIT: ");
		lcv = wait;
		while (lcv > 0) {
			printf("%d ", wqc[whd]);
			mark[wqc[whd]] = 1;
			whd = (whd + 1) % XDC_MAXIOPB;
			lcv--;
		}
		printf("\n");
		for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
			if (mark[lcv] == 0)
				printf("MARK: running %d: mode %d done %d errs %d errno 0x%x ttl %d buf %p\n",
				lcv, xdcsc->reqs[lcv].mode,
				xdcsc->iopbase[lcv].done,
				xdcsc->iopbase[lcv].errs,
				xdcsc->iopbase[lcv].errno,
				xdcsc->reqs[lcv].ttl, xdcsc->reqs[lcv].buf);
		}
	} else
		if (done > XDC_MAXIOPB - XDC_SUBWAITLIM)
			printf("%s: diag: lots of done jobs (%d)\n",
				xdcsc->sc_dev.dv_xname, done);

#endif
#ifdef XDC_DEBUG
	printf("%s: tick: csr 0x%x, w/f/r/d %d/%d/%d/%d\n",
		xdcsc->sc_dev.dv_xname,
		xdcsc->xdc->xdc_csr, xdcsc->nwait, xdcsc->nfree, xdcsc->nrun,
		xdcsc->ndone);
	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		if (xdcsc->reqs[lcv].mode)
		  printf("running %d: mode %d done %d errs %d errno 0x%x\n",
			 lcv,
			 xdcsc->reqs[lcv].mode, xdcsc->iopbase[lcv].done,
			 xdcsc->iopbase[lcv].errs, xdcsc->iopbase[lcv].errno);
	}
#endif

	/* reduce ttl for each request if one goes to zero, reset xdc */
	s = splbio();
	for (lcv = 0; lcv < XDC_MAXIOPB; lcv++) {
		if (xdcsc->reqs[lcv].mode == 0 ||
		    XD_STATE(xdcsc->reqs[lcv].mode) == XD_SUB_DONE)
			continue;
		xdcsc->reqs[lcv].ttl--;
		if (xdcsc->reqs[lcv].ttl == 0)
			reset = 1;
	}
	if (reset) {
		printf("%s: watchdog timeout\n", xdcsc->sc_dev.dv_xname);
		xdc_reset(xdcsc, 0, XD_RSET_NONE, XD_ERR_FAIL, NULL);
	}
	splx(s);

	/* until next time */

	timeout_add(&xdcsc->xdc_tick_tmo, XDC_TICKCNT);
}

/*
 * xdc_ioctlcmd: this function provides a user level interface to the
 * controller via ioctl.   this allows "format" programs to be written
 * in user code, and is also useful for some debugging.   we return
 * an error code.   called at user priority.
 */
int
xdc_ioctlcmd(xd, dev, xio)
	struct xd_softc *xd;
	dev_t   dev;
	struct xd_iocmd *xio;

{
	int     s, err, rqno, dummy;
	caddr_t dvmabuf = NULL, buf = NULL;
	struct xdc_softc *xdcsc;

	/* check sanity of requested command */

	switch (xio->cmd) {

	case XDCMD_NOP:	/* no op: everything should be zero */
		if (xio->subfn || xio->dptr || xio->dlen ||
		    xio->block || xio->sectcnt)
			return (EINVAL);
		break;

	case XDCMD_RD:		/* read / write sectors (up to XD_IOCMD_MAXS) */
	case XDCMD_WR:
		if (xio->subfn || xio->sectcnt > XD_IOCMD_MAXS ||
		    xio->sectcnt * XDFM_BPS != xio->dlen || xio->dptr == NULL)
			return (EINVAL);
		break;

	case XDCMD_SK:		/* seek: doesn't seem useful to export this */
		return (EINVAL);

	case XDCMD_WRP:	/* write parameters */
		return (EINVAL);/* not useful, except maybe drive
				 * parameters... but drive parameters should
				 * go via disklabel changes */

	case XDCMD_RDP:	/* read parameters */
		if (xio->subfn != XDFUN_DRV ||
		    xio->dlen || xio->block || xio->dptr)
			return (EINVAL);	/* allow read drive params to
						 * get hw_spt */
		xio->sectcnt = xd->hw_spt;	/* we already know the answer */
		return (0);
		break;

	case XDCMD_XRD:	/* extended read/write */
	case XDCMD_XWR:

		switch (xio->subfn) {

		case XDFUN_THD:/* track headers */
			if (xio->sectcnt != xd->hw_spt ||
			    (xio->block % xd->nsect) != 0 ||
			    xio->dlen != XD_IOCMD_HSZ * xd->hw_spt ||
			    xio->dptr == NULL)
				return (EINVAL);
			xio->sectcnt = 0;
			break;

		case XDFUN_FMT:/* NOTE: also XDFUN_VFY */
			if (xio->cmd == XDCMD_XRD)
				return (EINVAL);	/* no XDFUN_VFY */
			if (xio->sectcnt || xio->dlen ||
			    (xio->block % xd->nsect) != 0 || xio->dptr)
				return (EINVAL);
			break;

		case XDFUN_HDR:/* header, header verify, data, data ECC */
			return (EINVAL);	/* not yet */

		case XDFUN_DM:	/* defect map */
		case XDFUN_DMX:/* defect map (alternate location) */
			if (xio->sectcnt || xio->dlen != XD_IOCMD_DMSZ ||
			    (xio->block % xd->nsect) != 0 || xio->dptr == NULL)
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
		break;

	case XDCMD_TST:	/* diagnostics */
		return (EINVAL);

	default:
		return (EINVAL);/* ??? */
	}

	/* create DVMA buffer for request if needed */

	if (xio->dlen) {
		dvmabuf = dvma_malloc(xio->dlen, &buf, M_WAITOK);
		if (xio->cmd == XDCMD_WR || xio->cmd == XDCMD_XWR) {
			if ((err = copyin(xio->dptr, buf, xio->dlen)) != 0) {
				dvma_free(dvmabuf, xio->dlen, &buf);
				return (err);
			}
		}
	}
	/* do it! */

	err = 0;
	xdcsc = xd->parent;
	s = splbio();
	rqno = xdc_cmd(xdcsc, xio->cmd, xio->subfn, xd->xd_drive, xio->block,
	    xio->sectcnt, dvmabuf, XD_SUB_WAIT);
	if (rqno == XD_ERR_FAIL) {
		err = EIO;
		goto done;
	}
	xio->errno = xdcsc->reqs[rqno].errno;
	xio->tries = xdcsc->reqs[rqno].tries;
	XDC_DONE(xdcsc, rqno, dummy);

	if (xio->cmd == XDCMD_RD || xio->cmd == XDCMD_XRD)
		err = copyout(buf, xio->dptr, xio->dlen);

done:
	splx(s);
	if (dvmabuf)
		dvma_free(dvmabuf, xio->dlen, &buf);
	return (err);
}

/*
 * xdc_e2str: convert error code number into an error string
 */
char *
xdc_e2str(no)
	int     no;
{
	switch (no) {
	case XD_ERR_FAIL:
		return ("Software fatal error");
	case XD_ERR_AOK:
		return ("Successful completion");
	case XD_ERR_ICYL:
		return ("Illegal cylinder address");
	case XD_ERR_IHD:
		return ("Illegal head address");
	case XD_ERR_ISEC:
		return ("Illgal sector address");
	case XD_ERR_CZER:
		return ("Count zero");
	case XD_ERR_UIMP:
		return ("Unimplemented command");
	case XD_ERR_IF1:
		return ("Illegal field length 1");
	case XD_ERR_IF2:
		return ("Illegal field length 2");
	case XD_ERR_IF3:
		return ("Illegal field length 3");
	case XD_ERR_IF4:
		return ("Illegal field length 4");
	case XD_ERR_IF5:
		return ("Illegal field length 5");
	case XD_ERR_IF6:
		return ("Illegal field length 6");
	case XD_ERR_IF7:
		return ("Illegal field length 7");
	case XD_ERR_ISG:
		return ("Illegal scatter/gather length");
	case XD_ERR_ISPT:
		return ("Not enough sectors per track");
	case XD_ERR_ALGN:
		return ("Next IOPB address alignment error");
	case XD_ERR_SGAL:
		return ("Scatter/gather address alignment error");
	case XD_ERR_SGEC:
		return ("Scatter/gather with auto-ECC");
	case XD_ERR_SECC:
		return ("Soft ECC corrected");
	case XD_ERR_SIGN:
		return ("ECC ignored");
	case XD_ERR_ASEK:
		return ("Auto-seek retry recovered");
	case XD_ERR_RTRY:
		return ("Soft retry recovered");
	case XD_ERR_HECC:
		return ("Hard data ECC");
	case XD_ERR_NHDR:
		return ("Header not found");
	case XD_ERR_NRDY:
		return ("Drive not ready");
	case XD_ERR_TOUT:
		return ("Operation timeout");
	case XD_ERR_VTIM:
		return ("VMEDMA timeout");
	case XD_ERR_DSEQ:
		return ("Disk sequencer error");
	case XD_ERR_HDEC:
		return ("Header ECC error");
	case XD_ERR_RVFY:
		return ("Read verify");
	case XD_ERR_VFER:
		return ("Fatal VMEDMA error");
	case XD_ERR_VBUS:
		return ("VMEbus error");
	case XD_ERR_DFLT:
		return ("Drive faulted");
	case XD_ERR_HECY:
		return ("Header error/cylinder");
	case XD_ERR_HEHD:
		return ("Header error/head");
	case XD_ERR_NOCY:
		return ("Drive not on-cylinder");
	case XD_ERR_SEEK:
		return ("Seek error");
	case XD_ERR_ILSS:
		return ("Illegal sector size");
	case XD_ERR_SEC:
		return ("Soft ECC");
	case XD_ERR_WPER:
		return ("Write-protect error");
	case XD_ERR_IRAM:
		return ("IRAM self test failure");
	case XD_ERR_MT3:
		return ("Maintenance test 3 failure (DSKCEL RAM)");
	case XD_ERR_MT4:
		return ("Maintenance test 4 failure (header shift reg)");
	case XD_ERR_MT5:
		return ("Maintenance test 5 failure (VMEDMA regs)");
	case XD_ERR_MT6:
		return ("Maintenance test 6 failure (REGCEL chip)");
	case XD_ERR_MT7:
		return ("Maintenance test 7 failure (buffer parity)");
	case XD_ERR_MT8:
		return ("Maintenance test 8 failure (disk FIFO)");
	case XD_ERR_IOCK:
		return ("IOPB checksum miscompare");
	case XD_ERR_IODM:
		return ("IOPB DMA fatal");
	case XD_ERR_IOAL:
		return ("IOPB address alignment error");
	case XD_ERR_FIRM:
		return ("Firmware error");
	case XD_ERR_MMOD:
		return ("Illegal maintenance mode test number");
	case XD_ERR_ACFL:
		return ("ACFAIL asserted");
	default:
		return ("Unknown error");
	}
}
