/*	$OpenBSD: mpt_openbsd.c,v 1.29 2005/11/28 23:24:31 krw Exp $	*/
/*	$NetBSD: mpt_netbsd.c,v 1.7 2003/07/14 15:47:11 lukem Exp $	*/

/*
 * Copyright (c) 2004 Milos Urbanek
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
 */

/*
 * Copyright (c) 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2000, 2001 by Greg Ansley
 * Partially derived from Matt Jacob's ISP driver.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */
/*
 * Additional Copyright (c) 2002 by Matthew Jacob under same license.
 */

/*
 * mpt_openbsd.c:
 *
 * OpenBSD-specific routines for LSI Fusion adapters.  Includes some
 * bus_dma glue, and SCSI glue.
 *
 * Adapted from the NetBSD "mpt" driver by Milos Urbanek for
 * ZOOM International, s.r.o.
 */

#include <sys/cdefs.h>
/* __KERNEL_RCSID(0, "$NetBSD: mpt_netbsd.c,v 1.7 2003/07/14 15:47:11 lukem Exp $"); */

#include <dev/ic/mpt.h>			/* pulls in all headers */

void	mpt_run_ppr(mpt_softc_t *, int);
int	mpt_ppr(mpt_softc_t *, struct scsi_link *, int, int);
int	mpt_poll(mpt_softc_t *, struct scsi_xfer *, int);
void	mpt_timeout(void *);
void	mpt_done(mpt_softc_t *, uint32_t);
int	mpt_run_xfer(mpt_softc_t *, struct scsi_xfer *);
void	mpt_check_xfer_settings(mpt_softc_t *, struct scsi_xfer *, MSG_SCSI_IO_REQUEST *);
void	mpt_ctlop(mpt_softc_t *, void *vmsg, uint32_t);
void	mpt_event_notify_reply(mpt_softc_t *, MSG_EVENT_NOTIFY_REPLY *);

int	mpt_action(struct scsi_xfer *);
void	mpt_minphys(struct buf *);

struct cfdriver mpt_cd = {
	NULL, "mpt", DV_DULL
};

/* the below structure is so we have a default dev struct for our link struct */
static struct scsi_device mpt_dev = {
	NULL, /* Use default error handler */
	NULL, /* have a queue, served by this */
	NULL, /* have no async handler */
	NULL, /* Use default 'done' routine */
};

enum mpt_scsi_speed { U320, U160, U80 };

/*
 * try speed and
 * return 0 if failed
 * return 1 if passed
 */
int
mpt_ppr(mpt_softc_t *mpt, struct scsi_link *sc_link, int speed, int flags)
{
	CONFIG_PAGE_SCSI_DEVICE_0 page0;
	CONFIG_PAGE_SCSI_DEVICE_1 page1;
	uint8_t tp;
	int error;
	struct scsi_inquiry_data inqbuf;

	if (mpt->verbose > 1) {
		mpt_prt(mpt, "Entering PPR");
	}

	if (mpt->is_fc) {
		/*
		 * SCSI transport settings don't make any sense for
		 * Fibre Channel; silently ignore the request.
		 */
		return 1; /* success */
	}

	/*
	 * Always allow disconnect; we don't have a way to disable
	 * it right now, in any case.
	 */
	mpt->mpt_disc_enable |= (1 << sc_link->target);

	/*
	 * Enable tagged queueing.
	 */
	if (sc_link->quirks & SDEV_NOTAGS)
		mpt->mpt_tag_enable &= ~(1 << sc_link->target);
	else
		mpt->mpt_tag_enable |= (1 << sc_link->target);

	page1 = mpt->mpt_dev_page1[sc_link->target];

	/*
	 * Set the wide/narrow parameter for the target.
	 */
	if (sc_link->quirks & SDEV_NOWIDE)
		page1.RequestedParameters &= ~MPI_SCSIDEVPAGE1_RP_WIDE;
	else {
		page1.RequestedParameters |= MPI_SCSIDEVPAGE1_RP_WIDE;
	}

	/*
	 * Set the synchronous parameters for the target.
	 */
	page1.RequestedParameters &=
	    ~(MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK |
	    MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK |
	    MPI_SCSIDEVPAGE1_RP_DT | MPI_SCSIDEVPAGE1_RP_QAS |
	    MPI_SCSIDEVPAGE1_RP_IU);

	if (!(sc_link->quirks & SDEV_NOSYNC)) {
		int factor, offset, np;

		/*
		 * Factor:
		 * 0x08 = U320 = 6.25ns
		 * 0x09 = U160 = 12.5ns
		 * 0x0a = U80 = 25ns
		 */
		factor = (mpt->mpt_port_page0.Capabilities >> 8) & 0xff;
		offset = (mpt->mpt_port_page0.Capabilities >> 16) & 0xff;
		np = 0;

		switch (speed) {
		case U320:
			/* do nothing */
			break;

		case U160:
			factor = 0x09; /* force U160 */
			break;

		case U80:
			factor = 0x0a; /* force U80 */
		}

		if (factor < 0x9) {
			/* Ultra320 enable QAS & IU */
			np |= MPI_SCSIDEVPAGE1_RP_QAS | MPI_SCSIDEVPAGE1_RP_IU;
		}
		if (factor < 0xa) {
			/* >= Ultra160 enable DT transfer */
			np |= MPI_SCSIDEVPAGE1_RP_DT;
		}
		np |= (factor << 8) | (offset << 16);
		page1.RequestedParameters |= np;
	}

	/* write parameters out to chip */
	if (mpt_write_cfg_page(mpt, sc_link->target, &page1.Header)) {
		mpt_prt(mpt, "unable to write Device Page 1");
		return 0;
	}

	/* make sure the parameters were written */
	if (mpt_read_cfg_page(mpt, sc_link->target, &page1.Header)) {
		mpt_prt(mpt, "unable to read back Device Page 1");
		return 0;
	}

	mpt->mpt_dev_page1[sc_link->target] = page1;
	if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Target %d Page 1: RequestedParameters %x Config %x",
		    sc_link->target,
		    mpt->mpt_dev_page1[sc_link->target].RequestedParameters,
		    mpt->mpt_dev_page1[sc_link->target].Configuration);
	}

	/*
	 * use INQUIRY for PPR two reasons:
	 * 1) actually transfer data at requested speed
	 * 2) no need to test for TUR QUIRK
	 */
	error = scsi_inquire(sc_link, &inqbuf, flags);
	if (error) {
		mpt_prt(mpt, "Invalid INQUIRY on target: %d", sc_link->target);
		return 0;
	}

	/* read page 0 back to figure out if the PPR worked */
	page0 = mpt->mpt_dev_page0[sc_link->target];
	if (mpt_read_cfg_page(mpt, sc_link->target, &page0.Header)) {
		mpt_prt(mpt, "unable to read Device Page 0");
		return 0;
	}

	if (mpt->verbose > 1) {
		mpt_prt(mpt,
		    "SPI Tgt %d Page 0: NParms %x Information %x",
		    sc_link->target,
		    page0.NegotiatedParameters, page0.Information);
	}

	if (!(page0.NegotiatedParameters & 0x07) && (speed == U320)) {
		/*
		 * if lowest 3 aren't set the PPR probably failed,
		 * retry with other parameters
		 */
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "U320 PPR failed");
		}
		return 0;
	}

	if ((((page0.NegotiatedParameters >> 8) & 0xff) > 0x09) &&
	    (speed == U160)) {
		/* if transfer period > 0x09 then U160 PPR failed, retry */
		if (mpt->verbose > 1) {
			mpt_prt(mpt, "U160 PPR failed");
		}
		return 0;
	}

	/*
	 * Bit 3 - PPR rejected:  IOC sets this if the device rejects PPR.
	 * Bit 2 - WDTR rejected: IOC sets this if the device rejects WDTR.
	 * Bit 1 - SDTR Rejected: IOC sets this if the device rejects SDTR.
	 * Bit 0 - 1 A SCSI SDTR, WDTR, or PPR negotiation has occurred.
	 */
	if (page0.Information & 0x0e) {
		/* target rejected PPR message */
		mpt_prt(mpt, "Target %d rejected PPR message with %02x",
		    sc_link->target,
		    (uint8_t)page0.Information);

		return 0;
	}

	/* print PPR results */
	switch ((page0.NegotiatedParameters >> 8) & 0xff) {
	case 0x08:
		tp = 160;
		break;

	case 0x09:
		tp = 80;
		break;

	case 0x0a:
		tp = 40;
		break;

	case 0x0b:
		tp = 20;
		break;

	case 0x0c:
		tp = 10;
		break;

	default:
		tp = 0;
	}

	mpt_prt(mpt,
	    "target %d %s at %dMHz width %dbit offset %d QAS %d DT %d IU %d",
	    sc_link->target,
	    tp ? "Synchronous" : "Asynchronous",
	    tp,
	    (page0.NegotiatedParameters & 0x20000000) ? 16 : 8,
	    (page0.NegotiatedParameters >> 16) & 0xff,
	    (page0.NegotiatedParameters & 0x04) ? 1 : 0,
	    (page0.NegotiatedParameters & 0x02) ? 1 : 0,
	    (page0.NegotiatedParameters & 0x01) ? 1 : 0);

	return 1; /* success */
}

