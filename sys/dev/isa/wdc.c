/*	$OpenBSD: wdc.c,v 1.28 1998/04/28 05:41:08 angelos Exp $	*/
/*	$NetBSD: wd.c,v 1.150 1996/05/12 23:54:03 mycroft Exp $ */

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
 *
 * Atapi support added by Manuel Bouyer.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "isadma.h"

/* #undef ATAPI_DEBUG_WDC */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#if NISADMA > 0
#include <dev/isa/isadmavar.h>
#endif /* NISADMA > 0 */
#include <dev/isa/wdreg.h>
#include <dev/isa/wdlink.h>

#include "atapibus.h"
#if NATAPIBUS > 0
#include <dev/atapi/atapilink.h>
#endif	/* NATAPIBUS */

#define	WAITTIME	(10 * hz)	/* time to wait for a completion */
	/* this is a lot for hard drives, but not for cdroms */
#define RECOVERYTIME hz/2
#define WDCDELAY	100
#define WDCNDELAY	100000	/* delay = 100us; so 10s for a controller state change */
#if 0
/* If you enable this, it will report any delays more than 100us * N long. */
#define WDCNDELAY_DEBUG	50
#endif

#define	WDIORETRIES	3		/* number of retries before giving up */

#define	WDPART(dev)			DISKPART(dev)

LIST_HEAD(xfer_free_list, wdc_xfer) xfer_free_list;

int	wdcprobe	__P((struct device *, void *, void *));
int	wdcprint	__P((void *, const char *));
void	wdcattach	__P((struct device *, struct device *, void *));
int	wdcintr		__P((void *));

struct cfattach wdc_ca = {
	sizeof(struct wdc_softc), wdcprobe, wdcattach
};

struct cfdriver wdc_cd = {
	NULL, "wdc", DV_DULL
};

enum wdcreset_mode { WDCRESET_VERBOSE, WDCRESET_SILENT };

#if NWD > 0
int	wdc_ata_intr	__P((struct wdc_softc *, struct wdc_xfer *));
void	wdc_ata_start	__P((struct wdc_softc *, struct wdc_xfer *));
void	wdc_ata_done	__P((struct wdc_softc *, struct wdc_xfer *));
__inline static void u_int16_to_string __P((u_int16_t *, char *, size_t));
#endif	/* NWD */
int	wait_for_phase	__P((struct wdc_softc *, int));
int	wait_for_unphase __P((struct wdc_softc *, int));
void	wdcstart	__P((struct wdc_softc *));
int	wdcreset	__P((struct wdc_softc *, enum wdcreset_mode));
void	wdcrestart	__P((void *arg));
void	wdcunwedge	__P((struct wdc_softc *));
void	wdctimeout	__P((void *arg));
int	wdccontrol	__P((struct wd_link *));
void	wdc_free_xfer	__P((struct wdc_xfer *));
void	wdcerror	__P((struct wdc_softc*, char *));
void	wdcbit_bucket	__P(( struct wdc_softc *, int));
#if NATAPIBUS > 0
void	wdc_atapi_start	__P((struct wdc_softc *,struct wdc_xfer *));
int	wdc_atapi_intr	__P((struct wdc_softc *, struct wdc_xfer *));
void	wdc_atapi_done	__P((struct wdc_softc *, struct wdc_xfer *));
#endif	/* NATAPIBUS */

#ifdef ATAPI_DEBUG
static int wdc_nxfer;
#endif

#ifdef WDDEBUG
#define WDDEBUG_PRINT(args)	printf args
#else
#define WDDEBUG_PRINT(args)
#endif

int
wdcprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	struct wdc_softc *wdc = match;
	struct isa_attach_args *ia = aux;

#if NISADMA == 0
	if (ia->ia_drq != DRQUNK) {
		printf("cannot support dma wdc devices\n");
		return 0;
	}
#endif

	wdc->sc_iot = iot = ia->ia_iot;
	if (bus_space_map(iot, ia->ia_iobase, 8, 0, &ioh))
		return 0;
	wdc->sc_ioh = ioh;

	/* Check if we have registers that work. */
	/* Error register not writable, */
	bus_space_write_1(iot, ioh, wd_error, 0x5a);
	/* but all of cyl_lo are. */
	bus_space_write_1(iot, ioh, wd_cyl_lo, 0xa5);
	if (bus_space_read_1(iot, ioh, wd_error) == 0x5a ||
	    bus_space_read_1(iot, ioh, wd_cyl_lo) != 0xa5) {
		/*
		 * Test for a controller with no IDE master, just one
		 * ATAPI device. Select drive 1, and try again.
		 */
		bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM | 0x10);
		bus_space_write_1(iot, ioh, wd_error, 0x5a);
		bus_space_write_1(iot, ioh, wd_cyl_lo, 0xa5);
		if (bus_space_read_1(iot, ioh, wd_error) == 0x5a ||
		    bus_space_read_1(iot, ioh, wd_cyl_lo) != 0xa5)
			goto nomatch;
		wdc->sc_flags |= WDCF_ONESLAVE;
	}

	if (wdcreset(wdc, WDCRESET_SILENT) != 0) {
		/*
		 * if the reset failed,, there is no master. test for ATAPI
		 * signature on the slave device. If no ATAPI slave, wait 5s
		 * and retry a reset.
		 */
		bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM | 0x10);	/* slave */
		if (bus_space_read_1(iot, ioh, wd_cyl_lo) != 0x14 ||
		    bus_space_read_1(iot, ioh, wd_cyl_hi) != 0xeb) {
			delay(500000);
			if (wdcreset(wdc, WDCRESET_SILENT) != 0)
				goto nomatch;
		}
		wdc->sc_flags |= WDCF_ONESLAVE;
	}

	/* Select drive 0 or ATAPI slave device */
	if (wdc->sc_flags & WDCF_ONESLAVE)
		bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM | 0x10);
	else
		bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM);

	/* Wait for controller to become ready. */
	if (wait_for_unbusy(wdc) < 0)
		goto nomatch;

	/* Start drive diagnostics. */
	bus_space_write_1(iot, ioh, wd_command, WDCC_DIAGNOSE);

	/* Wait for command to complete. */
	if (wait_for_unbusy(wdc) < 0)
		goto nomatch;

	ia->ia_iosize = 8;
	ia->ia_msize = 0;
#ifdef notyet
	/* when we are ready for it... */
	bus_space_unmap(iot, ioh, 8);
#endif
	return 1;

nomatch:
	bus_space_unmap(iot, ioh, 8);
	return 0;
}

int
wdcprint(aux, wdc)
	void *aux;
	const char *wdc;
{
	struct wd_link *d_link = aux;

	if (!wdc)
		printf(" drive %d", d_link->sc_drive);
	return QUIET;
}

void
wdcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wdc_softc *wdc = (void *)self;
	struct isa_attach_args *ia = aux;
#if NWD > 0
	int drive;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
#endif	/* NWD */

	TAILQ_INIT(&wdc->sc_xfer);
	wdc->sc_drq = ia->ia_drq;

	printf("\n");

	wdc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_BIO, wdcintr, wdc, wdc->sc_dev.dv_xname);

	wdc->ctlr_link.flags = 0;
#ifdef ATAPI_DEBUG
	wdc_nxfer = 0;
#endif

#if NATAPIBUS > 0
	/*
	 * Attach an ATAPI bus, if configured.
	 */
	wdc->ab_link = malloc(sizeof(struct bus_link), M_DEVBUF, M_NOWAIT);
	if (wdc->ab_link == NULL) {
		printf("%s: can't allocate ATAPI link\n", self->dv_xname);
		return;
	}
	bzero(wdc->ab_link,sizeof(struct bus_link));
	wdc->ab_link->type = BUS;
	wdc->ab_link->wdc_softc = (caddr_t)wdc;
	wdc->ab_link->ctlr_link = &(wdc->ctlr_link);
	wdc->ab_link->ctrl = self->dv_unit;
	(void)config_found(self, (void *)wdc->ab_link, NULL);
