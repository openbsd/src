/*	$OpenBSD: dpt.c,v 1.35 2014/12/19 22:44:58 guenther Exp $	*/
/*	$NetBSD: dpt.c,v 1.12 1999/10/23 16:26:33 ad Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Andy Doran, Charles M. Hannum and by Jason R. Thorpe of the Numerical 
 * Aerospace Simulation Facility, NASA Ames Research Center.
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

/*
 * Portions of this code fall under the following copyright:
 *
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 */

/*
 * Driver for DPT EATA SCSI adapters.
 *
 * TODO:
 *
 * o Need a front-end for (newer) ISA boards.
 * o Handle older firmware better.
 * o Find a bunch of different firmware EEPROMs and try them out.
 * o Test with a bunch of different boards.
 * o dpt_readcfg() should not be using CP_PIO_GETCFG.
 * o An interface to userland applications.
 * o Some sysctls or a utility (eg dptctl(8)) to control parameters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/endian.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/dptreg.h>
#include <dev/ic/dptvar.h>

struct cfdriver dpt_cd = {
	NULL, "dpt", DV_DULL
};

void	*dpt_ccb_alloc(void *);
void	dpt_ccb_free(void *, void *);

struct scsi_adapter dpt_switch = {
	dpt_scsi_cmd,
	dpt_minphys
};

#ifndef offsetof
#define offsetof(type, member) ((size_t)(&((type *)0)->member))
#endif /* offsetof */

static char *dpt_cname[] = {
	"PM3334", "SmartRAID IV",
	"PM3332", "SmartRAID IV",
	"PM2144", "SmartCache IV",
	"PM2044", "SmartCache IV",
	"PM2142", "SmartCache IV",
	"PM2042", "SmartCache IV",
	"PM2041", "SmartCache IV",
	"PM3224", "SmartRAID III",
	"PM3222", "SmartRAID III", 
	"PM3021", "SmartRAID III",
	"PM2124", "SmartCache III",
	"PM2024", "SmartCache III",
	"PM2122", "SmartCache III",
	"PM2022", "SmartCache III",
	"PM2021", "SmartCache III",
	"SK2012", "SmartCache Plus", 
	"SK2011", "SmartCache Plus",
	NULL,     "unknown adapter, please report using sendbug(1)",
};

/*
 * Handle an interrupt from the HBA.
 */
int
dpt_intr(xxx_sc)
	void *xxx_sc;
{
	struct dpt_softc *sc;
	struct dpt_ccb *ccb;
	struct eata_sp *sp;
	static int moretimo;
	int more;

	sc = xxx_sc;
	sp = sc->sc_statpack;

	if (!sp) {
#ifdef DEBUG
		printf("%s: premature intr (st:%02x aux:%02x)\n",
			sc->sc_dv.dv_xname, dpt_inb(sc, HA_STATUS),
			dpt_inb(sc, HA_AUX_STATUS));
#else /* DEBUG */
		(void) dpt_inb(sc, HA_STATUS);
#endif /* DEBUG */
		return (0);
	}

	more = 0;

#ifdef DEBUG
	if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) == 0)
		printf("%s: spurious intr\n", sc->sc_dv.dv_xname);
#endif

	/* Don't get stalled by HA_ST_MORE */
	if (moretimo < DPT_MORE_TIMEOUT / 100)
		moretimo = 0;
	
	for (;;) {
		/*
		 * HBA might have interrupted while we were dealing with the
		 * last completed command, since we ACK before we deal; keep 
		 * polling. If no interrupt is signalled, but the HBA has
		 * indicated that more data will be available soon, hang 
		 * around. 
		 */ 
		if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) == 0) {
			if (more != 0 && moretimo++ < DPT_MORE_TIMEOUT / 100) {
				DELAY(10);
				continue;
			}
			break;
		}
		
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, sc->sc_spoff,
		    sizeof(struct eata_sp), BUS_DMASYNC_POSTREAD);

		if (!sp) {
			more = dpt_inb(sc, HA_STATUS) & HA_ST_MORE;

			/* Don't get stalled by HA_ST_MORE */
			if (moretimo < DPT_MORE_TIMEOUT / 100)
				moretimo = 0;
			continue;
		}

		/* Might have looped before HBA can reset HBA_AUX_INTR */
		if (sp->sp_ccbid == -1) {
			DELAY(50);
#ifdef DIAGNOSTIC
			printf("%s: slow reset of HA_AUX_STATUS?",
			    sc->sc_dv.dv_xname);
#endif
			if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) == 0)
				return (0);
#ifdef DIAGNOSTIC
			printf("%s: was a slow reset of HA_AUX_STATUS",
			    sc->sc_dv.dv_xname);
