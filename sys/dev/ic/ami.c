/*	$OpenBSD: ami.c,v 1.20 2003/02/25 00:26:08 tedu Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
 * All rights reserved.
 *
 * The SCSI emulation layer is derived from gdt(4) driver,
 * Copyright (c) 1999, 2000 Niklas Hallqvist. All rights reserved.
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * American Megatrends Inc. MegaRAID controllers driver
 *
 * This driver was made because these ppl and organizations
 * donated hardware and provided documentation:
 *
 * - 428 model card
 *	John Kerbawy, Stephan Matis, Mark Stovall;
 *
 * - 467 and 475 model cards, docs
 *	American Megatrends Inc.;
 *
 * - uninterruptable electric power for cvs
 *	Theo de Raadt.
 */

/* #define	AMI_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/amireg.h>
#include <dev/ic/amivar.h>

#ifdef AMI_DEBUG
#define	AMI_DPRINTF(m,a)	if (ami_debug & (m)) printf a
#define	AMI_D_CMD	0x0001
#define	AMI_D_INTR	0x0002
#define	AMI_D_MISC	0x0004
#define	AMI_D_DMA	0x0008
int ami_debug = 0
	| AMI_D_CMD
	| AMI_D_INTR
/*	| AMI_D_MISC */
/*	| AMI_D_DMA */
	;
#else
#define	AMI_DPRINTF(m,a)	/* m, a */
#endif

struct cfdriver ami_cd = {
	NULL, "ami", DV_DULL
};

int	ami_scsi_cmd(struct scsi_xfer *xs);
void	amiminphys(struct buf *bp);

struct scsi_adapter ami_switch = {
	ami_scsi_cmd, amiminphys, 0, 0,
};

struct scsi_device ami_dev = {
	NULL, NULL, NULL, NULL
};

int	ami_scsi_raw_cmd(struct scsi_xfer *xs);

struct scsi_adapter ami_raw_switch = {
	ami_scsi_raw_cmd, amiminphys, 0, 0,
};

struct scsi_device ami_raw_dev = {
	NULL, NULL, NULL, NULL
};

static __inline struct ami_ccb *ami_get_ccb(struct ami_softc *sc);
static __inline void ami_put_ccb(struct ami_ccb *ccb);
void ami_copyhds(struct ami_softc *sc, const u_int32_t *sizes,
	const u_int8_t *props, const u_int8_t *stats);
void *ami_allocmem(bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname);
void ami_freemem(bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname);
void ami_dispose(struct ami_softc *sc);
void ami_stimeout(void *v);
int  ami_cmd(struct ami_ccb *ccb, int flags, int wait);
int  ami_start(struct ami_ccb *ccb, int wait);
int  ami_complete(struct ami_ccb *ccb);
int  ami_done(struct ami_softc *sc, int idx);
void ami_copy_internal_data(struct scsi_xfer *xs, void *v, size_t size);
int  ami_inquire(struct ami_softc *sc, u_int8_t op);


static __inline struct ami_ccb *
ami_get_ccb(sc)
	struct ami_softc *sc;
{
	struct ami_ccb *ccb;

	ccb = TAILQ_LAST(&sc->sc_free_ccb, ami_queue_head);
	if (ccb) {
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ccb_link);
		ccb->ccb_state = AMI_CCB_READY;
	}
	return ccb;
}

static __inline void
ami_put_ccb(ccb)
	struct ami_ccb *ccb;
{
	struct ami_softc *sc = ccb->ccb_sc;

	ccb->ccb_state = AMI_CCB_FREE;
	TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
}