#endif	/* NATAPIBUS */
#if NWD > 0
	/*
	 * Attach standard IDE/ESDI/etc. disks to the controller.
	 */
	for (drive = 0; drive < 2; drive++) {
		/* test for ATAPI signature on this drive */
		bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM | (drive << 4));
		if (bus_space_read_1(iot, ioh, wd_cyl_lo) == 0x14 &&
		    bus_space_read_1(iot, ioh, wd_cyl_hi) == 0xeb) {
			continue;
		}
		/* controller active while autoconf */
		wdc->sc_flags |= WDCF_ACTIVE;

		if (wdccommandshort(wdc, drive, WDCC_RECAL) != 0 ||
            	    wait_for_ready(wdc) != 0) {
			wdc->d_link[drive] = NULL;
			wdc->sc_flags &= ~WDCF_ACTIVE;
		} else {
			wdc->sc_flags &= ~WDCF_ACTIVE;
			wdc->d_link[drive] = malloc(sizeof(struct wd_link),
			    M_DEVBUF, M_NOWAIT);
			if (wdc->d_link[drive] == NULL) {
				printf("%s: can't allocate link for drive %d\n",
				    self->dv_xname, drive);
				continue;
			}
			bzero(wdc->d_link[drive],sizeof(struct wd_link));
			wdc->d_link[drive]->type = DRIVE;
			wdc->d_link[drive]->wdc_softc =(caddr_t) wdc;
			wdc->d_link[drive]->ctlr_link = &(wdc->ctlr_link);
			wdc->d_link[drive]->sc_drive = drive;
#if NISADMA > 0
			if (wdc->sc_drq != DRQUNK)
				wdc->d_link[drive]->sc_mode = WDM_DMA;
			else
#endif	/* NISADMA */
				wdc->d_link[drive]->sc_mode = 0;

			(void)config_found(self, (void *)wdc->d_link[drive],
			    wdcprint);
		}
	}
#endif	/* NWD */
}

/*
 * Start I/O on a controller.  This does the calculation, and starts a read or
 * write operation.  Called to from wdstart() to start a transfer, from
 * wdcintr() to continue a multi-sector transfer or start the next transfer, or
 * wdcrestart() after recovering from an error.
 */
void
wdcstart(wdc)
	struct wdc_softc *wdc;
{
	struct wdc_xfer *xfer;

	if ((wdc->sc_flags & WDCF_ACTIVE) != 0 ) {
		WDDEBUG_PRINT(("wdcstart: already active\n"));
		return; /* controller aleady active */
	}
#ifdef DIAGNOSTIC
	if ((wdc->sc_flags & WDCF_IRQ_WAIT) != 0)
		panic("wdcstart: controller waiting for irq\n");
#endif
	/*
	 * XXX
	 * This is a kluge.  See comments in wd_get_parms().
	 */
	if ((wdc->sc_flags & WDCF_WANTED) != 0) {
#ifdef ATAPI_DEBUG_WDC
		printf("WDCF_WANTED\n");
#endif
		wdc->sc_flags &= ~WDCF_WANTED;
		wakeup(wdc);
		return;
	}
	/* is there a xfer ? */
	xfer = wdc->sc_xfer.tqh_first;
	if (xfer == NULL) {
#ifdef ATAPI_DEBUG2
		printf("wdcstart: null xfer\n");
#endif
		return;
	}
	wdc->sc_flags |= WDCF_ACTIVE;
#if NATAPIBUS > 0 && NWD > 0
	if (xfer->c_flags & C_ATAPI) {
#ifdef ATAPI_DEBUG_WDC
		printf("wdcstart: atapi\n");
#endif
		wdc_atapi_start(wdc, xfer);
	} else
		wdc_ata_start(wdc, xfer);
#else
#if NATAPIBUS > 0
#ifdef ATAPI_DEBUG_WDC
	printf("wdcstart: atapi\n");
#endif
	wdc_atapi_start(wdc, xfer);
#endif	/* NATAPIBUS */
#if NWD > 0
	wdc_ata_start(wdc, xfer);
#endif	/* NWD */
#endif	/* NATAPIBUS && NWD */
}

#if NWD > 0
void
wdc_ata_start(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	struct wd_link *d_link = xfer->d_link;
	struct buf *bp = xfer->c_bp;
	int nblks;

	if (xfer->c_errors >= WDIORETRIES) {
		wderror(d_link, bp, "wdcstart hard error");
		xfer->c_flags |= C_ERROR;
		wdc_ata_done(wdc, xfer);
		return;
	}

	/* Do control operations specially. */
	if (d_link->sc_state < READY) {
		/*
		 * Actually, we want to be careful not to mess with the control
		 * state if the device is currently busy, but we can assume
		 * that we never get to this point if that's the case.
		 */
		if (wdccontrol(d_link) == 0) {
			/* The drive is busy.  Wait. */
			return;
		}
	}

	/*
	 * WDCF_ERROR is set by wdcunwedge() and wdcintr() when an error is
	 * encountered.  If we are in multi-sector mode, then we switch to
	 * single-sector mode and retry the operation from the start.
	 */
	if (wdc->sc_flags & WDCF_ERROR) {
		wdc->sc_flags &= ~WDCF_ERROR;
		if ((wdc->sc_flags & WDCF_SINGLE) == 0) {
			wdc->sc_flags |= WDCF_SINGLE;
			xfer->c_skip = 0;
		}
	}


	/* When starting a transfer... */
	if (xfer->c_skip == 0) {
		struct buf *bp = xfer->c_bp;

		xfer->c_bcount = bp->b_bcount;
		xfer->c_blkno = (bp->b_blkno + xfer->c_p_offset) / (d_link->sc_lp->d_secsize / DEV_BSIZE);
		WDDEBUG_PRINT(("\n%s: wdc_ata_start %s %d@%d; map ",
		    wdc->sc_dev.dv_xname,
		    (xfer->c_flags & B_READ) ? "read" : "write",
		    xfer->c_bcount, xfer->c_blkno));
	} else {
		WDDEBUG_PRINT((" %d)0x%x", xfer->c_skip,
		    bus_space_read_1(iot, ioh, wd_altsts)));
	}

	/*
	 * When starting a multi-sector transfer, or doing single-sector
	 * transfers...
	 */
	if (xfer->c_skip == 0 || (wdc->sc_flags & WDCF_SINGLE) != 0 ||
	    d_link->sc_mode == WDM_DMA) {
		daddr_t blkno = xfer->c_blkno;
		int cylin, head, sector;
		int command;

		if ((wdc->sc_flags & WDCF_SINGLE) != 0)
			nblks = 1;
#if NISADMA > 0
		else if (d_link->sc_mode != WDM_DMA)
			nblks = xfer->c_bcount / d_link->sc_lp->d_secsize;
		else
			nblks =
			    min(xfer->c_bcount / d_link->sc_lp->d_secsize, 8);
#else
		else
			nblks = xfer->c_bcount / d_link->sc_lp->d_secsize;
#endif

		/* Check for bad sectors and adjust transfer, if necessary. */
		if ((d_link->sc_lp->d_flags & D_BADSECT) != 0
#ifdef B_FORMAT
		    && (bp->b_flags & B_FORMAT) == 0
#endif
		    ) {
			int blkdiff;
			int i;

			for (i = 0;
			    (blkdiff = d_link->sc_badsect[i]) != -1; i++) {
				blkdiff -= blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of xfer. */
					blkno = d_link->sc_lp->d_secperunit -
					    d_link->sc_lp->d_nsectors - i - 1;
				}
				if (blkdiff < nblks) {
					/* Bad block inside transfer. */
					wdc->sc_flags |= WDCF_SINGLE;
					nblks = 1;
				}
				break;
			}
			/* Tranfer is okay now. */
		}

		if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA) != 0) {
			sector = (blkno >> 0) & 0xff;
			cylin = (blkno >> 8) & 0xffff;
			head = (blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		} else {
			sector = blkno % d_link->sc_lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			blkno /= d_link->sc_lp->d_nsectors;
			head = blkno % d_link->sc_lp->d_ntracks;
			blkno /= d_link->sc_lp->d_ntracks;
			cylin = blkno;
			head |= WDSD_CHS;
		}

		if (d_link->sc_mode == WDM_PIOSINGLE ||
		    (wdc->sc_flags & WDCF_SINGLE) != 0)
			xfer->c_nblks = 1;
		else if (d_link->sc_mode == WDM_PIOMULTI)
			xfer->c_nblks = min(nblks, d_link->sc_multiple);
		else
			xfer->c_nblks = nblks;
		xfer->c_nbytes = xfer->c_nblks * d_link->sc_lp->d_secsize;

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			sector = d_link->sc_lp->d_gap3;
			nblks = d_link->sc_lp->d_nsectors;
			command = WDCC_FORMAT;
		} else