#endif
			/* Re-sync DMA map */
			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, 
			    sc->sc_spoff, sizeof(struct eata_sp),
			    BUS_DMASYNC_POSTREAD);
		}

		/* Make sure CCB ID from status packet is realistic */
		if (sp->sp_ccbid >= 0 && sp->sp_ccbid < sc->sc_nccbs) {
			/* Sync up DMA map and cache cmd status */
			ccb = sc->sc_ccbs + sp->sp_ccbid;

			bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, 
			    CCB_OFF(sc, ccb), sizeof(struct dpt_ccb), 
			    BUS_DMASYNC_POSTWRITE);

			ccb->ccb_hba_status = sp->sp_hba_status & 0x7F;
			ccb->ccb_scsi_status = sp->sp_scsi_status;

			/* 
			 * Ack the interrupt and process the CCB. If this
			 * is a private CCB it's up to dpt_poll() to notice.
			 */
			sp->sp_ccbid = -1;
			ccb->ccb_flg |= CCB_INTR;
			more = dpt_inb(sc, HA_STATUS) & HA_ST_MORE;
			if ((ccb->ccb_flg & CCB_PRIVATE) == 0)
				dpt_done_ccb(sc, ccb);
		} else {
			printf("%s: bogus status (returned CCB id %d)\n", 
			    sc->sc_dv.dv_xname, sp->sp_ccbid);

			/* Ack the interrupt */
			sp->sp_ccbid = -1;
			more = dpt_inb(sc, HA_STATUS) & HA_ST_MORE;
		}
		
		/* Don't get stalled by HA_ST_MORE */
		if (moretimo < DPT_MORE_TIMEOUT / 100)
			moretimo = 0;
	}

	return (0);
}

/*
 * Initialize and attach the HBA. This is the entry point from bus
 * specific probe-and-attach code.
 */