void *
ami_allocmem(dmat, map, segp, isize, nent, iname)
	bus_dma_tag_t dmat;
	bus_dmamap_t *map;
	bus_dma_segment_t *segp;
	size_t isize, nent;
	const char *iname;
{
	size_t total = isize * nent;
	caddr_t p;
	int error, rseg;

	/* XXX this is because we might have no dmamem_load_raw */
	if ((error = bus_dmamem_alloc(dmat, total, PAGE_SIZE, 0, segp, 1,
	    &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate %s%s (%d)\n",
		    iname, nent==1? "": "s", error);
		return (NULL);
	}

	if ((error = bus_dmamem_map(dmat, segp, rseg, total, &p,
	    BUS_DMA_NOWAIT))) {
		printf(": cannot map %s%s (%d)\n",
		    iname, nent==1? "": "s", error);
		return (NULL);
	}

	bzero(p, total);
	if ((error = bus_dmamap_create(dmat, total, 1,
	    total, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, map))) {
		printf(": cannot create %s dmamap (%d)\n", iname, error);
		return (NULL);
	}
	if ((error = bus_dmamap_load(dmat, *map, p, total, NULL,
	    BUS_DMA_NOWAIT))) {
		printf(": cannot load %s dma map (%d)\n", iname, error);
		return (NULL);
	}

	return (p);
}

void
ami_freemem(dmat, map, segp, isize, nent, iname)
	bus_dma_tag_t dmat;
	bus_dmamap_t *map;
	bus_dma_segment_t *segp;
	size_t isize, nent;
	const char *iname;
{
	bus_dmamem_free(dmat, segp, 1);
	bus_dmamap_destroy(dmat, *map);
	*map = NULL;
}

void
ami_dispose(sc)
	struct ami_softc *sc;
{
	register struct ami_ccb *ccb;

	/* traverse the ccbs and destroy the maps */
	for (ccb = &sc->sc_ccbs[AMI_MAXCMDS - 1]; ccb > sc->sc_ccbs; ccb--)
		if (ccb->ccb_dmamap)
			bus_dmamap_destroy(sc->dmat, ccb->ccb_dmamap);
	ami_freemem(sc->dmat, &sc->sc_sgmap, sc->sc_sgseg,
	    sizeof(struct ami_sgent) * AMI_SGEPERCMD, AMI_MAXCMDS, "sglist");
	ami_freemem(sc->dmat, &sc->sc_cmdmap, sc->sc_cmdseg,
	    sizeof(struct ami_iocmd), AMI_MAXCMDS + 1, "command");
}


void
ami_copyhds(sc, sizes, props, stats)
	struct ami_softc *sc;
	const u_int32_t *sizes;
	const u_int8_t *props, *stats;
{
	int i;

	for (i = 0; i < sc->sc_nunits; i++) {
		sc->sc_hdr[i].hd_present = 1;
		sc->sc_hdr[i].hd_is_logdrv = 1;
		sc->sc_hdr[i].hd_size = letoh32(sizes[i]);
		sc->sc_hdr[i].hd_prop = props[i];
		sc->sc_hdr[i].hd_stat = stats[i];
		if (sc->sc_hdr[i].hd_size > 0x200000) {
			sc->sc_hdr[i].hd_heads = 255;
			sc->sc_hdr[i].hd_secs = 63;
		} else {
			sc->sc_hdr[i].hd_heads = 64;
			sc->sc_hdr[i].hd_secs = 32;
		}
	}
}

int
ami_attach(sc)
	struct ami_softc *sc;
{
	/* struct ami_rawsoftc *rsc; */
	struct ami_ccb	*ccb;
	struct ami_iocmd *cmd;
	struct ami_sgent *sg;
	bus_dmamap_t idatamap;
	bus_dma_segment_t idataseg[1];
	const char *p;
	void	*idata;
	int	error;

	if (!(idata = ami_allocmem(sc->dmat, &idatamap, idataseg,
	    NBPG, 1, "init data"))) {
		ami_freemem(sc->dmat, &idatamap, idataseg,
		    NBPG, 1, "init data");
		return 1;
	}

	sc->sc_cmds = ami_allocmem(sc->dmat, &sc->sc_cmdmap, sc->sc_cmdseg,
	    sizeof(struct ami_iocmd), AMI_MAXCMDS+1, "command");
	if (!sc->sc_cmds) {
		ami_dispose(sc);
		ami_freemem(sc->dmat, &idatamap,
		    idataseg, NBPG, 1, "init data");
		return 1;
	}
	sc->sc_sgents = ami_allocmem(sc->dmat, &sc->sc_sgmap, sc->sc_sgseg,
	    sizeof(struct ami_sgent) * AMI_SGEPERCMD, AMI_MAXCMDS+1, "sglist");
	if (!sc->sc_sgents) {
		ami_dispose(sc);
		ami_freemem(sc->dmat, &idatamap,
		    idataseg, NBPG, 1, "init data");
		return 1;
	}

	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_ccbdone);
	TAILQ_INIT(&sc->sc_free_ccb);

	/* 0th command is a mailbox */
	for (ccb = &sc->sc_ccbs[AMI_MAXCMDS-1],
	     cmd = sc->sc_cmds  + sizeof(*cmd) * AMI_MAXCMDS,
	     sg = sc->sc_sgents + sizeof(*sg)  * AMI_MAXCMDS * AMI_SGEPERCMD;
	     cmd >= (struct ami_iocmd *)sc->sc_cmds;
	     cmd--, ccb--, sg -= AMI_SGEPERCMD) {

		cmd->acc_id = cmd - (struct ami_iocmd *)sc->sc_cmds;
		if (cmd->acc_id) {
			error = bus_dmamap_create(sc->dmat,
			    AMI_MAXFER, AMI_MAXOFFSETS, AMI_MAXFER, 0,
			    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
			    &ccb->ccb_dmamap);
			if (error) {
				printf(": cannot create ccb dmamap (%d)\n",
				    error);
				ami_dispose(sc);
				ami_freemem(sc->dmat, &idatamap,
				    idataseg, NBPG, 1, "init data");
				return (1);
			}
			ccb->ccb_sc = sc;
			ccb->ccb_cmd = cmd;
			ccb->ccb_state = AMI_CCB_FREE;
			ccb->ccb_cmdpa = htole32(sc->sc_cmdseg[0].ds_addr +
			    cmd->acc_id * sizeof(*cmd));
			ccb->ccb_sglist = sg;
			ccb->ccb_sglistpa = htole32(sc->sc_sgseg[0].ds_addr +
			    cmd->acc_id * sizeof(*sg) * AMI_SGEPERCMD);
			TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
		} else {
			sc->sc_mbox = cmd;
			sc->sc_mbox_pa = sc->sc_cmdseg[0].ds_addr;
			AMI_DPRINTF(AMI_D_CMD, ("mbox_pa=%llx ",
			    sc->sc_mbox_pa));
		}
	}

	timeout_set(&sc->sc_poll_tmo, (void (*)(void *))ami_intr, sc);

	(sc->sc_init)(sc);
	{
		paddr_t	pa = idataseg[0].ds_addr;
		ami_lock_t lock;

		lock = AMI_LOCK_AMI(sc);

		ccb = ami_get_ccb(sc);
		cmd = ccb->ccb_cmd;

		/* try FC inquiry first */
		cmd->acc_cmd = AMI_FCOP;
		cmd->acc_io.aio_channel = AMI_FC_EINQ3;
		cmd->acc_io.aio_param = AMI_FC_EINQ3_SOLICITED_FULL;
		cmd->acc_io.aio_data = htole32(pa);
		if (ami_cmd(ccb, 0, 1) == 0) {
			struct ami_fc_einquiry *einq = idata;
			struct ami_fc_prodinfo *pi = idata;

			sc->sc_nunits = einq->ain_nlogdrv;
			ami_copyhds(sc, einq->ain_ldsize, einq->ain_ldprop,
			    einq->ain_ldstat);

			ccb = ami_get_ccb(sc);
			cmd = ccb->ccb_cmd;

			cmd->acc_cmd = AMI_FCOP;
			cmd->acc_io.aio_channel = AMI_FC_PRODINF;
			cmd->acc_io.aio_param = 0;
			cmd->acc_io.aio_data = htole32(pa);
			if (ami_cmd(ccb, 0, 1) == 0) {
				sc->sc_maxunits = AMI_BIG_MAX_LDRIVES;

				bcopy (pi->api_fwver, sc->sc_fwver, 16);
				sc->sc_fwver[15] = '\0';
				bcopy (pi->api_biosver, sc->sc_biosver, 16);
				sc->sc_biosver[15] = '\0';
				sc->sc_channels = pi->api_channels;
				sc->sc_targets = pi->api_fcloops;
				sc->sc_memory = letoh16(pi->api_ramsize);
				sc->sc_maxcmds = pi->api_maxcmd;
				p = "FC loop";
			}
		}

		if (sc->sc_maxunits == 0) {
			struct ami_inquiry *inq = idata;

			ccb = ami_get_ccb(sc);
			cmd = ccb->ccb_cmd;

			cmd->acc_cmd = AMI_EINQUIRY;
			cmd->acc_io.aio_channel = 0;
			cmd->acc_io.aio_param = 0;
			cmd->acc_io.aio_data = htole32(pa);
			if (ami_cmd(ccb, 0, 1) != 0) {
				ccb = ami_get_ccb(sc);
				cmd = ccb->ccb_cmd;

				cmd->acc_cmd = AMI_INQUIRY;
				cmd->acc_io.aio_channel = 0;
				cmd->acc_io.aio_param = 0;
				cmd->acc_io.aio_data = htole32(pa);
				if (ami_cmd(ccb, 0, 1) != 0) {
					AMI_UNLOCK_AMI(sc, lock);
					printf(": cannot do inquiry\n");
					ami_dispose(sc);
					ami_freemem(sc->dmat, &idatamap,
					    idataseg, NBPG, 1, "init data");
					return (1);
				}
			}

			sc->sc_maxunits = AMI_MAX_LDRIVES;
			sc->sc_nunits = inq->ain_nlogdrv;
			ami_copyhds(sc, inq->ain_ldsize, inq->ain_ldprop,
			    inq->ain_ldstat);

			bcopy (inq->ain_fwver, sc->sc_fwver, 4);
			sc->sc_fwver[4] = '\0';
			bcopy (inq->ain_biosver, sc->sc_biosver, 4);
			sc->sc_biosver[4] = '\0';
			sc->sc_channels = inq->ain_channels;
			sc->sc_targets = inq->ain_targets;
			sc->sc_memory = inq->ain_ramsize;
			sc->sc_maxcmds = inq->ain_maxcmd;
			p = "target";
		}

		AMI_UNLOCK_AMI(sc, lock);

		if (sc->sc_maxcmds > AMI_MAXCMDS)
			sc->sc_maxcmds = 1 /* AMI_MAXCMDS */;
	}
	ami_freemem(sc->dmat, &idatamap, idataseg, NBPG, 1, "init data");

	/* hack for hp netraid version encoding */
	if ('A' <= sc->sc_fwver[2] && sc->sc_fwver[2] <= 'Z' &&
	    sc->sc_fwver[1] < ' ' && sc->sc_fwver[0] < ' ' &&
	    'A' <= sc->sc_biosver[2] && sc->sc_biosver[2] <= 'Z' &&
	    sc->sc_biosver[1] < ' ' && sc->sc_biosver[0] < ' ') {

		sprintf(sc->sc_fwver, "%c.%02d.%02d", sc->sc_fwver[2],
		    sc->sc_fwver[1], sc->sc_fwver[0]);
		sprintf(sc->sc_biosver, "%c.%02d.%02d", sc->sc_biosver[2],
		    sc->sc_biosver[1], sc->sc_biosver[0]);
	}

	printf(": FW %s, BIOS v%s, %dMB RAM\n"
	     "%s: %d channels, %d %ss, %d logical drives\n",
	    sc->sc_fwver, sc->sc_biosver, sc->sc_memory,
	    sc->sc_dev.dv_xname,
	    sc->sc_channels, sc->sc_targets, p, sc->sc_nunits);

	/* TODO: fetch & print cache strategy */
	/* TODO: fetch & print scsi and raid info */

	sc->sc_link.device = &ami_dev;
	sc->sc_link.openings = sc->sc_maxcmds;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &ami_switch;
	sc->sc_link.adapter_target = sc->sc_maxunits;
	sc->sc_link.adapter_buswidth = sc->sc_maxunits;

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);
#if 0
	rsc = malloc(sizeof(struct ami_rawsoftc) * sc->sc_channels,
	    M_DEVBUF, M_NOWAIT);
	if (!rsc) {
		printf("%s: no memory for raw interface\n",
		    sc->sc_dev.dv_xname);
		return (0);
	}

	bzero(rsc, sizeof(struct ami_rawsoftc) * sc->sc_channels);
	for (sc->sc_rawsoftcs = rsc;
	     rsc < &sc->sc_rawsoftcs[sc->sc_channels]; rsc++) {

		/* TODO fetch and print channel properties */

		rsc->sc_softc = sc;
		rsc->sc_channel = rsc - sc->sc_rawsoftcs;
		rsc->sc_link.device = &ami_raw_dev;
		rsc->sc_link.openings = sc->sc_maxcmds;
		rsc->sc_link.adapter_softc = rsc;
		rsc->sc_link.adapter = &ami_raw_switch;
		/* TODO fetch it from the controller */
		rsc->sc_link.adapter_target = sc->sc_targets;
		rsc->sc_link.adapter_buswidth = sc->sc_targets;

		config_found(&sc->sc_dev, &rsc->sc_link, scsiprint);
	}