#endif
		switch (d_link->sc_mode) {
#if NISADMA > 0
		case WDM_DMA:
			command = (xfer->c_flags & B_READ) ?
			    WDCC_READDMA : WDCC_WRITEDMA;
			/*
			 * Start the DMA channel and bounce the buffer if
			 * necessary.
			 */
			isadma_start(xfer->databuf + xfer->c_skip,
			    xfer->c_nbytes, wdc->sc_drq,
			    xfer->c_flags & B_READ ?
			    DMAMODE_READ : DMAMODE_WRITE);
			break;
#endif

		case WDM_PIOMULTI:
			command = (xfer->c_flags & B_READ) ?
			    WDCC_READMULTI : WDCC_WRITEMULTI;
			break;

		case WDM_PIOSINGLE:
			command = (xfer->c_flags & B_READ) ?
			    WDCC_READ : WDCC_WRITE;
			break;

		default:
#ifdef DIAGNOSTIC
			panic("bad wd mode");
#endif
			return;
		}

		/* Initiate command! */
		if (wdccommand(d_link, command, d_link->sc_drive,
		    cylin, head, sector, nblks) != 0) {
			wderror(d_link, NULL,
			    "wdc_ata_start: timeout waiting for unbusy");
			wdcunwedge(wdc);
			return;
		}

		WDDEBUG_PRINT(("sector %d cylin %d head %d addr %x sts %x\n",
		    sector, cylin, head, xfer->databuf,
		    bus_space_read_1(iot, ioh, wd_altsts)));

	} else if (xfer->c_nblks > 1) {
		/* The number of blocks in the last stretch may be smaller. */
		nblks = xfer->c_bcount / d_link->sc_lp->d_secsize;
		if (xfer->c_nblks > nblks) {
			xfer->c_nblks = nblks;
			xfer->c_nbytes = xfer->c_bcount;
		}
	}

	/* If this was a write and not using DMA, push the data. */
	if (d_link->sc_mode != WDM_DMA &&
	    (xfer->c_flags & (B_READ|B_WRITE)) == B_WRITE) {
		if (wait_for_drq(wdc) < 0) {
			wderror(d_link, NULL,
			    "wdc_ata_start: timeout waiting for drq");
			wdcunwedge(wdc);
			return;
		}

		/* Push out data. */
		if ((d_link->sc_flags & WDF_32BIT) == 0)
			bus_space_write_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes);
		else
			bus_space_write_raw_multi_4(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes);
	}

	wdc->sc_flags |= WDCF_IRQ_WAIT;
	WDDEBUG_PRINT(("wdc_ata_start: timeout "));
	timeout(wdctimeout, wdc, WAITTIME);
	WDDEBUG_PRINT(("done\n"));
}

int
wdc_ata_intr(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	struct wd_link *d_link = xfer->d_link;

	if (wait_for_unbusy(wdc) < 0) {
		wdcerror(wdc, "wdcintr: timeout waiting for unbusy");
		wdc->sc_status |= WDCS_ERR;	/* XXX */
	}

	untimeout(wdctimeout, wdc);

	/* Is it not a transfer, but a control operation? */
	if (d_link->sc_state < READY) {
		if (wdccontrol(d_link) == 0) {
			/* The drive is busy.  Wait. */
			return 1;
		}
		WDDEBUG_PRINT(
		    ("wdc_ata_start from wdc_ata_intr(open) flags %d\n",
		    wdc->sc_flags));
		wdc_ata_start(wdc,xfer);
		return 1;
	}

#if NISADMA > 0
	/* Turn off the DMA channel and unbounce the buffer. */
	if (d_link->sc_mode == WDM_DMA)
		isadma_done(wdc->sc_drq);
#endif

	/* Have we an error? */
	if (wdc->sc_status & WDCS_ERR) {
#ifdef WDDEBUG
		wderror(d_link, NULL, "wdc_ata_intr");
#endif
		if ((wdc->sc_flags & WDCF_SINGLE) == 0) {
			wdc->sc_flags |= WDCF_ERROR;
			goto restart;
		}

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT)
			goto bad;
#endif

		if (++xfer->c_errors == (WDIORETRIES + 1) / 2) {
			wdcunwedge(wdc);
			return 1;
		}
		if (xfer->c_errors < WDIORETRIES)
			goto restart;

		wderror(d_link, xfer->c_bp, "hard error");

#ifdef B_FORMAT
	bad:
#endif
		xfer->c_flags |= C_ERROR;
		goto done;
	}

	/* If this was a read and not using DMA, fetch the data. */
	if (d_link->sc_mode != WDM_DMA &&
	    (xfer->c_flags & (B_READ|B_WRITE)) == B_READ) {
		if ((wdc->sc_status & (WDCS_DRDY | WDCS_DSC | WDCS_DRQ))
		    != (WDCS_DRDY | WDCS_DSC | WDCS_DRQ)) {
			wderror(d_link, NULL, "wdcintr: read intr before drq");
			wdcunwedge(wdc);
			return 1;
		}

		/* Pull in data. */
		if ((d_link->sc_flags & WDF_32BIT) == 0)
			bus_space_read_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes);
		else
			bus_space_read_raw_multi_4(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes);
	}

	/* If we encountered any abnormalities, flag it as a soft error. */
	if (xfer->c_errors > 0 ||
	    (wdc->sc_status & WDCS_CORR) != 0) {
		wderror(d_link, xfer->c_bp, "soft error (corrected)");
		xfer->c_errors = 0;
	}

	/* Adjust pointers for the next block, if any. */
	xfer->c_blkno += xfer->c_nblks;
	xfer->c_skip += xfer->c_nbytes;
	xfer->c_bcount -= xfer->c_nbytes;

	/* See if this transfer is complete. */
	if (xfer->c_bcount > 0)
		goto restart;

done:
	/* Done with this transfer, with or without error. */
	wdc_ata_done(wdc, xfer);
	return 0;

restart:
	/* Start the next operation */
	WDDEBUG_PRINT(("wdc_ata_start from wdcintr flags %d\n",
	    wdc->sc_flags));
	wdc_ata_start(wdc, xfer);

	return 1;
}

void
wdc_ata_done(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct buf *bp = xfer->c_bp;
	struct wd_link *d_link = xfer->d_link;
	int s;

	WDDEBUG_PRINT(("wdc_ata_done\n"));

	/* remove this command from xfer queue */
	s = splbio();
	TAILQ_REMOVE(&wdc->sc_xfer, xfer, c_xferchain);
	wdc->sc_flags &= ~(WDCF_SINGLE | WDCF_ERROR | WDCF_ACTIVE);
	if (bp) {
		if (xfer->c_flags & C_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		}
		bp->b_resid = xfer->c_bcount;
		wddone(d_link, bp);
		biodone(bp);
	} else {
		wakeup(xfer->databuf);
	}
	xfer->c_skip = 0;
	wdc_free_xfer(xfer);
	d_link->openings++;
	wdstart((void *)d_link->wd_softc);
	WDDEBUG_PRINT(("wdcstart from wdc_ata_done, flags %d\n",
	    wdc->sc_flags));
	wdcstart(wdc);
	splx(s);
}

