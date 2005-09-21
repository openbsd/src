/*	$OpenBSD: ami.c,v 1.82 2005/09/21 10:36:14 dlg Exp $	*/

/*
 * Copyright (c) 2001 Michael Shalayeff
 * Copyright (c) 2005 Marco Peereboom
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

/*#define	AMI_DEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/amireg.h>
#include <dev/ic/amivar.h>

#include <dev/biovar.h>
#include "bio.h"

#ifdef AMI_DEBUG
#define	AMI_DPRINTF(m,a)	if (ami_debug & (m)) printf a
#define	AMI_D_CMD	0x0001
#define	AMI_D_INTR	0x0002
#define	AMI_D_MISC	0x0004
#define	AMI_D_DMA	0x0008
#define	AMI_D_IOCTL	0x0010
int ami_debug = 0
	| AMI_D_CMD
	| AMI_D_INTR
	| AMI_D_MISC
/*	| AMI_D_DMA */
/*	| AMI_D_IOCTL */
	;
#else
#define	AMI_DPRINTF(m,a)	/* m, a */
#endif

struct cfdriver ami_cd = {
	NULL, "ami", DV_DULL
};

int	ami_scsi_cmd(struct scsi_xfer *xs);
int	ami_scsi_ioctl(struct scsi_link *link, u_long cmd,
    caddr_t addr, int flag, struct proc *p);
void	amiminphys(struct buf *bp);

struct scsi_adapter ami_switch = {
	ami_scsi_cmd, amiminphys, 0, 0, ami_scsi_ioctl
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

struct ami_ccb *ami_get_ccb(struct ami_softc *sc);
void ami_put_ccb(struct ami_ccb *ccb);

void ami_write_inbound_db(struct ami_softc *, u_int32_t);
void ami_write_outbound_db(struct ami_softc *, u_int32_t);
u_int32_t ami_read_inbound_db(struct ami_softc *);
u_int32_t ami_read_outbound_db(struct ami_softc *);

void ami_copyhds(struct ami_softc *sc, const u_int32_t *sizes,
	const u_int8_t *props, const u_int8_t *stats);
void *ami_allocmem(bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname);
void ami_freemem(caddr_t p, bus_dma_tag_t dmat, bus_dmamap_t *map,
	bus_dma_segment_t *segp, size_t isize, size_t nent, const char *iname);
void ami_stimeout(void *v);
int  ami_cmd(struct ami_ccb *ccb, int flags, int wait);
int  ami_start(struct ami_ccb *ccb, int wait);
int  ami_done(struct ami_softc *sc, int idx);
void ami_copy_internal_data(struct scsi_xfer *xs, void *v, size_t size);
int  ami_inquire(struct ami_softc *sc, u_int8_t op);

#if NBIO > 0
int ami_mgmt(struct ami_softc *, u_int8_t, u_int8_t, u_int8_t, u_int8_t,
    size_t, void *);
int ami_drv_inq(struct ami_softc *, u_int8_t, u_int8_t, u_int8_t, void *);
int ami_ioctl(struct device *, u_long, caddr_t);
int ami_ioctl_inq(struct ami_softc *, struct bioc_inq *);
int ami_vol(struct ami_softc *, struct bioc_vol *,
    struct ami_big_diskarray *);
int ami_disk(struct ami_softc *, struct bioc_disk *,
    struct ami_big_diskarray *);
int ami_ioctl_vol(struct ami_softc *, struct bioc_vol *);
int ami_ioctl_disk(struct ami_softc *, struct bioc_disk *);
int ami_ioctl_alarm(struct ami_softc *, struct bioc_alarm *);
int ami_ioctl_setstate(struct ami_softc *, struct bioc_setstate *);
#endif /* NBIO > 0 */

struct ami_ccb *
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

void
ami_put_ccb(ccb)
	struct ami_ccb *ccb;
{
	struct ami_softc *sc = ccb->ccb_sc;

	ccb->ccb_state = AMI_CCB_FREE;
	ccb->ccb_wakeup = 0;
	TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, ccb_link);
}

void
ami_write_inbound_db(sc, v)
	struct ami_softc *sc;
	u_int32_t v;
{
	AMI_DPRINTF(AMI_D_CMD, ("awi %x ", v));

	bus_space_write_4(sc->iot, sc->ioh, AMI_QIDB, v);
	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QIDB, 4, BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
ami_read_inbound_db(sc)
	struct ami_softc *sc;
{
	u_int32_t rv;

	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QIDB, 4, BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->iot, sc->ioh, AMI_QIDB);
	AMI_DPRINTF(AMI_D_CMD, ("ari %x ", rv));

	return (rv);
}

void
ami_write_outbound_db(sc, v)
	struct ami_softc *sc;
	u_int32_t v;
{
	AMI_DPRINTF(AMI_D_CMD, ("awo %x ", v));

	bus_space_write_4(sc->iot, sc->ioh, AMI_QODB, v);
	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QODB, 4, BUS_SPACE_BARRIER_WRITE);
}

u_int32_t
ami_read_outbound_db(sc)
	struct ami_softc *sc;
{
	u_int32_t rv;

	bus_space_barrier(sc->iot, sc->ioh,
	    AMI_QODB, 4, BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->iot, sc->ioh, AMI_QODB);
	AMI_DPRINTF(AMI_D_CMD, ("aro %x ", rv));

	return (rv);
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
	if ((error = bus_dmamap_create(dmat, total, 1,
	    total, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, map))) {
		printf(": cannot create %s dmamap (%d)\n", iname, error);
		return (NULL);
	}

	if ((error = bus_dmamem_alloc(dmat, total, PAGE_SIZE, 0, segp, 1,
	    &rseg, BUS_DMA_NOWAIT))) {
		printf(": cannot allocate %s%s (%d)\n",
		    iname, nent==1? "": "s", error);
		goto destroy;
	}

	if ((error = bus_dmamem_map(dmat, segp, rseg, total, &p,
	    BUS_DMA_NOWAIT))) {
		printf(": cannot map %s%s (%d)\n",
		    iname, nent==1? "": "s", error);
		goto free;
	}

	if ((error = bus_dmamap_load(dmat, *map, p, total, NULL,
	    BUS_DMA_NOWAIT))) {
		printf(": cannot load %s dma map (%d)\n", iname, error);
		goto unmap;
	}

	bzero(p, total);

	return (p);

unmap:
	bus_dmamem_unmap(dmat, p, total);
free:
	bus_dmamem_free(dmat, segp, 1);
destroy:
	bus_dmamap_destroy(dmat, *map);

	return (NULL);
}

void
ami_freemem(p, dmat, map, segp, isize, nent, iname)
	caddr_t p;
	bus_dma_tag_t dmat;
	bus_dmamap_t *map;
	bus_dma_segment_t *segp;
	size_t isize, nent;
	const char *iname;
{
	size_t total = isize * nent;

	bus_dmamap_unload(dmat, *map);
	bus_dmamem_unmap(dmat, p, total);
	bus_dmamem_free(dmat, segp, 1);
	bus_dmamap_destroy(dmat, *map);
	*map = NULL;
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
	}
}

int
ami_attach(sc)
	struct ami_softc *sc;
{
	struct ami_rawsoftc *rsc;
	struct ami_ccb	*ccb;
	struct ami_iocmd *cmd;
	struct ami_passthrough *pt;
	struct ami_sgent *sg;
	bus_dmamap_t idatamap;
	bus_dma_segment_t idataseg[1];
	const char *p;
	void	*idata;
	int	error;
	/* u_int32_t *pp; */

	if (!(idata = ami_allocmem(sc->dmat, &idatamap, idataseg,
	    NBPG, 1, "init data")))
		return 1;

