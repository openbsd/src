/*	$OpenBSD: aic79xx_openbsd.c,v 1.12 2004/11/23 05:15:35 krw Exp $	*/

/*
 * Copyright (c) 2004 Milos Urbanek, Kenneth R. Westerback & Marco Peereboom
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Bus independent OpenBSD shim for the aic79xx based Adaptec SCSI controllers
 *
 * Copyright (c) 1994-2002 Justin T. Gibbs.
 * Copyright (c) 2001-2002 Adaptec Inc.
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
 */

#include <sys/cdefs.h>
/*
__FBSDID("$FreeBSD: src/sys/dev/aic7xxx/aic79xx_osm.c,v 1.16 2003/12/17 00:02:09 gibbs Exp $");
*/

#include <dev/ic/aic79xx_openbsd.h>
#include <dev/ic/aic79xx_inline.h>
#include <dev/ic/aic79xx.h>

#ifndef AHD_TMODE_ENABLE
#define AHD_TMODE_ENABLE 0
#endif

/* XXX milos add ahd_ioctl */
int	ahd_action(struct scsi_xfer *);
int	ahd_execute_scb(void *, bus_dma_segment_t *, int);
int	ahd_poll(struct ahd_softc *, int);
int	ahd_setup_data(struct ahd_softc *, struct scsi_xfer *, 
		    struct scb *);

void	ahd_adapter_req_set_xfer_mode(struct ahd_softc *, struct scb *);
void    ahd_minphys(struct buf *);

struct cfdriver ahd_cd = {
	        NULL, "ahd", DV_DULL
};