/* decode IDE strings, stored as if the words are big-endian.  */
__inline static void
u_int16_to_string(from, to, cnt)
	u_int16_t *from;
	char *to;
	size_t cnt;
{
	size_t i;

	for (i = 0; i < cnt; i += 2) {
		*to++ = (char)(*from >> 8 & 0xff);
		*to++ = (char)(*from++ & 0xff);
	}
}

/*
 * Get the drive parameters, if ESDI or ATA, or create fake ones for ST506.
 */
int
wdc_get_parms(d_link)
	struct wd_link *d_link;
{
	struct wdc_softc *wdc = (struct wdc_softc *)d_link->wdc_softc;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	u_int16_t tb[DEV_BSIZE / sizeof(u_int16_t)];
	int s, error;

	/*
	 * XXX
	 * The locking done here, and the length of time this may keep the rest
	 * of the system suspended, is a kluge.  This should be rewritten to
	 * set up a transfer and queue it through wdstart(), but it's called
	 * infrequently enough that this isn't a pressing matter.
	 */

	s = splbio();

	while ((wdc->sc_flags & WDCF_ACTIVE) != 0) {
		wdc->sc_flags |= WDCF_WANTED;
		error = tsleep(wdc, PRIBIO | PCATCH, "wdprm", 0);
		if (error != 0) {
			splx(s);
			return error;
		}
	}

	wdc->sc_flags |= WDCF_ACTIVE;

	if (wdccommandshort(wdc, d_link->sc_drive, WDCC_IDENTIFY) != 0 ||
	    wait_for_drq(wdc) != 0) {
		/*
		 * We `know' there's a drive here; just assume it's old.
		 * This geometry is only used to read the MBR and print a
		 * (false) attach message.
		 */
		strncpy(d_link->sc_lp->d_typename, "ST506",
		    sizeof d_link->sc_lp->d_typename);
		d_link->sc_lp->d_type = DTYPE_ST506;

		strncpy(d_link->sc_params.wdp_model, "ST506/MFM/RLL",
		    sizeof d_link->sc_params.wdp_model);
		d_link->sc_params.wdp_config = WD_CFG_FIXED;
		d_link->sc_params.wdp_cylinders = 1024;
		d_link->sc_params.wdp_heads = 8;
		d_link->sc_params.wdp_sectors = 17;
		d_link->sc_params.wdp_maxmulti = 0;
		d_link->sc_params.wdp_usedmovsd = 0;
		d_link->sc_params.wdp_capabilities = 0;
	} else {
		strncpy(d_link->sc_lp->d_typename, "ESDI/IDE",
		    sizeof d_link->sc_lp->d_typename);
		d_link->sc_lp->d_type = DTYPE_ESDI;

		/* Read in parameter block. */
		bus_space_read_multi_2(iot, ioh, wd_data, tb,
		    sizeof(tb) / sizeof(u_int16_t));
		d_link->sc_params.wdp_config = (u_int16_t)tb[0];
		d_link->sc_params.wdp_cylinders = (u_int16_t)tb[1];
		d_link->sc_params.wdp_heads = (u_int16_t)tb[3];
		d_link->sc_params.wdp_unfbytespertrk = (u_int16_t)tb[4];
		d_link->sc_params.wdp_unfbytespersec = (u_int16_t)tb[5];
		d_link->sc_params.wdp_sectors = (u_int16_t)tb[6];
		u_int16_to_string (tb + 7, d_link->sc_params.wdp_vendor1, 6);
		u_int16_to_string (tb + 10, d_link->sc_params.wdp_serial, 20);
		d_link->sc_params.wdp_buftype = (u_int16_t)tb[20];
		d_link->sc_params.wdp_bufsize = (u_int16_t)tb[21];
		d_link->sc_params.wdp_eccbytes = (u_int16_t)tb[22];
		u_int16_to_string (tb + 23, d_link->sc_params.wdp_revision, 8);
		u_int16_to_string (tb + 27, d_link->sc_params.wdp_model, 40);
		d_link->sc_params.wdp_maxmulti = (u_int8_t)(tb[47] & 0xff);
		d_link->sc_params.wdp_vendor2[0] = (u_int8_t)(tb[47] >> 8 &
		    0xff);
		d_link->sc_params.wdp_usedmovsd = (u_int16_t)tb[48];
		d_link->sc_params.wdp_vendor3[0] = (u_int8_t)(tb[49] & 0xff);
		d_link->sc_params.wdp_capabilities = (u_int8_t)(tb[49] >> 8 &
		    0xff);
		d_link->sc_params.wdp_vendor4[0] = (u_int8_t)(tb[50] & 0xff);
		d_link->sc_params.wdp_piotiming = (u_int8_t)(tb[50] >> 8 &
		    0xff);
		d_link->sc_params.wdp_vendor5[0] = (u_int8_t)(tb[51] & 0xff);
		d_link->sc_params.wdp_dmatiming = (u_int8_t)(tb[51] >> 8 &
		    0xff);
		d_link->sc_params.wdp_capvalid = (u_int16_t)tb[52];
		d_link->sc_params.wdp_curcyls = (u_int16_t)tb[53];
		d_link->sc_params.wdp_curheads = (u_int16_t)tb[54];
		d_link->sc_params.wdp_cursectors = (u_int16_t)tb[55];
		d_link->sc_params.wdp_curcapacity[0] = (u_int16_t)tb[56];
		d_link->sc_params.wdp_curcapacity[1] = (u_int16_t)tb[57];
		d_link->sc_params.wdp_curmulti = (u_int8_t)(tb[58] & 0xff);
		d_link->sc_params.wdp_valmulti = (u_int8_t)(tb[58] >> 8 & 0xff);
		d_link->sc_params.wdp_lbacapacity[0] = (u_int16_t)tb[59];
		d_link->sc_params.wdp_lbacapacity[1] = (u_int16_t)tb[60];
		d_link->sc_params.wdp_dma1word = (u_int16_t)tb[61];
		d_link->sc_params.wdp_dmamword = (u_int16_t)tb[62];
		d_link->sc_params.wdp_eidepiomode = (u_int16_t)tb[63];
		d_link->sc_params.wdp_eidedmamin = (u_int16_t)tb[64];
		d_link->sc_params.wdp_eidedmatime = (u_int16_t)tb[65];
		d_link->sc_params.wdp_eidepiotime = (u_int16_t)tb[66];
		d_link->sc_params.wdp_eidepioiordy = (u_int16_t)tb[67];
	}

	/* Clear any leftover interrupt. */
	(void) bus_space_read_1(iot, ioh, wd_status);

	/* Restart the queue. */
	WDDEBUG_PRINT(("wdcstart from wdc_get_parms flags %d\n",
	    wdc->sc_flags));
	wdc->sc_flags &= ~WDCF_ACTIVE;
	wdcstart(wdc);

	splx(s);
	return 0;
}

/*
 * Implement operations needed before read/write.
 * Returns 0 if operation still in progress, 1 if completed.
 */
int
wdccontrol(d_link)
	struct wd_link *d_link;
{
	struct wdc_softc *wdc = (void *)d_link->wdc_softc;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;

	WDDEBUG_PRINT(("wdccontrol\n"));

	switch (d_link->sc_state) {
	case RECAL:	/* Set SDH, step rate, do recal. */
		if (wdccommandshort(wdc, d_link->sc_drive, WDCC_RECAL) != 0) {
			wderror(d_link, NULL, "wdccontrol: recal failed (1)");
			goto bad;
		}
		d_link->sc_state = RECAL_WAIT;
		break;

	case RECAL_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(d_link, NULL, "wdccontrol: recal failed (2)");
			goto bad;
		}
		/* fall through */

