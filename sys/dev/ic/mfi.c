/* $OpenBSD: mfi.c,v 1.32 2006/05/16 15:50:51 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/mfireg.h>
#include <dev/ic/mfivar.h>

#if NBIO > 0
#include <dev/biovar.h>
#endif /* NBIO > 0 */

#ifdef MFI_DEBUG
uint32_t	mfi_debug = 0
		    | MFI_D_CMD
		    | MFI_D_INTR
		    | MFI_D_MISC
		    | MFI_D_DMA
		    | MFI_D_IOCTL
/*		    | MFI_D_RW */
		    | MFI_D_MEM
/*		    | MFI_D_CCB */
		;
#endif

struct cfdriver mfi_cd = {
	NULL, "mfi", DV_DULL
};

int	mfi_scsi_cmd(struct scsi_xfer *);
int	mfi_scsi_ioctl(struct scsi_link *, u_long, caddr_t, int, struct proc *);
void	mfiminphys(struct buf *bp);

struct scsi_adapter mfi_switch = {
	mfi_scsi_cmd, mfiminphys, 0, 0, mfi_scsi_ioctl
};

struct scsi_device mfi_dev = {
	NULL, NULL, NULL, NULL
};

struct mfi_ccb	*mfi_get_ccb(struct mfi_softc *);
void		mfi_put_ccb(struct mfi_ccb *);
int		mfi_init_ccb(struct mfi_softc *);

u_int32_t	mfi_read(struct mfi_softc *, bus_size_t);
void		mfi_write(struct mfi_softc *, bus_size_t, u_int32_t);
struct mfi_mem	*mfi_allocmem(struct mfi_softc *, size_t);
void		mfi_freemem(struct mfi_softc *, struct mfi_mem *);
int		mfi_transition_firmware(struct mfi_softc *);
int		mfi_initialize_firmware(struct mfi_softc *);

int		mfi_despatch_cmd(struct mfi_softc *, struct mfi_ccb *);
int		mfi_poll(struct mfi_softc *, struct mfi_ccb *);
int		mfi_start_xs(struct mfi_softc *, struct mfi_ccb *,
		    struct scsi_xfer *);

/* LD commands */
int		mfi_ld_inquiry(struct scsi_xfer *);
void		mfi_done_ld_inquiry(struct mfi_softc *, struct mfi_ccb *);
int		mfi_ld_tur(struct scsi_xfer *);
void		mfi_done_ld_tur(struct mfi_softc *, struct mfi_ccb *);
int		mfi_ld_readcap(struct scsi_xfer *);
void		mfi_done_ld_readcap(struct mfi_softc *, struct mfi_ccb *);

#if NBIO > 0
int		mfi_ioctl(struct device *, u_long, caddr_t);
int		mfi_ioctl_inq(struct mfi_softc *, struct bioc_inq *);
int		mfi_ioctl_vol(struct mfi_softc *, struct bioc_vol *);
int		mfi_ioctl_disk(struct mfi_softc *, struct bioc_disk *);
int		mfi_ioctl_alarm(struct mfi_softc *, struct bioc_alarm *);
int		mfi_ioctl_setstate(struct mfi_softc *, struct bioc_setstate *);
#endif /* NBIO > 0 */

struct mfi_ccb *
mfi_get_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	int			s;

	s = splbio();
	ccb = TAILQ_FIRST(&sc->sc_ccb_freeq);
	if (ccb) {
		TAILQ_REMOVE(&sc->sc_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = MFI_CCB_READY;
	}
	splx(s);

	DNPRINTF(MFI_D_CCB, "%s: mfi_get_ccb: %p\n", DEVNAME(sc), ccb);

	return (ccb);
}

void
mfi_put_ccb(struct mfi_ccb *ccb)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	int			s;

	DNPRINTF(MFI_D_CCB, "%s: mfi_put_ccb: %p\n", DEVNAME(sc), ccb);

	s = splbio();
	ccb->ccb_state = MFI_CCB_FREE;
	ccb->ccb_xs = NULL;
	ccb->ccb_flags = 0;
	ccb->ccb_done = NULL;
	ccb->ccb_direction = 0;
	ccb->ccb_frame_size = 0;
	ccb->ccb_extra_frames = 0;
	ccb->ccb_sgl = NULL;
	TAILQ_INSERT_TAIL(&sc->sc_ccb_freeq, ccb, ccb_link);
	splx(s);
}

