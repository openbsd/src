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
 * $Id: aic7xxx_openbsd.c,v 1.9 2002/09/06 05:04:41 smurph Exp $
 *
 * $FreeBSD: src/sys/dev/aic7xxx/aic7xxx_freebsd.c,v 1.26 2001/07/18 21:39:47 gibbs Exp $
 * $OpenBSD: aic7xxx_openbsd.c,v 1.9 2002/09/06 05:04:41 smurph Exp $
 */

#include <dev/ic/aic7xxx_openbsd.h>
#include <dev/ic/aic7xxx_inline.h>

struct cfdriver ahc_cd = {
	NULL, "ahc", DV_DULL
};

int32_t		ahc_action(struct scsi_xfer *xs);
static void	ahc_minphys(struct buf *bp);

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

#ifndef AHC_TMODE_ENABLE
#define AHC_TMODE_ENABLE 0
#endif

#define ccb_scb_ptr spriv_ptr0

#ifdef AHC_DEBUG
int     ahc_debug = AHC_DEBUG;
#endif

#if UNUSED
static void     ahc_dump_targcmd(struct target_cmd *cmd);
#endif
void		ahc_build_free_scb_list(struct ahc_softc *ahc);
int		ahc_execute_scb(void *arg, bus_dma_segment_t *dm_segs,
				int nsegments);
int		ahc_poll(struct ahc_softc *ahc, int wait);
void		ahc_timeout(void *);
int		ahc_setup_data(struct ahc_softc *ahc,
				    struct scsi_xfer *xs,
			       struct scb *scb);
void		ahc_set_recoveryscb(struct ahc_softc *ahc,
				    struct scb *scb);
int		ahc_init_scbdata(struct ahc_softc *ahc);
void		ahc_fini_scbdata(struct ahc_softc *ahc);
int		ahc_istagged_device(struct ahc_softc *ahc,
					 struct scsi_xfer *xs,
				    int nocmdcheck);
void		ahc_check_tags(struct ahc_softc *ahc,
			       struct scsi_xfer *xs);

/*
 * Routines to manage busy targets.  The old driver didn't need to 
 * pause the sequencer because no device registers were accessed.  Now
 * busy targets are controlled via the device registers and thus, we
 * have to pause the sequencer for chips that don't have the
 * auto-pause feature.  XXX smurph
 */
u_int ahc_pause_index_busy_tcl(struct ahc_softc *ahc, u_int tcl);
void  ahc_pause_unbusy_tcl(struct ahc_softc *ahc, u_int tcl);
void  ahc_pause_busy_tcl(struct ahc_softc *ahc, u_int tcl, u_int busyid);

u_int
ahc_pause_index_busy_tcl(ahc, tcl)
	struct ahc_softc *ahc;
	u_int tcl;
{
	u_int retval; 
	if (ahc->features & AHC_AUTOPAUSE) {
		retval = ahc_index_busy_tcl(ahc, tcl);
	} else {
		ahc_pause(ahc);
		retval = ahc_index_busy_tcl(ahc, tcl);
		ahc_unpause(ahc);
	}
	return retval;
}

void
ahc_pause_unbusy_tcl(ahc, tcl)
	struct ahc_softc *ahc;
	u_int tcl;
{
	if (ahc->features & AHC_AUTOPAUSE) {
		ahc_unbusy_tcl(ahc, tcl);
	} else {
		ahc_pause(ahc);
		ahc_unbusy_tcl(ahc, tcl);
		ahc_unpause(ahc);
	}
}
					     
void
ahc_pause_busy_tcl(ahc, tcl, busyid)
	struct ahc_softc *ahc;
	u_int tcl;
	u_int busyid;
{
	if (ahc->features & AHC_AUTOPAUSE) {
		ahc_busy_tcl(ahc, tcl, busyid);
	} else {
		ahc_pause(ahc);
		ahc_busy_tcl(ahc, tcl, busyid);
		ahc_unpause(ahc);
	}
}

/* Special routine to force negotiation for OpenBSD */
void 
ahc_force_neg(ahc)
	struct ahc_softc *ahc;
{
	int num_targets = AHC_NUM_TARGETS;
	int i;
	
	if ((ahc->features & (AHC_WIDE|AHC_TWIN)) == 0)
		num_targets = 8;

	for (i = 0; i < num_targets; i++) {
		struct ahc_initiator_tinfo *tinfo;
		struct ahc_tmode_tstate *tstate;
		u_int our_id;
		u_int target_id;
		char channel;

		channel = 'A';
		our_id = ahc->our_id;
		target_id = i;
		if (i > 7 && (ahc->features & AHC_TWIN) != 0) {
			channel = 'B';
			our_id = ahc->our_id_b;
			target_id = i % 8;
		}
		tinfo = ahc_fetch_transinfo(ahc, channel, our_id,
					    target_id, &tstate);
		tinfo->goal = tinfo->user; /* force negotiation */
		tstate->discenable = ahc->user_discenable;
	}
}