	case GEOMETRY:
		if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA) != 0)
			goto multimode;
		if (wdsetctlr(d_link) != 0) {
			/* Already printed a message. */
			goto bad;
		}
		d_link->sc_state = GEOMETRY_WAIT;
		break;

	case GEOMETRY_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(d_link, NULL, "wdccontrol: geometry failed");
			goto bad;
		}
		/* fall through */

	case MULTIMODE:
	multimode:
		if (d_link->sc_mode != WDM_PIOMULTI)
			goto ready;
		bus_space_write_1(iot, ioh, wd_seccnt, d_link->sc_multiple);
		if (wdccommandshort(wdc, d_link->sc_drive,
		    WDCC_SETMULTI) != 0) {
			wderror(d_link, NULL,
			    "wdccontrol: setmulti failed (1)");
			goto bad;
		}
		d_link->sc_state = MULTIMODE_WAIT;
		break;

	case MULTIMODE_WAIT:
		if (wdc->sc_status & WDCS_ERR) {
			wderror(d_link, NULL,
			    "wdccontrol: setmulti failed (2)");
			goto bad;
		}
		/* fall through */

	case READY:
	ready:
		d_link->sc_state = READY;
		/*
		 * The rest of the initialization can be done by normal means.
		 */
		return 1;

	bad:
		wdcunwedge(wdc);
		return 0;
	}

	wdc->sc_flags |= WDCF_IRQ_WAIT;
	timeout(wdctimeout, wdc, WAITTIME);
	return 0;
}
#endif	/* NWD */

int
wait_for_phase(wdc, wphase)
	struct wdc_softc *wdc;
	int wphase;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	int i, phase;

	for (i = 20000; i; i--) {
		phase = (bus_space_read_1(iot, ioh, wd_ireason) &
		    (WDCI_CMD | WDCI_IN)) |
		    (bus_space_read_1(iot, ioh, wd_status)
		    & WDCS_DRQ);
		if (phase == wphase)
			break;
		delay(10);
	}
	return (phase);
}

int
wait_for_unphase(wdc, wphase)
	struct wdc_softc *wdc;
	int wphase;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	int i, phase;

	for (i = 20000; i; i--) {
		phase = (bus_space_read_1(iot, ioh, wd_ireason) &
		    (WDCI_CMD | WDCI_IN)) |
		    (bus_space_read_1(iot, ioh, wd_status)
		    & WDCS_DRQ);
		if (phase != wphase)
			break;
		delay(10);
	}
	return (phase);
}

#if NATAPIBUS > 0
void
wdc_atapi_start(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	struct atapi_command_packet *acp = xfer->atapi_cmd;

#ifdef ATAPI_DEBUG_WDC
	printf("wdc_atapi_start, acp flags %lx\n",acp->flags);
#endif
	if (xfer->c_errors >= WDIORETRIES) {
		acp->status |= ERROR;
		acp->error = bus_space_read_1(iot, ioh, wd_error);
		wdc_atapi_done(wdc, xfer);
		return;
	}
	if (wait_for_unbusy(wdc) != 0) {
		if ((wdc->sc_status & WDCS_ERR) == 0) {
			printf("wdc_atapi_start: not ready, st = %02x\n",
			    wdc->sc_status);
			acp->status = ERROR;
			return;
		}
	}

	if (wdccommand((struct wd_link*)xfer->d_link, ATAPI_PACKET_COMMAND,
	    acp->drive, acp->data_size, 0, 0, 0) != 0) {
		printf("wdc_atapi_start: can't send atapi paket command\n");
		acp->status = ERROR;
		wdc->sc_flags |= WDCF_IRQ_WAIT;
		return;
	}
	if ((acp->flags & (ACAP_DRQ_INTR|ACAP_DRQ_ACCEL)) != ACAP_DRQ_INTR) {
		if (!(wdc->sc_flags & WDCF_BROKENPOLL)) {
			int phase = wait_for_phase(wdc, PHASE_CMDOUT);

			if (phase != PHASE_CMDOUT) {
				printf("wdc_atapi_start: timeout waiting "
				    "PHASE_CMDOUT, got 0x%x\n", phase);

				/* NEC SUCKS. */
				wdc->sc_flags |= WDCF_BROKENPOLL;
			}
		} else
			DELAY(10);	/* Simply pray for the data. */

		bus_space_write_raw_multi_2(iot, ioh, wd_data, acp->command,
		    acp->command_size);
	}
	wdc->sc_flags |= WDCF_IRQ_WAIT;

#ifdef ATAPI_DEBUG2
	printf("wdc_atapi_start: timeout\n");
#endif
	timeout(wdctimeout, wdc, WAITTIME);
	return;
}

int
wdc_atapi_get_params(ab_link, drive, id)
	struct bus_link *ab_link;
	u_int8_t drive;
	struct atapi_identify *id;
{
	struct wdc_softc *wdc = (void*)ab_link->wdc_softc;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	int status, len, excess = 0;
	int s, error;

	if (wdc->d_link[drive] != 0) {
#ifdef ATAPI_DEBUG_PROBE
		printf("wdc_atapi_get_params: WD drive %d\n", drive);

#endif
		return 0;
	}

	/*
	 * If there is only one ATAPI slave on the bus, don't probe
	 * drive 0 (master)
	 */

	if ((wdc->sc_flags & WDCF_ONESLAVE) && (drive != 1))
		return 0;

#ifdef ATAPI_DEBUG_PROBE
	printf("wdc_atapi_get_params: probing drive %d\n", drive);
#endif

	/*
	 * XXX
	 * The locking done here, and the length of time this may keep the rest
	 * of the system suspended, is a kluge.  This should be rewritten to
	 * set up a transfer and queue it through wdstart(), but it's called
	 * infrequently enough that this isn't a pressing matter.
	 */

	s = splbio();

	while ((wdc->sc_flags & WDCF_ACTIVE) != 0) {
		wdc->sc_flags |= WDCF_WANTED;
		if ((error = tsleep(wdc, PRIBIO | PCATCH, "atprm", 0)) != 0) {
			splx(s);
			return error;
		}
	}

	wdc->sc_flags |= WDCF_ACTIVE;
	error = 1;
	(void)wdcreset(wdc, WDCRESET_VERBOSE);
	if ((status = wdccommand((struct wd_link*)ab_link,
	    ATAPI_SOFT_RESET, drive, 0, 0, 0, 0)) != 0) {
#ifdef ATAPI_DEBUG
		printf("wdc_atapi_get_params: ATAPI_SOFT_RESET"
		    "failed for drive %d: status %d error %d\n",
		    drive, status, wdc->sc_error);
#endif
		error = 0;
		goto end;
	}
	if ((status = wait_for_unbusy(wdc)) != 0) {
#ifdef ATAPI_DEBUG
	printf("wdc_atapi_get_params: wait_for_unbusy failed "
	    "for drive %d: status %d error %d\n",
	    drive, status, wdc->sc_error);
#endif
		error = 0;
		goto end;
	}

	if (wdccommand((struct wd_link*)ab_link, ATAPI_IDENTIFY_DEVICE,
	    drive, sizeof(struct atapi_identify), 0, 0, 0) != 0 ||
	    atapi_ready(wdc) != 0) {
#ifdef ATAPI_DEBUG_PROBE
		printf("ATAPI_IDENTIFY_DEVICE failed for drive %d\n", drive);
#endif
		error = 0;
		goto end;
	}
	len = bus_space_read_1(iot, ioh, wd_cyl_lo) + 256 *
	    bus_space_read_1(iot, ioh, wd_cyl_hi);
	if (len != sizeof(struct atapi_identify)) {
		if (len < 142) {	/* XXX */
			printf("%s: drive %d returned %d/%d of identify device data, device unusuable\n",
			    wdc->sc_dev.dv_xname, drive, len,
			    sizeof(struct atapi_identify));

			error = 0;
			goto end;
		}

		excess = (len - sizeof(struct atapi_identify));
		if (excess < 0)
			excess = 0;
	}
	bus_space_read_raw_multi_2(iot, ioh, wd_data, (u_int8_t *)id,
	    sizeof(struct atapi_identify));
	wdcbit_bucket(wdc, excess);

