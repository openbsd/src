/*	$OpenBSD: vs.c,v 1.30 2004/04/16 23:35:50 miod Exp $ */

/*
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
 * MVME328S scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/malloc.h>

#include <uvm/uvm_param.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <machine/param.h>

#include <mvme88k/dev/vsreg.h>
#include <mvme88k/dev/vsvar.h>
#include <mvme88k/dev/vme.h>
#include <machine/cmmu.h>

int	vsmatch(struct device *, void *, void *);
void	vsattach(struct device *, struct device *, void *);
int	vs_scsicmd(struct scsi_xfer *);

struct scsi_adapter vs_scsiswitch = {
	vs_scsicmd,
	minphys,
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

int	do_vspoll(struct vs_softc *, int, int);
void	thaw_queue(struct vs_softc *, u_int8_t);
M328_SG	vs_alloc_scatter_gather(void);
M328_SG	vs_build_memory_structure(struct scsi_xfer *, M328_IOPB *);
int	vs_checkintr(struct vs_softc *, struct scsi_xfer *, int *);
void	vs_chksense(struct scsi_xfer *);
void	vs_dealloc_scatter_gather(M328_SG);
int	vs_eintr(void *);
M328_CQE *vs_getcqe(struct vs_softc *);
M328_IOPB *vs_getiopb(struct vs_softc *);
int	vs_initialize(struct vs_softc *);
int	vs_intr(struct vs_softc *);
void	vs_link_sg_element(sg_list_element_t *, vaddr_t, int);
void	vs_link_sg_list(sg_list_element_t *, vaddr_t, int);
int	vs_nintr(void *);
int	vs_poll(struct vs_softc *, struct scsi_xfer *);
void	vs_reset(struct vs_softc *);
void	vs_resync(struct vs_softc *);
void	vs_scsidone(struct vs_softc *, struct scsi_xfer *, int);

static __inline__ void vs_clear_return_info(struct vs_softc *);

int
vsmatch(pdp, vcf, args)
	struct device *pdp;
	void *vcf, *args;
{
	struct confargs *ca = args;

	return (!badvaddr((unsigned)ca->ca_vaddr, 1));
}

void
vsattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct vs_softc *sc = (struct vs_softc *)self;
	struct confargs *ca = args;
	struct vsreg *rp;
	int evec;
	int tmp;

	/* get the next available vector for the error interrupt */
	evec = vme_findvec(ca->ca_vec);

	if (ca->ca_vec < 0 || evec < 0) {
		printf(": no more interrupts!\n");
		return;
	}
	if (ca->ca_ipl < 0)
		ca->ca_ipl = IPL_BIO;

	printf(" vec 0x%x", evec);

	sc->sc_vsreg = rp = ca->ca_vaddr;

	sc->sc_ipl = ca->ca_ipl;
	sc->sc_nvec = ca->ca_vec;
	sc->sc_evec = evec;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = 7;
	sc->sc_link.adapter = &vs_scsiswitch;
	sc->sc_link.device = &vs_scsidev;
	sc->sc_link.luns = 1;
	sc->sc_link.openings = roundup(NUM_IOPB, 8) / 8;

	sc->sc_ih_n.ih_fn = vs_nintr;
	sc->sc_ih_n.ih_arg = sc;
	sc->sc_ih_n.ih_wantframe = 0;
	sc->sc_ih_n.ih_ipl = ca->ca_ipl;

	sc->sc_ih_e.ih_fn = vs_eintr;
	sc->sc_ih_e.ih_arg = sc;
	sc->sc_ih_e.ih_wantframe = 0;
	sc->sc_ih_e.ih_ipl = ca->ca_ipl;

	if (vs_initialize(sc))
		return;

	vmeintr_establish(sc->sc_nvec, &sc->sc_ih_n);
	vmeintr_establish(sc->sc_evec, &sc->sc_ih_e);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt_n);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt_e);

	/*
	 * attach all scsi units on us, watching for boot device
	 * (see dk_establish).
	 */
	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;		/* invalid flag to dk_establish */
	config_found(self, &sc->sc_link, scsiprint);
	bootpart = tmp;		    /* restore old value */
}

int
do_vspoll(sc, to, canreset)
	struct vs_softc *sc;
	int to;
	int canreset;
{
	int i;
	if (to <= 0 ) to = 50000;
	/* use cmd_wait values? */
	i = 10000;
	/*spl0();*/
	while (!(CRSW & (M_CRSW_CRBV | M_CRSW_CC))) {
		if (--i <= 0) {
#ifdef SDEBUG2
			printf ("waiting: timeout %d crsw 0x%x\n", to, CRSW);
#endif
			i = 50000;
			--to;
			if (to <= 0) {
				/*splx(s);*/
				if (canreset) {
					vs_reset(sc);
					vs_resync(sc);
				}
				printf ("timed out: timeout %d crsw 0x%x\n", to, CRSW);
				return 1;
			}
		}
	}
	return 0;
}