static struct scsi_adapter ahd_switch =
{
	ahd_action,
	ahd_minphys,
	0,
	0,
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device ahd_dev =
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
ahd_attach(struct ahd_softc *ahd)
{
	char   ahd_info[256];
	int	s;

	ahd_controller_info(ahd, ahd_info, sizeof ahd_info);
	printf("%s\n", ahd_info);
	ahd_lock(ahd, &s);

	/*
	 * fill in the prototype scsi_links.
	 */
        ahd->sc_channel.adapter_target = ahd->our_id;
        if (ahd->features & AHD_WIDE)
                ahd->sc_channel.adapter_buswidth = 16;
        ahd->sc_channel.adapter_softc = ahd;
        ahd->sc_channel.adapter = &ahd_switch;
        ahd->sc_channel.openings = 16;
        ahd->sc_channel.device = &ahd_dev;

	if (bootverbose) {
                ahd_controller_info(ahd, ahd_info, sizeof ahd_info);
                printf("%s: %s\n", ahd->sc_dev.dv_xname, ahd_info);
        }

	ahd_intr_enable(ahd, TRUE);

	if (ahd->flags & AHD_RESET_BUS_A)
                ahd_reset_channel(ahd, 'A', TRUE);

	ahd->sc_child = config_found((void *)&ahd->sc_dev,
	    &ahd->sc_channel, scsiprint);

	ahd_unlock(ahd, &s);

	return (1);

}

/*
 * Catch an interrupt from the adapter
 */
int
ahd_platform_intr(void *arg)
{
	struct	ahd_softc *ahd;

	/* XXX in ahc there is some bus_dmamap_sync(PREREAD|PREWRITE); */

	ahd = (struct ahd_softc *)arg; 
	return ahd_intr(ahd);
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahd_done(struct ahd_softc *ahd, struct scb *scb)
{
	struct scsi_xfer *xs = scb->xs;
	int s;

	/* XXX in ahc there is some bus_dmamap_sync(PREREAD|PREWRITE); */

	LIST_REMOVE(scb, pending_links);

	timeout_del(&xs->stimeout);

	if (xs->datalen) {
                int op;

                if ((xs->flags & SCSI_DATA_IN) != 0)
                        op = BUS_DMASYNC_POSTREAD;
                else
                        op = BUS_DMASYNC_POSTWRITE;
                bus_dmamap_sync(ahd->parent_dmat, scb->dmamap, 0,
                                scb->dmamap->dm_mapsize, op);
                bus_dmamap_unload(ahd->parent_dmat, scb->dmamap);
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
		LIST_FOREACH(list_scb, &ahd->pending_scbs, pending_links) {
			struct scsi_xfer *txs = list_scb->xs;
                        if (!(txs->flags & SCSI_POLL))
                                timeout_add(&list_scb->xs->stimeout,
                                    (list_scb->xs->timeout * hz)/1000);
                }

		if (aic_get_transaction_status(scb) != CAM_REQ_INPROG)
                	aic_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		ahd_print_path(ahd, scb);
                printf("%s: no longer in timeout, status = %x\n",
                       ahd_name(ahd), xs->status);
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

	if (xs->error != XS_NOERROR) {
		/* Don't clobber any existing error state */
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * Zero any sense not transferred by the
		 * device.  The SCSI spec mandates that any
		 * untransfered data should be assumed to be
		 * zero.  Complete the 'bounce' of sense information
		 * through buffers accessible via bus-space by
		 * copying it into the clients csio.
		 */
		memset(&xs->sense, 0, sizeof(struct scsi_sense_data));
                memcpy(&xs->sense, ahd_get_sense_buf(ahd, scb),
                    sizeof(struct scsi_sense_data));
                xs->error = XS_SENSE;
#if 0
	} else if ((scb->flags & SCB_PKT_SENSE) != 0) {
		struct scsi_status_iu_header *siu;
		u_int sense_len;
		int i;

		/*
		 * Copy only the sense data into the provided buffer.
		 */
		siu = (struct scsi_status_iu_header *)scb->sense_data;
		sense_len = MIN(scsi_4btoul(siu->sense_length),
				sizeof(struct scsi_sense_data));
		memset(&xs->sense, 0, &xs->sense);
		memcpy(&xs->sense, 
		       ahd_get_sense_buf(ahd, scb) + SIU_SENSE_OFFSET(siu),
		       sense_len);
		printf("Copied %d bytes of sense data offset %d:", sense_len,
		       SIU_SENSE_OFFSET(siu));
		for (i = 0; i < sense_len; i++)
			printf(" 0x%x", ((uint8_t *)&xs->sense)[i]);
		printf("\n");

                xs->error = XS_SENSE;
#endif
	}
#if 0	/* MU: no such settings in ahc */
	if (scb->flags & SCB_REQUEUE)
                xs->error = XS_REQUEUE;
#endif
	ahd_lock(ahd, &s);
	ahd_free_scb(ahd, scb);
	ahd_unlock(ahd, &s);

	xs->flags |= ITSDONE;
        scsi_done(xs);
}

void
ahd_minphys(bp)
        struct buf *bp;
{
        /*
         * Even though the card can transfer up to 16megs per command
         * we are limited by the number of segments in the dma segment
         * list that we can hold.  The worst case is that all pages are
         * discontinuous physically, hence the "page per segment" limit
         * enforced here.
         */
        if (bp->b_bcount > ((AHD_NSEG - 1) * PAGE_SIZE)) {
                bp->b_bcount = ((AHD_NSEG - 1) * PAGE_SIZE);
        }
        minphys(bp);
}

int32_t
ahd_action(struct scsi_xfer *xs)
{
	struct	ahd_softc *ahd;
	struct scb *scb;
	struct hardware_scb *hscb;
	u_int	target_id;
	u_int	our_id;
	int	s;
	int	dontqueue = 0;
	struct	ahd_initiator_tinfo *tinfo;
	struct	ahd_tmode_tstate *tstate;
	u_int	col_idx;
	u_int16_t quirks;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("ahd_action\n"));
	ahd = (struct ahd_softc *)xs->sc_link->adapter_softc;

	/* determine safety of software queueing */
	dontqueue = xs->flags & SCSI_POLL;

	target_id = xs->sc_link->target;
	our_id = SCSI_SCSI_ID(ahd, xs->sc_link);
	
	if ((ahd->flags & AHD_INITIATORROLE) == 0) {
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
		/* return 	ccb->ccb_h.status = CAM_PROVIDE_FAIL; */
	}
	/*
	 * get an scb to use.
	 */
	ahd_lock(ahd, &s);
	tinfo = ahd_fetch_transinfo(ahd, 'A', our_id, target_id, &tstate);

	quirks = xs->sc_link->quirks;

	if ((quirks & SDEV_NOTAGS) == 0 || 
	    (tinfo->curr.ppr_options & MSG_EXT_PPR_PROT_IUS) != 0)
		col_idx = AHD_NEVER_COL_IDX;
	else
		col_idx = AHD_BUILD_COL_IDX(target_id, xs->sc_link->lun);

	if ((scb = ahd_get_scb(ahd, col_idx)) == NULL) {
		ahd->flags |= AHD_RESOURCE_SHORTAGE;
		ahd_unlock(ahd, &s);
		xs->error = XS_DRIVER_STUFFUP;
		return (TRY_AGAIN_LATER);
	}
	ahd_unlock(ahd, &s);
		
	hscb = scb->hscb;
		
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("start scb(%p)\n", scb));

	scb->xs = xs;

	/*
	 * Put all the arguments for the xfer in the scb
	 */
	hscb->control = 0;
	hscb->scsiid = BUILD_SCSIID(ahd, xs->sc_link, target_id, our_id);
	hscb->lun = xs->sc_link->lun;
	if (xs->xs_control & XS_CTL_RESET) {
		hscb->cdb_len = 0;
		scb->flags |= SCB_DEVICE_RESET;
		hscb->control |= MK_MESSAGE;
		hscb->task_management = SIU_TASKMGMT_LUN_RESET;
		ahd_execute_scb(scb, NULL, 0);
	} else {
		hscb->task_management = 0;
	}
#if 0
	if (ccb->ccb_h.flags & CAM_TAG_ACTION_VALID)
		hscb->control |= ccb->csio.tag_action;
#endif
	timeout_set(&xs->stimeout, ahd_timeout, scb);

	return ahd_setup_data(ahd, xs, scb);
}

int
ahd_execute_scb(void *arg, bus_dma_segment_t *dm_segs, int nsegments)
{
	struct	scb *scb;
	struct	scsi_xfer *xs;
	struct	ahd_softc *ahd;
	struct	ahd_initiator_tinfo *tinfo;
	struct	ahd_tmode_tstate *tstate;
	u_int	mask;
	int	s;

	scb = (struct scb *)arg;
	xs = scb->xs;
	xs->error = CAM_REQ_INPROG;
	xs->status = 0;
	ahd = (struct ahd_softc *)xs->sc_link->adapter_softc;

	if (nsegments != 0) {
		void *sg;
		int op;
		u_int i;

		ahd_setup_data_scb(ahd, scb);

		/* Copy the segments into our SG list */
		for (i = nsegments, sg = scb->sg_list; i > 0; i--) {

			sg = ahd_sg_setup(ahd, scb, sg, dm_segs->ds_addr,
					  dm_segs->ds_len,
					  /*last*/i == 1);
			dm_segs++;
		}
		
		if ((xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(ahd->parent_dmat, scb->dmamap, 0,
				scb->dmamap->dm_mapsize, op);

	}

	ahd_lock(ahd, &s);

	/*
	 * Last time we need to check if this SCB needs to
	 * be aborted.
	 */
	if (xs->flags & ITSDONE) {
		if (nsegments != 0)
			bus_dmamap_unload(ahd->parent_dmat,
					  scb->dmamap);
		ahd_free_scb(ahd, scb);
		ahd_unlock(ahd, &s);
		return (COMPLETE);
	}

	tinfo = ahd_fetch_transinfo(ahd, SCSIID_CHANNEL(ahd, scb->hscb->scsiid),
				    SCSIID_OUR_ID(scb->hscb->scsiid),
				    SCSIID_TARGET(ahd, scb->hscb->scsiid),
				    &tstate);

	mask = SCB_GET_TARGET_MASK(ahd, scb);

	if ((tstate->discenable & mask) != 0)
		scb->hscb->control |= DISCENB;

	if ((tstate->tagenable & mask) != 0)
		scb->hscb->control |= TAG_ENB;

	/* disable tags and disconection to work around overlapped commands 
	 * for non U320 devices
	 */
	if ((tinfo->curr.ppr_options & MSG_EXT_PPR_PROT_IUS) == 0) {
		scb->hscb->control &= ~TAG_ENB;
		scb->hscb->control &= ~DISCENB;
	}

	if ((tinfo->curr.ppr_options & MSG_EXT_PPR_PROT_IUS) != 0) {
		scb->flags |= SCB_PACKETIZED;
		if (scb->hscb->task_management != 0)
			scb->hscb->control &= ~MK_MESSAGE;
	}

	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		scb->hscb->control |= MK_MESSAGE;
	}

	/* XXX with ahc there was some bus_dmamap_sync(PREREAD|PREWRITE); */

	LIST_INSERT_HEAD(&ahd->pending_scbs, scb, pending_links);

	if (!(xs->flags & SCSI_POLL))
		timeout_add(&xs->stimeout, (xs->timeout * hz) / 1000);

	scb->flags |= SCB_ACTIVE;

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		/* Define a mapping from our tag to the SCB. */
		ahd->scb_data.scbindex[SCB_GET_TAG(scb)] = scb;
		ahd_pause(ahd);
		ahd_set_scbptr(ahd, SCB_GET_TAG(scb));
		ahd_outb(ahd, RETURN_1, CONT_MSG_LOOP_TARG);
		ahd_unpause(ahd);
	} else {
		ahd_queue_scb(ahd, scb);
	}

	if (!(xs->flags & SCSI_POLL)) {
		int target = xs->sc_link->target;
		int lun = SCB_GET_LUN(scb);

		if (ahd->inited_target[target] == 0) {
			struct  ahd_devinfo devinfo;

			ahd_adapter_req_set_xfer_mode(ahd, scb);
			ahd_compile_devinfo(&devinfo, ahd->our_id, target, lun,
			    'A', /*XXX milos*/ROLE_UNKNOWN);
			ahd_scb_devinfo(ahd, &devinfo, scb);
			ahd_update_neg_request(ahd, &devinfo, tstate, tinfo,
				AHD_NEG_IF_NON_ASYNC);
									                        	ahd->inited_target[target] = 1;
		}

		ahd_unlock(ahd, &s);
		return (SUCCESSFULLY_QUEUED);
	}

	/*
	 * If we can't use interrupts, poll for completion
	 */
	SC_DEBUG(xs->sc_link, SDEV_DB3, ("cmd_poll\n"));

	do {
		if (ahd_poll(ahd, xs->timeout)) {
			if (!(xs->flags & SCSI_SILENT))
				printf("cmd fail\n");
			ahd_timeout(scb);
			break;
		}
	} while (!(xs->flags & ITSDONE));

	ahd_unlock(ahd, &s);
	return (COMPLETE);
}

int
ahd_poll(struct ahd_softc *ahd, int wait)
{
	while (--wait) {
		DELAY(1000);
		if (ahd_inb(ahd, INTSTAT) & INT_PEND)
			break;
	}

	if (wait == 0) {
		printf("%s: board is not responding\n", ahd_name(ahd));
		return (EIO);
	}

	ahd_intr((void *)ahd);
	return (0);
}

int
ahd_setup_data(struct ahd_softc *ahd, struct scsi_xfer *xs,
	       struct scb *scb)
{
	struct hardware_scb *hscb;
	
