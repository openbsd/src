/*	$OpenBSD: mesh.c,v 1.5 2002/09/15 02:02:43 deraadt Exp $	*/
/*	$NetBSD: mesh.c,v 1.1 1999/02/19 13:06:03 tsubai Exp $	*/

/*-
 * Copyright (C) 1999	Internet Research Institute, Inc.
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
 *	This product includes software developed by
 *	Internet Research Institute, Inc.
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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>
/*
#include "scsipi/scsi_all.h"
#include "scsipi/scsipi_all.h"
#include "scsipi/scsiconf.h"
#include "scsipi/scsi_message.h"*/

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include "dbdma.h"
#include "meshreg.h"

#define T_SYNCMODE 0x01		/* target uses sync mode */
#define T_SYNCNEGO 0x02		/* sync negotiation done */

struct mesh_tinfo {
	int flags;
	int period;
	int offset;
};

/* scb flags */
#define MESH_POLL	0x01
#define MESH_CHECK	0x02
#define MESH_SENSE	0x04
#define MESH_READ	0x80

struct mesh_scb {
	TAILQ_ENTRY(mesh_scb) chain;
	int flags;
	struct scsi_xfer *xs;
	struct scsi_generic cmd;
	int cmdlen;
	int target;			/* target SCSI ID */
	int resid;
	vaddr_t daddr;
	vsize_t dlen;
	int status;
};

/* sc_flags value */
#define MESH_DMA_ACTIVE	0x01

#define	MESH_DMALIST_MAX	32

struct mesh_softc {
	struct device sc_dev;		/* us as a device */
	struct scsi_link sc_link;
	struct scsi_adapter sc_adapter;

	u_char *sc_reg;			/* MESH base address */
	bus_dma_tag_t sc_dmat;
	dbdma_regmap_t *sc_dmareg;	/* DMA register address */
	dbdma_command_t *sc_dmacmd;	/* DMA command area */
	dbdma_t sc_dbdma;

	int sc_flags;
	int sc_cfflags;			/* copy of config flags */
	int sc_meshid;			/* MESH version */
	int sc_minsync;			/* minimum sync period */
	int sc_irq;
	int sc_freq;			/* SCSI bus frequency in MHz */
	int sc_id;			/* our SCSI ID */
	struct mesh_tinfo sc_tinfo[8];	/* target information */

	int sc_nextstate;
	int sc_prevphase;
	struct mesh_scb *sc_nexus;	/* current command */

	int sc_msgout;
	int sc_imsglen;
	int sc_omsglen;
	u_char sc_imsg[16];
	u_char sc_omsg[16];

	TAILQ_HEAD(, mesh_scb) free_scb;
	TAILQ_HEAD(, mesh_scb) ready_scb;
	struct mesh_scb sc_scb[16];

	struct timeout sc_tmo;
};

/* mesh_msgout() values */
#define SEND_REJECT	1
#define SEND_IDENTIFY	2
#define SEND_SDTR	4


#ifdef __OpenBSD__
#define scsi_print_addr   sc_print_addr
#define scsipi_done 	  scsi_done
#endif

static __inline int mesh_read_reg(struct mesh_softc *, int);
static __inline void mesh_set_reg(struct mesh_softc *, int, int);

int mesh_match(struct device *, struct cfdata *, void *);
void mesh_attach(struct device *, struct device *, void *);
void mesh_shutdownhook(void *);
int mesh_intr(void *);
void mesh_error(struct mesh_softc *, struct mesh_scb *, int, int);
void mesh_select(struct mesh_softc *, struct mesh_scb *);
void mesh_identify(struct mesh_softc *, struct mesh_scb *);
void mesh_command(struct mesh_softc *, struct mesh_scb *);
int mesh_dma_setup(struct mesh_softc *, struct mesh_scb *);
int mesh_dataio(struct mesh_softc *, struct mesh_scb *);
void mesh_status(struct mesh_softc *, struct mesh_scb *);
void mesh_msgin(struct mesh_softc *, struct mesh_scb *);
void mesh_msgout(struct mesh_softc *, int);
void mesh_bus_reset(struct mesh_softc *);
void mesh_reset(struct mesh_softc *);
int mesh_stp(struct mesh_softc *, int);
void mesh_setsync(struct mesh_softc *, struct mesh_tinfo *);
struct mesh_scb *mesh_get_scb(struct mesh_softc *);
void mesh_free_scb(struct mesh_softc *, struct mesh_scb *);
int mesh_scsi_cmd(struct scsi_xfer *);
void mesh_sched(struct mesh_softc *);
int mesh_poll(struct mesh_softc *, struct scsi_xfer *);
void mesh_done(struct mesh_softc *, struct mesh_scb *);
void mesh_timeout(void *);
void mesh_sense(struct mesh_softc *, struct mesh_scb *);
void mesh_minphys(struct buf *);


