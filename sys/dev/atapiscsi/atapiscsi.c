/*      $OpenBSD: atapiscsi.c,v 1.8 1999/07/25 07:09:19 csapuntz Exp $     */

/*
 * This code is derived from code with the copyright below.
 */

/*
 * Copyright (c) 1996, 1998 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#ifndef __BUS_SPACE_HAS_STREAM_METHODS
#define    bus_space_write_multi_stream_2    bus_space_write_multi_2
#define    bus_space_write_multi_stream_4    bus_space_write_multi_4
#define    bus_space_read_multi_stream_2    bus_space_read_multi_2
#define    bus_space_read_multi_stream_4    bus_space_read_multi_4
#endif /* __BUS_SPACE_HAS_STREAM_METHODS */

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include <dev/atapiscsi/atapiconf.h>

#define WDCDEBUG
#define DEBUG_INTR   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_STATUS 0x04
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef WDCDEBUG
int wdcdebug_atapi_mask = 0x0;
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_atapi_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

#define ATAPI_DELAY 10	/* 10 ms, this is used only before sending a cmd */

void  wdc_atapi_minphys  __P((struct buf *bp));
void  wdc_atapi_start	__P((struct channel_softc *,struct wdc_xfer *));
int   wdc_atapi_intr	 __P((struct channel_softc *, struct wdc_xfer *, int));
int   wdc_atapi_ctrl	 __P((struct channel_softc *, struct wdc_xfer *, int));
void  wdc_atapi_done	 __P((struct channel_softc *, struct wdc_xfer *));
void  wdc_atapi_reset	 __P((struct channel_softc *, struct wdc_xfer *));
int   wdc_atapi_send_cmd __P((struct scsi_xfer *sc_xfer));

#define MAX_SIZE MAXPHYS

struct atapiscsi_softc;
struct atapiscsi_xfer;

static int atapiscsi_match __P((struct device *, void *, void *));
static void atapiscsi_attach __P((struct device *, struct device *, void *));

int	wdc_atapi_get_params __P((struct atapiscsi_softc *, u_int8_t, int,
	    struct ataparams *)); 

#define ATAPI_TO_SCSI_SENSE(sc, atapi_error) \
   (sc)->error_code = XS_SHORTSENSE; (sc)->flags = (atapi_error) >> 4; 

struct atapiscsi_softc {
	struct device  sc_dev;
	struct  scsi_link  sc_adapterlink;
	struct  wdc_softc  *sc_wdc;
	u_int8_t sc_channel;  
	int  valid[2];
	struct ata_drive_datas *sc_drvs;
};

static struct scsi_adapter atapiscsi_switch = 
{
	wdc_atapi_send_cmd,
	wdc_atapi_minphys,
	NULL,
	NULL,
};

static struct scsi_device atapiscsi_dev = 
{
	NULL,
	NULL,
	NULL,
	NULL,
};

/* Inital version shares bus_link structure so it can easily
   be "attached to current" wdc driver */

struct cfattach atapiscsi_ca = {
	sizeof(struct atapiscsi_softc), atapiscsi_match, atapiscsi_attach
};

struct cfdriver atapiscsi_cd = {
	NULL, "atapiscsi", DV_DULL
};


int atapiscsi_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;

{
	struct ata_atapi_attach *aa_link = aux;
	struct cfdata *cf = match;

	if (aa_link == NULL)
		return (0);
	if (aa_link->aa_type != T_ATAPI)
		return (0);

	if (cf->cf_loc[0] != aa_link->aa_channel &&
	    cf->cf_loc[0] != -1)
		return (0);

	return (1);
}

void
atapiscsi_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;