	hscb = scb->hscb;
	xs->resid = xs->status = 0;
	xs->error = CAM_REQ_INPROG;
	
	hscb->cdb_len = xs->cmdlen;
	if (hscb->cdb_len > MAX_CDB_LEN) {
		int s;
		
		aic_set_transaction_status(scb, CAM_REQ_INVALID);
		ahd_lock(ahd, &s);
		ahd_free_scb(ahd, scb);
		ahd_unlock(ahd, &s);
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return (COMPLETE);
	}

	memcpy(hscb->shared_data.idata.cdb, xs->cmd, hscb->cdb_len);
		
	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

		error = bus_dmamap_load(ahd->parent_dmat,
					scb->dmamap, xs->data,
                                        xs->datalen, NULL,
                                        ((xs->flags & SCSI_NOSLEEP) ?
                                         BUS_DMA_NOWAIT : BUS_DMA_WAITOK) |
                                        BUS_DMA_STREAMING |
                                        ((xs->flags & XS_CTL_DATA_IN) ?
                                         BUS_DMA_READ : BUS_DMA_WRITE));
		if (error) {
#ifdef AHD_DEBUG
                        printf("%s: in ahd_setup_data(): bus_dmamap_load() "
                               "= %d\n",
                               ahd_name(ahd), error);
#endif
			xs->error = XS_BUSY;
                        xs->flags |= ITSDONE;
                        scsi_done(xs);
                        return (TRY_AGAIN_LATER);       /* XXX fvdl */
		}
		error = ahd_execute_scb(scb,
				scb->dmamap->dm_segs,
                                scb->dmamap->dm_nsegs);
		return error;
	} else {
		return ahd_execute_scb(scb, NULL, 0);
	}
}