#define MESH_DATAOUT	0
#define MESH_DATAIN	MESH_STATUS0_IO
#define MESH_COMMAND	MESH_STATUS0_CD
#define MESH_STATUS	(MESH_STATUS0_CD | MESH_STATUS0_IO)
#define MESH_MSGOUT	(MESH_STATUS0_MSG | MESH_STATUS0_CD)
#define MESH_MSGIN	(MESH_STATUS0_MSG | MESH_STATUS0_CD | MESH_STATUS0_IO)

#define MESH_SELECTING	8
#define MESH_IDENTIFY	9
#define MESH_COMPLETE	10
#define MESH_BUSFREE	11
#define MESH_UNKNOWN	-1

#define MESH_PHASE_MASK	(MESH_STATUS0_MSG | MESH_STATUS0_CD | MESH_STATUS0_IO)

struct cfattach mesh_ca = {
	sizeof(struct mesh_softc),(cfmatch_t)mesh_match,
	 mesh_attach
};


struct cfdriver mesh_cd = {
        NULL, "mesh", DV_DULL
};

struct scsi_device mesh_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

int
mesh_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct confargs *ca = aux;
	printf("MESH_MATCH called ,ca->ca_name= %s\n",ca->ca_name);

	if (strcmp(ca->ca_name, "mesh") != 0)
		return 0;

	return 1;
}

void
mesh_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct mesh_softc *sc = (void *)self;
	struct confargs *ca = aux;
	int i, error;
	u_int *reg;

	printf("MESH_ATTACH called\n");

	reg = ca->ca_reg;
	reg[0] += ca->ca_baseaddr;
	reg[2] += ca->ca_baseaddr;
	sc->sc_reg = mapiodev(reg[0], reg[1]);
	sc->sc_irq = ca->ca_intr[0];
	sc->sc_dmareg = mapiodev(reg[2], reg[3]);

	sc->sc_cfflags = self->dv_cfdata->cf_flags;
	sc->sc_meshid = mesh_read_reg(sc, MESH_MESH_ID) & 0x1f;
#if 0
	if (sc->sc_meshid != (MESH_SIGNATURE & 0x1f) {
		printf(": unknown MESH ID (0x%x)\n", sc->sc_meshid);
		return;
	}
#endif
	if (OF_getprop(ca->ca_node, "clock-frequency", &sc->sc_freq, 4) != 4) {
		printf(": cannot get clock-frequency\n");
		return;
	}

	sc->sc_dmat = ca->ca_dmat;
	if ((error = bus_dmamap_create(sc->sc_dmat,
	    MESH_DMALIST_MAX * DBDMA_COUNT_MAX, MESH_DMALIST_MAX,
	    DBDMA_COUNT_MAX, NBPG, BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		printf(": cannot create dma map, error = %d\n", error);
		return;
	}

	sc->sc_freq /= 1000000;	/* in MHz */
	sc->sc_minsync = 25;	/* maximum sync rate = 10MB/sec */
	sc->sc_id = 7;

	TAILQ_INIT(&sc->free_scb);
	TAILQ_INIT(&sc->ready_scb);
	for (i = 0; i < sizeof(sc->sc_scb)/sizeof(sc->sc_scb[0]); i++)
		TAILQ_INSERT_TAIL(&sc->free_scb, &sc->sc_scb[i], chain);

	sc->sc_dbdma = dbdma_alloc(sc->sc_dmat, MESH_DMALIST_MAX);
	sc->sc_dmacmd = sc->sc_dbdma->d_addr;
	timeout_set(&sc->sc_tmo, mesh_timeout, scb);

	mesh_reset(sc);
	mesh_bus_reset(sc);

	printf(" irq %d: %dMHz, SCSI ID %d\n",
		sc->sc_irq, sc->sc_freq, sc->sc_id);

	sc->sc_adapter.scsi_cmd = mesh_scsi_cmd;
	sc->sc_adapter.scsi_minphys = mesh_minphys;

/*	sc->sc_link.scsi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;*/
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.adapter = &sc->sc_adapter;
	sc->sc_link.device = &mesh_dev;
	sc->sc_link.openings = 2;
/*	sc->sc_link.scsi_scsi.max_target = 7;
	sc->sc_link.scsi_scsi.max_lun = 7;
	sc->sc_link.type = BUS_SCSI;
*/
	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

	printf("in mesh_attach, after config_found,calling mac_intr_establish,sc->sc_irq = %d\n",sc->sc_irq);

	/*intr_establish(sc->sc_irq, IST_LEVEL, IPL_BIO, mesh_intr, sc);*/
	mac_intr_establish(parent,sc->sc_irq,IST_LEVEL, IPL_BIO, mesh_intr, sc,"mesh intr");
	/* Reset SCSI bus when halt. */
	shutdownhook_establish(mesh_shutdownhook, sc);
}

#define MESH_SET_XFER(sc, count) do {					\
	mesh_set_reg(sc, MESH_XFER_COUNT0, count);			\
	mesh_set_reg(sc, MESH_XFER_COUNT1, count >> 8);			\
} while (0)

#define MESH_GET_XFER(sc) ((mesh_read_reg(sc, MESH_XFER_COUNT1) << 8) |	\
			   mesh_read_reg(sc, MESH_XFER_COUNT0))