void
dpt_init(sc, intrstr)
	struct dpt_softc *sc;
	const char *intrstr;
{
	struct eata_inquiry_data *ei;
	int i, j, error, rseg, mapsize;
	bus_dma_segment_t seg;
	struct eata_cfg *ec;
	char model[16];

	ec = &sc->sc_ec;
	
	/* Allocate the CCB/status packet/scratch DMA map and load */
	sc->sc_nccbs = min(betoh16(*(int16_t *)ec->ec_queuedepth),
			   DPT_MAX_CCBS);
	sc->sc_spoff = sc->sc_nccbs * sizeof(struct dpt_ccb);
	sc->sc_scroff = sc->sc_spoff + sizeof(struct eata_sp);
	sc->sc_scrlen = 256; /* XXX */
	mapsize = sc->sc_nccbs * sizeof(struct dpt_ccb) + sc->sc_scrlen +
	    sizeof(struct eata_sp);
		
	if ((error = bus_dmamem_alloc(sc->sc_dmat, mapsize, NBPG, 0, 
	    &seg, 1, &rseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate CCBs, error = %d\n",
		    sc->sc_dv.dv_xname, error);
		return;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, mapsize,
	    (caddr_t *)&sc->sc_ccbs, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map CCBs, error = %d\n",
		    sc->sc_dv.dv_xname, error);
		return;
	}

	if ((error = bus_dmamap_create(sc->sc_dmat, mapsize, mapsize, 1, 0, 
	    BUS_DMA_NOWAIT, &sc->sc_dmamap_ccb)) != 0) {
		printf("%s: unable to create CCB DMA map, error = %d\n",
		    sc->sc_dv.dv_xname, error);
		return;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_dmamap_ccb,
	    sc->sc_ccbs, mapsize, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load CCB DMA map, error = %d\n",
		    sc->sc_dv.dv_xname, error);
		return;
	}

	sc->sc_statpack = (struct eata_sp *)((caddr_t)sc->sc_ccbs +
	    sc->sc_spoff);
	sc->sc_sppa = sc->sc_dmamap_ccb->dm_segs[0].ds_addr + sc->sc_spoff;
	sc->sc_scr = (caddr_t)sc->sc_ccbs + sc->sc_scroff;
	sc->sc_scrpa = sc->sc_dmamap_ccb->dm_segs[0].ds_addr + sc->sc_scroff;
	sc->sc_statpack->sp_ccbid = -1;

	/* Initialize the CCBs */
	SLIST_INIT(&sc->sc_free_ccb);
	i = dpt_create_ccbs(sc, sc->sc_ccbs, sc->sc_nccbs);

	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	scsi_iopool_init(&sc->sc_iopool, sc, dpt_ccb_alloc, dpt_ccb_free);

	if (i == 0) {
		printf("%s: unable to create CCBs\n", sc->sc_dv.dv_xname);
		return;
	} else if (i != sc->sc_nccbs) {
		printf("%s: %d/%d CCBs created!\n", sc->sc_dv.dv_xname, i, 
		    sc->sc_nccbs);
		sc->sc_nccbs = i;
	}

	/* Get the page 0 inquiry data from the HBA */
	dpt_hba_inquire(sc, &ei);

	/* 
	 * dpt0 at pci0 dev 12 function 0: DPT SmartRAID III (PM3224A/9X-R)
	 * dpt0: interrupting at irq 10
	 * dpt0: 64 queued commands, 1 channel(s), adapter on ID(s) 7
	 */
	for (i = 0; ei->ei_vendor[i] != ' ' && i < 8; i++)
		;
	ei->ei_vendor[i] = '\0';

	for (i = 0; ei->ei_model[i] != ' ' && i < 7; i++)
		model[i] = ei->ei_model[i];
	for (j = 0; ei->ei_suffix[j] != ' ' && j < 7; j++)
		model[i++] = ei->ei_suffix[j];
	model[i] = '\0';

	/* Find the canonical name for the board */
	for (i = 0; dpt_cname[i] != NULL; i += 2)
		if (memcmp(ei->ei_model, dpt_cname[i], 6) == 0)
			break;
			
	printf("%s %s (%s)\n", ei->ei_vendor, dpt_cname[i + 1], model);

	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", sc->sc_dv.dv_xname, intrstr);

	printf("%s: %d queued commands, %d channel(s)\n", 
	    sc->sc_dv.dv_xname, sc->sc_nccbs, ec->ec_maxchannel + 1);

	/* Reset the SCSI bus */
	if (dpt_cmd(sc, NULL, 0, CP_IMMEDIATE, CPI_BUS_RESET))
		panic("%s: dpt_cmd failed", sc->sc_dv.dv_xname);
        DELAY(20000);
	
	/* Fill in each link and attach in turn */
	for (i = 0; i <= ec->ec_maxchannel; i++) {
		struct scsibus_attach_args saa;
		struct dpt_channel *ch;
		struct scsi_link *link;

		sc->sc_hbaid[i] = ec->ec_hba[3 - i];

		ch = &sc->sc_channel[i];
		ch->ch_sc = sc;
		ch->ch_index = i;

		link = &sc->sc_link[i];
		link->adapter_target = sc->sc_hbaid[i];
		link->luns = ec->ec_maxlun + 1;
		link->adapter_buswidth = ec->ec_maxtarget + 1;
		link->adapter = &dpt_switch;
		link->adapter_softc = ch;
		link->openings = sc->sc_nccbs;
		link->pool = &sc->sc_iopool;

		bzero(&saa, sizeof(saa));
		saa.saa_sc_link = link;

		config_found(&sc->sc_dv, &saa, scsiprint);
	}
}

/*
 * Cleanly shut down the HBA. The HBA must flush 
 * all data from its cache and mark array groups as clean.
 */
void
dpt_shutdown(void *v)
{
	struct dpt_softc *sc = v;

	dpt_cmd(sc, NULL, 0, CP_IMMEDIATE, CPI_POWEROFF_WARN);
	DELAY(5000*1000);
}

/*
 * Send an EATA command to the HBA.
 */
int
dpt_cmd(sc, cp, addr, eatacmd, icmd)
	struct dpt_softc *sc;
	struct eata_cp *cp;
	u_int32_t addr;
	int eatacmd, icmd;
{
	int i;
	
	for (i = 20000; i; i--) {
		if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_BUSY) == 0)
			break;
		DELAY(50);
	}

	/* Not the most graceful way to handle this */
	if (i == 0) {
		printf("%s: HBA timeout on EATA command issue; aborting\n", 
		    sc->sc_dv.dv_xname);
		return (-1);
	}
	
	if (cp == NULL)
		addr = 0;

	dpt_outb(sc, HA_DMA_BASE + 0, (u_int32_t)addr);
	dpt_outb(sc, HA_DMA_BASE + 1, (u_int32_t)addr >> 8);
	dpt_outb(sc, HA_DMA_BASE + 2, (u_int32_t)addr >> 16);
	dpt_outb(sc, HA_DMA_BASE + 3, (u_int32_t)addr >> 24);

	if (eatacmd == CP_IMMEDIATE) {
		if (cp == NULL) {
			/* XXX should really pass meaningful values */
			dpt_outb(sc, HA_ICMD_CODE2, 0);
			dpt_outb(sc, HA_ICMD_CODE1, 0);
		}
		dpt_outb(sc, HA_ICMD, icmd);
	}

        dpt_outb(sc, HA_COMMAND, eatacmd);
        return (0);
}