{
	struct atapiscsi_softc *as = (struct atapiscsi_softc *)self;
	struct ata_atapi_attach *aa_link = aux;
	struct ataparams ids;
	struct ataparams *id = &ids;
	int drive;

	printf("\n");

	/* Ouch */
	as->valid[0] = as->valid[1] = 0;

	as->sc_wdc = (struct wdc_softc *)parent;
	as->sc_drvs = aa_link->aa_drv_data;
	as->sc_channel = aa_link->aa_channel;
	as->sc_adapterlink.adapter_softc = as;
	as->sc_adapterlink.adapter_target = 7;
	as->sc_adapterlink.adapter_buswidth = 2;
	as->sc_adapterlink.adapter = &atapiscsi_switch;
	as->sc_adapterlink.device = &atapiscsi_dev;
	as->sc_adapterlink.openings = 1;
	as->sc_adapterlink.flags = SDEV_ATAPI;
	as->sc_adapterlink.quirks = SDEV_NOLUNS;
	as->sc_wdc->channels[as->sc_channel]->ch_as = as;

	for (drive = 0; drive < 2 ; drive++ ) {
		struct ata_drive_datas *drvp = &as->sc_drvs[drive];
			
		if ((drvp->drive_flags & DRIVE_ATAPI) &&
		    (wdc_atapi_get_params(as, drive,
		    SCSI_POLL|SCSI_NOSLEEP, id) == COMPLETE)) {

			as->valid[drive] = 1;

			/* This is wrong. All the devices on the ATAPI bus
			   will be called atapibus by the IDE side of the
			   driver. */
			drvp->drv_softc = (struct device *)as;
			wdc_probe_caps(drvp);

			if ((id->atap_config & ATAPI_CFG_CMD_MASK) 
			    == ATAPI_CFG_CMD_16)
				drvp->atapi_cap |= ACAP_LEN;

			drvp->atapi_cap |= (id->atap_config & ATAPI_CFG_DRQ_MASK);
		}
	}
}


void
wdc_atapibus_final_attach(chp)
	struct channel_softc *chp;
{
	if (chp->ch_as)
		config_found((struct device *)chp->ch_as, 
			     &chp->ch_as->sc_adapterlink, scsiprint);
}

void
wdc_atapibus_attach(chp)
	struct channel_softc *chp;
{
	struct wdc_softc *wdc = chp->wdc;
	int channel = chp->channel;
	struct ata_atapi_attach aa_link;

	/*
	 * Fill in the adapter.
	 */
	memset(&aa_link, 0, sizeof(struct ata_atapi_attach));
	aa_link.aa_type = T_ATAPI;
	aa_link.aa_channel = channel;
	aa_link.aa_openings = 1;
	aa_link.aa_drv_data = chp->ch_drive; /* pass the whole array */
#if 0
	aa_link.aa_bus_private = &wdc->sc_atapi_adapter;
#endif
	(void)config_found(&wdc->sc_dev, (void *)&aa_link, atapi_print);
}

void
wdc_atapi_minphys (struct buf *bp)
{
	if(bp->b_bcount > MAX_SIZE)
		bp->b_bcount = MAX_SIZE;
	minphys(bp);
}


/*
 *  The scsi_cmd interface works as follows:
 *
 */

int
wdc_atapi_get_params(as, drive, flags, id)
	struct atapiscsi_softc *as;
	u_int8_t drive;
	int flags;
	struct ataparams *id;
{
	struct wdc_softc *wdc = as->sc_wdc;
	struct channel_softc *chp =
	    wdc->channels[as->sc_channel];
	struct wdc_command wdc_c;

