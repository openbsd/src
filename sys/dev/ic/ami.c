/*	$OpenBSD: ami.c,v 1.3 2001/03/14 23:42:33 deraadt Exp $	*/

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

#include <vm/vm.h>
#include <vm/vm_kern.h>
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
int ami_debug = 0xffff;
#else
#define	AMI_DPRINTF(m,a)	/* m, a */
#endif

struct cfdriver ami_cd = {
	NULL, "ami", DV_DULL
};

int	ami_scsi_cmd __P((struct scsi_xfer *xs));
void	amiminphys __P((struct buf *bp));

struct scsi_adapter ami_switch = {
	ami_scsi_cmd, amiminphys, 0, 0,
};

struct scsi_device ami_dev = {
	NULL, NULL, NULL, NULL
};

int	ami_scsi_raw_cmd __P((struct scsi_xfer *xs));

struct scsi_adapter ami_raw_switch = {
	ami_scsi_raw_cmd, amiminphys, 0, 0,
};

struct scsi_device ami_raw_dev = {
	NULL, NULL, NULL, NULL
};

static __inline struct ami_ccb *ami_get_ccb __P((struct ami_softc *sc));
static __inline void ami_put_ccb __P((struct ami_ccb *ccb));
void ami_copyhds __P((struct ami_softc *sc, const u_int32_t *sizes,
	const u_int8_t *props, const u_int8_t *stats));
void *ami_allocmem __P((bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname));
void ami_freemem __P((bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname));
void ami_dispose __P((struct ami_softc *sc));
void ami_requeue __P((void *v));
int  ami_cmd __P((struct ami_ccb *ccb, int flags, int wait));
int  ami_start __P((struct ami_ccb *ccb, int wait));
int  ami_complete __P((struct ami_ccb *ccb));
int  ami_done __P((struct ami_softc *sc, int idx));
void ami_copy_internal_data __P((struct scsi_xfer *xs, void *v, size_t size));
int  ami_inquire __P((struct ami_softc *sc, u_int8_t op));


static __inline struct ami_ccb *
ami_get_ccb(sc)
	struct ami_softc *sc;
{
	struct ami_ccb *ccb;

