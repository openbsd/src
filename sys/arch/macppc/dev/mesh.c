/*	$OpenBSD: mesh.c,v 1.24 2010/01/13 06:09:44 krw Exp $	*/
/*	$NetBSD: mesh.c,v 1.1 1999/02/19 13:06:03 tsubai Exp $	*/

/*-
 * Copyright (c) 2000	Tsubai Masanari.
 * Copyright (c) 1999	Internet Research Institute, Inc.
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

#include <dev/ofw/openfirm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <macppc/dev/dbdma.h>

#ifdef MESH_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

/* MESH register offsets */
#define MESH_XFER_COUNT0	0x00	/* transfer count (low)  */
#define MESH_XFER_COUNT1	0x10	/* transfer count (high) */
#define MESH_FIFO		0x20	/* FIFO (16byte depth) */
#define MESH_SEQUENCE		0x30	/* command register */
#define MESH_BUS_STATUS0	0x40
#define MESH_BUS_STATUS1	0x50
#define MESH_FIFO_COUNT		0x60
#define MESH_EXCEPTION		0x70
#define MESH_ERROR		0x80
#define MESH_INTR_MASK		0x90
#define MESH_INTERRUPT		0xa0
#define MESH_SOURCE_ID		0xb0
#define MESH_DEST_ID		0xc0
#define MESH_SYNC_PARAM		0xd0
#define MESH_MESH_ID		0xe0	/* MESH version */
#define MESH_SEL_TIMEOUT	0xf0	/* selection timeout delay */

#define MESH_SIGNATURE		0xe2	/* XXX wrong! */

/* MESH commands */
#define MESH_CMD_ARBITRATE	0x01
#define MESH_CMD_SELECT		0x02
#define MESH_CMD_COMMAND	0x03
#define MESH_CMD_STATUS		0x04
#define MESH_CMD_DATAOUT	0x05
#define MESH_CMD_DATAIN		0x06
#define MESH_CMD_MSGOUT		0x07
#define MESH_CMD_MSGIN		0x08
#define MESH_CMD_BUSFREE	0x09
#define MESH_CMD_ENABLE_PARITY	0x0A
#define MESH_CMD_DISABLE_PARITY	0x0B
#define MESH_CMD_ENABLE_RESEL	0x0C
#define MESH_CMD_DISABLE_RESEL	0x0D
#define MESH_CMD_RESET_MESH	0x0E
#define MESH_CMD_FLUSH_FIFO	0x0F
#define MESH_SEQ_DMA		0x80
#define MESH_SEQ_TARGET		0x40
#define MESH_SEQ_ATN		0x20
#define MESH_SEQ_ACTNEG		0x10

/* INTERRUPT/INTR_MASK register bits */
#define MESH_INTR_ERROR		0x04
#define MESH_INTR_EXCEPTION	0x02
#define MESH_INTR_CMDDONE	0x01

/* EXCEPTION register bits */
#define MESH_EXC_SELATN		0x20	/* selected and ATN asserted (T) */
#define MESH_EXC_SELECTED	0x10	/* selected (T) */
#define MESH_EXC_RESEL		0x08	/* reselected */
#define MESH_EXC_ARBLOST	0x04	/* arbitration lost */
#define MESH_EXC_PHASEMM	0x02	/* phase mismatch */
#define MESH_EXC_SELTO		0x01	/* selection timeout */

/* ERROR register bits */
#define MESH_ERR_DISCONNECT	0x40	/* unexpected disconnect */
#define MESH_ERR_SCSI_RESET	0x20	/* Rst signal asserted */
#define MESH_ERR_SEQERR		0x10	/* sequence error */
#define MESH_ERR_PARITY_ERR3	0x08	/* parity error */
#define MESH_ERR_PARITY_ERR2	0x04
#define MESH_ERR_PARITY_ERR1	0x02
#define MESH_ERR_PARITY_ERR0	0x01

/* BUS_STATUS0 status bits */
#define MESH_STATUS0_REQ32	0x80
#define MESH_STATUS0_ACK32	0x40
#define MESH_STATUS0_REQ	0x20
#define MESH_STATUS0_ACK	0x10
#define MESH_STATUS0_ATN	0x08
#define MESH_STATUS0_MSG	0x04
#define MESH_STATUS0_CD		0x02
#define MESH_STATUS0_IO		0x01