	/* if no ATAPI device detected at wdc attach time, skip */
	/*
	 * XXX this will break scsireprobe if this is of any interest for
	 * ATAPI devices one day.
	 */
	if ((chp->ch_drive[drive].drive_flags & DRIVE_ATAPI) == 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: drive %d not present\n",
		    drive), DEBUG_PROBE);
		return -1;
	}
	memset(&wdc_c, 0, sizeof(struct wdc_command));
	wdc_c.r_command = ATAPI_SOFT_RESET;
	wdc_c.r_st_bmask = 0;
	wdc_c.r_st_pmask = 0;
	wdc_c.flags = AT_POLL;
	wdc_c.timeout = WDC_RESET_WAIT;
	if (wdc_exec_command(&chp->ch_drive[drive], &wdc_c) != WDC_COMPLETE) {
		printf("wdc_atapi_get_params: ATAPI_SOFT_RESET failed for"
		    " drive %s:%d:%d: driver failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive);
		panic("wdc_atapi_get_params");
	}
	if (wdc_c.flags & (AT_ERROR | AT_TIMEOU | AT_DF)) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_SOFT_RESET "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive, 
		    wdc_c.r_error), DEBUG_PROBE);
		return -1;
	}
	chp->ch_drive[drive].state = 0;

	bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_status);
	
	/* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);
	if (ata_get_params(&chp->ch_drive[drive], AT_POLL, id) != 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_IDENTIFY_DEVICE "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive, 
		    wdc_c.r_error), DEBUG_PROBE);
		return -1;
	}
	return COMPLETE;
}

int
wdc_atapi_send_cmd(sc_xfer)
	struct scsi_xfer *sc_xfer;
{
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	struct wdc_softc *wdc = as->sc_wdc;
	struct wdc_xfer *xfer;
	int flags = sc_xfer->flags;
	int channel = as->sc_channel;
	int drive = sc_xfer->sc_link->target;
	int s, ret;

	WDCDEBUG_PRINT(("wdc_atapi_send_cmd %s:%d:%d\n",
	    wdc->sc_dev.dv_xname, channel, drive), DEBUG_XFERS);

	if (drive > 1 || !as->valid[drive]) {
		sc_xfer->error = XS_DRIVER_STUFFUP;
		return COMPLETE;
	}

	xfer = wdc_get_xfer(flags & SCSI_NOSLEEP ? WDC_NOSLEEP : WDC_CANSLEEP);
	if (xfer == NULL) {
		return TRY_AGAIN_LATER;
	}
	if (sc_xfer->flags & SCSI_POLL)
		xfer->c_flags |= C_POLL;
	xfer->drive = drive;
	xfer->c_flags |= C_ATAPI;
	xfer->cmd = sc_xfer;
	xfer->databuf = sc_xfer->data;
	xfer->c_bcount = sc_xfer->datalen;
	xfer->c_start = wdc_atapi_start;
	xfer->c_intr = wdc_atapi_intr;
	s = splbio();
	wdc_exec_xfer(wdc->channels[channel], xfer);
#ifdef DIAGNOSTIC
	if ((sc_xfer->flags & SCSI_POLL) != 0 &&
	    (sc_xfer->flags & ITSDONE) == 0)
		panic("wdc_atapi_send_cmd: polled command not done");
#endif
	ret = (sc_xfer->flags & ITSDONE) ? COMPLETE : SUCCESSFULLY_QUEUED;
	splx(s);
	return ret;
}

void
wdc_atapi_start(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	WDCDEBUG_PRINT(("wdc_atapi_start %s:%d:%d, scsi flags 0x%x \n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive,
	    sc_xfer->flags), DEBUG_XFERS);
	/* Adjust C_DMA, it may have changed if we are requesting sense */
	if ((drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA)) &&
	    (sc_xfer->datalen > 0 || (xfer->c_flags & C_SENSE)))
		xfer->c_flags |= C_DMA;
	else
		xfer->c_flags &= ~C_DMA;
	/* Do control operations specially. */
	if (drvp->state < READY) {
		if (drvp->state != IDENTIFY) {
			printf("%s:%d:%d: bad state %d in wdc_atapi_start\n",
			    chp->wdc->sc_dev.dv_xname, chp->channel,
			    xfer->drive, drvp->state);
			panic("wdc_atapi_start: bad state");
		}
		wdc_atapi_ctrl(chp, xfer, 0);
		return;
	}
	
	if (wdc_select_drive(chp, xfer->drive, ATAPI_DELAY) < 0) {
		printf("wdc_atapi_start: not ready, st = %02x\n",
		    chp->ch_status);
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return;
	}

	/*
	 * Even with WDCS_ERR, the device should accept a command packet
	 * Limit length to what can be stuffed into the cylinder register
	 * (16 bits).  Some CD-ROMs seem to interpret '0' as 65536,
	 * but not all devices do that and it's not obvious from the
	 * ATAPI spec that that behaviour should be expected.  If more
	 * data is necessary, multiple data transfer phases will be done.
	 */

	wdccommand(chp, xfer->drive, ATAPI_PKT_CMD, 
	    sc_xfer->datalen <= 0xffff ? sc_xfer->datalen : 0xffff,
	    0, 0, 0, 
	    (xfer->c_flags & C_DMA) ? ATAPI_PKT_CMD_FTRE_DMA : 0);

	/*
	 * If there is no interrupt for CMD input, busy-wait for it (done in 
	 * the interrupt routine. If it is a polled command, call the interrupt
	 * routine until command is done.
	 */
	if (((drvp->atapi_cap & ATAPI_CFG_DRQ_MASK) != ATAPI_CFG_IRQ_DRQ)
	    || (sc_xfer->flags & SCSI_POLL)) {
		/* Wait for at last 400ns for status bit to be valid */
		DELAY(1);
		wdc_atapi_intr(chp, xfer, 0);
	} else {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		timeout(wdctimeout, chp, hz);
	}

	if (sc_xfer->flags & SCSI_POLL) {
		while ((sc_xfer->flags & ITSDONE) == 0) {
			/* Wait for at last 400ns for status bit to be valid */
			DELAY(1);
			wdc_atapi_intr(chp, xfer, 0);
		}
	}
}