/*
 * Wait for the HBA to reach an arbitrary state.
 */
int
dpt_wait(sc, mask, state, ms)
        struct dpt_softc *sc;
        u_int8_t mask, state;
        int ms;
{
     
        for (ms *= 10; ms; ms--) {
                if ((dpt_inb(sc, HA_STATUS) & mask) == state)
                	return (0);
                DELAY(100);
        }
        return (-1);
}

/*
 * Wait for the specified CCB to finish. This is used when we may not be
 * able to sleep and/or interrupts are disabled (eg autoconfiguration). 
 * The timeout value from the CCB is used. This should only be used for 
 * CCB_PRIVATE requests; otherwise the CCB will get recycled before we get 
 * a look at it.
 */
int
dpt_poll(sc, ccb)
        struct dpt_softc *sc;
        struct dpt_ccb *ccb;
{
	int i, s;

#ifdef DEBUG
	if ((ccb->ccb_flg & CCB_PRIVATE) == 0)
		panic("dpt_poll: called for non-CCB_PRIVATE request");
#endif

 	if ((ccb->ccb_flg & CCB_INTR) != 0)
        	return (0);                

        for (i = ccb->ccb_timeout * 20; i; i--) {
                if ((dpt_inb(sc, HA_AUX_STATUS) & HA_AUX_INTR) != 0) {
			s = splbio();
                	dpt_intr(sc);
			splx(s);
		}
                if ((ccb->ccb_flg & CCB_INTR) != 0)
                	return (0);
                DELAY(50);
        }
        return (-1);
}

/*
 * Read the EATA configuration from the HBA and perform some sanity checks.
 */
int
dpt_readcfg(sc)
	struct dpt_softc *sc;
{
	struct eata_cfg *ec;
	int i, j, stat;
	u_int16_t *p;

	ec = &sc->sc_ec;

	/* Older firmware may puke if we talk to it too soon after reset */
	dpt_outb(sc, HA_COMMAND, CP_RESET);
        DELAY(750000);

	for (i = 1000; i; i--) {
		if ((dpt_inb(sc, HA_STATUS) & HA_ST_READY) != 0)
			break;
		DELAY(2000);
	}
	
	if (i == 0) {
		printf("%s: HBA not ready after reset: %02x\n", 
		    sc->sc_dv.dv_xname, dpt_inb(sc, HA_STATUS));
		return (-1);
	}

	while((((stat = dpt_inb(sc, HA_STATUS))
            != (HA_ST_READY|HA_ST_SEEK_COMPLETE))
            && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR))
            && (stat != (HA_ST_READY|HA_ST_SEEK_COMPLETE|HA_ST_ERROR|HA_ST_DRQ)))
            || (dpt_wait(sc, HA_ST_BUSY, 0, 2000))) {
        	/* RAID drives still spinning up? */
                if((dpt_inb(sc, HA_ERROR) != 'D')
                    || (dpt_inb(sc, HA_ERROR + 1) != 'P')
                    || (dpt_inb(sc, HA_ERROR + 2) != 'T')) {
                    	printf("%s: HBA not ready\n", sc->sc_dv.dv_xname);
                        return (-1);
		}
        }

	/* 
	 * Issue the read-config command and wait for the data to appear.
	 * XXX we shouldn't be doing this with PIO, but it makes it a lot
	 * easier as no DMA setup is required.
	 */
	dpt_outb(sc, HA_COMMAND, CP_PIO_GETCFG);
	memset(ec, 0, sizeof(*ec));
	i = (offsetof(struct eata_cfg, ec_cfglen) + sizeof(ec->ec_cfglen)) >> 1;
	p = (u_int16_t *)ec;
	
	if (dpt_wait(sc, 0xFF, HA_ST_DATA_RDY, 2000)) {
		printf("%s: cfg data didn't appear (status:%02x)\n", 
		    sc->sc_dv.dv_xname, dpt_inb(sc, HA_STATUS));
  		return (-1);
  	}

	/* Begin reading */
 	while (i--)
		*p++ = dpt_inw(sc, HA_DATA);

	i = ec->ec_cfglen;
	if (i > sizeof(struct eata_cfg) - offsetof(struct eata_cfg, ec_cfglen) -
	    sizeof(ec->ec_cfglen))
		i = sizeof(struct eata_cfg) - offsetof(struct eata_cfg, ec_cfglen) -
		    sizeof(ec->ec_cfglen);

        j = i + offsetof(struct eata_cfg, ec_cfglen) + sizeof(ec->ec_cfglen);
        i >>= 1;

	while (i--)
                *p++ = dpt_inw(sc, HA_DATA);
        
        /* Flush until we have read 512 bytes. */
        i = (512 - j + 1) >> 1;
	while (i--)
 		dpt_inw(sc, HA_DATA);
        
        /* Defaults for older Firmware */
	if (p <= (u_short *)&ec->ec_hba[DPT_MAX_CHANNELS - 1])
		ec->ec_hba[DPT_MAX_CHANNELS - 1] = 7;

        if ((dpt_inb(sc, HA_STATUS) & HA_ST_ERROR) != 0) {
        	printf("%s: HBA error\n", sc->sc_dv.dv_xname);
        	return (-1);
        }
        
        if (!ec->ec_hbavalid) {
                printf("%s: ec_hba field invalid\n", sc->sc_dv.dv_xname);
		return (-1);
	}
	
	if (memcmp(ec->ec_eatasig, "EATA", 4) != 0) {
	        printf("%s: EATA signature mismatch\n", sc->sc_dv.dv_xname);
		return (-1);
	}
	
	if (!ec->ec_dmasupported) {
	        printf("%s: DMA not supported\n", sc->sc_dv.dv_xname);
		return (-1);
	}

	return (0);
}