int
mfi_init_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	uint32_t		i;
	int			error;

	DNPRINTF(MFI_D_CCB, "%s: mfi_init_ccb\n", DEVNAME(sc));

	sc->sc_ccb = malloc(sizeof(struct mfi_ccb) * sc->sc_max_cmds,
	    M_DEVBUF, M_WAITOK);
	memset(sc->sc_ccb, 0, sizeof(struct mfi_ccb) * sc->sc_max_cmds);

	for (i = 0; i < sc->sc_max_cmds; i++) {
		ccb = &sc->sc_ccb[i];

		ccb->ccb_sc = sc;

		/* select i'th frame */
		ccb->ccb_frame = (union mfi_frame *)
		    (MFIMEM_KVA(sc->sc_frames) + sc->sc_frames_size * i);
		ccb->ccb_pframe = htole32(
		    MFIMEM_DVA(sc->sc_frames) + sc->sc_frames_size * i);
		ccb->ccb_frame->mfr_header.mfh_context = i;

		/* select i'th sense */
		ccb->ccb_sense = (struct mfi_sense *)
		    (MFIMEM_KVA(sc->sc_sense) + MFI_SENSE_SIZE * i);
		ccb->ccb_psense = htole32(
		    (MFIMEM_DVA(sc->sc_sense) + MFI_SENSE_SIZE * i));

		/* create a dma map for transfer */
		error = bus_dmamap_create(sc->sc_dmat,
		    MAXPHYS, sc->sc_max_sgl, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ccb_dmamap);
		if (error) {
			printf("%s: cannot create ccb dmamap (%d)\n",
			    DEVNAME(sc), error);
			goto destroy;
		}

		DNPRINTF(MFI_D_CCB,
		    "ccb(%d): %p frame: %x (%x) sense: %x (%x) map: %x\n",
		    ccb->ccb_frame->mfr_header.mfh_context, ccb,
		    ccb->ccb_frame, ccb->ccb_pframe,
		    ccb->ccb_sense, ccb->ccb_psense,
		    ccb->ccb_dmamap);

		/* add ccb to queue */
		mfi_put_ccb(ccb);
	}

	return (0);
destroy:
	/* free dma maps and ccb memory */
	while (i) {
		ccb = &sc->sc_ccb[i];
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
		i--;
	}

	free(sc->sc_ccb, M_DEVBUF);

	return (1);
}

u_int32_t
mfi_read(struct mfi_softc *sc, bus_size_t r)
{
	u_int32_t rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MFI_D_RW, "mr 0x%x 0x08%x ", r, rv);
	return (rv);
}

void
mfi_write(struct mfi_softc *sc, bus_size_t r, u_int32_t v)
{
	DNPRINTF(MFI_D_RW, "mw 0x%x 0x%08x", r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

struct mfi_mem *
mfi_allocmem(struct mfi_softc *sc, size_t size)
{
	struct mfi_mem		*mm;
	int			nsegs;

	DNPRINTF(MFI_D_MEM, "%s: mfi_allocmem: %d\n", DEVNAME(sc),
	    size);

	mm = malloc(sizeof(struct mfi_mem), M_DEVBUF, M_NOWAIT);
	if (mm == NULL)
		return (NULL);

	memset(mm, 0, sizeof(struct mfi_mem));
	mm->am_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mm->am_map) != 0)
		goto amfree; 

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mm->am_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mm->am_seg, nsegs, size, &mm->am_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mm->am_map, mm->am_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	DNPRINTF(MFI_D_MEM, "  kva: %p  dva: %p  map: %p\n",
	    mm->am_kva, mm->am_map->dm_segs[0].ds_addr, mm->am_map);

	memset(mm->am_kva, 0, size);
	return (mm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
amfree:
	free(mm, M_DEVBUF);

	return (NULL);
}

void
mfi_freemem(struct mfi_softc *sc, struct mfi_mem *mm)
{
	DNPRINTF(MFI_D_MEM, "%s: mfi_freemem: %p\n", DEVNAME(sc), mm);

	bus_dmamap_unload(sc->sc_dmat, mm->am_map);
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, mm->am_size);
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
	free(mm, M_DEVBUF);
}

