/*	$OpenBSD: adw.c,v 1.1 1998/11/17 06:14:58 downsj Exp $	*/
/* $NetBSD: adw.c,v 1.3 1998/10/10 00:28:33 thorpej Exp $	 */

/*
 * Generic driver for the Advanced Systems Inc. SCSI controllers
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/adwlib.h>
#include <dev/ic/adw.h>

#ifndef DDB
#define	Debugger()	panic("should call debugger here (adv.c)")
#endif				/* ! DDB */

/******************************************************************************/


static void adw_enqueue __P((ADW_SOFTC *, struct scsi_xfer *, int));
static struct scsi_xfer *adw_dequeue __P((ADW_SOFTC *));

static int adw_alloc_ccbs __P((ADW_SOFTC *));
static int adw_create_ccbs __P((ADW_SOFTC *, ADW_CCB *, int));
static void adw_free_ccb __P((ADW_SOFTC *, ADW_CCB *));
static void adw_reset_ccb __P((ADW_CCB *));
static int adw_init_ccb __P((ADW_SOFTC *, ADW_CCB *));
static ADW_CCB *adw_get_ccb __P((ADW_SOFTC *, int));
static void adw_queue_ccb __P((ADW_SOFTC *, ADW_CCB *));
static void adw_start_ccbs __P((ADW_SOFTC *));

static int adw_scsi_cmd __P((struct scsi_xfer *));
static int adw_build_req __P((struct scsi_xfer *, ADW_CCB *));
static void adw_build_sglist __P((ADW_CCB *, ADW_SCSI_REQ_Q *));
static void adwminphys __P((struct buf *));
static void adw_wide_isr_callback __P((ADW_SOFTC *, ADW_SCSI_REQ_Q *));

static int adw_poll __P((ADW_SOFTC *, struct scsi_xfer *, int));
static void adw_timeout __P((void *));
static void adw_watchdog __P((void *));


/******************************************************************************/


struct cfdriver adw_cd = {
	NULL, "adw", DV_DULL
};


struct scsi_adapter adw_switch =
{
	adw_scsi_cmd,		/* called to start/enqueue a SCSI command */
	adwminphys,		/* to limit the transfer to max device can do */
	0,
	0,
};


/* the below structure is so we have a default dev struct for out link struct */
struct scsi_device adw_dev =
{
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};


#define ADW_ABORT_TIMEOUT       10000	/* time to wait for abort (mSec) */
#define ADW_WATCH_TIMEOUT       10000	/* time to wait for watchdog (mSec) */


/******************************************************************************/
/* scsi_xfer queue routines                      */
/******************************************************************************/

/*
 * Insert a scsi_xfer into the software queue.  We overload xs->free_list
 * to avoid having to allocate additional resources (since we're used
 * only during resource shortages anyhow.
 */
static void
adw_enqueue(sc, xs, infront)
	ADW_SOFTC      *sc;
	struct scsi_xfer *xs;
	int             infront;
{

	if (infront || sc->sc_queue.lh_first == NULL) {
		if (sc->sc_queue.lh_first == NULL)
			sc->sc_queuelast = xs;
		LIST_INSERT_HEAD(&sc->sc_queue, xs, free_list);
		return;
	}
	LIST_INSERT_AFTER(sc->sc_queuelast, xs, free_list);
	sc->sc_queuelast = xs;
}


/*
 * Pull a scsi_xfer off the front of the software queue.
 */
static struct scsi_xfer *
adw_dequeue(sc)
	ADW_SOFTC      *sc;
{
	struct scsi_xfer *xs;

	xs = sc->sc_queue.lh_first;
	LIST_REMOVE(xs, free_list);

	if (sc->sc_queue.lh_first == NULL)
		sc->sc_queuelast = NULL;

	return (xs);
}


/******************************************************************************/
/* Control Blocks routines                        */
/******************************************************************************/