int
mesh_read_reg(sc, reg)
	struct mesh_softc *sc;
	int reg;
{
	return in8(sc->sc_reg + reg);
}

void
mesh_set_reg(sc, reg, val)
	struct mesh_softc *sc;
	int reg, val;
{
	out8(sc->sc_reg + reg, val);
}

void
mesh_shutdownhook(arg)
	void *arg;
{
	struct mesh_softc *sc = arg;

	/* Set to async mode. */
	mesh_set_reg(sc, MESH_SYNC_PARAM, 2);
}

int
mesh_intr(arg)
	void *arg;
{
	struct mesh_softc *sc = arg;
	struct mesh_scb *scb;
	u_char intr, exception, error, status0, status1;
	int i;

	intr = mesh_read_reg(sc, MESH_INTERRUPT);

#ifdef MESH_DEBUG
	if (intr == 0) {
		printf("mesh: stray interrupt\n");
		return 0;
	}
#endif

	exception = mesh_read_reg(sc, MESH_EXCEPTION);
	error = mesh_read_reg(sc, MESH_ERROR);
	status0 = mesh_read_reg(sc, MESH_BUS_STATUS0);
	status1 = mesh_read_reg(sc, MESH_BUS_STATUS1);

	/* clear interrupt */
	mesh_set_reg(sc, MESH_INTERRUPT, intr);

	scb = sc->sc_nexus;
	if (scb == NULL) {
#ifdef MESH_DEBUG
		printf("mesh: NULL nexus\n");
#endif
		return 1;
	}

	if (sc->sc_flags & MESH_DMA_ACTIVE) {
		dbdma_stop(sc->sc_dmareg);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);

		sc->sc_flags &= ~MESH_DMA_ACTIVE;
		scb->resid = MESH_GET_XFER(sc);

		if (mesh_read_reg(sc, MESH_FIFO_COUNT) != 0)
			panic("mesh: FIFO != 0");	/* XXX */
	}

	if (intr & MESH_INTR_ERROR) {
		mesh_error(sc, scb, error, 0);
		return 1;
	}

	if (intr & MESH_INTR_EXCEPTION) {
		/* selection timeout */
		if (exception & MESH_EXC_SELTO) {
			mesh_error(sc, scb, 0, exception);
			return 1;
		}

		/* phase mismatch */
		if (exception & MESH_EXC_PHASEMM) {
			sc->sc_nextstate = status0 & MESH_PHASE_MASK;
#if 0
			printf("mesh: PHASE MISMATCH cdb =");
			printf(" %02x", scb->cmd.opcode);
			for (i = 0; i < 5; i++) {
				printf(" %02x", scb->cmd.bytes[i]);
			}
			printf("\n");
#endif
		}
	}

	if (sc->sc_nextstate == MESH_UNKNOWN)
		sc->sc_nextstate = status0 & MESH_PHASE_MASK;

	switch (sc->sc_nextstate) {

	case MESH_IDENTIFY:
		mesh_identify(sc, scb);
		break;
	case MESH_COMMAND:
		mesh_command(sc, scb);
		printf("mesh_intr:case MESH_COMMAND\n");
		break;
	case MESH_DATAIN:
	case MESH_DATAOUT:
		if (mesh_dataio(sc, scb))
			return (1);
		printf("mesh_intr:case MESH_DATAIN or MESH_DATAOUT\n");
		break;
	case MESH_STATUS:
		mesh_status(sc, scb);
		printf("mesh_intr:case MESH_STATUS\n");
		break;
	case MESH_MSGIN:
		mesh_msgin(sc, scb);
		printf("mesh_intr:case MESH_MSGIN\n");
		break;
	case MESH_COMPLETE:
		printf("mesh_intr:case MESH_COMPLETE\n");
		mesh_done(sc, scb);
		break;

	default:
		panic("mesh: unknown state (0x%x)", sc->sc_nextstate);
	}

	return 1;
}