int
mfi_transition_firmware(struct mfi_softc *sc)
{
	int32_t			fw_state, cur_state;
	int			max_wait, i;

	fw_state = mfi_read(sc, MFI_OMSG0) & MFI_STATE_MASK;

	DNPRINTF(MFI_D_CMD, "%s: mfi_transition_firmware: %#x\n", DEVNAME(sc),
	    fw_state);

	while (fw_state != MFI_STATE_READY) {
		DNPRINTF(MFI_D_MISC,
		    "%s: waiting for firmware to become ready\n",
		    DEVNAME(sc));
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_STATE_FAULT:
			printf("%s: firmware fault\n", DEVNAME(sc));
			return (1);
		case MFI_STATE_WAIT_HANDSHAKE:
			mfi_write(sc, MFI_IDB, MFI_INIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_STATE_OPERATIONAL:
			mfi_write(sc, MFI_IDB, MFI_INIT_READY);
			max_wait = 10;
			break;
		case MFI_STATE_UNDEFINED:
		case MFI_STATE_BB_INIT:
			max_wait = 2;
			break;
		case MFI_STATE_FW_INIT:
		case MFI_STATE_DEVICE_SCAN:
		case MFI_STATE_FLUSH_CACHE:
			max_wait = 20;
			break;
		default:
			printf("%s: unknown firmware state %d\n",
			    DEVNAME(sc), fw_state);
			return (1);
		}
		for (i = 0; i < (max_wait * 10); i++) {
			fw_state = mfi_read(sc, MFI_OMSG0) & MFI_STATE_MASK;
			if (fw_state == cur_state)
				DELAY(100000);
			else
				break;
		}
		if (fw_state == cur_state) {
			printf("%s: firmware stuck in state %#x\n",
			    DEVNAME(sc), fw_state);
			return (1);
		}
	}

	return (0);
}

int
mfi_initialize_firmware(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	struct mfi_init_frame	*init;
	struct mfi_init_qinfo	*qinfo;

	DNPRINTF(MFI_D_MISC, "%s: mfi_initialize_firmware\n", DEVNAME(sc));

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (1);

	init = &ccb->ccb_frame->mfr_init;
	qinfo = (struct mfi_init_qinfo *)((uint8_t *)init + MFI_FRAME_SIZE);

	memset(qinfo, 0, sizeof *qinfo);
	qinfo->miq_rq_entries = sc->sc_max_cmds + 1;
	qinfo->miq_rq_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_reply_q));
	qinfo->miq_pi_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_producer));
	qinfo->miq_ci_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_consumer));

	init->mif_header.mfh_cmd = MFI_CMD_INIT;
	init->mif_header.mfh_data_len = sizeof *qinfo;
	init->mif_qinfo_new_addr_lo = htole32(ccb->ccb_pframe + MFI_FRAME_SIZE);

	DNPRINTF(MFI_D_MISC, "%s: entries: %x rq: %x pi: %x ci: %x\n",
	    DEVNAME(sc),
	    qinfo->miq_rq_entries, qinfo->miq_rq_addr_lo,
	    qinfo->miq_pi_addr_lo, qinfo->miq_ci_addr_lo);

	if (mfi_poll(sc, ccb)) {
		printf("%s: mfi_initialize_firmware failed\n", DEVNAME(sc));
		return (1);
	}

	mfi_put_ccb(ccb);

	return (0);
}

void
mfiminphys(struct buf *bp)
{
	DNPRINTF(MFI_D_MISC, "mfiminphys: %d\n", bp->b_bcount);

	/* XXX currently using MFI_MAXFER = MAXPHYS */
	if (bp->b_bcount > MFI_MAXFER)
		bp->b_bcount = MFI_MAXFER;
	minphys(bp);
}