int
wdc_atapi_intr(chp, xfer, irq)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int irq;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	int len, phase, i, retries=0;
	int ire, dma_err = 0;
	int dma_flags = 0;
	struct scsi_generic _cmd_reqsense;
	struct scsi_sense *cmd_reqsense =
	    (struct scsi_sense *)&_cmd_reqsense;
	u_int32_t cmd[4];
	int  cmdlen = (drvp->atapi_cap & ACAP_LEN) ? 16 : 12;

	memset (cmd, 0, sizeof(cmd));

	WDCDEBUG_PRINT(("wdc_atapi_intr %s:%d:%d\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive), DEBUG_INTR);

	/* Is it not a transfer, but a control operation? */
	if (drvp->state < READY) {
		printf("%s:%d:%d: bad state %d in wdc_atapi_intr\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		    drvp->state);
		panic("wdc_atapi_intr: bad state\n");
	}

	/* We should really wait_for_unbusy here too before 
	   switching drives. */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_sdh,
	    WDSD_IBM | (xfer->drive << 4));

	/* Ack interrupt done in wait_for_unbusy */
	if (wait_for_unbusy(chp,
	    (irq == 0) ? sc_xfer->timeout : 0) != 0) {
		if (irq && (xfer->c_flags & C_TIMEOU) == 0)
			return 0; /* IRQ was not for us */
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip=%d\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		    xfer->c_bcount, xfer->c_skip);
		if (xfer->c_flags & C_DMA)
			drvp->n_dmaerrs++;
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return 1;
	}
	/* If we missed an IRQ and were using DMA, flag it as a DMA error */
	if ((xfer->c_flags & C_TIMEOU) && (xfer->c_flags & C_DMA))
		drvp->n_dmaerrs++;
	/* 
	 * if the request sense command was aborted, report the short sense
	 * previously recorded, else continue normal processing
	 */

	if ((xfer->c_flags & C_SENSE) != 0 &&
	    (chp->ch_status & WDCS_ERR) != 0 &&
	    (chp->ch_error & WDCE_ABRT) != 0) {
		WDCDEBUG_PRINT(("wdc_atapi_intr: request_sense aborted, "
		    "calling wdc_atapi_done()"
		    ), DEBUG_INTR);
		wdc_atapi_done(chp, xfer);
		return 1;
	}

	if (xfer->c_flags & C_DMA) {
		dma_flags = ((sc_xfer->flags & SCSI_DATA_IN) ||
		    (xfer->c_flags & C_SENSE)) ?  WDC_DMA_READ : 0;
		dma_flags |= sc_xfer->flags & SCSI_POLL ? WDC_DMA_POLL : 0;
	}
again:
	len = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_lo) +
	    256 * bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_hi);
	ire = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_ireason);
	phase = (ire & (WDCI_CMD | WDCI_IN)) | (chp->ch_status & WDCS_DRQ);
	WDCDEBUG_PRINT(("wdc_atapi_intr: c_bcount %d len %d st 0x%x err 0x%x "
	    "ire 0x%x :", xfer->c_bcount,
	    len, chp->ch_status, chp->ch_error, ire), DEBUG_INTR);

	switch (phase) {
	case PHASE_CMDOUT:
		if (xfer->c_flags & C_SENSE) {
			memset(cmd_reqsense, 0, sizeof(struct scsi_generic));
			cmd_reqsense->opcode = REQUEST_SENSE;
			cmd_reqsense->length = xfer->c_bcount;
			bcopy(cmd_reqsense, cmd, sizeof(struct scsi_generic));
		} else {
			bcopy(sc_xfer->cmd, cmd, sc_xfer->cmdlen);
		}
		WDCDEBUG_PRINT(("PHASE_CMDOUT\n"), DEBUG_INTR);
		/* Init the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			if ((*chp->wdc->dma_init)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive,
			    xfer->databuf, xfer->c_bcount, dma_flags) != 0) {
				sc_xfer->error = XS_DRIVER_STUFFUP;
				break;
			}
		}

		/* send packet command */
		/* Commands are 12 or 16 bytes long. It's 32-bit aligned */
		if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM)) {
			if (drvp->drive_flags & DRIVE_CAP32) {
				bus_space_write_multi_4(chp->data32iot,
				    chp->data32ioh, 0,
				    (u_int32_t *)cmd,
				    cmdlen >> 2);
			} else {
				bus_space_write_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)cmd,
				    cmdlen >> 1);
			}
		} else {
			if (drvp->drive_flags & DRIVE_CAP32) {
				bus_space_write_multi_stream_4(chp->data32iot,
				    chp->data32ioh, 0,
				    (u_int32_t *)cmd,
				    cmdlen >> 2);
			} else {
				bus_space_write_multi_stream_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)cmd,
				    cmdlen >> 1);
			}
		}
		/* Start the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			(*chp->wdc->dma_start)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive, dma_flags);
		}

		if ((sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
		}
		return 1;

	 case PHASE_DATAOUT:
		/* write data */
		WDCDEBUG_PRINT(("PHASE_DATAOUT\n"), DEBUG_INTR);
		if ((sc_xfer->flags & SCSI_DATA_OUT) == 0 ||
		    (xfer->c_flags & C_DMA) != 0) {
			printf("wdc_atapi_intr: bad data phase DATAOUT\n");
			if (xfer->c_flags & C_DMA) {
				(*chp->wdc->dma_finish)(chp->wdc->dma_arg,
				    chp->channel, xfer->drive, dma_flags);
				drvp->n_dmaerrs++;
			}
			sc_xfer->error = XS_TIMEOUT;
			wdc_atapi_reset(chp, xfer);
			return 1;
		}
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: write only "
			    "%d of %d requested bytes\n", xfer->c_bcount, len);
			if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM)) {
				bus_space_write_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    xfer->c_bcount >> 1);
			} else {
				bus_space_write_multi_stream_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    xfer->c_bcount >> 1);
			}
			for (i = xfer->c_bcount; i < len; i += 2)
				bus_space_write_2(chp->cmd_iot, chp->cmd_ioh,
				    wd_data, 0);
			xfer->c_skip += xfer->c_bcount;
			xfer->c_bcount = 0;
		} else {
			if (drvp->drive_flags & DRIVE_CAP32) {
			    if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM))
				bus_space_write_multi_4(chp->data32iot,
				    chp->data32ioh, 0,
				    (u_int32_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 2);
			    else
				bus_space_write_multi_stream_4(chp->data32iot,
				    chp->data32ioh, wd_data,
				    (u_int32_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 2);

			    xfer->c_skip += len & 0xfffffffc;
			    xfer->c_bcount -= len & 0xfffffffc;
			    len = len & 0x03;
			}
			if (len > 0) {
			    if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM))
				bus_space_write_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 1);
			    else
				bus_space_write_multi_stream_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 1);
			    xfer->c_skip += len;
			    xfer->c_bcount -= len;
			}
		}

		if ((sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
		}
		return 1;

	case PHASE_DATAIN:
		/* Read data */
		WDCDEBUG_PRINT(("PHASE_DATAIN\n"), DEBUG_INTR);
		if (((sc_xfer->flags & SCSI_DATA_IN) == 0 &&
		    (xfer->c_flags & C_SENSE) == 0) || 
		    (xfer->c_flags & C_DMA) != 0) {
			printf("wdc_atapi_intr: bad data phase DATAIN\n");
			if (xfer->c_flags & C_DMA) {
				(*chp->wdc->dma_finish)(chp->wdc->dma_arg,
				    chp->channel, xfer->drive, dma_flags);
				drvp->n_dmaerrs++;
			}
			sc_xfer->error = XS_TIMEOUT;
			wdc_atapi_reset(chp, xfer);
			return 1;
		}
		if (xfer->c_bcount < len) {
			printf("wdc_atapi_intr: warning: reading only "
			    "%d of %d bytes\n", xfer->c_bcount, len);
			if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM)) {
			    bus_space_read_multi_2(chp->cmd_iot,
			    chp->cmd_ioh, wd_data,
			    (u_int16_t *)((char *)xfer->databuf +
			                  xfer->c_skip),
			    xfer->c_bcount >> 1);
			} else {
			    bus_space_read_multi_stream_2(chp->cmd_iot,
			    chp->cmd_ioh, wd_data,
			    (u_int16_t *)((char *)xfer->databuf +
			                  xfer->c_skip),
			    xfer->c_bcount >> 1);
			}
			wdcbit_bucket(chp, len - xfer->c_bcount);
			xfer->c_skip += xfer->c_bcount;
			xfer->c_bcount = 0;
		} else {
			if (drvp->drive_flags & DRIVE_CAP32) {
			    if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM))
				bus_space_read_multi_4(chp->data32iot,
				    chp->data32ioh, 0,
				    (u_int32_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 2);
			    else
				bus_space_read_multi_stream_4(chp->data32iot,
				    chp->data32ioh, wd_data,
				    (u_int32_t *)((char *)xfer->databuf +
				                  xfer->c_skip),
				    len >> 2);
				
			    xfer->c_skip += len & 0xfffffffc;
			    xfer->c_bcount -= len & 0xfffffffc;
			    len = len & 0x03;
			}
			if (len > 0) {
			    if ((chp->wdc->cap & WDC_CAPABILITY_ATAPI_NOSTREAM))
				bus_space_read_multi_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip), 
				    len >> 1);
			    else
				bus_space_read_multi_stream_2(chp->cmd_iot,
				    chp->cmd_ioh, wd_data,
				    (u_int16_t *)((char *)xfer->databuf +
				                  xfer->c_skip), 
				    len >> 1);
			    xfer->c_skip += len;
			    xfer->c_bcount -=len;
			}
		}

		if ((sc_xfer->flags & SCSI_POLL) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
		}
		return 1;

	case PHASE_ABORTED:
	case PHASE_COMPLETED:
		WDCDEBUG_PRINT(("PHASE_COMPLETED\n"), DEBUG_INTR);
		/* turn off DMA channel */
		if (xfer->c_flags & C_DMA) {
			dma_err = (*chp->wdc->dma_finish)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive, dma_flags);
			if (xfer->c_flags & C_SENSE)
				xfer->c_bcount -=
				    sizeof(sc_xfer->sense);
			else
				xfer->c_bcount -= sc_xfer->datalen;
		}
		if (xfer->c_flags & C_SENSE) {
			if ((chp->ch_status & WDCS_ERR) || dma_err < 0) {
				/*
				 * request sense failed ! it's not suppossed
				 * to be possible
				 */
				if (dma_err < 0)
					drvp->n_dmaerrs++;
				sc_xfer->error = XS_RESET;
				wdc_atapi_reset(chp, xfer);
				return (1);
			} else if (xfer->c_bcount <
			    sizeof(sc_xfer->sense)) {
				/* use the sense we just read */
				sc_xfer->error = XS_SENSE;
			} else {
				/*
				 * command completed, but no data was read.
				 * use the short sense we saved previsouly.
				 */
				sc_xfer->error = XS_SHORTSENSE;
			}
		} else {
			sc_xfer->resid = xfer->c_bcount;
			if (chp->ch_status & WDCS_ERR) {
				/* save the short sense */
				sc_xfer->error = XS_SHORTSENSE;
				ATAPI_TO_SCSI_SENSE(&sc_xfer->sense, chp->ch_error);
				if ((sc_xfer->sc_link->quirks &
				    ADEV_NOSENSE) == 0) {
					/*
					 * let the driver issue a
					 * 'request sense'
					 */
					xfer->databuf = &sc_xfer->sense;
					xfer->c_bcount =
					    sizeof(sc_xfer->sense);
					xfer->c_skip = 0;
					xfer->c_flags |= C_SENSE;
					wdc_atapi_start(chp, xfer);
					return 1;
				}
			} else if (dma_err < 0) {
				drvp->n_dmaerrs++;
				sc_xfer->error = XS_RESET;
				wdc_atapi_reset(chp, xfer);
				return (1);
			}
		}
		if (xfer->c_bcount != 0) {
			WDCDEBUG_PRINT(("wdc_atapi_intr: bcount value is "
			    "%d after io\n", xfer->c_bcount), DEBUG_XFERS);
		}
