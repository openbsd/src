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

#undef ATAPI_DEBUG_WDC

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

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>
#include <dev/isa/wdreg.h>
#include <dev/isa/wdlink.h>

#include <dev/atapi/atapilink.h>

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
void	wdcattach	__P((struct device *, struct device *, void *));
int	wdcintr		__P((void *));

struct cfattach wdc_ca = {
	sizeof(struct wdc_softc), wdcprobe, wdcattach
};

struct cfdriver wdc_cd = {
	NULL, "wdc", DV_DULL
};

int	wdc_ata_intr	__P((struct wdc_softc *,struct wdc_xfer *));
void	wdcstart	__P((struct wdc_softc *));
void	wdc_ata_start	__P((struct wdc_softc *,struct wdc_xfer *));
void	wdc_atapi_start	__P((struct wdc_softc *,struct wdc_xfer *));
int	wdcreset	__P((struct wdc_softc *));
void	wdcrestart	__P((void *arg));
void	wdcunwedge	__P((struct wdc_softc *));
void	wdctimeout	__P((void *arg));
int	wdccontrol	__P((struct wd_link *));
void	wdc_ata_done	__P((struct wdc_softc *, struct wdc_xfer *));
void	wdc_free_xfer	__P((struct wdc_xfer *));
void	wdcerror	__P((struct wdc_softc*, char *));
void	wdcbit_bucket	__P(( struct wdc_softc *, int));
int	wdc_atapi_intr	__P((struct wdc_softc *, struct wdc_xfer *));
void	wdc_atapi_done	__P((struct wdc_softc *, struct wdc_xfer *));

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
	struct wdc_softc *wdc = match;
	struct isa_attach_args *ia = aux;
	int iobase;

	wdc->sc_iobase = iobase = ia->ia_iobase;

	/* Check if we have registers that work. */
	outb(iobase+wd_error, 0x5a);	/* Error register not writable, */
	outb(iobase+wd_cyl_lo, 0xa5);	/* but all of cyllo are. */
	if (inb(iobase+wd_error) == 0x5a || inb(iobase+wd_cyl_lo) != 0xa5) {
		/*
		 * Test for a controller with no IDE master, just one
		 * ATAPI device. Select drive 1, and try again.
		 */
		outb(iobase+wd_sdh, WDSD_IBM | 0x10);
		outb(iobase+wd_error, 0x5a);
		outb(iobase+wd_cyl_lo, 0xa5);
		if (inb(iobase+wd_error) == 0x5a || inb(iobase+wd_cyl_lo)
		    != 0xa5)
			return 0;
		wdc->sc_flags |= WDCF_ONESLAVE;
	}

	if (wdcreset(wdc) != 0) {
		delay(500000);
		if (wdcreset(wdc) != 0)
			return 0;
	}

	/* Select drive 0 or ATAPI slave device */
	if (wdc->sc_flags & WDCF_ONESLAVE)
		outb(iobase+wd_sdh, WDSD_IBM | 0x10);
	else
		outb(iobase+wd_sdh, WDSD_IBM);

	/* Wait for controller to become ready. */
	if (wait_for_unbusy(wdc) < 0)
		return 0;
    
	/* Start drive diagnostics. */
	outb(iobase+wd_command, WDCC_DIAGNOSE);

	/* Wait for command to complete. */
	if (wait_for_unbusy(wdc) < 0)
		return 0;

	ia->ia_iosize = 8;
	ia->ia_msize = 0;
	return 1;
}

int
wdprint(aux, wdc)
	void *aux;
	char *wdc;
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
	int drive;

	TAILQ_INIT(&wdc->sc_xfer);
	wdc->sc_drq = ia->ia_drq;

	printf("\n");

	wdc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	    IPL_BIO, wdcintr, wdc, wdc->sc_dev.dv_xname);

	wdc->ctlr_link.flags = 0;
#ifdef ATAPI_DEBUG
	wdc_nxfer = 0;