int
ahc_createdmamem(ahc, dmat, size, mapp, vaddr, baddr, seg, nseg, what)
	struct ahc_softc *ahc;
	bus_dma_tag_t dmat;
	int size;
	bus_dmamap_t *mapp;
	caddr_t *vaddr;
	bus_addr_t *baddr;
	bus_dma_segment_t *seg;
	int *nseg;
	const char *what;
{
	int error, level = 0;
	int dma_flags = BUS_DMA_NOWAIT;

	dmat = ahc->parent_dmat;
	
	if ((ahc->chip & AHC_VL) !=0)
		dma_flags |= ISABUS_DMA_32BIT;
	
	if ((error = bus_dmamem_alloc(dmat, size, NBPG, 0,
			seg, 1, nseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: failed to %s DMA map for %s, error = %d\n",
			"allocate", ahc_name(ahc), what, error);
		goto out;
	}
	level++;

	if ((error = bus_dmamem_map(dmat, seg, *nseg, size, vaddr,
			BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: failed to %s DMA map for %s, error = %d\n",
			"map", ahc_name(ahc), what, error);
		goto out;
	}
	level++;

	if ((error = bus_dmamap_create(dmat, size, 1, size, 0,
			dma_flags, mapp)) != 0) {
		printf("%s: failed to %s DMA map for %s, error = %d\n",
			"create", ahc_name(ahc), what, error);
		goto out;
	}
	level++;

	if ((error = bus_dmamap_load(dmat, *mapp, *vaddr, size, NULL,
			BUS_DMA_NOWAIT)) != 0) {
		printf("%s: failed to %s DMA map for %s, error = %d\n",
			"load", ahc_name(ahc), what, error);
		goto out;
	}

	*baddr = (*mapp)->dm_segs[0].ds_addr;
	return 0;
out:
	switch (level) {
	case 3:
		bus_dmamap_destroy(dmat, *mapp);
		/* FALLTHROUGH */
	case 2:
		bus_dmamem_unmap(dmat, *vaddr, size);
		/* FALLTHROUGH */
	case 1:
		bus_dmamem_free(dmat, seg, *nseg);
		break;
	default:
		break;
	}

	return error;
}

void
ahc_freedmamem(tag, size, map, vaddr, seg, nseg)
	bus_dma_tag_t tag;
	int size;
	bus_dmamap_t map;
	caddr_t vaddr;
	bus_dma_segment_t *seg;
	int nseg;
{
	bus_dmamap_unload(tag, map);
	bus_dmamap_destroy(tag, map);
	bus_dmamem_unmap(tag, vaddr, size);
	bus_dmamem_free(tag, seg, nseg);
}

void
ahc_alloc_scbs(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;
	struct scb *next_scb;
	struct sg_map_node *sg_map;
	bus_addr_t physaddr;
	struct ahc_dma_seg *segs;
	int newcount;
	int i;
	int dma_flags = 0;

	scb_data = ahc->scb_data;
	if (scb_data->numscbs >= AHC_SCB_MAX)
		/* Can't allocate any more */
		return;

	next_scb = &scb_data->scbarray[scb_data->numscbs];

	sg_map = malloc(sizeof(*sg_map), M_DEVBUF, M_NOWAIT);

	if (sg_map == NULL)
		return;
	
	if (ahc_createdmamem(ahc, scb_data->sg_dmat, PAGE_SIZE, 
			     &sg_map->sg_dmamap, (caddr_t *)&sg_map->sg_vaddr,
			     &sg_map->sg_physaddr, &sg_map->sg_dmasegs, 
			     &sg_map->sg_nseg, "SG space") < 0) {
		free(sg_map, M_DEVBUF);
		return;
	}

	SLIST_INSERT_HEAD(&scb_data->sg_maps, sg_map, links);

	segs = sg_map->sg_vaddr;
	physaddr = sg_map->sg_physaddr;

	newcount = (PAGE_SIZE / (AHC_NSEG * sizeof(struct ahc_dma_seg)));
	for (i = 0; scb_data->numscbs < AHC_SCB_MAX && i < newcount; i++) {
		struct scb_platform_data *pdata;
		int error;
		
		pdata = (struct scb_platform_data *)malloc(sizeof(*pdata),
							   M_DEVBUF, M_NOWAIT);
		if (pdata == NULL)
			break;
		bzero(pdata, sizeof(*pdata));
		next_scb->platform_data = pdata;
		next_scb->sg_map = sg_map;
		next_scb->sg_list = segs;
		/*
		 * The sequencer always starts with the second entry.
		 * The first entry is embedded in the scb.
		 */
		next_scb->sg_list_phys = physaddr + sizeof(struct ahc_dma_seg);
		next_scb->ahc_softc = ahc;
		next_scb->flags = SCB_FREE;
		
		/* set up AHA-284x correctly. */
		dma_flags = ((ahc->chip & AHC_VL) !=0) ? 
			BUS_DMA_NOWAIT|ISABUS_DMA_32BIT :
			BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW;
		
		ahc->buffer_dmat = ahc->parent_dmat;
		error = bus_dmamap_create(ahc->buffer_dmat,
					  AHC_MAXTRANSFER_SIZE, AHC_NSEG,
					  MAXBSIZE, 0, dma_flags, 
					  &next_scb->dmamap);
		if (error !=0) 
			break;
		
		next_scb->hscb = &scb_data->hscbs[scb_data->numscbs];
		next_scb->hscb->tag = ahc->scb_data->numscbs;
		SLIST_INSERT_HEAD(&ahc->scb_data->free_scbs,
				  next_scb, links.sle);
		segs += AHC_NSEG;
		physaddr += (AHC_NSEG * sizeof(struct ahc_dma_seg));
		next_scb++;
		ahc->scb_data->numscbs++;
	}
}

int
ahc_init_scbdata(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;

	scb_data = ahc->scb_data;
	scb_data->init_level = 0;
	SLIST_INIT(&scb_data->free_scbs);
	SLIST_INIT(&scb_data->sg_maps);

	/* Allocate SCB resources */
	scb_data->scbarray =
	    (struct scb *)malloc(sizeof(struct scb) * AHC_SCB_MAX,
				 M_DEVBUF, M_NOWAIT);
	if (scb_data->scbarray == NULL)
		return (ENOMEM);
	memset(scb_data->scbarray, 0, sizeof(struct scb) * AHC_SCB_MAX);

	/* set dma tags */
	scb_data->hscb_dmat = ahc->parent_dmat;
	scb_data->sense_dmat = ahc->parent_dmat;
	scb_data->sg_dmat = ahc->parent_dmat;
	
	/* Determine the number of hardware SCBs and initialize them */
	scb_data->maxhscbs = ahc_probe_scbs(ahc);
	if ((ahc->flags & AHC_PAGESCBS) != 0) {
		/* SCB 0 heads the free list */
		ahc_outb(ahc, FREE_SCBH, 0);
	} else {
		ahc_outb(ahc, FREE_SCBH, SCB_LIST_NULL);
	}

	if (ahc->scb_data->maxhscbs == 0) {
		printf("%s: No SCB space found\n", ahc_name(ahc));
		return (ENXIO);
	}

	ahc_build_free_scb_list(ahc);

	/*
	 * Create our DMA mappings.  These tags define the kinds of device
	 * accessible memory allocations and memory mappings we will
	 * need to perform during normal operation.
	 *
	 * Unless we need to further restrict the allocation, we rely
	 * on the restrictions of the parent dmat, hence the common
	 * use of MAXADDR and MAXSIZE.
	 */
	if (ahc_createdmamem(ahc, scb_data->hscb_dmat,
	    AHC_SCB_MAX * sizeof(struct hardware_scb), 
	    &scb_data->hscb_dmamap, (caddr_t *)&scb_data->hscbs, 
	    &scb_data->hscb_busaddr, &scb_data->hscb_seg,
	    &scb_data->hscb_nseg, "hardware SCB structures") < 0)
		goto error_exit;
	
	scb_data->init_level++;

	/* DMA for our sense buffers */
	if (ahc_createdmamem(ahc, scb_data->sense_dmat,
	    AHC_SCB_MAX * sizeof(struct scsi_sense_data),
	    &scb_data->sense_dmamap, (caddr_t *)&scb_data->sense,
	    &scb_data->sense_busaddr, &scb_data->sense_seg,
	    &scb_data->sense_nseg, "sense buffers") < 0)
		goto error_exit;

	scb_data->init_level++;
	
	/* Perform initial CCB allocation */
	memset(scb_data->hscbs, 0, AHC_SCB_MAX * sizeof(struct hardware_scb));
	ahc_alloc_scbs(ahc);

	if (scb_data->numscbs == 0) {
		printf("%s: Unable to allocate initial scbs\n",
		       ahc_name(ahc));
		goto error_exit;
	}
	scb_data->init_level++;

	/*
	 * Tell the sequencer which SCB will be the next one it receives.
	 */
	ahc->next_queued_scb = ahc_get_scb(ahc);
	ahc_outb(ahc, NEXT_QUEUED_SCB, ahc->next_queued_scb->hscb->tag);

	/*
	 * Note that we were successfull
	 */
	return (0); 

error_exit:

	return (ENOMEM);
}

void
ahc_fini_scbdata(ahc)
	struct ahc_softc *ahc;
{
	struct scb_data *scb_data;

	scb_data = ahc->scb_data;

	switch (scb_data->init_level) {
	default:
	case 3:
	{
		struct sg_map_node *sg_map;

		while ((sg_map = SLIST_FIRST(&scb_data->sg_maps))!= NULL) {
			SLIST_REMOVE_HEAD(&scb_data->sg_maps, links);
			ahc_freedmamem(ahc->parent_dmat, PAGE_SIZE,
				       sg_map->sg_dmamap,
				       (caddr_t)sg_map->sg_vaddr,
				       &sg_map->sg_dmasegs, sg_map->sg_nseg);
			free(sg_map, M_DEVBUF);
		}
	}
	/*FALLTHROUGH*/
	case 2:
		ahc_freedmamem(ahc->parent_dmat,
			       AHC_SCB_MAX * sizeof(struct scsi_sense_data),
			       scb_data->sense_dmamap, (caddr_t)scb_data->sense,
			       &scb_data->sense_seg, scb_data->sense_nseg);
	/*FALLTHROUGH*/
	case 1:
		ahc_freedmamem(ahc->parent_dmat,
			       AHC_SCB_MAX * sizeof(struct hardware_scb), 
			       scb_data->hscb_dmamap, (caddr_t)scb_data->hscbs,
			       &scb_data->hscb_seg, scb_data->hscb_nseg);
	/*FALLTHROUGH*/
	}
	if (scb_data->scbarray != NULL)
		free(scb_data->scbarray, M_DEVBUF);
}

void
ahc_free(ahc)
	struct ahc_softc *ahc;
{
	ahc_fini_scbdata(ahc);
	if (ahc->init_level != 0)
		ahc_freedmamem(ahc->parent_dmat, ahc->shared_data_size,
		    ahc->shared_data_dmamap, ahc->qoutfifo,
		    &ahc->shared_data_seg, ahc->shared_data_nseg);

	if (ahc->scb_data != NULL)
		free(ahc->scb_data, M_DEVBUF);
	return;
}

/*
 * Attach all the sub-devices we can find
 */
int
ahc_attach(ahc)
	struct ahc_softc *ahc;
{
	char   ahc_info[256];
	int s;
	ahc_lock(ahc, &s);
	
	ahc_controller_info(ahc, ahc_info);
	printf("%s: %s\n", ahc_name(ahc), ahc_info);
	/*
	 * Initialize the software queue.
	 */
	LIST_INIT(&ahc->platform_data->sc_xxxq);

#ifdef AHC_BROKEN_CACHE
	if (cpu_class == CPUCLASS_386)	/* doesn't have "wbinvd" instruction */
		ahc_broken_cache = 0;
#endif
	/*
	 * fill in the prototype scsi_links.
	 */
	ahc->platform_data->sc_link.adapter_target = ahc->our_id;
	if (ahc->features & AHC_WIDE)
		ahc->platform_data->sc_link.adapter_buswidth = 16;
	ahc->platform_data->sc_link.adapter_softc = ahc;
	ahc->platform_data->sc_link.adapter = &ahc_switch;
	ahc->platform_data->sc_link.openings = 2;
	ahc->platform_data->sc_link.device = &ahc_dev;
	ahc->platform_data->sc_link.flags = SCSIDEBUG_LEVEL;
	
	if (ahc->features & AHC_TWIN) {
		/* Configure the second scsi bus */
		ahc->platform_data->sc_link_b = ahc->platform_data->sc_link;
		ahc->platform_data->sc_link_b.adapter_target = ahc->our_id_b;
		if (ahc->features & AHC_WIDE)
			ahc->platform_data->sc_link.adapter_buswidth = 16;
		ahc->platform_data->sc_link_b.adapter_softc = ahc;
		ahc->platform_data->sc_link_b.adapter = &ahc_switch;
		ahc->platform_data->sc_link_b.openings = 2;
		ahc->platform_data->sc_link_b.device = &ahc_dev;
		ahc->platform_data->sc_link_b.flags = SCSIDEBUG_LEVEL;
	}

	/*
	 * ask the adapter what subunits are present
	 */
	if (ahc->platform_data->channel_b_primary == FALSE) {
		/* make SCSI_IS_SCSIBUS_B() == false, while probing channel A */
		ahc->platform_data->sc_link_b.scsibus = 0xff;
		config_found((void *)ahc, &ahc->platform_data->sc_link, scsiprint);
		if (ahc->features & AHC_TWIN)
			config_found((void *)ahc, &ahc->platform_data->sc_link_b, scsiprint);
	} else {
		/*
		 * if implementation of SCSI_IS_SCSIBUS_B() is changed to use
		 * ahc->sc_link.scsibus, then "ahc->sc_link.scsibus = 0xff;"
		 * is needed, here.
		 */
		if (ahc->features & AHC_TWIN)
			config_found((void *)ahc, &ahc->platform_data->sc_link_b, scsiprint);
		config_found((void *)ahc, &ahc->platform_data->sc_link, scsiprint);
	}
	ahc_unlock(ahc, &s);
	return 1;
}

/*
 * Catch an interrupt from the adapter
 */
int
ahc_platform_intr(arg)
	void *arg;
{
	struct	ahc_softc *ahc;
	u_int	intstat = 0;
	u_int	errstat = 0;

	/*
	 * Any interrupts to process?
	 */
	ahc = (struct ahc_softc *)arg; 
	
	intstat = ahc_inb(ahc, INTSTAT);
	
	/* Only check PCI error on PCI cards */
        if ((ahc->chip & AHC_PCI) != 0) {
		errstat = ahc_inb(ahc, ERROR);
		if ((intstat & INT_PEND) == 0 && (errstat & PCIERRSTAT)) {
			if (ahc->unsolicited_ints > 500) {
				ahc->unsolicited_ints = 0;
				ahc->bus_intr(ahc);
			}
			ahc->unsolicited_ints++;
			/* claim the interrupt */
			return 1;
		}
	}
	
	if ((intstat & INT_PEND) == 0){
		/* This interrupt is not for us */
		return 0;
	}
	
	ahc_intr(ahc);
	return 1; 
}

/*
 * We have an scb which has been processed by the
 * adaptor, now we look to see how the operation
 * went.
 */
void
ahc_done(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	struct scsi_xfer *xs = scb->io_ctx;
	struct scsi_link *sc_link = xs->sc_link;
	int requeue = 0;
	int target;
	int lun;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("ahc_done\n"));
	
#ifdef maybe_not_such_a_good_idea
	/* Don't smash a disconnected SCB */
	if ((scb->hscb->control & DISCONNECTED) != 0){
		printf("disconnected sbc (tag %d) in ahc_done(ahc)!!!\n");
		if ((xs = ahc->platform_data->sc_xxxq.lh_first) != NULL)
			(void) ahc_action(xs);
		return;
	}
#endif	
	
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

#ifdef AHC_DEBUG
	if ((ahc_debug & AHC_SHOWCMDS)) {
		ahc_print_path(ahc, scb);
		printf("ahc_done: opcode 0x%x tag %x flags %x status %d error %d\n", 
		       xs->cmdstore.opcode, scb->hscb->tag, 
		       scb->flags, xs->status, xs->error);
	}
#endif
	
	target = sc_link->target;
	lun = sc_link->lun;

	if (xs->datalen) {
		int op;
	
		if ((xs->flags & SCSI_DATA_IN) != 0)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		ahc->buffer_dmat = ahc->parent_dmat;
		bus_dmamap_sync(ahc->buffer_dmat, scb->dmamap,
		    0, scb->dmamap->dm_mapsize, op);
		
		bus_dmamap_unload(ahc->buffer_dmat, scb->dmamap);
	}

	/*
	 * Unbusy this target/channel/lun.
	 * XXX if we are holding two commands per lun, 
	 *     send the next command.
	 */
	if (!(scb->hscb->control & TAG_ENB)){
		ahc_pause_unbusy_tcl(ahc, XS_TCL(xs));
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
			struct scsi_xfer *txs = list_scb->io_ctx;
			if (!(txs->flags & SCSI_POLL))
				timeout_add(&list_scb->io_ctx->stimeout,
				    (list_scb->io_ctx->timeout * hz)/1000);
		}

		if (xs->error != XS_NOERROR)
			ahc_set_transaction_status(scb, CAM_CMD_TIMEOUT);
		ahc_print_path(ahc, scb);
		printf("no longer in timeout, status = %x\n", xs->status);
	}

	if (xs->error != XS_NOERROR) {
		/* Don't clobber any existing error state */
	} else if ((scb->flags & SCB_SENSE) != 0) {
		/*
		 * We performed autosense retrieval.
		 *
		 * Zero the sense data before having
		 * the drive fill it.  The SCSI spec mandates
		 * that any untransfered data should be
		 * assumed to be zero.  Complete the 'bounce'
		 * of sense information through buffers accessible
		 * via bus-space by copying it into the clients
		 * csio.
		 */
		memset(&xs->sense, 0, sizeof(struct scsi_sense_data));
		memcpy(&xs->sense, ahc_get_sense_buf(ahc, scb),
		       ahc_le32toh((scb->sg_list->len & AHC_SG_LEN_MASK)));
		xs->error = XS_SENSE;
	}
	
	if (scb->platform_data->flags & SCB_FREEZE_QUEUE) {
		/* keep negs from happening */
		if (ahc->platform_data->devqueue_blocked[target] > 0) {
			ahc->platform_data->devqueue_blocked[target]--;
		}
		scb->platform_data->flags &= ~SCB_FREEZE_QUEUE;
	}
	
	requeue = scb->platform_data->flags & SCB_REQUEUE;
	ahc_free_scb(ahc, scb);

	if (requeue) {
		/*
		 * Re-insert at the front of the private queue to
		 * preserve order.
		 */
		int s;
		ahc_lock(ahc, &s);
		ahc_list_insert_head(ahc, xs);
		ahc_unlock(ahc, &s);
	} else {
		if ((xs->sc_link->lun == 0) &&
		    (xs->flags & SCSI_POLL) &&
		    (xs->error == XS_NOERROR))
		ahc_check_tags(ahc, xs);
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}

	/*
	 * If there are entries in the software queue, try to
	 * run the first one.  We should be more or less guaranteed
	 * to succeed, since we just freed an SCB.
	 *
	 * NOTE: ahc_action() relies on our calling it with
	 * the first entry in the queue.
	 */
	if ((xs = ahc->platform_data->sc_xxxq.lh_first) != NULL)
		(void) ahc_action(xs);
}

