/*      $OpenBSD: atapiscsi.c,v 1.24 2000/01/12 17:14:02 csapuntz Exp $     */

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
#define ATAPI_CTRL_WAIT 4000

/* When polling, let the exponential backoff max out at 1 second's interval. */
#define ATAPI_POLL_MAXTIC (hz)

void  wdc_atapi_minphys __P((struct buf *bp));
void  wdc_atapi_start __P((struct channel_softc *,struct wdc_xfer *));

void  wdc_atapi_timer_handler __P((void *));

int   wdc_atapi_real_start __P((struct channel_softc *, struct wdc_xfer *,
    int));
int   wdc_atapi_real_start_2 __P((struct channel_softc *, struct wdc_xfer *,
    int));
int   wdc_atapi_intr_command __P((struct channel_softc *, struct wdc_xfer *,
    int));

int   wdc_atapi_intr_data __P((struct channel_softc *, struct wdc_xfer *,
    int));
int   wdc_atapi_intr_complete __P((struct channel_softc *, struct wdc_xfer *,
    int));

int   wdc_atapi_intr_for_us __P((struct channel_softc *, struct wdc_xfer *,
    int));

int   wdc_atapi_send_packet __P((struct channel_softc *, struct wdc_xfer *,
    int));
int   wdc_atapi_dma_flags __P((struct wdc_xfer *));

int   wdc_atapi_ctrl __P((struct channel_softc *, struct wdc_xfer *, int));
char  *wdc_atapi_in_data_phase __P((struct wdc_xfer *, int, int));

int   wdc_atapi_intr __P((struct channel_softc *, struct wdc_xfer *, int));

int   wdc_atapi_done __P((struct channel_softc *, struct wdc_xfer *, int));
int   wdc_atapi_reset __P((struct channel_softc *, struct wdc_xfer *, int));
int   wdc_atapi_reset_2 __P((struct channel_softc *, struct wdc_xfer *, int));
int   wdc_atapi_send_cmd __P((struct scsi_xfer *sc_xfer));

int   wdc_atapi_tape_done __P((struct channel_softc *, struct wdc_xfer *, int));
#define MAX_SIZE MAXPHYS

struct atapiscsi_softc;
struct atapiscsi_xfer;

int	atapiscsi_match __P((struct device *, void *, void *));
void	atapiscsi_attach __P((struct device *, struct device *, void *));

int	wdc_atapi_get_params __P((struct channel_softc *, u_int8_t, struct ataparams *)); 

int	atapi_dsc_wait __P((struct ata_drive_datas *, int));
int	atapi_dsc_ready __P((void *));
void    atapi_dsc_check __P((void *));
int	atapi_dsc_semiready __P((void *));
int	atapi_poll_wait __P((int (*) __P((void *)), void *, int, int, char *));
int     atapi_to_scsi_sense __P((struct scsi_xfer *, u_int8_t));

struct atapiscsi_softc {
	struct device  sc_dev;
	struct  scsi_link  sc_adapterlink;
	struct channel_softc *chp;
	enum atapi_state { as_none, as_cmdout, as_data, as_completed };
	enum atapi_state protocol_phase;

	int retries;
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

int
wdc_atapi_get_params(chp, drive, id)
	struct channel_softc *chp;
	u_int8_t drive;
	struct ataparams *id;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[drive];
	struct wdc_command wdc_c;
	int retries = 3;

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

	CHP_READ_REG(chp, wdr_status);
	
	/* Some ATAPI devices need a bit more time after software reset. */
	delay(5000);