#endif
	return 0;
}

int
ami_quartz_init(sc)
	struct ami_softc *sc;
{
	bus_space_write_4(sc->iot, sc->ioh, AMI_QIDB, 0);
	bus_space_barrier(sc->iot, sc->ioh,
		    AMI_QIDB, 4, BUS_SPACE_BARRIER_WRITE);

	return 0;
}

int
ami_quartz_exec(sc, cmd)
	struct ami_softc *sc;
	struct ami_iocmd *cmd;
{
	u_int32_t qidb;

	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QIDB, 4, BUS_SPACE_BARRIER_READ);
	qidb = bus_space_read_4(sc->iot, sc->ioh, AMI_QIDB);
	if (qidb & (AMI_QIDB_EXEC | AMI_QIDB_ACK)) {
		AMI_DPRINTF(AMI_D_CMD, ("qidb1=%x ", qidb));
		return (EBUSY);
	}

	/* do not scramble the busy mailbox */
	if (sc->sc_mbox->acc_busy) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return (EBUSY);
	}

	*sc->sc_mbox = *cmd;
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, sizeof(*cmd),
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	qidb = sc->sc_mbox_pa | AMI_QIDB_EXEC;
	AMI_DPRINTF(AMI_D_CMD, ("qidb2=%x ", qidb));
	bus_space_write_4(sc->iot, sc->ioh, AMI_QIDB, qidb);
	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QIDB, 4, BUS_SPACE_BARRIER_WRITE);
	return (0);
}