 end:	/* Restart the queue. */
	WDDEBUG_PRINT(("wdcstart from wdc_atapi_get_parms flags %d\n",
	    wdc->sc_flags));
	wdc->sc_flags &= ~WDCF_ACTIVE;
	wdcstart(wdc);
	splx(s);
	return error;
}

void
wdc_atapi_send_command_packet(ab_link, acp)
	struct bus_link *ab_link;
	struct atapi_command_packet *acp;
{
	struct wdc_softc *wdc = (void*)ab_link->wdc_softc;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	struct wdc_xfer *xfer;
	u_int8_t flags = acp->flags & 0xff;

	if (flags & A_POLLED) {   /* Must use the queue and wdc_atapi_start */
		struct wdc_xfer xfer_s;
		int i, phase;

#ifdef ATAPI_DEBUG_WDC
		printf("wdc_atapi_send_cmd: "
		    "flags %ld drive %d cmdlen %d datalen %d",
		    acp->flags, acp->drive, acp->command_size, acp->data_size);
#endif
		xfer = &xfer_s;
		bzero(xfer, sizeof(xfer_s));
		xfer->c_flags = C_INUSE|C_ATAPI|acp->flags;
		xfer->d_link = (struct wd_link *)ab_link;
		xfer->c_link = ab_link->ctlr_link;
		xfer->c_bp = acp->bp;
		xfer->atapi_cmd = acp;
		xfer->c_blkno = 0;
		xfer->databuf = acp->databuf;
		xfer->c_bcount = acp->data_size;
		if (wait_for_unbusy (wdc) != 0)  {
			if ((wdc->sc_status & WDCS_ERR) == 0) {
				printf("wdc_atapi_send_command: not ready, "
				    "st = %02x\n", wdc->sc_status);
				acp->status = ERROR;
				return;
			}
		}

		/* Turn off interrupts.  */
		bus_space_write_1(iot, ioh, wd_ctlr, WDCTL_4BIT | WDCTL_IDS);
		delay(1000);

		if (wdccommand((struct wd_link*)ab_link,
		    ATAPI_PACKET_COMMAND, acp->drive, acp->data_size,
		    0, 0, 0) != 0) {
			printf("can't send atapi paket command\n");
			acp->status = ERROR;
			return;
		}

		/* Wait for cmd i/o phase. */
		phase = wait_for_phase(wdc, PHASE_CMDOUT);
		if (phase != PHASE_CMDOUT)
			printf("wdc_atapi_send_command_packet: "
			    "got wrong phase (0x%x) wanted cmd I/O\n",
			    phase);

		bus_space_write_raw_multi_2(iot, ioh, wd_data, acp->command,
		    acp->command_size);

		/* Wait for data i/o phase. */
		phase = wait_for_unphase(wdc, PHASE_CMDOUT);
		if (phase == PHASE_CMDOUT)
			printf("wdc_atapi_send_command_packet: "
			    "got wrong phase (0x%x) wanted data I/O\n",
			    phase);

		while (wdc_atapi_intr(wdc, xfer)) {
			for (i = 2000; i > 0; --i) {
				if ((bus_space_read_1(iot, ioh, wd_status) &
				    WDCS_DRQ) == 0)
					break;
				delay(10);
			}
#ifdef ATAPI_DEBUG_WDC
			printf("wdc_atapi_send_command_packet: i = %d\n", i);
#endif
		}

		/* Turn on interrupts again. */
		bus_space_write_1(iot, ioh, wd_ctlr, WDCTL_4BIT);
		delay(1000);

		wdc->sc_flags &= ~(WDCF_IRQ_WAIT | WDCF_SINGLE | WDCF_ERROR);
		xfer->c_errors = 0;
		xfer->c_skip = 0;
		return;
	} else {	/* POLLED */
		xfer = wdc_get_xfer(ab_link->ctlr_link,
		    flags & A_NOSLEEP ? IDE_NOSLEEP : 0);
		if (xfer == NULL) {
			acp->status = ERROR;
			return;
		}
		xfer->c_flags |= C_ATAPI|acp->flags;
		xfer->d_link = (struct wd_link*) ab_link;
		xfer->c_link = ab_link->ctlr_link;
		xfer->c_bp = acp->bp;
		xfer->atapi_cmd = acp;
		xfer->c_blkno = 0;
		xfer->databuf = acp->databuf;
		xfer->c_bcount = acp->data_size;
		wdc_exec_xfer((struct wd_link*)ab_link,xfer);
		return;
	}
}

int
wdc_atapi_intr(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	struct atapi_command_packet *acp = xfer->atapi_cmd;
	int len, phase, i, retries = 0;
	int err, st, ire;

	if (wait_for_unbusy(wdc) < 0) {
		printf("wdc_atapi_intr: controller busy\n");
		acp->status = ERROR;
		acp->error = bus_space_read_1(iot, ioh, wd_error);
		return 0;
	}

#ifdef ATAPI_DEBUG2
	printf("wdc_atapi_intr: %s\n", wdc->sc_dev.dv_xname);
#endif

again:
	len = bus_space_read_1(iot, ioh, wd_cyl_lo) +
	    256 * bus_space_read_1(iot, ioh, wd_cyl_hi);

	st = bus_space_read_1(iot, ioh, wd_status);
	err = bus_space_read_1(iot, ioh, wd_error);
	ire = bus_space_read_1(iot, ioh, wd_ireason);

	phase = (ire & (WDCI_CMD | WDCI_IN)) | (st & WDCS_DRQ);
#ifdef ATAPI_DEBUG_WDC
	printf("wdc_atapi_intr: len %d st %d err %d ire %d :",
	    len, st, err, ire);
#endif
	switch (phase) {
	case PHASE_CMDOUT:
		/* send packet command */
#ifdef ATAPI_DEBUG_WDC
		printf("PHASE_CMDOUT\n");
#endif

#ifdef ATAPI_DEBUG_WDC
		{
			int i;
			char *c = (char *)acp->command;

			printf("wdc_atapi_intr: cmd ");
			for (i = 0; i < acp->command_size; i++)
				printf("0x%x ", c[i]);
			printf("\n");
		}
#endif

		wdc->sc_flags |= WDCF_IRQ_WAIT;
		bus_space_write_raw_multi_2(iot, ioh, wd_data, acp->command,
		    acp->command_size);
		return 1;

	case PHASE_DATAOUT:
		/* write data */
#ifdef ATAPI_DEBUG_WDC
		printf("PHASE_DATAOUT\n");
#endif
		if ((acp->flags & (B_READ|B_WRITE)) != B_WRITE) {
			printf("wdc_atapi_intr: bad data phase\n");
			acp->status = ERROR;
			return 1;
		}
		wdc->sc_flags |= WDCF_IRQ_WAIT;
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: write only "
			    "%d of %d requested bytes\n", xfer->c_bcount, len);
			bus_space_write_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_bcount);
			for (i = xfer->c_bcount; i < len;
			    i += sizeof(u_int16_t))
				bus_space_write_2(iot, ioh, wd_data, 0);
			xfer->c_bcount = 0;
			return 1;
		} else {
			bus_space_write_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, len);
			xfer->c_skip += len;
			xfer->c_bcount -= len;
			return 1;
		}

	case PHASE_DATAIN:
		/* Read data */
#ifdef ATAPI_DEBUG_WDC
		printf("PHASE_DATAIN\n");