int
mfi_attach(struct mfi_softc *sc)
{
	uint32_t		status, frames;

	DNPRINTF(MFI_D_MISC, "%s: mfi_attach\n", DEVNAME(sc));

	if (mfi_transition_firmware(sc))
		return (1);

	TAILQ_INIT(&sc->sc_ccb_freeq);

	status = mfi_read(sc, MFI_OMSG0);
	sc->sc_max_cmds = status & MFI_STATE_MAXCMD_MASK;
	sc->sc_max_sgl = (status & MFI_STATE_MAXSGL_MASK) >> 16;
	DNPRINTF(MFI_D_MISC, "%s: max commands: %u, max sgl: %u\n",
	    DEVNAME(sc), sc->sc_max_cmds, sc->sc_max_sgl);

	/* consumer/producer and reply queue memory */
	sc->sc_pcq = mfi_allocmem(sc, (sizeof(uint32_t) * sc->sc_max_cmds) +
	    sizeof(struct mfi_prod_cons));
	if (sc->sc_pcq == NULL) {
		printf("%s: unable to allocate reply queue memory\n",
		    DEVNAME(sc));
		goto nopcq;
	}

	/* frame memory */
	/* we are not doing 64 bit IO so only calculate # of 32 bit frames */
	frames = (sizeof(struct mfi_sg32) * sc->sc_max_sgl +
	    MFI_FRAME_SIZE - 1) / MFI_FRAME_SIZE + 1;
	sc->sc_frames_size = frames * MFI_FRAME_SIZE;
	sc->sc_frames = mfi_allocmem(sc, sc->sc_frames_size * sc->sc_max_cmds);
	if (sc->sc_frames == NULL) {
		printf("%s: unable to allocate frame memory\n", DEVNAME(sc));
		goto noframe;
	}
	/* XXX hack, fix this */
	if (MFIMEM_DVA(sc->sc_frames) & 0x3f) {
		printf("%s: improper frame alignment (%#x) FIXME\n",
		    DEVNAME(sc), MFIMEM_DVA(sc->sc_pcq));
		goto noframe;
	}

	/* sense memory */
	sc->sc_sense = mfi_allocmem(sc, sc->sc_max_cmds * MFI_SENSE_SIZE);
	if (sc->sc_sense == NULL) {
		printf("%s: unable to allocate sense memory\n", DEVNAME(sc));
		goto nosense;
	}

	/* now that we have all memory bits go initialize ccbs */
	if (mfi_init_ccb(sc)) {
		printf("%s: could not init ccb list\n", DEVNAME(sc));
		goto noinit;
	}

	/* kickstart firmware with all addresses and pointers */
	if (mfi_initialize_firmware(sc)) {
		printf("%s: could not initialize firmware\n", DEVNAME(sc));
		goto noinit;
	}

#if NBIO > 0
	if (bio_register(&sc->sc_dev, mfi_ioctl) != 0)
		panic("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = mfi_ioctl;
#endif /* NBIO > 0 */

	sc->sc_max_ld = MFI_MAX_LD;

	/* XXX fake one ld for now */
	sc->sc_ld_cnt = 2;
	sc->sc_ld[0].ld_present = 1;
	sc->sc_ld[1].ld_present = 1;
	sc->sc_max_ld = 2;

	if (sc->sc_ld_cnt)
		sc->sc_link.openings = sc->sc_max_cmds / sc->sc_ld_cnt;
	else
		sc->sc_link.openings = sc->sc_max_cmds;

	sc->sc_link.device = &mfi_dev;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &mfi_switch;
	sc->sc_link.adapter_target = MFI_MAX_LD;
	sc->sc_link.adapter_buswidth = sc->sc_max_ld;
#if 0
	printf(", FW %s, BIOS v%s, %dMB RAM\n"
	    "%s: %d channels, %d %ss, %d logical drives\n",
	    sc->sc_fwver, sc->sc_biosver, sc->sc_memory, DEVNAME(sc),
	    sc->sc_channels, sc->sc_targets, p, sc->sc_ld_cnt);
#endif

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

	/* enable interrupts */
	mfi_write(sc, MFI_OMSK, MFI_ENABLE_INTR);

	return (0);
noinit:
	mfi_freemem(sc, sc->sc_sense);
nosense:
	mfi_freemem(sc, sc->sc_frames);
noframe:
	mfi_freemem(sc, sc->sc_pcq);
nopcq:
	return (1);
}

int
mfi_despatch_cmd(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	DNPRINTF(MFI_D_CMD, "%s: mfi_despatch_cmd\n", DEVNAME(sc));

	mfi_write(sc, MFI_IQP, (ccb->ccb_pframe >> 3) | ccb->ccb_extra_frames);

	return(0);
}

int
mfi_poll(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	struct mfi_frame_header	*hdr;
	int			to = 0;

	DNPRINTF(MFI_D_CMD, "%s: mfi_poll\n", DEVNAME(sc));

	hdr = &ccb->ccb_frame->mfr_header;
	hdr->mfh_cmd_status = 0xff;
	hdr->mfh_flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	mfi_despatch_cmd(sc, ccb);

	while (hdr->mfh_cmd_status == 0xff) {
		delay(1000);
		if (to++ > 5000) /* XXX 5 seconds busywait sucks */
			break;
	}
	if (hdr->mfh_cmd_status == 0xff) {
		printf("%s: timeout on ccb %d\n", DEVNAME(sc),
		    hdr->mfh_context);
		ccb->ccb_flags |= MFI_CCB_F_ERR;
		return (1);
	}
	
	return (0);
}

int
mfi_intr(void *arg)
{
	struct mfi_softc	*sc = arg;
	struct mfi_prod_cons	*pcq;
	uint32_t		status, producer, consumer, ctx;
	int			s, claimed = 0;

	status = mfi_read(sc, MFI_OSTS);
	if ((status & MFI_OSTS_INTR_VALID) == 0)
		return (claimed);
	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_OSTS, status);

	DNPRINTF(MFI_D_INTR, "%s: mfi_intr\n", DEVNAME(sc));

	pcq = MFIMEM_KVA(sc->sc_pcq);
	producer = pcq->mpc_producer;
	consumer = pcq->mpc_consumer;

	s = splbio();
	while (consumer != producer) {
		ctx = pcq->mpc_reply_q[consumer];
		pcq->mpc_reply_q[consumer] = MFI_INVALID_CTX;
		if (ctx == MFI_INVALID_CTX)
			printf("%s: invalid context, p: %d c: %d\n",
			    DEVNAME(sc), producer, consumer);
		else {
			/* XXX remove from queue and call scsi_done */
			claimed = 1;
		}
		consumer++;
		if (consumer == sc->sc_max_cmds)
			consumer = 0;
	}
	splx(s);

	pcq->mpc_consumer = consumer;

	return (claimed);
}

