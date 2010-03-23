/*	$OpenBSD: vs.c,v 1.79 2010/03/23 01:57:19 krw Exp $	*/

/*
 * Copyright (c) 2004, 2009, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * MVME328S SCSI adaptor driver
 */

/* This card lives in D16 space */
#define	__BUS_SPACE_RESTRICT_D16__

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/cmmu.h>
#include <machine/cpu.h>

#include <mvme88k/dev/vsreg.h>
#include <mvme88k/dev/vsvar.h>
#include <mvme88k/dev/vme.h>

int	vsmatch(struct device *, void *, void *);
void	vsattach(struct device *, struct device *, void *);
void	vs_minphys(struct buf *, struct scsi_link *);
void	vs_scsicmd(struct scsi_xfer *);

struct scsi_adapter vs_scsiswitch = {
	vs_scsicmd,
	vs_minphys,
	0,			/* no lun support */
	0,			/* no lun support */
};

struct scsi_device vs_scsidev = {
	NULL,		/* use default error handler */
	NULL,		/* do not have a start function */
	NULL,		/* have no async handler */
	NULL,		/* Use default done routine */
};

struct cfattach vs_ca = {
	sizeof(struct vs_softc), vsmatch, vsattach,
};

struct cfdriver vs_cd = {
	NULL, "vs", DV_DULL,
};

int	do_vspoll(struct vs_softc *, struct scsi_xfer *, int);
void	thaw_queue(struct vs_softc *, int);
void	thaw_all_queues(struct vs_softc *);
int	vs_alloc_sg(struct vs_softc *);
int	vs_alloc_wq(struct vs_softc *);
void	vs_build_sg_list(struct vs_softc *, struct vs_cb *, bus_addr_t);
void	vs_chksense(struct vs_cb *, struct scsi_xfer *);
int	vs_eintr(void *);
int	vs_getcqe(struct vs_softc *, bus_addr_t *, bus_addr_t *);
int	vs_identify(struct vs_channel *, int);
int	vs_initialize(struct vs_softc *);
int	vs_intr(struct vs_softc *);
int	vs_load_command(struct vs_softc *, struct vs_cb *, bus_addr_t,
	    bus_addr_t, struct scsi_link *, int, struct scsi_generic *, int,
	    uint8_t *, int);
int	vs_nintr(void *);
void	vs_poll(struct vs_softc *, struct vs_cb *);
void	vs_print_addr(struct vs_softc *, struct scsi_xfer *);
struct vs_cb *vs_find_queue(struct scsi_link *, struct vs_softc *);
void	vs_reset(struct vs_softc *, int);
void	vs_resync(struct vs_softc *);
void	vs_scsidone(struct vs_softc *, struct vs_cb *);
int	vs_unit_value(int, int, int);

static __inline__ void vs_free(struct vs_softc *, struct vs_cb *);
static __inline__ void vs_clear_return_info(struct vs_softc *);

int
vsmatch(struct device *device, void *cf, void *args)
{
	struct confargs *ca = args;
	bus_space_tag_t iot = ca->ca_iot;
	bus_space_handle_t ioh;
	int rc;
	u_int16_t id;

	if (bus_space_map(iot, ca->ca_paddr, S_SHORTIO, 0, &ioh) != 0)
		return 0;
	rc = badaddr((vaddr_t)bus_space_vaddr(iot, ioh) + sh_CSS + CSB_TYPE, 2);
	if (rc == 0) {
		id = bus_space_read_2(iot, ioh, sh_CSS + CSB_TYPE);
		if (id != JAGUAR && id != COUGAR)
			rc = 1;
		/*
		 * Note that this will reject Cougar boards configured with
		 * less than 2KB of short I/O memory.
		 * Is it worth checking for a Cougar signature at lower
		 * addresses, knowing that we can't really work unless
		 * the board is jumped to enable the whole 2KB?
		 */
	}
	bus_space_unmap(iot, ioh, S_SHORTIO);

	return rc == 0;
}