void
mesh_error(sc, scb, error, exception)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
	int error, exception;
{
	if (error & MESH_ERR_SCSI_RESET) {
		printf("mesh: SCSI RESET\n");

		/* Wait until the RST signal is deasserted. */
		while (mesh_read_reg(sc, MESH_BUS_STATUS1) & MESH_STATUS1_RST);
		mesh_reset(sc);
		return;
	}

	if (error & MESH_ERR_PARITY_ERR0) {
		printf("mesh: parity error\n");
		scb->xs->error = XS_DRIVER_STUFFUP;
	}

	if (error & MESH_ERR_DISCONNECT) {
		printf("mesh: unexpected disconnect\n");
		if (sc->sc_nextstate != MESH_COMPLETE)
			scb->xs->error = XS_DRIVER_STUFFUP;
	}

	if (exception & MESH_EXC_SELTO) {
		/* XXX should reset bus here? */
		scb->xs->error = XS_DRIVER_STUFFUP;
	}

	mesh_done(sc, scb);
}

void
mesh_select(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	struct mesh_tinfo *ti = &sc->sc_tinfo[scb->target];

	mesh_setsync(sc, ti);
	MESH_SET_XFER(sc, 0);

	/* arbitration */

	/*
	 * MESH mistakenly asserts TARGET ID bit along with its own ID bit
	 * in arbitration phase (like selection).  So we should load
	 * initiator ID to DestID register temporarily.
	 */
	mesh_set_reg(sc, MESH_DEST_ID, sc->sc_id);
	mesh_set_reg(sc, MESH_INTR_MASK, 0);	/* disable intr. */
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_ARBITRATE);

	while (mesh_read_reg(sc, MESH_INTERRUPT) == 0);
	mesh_set_reg(sc, MESH_INTERRUPT, 1);
	mesh_set_reg(sc, MESH_INTR_MASK, 7);

	/* selection */
	mesh_set_reg(sc, MESH_DEST_ID, scb->target);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_SELECT | MESH_SEQ_ATN);

	sc->sc_prevphase = MESH_SELECTING;
	sc->sc_nextstate = MESH_IDENTIFY;

	timeout_add(&sc->sc_tmo, 10*hz);
}

void
mesh_identify(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_FLUSH_FIFO);
	mesh_msgout(sc, SEND_IDENTIFY);

	sc->sc_nextstate = MESH_COMMAND;
	printf("mesh_identify called\n");
}

void
mesh_command(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	struct mesh_tinfo *ti = &sc->sc_tinfo[scb->target];
	int i;
	char *cmdp;

	if ((ti->flags & T_SYNCNEGO) == 0) {
		ti->period = sc->sc_minsync;
		ti->offset = 15;
		mesh_msgout(sc, SEND_SDTR);
		sc->sc_prevphase = MESH_COMMAND;
		sc->sc_nextstate = MESH_MSGIN;
		return;
	}

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_FLUSH_FIFO);

	MESH_SET_XFER(sc, scb->cmdlen);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_COMMAND);

	cmdp = (char *)&scb->cmd;
	for (i = 0; i < scb->cmdlen; i++)
		mesh_set_reg(sc, MESH_FIFO, *cmdp++);

	if (scb->resid == 0)
		sc->sc_nextstate = MESH_STATUS;		/* no data xfer */
	else
		sc->sc_nextstate = MESH_DATAIN;
}