int
vs_poll(sc, xs)
	struct vs_softc *sc;
	struct scsi_xfer *xs;
{
	int status;
	int to;

	/*s = splbio();*/
	to = xs->timeout / 1000;
	for (;;) {
		if (do_vspoll(sc, to, 1)) {
			xs->error = XS_SELTIMEOUT;
			xs->status = -1;
			xs->flags |= ITSDONE;
			/* clear the return information */
			vs_clear_return_info(sc);
			if (xs->flags & SCSI_POLL)
				return (COMPLETE);
			break;
		}
		if (vs_checkintr(sc, xs, &status)) {
			vs_scsidone(sc, xs, status);
		}
		if (CRSW & M_CRSW_ER)
			CRB_CLR_ER(CRSW);
		CRB_CLR_DONE(CRSW);
		if (xs->flags & ITSDONE) break;
	}
	/* clear the return information */
	vs_clear_return_info(sc);
	return (COMPLETE);
}

void
thaw_queue(sc, target)
	struct vs_softc *sc;
	u_int8_t target;
{
	u_short t;
	t = target << 8;
	t |= 0x0001;
	THAW_REG = t;
	/* loop until thawed */
	while (THAW_REG & 0x01);
}

void
vs_scsidone(sc, xs, stat)
	struct vs_softc *sc;
	struct scsi_xfer *xs;
	int stat;
{
	int tgt;
	xs->status = stat;

	while (xs->status == SCSI_CHECK) {
		vs_chksense(xs);
		tgt = xs->sc_link->target + 1;
		thaw_queue(sc, tgt);
	}

	tgt = xs->sc_link->target + 1;
	xs->flags |= ITSDONE;
	/*sc->sc_tinfo[slp->target].cmds++;*/

	/* thaw all work queues */
	thaw_queue(sc, tgt);
	scsi_done(xs);
}

int
vs_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *slp = xs->sc_link;
	struct vs_softc *sc = slp->adapter_softc;
	int flags;
	unsigned long buf, len;
	u_short iopb_len;
	M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
	M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
	M328_IOPB *miopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;
	M328_CQE *cqep;
	M328_IOPB *iopb;
	M328_CMD *m328_cmd;

	flags = xs->flags;

#ifdef SDEBUG
	printf("scsi_cmd() ");
	if (xs->cmd->opcode == 0) {
		printf("TEST_UNIT_READY ");
	} else if (xs->cmd->opcode == REQUEST_SENSE) {
		printf("REQUEST_SENSE   ");
	} else if (xs->cmd->opcode == INQUIRY) {
		printf("INQUIRY         ");
	} else if (xs->cmd->opcode == MODE_SELECT) {
		printf("MODE_SELECT     ");
	} else if (xs->cmd->opcode == MODE_SENSE) {
		printf("MODE_SENSE      ");
	} else if (xs->cmd->opcode == START_STOP) {
		printf("START_STOP      ");
	} else if (xs->cmd->opcode == RESERVE) {
		printf("RESERVE         ");
	} else if (xs->cmd->opcode == RELEASE) {
		printf("RELEASE         ");
	} else if (xs->cmd->opcode == PREVENT_ALLOW) {
		printf("PREVENT_ALLOW   ");
	} else if (xs->cmd->opcode == POSITION_TO_ELEMENT) {
		printf("POSITION_TO_EL  ");
	} else if (xs->cmd->opcode == CHANGE_DEFINITION) {
		printf("CHANGE_DEF      ");
	} else if (xs->cmd->opcode == MODE_SENSE_BIG) {
		printf("MODE_SENSE_BIG  ");
	} else if (xs->cmd->opcode == MODE_SELECT_BIG) {
		printf("MODE_SELECT_BIG ");
	} else if (xs->cmd->opcode == 0x25) {
		printf("READ_CAPACITY   ");
	} else if (xs->cmd->opcode == 0x08) {
		printf("READ_COMMAND    ");
	}
