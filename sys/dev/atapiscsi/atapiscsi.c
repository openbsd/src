/*      $OpenBSD: atapiscsi.c,v 1.15 1999/10/29 01:15:16 deraadt Exp $     */

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
#include <scsi/scsi_tape.h>
#include <scsi/scsiconf.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

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
#define DEBUG_DSC    0x20
#define DEBUG_POLL   0x40
#define DEBUG_ERRORS 0x80   /* Debug error handling code */

#ifdef WDCDEBUG
int wdcdebug_atapi_mask = 0;
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_atapi_mask & (level)) \
		printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif

/* 10 ms, this is used only before sending a cmd.  */
#define ATAPI_DELAY 10
#define ATAPI_RESET_WAIT 2000

/* When polling, let the exponential backoff max out at 1 second's interval. */
#define ATAPI_POLL_MAXTIC (hz)

void  wdc_atapi_minphys __P((struct buf *bp));
void  wdc_atapi_start __P((struct channel_softc *,struct wdc_xfer *));
int   wdc_atapi_intr __P((struct channel_softc *, struct wdc_xfer *, int));
int   wdc_atapi_ctrl __P((struct channel_softc *, struct wdc_xfer *, int));
void  wdc_atapi_done __P((struct channel_softc *, struct wdc_xfer *));
void  wdc_atapi_reset __P((struct channel_softc *, struct wdc_xfer *));
int   wdc_atapi_send_cmd __P((struct scsi_xfer *sc_xfer));

#define MAX_SIZE MAXPHYS

struct atapiscsi_softc;
struct atapiscsi_xfer;

int	atapiscsi_match __P((struct device *, void *, void *));
void	atapiscsi_attach __P((struct device *, struct device *, void *));

int	wdc_atapi_get_params __P((struct channel_softc *, u_int8_t, struct ataparams *)); 

int	atapi_dsc_wait __P((struct ata_drive_datas *, int));
int	atapi_dsc_ready __P((void *));
int	atapi_dsc_semiready __P((void *));
int	atapi_poll_wait __P((int (*) __P((void *)), void *, int, int, char *));
void    atapi_to_scsi_sense __P((struct scsi_xfer *, u_int8_t));

struct atapiscsi_softc {
	struct device  sc_dev;
	struct  scsi_link  sc_adapterlink;
	struct channel_softc *chp;
	enum atapi_state { as_none, as_cmdout, as_data } protocol_phase;

	int diagnostics_printed;
#define ATAPI_DIAG_UNEXP_CMD  0x01
#define ATAPI_DIAG_POLARITY   0x02
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


int
atapiscsi_match(parent, match, aux)
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
	struct channel_softc *chp = aa_link->aa_bus_private;
	struct ataparams ids;
	struct ataparams *id = &ids;
	int drive;

	printf("\n");

	as->chp = chp;
	as->sc_adapterlink.adapter_softc = as;
	as->sc_adapterlink.adapter_target = 7;
	as->sc_adapterlink.adapter_buswidth = 2;
	as->sc_adapterlink.adapter = &atapiscsi_switch;
	as->sc_adapterlink.device = &atapiscsi_dev;
	as->sc_adapterlink.openings = 1;
	as->sc_adapterlink.flags = SDEV_ATAPI;
	as->sc_adapterlink.quirks = SDEV_NOLUNS;

