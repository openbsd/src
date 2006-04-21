/* $OpenBSD: mfi.c,v 1.22 2006/04/21 21:39:32 marco Exp $ */
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
#endif

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
	ccb->ccb_extra_frames = 0;
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

	DNPRINTF(MFI_D_CMD, "%s: mfi_transition_mfi: %#x\n", DEVNAME(sc),
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

	DNPRINTF(MFI_D_CMD, "mfi_initialize_firmware\n");

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (1);

	init = &ccb->ccb_frame->mfr_init;
	qinfo = (struct mfi_init_qinfo *)((uint8_t *)init + MFI_FRAME_SIZE);

	memset(qinfo, 0, sizeof *qinfo);
	qinfo->miq_rq_entries = sc->sc_max_cmds + 1;
	qinfo->miq_rq_addr_lo = MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_reply_q);
	qinfo->miq_pi_addr_lo = MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_producer);
	qinfo->miq_ci_addr_lo = MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_consumer);

	init->mif_header.mfh_cmd = MFI_CMD_INIT;
	init->mif_header.mfh_data_len = sizeof *qinfo;
	init->mif_qinfo_new_addr_lo = ccb->ccb_pframe + MFI_FRAME_SIZE;

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

#define MFI_MAXFER 4096
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
	DNPRINTF(MFI_D_CMD, "mfi_despatch_cmd\n");

	mfi_write(sc, MFI_IQP, (ccb->ccb_pframe >> 3) | ccb->ccb_extra_frames);

	return(0);
}

int
mfi_poll(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	struct mfi_frame_header	*hdr;
	int			to = 0;

	DNPRINTF(MFI_D_CMD, "mfi_poll\n");

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

int
mfi_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	DNPRINTF(MFI_D_IOCTL, "mfi_scsi_ioctl\n");
#if 0
	struct ami_softc *sc = (struct ami_softc *)link->adapter_softc;

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
#endif
		return (ENOTTY);
}

int
mfi_scsi_cmd(struct scsi_xfer *xs)
{
	DNPRINTF(MFI_D_CMD, "mfi_scsi_cmd\n");
#if 0
	struct scsi_link *link = xs->sc_link;
	struct ami_softc *sc = link->adapter_softc;
	struct device *dev = link->device_softc;
#endif
	return (0);
}