/*
 * Adjust the size of each I/O before it passes to the SCSI layer.
 */
void
dpt_minphys(struct buf *bp, struct scsi_link *sl)
{
	if (bp->b_bcount > DPT_MAX_XFER)
		bp->b_bcount = DPT_MAX_XFER;
	minphys(bp);
}

/*
 * Put a CCB onto the freelist.
 */
void
dpt_ccb_free(void *xsc, void *xccb)
{
	struct dpt_softc *sc = xsc;
	struct dpt_ccb *ccb = xccb;

	ccb->ccb_flg = 0;

	mtx_enter(&sc->sc_ccb_mtx);
	SLIST_INSERT_HEAD(&sc->sc_free_ccb, ccb, ccb_chain);
	mtx_leave(&sc->sc_ccb_mtx);
}

/*
 * Initialize the specified CCB.
 */
int
dpt_init_ccb(sc, ccb)
	struct dpt_softc *sc;
	struct dpt_ccb *ccb;
{
	int error;
	
	/* Create the DMA map for this CCB's data */
	error = bus_dmamap_create(sc->sc_dmat, DPT_MAX_XFER, DPT_SG_SIZE, 
	    DPT_MAX_XFER, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &ccb->ccb_dmamap_xfer);
	
	if (error) {
		printf("%s: can't create ccb dmamap (%d)\n", 
		   sc->sc_dv.dv_xname, error);
		return (error);
	}

	ccb->ccb_flg = 0;
	ccb->ccb_ccbpa = sc->sc_dmamap_ccb->dm_segs[0].ds_addr +
	    CCB_OFF(sc, ccb);
	return (0);
}

/*
 * Create a set of CCBs and add them to the free list.
 */
int
dpt_create_ccbs(sc, ccbstore, count)
	struct dpt_softc *sc;
	struct dpt_ccb *ccbstore;
	int count;
{
	struct dpt_ccb *ccb;
	int i, error;

	memset(ccbstore, 0, sizeof(struct dpt_ccb) * count);
	
	for (i = 0, ccb = ccbstore; i < count; i++, ccb++) {
		if ((error = dpt_init_ccb(sc, ccb)) != 0) {
			printf("%s: unable to init ccb, error = %d\n",
			    sc->sc_dv.dv_xname, error);
			break;
		}
		ccb->ccb_id = i;
		SLIST_INSERT_HEAD(&sc->sc_free_ccb, ccb, ccb_chain);
	}

	return (i);
}

/*
 * Get a free ccb. If there are none, see if we can allocate a new one. If 
 * none are available right now and we are permitted to sleep, then wait 
 * until one becomes free, otherwise return an error.
 */
void *
dpt_ccb_alloc(void *xsc)
{
	struct dpt_softc *sc = xsc;
	struct dpt_ccb *ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = SLIST_FIRST(&sc->sc_free_ccb);
	if (ccb != NULL) {
		SLIST_REMOVE_HEAD(&sc->sc_free_ccb, ccb_chain);
		ccb->ccb_flg |= CCB_ALLOC;
	}
	mtx_leave(&sc->sc_ccb_mtx);

	return (ccb);
}

/*
 * We have a CCB which has been processed by the HBA, now we look to see how 
 * the operation went. CCBs marked with CCB_PRIVATE are not automatically
 * passed here by dpt_intr().
 */
