/* $NetBSD: adw.c,v 1.14 2000/02/12 19:19:42 thorpej Exp $	*/

/*
 * Generic driver for the Advanced Systems Inc. SCSI controllers
 *
 * Copyright (c) 1998, 1999, 2000 The NetBSD Foundation, Inc.
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
#define	Debugger()	panic("should call debugger here (adw.c)")
#endif				/* ! DDB */

/******************************************************************************/


static void adw_enqueue __P((ADW_SOFTC *, struct scsi_xfer *, int));
static struct scsi_xfer *adw_dequeue __P((ADW_SOFTC *));

static int adw_alloc_controls __P((ADW_SOFTC *));
static int adw_alloc_carriers __P((ADW_SOFTC *));
static int adw_create_carriers __P((ADW_SOFTC *));
static int adw_init_carrier __P((ADW_SOFTC *, ADW_CARRIER *));
static int adw_create_ccbs __P((ADW_SOFTC *, ADW_CCB *, int));
static void adw_free_ccb __P((ADW_SOFTC *, ADW_CCB *));
static void adw_reset_ccb __P((ADW_CCB *));
static int adw_init_ccb __P((ADW_SOFTC *, ADW_CCB *));
static ADW_CCB *adw_get_ccb __P((ADW_SOFTC *, int));
static int adw_queue_ccb __P((ADW_SOFTC *, ADW_CCB *, int));

static int adw_scsi_cmd __P((struct scsi_xfer *));
static int adw_build_req __P((struct scsi_xfer *, ADW_CCB *, int));
static void adw_build_sglist __P((ADW_CCB *, ADW_SCSI_REQ_Q *, ADW_SG_BLOCK *));
static void adwminphys __P((struct buf *));
static void adw_isr_callback __P((ADW_SOFTC *, ADW_SCSI_REQ_Q *));
static void adw_async_callback __P((ADW_SOFTC *, u_int8_t));

static int adw_poll __P((ADW_SOFTC *, struct scsi_xfer *, int));
static void adw_timeout __P((void *));


/******************************************************************************/