#endif
	if (flags & SCSI_POLL) {
		cqep = mc;
		iopb = miopb;
	} else {
		cqep = vs_getcqe(sc);
		iopb = vs_getiopb(sc);
	}
	if (cqep == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}

	iopb_len = sizeof(M328_short_IOPB) + xs->cmdlen;
	d16_bzero(iopb, sizeof(M328_IOPB));

	d16_bcopy(xs->cmd, &iopb->iopb_SCSI[0], xs->cmdlen);
	iopb->iopb_CMD = IOPB_SCSI;
	iopb->iopb_UNIT = slp->lun << 3;
	iopb->iopb_UNIT |= slp->target;
	iopb->iopb_NVCT = (u_char)sc->sc_nvec;
	iopb->iopb_EVCT = (u_char)sc->sc_evec;

	/*
	 * Since the 88k doesn't support cache snooping, we have
	 * to flush the cache for a write and flush with inval for
	 * a read, prior to starting the IO.
	 */
	if (xs->flags & SCSI_DATA_IN) {	 /* read */
		dma_cachectl((vaddr_t)xs->data, xs->datalen,
			     DMA_CACHE_SYNC_INVAL);
		iopb->iopb_OPTION |= OPT_READ;
	} else {			 /* write */
		dma_cachectl((vaddr_t)xs->data, xs->datalen,
			     DMA_CACHE_SYNC);
		iopb->iopb_OPTION |= OPT_WRITE;
	}

	if (flags & SCSI_POLL) {
		iopb->iopb_OPTION |= OPT_INTDIS;
		iopb->iopb_LEVEL = 0;
	} else {
		iopb->iopb_OPTION |= OPT_INTEN;
		iopb->iopb_LEVEL = sc->sc_ipl;
	}
	iopb->iopb_ADDR = ADDR_MOD;

	/*
	 * Wait until we can use the command queue entry.
	 * Should only have to wait if the master command
	      * queue entry is busy and we are polling.
	 */
	while (cqep->cqe_QECR & M_QECR_GO);

	cqep->cqe_IOPB_ADDR = OFF(iopb);
	cqep->cqe_IOPB_LENGTH = iopb_len;
	if (flags & SCSI_POLL) {
		cqep->cqe_WORK_QUEUE = 0;
	} else {
		cqep->cqe_WORK_QUEUE = slp->target + 1;
	}

	MALLOC(m328_cmd, M328_CMD*, sizeof(M328_CMD), M_DEVBUF, M_WAITOK);

	m328_cmd->xs = xs;
	if (xs->datalen) {
		m328_cmd->top_sg_list = vs_build_memory_structure(xs, iopb);
	} else {
		m328_cmd->top_sg_list = (M328_SG)0;
	}

	LV(cqep->cqe_CTAG, m328_cmd);

	if (crb->crb_CRSW & M_CRSW_AQ) {
		cqep->cqe_QECR = M_QECR_AA;
	}
	VL(buf, iopb->iopb_BUFF);
	VL(len, iopb->iopb_LENGTH);
#ifdef SDEBUG
	printf("tgt %d lun %d buf %x len %d wqn %d ipl %d crsw 0x%x\n",
	       slp->target, slp->lun, buf, len, cqep->cqe_WORK_QUEUE,
	       iopb->iopb_LEVEL, crb->crb_CRSW);
#endif
	cqep->cqe_QECR |= M_QECR_GO;

	if (flags & SCSI_POLL) {
		/* poll for the command to complete */
		vs_poll(sc, xs);
		return (COMPLETE);
	}
	return (SUCCESSFULLY_QUEUED);
}

void
vs_chksense(xs)
	struct scsi_xfer *xs;
{
	int s;
	struct scsi_link *slp = xs->sc_link;
	struct vs_softc *sc = slp->adapter_softc;
	struct scsi_sense *ss;
	M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
	M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	M328_IOPB *miopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;

	/* ack and clear the error */
	CRB_CLR_ER(CRSW);
	CRB_CLR_DONE(CRSW);
	xs->status = 0;

	d16_bzero(miopb, sizeof(M328_IOPB));
	/* This is a command, so point to it */
	ss = (void *)&miopb->iopb_SCSI[0];
	d16_bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = slp->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);

	miopb->iopb_CMD = IOPB_SCSI;
	miopb->iopb_OPTION = OPT_READ;
	miopb->iopb_NVCT = (u_char)sc->sc_nvec;
	miopb->iopb_EVCT = (u_char)sc->sc_evec;
	miopb->iopb_LEVEL = 0; /*sc->sc_ipl;*/
	miopb->iopb_ADDR = ADDR_MOD;
	LV(miopb->iopb_BUFF, kvtop((vaddr_t)&xs->sense));
	LV(miopb->iopb_LENGTH, sizeof(struct scsi_sense_data));

	d16_bzero(mc, sizeof(M328_CQE));
	mc->cqe_IOPB_ADDR = OFF(miopb);
	mc->cqe_IOPB_LENGTH = sizeof(M328_short_IOPB) +
			      sizeof(struct scsi_sense);
	mc->cqe_WORK_QUEUE = 0;
	mc->cqe_QECR = M_QECR_GO;
	/* poll for the command to complete */
	s = splbio();
	do_vspoll(sc, 0, 1);
	/*
	if (xs->cmd->opcode != PREVENT_ALLOW) {
	   xs->error = XS_SENSE;
	}
	*/
	xs->status = riopb->iopb_STATUS >> 8;
#ifdef SDEBUG
	scsi_print_sense(xs, 2);
#endif
	splx(s);
}

M328_CQE *
vs_getcqe(sc)
	struct vs_softc *sc;
{
	M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
	M328_CQE *cqep;