void
dpt_done_ccb(sc, ccb)
	struct dpt_softc *sc;
	struct dpt_ccb *ccb;
{
	struct scsi_sense_data *s1, *s2;
	struct scsi_xfer *xs;
	bus_dma_tag_t dmat;
	
	dmat = sc->sc_dmat;
	xs = ccb->ccb_xs;

	SC_DEBUG(xs->sc_link, SDEV_DB2, ("dpt_done_ccb\n"));

	/*
	 * If we were a data transfer, unload the map that described the 
	 * data buffer.
	 */
	if (xs->datalen) {
		bus_dmamap_sync(dmat, ccb->ccb_dmamap_xfer, 0,
		    ccb->ccb_dmamap_xfer->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dmat, ccb->ccb_dmamap_xfer);
	}

	/*
	 * Otherwise, put the results of the operation into the xfer and 
	 * call whoever started it.
	 */
#ifdef DIAGNOSTIC
	if ((ccb->ccb_flg & CCB_ALLOC) == 0) {
		panic("%s: done ccb not allocated!", sc->sc_dv.dv_xname);
		return;
	}
#endif
	
	if (xs->error == XS_NOERROR) {
		if (ccb->ccb_hba_status != HA_NO_ERROR) {
			switch (ccb->ccb_hba_status) {
			case HA_ERROR_SEL_TO:
				xs->error = XS_SELTIMEOUT;
				break;
			case HA_ERROR_RESET:
				xs->error = XS_RESET;
				break;
			default:	/* Other scsi protocol messes */
				printf("%s: HBA status %x\n",
				    sc->sc_dv.dv_xname, ccb->ccb_hba_status);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else if (ccb->ccb_scsi_status != SCSI_OK) {
			switch (ccb->ccb_scsi_status) {
			case SCSI_CHECK:
				s1 = &ccb->ccb_sense;
				s2 = &xs->sense;
				*s2 = *s1;
				xs->error = XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			default:
				printf("%s: SCSI status %x\n",
				    sc->sc_dv.dv_xname, ccb->ccb_scsi_status);
				xs->error = XS_DRIVER_STUFFUP;
			}
		} else
			xs->resid = 0;
			
		xs->status = ccb->ccb_scsi_status;
	}

	/* Mark the command as done */
	scsi_done(xs);
}

/*
 * Start a SCSI command.
 */
void
dpt_scsi_cmd(struct scsi_xfer *xs)
{
	int error, i, flags, s;
	struct scsi_link *sc_link;
	struct dpt_channel *ch;
	struct dpt_softc *sc;
	struct dpt_ccb *ccb;
	struct eata_sg *sg;
	struct eata_cp *cp;
	bus_dma_tag_t dmat;
	bus_dmamap_t xfer;

	sc_link = xs->sc_link;
	flags = xs->flags;
	ch = sc_link->adapter_softc;
	sc = ch->ch_sc;
	dmat = sc->sc_dmat;

	SC_DEBUG(sc_link, SDEV_DB2, ("dpt_scsi_cmd\n"));

	/* Cmds must be no more than 12 bytes for us */
	if (xs->cmdlen > 12) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	/* XXX we can't reset devices just yet */
	if ((xs->flags & SCSI_RESET) != 0) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	ccb = xs->io;
	ccb->ccb_flg &= ~CCB_ALLOC;

	ccb->ccb_xs = xs;
	ccb->ccb_timeout = xs->timeout;

	cp = &ccb->ccb_eata_cp;
	bcopy(xs->cmd, &cp->cp_scsi_cmd, xs->cmdlen);
	cp->cp_ccbid = ccb->ccb_id;
	cp->cp_id = sc_link->target;
	cp->cp_lun = sc_link->lun;
	cp->cp_channel = ch->ch_index;
	cp->cp_senselen = sizeof(ccb->ccb_sense);
	cp->cp_stataddr = htobe32(sc->sc_sppa);
	cp->cp_dispri = 1;
	cp->cp_identify = 1;
	cp->cp_autosense = 1;
	cp->cp_datain = ((xs->flags & SCSI_DATA_IN) != 0);
	cp->cp_dataout = ((xs->flags & SCSI_DATA_OUT) != 0);
	cp->cp_interpret = (sc->sc_hbaid[ch->ch_index] == sc_link->target);

	/* Synchronous xfers musn't write-back through the cache */
	if (xs->bp != NULL && (xs->bp->b_flags & (B_ASYNC | B_READ)) == 0)
		cp->cp_nocache = 1;
	else
		cp->cp_nocache = 0;

	cp->cp_senseaddr = htobe32(sc->sc_dmamap_ccb->dm_segs[0].ds_addr +
	    CCB_OFF(sc, ccb) + offsetof(struct dpt_ccb, ccb_sense));
	    
	if (xs->datalen) {
		xfer = ccb->ccb_dmamap_xfer;
		error = bus_dmamap_load(dmat, xfer, xs->data, 
		    xs->datalen, NULL, (xs->flags & SCSI_NOSLEEP) ? 
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);

		if (error) {
			printf("%s: dpt_scsi_cmd: ", sc->sc_dv.dv_xname); 
			if (error == EFBIG)
				printf("more than %d dma segs\n", DPT_SG_SIZE);
			else
				printf("error %d loading dma map\n", error);
		
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			return;
		}

		bus_dmamap_sync(dmat, xfer, 0, xfer->dm_mapsize,
		    (flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

		/* Don't bother using scatter/gather for just 1 segment */
		if (xfer->dm_nsegs == 1) {
			cp->cp_dataaddr = htobe32(xfer->dm_segs[0].ds_addr);
			cp->cp_datalen = htobe32(xfer->dm_segs[0].ds_len);
			cp->cp_scatter = 0;
		} else {
			/*
			 * Load the hardware scatter/gather map with the
			 * contents of the DMA map.
			 */
			sg = ccb->ccb_sg;
			for (i = 0; i < xfer->dm_nsegs; i++, sg++) {
				sg->sg_addr =
				  htobe32(xfer->dm_segs[i].ds_addr);
				sg->sg_len =
				  htobe32(xfer->dm_segs[i].ds_len);
			}
			cp->cp_dataaddr = htobe32(CCB_OFF(sc, ccb) + 
			    sc->sc_dmamap_ccb->dm_segs[0].ds_addr +
			    offsetof(struct dpt_ccb, ccb_sg));
			cp->cp_datalen = htobe32(i * sizeof(struct eata_sg));
			cp->cp_scatter = 1;
		}
	} else {
		cp->cp_dataaddr = 0;
		cp->cp_datalen = 0;
		cp->cp_scatter = 0;
	}

	/* Sync up CCB and status packet */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, CCB_OFF(sc, ccb), 
	    sizeof(struct dpt_ccb), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, sc->sc_spoff, 
	    sizeof(struct eata_sp), BUS_DMASYNC_PREREAD);

	/* 
	 * Start the command. If we are polling on completion, mark it
	 * private so that dpt_intr/dpt_done_ccb don't recycle the CCB 
	 * without us noticing.
	 */
	if ((xs->flags & SCSI_POLL) != 0)
		ccb->ccb_flg |= CCB_PRIVATE; 
	
	if (dpt_cmd(sc, &ccb->ccb_eata_cp, ccb->ccb_ccbpa, CP_DMA_CMD, 0)) {
		printf("%s: dpt_cmd failed\n", sc->sc_dv.dv_xname);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	if ((xs->flags & SCSI_POLL) == 0)
		return;

	/* Don't wait longer than this single command wants to wait */
	if (dpt_poll(sc, ccb)) {
		dpt_timeout(ccb);
		/* Wait for abort to complete */
		if (dpt_poll(sc, ccb))
			dpt_timeout(ccb);
	} 
	
	s = splbio();
	dpt_done_ccb(sc, ccb);
	splx(s);
}

/*
 * Specified CCB has timed out, abort it.
 */
void
dpt_timeout(arg)
	void *arg;
{
	struct scsi_link *sc_link;
	struct scsi_xfer *xs;
	struct dpt_channel *ch;
	struct dpt_softc *sc;
 	struct dpt_ccb *ccb;
	int s;
	
	ccb = arg;
	xs = ccb->ccb_xs;
	sc_link = xs->sc_link;
	ch = sc_link->adapter_softc;
	sc = ch->ch_sc;

	sc_print_addr(sc_link);
	printf("timed out (status:%02x aux status:%02x)", 
	    dpt_inb(sc, HA_STATUS), dpt_inb(sc, HA_AUX_STATUS));

	s = splbio();

	if ((ccb->ccb_flg & CCB_ABORT) != 0) {
		/* Abort timed out, reset the HBA */
		printf(" AGAIN, resetting HBA\n");
		dpt_outb(sc, HA_COMMAND, CP_RESET);
		DELAY(750000);
	} else {
		/* Abort the operation that has timed out */
		printf("\n");
		ccb->ccb_xs->error = XS_TIMEOUT;
		ccb->ccb_timeout = DPT_ABORT_TIMEOUT;
		ccb->ccb_flg |= CCB_ABORT;
		/* Start the abort */
		if (dpt_cmd(sc, &ccb->ccb_eata_cp, ccb->ccb_ccbpa, 
		    CP_IMMEDIATE, CPI_SPEC_ABORT))
		    printf("%s: dpt_cmd failed\n", sc->sc_dv.dv_xname);
	}

	splx(s);
}

#ifdef DEBUG
/*
 * Dump the contents of an EATA status packet.
 */
void
dpt_dump_sp(sp)
	struct eata_sp *sp;
{
	int i;
	
	printf("\thba_status\t%02x\n", sp->sp_hba_status);
	printf("\tscsi_status\t%02x\n", sp->sp_scsi_status);	
	printf("\tinv_residue\t%d\n", sp->sp_inv_residue);	
	printf("\tccbid\t\t%d\n", sp->sp_ccbid);
	printf("\tid_message\t%d\n", sp->sp_id_message);
	printf("\tque_message\t%d\n", sp->sp_que_message);	
	printf("\ttag_message\t%d\n", sp->sp_tag_message);
	printf("\tmessages\t");
	
	for (i = 0; i < 9; i++)
		printf("%d ", sp->sp_messages[i]);
		
	printf("\n");
}
#endif	/* DEBUG */

/*
 * Get inquiry data from the adapter.
 */
void
dpt_hba_inquire(sc, ei)
	struct dpt_softc *sc;
	struct eata_inquiry_data **ei;
{
	struct dpt_ccb *ccb;
	struct eata_cp *cp;
	bus_dma_tag_t dmat;
	
	*ei = (struct eata_inquiry_data *)sc->sc_scr;
	dmat = sc->sc_dmat;

	/* Get a CCB and mark as private */
	if ((ccb = scsi_io_get(&sc->sc_iopool, SCSI_NOSLEEP)) == NULL)
		panic("%s: no CCB for inquiry", sc->sc_dv.dv_xname);
	
	ccb->ccb_flg |= CCB_PRIVATE;
	ccb->ccb_timeout = 200;

	/* Put all the arguments into the CCB */
	cp = &ccb->ccb_eata_cp;
	cp->cp_ccbid = ccb->ccb_id;
	cp->cp_id = sc->sc_hbaid[0];
	cp->cp_lun = 0;
	cp->cp_channel = 0;
	cp->cp_senselen = sizeof(ccb->ccb_sense);
	cp->cp_stataddr = htobe32(sc->sc_sppa);
	cp->cp_dispri = 1;
	cp->cp_identify = 1;
	cp->cp_autosense = 0;
	cp->cp_interpret = 1;
	cp->cp_nocache = 0;
	cp->cp_datain = 1;
	cp->cp_dataout = 0;
	cp->cp_senseaddr = 0;
	cp->cp_dataaddr = htobe32(sc->sc_scrpa);
	cp->cp_datalen = htobe32(sizeof(struct eata_inquiry_data));
	cp->cp_scatter = 0;
	
	/* Put together the SCSI inquiry command */
	memset(&cp->cp_scsi_cmd, 0, 12);	/* XXX */
	cp->cp_scsi_cmd = INQUIRY;
	cp->cp_len = sizeof(struct eata_inquiry_data);

	/* Sync up CCB, status packet and scratch area */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, CCB_OFF(sc, ccb), 
	    sizeof(struct dpt_ccb), BUS_DMASYNC_PREWRITE);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, sc->sc_spoff, 
	    sizeof(struct eata_sp), BUS_DMASYNC_PREREAD);
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, sc->sc_scroff, 
	    sizeof(struct eata_inquiry_data), BUS_DMASYNC_PREREAD);

	/* Start the command and poll on completion */
	if (dpt_cmd(sc, &ccb->ccb_eata_cp, ccb->ccb_ccbpa, CP_DMA_CMD, 0))
		panic("%s: dpt_cmd failed", sc->sc_dv.dv_xname);

	if (dpt_poll(sc, ccb))
		panic("%s: inquiry timed out", sc->sc_dv.dv_xname);

	if (ccb->ccb_hba_status != HA_NO_ERROR ||
	    ccb->ccb_scsi_status != SCSI_OK)
	    	panic("%s: inquiry failed (hba:%02x scsi:%02x", 
	    	    sc->sc_dv.dv_xname, ccb->ccb_hba_status,
	    	    ccb->ccb_scsi_status);
	
	/* Sync up the DMA map and free CCB, returning */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_dmamap_ccb, sc->sc_scroff, 
	    sizeof(struct eata_inquiry_data), BUS_DMASYNC_POSTREAD);
	scsi_io_put(&sc->sc_iopool, ccb);
}