int
mesh_dma_setup(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	int datain = scb->flags & MESH_READ;
	dbdma_command_t *cmdp;
	u_int cmd;
	int i, error;

	if ((error = bus_dmamap_load(sc->dmat, sc->sc_dmamap, scb->daddr,
	    scb->dlen, NULL, BUS_DMA_NOWAIT)) != 0)
		return (error);

	cmdp = sc->sc_dmacmd;
	cmd = datain ? DBDMA_CMD_IN_MORE : DBDMA_CMD_OUT_MORE;

	for (i = 0; i < sc->sc_dmamap->dm_nsegs; i++, cmdp++) {
		if (i + 1 == sc->sc_dmamap->dm_nsegs)
			cmd = read ? DBDMA_CMD_IN_LAST : DBDMA_CMD_OUT_LAST;
		DBDMA_BUILD(cmdp, cmd, 0, sc->sc_dmamap->dm_segs[i].ds_len,
			sc->sc_dmamap->dm_segs[i].ds_addr,
			DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	}

	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
		DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	return (0);
}

int
mesh_dataio(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	int error;

	if ((error = mesh_dma_setup(sc, scb)))
		return (error);

	if (scb->dlen == 65536)
		MESH_SET_XFER(sc, 0);	/* TC = 0 means 64KB transfer */
	else
		MESH_SET_XFER(sc, scb->dlen);

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_SEQ_DMA |
	    (scb->flags & MESH_READ)? MESH_CMD_DATAIN : MESH_CMD_DATAOUT);

	dbdma_start(sc->sc_dmareg, sc->sc_dbdma);
	sc->sc_flags |= MESH_DMA_ACTIVE;
	sc->sc_nextstate = MESH_STATUS;

	return (0);
}

void
mesh_status(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	if (mesh_read_reg(sc, MESH_FIFO_COUNT) == 0) {	/* XXX cheat */
		MESH_SET_XFER(sc, 1);
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_STATUS);
		sc->sc_nextstate = MESH_STATUS;
		return;
	}

	scb->status = mesh_read_reg(sc, MESH_FIFO);

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_FLUSH_FIFO);
	MESH_SET_XFER(sc, 1);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGIN);

	sc->sc_nextstate = MESH_MSGIN;
}

#define IS1BYTEMSG(m) (((m) != 1 && (m) < 0x20) || (m) & 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 1)

void
mesh_msgin(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	int i;

	if (mesh_read_reg(sc, MESH_FIFO_COUNT) == 0) {	/* XXX cheat */
		MESH_SET_XFER(sc, 1);
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGIN);
		sc->sc_imsglen = 0;
		sc->sc_nextstate = MESH_MSGIN;
		return;
	}

	sc->sc_imsg[sc->sc_imsglen++] = mesh_read_reg(sc, MESH_FIFO);

	if (sc->sc_imsglen == 1 && IS1BYTEMSG(sc->sc_imsg[0]))
		goto gotit;
	if (sc->sc_imsglen == 2 && IS2BYTEMSG(sc->sc_imsg[0]))
		goto gotit;
	if (sc->sc_imsglen >= 3 && ISEXTMSG(sc->sc_imsg[0]) &&
	    sc->sc_imsglen == sc->sc_imsg[1] + 2)
		goto gotit;

	sc->sc_nextstate = MESH_MSGIN;
	MESH_SET_XFER(sc, 1);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGIN);
	return;

gotit:
#ifdef DEBUG
	printf("msgin:");
	for (i = 0; i < sc->sc_imsglen; i++)
		printf(" 0x%02x", sc->sc_imsg[i]);
	printf("\n");
#endif

	switch (sc->sc_imsg[0]) {
	case MSG_CMDCOMPLETE:
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_BUSFREE);
		sc->sc_nextstate = MESH_COMPLETE;
		sc->sc_imsglen = 0;
		return;

	case MSG_MESSAGE_REJECT:
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			printf("SDTR rejected\n");
			printf("using async mode\n");
			sc->sc_tinfo[scb->target].period = 0;
			sc->sc_tinfo[scb->target].offset = 0;
			mesh_setsync(sc, &sc->sc_tinfo[scb->target]);
			break;
		}
		break;

	case MSG_NOOP:
		break;

	case MSG_EXTENDED:
		goto extended_msg;

	default:
		scsi_print_addr(scb->xs->sc_link);
		printf("unrecognized MESSAGE(0x%02x); sending REJECT\n",
			sc->sc_imsg[0]);

	reject:
		mesh_msgout(sc, SEND_REJECT);
		return;
	}
	goto done;