void
mfi_done_ld_inquiry(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	struct scsi_xfer	*xs = ccb->ccb_xs;
	struct scsi_link	*link = xs->sc_link;

	DNPRINTF(MFI_D_CMD, "%s: mfi_ld_done_inquiry: %.0x\n",
	    DEVNAME(sc), link->target);
}

int
mfi_ld_inquiry(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_softc	*sc = link->adapter_softc;
	struct mfi_ccb		*ccb;
	struct mfi_pass_frame	*pf;

	DNPRINTF(MFI_D_CMD, "%s: mfi_ld_inquiry: %d\n",
	    DEVNAME(sc), link->target);

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (TRY_AGAIN_LATER);

	pf = &ccb->ccb_frame->mfr_pass;
	pf->mpf_header.mfh_cmd = MFI_CMD_LD_SCSI_IO;
	pf->mpf_header.mfh_target_id = link->target;
	pf->mpf_header.mfh_lun_id = 0;
	pf->mpf_header.mfh_cdb_len = 6;
	pf->mpf_header.mfh_timeout = 0;
	pf->mpf_header.mfh_data_len= sizeof(struct scsi_inquiry_data); /* XXX */
	pf->mpf_header.mfh_sense_len = MFI_SENSE_SIZE;

	pf->mpf_sense_addr_hi = 0;
	pf->mpf_sense_addr_lo = htole32(ccb->ccb_psense);

	memset(pf->mpf_cdb, 0, 16);
	pf->mpf_cdb[0] = INQUIRY;
	pf->mpf_cdb[4] = sizeof(struct scsi_inquiry_data);

	ccb->ccb_done = mfi_done_ld_inquiry;
	ccb->ccb_xs = xs; /* XXX here or in mfi_start_xs? */
	ccb->ccb_sgl = &pf->mpf_sgl;
	ccb->ccb_direction = MFI_DATA_IN;
	ccb->ccb_frame_size = MFI_PASS_FRAME_SIZE;

	return (mfi_start_xs(sc, ccb, xs));
}