static void
ahc_minphys(bp)
	struct buf *bp;
{
	/*
	 * Even though the card can transfer up to 16megs per command
	 * we are limited by the number of segments in the dma segment
	 * list that we can hold.  The worst case is that all pages are
	 * discontinuous physically, hense the "page per segment" limit
	 * enforced here.
	 */
	if (bp->b_bcount > ((AHC_NSEG - 1) * PAGE_SIZE)) {
		bp->b_bcount = ((AHC_NSEG - 1) * PAGE_SIZE);
	}
	minphys(bp);
}

int32_t
ahc_action(xs)
	struct scsi_xfer *xs;
{
	struct scsi_xfer *first_xs, *next_xs = NULL;
	struct ahc_softc *ahc;
	struct scb *scb;
	struct hardware_scb *hscb;	
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_tmode_tstate *tstate;
	u_int target_id;
	u_int our_id;
	char channel;
	int s, tcl;
	u_int16_t mask;
	int dontqueue = 0, fromqueue = 0;

	SC_DEBUG(xs->sc_link, SDEV_DB3, ("ahc_action\n"));
	ahc = (struct ahc_softc *)xs->sc_link->adapter_softc;

	/* must protect the queue */
	ahc_lock(ahc, &s);

	if (xs == ahc->platform_data->sc_xxxq.lh_first) {
		/*
		 * Called from ahc_done. Calling with the first entry in
		 * the queue is really just a way of seeing where we're
		 * called from. Now, find the first eligible SCB to send,
		 * e.g. one which will be accepted immediately.
		 */
		if (ahc->platform_data->queue_blocked) {
			ahc_unlock(ahc, &s);
			return (TRY_AGAIN_LATER);
		}

		xs = ahc_first_xs(ahc);
		if (xs == NULL) {
			ahc_unlock(ahc, &s);
			return (TRY_AGAIN_LATER);
		}

		next_xs = ahc_list_next(ahc, xs);
		ahc_list_remove(ahc, xs);
		fromqueue = 1;
		goto get_scb;
	}

