/*	$OpenBSD: aic7xxx_openbsd.c,v 1.24 2004/09/24 14:56:56 henning Exp $	*/
/*	$NetBSD: aic7xxx_osm.c,v 1.14 2003/11/02 11:07:44 wiz Exp $	*/

/*
 * Bus independent OpenBSD shim for the aic7xxx based adaptec SCSI controllers
 *
 * Copyright (c) 1994-2001 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Steve Murphree, Jr.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * //depot/aic7xxx/freebsd/dev/aic7xxx/aic7xxx_osm.c#12 $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic7xxx_osm.c,v 1.31 2002/11/30 19:08:58 scottl Exp $
 */
/*
 * Ported from FreeBSD by Pascal Renauld, Network Storage Solutions, Inc. - April 2003
 */

#include <sys/cdefs.h>
/* __KERNEL_RCSID(0, "$NetBSD: aic7xxx_osm.c,v 1.14 2003/11/02 11:07:44 wiz Exp $"); */

#include <dev/ic/aic7xxx_openbsd.h>
#include <dev/ic/aic7xxx_inline.h>

#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif


int	ahc_action(struct scsi_xfer *);
int	ahc_execute_scb(void *, bus_dma_segment_t *, int);
int	ahc_poll(struct ahc_softc *, int);
int	ahc_setup_data(struct ahc_softc *, struct scsi_xfer *, struct scb *);
void	ahc_set_recoveryscb(struct ahc_softc *, struct scb *);

void	ahc_minphys(struct buf *);
void	ahc_adapter_req_set_xfer_mode(struct ahc_softc *, struct scb *);


struct cfdriver ahc_cd = {
	NULL, "ahc", DV_DULL
};