/*
 * Run PPR on all attached devices
 */
void
mpt_run_ppr(mpt_softc_t *mpt, int flags)
{
	struct scsi_link *sc_link;
	struct device *dev;
	u_int8_t target;
	u_int16_t buswidth;

	/* walk device list */
	for (dev = TAILQ_FIRST(&alldevs); dev != NULL;
	    dev = TAILQ_NEXT(dev, dv_list)) {
		if (dev->dv_parent == (struct device *)mpt) {
			/* found scsibus softc */
			buswidth = ((struct scsi_link *)&mpt->sc_link)->
			    adapter_buswidth;
			/* printf("mpt_softc: %x  scsibus: %x  buswidth: %d\n",
			 *     mpt, dev, buswidth); */
			/* walk target list */
			for (target = 0; target < buswidth; target++) {
				sc_link = ((struct scsibus_softc *)dev)->
				    sc_link[target][0];
				if ((sc_link != NULL)) {
					/* got a device! run PPR */
					/* skip CPU devices since they
					 * can crash at U320 speeds */
					if ((sc_link->inqdata.device & SID_TYPE)
					    == T_PROCESSOR) {
						continue;
					}
					if (mpt_ppr(mpt, sc_link, U320, flags)){
						mpt->mpt_negotiated_speed
						    [target] = U320;
						continue;
					}

					if (mpt_ppr(mpt, sc_link, U160, flags)){
						mpt->mpt_negotiated_speed
						    [target] = U160;
						continue;
					}

					if (mpt_ppr(mpt, sc_link, U80, flags)) {
						mpt->mpt_negotiated_speed
						    [target] = U80;
						continue;
					}
				} /* sc_link */
			} /* for target */
		} /* if dev */
	} /* end for dev */
}

/*
 * Complete attachment of hardware, include subdevices.
 */
void
mpt_attach(mpt_softc_t *mpt)
{
	struct scsi_link *lptr = &mpt->sc_link;

	mpt->bus = 0;		/* XXX ?? */

	/* Fill in the scsi_adapter. */
	mpt->sc_adapter.scsi_cmd = mpt_action;
	mpt->sc_adapter.scsi_minphys = mpt_minphys;

	/* Fill in the prototype scsi_link */
	lptr->adapter_softc = mpt;
	lptr->device = &mpt_dev;
	lptr->adapter = &mpt->sc_adapter;
	lptr->flags = 0;
	lptr->luns = 8;

	if (mpt->is_fc) {
		lptr->adapter_buswidth = 256;
		lptr->adapter_target = 256;
	} else {
		lptr->adapter_buswidth = 16;
		lptr->adapter_target = mpt->mpt_ini_id;
	}
	lptr->openings = MPT_MAX_REQUESTS(mpt) / lptr->adapter_buswidth;

#ifdef MPT_DEBUG
	mpt->verbose = 2;
#endif

	mpt_prt(mpt, "IM support: %x", mpt->im_support);

	(void) config_found(&mpt->mpt_dev, lptr, scsiprint);

	/* done attaching now walk targets and PPR them */
	/* FC does not do PPR */
	if (!mpt->is_fc) {
		mpt_run_ppr(mpt, SCSI_POLL);
	}
}