 retry:
	if (ata_get_params(drvp, AT_POLL, id) != 0) {
		WDCDEBUG_PRINT(("wdc_atapi_get_params: ATAPI_IDENTIFY_DEVICE "
		    "failed for drive %s:%d:%d\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, drive), 
		    DEBUG_PROBE);

		if (retries--) {
			delay(100000);
			goto retry;
		}

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
	struct wdc_xfer *xfer;
	int flags = sc_xfer->flags;
	int s, ret;

	WDCDEBUG_PRINT(("wdc_atapi_send_cmd %s:%d:%d\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drive), DEBUG_XFERS);

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
		switch (sc_xfer->cmd->opcode) {
		case READ:
		case WRITE:
			xfer->c_flags |= C_MEDIA_ACCESS;

			/* If we are not in buffer availability mode,
			   we limit the first request to 0 bytes, which
			   gets us into buffer availability mode without
			   holding the bus.  */
			if (!(drvp->drive_flags & DRIVE_DSCBA)) {
				xfer->c_bcount = 0;
				xfer->transfer_len = 
				  _3btol(((struct scsi_rw_tape *)
					  sc_xfer->cmd)->len);
				_lto3b(0,
				    ((struct scsi_rw_tape *)
				    sc_xfer->cmd)->len);
				xfer->c_done = wdc_atapi_tape_done;
				WDCDEBUG_PRINT(
				    ("R/W in completion mode, do 0 blocks\n"),
				    DEBUG_DSC);
			} else
				WDCDEBUG_PRINT(("R/W %d blocks %d bytes\n",
				    _3btol(((struct scsi_rw_tape *)	
					sc_xfer->cmd)->len), 
				    sc_xfer->datalen),
				    DEBUG_DSC);

			/* DSC will change to buffer availability mode.
			   We reflect this in wdc_atapi_intr.  */
			break;

		case ERASE:		/* Media access commands */
		case LOAD:
		case REWIND:
		case SPACE:
		case WRITE_FILEMARKS:
#if 0
		case LOCATE:
		case READ_POSITION:
#endif

			xfer->c_flags |= C_MEDIA_ACCESS;
			break;

		default:
			WDCDEBUG_PRINT(("no media access\n"), DEBUG_DSC);
		}
	}

	wdc_exec_xfer(chp, xfer);
#ifdef DIAGNOSTIC
	if ((xfer->c_flags & C_POLL) != 0 &&
	    (sc_xfer->flags & ITSDONE) == 0)
		panic("wdc_atapi_send_cmd: polled command not done");
#endif
	ret = (sc_xfer->flags & ITSDONE) ? COMPLETE : SUCCESSFULLY_QUEUED;
	splx(s);
	return (ret);
}


/*
 * Returns 1 if we experienced an ATA-level abort command
 *           (ABRT bit set but no additional sense)
 *         0 if normal command processing
 */
int
atapi_to_scsi_sense(xfer, flags)
	struct scsi_xfer *xfer;
	u_int8_t flags;
{
	struct scsi_sense_data *sense = &xfer->sense;
	int ret = 0;

	xfer->error = XS_SHORTSENSE;

	sense->error_code = SSD_ERRCODE_VALID | 0x70;
	sense->flags = (flags >> 4);

	WDCDEBUG_PRINT(("Atapi error: %d ", (flags >> 4)), DEBUG_ERRORS);

	if ((flags & 4) && (sense->flags == 0)) {
		sense->flags = SKEY_ABORTED_COMMAND;
		WDCDEBUG_PRINT(("ABRT "), DEBUG_ERRORS);
		ret = 1;
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
		if (sense->flags == 0)
			xfer->error = XS_NOERROR;
	}

	WDCDEBUG_PRINT(("\n"), DEBUG_ERRORS);
	return (ret);
}

int wdc_atapi_drive_selected __P((struct channel_softc *, int));

int
wdc_atapi_drive_selected(chp, drive)
	struct channel_softc *chp;
	int drive;
{
	u_int8_t reg = CHP_READ_REG(chp, wdr_sdh);

	return ((reg & 0x10) == (drive << 4));
}

enum atapi_context {
	ctxt_process = 0,
	ctxt_timer = 1,
	ctxt_interrupt = 2
};

int wdc_atapi_the_machine __P((struct channel_softc *, struct wdc_xfer *,
    enum atapi_context));

int wdc_atapi_the_poll_machine __P((struct channel_softc *, struct wdc_xfer *));

void
wdc_atapi_start(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
{
	xfer->next = wdc_atapi_real_start;

	wdc_atapi_the_machine(chp, xfer, ctxt_process);
}


void
wdc_atapi_timer_handler(arg)
	void *arg;
{
	struct wdc_xfer *xfer = arg;
	struct channel_softc *chp = xfer->chp;
	int s;

	xfer->c_flags &= ~C_POLL_MACHINE;

	/* There is a race here between us and the interrupt */
	s = splbio();
	wdc_atapi_the_machine(chp, xfer, ctxt_timer);
	splx(s);
}


int
wdc_atapi_intr(chp, xfer, irq)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int irq;
{
	/* XXX we should consider an alternate signaling regime here */
	if (xfer->c_flags & C_TIMEOU) {
		xfer->c_flags &= ~C_TIMEOU;
		return (wdc_atapi_the_machine(chp, xfer, ctxt_timer));
	}

	return (wdc_atapi_the_machine(chp, xfer, ctxt_interrupt));
}

#define CONTINUE_POLL 0
#define GOTO_NEXT 1
#define DONE 2

int
wdc_atapi_the_poll_machine(chp, xfer)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;	
{
	int  idx = 0, ret;
	int  current_timeout = 10;

	xfer->timeout = -1;

	while (1) {
		idx++;

		xfer->timeout = -1;
		xfer->delay = 0;
		xfer->expect_irq = 0;

		ret = (xfer->next)(chp, xfer, (current_timeout * 1000 <= idx));

		if (xfer->timeout != -1) {
			current_timeout = xfer->timeout;
			idx = 0;
		}

		if (xfer->delay != 0) {
			delay (1000 * xfer->delay);
			idx += 1000 * xfer->delay;
		}

		switch (ret) {
		case GOTO_NEXT:
			break;
			
	        case CONTINUE_POLL:
			DELAY(1);
			break;

		case DONE:
			wdc_free_xfer(chp, xfer);
			wdcstart(chp);
			return (0);
		}
	}
}

int
wdc_atapi_the_machine(chp, xfer, ctxt)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;	
	enum atapi_context ctxt;
{
	int idx = 0, ret;
	int claim_irq = 0;
	extern int ticks;
	int timeout_delay = hz / 10;

	if (xfer->c_flags & C_POLL) {
		if (ctxt != ctxt_process) 
			return (0);

		wdc_atapi_the_poll_machine(chp, xfer);
		return (0);
	}

 do_op:
	idx++;

	xfer->timeout = -1;
	xfer->claim_irq = 0;
	xfer->delay = 0;

	ret = (xfer->next)(chp, xfer, 
			   xfer->endticks && (ticks - xfer->endticks >= 0));

	if (xfer->timeout != -1) 
		xfer->endticks = max((xfer->timeout * 1000) / hz, 1) + ticks;

	if (xfer->claim_irq) claim_irq = xfer->claim_irq;

	if (xfer->delay) timeout_delay = max(xfer->delay * hz / 1000, 1);

	switch (ret) {
	case GOTO_NEXT:
		if (xfer->expect_irq) {
			chp->ch_flags |= WDCF_IRQ_WAIT;
			xfer->expect_irq = 0;
			timeout(wdctimeout, chp, xfer->endticks - ticks);

			return (claim_irq);
		}

		if (xfer->delay)
			break;

		goto do_op;

	case CONTINUE_POLL:
		if (xfer->delay) break;
		if (idx >= 50) break;

		DELAY(1);
		goto do_op;

	case DONE:
		if (xfer->c_flags & C_POLL_MACHINE)
			untimeout (wdc_atapi_timer_handler, xfer);

		wdc_free_xfer(chp, xfer);
		wdcstart(chp);

		return (claim_irq);
	}

	timeout(wdc_atapi_timer_handler, xfer, timeout_delay);
	xfer->c_flags |= C_POLL_MACHINE;
	return (claim_irq);
}


void wdc_atapi_update_status __P((struct channel_softc *));

void
wdc_atapi_update_status(chp)
	struct channel_softc *chp;
{
	chp->ch_status = CHP_READ_REG(chp, wdr_status);

	if (chp->ch_status == 0xff && (chp->ch_flags & WDCF_ONESLAVE)) {
		CHP_WRITE_REG(chp, wdr_sdh, WDSD_IBM | 0x10);

		chp->ch_status = CHP_READ_REG(chp, wdr_status);
	}

	if ((chp->ch_status & (WDCS_BSY | WDCS_ERR)) == WDCS_ERR)
		chp->ch_error = CHP_READ_REG(chp, wdr_error);
}

int
wdc_atapi_real_start(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
#ifdef WDCDEBUG
	struct scsi_xfer *sc_xfer = xfer->cmd;
#endif
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	WDCDEBUG_PRINT(("wdc_atapi_start %s:%d:%d, scsi flags 0x%x\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive,
	    sc_xfer->flags), DEBUG_XFERS);

	/* Adjust C_DMA, it may have changed if we are requesting sense */
	if (!(xfer->c_flags & C_POLL) && 
	    (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA)) &&
	    (xfer->c_bcount > 0 || (xfer->c_flags & C_SENSE)))
		xfer->c_flags |= C_DMA;
	else
		xfer->c_flags &= ~C_DMA;


	CHP_WRITE_REG(chp, wdr_sdh, WDSD_IBM | (xfer->drive << 4));

	DELAY(1);

	xfer->next = wdc_atapi_real_start_2;
	xfer->timeout = ATAPI_DELAY;

	return (GOTO_NEXT);
}


int
wdc_atapi_real_start_2(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	if (timeout) {
		printf("wdc_atapi_start: not ready, st = %02x\n",
		    chp->ch_status);

		sc_xfer->error = XS_TIMEOUT;
		xfer->next = wdc_atapi_reset;
		return (GOTO_NEXT);
	} else {
		wdc_atapi_update_status(chp);
		
		if (chp->ch_status & WDCS_BSY)
			return (CONTINUE_POLL);
	}
    
	/* Do control operations specially. */
	if (drvp->state < READY) {
		if (drvp->state != IDENTIFY) {
			printf("%s:%d:%d: bad state %d in wdc_atapi_start\n",
			    chp->wdc->sc_dev.dv_xname, chp->channel,
			    xfer->drive, drvp->state);
			panic("wdc_atapi_start: bad state");
		}

		xfer->next = wdc_atapi_ctrl;
		return (GOTO_NEXT);
	}

	xfer->next = wdc_atapi_send_packet;
	return (GOTO_NEXT);
}


int
wdc_atapi_send_packet(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;

	/*
	 * Even with WDCS_ERR, the device should accept a command packet
	 * Limit length to what can be stuffed into the cylinder register
	 * (16 bits).  Some CD-ROMs seem to interpret '0' as 65536,
	 * but not all devices do that and it's not obvious from the
	 * ATAPI spec that that behaviour should be expected.  If more
	 * data is necessary, multiple data transfer phases will be done.
	 */

	wdccommand(chp, xfer->drive, ATAPI_PKT_CMD, 
	    xfer->c_bcount <= 0xfffe ? xfer->c_bcount : 0xfffe,
	    0, 0, 0, 
	    (xfer->c_flags & C_DMA) ? ATAPI_PKT_CMD_FTRE_DMA : 0);

	as->protocol_phase = as_cmdout;
	as->retries = 0;

	DELAY(1);

	xfer->next = wdc_atapi_intr_for_us;
	xfer->timeout = sc_xfer->timeout;

	if ((drvp->atapi_cap & ATAPI_CFG_DRQ_MASK) == ATAPI_CFG_IRQ_DRQ) {
		/* We expect an IRQ to tell us of the next state */
		xfer->expect_irq = 1;
	}
	return (GOTO_NEXT);
}

int
wdc_atapi_dma_flags(xfer)
	struct wdc_xfer *xfer;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	int dma_flags;

	dma_flags = ((sc_xfer->flags & SCSI_DATA_IN) ||
	    (xfer->c_flags & C_SENSE)) ?  WDC_DMA_READ : 0;
	dma_flags |= (xfer->c_flags & C_POLL) ? WDC_DMA_POLL : 0;

	return (dma_flags);
}

int
wdc_atapi_intr_command(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	int i;
	u_int8_t cmd[16];
	struct scsi_sense *cmd_reqsense;
	int cmdlen = (drvp->atapi_cap & ACAP_LEN) ? 16 : 12;
	int  dma_flags;

	dma_flags = wdc_atapi_dma_flags(xfer);

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

			xfer->next = wdc_atapi_done;
			return (GOTO_NEXT);
		}
	}