/* BUS_STATUS1 status bits */
#define MESH_STATUS1_RST	0x80
#define MESH_STATUS1_BSY	0x40
#define MESH_STATUS1_SEL	0x20

#define T_SYNCMODE 0x01		/* target uses sync mode */
#define T_SYNCNEGO 0x02		/* sync negotiation done */

struct mesh_tinfo {
	int flags;
	int period;
	int offset;
};

/* scb flags */
#define MESH_POLL		0x01
#define MESH_CHECK		0x02
#define MESH_SENSE		0x04
#define MESH_READ		0x80

struct mesh_scb {
	TAILQ_ENTRY(mesh_scb) chain;
	int flags;
	struct scsi_xfer *xs;
	struct scsi_generic cmd;
	int cmdlen;
	int target;			/* target SCSI ID */
	int resid;
	void *daddr;
	vsize_t dlen;
	int status;
};

/* sc_flags value */
#define MESH_DMA_ACTIVE	0x01

#define MESH_DMALIST_MAX	32

struct mesh_softc {
	struct device sc_dev;		/* us as a device */
	struct scsi_link sc_link;

	struct scsibus_softc *sc_scsibus;

	u_char *sc_reg;			/* MESH base address */
	bus_dmamap_t sc_dmamap;
	bus_dma_tag_t sc_dmat;
	struct dbdma_regmap *sc_dmareg;	/* DMA register address */
	struct dbdma_command *sc_dmacmd;	/* DMA command area */
	dbdma_t sc_dbdma;

	int sc_flags;
	int sc_cfflags;			/* copy of config flags */
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

static inline int mesh_read_reg(struct mesh_softc *, int);
static inline void mesh_set_reg(struct mesh_softc *, int, int);

int mesh_match(struct device *, void *, void *);
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
int mesh_poll(struct scsi_xfer *);
void mesh_done(struct mesh_softc *, struct mesh_scb *);
void mesh_timeout(void *);
void mesh_minphys(struct buf *, struct scsi_link *);

struct cfattach mesh_ca = {
	sizeof(struct mesh_softc), mesh_match, mesh_attach
};

struct cfdriver mesh_cd = {
	NULL, "mesh", DV_DULL
};

struct scsi_adapter mesh_switch = {
	mesh_scsi_cmd, mesh_minphys, NULL, NULL
};

struct scsi_device mesh_dev = {
	NULL, NULL, NULL, NULL
};

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

int
mesh_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	char compat[32];

	if (strcmp(ca->ca_name, "mesh") == 0)
		return 1;

	memset(compat, 0, sizeof(compat));
	OF_getprop(ca->ca_node, "compatible", compat, sizeof(compat));
	if (strcmp(compat, "chrp,mesh0") == 0)
		return 1;

	return 0;
}