#ifdef DIAGNOSTIC
		if (xfer->c_bcount < 0) {
			printf("wdc_atapi_intr warning: bcount value "
			    "is %d after io\n", xfer->c_bcount);
		}
#endif
		break;

	default:
		if (++retries<500) {
			DELAY(100);
			chp->ch_status = bus_space_read_1(chp->cmd_iot,
			    chp->cmd_ioh, wd_status);
			chp->ch_error = bus_space_read_1(chp->cmd_iot,
			    chp->cmd_ioh, wd_error);
			goto again;
		}
		printf("wdc_atapi_intr: unknown phase 0x%x\n", phase);
		if (chp->ch_status & WDCS_ERR) {
			sc_xfer->error = XS_SHORTSENSE;
			ATAPI_TO_SCSI_SENSE(&sc_xfer->sense, chp->ch_error);
		} else {
			sc_xfer->error = XS_RESET;
			wdc_atapi_reset(chp, xfer);
			return (1);
		}
	}
	WDCDEBUG_PRINT(("wdc_atapi_intr: wdc_atapi_done() (end), error 0x%x "
	    "\n", sc_xfer->error),
	    DEBUG_INTR);
	wdc_atapi_done(chp, xfer);
	return (1);
}

int
wdc_atapi_ctrl(chp, xfer, irq)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int irq;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	char *errstring = NULL;
	int delay = (irq == 0) ? ATAPI_DELAY : 0;

	/* Ack interrupt done in wait_for_unbusy */