static int
adw_alloc_ccbs(sc)
	ADW_SOFTC      *sc;
{
	bus_dma_segment_t seg;
	int             error, rseg;

	/*
         * Allocate the control blocks.
         */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct adw_control),
			   NBPG, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate control structures,"
		       " error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
		   sizeof(struct adw_control), (caddr_t *) & sc->sc_control,
				 BUS_DMA_NOWAIT | BUS_DMAMEM_NOSYNC)) != 0) {
		printf("%s: unable to map control structures, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
	/*
         * Create and load the DMA map used for the control blocks.
         */
	if ((error = bus_dmamap_create(sc->sc_dmat, sizeof(struct adw_control),
			   1, sizeof(struct adw_control), 0, BUS_DMA_NOWAIT,
				       &sc->sc_dmamap_control)) != 0) {
		printf("%s: unable to create control DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_control,
			   sc->sc_control, sizeof(struct adw_control), NULL,
				     BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load control DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
	return (0);
}


/*
 * Create a set of ccbs and add them to the free list.  Called once
 * by adw_init().  We return the number of CCBs successfully created.
 */
static int
adw_create_ccbs(sc, ccbstore, count)
	ADW_SOFTC      *sc;
	ADW_CCB        *ccbstore;
	int             count;
{
	ADW_CCB        *ccb;
	int             i, error;

	bzero(ccbstore, sizeof(ADW_CCB) * count);
	for (i = 0; i < count; i++) {
		ccb = &ccbstore[i];
		if ((error = adw_init_ccb(sc, ccb)) != 0) {
			printf("%s: unable to initialize ccb, error = %d\n",
			       sc->sc_dev.dv_xname, error);
			return (i);
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_ccb, ccb, chain);
	}

	return (i);
}


/*
 * A ccb is put onto the free list.
 */
static void
adw_free_ccb(sc, ccb)
	ADW_SOFTC      *sc;
	ADW_CCB        *ccb;
{
	int             s;

	s = splbio();

	adw_reset_ccb(ccb);
	TAILQ_INSERT_HEAD(&sc->sc_free_ccb, ccb, chain);

	/*
         * If there were none, wake anybody waiting for one to come free,
         * starting with queued entries.
         */
	if (ccb->chain.tqe_next == 0)
		wakeup(&sc->sc_free_ccb);

	splx(s);
}


static void
adw_reset_ccb(ccb)
	ADW_CCB        *ccb;
{

	ccb->flags = 0;
}


static int
adw_init_ccb(sc, ccb)
	ADW_SOFTC      *sc;
	ADW_CCB        *ccb;
{
	int             error;

	/*
         * Create the DMA map for this CCB.
         */
	error = bus_dmamap_create(sc->sc_dmat,
				  (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
			 ADW_MAX_SG_LIST, (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
		   0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		printf("%s: unable to create DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
	adw_reset_ccb(ccb);
	return (0);
}


/*
 * Get a free ccb
 *
 * If there are none, see if we can allocate a new one
 */
static ADW_CCB *
adw_get_ccb(sc, flags)
	ADW_SOFTC      *sc;
	int             flags;
{
	ADW_CCB        *ccb = 0;
	int             s;

	s = splbio();

	/*
         * If we can and have to, sleep waiting for one to come free
         * but only if we can't allocate a new one.
         */
	for (;;) {
		ccb = sc->sc_free_ccb.tqh_first;
		if (ccb) {
			TAILQ_REMOVE(&sc->sc_free_ccb, ccb, chain);
			break;
		}
		if ((flags & SCSI_NOSLEEP) != 0)
			goto out;

		tsleep(&sc->sc_free_ccb, PRIBIO, "adwccb", 0);
	}

	ccb->flags |= CCB_ALLOC;

out:
	splx(s);
	return (ccb);
}


/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
static void
adw_queue_ccb(sc, ccb)
	ADW_SOFTC      *sc;
	ADW_CCB        *ccb;
{

	TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);

	adw_start_ccbs(sc);
}


static void
adw_start_ccbs(sc)
	ADW_SOFTC      *sc;
{
	ADW_CCB        *ccb;

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {
		if (ccb->flags & CCB_WATCHDOG)
			untimeout(adw_watchdog, ccb);

		if (AdvExeScsiQueue(sc, &ccb->scsiq) == ADW_BUSY) {
			ccb->flags |= CCB_WATCHDOG;
			timeout(adw_watchdog, ccb,
				(ADW_WATCH_TIMEOUT * hz) / 1000);
			break;
		}
		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(adw_timeout, ccb, (ccb->timeout * hz) / 1000);
	}
}


/******************************************************************************/
/* SCSI layer interfacing routines                    */
/******************************************************************************/


int
adw_init(sc)
	ADW_SOFTC      *sc;
{
	u_int16_t       warn_code;


	sc->cfg.lib_version = (ADW_LIB_VERSION_MAJOR << 8) |
		ADW_LIB_VERSION_MINOR;
	sc->cfg.chip_version =
		ADW_GET_CHIP_VERSION(sc->sc_iot, sc->sc_ioh, sc->bus_type);

	/*
	 * Reset the chip to start and allow register writes.
	 */
	if (ADW_FIND_SIGNATURE(sc->sc_iot, sc->sc_ioh) == 0) {
		panic("adw_init: adw_find_signature failed");
	} else {
		AdvResetChip(sc->sc_iot, sc->sc_ioh);

		warn_code = AdvInitFromEEP(sc);
		if (warn_code & ASC_WARN_EEPROM_CHKSUM)
			printf("%s: Bad checksum found. "
			       "Setting default values\n",
			       sc->sc_dev.dv_xname);
		if (warn_code & ASC_WARN_EEPROM_TERMINATION)
			printf("%s: Bad bus termination setting."
			       "Using automatic termination.\n",
			       sc->sc_dev.dv_xname);

		/*
		 * Reset the SCSI Bus if the EEPROM indicates that SCSI Bus
		 * Resets should be performed.
		 */
		if (sc->bios_ctrl & BIOS_CTRL_RESET_SCSI_BUS)
			AdvResetSCSIBus(sc);
	}

	sc->isr_callback = (ulong) adw_wide_isr_callback;

	return (0);
}


void
adw_attach(sc)
	ADW_SOFTC      *sc;
{
	int             i, error;


	/*
	 * Initialize the ASC3550.
	 */
	switch (AdvInitAsc3550Driver(sc)) {
	case ASC_IERR_MCODE_CHKSUM:
		panic("%s: Microcode checksum error",
		      sc->sc_dev.dv_xname);
		break;

	case ASC_IERR_ILLEGAL_CONNECTION:
		panic("%s: All three connectors are in use",
		      sc->sc_dev.dv_xname);
		break;

	case ASC_IERR_REVERSED_CABLE:
		panic("%s: Cable is reversed",
		      sc->sc_dev.dv_xname);
		break;

	case ASC_IERR_SINGLE_END_DEVICE:
		panic("%s: single-ended device is attached to"
		      " one of the connectors",
		      sc->sc_dev.dv_xname);
		break;
	}


	/*
         * fill in the prototype scsi_link.
         */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->chip_scsi_id;
	sc->sc_link.adapter = &adw_switch;
	sc->sc_link.device = &adw_dev;
	sc->sc_link.openings = 4;
	sc->sc_link.adapter_buswidth = ADW_MAX_TID;


	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);
	LIST_INIT(&sc->sc_queue);


	/*
         * Allocate the Control Blocks.
         */
	error = adw_alloc_ccbs(sc);
	if (error)
		return; /* (error) */ ;

	/*
	 * Create and initialize the Control Blocks.
	 */
	i = adw_create_ccbs(sc, sc->sc_control->ccbs, ADW_MAX_CCB);
	if (i == 0) {
		printf("%s: unable to create control blocks\n",
		       sc->sc_dev.dv_xname);
		return; /* (ENOMEM) */ ;
	} else if (i != ADW_MAX_CCB) {
		printf("%s: WARNING: only %d of %d control blocks"
		       " created\n",
		       sc->sc_dev.dv_xname, i, ADW_MAX_CCB);
	}
	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);
}


static void
adwminphys(bp)
	struct buf     *bp;
{

	if (bp->b_bcount > ((ADW_MAX_SG_LIST - 1) * PAGE_SIZE))
		bp->b_bcount = ((ADW_MAX_SG_LIST - 1) * PAGE_SIZE);
	minphys(bp);
}


/*
 * start a scsi operation given the command and the data address.
 * Also needs the unit, target and lu.
 */
static int
adw_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	ADW_SOFTC      *sc = sc_link->adapter_softc;
	ADW_CCB        *ccb;
	int             s, fromqueue = 1, dontqueue = 0;

	s = splbio();		/* protect the queue */

	/*
         * If we're running the queue from adw_done(), we've been
         * called with the first queue entry as our argument.
         */
	if (xs == sc->sc_queue.lh_first) {
		xs = adw_dequeue(sc);
		fromqueue = 1;
	} else {

		/* Polled requests can't be queued for later. */
		dontqueue = xs->flags & SCSI_POLL;

		/*
                 * If there are jobs in the queue, run them first.
                 */
		if (sc->sc_queue.lh_first != NULL) {
			/*
                         * If we can't queue, we have to abort, since
                         * we have to preserve order.
                         */
			if (dontqueue) {
				splx(s);
				xs->error = XS_DRIVER_STUFFUP;
				return (TRY_AGAIN_LATER);
			}
			/*
                         * Swap with the first queue entry.
                         */
			adw_enqueue(sc, xs, 0);
			xs = adw_dequeue(sc);
			fromqueue = 1;
		}
	}


	/*
         * get a ccb to use. If the transfer
         * is from a buf (possibly from interrupt time)
         * then we can't allow it to sleep
         */

	if ((ccb = adw_get_ccb(sc, xs->flags)) == NULL) {
		/*
                 * If we can't queue, we lose.
                 */
		if (dontqueue) {
			splx(s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}
		/*
                 * Stuff ourselves into the queue, in front
                 * if we came off in the first place.
                 */
		adw_enqueue(sc, xs, fromqueue);
		splx(s);
		return (SUCCESSFULLY_QUEUED);
	}
	splx(s);		/* done playing with the queue */

	ccb->xs = xs;
	ccb->timeout = xs->timeout;

	if (adw_build_req(xs, ccb)) {
		s = splbio();
		adw_queue_ccb(sc, ccb);
		splx(s);

		/*
	         * Usually return SUCCESSFULLY QUEUED
	         */
		if ((xs->flags & SCSI_POLL) == 0)
			return (SUCCESSFULLY_QUEUED);

		/*
	         * If we can't use interrupts, poll on completion
	         */
		if (adw_poll(sc, xs, ccb->timeout)) {
			adw_timeout(ccb);
			if (adw_poll(sc, xs, ccb->timeout))
				adw_timeout(ccb);
		}
	}
	return (COMPLETE);
}


/*
 * Build a request structure for the Wide Boards.
 */
static int
adw_build_req(xs, ccb)
	struct scsi_xfer *xs;
	ADW_CCB        *ccb;
{
	struct scsi_link *sc_link = xs->sc_link;
	ADW_SOFTC      *sc = sc_link->adapter_softc;
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_SCSI_REQ_Q *scsiqp;
	int             error;

	scsiqp = &ccb->scsiq;
	bzero(scsiqp, sizeof(ADW_SCSI_REQ_Q));

	/*
	 * Set the ADW_SCSI_REQ_Q 'ccb_ptr' to point to the CCB structure.
	 */
	scsiqp->ccb_ptr = (ulong) ccb;


	/*
	 * Build the ADW_SCSI_REQ_Q request.
	 */

	/*
	 * Set CDB length and copy it to the request structure.
	 */
	bcopy(xs->cmd, &scsiqp->cdb, scsiqp->cdb_len = xs->cmdlen);

	scsiqp->target_id = sc_link->target;
	scsiqp->target_lun = sc_link->lun;

	scsiqp->vsense_addr = (ulong) & ccb->scsi_sense;
	scsiqp->sense_addr = sc->sc_dmamap_control->dm_segs[0].ds_addr +
		ADW_CCB_OFF(ccb) + offsetof(struct adw_ccb, scsi_sense);
	scsiqp->sense_len = sizeof(struct scsi_sense_data);

	/*
	 * Build ADW_SCSI_REQ_Q for a scatter-gather buffer command.
	 */
	if (xs->datalen) {
		/*
                 * Map the DMA transfer.
                 */
#ifdef TFS
		if (xs->flags & SCSI_DATA_UIO) {
			error = bus_dmamap_load_uio(dmat,
				ccb->dmamap_xfer, (struct uio *) xs->data,
				(xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
					BUS_DMA_WAITOK);
		} else
#endif				/* TFS */
		{
			error = bus_dmamap_load(dmat,
			      ccb->dmamap_xfer, xs->data, xs->datalen, NULL,
				(xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
					BUS_DMA_WAITOK);
		}

		if (error) {
			if (error == EFBIG) {
				printf("%s: adw_scsi_cmd, more than %d dma"
				       " segments\n",
				       sc->sc_dev.dv_xname, ADW_MAX_SG_LIST);
			} else {
				printf("%s: adw_scsi_cmd, error %d loading"
				       " dma map\n",
				       sc->sc_dev.dv_xname, error);
			}

			xs->error = XS_DRIVER_STUFFUP;
			adw_free_ccb(sc, ccb);
			return (0);
		}
		bus_dmamap_sync(dmat, ccb->dmamap_xfer,
			  (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
				BUS_DMASYNC_PREWRITE);

		/*
		 * Build scatter-gather list.
		 */
		scsiqp->data_cnt = xs->datalen;
		scsiqp->vdata_addr = (ulong) xs->data;
		scsiqp->data_addr = ccb->dmamap_xfer->dm_segs[0].ds_addr;
		scsiqp->sg_list_ptr = &ccb->sg_block[0];
		bzero(scsiqp->sg_list_ptr,
				sizeof(ADW_SG_BLOCK) * ADW_NUM_SG_BLOCK);
		adw_build_sglist(ccb, scsiqp);
	} else {
		/*
                 * No data xfer, use non S/G values.
                 */
		scsiqp->data_cnt = 0;
		scsiqp->vdata_addr = 0;
		scsiqp->data_addr = 0;
		scsiqp->sg_list_ptr = NULL;
	}

	return (1);
}


/*
 * Build scatter-gather list for Wide Boards.
 */
static void
adw_build_sglist(ccb, scsiqp)
	ADW_CCB        *ccb;
	ADW_SCSI_REQ_Q *scsiqp;
{
	struct scsi_xfer *xs = ccb->xs;
	ADW_SOFTC      *sc = xs->sc_link->adapter_softc;
	ADW_SG_BLOCK   *sg_block = scsiqp->sg_list_ptr;
	ulong           sg_block_next_addr;	/* block and its next */
	ulong           sg_block_physical_addr;
	int             sg_block_index, i;	/* how many SG entries */
	bus_dma_segment_t *sg_list = &ccb->dmamap_xfer->dm_segs[0];
	int             sg_elem_cnt = ccb->dmamap_xfer->dm_nsegs;


	sg_block_next_addr = (ulong) sg_block;	/* allow math operation */
	sg_block_physical_addr = sc->sc_dmamap_control->dm_segs[0].ds_addr +
		ADW_CCB_OFF(ccb) + offsetof(struct adw_ccb, sg_block[0]);
	scsiqp->sg_real_addr = sg_block_physical_addr;

	/*
	 * If there are more than NO_OF_SG_PER_BLOCK dma segments (hw sg-list)
	 * then split the request into multiple sg-list blocks.
	 */

	sg_block_index = 0;
	do {
		sg_block->first_entry_no = sg_block_index;
		for (i = 0; i < NO_OF_SG_PER_BLOCK; i++) {
			sg_block->sg_list[i].sg_addr = sg_list->ds_addr;
			sg_block->sg_list[i].sg_count = sg_list->ds_len;

			if (--sg_elem_cnt == 0) {
				/* last entry, get out */
				scsiqp->sg_entry_cnt = sg_block_index + i + 1;
				sg_block->last_entry_no = sg_block_index + i;
				sg_block->sg_ptr = NULL; /* next link = NULL */
				return;
			}
			sg_list++;
		}
		sg_block_next_addr += sizeof(ADW_SG_BLOCK);
		sg_block_physical_addr += sizeof(ADW_SG_BLOCK);

		sg_block_index += NO_OF_SG_PER_BLOCK;
		sg_block->sg_ptr = (ADW_SG_BLOCK *) sg_block_physical_addr;
		sg_block->last_entry_no = sg_block_index - 1;
		sg_block = (ADW_SG_BLOCK *) sg_block_next_addr;	/* virt. addr */
	}
	while (1);
}


int
adw_intr(arg)
	void           *arg;
{
	ADW_SOFTC      *sc = arg;
	struct scsi_xfer *xs;


	AdvISR(sc);

	/*
         * If there are queue entries in the software queue, try to
         * run the first one.  We should be more or less guaranteed
         * to succeed, since we just freed a CCB.
         *
         * NOTE: adw_scsi_cmd() relies on our calling it with
         * the first entry in the queue.
         */
	if ((xs = sc->sc_queue.lh_first) != NULL)
		(void) adw_scsi_cmd(xs);

	return (1);
}


/*
 * Poll a particular unit, looking for a particular xs
 */
static int
adw_poll(sc, xs, count)
	ADW_SOFTC      *sc;
	struct scsi_xfer *xs;
	int             count;
{

	/* timeouts are in msec, so we loop in 1000 usec cycles */
	while (count) {
		adw_intr(sc);
		if (xs->flags & ITSDONE)
			return (0);
		delay(1000);	/* only happens in boot so ok */
		count--;
	}
	return (1);
}


static void
adw_timeout(arg)
	void           *arg;
{
	ADW_CCB        *ccb = arg;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	ADW_SOFTC      *sc = sc_link->adapter_softc;
	int             s;

	sc_print_addr(sc_link);
	printf("timed out");

	s = splbio();

	/*
         * If it has been through before, then a previous abort has failed,
         * don't try abort again, reset the bus instead.
         */
	if (ccb->flags & CCB_ABORT) {
		/* abort timed out */
		printf(" AGAIN. Resetting Bus\n");
		/* Lets try resetting the bus! */
		AdvResetSCSIBus(sc);
		ccb->timeout = ADW_ABORT_TIMEOUT;
		adw_queue_ccb(sc, ccb);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		ADW_ABORT_CCB(sc, ccb);
		xs->error = XS_TIMEOUT;
		ccb->timeout = ADW_ABORT_TIMEOUT;
		ccb->flags |= CCB_ABORT;
		adw_queue_ccb(sc, ccb);
	}

	splx(s);
}


static void
adw_watchdog(arg)
	void           *arg;
{
	ADW_CCB        *ccb = arg;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	ADW_SOFTC      *sc = sc_link->adapter_softc;
	int             s;

	s = splbio();

	ccb->flags &= ~CCB_WATCHDOG;
	adw_start_ccbs(sc);

	splx(s);
}


/******************************************************************************/
/* NARROW and WIDE boards Interrupt callbacks                */
/******************************************************************************/


/*
 * adw_wide_isr_callback() - Second Level Interrupt Handler called by AdvISR()
 *
 * Interrupt callback function for the Wide SCSI Adv Library.
 */
static void
adw_wide_isr_callback(sc, scsiq)
	ADW_SOFTC      *sc;
	ADW_SCSI_REQ_Q *scsiq;
{
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_CCB        *ccb = (ADW_CCB *) scsiq->ccb_ptr;
	struct scsi_xfer *xs = ccb->xs;
	struct scsi_sense_data *s1, *s2;
	//int           underrun = ASC_FALSE;


	untimeout(adw_timeout, ccb);

	/*
         * If we were a data transfer, unload the map that described
         * the data buffer.
         */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->dmamap_xfer,
			 (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
				BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ccb->dmamap_xfer);
	}
	if ((ccb->flags & CCB_ALLOC) == 0) {
		printf("%s: exiting ccb not allocated!\n", sc->sc_dev.dv_xname);
		Debugger();
		return;
	}
	/*
	 * Check for an underrun condition.
	 */
	/*
	 * if (xs->request_bufflen != 0 && scsiqp->data_cnt != 0) {
	 * ASC_DBG1(1, "adw_isr_callback: underrun condition %lu bytes\n",
	 * scsiqp->data_cnt); underrun = ASC_TRUE; }
	 */
	/*
	 * 'done_status' contains the command's ending status.
	 */
	switch (scsiq->done_status) {
	case QD_NO_ERROR:
		switch (scsiq->host_status) {
		case QHSTA_NO_ERROR:
			xs->error = XS_NOERROR;
			xs->resid = 0;
			break;
		default:
			/* QHSTA error occurred. */
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/*
		 * If there was an underrun without any other error,
		 * set DID_ERROR to indicate the underrun error.
		 *
		 * Note: There is no way yet to indicate the number
		 * of underrun bytes.
		 */
		/*
		 * if (xs->error == XS_NOERROR && underrun == ASC_TRUE) {
		 * scp->result = HOST_BYTE(DID_UNDERRUN); }
		 */ break;

	case QD_WITH_ERROR:
		switch (scsiq->host_status) {
		case QHSTA_NO_ERROR:
			if (scsiq->scsi_status == SS_CHK_CONDITION) {
				s1 = &ccb->scsi_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
			} else {
				xs->error = XS_DRIVER_STUFFUP;
			}
			break;

		default:
			/* Some other QHSTA error occurred. */
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case QD_ABORTED_BY_HOST:
	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}


	adw_free_ccb(sc, ccb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}