void
mesh_attach(struct device *parent, struct device *self, void *aux)
{
	struct mesh_softc *sc = (void *)self;
	struct confargs *ca = aux;
	struct scsibus_attach_args saa;
	int i, error;
	u_int *reg;

	reg = ca->ca_reg;
	reg[0] += ca->ca_baseaddr;
	reg[2] += ca->ca_baseaddr;
	if ((sc->sc_reg = mapiodev(reg[0], reg[1])) == NULL) {
		printf(": cannot map device registers\n");
		return;
	}

	sc->sc_irq = ca->ca_intr[0];
	if ((sc->sc_dmareg = mapiodev(reg[2], reg[3])) == NULL) {
		printf(": cannot map DMA registers\n");
		goto noreg;
	}

	sc->sc_cfflags = sc->sc_dev.dv_cfdata->cf_flags;

	if (OF_getprop(ca->ca_node, "clock-frequency", &sc->sc_freq, 4) != 4) {
		printf(": cannot get clock-frequency\n");
		goto nofreq;
	}

	sc->sc_dmat = ca->ca_dmat;
	if ((error = bus_dmamap_create(sc->sc_dmat,
	    MESH_DMALIST_MAX * DBDMA_COUNT_MAX, MESH_DMALIST_MAX,
	    DBDMA_COUNT_MAX, NBPG, BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		printf(": cannot create DMA map, error = %d\n", error);
		goto nofreq;
	}

	sc->sc_freq /= 1000000;	/* in MHz */
	sc->sc_minsync = 25;	/* maximum sync rate = 10MB/sec */
	sc->sc_id = 7;

	TAILQ_INIT(&sc->free_scb);
	TAILQ_INIT(&sc->ready_scb);
	for (i = 0; i < sizeof(sc->sc_scb)/sizeof(sc->sc_scb[0]); i++)
		TAILQ_INSERT_TAIL(&sc->free_scb, &sc->sc_scb[i], chain);

	if ((sc->sc_dbdma = dbdma_alloc(sc->sc_dmat, MESH_DMALIST_MAX))
	    == NULL) {
		printf(": cannot alloc dma descriptors\n");
		goto nodbdma;
	}

	sc->sc_dmacmd = sc->sc_dbdma->d_addr;
	timeout_set(&sc->sc_tmo, mesh_timeout, sc);

	mesh_reset(sc);
	mesh_bus_reset(sc);

	printf(" irq %d: %dMHz\n", sc->sc_irq, sc->sc_freq);

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.device = &mesh_dev;
	sc->sc_link.adapter = &mesh_switch;
	sc->sc_link.openings = 2;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dev, &saa, scsiprint);

	mac_intr_establish(parent, sc->sc_irq, IST_LEVEL, IPL_BIO, mesh_intr,
	    sc, sc->sc_dev.dv_xname);

	/* Reset SCSI bus when halt. */
	shutdownhook_establish(mesh_shutdownhook, sc);

	return;
nodbdma:
	bus_dmamap_destroy(sc->sc_dmat, sc->sc_dmamap);
nofreq:
	unmapiodev(sc->sc_dmareg, reg[3]);
noreg:
	unmapiodev(sc->sc_reg, reg[1]);
}

#define MESH_SET_XFER(sc, count) do {					\
	mesh_set_reg(sc, MESH_XFER_COUNT0, count);			\
	mesh_set_reg(sc, MESH_XFER_COUNT1, count >> 8);			\
} while (0)

#define MESH_GET_XFER(sc) ((mesh_read_reg(sc, MESH_XFER_COUNT1) << 8) |	\
			   mesh_read_reg(sc, MESH_XFER_COUNT0))

int
mesh_read_reg(struct mesh_softc *sc, int reg)
{
	return in8(sc->sc_reg + reg);
}

void
mesh_set_reg(struct mesh_softc *sc, int reg, int val)
{
	out8(sc->sc_reg + reg, val);
}

void
mesh_shutdownhook(void *arg)
{
	struct mesh_softc *sc = arg;

	/* Set to async mode. */
	mesh_set_reg(sc, MESH_SYNC_PARAM, 2);
	mesh_bus_reset(sc);
}

#ifdef MESH_DEBUG
static char scsi_phase[][8] = {
	"DATAOUT",
	"DATAIN",
	"COMMAND",
	"STATUS",
	"",
	"",
	"MSGOUT",
	"MSGIN"
};
#endif