	sc->sc_cmds = ami_allocmem(sc->dmat, &sc->sc_cmdmap, sc->sc_cmdseg,
	    sizeof(struct ami_iocmd), AMI_MAXCMDS+1, "command");
	if (!sc->sc_cmds)
		goto free_idata;

	sc->sc_pts = ami_allocmem(sc->dmat, &sc->sc_ptmap, sc->sc_ptseg,
	    sizeof(struct ami_passthrough), AMI_MAXCMDS+1, "ptlist");
	if (!sc->sc_pts)
		goto free_cmds;

	sc->sc_sgents = ami_allocmem(sc->dmat, &sc->sc_sgmap, sc->sc_sgseg,
	    sizeof(struct ami_sgent) * AMI_SGEPERCMD, AMI_MAXCMDS+1, "sglist");
	if (!sc->sc_sgents)
		goto free_pts;

	TAILQ_INIT(&sc->sc_ccbq);
	TAILQ_INIT(&sc->sc_ccbdone);
	TAILQ_INIT(&sc->sc_free_ccb);

	/* 0th command is a mailbox */
	for (ccb = &sc->sc_ccbs[AMI_MAXCMDS-1],
	     cmd = sc->sc_cmds  + sizeof(*cmd) * AMI_MAXCMDS,
	     pt = sc->sc_pts  + sizeof(*pt) * AMI_MAXCMDS,
	     sg = sc->sc_sgents + sizeof(*sg)  * AMI_MAXCMDS * AMI_SGEPERCMD;
	     cmd >= (struct ami_iocmd *)sc->sc_cmds;
	     cmd--, ccb--, pt--, sg -= AMI_SGEPERCMD) {

		cmd->acc_id = cmd - (struct ami_iocmd *)sc->sc_cmds;
		if (cmd->acc_id) {
			error = bus_dmamap_create(sc->dmat,
			    AMI_MAXFER, AMI_MAXOFFSETS, AMI_MAXFER, 0,
			    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
			    &ccb->ccb_dmamap);
			if (error) {
				printf(": cannot create ccb dmamap (%d)\n",
				    error);
				goto destroy;
			}
			ccb->ccb_sc = sc;
			ccb->ccb_cmd = cmd;
			ccb->ccb_state = AMI_CCB_FREE;
			ccb->ccb_cmdpa = htole32(sc->sc_cmdseg[0].ds_addr +
			    cmd->acc_id * sizeof(*cmd));
			ccb->ccb_pt = pt;
			ccb->ccb_ptpa = htole32(sc->sc_ptseg[0].ds_addr +
			    cmd->acc_id * sizeof(*pt));
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
		if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) == 0) {
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
			if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) == 0) {
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
			if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) != 0) {
				ccb = ami_get_ccb(sc);
				cmd = ccb->ccb_cmd;

				cmd->acc_cmd = AMI_INQUIRY;
				cmd->acc_io.aio_channel = 0;
				cmd->acc_io.aio_param = 0;
				cmd->acc_io.aio_data = htole32(pa);
				if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) != 0) {
					AMI_UNLOCK_AMI(sc, lock);
					printf(": cannot do inquiry\n");
					goto destroy;
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
#if 0
		/* FIXME need to find a way to detect if fw supports this
		 * calling it this way crashes fw when io is ran to
		 * multiple logical disks
		 */

		/* reset the IO completion values to 0
		 * the firmware either has at least pp[0] IOs outstanding
		 * -or-
		 * it times out pp[1] us before it completes any IO
		 * if the values remain unchanged it locksteps the driver
		 * to a maximum of 4 outstanding IOs and it hits the 5us timer
		 * continuously (these are the default values)
		 * this trick only works with firmwares newer than 5/13/05
		 * Setting the values outright will hang old firmwares so
		 * we need to read them first before setting them.
		 */
		ccb = ami_get_ccb(sc);
		ccb->ccb_data = NULL;
		cmd = ccb->ccb_cmd;

		cmd->acc_cmd = AMI_MISC;
		cmd->acc_io.aio_channel = AMI_GET_IO_CMPL; /* sub opcode */
		cmd->acc_io.aio_param = 0;
		cmd->acc_io.aio_data = htole32(pa);

		if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) != 0) {
			AMI_DPRINTF(AMI_D_MISC, ("getting io completion values"
			    " failed\n"));
		} else {
			ccb = ami_get_ccb(sc);
			ccb->ccb_data = NULL;
			cmd = ccb->ccb_cmd;

			cmd->acc_cmd = AMI_MISC;
			cmd->acc_io.aio_channel = AMI_SET_IO_CMPL;
			cmd->acc_io.aio_param = 0;
			cmd->acc_io.aio_data = htole32(pa);

			/* set parameters */
			pp = idata;
			pp[0] = 0; /* minimal outstanding commands, 0 disable */
			pp[1] = 0; /* maximal timeout in us, 0 disable */

			if (ami_cmd(ccb, BUS_DMA_NOWAIT, 1) != 0) {
				AMI_DPRINTF(AMI_D_MISC, ("setting io completion"
				    " values failed\n"));
			} else {
				AMI_DPRINTF(AMI_D_MISC, ("setting io completion"
				    " values succeeded\n"));
			}
		}
#endif
		if (sc->sc_flags & AMI_BROKEN) {
			sc->sc_link.openings = 1;
			sc->sc_maxcmds = 1;
			sc->sc_maxunits = 1;
		} else {
			sc->sc_maxunits = AMI_BIG_MAX_LDRIVES;
			if (sc->sc_maxcmds > AMI_MAXCMDS)
				sc->sc_maxcmds = AMI_MAXCMDS;

			if (sc->sc_nunits)
				sc->sc_link.openings =
				    sc->sc_maxcmds / sc->sc_nunits;
			else
				sc->sc_link.openings = sc->sc_maxcmds;
		}

		AMI_UNLOCK_AMI(sc, lock);
	}
	ami_freemem(idata, sc->dmat, &idatamap, idataseg, NBPG, 1, "init data");

	/* hack for hp netraid version encoding */
	if ('A' <= sc->sc_fwver[2] && sc->sc_fwver[2] <= 'Z' &&
	    sc->sc_fwver[1] < ' ' && sc->sc_fwver[0] < ' ' &&
	    'A' <= sc->sc_biosver[2] && sc->sc_biosver[2] <= 'Z' &&
	    sc->sc_biosver[1] < ' ' && sc->sc_biosver[0] < ' ') {

		snprintf(sc->sc_fwver, sizeof sc->sc_fwver, "%c.%02d.%02d",
		    sc->sc_fwver[2], sc->sc_fwver[1], sc->sc_fwver[0]);
		snprintf(sc->sc_biosver, sizeof sc->sc_biosver, "%c.%02d.%02d",
		    sc->sc_biosver[2], sc->sc_biosver[1], sc->sc_biosver[0]);
	}

	/* TODO: fetch & print cache strategy */
	/* TODO: fetch & print scsi and raid info */

	sc->sc_link.device = &ami_dev;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &ami_switch;
	sc->sc_link.adapter_target = sc->sc_maxunits;
	sc->sc_link.adapter_buswidth = sc->sc_maxunits;

#ifdef AMI_DEBUG
	printf(": FW %s, BIOS v%s, %dMB RAM\n"
	    "%s: %d channels, %d %ss, %d logical drives, "
	    "openings %d, max commands %d, quirks: %04x\n",
	    sc->sc_fwver, sc->sc_biosver, sc->sc_memory,
	    sc->sc_dev.dv_xname,
	    sc->sc_channels, sc->sc_targets, p, sc->sc_nunits,
	    sc->sc_link.openings, sc->sc_maxcmds, sc->sc_flags);