	cqep = (M328_CQE *)&sc->sc_vsreg->sh_CQE[mcsb->mcsb_QHDP];

	if (cqep->cqe_QECR & M_QECR_GO)
		return NULL; /* Hopefully, this will never happen */
	mcsb->mcsb_QHDP++;
	if (mcsb->mcsb_QHDP == NUM_CQE)	mcsb->mcsb_QHDP = 0;
	d16_bzero(cqep, sizeof(M328_CQE));
	return cqep;
}

M328_IOPB *
vs_getiopb(sc)
	struct vs_softc *sc;
{
	M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
	M328_IOPB *iopb;
	int slot;

	if (mcsb->mcsb_QHDP == 0) {
		slot = NUM_CQE;
	} else {
		slot = mcsb->mcsb_QHDP - 1;
	}
	iopb = (M328_IOPB *)&sc->sc_vsreg->sh_IOPB[slot];
	d16_bzero(iopb, sizeof(M328_IOPB));
	return iopb;
}

int
vs_initialize(sc)
	struct vs_softc *sc;
{
	M328_CIB *cib = (M328_CIB *)&sc->sc_vsreg->sh_CIB;
	M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
	M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
	M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
	M328_IOPB *iopb;
	M328_WQCF *wiopb = (M328_WQCF *)&sc->sc_vsreg->sh_MCE_IOPB;
	u_short i, crsw;
	int failed = 0;

	CRB_CLR_DONE(CRSW);
	d16_bzero(cib, sizeof(M328_CIB));
	mcsb->mcsb_QHDP = 0;
	sc->sc_qhp = 0;
	cib->cib_NCQE = 10;
	cib->cib_BURST = 0;
	cib->cib_NVECT = sc->sc_ipl << 8;
	cib->cib_NVECT |= sc->sc_nvec;
	cib->cib_EVECT = sc->sc_ipl << 8;
	cib->cib_EVECT |= sc->sc_evec;
	cib->cib_PID = 0x07;
	cib->cib_SID = 0x00;
	cib->cib_CRBO = OFF(crb);
	cib->cib_SELECT_msw = HI(SELECTION_TIMEOUT);
	cib->cib_SELECT_lsw = LO(SELECTION_TIMEOUT);
	cib->cib_WQ0TIMO_msw = HI(4);
	cib->cib_WQ0TIMO_lsw = LO(4);
	cib->cib_VMETIMO_msw = 0; /*HI(VME_BUS_TIMEOUT);*/
	cib->cib_VMETIMO_lsw = 0; /*LO(VME_BUS_TIMEOUT);*/
	cib->cib_ERR_FLGS = M_ERRFLGS_RIN | M_ERRFLGS_RSE;
	cib->cib_SBRIV = sc->sc_ipl << 8;
	cib->cib_SBRIV |= sc->sc_evec;
	cib->cib_SOF0 = 0x15;
	cib->cib_SRATE0 = 100/4;
	cib->cib_SOF1 = 0x0;
	cib->cib_SRATE1 = 0x0;

	iopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;
	d16_bzero(iopb, sizeof(M328_IOPB));
	iopb->iopb_CMD = CNTR_INIT;
	iopb->iopb_OPTION = 0;
	iopb->iopb_NVCT = (u_char)sc->sc_nvec;
	iopb->iopb_EVCT = (u_char)sc->sc_evec;
	iopb->iopb_LEVEL = 0; /*sc->sc_ipl;*/
	iopb->iopb_ADDR = SHIO_MOD;
	LV(iopb->iopb_BUFF, OFF(cib));
	LV(iopb->iopb_LENGTH, sizeof(M328_CIB));

	d16_bzero(mc, sizeof(M328_CQE));
	mc->cqe_IOPB_ADDR = OFF(iopb);
	mc->cqe_IOPB_LENGTH = sizeof(M328_IOPB);
	mc->cqe_WORK_QUEUE = 0;
	mc->cqe_QECR = M_QECR_GO;
	/* poll for the command to complete */
	do_vspoll(sc, 0, 1);
	CRB_CLR_DONE(CRSW);