	wdc_output_bytes(drvp, cmd, cmdlen);

	if (xfer->c_bcount == 0)
		as->protocol_phase = as_completed;
	else
		as->protocol_phase = as_data;

	/* Start the DMA channel if necessary */
	if (xfer->c_flags & C_DMA) {
		(*chp->wdc->dma_start)(chp->wdc->dma_arg,
		    chp->channel, xfer->drive, 
		    dma_flags);
	}

	xfer->expect_irq = 1;

	/* If we read/write to a tape we will get into buffer
	   availability mode.  */
	if (drvp->atapi_cap & ACAP_DSC) {
		if ((sc_xfer->cmd->opcode == READ ||
		       sc_xfer->cmd->opcode == WRITE)) {
			drvp->drive_flags |= DRIVE_DSCBA;
			WDCDEBUG_PRINT(("set DSCBA\n"), DEBUG_DSC);
		} else if ((xfer->c_flags & C_MEDIA_ACCESS) &&
		    (drvp->drive_flags & DRIVE_DSCBA)) {
			/* Clause 3.2.4 of QIC-157 D.

			   Any media access command other than read or
			   write will switch DSC back to completion
			   mode */
			drvp->drive_flags &= ~DRIVE_DSCBA;
			WDCDEBUG_PRINT(("clear DCSBA\n"), DEBUG_DSC);
		}
	}