#else
	printf(": FW %s, BIOS v%s, %dMB RAM\n"
	    "%s: %d channels, %d %ss, %d logical drives\n",
	    sc->sc_fwver, sc->sc_biosver, sc->sc_memory,
	    sc->sc_dev.dv_xname,
	    sc->sc_channels, sc->sc_targets, p, sc->sc_nunits);
#endif /* AMI_DEBUG */

	if (sc->sc_flags & AMI_BROKEN && sc->sc_nunits > 1)
		printf("%s: firmware buggy, limiting access to first logical "
		    "disk\n", sc->sc_dev.dv_xname);

#if NBIO > 0
	if (bio_register(&sc->sc_dev, ami_ioctl) != 0)
		printf("%s: controller registration failed",
		    sc->sc_dev.dv_xname);
	else
		sc->sc_ioctl = ami_ioctl;
#endif /* NBIO > 0 */

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

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
		rsc->sc_proctarget = -1;
		/* TODO fetch it from the controller */
		rsc->sc_link.adapter_target = 16;
		rsc->sc_link.adapter_buswidth = 16;

		config_found(&sc->sc_dev, &rsc->sc_link, scsiprint);
	}

	return 0;

destroy:
	for (ccb = &sc->sc_ccbs[AMI_MAXCMDS - 1]; ccb > sc->sc_ccbs; ccb--)
		if (ccb->ccb_dmamap)
			bus_dmamap_destroy(sc->dmat, ccb->ccb_dmamap);

	ami_freemem(sc->sc_sgents, sc->dmat, &sc->sc_sgmap, sc->sc_sgseg,
	    sizeof(struct ami_sgent) * AMI_SGEPERCMD, AMI_MAXCMDS+1, "sglist");
free_pts:
	ami_freemem(sc->sc_pts, sc->dmat, &sc->sc_ptmap, sc->sc_ptseg,
	    sizeof(struct ami_passthrough), AMI_MAXCMDS+1, "ptlist");
free_cmds:
	ami_freemem(sc->sc_cmds, sc->dmat, &sc->sc_cmdmap, sc->sc_cmdseg,
	    sizeof(struct ami_iocmd), AMI_MAXCMDS+1, "command");
free_idata:
	ami_freemem(idata, sc->dmat, &idatamap, idataseg, NBPG, 1, "init data");

	return 1;
}

int
ami_quartz_init(sc)
	struct ami_softc *sc;
{
	ami_write_inbound_db(sc, 0);

	return 0;
}

int
ami_quartz_exec(sc, cmd)
	struct ami_softc *sc;
	struct ami_iocmd *cmd;
{
	u_int32_t qidb, i;

	i = 0;
	while (sc->sc_mbox->acc_busy && (i < AMI_MAX_BUSYWAIT)) {
		delay(1);
		i++;
	}
	if (sc->sc_mbox->acc_busy) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return (EBUSY);
	}

	memcpy((struct ami_iocmd *)sc->sc_mbox, cmd, 16);
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->sc_mbox->acc_busy = 1;
	sc->sc_mbox->acc_poll = 0;
	sc->sc_mbox->acc_ack = 0;

	qidb = sc->sc_mbox_pa | AMI_QIDB_EXEC;
	ami_write_inbound_db(sc, qidb);

	return (0);
}

int
ami_quartz_done(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int32_t qdb, i, n;
	u_int8_t nstat, status;
	u_int8_t completed[AMI_MAXSTATACK];

	qdb = ami_read_outbound_db(sc);
	if (qdb != AMI_QODB_READY)
		return (0); /* nothing to do */

	ami_write_outbound_db(sc, AMI_QODB_READY);

	/*
	 * The following sequence is not supposed to have a timeout clause
	 * since the firmware has a "guarantee" that all commands will
	 * complete.  The choice is either panic or hoping for a miracle
	 * and that the IOs will complete much later.
	 */
	i = 0;
	while ((nstat = sc->sc_mbox->acc_nstat) == 0xff) {
		bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
		    BUS_DMASYNC_POSTREAD);
		delay(1);
		if (i++ > 1000000)
			return (0); /* nothing to do */
	}
	sc->sc_mbox->acc_nstat = 0xff;
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
	    BUS_DMASYNC_POSTWRITE);

	/* wait until fw wrote out all completions */
	i = 0;
	AMI_DPRINTF(AMI_D_CMD, ("aqd %d ", nstat));
	for (n = 0; n < nstat; n++) {
		bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
		    BUS_DMASYNC_PREREAD);
		while ((completed[n] = sc->sc_mbox->acc_cmplidl[n]) ==
		    0xff) {
			delay(1);
			if (i++ > 1000000)
				return (0); /* nothing to do */
		}
		sc->sc_mbox->acc_cmplidl[n] = 0xff;
		bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
		    BUS_DMASYNC_POSTWRITE);
	}

	/* this should never happen, someone screwed up the completion status */
	if ((status = sc->sc_mbox->acc_status) == 0xff)
		panic("%s: status 0xff from the firmware", sc->sc_dev.dv_xname);

	sc->sc_mbox->acc_status = 0xff;

	/* copy mailbox to temporary one and fixup other changed values */
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 16,
	    BUS_DMASYNC_POSTWRITE);
	memcpy(mbox, (struct ami_iocmd *)sc->sc_mbox, 16);
	mbox->acc_nstat = nstat;
	mbox->acc_status = status;
	for (n = 0; n < nstat; n++)
		mbox->acc_cmplidl[n] = completed[n];

	/* ack interrupt */
	ami_write_inbound_db(sc, AMI_QIDB_ACK);

	return (1);	/* ready to complete all IOs in acc_cmplidl */
}

int
ami_quartz_poll(sc, cmd)
	struct ami_softc *sc;
	struct ami_iocmd *cmd;
{
	/* struct scsi_xfer *xs = ccb->ccb_xs; */
	u_int32_t qidb, i;
	u_int8_t status, ready;

	if (sc->sc_dis_poll)
		return 1; /* fail */

	i = 0;
	while (sc->sc_mbox->acc_busy && (i < AMI_MAX_BUSYWAIT)) {
		delay(1);
		i++;
	}
	if (sc->sc_mbox->acc_busy) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return (EBUSY);
	}

	memcpy((struct ami_iocmd *)sc->sc_mbox, cmd, 16);
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 16,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->sc_mbox->acc_id = 0xfe;
	sc->sc_mbox->acc_busy = 1;
	sc->sc_mbox->acc_poll = 0;
	sc->sc_mbox->acc_ack = 0;

	sc->sc_mbox->acc_nstat = 0xff;
	sc->sc_mbox->acc_status = 0xff;

	/* send command to firmware */
	qidb = sc->sc_mbox_pa | AMI_QIDB_EXEC;
	ami_write_inbound_db(sc, qidb);

	while ((sc->sc_mbox->acc_nstat == 0xff) && (i < AMI_MAX_POLLWAIT)) {
		delay(1);
		i++;
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: command not accepted, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1;
	}

	sc->sc_mbox->acc_nstat = 0xff;

	while ((sc->sc_mbox->acc_status == 0xff) && (i < AMI_MAX_POLLWAIT)) {
		delay(1);
		i++;
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: bad status, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1;
	}
	status = sc->sc_mbox->acc_status;
	sc->sc_mbox->acc_status = 0xff;

	/* poll firmware */
	while ((sc->sc_mbox->acc_poll != 0x77) && (i < AMI_MAX_POLLWAIT)) {
		delay(1);
		i++;
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: firmware didn't reply, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1;
	}

	sc->sc_mbox->acc_poll = 0;
	sc->sc_mbox->acc_ack = 0x77;

	/* ack */
	qidb = sc->sc_mbox_pa | AMI_QIDB_ACK;
	ami_write_inbound_db(sc, qidb);

	while((ami_read_inbound_db(sc) & AMI_QIDB_ACK) &&
	    (i < AMI_MAX_POLLWAIT)) {
		delay(1);
		i++;
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: firmware didn't ack the ack, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1;
	}

	ready = sc->sc_mbox->acc_cmplidl[0];

	for (i = 0; i < AMI_MAXSTATACK; i++)
		sc->sc_mbox->acc_cmplidl[i] = 0xff;