	for (drive = 0; drive < 2 ; drive++ ) {
		struct ata_drive_datas *drvp = &chp->ch_drive[drive];
			
		if ((drvp->drive_flags & DRIVE_ATAPI) &&
		    (wdc_atapi_get_params(chp, drive, id) == COMPLETE)) {
			/* Temporarily, the device will be called
			   atapiscsi. */
			drvp->drv_softc = (struct device*)as;
			wdc_probe_caps(drvp, id);

			WDCDEBUG_PRINT(
			    ("general config %04x capabilities %04x ",
			    id->atap_config, id->atap_capabilities1),
			    DEBUG_PROBE);

			/* Tape drives do funny DSC stuff */
			if (ATAPI_CFG_TYPE(id->atap_config) == 
			    ATAPI_CFG_TYPE_SEQUENTIAL)
				drvp->atapi_cap |= ACAP_DSC;

			if ((id->atap_config & ATAPI_CFG_CMD_MASK) ==
			    ATAPI_CFG_CMD_16)
				drvp->atapi_cap |= ACAP_LEN;

			drvp->atapi_cap |=
			    (id->atap_config & ATAPI_CFG_DRQ_MASK);

			WDCDEBUG_PRINT(("driver caps %04x\n", drvp->atapi_cap),
			    DEBUG_PROBE);
		} else
			drvp->drive_flags &= ~DRIVE_ATAPI;
	}

	as->sc_adapterlink.scsibus = (u_int8_t)-1;

	config_found((struct device *)as, 
		     &as->sc_adapterlink, scsiprint);

	if (as->sc_adapterlink.scsibus != (u_int8_t)-1) {
		int bus = as->sc_adapterlink.scsibus;

		for (drive = 0; drive < 2; drive++) {
			extern struct cfdriver scsibus_cd;

			struct scsibus_softc *scsi = scsibus_cd.cd_devs[bus];
			struct scsi_link *link = scsi->sc_link[drive][0];
			struct ata_drive_datas *drvp = &chp->ch_drive[drive];

			if (drvp->drv_softc == (struct device *)as && link) {
				drvp->drv_softc = link->device_softc;
				wdc_print_caps(drvp);
			}
		}
	}
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
	bzero(&aa_link, sizeof(struct ata_atapi_attach));
	aa_link.aa_type = T_ATAPI;
	aa_link.aa_channel = channel;
	aa_link.aa_openings = 1;
	aa_link.aa_drv_data = NULL; 
	aa_link.aa_bus_private = chp;

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
wdc_atapi_get_params(chp, drive, id)
	struct channel_softc *chp;
	u_int8_t drive;
	struct ataparams *id;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[drive];
	struct wdc_command wdc_c;

	/* if no ATAPI device detected at wdc attach time, skip */
	/*
	 * XXX this will break scsireprobe if this is of any interest for
	 * ATAPI devices one day.
	 */
	if ((drvp->drive_flags & DRIVE_ATAPI) == 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: drive %d not present\n",
		    drive), DEBUG_PROBE);
		return (-1);
	}
	bzero(&wdc_c, sizeof(struct wdc_command));
	wdc_c.r_command = ATAPI_SOFT_RESET;
	wdc_c.r_st_bmask = 0;
	wdc_c.r_st_pmask = 0;
	wdc_c.flags = AT_POLL;
	wdc_c.timeout = ATAPI_RESET_WAIT;
	if (wdc_exec_command(drvp, &wdc_c) != WDC_COMPLETE) {
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
		return (-1);
	}
	drvp->state = 0;

	bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_status);
	
	/* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);
	if (ata_get_params(drvp, AT_POLL, id) != 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_IDENTIFY_DEVICE "
		    "failed for drive %s:%d:%d: error 0x%x\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive, 
		    wdc_c.r_error), DEBUG_PROBE);
		return (-1);
	}
	return (COMPLETE);
}

int
wdc_atapi_send_cmd(sc_xfer)
	struct scsi_xfer *sc_xfer;
{
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	int drive = sc_xfer->sc_link->target;
	struct channel_softc *chp = as->chp;
	struct ata_drive_datas *drvp = &chp->ch_drive[drive];
	struct wdc_softc *wdc = chp->wdc;
	struct wdc_xfer *xfer;
	int flags = sc_xfer->flags;
	int s, ret, saved_datalen;
	char saved_len_bytes[3];

restart:
	saved_datalen = 0;

	WDCDEBUG_PRINT(("wdc_atapi_send_cmd %s:%d:%d\n",
	    wdc->sc_dev.dv_xname, chp->channel, drive), DEBUG_XFERS);

	if (drive > 1 || !(drvp->drive_flags & DRIVE_ATAPI)) {
		sc_xfer->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}

	xfer = wdc_get_xfer(flags & SCSI_NOSLEEP ? WDC_NOSLEEP : WDC_CANSLEEP);
	if (xfer == NULL) {
		return (TRY_AGAIN_LATER);
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

	if (drvp->atapi_cap & ACAP_DSC) {
		WDCDEBUG_PRINT(("about to send cmd %x ", sc_xfer->cmd->opcode),
		    DEBUG_DSC);
		xfer->c_flags |= C_NEEDDONE;
		switch (sc_xfer->cmd->opcode) {
		case READ:
		case WRITE:
			/* If we are not in buffer availability mode,
			   we limit the first request to 0 bytes, which
			   gets us into buffer availability mode without
			   holding the bus.  */
			if (!(drvp->drive_flags & DRIVE_DSCBA)) {
				saved_datalen = sc_xfer->datalen;
				xfer->c_flags &= ~C_NEEDDONE;
				sc_xfer->datalen = xfer->c_bcount = 0;
				bcopy(
				    ((struct scsi_rw_tape *)sc_xfer->cmd)->len,
				    saved_len_bytes, 3);
				_lto3b(0,
				    ((struct scsi_rw_tape *)
				    sc_xfer->cmd)->len);
				WDCDEBUG_PRINT(
				    ("R/W in completion mode, do 0 blocks\n"),
				    DEBUG_DSC);
			} else
				WDCDEBUG_PRINT(("R/W %d blocks %d bytes\n",
				    _3btol(((struct scsi_rw_tape *)
				    sc_xfer->cmd)->len), sc_xfer->datalen),
				    DEBUG_DSC);

			/* DSC will change to buffer availability mode.
			   We reflect this in wdc_atapi_intr.  */
			break;

		case ERASE:		/* Media access commands */
		case LOAD:
		case REWIND:
		case SPACE:
#if 0
		case LOCATE:
		case READ_POSITION:
		case WRITE_FILEMARK:
#endif
			/* DSC will change to command completion mode.
			   We can reflect this early.  */
			drvp->drive_flags &= ~DRIVE_DSCBA;
			WDCDEBUG_PRINT(("clear DCSBA\n"), DEBUG_DSC);
			break;

		default:
			WDCDEBUG_PRINT(("no media access\n"), DEBUG_DSC);
		}
	}

	wdc_exec_xfer(chp, xfer);
#ifdef DIAGNOSTIC
	if (((sc_xfer->flags & SCSI_POLL) != 0 ||
	     (drvp->atapi_cap & ACAP_DSC) != 0) &&
	    (sc_xfer->flags & ITSDONE) == 0)
		panic("wdc_atapi_send_cmd: polled command not done");
#endif
	if ((drvp->atapi_cap & ACAP_DSC) && saved_datalen != 0) {
		sc_xfer->datalen = saved_datalen;
		bcopy(saved_len_bytes,
		    ((struct scsi_rw_tape *)sc_xfer->cmd)->len, 3);
		sc_xfer->flags &= ~ITSDONE;
		splx(s);
		goto restart;
	}
	ret = (sc_xfer->flags & ITSDONE) ? COMPLETE : SUCCESSFULLY_QUEUED;
	splx(s);
	return (ret);
}

void    
atapi_to_scsi_sense(xfer, flags)
	struct scsi_xfer *xfer;
	u_int8_t flags;
{
	struct scsi_sense_data *sense = &xfer->sense;
	
	sense->error_code = SSD_ERRCODE_VALID | 0x70;
	sense->flags = (flags >> 4);

	WDCDEBUG_PRINT(("Atapi error: %d ", (flags >> 4)), DEBUG_ERRORS);

	if ((flags & 0x4) && (sense->flags == 0)) {
		sense->flags = SKEY_ABORTED_COMMAND;
		WDCDEBUG_PRINT(("ABRT "), DEBUG_ERRORS);
	}

	if (flags & 0x1) {
		sense->flags |= SSD_ILI;
		WDCDEBUG_PRINT(("ILI "), DEBUG_ERRORS);
	}

	if (flags & 0x2) {
		sense->flags |= SSD_EOM;
		WDCDEBUG_PRINT(("EOM "), DEBUG_ERRORS);
	}

	/* Media change requested */
	/* Let's ignore these in version 1 */
	if (flags & 0x8) {
		WDCDEBUG_PRINT(("MCR "), DEBUG_ERRORS);
	}

	WDCDEBUG_PRINT(("\n"), DEBUG_ERRORS);
}