int
ami_quartz_done(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int32_t qdb;

	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QIDB, 4, BUS_SPACE_BARRIER_READ);
	qdb = bus_space_read_4(sc->iot, sc->ioh, AMI_QIDB);
	if (qdb & (AMI_QIDB_EXEC | AMI_QIDB_ACK)) {
		AMI_DPRINTF(AMI_D_CMD, ("qidb3=%x ", qdb));
		return (0);
	}

	/* do not scramble the busy mailbox */
	if (sc->sc_mbox->acc_busy) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return (0);
	}

	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QODB, 4, BUS_SPACE_BARRIER_READ);
	qdb = bus_space_read_4(sc->iot, sc->ioh, AMI_QODB);
	if (qdb == AMI_QODB_READY) {

		bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, sizeof(*mbox),
		    BUS_DMASYNC_POSTWRITE);
		*mbox = *sc->sc_mbox;

		/* ack interrupt */
		bus_space_write_4(sc->iot, sc->ioh, AMI_QODB, AMI_QODB_READY);
		bus_space_barrier(sc->iot, sc->ioh,
		    AMI_QODB, 4, BUS_SPACE_BARRIER_WRITE);

		qdb = sc->sc_mbox_pa | AMI_QIDB_ACK;
		bus_space_write_4(sc->iot, sc->ioh, AMI_QIDB, qdb);
		bus_space_barrier(sc->iot, sc->ioh,
		    AMI_QIDB, 4, BUS_SPACE_BARRIER_WRITE);
		return (1);
	}

	AMI_DPRINTF(AMI_D_CMD, ("qodb=%x ", qdb));

	return (0);
}

int
ami_schwartz_init(sc)
	struct ami_softc *sc;
{
	u_int32_t a = (u_int32_t)sc->sc_mbox_pa;

	bus_space_write_4(sc->iot, sc->ioh, AMI_SMBADDR, a);
	/* XXX 40bit address ??? */
	bus_space_write_1(sc->iot, sc->ioh, AMI_SMBENA, 0);

	bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_ACK);
	bus_space_write_1(sc->iot, sc->ioh, AMI_SIEM, AMI_SEIM_ENA |
	    bus_space_read_1(sc->iot, sc->ioh, AMI_SIEM));

	return 0;
}

int
ami_schwartz_exec(sc, cmd)
	struct ami_softc *sc;
	struct ami_iocmd *cmd;
{
	if (bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY)
		return EBUSY;

	*sc->sc_mbox = *cmd;
	bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_EXEC);
	return 0;
}

int
ami_schwartz_done(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int8_t stat;
#if 0
	/* do not scramble the busy mailbox */
	if (sc->sc_mbox->acc_busy)
		return (0);
#endif
	if (bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY)
		return 0;

	stat = bus_space_read_1(sc->iot, sc->ioh, AMI_ISTAT);
	if (stat & AMI_ISTAT_PEND) {
		bus_space_write_1(sc->iot, sc->ioh, AMI_ISTAT, stat);

		*mbox = *sc->sc_mbox;

		bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_ACK);

		return 1;
	}

	return 0;
}