#if 0
	/* FIXME */
	/* am I a scsi command? if so complete it */
	if (xs) {
		printf("sc ");
		if (!ami_done(sc, ready))
			status = 0;
		else
			status = 1; /* failed */
	} else /* need to clean up ccb ourselves */
		ami_put_ccb(ccb);
#endif


	return status;
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
	if (bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return EBUSY;
	}

	memcpy((struct ami_iocmd *)sc->sc_mbox, cmd, 16);
	sc->sc_mbox->acc_busy = 1;
	sc->sc_mbox->acc_poll = 0;
	sc->sc_mbox->acc_ack = 0;

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
		AMI_DPRINTF(AMI_D_CMD, ("asd %d ", mbox->acc_nstat));

		bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_ACK);

		return 1;
	}

	return 0;
}

int
ami_schwartz_poll(sc, mbox)
	struct ami_softc *sc;
	struct ami_iocmd *mbox;
{
	u_int8_t status;
	u_int32_t i;
	int rv;

	if (sc->sc_dis_poll) {
		return 1; /* fail */
	}

	for (i = 0; i < AMI_MAX_POLLWAIT; i++) {
		if (!(bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY))
			break;
		delay(1);
	}
	if (i >= AMI_MAX_POLLWAIT) {
		AMI_DPRINTF(AMI_D_CMD, ("mbox_busy "));
		return (EBUSY);
	}

	memcpy((struct ami_iocmd *)sc->sc_mbox, mbox, 16);
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 16,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->sc_mbox->acc_busy = 1;
	sc->sc_mbox->acc_poll = 0;
	sc->sc_mbox->acc_ack = 0;
	/* send command to firmware */
	bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_EXEC);

	/* wait until no longer busy */
	for (i = 0; i < AMI_MAX_POLLWAIT; i++) {
		if (!(bus_space_read_1(sc->iot, sc->ioh, AMI_SMBSTAT) & AMI_SMBST_BUSY))
			break;
		delay(1);
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: command not accepted, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1; /* fail */
	}

	/* wait for interrupt bit */
	for (i = 0; i < AMI_MAX_POLLWAIT; i++) {
		status = bus_space_read_1(sc->iot, sc->ioh, AMI_ISTAT);
		if (status & AMI_ISTAT_PEND)
			break;
		delay(1);
	}
	if (i >= AMI_MAX_POLLWAIT) {
		printf("%s: interrupt didn't arrive, polling disabled\n",
		    sc->sc_dev.dv_xname);
		sc->sc_dis_poll = 1;
		return 1; /* fail */
	}

	/* write ststus back to firmware */
	bus_space_write_1(sc->iot, sc->ioh, AMI_ISTAT, status);

	/* copy mailbox and status back */
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, 128,
	    BUS_DMASYNC_PREREAD);
	*mbox = *sc->sc_mbox;
	rv = sc->sc_mbox->acc_status;

	/* ack interrupt */
	bus_space_write_1(sc->iot, sc->ioh, AMI_SCMD, AMI_SCMD_ACK);

	return rv;
}

int
ami_cmd(ccb, flags, wait)
	struct ami_ccb *ccb;
	int flags, wait;
{
	struct ami_softc *sc = ccb->ccb_sc;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error = 0, i;

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

		if (cmd->acc_cmd == AMI_PASSTHRU)
			cmd->acc_passthru.apt_data = ccb->ccb_ptpa;

		sgd = dmap->dm_segs;
		AMI_DPRINTF(AMI_D_DMA, ("data=%p/%u<0x%lx/%u",
		    ccb->ccb_data, ccb->ccb_len,
		    sgd->ds_addr, sgd->ds_len));

		if(dmap->dm_nsegs > 1) {
			struct ami_sgent *sgl = ccb->ccb_sglist;

			if (cmd->acc_cmd == AMI_PASSTHRU) {
				ccb->ccb_pt->apt_nsge = dmap->dm_nsegs;
				ccb->ccb_pt->apt_data = ccb->ccb_sglistpa;
			} else {
				cmd->acc_mbox.amb_nsge = dmap->dm_nsegs;
				cmd->acc_mbox.amb_data = ccb->ccb_sglistpa;
			}

			for (i = 0; i < dmap->dm_nsegs; i++, sgd++) {
				sgl[i].asg_addr = htole32(sgd->ds_addr);
				sgl[i].asg_len  = htole32(sgd->ds_len);
				if (i)
					AMI_DPRINTF(AMI_D_DMA, (",0x%lx/%u",
					    sgd->ds_addr, sgd->ds_len));
			}
		} else {
			if (cmd->acc_cmd == AMI_PASSTHRU) {
				ccb->ccb_pt->apt_nsge = 0;
				ccb->ccb_pt->apt_data = htole32(sgd->ds_addr);
			} else {
				cmd->acc_mbox.amb_nsge = 0;
				cmd->acc_mbox.amb_data = htole32(sgd->ds_addr);
			}
		}
		AMI_DPRINTF(AMI_D_DMA, ("> "));

		bus_dmamap_sync(sc->dmat, dmap, 0, dmap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);
	} else
		ccb->ccb_cmd->acc_mbox.amb_nsge = 0;
	bus_dmamap_sync(sc->dmat, sc->sc_cmdmap, 0, sc->sc_cmdmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if (wait) {
		AMI_DPRINTF(AMI_D_DMA, ("waiting "));
		/* FIXME remove all wait out ami_start */
		if ((error = sc->sc_poll(sc, ccb->ccb_cmd))) {
			AMI_DPRINTF(AMI_D_MISC, ("pf "));
		}
		if (ccb->ccb_data)
			bus_dmamap_unload(sc->dmat, dmap);
		if (ccb->ccb_wakeup)
			ccb->ccb_wakeup = 0;
		else
			ami_put_ccb(ccb);
	} else if ((error = ami_start(ccb, wait))) {
		AMI_DPRINTF(AMI_D_DMA, ("error=%d ", error));
		__asm __volatile(".globl _bpamierr\n_bpamierr:");
		if (ccb->ccb_data)
			bus_dmamap_unload(sc->dmat, dmap);
		ami_put_ccb(ccb);
	}

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
				/* add 5sec for whacky done() loops */
				tv.tv_sec = 5 + xs->timeout / 1000;
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

/* FIXME timeouts should be rethought */
void
ami_stimeout(v)
	void *v;
{
	struct ami_ccb *ccb = v;
	struct ami_softc *sc = ccb->ccb_sc;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct ami_iocmd *cmd = ccb->ccb_cmd;
	volatile struct ami_iocmd *mbox = sc->sc_mbox;
	ami_lock_t lock;

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

		/* XXX ccb might complete and overwrite this mem */
		if (ccb->ccb_data != NULL) {
			bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmamap->dm_mapsize,
			    (ccb->ccb_dir == AMI_CCB_IN) ?
			    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);
			ccb->ccb_data = NULL;
		}
		TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);
		ami_put_ccb(ccb);
		xs->error = XS_TIMEOUT;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		break;
	case AMI_CCB_FREE:
	case AMI_CCB_READY:
		panic("ami_stimeout(%d) botch", cmd->acc_id);
	}
	AMI_UNLOCK_AMI(sc, lock);
}