#endif
		if ((acp->flags & (B_READ|B_WRITE)) != B_READ) {
			printf("wdc_atapi_intr: bad data phase\n");
			acp->status = ERROR;
			return 1;
		}
		wdc->sc_flags |= WDCF_IRQ_WAIT;
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: reading only "
			    "%d of %d bytes\n", xfer->c_bcount, len);
			bus_space_read_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_bcount);
			wdcbit_bucket(wdc, len - xfer->c_bcount);
			xfer->c_bcount = 0;
			return 1;
		} else {
			bus_space_read_raw_multi_2(iot, ioh, wd_data,
			    xfer->databuf + xfer->c_skip, len);
			xfer->c_skip += len;
			xfer->c_bcount -=len;
			return 1;
		}

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
#ifdef ATAPI_DEBUG_WDC
		printf("PHASE_COMPLETED\n");
#endif
		if (st & WDCS_ERR) {
			acp->error = bus_space_read_1(iot, ioh, wd_error);
			acp->status = ERROR;
		}
#ifdef ATAPI_DEBUG_WDC
		if (xfer->c_bcount != 0) {
			printf("wdc_atapi_intr warning: bcount value "
			    "is %d after io\n", xfer->c_bcount);
		}
#endif
		break;

	default:
		if (++retries < 500) {
			DELAY(100);
			goto again;
		}
		printf("wdc_atapi_intr: unknown phase %d\n", phase);
		acp->status = ERROR;
	}

	wdc_atapi_done(wdc, xfer);
	return (0);
}


void
wdc_atapi_done(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct atapi_command_packet *acp = xfer->atapi_cmd;
	int s;

	acp->data_size = xfer->c_bcount;

	s = splbio();

	/* remove this command from xfer queue */
	xfer->c_skip = 0;
	if ((xfer->c_flags & A_POLLED) == 0) {
		untimeout(wdctimeout, wdc);
		TAILQ_REMOVE(&wdc->sc_xfer, xfer, c_xferchain);
		wdc->sc_flags &= ~(WDCF_SINGLE | WDCF_ERROR | WDCF_ACTIVE);
		wdc_free_xfer(xfer);
#ifdef ATAPI_DEBUG
		printf("wdc_atapi_done: atapi_done\n");
#endif
		atapi_done(acp);
#ifdef WDDEBUG
		printf("wdcstart from wdc_atapi_intr, flags %d\n",
		    wdc->sc_flags);
#endif
		wdcstart(wdc);
	} else
		wdc->sc_flags &= ~(WDCF_SINGLE | WDCF_ERROR | WDCF_ACTIVE);

	splx(s);
}
#endif	/* NATAPIBUS */

/*
 * Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start the
 * next request.  Also check for a partially done transfer, and continue with
 * the next chunk if so.
 */
int
wdcintr(arg)
	void *arg;
{
	struct wdc_softc *wdc = arg;
	struct wdc_xfer *xfer;

	if ((wdc->sc_flags & WDCF_IRQ_WAIT) == 0) {
		bus_space_tag_t iot = wdc->sc_iot;
		bus_space_handle_t ioh = wdc->sc_ioh;
		u_char s;
#ifdef ATAPI_DEBUG_WDC
		u_char e, i;
#endif
		DELAY(100);

		/* Clear the pending interrupt and abort. */
		s = bus_space_read_1(iot, ioh, wd_status);
		if (s != (WDCS_DRDY|WDCS_DSC)) {
#ifdef ATAPI_DEBUG_WDC
			e = bus_space_read_1(iot, ioh, wd_error);
			i = bus_space_read_1(iot, ioh, wd_seccnt);

			printf("wdcintr: inactive controller, "
			    "punting st=%02x er=%02x irr=%02x\n", s, e, i);
#else
			bus_space_read_1(iot, ioh, wd_error);
			bus_space_read_1(iot, ioh, wd_seccnt);
#endif

			if (s & WDCS_DRQ) {
				int len = 256 * bus_space_read_1(iot, ioh,
				    wd_cyl_hi) +
				    bus_space_read_1(iot, ioh, wd_cyl_lo);
#ifdef ATAPI_DEBUG_WDC
				printf("wdcintr: clearing up %d bytes\n", len);
#endif
				wdcbit_bucket(wdc, len);
			}
		}
		return 0;
	}

	WDDEBUG_PRINT(("wdcintr\n"));

	wdc->sc_flags &= ~WDCF_IRQ_WAIT;
	xfer = wdc->sc_xfer.tqh_first;
#if NATAPIBUS > 0 && NWD > 0
	if (xfer->c_flags & C_ATAPI) {
		(void)wdc_atapi_intr(wdc, xfer);
		return 0;
	} else
		return wdc_ata_intr(wdc, xfer);
#else
#if NATAPIBUS > 0
	(void)wdc_atapi_intr(wdc, xfer);
	return 0;
#endif	/* NATAPIBUS */
#if NWD > 0
	return wdc_ata_intr(wdc, xfer);
#endif	/* NWD */
#endif	/* NATAPIBUS && NWD */
}

int
wdcreset(wdc, mode)
	struct wdc_softc *wdc;
	enum wdcreset_mode mode;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;

	/* Reset the device. */
	bus_space_write_1(iot, ioh, wd_ctlr, WDCTL_RST|WDCTL_IDS);
	delay(1000);
	bus_space_write_1(iot, ioh, wd_ctlr, WDCTL_IDS);
	delay(1000);
	(void) bus_space_read_1(iot, ioh, wd_error);
	bus_space_write_1(iot, ioh, wd_ctlr, WDCTL_4BIT);

	if (wait_for_unbusy(wdc) < 0) {
		if (mode != WDCRESET_SILENT)
			printf("%s: reset failed\n", wdc->sc_dev.dv_xname);
		return 1;
	}

	return 0;
}

void
wdcrestart(arg)
	void *arg;
{
	struct wdc_softc *wdc = arg;
	int s;

	s = splbio();
	wdcstart(wdc);
	splx(s);
}

/*
 * Unwedge the controller after an unexpected error.  We do this by resetting
 * it, marking all drives for recalibration, and stalling the queue for a short
 * period to give the reset time to finish.
 * NOTE: We use a timeout here, so this routine must not be called during
 * autoconfig or dump.
 */
void
wdcunwedge(wdc)
	struct wdc_softc *wdc;
{
	int unit;

#ifdef ATAPI_DEBUG
	printf("wdcunwedge\n");
#endif

	untimeout(wdctimeout, wdc);
	wdc->sc_flags &= ~WDCF_IRQ_WAIT;
	(void) wdcreset(wdc, WDCRESET_VERBOSE);

	/* Schedule recalibrate for all drives on this controller. */
	for (unit = 0; unit < 2; unit++) {
		if (!wdc->d_link[unit]) continue;
		if (wdc->d_link[unit]->sc_state > RECAL)
			wdc->d_link[unit]->sc_state = RECAL;
	}

	wdc->sc_flags |= WDCF_ERROR;

	/* Wake up in a little bit and restart the operation. */
	WDDEBUG_PRINT(("wdcrestart from wdcunwedge\n"));
	wdc->sc_flags &= ~WDCF_ACTIVE;
	timeout(wdcrestart, wdc, RECOVERYTIME);
}

int
wdcwait(wdc, mask)
	struct wdc_softc *wdc;
	int mask;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	int timeout = 0;
	u_char status;
#ifdef WDCNDELAY_DEBUG
	extern int cold;