extended_msg:
	/* process an extended message */
	switch (sc->sc_imsg[2]) {
	case MSG_EXT_SDTR:
	  {
		struct mesh_tinfo *ti = &sc->sc_tinfo[scb->target];
		int period = sc->sc_imsg[3];
		int offset = sc->sc_imsg[4];
		int r = 250 / period;
		int s = (100*250) / period - 100 * r;

		if (period < sc->sc_minsync) {
			ti->period = sc->sc_minsync;
			ti->offset = 15;
			mesh_msgout(sc, SEND_SDTR);
			return;
		}
		scsi_print_addr(scb->xs->sc_link);
		/* XXX if (offset != 0) ... */
		printf("max sync rate %d.%02dMb/s\n", r, s);
		ti->period = period;
		ti->offset = offset;
		ti->flags |= T_SYNCNEGO;
		ti->flags |= T_SYNCMODE;
		mesh_setsync(sc, ti);
		goto done;
	  }
	default:
		printf("%s target %d: rejecting extended message 0x%x\n",
			sc->sc_dev.dv_xname, scb->target, sc->sc_imsg[0]);
		goto reject;
	}

done:
	sc->sc_imsglen = 0;
	sc->sc_nextstate = MESH_UNKNOWN;

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_BUSFREE); /* XXX really? */
}

void
mesh_msgout(sc, msg)
	struct mesh_softc *sc;
	int msg;
{
	struct mesh_scb *scb = sc->sc_nexus;
	struct mesh_tinfo *ti;
	int lun, i;

	switch (msg) {
	case SEND_REJECT:
		sc->sc_omsglen = 1;
		sc->sc_omsg[0] = MSG_MESSAGE_REJECT;
		break;
	case SEND_IDENTIFY:
		lun = scb->xs->sc_link->lun;
		sc->sc_omsglen = 1;
		sc->sc_omsg[0] = MSG_IDENTIFY(lun, 0);
		break;
	case SEND_SDTR:
		ti = &sc->sc_tinfo[scb->target];
		sc->sc_omsglen = 5;
		sc->sc_omsg[0] = MSG_EXTENDED;
		sc->sc_omsg[1] = 3;
		sc->sc_omsg[2] = MSG_EXT_SDTR;
		sc->sc_omsg[3] = ti->period;
		sc->sc_omsg[4] = ti->offset;
		break;
	}
	sc->sc_msgout = msg;

	MESH_SET_XFER(sc, sc->sc_omsglen);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGOUT | MESH_SEQ_ATN);

	for (i = 0; i < sc->sc_omsglen; i++)
		mesh_set_reg(sc, MESH_FIFO, sc->sc_omsg[i]);

	sc->sc_nextstate = MESH_UNKNOWN;
}

void
mesh_bus_reset(sc)
	struct mesh_softc *sc;
{
	/* Disable interrupts. */
	mesh_set_reg(sc, MESH_INTR_MASK, 0);

	/* Assert RST line. */
	mesh_set_reg(sc, MESH_BUS_STATUS1, MESH_STATUS1_RST);
	delay(50);
	mesh_set_reg(sc, MESH_BUS_STATUS1, 0);

	mesh_reset(sc);
}

void
mesh_reset(sc)
	struct mesh_softc *sc;
{
	int i;

	/* Reset DMA first. */
	dbdma_reset(sc->sc_dmareg);

	/* Disable interrupts. */
	mesh_set_reg(sc, MESH_INTR_MASK, 0);

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_RESET_MESH);
	delay(1);

	/* Wait for reset done. */
	while (mesh_read_reg(sc, MESH_INTERRUPT) == 0);

	/* Clear interrupts */
	mesh_set_reg(sc, MESH_INTERRUPT, 0x7);

	/* Set SCSI ID */
	mesh_set_reg(sc, MESH_SOURCE_ID, sc->sc_id);

	/* Set to async mode by default. */
	mesh_set_reg(sc, MESH_SYNC_PARAM, 2);

	/* Set selection timeout to 250ms. */
	mesh_set_reg(sc, MESH_SEL_TIMEOUT, 250 * sc->sc_freq / 500);

	/* Enable parity check. */
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_ENABLE_PARITY);

	/* Enable all interrupts. */
	mesh_set_reg(sc, MESH_INTR_MASK, 0x7);

	for (i = 0; i < 7; i++) {
		struct mesh_tinfo *ti = &sc->sc_tinfo[i];

		ti->flags = 0;
		ti->period = ti->offset = 0;
		if (sc->sc_cfflags & (1 << i)) {
			ti->flags |= T_SYNCNEGO;
		}
	}
	sc->sc_nexus = NULL;
}