int
ami_done(sc, idx)
	struct ami_softc *sc;
	int	idx;
{
	struct ami_ccb *ccb = &sc->sc_ccbs[idx - 1];
	struct scsi_xfer *xs = ccb->ccb_xs;
	ami_lock_t lock;

	AMI_DPRINTF(AMI_D_CMD, ("done(%d) ", ccb->ccb_cmd->acc_id));

	if (ccb->ccb_state != AMI_CCB_QUEUED) {
		printf("%s: unqueued ccb %d ready, state = %d\n",
		    sc->sc_dev.dv_xname, idx, ccb->ccb_state);
		return (1);
	}

	lock = AMI_LOCK_AMI(sc);
	ccb->ccb_state = AMI_CCB_READY;
	TAILQ_REMOVE(&sc->sc_ccbq, ccb, ccb_link);

	/* dma sync cmd/pt/sglist */

	if (ccb->ccb_data != NULL) {
		bus_dmamap_sync(sc->dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_dir == AMI_CCB_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->dmat, ccb->ccb_dmamap);

		ccb->ccb_data = NULL;
	}

	if (xs) {
		timeout_del(&xs->stimeout);

		xs->resid = 0;
		xs->flags |= ITSDONE;
		AMI_DPRINTF(AMI_D_CMD, ("scsi_done(%d) ", idx));
		scsi_done(xs);

		ccb->ccb_xs = NULL;
	}

	if (ccb->ccb_wakeup) {
		ccb->ccb_wakeup = 0;
		wakeup(ccb);
	} else
		ami_put_ccb(ccb);

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
	struct device *dev = link->device_softc;
	struct ami_ccb *ccb;
	int error;
	ami_lock_t lock;
	char type;

	AMI_DPRINTF(AMI_D_CMD, ("ami_scsi_raw_cmd "));

	lock = AMI_LOCK_AMI(sc);

	if (!cold && target == rsc->sc_proctarget)
		strlcpy(rsc->sc_procdev, dev->dv_xname,
		    sizeof(rsc->sc_procdev));

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

	ccb->ccb_xs = xs;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;
	ccb->ccb_dir = (xs->flags & SCSI_DATA_IN) ? AMI_CCB_IN : AMI_CCB_OUT;

	ccb->ccb_cmd->acc_cmd = AMI_PASSTHRU;
	
	memset(ccb->ccb_pt, 0, sizeof(struct ami_passthrough));
	ccb->ccb_pt->apt_param = AMI_PTPARAM(AMI_TIMEOUT_6,1,0);
	ccb->ccb_pt->apt_channel = channel;
	ccb->ccb_pt->apt_target = target;
	bcopy(xs->cmd, ccb->ccb_pt->apt_cdb, AMI_MAX_CDB);
	ccb->ccb_pt->apt_ncdb = xs->cmdlen;
	ccb->ccb_pt->apt_nsense = AMI_MAX_SENSE;
	ccb->ccb_pt->apt_datalen = xs->datalen;

	if ((error = ami_cmd(ccb, ((xs->flags & SCSI_NOSLEEP) ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK), xs->flags & SCSI_POLL))) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		AMI_UNLOCK_AMI(sc, lock);
		return (COMPLETE);
	}

	if (xs->flags & SCSI_POLL) {
		if (xs->cmd->opcode == INQUIRY) {
			type = ((struct scsi_inquiry_data *)xs->data)->device
			    & SID_TYPE;

			if (!(type == T_PROCESSOR || type == T_ENCLOSURE))
				xs->error = XS_DRIVER_STUFFUP;
			else
				rsc->sc_proctarget = target; /* save target */
		}

		scsi_done(xs);
	}

	AMI_UNLOCK_AMI(sc, lock);
	if (xs->flags & SCSI_POLL)
		return (COMPLETE);
	else
		return (SUCCESSFULLY_QUEUED);
}

int
ami_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *link = xs->sc_link;
	struct ami_softc *sc = link->adapter_softc;
	struct device *dev = link->device_softc;
	struct ami_ccb *ccb;
	struct ami_iocmd *cmd;
	struct scsi_inquiry_data inq;
	struct scsi_sense_data sd;
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
		AMI_DPRINTF(AMI_D_CMD, ("no target %d ", target));
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
		/* save off sd? after autoconf */
		if (!cold)	/* XXX bogus */
			strlcpy(sc->sc_hdr[target].dev, dev->dv_xname,
			    sizeof(sc->sc_hdr[target].dev));

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
		strlcpy(inq.vendor, "AMI    ", sizeof inq.vendor);
		snprintf(inq.product, sizeof inq.product, "Host drive  #%02d",
		    target);
		strlcpy(inq.revision, "   ", sizeof inq.revision);
		ami_copy_internal_data(xs, &inq, sizeof inq);
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
		ccb->ccb_data = xs->data;
		ccb->ccb_len = xs->datalen;
		ccb->ccb_dir = (xs->flags & SCSI_DATA_IN) ?
		    AMI_CCB_IN : AMI_CCB_OUT;

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
		AMI_DPRINTF(AMI_D_CMD, ("unsupported scsi command %#x tgt %d ",
		    xs->cmd->opcode, target));
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
	int i, rv = 0;
	ami_lock_t lock;

	if (TAILQ_EMPTY(&sc->sc_ccbq))
		return (0);

	AMI_DPRINTF(AMI_D_INTR, ("intr "));

	lock = AMI_LOCK_AMI(sc);
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

	AMI_UNLOCK_AMI(sc, lock);
	AMI_DPRINTF(AMI_D_INTR, ("exit "));
	return (rv);
}

int
ami_scsi_ioctl(struct scsi_link *link, u_long cmd,
    caddr_t addr, int flag, struct proc *p)
{
	struct ami_softc *sc = (struct ami_softc *)link->adapter_softc;
	/* struct device *dev = (struct device *)link->device_softc; */
	/* u_int8_t target = link->target; */

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
}

#if NBIO > 0
int
ami_ioctl(dev, cmd, addr)
	struct device *dev;
	u_long cmd;
	caddr_t addr;
{
	struct ami_softc *sc = (struct ami_softc *)dev;
	ami_lock_t lock;
	int error = 0;

	AMI_DPRINTF(AMI_D_IOCTL, ("%s: ioctl ", sc->sc_dev.dv_xname));

	if (sc->sc_flags & AMI_BROKEN)
		return ENODEV; /* can't do this to broken device for now */

	lock = AMI_LOCK_AMI(sc);
	if (sc->sc_flags & AMI_CMDWAIT) {
		AMI_UNLOCK_AMI(sc, lock);
		return EBUSY;
	}

	sc->sc_flags |= AMI_CMDWAIT;

	switch (cmd) {
	case BIOCINQ:
		AMI_DPRINTF(AMI_D_IOCTL, ("inq "));
		error = ami_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		AMI_DPRINTF(AMI_D_IOCTL, ("vol "));
		error = ami_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		AMI_DPRINTF(AMI_D_IOCTL, ("disk "));
		error = ami_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		AMI_DPRINTF(AMI_D_IOCTL, ("alarm "));
		error = ami_ioctl_alarm(sc, (struct bioc_alarm *)addr);
		break;

	case BIOCSETSTATE:
		AMI_DPRINTF(AMI_D_IOCTL, ("setstate "));
		error = ami_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	default:
		AMI_DPRINTF(AMI_D_IOCTL, ("%s: invalid ioctl\n",
		    sc->sc_dev.dv_xname));
		error = EINVAL;
	}

	sc->sc_flags &= ~AMI_CMDWAIT;

	AMI_UNLOCK_AMI(sc, lock);

	return (error);
}