	ccb = TAILQ_LAST(&sc->sc_free_ccb, ami_queue_head);
	if (ccb) {
		ccb->ccb_state = AMI_CCB_READY;
		TAILQ_REMOVE(&sc->sc_free_ccb, ccb, ccb_link);
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
		sc->sc_hdr[i].hd_size = sizes[i];
		sc->sc_hdr[i].hd_prop = props[i];
		sc->sc_hdr[i].hd_stat = stats[i];
		if (sizes[i] > 0x200000) {
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
	    sizeof(struct ami_iocmd), AMI_MAXCMDS + 1, "command");
	if (!sc->sc_cmds) {
		ami_dispose(sc);
		ami_freemem(sc->dmat, &idatamap,
		    idataseg, NBPG, 1, "init data");
		return 1;
	}
	sc->sc_sgents = ami_allocmem(sc->dmat, &sc->sc_sgmap, sc->sc_sgseg,
	    sizeof(struct ami_sgent) * AMI_SGEPERCMD, AMI_MAXCMDS, "sglist");
	if (!sc->sc_sgents) {
		ami_dispose(sc);
		ami_freemem(sc->dmat, &idatamap,
		    idataseg, NBPG, 1, "init data");
		return 1;
	}

	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_ccb2q);
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
			ccb->ccb_cmdpa = sc->sc_cmdseg[0].ds_addr +
			    cmd->acc_id * sizeof(*cmd);
			ccb->ccb_sglist = sg;
			ccb->ccb_sglistpa = sc->sc_sgseg[0].ds_addr +
			    cmd->acc_id * sizeof(*sg) * AMI_SGEPERCMD;
			TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
		} else {
			sc->sc_mbox = cmd;
			sc->sc_mbox_pa = sc->sc_cmdseg[0].ds_addr;
		}
	}

	(sc->sc_init)(sc);
	{
		struct ami_inquiry *inq = idata;
		struct ami_fc_einquiry *einq = idata;
		paddr_t	pa = idataseg[0].ds_addr;

		ccb = ami_get_ccb(sc);
		cmd = ccb->ccb_cmd;

		/* try FC inquiry first */
		cmd->acc_cmd = AMI_FCOP;
		cmd->acc_io.aio_channel = AMI_FC_EINQ3;
		cmd->acc_io.aio_param = AMI_FC_EINQ3_SOLICITED_FULL;
		cmd->acc_io.aio_data = pa;
		if (ami_cmd(ccb, 0, 1) == 0) {
			struct ami_fc_prodinfo *pi = idata;

			sc->sc_nunits = einq->ain_nlogdrv;
			ami_copyhds(sc, einq->ain_ldsize, einq->ain_ldprop,
			    einq->ain_ldstat);

			ccb = ami_get_ccb(sc);
			cmd = ccb->ccb_cmd;

			cmd->acc_cmd = AMI_FCOP;
			cmd->acc_io.aio_channel = AMI_FC_PRODINF;
			cmd->acc_io.aio_param = 0;
			cmd->acc_io.aio_data = pa;
			if (ami_cmd(ccb, 0, 1) == 0) {
				sc->sc_maxunits = AMI_BIG_MAX_LDRIVES;

				bcopy (pi->api_fwver, sc->sc_fwver, 16);
				sc->sc_fwver[16] = '\0';
				bcopy (pi->api_biosver, sc->sc_biosver, 16);
				sc->sc_biosver[16] = '\0';
				sc->sc_channels = pi->api_channels;
				sc->sc_targets = pi->api_fcloops;
				sc->sc_memory = pi->api_ramsize;
				sc->sc_maxcmds = pi->api_maxcmd;
				p = "FC loop";
			}
		}

		if (sc->sc_maxunits == 0) {
			ccb = ami_get_ccb(sc);
			cmd = ccb->ccb_cmd;

			cmd->acc_cmd = AMI_EINQUIRY;
			cmd->acc_io.aio_channel = 0;
			cmd->acc_io.aio_param = 0;
			cmd->acc_io.aio_data = pa;
			if (ami_cmd(ccb, 0, 1) != 0) {
				ccb = ami_get_ccb(sc);
				cmd = ccb->ccb_cmd;

				cmd->acc_cmd = AMI_INQUIRY;
				cmd->acc_io.aio_channel = 0;
				cmd->acc_io.aio_param = 0;
				cmd->acc_io.aio_data = kvtop((caddr_t)&inq);
				if (ami_cmd(ccb, 0, 1) != 0) {
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

		if (sc->sc_maxcmds > AMI_MAXCMDS)
			sc->sc_maxcmds = AMI_MAXCMDS;
	}
	ami_freemem(sc->dmat, &idatamap, idataseg, NBPG, 1, "init data");

	/* TODO: fetch & print cache strategy */
	/* TODO: fetch & print scsi and raid info */
	printf(": FW %s, BIOS v%s, %dMB RAM\n"
	     "%s: %d channels, %d %ss, %d logical drives\n",
	    sc->sc_fwver, sc->sc_biosver, sc->sc_memory,
	    sc->sc_dev.dv_xname,
	    sc->sc_channels, sc->sc_targets, p, sc->sc_nunits);

	timeout_set(&sc->sc_requeue_tmo, ami_requeue, sc);

	sc->sc_link.device = &ami_dev;
	sc->sc_link.openings = sc->sc_maxcmds;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &ami_switch;
	sc->sc_link.adapter_target = sc->sc_maxunits;
	sc->sc_link.adapter_buswidth = sc->sc_maxunits;
	sc->sc_link_raw = sc->sc_link;

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

#if notyet
	sc->sc_link_raw.device = &ami_raw_dev;
	sc->sc_link_raw.adapter = &ami_raw_switch;
	sc->sc_link_raw.adapter_target = AMI_MAX_PDRIVES;
	sc->sc_link_raw.adapter_buswidth = AMI_MAX_PDRIVES;

	config_found(&sc->sc_dev, &sc->sc_link_raw, scsiprint);
#endif

	return 0;
}

int
ami_quartz_init(sc)
	struct ami_softc *sc;
{
	return 0;
}

int
ami_quartz_exec(sc)
	struct ami_softc *sc;
{
	u_int32_t qidb;

	qidb = bus_space_read_4(sc->iot, sc->ioh, AMI_QIDB);
	if (letoh32(qidb) & AMI_QIDB_EXEC)
		return EBUSY;

	qidb = sc->sc_mbox_pa | AMI_QIDB_EXEC;
	bus_space_write_4(sc->iot, sc->ioh, AMI_QIDB, htole32(qidb));
	return 0;
}

int
ami_quartz_done(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int32_t qodb;

	qodb = bus_space_read_4(sc->iot, sc->ioh, AMI_QODB);
	if (letoh32(qodb) == AMI_QODB_READY) {

		*mbox = *sc->sc_mbox;

		/* ack interrupt */
		bus_space_write_4(sc->iot, sc->ioh, AMI_QODB, AMI_QODB_READY);

		qodb = sc->sc_mbox_pa | AMI_QIDB_ACK;
		bus_space_write_4(sc->iot, sc->ioh, AMI_QODB, htole32(qodb));
		return 1;
	}

	return 0;
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
ami_schwartz_exec(sc)
	struct ami_softc *sc;
{
	if (bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY)
		return EAGAIN;
	bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_EXEC);
	return 0;
}

int
ami_schwartz_done(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int8_t stat;

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
	int error, s, i;

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
			return error;
		}

		sgd = dmap->dm_segs;
		AMI_DPRINTF(AMI_D_DMA, ("data=%p/%u<0x%lx/%u",
		    ccb->ccb_data, ccb->ccb_len,
		    sgd->ds_addr, sgd->ds_len));

		if(dmap->dm_nsegs > 1) {
			struct ami_sgent *sgl = ccb->ccb_sglist;

			cmd->acc_mbox.amb_nsge = htole32(dmap->dm_nsegs);
			cmd->acc_mbox.amb_data = htole32(ccb->ccb_sglistpa);

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

		bus_dmamap_sync(sc->dmat, dmap, BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, BUS_DMASYNC_PREWRITE);

	/* XXX somehow interrupts have started to happen in autoconf() */
	if (wait)
		s = splbio();

	if ((error = ami_start(ccb, wait))) {
		if (ccb->ccb_data)
			bus_dmamap_unload(sc->dmat, dmap);
		ami_put_ccb(ccb);
	} else if (wait)
		if ((error = ami_complete(ccb)))
			ami_put_ccb(ccb);

	if (wait)
		splx(s);

	return error;
}

int
ami_start(ccb, wait)
	struct ami_ccb *ccb;
	int wait;
{
	struct ami_softc *sc = ccb->ccb_sc;
	struct ami_iocmd *cmd = ccb->ccb_cmd;
	volatile struct ami_iocmd *mbox = sc->sc_mbox;
	int s, i;

	AMI_DPRINTF(AMI_D_CMD, ("start(%d) ", cmd->acc_id));

	if (ccb->ccb_state != AMI_CCB_READY) {
		printf("%s: ccb %d not ready <%d>\n",
		    sc->sc_dev.dv_xname, cmd->acc_id, ccb->ccb_state);
		return EINVAL;
	}

	if (mbox->acc_busy && !wait) {

		ccb->ccb_state = AMI_CCB_PREQUEUED;
		s = splclock();
		TAILQ_INSERT_TAIL(&sc->sc_ccb2q, ccb, ccb_link);
		if (!sc->sc_timeout) {
			sc->sc_timeout++;
			splx(s);
			timeout_add(&sc->sc_requeue_tmo, 0);
		} else
			splx(s);
		return 0;
	}

	for (i = 10000; i-- && mbox->acc_busy; DELAY(100));

	if (mbox->acc_busy) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return 1;
	}

	AMI_DPRINTF(AMI_D_CMD, ("exec "));

	ccb->ccb_state = AMI_CCB_QUEUED;
	TAILQ_INSERT_TAIL(&sc->sc_ccbq, ccb, ccb_link);

	cmd->acc_busy = 1;
	cmd->acc_poll = 0;
	cmd->acc_ack = 0;
	*mbox = *cmd;

	if ((i = (sc->sc_exec)(sc)))
		cmd->acc_busy = 0;

	return i;
}

void
ami_requeue(v)
	void *v;
{
	struct ami_softc *sc = v;
	struct ami_ccb *ccb;
	struct ami_iocmd *cmd;
	volatile struct ami_iocmd *mbox = sc->sc_mbox;
	int s;

	if (mbox->acc_busy) {
		timeout_add(&sc->sc_requeue_tmo, 1);
		return;
	}

	s = splclock();
	ccb = TAILQ_FIRST(&sc->sc_ccb2q);
	TAILQ_REMOVE(&sc->sc_ccb2q, ccb, ccb_link);
	splx(s);

	cmd = ccb->ccb_cmd;
	AMI_DPRINTF(AMI_D_CMD, ("requeue(%d) ", cmd->acc_id));
	ccb->ccb_state = AMI_CCB_READY;

	if (!ami_start(ccb, 0))
		AMI_DPRINTF(AMI_D_CMD, ("requeue(%d) again\n", cmd->acc_id));

	if (!TAILQ_EMPTY(&sc->sc_ccb2q))
		timeout_add(&sc->sc_requeue_tmo, 1);
	else
		sc->sc_timeout = 0;
}

int
ami_complete(ccb)
	struct ami_ccb *ccb;
{
	struct ami_softc *sc = ccb->ccb_sc;
	struct ami_iocmd mbox;
	int i, j, rv, status;

	for (rv = 1, status = 0, i = 10000; !status && rv && i--; DELAY(100))
		if ((sc->sc_done)(sc, &mbox)) {
			AMI_DPRINTF(AMI_D_CMD, ("got-%d ", mbox.acc_nstat));
			status = mbox.acc_status;
			for (j = 0; j < mbox.acc_nstat; j++ ) {
				int ready = mbox.acc_cmplidl[j];

				AMI_DPRINTF(AMI_D_CMD, ("ready=%x ", ready));

				/* XXX could it happen that scsi_done allocs it? */
				if (!ami_done(sc, ready) &&
				    ccb->ccb_state == AMI_CCB_FREE)
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
	struct scsi_xfer *xs;
	struct ami_ccb *ccb = &sc->sc_ccbs[idx - 1];

	AMI_DPRINTF(AMI_D_CMD, ("done(%d) ", idx));

	if (ccb->ccb_state != AMI_CCB_QUEUED) {
		printf("%s: unqueued ccb %d ready, state = %d\n",
		    sc->sc_dev.dv_xname, idx, ccb->ccb_state);
		return 1;
	}

	TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);

	if ((xs = ccb->ccb_xs)) {
		if (xs->cmd->opcode != PREVENT_ALLOW &&
		    xs->cmd->opcode != SYNCHRONIZE_CACHE) {
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap,
			    (xs->flags & SCSI_DATA_IN) ?
			    BUS_DMASYNC_POSTREAD :
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
		}
	} else {
		struct ami_iocmd *cmd = ccb->ccb_cmd;

		switch (cmd->acc_cmd) {
		case AMI_INQUIRY:
		case AMI_EINQUIRY:
		case AMI_EINQUIRY3:
		case AMI_READ:
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
			break;
		case AMI_WRITE:
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
			break;
		default:
			/* no data */
		}
	}

	ami_put_ccb(ccb);

	if (xs) {
		xs->resid = 0;
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}

	return 0;
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
	AMI_DPRINTF(AMI_D_CMD, ("ami_scsi_raw_cmd "));

	/* XXX Not yet implemented */
	xs->error = XS_DRIVER_STUFFUP;
	return (COMPLETE);
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

	if (target >= sc->sc_nunits || !sc->sc_hdr[target].hd_present ||
	    link->lun != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}

	AMI_DPRINTF(AMI_D_CMD, ("ami_scsi_cmd "));

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
			return (TRY_AGAIN_LATER);
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
		return (COMPLETE);

	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		lock = AMI_LOCK_AMI(sc);

		flags = 0;
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
					flags = 0;
			}
			if (blockno >= sc->sc_hdr[target].hd_size ||
			    blockno + blockcnt > sc->sc_hdr[target].hd_size) {
				AMI_UNLOCK_AMI(sc, lock);
				printf("%s: out of bounds %u-%u >= %u\n",
				    sc->sc_dev.dv_xname, blockno, blockcnt,
				    sc->sc_hdr[target].hd_size);
				scsi_done(xs);
				xs->error = XS_DRIVER_STUFFUP;
				return (COMPLETE);
			}
		}

		if ((ccb = ami_get_ccb(sc)) == NULL) {
			scsi_done(xs);
			xs->error = XS_DRIVER_STUFFUP;
			return (COMPLETE);
		}

		ccb->ccb_xs = xs;
		ccb->ccb_data = xs->data;
		ccb->ccb_len  = xs->datalen;
		cmd = ccb->ccb_cmd;
		cmd->acc_mbox.amb_nsect = blockcnt;
		cmd->acc_mbox.amb_lba = blockno;
		cmd->acc_mbox.amb_ldn = target;
		cmd->acc_mbox.amb_data = 0;

		switch (xs->cmd->opcode) {
		case SYNCHRONIZE_CACHE:	
			cmd->acc_cmd = AMI_FLUSH;
			/* XXX do other fields matter ? */
			break;
		case READ_COMMAND: case READ_BIG:
			cmd->acc_cmd = AMI_READ;
			break;
		case WRITE_COMMAND: case WRITE_BIG:
			cmd->acc_cmd = AMI_WRITE;
			break;
#ifdef DIAGNOSTIC
		default:
			printf("%s: but how?\n", sc->sc_dev.dv_xname);
			xs->error = XS_DRIVER_STUFFUP;
			return (COMPLETE);
#endif
		}

		if ((error = ami_cmd(ccb, ((xs->flags & SCSI_NOSLEEP)?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK), xs->flags & SCSI_POLL))) {

			AMI_UNLOCK_AMI(sc, lock);
			AMI_DPRINTF(AMI_D_CMD, ("failed %p ", xs));
			if (xs->flags & SCSI_POLL) {
				xs->error = XS_TIMEOUT;
				return (TRY_AGAIN_LATER);
			} else {
				scsi_done(xs);
				xs->error = XS_DRIVER_STUFFUP;
				return (COMPLETE);
			}
		}

		AMI_UNLOCK_AMI(sc, lock);

		if (xs->flags & SCSI_POLL) {
			scsi_done(xs);
			return (COMPLETE);
		}
		return (SUCCESSFULLY_QUEUED);

	default:
		AMI_DPRINTF(AMI_D_CMD, ("unknown opc %d ", xs->cmd->opcode));
		xs->error = XS_DRIVER_STUFFUP;
	}

	return (COMPLETE);
}

int
ami_intr(v)
	void *v;
{
	struct ami_softc *sc = v;
	struct ami_iocmd mbox;
	int i, rv;

	while ((sc->sc_done)(sc, &mbox)) {
		AMI_DPRINTF(AMI_D_CMD, ("got-%d ", mbox.acc_nstat));
		for (i = 0; i < mbox.acc_nstat; i++ ) {
			register int ready = mbox.acc_cmplidl[i];

			AMI_DPRINTF(AMI_D_CMD, ("ready=%x ", ready));

			if (!ami_done(sc, ready))
				rv++;
		}
	}

	return rv;
}