again:
	WDCDEBUG_PRINT(("wdc_atapi_ctrl %s:%d:%d state %d\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive, drvp->state),
	    DEBUG_INTR | DEBUG_FUNCS);

	/* We shouldn't have to select the drive in these states.
	   If we do, there are other, more serious problems */
	if (drvp->state != IDENTIFY_WAIT &&
	    drvp->state != PIOMODE_WAIT &&
	    drvp->state != DMAMODE_WAIT)
		wdc_select_drive(chp, xfer->drive, delay);

	switch (drvp->state) {
		/* You need to send an ATAPI drive an ATAPI-specific
		   command to revive it after a hard reset. Identify
		   is about the most innocuous thing you can do
		   that's guaranteed to be there */
	case IDENTIFY:
		wdccommandshort(chp, drvp->drive, ATAPI_IDENTIFY_DEVICE);
		drvp->state = IDENTIFY_WAIT;
		break;
	
	case IDENTIFY_WAIT:
		errstring = "IDENTIFY";
		
		/* Some ATAPI devices need to try to read the media
		   before responding. Well, let's hope resets while
		   polling are few and far between */
		if (wdcwait(chp, 0, 0, (irq == 0) ? 10000 : 0)) 
			goto timeout;

		if (!(chp->ch_status & WDCS_DRQ) &&
		    chp->ch_status & WDCS_ERR) {
			chp->ch_error = bus_space_read_1(chp->cmd_iot,
							 chp->cmd_ioh, 
							 wd_error);
			goto error;
		}

		wdcbit_bucket(chp, 512);

		drvp->state = PIOMODE;
		break;

	case PIOMODE:
piomode:
		/* Don't try to set mode if controller can't be adjusted */
		if ((chp->wdc->cap & WDC_CAPABILITY_MODE) == 0)
			goto ready;
		/* Also don't try if the drive didn't report its mode */
		if ((drvp->drive_flags & DRIVE_MODE) == 0)
			goto ready;;
		wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
		    0x08 | drvp->PIO_mode, WDSF_SET_MODE);
		drvp->state = PIOMODE_WAIT;
		break;
	case PIOMODE_WAIT:
		errstring = "piomode";
		if (wait_for_unbusy(chp, delay))
			goto timeout;
		if (chp->ch_status & WDCS_ERR) {
			if (drvp->PIO_mode < 3) {
				drvp->PIO_mode = 3;
				goto piomode;
			} else {
				goto error;
			}
		}
	/* fall through */

	case DMAMODE:
		if (drvp->drive_flags & DRIVE_UDMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x40 | drvp->UDMA_mode, WDSF_SET_MODE);
		} else if (drvp->drive_flags & DRIVE_DMA) {
			wdccommand(chp, drvp->drive, SET_FEATURES, 0, 0, 0,
			    0x20 | drvp->DMA_mode, WDSF_SET_MODE);
		} else {
			goto ready;
		}
		drvp->state = DMAMODE_WAIT;
		break;
	case DMAMODE_WAIT:
		errstring = "dmamode";
		if (wait_for_unbusy(chp, delay))
			goto timeout;
		if (chp->ch_status & WDCS_ERR)
			goto error;
	/* fall through */

	case READY:
	ready:
		drvp->state = READY;
		xfer->c_intr = wdc_atapi_intr;
		wdc_atapi_start(chp, xfer);
		return 1;
	}
	if ((sc_xfer->flags & SCSI_POLL) == 0) {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		xfer->c_intr = wdc_atapi_ctrl;
		timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
	} else {
		goto again;
	}
	return 1;