int
mesh_stp(sc, v)
	struct mesh_softc *sc;
	int v;
{
	/*
	 * stp(v) = 5 * clock_period         (v == 0)
	 *        = (v + 2) * 2 clock_period (v > 0)
	 */

	if (v == 0)
		return 5 * 250 / sc->sc_freq;
	else
		return (v + 2) * 2 * 250 / sc->sc_freq;
}

void
mesh_setsync(sc, ti)
	struct mesh_softc *sc;
	struct mesh_tinfo *ti;
{
	int period = ti->period;
	int offset = ti->offset;
	int v;

	if ((ti->flags & T_SYNCMODE) == 0)
		offset = 0;

	if (offset == 0) {	/* async mode */
		mesh_set_reg(sc, MESH_SYNC_PARAM, 2);
		return;
	}

	v = period * sc->sc_freq / 250 / 2 - 2;
	if (v < 0)
		v = 0;
	if (mesh_stp(sc, v) < period)
		v++;
	if (v > 15)
		v = 15;
	mesh_set_reg(sc, MESH_SYNC_PARAM, (offset << 4) | v);
}

struct mesh_scb *
mesh_get_scb(sc)
	struct mesh_softc *sc;
{
	struct mesh_scb *scb;
	int s;

	s = splbio();
	while ((scb = sc->free_scb.tqh_first) == NULL)
		tsleep(&sc->free_scb, PRIBIO, "meshscb", 0);
	TAILQ_REMOVE(&sc->free_scb, scb, chain);
	splx(s);

	return scb;
}

void
mesh_free_scb(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	int s;

	s = splbio();
	TAILQ_INSERT_HEAD(&sc->free_scb, scb, chain);
	if (scb->chain.tqe_next == NULL)
		wakeup(&sc->free_scb);
	splx(s);
}

int
mesh_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct mesh_softc *sc = sc_link->adapter_softc;
	struct mesh_scb *scb;
	u_int flags;
	int s;

	flags = xs->flags;

	scb = mesh_get_scb(sc);
	scb->xs = xs;
	scb->flags = 0;
	scb->status = 0;
	scb->daddr = (vaddr_t)xs->data;
	scb->dlen = xs->datalen;
	scb->resid = xs->datalen;
	bcopy(xs->cmd, &scb->cmd, xs->cmdlen);
	scb->cmdlen = xs->cmdlen;

	scb->target = sc_link->target;
	sc->sc_imsglen = 0;	/* XXX ? */

	printf("messh_scsi_cmd,scb->target=%d\n",scb->target);

	if (flags & SCSI_POLL){
		printf("mesh_scsi_cmd:flags=SCSI_POLL\n");
		scb->flags |= MESH_POLL;
	}

#if 0
	if (flags & SCSI_DATA_OUT)
		scb->flags &= ~MESH_READ;
#endif
	if (flags & SCSI_DATA_IN){
		printf("mesh_scsi_cmd:flags=SCSI_DATA_IN\n");
		scb->flags |= MESH_READ;
	}

	s = splbio();

	TAILQ_INSERT_TAIL(&sc->ready_scb, scb, chain);

	if (sc->sc_nexus == NULL){
		printf("mesh_scsi_cmd:sc->sc_nexus == NULL,calling mesh_sched\n");	/* IDLE */
		mesh_sched(sc);
	}

	splx(s);

	if ((flags & SCSI_POLL) == 0){
		printf("mesh_scsi_cmd: returning SUCCESSFULLY_QUEUED\n");
		return SUCCESSFULLY_QUEUED;
	}

	if (mesh_poll(sc, xs)) {
		printf("mesh: timeout\n");
		if (mesh_poll(sc, xs))
			printf("mesh: timeout again\n");
	}
	printf("mesh_scsi_cmd: returning COMPLETE\n");
	return COMPLETE;
}