void
mfi_done_ld_tur(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	struct scsi_xfer	*xs = ccb->ccb_xs;
	struct scsi_link	*link = xs->sc_link;

	DNPRINTF(MFI_D_CMD, "%s: mfi_done_ld_tur: %.0x\n",
	    DEVNAME(sc), link->target);
}

int
mfi_ld_tur(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_softc	*sc = link->adapter_softc;
	struct mfi_ccb		*ccb;
	struct mfi_pass_frame	*pf;

	DNPRINTF(MFI_D_CMD, "%s: mfi_ld_tur: %d\n",
	    DEVNAME(sc), link->target);

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (TRY_AGAIN_LATER);

	pf = &ccb->ccb_frame->mfr_pass;
	pf->mpf_header.mfh_cmd = MFI_CMD_LD_SCSI_IO;
	pf->mpf_header.mfh_target_id = link->target;
	pf->mpf_header.mfh_lun_id = 0;
	pf->mpf_header.mfh_cdb_len = 6;
	pf->mpf_header.mfh_timeout = 0;
	pf->mpf_header.mfh_data_len= 0;
	pf->mpf_header.mfh_sense_len = MFI_SENSE_SIZE;

	pf->mpf_sense_addr_hi = 0;
	pf->mpf_sense_addr_lo = htole32(ccb->ccb_psense);

	memset(pf->mpf_cdb, 0, 16);
	pf->mpf_cdb[0] = TEST_UNIT_READY;

	ccb->ccb_done = mfi_done_ld_tur;
	ccb->ccb_xs = xs; /* XXX here or in mfi_start_xs? */
	ccb->ccb_sgl = &pf->mpf_sgl;
	ccb->ccb_direction = 0;
	ccb->ccb_frame_size = MFI_PASS_FRAME_SIZE;

	/* XXX don't do this here, make something generic */
	if (ccb->ccb_xs->flags & SCSI_POLL) {
		if (mfi_poll(sc, ccb)) {
			printf("%s: mfi_poll failed\n", DEVNAME(sc));
			xs->error = XS_DRIVER_STUFFUP;
			xs->flags |= ITSDONE;
			scsi_done(xs);
		}
		DNPRINTF(MFI_D_DMA, "%s: mfi_ld_tur complete %d\n",
		    DEVNAME(sc), ccb->ccb_dmamap->dm_nsegs);
		mfi_put_ccb(ccb);
		return (COMPLETE);
	}

	mfi_despatch_cmd(sc, ccb);

	DNPRINTF(MFI_D_DMA, "%s: mfi_ld_tur: queued %d\n", DEVNAME(sc),
	    ccb->ccb_dmamap->dm_nsegs);

	return (SUCCESSFULLY_QUEUED);
}

void
mfi_done_ld_readcap(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	struct scsi_xfer	*xs = ccb->ccb_xs;
	struct scsi_link	*link = xs->sc_link;

	DNPRINTF(MFI_D_CMD, "%s: mfi_ld_done_readcap: %.0x\n",
	    DEVNAME(sc), link->target);
}