	/* initialize work queues */
	for (i = 1; i < 8; i++) {
		d16_bzero(wiopb, sizeof(M328_IOPB));
		wiopb->wqcf_CMD = CNTR_INIT_WORKQ;
		wiopb->wqcf_OPTION = 0;
		wiopb->wqcf_NVCT = (u_char)sc->sc_nvec;
		wiopb->wqcf_EVCT = (u_char)sc->sc_evec;
		wiopb->wqcf_ILVL = 0; /*sc->sc_ipl;*/
		wiopb->wqcf_WORKQ = i;
		wiopb->wqcf_WOPT = (WQO_FOE | WQO_INIT);
		wiopb->wqcf_SLOTS = JAGUAR_MAX_Q_SIZ;
		LV(wiopb->wqcf_CMDTO, 4); /* 1 second */

		d16_bzero(mc, sizeof(M328_CQE));
		mc->cqe_IOPB_ADDR = OFF(wiopb);
		mc->cqe_IOPB_LENGTH = sizeof(M328_IOPB);
		mc->cqe_WORK_QUEUE = 0;
		mc->cqe_QECR = M_QECR_GO;
		/* poll for the command to complete */
		do_vspoll(sc, 0, 1);
		if (CRSW & M_CRSW_ER) {
			/*printf("\nerror: queue %d status = 0x%x\n", i, riopb->iopb_STATUS);*/
			/*failed = 1;*/
			CRB_CLR_ER(CRSW);
		}
		CRB_CLR_DONE(CRSW);
		delay(500);
	}
	/* start queue mode */
	CRSW = 0;
	mcsb->mcsb_MCR |= M_MCR_SQM;
	crsw = CRSW;
	do_vspoll(sc, 0, 1);
	if (CRSW & M_CRSW_ER) {
		printf("error: status = 0x%x\n", riopb->iopb_STATUS);
		CRB_CLR_ER(CRSW);
	}
	CRB_CLR_DONE(CRSW);

	if (failed) {
		printf(": failed!\n");
		return (1);
	}
	/* reset SCSI bus */
	vs_reset(sc);
	/* sync all devices */
	vs_resync(sc);
	printf(": target %d\n", sc->sc_link.adapter_target);
	return (0); /* success */
}

void
vs_resync(sc)
	struct vs_softc *sc;
{
	M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
	M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	M328_DRCF *devreset = (M328_DRCF *)&sc->sc_vsreg->sh_MCE_IOPB;
	u_short i;
	for (i=0; i<7; i++) {
		d16_bzero(devreset, sizeof(M328_DRCF));
		devreset->drcf_CMD = CNTR_DEV_REINIT;
		devreset->drcf_OPTION = 0x00;	    /* no interrupts yet... */
		devreset->drcf_NVCT = sc->sc_nvec;
		devreset->drcf_EVCT = sc->sc_evec;
		devreset->drcf_ILVL = 0;
		devreset->drcf_UNIT = i;

		d16_bzero(mc, sizeof(M328_CQE));
		mc->cqe_IOPB_ADDR = OFF(devreset);
		mc->cqe_IOPB_LENGTH = sizeof(M328_DRCF);
		mc->cqe_WORK_QUEUE = 0;
		mc->cqe_QECR = M_QECR_GO;
		/* poll for the command to complete */
		do_vspoll(sc, 0, 0);
		if (riopb->iopb_STATUS) {
#ifdef SDEBUG
			printf("status: %x\n", riopb->iopb_STATUS);
#endif
			sc->sc_tinfo[i].avail = 0;
		} else {
			sc->sc_tinfo[i].avail = 1;
		}
		if (CRSW & M_CRSW_ER) {
			CRB_CLR_ER(CRSW);
		}
		CRB_CLR_DONE(CRSW);
	}
}

void
vs_reset(sc)
	struct vs_softc *sc;
{
	u_int s;
	M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
	M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	M328_SRCF *reset = (M328_SRCF *)&sc->sc_vsreg->sh_MCE_IOPB;

	d16_bzero(reset, sizeof(M328_SRCF));
	reset->srcf_CMD = IOPB_RESET;
	reset->srcf_OPTION = 0x00;	 /* no interrupts yet... */
	reset->srcf_NVCT = sc->sc_nvec;
	reset->srcf_EVCT = sc->sc_evec;
	reset->srcf_ILVL = 0;
	reset->srcf_BUSID = 0;
	s = splbio();

	d16_bzero(mc, sizeof(M328_CQE));
	mc->cqe_IOPB_ADDR = OFF(reset);
	mc->cqe_IOPB_LENGTH = sizeof(M328_SRCF);
	mc->cqe_WORK_QUEUE = 0;
	mc->cqe_QECR = M_QECR_GO;
	/* poll for the command to complete */
	while (1) {
		do_vspoll(sc, 0, 0);
		/* ack & clear scsi error condition cause by reset */
		if (CRSW & M_CRSW_ER) {
			CRB_CLR_ER(CRSW);
			CRB_CLR_DONE(CRSW);
			riopb->iopb_STATUS = 0;
			break;
		}
		CRB_CLR_DONE(CRSW);
	}
	/* thaw all work queues */
	thaw_queue(sc, 0xFF);
	splx (s);
}

/*
 * Process an interrupt from the MVME328
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */

int
vs_checkintr(sc, xs, status)
	struct vs_softc *sc;
	struct scsi_xfer *xs;
	int   *status;
{
	int   target = -1;
	int   lun = -1;
	M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	struct scsi_generic *cmd;
	u_long buf;
	u_long len;
	u_char error;

	target = xs->sc_link->target;
	lun = xs->sc_link->lun;
	cmd = (struct scsi_generic *)&riopb->iopb_SCSI[0];

	VL(buf, riopb->iopb_BUFF);
	VL(len, riopb->iopb_LENGTH);
	*status = riopb->iopb_STATUS >> 8;
	error = riopb->iopb_STATUS & 0xFF;

#ifdef SDEBUG
	printf("scsi_chk() ");

	if (xs->cmd->opcode == 0) {
		printf("TEST_UNIT_READY ");
	} else if (xs->cmd->opcode == REQUEST_SENSE) {
		printf("REQUEST_SENSE   ");
	} else if (xs->cmd->opcode == INQUIRY) {
		printf("INQUIRY         ");
	} else if (xs->cmd->opcode == MODE_SELECT) {
		printf("MODE_SELECT     ");
	} else if (xs->cmd->opcode == MODE_SENSE) {
		printf("MODE_SENSE      ");
	} else if (xs->cmd->opcode == START_STOP) {
		printf("START_STOP      ");
	} else if (xs->cmd->opcode == RESERVE) {
		printf("RESERVE         ");
	} else if (xs->cmd->opcode == RELEASE) {
		printf("RELEASE         ");
	} else if (xs->cmd->opcode == PREVENT_ALLOW) {
		printf("PREVENT_ALLOW   ");
	} else if (xs->cmd->opcode == POSITION_TO_ELEMENT) {
		printf("POSITION_TO_EL  ");
	} else if (xs->cmd->opcode == CHANGE_DEFINITION) {
		printf("CHANGE_DEF      ");
	} else if (xs->cmd->opcode == MODE_SENSE_BIG) {
		printf("MODE_SENSE_BIG  ");
	} else if (xs->cmd->opcode == MODE_SELECT_BIG) {
		printf("MODE_SELECT_BIG ");
	} else if (xs->cmd->opcode == 0x25) {
		printf("READ_CAPACITY   ");
	} else if (xs->cmd->opcode == 0x08) {
		printf("READ_COMMAND    ");
	}

	printf("tgt %d lun %d buf %x len %d status %x ", target, lun, buf, len, riopb->iopb_STATUS);

	if (CRSW & M_CRSW_EX) {
		printf("[ex]");
	}
	if (CRSW & M_CRSW_QMS) {
		printf("[qms]");
	}
	if (CRSW & M_CRSW_SC) {
		printf("[sc]");
	}
	if (CRSW & M_CRSW_SE) {
		printf("[se]");
	}
	if (CRSW & M_CRSW_AQ) {
		printf("[aq]");
	}
	if (CRSW & M_CRSW_ER) {
		printf("[er]");
	}
	printf("\n");
#endif
	if (len != xs->datalen) {
		xs->resid = xs->datalen - len;
	} else {
		xs->resid = 0;
	}

	if (error == SCSI_SELECTION_TO) {
		xs->error = XS_SELTIMEOUT;
		xs->status = -1;
		*status = -1;
	}
	return 1;
}

/* normal interrupt routine */
int
vs_nintr(vsc)
	void *vsc;
{
	struct vs_softc *sc = (struct vs_softc *)vsc;
	M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
	M328_CMD *m328_cmd;
	struct scsi_xfer *xs;
	int status;
	int s;

	if ((CRSW & CONTROLLER_ERROR) == CONTROLLER_ERROR)
		return(vs_eintr(sc));

	s = splbio();
	/* Got a valid interrupt on this device */
	sc->sc_intrcnt_n.ev_count++;

	VL((unsigned long)m328_cmd, crb->crb_CTAG);
#ifdef SDEBUG
	printf("Interrupt!!! ");
	printf("m328_cmd == 0x%x\n", m328_cmd);
#endif
	/*
	 * If this is a controller error, there won't be a m328_cmd
	 * pointer in the CTAG feild.  Bad things happen if you try
	 * to point to address 0.  Controller error should be handled
	 * in vsdma.c  I'll change this soon - steve.
	 */
	if (m328_cmd) {
		xs = m328_cmd->xs;
		if (m328_cmd->top_sg_list) {
			vs_dealloc_scatter_gather(m328_cmd->top_sg_list);
			m328_cmd->top_sg_list = (M328_SG)0;
		}
		FREE(m328_cmd, M_DEVBUF); /* free the command tag */
		if (vs_checkintr(sc, xs, &status)) {
			vs_scsidone(sc, xs, status);
		}
	}
	/* ack the interrupt */
	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER(CRSW);
	CRB_CLR_DONE(CRSW);
	/* clear the return information */
	vs_clear_return_info(sc);
	splx(s);
	return (1);
}