int
mesh_intr(void *arg)
{
	struct mesh_softc *sc = arg;
	struct mesh_scb *scb;
	int fifocnt;
	u_char intr, exception, error, status0, status1;

	intr = mesh_read_reg(sc, MESH_INTERRUPT);

	if (intr == 0) {
		DPRINTF("%s: stray interrupt\n", sc->sc_dev.dv_xname);
		return 0;
	}
	exception = mesh_read_reg(sc, MESH_EXCEPTION);
	error = mesh_read_reg(sc, MESH_ERROR);
	status0 = mesh_read_reg(sc, MESH_BUS_STATUS0);
	status1 = mesh_read_reg(sc, MESH_BUS_STATUS1);

	/* clear interrupt */
	mesh_set_reg(sc, MESH_INTERRUPT, intr);

#ifdef MESH_DEBUG
{

	printf("mesh_intr status0 = 0x%x (%s), exc = 0x%x\n",
	    status0, scsi_phase[status0 & 7], exception);
}
#endif

	scb = sc->sc_nexus;
	if (scb == NULL) {
		DPRINTF("%s: NULL nexus\n", sc->sc_dev.dv_xname);
		return 1;
	}

	if (intr & MESH_INTR_CMDDONE) {
		if (sc->sc_flags & MESH_DMA_ACTIVE) {
			dbdma_stop(sc->sc_dmareg);
			bus_dmamap_unload(sc->sc_dmat, sc->sc_dmamap);

			sc->sc_flags &= ~MESH_DMA_ACTIVE;
			scb->resid = MESH_GET_XFER(sc);

			fifocnt = mesh_read_reg(sc, MESH_FIFO_COUNT);
			if (fifocnt != 0) {
				if (scb->flags & MESH_READ) {
					char *cp;

					cp = (char *)scb->daddr + scb->dlen
					    - fifocnt;
					DPRINTF("fifocnt = %d, resid = %d\n",
					    fifocnt, scb->resid);
					while (fifocnt > 0) {
						*cp++ = mesh_read_reg(sc,
						    MESH_FIFO);
						fifocnt--;
					}
				} else {
					mesh_set_reg(sc, MESH_SEQUENCE,
					    MESH_CMD_FLUSH_FIFO);
				}
			} else {
				/* Clear all interrupts */
				mesh_set_reg(sc, MESH_INTERRUPT, 7);
			}
		}
	}

	if (intr & MESH_INTR_ERROR) {
		printf("%s: error %02x %02x\n",
		    sc->sc_dev.dv_xname, error, exception);
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
			DPRINTF("%s: PHASE MISMATCH; nextstate = %d -> ",
			    sc->sc_dev.dv_xname, sc->sc_nextstate);
			sc->sc_nextstate = status0 & MESH_PHASE_MASK;

			DPRINTF("%d, resid = %d\n",
			    sc->sc_nextstate, scb->resid);
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
		break;
	case MESH_DATAIN:
	case MESH_DATAOUT:
		if (mesh_dataio(sc, scb)) {
			scb->xs->error = XS_DRIVER_STUFFUP;
			mesh_done(sc, scb);
		}
		break;
	case MESH_STATUS:
		mesh_status(sc, scb);
		break;
	case MESH_MSGIN:
		mesh_msgin(sc, scb);
		break;
	case MESH_COMPLETE:
		mesh_done(sc, scb);
		break;

	default:
		printf("%s: unknown state (%d)\n", sc->sc_dev.dv_xname,
		    sc->sc_nextstate);
		scb->xs->error = XS_DRIVER_STUFFUP;
		mesh_done(sc, scb);
	}

	return 1;
}

void
mesh_error(struct mesh_softc *sc, struct mesh_scb *scb, int error,
    int exception)
{
	if (error & MESH_ERR_SCSI_RESET) {
		printf("%s: SCSI RESET\n", sc->sc_dev.dv_xname);

		/* Wait until the RST signal is deasserted. */
		while (mesh_read_reg(sc, MESH_BUS_STATUS1) & MESH_STATUS1_RST);
			mesh_reset(sc);
		return;
	}

	if (error & MESH_ERR_PARITY_ERR0) {
		printf("%s: parity error\n", sc->sc_dev.dv_xname);
		scb->xs->error = XS_DRIVER_STUFFUP;
	}

	if (error & MESH_ERR_DISCONNECT) {
		printf("%s: unexpected disconnect\n", sc->sc_dev.dv_xname);
		if (sc->sc_nextstate != MESH_COMPLETE)
			scb->xs->error = XS_DRIVER_STUFFUP;
	}

	if (exception & MESH_EXC_SELTO) {
		/* XXX should reset bus here? */
		scb->xs->error = XS_SELTIMEOUT;
	}

	mesh_done(sc, scb);
}

void
mesh_select(struct mesh_softc *sc, struct mesh_scb *scb)
{
	struct mesh_tinfo *ti = &sc->sc_tinfo[scb->target];

	DPRINTF("mesh_select\n");

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

	timeout_add_sec(&sc->sc_tmo, 10);
}