int
mpt_dma_mem_alloc(mpt_softc_t *mpt)
{
	bus_dma_segment_t reply_seg, request_seg;
	int reply_rseg, request_rseg;
	bus_addr_t pptr, end;
	caddr_t vptr;
	size_t len;
	int error, i;

	/* Check if we have already allocated the reply memory. */
	if (mpt->reply != NULL)
		return (0);

	/*
	 * Allocate the request pool.  This isn't really DMA'd memory,
	 * but it's a convenient place to do it.
	 */
	len = sizeof(request_t) * MPT_MAX_REQUESTS(mpt);
	mpt->request_pool = malloc(len, M_DEVBUF, M_WAITOK);
	if (mpt->request_pool == NULL) {
		printf("%s: unable to allocate request pool\n",
		    mpt->mpt_dev.dv_xname);
		return (ENOMEM);
	}
	bzero(mpt->request_pool, len);
	/*
	 * Allocate DMA resources for reply buffers.
	 */
	error = bus_dmamem_alloc(mpt->sc_dmat, PAGE_SIZE, PAGE_SIZE, 0,
	    &reply_seg, 1, &reply_rseg, 0);
	if (error) {
		printf("%s: unable to allocate reply area, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_0;
	}

	error = bus_dmamem_map(mpt->sc_dmat, &reply_seg, reply_rseg, PAGE_SIZE,
	    (caddr_t *) &mpt->reply, BUS_DMA_COHERENT/*XXX*/);
	if (error) {
		printf("%s: unable to map reply area, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_1;
	}

	error = bus_dmamap_create(mpt->sc_dmat, PAGE_SIZE, 1, PAGE_SIZE,
	    0, 0, &mpt->reply_dmap);
	if (error) {
		printf("%s: unable to create reply DMA map, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_2;
	}

	error = bus_dmamap_load(mpt->sc_dmat, mpt->reply_dmap, mpt->reply,
	    PAGE_SIZE, NULL, 0);
	if (error) {
		printf("%s: unable to load reply DMA map, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_3;
	}
	mpt->reply_phys = mpt->reply_dmap->dm_segs[0].ds_addr;

	/*
	 * Allocate DMA resources for request buffers.
	 */
	error = bus_dmamem_alloc(mpt->sc_dmat, MPT_REQ_MEM_SIZE(mpt),
	    PAGE_SIZE, 0, &request_seg, 1, &request_rseg, 0);
	if (error) {
		printf("%s: unable to allocate request area, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_4;
	}

	error = bus_dmamem_map(mpt->sc_dmat, &request_seg, request_rseg,
	    MPT_REQ_MEM_SIZE(mpt), (caddr_t *) &mpt->request, 0);
	if (error) {
		printf("%s: unable to map request area, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_5;
	}

	error = bus_dmamap_create(mpt->sc_dmat, MPT_REQ_MEM_SIZE(mpt), 1,
	    MPT_REQ_MEM_SIZE(mpt), 0, 0, &mpt->request_dmap);
	if (error) {
		printf("%s: unable to create request DMA map, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_6;
	}

	error = bus_dmamap_load(mpt->sc_dmat, mpt->request_dmap, mpt->request,
	    MPT_REQ_MEM_SIZE(mpt), NULL, 0);
	if (error) {
		printf("%s: unable to load request DMA map, error = %d\n",
		    mpt->mpt_dev.dv_xname, error);
		goto fail_7;
	}
	mpt->request_phys = mpt->request_dmap->dm_segs[0].ds_addr;

	pptr = mpt->request_phys;
	vptr = (caddr_t) mpt->request;
	end = pptr + MPT_REQ_MEM_SIZE(mpt);

	for (i = 0; pptr < end; i++) {
		request_t *req = &mpt->request_pool[i];
		req->index = i;

		/* Store location of Request Data */
		req->req_pbuf = pptr;
		req->req_vbuf = vptr;

		pptr += MPT_REQUEST_AREA;
		vptr += MPT_REQUEST_AREA;

		req->sense_pbuf = (pptr - MPT_SENSE_SIZE);
		req->sense_vbuf = (vptr - MPT_SENSE_SIZE);

		error = bus_dmamap_create(mpt->sc_dmat, MAXPHYS,
		    MPT_SGL_MAX, MAXPHYS, 0, 0, &req->dmap);
		if (error) {
			printf("%s: unable to create req %d DMA map, error = ",
			    "%d", mpt->mpt_dev.dv_xname, i, error);
			goto fail_8;
		}
	}

	return (0);

 fail_8:
	for (--i; i >= 0; i--) {
		request_t *req = &mpt->request_pool[i];
		if (req->dmap != NULL)
			bus_dmamap_destroy(mpt->sc_dmat, req->dmap);
	}
	bus_dmamap_unload(mpt->sc_dmat, mpt->request_dmap);
 fail_7:
	bus_dmamap_destroy(mpt->sc_dmat, mpt->request_dmap);
 fail_6:
	bus_dmamem_unmap(mpt->sc_dmat, (caddr_t)mpt->request, PAGE_SIZE);
 fail_5:
	bus_dmamem_free(mpt->sc_dmat, &request_seg, request_rseg);
 fail_4:
	bus_dmamap_unload(mpt->sc_dmat, mpt->reply_dmap);
 fail_3:
	bus_dmamap_destroy(mpt->sc_dmat, mpt->reply_dmap);
 fail_2:
	bus_dmamem_unmap(mpt->sc_dmat, (caddr_t)mpt->reply, PAGE_SIZE);
 fail_1:
	bus_dmamem_free(mpt->sc_dmat, &reply_seg, reply_rseg);
 fail_0:
	free(mpt->request_pool, M_DEVBUF);

	mpt->reply = NULL;
	mpt->request = NULL;
	mpt->request_pool = NULL;

	return (error);
}

int
mpt_intr(void *arg)
{
	mpt_softc_t *mpt = arg;
	int nrepl = 0;
	uint32_t reply;

	/*
	if ((mpt_read(mpt, MPT_OFFSET_INTR_STATUS) & MPT_INTR_REPLY_READY) == 0)
		return (0);
	*/

	/*
	 * Speed up trick to save one PCI read.
	 * Reply FIFO replies 0xffffffff whenever
	 * MPT_OFFSET_INTR_STATUS & MPT_INTR_REPLY_READY == 0
	 *
	 */

	reply = mpt_pop_reply_queue(mpt);

	if (reply == 0xffffffff) {
		/* check doorbell, this is error path not IO path */
		/* FIXME for now ignore strays and doorbells */
		return (0);
	}

	while (reply != MPT_REPLY_EMPTY) {
		nrepl++;
		if (mpt->verbose > 1) {
			if ((reply & MPT_CONTEXT_REPLY) != 0) {
				/* Address reply; IOC has something to say */
				mpt_print_reply(MPT_REPLY_PTOV(mpt, reply));
			} else {
				/* Context reply; all went well */
				mpt_prt(mpt, "context %u reply OK", reply);
			}
		}
		mpt_done(mpt, reply);
		reply = mpt_pop_reply_queue(mpt);
	}
	return (nrepl != 0);
}

void
mpt_prt(mpt_softc_t *mpt, const char *fmt, ...)
{
	va_list ap;

	printf("%s: ", mpt->mpt_dev.dv_xname);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

int
mpt_poll(mpt_softc_t *mpt, struct scsi_xfer *xs, int count)
{

	/* Timeouts are in msec, so we loop in 1000usec cycles */
	while (count) {
		mpt_intr(mpt);
		if (xs->flags & ITSDONE) {
			return (0);
		}
		delay(1000);		/* only happens in boot, so ok */
		count--;
	}
	return (1);
}

void
mpt_timeout(void *arg)
{
	request_t *req = arg;
	struct scsi_xfer *xs = req->xfer;
	struct scsi_link *linkp = xs->sc_link;
	mpt_softc_t *mpt = (void *) linkp->adapter_softc;
	uint32_t oseq;
	int s, index;

	mpt_prt(mpt, "command timeout");
	sc_print_addr(linkp);

	s = splbio();

	oseq = req->sequence;
	mpt->timeouts++;
	if (mpt_intr(mpt)) {
		if (req->sequence != oseq) {
			mpt_prt(mpt, "recovered from command timeout");
			splx(s);
			return;
		}
	}
	mpt_prt(mpt,
	    "timeout on request index = 0x%x, seq = 0x%08x",
	    req->index, req->sequence);
	mpt_check_doorbell(mpt);
	mpt_prt(mpt, "Status 0x%08x, Mask 0x%08x, Doorbell 0x%08x",
	    mpt_read(mpt, MPT_OFFSET_INTR_STATUS),
	    mpt_read(mpt, MPT_OFFSET_INTR_MASK),
	    mpt_read(mpt, MPT_OFFSET_DOORBELL));
	mpt_prt(mpt, "request state: %s", mpt_req_state(req->debug));
	if (mpt->verbose > 1)
		mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);

	for(index = 0; index < MPT_MAX_REQUESTS(mpt); index++)
		if (req == &mpt->request_pool[index]) {
			req->debug = REQ_TIMEOUT;
			break;
		}

	mpt_done(mpt, index);

	splx(s);
}

void
mpt_done(mpt_softc_t *mpt, uint32_t reply)
{
	struct scsi_xfer *xs = NULL;
	struct scsi_link *linkp;
	int index;
	request_t *req;
	MSG_REQUEST_HEADER *mpt_req;
	MSG_SCSI_IO_REPLY *mpt_reply;


	if ((reply & MPT_CONTEXT_REPLY) == 0) {
		/* context reply (ok) */
		mpt_reply = NULL;
		index = reply & MPT_CONTEXT_MASK;
	} else {
		/* address reply (error) */

		/* XXX BUS_DMASYNC_POSTREAD XXX */
		mpt_reply = MPT_REPLY_PTOV(mpt, reply);
		if (mpt->verbose > 1) {
			uint32_t *pReply = (uint32_t *) mpt_reply;

			mpt_prt(mpt, "Address Reply (index %u):",
			    mpt_reply->MsgContext & 0xffff);
			mpt_prt(mpt, "%08x %08x %08x %08x",
			    pReply[0], pReply[1], pReply[2], pReply[3]);
			mpt_prt(mpt, "%08x %08x %08x %08x",
			    pReply[4], pReply[5], pReply[6], pReply[7]);
			mpt_prt(mpt, "%08x %08x %08x %08x",
			    pReply[8], pReply[9], pReply[10], pReply[11]);
		}
		index = mpt_reply->MsgContext;
	}

	/*
	 * Address reply with MessageContext high bit set.
	 * This is most likely a notify message, so we try
	 * to process it, then free it.
	 */
	if ((index & 0x80000000) != 0) {
		if (mpt_reply != NULL)
			mpt_ctlop(mpt, mpt_reply, reply);
		else
			mpt_prt(mpt, "mpt_done: index 0x%x, NULL reply", index);
		return;
	}

	/* Did we end up with a valid index into the table? */
	if (index < 0 || index >= MPT_MAX_REQUESTS(mpt)) {
		mpt_prt(mpt, "mpt_done: invalid index (0x%x) in reply", index);
		return;
	}

	req = &mpt->request_pool[index];

	/* Make sure memory hasn't been trashed. */
	if (req->index != index) {
		mpt_prt(mpt, "mpt_done: corrupted request_t (0x%x)", index);
		return;
	}

	MPT_SYNC_REQ(mpt, req, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	mpt_req = req->req_vbuf;

	/* Short cut for task management replies; nothing more for us to do. */
	if (mpt_req->Function == MPI_FUNCTION_SCSI_TASK_MGMT) {
		if (mpt->verbose > 1)
			mpt_prt(mpt, "mpt_done: TASK MGMT");
		goto done;
	}

	if (mpt_req->Function == MPI_FUNCTION_PORT_ENABLE)
		goto done;

	/*
	 * At this point, it had better be a SCSI I/O command, but don't
	 * crash if it isn't.
	 */
	if (mpt_req->Function != MPI_FUNCTION_SCSI_IO_REQUEST) {
		if (mpt->verbose > 1)
			mpt_prt(mpt, "mpt_done: unknown Function 0x%x (0x%x)",
			    mpt_req->Function, index);
		goto done;
	}

	/* Recover scsi_xfer from the request structure. */
	xs = req->xfer;

	/* Can't have a SCSI command without a scsi_xfer. */
	if (xs == NULL) {
		mpt_prt(mpt,
		    "mpt_done: no scsi_xfer, index = 0x%x, seq = 0x%08x",
		    req->index, req->sequence);
		mpt_prt(mpt, "request state: %s", mpt_req_state(req->debug));
		mpt_prt(mpt, "mpt_request:");
		mpt_print_scsi_io_request((MSG_SCSI_IO_REQUEST *)req->req_vbuf);

		if (mpt_reply != NULL) {
			mpt_prt(mpt, "mpt_reply:");
			mpt_print_reply(mpt_reply);
		} else {
			mpt_prt(mpt, "context reply: 0x%08x", reply);
		}
		goto done;
	}

	timeout_del(&xs->stimeout);

	linkp = xs->sc_link;

	/*
	 * If we were a data transfer, unload the map that described
	 * the data buffer.
	 */
	if (xs->datalen != 0) {
		bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
		    req->dmap->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD
						      : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(mpt->sc_dmat, req->dmap);
	}

	if (req->debug == REQ_TIMEOUT) {
		xs->error = XS_TIMEOUT;
		xs->status = SCSI_OK;
		xs->resid = 0;
		goto done;
	} else if (mpt_reply == NULL) {
		/*
		 * Context reply; report that the command was
		 * successful!
		 *
		 * Also report the xfer mode, if necessary.
		 */
		xs->error = XS_NOERROR;
		xs->status = SCSI_OK;
		xs->resid = 0;
		goto done;
	}

	xs->status = mpt_reply->SCSIStatus;
	switch (mpt_reply->IOCStatus) {
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:
		/*
		 * Yikes!  Tagged queue full comes through this path!
		 *
		 * So we'll change it to a status error and anything
		 * that returns status should probably be a status
		 * error as well.
		 */
		xs->resid = xs->datalen - mpt_reply->TransferCount;
		if (mpt_reply->SCSIState &
		    MPI_SCSI_STATE_NO_SCSI_STATUS) {
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/* FALLTHROUGH */
	case MPI_IOCSTATUS_SUCCESS:
	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (xs->status) {
		case SCSI_OK:
			xs->resid = 0;
			break;

		case SCSI_CHECK:
			xs->error = XS_SENSE;
			break;

		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;

		case SCSI_QUEUE_FULL:
			xs->error = XS_TIMEOUT;
			xs->retries++;
			break;
		default:
			sc_print_addr(linkp);
			mpt_prt(mpt, "invalid status code %d", xs->status);
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case MPI_IOCSTATUS_BUSY:
	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:
		xs->error = XS_BUSY;
		break;

	case MPI_IOCSTATUS_SCSI_INVALID_BUS:
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		xs->error = XS_SELTIMEOUT;
		break;

	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:
		/* XXX This is a bus-reset */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	default:
		/* XXX unrecognized HBA error */
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (mpt_reply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_VALID) {
		memcpy(&xs->sense, req->sense_vbuf,
		    sizeof(xs->sense));
	} else if (mpt_reply->SCSIState & MPI_SCSI_STATE_AUTOSENSE_FAILED) {
		/*
		 * This will cause the scsi layer to issue
		 * a REQUEST SENSE.
		 */
		if (xs->status == SCSI_CHECK)
			xs->error = XS_BUSY;
	}

 done:
	/* If IOC done with this requeset, free it up. */
	if (mpt_reply == NULL || (mpt_reply->MsgFlags & 0x80) == 0)
		mpt_free_request(mpt, req);

	/* If address reply, give the buffer back to the IOC. */
	if (mpt_reply != NULL)
		mpt_free_reply(mpt, (reply << 1));

	if (xs != NULL) {
		xs->flags |= ITSDONE;
		scsi_done(xs);
	}
}

int
mpt_run_xfer(mpt_softc_t *mpt, struct scsi_xfer *xs)
{
	struct scsi_link *linkp = xs->sc_link;
	request_t *req;
	MSG_SCSI_IO_REQUEST *mpt_req;
	int error, s;

	s = splbio();
	req = mpt_get_request(mpt);
	if (req == NULL) {
		/* This should happen very infrequently. */
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		splx(s);
		return (COMPLETE);
	}
	splx(s);

	/* Link the req and the scsi_xfer. */
	req->xfer = xs;

	/* Now we build the command for the IOC */
	mpt_req = req->req_vbuf;
	bzero(mpt_req, sizeof(*mpt_req));

	mpt_req->Function = MPI_FUNCTION_SCSI_IO_REQUEST;
	mpt_req->Bus = mpt->bus;

	mpt_req->SenseBufferLength =
	    (sizeof(xs->sense) < MPT_SENSE_SIZE) ?
	    sizeof(xs->sense) : MPT_SENSE_SIZE;

	/*
	 * We use the message context to find the request structure when
	 * we get the command completion interrupt from the IOC.
	 */
	mpt_req->MsgContext = req->index;

	/* Which physical device to do the I/O on. */
	mpt_req->TargetID = linkp->target;
	mpt_req->LUN[1] = linkp->lun;

	/* Set the direction of the transfer. */
	if (xs->flags & SCSI_DATA_IN)
		mpt_req->Control = MPI_SCSIIO_CONTROL_READ;
	else if (xs->flags & SCSI_DATA_OUT)
		mpt_req->Control = MPI_SCSIIO_CONTROL_WRITE;
	else
		mpt_req->Control = MPI_SCSIIO_CONTROL_NODATATRANSFER;

	mpt_check_xfer_settings(mpt, xs, mpt_req);

	/* Copy the SCSI command block into place. */
	memcpy(mpt_req->CDB, xs->cmd, xs->cmdlen);

	mpt_req->CDBLength = xs->cmdlen;
	mpt_req->DataLength = xs->datalen;
	mpt_req->SenseBufferLowAddr = req->sense_pbuf;

	/*
	 * Map the DMA transfer.
	 */
	if (xs->datalen) {
		SGE_SIMPLE32 *se;

		error = bus_dmamap_load(mpt->sc_dmat, req->dmap, xs->data,
		    xs->datalen, NULL,
		    ((xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT
						       : BUS_DMA_WAITOK) |
		    BUS_DMA_STREAMING |
		    ((xs->flags & SCSI_DATA_IN) ? BUS_DMA_READ
						       : BUS_DMA_WRITE));
		switch (error) {
		case 0:
			break;

		case ENOMEM:
		case EAGAIN:
			xs->error = XS_DRIVER_STUFFUP;
			goto out_bad;
		default:
			mpt_prt(mpt, "error %d loading DMA map", error);
 out_bad:
			s = splbio();
			mpt_free_request(mpt, req);
			splx(s);
			return (TRY_AGAIN_LATER);
		}

		if (req->dmap->dm_nsegs > MPT_NSGL_FIRST(mpt)) {
			int seg, i, nleft = req->dmap->dm_nsegs;
			uint32_t flags;
			SGE_CHAIN32 *ce;

			seg = 0;

			mpt_req->DataLength = xs->datalen;
			flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
			if (xs->flags & SCSI_DATA_OUT)
				flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

			se = (SGE_SIMPLE32 *) &mpt_req->SGL;
			for (i = 0; i < MPT_NSGL_FIRST(mpt) - 1;
			    i++, se++, seg++) {
				uint32_t tf;

				bzero(se, sizeof(*se));
				se->Address = req->dmap->dm_segs[seg].ds_addr;
				MPI_pSGE_SET_LENGTH(se,
				    req->dmap->dm_segs[seg].ds_len);
				tf = flags;
				if (i == MPT_NSGL_FIRST(mpt) - 2)
					tf |= MPI_SGE_FLAGS_LAST_ELEMENT;
				MPI_pSGE_SET_FLAGS(se, tf);
				nleft--;
			}

			/*
			 * Tell the IOC where to find the first chain element.
			 */
			mpt_req->ChainOffset =
			    ((char *)se - (char *)mpt_req) >> 2;

			/*
			 * Until we're finished with all segments...
			 */
			while (nleft) {
				int ntodo;

				/*
				 * Construct the chain element that points to
				 * the next segment.
				 */
				ce = (SGE_CHAIN32 *) se++;
				if (nleft > MPT_NSGL(mpt)) {
					ntodo = MPT_NSGL(mpt) - 1;
					ce->NextChainOffset = (MPT_RQSL(mpt) -
					    sizeof(SGE_SIMPLE32)) >> 2;
					ce->Length = MPT_NSGL(mpt)
						* sizeof(SGE_SIMPLE32);
				} else {
					ntodo = nleft;
					ce->NextChainOffset = 0;
					ce->Length = ntodo
						* sizeof(SGE_SIMPLE32);
				}
				ce->Address = req->req_pbuf +
				    ((char *)se - (char *)mpt_req);
				ce->Flags = MPI_SGE_FLAGS_CHAIN_ELEMENT;
				for (i = 0; i < ntodo; i++, se++, seg++) {
					uint32_t tf;

					bzero(se, sizeof(*se));
					se->Address =
					    req->dmap->dm_segs[seg].ds_addr;
					MPI_pSGE_SET_LENGTH(se,
					    req->dmap->dm_segs[seg].ds_len);
					tf = flags;
					if (i == ntodo - 1) {
						tf |=
						    MPI_SGE_FLAGS_LAST_ELEMENT;
						if (ce->NextChainOffset == 0) {
							tf |=
						    MPI_SGE_FLAGS_END_OF_LIST |
						    MPI_SGE_FLAGS_END_OF_BUFFER;
						}
					}
					MPI_pSGE_SET_FLAGS(se, tf);
					nleft--;
				}
			}
			bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
			    req->dmap->dm_mapsize,
			    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);
		} else {
			int i;
			uint32_t flags;

			mpt_req->DataLength = xs->datalen;
			flags = MPI_SGE_FLAGS_SIMPLE_ELEMENT;
			if (xs->flags & SCSI_DATA_OUT)
				flags |= MPI_SGE_FLAGS_HOST_TO_IOC;

			/* Copy the segments into our SG list. */
			se = (SGE_SIMPLE32 *) &mpt_req->SGL;
			for (i = 0; i < req->dmap->dm_nsegs;
			    i++, se++) {
				uint32_t tf;

				bzero(se, sizeof(*se));
				se->Address = req->dmap->dm_segs[i].ds_addr;
				MPI_pSGE_SET_LENGTH(se,
				    req->dmap->dm_segs[i].ds_len);
				tf = flags;
				if (i == req->dmap->dm_nsegs - 1) {
					tf |=
					    MPI_SGE_FLAGS_LAST_ELEMENT |
					    MPI_SGE_FLAGS_END_OF_BUFFER |
					    MPI_SGE_FLAGS_END_OF_LIST;
				}
				MPI_pSGE_SET_FLAGS(se, tf);
			}
			bus_dmamap_sync(mpt->sc_dmat, req->dmap, 0,
			    req->dmap->dm_mapsize,
			    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
			    BUS_DMASYNC_PREWRITE);
		}
	} else {
		/*
		 * No data to transfer; just make a single simple SGL
		 * with zero length.
		 */
		SGE_SIMPLE32 *se = (SGE_SIMPLE32 *) &mpt_req->SGL;
		bzero(se, sizeof(*se));
		MPI_pSGE_SET_FLAGS(se,
		    (MPI_SGE_FLAGS_LAST_ELEMENT | MPI_SGE_FLAGS_END_OF_BUFFER |
		    MPI_SGE_FLAGS_SIMPLE_ELEMENT | MPI_SGE_FLAGS_END_OF_LIST));
	}

	if (mpt->verbose > 1)
		mpt_print_scsi_io_request(mpt_req);

	s = splbio();

	/* Always reset xs->stimeout, lest we timeout_del() with trash */
	timeout_set(&xs->stimeout, mpt_timeout, req);

	if ((xs->flags & SCSI_POLL) == 0)
		timeout_add(&xs->stimeout, mstohz(xs->timeout));
	mpt_send_cmd(mpt, req);
	splx(s);

	if ((xs->flags & SCSI_POLL) == 0) {
		return (SUCCESSFULLY_QUEUED);
	}
	/*
	 * If we can't use interrupts, poll on completion.
	 */
	if (mpt_poll(mpt, xs, xs->timeout)) {
		mpt_timeout(req);
		/* XXX scsi_done called
		return (TRY_AGAIN_LATER);
		*/
		/* XXX MP this does not look correct */
		return (COMPLETE);
	}

	return (COMPLETE);
}

void
mpt_ctlop(mpt_softc_t *mpt, void *vmsg, uint32_t reply)
{
	MSG_DEFAULT_REPLY *dmsg = vmsg;

	switch (dmsg->Function) {
	case MPI_FUNCTION_EVENT_NOTIFICATION:
		mpt_event_notify_reply(mpt, vmsg);
		mpt_free_reply(mpt, (reply << 1));
		break;

	case MPI_FUNCTION_EVENT_ACK:
		mpt_free_reply(mpt, (reply << 1));
		break;

	case MPI_FUNCTION_PORT_ENABLE:
	    {
		MSG_PORT_ENABLE_REPLY *msg = vmsg;
		int index = msg->MsgContext & ~0x80000000;
		if (mpt->verbose > 1)
			mpt_prt(mpt, "enable port reply index %d", index);
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
		}
		mpt_free_reply(mpt, (reply << 1));
		break;
	    }

	case MPI_FUNCTION_CONFIG:
	    {
		MSG_CONFIG_REPLY *msg = vmsg;
		int index = msg->MsgContext & ~0x80000000;
		if (index >= 0 && index < MPT_MAX_REQUESTS(mpt)) {
			request_t *req = &mpt->request_pool[index];
			req->debug = REQ_DONE;
			req->sequence = reply;
		} else
			mpt_free_reply(mpt, (reply << 1));
		break;
	    }

	default:
		mpt_prt(mpt, "unknown ctlop: 0x%x", dmsg->Function);
	}
}

void
mpt_event_notify_reply(mpt_softc_t *mpt, MSG_EVENT_NOTIFY_REPLY *msg)
{

	switch (msg->Event) {
	case MPI_EVENT_LOG_DATA:
	    {
		int i;

		/* Some error occurrerd that the Fusion wants logged. */
		mpt_prt(mpt, "EvtLogData: IOCLogInfo: 0x%08x", msg->IOCLogInfo);
		mpt_prt(mpt, "EvtLogData: Event Data:");
		for (i = 0; i < msg->EventDataLength; i++) {
			if ((i % 4) == 0)
				printf("%s:\t", mpt->mpt_dev.dv_xname);
			printf("0x%08x%c", msg->Data[i],
			    ((i % 4) == 3) ? '\n' : ' ');
		}
		if ((i % 4) != 0)
			printf("\n");
		break;
	    }

	case MPI_EVENT_UNIT_ATTENTION:
		mpt_prt(mpt, "Unit Attn: Bus 0x%02x Target 0x%02x",
		    (msg->Data[0] >> 8) & 0xff, msg->Data[0] & 0xff);
		break;

	case MPI_EVENT_IOC_BUS_RESET:
		/* We generated a bus reset. */
		mpt_prt(mpt, "IOC Bus Reset Port %d",
		    (msg->Data[0] >> 8) & 0xff);
		break;

	case MPI_EVENT_EXT_BUS_RESET:
		/* Someone else generated a bus reset. */
		mpt_prt(mpt, "External Bus Reset");
		/*
		 * These replies don't return EventData like the MPI
		 * spec says they do.
		 */
		/* XXX Send an async event? */
		break;

	case MPI_EVENT_RESCAN:
		/*
		 * In general, thise means a device has been added
		 * to the loop.
		 */
		mpt_prt(mpt, "Rescan Port %d", (msg->Data[0] >> 8) & 0xff);
		/* XXX Send an async event? */
		break;

	case MPI_EVENT_LINK_STATUS_CHANGE:
		mpt_prt(mpt, "Port %d: Link state %s",
		    (msg->Data[1] >> 8) & 0xff,
		    (msg->Data[0] & 0xff) == 0 ? "Failed" : "Active");
		break;

	case MPI_EVENT_LOOP_STATE_CHANGE:
		switch ((msg->Data[0] >> 16) & 0xff) {
		case 0x01:
			mpt_prt(mpt,
			    "Port %d: FC Link Event: LIP(%02x,%02x) "
			    "(Loop Initialization)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			switch ((msg->Data[0] >> 8) & 0xff) {
			case 0xf7:
				if ((msg->Data[0] & 0xff) == 0xf7)
					mpt_prt(mpt, "\tDevice needs AL_PA");
				else
					mpt_prt(mpt, "\tDevice %02x doesn't "
					    "like FC performance",
					    msg->Data[0] & 0xff);
				break;

			case 0xf8:
				if ((msg->Data[0] & 0xff) == 0xf7)
					mpt_prt(mpt, "\tDevice detected loop "
					    "failure before acquiring AL_PA");
				else
					mpt_prt(mpt, "\tDevice %02x detected "
					    "loop failure",
					    msg->Data[0] & 0xff);
				break;

			default:
				mpt_prt(mpt, "\tDevice %02x requests that "
				    "device %02x reset itself",
				    msg->Data[0] & 0xff,
				    (msg->Data[0] >> 8) & 0xff);
				break;
			}
			break;

		case 0x02:
			mpt_prt(mpt, "Port %d: FC Link Event: LPE(%02x,%02x) "
			    "(Loop Port Enable)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			break;

		case 0x03:
			mpt_prt(mpt, "Port %d: FC Link Event: LPB(%02x,%02x) "
			    "(Loop Port Bypass)",
			    (msg->Data[1] >> 8) & 0xff,
			    (msg->Data[0] >> 8) & 0xff,
			    (msg->Data[0]     ) & 0xff);
			break;

		default:
			mpt_prt(mpt, "Port %d: FC Link Event: "
			    "Unknown event (%02x %02x %02x)",
			    (msg->Data[1] >>  8) & 0xff,
			    (msg->Data[0] >> 16) & 0xff,
			    (msg->Data[0] >>  8) & 0xff,
			    (msg->Data[0]      ) & 0xff);
			break;
		}
		break;

	case MPI_EVENT_LOGOUT:
		mpt_prt(mpt, "Port %d: FC Logout: N_PortID: %02x",
		    (msg->Data[1] >> 8) & 0xff, msg->Data[0]);
		break;

	case MPI_EVENT_EVENT_CHANGE:
		/*
		 * This is just an acknowledgement of our
		 * mpt_send_event_request().
		 */
		break;

	default:
		mpt_prt(mpt, "Unknown async event: 0x%x", msg->Event);
		break;
	}

	if (msg->AckRequired) {
		MSG_EVENT_ACK *ackp;
		request_t *req;

		if ((req = mpt_get_request(mpt)) == NULL) {
			/* XXX XXX XXX XXXJRT */
			panic("mpt_event_notify_reply: unable to allocate "
			    "request structure");
		}

		ackp = (MSG_EVENT_ACK *) req->req_vbuf;
		bzero(ackp, sizeof(*ackp));
		ackp->Function = MPI_FUNCTION_EVENT_ACK;
		ackp->Event = msg->Event;
		ackp->EventContext = msg->EventContext;
		ackp->MsgContext = req->index | 0x80000000;
		mpt_check_doorbell(mpt);
		mpt_send_cmd(mpt, req);
	}
}

void
mpt_check_xfer_settings(mpt_softc_t *mpt, struct scsi_xfer *xs, MSG_SCSI_IO_REQUEST *mpt_req)
{
	if (mpt->is_fc) {
		/*
		 * SCSI transport settings don't make any sense for
		 * Fibre Channel; silently ignore the request.
		 */
		return;
	}

	/*
	 * XXX never do these commands with tags. Should really be
	 * in a higher layer.
	 */
	if (xs->cmd->opcode == INQUIRY ||
	    xs->cmd->opcode == TEST_UNIT_READY ||
	    xs->cmd->opcode == REQUEST_SENSE)
		return;

	/* Set the queue behavior. */
	if (mpt->is_fc || (mpt->mpt_tag_enable & (1 << xs->sc_link->target))) {
			mpt_req->Control |= MPI_SCSIIO_CONTROL_SIMPLEQ;
	} else {
		mpt_req->Control |= MPI_SCSIIO_CONTROL_UNTAGGED;
		mpt_req->Control |= MPI_SCSIIO_CONTROL_NO_DISCONNECT;
	}
}

/*****************************************************************************
 * SCSI interface routines
 *****************************************************************************/

int
mpt_action(struct scsi_xfer *xfer)
{
	mpt_softc_t *mpt = (void *) xfer->sc_link->adapter_softc;
	int ret;

	ret = mpt_run_xfer(mpt, xfer);

	return ret;
}

void
mpt_minphys(struct buf *bp)
{

/*
 * Subtract one from the SGL limit, since we need an extra one to handle
 * an non-page-aligned transfer.
 */
#define	MPT_MAX_XFER	((MPT_SGL_MAX - 1) * PAGE_SIZE)

	if (bp->b_bcount > MPT_MAX_XFER)
		bp->b_bcount = MPT_MAX_XFER;
	minphys(bp);
}

/*
 * Allocate DMA resources for FW image
 *
 * img_sz : size of image
 * maxsgl : maximum number of DMA segments
 */
int
mpt_alloc_fw_mem(mpt_softc_t *mpt, uint32_t img_sz, int maxsgl)
{
	int error;

	error = bus_dmamap_create(mpt->sc_dmat, img_sz, maxsgl, img_sz,
	    0, 0, &mpt->fw_dmap);
	if (error) {
		mpt_prt(mpt, "unable to create request DMA map, error = %d",
		    error);
		goto fw_fail0;
	}

	error = bus_dmamem_alloc(mpt->sc_dmat, img_sz, PAGE_SIZE, 0,
		&mpt->fw_seg, 1, &mpt->fw_rseg, 0);
	if (error) {
		mpt_prt(mpt, "unable to allocate fw memory, error = %d", error);
		goto fw_fail1;
	}

	error = bus_dmamem_map(mpt->sc_dmat, &mpt->fw_seg, mpt->fw_rseg, img_sz,
		(caddr_t *)&mpt->fw, BUS_DMA_COHERENT);
	if (error) {
		mpt_prt(mpt, "unable to map fw area, error = %d", error);
		goto fw_fail2;
	}

	error = bus_dmamap_load(mpt->sc_dmat, mpt->fw_dmap, mpt->fw, img_sz,
		NULL, 0);
	if (error) {
		mpt_prt(mpt, "unable to load request DMA map, error = %d", error);
		goto fw_fail3;
	}

	return(error);

fw_fail3:
	bus_dmamem_unmap(mpt->sc_dmat, (caddr_t)mpt->fw, img_sz);
fw_fail2:
	bus_dmamem_free(mpt->sc_dmat, &mpt->fw_seg, mpt->fw_rseg);
fw_fail1:
	bus_dmamap_destroy(mpt->sc_dmat, mpt->fw_dmap);
fw_fail0:
	mpt->fw = NULL;
	return (error);
}

void
mpt_free_fw_mem(mpt_softc_t *mpt)
{
	bus_dmamap_unload(mpt->sc_dmat, mpt->fw_dmap);
	bus_dmamem_unmap(mpt->sc_dmat, (caddr_t)mpt->fw, mpt->fw_image_size);
	bus_dmamem_free(mpt->sc_dmat, &mpt->fw_seg, mpt->fw_rseg);
	bus_dmamap_destroy(mpt->sc_dmat, mpt->fw_dmap);
}