	return (GOTO_NEXT);
}


char *
wdc_atapi_in_data_phase(xfer, len, ire)
	struct wdc_xfer *xfer;
	int len, ire;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	char *message;

	if (as->protocol_phase != as_data) {
		message = "unexpected data phase";
		goto unexpected_state;
	}

	if (ire & WDCI_CMD) {
		message = "unexpectedly in command phase";
		goto unexpected_state;
	}

	if (!(xfer->c_flags & C_SENSE)) {
		if (!(sc_xfer->flags & (SCSI_DATA_IN | SCSI_DATA_OUT))) {
			message = "data phase where none expected";
			goto unexpected_state;
		}
		
		/* Make sure polarities match */
		if (((ire & WDCI_IN) == WDCI_IN) ==
		    ((sc_xfer->flags & SCSI_DATA_OUT) == SCSI_DATA_OUT)) {
			message = "data transfer direction disagreement";
			goto unexpected_state;
		}
	} else {
		if (!(ire & WDCI_IN)) {
			message = "data transfer direction disagreement during sense";
			goto unexpected_state;
		}
	}
	
	if (len == 0) {
		message = "zero length transfer requested in data phase";
		goto unexpected_state;
	}


	return (0);

 unexpected_state:

	return (message);
}

int
wdc_atapi_intr_data(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	int len, ire;
	char *message;

	len = (CHP_READ_REG(chp, wdr_cyl_hi) << 8) |
	    CHP_READ_REG(chp, wdr_cyl_lo);
	ire = CHP_READ_REG(chp, wdr_ireason);

	if ((message = wdc_atapi_in_data_phase(xfer, len, ire))) {
		/* The drive has dropped BSY before setting up the
		   registers correctly for DATA phase. This drive is
		   not compliant with ATA/ATAPI-4.

		   Give the drive 100ms to get its house in order
		   before we try again.  */
		if (!timeout) {
			xfer->delay = 100;
			return (CONTINUE_POLL);	
		}
	}

	if (timeout) {
		printf ("wdc_atapi_intr_data: error: %s\n", message);
		
		sc_xfer->error = XS_RESET;
		xfer->next = wdc_atapi_reset;
		return (GOTO_NEXT);
	}

	
	if (xfer->c_bcount >= len) {
		WDCDEBUG_PRINT(("wdc_atapi_intr: c_bcount %d len %d "
		    "st 0x%x err 0x%x "
		    "ire 0x%x\n", xfer->c_bcount,
		    len, chp->ch_status, chp->ch_error, ire), DEBUG_INTR);
		
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
			 
			CHP_WRITE_RAW_MULTI_2(chp, NULL, 
			    len - xfer->c_bcount);
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

	xfer->expect_irq = 1;
	xfer->next = wdc_atapi_intr_for_us;

	return (GOTO_NEXT);
}


int
wdc_atapi_intr_complete(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;

{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;
	int dma_err = 0;
	int dma_flags = wdc_atapi_dma_flags(xfer);

	WDCDEBUG_PRINT(("PHASE_COMPLETED\n"), DEBUG_INTR);

	/* turn off DMA channel */
	if (as->protocol_phase == as_data &&
	    xfer->c_flags & C_DMA) {
		dma_err = (*chp->wdc->dma_finish)(chp->wdc->dma_arg,
		     chp->channel, xfer->drive, dma_flags);

		/* Assume everything was transferred */
		xfer->c_bcount = 0;
	}

	as->protocol_phase = as_none;

	if (xfer->c_flags & C_SENSE) {
		if (chp->ch_status & WDCS_ERR) {
			if (chp->ch_error & WDCE_ABRT) {
				WDCDEBUG_PRINT(("wdc_atapi_intr: request_sense aborted, "
						"calling wdc_atapi_done()"
					), DEBUG_INTR);
				xfer->next = wdc_atapi_done;
				return (GOTO_NEXT);
			}

			/*
			 * request sense failed ! it's not suppossed
 			 * to be possible
			 */

			sc_xfer->error = XS_RESET;
			xfer->next = wdc_atapi_reset;
			return (GOTO_NEXT);

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
			if (!atapi_to_scsi_sense(sc_xfer, chp->ch_error) &&
			    (sc_xfer->sc_link->quirks &
			     ADEV_NOSENSE) == 0) {
				/*
				 * let the driver issue a
				 * 'request sense'
				 */
				xfer->databuf = &sc_xfer->sense;
				xfer->c_bcount = sizeof(sc_xfer->sense);
				xfer->c_skip = 0;
				xfer->c_done = NULL;
				xfer->c_flags |= C_SENSE;
				xfer->next = wdc_atapi_real_start;
				return (GOTO_NEXT);
			}
		}		
	}

        if (dma_err < 0) { 
		drvp->n_dmaerrs++;
		sc_xfer->error = XS_RESET;

		xfer->next = wdc_atapi_reset;
		return (GOTO_NEXT);
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


	if (xfer->c_done) 
		xfer->next = xfer->c_done;
	else 
		xfer->next = wdc_atapi_done;

	return (GOTO_NEXT);
}

int
wdc_atapi_intr_for_us(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;

{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct atapiscsi_softc *as = sc_xfer->sc_link->adapter_softc;

	WDCDEBUG_PRINT(("ATAPI_INTR\n"), DEBUG_INTR);

	wdc_atapi_update_status(chp);

	if (timeout) {
		printf("%s:%d:%d: device timeout, c_bcount=%d, c_skip=%d\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
		    xfer->c_bcount, xfer->c_skip);

		if (xfer->c_flags & C_DMA)
			drvp->n_dmaerrs++;
		
		sc_xfer->error = XS_TIMEOUT;
		xfer->next = wdc_atapi_reset;
		return (GOTO_NEXT);
	}


	if (chp->ch_status & WDCS_BSY)
		return (CONTINUE_POLL);

	if (!wdc_atapi_drive_selected(chp, xfer->drive))
	{
		CHP_WRITE_REG(chp, wdr_sdh, WDSD_IBM | (xfer->drive << 4));
		delay (1);

		return (CONTINUE_POLL);
	}

	if (as->protocol_phase != as_cmdout &&
	    (xfer->c_flags & C_MEDIA_ACCESS) &&
	    !(chp->ch_status & WDCS_DSC)) {
		xfer->delay = 100;
		return (CONTINUE_POLL);
	}

	xfer->claim_irq = 1;

	if (chp->ch_status & WDCS_DRQ) {
		if (as->protocol_phase == as_cmdout)
			return (wdc_atapi_intr_command(chp, xfer, timeout));

		return (wdc_atapi_intr_data(chp, xfer, timeout));
	}
	
	return (wdc_atapi_intr_complete(chp, xfer, timeout));
}

int
wdc_atapi_ctrl(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	char *errstring = NULL;

	switch (drvp->state) {
	case IDENTIFY:
	case IDENTIFY_WAIT:
		errstring = "IDENTIFY";
		break;
		
	case PIOMODE:
		errstring = "Post IDENTIFY";
		break;

	case PIOMODE_WAIT:
		errstring = "PIOMODE";
		break;
	case DMAMODE_WAIT:
		errstring = "dmamode";
		break;
	default:
		errstring = "unknown state";
		break;
	}

	if (timeout) {
		if (drvp->state != IDENTIFY)
			goto timeout;
		else {
#ifdef DIAGNOSTIC
			printf ("wdc_atapi_ctrl: timeout before IDENTIFY."
			    "Should not happen\n");
#endif
			sc_xfer->error = XS_DRIVER_STUFFUP;
			xfer->next = wdc_atapi_done;
			return (GOTO_NEXT);
		}
	}

	wdc_atapi_update_status(chp);

	if (chp->ch_status & WDCS_BSY)
		return (CONTINUE_POLL);

	if (!wdc_atapi_drive_selected(chp, xfer->drive))
	{
		CHP_WRITE_REG(chp, wdr_sdh, WDSD_IBM | (xfer->drive << 4));
		delay (1);

		return (CONTINUE_POLL);
	}


	xfer->claim_irq = 1;

	WDCDEBUG_PRINT(("wdc_atapi_ctrl %s:%d:%d state %d\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, drvp->drive, drvp->state),
	    DEBUG_INTR | DEBUG_FUNCS);

	switch (drvp->state) {
		/* You need to send an ATAPI drive an ATAPI-specific
		   command to revive it after a hard reset. Identify
		   is about the most innocuous thing you can do
		   that's guaranteed to be there */
	case IDENTIFY:
		wdccommandshort(chp, drvp->drive, ATAPI_IDENTIFY_DEVICE);
		drvp->state = IDENTIFY_WAIT;
		xfer->timeout = ATAPI_CTRL_WAIT;
		xfer->expect_irq = 1;
		break;
	
	case IDENTIFY_WAIT:
		/* We don't really care if this operation failed.
		   It's just there to wake the drive from its stupor. */
		if (!(chp->ch_status & WDCS_ERR)) {
			wdcbit_bucket(chp, 512);
	
			xfer->timeout = 100;
			drvp->state = PIOMODE;
			break;
		}

		drvp->state = PIOMODE;

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
		xfer->timeout = ATAPI_CTRL_WAIT;
		xfer->expect_irq = 1;
		break;
	case PIOMODE_WAIT:
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

		xfer->timeout = ATAPI_CTRL_WAIT;
		xfer->expect_irq = 1;
		break;

	case DMAMODE_WAIT:
		if (chp->ch_status & WDCS_ERR)
			drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
	/* fall through */

	case READY:
	ready:
		drvp->state = READY;
		xfer->next = wdc_atapi_real_start;
		break;
	}
	return (GOTO_NEXT);

timeout:
	printf("%s:%d:%d: %s timed out\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive, errstring);
	sc_xfer->error = XS_TIMEOUT;
	xfer->next = wdc_atapi_reset;
	return (GOTO_NEXT);

error:
	printf("%s:%d:%d: %s ",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    errstring);
	printf("error (0x%x)\n", chp->ch_error);

	sc_xfer->error = XS_DRIVER_STUFFUP;

	xfer->next = wdc_atapi_reset;
	return (GOTO_NEXT);
}

int
wdc_atapi_tape_done(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;

	if (sc_xfer->error != XS_NOERROR) {
		xfer->next = wdc_atapi_done;
		return (GOTO_NEXT);
	}

	_lto3b(xfer->transfer_len,
	    ((struct scsi_rw_tape *)
		sc_xfer->cmd)->len);

	xfer->c_bcount = sc_xfer->datalen;
	xfer->c_done = NULL;
	xfer->c_skip = 0;

	xfer->next = wdc_atapi_real_start;
	return (GOTO_NEXT);
}

	
int
wdc_atapi_done(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct scsi_xfer *sc_xfer = xfer->cmd;
	int need_done = xfer->c_flags & C_NEEDDONE;
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	int doing_dma = xfer->c_flags & C_DMA;

	WDCDEBUG_PRINT(("wdc_atapi_done %s:%d:%d: flags 0x%x\n",
	    chp->wdc->sc_dev.dv_xname, chp->channel, xfer->drive,
	    (u_int)xfer->c_flags), DEBUG_XFERS);

	sc_xfer->flags |= ITSDONE;
	if (drvp->n_dmaerrs ||
	    (sc_xfer->error != XS_NOERROR && sc_xfer->error != XS_SENSE &&
	    sc_xfer->error != XS_SHORTSENSE)) {
#if 0
		printf("wdc_atapi_done: sc_xfer->error %d\n", sc_xfer->error);
#endif
		drvp->n_dmaerrs = 0;
		if (doing_dma)
			wdc_downgrade_mode(drvp);
	}

	if (need_done) {
		WDCDEBUG_PRINT(("wdc_atapi_done: scsi_done\n"), DEBUG_XFERS);
		scsi_done(sc_xfer);
	}

	return (DONE);
}


int
wdc_atapi_reset(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];

	wdccommandshort(chp, xfer->drive, ATAPI_SOFT_RESET);
	drvp->state = 0;

	/* Some ATAPI devices need extra time to find their
	   brains after a reset
	 */
	xfer->next = wdc_atapi_reset_2;
	xfer->delay = 10;
	xfer->timeout = ATAPI_RESET_WAIT;
	return (GOTO_NEXT);
}

int
wdc_atapi_reset_2(chp, xfer, timeout)
	struct channel_softc *chp;
	struct wdc_xfer *xfer;
	int timeout;
{
	struct ata_drive_datas *drvp = &chp->ch_drive[xfer->drive];
	struct scsi_xfer *sc_xfer = xfer->cmd;
	
	if (timeout) {
		printf("%s:%d:%d: reset failed\n",
		    chp->wdc->sc_dev.dv_xname, chp->channel,
		    xfer->drive);
		sc_xfer->error = XS_SELTIMEOUT;
		wdc_reset_channel(drvp);
		
		xfer->next = wdc_atapi_done;
		return (GOTO_NEXT);
	}

	wdc_atapi_update_status(chp);

	if (chp->ch_status & (WDCS_BSY | WDCS_DRQ)) {
		return (CONTINUE_POLL);
	}

	xfer->next = wdc_atapi_done;
	return (GOTO_NEXT);
}