int
vs_eintr(vsc)
	void *vsc;
{
	struct vs_softc *sc = (struct vs_softc *)vsc;
	M328_CEVSB *crb = (M328_CEVSB *)&sc->sc_vsreg->sh_CRB;
	M328_CMD *m328_cmd;
	struct scsi_xfer *xs;
	int crsw = crb->cevsb_CRSW;
#ifdef SDEBUG
	int type = crb->cevsb_TYPE;
	int length = crb->cevsb_IOPB_LENGTH;
	int wq = crb->cevsb_WORK_QUEUE;
#endif
	int ecode = crb->cevsb_ERROR;
	int status, s;

	s = splbio();

	/* Got a valid interrupt on this device */
	sc->sc_intrcnt_e.ev_count++;

	VL((unsigned long)m328_cmd, crb->cevsb_CTAG);
#ifdef SDEBUG
	printf("Error Interrupt!!! ");
	printf("m328_cmd == 0x%x\n", m328_cmd);
#endif
	xs = m328_cmd->xs;

	if (crsw & M_CRSW_RST) {
		printf("%s: SCSI Bus Reset!\n", vs_name(sc));
		/* clear the return information */
		vs_clear_return_info(sc);
		splx(s);
		return(1);
	}
	switch (ecode) {
	case CEVSB_ERR_TYPE:
		printf("%s: IOPB Type error!\n", vs_name(sc));
		break;
	case CEVSB_ERR_TO:
		printf("%s: Timeout!\n", vs_name(sc));
		xs->error = XS_SELTIMEOUT;
		xs->status = -1;
		xs->flags |= ITSDONE;
		status = -1;
		scsi_done(xs);
		break;
	case CEVSB_ERR_TR:	/* Target Reconnect, no IOPB */
		printf("%s: Target Reconnect error!\n", vs_name(sc));
		break;
	case CEVSB_ERR_OF:	/* Overflow */
		printf("%s: Overflow error!\n", vs_name(sc));
		break;
	case CEVSB_ERR_BD:	/* Bad direction */
		printf("%s: Bad Direction!\n", vs_name(sc));
		break;
	case CEVSB_ERR_NR:	/* Non-Recoverable Error */
		printf("%s: Non-Recoverable error!\n", vs_name(sc));
		break;
	case CESVB_ERR_PANIC:	/* Board Panic!!! */
		printf("%s: Board Panic!!!\n", vs_name(sc));
		break;
	default:
		printf("%s: Uh oh!... Error 0x%x\n", vs_name(sc), ecode);
#ifdef DDB
		Debugger();
#endif
	}
#ifdef SDEBUG
	printf("%s: crsw = 0x%x iopb_type = %d iopb_len = %d wq = %d error = 0x%x\n",
	       vs_name(sc), crsw, type, length, wq, ecode);
#endif
	if (CRSW & M_CRSW_ER)
		CRB_CLR_ER(CRSW);
	CRB_CLR_DONE(CRSW);
	thaw_queue(sc, 0xFF);
	/* clear the return information */
	vs_clear_return_info(sc);
	splx(s);
	return(1);
}

static __inline__ void
vs_clear_return_info(sc)
	struct vs_softc *sc;
{
        M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
	M328_CEVSB *crb = (M328_CEVSB *)&sc->sc_vsreg->sh_CRB;
	d16_bzero(riopb, sizeof(M328_IOPB));
	d16_bzero(crb, sizeof(M328_CEVSB));
}

/*
 * Useful functions for scatter/gather list
 */

M328_SG
vs_alloc_scatter_gather(void)
{
	M328_SG sg;

	MALLOC(sg, M328_SG, sizeof(struct m328_sg), M_DEVBUF, M_WAITOK);
	bzero(sg, sizeof(struct m328_sg));

	return (sg);
}

void
vs_dealloc_scatter_gather(sg)
	M328_SG sg;
{
	int i;

	if (sg->level > 0) {
		for (i=0; sg->down[i] && i<MAX_SG_ELEMENTS; i++) {
			vs_dealloc_scatter_gather(sg->down[i]);
		}
	}
	FREE(sg, M_DEVBUF);
}

void
vs_link_sg_element(element, phys_add, len)
	sg_list_element_t *element;
	vaddr_t phys_add;
	int len;
{
	element->count.bytes = len;
	LV(element->address, phys_add);
	element->link = 0; /* FALSE */
	element->transfer_type = NORMAL_TYPE;
	element->memory_type = LONG_TRANSFER;
	element->address_modifier = 0xD;
}

void
vs_link_sg_list(list, phys_add, elements)
	sg_list_element_t *list;
	vaddr_t phys_add;
	int elements;
{

	list->count.scatter.gather  = elements;
	LV(list->address, phys_add);
	list->link = 1;	   /* TRUE */
	list->transfer_type = NORMAL_TYPE;
	list->memory_type = LONG_TRANSFER;
	list->address_modifier = 0xD;
}