int
ami_drv_inq(sc, ch, tg, page, inqbuf)
	struct ami_softc *sc;
	u_int8_t ch;
	u_int8_t tg;
	u_int8_t page;
	void *inqbuf;
{
	struct ami_ccb *ccb;
	struct ami_iocmd *cmd;
	struct ami_passthrough *ps;
	struct scsi_inquiry_data *pp;
	void *idata;
	bus_dmamap_t idatamap;
	bus_dma_segment_t idataseg[1];
	paddr_t	pa;
	int error = 0;

	ccb = ami_get_ccb(sc);
	if (ccb == NULL)
		return (ENOMEM);

	if (!(idata = ami_allocmem(sc->dmat, &idatamap, idataseg, NBPG, 1,
	    "ami mgmt"))) {
		ami_put_ccb(ccb);
	    	return (ENOMEM);
	}

	pa = idataseg[0].ds_addr;
	ps = idata;
	pp = idata + sizeof *ps;

	ccb->ccb_data = NULL;
	ccb->ccb_wakeup = 1;
	cmd = ccb->ccb_cmd;

	cmd->acc_cmd = AMI_PASSTHRU;
	cmd->acc_passthru.apt_data = htole32(pa);

	memset(ps, 0, sizeof *ps);

	ps->apt_channel = ch;
	ps->apt_target = tg;
	ps->apt_ncdb = sizeof(struct scsi_inquiry);
	ps->apt_nsense = sizeof(struct scsi_sense_data);

	ps->apt_cdb[0] = INQUIRY;
	ps->apt_cdb[1] = 0;
	ps->apt_cdb[2] = 0;
	ps->apt_cdb[3] = 0;
	ps->apt_cdb[4] = sizeof(struct scsi_inquiry_data); /* INQUIRY length */
	ps->apt_cdb[5] = 0;

	if (page != 0) {
		ps->apt_cdb[1] = SI_EVPD;
		ps->apt_cdb[2] = page;
	}

	ps->apt_data = htole32(pa + sizeof *ps);
	ps->apt_datalen = sizeof(struct scsi_inquiry_data);

	if (ami_cmd(ccb, BUS_DMA_WAITOK, 0) == 0) {
		while (ccb->ccb_wakeup)
			tsleep(ccb, PRIBIO, "ami_drv_inq", 0);

		ami_put_ccb(ccb);

		if (ps->apt_scsistat == 0x00) {
			memcpy(inqbuf, pp, sizeof(struct scsi_inquiry_data));

			if (pp->device != T_DIRECT)
				error = EINVAL;

			goto bail;
		}

		error = EINVAL;
	}
	else 
		error = EINVAL;

bail:
	ami_freemem(idata, sc->dmat, &idatamap, idataseg, NBPG, 1, "ami mgmt");

	return (error);
}

int
ami_mgmt(sc, opcode, par1, par2, par3, size, buffer)
	struct ami_softc *sc;
	u_int8_t opcode;
	u_int8_t par1;
	u_int8_t par2;
	u_int8_t par3;
	size_t size;
	void *buffer;
{
	struct ami_ccb *ccb;
	struct ami_iocmd *cmd;
	void *idata;
	bus_dmamap_t idatamap;
	bus_dma_segment_t idataseg[1];
	paddr_t	pa;
	int error = 0;

	ccb = ami_get_ccb(sc);
	if (ccb == NULL)
		return (ENOMEM);

	if (!(idata = ami_allocmem(sc->dmat, &idatamap, idataseg, NBPG,
	    (size / NBPG) + 1, "ami mgmt"))) {
		ami_put_ccb(ccb);
		return (ENOMEM);
	}

	pa = idataseg[0].ds_addr;

	ccb->ccb_data = NULL;
	ccb->ccb_wakeup = 1;
	cmd = ccb->ccb_cmd;

	cmd->acc_cmd = opcode;

	/*
	 * some commands require data to be written to idata before sending
	 * command to fw
	 */
	switch (opcode) {
	case AMI_SPEAKER:
		*((char *)idata) = par1;
		break;
	default:
		cmd->acc_io.aio_channel = par1;
		cmd->acc_io.aio_param = par2;
		cmd->acc_io.aio_pad[0] = par3;
	};

	cmd->acc_io.aio_data = htole32(pa);

	if (ami_cmd(ccb, BUS_DMA_WAITOK, 0) == 0) {
		while (ccb->ccb_wakeup)
			tsleep(ccb, PRIBIO,"ami_mgmt", 0);

		ami_put_ccb(ccb);

		/* XXX how do commands fail? */
		
		if (buffer)
			memcpy(buffer, idata, size);
	}
	else
		error = EINVAL;

	ami_freemem(idata, sc->dmat, &idatamap, idataseg, NBPG,
	    (size / NBPG) + 1, "ami mgmt");

	return (error);
}

int
ami_ioctl_inq(sc, bi)
	struct ami_softc *sc;
	struct bioc_inq *bi;
{
	struct ami_big_diskarray *p; /* struct too large for stack */
	char *plist;
	int i, s, t;
	int off;
	int error = 0;
	struct scsi_inquiry_data inqbuf;
	u_int8_t ch, tg;