void
vsattach(struct device *parent, struct device *self, void *args)
{
	struct vs_softc *sc = (struct vs_softc *)self;
	struct vs_channel *vc;
	struct confargs *ca = args;
	struct scsi_link *sc_link;
	struct scsibus_attach_args saa;
	int evec, bus;
	int tmp;

	/* get the next available vector for the error interrupt */
	evec = vme_findvec(ca->ca_vec);

	if (ca->ca_vec < 0 || evec < 0) {
		printf(": no more interrupts!\n");
		return;
	}
	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_BIO;

	printf(" vec 0x%x: ", evec);

	sc->sc_dmat = ca->ca_dmat;
	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, S_SHORTIO, 0,
	    &sc->sc_ioh) != 0) {
		printf("can't map registers!\n");
		return;
	}

	sc->sc_bid = csb_read(2, CSB_TYPE);
	sc->sc_ipl = ca->ca_ipl;
	sc->sc_nvec = ca->ca_vec;
	sc->sc_evec = evec;

	if (vs_initialize(sc))
		return;

	sc->sc_ih_n.ih_fn = vs_nintr;
	sc->sc_ih_n.ih_arg = sc;
	sc->sc_ih_n.ih_wantframe = 0;
	sc->sc_ih_n.ih_ipl = ca->ca_ipl;

	sc->sc_ih_e.ih_fn = vs_eintr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_wantframe = 0;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;

	vmeintr_establish(sc->sc_nvec, &sc->sc_ih_n, self->dv_xname);
	snprintf(sc->sc_intrname_e, sizeof sc->sc_intrname_e,
	    "%s_err", self->dv_xname);
	vmeintr_establish(sc->sc_evec, &sc->sc_ih_e, sc->sc_intrname_e);

	/*
	 * Attach all scsi units on us, watching for boot device
	 * (see device_register).
	 */
	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;		/* invalid flag to device_register */

	for (bus = 0; bus < 2; bus++) {
		vc = &sc->sc_channel[bus];
		if (vc->vc_id < 0)
			continue;

		sc_link = &vc->vc_link;
		sc_link->adapter = &vs_scsiswitch;
		sc_link->adapter_buswidth = vc->vc_width;
		sc_link->adapter_softc = sc;
		sc_link->adapter_target = vc->vc_id;
		sc_link->device = &vs_scsidev;
		if (sc->sc_bid != JAGUAR)
			sc_link->luns = 1;	/* not enough queues */
		sc_link->openings = 1;
		if (bus != 0)
			sc_link->flags = SDEV_2NDBUS;

		printf("%s: channel %d", sc->sc_dev.dv_xname, bus);
		switch (vc->vc_type) {
		case VCT_SE:
			printf(", single-ended");
			break;
		case VCT_DIFFERENTIAL:
			printf(", differential");
			break;
		}
		printf("\n");

		if (vc->vc_width == 0) {
			printf("%s: daughterboard disabled, "
			    "not enough on-board memory\n",
			    sc->sc_dev.dv_xname);
			continue;
		}

		bzero(&saa, sizeof(saa));
		saa.saa_sc_link = &vc->vc_link;

		bootbus = bus;
		config_found(self, &saa, scsiprint);
	}

	bootpart = tmp;		    /* restore old values */
	bootbus = 0;
}

void
vs_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > ptoa(MAX_SG_ELEMENTS))
		bp->b_bcount = ptoa(MAX_SG_ELEMENTS);
	minphys(bp);
}

void
vs_print_addr(struct vs_softc *sc, struct scsi_xfer *xs)
{
	if (xs == NULL)
		printf("%s: ", sc->sc_dev.dv_xname);
	else {
		sc_print_addr(xs->sc_link);

		/* print bus number too if appropriate */
		if (sc->sc_channel[1].vc_width >= 0)
			printf("(channel %d) ",
			    !!(xs->sc_link->flags & SDEV_2NDBUS));
	}
}

int
do_vspoll(struct vs_softc *sc, struct scsi_xfer *xs, int canreset)
{
	int to;
	int crsw, bus;

	if (xs != NULL) {
		bus = !!(xs->sc_link->flags & SDEV_2NDBUS);
		to = xs->timeout;
		if (to == 0)
			to = 2000;
	} else {
		bus = -1;
		to = 2000;
	}

	while (((crsw = CRSW) & (M_CRSW_CRBV | M_CRSW_CC)) == 0) {
		if (to-- <= 0) {
			vs_print_addr(sc, xs);
			printf("command timeout, crsw 0x%x\n", crsw);

			if (canreset) {
				vs_reset(sc, bus);
				vs_resync(sc);
			}
			return 1;
		}
		delay(1000);
	}
#ifdef VS_DEBUG
	printf("%s: crsw %04x to %d/%d\n",
	    __func__, crsw, to, xs ? xs->timeout : 2000);
#endif
	return 0;
}

void
vs_poll(struct vs_softc *sc, struct vs_cb *cb)
{
	struct scsi_xfer *xs;
	int s;
	int rc;

	xs = cb->cb_xs;
	rc = do_vspoll(sc, xs, 1);

	s = splbio();
	if (rc != 0) {
		xs->error = XS_SELTIMEOUT;
		xs->status = -1;
#ifdef VS_DEBUG
		printf("%s: polled command timed out\n", __func__);
#endif
		vs_free(sc, cb);
		scsi_done(xs);
	} else
		vs_scsidone(sc, cb);
	splx(s);

	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER;
	CRB_CLR_DONE;

	vs_clear_return_info(sc);
}

void
thaw_queue(struct vs_softc *sc, int target)
{
	THAW(target);

	/* loop until thawed */
	while (THAW_REG & M_THAW_TWQE)
		;
}

void
thaw_all_queues(struct vs_softc *sc)
{
	int i;

	for (i = 1; i <= sc->sc_nwq; i++)
		thaw_queue(sc, i);
}

void
vs_scsidone(struct vs_softc *sc, struct vs_cb *cb)
{
	struct scsi_xfer *xs = cb->cb_xs;
	u_int32_t len;
	int error;

	len = vs_read(4, sh_RET_IOPB + IOPB_LENGTH);
	xs->resid = xs->datalen - len;

	error = vs_read(2, sh_RET_IOPB + IOPB_STATUS);
#ifdef VS_DEBUG
	printf("%s: queue %d, len %u (resid %d) error %d\n",
	    __func__, cb->cb_q, len, xs->resid, error);
	if (error != 0)
		printf("%s: last select %d %d, phase %02x %02x\n",
		    __func__, csb_read(1, CSB_LPDS), csb_read(1, CSB_LSDS),
		    csb_read(1, CSB_PPS), csb_read(1, CSB_SPS));
#endif
	if ((error & 0xff) == SCSI_SELECTION_TO) {
		xs->error = XS_SELTIMEOUT;
		xs->status = -1;
	} else {
		if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
			bus_dmamap_sync(sc->sc_dmat, cb->cb_dmamap, 0,
			    cb->cb_dmalen, (xs->flags & SCSI_DATA_IN) ?
			      BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, cb->cb_dmamap);
		}

		xs->status = error >> 8;
	}

	while (xs->status == SCSI_CHECK) {
		vs_chksense(cb, xs);
	}

	vs_free(sc, cb);
	scsi_done(xs);
}