int
ami_cmd(ccb, flags, wait)
	struct ami_ccb *ccb;
	int flags, wait;
{
	struct ami_softc *sc = ccb->ccb_sc;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error = 0, i, s;

	if (ccb->ccb_data) {
		struct ami_iocmd *cmd = ccb->ccb_cmd;
		bus_dma_segment_t *sgd;

		error = bus_dmamap_load(sc->dmat, dmap, ccb->ccb_data,
		    ccb->ccb_len, NULL, flags);
		if (error) {
			if (error == EFBIG)
				printf("more than %d dma segs\n", AMI_MAXOFFSETS);
			else
				printf("error %d loading dma map\n", error);

			ami_put_ccb(ccb);
			return (error);
		}

		sgd = dmap->dm_segs;
		AMI_DPRINTF(AMI_D_DMA, ("data=%p/%u<0x%lx/%u",
		    ccb->ccb_data, ccb->ccb_len,
		    sgd->ds_addr, sgd->ds_len));

		if(dmap->dm_nsegs > 1) {
			struct ami_sgent *sgl = ccb->ccb_sglist;

			cmd->acc_mbox.amb_nsge = htole32(dmap->dm_nsegs);
			cmd->acc_mbox.amb_data = ccb->ccb_sglistpa;

			for (i = 0; i < dmap->dm_nsegs; i++, sgd++) {
				sgl[i].asg_addr = htole32(sgd->ds_addr);
				sgl[i].asg_len  = htole32(sgd->ds_len);
				if (i)
					AMI_DPRINTF(AMI_D_DMA, (",0x%lx/%u",
					    sgd->ds_addr, sgd->ds_len));
			}
		} else {
			cmd->acc_mbox.amb_nsge = htole32(0);
			cmd->acc_mbox.amb_data = htole32(sgd->ds_addr);
		}
		AMI_DPRINTF(AMI_D_DMA, ("> "));

		bus_dmamap_sync(sc->dmat, dmap, 0, dmap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);
	} else
		ccb->ccb_cmd->acc_mbox.amb_nsge = htole32(0);
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, sc->sc_cmdmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	s = splimp();
	if ((error = ami_start(ccb, wait))) {
		AMI_DPRINTF(AMI_D_DMA, ("error=%d ", error));
		__asm __volatile(".globl _bpamierr\n_bpamierr:");
		if (ccb->ccb_data)
			bus_dmamap_unload(sc->dmat, dmap);
		ami_put_ccb(ccb);
	} else if (wait) {
		AMI_DPRINTF(AMI_D_DMA, ("waiting "));
		if ((error = ami_complete(ccb)))
			ami_put_ccb(ccb);
	}
	splx(s);

	return (error);
}

int
ami_start(ccb, wait)
	struct ami_ccb *ccb;
	int wait;
{
	struct ami_softc *sc = ccb->ccb_sc;
	struct ami_iocmd *cmd = ccb->ccb_cmd;
	struct scsi_xfer *xs = ccb->ccb_xs;
	volatile struct ami_iocmd *mbox = sc->sc_mbox;
	int i;

	AMI_DPRINTF(AMI_D_CMD, ("start(%d) ", cmd->acc_id));

	if (ccb->ccb_state != AMI_CCB_READY) {
		printf("%s: ccb %d not ready <%d>\n",
		    sc->sc_dev.dv_xname, cmd->acc_id, ccb->ccb_state);
		return (EINVAL);
	}

	if (xs)
		timeout_set(&xs->stimeout, ami_stimeout, ccb);

	if (wait && mbox->acc_busy) {

		for (i = 100000; i-- && mbox->acc_busy; DELAY(10));

		if (mbox->acc_busy) {
			AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
			return (EAGAIN);
		}
	}

	AMI_DPRINTF(AMI_D_CMD, ("exec "));

	cmd->acc_busy = 1;
	cmd->acc_poll = 0;
	cmd->acc_ack = 0;

	if (!(i = (sc->sc_exec)(sc, cmd))) {
		ccb->ccb_state = AMI_CCB_QUEUED;
		TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ccb_link);
		if (!wait) {
#ifdef AMI_POLLING
			if (!timeout_pending(&sc->sc_poll_tmo))
				timeout_add(&sc->sc_poll_tmo, 1);
#endif
			if (xs) {
				struct timeval tv;
				tv.tv_sec = xs->timeout / 1000;
				tv.tv_usec = 1000 * (xs->timeout % 1000);
				timeout_add(&xs->stimeout, tvtohz(&tv));
			}
		}
	} else if (!wait && xs) {
		AMI_DPRINTF(AMI_D_CMD, ("2queue1(%d) ", cmd->acc_id));
		ccb->ccb_state = AMI_CCB_PREQUEUED;
		timeout_add(&xs->stimeout, 1);
		return (0);
	}

	return (i);
}

void
ami_stimeout(v)
	void *v;
{
	struct ami_ccb *ccb = v;
	struct ami_softc *sc = ccb->ccb_sc;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct ami_iocmd *cmd = ccb->ccb_cmd;
	volatile struct ami_iocmd *mbox = sc->sc_mbox;
	ami_lock_t lock, s;

	lock = AMI_LOCK_AMI(sc);
	switch (ccb->ccb_state) {
	case AMI_CCB_PREQUEUED:
		if (mbox->acc_busy) {
			timeout_add(&xs->stimeout, 1);
			break;
		}

		AMI_DPRINTF(AMI_D_CMD, ("requeue(%d) ", cmd->acc_id));

		ccb->ccb_state = AMI_CCB_READY;
		if (ami_start(ccb, 0)) {
			AMI_DPRINTF(AMI_D_CMD, ("requeue(%d) again\n", cmd->acc_id));
			ccb->ccb_state = AMI_CCB_PREQUEUED;
			timeout_add(&xs->stimeout, 1);
		}
		break;

	case AMI_CCB_QUEUED:
		/* XXX need to kill all cmds in the queue and reset the card */
		printf("%s: timeout ccb %d\n",
		    sc->sc_dev.dv_xname, cmd->acc_id);
		AMI_DPRINTF(AMI_D_CMD, ("timeout(%d) ", cmd->acc_id));
		if (xs->cmd->opcode != PREVENT_ALLOW &&
		    xs->cmd->opcode != SYNCHRONIZE_CACHE) {
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmamap->dm_mapsize,
			    (xs->flags & SCSI_DATA_IN) ?
			    BUS_DMASYNC_POSTREAD :
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
		}
		s = splimp();
		TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);
		ami_put_ccb(ccb);
		splx(s);
		xs->error = XS_TIMEOUT;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		break;
	case AMI_CCB_FREE:
	case AMI_CCB_READY:
		panic("ami_stimeout(%p) botch", cmd->acc_id);
	}
	AMI_UNLOCK_AMI(sc, lock);
}