M328_SG
vs_build_memory_structure(xs, iopb)
	struct scsi_xfer *xs;
	M328_IOPB  *iopb;	       /* the iopb */
{
	M328_SG   sg;
	vaddr_t starting_point_virt, starting_point_phys, point_virt,
	point1_phys, point2_phys, virt;
	unsigned len;
	int       level;

	sg = (M328_SG)0;   /* Hopefully we need no scatter/gather list */

	/*
	 * We have the following things:
	 *	virt			the virtual address of the contiguous virtual memory block
	 *	len			the length of the contiguous virtual memory block
	 *	starting_point_virt	the virtual address of the contiguous *physical* memory block
	 *	starting_point_phys	the *physical* address of the contiguous *physical* memory block
	 *	point_virt		the pointer to the virtual memory we are checking at the moment
	 *	point1_phys		the pointer to the *physical* memory we are checking at the moment
	 *	point2_phys		the pointer to the *physical* memory we are checking at the moment
	 */

	level = 0;
	virt = starting_point_virt = (vaddr_t)xs->data;
	point1_phys = starting_point_phys = kvtop((vaddr_t)xs->data);
	len = xs->datalen;
	/*
	 * Check if we need scatter/gather
	 */

	if (len > PAGE_SIZE) {
		for (level = 0, point_virt = round_page(starting_point_virt+1);
		    /* if we do already scatter/gather we have to stay in the loop and jump */
		    point_virt < virt + (vaddr_t)len || sg ;
		    point_virt += PAGE_SIZE) {			   /* out later */

			point2_phys = kvtop(point_virt);

			if ((point2_phys - trunc_page(point1_phys) - PAGE_SIZE) ||		   /* physical memory is not contiguous */
			    (point_virt - starting_point_virt >= MAX_SG_BLOCK_SIZE && sg)) {   /* we only can access (1<<16)-1 bytes in scatter/gather_mode */
				if (point_virt - starting_point_virt >= MAX_SG_BLOCK_SIZE) {	       /* We were walking too far for one scatter/gather block ... */
					assert( MAX_SG_BLOCK_SIZE > PAGE_SIZE );
					point_virt = trunc_page(starting_point_virt+MAX_SG_BLOCK_SIZE-1);    /* So go back to the beginning of the last matching page */
					/* and generate the physical address of
					 * this location for the next time. */
					point2_phys = kvtop(point_virt);
				}

				if (!sg) {
					/* We allocate our fist scatter/gather list */
					sg = vs_alloc_scatter_gather();
				}
#if 1 /* broken firmware */

				if (sg->elements >= MAX_SG_ELEMENTS) {
					vs_dealloc_scatter_gather(sg);
					return (NULL);
				}

#else /* if the firmware will ever get fixed */
				while (sg->elements >= MAX_SG_ELEMENTS) {
					if (!sg->up) { /* If the list full in this layer ? */
						sg->up = vs_alloc_scatter_gather();
						sg->up->level = sg->level+1;
						sg->up->down[0] = sg;
						sg->up->elements = 1;
					}
					/* link this full list also in physical memory */
					vs_link_sg_list(&(sg->up->list[sg->up->elements-1]),
							kvtop((vaddr_t)sg->list),
							sg->elements);
					sg = sg->up;	  /* Climb up */
				}
				while (sg->level) {  /* As long as we are not a the base level */
					int i;

					i = sg->elements;
					/* We need a new element */
					sg->down[i] = vs_alloc_scatter_gather();
					sg->down[i]->level = sg->level - 1;
					sg->down[i]->up = sg;
					sg->elements++;
					sg = sg->down[i]; /* Climb down */
				}
#endif /* 1 */
				if (point_virt < virt+(vaddr_t)len) {
					/* linking element */
					vs_link_sg_element(&(sg->list[sg->elements]),
							   starting_point_phys,
							   point_virt-starting_point_virt);
					sg->elements++;
				} else {
					/* linking last element */
					vs_link_sg_element(&(sg->list[sg->elements]),
							   starting_point_phys,
							   (vaddr_t)(virt+len)-starting_point_virt);
					sg->elements++;
					break;			       /* We have now collected all blocks */
				}
				starting_point_virt = point_virt;
				starting_point_phys = point2_phys;
			}
			point1_phys = point2_phys;
		}
	}

	/*
	 * Climb up along the right side of the tree until we reach the top.
	 */

	if (sg) {
		while (sg->up) {
			/* link this list also in physical memory */
			vs_link_sg_list(&(sg->up->list[sg->up->elements-1]),
					kvtop((vaddr_t)sg->list),
					sg->elements);
			sg = sg->up;		       /* Climb up */
		}

		iopb->iopb_OPTION |= M_OPT_SG;
		iopb->iopb_ADDR |= M_ADR_SG_LINK;
		LV(iopb->iopb_BUFF, kvtop((vaddr_t)sg->list));
		LV(iopb->iopb_LENGTH, sg->elements);
		LV(iopb->iopb_SGTTL, len);
	} else {
		/* no scatter/gather necessary */
		LV(iopb->iopb_BUFF, starting_point_phys);
		LV(iopb->iopb_LENGTH, len);
	}
	return (sg);
}