void
mesh_identify(struct mesh_softc *sc, struct mesh_scb *scb)
{
	struct mesh_tinfo *ti = &sc->sc_tinfo[scb->target];

	DPRINTF("mesh_identify\n");
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_FLUSH_FIFO);

	if ((ti->flags & T_SYNCNEGO) == 0) {
		ti->period = sc->sc_minsync;
		ti->offset = 15;
		mesh_msgout(sc, SEND_IDENTIFY | SEND_SDTR);
		sc->sc_nextstate = MESH_MSGIN;
	} else {
		mesh_msgout(sc, SEND_IDENTIFY);
		sc->sc_nextstate = MESH_COMMAND;
	}
}

void
mesh_command(struct mesh_softc *sc, struct mesh_scb *scb)
{
	int i;
	char *cmdp;

#ifdef MESH_DEBUG
	printf("mesh_command cdb = %02x", scb->cmd.opcode);
	for (i = 0; i < 5; i++)
		printf(" %02x", scb->cmd.bytes[i]);
	printf("\n");
#endif

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
mesh_dma_setup(struct mesh_softc *sc, struct mesh_scb *scb)
{
	int datain = scb->flags & MESH_READ;
	struct dbdma_command *cmdp;
	u_int cmd;
	int i, error;

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap, scb->daddr,
	    scb->dlen, NULL, BUS_DMA_NOWAIT)) != 0)
		return (error);

	cmdp = sc->sc_dmacmd;
	cmd = datain ? DBDMA_CMD_IN_MORE : DBDMA_CMD_OUT_MORE;

	for (i = 0; i < sc->sc_dmamap->dm_nsegs; i++, cmdp++) {
		if (i + 1 == sc->sc_dmamap->dm_nsegs)
			cmd = datain ? DBDMA_CMD_IN_LAST : DBDMA_CMD_OUT_LAST;
		DBDMA_BUILD(cmdp, cmd, 0, sc->sc_dmamap->dm_segs[i].ds_len,
		    sc->sc_dmamap->dm_segs[i].ds_addr, DBDMA_INT_NEVER,
		    DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);
	}

	DBDMA_BUILD(cmdp, DBDMA_CMD_STOP, 0, 0, 0,
	    DBDMA_INT_NEVER, DBDMA_WAIT_NEVER, DBDMA_BRANCH_NEVER);

	return(0);
}

int
mesh_dataio(struct mesh_softc *sc, struct mesh_scb *scb)
{
	int error;

	if ((error = mesh_dma_setup(sc, scb)))
		return(error);

	if (scb->dlen == 65536)
		MESH_SET_XFER(sc, 0);	/* TC = 0 means 64KB transfer */
	else
		MESH_SET_XFER(sc, scb->dlen);

	if (scb->flags & MESH_READ)
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_DATAIN | MESH_SEQ_DMA);
	else
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_DATAOUT | MESH_SEQ_DMA);

	dbdma_start(sc->sc_dmareg, sc->sc_dbdma);
	sc->sc_flags |= MESH_DMA_ACTIVE;
	sc->sc_nextstate = MESH_STATUS;

	return(0);
}

void
mesh_status(struct mesh_softc *sc, struct mesh_scb *scb)
{
	if (mesh_read_reg(sc, MESH_FIFO_COUNT) == 0) {	/* XXX cheat */
		DPRINTF("mesh_status(0)\n");
		MESH_SET_XFER(sc, 1);
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_STATUS);
		sc->sc_nextstate = MESH_STATUS;
		return;
	}

	scb->status = mesh_read_reg(sc, MESH_FIFO);
	DPRINTF("mesh_status(1): status = 0x%x\n", scb->status);
	if (mesh_read_reg(sc, MESH_FIFO_COUNT) != 0)
		DPRINTF("FIFO_COUNT=%d\n", mesh_read_reg(sc, MESH_FIFO_COUNT));

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_FLUSH_FIFO);
	MESH_SET_XFER(sc, 1);
	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGIN);

	sc->sc_nextstate = MESH_MSGIN;
}