int
ami_complete(ccb)
	struct ami_ccb *ccb;
{
	struct ami_softc *sc = ccb->ccb_sc;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct ami_iocmd mbox;
	int i, j, rv, status;

	i = 1 * (xs? xs->timeout: 1000);
	AMI_DPRINTF(AMI_D_CMD, ("%d ", i));
	for (rv = 1, status = 0; !status && rv && i--; DELAY(1000))
		if ((sc->sc_done)(sc, &mbox)) {
			AMI_DPRINTF(AMI_D_CMD, ("got#%d ", mbox.acc_nstat));
			status = mbox.acc_status;
			for (j = 0; j < mbox.acc_nstat; j++ ) {
				int ready = mbox.acc_cmplidl[j];

				AMI_DPRINTF(AMI_D_CMD, ("ready=%x ", ready));

				if (!ami_done(sc, ready) &&
				    ccb->ccb_cmd->acc_id == ready)
					rv = 0;
			}
		}

	if (status) {
		AMI_DPRINTF(AMI_D_CMD, ("aborted\n"));
	} else if (!rv) {
		AMI_DPRINTF(AMI_D_CMD, ("complete\n"));
	} else if (i < 0) {
		AMI_DPRINTF(AMI_D_CMD, ("timeout\n"));
	} else
		AMI_DPRINTF(AMI_D_CMD, ("screwed\n"));

	return rv? rv : status;
}

int
ami_done(sc, idx)
	struct ami_softc *sc;
	int	idx;
{
	struct ami_ccb *ccb = &sc->sc_ccbs[idx - 1];
	struct scsi_xfer *xs = ccb->ccb_xs;
	ami_lock_t lock, s;

	AMI_DPRINTF(AMI_D_CMD, ("done(%d) ", ccb->ccb_cmd->acc_id));

	if (ccb->ccb_state != AMI_CCB_QUEUED) {
		printf("%s: unqueued ccb %d ready, state = %d\n",
		    sc->sc_dev.dv_xname, idx, ccb->ccb_state);
		return (1);
	}

	lock = AMI_LOCK_AMI(sc);
	s = splimp();
	ccb->ccb_state = AMI_CCB_READY;
	TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);

	if (xs) {
		timeout_del(&xs->stimeout);
		if (xs->cmd->opcode != PREVENT_ALLOW &&
		    xs->cmd->opcode != SYNCHRONIZE_CACHE) {
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmamap->dm_mapsize,
			    (xs->flags & SCSI_DATA_IN) ?
			    BUS_DMASYNC_POSTREAD :
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
		}
		ccb->ccb_xs = NULL;
	} else {
		struct ami_iocmd *cmd = ccb->ccb_cmd;

		switch (cmd->acc_cmd) {
		case AMI_INQUIRY:
		case AMI_EINQUIRY:
		case AMI_EINQUIRY3:
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
			break;
		default:
			/* no data */
			break;
		}
	}

	ami_put_ccb(ccb);
	splx(s);

	if (xs) {
		xs->resid = 0;
		xs->flags |= ITSDONE;
		AMI_DPRINTF(AMI_D_CMD, ("scsi_done(%d) ", idx));
		scsi_done(xs);
	}

	AMI_UNLOCK_AMI(sc, lock);

	return (0);
}

void
amiminphys(bp)
	struct buf *bp;
{
	if (bp->b_bcount > AMI_MAXFER)
		bp->b_bcount = AMI_MAXFER;
	minphys(bp);
}

void
ami_copy_internal_data(xs, v, size)
	struct scsi_xfer *xs;
	void *v;
	size_t size;
{
	size_t copy_cnt;

	AMI_DPRINTF(AMI_D_MISC, ("ami_copy_internal_data "));

	if (!xs->datalen)
		printf("uio move not yet supported\n");
	else {
		copy_cnt = MIN(size, xs->datalen);
		bcopy(v, xs->data, copy_cnt);
	}
}