#endif

	/*
	 * Attach standard IDE/ESDI/etc. disks to the controller.
	 */
	for (drive = 0; drive < 2; drive++) {
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
			if (wdc->sc_drq != DRQUNK) 
				wdc->d_link[drive]->sc_mode = WDM_DMA;
			else
				wdc->d_link[drive]->sc_mode = 0;

			(void)config_found(self, (void *)wdc->d_link[drive],
			    wdprint);
		}
	}

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
	if (xfer->c_flags & C_ATAPI) {
#ifdef ATAPI_DEBUG_WDC
		printf("wdcstart: atapi\n");
#endif
		wdc_atapi_start(wdc,xfer);
	} else {
		wdc_ata_start(wdc,xfer);
	}
}


void
wdc_ata_start(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct wd_link *d_link;
	struct buf *bp = xfer->c_bp;
	int nblks;

	d_link=xfer->d_link;

loop:
    
	if (wdc->sc_errors >= WDIORETRIES) {
		wderror(d_link, bp, "hard error");
		xfer->c_flags |= C_ERROR;
		wdc_ata_done(wdc, xfer);
		return;
	}

	/* Do control operations specially. */
	if (d_link->sc_state < OPEN) {
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
		daddr_t blkno;

		WDDEBUG_PRINT(("\n%s: wdc_ata_start %s %d@%d; map ",
		    wdc->sc_dev.dv_xname,
		    (xfer->c_flags & B_READ) ? "read" : "write",
		    xfer->c_bcount, xfer->c_blkno));

		blkno = xfer->c_blkno+xfer->c_p_offset;
		xfer->c_blkno = blkno / (d_link->sc_lp->d_secsize / DEV_BSIZE);
	} else {
		WDDEBUG_PRINT((" %d)%x", xfer->c_skip,
		    inb(wdc->sc_iobase + wd_altsts)));
	}

	/*
	 * When starting a multi-sector transfer, or doing single-sector
	 * transfers...
	 */
	if (xfer->c_skip == 0 || (wdc->sc_flags & WDCF_SINGLE) != 0 ||
	    d_link->sc_mode == WDM_DMA) {
		daddr_t blkno = xfer->c_blkno;
		long cylin, head, sector;
		int command;

		if ((wdc->sc_flags & WDCF_SINGLE) != 0)
			nblks = 1;
		else if (d_link->sc_mode != WDM_DMA)
			nblks = xfer->c_bcount / d_link->sc_lp->d_secsize;
		else
			nblks =
			    min(xfer->c_bcount / d_link->sc_lp->d_secsize, 8);

		/* Check for bad sectors and adjust transfer, if necessary. */
		if ((d_link->sc_lp->d_flags & D_BADSECT) != 0
#ifdef B_FORMAT
		    && (bp->b_flags & B_FORMAT) == 0
#endif
		    ) {
			long blkdiff;
			int i;

			for (i = 0;
			    (blkdiff = d_link->sc_badsect[i]) != -1; i++) {
				blkdiff -= blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of transfer. */
					blkno =
					    d_link->sc_lp->d_secperunit -
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
		case WDM_DMA:
			command = (xfer->c_flags & B_READ) ?
			    WDCC_READDMA : WDCC_WRITEDMA;
			/*
			 * Start the DMA channel and bounce the buffer if
			 * necessary.
			 */
			isa_dmastart(xfer->c_flags & B_READ ?
			    DMAMODE_READ : DMAMODE_WRITE,
			    xfer->databuf + xfer->c_skip,
			    xfer->c_nbytes, wdc->sc_drq);
			break;

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
		    inb(wdc->sc_iobase + wd_altsts)));

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
			outsw(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip,
			    xfer->c_nbytes >> 1);
		else
			outsl(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip,
			    xfer->c_nbytes >> 2);
	}

	wdc->sc_flags |= WDCF_IRQ_WAIT;
	WDDEBUG_PRINT(("wdc_ata_start: timeout "));
	timeout(wdctimeout, wdc, WAITTIME);
	WDDEBUG_PRINT(("done\n"));
}