void
mesh_msgin(struct mesh_softc *sc, struct mesh_scb *scb)
{
	DPRINTF("mesh_msgin\n");

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
#ifdef MESH_DEBUG
	{
		int i;
		printf("msgin:");
		for (i = 0; i < sc->sc_imsglen; i++)
			printf(" 0x%02x", sc->sc_imsg[i]);
		printf("\n");
	}
#endif

	switch (sc->sc_imsg[0]) {
	case MSG_CMDCOMPLETE:
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_BUSFREE);
		sc->sc_nextstate = MESH_COMPLETE;
		sc->sc_imsglen = 0;
		return;

	case MSG_MESSAGE_REJECT:
		if (sc->sc_msgout & SEND_SDTR) {
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
		sc_print_addr(scb->xs->sc_link);
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
		sc_print_addr(scb->xs->sc_link);
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
mesh_msgout(struct mesh_softc *sc, int msg)
{
	struct mesh_scb *scb = sc->sc_nexus;
	struct mesh_tinfo *ti;
	int lun, len, i;

	DPRINTF("mesh_msgout: sending");

	sc->sc_msgout = msg;
	len = 0;

	if (msg & SEND_REJECT) {
		DPRINTF(" REJECT");
		sc->sc_omsg[len++] = MSG_MESSAGE_REJECT;
	}
	if (msg & SEND_IDENTIFY) {
		DPRINTF(" IDENTIFY");
		lun = scb->xs->sc_link->lun;
		sc->sc_omsg[len++] = MSG_IDENTIFY(lun, 0);
	}
	if (msg & SEND_SDTR) {
		DPRINTF(" SDTR");
		ti = &sc->sc_tinfo[scb->target];
		sc->sc_omsg[len++] = MSG_EXTENDED;
		sc->sc_omsg[len++] = 3;
		sc->sc_omsg[len++] = MSG_EXT_SDTR;
		sc->sc_omsg[len++] = ti->period;
		sc->sc_omsg[len++] = ti->offset;
	}
	DPRINTF("\n");

	MESH_SET_XFER(sc, len);
	if (len == 1) {
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGOUT);
		mesh_set_reg(sc, MESH_FIFO, sc->sc_omsg[0]);
	} else {
		mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_MSGOUT | MESH_SEQ_ATN);

		for (i = 0; i < len - 1; i++)
			mesh_set_reg(sc, MESH_FIFO, sc->sc_omsg[i]);

		/* Wait for the FIFO empty... */
		while (mesh_read_reg(sc, MESH_FIFO_COUNT) > 0);

		/* ...then write the last byte. */
		mesh_set_reg(sc, MESH_FIFO, sc->sc_omsg[i]);
	}
	sc->sc_nextstate = MESH_UNKNOWN;
}

void
mesh_bus_reset(struct mesh_softc *sc)
{
	DPRINTF("mesh_bus_reset\n");

	/* Disable interrupts. */
	mesh_set_reg(sc, MESH_INTR_MASK, 0);

	/* Assert RST line. */
	mesh_set_reg(sc, MESH_BUS_STATUS1, MESH_STATUS1_RST);
	delay(50);
	mesh_set_reg(sc, MESH_BUS_STATUS1, 0);

	mesh_reset(sc);
}

void
mesh_reset(struct mesh_softc *sc)
{
	int i;

	DPRINTF("mesh_reset\n");

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
		if (sc->sc_cfflags & (0x100 << i))
			ti->flags |= T_SYNCNEGO;
	}
	sc->sc_nexus = NULL;
}

int
mesh_stp(struct mesh_softc *sc, int v)
{
	/*
	 * stp(v) = 5 * clock_period	 (v == 0)
	 *	= (v + 2) * 2 clock_period (v > 0)
	 */

	if (v == 0)
		return 5 * 250 / sc->sc_freq;
	else
		return (v + 2) * 2 * 250 / sc->sc_freq;
}

void
mesh_setsync(struct mesh_softc *sc, struct mesh_tinfo *ti)
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
mesh_get_scb(struct mesh_softc *sc)
{
	struct mesh_scb *scb;

	scb = TAILQ_FIRST(&sc->free_scb);
	if (scb)
		TAILQ_REMOVE(&sc->free_scb, scb, chain);

	return scb;
}

void
mesh_free_scb(struct mesh_softc *sc, struct mesh_scb *scb)
{
	TAILQ_INSERT_TAIL(&sc->free_scb, scb, chain);
}