timeout:
	if (irq && (xfer->c_flags & C_TIMEOU) == 0) {
		return 0; /* IRQ was not for us */
	}
	printf("%s:%d:%d: %s timed out\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive, errstring);
	sc_xfer->error = XS_TIMEOUT;
	wdc_atapi_reset(chp, xfer);
	return 1;
error:
	printf("%s:%d:%d: %s ",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    errstring);
	printf("error (0x%x)\n", chp->ch_error);
	sc_xfer->error = XS_SHORTSENSE;
	ATAPI_TO_SCSI_SENSE(&sc_xfer->sense, chp->ch_error);
	wdc_atapi_reset(chp, xfer);
	return 1;
}

void
wdc_atapi_done(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	int need_done =  xfer->c_flags & C_NEEDDONE;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	WDCDEBUG_PRINT(("wdc_atapi_done %s:%d:%d: flags 0x%x\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    (u_int)xfer->c_flags), DEBUG_XFERS);
	/* remove this command from xfer queue */
	wdc_free_xfer(chp, xfer);
	sc_xfer->flags |= ITSDONE;
	if (drvp->n_dmaerrs ||
	  (sc_xfer->error != XS_NOERROR && sc_xfer->error != XS_SENSE &&
	  sc_xfer->error != XS_SHORTSENSE)) {
		drvp->n_dmaerrs = 0;
		wdc_downgrade_mode(drvp);
	}
	    
	if (need_done) {
		WDCDEBUG_PRINT(("wdc_atapi_done: scsi_done\n"), DEBUG_XFERS);
		scsi_done(sc_xfer);
	}
	WDCDEBUG_PRINT(("wdcstart from wdc_atapi_done, flags 0x%x\n",
	    chp->ch_flags), DEBUG_XFERS);
	wdcstart(chp);
}

void
wdc_atapi_reset(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct scsi_xfer *sc_xfer = xfer->cmd;

	chp->ch_status = bus_space_read_1(chp->cmd_iot,
					 chp->cmd_ioh, 
					 wd_status);

	if (chp->ch_status & WDCS_DRQ) {
		printf ("wdc_atapi_reset: DRQ is asserted. We've really messed up\n");
		wdc_reset_channel(drvp);
	}

	wdccommandshort(chp, xfer->drive, ATAPI_SOFT_RESET);

	/* Some ATAPI devices need extra time to find their
	   brains after a reset
	 */
	delay(5000);

	drvp->state = 0;
	if (wait_for_unbusy(chp, WDC_RESET_WAIT) != 0) {
		printf("%s:%d:%d: reset failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel,
		    xfer->drive);
		sc_xfer->error = XS_SELTIMEOUT;
	}

	wdc_atapi_done(chp, xfer);
	return;
}