	/* determine safety of software queueing */
	dontqueue = xs->flags & SCSI_POLL;
	
	/*
	 * If no new requests are accepted, just insert into the
	 * private queue to wait for our turn.
	 */
	tcl = XS_TCL(xs);
	
	if (ahc->platform_data->queue_blocked ||
	    ahc->platform_data->devqueue_blocked[xs->sc_link->target] ||
	    (!ahc_istagged_device(ahc, xs, 0) &&
	     ahc_pause_index_busy_tcl(ahc, tcl) != SCB_LIST_NULL)) {
		if (dontqueue) {
			ahc_unlock(ahc, &s);
			xs->error = XS_DRIVER_STUFFUP;
			return TRY_AGAIN_LATER;
		}
		ahc_list_insert_tail(ahc, xs);
		ahc_unlock(ahc, &s);
		return SUCCESSFULLY_QUEUED;
	}

	first_xs = ahc_first_xs(ahc);

	/* determine safety of software queueing */
	dontqueue = xs->flags & SCSI_POLL;

	/*
	 * Handle situations where there's already entries in the
	 * queue.
	 */
	if (first_xs != NULL) {
		/*
		 * If we can't queue, we have to abort, since
		 * we have to preserve order.
		 */
		if (dontqueue) {
			ahc_unlock(ahc, &s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * Swap with the first queue entry.
		 */
		ahc_list_insert_tail(ahc, xs);
		xs = first_xs;
		next_xs = ahc_list_next(ahc, xs);
		ahc_list_remove(ahc, xs);
		fromqueue = 1;
	}

get_scb:

	target_id = xs->sc_link->target;
	our_id = SCSI_SCSI_ID(ahc, xs->sc_link);

	/*
	 * get an scb to use.
	 */
	if ((scb = ahc_get_scb(ahc)) == NULL) {
		if (dontqueue) {
			ahc_unlock(ahc, &s);
			xs->error = XS_DRIVER_STUFFUP;
			return (TRY_AGAIN_LATER);
		}

		/*
		 * If we were pulled off the queue, put ourselves
		 * back to where we came from, otherwise tack ourselves
		 * onto the end.
		 */
		if (fromqueue && next_xs != NULL)
			ahc_list_insert_before(ahc, xs, next_xs);
		else
			ahc_list_insert_tail(ahc, xs);

		ahc_unlock(ahc, &s);
		return (SUCCESSFULLY_QUEUED);
	}

	tcl = XS_TCL(xs);

#ifdef DIAGNOSTIC
	if (!ahc_istagged_device(ahc, xs, 0) &&
	    ahc_pause_index_busy_tcl(ahc, tcl) != SCB_LIST_NULL)
		panic("ahc: queuing for busy target");
#endif
	
	scb->io_ctx = xs;
	hscb = scb->hscb;
	
	hscb->control = 0;
	
	timeout_set(&xs->stimeout, ahc_timeout, scb);

	if (ahc_istagged_device(ahc, xs, 0)){
		hscb->control |= TAG_ENB;
	} else {
		ahc_pause_busy_tcl(ahc, tcl, scb->hscb->tag);
	}

	ahc_unlock(ahc, &s);
 
	channel = SCSI_CHANNEL(ahc, xs->sc_link);
	if (ahc->platform_data->inited_channels[channel - 'A'] == 0) {
		if ((channel == 'A' && (ahc->flags & AHC_RESET_BUS_A)) ||
		    (channel == 'B' && (ahc->flags & AHC_RESET_BUS_B))) {
			ahc_lock(ahc, &s);
			ahc_reset_channel(ahc, channel, TRUE);
			ahc_unlock(ahc, &s);
		}
		ahc->platform_data->inited_channels[channel - 'A'] = 1;
	}

	/*
	 * Put all the arguments for the xfer in the scb
	 */
	hscb->scsiid = BUILD_SCSIID(ahc, xs->sc_link, target_id, our_id);
	hscb->lun = XS_LUN(xs);
	
	mask = SCB_GET_TARGET_MASK(ahc, scb);
	tinfo = ahc_fetch_transinfo(ahc, SCSI_CHANNEL(ahc, xs->sc_link), our_id,
				    target_id, &tstate);
	
	if (ahc->platform_data->inited_targets[target_id] == 0) {
		struct ahc_devinfo devinfo;

		ahc_lock(ahc, &s);
		ahc_compile_devinfo(&devinfo, our_id, target_id,
		    XS_LUN(xs), SCSI_CHANNEL(ahc, xs->sc_link),
		    ROLE_INITIATOR);
		ahc_update_neg_request(ahc, &devinfo, tstate, tinfo,
				       /*force*/TRUE);
		ahc->platform_data->inited_targets[target_id] = 1;
		ahc_unlock(ahc, &s);
	}

	hscb->scsirate = tinfo->scsirate;
	hscb->scsioffset = tinfo->curr.offset;
	if ((tstate->ultraenb & mask) != 0)
		hscb->control |= ULTRAENB;
		
	if ((tstate->discenable & mask) != 0)
		hscb->control |= DISCENB;

	if ((tstate->auto_negotiate & mask) != 0) {
		scb->flags |= SCB_AUTO_NEGOTIATE;
		hscb->control |= MK_MESSAGE;
	}

	if (xs->flags & SCSI_RESET) {
		scb->flags |= SCB_DEVICE_RESET;
		hscb->control |= MK_MESSAGE;
		return ahc_execute_scb(scb, NULL, 0);
	}

	return ahc_setup_data(ahc, xs, scb);
}

int
ahc_execute_scb(arg, dm_segs, nsegments)
	void *arg;
	bus_dma_segment_t *dm_segs;
	int nsegments;
{
	struct	scb *scb;
	struct	scsi_xfer *xs;
	struct	ahc_softc *ahc;
	struct	ahc_initiator_tinfo *tinfo;
	struct	ahc_tmode_tstate *tstate;
	u_int	mask;
	int	s;

	scb = (struct scb *)arg;
	xs = scb->io_ctx;
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

		ahc->buffer_dmat = ahc->parent_dmat;
		bus_dmamap_sync(ahc->buffer_dmat, scb->dmamap,
				0, scb->dmamap->dm_mapsize, op);
		
		sg--;
		sg->len |= ahc_htole32(AHC_DMA_LAST_SEG);

		/* Copy the first SG into the "current" data pointer area */
		scb->hscb->dataptr = scb->sg_list->addr;
		scb->hscb->datacnt = scb->sg_list->len;
	} else {
		scb->hscb->sgptr = SG_LIST_NULL;
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

		if (!ahc_istagged_device(ahc, xs, 0)){
			ahc_pause_unbusy_tcl(ahc, XS_TCL(xs));
		}
			
		if (nsegments != 0)
			bus_dmamap_unload(ahc->buffer_dmat, scb->dmamap);
		
		ahc_free_scb(ahc, scb);
		ahc_unlock(ahc, &s);
		return (COMPLETE);
	}

#ifdef DIAGNOSTIC
	if (scb->sg_count > 255)
		panic("ahc bad sg_count");
#endif

	/* Fixup byte order */
	scb->hscb->dataptr = ahc_htole32(scb->hscb->dataptr); 
	scb->hscb->datacnt = ahc_htole32(scb->hscb->datacnt);
	scb->hscb->sgptr = ahc_htole32(scb->hscb->sgptr);
	
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
	
	LIST_INSERT_HEAD(&ahc->pending_scbs, scb, pending_links);

#ifdef AHC_DEBUG
	if ((ahc_debug & AHC_SHOWCMDS)) {
		ahc_print_path(ahc, scb);
		printf("opcode 0x%x tag %x len %d flags %x "
		       "control %x fpos %u rate %x\n",
		       xs->cmdstore.opcode, scb->hscb->tag, 
		       scb->hscb->cdb_len, scb->flags, 
		       scb->hscb->control, ahc->qinfifonext,
		       scb->hscb->scsirate);
	}
#endif

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
			ahc_unlock(ahc, &s);
			return (SUCCESSFULLY_QUEUED);
		}
	}
	
	scb->flags |= SCB_ACTIVE;

	if (!(xs->flags & SCSI_POLL))
		timeout_add(&xs->stimeout, (xs->timeout * hz) / 1000);

	if ((scb->flags & SCB_TARGET_IMMEDIATE) != 0) {
		/* Define a mapping from our tag to the SCB. */
		ahc->scb_data->scbindex[scb->hscb->tag] = scb;
		ahc_pause(ahc);
		if ((ahc->flags & AHC_PAGESCBS) == 0)
			ahc_outb(ahc, SCBPTR, scb->hscb->tag);
		ahc_outb(ahc, SCB_TAG, scb->hscb->tag);
		ahc_outb(ahc, RETURN_1, CONT_MSG_LOOP);
		ahc_unpause(ahc);
	} else {
		ahc_queue_scb(ahc, scb);
	}


	if (!(xs->flags & SCSI_POLL)) {
		ahc_unlock(ahc, &s);
		return (SUCCESSFULLY_QUEUED);
	}
	
	/*
	 * If we can't use interrupts, poll for completion
	 */
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
ahc_poll(ahc, wait)
	struct	ahc_softc *ahc;
	int	wait;	/* in msec */
{
	while (--wait) {
		DELAY(1000);
		if ((ahc->chip & AHC_PCI) != 0 && (ahc_inb(ahc, ERROR) & PCIERRSTAT) != 0)
			ahc->bus_intr(ahc);
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
ahc_setup_data(ahc, xs, scb)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	struct scb *scb;
{
	struct hardware_scb *hscb;
	
	hscb = scb->hscb;
	xs->resid = xs->status = 0;
	xs->error = XS_NOERROR;
	
	hscb->cdb_len = xs->cmdlen;

	if (hscb->cdb_len > 12) {
		memcpy(hscb->cdb32, xs->cmd,
		       hscb->cdb_len);
		scb->flags |= SCB_CDB32_PTR;
	} else {
		memcpy(hscb->shared_data.cdb,
		       xs->cmd,
		       hscb->cdb_len);
	}

	/* Only use S/G if there is a transfer */
	if (xs->datalen) {
		int error;

		error = bus_dmamap_load(ahc->buffer_dmat,
					scb->dmamap, xs->data,
					xs->datalen, NULL,
					(xs->flags & SCSI_NOSLEEP) ?
					BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		if (error) {
			if (!ahc_istagged_device(ahc, xs, 0)){
				ahc_pause_unbusy_tcl(ahc, XS_TCL(xs));
			}
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
ahc_set_recoveryscb(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{

	if ((scb->flags & SCB_RECOVERY_SCB) == 0) {
		struct scb *list_scb;

		scb->flags |= SCB_RECOVERY_SCB;

		/*
		 * Take all queued, but not sent SCBs out of the equation.
		 * Also ensure that no new CCBs are queued to us while we
		 * try to fix this problem.
		 */
		ahc->platform_data->queue_blocked = 1;

		/*
		 * Go through all of our pending SCBs and remove
		 * any scheduled timeouts for them.  We will reschedule
		 * them after we've successfully fixed this problem.
		 */
		LIST_FOREACH(list_scb, &ahc->pending_scbs, pending_links) {
			timeout_del(&list_scb->io_ctx->stimeout);
		}
	}
}

void
ahc_timeout(arg)
	void *arg;
{
	struct	scb *scb;
	struct	ahc_softc *ahc;
	int	s, found;
	u_int	last_phase;
	int	target;
	int	lun;
	int	i;
	char	channel;

	scb = (struct scb *)arg; 
	ahc = (struct ahc_softc *)scb->io_ctx->sc_link->adapter_softc;

	ahc_lock(ahc, &s);

	/*
	 * Ensure that the card doesn't do anything
	 * behind our back.  Also make sure that we
	 * didn't "just" miss an interrupt that would
	 * affect this timeout.
	 */
	ahc_pause_and_flushwork(ahc);

	if ((scb->flags & SCB_ACTIVE) == 0) {
		/* Previous timeout took care of me already */
		printf("%s: Timedout SCB already complete. "
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
	
	/*
	 * Take a snapshot of the bus state and print out
	 * some information so we can track down driver bugs.
	 */
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
		 * If the bus is idle and we are actiing as the initiator
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

		saved_scbptr = ahc_inb(ahc, SCBPTR);
		active_scb_index = ahc_inb(ahc, SCB_TAG);
		
		if (last_phase != P_BUSFREE 
		  && (ahc_inb(ahc, SEQ_FLAGS) & IDENTIFY_SEEN) != 0
		  && (active_scb_index < ahc->scb_data->numscbs)) {
			struct scb *active_scb;

			/*
			 * If the active SCB is not from our device,
			 * assume that another device is hogging the bus
			 * and wait for it's timeout to expire before
			 * taking additional action.
			 */ 
			active_scb = ahc_lookup_scb(ahc, active_scb_index);
			if (active_scb != scb) {
				u_int	newtimeout;

				ahc_print_path(ahc, active_scb);
				printf("Other SCB Timeout%s",
				       (scb->flags & SCB_OTHERTCL_TIMEOUT) != 0
				       ? " again\n" : "\n");
				scb->flags |= SCB_OTHERTCL_TIMEOUT;
				newtimeout = MAX(active_scb->io_ctx->timeout,
						 scb->io_ctx->timeout);
				timeout_add(&scb->io_ctx->stimeout,
				    (newtimeout * hz) / 1000);
				ahc_unpause(ahc);
				ahc_unlock(ahc, &s);
				return;
			} 
			
			/* It's us */
			if ((scb->hscb->control & TARGET_SCB) != 0) {

				/*
				 * Send back any queued up transactions
				 * and properly record the error condition.
				 */
				ahc_freeze_devq(ahc, scb);
				ahc_set_transaction_status(scb,
							   CAM_CMD_TIMEOUT);
				ahc_freeze_scb(scb);
				ahc_done(ahc, scb);

				/* Will clear us from the bus */
				ahc_restart(ahc);
				ahc_unlock(ahc, &s);
				return;
			} 

			ahc_set_recoveryscb(ahc, active_scb);
			ahc_outb(ahc, MSG_OUT, MSG_BUS_DEV_RESET);
			ahc_outb(ahc, SCSISIGO, last_phase|ATNO);
			ahc_print_path(ahc, active_scb);
			printf("BDR message in message buffer\n");
			active_scb->flags |=  SCB_DEVICE_RESET;
			timeout_add(&active_scb->io_ctx->stimeout, 2 * hz);
			ahc_unpause(ahc);
		} else {
			int	 disconnected;

			/* XXX Shouldn't panic.  Just punt instead */
			if ((scb->hscb->control & TARGET_SCB) != 0)
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
				scb->flags |= /*SCB_QUEUED_MSG | */
					SCB_DEVICE_RESET;
				
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
				timeout_add(&scb->io_ctx->stimeout, 2 * hz);
				ahc_unpause(ahc);
			} else {
				/* Go "immediatly" to the bus reset */
				/* This shouldn't happen */
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
ahc_send_async(ahc, channel, target, lun, code, opt_arg)
	struct ahc_softc *ahc;
	char channel;
	u_int target, lun, code;
	void *opt_arg;
{
	/* Nothing to do here for OpenBSD */
}

void
ahc_platform_set_tags(ahc, devinfo, alg)
	struct ahc_softc *ahc;
	struct ahc_devinfo *devinfo;
	ahc_queue_alg alg;
{
	struct ahc_initiator_tinfo *tinfo;
	struct ahc_tmode_tstate *tstate;

	tinfo = ahc_fetch_transinfo(ahc, devinfo->channel,
				    devinfo->our_scsiid,
				    devinfo->target,
				    &tstate);

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
ahc_platform_alloc(ahc, platform_arg)
	struct ahc_softc *ahc;
	void *platform_arg;
{
	ahc->platform_data = malloc(sizeof(struct ahc_platform_data), M_DEVBUF,
	    M_NOWAIT);
	if (ahc->platform_data == NULL)
		return (ENOMEM);
	bzero(ahc->platform_data, sizeof(struct ahc_platform_data));
	
	/* Just do some initialization... */
	ahc->scb_data = NULL;
	ahc->platform_data->ih = NULL;
	ahc->platform_data->channel_b_primary = FALSE;

	return (0);
}

void
ahc_platform_free(ahc)
	struct ahc_softc *ahc;
{
	free(ahc->platform_data, M_DEVBUF);
}

int
ahc_softc_comp(lahc, rahc)
	struct ahc_softc *lahc;
	struct ahc_softc *rahc;
{
	/* We don't sort softcs under OpenBSD so report equal always */
	return (0);
}

void
ahc_check_tags(ahc, xs)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
{
	struct ahc_devinfo devinfo;

	if (xs->sc_link->quirks & SDEV_NOTAGS)
		return;

	if (ahc_istagged_device(ahc, xs, 1))
		return;

	ahc_compile_devinfo(&devinfo,
			    SCSI_SCSI_ID(ahc, xs->sc_link),
			    XS_SCSI_ID(xs),
			    XS_LUN(xs),
			    SCSI_CHANNEL(ahc, xs->sc_link),
			    ROLE_INITIATOR);

	ahc_set_tags(ahc, &devinfo, AHC_QUEUE_TAGGED);

	printf("%s: target %d using tagged queuing\n",
	       ahc_name(ahc), XS_SCSI_ID(xs));

	if (ahc->scb_data->maxhscbs >= 16 || 
	    (ahc->flags & AHC_PAGESCBS)) {
		/* Default to 16 tags */
		xs->sc_link->openings += 14;
	} else {
		/*	
		 * Default to 4 tags on whimpy
		 * cards that don't have much SCB
		 * space and can't page.  This prevents
		 * a single device from hogging all
		 * slots.  We should really have a better
		 * way of providing fairness.
		 */
		xs->sc_link->openings += 2;
	}
}

int
ahc_istagged_device(ahc, xs, nocmdcheck)
	struct ahc_softc *ahc;
	struct scsi_xfer *xs;
	int nocmdcheck;
{
	char channel;
	u_int our_id, target;
	struct ahc_tmode_tstate *tstate;
	struct ahc_devinfo devinfo;

	if (xs->sc_link->quirks & SDEV_NOTAGS)
		return 0;

	/*
	 * XXX never do these commands with tags. Should really be
	 * in a higher layer.
	 */
	if (!nocmdcheck && (xs->cmd->opcode == INQUIRY ||
	     xs->cmd->opcode == TEST_UNIT_READY ||
	     xs->cmd->opcode == REQUEST_SENSE))
		return 0;

	channel = SCSI_CHANNEL(ahc, xs->sc_link);
	our_id = SCSI_SCSI_ID(ahc, xs->sc_link);
	target = XS_SCSI_ID(xs);
	(void)ahc_fetch_transinfo(ahc, channel, our_id, target, &tstate);

	ahc_compile_devinfo(&devinfo, our_id, target, XS_LUN(xs),
			    channel, ROLE_INITIATOR);

	return (tstate->tagenable & devinfo.target_mask);
}

#if UNUSED
static void
ahc_dump_targcmd(cmd)
	struct target_cmd *cmd;
{
	uint8_t *byte;
	uint8_t *last_byte;
	int i;

	byte = &cmd->initiator_channel;
	/* Debugging info for received commands */
	last_byte = &cmd[1].initiator_channel;

	i = 0;
	while (byte < last_byte) {
		if (i == 0)
			printf("\t");
		printf("%#x", *byte++);
		i++;
		if (i == 8) {
			printf("\n");
			i = 0;
		} else {
			printf(", ");
		}
	}
}
#endif

#ifndef AHC_INLINES
/* 
 * This is a hack to keep from modifying the main
 * driver code as much as possible.  This function
 * does CAM to SCSI api stuff.
 */
void ahc_set_transaction_status(scb, status)
	struct scb *scb;
	uint32_t status;
{
	/* don't wipe the error */
	if (scb->io_ctx->error == XS_NOERROR){
		switch (status) {
		case CAM_CMD_TIMEOUT:
			status = XS_TIMEOUT;
			break;
		case CAM_BDR_SENT:
		case CAM_SCSI_BUS_RESET:
			status = XS_RESET;
			break;
		case CAM_UNEXP_BUSFREE:
		case CAM_REQ_TOO_BIG:
		case CAM_REQ_ABORTED:
		case CAM_AUTOSENSE_FAIL:
		case CAM_NO_HBA:
			status = XS_DRIVER_STUFFUP;
			break;
		case CAM_SEL_TIMEOUT:
			status = XS_SELTIMEOUT;
			break;
		case CAM_REQUEUE_REQ:
			scb->platform_data->flags |= SCB_REQUEUE;
			scb->io_ctx->error = XS_NOERROR;
			break;
		case CAM_SCSI_STATUS_ERROR:
		default:
			status = scb->io_ctx->error;
			break;
		}
	} else {
		status = scb->io_ctx->error;
	}
	scb->io_ctx->error = status;
}

void ahc_set_transaction_tag(scb, enabled, type)
	struct scb *scb;
	int enabled;
	u_int type;
{
	struct scsi_xfer *xs = scb->io_ctx;
	switch (type) {
	case MSG_SIMPLE_TASK:
	case MSG_ORDERED_TASK:
		if (enabled)
			xs->sc_link->quirks &= ~SDEV_NOTAGS;
		else
			xs->sc_link->quirks |= SDEV_NOTAGS;
		break;
	}
}

void ahc_platform_scb_free(ahc, scb)
	struct ahc_softc *ahc;
	struct scb *scb;
{
	int s;

	ahc_lock(ahc, &s);
	
	if ((ahc->flags & AHC_RESOURCE_SHORTAGE) != 0 ||
	    (scb->flags & SCB_RECOVERY_SCB) != 0) {
		ahc->flags &= ~AHC_RESOURCE_SHORTAGE;
		ahc->platform_data->queue_blocked = 0;
	}
	
	timeout_del(&scb->io_ctx->stimeout);
	
	ahc_unlock(ahc, &s);
}
#endif