void
wdc_atapi_start(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;

	WDCDEBUG_PRINT(("wdc_atapi_start %s:%d:%d, scsi flags 0x%x\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive,
	    sc_xfer->flags), DEBUG_XFERS);
	/* Adjust C_DMA, it may have changed if we are requesting sense */
	if (!(xfer->c_flags & C_POLL) && 
	    (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA)) &&
	    (sc_xfer->datalen > 0 || (xfer->c_flags & C_SENSE)))
		xfer->c_flags |= C_DMA;
	else
		xfer->c_flags &= ~C_DMA;

	if (wdc_select_drive(chp, xfer->drive, ATAPI_DELAY) < 0) {
		printf("wdc_atapi_start: not ready, st = %02x\n",
		    chp->ch_status);
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return;
	}
    
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
	
	if (drvp->atapi_cap & ACAP_DSC) {
		if (atapi_dsc_wait(drvp, sc_xfer->timeout)) {
			sc_xfer->error = XS_TIMEOUT;
			wdc_atapi_reset(chp, xfer);
			return;
		}
		WDCDEBUG_PRINT(("wdc_atapi_start %s:%d:%d, DSC asserted\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive),
		    DEBUG_DSC);
		drvp->drive_flags &= ~DRIVE_DSCWAIT;
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

	as->protocol_phase = as_cmdout;

	/*
	 * If there is no interrupt for CMD input, busy-wait for it (done in 
	 * the interrupt routine. If it is a polled command, call the interrupt
	 * routine until command is done.
	 */
	if (((drvp->atapi_cap & ATAPI_CFG_DRQ_MASK) != ATAPI_CFG_IRQ_DRQ) ||
	    (sc_xfer->flags & SCSI_POLL) || (drvp->atapi_cap & ACAP_DSC)) {
		/* Wait for at last 400ns for status bit to be valid */
		DELAY(1);
		wdc_atapi_intr(chp, xfer, 0);
	} else {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		timeout(wdctimeout, chp, hz);
		return;
	}

	if ((sc_xfer->flags & SCSI_POLL) || (drvp->atapi_cap & ACAP_DSC)) {
		while ((sc_xfer->flags & ITSDONE) == 0) {
			if (drvp->atapi_cap & ACAP_DSC) {
				if (atapi_poll_wait(
					(drvp->drive_flags & DRIVE_DSCWAIT) ?
					    atapi_dsc_ready : atapi_dsc_semiready,
					    drvp, sc_xfer->timeout, PZERO + PCATCH,
					    "atapist")) {
					sc_xfer->error = XS_TIMEOUT;
					wdc_atapi_reset(chp, xfer);
					return;
				}
			} else
				/* Wait for at last 400ns for status bit to
				   be valid */
				DELAY(1);

			wdc_atapi_intr(chp, xfer, 0);
		}
	}
}

int
atapi_dsc_semiready(arg)
	void *arg;
{
	struct ata_drive_datas *drvp = arg;
	struct channel_softc *chp = drvp->chnl_softc;

	/* We should really wait_for_unbusy here too before 
	   switching drives. */
	bus_space_write_1(chp->cmd_iot, chp->cmd_ioh, wd_sdh,
	    WDSD_IBM | (drvp->drive << 4));

	return (wait_for_unbusy(chp, 0) == 0);
}


int wdc_atapi_intr_drq __P((struct channel_softc *, struct wdc_xfer *, int));

int
wdc_atapi_intr_drq(chp, xfer, irq)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int irq;

{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	int len, phase, i;
	int ire;
	int dma_flags = 0;
	u_int8_t cmd[16];
	struct scsi_sense *cmd_reqsense;
	int cmdlen = (drvp->atapi_cap & ACAP_LEN) ? 16 : 12;

	if (xfer->c_flags & C_DMA) {
		dma_flags = ((sc_xfer->flags & SCSI_DATA_IN) ||
		    (xfer->c_flags & C_SENSE)) ?  WDC_DMA_READ : 0;
		dma_flags |= ((sc_xfer->flags & SCSI_POLL) ||
		    (drvp->atapi_cap & ACAP_DSC)) ? WDC_DMA_POLL : 0;
	}


	if (as->protocol_phase == as_cmdout) {
		bzero(cmd, sizeof(cmd));

		if (xfer->c_flags & C_SENSE) {
			cmd_reqsense = (struct scsi_sense *)&cmd[0];
			cmd_reqsense->opcode = REQUEST_SENSE;
			cmd_reqsense->length = xfer->c_bcount;
		} else 
			bcopy(sc_xfer->cmd, cmd, sc_xfer->cmdlen);

		for (i = 0; i < 12; i++)
			WDCDEBUG_PRINT(("%02x ", cmd[i]), DEBUG_INTR);
		WDCDEBUG_PRINT((": PHASE_CMDOUT\n"), DEBUG_INTR);

		/* Init the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			if ((*chp->wdc->dma_init)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive, xfer->databuf, 
			    xfer->c_bcount, dma_flags) != 0) {
				sc_xfer->error = XS_DRIVER_STUFFUP;
				wdc_atapi_done(chp, xfer);
				return (1);
			}
		}


		wdc_output_bytes(drvp, cmd, cmdlen);

		as->protocol_phase = as_data;

		/* Start the DMA channel if necessary */
		if (xfer->c_flags & C_DMA) {
			(*chp->wdc->dma_start)(chp->wdc->dma_arg,
			    chp->channel, xfer->drive, 
			    dma_flags);
		}

		if ((sc_xfer->flags & SCSI_POLL) == 0 &&
		    (drvp->atapi_cap & ACAP_DSC) == 0) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
		}

		/* If we read/write to a tape we will get into buffer
		   availability mode.  */
		if (drvp->atapi_cap & ACAP_DSC) {
			if (!(drvp->drive_flags & DRIVE_DSCBA) &&
			    (sc_xfer->cmd->opcode == READ ||
				sc_xfer->cmd->opcode == WRITE)) {
				drvp->drive_flags |= DRIVE_DSCBA;
				WDCDEBUG_PRINT(("set DSCBA\n"), DEBUG_DSC);
			}
			if (sc_xfer->cmd->opcode == READ)
				drvp->drive_flags |= DRIVE_DSCWAIT;
		}
 		return (1);
	}

	if (as->protocol_phase != as_data) {
		panic ("wdc_atapi_intr_drq: bad protocol phase");
	}

	len = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_lo) +
		256 * bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_cyl_hi);
	ire = bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_ireason);
	phase = (ire & (WDCI_CMD | WDCI_IN)) | (chp->ch_status & WDCS_DRQ);
	WDCDEBUG_PRINT(("wdc_atapi_intr: c_bcount %d len %d st 0x%x err 0x%x "
	    "ire 0x%x :", xfer->c_bcount,
	    len, chp->ch_status, chp->ch_error, ire), DEBUG_INTR);

	/* Possibility to explore; what if we get an interrupt
	   during DMA ? */
	if ((xfer->c_flags & C_DMA) != 0) {
		printf("wdc_atapi_intr_drq: Unexpected "
		    "interrupt during DMA mode");

		goto abort_data;
	}


	if (ire & WDCI_CMD) {
		/* Something messed up */
		if (!(as->diagnostics_printed & ATAPI_DIAG_UNEXP_CMD)) {
			printf ("wdc_atapi_intr_drq: Unexpectedly "
			    "in the command phase. Please report this.\n");
			as->diagnostics_printed |= ATAPI_DIAG_UNEXP_CMD;
		}
		goto abort_data;
	}
	
	/* Make sure polarities match */
	if (((ire & WDCI_IN) == WDCI_IN) ==
	    ((sc_xfer->flags & SCSI_DATA_OUT) == SCSI_DATA_OUT)) {
		if (!(as->diagnostics_printed & ATAPI_DIAG_POLARITY)) {
			printf ("wdc_atapi_intr_drq: Polarity problem "
			    "in transfer. Please report this.\n");
			as->diagnostics_printed |= ATAPI_DIAG_POLARITY;
		}
		goto abort_data;
	}
	
	WDCDEBUG_PRINT(("PHASE_DATA\n"), DEBUG_INTR);
	
	if (len == 0) 
	        goto abort_data;
	  
	if (xfer->c_bcount >= len) {
		/* Common case */
		if (sc_xfer->flags & SCSI_DATA_OUT)
			wdc_output_bytes(drvp, (u_int8_t *)xfer->databuf +
			    xfer->c_skip, len);
		else
			wdc_input_bytes(drvp, (u_int8_t *)xfer->databuf +
			    xfer->c_skip, len);

		xfer->c_skip += len;
		xfer->c_bcount -= len;
	} else {
		/* Exceptional case */
		if (sc_xfer->flags & SCSI_DATA_OUT) {
			printf("wdc_atapi_intr: warning: write only "
			    "%d of %d requested bytes\n", xfer->c_bcount, len);

			wdc_output_bytes(drvp, (u_int8_t *)xfer->databuf +
			    xfer->c_skip, xfer->c_bcount);
			 
			for (i = xfer->c_bcount; i < len; i += 2)
				bus_space_write_2(chp->cmd_iot, chp->cmd_ioh,
				    wd_data, 0);
		} else {
			printf("wdc_atapi_intr: warning: reading only "
			    "%d of %d bytes\n", xfer->c_bcount, len);
			 
			wdc_input_bytes(drvp,
			    (char *)xfer->databuf + xfer->c_skip,
			    xfer->c_bcount);
			wdcbit_bucket(chp, len - xfer->c_bcount);
		}

		xfer->c_skip += xfer->c_bcount;
		xfer->c_bcount = 0;
	}

	if ((sc_xfer->flags & SCSI_POLL) == 0 &&
	    (drvp->atapi_cap & ACAP_DSC) == 0) {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
	} else if (drvp->atapi_cap & ACAP_DSC)
		drvp->drive_flags |= DRIVE_DSCWAIT;
	return (1);

 abort_data:
	if (xfer->c_flags & C_DMA) {
		(*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		    chp->channel, xfer->drive, dma_flags);
		drvp->n_dmaerrs++;
	}

	sc_xfer->error = XS_TIMEOUT;
	wdc_atapi_reset(chp, xfer);

	return (1);
}