void
mesh_sched(sc)
	struct mesh_softc *sc;
{
	struct scsi_xfer *xs;
	struct scsi_link *sc_link;
	struct mesh_scb *scb;

	scb = sc->ready_scb.tqh_first;
start:
	if (scb == NULL)
		return;

	xs = scb->xs;
	sc_link = xs->sc_link;

	if (sc->sc_nexus == NULL) {
		TAILQ_REMOVE(&sc->ready_scb, scb, chain);
		sc->sc_nexus = scb;
		mesh_select(sc, scb);
		return;
	}

	scb = scb->chain.tqe_next;
	goto start;
}

int
mesh_poll(sc, xs)
	struct mesh_softc *sc;
	struct scsi_xfer *xs;
{
	int count = xs->timeout;
	printf("in mesh_poll,timeout=%d\n",xs->timeout);


	while (count) {
		if (mesh_read_reg(sc, MESH_INTERRUPT))
			mesh_intr(sc);

		if (xs->flags & ITSDONE)
			return 0;
		DELAY(1000);
		count--;
	};
	return 1;
}

void
mesh_done(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	struct scsi_xfer *xs = scb->xs;

#ifdef MESH_SHOWSTATE
	printf("mesh_done\n");
#endif

	sc->sc_nextstate = MESH_BUSFREE;
	sc->sc_nexus = NULL;

	timeout_del(&sc->sc_tmo);

	if (scb->status == SCSI_BUSY) {
		xs->error = XS_BUSY;
		printf("Target busy\n");
	}

	if (scb->status == SCSI_CHECK) {
		if (scb->flags & MESH_SENSE)
			panic("SCSI_CHECK && MESH_SENSE?");
		xs->resid = scb->resid;
		mesh_sense(sc, scb);
		return;
	}

	if (xs->error == XS_NOERROR) {
		xs->status = scb->status;
		if (scb->flags & MESH_SENSE)
			xs->error = XS_SENSE;
		else
			xs->resid = scb->resid;
	}

	xs->flags |= ITSDONE;

	mesh_set_reg(sc, MESH_SYNC_PARAM, 2);

	if ((xs->flags & SCSI_POLL) == 0)
		mesh_sched(sc);

	scsi_done(xs);
	mesh_free_scb(sc, scb);
}

void
mesh_timeout(arg)
	void *arg;
{
	struct mesh_scb *scb = arg;
	struct mesh_softc *sc = scb->xs->sc_link->adapter_softc;
	int s;
	int status0, status1;
	int intr, error, exception;

	printf("mesh: timeout state=%x\n", sc->sc_nextstate);
	intr = mesh_read_reg(sc, MESH_INTERRUPT);


	exception = mesh_read_reg(sc, MESH_EXCEPTION);

	error = mesh_read_reg(sc, MESH_ERROR);

	status0 = mesh_read_reg(sc, MESH_BUS_STATUS0);

	status1 = mesh_read_reg(sc, MESH_BUS_STATUS1);

#if 1
printf("intr 0x%02x, except 0x%02x, err 0x%02x\n", intr, exception, error);
#endif

	s = splbio();

	if (sc->sc_flags & MESH_DMA_ACTIVE) {
		printf("mesh: resetting dma\n");
		dbdma_reset(sc->sc_dmareg);
	}
	scb->xs->error = XS_TIMEOUT;

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_BUSFREE);

	sc->sc_nextstate = MESH_COMPLETE;

	splx(s);
	printf("rerturning from mesh_timeout\n");
}

void
mesh_sense(sc, scb)
	struct mesh_softc *sc;
	struct mesh_scb *scb;
{
	struct scsi_xfer *xs = scb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct scsi_sense *ss = (void *)&scb->cmd;

	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = sc_link->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);
	scb->cmdlen = sizeof(*ss);
	scb->daddr = (vaddr_t)&xs->sense;
	scb->dlen = sizeof(struct scsi_sense_data);
	scb->resid = scb->dlen;
	bzero((void *)scb->daddr, scb->dlen);

	scb->flags |= MESH_SENSE | MESH_READ;

	TAILQ_INSERT_HEAD(&sc->ready_scb, scb, chain);
	if (sc->sc_nexus == NULL)
		mesh_sched(sc);
}

void
mesh_minphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > 64*1024)
		bp->b_bcount = 64*1024;

	minphys(bp);
}