int
mfi_ld_readcap(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_softc	*sc = link->adapter_softc;
	struct mfi_ccb		*ccb;
	struct mfi_pass_frame	*pf;

	DNPRINTF(MFI_D_CMD, "%s: mfi_ld_readcap: %d\n",
	    DEVNAME(sc), link->target);

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (TRY_AGAIN_LATER);

	pf = &ccb->ccb_frame->mfr_pass;
	pf->mpf_header.mfh_cmd = MFI_CMD_LD_SCSI_IO;
	pf->mpf_header.mfh_target_id = link->target;
	pf->mpf_header.mfh_lun_id = 0;
	pf->mpf_header.mfh_cdb_len = 6;
	pf->mpf_header.mfh_timeout = 0;
	pf->mpf_header.mfh_data_len= sizeof(struct scsi_read_capacity);
	pf->mpf_header.mfh_sense_len = MFI_SENSE_SIZE;

	pf->mpf_sense_addr_hi = 0;
	pf->mpf_sense_addr_lo = htole32(ccb->ccb_psense);

	memset(pf->mpf_cdb, 0, 16);
	pf->mpf_cdb[0] = READ_CAPACITY; /* XXX other drivers use READCAP 16 */

	ccb->ccb_done = mfi_done_ld_readcap;
	ccb->ccb_xs = xs; /* XXX here or in mfi_start_xs? */
	ccb->ccb_sgl = &pf->mpf_sgl;
	ccb->ccb_direction = MFI_DATA_IN;
	ccb->ccb_frame_size = MFI_PASS_FRAME_SIZE;

	return (mfi_start_xs(sc, ccb, xs));
}

int
mfi_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_softc	*sc = link->adapter_softc;
	/* struct device		*dev = link->device_softc; */
	struct mfi_ccb		*ccb;
	u_int8_t		target = link->target;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_cmd opcode: %02x\n",
	    DEVNAME(sc), xs->cmd->opcode);

	/* only issue IO through this path, create seperate path for mgmt */

	if (!cold) {
		DNPRINTF(MFI_D_CMD, "%s: no interrupt io yet %02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	if (target >= MFI_MAX_LD || !sc->sc_ld[target].ld_present ||
	    link->lun != 0) {
		DNPRINTF(MFI_D_CMD, "%s: invalid target %d\n",
		    DEVNAME(sc), target);
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	/* IO path */
	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		break;

	/* DCDB */
	case INQUIRY:
		return (mfi_ld_inquiry(xs));
		/* NOTREACHED */

	case TEST_UNIT_READY:
		return (mfi_ld_tur(xs));
		/* NOTREACHED */

	case START_STOP:
		DNPRINTF(MFI_D_CMD, "%s: start stop complete %d\n",
		    DEVNAME(sc), target);
		return (COMPLETE);
		
	case READ_CAPACITY:
		return (mfi_ld_readcap(xs));
		/* NOTREACHED */

#if 0
	case VERIFY:
#endif
	case SYNCHRONIZE_CACHE:
	case PREVENT_ALLOW:
	case REQUEST_SENSE:
		DNPRINTF(MFI_D_CMD, "%s: not implemented yet %02x\n",
		    DEVNAME(sc), xs->cmd->opcode);
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
		/* NOTREACHED */

	/* illegal opcode */
	default:
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20; /* invalid opcode */
		xs->error = XS_SENSE;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
		/* NOTREACHED */
	}

	DNPRINTF(MFI_D_CMD, "%s: start io %d\n", DEVNAME(sc), target);

	return (mfi_start_xs(sc, ccb, xs));
}

int
mfi_start_xs(struct mfi_softc *sc, struct mfi_ccb *ccb,
    struct scsi_xfer *xs)
{
	struct mfi_frame_header	*hdr;
	bus_dma_segment_t	*sgd;
	union mfi_sgl		*sgl;
	int			error, i;

	DNPRINTF(MFI_D_DMA, "%s: mfi_start_xs: %p\n", DEVNAME(sc), xs);