int
wdc_atapi_intr(chp, xfer, irq)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int irq;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	int dma_err = 0;
	int dma_flags = 0;

	WDCDEBUG_PRINT(("wdc_atapi_intr %s:%d:%d\n",
			chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive), 
		       DEBUG_INTR);

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
			return (0); /* IRQ was not for us */
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip=%d\n",
		       chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		       xfer->c_bcount, xfer->c_skip);
		if (xfer->c_flags & C_DMA)
			drvp->n_dmaerrs++;
		sc_xfer->error = XS_TIMEOUT;
		wdc_atapi_reset(chp, xfer);
		return (1);
	}
	/* If we missed an IRQ and were using DMA, flag it as a DMA error */
	if ((xfer->c_flags & C_TIMEOU) && (xfer->c_flags & C_DMA))
		drvp->n_dmaerrs++;

	if (chp->ch_status & WDCS_DRQ) 
		return (wdc_atapi_intr_drq(chp, xfer, irq));
	
	/* DRQ was dropped. This means the command is over.
	   Do cleanup, check for errors, etc. */
	if (xfer->c_flags & C_DMA) {
		dma_flags = ((sc_xfer->flags & SCSI_DATA_IN) ||
			     (xfer->c_flags & C_SENSE)) ?  WDC_DMA_READ : 0;
		dma_flags |= ((sc_xfer->flags & SCSI_POLL) ||
			      (drvp->atapi_cap & ACAP_DSC)) ? WDC_DMA_POLL : 0;
	}

	WDCDEBUG_PRINT(("PHASE_COMPLETED\n"), DEBUG_INTR);

	/* turn off DMA channel */
	if (as->protocol_phase == as_data &&
	    xfer->c_flags & C_DMA) {
		dma_err = (*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		     chp->channel, xfer->drive, dma_flags);

		/* Assume everything was transferred */
		if (xfer->c_flags & C_SENSE)
			xfer->c_bcount -= sizeof(sc_xfer->sense);
		else
			xfer->c_bcount -= sc_xfer->datalen;
	}

	as->protocol_phase = as_none;

	if (xfer->c_flags & C_SENSE) {
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error & WDCE_ABRT) {
				WDCDEBUG_PRINT(("wdc_atapi_intr: request_sense aborted, "
						"calling wdc_atapi_done()"
					), DEBUG_INTR);
				wdc_atapi_done(chp, xfer);
				return (1);
			}

			/*
			 * request sense failed ! it's not suppossed
 			 * to be possible
			 */

			sc_xfer->error = XS_RESET;
			wdc_atapi_reset(chp, xfer);
			return (1);
		} else if (xfer->c_bcount < sizeof(sc_xfer->sense)) {
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
			atapi_to_scsi_sense(sc_xfer, chp->ch_error);
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
				return (1);
			}
		} 
	}

        if (dma_err < 0) { 
		drvp->n_dmaerrs++;
		sc_xfer->error = XS_RESET;
		wdc_atapi_reset(chp, xfer);
		return (1);
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
	int delay = (irq == 0) ? ATAPI_DELAY : 1;

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

	/* Don't timeout during configuration */
	xfer->c_flags &= ~C_TIMEOU;

	switch (drvp->state) {
		/* You need to send an ATAPI drive an ATAPI-specific
		   command to revive it after a hard reset. Identify
		   is about the most innocuous thing you can do
		   that's guaranteed to be there */
	case IDENTIFY:
#if 0
		wdccommandshort(chp, drvp->drive, ATAPI_IDENTIFY_DEVICE);
		drvp->state = IDENTIFY_WAIT;
		break;
	
	case IDENTIFY_WAIT:
		errstring = "IDENTIFY";
		
		/* Some ATAPI devices need to try to read the media
		   before responding. Well, let's hope resets while
		   polling are few and far between */
		if (wdcwait(chp, 0, 0, delay))
			goto timeout;

		/* We don't really care if this operation failed.
		   It's just there to wake the drive from its stupor. */
		if (!(chp->ch_status & WDCS_ERR)) {
			wdcbit_bucket(chp, 512);
	
			errstring = "Post IDENTIFY";

			delay = ATAPI_DELAY;
		}

		drvp->state = PIOMODE;
		goto again;
#else
		drvp->state = PIOMODE;
#endif
	case PIOMODE:
piomode:
		/* Don't try to set mode if controller can't be adjusted */
		if ((chp->wdc->cap & WDC_CAPABILITY_MODE) == 0)
			goto ready;
		/* Also don't try if the drive didn't report its mode */
		if ((drvp->drive_flags & DRIVE_MODE) == 0)
			goto ready;
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
		return (1);
	}
	if ((sc_xfer->flags & SCSI_POLL) == 0 &&
	    (drvp->atapi_cap & ACAP_DSC) == 0) {
		chp->ch_flags |= WDCF_IRQ_WAIT;
		xfer->c_intr = wdc_atapi_ctrl;
		timeout(wdctimeout, chp, sc_xfer->timeout * hz / 1000);
	} else {
		goto again;
	}
	return (1);

timeout:
	if (irq && (xfer->c_flags & C_TIMEOU) == 0) {
		return (0); /* IRQ was not for us */
	}
	printf("%s:%d:%d: %s timed out\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive, errstring);
	sc_xfer->error = XS_TIMEOUT;
	wdc_atapi_reset(chp, xfer);
	return (1);
error:
	printf("%s:%d:%d: %s ",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    errstring);
	printf("error (0x%x)\n", chp->ch_error);
	sc_xfer->error = XS_SHORTSENSE;
	atapi_to_scsi_sense(sc_xfer, chp->ch_error);
	wdc_atapi_reset(chp, xfer);
	return (1);
}

void
wdc_atapi_done(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	int need_done = xfer->c_flags & C_NEEDDONE;
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
		printf("wdc_atapi_done: sc_xfer->error %d\n", sc_xfer->error);
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

/* Wait until DSC gets asserted.  */
int
atapi_dsc_wait(drvp, timo)
	struct ata_drive_datas *drvp;
	int timo;
{
	struct channel_softc *chp = drvp->chnl_softc;

	chp->ch_flags &= ~WDCF_ACTIVE;
#if 0
	/* XXX Something like this may be needed I have not investigated
	   close enough yet.  If so we may need to put it back after
	   the poll wait.  */
	TAILQ_REMOVE(&chp->ch_queue->sc_xfer, xfer, c_xferchain);
#endif
	return (atapi_poll_wait(atapi_dsc_ready, drvp, timo, PZERO + PCATCH,
	    "atapidsc"));
}

int
atapi_dsc_ready(arg)
	void *arg;
{
	struct ata_drive_datas *drvp = arg;
	struct channel_softc *chp = drvp->chnl_softc;

	if (chp->ch_flags & WDCF_ACTIVE)
		return (0);
	wdc_select_drive(chp, drvp->drive, 0);
	chp->ch_status =
	    bus_space_read_1(chp->cmd_iot, chp->cmd_ioh, wd_status);
	return ((chp->ch_status & (WDCS_BSY | WDCS_DSC)) == WDCS_DSC);
}

int
atapi_poll_wait(ready, arg, timo, pri, msg)
	int (*ready) __P((void *));
	void *arg;
	int timo;
	int pri;
	char *msg;
{
	int maxtic, tic = 0, error;
	u_int64_t starttime = time.tv_sec * 1000 + time.tv_usec / 1000;
	u_int64_t endtime = starttime + timo;

	while (1) {
		WDCDEBUG_PRINT(("atapi_poll_wait: msg=%s tic=%d\n", msg, tic),
		    DEBUG_POLL);
		if (ready(arg))
			return (0);

#if 0
		/* Exponential backoff.  */
		tic = tic + tic + 1;
#else
		tic = min(hz / 100, 1);
#endif
		maxtic = (int)
		    (endtime - (time.tv_sec * 1000 + time.tv_usec / 1000));
		if (maxtic <= 0)
			return (EWOULDBLOCK);
		if (tic > maxtic)
			tic = maxtic;
		if (tic > ATAPI_POLL_MAXTIC)
			tic = ATAPI_POLL_MAXTIC;
		error = tsleep(arg, pri, msg, tic);
		if (error != EWOULDBLOCK)
			return (error);
	}
}

void
wdc_atapi_reset(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct scsi_xfer *sc_xfer = xfer->cmd;

	wdccommandshort(chp, xfer->drive, ATAPI_SOFT_RESET);

	/* Some ATAPI devices need extra time to find their
	   brains after a reset
	 */
	delay(5000);

	drvp->state = 0;
	if (wdcwait(chp, WDCS_DRQ, 0, ATAPI_RESET_WAIT) != 0) {
		printf("%s:%d:%d: reset failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel,
		    xfer->drive);
		sc_xfer->error = XS_SELTIMEOUT;
		wdc_reset_channel(drvp);
	}

	wdc_atapi_done(chp, xfer);
	return;
}