void
wdc_atapi_start(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct atapi_command_packet *acp = xfer->atapi_cmd;

#ifdef ATAPI_DEBUG_WDC
	printf("wdc_atapi_start, acp flags %lx\n",acp->flags);
#endif
	if (wdc->sc_errors >= WDIORETRIES) {
		acp->status |= ERROR;
		acp->error = inb (wdc->sc_iobase + wd_error);
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
	if ((acp->flags & 0x0300) != ACAP_DRQ_INTR) {
		int i, phase;
		for (i=20000; i>0; --i) {
			phase = (inb(wdc->sc_iobase + wd_ireason) &
			    (WDCI_CMD | WDCI_IN)) |
			    (inb(wdc->sc_iobase + wd_status) & WDCS_DRQ);
			if (phase == PHASE_CMDOUT)
				break;
			delay(10);
		}
		if (phase != PHASE_CMDOUT ) {
			printf("wdc_atapi_start: timout waiting PHASE_CMDOUT");
			acp->status = ERROR;
			wdc_atapi_done(wdc, xfer);
			return;
		}
		outsw(wdc->sc_iobase + wd_data, acp->command,
		    acp->command_size / sizeof(short));
	}
	wdc->sc_flags |= WDCF_IRQ_WAIT;

#ifdef ATAPI_DEBUG2
	printf("wdc_atapi_start: timeout\n");
#endif
	timeout(wdctimeout, wdc, WAITTIME);
	return;
}


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
		/* Clear the pending interrupt and abort. */
		u_char s = inb(wdc->sc_iobase+wd_status);
		u_char e = inb(wdc->sc_iobase+wd_error);
		u_char i = inb(wdc->sc_iobase+wd_seccnt);

#ifdef ATAPI_DEBUG_WDC
		printf("wdcintr: inactive controller, "
		    "punting st=%02x er=%02x irr=%02x\n", s, e, i);
#endif

		if (s & WDCS_DRQ) {
			int len = inb (wdc->sc_iobase + wd_cyl_lo) +
			    256 * inb (wdc->sc_iobase + wd_cyl_hi);
#ifdef ATAPI_DEBUG_WDC
			printf ("wdcintr: clearing up %d bytes\n", len);
#endif
			wdcbit_bucket (wdc, len);
		}
		return 0;
	}

	WDDEBUG_PRINT(("wdcintr\n"));

	wdc->sc_flags &= ~WDCF_IRQ_WAIT;
	xfer = wdc->sc_xfer.tqh_first;
	if (xfer->c_flags & C_ATAPI) {
		(void) wdc_atapi_intr(wdc,xfer);
		return 0;
	} else {
		return wdc_ata_intr(wdc,xfer);
	}
}