	error = bus_dmamap_load(sc->sc_dmat, ccb->ccb_dmamap,
	    xs->data, xs->datalen, NULL,
	    (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		if (error == EFBIG)
			printf("more than %d dma segs\n", sc->sc_max_sgl);
		else
			printf("error %d loading dma map\n", error);

		mfi_put_ccb(ccb);
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	hdr = &ccb->ccb_frame->mfr_header;
	sgl = ccb->ccb_sgl;
	sgd = ccb->ccb_dmamap->dm_segs;
	for (i = 0; i < ccb->ccb_dmamap->dm_nsegs; i++) {
		sgl->sg32[i].addr = htole32(sgd[i].ds_addr);
		sgl->sg32[i].len = htole32(sgd[i].ds_len);
	}

	if (ccb->ccb_direction == MFI_DATA_IN) {
		hdr->mfh_flags |= MFI_FRAME_DIR_READ;
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	} else {
		hdr->mfh_flags |= MFI_FRAME_DIR_WRITE;
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}

	hdr->mfh_sg_count = ccb->ccb_dmamap->dm_nsegs;
	ccb->ccb_frame_size += sc->sc_frames_size * ccb->ccb_dmamap->dm_nsegs;
	ccb->ccb_extra_frames = (ccb->ccb_frame_size - 1) / MFI_FRAME_SIZE;

	if (xs->flags & SCSI_POLL) {
		if (mfi_poll(sc, ccb)) {
			printf("%s: mfi_poll failed\n", DEVNAME(sc));
			xs->error = XS_DRIVER_STUFFUP;
			xs->flags |= ITSDONE;
			scsi_done(xs);
		}
		DNPRINTF(MFI_D_DMA, "%s: mfi_start_xs complete %d\n",
		    DEVNAME(sc), ccb->ccb_dmamap->dm_nsegs);
		mfi_put_ccb(ccb);
		return (COMPLETE);
	}

	mfi_despatch_cmd(sc, ccb);

	DNPRINTF(MFI_D_DMA, "%s: mfi_start_xs: queued %d\n", DEVNAME(sc),
	    ccb->ccb_dmamap->dm_nsegs);

	return (SUCCESSFULLY_QUEUED);
}

int
mfi_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	struct mfi_softc	*sc = (struct mfi_softc *)link->adapter_softc;

	DNPRINTF(MFI_D_IOCTL, "mfi_scsi_ioctl\n");

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
}

#if NBIO > 0
int
mfi_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct mfi_softc	*sc = (struct mfi_softc *)dev;
	int error = 0;

	DNPRINTF(MFI_D_IOCTL, "%s: ioctl ", DEVNAME(sc));

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(MFI_D_IOCTL, "inq ");
		error = mfi_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		DNPRINTF(MFI_D_IOCTL, "vol ");
		error = mfi_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		DNPRINTF(MFI_D_IOCTL, "disk ");
		error = mfi_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		DNPRINTF(MFI_D_IOCTL, "alarm ");
		error = mfi_ioctl_alarm(sc, (struct bioc_alarm *)addr);
		break;

	case BIOCSETSTATE:
		DNPRINTF(MFI_D_IOCTL, "setstate ");
		error = mfi_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	default:
		DNPRINTF(MFI_D_IOCTL, " invalid ioctl\n");
		error = EINVAL;
	}

	return (error);
}

int
mfi_ioctl_inq(struct mfi_softc *sc, struct bioc_inq *bi)
{
	return (ENOTTY); /* XXX not yet */
}

int
mfi_ioctl_vol(struct mfi_softc *sc, struct bioc_vol *bv)
{
	return (ENOTTY); /* XXX not yet */
}

int
mfi_ioctl_disk(struct mfi_softc *sc, struct bioc_disk *bd)
{
	return (ENOTTY); /* XXX not yet */
}

int
mfi_ioctl_alarm(struct mfi_softc *sc, struct bioc_alarm *ba)
{
	return (ENOTTY); /* XXX not yet */
}

int
mfi_ioctl_setstate(struct mfi_softc *sc, struct bioc_setstate *bs)
{
	return (ENOTTY); /* XXX not yet */
}
#endif /* NBIO > 0 */