void
ahd_platform_set_tags(struct ahd_softc *ahd,
		      struct ahd_devinfo *devinfo, ahd_queue_alg alg)
{
	struct ahd_tmode_tstate *tstate;

        ahd_fetch_transinfo(ahd, devinfo->channel, devinfo->our_scsiid,
                            devinfo->target, &tstate);

        if (alg != AHD_QUEUE_NONE) 
                tstate->tagenable |= devinfo->target_mask;
        else
                tstate->tagenable &= ~devinfo->target_mask;
}

int
ahd_platform_alloc(struct ahd_softc *ahd, void *platform_arg)
{
	ahd->platform_data = malloc(sizeof(struct ahd_platform_data), M_DEVBUF,
	    M_NOWAIT /*| M_ZERO*/);
	if (ahd->platform_data == NULL)
		return (ENOMEM);

	memset(ahd->platform_data, 0, sizeof(struct ahd_platform_data));

	return (0);
}

void
ahd_platform_free(struct ahd_softc *ahd)
{
	free(ahd->platform_data, M_DEVBUF);
}

int
ahd_softc_comp(struct ahd_softc *lahd, struct ahd_softc *rahd)
{
	/* We don't sort softcs under OpenBSD so report equal always */
	return (0);
}

int
ahd_detach(struct device *self, int flags)
{
	int rv = 0;

        struct ahd_softc *ahd = (struct ahd_softc*)self;

        if (ahd->sc_child != NULL)
                rv = config_detach((void *)ahd->sc_child, flags);

        shutdownhook_disestablish(ahd->shutdown_hook);

        ahd_free(ahd);

        return rv;
}