void
vs_scsicmd(struct scsi_xfer *xs)
{
	struct scsi_link *slp = xs->sc_link;
	struct vs_softc *sc = slp->adapter_softc;
	int flags;
	bus_addr_t cqep, iopb;
	struct vs_cb *cb;
	int s;
	int rc;

	flags = xs->flags;
	if (flags & SCSI_POLL) {
		cb = sc->sc_cb;
		cqep = sh_MCE;
		iopb = sh_MCE_IOPB;

#ifdef VS_DEBUG
		if (mce_read(2, CQE_QECR) & M_QECR_GO)
			printf("%s: master command queue busy\n",
			    sc->sc_dev.dv_xname);
#endif
		/* Wait until we can use the command queue entry. */
		while (mce_read(2, CQE_QECR) & M_QECR_GO)
			;
#ifdef VS_DEBUG
		if (cb->cb_xs != NULL) {
			printf("%s: master command not idle\n",
			    sc->sc_dev.dv_xname);
			xs->error = XS_NO_CCB;
			s = splbio();
			scsi_done(xs);
			splx(s);
			return;
		}
#endif
		s = splbio();
	} else {
		s = splbio();
		cb = vs_find_queue(slp, sc);
		if (cb == NULL) {
			splx(s);
#ifdef VS_DEBUG
			printf("%s: queue for target %d is busy\n",
			    sc->sc_dev.dv_xname, slp->target);
#endif
			xs->error = XS_NO_CCB;
			s = splbio();
			scsi_done(xs);
			splx(s);
			return;
		}
		if (vs_getcqe(sc, &cqep, &iopb)) {
			/* XXX shouldn't happen since our queue is ready */
			splx(s);
#ifdef VS_DEBUG
			printf("%s: no free CQEs\n", sc->sc_dev.dv_xname);
#endif
			xs->error = XS_NO_CCB;
			s = splbio();
			scsi_done(xs);
			splx(s);
			return;
		}
	}

#ifdef VS_DEBUG
	printf("%s: sending SCSI command %02x (length %d) on queue %d\n",
	    __func__, xs->cmd->opcode, xs->cmdlen, cb->cb_q);
#endif
	rc = vs_load_command(sc, cb, cqep, iopb, slp, xs->flags,
	    xs->cmd, xs->cmdlen, xs->data, xs->datalen);
	if (rc != 0) {
		printf("%s: unable to load DMA map: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		splx(s);
		return;
	}

	vs_write(1, cqep + CQE_WORK_QUEUE, cb->cb_q);

	cb->cb_xs = xs;
	splx(s);

	vs_write(4, cqep + CQE_CTAG, (u_int32_t)cb);

	if (crb_read(2, CRB_CRSW) & M_CRSW_AQ)
		vs_write(2, cqep + CQE_QECR, M_QECR_AA | M_QECR_GO);
	else
		vs_write(2, cqep + CQE_QECR, M_QECR_GO);

	if (flags & SCSI_POLL) {
		/* poll for the command to complete */
		vs_poll(sc, cb);
	}
}

int
vs_load_command(struct vs_softc *sc, struct vs_cb *cb, bus_addr_t cqep,
    bus_addr_t iopb, struct scsi_link *slp, int flags,
    struct scsi_generic *cmd, int cmdlen, uint8_t *data, int datalen)
{
	unsigned int iopb_len;
	int option;
	int rc;

	/*
	 * We should only provide the iopb len if the controller is not
	 * able to compute it from the SCSI command group.
	 * Note that Jaguar has no knowledge of group 2.
	 */
	switch ((cmd->opcode) >> 5) {
	case 0:
	case 1:
	case 5:
		iopb_len = 0;
		break;
	case 2:
		if (sc->sc_bid == COUGAR)
			iopb_len = 0;
		else
		/* FALLTHROUGH */
	default:
		iopb_len = IOPB_SHORT_SIZE + ((cmdlen + 1) >> 1);
		break;
	}

	vs_bzero(iopb, IOPB_LONG_SIZE);
	bus_space_write_region_1(sc->sc_iot, sc->sc_ioh, iopb + IOPB_SCSI_DATA,
	    (u_int8_t *)cmd, cmdlen);
	vs_write(2, iopb + IOPB_CMD, IOPB_PASSTHROUGH);
	vs_write(2, iopb + IOPB_UNIT,
	  vs_unit_value(slp->flags & SDEV_2NDBUS, slp->target, slp->lun));
#ifdef VS_DEBUG
	printf("%s: target %d lun %d encoded as %04x\n",
	    __func__, slp->target, slp->lun, (u_int)
	    vs_unit_value(slp->flags & SDEV_2NDBUS, slp->target, slp->lun));
#endif
	vs_write(1, iopb + IOPB_NVCT, sc->sc_nvec);
	vs_write(1, iopb + IOPB_EVCT, sc->sc_evec);

	/*
	 * Setup DMA map for data transfer
	 */
	if (flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
		cb->cb_dmalen = (bus_size_t)datalen;
		rc = bus_dmamap_load(sc->sc_dmat, cb->cb_dmamap,
		    data, cb->cb_dmalen, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_STREAMING |
		    ((flags & SCSI_DATA_IN) ? BUS_DMA_READ : BUS_DMA_WRITE));
		if (rc != 0)
			return rc;

		bus_dmamap_sync(sc->sc_dmat, cb->cb_dmamap, 0,
		    cb->cb_dmalen, (flags & SCSI_DATA_IN) ?
		      BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	}

	option = 0;
	if (flags & SCSI_DATA_OUT)
		option |= M_OPT_DIR;
	if (slp->adapter_buswidth > 8)
		option |= M_OPT_GO_WIDE;

	if (flags & SCSI_POLL) {
		vs_write(2, iopb + IOPB_OPTION, option);
		vs_write(2, iopb + IOPB_LEVEL, 0);
	} else {
		vs_write(2, iopb + IOPB_OPTION, option | M_OPT_IE);
		vs_write(2, iopb + IOPB_LEVEL, sc->sc_ipl);
	}
	vs_write(2, iopb + IOPB_ADDR, ADDR_MOD);

	if (flags & (SCSI_DATA_IN | SCSI_DATA_OUT))
		vs_build_sg_list(sc, cb, iopb);

	vs_bzero(cqep, CQE_SIZE);
	vs_write(2, cqep + CQE_IOPB_ADDR, iopb);
	vs_write(1, cqep + CQE_IOPB_LENGTH, iopb_len);
	/* CQE_WORK_QUEUE to be filled by the caller */

	return 0;
}

void
vs_chksense(struct vs_cb *cb, struct scsi_xfer *xs)
{
	struct scsi_link *slp = xs->sc_link;
	struct vs_softc *sc = slp->adapter_softc;
	struct scsi_sense ss;
	int rc;
	int s;

#ifdef VS_DEBUG
	printf("%s: target %d\n", slp->target);
#endif
	/* ack and clear the error */
	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER;
	CRB_CLR_DONE;
	xs->status = 0;

	/* Wait until we can use the command queue entry. */
	while (mce_read(2, CQE_QECR) & M_QECR_GO)
		;

	bzero(&ss, sizeof ss);
	ss.opcode = REQUEST_SENSE;
	ss.byte2 = slp->lun << 5;
	ss.length = sizeof(xs->sense);

#ifdef VS_DEBUG
	printf("%s: sending SCSI command %02x (length %d) on queue %d\n",
	    __func__, ss.opcode, sizeof ss, 0);
#endif
	rc = vs_load_command(sc, cb, sh_MCE, sh_MCE_IOPB, slp,
	    SCSI_DATA_IN | SCSI_POLL,
	    (struct scsi_generic *)&ss, sizeof ss, (uint8_t *)&xs->sense,
	    sizeof(xs->sense));
	if (rc != 0) {
		printf("%s: unable to load DMA map: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		xs->error = XS_DRIVER_STUFFUP;
		xs->status = 0;
		return;
	}

	mce_write(1, CQE_WORK_QUEUE, 0);
	mce_write(2, CQE_QECR, M_QECR_GO);

	/* poll for the command to complete */
	s = splbio();
	do_vspoll(sc, xs, 1);
	xs->status = vs_read(2, sh_RET_IOPB + IOPB_STATUS) >> 8;
	splx(s);
}

int
vs_getcqe(struct vs_softc *sc, bus_addr_t *cqep, bus_addr_t *iopbp)
{
	bus_addr_t cqe, iopb;
	int qhdp;

	qhdp = mcsb_read(2, MCSB_QHDP);
	cqe = sh_CQE(qhdp);
	iopb = sh_IOPB(qhdp);

	if (vs_read(2, cqe + CQE_QECR) & M_QECR_GO) {
		/* queue still in use, should never happen */
		return EAGAIN;
	}

	if (++qhdp == NUM_CQE)
		qhdp = 0;
	mcsb_write(2, MCSB_QHDP, qhdp);

	vs_bzero(cqe, CQE_SIZE);
	*cqep = cqe;
	*iopbp = iopb;
	return (0);
}

int
vs_identify(struct vs_channel *vc, int cid)
{
	vc->vc_width = 0;
	vc->vc_type = VCT_UNKNOWN;

	if (vc->vc_id < 0)
		return (0);

	switch (cid) {
	case 0x00:
		vc->vc_width = 8;
		vc->vc_type = VCT_SE;
		break;
	case 0x01:
		vc->vc_width = 8;
		vc->vc_type = VCT_DIFFERENTIAL;
		break;
	case 0x02:
		vc->vc_width = 16;
		vc->vc_type = VCT_SE;
		break;
	case 0x03:
	case 0x0e:
		vc->vc_width = 16;
		vc->vc_type = VCT_DIFFERENTIAL;
		break;
	default:
		vc->vc_id = -1;
		return (0);
	}

	return (vc->vc_width - 1);
}

int
vs_initialize(struct vs_softc *sc)
{
	int i, msr, id, rc;
	u_int targets;

	/*
	 * Reset the board, and wait for it to get ready.
	 * The reset signal is applied for 70 usec, and the board status
	 * is not tested until 100 usec after the reset signal has been
	 * cleared, per the manual (MVME328/D1) pages 4-6 and 4-9.
	 */

	mcsb_write(2, MCSB_MCR, M_MCR_RES | M_MCR_SFEN);
	delay(70);
	mcsb_write(2, MCSB_MCR, M_MCR_SFEN);

	delay(100);
	i = 0;
	for (;;) {
		msr = mcsb_read(2, MCSB_MSR);
		if ((msr & (M_MSR_BOK | M_MSR_CNA)) == M_MSR_BOK)
			break;
		if (++i > 5000) {
			printf("board reset failed, status %x\n", msr);
			return 1;
		}
		delay(1000);
	}

	/* describe the board */
	switch (sc->sc_bid) {
	default:
	case JAGUAR:
		printf("Jaguar");
		break;
	case COUGAR:
		id = csb_read(1, CSB_EXTID);
		switch (id) {
		case 0x00:
			printf("Cougar");
			break;
		case 0x02:
			printf("Cougar II");
			break;
		default:
			printf("unknown Cougar version %02x", id);
			break;
		}
		break;
	}

	/* initialize channels id */
	sc->sc_channel[0].vc_id = csb_read(1, CSB_PID);
	sc->sc_channel[1].vc_id = -1;
	switch (id = csb_read(1, CSB_DBID)) {
	case DBID_SCSI2:
	case DBID_SCSI:
		sc->sc_channel[1].vc_id = csb_read(1, CSB_SID);
		break;
	case DBID_PRINTER:
		printf(", printer port");
		break;
	case DBID_NONE:
		break;
	default:
		printf(", unknown daughterboard id %x", id);
		break;
	}

	printf("\n");

	/*
	 * On cougar boards, find how many work queues we can use,
	 * and whether we are on wide or narrow buses.
	 */
	switch (sc->sc_bid) {
	case COUGAR:
		sc->sc_nwq = csb_read(2, CSB_NWQ);
		/*
		 * Despite what the documentation says, this value is not
		 * always provided. If it is invalid, decide on the number
		 * of available work queues from the memory size, as the
		 * firmware does.
		 */
#ifdef VS_DEBUG
		printf("%s: controller reports %d work queues\n",
		    __func__, sc->sc_nwq);
#endif
		if (sc->sc_nwq != 0x0f && sc->sc_nwq != 0xff) {
			if (csb_read(2, CSB_BSIZE) >= 0x0100)
				sc->sc_nwq = 0xff;	/* >= 256KB, 255 WQ */
			else
				sc->sc_nwq = 0x0f;	/* < 256KB, 15 WQ */
		}
#ifdef VS_DEBUG
		printf("%s: driver deducts %d work queues\n",
		    __func__, sc->sc_nwq);
#endif
		if (sc->sc_nwq > NUM_WQ)
			sc->sc_nwq = NUM_WQ;

		targets = vs_identify(&sc->sc_channel[0],
		    csb_read(1, CSB_PFECID));
		targets += vs_identify(&sc->sc_channel[1],
		    csb_read(1, CSB_SFECID));

		if (sc->sc_nwq > targets)
			sc->sc_nwq = targets;
		else {
			/*
			 * We can't drive the daugther board if there is not
			 * enough on-board memory for all the work queues.
			 * XXX This might work by moving everything off-board?
			 */
			if (sc->sc_nwq < targets)
				sc->sc_channel[1].vc_width = 0;
		}
		break;
	default:
	case JAGUAR:
		sc->sc_nwq = JAGUAR_MAX_WQ;
		sc->sc_channel[0].vc_width = sc->sc_channel[1].vc_width = 8;
		break;
	}

	CRB_CLR_DONE;
	mcsb_write(2, MCSB_QHDP, 0);

	vs_bzero(sh_CIB, CIB_SIZE);
	cib_write(2, CIB_NCQE, NUM_CQE);
	cib_write(2, CIB_BURST, 0);
	cib_write(2, CIB_NVECT, (sc->sc_ipl << 8) | sc->sc_nvec);
	cib_write(2, CIB_EVECT, (sc->sc_ipl << 8) | sc->sc_evec);
	cib_write(2, CIB_PID, 0x08);	/* use default */
	cib_write(2, CIB_SID, 0x08);	/* use default */
	cib_write(2, CIB_CRBO, sh_CRB);
	cib_write(4, CIB_SELECT, SELECTION_TIMEOUT);
	cib_write(4, CIB_WQTIMO, 4);
	cib_write(4, CIB_VMETIMO, 0 /* VME_BUS_TIMEOUT */);
	cib_write(2, CIB_ERR_FLGS, M_ERRFLGS_RIN | M_ERRFLGS_RSE);
	cib_write(2, CIB_SBRIV, (sc->sc_ipl << 8) | sc->sc_evec);
	cib_write(1, CIB_SOF0, 0x15);
	cib_write(1, CIB_SRATE0, 100 / 4);
	cib_write(1, CIB_SOF1, 0);
	cib_write(1, CIB_SRATE1, 0);

	vs_bzero(sh_MCE_IOPB, IOPB_LONG_SIZE);
	mce_iopb_write(2, IOPB_CMD, CNTR_INIT);
	mce_iopb_write(2, IOPB_OPTION, 0);
	mce_iopb_write(1, IOPB_NVCT, sc->sc_nvec);
	mce_iopb_write(1, IOPB_EVCT, sc->sc_evec);
	mce_iopb_write(2, IOPB_LEVEL, 0 /* sc->sc_ipl */);
	mce_iopb_write(2, IOPB_ADDR, SHIO_MOD);
	mce_iopb_write(4, IOPB_BUFF, sh_CIB);
	mce_iopb_write(4, IOPB_LENGTH, CIB_SIZE);

	vs_bzero(sh_MCE, CQE_SIZE);
	mce_write(2, CQE_IOPB_ADDR, sh_MCE_IOPB);
	mce_write(1, CQE_IOPB_LENGTH, 0);
	mce_write(1, CQE_WORK_QUEUE, 0);
	mce_write(2, CQE_QECR, M_QECR_GO);

	/* poll for the command to complete */
	do_vspoll(sc, NULL, 1);

	if ((rc = vs_alloc_sg(sc)) != 0)
		return rc;

	if ((rc = vs_alloc_wq(sc)) != 0)
		return rc;

	/* initialize work queues */
#ifdef VS_DEBUG
	printf("%s: initializing %d work queues\n",
	    __func__, sc->sc_nwq);
#endif

	for (i = 1; i <= sc->sc_nwq; i++) {
		/* Wait until we can use the command queue entry. */
		while (mce_read(2, CQE_QECR) & M_QECR_GO)
			;

		vs_bzero(sh_MCE_IOPB, IOPB_LONG_SIZE);
		mce_iopb_write(2, WQCF_CMD, CNTR_INIT_WORKQ);
		mce_iopb_write(2, WQCF_OPTION, 0);
		mce_iopb_write(1, WQCF_NVCT, sc->sc_nvec);
		mce_iopb_write(1, WQCF_EVCT, sc->sc_evec);
		mce_iopb_write(2, WQCF_ILVL, 0 /* sc->sc_ipl */);
		mce_iopb_write(2, WQCF_WORKQ, i);
		mce_iopb_write(2, WQCF_WOPT, M_WOPT_FE | M_WOPT_IWQ);
		if (sc->sc_bid == JAGUAR)
			mce_iopb_write(2, WQCF_SLOTS, JAGUAR_MAX_Q_SIZ);
		mce_iopb_write(4, WQCF_CMDTO, 4);	/* 1 second */
		if (sc->sc_bid != JAGUAR)
			mce_iopb_write(2, WQCF_UNIT,
			    vs_unit_value(i > sc->sc_channel[0].vc_width,
				i - sc->sc_channel[0].vc_width, 0));

		vs_bzero(sh_MCE, CQE_SIZE);
		mce_write(2, CQE_IOPB_ADDR, sh_MCE_IOPB);
		mce_write(1, CQE_IOPB_LENGTH, 0);
		mce_write(1, CQE_WORK_QUEUE, 0);
		mce_write(2, CQE_QECR, M_QECR_GO);

		/* poll for the command to complete */
		do_vspoll(sc, NULL, 1);
		if (CRSW & M_CRSW_ER) {
			printf("%s: work queue %d initialization error 0x%x\n",
			    sc->sc_dev.dv_xname, i,
			    vs_read(2, sh_RET_IOPB + IOPB_STATUS));
			return ENXIO;
		}
		CRB_CLR_DONE;
	}

	/* start queue mode */
	mcsb_write(2, MCSB_MCR, mcsb_read(2, MCSB_MCR) | M_MCR_SQM);

	/* reset all SCSI buses */
	vs_reset(sc, -1);
	/* sync all devices */
	vs_resync(sc);

	return 0;
}

/*
 * Allocate memory for the scatter/gather lists.
 *
 * Since vs_minphys() makes sure we won't need more than flat lists of
 * up to MAX_SG_ELEMENTS entries, we need to allocate storage for one
 * such list per work queue.
 */
int
vs_alloc_sg(struct vs_softc *sc)
{
	size_t sglen;
	int nseg;
	int rc;

	sglen = sc->sc_nwq * MAX_SG_ELEMENTS * sizeof(struct vs_sg_entry);
	sglen = round_page(sglen);

	rc = bus_dmamem_alloc(sc->sc_dmat, sglen, 0, 0,
	    &sc->sc_sgseg, 1, &nseg, BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to allocate s/g memory: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail1;
	}
	rc = bus_dmamem_map(sc->sc_dmat, &sc->sc_sgseg, nseg, sglen,
	    (caddr_t *)&sc->sc_sgva, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (rc != 0) {
		printf("%s: unable to map s/g memory: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail2;
	}
	rc = bus_dmamap_create(sc->sc_dmat, sglen, 1, sglen, 0,
	    BUS_DMA_NOWAIT /* | BUS_DMA_ALLOCNOW */, &sc->sc_sgmap);
	if (rc != 0) {
		printf("%s: unable to create s/g dma map: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail3;
	}
	rc = bus_dmamap_load(sc->sc_dmat, sc->sc_sgmap, sc->sc_sgva,
	    sglen, NULL, BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: unable to load s/g dma map: error %d\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail4;
	}

	return 0;

fail4:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_sgmap);
fail3:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_sgva, PAGE_SIZE);
fail2:
	bus_dmamem_free(sc->sc_dmat, &sc->sc_sgseg, 1);
fail1:
	return rc;
}

/*
 * Allocate one command block per work qeue.
 */
int
vs_alloc_wq(struct vs_softc *sc)
{
	struct vs_cb *cb;
	u_int i;
	int rc;

	sc->sc_cb = malloc(sc->sc_nwq * sizeof(struct vs_cb), M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (sc->sc_cb == NULL) {
		printf("%s: unable to allocate %d work queues\n",
		    sc->sc_dev.dv_xname, sc->sc_nwq);
		return ENOMEM;
	}

	for (i = 0, cb = sc->sc_cb; i <= sc->sc_nwq; i++, cb++) {
		cb->cb_q = i;

		rc = bus_dmamap_create(sc->sc_dmat, ptoa(MAX_SG_ELEMENTS),
		    MAX_SG_ELEMENTS, MAX_SG_ELEMENT_SIZE, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &cb->cb_dmamap);
		if (rc != 0) {
			printf("%s: unable to create dma map for queue %d"
			    ": error %d\n",
			    sc->sc_dev.dv_xname, i, rc);
			goto fail;
		}
	}

	return 0;

fail:
	while (i != 0) {
		i--; cb--;
		bus_dmamap_destroy(sc->sc_dmat, cb->cb_dmamap);
	}
	free(sc->sc_cb, M_DEVBUF);
	sc->sc_cb = NULL;

	return rc;
}

void
vs_resync(struct vs_softc *sc)
{
	struct vs_channel *vc;
	int bus, target;

	for (bus = 0; bus < 2; bus++) {
		vc = &sc->sc_channel[bus];
		if (vc->vc_id < 0 || vc->vc_width == 0)
			break;

		for (target = 0; target < vc->vc_width; target++) {
			if (target == vc->vc_id)
				continue;

			/* Wait until we can use the command queue entry. */
			while (mce_read(2, CQE_QECR) & M_QECR_GO)
				;

			vs_bzero(sh_MCE_IOPB, IOPB_SHORT_SIZE);
			mce_iopb_write(2, DRCF_CMD, CNTR_DEV_REINIT);
			mce_iopb_write(2, DRCF_OPTION, 0); /* prefer polling */
			mce_iopb_write(1, DRCF_NVCT, sc->sc_nvec);
			mce_iopb_write(1, DRCF_EVCT, sc->sc_evec);
			mce_iopb_write(2, DRCF_ILVL, 0);
			mce_iopb_write(2, DRCF_UNIT,
			    vs_unit_value(bus, target, 0));

			vs_bzero(sh_MCE, CQE_SIZE);
			mce_write(2, CQE_IOPB_ADDR, sh_MCE_IOPB);
			mce_write(1, CQE_IOPB_LENGTH, 0);
			mce_write(1, CQE_WORK_QUEUE, 0);
			mce_write(2, CQE_QECR, M_QECR_GO);

			/* poll for the command to complete */
			do_vspoll(sc, NULL, 0);
			if (CRSW & M_CRSW_ER)
				CRB_CLR_ER;
			CRB_CLR_DONE;
		}
	}
}

void
vs_reset(struct vs_softc *sc, int bus)
{
	int b, s;

	s = splbio();

	for (b = 0; b < 2; b++) {
		if (bus >= 0 && b != bus)
			continue;

		/* Wait until we can use the command queue entry. */
		while (mce_read(2, CQE_QECR) & M_QECR_GO)
			;

		vs_bzero(sh_MCE_IOPB, IOPB_SHORT_SIZE);
		mce_iopb_write(2, SRCF_CMD, IOPB_RESET);
		mce_iopb_write(2, SRCF_OPTION, 0);	/* prefer polling */
		mce_iopb_write(1, SRCF_NVCT, sc->sc_nvec);
		mce_iopb_write(1, SRCF_EVCT, sc->sc_evec);
		mce_iopb_write(2, SRCF_ILVL, 0);
		mce_iopb_write(2, SRCF_BUSID, b << 15);

		vs_bzero(sh_MCE, CQE_SIZE);
		mce_write(2, CQE_IOPB_ADDR, sh_MCE_IOPB);
		mce_write(1, CQE_IOPB_LENGTH, 0);
		mce_write(1, CQE_WORK_QUEUE, 0);
		mce_write(2, CQE_QECR, M_QECR_GO);

		/* poll for the command to complete */
		for (;;) {
			do_vspoll(sc, NULL, 0);
			/* ack & clear scsi error condition cause by reset */
			if (CRSW & M_CRSW_ER) {
				CRB_CLR_DONE;
				vs_write(2, sh_RET_IOPB + IOPB_STATUS, 0);
				break;
			}
			CRB_CLR_DONE;
		}
	}

	thaw_all_queues(sc);

	splx(s);
}

/* free a cb and thaw its queue; invoked at splbio */
static __inline__ void
vs_free(struct vs_softc *sc, struct vs_cb *cb)
{
	if (cb->cb_q != 0)
		thaw_queue(sc, cb->cb_q);
	cb->cb_xs = NULL;
}

/* normal interrupt routine */
int
vs_nintr(void *vsc)
{
	struct vs_softc *sc = (struct vs_softc *)vsc;
	struct vs_cb *cb;
	int s;

#if 0	/* bogus! */
	if ((CRSW & CONTROLLER_ERROR) == CONTROLLER_ERROR)
		return vs_eintr(sc);
#endif

	/* Got a valid interrupt on this device */
	s = splbio();
	cb = (struct vs_cb *)crb_read(4, CRB_CTAG);

	/*
	 * If this is a controller error, there won't be a cb
	 * pointer in the CTAG field.  Bad things happen if you try
	 * to point to address 0.  But then, we should have caught
	 * the controller error above.
	 */
	if (cb != NULL) {
#ifdef VS_DEBUG
		printf("%s: interrupt for queue %d\n", __func__, cb->cb_q);
#endif
		vs_scsidone(sc, cb);
	} else {
#ifdef VS_DEBUG
		printf("%s: normal interrupt but no related command???\n",
		    __func__);
#endif
	}

	/* ack the interrupt */
	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER;
	CRB_CLR_DONE;

	vs_clear_return_info(sc);
	splx(s);

	return 1;
}

/* error interrupts */
int
vs_eintr(void *vsc)
{
	struct vs_softc *sc = (struct vs_softc *)vsc;
	struct vs_cb *cb;
	struct scsi_xfer *xs;
	int crsw, ecode;
	int s;

	/* Got a valid interrupt on this device */
	s = splbio();

	crsw = vs_read(2, sh_CEVSB + CEVSB_CRSW);
	ecode = vs_read(1, sh_CEVSB + CEVSB_ERROR);
	cb = (struct vs_cb *)crb_read(4, CRB_CTAG);
	xs = cb != NULL ? cb->cb_xs : NULL;

#ifdef VS_DEBUG
	printf("%s: error interrupt, crsw %04x, error %d, queue %d\n",
	    __func__, (u_int)crsw, ecode, cb ? cb->cb_q : -1);
#endif
	vs_print_addr(sc, xs);

	if (crsw & M_CRSW_RST) {
		printf("bus reset\n");
	} else {
		switch (ecode) {
		case CEVSB_ERR_TYPE:
			printf("IOPB type error\n");
			break;
		case CEVSB_ERR_TO:
			printf("timeout\n");
			break;
		case CEVSB_ERR_TR:
			printf("reconnect error\n");
			break;
		case CEVSB_ERR_OF:
			printf("overflow\n");
			break;
		case CEVSB_ERR_BD:
			printf("bad direction\n");
			break;
		case CEVSB_ERR_NR:
			printf("non-recoverable error\n");
			break;
		case CEVSB_ERR_PANIC:
			printf("board panic\n");
			break;
		default:
			printf("unexpected error %x\n", ecode);
			break;
		}
	}

	if (xs != NULL) {
		xs->error = XS_SELTIMEOUT;
		xs->status = -1;
		scsi_done(xs);
	}

	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER;
	CRB_CLR_DONE;

	thaw_all_queues(sc);
	vs_clear_return_info(sc);
	splx(s);

	return 1;
}

static void
vs_clear_return_info(struct vs_softc *sc)
{
	vs_bzero(sh_RET_IOPB, CRB_SIZE + IOPB_LONG_SIZE);
}

/*
 * Choose the first available work queue (invoked at splbio).
 * We used a simple round-robin mechanism which is faster than rescanning
 * from the beginning if we have more than one target on the bus.
 */
struct vs_cb *
vs_find_queue(struct scsi_link *sl, struct vs_softc *sc)
{
	struct vs_cb *cb;
	u_int q;

	/*
	 * Map the target number (0-7/15) to the 1-7/15 range, target 0
	 * picks the host adapter target number (since host adapter
	 * commands are issued on queue #0).
	 */
	q = sl->target;
	if (q == 0)
		q = sl->adapter_target;
	if (sl->flags & SDEV_2NDBUS)
		q += sc->sc_channel[0].vc_width - 1; /* map to 8-14 or 16-30 */

	if ((cb = sc->sc_cb + q)->cb_xs == NULL)
		return (cb);

	return (NULL);
}

/*
 * Encode a specific target.
 */
int
vs_unit_value(int bus, int tgt, int lun)
{
	int unit = 0;

	if (bus != 0)
		unit |= M_UNIT_BUS;	/* secondary bus */

	if (tgt > 7 || lun > 7) {
		/* extended addressing (for Cougar II-Wide only) */
		unit |= M_UNIT_EXT;
		unit |= (lun & 0x3f) << 8;
		unit |= (tgt & 0x0f) << 0;
	} else {
		unit |= lun << 3;
		unit |= tgt << 0;
	}

	return (unit);
}

/*
 * Build the scatter/gather list for the given control block and update
 * its IOPB.
 */
void
vs_build_sg_list(struct vs_softc *sc, struct vs_cb *cb, bus_addr_t iopb)
{
	struct vs_sg_entry *sgentry;
	int segno;
	bus_dma_segment_t *seg = cb->cb_dmamap->dm_segs;
	bus_size_t sgoffs;
	bus_size_t len;

	/*
	 * No need to build a scatter/gather chain if there is only
	 * one contiguous physical area.
	 */
	if (cb->cb_dmamap->dm_nsegs == 1) {
		vs_write(4, iopb + IOPB_BUFF, seg->ds_addr);
		vs_write(4, iopb + IOPB_LENGTH, cb->cb_dmalen);
		return;
	}

	/*
	 * Otherwise, we need to build the flat s/g list.
	 */

	sgentry = sc->sc_sgva + cb->cb_q * MAX_SG_ELEMENTS;
	sgoffs = (vaddr_t)sgentry - (vaddr_t)sc->sc_sgva;

	len = cb->cb_dmalen;
	for (segno = 0; segno < cb->cb_dmamap->dm_nsegs; seg++, segno++) {
		if (seg->ds_len > len) {
			sgentry->count.bytes = htobe16(len);
			len = 0;
		} else {
			sgentry->count.bytes = htobe16(seg->ds_len);
			len -= seg->ds_len;
		}
		sgentry->pa_high = htobe16(seg->ds_addr >> 16);
		sgentry->pa_low = htobe16(seg->ds_addr & 0xffff);
		sgentry->addr = htobe16(ADDR_MOD);
		sgentry++;
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_sgmap, sgoffs,
	    cb->cb_dmamap->dm_nsegs * sizeof(struct vs_sg_entry),
	    BUS_DMASYNC_PREWRITE);

	vs_write(2, iopb + IOPB_OPTION,
	    vs_read(2, iopb + IOPB_OPTION) | M_OPT_SG);
	vs_write(2, iopb + IOPB_ADDR,
	    vs_read(2, iopb + IOPB_ADDR) | M_ADR_SG_LINK);
	vs_write(4, iopb + IOPB_BUFF,
	    sc->sc_sgmap->dm_segs[0].ds_addr + sgoffs);
	vs_write(4, iopb + IOPB_LENGTH, cb->cb_dmamap->dm_nsegs);
	vs_write(4, iopb + IOPB_SGTTL, cb->cb_dmalen);
}