#endif

	WDDEBUG_PRINT(("wdcwait\n"));

	for (;;) {
		wdc->sc_status = status = bus_space_read_1(iot, ioh,
		    wd_status);
		/*
		 * XXX
		 * If a single slave ATAPI device is attached, it may
		 * have released the bus. Select it and try again.
		 */
		if (status == 0xff && wdc->sc_flags & WDCF_ONESLAVE) {
			bus_space_write_1(iot, ioh, wd_sdh,
			    WDSD_IBM | 0x10);
			wdc->sc_status = status = bus_space_read_1(iot, ioh,
			    wd_status);
		}
		if ((status & WDCS_BSY) == 0 && (status & mask) == mask)
			break;
		if (++timeout > WDCNDELAY) {
#ifdef ATAPI_DEBUG2
			printf("wdcwait: timeout, status 0x%x\n", status);
#endif
			return -1;
		}
		delay(WDCDELAY);
	}
	if (status & WDCS_ERR) {
		wdc->sc_error = bus_space_read_1(iot, ioh, wd_error);
		return WDCS_ERR;
	}
#ifdef WDCNDELAY_DEBUG
	/* After autoconfig, there should be no long delays. */
	if (!cold && timeout > WDCNDELAY_DEBUG) {
		struct wdc_xfer *xfer = wdc->sc_xfer.tqh_first;
		if (xfer == NULL)
			printf("%s: warning: busy-wait took %dus\n",
	    		wdc->sc_dev.dv_xname, WDCDELAY * timeout);
		else
			printf("%s(%s): warning: busy-wait took %dus\n",
				wdc->sc_dev.dv_xname,
			    ((struct device*)xfer->d_link->wd_softc)->dv_xname,
				WDCDELAY * timeout);
	}
#endif
	return 0;
}

void
wdctimeout(arg)
	void *arg;
{
	struct wdc_softc *wdc = (struct wdc_softc *)arg;
	int s;

	WDDEBUG_PRINT(("wdctimeout\n"));

	s = splbio();
	if ((wdc->sc_flags & WDCF_IRQ_WAIT) != 0) {
		wdc->sc_flags &= ~WDCF_IRQ_WAIT;
		wdcerror(wdc, "lost interrupt");
		wdcunwedge(wdc);
	} else
		wdcerror(wdc, "missing untimeout");
	splx(s);
}

/*
 * Wait for the drive to become ready and send a command.
 * Return -1 if busy for too long or 0 otherwise.
 * Assumes interrupts are blocked.
 */
int
wdccommand(d_link, command, drive, cylin, head, sector, count)
        struct wd_link *d_link;
        int command;
        int drive, cylin, head, sector, count;
{
	struct wdc_softc *wdc = (void*)d_link->wdc_softc;
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
        int stat;

	WDDEBUG_PRINT(("wdccommand drive %d\n", drive));

#if defined(DIAGNOSTIC) && defined(WDCDEBUG)
	if ((wdc->sc_flags & WDCF_ACTIVE) == 0)
		printf("wdccommand: controler not active (drive %d)\n", drive);
#endif

        /* Select drive, head, and addressing mode. */
        bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM | (drive << 4) | head);

        /* Wait for it to become ready to accept a command. */
        if (command == WDCC_IDP || d_link->type == BUS)
                stat = wait_for_unbusy(wdc);
        else
                stat = wdcwait(wdc, WDCS_DRDY);

        if (stat < 0) {
#ifdef ATAPI_DEBUG
		printf("wdcommand: xfer failed (wait_for_unbusy) status %d\n",
		    stat);
#endif
                return -1;
	}

        /* Load parameters. */
        if (d_link->type == DRIVE && d_link->sc_lp->d_type == DTYPE_ST506)
                bus_space_write_1(iot, ioh, wd_precomp,
		    d_link->sc_lp->d_precompcyl / 4);
        else
                bus_space_write_1(iot, ioh, wd_features, 0);
        bus_space_write_1(iot, ioh, wd_cyl_lo, cylin);
        bus_space_write_1(iot, ioh, wd_cyl_hi, cylin >> 8);
        bus_space_write_1(iot, ioh, wd_sector, sector);
        bus_space_write_1(iot, ioh, wd_seccnt, count);

        /* Send command. */
        bus_space_write_1(iot, ioh, wd_command, command);

        return 0;
}

/*
 * Simplified version of wdccommand().
 */
int
wdccommandshort(wdc, drive, command)
	struct wdc_softc *wdc;
        int drive;
        int command;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;

	WDDEBUG_PRINT(("wdccommandshort\n"));

#if defined(DIAGNOSTIC) && defined(WDCDEBUG)
	if ((wdc->sc_flags & WDCF_ACTIVE) == 0)
		printf("wdccommandshort: controller not active (drive %d)\n",
		    drive);
#endif

        /* Select drive. */
        bus_space_write_1(iot, ioh, wd_sdh, WDSD_IBM|(drive << 4));

        if (wdcwait(wdc, WDCS_DRDY) < 0)
                return -1;

        bus_space_write_1(iot, ioh, wd_command, command);

	return 0;
}

void
wdc_exec_xfer(d_link, xfer)
	struct wd_link *d_link;
	struct wdc_xfer *xfer;
{
	struct wdc_softc *wdc = (struct wdc_softc *)d_link->wdc_softc;
	int s;

	WDDEBUG_PRINT(("wdc_exec_xfer\n"));

	s = splbio();

	/* insert at the end of command list */
	TAILQ_INSERT_TAIL(&wdc->sc_xfer,xfer , c_xferchain)
	WDDEBUG_PRINT(("wdcstart from wdc_exec_xfer, flags %d\n",
	    wdc->sc_flags));
	wdcstart(wdc);
	splx(s);
}

struct wdc_xfer *
wdc_get_xfer(c_link,flags)
	struct wdc_link *c_link;
	int flags;
{
	struct wdc_xfer *xfer;
	int s;

	s = splbio();
	if ((xfer = xfer_free_list.lh_first) != NULL) {
		LIST_REMOVE(xfer, free_list);
		splx(s);
#ifdef DIAGNOSTIC
		if ((xfer->c_flags & C_INUSE) != 0)
			panic("wdc_get_xfer: xfer already in use\n");
#endif
	} else {
		splx(s);
#ifdef ATAPI_DEBUG
		printf("wdc:making xfer %d\n",wdc_nxfer);
#endif
		xfer = malloc(sizeof(*xfer), M_DEVBUF,
		    ((flags & IDE_NOSLEEP) != 0 ? M_NOWAIT : M_WAITOK));
		if (xfer == NULL)
			return 0;

#ifdef DIAGNOSTIC
		xfer->c_flags &= ~C_INUSE;
#endif
#ifdef ATAPI_DEBUG
		wdc_nxfer++;
#endif
	}
#ifdef DIAGNOSTIC
	if ((xfer->c_flags & C_INUSE) != 0)
		panic("wdc_get_xfer: xfer already in use\n");
#endif
	bzero(xfer, sizeof(struct wdc_xfer));
	xfer->c_flags = C_INUSE;
	xfer->c_link = c_link;
	return xfer;
}

void
wdc_free_xfer(xfer)
	struct wdc_xfer *xfer;
{
	int s;

	s = splbio();
	xfer->c_flags &= ~C_INUSE;
	LIST_INSERT_HEAD(&xfer_free_list, xfer, free_list);
	splx(s);
}

void
wdcerror(wdc, msg)
	struct wdc_softc *wdc;
	char *msg;
{
	struct wdc_xfer *xfer = wdc->sc_xfer.tqh_first;
	if (xfer == NULL)
		printf("%s: %s\n", wdc->sc_dev.dv_xname, msg);
	else
		printf("%s(%s): %s\n", wdc->sc_dev.dv_xname,
		    ((struct device*)xfer->d_link->wd_softc)->dv_xname, msg);
}

/*
 * the bit bucket
 */
void
wdcbit_bucket(wdc, size)
	struct wdc_softc *wdc;
	int size;
{
	bus_space_tag_t iot = wdc->sc_iot;
	bus_space_handle_t ioh = wdc->sc_ioh;
	int i;

	for (i = 0 ; i < size / 2 ; i++) {
		u_int16_t null;

		bus_space_read_multi_2(iot, ioh, wd_data, &null, 1);
	}

	if (size % 2)
		bus_space_read_1(iot, ioh, wd_data);
}