int
ami_scsi_raw_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct ami_rawsoftc *rsc = link->adapter_softc;
	struct ami_softc *sc = rsc->sc_softc;
	u_int8_t channel = rsc->sc_channel, target = link->target;
	struct ami_ccb *ccb, *ccb1;
	struct ami_iocmd *cmd;
	struct ami_passthrough *ps;
	int error;
	ami_lock_t lock;

	AMI_DPRINTF(AMI_D_CMD, ("ami_scsi_raw_cmd "));

	lock = AMI_LOCK_AMI(sc);

	if (xs->cmdlen > AMI_MAX_CDB) {
		AMI_DPRINTF(AMI_D_CMD, ("CDB too big %p ", xs));
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20; /* illcmd, 0x24 illfield */
		xs->error = XS_SENSE;
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	xs->error = XS_NOERROR;

	if ((ccb = ami_get_ccb(sc)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	if ((ccb1 = ami_get_ccb(sc)) == NULL) {
		ami_put_ccb(ccb);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	ccb->ccb_xs = xs;
	ccb->ccb_ccb1 = ccb1;
	ccb->ccb_len  = xs->datalen;
	ccb->ccb_data = xs->data;

	ps = (struct ami_passthrough *)ccb1->ccb_cmd;
	ps->apt_param = AMI_PTPARAM(AMI_TIMEOUT_6,1,0);
	ps->apt_channel = channel;
	ps->apt_target = target;
	bcopy(xs->cmd, ps->apt_cdb, AMI_MAX_CDB);
	ps->apt_ncdb = xs->cmdlen;
	ps->apt_nsense = AMI_MAX_SENSE;

	cmd = ccb->ccb_cmd;
	cmd->acc_cmd = AMI_PASSTHRU;
	cmd->acc_passthru.apt_data = ccb1->ccb_cmdpa;

	if ((error = ami_cmd(ccb, ((xs->flags & SCSI_NOSLEEP)?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK), xs->flags & SCSI_POLL))) {

		AMI_DPRINTF(AMI_D_CMD, ("failed %p ", xs));
		if (xs->flags & SCSI_POLL) {
			xs->error = XS_TIMEOUT;
			AMI_UNLOCK_AMI(sc, lock);
			return (TRY_AGAIN_LATER);
		} else {
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			AMI_UNLOCK_AMI(sc, lock);
			return (COMPLETE);
		}
	}


	if (xs->flags & SCSI_POLL) {
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	AMI_UNLOCK_AMI(sc, lock);
	return (SUCCESSFULLY_QUEUED);
}

int
ami_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct ami_softc *sc = link->adapter_softc;
	struct ami_ccb *ccb;
	struct ami_iocmd *cmd;
	struct scsi_inquiry_data inq;
	struct scsi_sense_data sd;
	struct {
		struct scsi_mode_header hd;
		struct scsi_blk_desc bd;
		union scsi_disk_pages dp;
	} mpd;
	struct scsi_read_cap_data rcd;
	u_int8_t target = link->target;
	u_int32_t blockno, blockcnt;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	int error, flags;
	ami_lock_t lock;

	AMI_DPRINTF(AMI_D_CMD, ("ami_scsi_cmd "));

	lock = AMI_LOCK_AMI(sc);
	if (target >= sc->sc_nunits || !sc->sc_hdr[target].hd_present ||
	    link->lun != 0) {
		AMI_DPRINTF(AMI_D_CMD, ("no taget %d ", target));
		/* XXX should be XS_SENSE and sense filled out */
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	error = 0;
	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	case TEST_UNIT_READY:
	case START_STOP:
#if 0
	case VERIFY:
#endif
		AMI_DPRINTF(AMI_D_CMD, ("opc %d tgt %d ", xs->cmd->opcode,
		    target));
		break;

	case REQUEST_SENSE:
		AMI_DPRINTF(AMI_D_CMD, ("REQUEST SENSE tgt %d ", target));
		bzero(&sd, sizeof sd);
		sd.error_code = 0x70;
		sd.segment = 0;
		sd.flags = SKEY_NO_SENSE;
		*(u_int32_t*)sd.info = htole32(0);
		sd.extra_len = 0;
		ami_copy_internal_data(xs, &sd, sizeof sd);
		break;

	case INQUIRY:
		AMI_DPRINTF(AMI_D_CMD, ("INQUIRY tgt %d ", target));
		bzero(&inq, sizeof inq);
		inq.device = T_DIRECT;
		inq.dev_qual2 = 0;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		strcpy(inq.vendor, "AMI    ");
		sprintf(inq.product, "Host drive  #%02d", target);
		strcpy(inq.revision, "   ");
		ami_copy_internal_data(xs, &inq, sizeof inq);
		break;

	case MODE_SENSE:
		AMI_DPRINTF(AMI_D_CMD, ("MODE SENSE tgt %d ", target));

		bzero(&mpd, sizeof mpd);
		switch (((struct scsi_mode_sense *)xs->cmd)->page) {
		case 4:
			/* scsi_disk.h says this should be 0x16 */
			mpd.dp.rigid_geometry.pg_length = 0x16;
			mpd.hd.data_length = sizeof mpd.hd + sizeof mpd.bd +
			    mpd.dp.rigid_geometry.pg_length;
			mpd.hd.blk_desc_len = sizeof mpd.bd;

			mpd.hd.dev_spec = 0;	/* writeprotect ? XXX */
			_lto3b(AMI_SECTOR_SIZE, mpd.bd.blklen);
			mpd.dp.rigid_geometry.pg_code = 4;
			_lto3b(sc->sc_hdr[target].hd_size /
			    sc->sc_hdr[target].hd_heads /
			    sc->sc_hdr[target].hd_secs,
			    mpd.dp.rigid_geometry.ncyl);
			mpd.dp.rigid_geometry.nheads =
			    sc->sc_hdr[target].hd_heads;
			ami_copy_internal_data(xs, (u_int8_t *)&mpd,
			    sizeof mpd);
			break;

		default:
			printf("%s: mode sense page %d not simulated\n",
			    sc->sc_dev.dv_xname,
			    ((struct scsi_mode_sense *)xs->cmd)->page);
			xs->error = XS_DRIVER_STUFFUP;
		}
		break;

	case READ_CAPACITY:
		AMI_DPRINTF(AMI_D_CMD, ("READ CAPACITY tgt %d ", target));
		bzero(&rcd, sizeof rcd);
		_lto4b(sc->sc_hdr[target].hd_size - 1, rcd.addr);
		_lto4b(AMI_SECTOR_SIZE, rcd.length);
		ami_copy_internal_data(xs, &rcd, sizeof rcd);
		break;

	case PREVENT_ALLOW:
		AMI_DPRINTF(AMI_D_CMD, ("PREVENT/ALLOW "));
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);

	case SYNCHRONIZE_CACHE:
		AMI_DPRINTF(AMI_D_CMD, ("SYNCHRONIZE CACHE "));
		error++;
	case READ_COMMAND:
		if (!error) {
			AMI_DPRINTF(AMI_D_CMD, ("READ "));
			error++;
		}
	case READ_BIG:
		if (!error) {
			AMI_DPRINTF(AMI_D_CMD, ("READ BIG "));
			error++;
		}
	case WRITE_COMMAND:
		if (!error) {
			AMI_DPRINTF(AMI_D_CMD, ("WRITE "));
			error++;
		}
	case WRITE_BIG:
		if (!error) {
			AMI_DPRINTF(AMI_D_CMD, ("WRITE BIG "));
			error++;
		}

		flags = xs->flags;
		if (xs->cmd->opcode != SYNCHRONIZE_CACHE) {
			/* A read or write operation. */
			if (xs->cmdlen == 6) {
				rw = (struct scsi_rw *)xs->cmd;
				blockno = _3btol(rw->addr) &
				    (SRW_TOPADDR << 16 | 0xffff);
				blockcnt = rw->length ? rw->length : 0x100;
			} else {
				rwb = (struct scsi_rw_big *)xs->cmd;
				blockno = _4btol(rwb->addr);
				blockcnt = _2btol(rwb->length);
				/* TODO: reflect DPO & FUA flags */
				if (xs->cmd->opcode == WRITE_BIG &&
				    rwb->byte2 & 0x18)
					flags |= 0;
			}
			if (blockno >= sc->sc_hdr[target].hd_size ||
			    blockno + blockcnt > sc->sc_hdr[target].hd_size) {
				printf("%s: out of bounds %u-%u >= %u\n",
				    sc->sc_dev.dv_xname, blockno, blockcnt,
				    sc->sc_hdr[target].hd_size);
				xs->error = XS_DRIVER_STUFFUP;
				scsi_done(xs);
				AMI_UNLOCK_AMI(sc, lock);
				return (COMPLETE);
			}
		}

		if ((ccb = ami_get_ccb(sc)) == NULL) {
			AMI_DPRINTF(AMI_D_CMD, ("no more ccbs "));
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			AMI_UNLOCK_AMI(sc, lock);
		__asm __volatile(".globl _bpamiccb\n_bpamiccb:");
			return (COMPLETE);
		}

		ccb->ccb_xs = xs;
		ccb->ccb_ccb1 = NULL;
		ccb->ccb_len  = xs->datalen;
		ccb->ccb_data = xs->data;
		cmd = ccb->ccb_cmd;
		cmd->acc_mbox.amb_nsect = htole16(blockcnt);
		cmd->acc_mbox.amb_lba = htole32(blockno);
		cmd->acc_mbox.amb_ldn = target;
		cmd->acc_mbox.amb_data = 0;

		switch (xs->cmd->opcode) {
		case SYNCHRONIZE_CACHE:
			cmd->acc_cmd = AMI_FLUSH;
			if (xs->timeout < 30000)
				xs->timeout = 30000;	/* at least 30sec */
			break;
		case READ_COMMAND: case READ_BIG:
			cmd->acc_cmd = AMI_READ;
			break;
		case WRITE_COMMAND: case WRITE_BIG:
			cmd->acc_cmd = AMI_WRITE;
			break;
		}

		if ((error = ami_cmd(ccb, ((flags & SCSI_NOSLEEP)?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK), flags & SCSI_POLL))) {

			AMI_DPRINTF(AMI_D_CMD, ("failed %p ", xs));
		__asm __volatile(".globl _bpamifail\n_bpamifail:");
			if (flags & SCSI_POLL) {
				xs->error = XS_TIMEOUT;
				AMI_UNLOCK_AMI(sc, lock);
				return (TRY_AGAIN_LATER);
			} else {
				xs->error = XS_DRIVER_STUFFUP;
				scsi_done(xs);
				AMI_UNLOCK_AMI(sc, lock);
				return (COMPLETE);
			}
		}

		AMI_UNLOCK_AMI(sc, lock);
		if (flags & SCSI_POLL)
			return (COMPLETE);
		else
			return (SUCCESSFULLY_QUEUED);

	default:
		AMI_DPRINTF(AMI_D_CMD, ("unknown opc %d ", xs->cmd->opcode));
		xs->error = XS_DRIVER_STUFFUP;
	}

	AMI_UNLOCK_AMI(sc, lock);
	return (COMPLETE);
}

int
ami_intr(v)
	void *v;
{
	struct ami_softc *sc = v;
	struct ami_iocmd mbox;
	int i, s, rv = 0;
	ami_lock_t lock;

	if (TAILQ_EMPTY(&sc->sc_ccbq))
		return (0);

	AMI_DPRINTF(AMI_D_INTR, ("intr "));

	lock = AMI_LOCK_AMI(sc);
	s = splimp();	/* XXX need to do this to mask timeouts */
	while ((sc->sc_done)(sc, &mbox)) {
		AMI_DPRINTF(AMI_D_CMD, ("got#%d ", mbox.acc_nstat));
		for (i = 0; i < mbox.acc_nstat; i++ ) {
			int ready = mbox.acc_cmplidl[i];

			AMI_DPRINTF(AMI_D_CMD, ("ready=%d ", ready));

			if (!ami_done(sc, ready))
				rv |= 1;
		}
	}

#ifdef AMI_POLLING
	if (!TAILQ_EMPTY(&sc->sc_ccbq) && !timeout_pending(&sc->sc_poll_tmo)) {
		AMI_DPRINTF(AMI_D_INTR, ("tmo "));
		timeout_add(&sc->sc_poll_tmo, 2);
	}
#endif

	splx(s);
	AMI_UNLOCK_AMI(sc, lock);
	AMI_DPRINTF(AMI_D_INTR, ("exit "));
	return (rv);
}