int
mesh_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *sc_link = xs->sc_link;;
	struct mesh_softc *sc = sc_link->adapter_softc;
	struct mesh_scb *scb;
	u_int flags;
	int s;

	flags = xs->flags;
	s = splbio();
	scb = mesh_get_scb(sc);
	splx(s);
	if (scb == NULL)
		return (NO_CCB);
	DPRINTF("cmdlen: %d\n", xs->cmdlen);
	scb->xs = xs;
	scb->flags = 0;
	scb->status = 0;
	scb->daddr = xs->data;
	scb->dlen = xs->datalen;
	scb->resid = xs->datalen;
	bcopy(xs->cmd, &scb->cmd, xs->cmdlen);
	scb->cmdlen = xs->cmdlen;
	scb->target = sc_link->target;
	sc->sc_imsglen = 0;	/* XXX ? */

	if (flags & SCSI_POLL)
		scb->flags |= MESH_POLL;

	if (flags & SCSI_DATA_IN)
		scb->flags |= MESH_READ;

	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_scb, scb, chain);
	if (sc->sc_nexus == NULL)
		mesh_sched(sc);
	splx(s);

	if (xs->flags & SCSI_POLL) {
		if (mesh_poll(xs)) {
			printf("%s: poll timeout\n",
			    sc->sc_dev.dv_xname);

		}
		return COMPLETE;
	}

	return SUCCESSFULLY_QUEUED;
}

void
mesh_sched(struct mesh_softc *sc)
{
	struct mesh_scb *scb;

	TAILQ_FOREACH(scb, &sc->ready_scb, chain) {
		if (sc->sc_nexus == NULL) {
			TAILQ_REMOVE(&sc->ready_scb, scb, chain);
			sc->sc_nexus = scb;
			mesh_select(sc, scb);
			return;
		}
	}
}

int
mesh_poll(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mesh_softc *sc = link->adapter_softc;

	int count = xs->timeout;
	while (count) {
		if (mesh_read_reg(sc, MESH_INTERRUPT))
			mesh_intr(sc);

		if (xs->flags & ITSDONE)
			return 0;
		DELAY(1000);
		count--;
	}

	return 1;
}

void
mesh_done(struct mesh_softc *sc, struct mesh_scb *scb)
{
	struct scsi_xfer *xs = scb->xs;

	DPRINTF("mesh_done\n");

	sc->sc_nextstate = MESH_BUSFREE;
	sc->sc_nexus = NULL;

	timeout_del(&sc->sc_tmo);

	if (scb->status == SCSI_BUSY) {
		xs->error = XS_BUSY;
		printf("Target busy\n");
	}

	if (scb->status == SCSI_CHECK)
		xs->error = XS_BUSY;

	xs->status = scb->status;
	xs->resid = scb->resid;

	mesh_set_reg(sc, MESH_SYNC_PARAM, 2);

	if ((xs->flags & SCSI_POLL) == 0)
		mesh_sched(sc);

	scsi_done(xs);
	mesh_free_scb(sc, scb);
}

void
mesh_timeout(void *arg)
{

	struct mesh_softc *sc = arg;
	struct mesh_scb *scb = sc->sc_nexus;
	int s;
	int status0, status1;
	int intr, error, exception, imsk;

	printf("%s: timeout state %d\n", sc->sc_dev.dv_xname, sc->sc_nextstate);

	intr = mesh_read_reg(sc, MESH_INTERRUPT);
	imsk = mesh_read_reg(sc, MESH_INTR_MASK);
	exception = mesh_read_reg(sc, MESH_EXCEPTION);
	error = mesh_read_reg(sc, MESH_ERROR);
	status0 = mesh_read_reg(sc, MESH_BUS_STATUS0);
	status1 = mesh_read_reg(sc, MESH_BUS_STATUS1);

	s = splbio();
	if (sc->sc_flags & MESH_DMA_ACTIVE) {
		dbdma_reset(sc->sc_dmareg);
	}
	scb->xs->error = XS_TIMEOUT;

	mesh_set_reg(sc, MESH_SEQUENCE, MESH_CMD_BUSFREE);
	sc->sc_nextstate = MESH_COMPLETE;

	splx(s);
}

void
mesh_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > 64*1024)
		bp->b_bcount = 64*1024;

	minphys(bp);
}