struct cfdriver adw_cd = {
	NULL, "adw", DV_DULL
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
/* scsi_xfer queue routines                                                   */
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
/*                                Control Blocks routines                     */
/******************************************************************************/


static int
adw_alloc_controls(sc)
	ADW_SOFTC      *sc;
{
	bus_dma_segment_t seg;
	int             error, rseg;

	/*
         * Allocate the control structure.
         */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct adw_control),
			   NBPG, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate control structures,"
		       " error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
		   sizeof(struct adw_control), (caddr_t *) & sc->sc_control,
				 BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
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


static int
adw_alloc_carriers(sc)
	ADW_SOFTC      *sc;
{
	bus_dma_segment_t seg;
	int             error, rseg;

	/*
         * Allocate the control structure.
         */
	sc->sc_control->carriers = malloc(ADW_CARRIER_SIZE * ADW_MAX_CARRIER,
			M_DEVBUF, M_WAITOK);
	if(!sc->sc_control->carriers) {
		printf("%s: malloc() failed in allocating carrier structures,"
		       " error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}

	if ((error = bus_dmamem_alloc(sc->sc_dmat,
			ADW_CARRIER_SIZE * ADW_MAX_CARRIER,
			NBPG, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate carrier structures,"
		       " error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
			ADW_CARRIER_SIZE * ADW_MAX_CARRIER,
			(caddr_t *) &sc->sc_control->carriers,
			BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map carrier structures,"
			" error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}

	/*
         * Create and load the DMA map used for the control blocks.
         */
	if ((error = bus_dmamap_create(sc->sc_dmat,
			ADW_CARRIER_SIZE * ADW_MAX_CARRIER, 1,
			ADW_CARRIER_SIZE * ADW_MAX_CARRIER, 0, BUS_DMA_NOWAIT,
			&sc->sc_dmamap_carrier)) != 0) {
		printf("%s: unable to create carriers DMA map,"
			" error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
			sc->sc_dmamap_carrier, sc->sc_control->carriers,
			ADW_CARRIER_SIZE * ADW_MAX_CARRIER, NULL,
			BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load carriers DMA map,"
			" error = %d\n", sc->sc_dev.dv_xname, error);
		return (error);
	}

	error = bus_dmamap_create(sc->sc_dmat, ADW_CARRIER_SIZE* ADW_MAX_CARRIER,
			1, ADW_CARRIER_SIZE * ADW_MAX_CARRIER,
			0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
			&sc->sc_control->dmamap_xfer);
	if (error) {
		printf("%s: unable to create Carrier DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}

	return (0);
}


/*
 * Create a set of Carriers and add them to the free list.  Called once
 * by adw_init().  We return the number of Carriers successfully created.
 */
static int
adw_create_carriers(sc)
	ADW_SOFTC	*sc;
{
	ADW_CARRIER	*carr;
	u_int32_t	carr_next = NULL;
	int		i, error;

	for(i=0; i < ADW_MAX_CARRIER; i++) {
		carr = (ADW_CARRIER *)(((u_int8_t *)sc->sc_control->carriers) +
				(ADW_CARRIER_SIZE * i));
		if ((error = adw_init_carrier(sc, carr)) != 0) {
			printf("%s: unable to initialize carrier, error = %d\n",
			       sc->sc_dev.dv_xname, error);
			return (i);
		}
		carr->next_vpa = carr_next;
		carr_next = carr->carr_pa;
		carr->id = i;
	}
	sc->carr_freelist = carr;
	return (i);
}


static int
adw_init_carrier(sc, carr)
	ADW_SOFTC	*sc;
	ADW_CARRIER	*carr;
{
	u_int32_t	carr_pa;
	int		/*error, */hashnum;

	/*
         * Create the DMA map for all of the Carriers.
         */
/*	error = bus_dmamap_create(sc->sc_dmat, ADW_CARRIER_SIZE,
			1, ADW_CARRIER_SIZE,
			0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
			&carr->dmamap_xfer);
	if (error) {
		printf("%s: unable to create Carrier DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}
*/
	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	carr_pa = ADW_CARRIER_ADDR(sc, carr);
	carr->carr_pa = carr_pa;
	hashnum = CARRIER_HASH(carr_pa);
	carr->nexthash = sc->sc_carrhash[hashnum];
	sc->sc_carrhash[hashnum] = carr;

	return(0);
}


/*
 * Given a physical address, find the Carrier that it corresponds to.
 */
ADW_CARRIER *
adw_carrier_phys_kv(sc, carr_phys)
	ADW_SOFTC	*sc;
	u_int32_t	carr_phys;
{
	int hashnum = CARRIER_HASH(carr_phys);
	ADW_CARRIER *carr = sc->sc_carrhash[hashnum];

	while (carr) {
		if (carr->carr_pa == carr_phys)
			break;
		carr = carr->nexthash;
	}
	return (carr);
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
	int	hashnum, error;

	/*
         * Create the DMA map for this CCB.
         */
	error = bus_dmamap_create(sc->sc_dmat,
				  (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
			 ADW_MAX_SG_LIST, (ADW_MAX_SG_LIST - 1) * PAGE_SIZE,
		   0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->dmamap_xfer);
	if (error) {
		printf("%s: unable to create CCB DMA map, error = %d\n",
		       sc->sc_dev.dv_xname, error);
		return (error);
	}

	/*
	 * put in the phystokv hash table
	 * Never gets taken out.
	 */
	ccb->hashkey = sc->sc_dmamap_control->dm_segs[0].ds_addr +
	    ADW_CCB_OFF(ccb);
	hashnum = CCB_HASH(ccb->hashkey);
	ccb->nexthash = sc->sc_ccbhash[hashnum];
	sc->sc_ccbhash[hashnum] = ccb;
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
 * Given a physical address, find the ccb that it corresponds to.
 */
ADW_CCB *
adw_ccb_phys_kv(sc, ccb_phys)
	ADW_SOFTC	*sc;
	u_int32_t	ccb_phys;
{
	int hashnum = CCB_HASH(ccb_phys);
	ADW_CCB *ccb = sc->sc_ccbhash[hashnum];

	while (ccb) {
		if (ccb->hashkey == ccb_phys)
			break;
		ccb = ccb->nexthash;
	}
	return (ccb);
}


/*
 * Queue a CCB to be sent to the controller, and send it if possible.
 */
static int
adw_queue_ccb(sc, ccb, retry)
	ADW_SOFTC      *sc;
	ADW_CCB        *ccb;
	int		retry;
{
	int		errcode;

	if(!retry)
		TAILQ_INSERT_TAIL(&sc->sc_waiting_ccb, ccb, chain);

	while ((ccb = sc->sc_waiting_ccb.tqh_first) != NULL) {

		errcode = AdvExeScsiQueue(sc, &ccb->scsiq);
		switch(errcode) {
		case ADW_SUCCESS:
			break;

		case ADW_BUSY:
			printf("ADW_BUSY\n");
			return(ADW_BUSY);

		case ADW_ERROR:
			printf("ADW_ERROR\n");
			TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);
			return(ADW_ERROR);
		}

		TAILQ_REMOVE(&sc->sc_waiting_ccb, ccb, chain);

		if ((ccb->xs->flags & SCSI_POLL) == 0)
			timeout(adw_timeout, ccb, (ccb->timeout * hz) / 1000);
	}

	return(errcode);
}


/******************************************************************************/
/*                           SCSI layer interfacing routines                  */
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

		warn_code = (sc->chip_type == ADV_CHIP_ASC3550)?
				AdvInitFrom3550EEP(sc) :
				AdvInitFrom38C0800EEP(sc);

		if (warn_code & ASC_WARN_EEPROM_CHKSUM)
			printf("%s: Bad checksum found. "
			       "Setting default values\n",
			       sc->sc_dev.dv_xname);
		if (warn_code & ASC_WARN_EEPROM_TERMINATION)
			printf("%s: Bad bus termination setting."
			       "Using automatic termination.\n",
			       sc->sc_dev.dv_xname);
	}

	sc->isr_callback = (ADW_CALLBACK) adw_isr_callback;
	sc->async_callback = (ADW_CALLBACK) adw_async_callback;

	return (0);
}


void
adw_attach(sc)
	ADW_SOFTC      *sc;
{
	int             i, error;


	TAILQ_INIT(&sc->sc_free_ccb);
	TAILQ_INIT(&sc->sc_waiting_ccb);
	LIST_INIT(&sc->sc_queue);


	/*
         * Allocate the Control Blocks.
         */
	error = adw_alloc_controls(sc);
	if (error)
		return; /* (error) */ ;

	bzero(sc->sc_control, sizeof(struct adw_control));

	/*
	 * Create and initialize the Control Blocks.
	 */
	i = adw_create_ccbs(sc, sc->sc_control->ccbs, ADW_MAX_CCB);
	if (i == 0) {
		printf("%s: unable to create Control Blocks\n",
		       sc->sc_dev.dv_xname);
		return; /* (ENOMEM) */ ;
	} else if (i != ADW_MAX_CCB) {
		printf("%s: WARNING: only %d of %d Control Blocks"
		       " created\n",
		       sc->sc_dev.dv_xname, i, ADW_MAX_CCB);
	}

	/*
	 * Create and initialize the Carriers.
	 */
	error = adw_alloc_carriers(sc);
	if (error)
		return; /* (error) */ ;

	bzero(sc->sc_control->carriers, ADW_CARRIER_SIZE * ADW_MAX_CARRIER);

	i = adw_create_carriers(sc);
	if (i == 0) {
		printf("%s: unable to create Carriers\n",
		       sc->sc_dev.dv_xname);
		return; /* (ENOMEM) */ ;
	} else if (i != ADW_MAX_CARRIER) {
		printf("%s: WARNING: only %d of %d Carriers created\n",
		       sc->sc_dev.dv_xname, i, ADW_MAX_CARRIER);
	}


	/*
	 * Initialize the ASC3550.
	 */
	error = (sc->chip_type == ADV_CHIP_ASC3550)?
			AdvInitAsc3550Driver(sc) :
			AdvInitAsc38C0800Driver(sc);
	switch (error) {
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

	case ASC_IERR_NO_CARRIER:
		panic("%s: no carrier",
		      sc->sc_dev.dv_xname);
		break;

	case ASC_WARN_BUSRESET_ERROR:
		printf("%s: WARNING: Bus Reset Error\n",
		      sc->sc_dev.dv_xname);
		break;
	}

	/*
	 * Fill in the adapter.
	 */
	sc->sc_adapter.scsi_cmd = adw_scsi_cmd;
	sc->sc_adapter.scsi_minphys = adwminphys;

	/*
         * fill in the prototype scsi_link.
         */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->chip_scsi_id;
	sc->sc_link.adapter = &sc->sc_adapter;
	sc->sc_link.device = &adw_dev;
	sc->sc_link.openings = 4;
	sc->sc_link.adapter_buswidth = ADW_MAX_TID+1;

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
	int             s, fromqueue = 1, dontqueue = 0, nowait = 0, retry = 0;
	int		flags;

	s = splbio();		/* protect the queue */

	/*
         * If we're running the queue from adw_done(), we've been
         * called with the first queue entry as our argument.
         */
	if (xs == sc->sc_queue.lh_first) {
		xs = adw_dequeue(sc);
		fromqueue = 1;
		nowait = 1;
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

	flags = xs->flags;
	if (nowait)
		flags |= SCSI_NOSLEEP;
	if ((ccb = adw_get_ccb(sc, flags)) == NULL) {
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

	if (adw_build_req(xs, ccb, flags)) {
retryagain:
		s = splbio();
		retry = adw_queue_ccb(sc, ccb, retry);
		splx(s);

		switch(retry) {
		case ADW_BUSY:
			goto retryagain;

		case ADW_ERROR:
			xs->error = XS_DRIVER_STUFFUP;
			return (COMPLETE);
			
		}

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
adw_build_req(xs, ccb, flags)
	struct scsi_xfer *xs;
	ADW_CCB        *ccb;
	int		flags;
{
	struct scsi_link *sc_link = xs->sc_link;
	ADW_SOFTC      *sc = sc_link->adapter_softc;
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_SCSI_REQ_Q *scsiqp;
	int             error;

	scsiqp = &ccb->scsiq;
	bzero(scsiqp, sizeof(ADW_SCSI_REQ_Q));

	/*
	 * Set the ADW_SCSI_REQ_Q 'ccb_ptr' to point to the
	 * physical CCB structure.
	 */
	scsiqp->ccb_ptr = ccb->hashkey;

	/*
	 * Build the ADW_SCSI_REQ_Q request.
	 */

	/*
	 * Set CDB length and copy it to the request structure.
	 */
	bcopy(xs->cmd, &scsiqp->cdb, scsiqp->cdb_len = xs->cmdlen);

	scsiqp->target_id = sc_link->target;
	scsiqp->target_lun = sc_link->lun;

	scsiqp->vsense_addr = &ccb->scsi_sense;
	scsiqp->sense_addr = sc->sc_dmamap_control->dm_segs[0].ds_addr +
			ADW_CCB_OFF(ccb) + offsetof(struct adw_ccb, scsi_sense);
/*	scsiqp->sense_addr = ccb->hashkey +
	    offsetof(struct adw_ccb, scsi_sense);
*/	scsiqp->sense_len = sizeof(struct scsi_sense_data);

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
				(flags & SCSI_NOSLEEP) ?
				BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		} else
#endif		/* TFS */
		{
			error = bus_dmamap_load(dmat,
			      ccb->dmamap_xfer, xs->data, xs->datalen, NULL,
				(flags & SCSI_NOSLEEP) ?
				BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
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
				(xs->flags & SCSI_DATA_IN) ?
				BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

		/*
		 * Build scatter-gather list.
		 */
		scsiqp->data_cnt = xs->datalen;
		scsiqp->vdata_addr = xs->data;
		scsiqp->data_addr = ccb->dmamap_xfer->dm_segs[0].ds_addr;
		bzero(ccb->sg_block, sizeof(ADW_SG_BLOCK) * ADW_NUM_SG_BLOCK);
		adw_build_sglist(ccb, scsiqp, ccb->sg_block);
	} else {
		/*
                 * No data xfer, use non S/G values.
                 */
		scsiqp->data_cnt = 0;
		scsiqp->vdata_addr = 0;
		scsiqp->data_addr = 0;
	}

	return (1);
}


/*
 * Build scatter-gather list for Wide Boards.
 */
static void
adw_build_sglist(ccb, scsiqp, sg_block)
	ADW_CCB        *ccb;
	ADW_SCSI_REQ_Q *scsiqp;
	ADW_SG_BLOCK   *sg_block;
{
	u_long          sg_block_next_addr;	/* block and its next */
	u_int32_t       sg_block_physical_addr;
	int             i;	/* how many SG entries */
	bus_dma_segment_t *sg_list = &ccb->dmamap_xfer->dm_segs[0];
	int             sg_elem_cnt = ccb->dmamap_xfer->dm_nsegs;


	sg_block_next_addr = (u_long) sg_block;	/* allow math operation */
	sg_block_physical_addr = ccb->hashkey +
	    offsetof(struct adw_ccb, sg_block[0]);
	scsiqp->sg_real_addr = sg_block_physical_addr;

	/*
	 * If there are more than NO_OF_SG_PER_BLOCK dma segments (hw sg-list)
	 * then split the request into multiple sg-list blocks.
	 */

	do {
		for (i = 0; i < NO_OF_SG_PER_BLOCK; i++) {
			sg_block->sg_list[i].sg_addr = sg_list->ds_addr;
			sg_block->sg_list[i].sg_count = sg_list->ds_len;

			if (--sg_elem_cnt == 0) {
				/* last entry, get out */
				sg_block->sg_cnt = i + i;
				sg_block->sg_ptr = NULL; /* next link = NULL */
				return;
			}
			sg_list++;
		}
		sg_block_next_addr += sizeof(ADW_SG_BLOCK);
		sg_block_physical_addr += sizeof(ADW_SG_BLOCK);

		sg_block->sg_cnt = NO_OF_SG_PER_BLOCK;
		sg_block->sg_ptr = sg_block_physical_addr;
		sg_block = (ADW_SG_BLOCK *) sg_block_next_addr;	/* virt. addr */
	} while (1);
}


int
adw_intr(arg)
	void           *arg;
{
	ADW_SOFTC      *sc = arg;
	struct scsi_xfer *xs;


	if(AdvISR(sc) != ADW_FALSE) {
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

	return (0);
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
	if (ccb->flags & CCB_ABORTED) {
	/*
	 * Abort Timed Out
	 * Lets try resetting the bus!
	 */
		printf(" AGAIN. Resetting SCSI Bus\n");
		ccb->flags &= ~CCB_ABORTED;
		/* AdvResetSCSIBus() will call sbreset_callback() */
		AdvResetSCSIBus(sc);
	} else {
	/*
	 * Abort the operation that has timed out
	 */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ccb->flags |= CCB_ABORTING;
		/* ADW_ABORT_CCB() will implicitly call isr_callback() */
		ADW_ABORT_CCB(sc, ccb);
	}

	splx(s);
}


/******************************************************************************/
/*                           WIDE boards Interrupt callbacks                  */
/******************************************************************************/


/*
 * adw_isr_callback() - Second Level Interrupt Handler called by AdvISR()
 *
 * Interrupt callback function for the Wide SCSI Adv Library.
 */
static void
adw_isr_callback(sc, scsiq)
	ADW_SOFTC      *sc;
	ADW_SCSI_REQ_Q *scsiq;
{
	bus_dma_tag_t   dmat = sc->sc_dmat;
	ADW_CCB        *ccb;
	struct scsi_xfer *xs;
	struct scsi_sense_data *s1, *s2;
//	int		 s;


	ccb = adw_ccb_phys_kv(sc, scsiq->ccb_ptr);

	untimeout(adw_timeout, ccb);

/*	if(ccb->flags & CCB_ABORTING) {
		printf("Retrying request\n");
		ccb->flags &= ~CCB_ABORTING;
		ccb->flags |= CCB_ABORTED;
		s = splbio();
		adw_queue_ccb(sc, ccb);
		splx(s);
		return;
	}
*/
	xs = ccb->xs;

	/*
         * If we were a data transfer, unload the map that described
         * the data buffer.
         */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->dmamap_xfer,
			 (xs->flags & SCSI_DATA_IN) ?
			 BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
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
		break;

	case QD_WITH_ERROR:
		switch (scsiq->host_status) {
		case QHSTA_NO_ERROR:
			switch(scsiq->scsi_status) {
			case SS_CHK_CONDITION:
			case SS_CMD_TERMINATED:
				s1 = &ccb->scsi_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SS_TARGET_BUSY:
			case SS_RSERV_CONFLICT:
			case SS_QUEUE_FULL:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case SS_CONDITION_MET:
			case SS_INTERMID:
			case SS_INTERMID_COND_MET:
				xs->error = XS_DRIVER_STUFFUP;
				break;
			case SS_GOOD:
				break;
			}
			break;

		case QHSTA_M_SEL_TIMEOUT:
			xs->error = XS_DRIVER_STUFFUP;
			break;

		default:
			/* Some other QHSTA error occurred. */
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case QD_ABORTED_BY_HOST:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	adw_free_ccb(sc, ccb);
	xs->flags |= ITSDONE;
	scsi_done(xs);
}


/*
 * adv_async_callback() - Adv Library asynchronous event callback function.
 */
static void
adw_async_callback(sc, code)
	ADW_SOFTC	*sc;
	u_int8_t	code;
{
	switch (code) {
	case ADV_ASYNC_SCSI_BUS_RESET_DET:
		/*
		 * The firmware detected a SCSI Bus reset.
		 */
		break;

	case ADV_ASYNC_RDMA_FAILURE:
		/*
		 * Handle RDMA failure by resetting the SCSI Bus and
		 * possibly the chip if it is unresponsive. Log the error
		 * with a unique code.
		 */
		AdvResetSCSIBus(sc);
		break;

	case ADV_HOST_SCSI_BUS_RESET:
               /*
                * Host generated SCSI bus reset occurred.
                */
              break;

	default:
		break;
	}
}