static struct scsi_adapter ahc_switch =
{
	ahc_action,
	ahc_minphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device ahc_dev =
{
	NULL, /* Use default error handler */
	NULL, /* have a queue, served by this */
	NULL, /* have no async handler */
	NULL, /* Use default 'done' routine */
};

/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(struct ahc_softc *ahc)
{
	char ahc_info[256];
	int s;

        ahc_lock(ahc, &s);

	/*
	 * fill in the prototype scsi_links.
	 */
	ahc->sc_channel.adapter_target = ahc->our_id;
	if (ahc->features & AHC_WIDE)
		ahc->sc_channel.adapter_buswidth = 16;
	ahc->sc_channel.adapter_softc = ahc;
	ahc->sc_channel.adapter = &ahc_switch;
	ahc->sc_channel.openings = 16;
	ahc->sc_channel.device = &ahc_dev;

	if (ahc->features & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->sc_channel_b = ahc->sc_channel;
		ahc->sc_channel_b.adapter_target = ahc->our_id_b;
	}

	if (bootverbose) {
		ahc_controller_info(ahc, ahc_info, sizeof ahc_info);
		printf("%s: %s\n", ahc->sc_dev.dv_xname, ahc_info);
	}

	ahc_intr_enable(ahc, TRUE);

	if (ahc->flags & AHC_RESET_BUS_A)
		ahc_reset_channel(ahc, 'A', TRUE);
	if ((ahc->features & AHC_TWIN) && ahc->flags & AHC_RESET_BUS_B)
		ahc_reset_channel(ahc, 'B', TRUE);

	if ((ahc->flags & AHC_PRIMARY_CHANNEL) == 0) {
		/*
		 * Ensure SCSI_IS_SCSIBUS_B() returns false for sc_channel
		 * until sc_channel_b has been properly initialized by scsi
		 * layer.
		 */
		ahc->sc_channel_b.scsibus = 0xff;
		ahc->sc_child = config_found((void *)&ahc->sc_dev,
		    &ahc->sc_channel, scsiprint);
		if (ahc->features & AHC_TWIN)
			ahc->sc_child_b = config_found((void *)&ahc->sc_dev,
			    &ahc->sc_channel_b, scsiprint);
	} else {
		/*
		 * Ensure SCSI_IS_SCSIBUS_B() returns false for sc_channel_b
		 * until sc_channel has been properly initialized by scsi
		 * layer.
		 */
		ahc->sc_channel.scsibus = 0xff;
		if (ahc->features & AHC_TWIN)
			ahc->sc_child = config_found((void *)&ahc->sc_dev,
			    &ahc->sc_channel_b, scsiprint);
		ahc->sc_child_b = config_found((void *)&ahc->sc_dev,
		    &ahc->sc_channel, scsiprint);
	}

	ahc_unlock(ahc, &s);
	return (1);
}

/*
 * Catch an interrupt from the adapter
 */
int
ahc_platform_intr(void *arg)
{
	struct	ahc_softc *ahc = (struct ahc_softc *)arg;

	bus_dmamap_sync(ahc->parent_dmat, ahc->scb_data->hscb_dmamap,
	    0, ahc->scb_data->hscb_dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	return ahc_intr(ahc);
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahc_done(struct ahc_softc *ahc, struct scb *scb)
{
	struct scsi_xfer *xs = scb->xs;
	int s;

	bus_dmamap_sync(ahc->parent_dmat, ahc->scb_data->hscb_dmamap,
	    0, ahc->scb_data->hscb_dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	LIST_REMOVE(scb, pending_links);
	if ((scb->flags & SCB_UNTAGGEDQ) != 0) {
		struct scb_tailq *untagged_q;
		int target_offset;

		target_offset = SCB_GET_TARGET_OFFSET(ahc, scb);
		untagged_q = &ahc->untagged_queues[target_offset];
		TAILQ_REMOVE(untagged_q, scb, links.tqe);
		scb->flags &= ~SCB_UNTAGGEDQ;
		ahc_run_untagged_queue(ahc, untagged_q);
	}

	timeout_del(&xs->stimeout);

	if (xs->datalen) {
		int op;

		if ((xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(ahc->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);
		bus_dmamap_unload(ahc->parent_dmat, scb->dmamap);
	}

	/*
	 * If the recovery SCB completes, we have to be
	 * out of our timeout.
	 */
	if ((scb->flags & SCB_RECOVERY_SCB) != 0) {
		struct	scb *list_scb;

		/*
		 * We were able to complete the command successfully,
		 * so reinstate the timeouts for all other pending
		 * commands.
		 */
		LIST_FOREACH(list_scb, &ahc->pending_scbs, pending_links) {
			struct scsi_xfer *txs = list_scb->xs;
			if (!(txs->flags & SCSI_POLL))
				timeout_add(&list_scb->xs->stimeout,
				    (list_scb->xs->timeout * hz)/1000);
		}

		if (xs->error != CAM_REQ_INPROG)
			ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		ahc_print_path(ahc, scb);
		printf("no longer in timeout, status = %x\n", xs->status);
	}

	/* Translate the CAM status code to a SCSI error code. */
	switch (xs->error) {
	case CAM_SCSI_STATUS_ERROR:
	case CAM_REQ_INPROG:
	case CAM_REQ_CMP:
		switch (xs->status) {
		case SCSI_TASKSET_FULL:
			/* SCSI Layer won't requeue, so we force infinite
			 * retries until queue space is available. XS_BUSY
			 * is dangerous because if the NOSLEEP flag is set
			 * it can cause the I/O to return EIO. XS_BUSY code
			 * falls through to XS_TIMEOUT anyway.
			 */
			xs->error = XS_TIMEOUT;
			xs->retries++;
			break;
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
		case SCSI_CHECK:
		case SCSI_TERMINATED:
			if ((scb->flags & SCB_SENSE) == 0) {
				/* CHECK on CHECK? */
				xs->error = XS_DRIVER_STUFFUP;
			} else
				xs->error = XS_NOERROR;
			break;
		default:
			xs->error = XS_NOERROR;
			break;
		}
		break;
	case CAM_BUSY:
		xs->error = XS_BUSY;
		break;
	case CAM_CMD_TIMEOUT:
		xs->error = XS_TIMEOUT;
		break;
	case CAM_BDR_SENT:
	case CAM_SCSI_BUS_RESET:
		xs->error = XS_RESET;
	case CAM_REQUEUE_REQ:
		xs->error = XS_TIMEOUT;
		xs->retries++;
		break;
	case CAM_SEL_TIMEOUT:
		xs->error = XS_SELTIMEOUT;
		break;
	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	/* Don't clobber any existing error state */
	if (xs->error != XS_NOERROR) {
	  /* Don't clobber any existing error state */
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * Zero any sense not transferred by the
		 * device.  The SCSI spec mandates that any
		 * untransferred data should be assumed to be
		 * zero.  Complete the 'bounce' of sense information
		 * through buffers accessible via bus-space by
		 * copying it into the clients csio.
		 */
		memset(&xs->sense, 0, sizeof(struct scsi_sense_data));
		memcpy(&xs->sense, ahc_get_sense_buf(ahc, scb),
		    ahc_le32toh(scb->sg_list->len) & AHC_SG_LEN_MASK);
		xs->error = XS_SENSE;
	}

        ahc_lock(ahc, &s);       
	ahc_free_scb(ahc, scb);
        ahc_unlock(ahc, &s);       

	xs->flags |= ITSDONE;
	scsi_done(xs);
}

void
ahc_minphys(bp)
	struct buf *bp;
{
	/*
	 * Even though the card can transfer up to 16megs per command
	 * we are limited by the number of segments in the dma segment
	 * list that we can hold.  The worst case is that all pages are
	 * discontinuous physically, hence the "page per segment" limit
	 * enforced here.
	 */
	if (bp->b_bcount > ((AHC_NSEG - 1) * PAGE_SIZE)) {
		bp->b_bcount = ((AHC_NSEG - 1) * PAGE_SIZE);
	}
	minphys(bp);
}

int32_t
ahc_action(struct scsi_xfer *xs)
{
	struct ahc_softc *ahc;
	struct scb *scb;
	struct hardware_scb *hscb;
	u_int target_id;
	u_int our_id;
	int s;
	int dontqueue = 0;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("ahc_action\n"));
	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;

	/* determine safety of software queueing */
	dontqueue = xs->flags & SCSI_POLL;

	target_id = xs->sc_link->target;
	our_id = SCSI_SCSI_ID(ahc, xs->sc_link);

	/*
	 * get an scb to use.
	 */
	ahc_lock(ahc, &s);
	if ((scb = ahc_get_scb(ahc)) == NULL) {
		ahc_unlock(ahc, &s);
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	ahc_unlock(ahc, &s);

	hscb = scb->hscb;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%p)\n", scb));
	scb->xs = xs;
	timeout_set(&xs->stimeout, ahc_timeout, scb);

	/*
	 * Put all the arguments for the xfer in the scb
	 */
	hscb->control = 0;
	hscb->scsiid = BUILD_SCSIID(ahc, xs->sc_link, target_id, our_id);
	hscb->lun = xs->sc_link->lun;
	if (xs->xs_control & XS_CTL_RESET) {
		hscb->cdb_len = 0;
		scb->flags |= SCB_DEVICE_RESET;
		hscb->control |= MK_MESSAGE;
		ahc_execute_scb(scb, NULL, 0);
	}

	return ahc_setup_data(ahc, xs, scb);
}

int
ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nsegments)
{
	struct	scb *scb;
	struct	scsi_xfer *xs;
	struct	ahc_softc *ahc;
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;

	u_int	mask;
	int	s;

	scb = (struct scb *)arg;
	xs = scb->xs;
	xs->error = CAM_REQ_INPROG;
	xs->status = 0;
	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;

	if (nsegments != 0) {
		struct	  ahc_dma_seg *sg;
		bus_dma_segment_t *end_seg;
		int op;
		
		end_seg = dm_segs + nsegments;

		/* Copy the segments into our SG list */
		sg = scb->sg_list;
		while (dm_segs < end_seg) {
			uint32_t len;

			sg->addr = ahc_htole32(dm_segs->ds_addr);
			len = dm_segs->ds_len
			    | ((dm_segs->ds_addr >> 8) & 0x7F000000);
			sg->len = ahc_htole32(len);
			sg++;
			dm_segs++;
		}

		/*
		 * Note where to find the SG entries in bus space.
		 * We also set the full residual flag which the 
		 * sequencer will clear as soon as a data transfer
		 * occurs.
		 */
		scb->hscb->sgptr = ahc_htole32(scb->sg_list_phys|SG_FULL_RESID);

		if ((xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(ahc->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);

		sg--;
		sg->len |= ahc_htole32(AHC_DMA_LAST_SEG);

		bus_dmamap_sync(ahc->parent_dmat, scb->sg_map->sg_dmamap,
		    0, scb->sg_map->sg_dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Copy the first SG into the "current" data pointer area */
		scb->hscb->dataptr = scb->sg_list->addr;
		scb->hscb->datacnt = scb->sg_list->len;
	} else {
		scb->hscb->sgptr = ahc_htole32(SG_LIST_NULL);
		scb->hscb->dataptr = 0;
		scb->hscb->datacnt = 0;
	}

	scb->sg_count = nsegments;

	ahc_lock(ahc, &s);

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (xs->flags & ITSDONE) {
		if (nsegments != 0)
			bus_dmamap_unload(ahc->parent_dmat, scb->dmamap);

		ahc_free_scb(ahc, scb);
		ahc_unlock(ahc, &s);
		return (COMPLETE);
	}

	tinfo = ahc_fetch_transinfo(ahc, SCSIID_CHANNEL(ahc, scb->hscb->scsiid),
				    SCSIID_OUR_ID(scb->hscb->scsiid),
				    SCSIID_TARGET(ahc, scb->hscb->scsiid),
				    &tstate);

	mask = SCB_GET_TARGET_MASK(ahc, scb);
	scb->hscb->scsirate = tinfo->scsirate;
	scb->hscb->scsioffset = tinfo->curr.offset;

	if ((tstate->ultraenb & mask) != 0)
		scb->hscb->control |= ULTRAENB;

	if ((tstate->discenable & mask) != 0)
	    	scb->hscb->control |= DISCENB;

	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	}

	if ((tstate->tagenable & mask) != 0)
		scb->hscb->control |= TAG_ENB;

	bus_dmamap_sync(ahc->parent_dmat, ahc->scb_data->hscb_dmamap,
	    0, ahc->scb_data->hscb_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	LIST_INSERT_HEAD(&ahc->pending_scbs, scb, pending_links);

	if (!(xs->flags & SCSI_POLL))
		timeout_add(&xs->stimeout, (xs->timeout * hz) / 1000);

	/*
	 * We only allow one untagged transaction
	 * per target in the initiator role unless
	 * we are storing a full busy target *lun*
	 * table in SCB space.
	 *
	 * This really should not be of any
	 * concern, as we take care to avoid this
	 * in ahc_done().  XXX smurph
	 */
	if ((scb->hscb->control & (TARGET_SCB|TAG_ENB)) == 0
	    && (ahc->flags & AHC_SCB_BTT) == 0) {
		struct scb_tailq *untagged_q;
		int target_offset;

		target_offset = SCB_GET_TARGET_OFFSET(ahc, scb);
		untagged_q = &(ahc->untagged_queues[target_offset]);
		TAILQ_INSERT_TAIL(untagged_q, scb, links.tqe);
		scb->flags |= SCB_UNTAGGEDQ;
		if (TAILQ_FIRST(untagged_q) != scb) {
			if (xs->flags & SCSI_POLL)
				goto poll;
			else {		
				ahc_unlock(ahc, &s);
				return (SUCCESSFULLY_QUEUED);
			}
		}
	}
	scb->flags |= SCB_ACTIVE;

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		/* Define a mapping from our tag to the SCB. */
		ahc->scb_data->scbindex[scb->hscb->tag] = scb;
		ahc_pause(ahc);
		if ((ahc->flags & AHC_PAGESCBS) == 0)
			ahc_outb(ahc, SCBPTR, scb->hscb->tag);
		ahc_outb(ahc, TARG_IMMEDIATE_SCB, scb->hscb->tag);
		ahc_unpause(ahc);
	} else {
		ahc_queue_scb(ahc, scb);
	}

	if (!(xs->flags & SCSI_POLL)) {
		if (ahc->inited_target[xs->sc_link->target] == 0) {
			struct	ahc_devinfo devinfo;

			ahc_adapter_req_set_xfer_mode(ahc, scb);
			ahc_scb_devinfo(ahc, &devinfo, scb);
			ahc_update_neg_request(ahc, &devinfo, tstate, tinfo,
			    AHC_NEG_IF_NON_ASYNC);

			ahc->inited_target[xs->sc_link->target] = 1;
		}
		ahc_unlock(ahc, &s);
		return (SUCCESSFULLY_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll for completion
	 */
poll:
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_poll\n"));

	do {
		if (ahc_poll(ahc, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			ahc_timeout(scb);
			break;
		}
	} while (!(xs->flags & ITSDONE));

	ahc_unlock(ahc, &s);
	return (COMPLETE);
}

int
ahc_poll(struct ahc_softc *ahc, int wait)
{
	while (--wait) {
		DELAY(1000);
		if (ahc_inb(ahc, INTSTAT) & INT_PEND)
			break;
	}

	if (wait == 0) {
		printf("%s: board is not responding\n", ahc_name(ahc));
		return (EIO);
	}

	ahc_intr((void *)ahc);
	return (0);
}

int
ahc_setup_data(struct ahc_softc *ahc, struct scsi_xfer *xs,
	       struct scb *scb)
{
	struct hardware_scb *hscb;
	
	hscb = scb->hscb;
	xs->resid = xs->status = 0;
	xs->error = CAM_REQ_INPROG;

	hscb->cdb_len = xs->cmdlen;
	if (hscb->cdb_len > sizeof(hscb->cdb32)) {
		int s;

		ahc_set_transaction_status(scb, CAM_REQ_INVALID);
		ahc_lock(ahc, &s);
		ahc_free_scb(ahc, scb);
		ahc_unlock(ahc, &s);
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	if (hscb->cdb_len > 12) {
		memcpy(hscb->cdb32, xs->cmd, hscb->cdb_len);
		scb->flags |= SCB_CDB32_PTR;
	} else {
		memcpy(hscb->shared_data.cdb, xs->cmd, hscb->cdb_len);
	}
		
	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

                error = bus_dmamap_load(ahc->parent_dmat,
					scb->dmamap, xs->data,
					xs->datalen, NULL,
					(xs->flags & SCSI_NOSLEEP) ?
					BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		if (error) {
#ifdef AHC_DEBUG
                        printf("%s: in ahc_setup_data(): bus_dmamap_load() "
			       "= %d\n",
			       ahc_name(ahc), error);
#endif
			xs->error = XS_BUSY;
			xs->flags |= ITSDONE;
			scsi_done(xs);
			return (TRY_AGAIN_LATER);	/* XXX fvdl */
}
		error = ahc_execute_scb(scb,
					scb->dmamap->dm_segs,
					scb->dmamap->dm_nsegs);
		return error;
	} else {
		return ahc_execute_scb(scb, NULL, 0);
	}
}

void
ahc_set_recoveryscb(struct ahc_softc *ahc, struct scb *scb) {

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct scb *list_scb;

		scb->flags |= SCB_RECOVERY_SCB;

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		LIST_FOREACH(list_scb, &ahc->pending_scbs, pending_links) {
			timeout_del(&list_scb->xs->stimeout);
		}
	}
}

void
ahc_timeout(void *arg)
{
	struct	scb *scb;
	struct	ahc_softc *ahc;
	int	s;
	int	found;
	u_int	last_phase;
	int	target;
	int	lun;
	int	i;
	char	channel;

	scb = (struct scb *)arg;
	ahc = (struct ahc_softc *)scb->xs->sc_link->adapter_softc;

	ahc_lock(ahc, &s);

	ahc_pause_and_flushwork(ahc);

	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("%s: Timed out SCB already complete. "
		       "Interrupts may not be functioning.\n", ahc_name(ahc));
		ahc_unpause(ahc);
		ahc_unlock(ahc, &s);
		return;
	}

	target = SCB_GET_TARGET(ahc, scb);
	channel = SCB_GET_CHANNEL(ahc, scb);
	lun = SCB_GET_LUN(scb);

	ahc_print_path(ahc, scb);
	printf("SCB 0x%x - timed out\n", scb->hscb->tag);
	ahc_dump_card_state(ahc);
	last_phase = ahc_inb(ahc, LASTPHASE);
	if (scb->sg_count > 0) {
		for (i = 0; i < scb->sg_count; i++) {
			printf("sg[%d] - Addr 0x%x : Length %d\n",
			       i,
			       scb->sg_list[i].addr,
			       scb->sg_list[i].len & AHC_SG_LEN_MASK);
		}
	}
	if (scb->flags & (SCB_DEVICE_RESET|SCB_ABORT)) {
		/*
		 * Been down this road before.
		 * Do a full bus reset.
		 */
bus_reset:
		ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		found = ahc_reset_channel(ahc, channel, /*Initiate Reset*/TRUE);
		printf("%s: Issued Channel %c Bus Reset. "
		       "%d SCBs aborted\n", ahc_name(ahc), channel, found);
	} else {
		/*
		 * If we are a target, transition to bus free and report
		 * the timeout.
		 * 
		 * The target/initiator that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths).
		 * If the bus is idle and we are acting as the initiator
		 * for this request, queue a BDR message to the timed out
		 * target.  Otherwise, if the timed out transaction is
		 * active:
		 *   Initiator transaction:
		 *	Stuff the message buffer with a BDR message and assert
		 *	ATN in the hopes that the target will let go of the bus
		 *	and go to the mesgout phase.  If this fails, we'll
		 *	get another timeout 2 seconds later which will attempt
		 *	a bus reset.
		 *
		 *   Target transaction:
		 *	Transition to BUS FREE and report the error.
		 *	It's good to be the target!
		 */
		u_int active_scb_index;
		u_int saved_scbptr;

		bus_dmamap_sync(ahc->parent_dmat,
		    ahc->scb_data->hscb_dmamap,
		    0, ahc->scb_data->hscb_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		saved_scbptr = ahc_inb(ahc, SCBPTR);
		active_scb_index = ahc_inb(ahc, SCB_TAG);

		if ((ahc_inb(ahc, SEQ_FLAGS) & NOT_IDENTIFIED) == 0
		  && (active_scb_index < ahc->scb_data->numscbs)) {
			struct scb *active_scb;

			/*
			 * If the active SCB is not us, assume that
			 * the active SCB has a longer timeout than
			 * the timedout SCB, and wait for the active
			 * SCB to timeout.
			 */ 
			active_scb = ahc_lookup_scb(ahc, active_scb_index);
			if (active_scb != scb) {
				u_int	newtimeout;

				ahc_print_path(ahc, active_scb);
				printf("Other SCB Timeout%s",
			 	       (scb->flags & SCB_OTHERTCL_TIMEOUT) != 0
				       ? " again\n" : "\n");
				scb->flags |= SCB_OTHERTCL_TIMEOUT;
				newtimeout = MAX(active_scb->xs->timeout,
						 scb->xs->timeout);
				timeout_add(&scb->xs->stimeout,
				    (newtimeout * hz) / 1000);
				ahc_unpause(ahc);
				ahc_unlock(ahc, &s);
				return;
			} 

			/* It's us */
			if ((scb->flags & SCB_TARGET_SCB) != 0) {

				/*
				 * Send back any queued up transactions
				 * and properly record the error condition.
				 */
				ahc_abort_scbs(ahc, SCB_GET_TARGET(ahc, scb),
					       SCB_GET_CHANNEL(ahc, scb),
					       SCB_GET_LUN(scb),
					       scb->hscb->tag,
					       ROLE_TARGET,
					       CAM_CMD_TIMEOUT);

				/* Will clear us from the bus */
				ahc_restart(ahc);
				ahc_unlock(ahc, &s);
				return;
			}

			ahc_set_recoveryscb(ahc, active_scb);
			ahc_outb(ahc, MSG_OUT, HOST_MSG);
			ahc_outb(ahc, SCSISIGO, last_phase|ATNO);
			ahc_print_path(ahc, active_scb);
			printf("BDR message in message buffer\n");
			active_scb->flags |=  SCB_DEVICE_RESET;
			timeout_add(&active_scb->xs->stimeout, 2 * hz);
			ahc_unpause(ahc);
		} else {
			int	 disconnected;

			bus_dmamap_sync(ahc->parent_dmat,
			    ahc->scb_data->hscb_dmamap,
			    0, ahc->scb_data->hscb_dmamap->dm_mapsize,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			/* XXX Shouldn't panic.  Just punt instead */
			if ((scb->flags & SCB_TARGET_SCB) != 0)
				panic("Timed-out target SCB but bus idle");

			if (last_phase != P_BUSFREE
			 && (ahc_inb(ahc, SSTAT0) & TARGET) != 0) {
				/* XXX What happened to the SCB? */
				/* Hung target selection.  Goto busfree */
				printf("%s: Hung target selection\n",
				       ahc_name(ahc));
				ahc_restart(ahc);
				ahc_unlock(ahc, &s);
				return;
			}

			if (ahc_search_qinfifo(ahc, target, channel, lun,
					       scb->hscb->tag, ROLE_INITIATOR,
					       /*status*/0, SEARCH_COUNT) > 0) {
				disconnected = FALSE;
			} else {
				disconnected = TRUE;
			}

			if (disconnected) {

				ahc_set_recoveryscb(ahc, scb);
				/*
				 * Actually re-queue this SCB in an attempt
				 * to select the device before it reconnects.
				 * In either case (selection or reselection),
				 * we will now issue a target reset to the
				 * timed-out device.
				 *
				 * Set the MK_MESSAGE control bit indicating
				 * that we desire to send a message.  We
				 * also set the disconnected flag since
				 * in the paging case there is no guarantee
				 * that our SCB control byte matches the
				 * version on the card.  We don't want the
				 * sequencer to abort the command thinking
				 * an unsolicited reselection occurred.
				 */
				scb->hscb->control |= MK_MESSAGE|DISCONNECTED;
				scb->flags |= SCB_DEVICE_RESET;

				/*
				 * Remove any cached copy of this SCB in the
				 * disconnected list in preparation for the
				 * queuing of our abort SCB.  We use the
				 * same element in the SCB, SCB_NEXT, for
				 * both the qinfifo and the disconnected list.
				 */
				ahc_search_disc_list(ahc, target, channel,
						     lun, scb->hscb->tag,
						     /*stop_on_first*/TRUE,
						     /*remove*/TRUE,
						     /*save_state*/FALSE);

				/*
				 * In the non-paging case, the sequencer will
				 * never re-reference the in-core SCB.
				 * To make sure we are notified during
				 * reslection, set the MK_MESSAGE flag in
				 * the card's copy of the SCB.
				 */
				if ((ahc->flags & AHC_PAGESCBS) == 0) {
					ahc_outb(ahc, SCBPTR, scb->hscb->tag);
					ahc_outb(ahc, SCB_CONTROL,
						 ahc_inb(ahc, SCB_CONTROL)
						| MK_MESSAGE);
				}

				/*
				 * Clear out any entries in the QINFIFO first
				 * so we are the next SCB for this target
				 * to run.
				 */
				ahc_search_qinfifo(ahc,
						   SCB_GET_TARGET(ahc, scb),
						   channel, SCB_GET_LUN(scb),
						   SCB_LIST_NULL,
						   ROLE_INITIATOR,
						   CAM_REQUEUE_REQ,
						   SEARCH_COMPLETE);
				ahc_print_path(ahc, scb);
				printf("Queuing a BDR SCB\n");
				ahc_qinfifo_requeue_tail(ahc, scb);
				ahc_outb(ahc, SCBPTR, saved_scbptr);
				timeout_add(&scb->xs->stimeout, 2 * hz);
				ahc_unpause(ahc);
			} else {
				/* Go "immediately" to the bus reset. */
				/* This shouldn't happen. */
				ahc_set_recoveryscb(ahc, scb);
				ahc_print_path(ahc, scb);
				printf("SCB %d: Immediate reset.  "
					"Flags = 0x%x\n", scb->hscb->tag,
					scb->flags);
				goto bus_reset;
			}
		}
	}
	ahc_unlock(ahc, &s);
}


void
ahc_platform_set_tags(struct ahc_softc *ahc,
		      struct ahc_devinfo *devinfo, int alg)
{
	struct ahc_tmode_tstate *tstate;

	ahc_fetch_transinfo(ahc, devinfo->channel, devinfo->our_scsiid,
			    devinfo->target, &tstate);

	/* XXXX Need to check quirks before doing this! XXXX */

	switch (alg) {
	case AHC_QUEUE_BASIC:
	case AHC_QUEUE_TAGGED:
		tstate->tagenable |= devinfo->target_mask;
		break;
	case AHC_QUEUE_NONE:
		tstate->tagenable &= ~devinfo->target_mask;
		break;
	}
}

int
ahc_platform_alloc(struct ahc_softc *ahc, void *platform_arg)
{
	if (sizeof(struct ahc_platform_data) > 0) {
		ahc->platform_data = malloc(sizeof(struct ahc_platform_data),
		    M_DEVBUF, M_NOWAIT);
		if (ahc->platform_data == NULL)
			return (ENOMEM);
		bzero(ahc->platform_data, sizeof(struct ahc_platform_data));
	}

	return (0);
}

void
ahc_platform_free(struct ahc_softc *ahc)
{
	if (sizeof(struct ahc_platform_data) > 0)
		free(ahc->platform_data, M_DEVBUF);
}

int
ahc_softc_comp(struct ahc_softc *lahc, struct ahc_softc *rahc)
{
	return (0);
}

void
ahc_send_async(struct ahc_softc *ahc, char channel, u_int target, u_int lun,
		u_int code, void *opt_arg)
{
	/* Nothing to do here for OpenBSD */
}

void
ahc_adapter_req_set_xfer_mode(struct ahc_softc *ahc, struct scb *scb)
{
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_tmode_tstate *tstate;
	struct ahc_syncrate *syncrate;
	struct ahc_devinfo devinfo;
	u_int16_t quirks;
	u_int width, ppr_options, period, offset;
	int s;

	s = splbio();

	ahc_scb_devinfo(ahc, &devinfo, scb);
	quirks = scb->xs->sc_link->quirks;
	tinfo = ahc_fetch_transinfo(ahc, devinfo.channel,
	    devinfo.our_scsiid, devinfo.target, &tstate);

	tstate->discenable |= (ahc->user_discenable & devinfo.target_mask);

	if (quirks & SDEV_NOTAGS)
		tstate->tagenable &= ~devinfo.target_mask;
	else if (ahc->user_tagenable & devinfo.target_mask)
		tstate->tagenable |= devinfo.target_mask;

	if (quirks & SDEV_NOWIDE)
		width = MSG_EXT_WDTR_BUS_8_BIT;
	else
		width = MSG_EXT_WDTR_BUS_16_BIT;

	ahc_validate_width(ahc, NULL, &width, ROLE_UNKNOWN);
	if (width > tinfo->user.width)
		width = tinfo->user.width;
	ahc_set_width(ahc, &devinfo, width, AHC_TRANS_GOAL, FALSE);

	if (quirks & SDEV_NOSYNC) {
		period = 0;
		offset = 0;
	} else {
		period = tinfo->user.period;
		offset = tinfo->user.offset;
	}

	/* XXX Look at saved INQUIRY flags for PPR capabilities XXX */ 
	ppr_options = tinfo->user.ppr_options;
	/* XXX Other reasons to avoid ppr? XXX */
	if (width < MSG_EXT_WDTR_BUS_16_BIT)
		ppr_options = 0;

	if ((tstate->discenable & devinfo.target_mask) == 0 ||
	    (tstate->tagenable & devinfo.target_mask) == 0)
		ppr_options &= ~MSG_EXT_PPR_PROT_IUS;

	syncrate = ahc_find_syncrate(ahc, &period, &ppr_options,
	    AHC_SYNCRATE_MAX);
	ahc_validate_offset(ahc, NULL, syncrate, &offset, width,
	    ROLE_UNKNOWN);

	if (offset == 0) {
		period = 0;
		ppr_options = 0;
	}

	if (ppr_options != 0 && tinfo->user.transport_version >= 3) {
		tinfo->goal.transport_version = tinfo->user.transport_version;
		tinfo->curr.transport_version = tinfo->user.transport_version;
	}

	ahc_set_syncrate(ahc, &devinfo, syncrate, period, offset, ppr_options,
	    AHC_TRANS_GOAL, FALSE);

	splx(s);
}