int
wdc_ata_intr(wdc,xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct wd_link *d_link;

	d_link = xfer->d_link;

	if (wait_for_unbusy(wdc) < 0) {
		wdcerror(wdc, "wdcintr: timeout waiting for unbusy");
		wdc->sc_status |= WDCS_ERR;	/* XXX */
	}

	untimeout(wdctimeout, wdc);

	/* Is it not a transfer, but a control operation? */
	if (d_link->sc_state < OPEN) {
		if (wdccontrol(d_link) == 0) {
			/* The drive is busy.  Wait. */
			return 1;
		}
		WDDEBUG_PRINT(("wdc_ata_start from wdc_ata_intr(open) flags %d\n",
		    dc->sc_flags));
		wdc_ata_start(wdc,xfer);
		return 1;
	}

	/* Turn off the DMA channel and unbounce the buffer. */
	if (d_link->sc_mode == WDM_DMA)
		isa_dmadone(xfer->c_flags & B_READ ?
		    DMAMODE_READ : DMAMODE_WRITE,
		    xfer->databuf + xfer->c_skip, xfer->c_nbytes,
		    wdc->sc_drq);

	/* Have we an error? */
	if (wdc->sc_status & WDCS_ERR) {
#ifdef WDDEBUG
		wderror(d_link, NULL, "wdc_ata_start");
#endif
		if ((wdc->sc_flags & WDCF_SINGLE) == 0) {
			wdc->sc_flags |= WDCF_ERROR;
			goto restart;
		}

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT)
			goto bad;
#endif

		wdcunwedge(wdc);
		if (++wdc->sc_errors < WDIORETRIES)
			return 1;

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
			insw(wdc->sc_iobase+wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes >> 1);
		else
			insl(wdc->sc_iobase+wd_data,
			    xfer->databuf + xfer->c_skip, xfer->c_nbytes >> 2);
	}
    
	/* If we encountered any abnormalities, flag it as a soft error. */
	if (wdc->sc_errors > 0 ||
	    (wdc->sc_status & WDCS_CORR) != 0) {
		wderror(d_link, xfer->c_bp, "soft error (corrected)");
		wdc->sc_errors = 0;
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

int
wdcreset(wdc)
	struct wdc_softc *wdc;
{
	int iobase = wdc->sc_iobase;

	/* Reset the device. */
	outb(iobase+wd_ctlr, WDCTL_RST | WDCTL_IDS);
	delay(1000);
	outb(iobase+wd_ctlr, WDCTL_IDS);
	delay(1000);
	(void) inb(iobase+wd_error);
	outb(iobase+wd_ctlr, WDCTL_4BIT);

	if (wait_for_unbusy(wdc) < 0) {
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
	(void) wdcreset(wdc);

	/* Schedule recalibrate for all drives on this controller. */
	for (unit = 0; unit < 2; unit++) {
		if (!wdc->d_link[unit]) continue;
		if (wdc->d_link[unit]->sc_state > RECAL)
			wdc->d_link[unit]->sc_state = RECAL;
	}

	wdc->sc_flags |= WDCF_ERROR;
	++wdc->sc_errors;

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
	int iobase = wdc->sc_iobase;
	int timeout = 0;
	u_char status;
#ifdef WDCNDELAY_DEBUG
	extern int cold;
#endif

	WDDEBUG_PRINT(("wdcwait\n"));

	for (;;) {
		wdc->sc_status = status = inb(iobase+wd_status);
		/*
		 * XXX
		 * If a single slave ATAPI device is attached, it may
		 * have released the bus. Select it and try again.
		 */
		if (status == 0xff && wdc->sc_flags & WDCF_ONESLAVE) {
			outb(iobase+wd_sdh, WDSD_IBM | 0x10);
			wdc->sc_status = status = inb(iobase+wd_status);
		}
		if ((status & WDCS_BSY) == 0 && (status & mask) == mask)
			break;
		if (++timeout > WDCNDELAY) {
#ifdef ATAPI_DEBUG2
			printf("wdcwait: timeout, status %x\n", status);
#endif
			return -1;
		}
		delay(WDCDELAY);
	}
	if (status & WDCS_ERR) {
		wdc->sc_error = inb(iobase+wd_error);
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
        int iobase = wdc->sc_iobase;
        int stat;

	WDDEBUG_PRINT(("wdccommand drive %d\n", drive));

#if defined(DIAGNOSTIC) && defined(WDCDEBUG)
	if ((wdc->sc_flags & WDCF_ACTIVE) == 0)
		printf("wdccommand: controler not active (drive %d)\n", drive);
#endif

        /* Select drive, head, and addressing mode. */
        outb(iobase+wd_sdh, WDSD_IBM | (drive << 4) | head);

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
                outb(iobase + wd_precomp, d_link->sc_lp->d_precompcyl / 4);
        else
                outb(iobase + wd_features, 0);
        outb(iobase + wd_cyl_lo, cylin);
        outb(iobase + wd_cyl_hi, cylin >> 8);
        outb(iobase + wd_sector, sector);
        outb(iobase + wd_seccnt, count);

        /* Send command. */
        outb(iobase + wd_command, command);

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
	int iobase = wdc->sc_iobase;

	WDDEBUG_PRINT(("wdccommandshort\n"));

#if defined(DIAGNOSTIC) && defined(WDCDEBUG)
	if ((wdc->sc_flags & WDCF_ACTIVE) == 0)
		printf("wdccommandshort: controler not active (drive %d)\n",
		    drive);
#endif

        /* Select drive. */
        outb(iobase + wd_sdh, WDSD_IBM | (drive << 4));

        if (wdcwait(wdc, WDCS_DRDY) < 0)
                return -1;

        outb(iobase + wd_command, command);

	return 0;
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
	wdc->sc_errors = 0;
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
	wdstart((void*)d_link->wd_softc);
	WDDEBUG_PRINT(("wdcstart from wdc_ata_done, flags %d\n",
	    wdc->sc_flags));
	wdcstart(wdc);
	splx(s);
}

void
wdc_exec_xfer(d_link, xfer)
	struct wd_link *d_link;
	struct wdc_xfer *xfer;
{
	struct wdc_softc *wdc=(struct wdc_softc *)d_link->wdc_softc;
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
	if (xfer = xfer_free_list.lh_first) {
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
	bzero(xfer,sizeof(struct wdc_xfer));
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

/*
 * Implement operations needed before read/write.
 * Returns 0 if operation still in progress, 1 if completed.
 */
int
wdccontrol(d_link)
	struct wd_link *d_link;
{
	struct wdc_softc *wdc = (void *)d_link->wdc_softc;

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
			goto open;
		outb(wdc->sc_iobase + wd_seccnt, d_link->sc_multiple);
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

	case OPEN:
	open:
		wdc->sc_errors = 0;
		d_link->sc_state = OPEN;
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

/*
 * Get the drive parameters, if ESDI or ATA, or create fake ones for ST506.
 */
int
wdc_get_parms(d_link)
	struct wd_link *d_link;
{
	struct wdc_softc *wdc = (struct wdc_softc *)d_link->wdc_softc;
	int i;
	char tb[DEV_BSIZE];
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
		if ((error = tsleep(wdc, PRIBIO | PCATCH, "wdprm", 0)) != 0) {
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

		strncpy(d_link->sc_params.wdp_model, "unknown",
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
		insw(wdc->sc_iobase + wd_data, tb, sizeof(tb) / sizeof(short));
		bcopy(tb, &d_link->sc_params, sizeof(struct wdparams));

		/* Shuffle string byte order. */
		for (i = 0; i < sizeof(d_link->sc_params.wdp_model); i += 2) {
			u_short *p;
			p = (u_short *)(d_link->sc_params.wdp_model + i);
			*p = ntohs(*p);
		}
	}

	/* Clear any leftover interrupt. */
	(void) inb(wdc->sc_iobase + wd_status);

	/* Restart the queue. */
	WDDEBUG_PRINT(("wdcstart from wdc_get_parms flags %d\n",
	    wdc->sc_flags));
	wdc->sc_flags &= ~WDCF_ACTIVE;
	wdcstart(wdc);

	splx(s);
	return 0;
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

int
wdc_atapi_get_params(ab_link, drive, id)
	struct bus_link *ab_link;
	u_int8_t drive;
	struct atapi_identify *id;
{
	struct wdc_softc *wdc = (void*)ab_link->wdc_softc;
	int status, len, excess = 0;
	int s, error;

	if (wdc->d_link[drive] != 0) {
#ifdef ATAPI_DEBUG_PROBE
		printf("wdc_atapi_get_params: WD drive %d\n", drive);
#endif
		return 0;
	}

	/*
	 * If there is only one ATAPI slave ion the bus,don't probe
	 * drive 0 (master)
	 */

	if (wdc->sc_flags & WDCF_ONESLAVE && drive != 1)
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
	(void)wdcreset(wdc);
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
	len = inb(wdc->sc_iobase + wd_cyl_lo) + 256 *
	    inb(wdc->sc_iobase + wd_cyl_hi);
	if (len != sizeof(struct atapi_identify)) {
		printf("Warning drive %d returned %d/%d of "
		    "indentify device data\n", drive, len,
		    sizeof(struct atapi_identify));
		excess = len - sizeof(struct atapi_identify);
		if (excess < 0)
			excess = 0;
	}
	insw(wdc->sc_iobase + wd_data, id,
	    sizeof(struct atapi_identify)/sizeof(short));
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
	struct wdc_xfer *xfer;
	u_int8_t flags = acp->flags & 0xff;
	int s;
	
	if (flags & A_POLLED) {   /* Must use the queue and wdc_atapi_start */
		struct wdc_xfer xfer_s;
		int i;
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

		if (wdccommand((struct wd_link*)ab_link,
		    ATAPI_PACKET_COMMAND, acp->drive, acp->data_size,
		    0, 0, 0) != 0) {
			printf("can't send atapi paket command\n");
			acp->status = ERROR;
			return;
		}

		/* Wait for cmd i/o phase. */
		for (i = 20000; i > 0; --i) {
			int phase;
			phase = (inb(wdc->sc_iobase + wd_ireason) &
			    (WDCI_CMD | WDCI_IN)) |
			    (inb(wdc->sc_iobase + wd_status) & WDCS_DRQ);
			if (phase == PHASE_CMDOUT)
				break;
			delay(10);
		}
#ifdef ATAPI_DEBUG_WDC
		printf("Wait for cmd i/o phase: i = %d\n", i);
#endif

		outsw(wdc->sc_iobase + wd_data, acp->command,
		    acp->command_size/ sizeof (short));

		/* Wait for data i/o phase. */
		for ( i= 20000; i > 0; --i) {
			int phase;
			phase = (inb(wdc->sc_iobase + wd_ireason) &
			    (WDCI_CMD | WDCI_IN)) |
			    (inb(wdc->sc_iobase + wd_status) & WDCS_DRQ);
			if (phase != PHASE_CMDOUT)
				break;
			delay(10);
		}

#ifdef ATAPI_DEBUG_WDC
		printf("Wait for data i/o phase: i = %d\n", i);
#endif
		while (wdc_atapi_intr(wdc, xfer)) {
			for (i = 2000; i > 0; --i)
				if ((inb(wdc->sc_iobase + wd_status)
				    & WDCS_DRQ) == 0)
					break;
#ifdef ATAPI_DEBUG_WDC
			printf("wdc_atapi_intr: i = %d\n", i);
#endif
		}
		wdc->sc_flags &= ~(WDCF_IRQ_WAIT | WDCF_SINGLE | WDCF_ERROR);
		wdc->sc_errors = 0;
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


/* 
 * the bit bucket
 */
void
wdcbit_bucket(wdc, size)
	struct wdc_softc *wdc; 
	int size;
{
	int iobase = wdc->sc_iobase; 
	int i;

	for (i = 0 ; i < size / 2 ; i++) {
		short null; 
		(void)insw(iobase + wd_data, &null, 1);
	} 

	if (size % 2)
		(void)inb(iobase + wd_data);
}

int
wdc_atapi_intr(wdc, xfer)
	struct wdc_softc *wdc;
	struct wdc_xfer *xfer;
{
	struct atapi_command_packet *acp = xfer->atapi_cmd;
	int len, phase, i;
	int err, st, ire;

	if (wait_for_unbusy(wdc) < 0) {
		printf("wdc_atapi_intr: controller busy\n");
		acp->status = ERROR;
		acp->error = inb (wdc->sc_iobase + wd_error);
		return 0;
	}

#ifdef ATAPI_DEBUG2
	printf("wdc_atapi_intr: %s\n", wdc->sc_dev.dv_xname);
#endif

	len = inb(wdc->sc_iobase + wd_cyl_lo) +
	    256 * inb(wdc->sc_iobase + wd_cyl_hi);

	st = inb(wdc->sc_iobase + wd_status);
	err = inb(wdc->sc_iobase + wd_error);
	ire = inb(wdc->sc_iobase + wd_ireason);
	
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
				printf("%x ", c[i]);
			printf("\n");
		}
#endif

		wdc->sc_flags |= WDCF_IRQ_WAIT;
		outsw(wdc->sc_iobase + wd_data, acp->command,
		    acp->command_size/ sizeof (short));
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
			outsw(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip,
			    xfer->c_bcount / sizeof(short));
			for (i = xfer->c_bcount; i < len; i += sizeof(short))
				outw(wdc->sc_iobase + wd_data, 0);
			xfer->c_bcount = 0;
			return 1;
		} else {
			outsw(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip, len / sizeof(short));
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
			insw(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip,
			    xfer->c_bcount / sizeof(short));
			wdcbit_bucket(wdc, len - xfer->c_bcount);
			xfer->c_bcount = 0;
			return 1;
		} else {
			insw(wdc->sc_iobase + wd_data,
			    xfer->databuf + xfer->c_skip, len / sizeof(short));
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
			acp->error = inb(wdc->sc_iobase + wd_error);
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
	wdc->sc_errors = 0;
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