void
ahd_adapter_req_set_xfer_mode(struct ahd_softc *ahd, struct scb *scb)
{
	struct ahd_initiator_tinfo *tinfo;
	struct ahd_tmode_tstate *tstate;
	int target_id, our_id;
	struct ahd_devinfo devinfo;
	u_int16_t quirks;
	u_int width, ppr_options, period, offset;
	int s;

	target_id = scb->xs->sc_link->target;
	our_id = SCSI_SCSI_ID(ahd, scb->xs->sc_link);

	s = splbio();

	quirks = scb->xs->sc_link->quirks;
	tinfo = ahd_fetch_transinfo(ahd, 'A', our_id, target_id, &tstate);
	ahd_compile_devinfo(&devinfo, our_id, target_id, 0, 'A',
	    ROLE_INITIATOR);

	tstate->discenable |= (ahd->user_discenable & devinfo.target_mask);

	if (quirks & SDEV_NOTAGS)
		tstate->tagenable &= ~devinfo.target_mask;
	else if (ahd->user_tagenable & devinfo.target_mask)
		tstate->tagenable |= devinfo.target_mask;

	if (quirks & SDEV_NOWIDE)
		width = MSG_EXT_WDTR_BUS_8_BIT;
	else
		width = MSG_EXT_WDTR_BUS_16_BIT;

	ahd_validate_width(ahd, NULL, &width, ROLE_UNKNOWN);
	if (width > tinfo->user.width)
		width = tinfo->user.width;
	ahd_set_width(ahd, &devinfo, width, AHD_TRANS_GOAL, FALSE);

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

	ahd_find_syncrate(ahd, &period, &ppr_options, AHD_SYNCRATE_MAX);
	ahd_validate_offset(ahd, NULL, period, &offset, width, ROLE_UNKNOWN);

	if (offset == 0) {
		period = 0;
		ppr_options = 0;
	}

	if (ppr_options != 0 && tinfo->user.transport_version >= 3) {
		tinfo->goal.transport_version = tinfo->user.transport_version;
		tinfo->curr.transport_version = tinfo->user.transport_version;
	}

	ahd_set_syncrate(ahd, &devinfo, period, offset, ppr_options,
		AHD_TRANS_GOAL, FALSE);

	splx(s);
}

void
aic_scb_timer_reset(struct scb *scb, u_int usec)
{
	if (!(scb->xs->xs_control & XS_CTL_POLL)) {
		callout_reset(&scb->xs->xs_callout,
		    (usec * hz)/1000000, ahd_timeout, scb);
	}
}

void
ahd_flush_device_writes(struct ahd_softc *ahd)
{
	/* XXX Is this sufficient for all architectures??? */
	ahd_inb(ahd, INTSTAT);
}

void
aic_platform_scb_free(struct ahd_softc *ahd, struct scb *scb)
{
	int s;

	ahd_lock(ahd, &s);

	if ((ahd->flags & AHD_RESOURCE_SHORTAGE) != 0 ||
	    (scb->flags & SCB_RECOVERY_SCB) != 0) {
		ahd->flags &= ~AHD_RESOURCE_SHORTAGE;
	}

	if (!cold) {
		/* we are no longer in autoconf */
		timeout_del(&scb->xs->stimeout);
	}

	ahd_unlock(ahd, &s);
}

void
ahd_print_path(struct ahd_softc *ahd, struct scb *scb)
{
	sc_print_addr(scb->xs->sc_link);
}

void
ahd_platform_dump_card_state(struct ahd_softc *ahd)
{
	/* Nothing to do here for OpenBSD */
	printf("FEATURES = 0x%x, FLAGS = 0x%x, CHIP = 0x%x BUGS =0x%x\n",
		ahd->features, ahd->flags, ahd->chip, ahd->bugs);
}

void
ahd_platform_flushwork(struct ahd_softc *ahd)
{
}