	p = malloc(sizeof *p, M_DEVBUF, M_NOWAIT);
	if (!p) {
		printf("%s: no memory for disk array\n",sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	plist = malloc(AMI_BIG_MAX_PDRIVES, M_DEVBUF, M_NOWAIT);
	if (!plist) {
		printf("%s: no memory for disk list\n",sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto bail;
	}

	if (ami_mgmt(sc, AMI_FCOP, AMI_FC_RDCONF, 0, 0, sizeof *p, p)) {
		error = EINVAL;
		goto bail2;
	}

	memset(plist, 0, AMI_BIG_MAX_PDRIVES);

	bi->bi_novol = p->ada_nld;
	bi->bi_nodisk = 0;

	strlcpy(bi->bi_dev, sc->sc_dev.dv_xname, sizeof(bi->bi_dev));

	/* do we actually care how many disks we have at this point? */
	for (i = 0; i < p->ada_nld; i++)
		for (s = 0; s < p->ald[i].adl_spandepth; s++)
			for (t = 0; t < p->ald[i].adl_nstripes; t++) {
				off = p->ald[i].asp[s].adv[t].add_channel *
				    AMI_MAX_TARGET +
				    p->ald[i].asp[s].adv[t].add_target;

				if (!plist[off]) {
					plist[off] = 1;
					bi->bi_nodisk++;
				}
			}

	/*
	 * hack warning!
	 * Megaraid cards sometimes return a size in the PD structure
	 * even though there is no disk in that slot.  Work around
	 * that by issuing an INQUIRY to determine if there is
	 * an actual disk in the slot.
	 */
	for(i = 0; i < ((sc->sc_flags & AMI_QUARTZ) ?
	    AMI_BIG_MAX_PDRIVES : AMI_MAX_PDRIVES); i++) {
	    	/* skip claimed drives */
	    	if (plist[i])
			continue;

	    	/*
		 * poke drive to make sure its there.  If it is it is either
		 * unused or a hot spare; at this point we dont care which it is
		 */
		if (p->apd[i].adp_size) {
			ch = (i & 0xf0) >> 4;
			tg = i & 0x0f;

			if (!ami_drv_inq(sc, ch, tg, 0, &inqbuf)) {
				bi->bi_novol++;
				bi->bi_nodisk++;
				plist[i] = 1;
			}
		}
	}

bail2:
	free(plist, M_DEVBUF);
bail:
	free(p, M_DEVBUF);

	return (error);
}

int
ami_vol(sc, bv, p)
	struct ami_softc *sc;
	struct bioc_vol *bv;
	struct ami_big_diskarray *p;
{
	struct scsi_inquiry_data inqbuf;
	char *plist;
	int i, s, t, off;
	int ld = p->ada_nld, error = EINVAL;
	u_int8_t ch, tg;

	plist = malloc(AMI_BIG_MAX_PDRIVES, M_DEVBUF, M_NOWAIT);
	if (!plist) {
		printf("%s: no memory for disk list\n",sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	memset(plist, 0, AMI_BIG_MAX_PDRIVES);

	/* setup plist */
	for (i = 0; i < p->ada_nld; i++)
		for (s = 0; s < p->ald[i].adl_spandepth; s++)
			for (t = 0; t < p->ald[i].adl_nstripes; t++) {
				off = p->ald[i].asp[s].adv[t].add_channel *
				    AMI_MAX_TARGET +
				    p->ald[i].asp[s].adv[t].add_target;

				if (!plist[off])
					plist[off] = 1;
			}

	for(i = 0; i < ((sc->sc_flags & AMI_QUARTZ) ?
	    AMI_BIG_MAX_PDRIVES : AMI_MAX_PDRIVES); i++) {
	    	/* skip claimed drives */
	    	if (plist[i])
			continue;

	    	/*
		 * poke drive to make sure its there.  If it is it is either
		 * unused or a hot spare; at this point we dont care which it is
		 */
		if (p->apd[i].adp_size) {
			ch = (i & 0xf0) >> 4;
			tg = i & 0x0f;

			if (!ami_drv_inq(sc, ch, tg, 0, &inqbuf)) {
				if (ld != bv->bv_volid) {
					ld++;
					continue;
				}

				bv->bv_status = BIOC_SVONLINE;
				bv->bv_size = (u_quad_t)p->apd[i].adp_size *
				    (u_quad_t)512;
				bv->bv_nodisk = 1;
				strlcpy(bv->bv_dev,
				    sc->sc_hdr[bv->bv_volid].dev,
				    sizeof(bv->bv_dev));

				if (p->apd[i].adp_ostatus == AMI_PD_HOTSPARE
				    && p->apd[i].adp_type == 0)
					bv->bv_level = -1;
				else
					bv->bv_level = -2;

				error = 0;
				goto bail;
			}
		}
	}

bail:
	free(plist, M_DEVBUF);

	return (error);
}

int
ami_disk(sc, bd, p)
	struct ami_softc *sc;
	struct bioc_disk *bd;
	struct ami_big_diskarray *p;
{
	struct scsi_inquiry_data inqbuf;
	struct scsi_inquiry_vpd vpdbuf;
	char *plist;
	int i, s, t, off;
	int ld = p->ada_nld, error = EINVAL;
	u_int8_t ch, tg;

	plist = malloc(AMI_BIG_MAX_PDRIVES, M_DEVBUF, M_NOWAIT);
	if (!plist) {
		printf("%s: no memory for disk list\n",sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	memset(plist, 0, AMI_BIG_MAX_PDRIVES);

	/* setup plist */
	for (i = 0; i < p->ada_nld; i++)
		for (s = 0; s < p->ald[i].adl_spandepth; s++)
			for (t = 0; t < p->ald[i].adl_nstripes; t++) {
				off = p->ald[i].asp[s].adv[t].add_channel *
				    AMI_MAX_TARGET +
				    p->ald[i].asp[s].adv[t].add_target;

				if (!plist[off])
					plist[off] = 1;
			}

	for(i = 0; i < ((sc->sc_flags & AMI_QUARTZ) ?
	    AMI_BIG_MAX_PDRIVES : AMI_MAX_PDRIVES); i++) {
	    	/* skip claimed drives */
	    	if (plist[i])
			continue;

	    	/*
		 * poke drive to make sure its there.  If it is it is either
		 * unused or a hot spare; at this point we dont care which it is
		 */
		if (p->apd[i].adp_size) {
			ch = (i & 0xf0) >> 4;
			tg = i & 0x0f;

			if (!ami_drv_inq(sc, ch, tg, 0, &inqbuf)) {
				char vend[8+16+4+1];

				if (ld != bd->bd_volid) {
					ld++;
					continue;
				}

				bcopy(inqbuf.vendor, vend,
				    sizeof vend - 1);

				vend[sizeof vend - 1] = '\0';
				strlcpy(bd->bd_vendor, vend,
				    sizeof(bd->bd_vendor));

				if (!ami_drv_inq(sc, ch, tg, 0x80, &vpdbuf)) {
					char ser[32 + 1];

					bcopy(vpdbuf.serial, ser,
					    sizeof ser - 1);

					ser[sizeof ser - 1] = '\0';
					if (vpdbuf.page_length < sizeof ser)
						ser[vpdbuf.page_length] = '\0';
					strlcpy(bd->bd_serial, ser,
					    sizeof(bd->bd_serial));
				}

				bd->bd_size = (u_quad_t)p->apd[i].adp_size *
				    (u_quad_t)512;

				bd->bd_channel = ch;
				bd->bd_target = tg;

				strlcpy(bd->bd_procdev,
				    sc->sc_rawsoftcs[ch].sc_procdev,
				    sizeof(bd->bd_procdev));

				if (p->apd[i].adp_ostatus == AMI_PD_HOTSPARE
				    && p->apd[i].adp_type == 0)
					bd->bd_status = BIOC_SDHOTSPARE;
				else
					bd->bd_status = BIOC_SDUNUSED;

				error = 0;
				goto bail;
			}
		}
	}

bail:
	free(plist, M_DEVBUF);

	return (error);
}

int
ami_ioctl_vol(sc, bv)
	struct ami_softc *sc;
	struct bioc_vol *bv;
{
	struct ami_big_diskarray *p; /* struct too large for stack */
	int i, s, t;
	int error = 0;

	p = malloc(sizeof *p, M_DEVBUF, M_NOWAIT);
	if (!p) {
		printf("%s: no memory for raw interface\n",sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	if (ami_mgmt(sc, AMI_FCOP, AMI_FC_RDCONF, 0, 0, sizeof *p, p)) {
		error = EINVAL;
		goto bail;
	}

	if (bv->bv_volid >= p->ada_nld) {
		error = ami_vol(sc, bv, p);
		goto bail;
	}

	i = bv->bv_volid;

	switch (p->ald[i].adl_status) {
	case AMI_RDRV_OFFLINE:
		bv->bv_status = BIOC_SVOFFLINE;
		break;

	case AMI_RDRV_DEGRADED:
		bv->bv_status = BIOC_SVDEGRADED;
		break;

	case AMI_RDRV_OPTIMAL:
		bv->bv_status = BIOC_SVONLINE;
		break;

	default:
		bv->bv_status = BIOC_SVINVALID;
	}

	bv->bv_size = 0;
	bv->bv_level = p->ald[i].adl_raidlvl;
	bv->bv_nodisk = 0;

	for (s = 0; s < p->ald[i].adl_spandepth; s++) {
		for (t = 0; t < p->ald[i].adl_nstripes; t++)
			bv->bv_nodisk++;

		switch (bv->bv_level) {
		case 0:
			bv->bv_size += p->ald[i].asp[s].ads_length *
			    p->ald[i].adl_nstripes;
			break;

		case 1:
			bv->bv_size += p->ald[i].asp[s].ads_length;
			break;

		case 5:
			bv->bv_size += p->ald[i].asp[s].ads_length *
			    (p->ald[i].adl_nstripes - 1);
			break;
		}
	}

	if (p->ald[i].adl_spandepth > 1)
		bv->bv_level *= 10;

	bv->bv_size *= (u_quad_t)512;

	strlcpy(bv->bv_dev, sc->sc_hdr[i].dev, sizeof(bv->bv_dev));
	
bail:
	free(p, M_DEVBUF);

	return (error);
}

int
ami_ioctl_disk(sc, bd)
	struct ami_softc *sc;
	struct bioc_disk *bd;
{
	struct scsi_inquiry_data inqbuf;
	struct scsi_inquiry_vpd vpdbuf;
	struct ami_big_diskarray *p; /* struct too large for stack */
	int i, s, t, d;
	int off;
	int error = 0;
	u_int16_t ch, tg;

	p = malloc(sizeof *p, M_DEVBUF, M_NOWAIT);
	if (!p) {
		printf("%s: no memory for raw interface\n",sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	if (ami_mgmt(sc, AMI_FCOP, AMI_FC_RDCONF, 0, 0, sizeof *p, p)) {
		error = EINVAL;
		goto bail;
	}

	if (bd->bd_volid >= p->ada_nld) {
		error = ami_disk(sc, bd, p);
		goto bail;
	}

	i = bd->bd_volid;
	error = EINVAL;

	for (s = 0, d = 0; s < p->ald[i].adl_spandepth; s++)
		for (t = 0; t < p->ald[i].adl_nstripes; t++) {
			if (d != bd->bd_diskid) {
				d++;
				continue;
			}

			off = p->ald[i].asp[s].adv[t].add_channel *
			    AMI_MAX_TARGET +
			    p->ald[i].asp[s].adv[t].add_target;

			switch (p->apd[off].adp_ostatus) {
			case AMI_PD_UNCNF:
				bd->bd_status = BIOC_SDUNUSED;
				break;

			case AMI_PD_ONLINE:
				bd->bd_status = BIOC_SDONLINE;
				break;

			case AMI_PD_FAILED:
				bd->bd_status = BIOC_SDFAILED;
				break;

			case AMI_PD_RBLD:
				bd->bd_status = BIOC_SDREBUILD;
				break;

			case AMI_PD_HOTSPARE:
				bd->bd_status = BIOC_SDHOTSPARE;
				break;

			default:
				bd->bd_status = BIOC_SDINVALID;
			}

			bd->bd_size = (u_quad_t)p->apd[off].adp_size *
			    (u_quad_t)512;

			ch = p->ald[i].asp[s].adv[t].add_target >> 4;
			tg = p->ald[i].asp[s].adv[t].add_target & 0x0f;

			if (!ami_drv_inq(sc, ch, tg, 0, &inqbuf)) {
				char vend[8+16+4+1];

				bcopy(inqbuf.vendor, vend, sizeof vend - 1);

				vend[sizeof vend - 1] = '\0';
				strlcpy(bd->bd_vendor, vend,
				    sizeof(bd->bd_vendor));
			}

			if (!ami_drv_inq(sc, ch, tg, 0x80, &vpdbuf)) {
				char ser[32 + 1];

				bcopy(vpdbuf.serial, ser, sizeof ser - 1);

				ser[sizeof ser - 1] = '\0';
				if (vpdbuf.page_length < sizeof ser)
					ser[vpdbuf.page_length] = '\0';
				strlcpy(bd->bd_serial, ser,
				    sizeof(bd->bd_serial));
			}

			bd->bd_channel = ch;
			bd->bd_target = tg;

			strlcpy(bd->bd_procdev, sc->sc_rawsoftcs[ch].sc_procdev,
			    sizeof(bd->bd_procdev));

			error = 0;
			goto bail;
		}

	/* XXX if we reach this do dedicated hotspare magic*/
bail:
	free(p, M_DEVBUF);

	return (error);
}

int ami_ioctl_alarm(sc, ba)
	struct ami_softc *sc;
	struct bioc_alarm *ba;
{
	int error = 0;
	u_int8_t func, ret;

	switch(ba->ba_opcode) {
	case BIOC_SADISABLE:
		func = AMI_SPKR_OFF;
		break;

	case BIOC_SAENABLE:
		func = AMI_SPKR_ON;
		break;

	case BIOC_SASILENCE:
		func = AMI_SPKR_SHUT;
		break;

	case BIOC_GASTATUS:
		func = AMI_SPKR_GVAL;
		break;

	case BIOC_SATEST:
		func = AMI_SPKR_TEST;
		break;

	default:
		AMI_DPRINTF(AMI_D_IOCTL, ("%s: biocalarm invalid opcode %x\n",
		    sc->sc_dev.dv_xname, ba->ba_opcode));
		return (EINVAL);
	}

	if (ami_mgmt(sc, AMI_SPEAKER, func, 0, 0, sizeof ret, &ret))
		error = EINVAL;
	else
		if (ba->ba_opcode == BIOC_GASTATUS)
			ba->ba_status = ret;
		else
			ba->ba_status = 0;

	return (error);
}

int
ami_ioctl_setstate(sc, bs)
	struct ami_softc *sc;
	struct bioc_setstate *bs;
{
	int func;
	struct ami_big_diskarray *p;
	struct scsi_inquiry_data inqbuf;
	int off;

	switch (bs->bs_status) {
	case BIOC_SSONLINE:
		func = AMI_STATE_ON;
		break;

	case BIOC_SSOFFLINE:
		func = AMI_STATE_FAIL;
		break;

	case BIOC_SSHOTSPARE:
		p = malloc(sizeof *p, M_DEVBUF, M_NOWAIT);
		if (!p) {
			printf("%s: no memory for setstate\n",
			    sc->sc_dev.dv_xname);
			return (ENOMEM);
		}

		if (ami_mgmt(sc, AMI_FCOP, AMI_FC_RDCONF, 0, 0, sizeof *p, p))
			goto bail;

		off = bs->bs_channel * AMI_MAX_TARGET + bs->bs_target;

		if (ami_drv_inq(sc, bs->bs_channel, bs->bs_target, 0,
		    &inqbuf))
			goto bail;

		free(p, M_DEVBUF);

		func = AMI_STATE_SPARE;
		break;

	default:
		AMI_DPRINTF(AMI_D_IOCTL, ("%s: biocsetstate invalid opcode %x\n"
		    , sc->sc_dev.dv_xname, bs->bs_status));
		return (EINVAL);
	}

	if (ami_mgmt(sc, AMI_CHSTATE, bs->bs_channel, bs->bs_target, func,
	    0, NULL))
		return (EINVAL);

	return (0);

bail:
	free(p, M_DEVBUF);

	return (EINVAL);
}
#endif /* NBIO > 0 */

#ifdef AMI_DEBUG
void
ami_print_mbox(mbox)
	struct ami_iocmd *mbox;
{
	int i;

	printf("acc_cmd: %d  aac_id: %d  acc_busy: %d  acc_nstat: %d",
	    mbox->acc_cmd, mbox->acc_id, mbox->acc_busy, mbox->acc_nstat);
	printf("acc_status: %d  acc_poll: %d  acc_ack: %d\n",
	    mbox->acc_status, mbox->acc_poll, mbox->acc_ack);

	printf("acc_cmplidl: ");
	for (i = 0; i < AMI_MAXSTATACK; i++) {
		printf("[%d] = %d  ", i, mbox->acc_cmplidl[i]);
	}

	printf("\n");
}
#endif /* AMI_DEBUG */
