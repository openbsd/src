/*
 * Copyright (c) 1995 Theo de Raadt
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Based IN PART on aic6360 by Jarle Greipsland, an older esp driver
 * by Peter Galbavy, and work by Charles Hannum on a few other drivers.
 */

/*
 * todo:
 * fix & enable sync
 * confirm parity and bus failures do not lock up driver
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/espreg.h>
#include <sparc/dev/espvar.h>

int esp_debug = ESP_SHOWPHASE|ESP_SHOWMISC|ESP_SHOWTRAC|ESP_SHOWCMDS; /**/ 

#if 1
#define ESPD(x)	
#else
#define ESPD(x)	printf x
#endif

void	espattach	__P((struct device *, struct device *, void *));
int	espmatch	__P((struct device *, void *, void *));
int	espprint	__P((void *, char *));
void	espreadregs	__P((struct esp_softc *));
int	espgetbyte	__P((struct esp_softc *, int));
void	espselect	__P((struct esp_softc *));
void	esp_reset	__P((struct esp_softc *));
void	esp_init	__P((struct esp_softc *, int));
int	esp_scsi_cmd	__P((struct scsi_xfer *));
int	esp_poll	__P((struct esp_softc *, struct ecb *));
int	espphase	__P((struct esp_softc *));
void	esp_sched	__P((struct esp_softc *));
void	esp_done	__P((struct ecb *));
void	esp_msgin	__P((struct esp_softc *));
void	esp_makemsg	__P((struct esp_softc *));
void	esp_msgout	__P((struct esp_softc *));
int	espintr		__P((struct esp_softc *));
void	esp_timeout	__P((void *arg));

struct cfdriver espcd = {
	NULL, "esp", espmatch, espattach, DV_DULL, sizeof(struct esp_softc)
};

struct scsi_adapter esp_switch = {
	esp_scsi_cmd, minphys, NULL, NULL
};

struct scsi_device esp_dev = {
	NULL, NULL, NULL, NULL
};

/*
 * Does anyone actually use this, and what for ?
 */
int
espprint(aux, name)
	void *aux;
	char *name;
{
	return (UNCONF);
}


int
espmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return (1);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}


/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	struct esp_softc *sc = (void *)self;
	struct espregs *espr;
	struct bootpath *bp;
	int freq;

	/*
	 * Make sure things are sane. I don't know if this is ever
	 * necessary, but it seem to be in all of Torek's code.
	 */
	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}

	sc->sc_pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d", sc->sc_pri);

	/*
	 * Map my registers in, if they aren't already in virtual
	 * address space.
	 */
	if (ca->ca_ra.ra_vaddr)
		sc->sc_regs = (struct espregs *) ca->ca_ra.ra_vaddr;
	else {
		sc->sc_regs = (struct espregs *)
		    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len,
		    ca->ca_bustype);
	}
	espr = sc->sc_regs;

	/* Other settings */
	sc->sc_node = ca->ca_ra.ra_node;
	if (ca->ca_bustype == BUS_SBUS) {
		sc->sc_id = getpropint(sc->sc_node, "initiator-id", 7);
		sc->sc_freq = getpropint(sc->sc_node, "clock-frequency", -1);
	} else {
		sc->sc_id = 7;
		sc->sc_freq = 24000000;
	}
	if (sc->sc_freq < 0)
		sc->sc_freq = ((struct sbus_softc *)
		    sc->sc_dev.dv_parent)->sc_clockfreq;

	freq = sc->sc_freq / 1000000;		/* gimme Mhz */

	/*
	 * This is the value used to start sync negotiations. For a
	 * 25Mhz clock, this gives us 40, or 160nS, or 6.25Mb/s. It
	 * is constant for each adapter. In turn, notice that the ESP
	 * register "SYNCTP" is = (250 / the negotiated period).
	 */
	sc->sc_minsync = 1000 / freq;

	/* 0 is actually 8, even though the register only has 3 bits */
	sc->sc_ccf = FREQTOCCF(freq) & 0x07;

	/* The value must not be 1 -- make it 2 */
	if (sc->sc_ccf == 1)
		sc->sc_ccf = 2;

	/*
	 * The recommended timeout is 250ms (1/4 seconds). This
	 * register is loaded with a value calculated as follows:
	 *
	 *		(timout period) x (CLK frequency)
	 *	reg = -------------------------------------
	 *		 8192 x (Clock Conversion Factor)
	 *
	 * We have the CCF from above. For a 25MHz clock this gives
	 * a constant of 153 (we round up).
	 */
	sc->sc_timeout = (sc->sc_freq / 4 / 8192 / sc->sc_ccf) + 1;

	/*
	 * find the corresponding DMA controller.
	 */
	sc->sc_dma = ((struct dma_softc *)getdevunit("dma",
	    sc->sc_dev.dv_unit));
	if (sc->sc_dma)
		sc->sc_dma->sc_esp = sc;

	sc->sc_cfg1 = sc->sc_id | ESPCFG1_PARENB;
	sc->sc_cfg2 = 0;
	sc->sc_cfg3 = 0;

	/*
	 * The ESP100 only has a cfg1 register. The ESP100A has it, but
	 * lacks the cfg3 register. Thus, we can tell which chip we have.
	 * XXX: what about the FAS100A?
	 */
	espr->espr_cfg2 = ESPCFG2_SCSI2 | ESPCFG2_RPE;
	if ((espr->espr_cfg2 & ~ESPCFG2_RSVD) != (ESPCFG2_SCSI2 | ESPCFG2_RPE)) {
		printf(": ESP100");
		sc->sc_rev = ESP100;
	} else {
		espr->espr_cfg2 = 0;
		espr->espr_cfg3 = 0;
		espr->espr_cfg3 = ESPCFG3_CDB | ESPCFG3_FCLK;
		if (espr->espr_cfg3 != (ESPCFG3_CDB | ESPCFG3_FCLK)) {
			printf(": ESP100A");
			/* XXX sc->sc_cfg2 = ESPCFG2_SCSI2 | ESPCFG2_FE; */
			sc->sc_rev = ESP100A;
		} else {
			espr->espr_cfg3 = 0;
			printf(": ESP200");
			sc->sc_rev = ESP200;
		}
	}

	sc->sc_state = 0;
	esp_init(sc, 1);

	printf(" %dMhz, target %d\n", freq, sc->sc_id);

#if defined(SUN4C) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		/* add to the sbus structures */
		sc->sc_sd.sd_reset = (void *) esp_reset;
		sbus_establish(&sc->sc_sd, &sc->sc_dev);

		/*
		 * If the device is in an SBUS slave slot, bail now.
		 */
		if (sbus_slavecheck(self, ca))
			return;
	}
#endif /* SUN4C || SUN4M */

	/* and the interupts */
	sc->sc_ih.ih_fun = (void *) espintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(sc->sc_pri, &sc->sc_ih);
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	/*
	 * fill in the prototype scsi_link.
	 */
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_id;
	sc->sc_link.adapter = &esp_switch;
	sc->sc_link.device = &esp_dev;
	sc->sc_link.openings = 2;

	/*
	 * If the boot path is "esp", and the numbers point to
	 * this controller, then advance the bootpath a step.
	 * XXX boot /sbus/esp/sd@3,0 passes in esp@0,0 -- which
	 * is almost always wrong, but match in that case anyways.
	 */
	bp = ca->ca_ra.ra_bp;
	switch (ca->ca_bustype) {
	case BUS_SBUS:
		if (bp != NULL && strcmp(bp->name, "esp") == 0 &&
		    ((bp->val[0]==ca->ca_slot &&
		     (bp->val[1]==ca->ca_offset || bp->val[1]==0)) ||
		    (bp->val[0]==-1 && bp->val[1]==sc->sc_dev.dv_unit)))
			bootpath_store(1, bp + 1);
		break;
	default:
		if (bp != NULL && strcmp(bp->name, "esp") == 0 &&
		    bp->val[0] == -1 && bp->val[1] == sc->sc_dev.dv_unit)
			bootpath_store(1, bp + 1);
		break;
	}

	/*
	 * Now try to attach all the sub-devices
	 */
	config_found(self, &sc->sc_link, espprint);

	bootpath_store(1, NULL);
}


/*
 * Warning: This inserts two commands into the ESP command fifo.  Because
 * there are only 2 entries in that fifo, you may need to put a DELAY(1)
 * after calling this, if you are going to issue another command soon.
 */
void
espflush(sc)
	struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;

	if (espr->espr_fflag & ESPFIFO_FF) {
		espr->espr_cmd = ESPCMD_FLUSH;
		espr->espr_cmd = ESPCMD_NOP;
	}
}

/*
 * returns -1 if no byte is available because fifo is empty.
 */
int
espgetbyte(sc, dotrans)
	struct esp_softc *sc;
	int dotrans;
{
	struct espregs *espr = sc->sc_regs;

	if (espr->espr_fflag & ESPFIFO_FF)
		return espr->espr_fifo;
	return -1;
}

/*
 * Send a command to a target, as well as any required messages.
 * There are three cases. The first two cases are fairly simple..
 * 1) command alone
 * 	load command into fifo, and use ESPCMD_SELNATN
 * 2) MSG_IDENTIFY + command
 *	load message and command into fifo, and use ESPCMD_SELATN
 * 3) a bunch of messages + command
 *	load first message byte into fifo. Use ESPCMD_SELATNS. When the
 *	next interrupt occurs, load the remainer of the message into
 *	the fifo and use ESPCMD_TRANS. When the device is ready to
 *	receive the command, it will switch into COMMAND_PHASE, and
 *	at that point we will feed it the command.
 */
void
espselect(sc)
	struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;
	register struct ecb *ecb = sc->sc_nexus;
	register struct scsi_link *sc_link = ecb->xs->sc_link;
	struct esp_tinfo *ti = &sc->sc_tinfo[sc_link->target];
	u_char *cmd = (u_char *)&ecb->cmd;
	int loadcmd = 1;
	int outcmd, i;

	espr->espr_selid = sc_link->target;
	espr->espr_syncoff = ti->offset;
	espr->espr_synctp = ti->synctp;

	sc->sc_state = ESPS_SELECTING;

	if (ecb->xs->flags & SCSI_RESET)
		sc->sc_msgpriq = SEND_DEV_RESET;
	else if (ti->flags & DO_NEGOTIATE)
		sc->sc_msgpriq = SEND_SDTR;
	else
		sc->sc_msgpriq = SEND_IDENTIFY;

	if (sc->sc_msgpriq) {
		esp_makemsg(sc);

		ESPD(("OM["));
		for (i = 0; i < sc->sc_omlen; i++)
			ESPD(("%02x%c", sc->sc_omp[i],
			    (i == sc->sc_omlen-1) ? ']' : ' '));
		ESPD((" "));

		espr->espr_fifo = sc->sc_omp[0];	/* 1st msg byte only */
		if (sc->sc_omlen == 1) {
			outcmd = ESPCMD_SELATN;
		} else {
			outcmd = ESPCMD_SELATNS;
			/* and this will will load the rest of the msg bytes */
			sc->sc_state = ESPS_SELECTSTOP;
			loadcmd = 0;
		}
	} else
		outcmd = ESPCMD_SELNATN;

	ESPD(("P%d/%02x/%d ", (ecb->xs->flags & SCSI_POLL) ? 1 : 0, outcmd, loadcmd));

	if (loadcmd) {
		ESPD(("CMD["));
		for (i = 0; i < ecb->clen; i++)
			ESPD(("%02x%c", cmd[i],
			    (i == ecb->clen-1) ? ']' : ' '));
		ESPD((" "));

		/* load the command into the FIFO */
		for (i = 0; i < ecb->clen; i++)
			espr->espr_fifo = cmd[i];
	}

	espr->espr_cmd = outcmd;
	if (!(ecb->xs->flags & SCSI_POLL))
		sc->sc_flags |= ESPF_MAYDISCON;
}


/*
 * Reset the ESP and DMA chips. Stops any transactions dead in the water;
 * does not cause an interrupt.
 */
void 
esp_reset(sc)
	struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;

	dmareset(sc->sc_dma);		/* reset DMA first */

	espr->espr_cmd = ESPCMD_RSTCHIP;
	DELAY(5);
	espr->espr_cmd = ESPCMD_NOP;
	DELAY(5);

	/* do these backwards, and fall through */
	switch (sc->sc_rev) {
	case ESP200:
		espr->espr_cfg3 = sc->sc_cfg3;
	case ESP100A:
		espr->espr_cfg2 = sc->sc_cfg2;
	case ESP100:
		espr->espr_cfg1 = sc->sc_cfg1;
		espr->espr_ccf = sc->sc_ccf;
		espr->espr_syncoff = 0;
		espr->espr_timeout = sc->sc_timeout;
		break;
	}
}

/*
 * Initialize esp state machine
 */
void
esp_init(sc, doreset)
	struct esp_softc *sc;
	int doreset;
{
	struct espregs *espr = sc->sc_regs;
	struct ecb *ecb;
	int i;
	
	/*
	 * reset the chip to a known state
	 */
	esp_reset(sc);

	if (doreset) {
		espr->espr_cmd = ESPCMD_RSTSCSI;
		DELAY(500);
		/* cheat: we don't want the state machine to reset again.. */
		esp_reset(sc);
	}

	if (sc->sc_state == 0) {	/* First time through */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = 0;
		ecb = sc->sc_ecb;
		bzero(ecb, sizeof(sc->sc_ecb));
		for (i = 0; i < sizeof(sc->sc_ecb) / sizeof(*ecb); i++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		sc->sc_state = ESPS_IDLE;
		if (sc->sc_nexus != NULL) {
			sc->sc_nexus->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, sc->sc_nexus);
			esp_done(sc->sc_nexus);
		}
		sc->sc_nexus = NULL;
		while (ecb = sc->nexus_list.tqh_first) {
			ecb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, ecb);
			esp_done(ecb);
		}
	}
	
	for (i = 0; i < 8; i++) {
		struct esp_tinfo *ti = &sc->sc_tinfo[i];

		ti->flags = DO_NEGOTIATE;
		ti->period = sc->sc_minsync;
		ti->synctp = 250 / ti->period;
		ti->offset = ESP_SYNC_REQOFF;
	}
	sc->sc_state = ESPS_IDLE;
}

/*
 * Start a SCSI-command: This function is called by the higher level
 * SCSI-driver to queue/run SCSI-commands.
 */
int 
esp_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;
	struct ecb *ecb;
	int	s;
	
	/*ESPD(("NS%08x/%08x/%d ", xs, xs->data, xs->datalen));*/

	/* XXX: set lun */
	xs->cmd->bytes[0] |= (sc_link->lun << SCSI_CMD_LUN_SHIFT);

	/* Get a esp command block */
	s = splbio();
	ecb = sc->free_list.tqh_first;
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
	}
	splx(s);
		
	if (ecb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

	/* Initialize ecb */
	ecb->flags = ECB_ACTIVE;
	ecb->xs = xs;
	bcopy(xs->cmd, &ecb->cmd, xs->cmdlen);
	ecb->clen = xs->cmdlen;
	ecb->daddr = xs->data;
	ecb->dleft = xs->datalen;
	ecb->stat = 0;
	
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_list, ecb, chain);
	if ((xs->flags & SCSI_POLL) == 0)
		timeout(esp_timeout, ecb, (xs->timeout * hz) / 1000);

	if (sc->sc_state == ESPS_IDLE)
		esp_sched(sc);
	splx(s);

	/* Not allowed to use interrupts, use polling instead */
	if (xs->flags & SCSI_POLL)
		return esp_poll(sc, ecb);
	return SUCCESSFULLY_QUEUED;
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
esp_poll(sc, ecb)
	struct esp_softc *sc;
	struct ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	int	count = xs->timeout * 1000;

	ESP_TRACE(("esp_poll\n"));
	while (count) {
		if (dmapending(sc->sc_dma)) {
			/*
			 * We decrement the interrupt event counter to
			 * repair it... because this isn't a real interrupt.
			 */
			if (espintr(sc))
				--sc->sc_intrcnt.ev_count;
			continue;
		}
		if (xs->flags & ITSDONE)
			break;
		DELAY(1);
		count--;
	}

	if (count == 0) {
		ecb->xs->error = XS_TIMEOUT;
		sc_print_addr(ecb->xs->sc_link);
		printf("timed out\n");
		esp_reset(sc);
	}
	dmaenintr(sc->sc_dma);
	return COMPLETE;
}


/*
 * Notice that we do not read the live register on an ESP100. On the
 * ESP100A and above the FE (Feature Enable) bit in config 2 latches
 * the phase in the register so it is safe to read.
 */
int
espphase(sc)
	struct esp_softc *sc;
{

	if (sc->sc_rev > ESP100)
		return (sc->sc_regs->espr_stat & ESPSTAT_PHASE);
	return (sc->sc_espstat & ESPSTAT_PHASE);
}


/*
 * Schedule a scsi operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from esp_scsi_cmd and esp_done.  This may
 * save us an unecessary interrupt just to get things going.  Should only be
 * called when state == ESPS_IDLE and at bio pl.
 */
void
esp_sched(sc)
	struct esp_softc *sc;
{
	struct scsi_link *sc_link;
	struct ecb *ecb;
	int t;
	
	ESP_TRACE(("esp_sched\n"));

	/*
	 * Find first ecb in ready queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	for (ecb = sc->ready_list.tqh_first; ecb; ecb = ecb->chain.tqe_next) {
		caddr_t cmd = (caddr_t) &ecb->cmd;
		sc_link = ecb->xs->sc_link;
		t = sc_link->target;
		if (!(sc->sc_tinfo[t].lubusy & (1 << sc_link->lun))) {
			struct esp_tinfo *ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			sc->sc_nexus = ecb;
			sc->sc_flags = 0;
			sc_link = ecb->xs->sc_link;
			espselect(sc);
			ti = &sc->sc_tinfo[sc_link->target];
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			ti->lubusy |= (1<<sc_link->lun);
			break;
		} else
			ESP_MISC(("%d:%d busy\n", t, sc_link->lun));
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
esp_done(ecb)
	struct ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;

	ESP_TRACE(("esp_done "));

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the 
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and 
	 * push the command back into the ready queue *before* any other 
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if (ecb->flags & ECB_CHKSENSE)
			xs->error = XS_SENSE;
		else if ((ecb->stat & ST_MASK) == SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&ecb->cmd;

			ESPD(("RS "));
			ESP_MISC(("requesting sense "));
			/* First, save the return values */
			xs->resid = ecb->dleft;
			xs->status = ecb->stat;
			/* Next, setup a request sense command block */
			bzero(ss, sizeof(*ss));
			ss->opcode = REQUEST_SENSE;
			ss->byte2 = sc_link->lun << SCSI_CMD_LUN_SHIFT;
			ss->length = sizeof(struct scsi_sense_data);
			ecb->clen = sizeof(*ss);
			ecb->daddr = (char *)&xs->sense;
			ecb->dleft = sizeof(struct scsi_sense_data);
			ecb->flags = ECB_ACTIVE|ECB_CHKSENSE;
			TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
			sc->sc_tinfo[sc_link->target].lubusy &=
			    ~(1<<sc_link->lun);
			sc->sc_tinfo[sc_link->target].senses++;
			/* found it */
			if (sc->sc_nexus != ecb)
				TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
			sc->sc_state = ESPS_IDLE;
			esp_sched(sc);
			return;
		} else
			xs->resid = ecb->dleft;
	}

	xs->flags |= ITSDONE;

#if ESP_DEBUG > 1
	if (esp_debug & ESP_SHOWMISC) {
		printf("err=0x%02x ", xs->error);
		if (xs->error == XS_SENSE)
			printf("sense=%02x\n", xs->sense.error_code);
	}
	if ((xs->resid || xs->error > XS_SENSE) && (esp_debug & ESP_SHOWMISC)) {
		if (xs->resid)
			printf("esp_done: resid=%d\n", xs->resid);
		if (xs->error)
			printf("esp_done: error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ECB from whatever queue it's on.  We have to do a bit of
	 * a hack to figure out which queue it's on.  Note that it is *not*
	 * necessary to cdr down the ready queue, but we must cdr down the
	 * nexus queue and see if it's there, so we can mark the unit as no
	 * longer busy.  This code is sickening, but it works.
	 */
	if (ecb == sc->sc_nexus) {
		sc->sc_tinfo[sc_link->target].lubusy &= ~(1<<sc_link->lun);
		sc->sc_nexus = NULL;
		sc->sc_state = ESPS_IDLE;
		esp_sched(sc);
	} else if (sc->ready_list.tqh_last == &ecb->chain.tqe_next) {
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
	} else {
		register struct ecb *ecb2;

		for (ecb2 = sc->nexus_list.tqh_first; ecb2;
		    ecb2 = ecb2->chain.tqe_next)
			if (ecb2 == ecb)
				break;
		if (ecb2) {
			TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
			sc->sc_tinfo[sc_link->target].lubusy &=
			    ~(1<<sc_link->lun);
		} else if (ecb->chain.tqe_next) {
			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
		} else {
			printf("%s: can't find matching ecb\n",
			    sc->sc_dev.dv_xname);
			Debugger();
		}
	}
	/* Put it on the free list. */
	ecb->flags = ECB_FREE;
	TAILQ_INSERT_HEAD(&sc->free_list, ecb, chain);

	sc->sc_tinfo[sc_link->target].cmds++;
	ESPD(("DONE%d ", xs->resid));
	scsi_done(xs);
}

/*
 * INTERRUPT/PROTOCOL ENGINE
 */

/*
 * Schedule an outgoing message by prioritizing it, and asserting
 * attention on the bus. We can only do this when we are the initiator
 * else there will be an illegal command interrupt.
 */
#define esp_sched_msgout(m) \
	do { \
		sc->sc_regs->espr_cmd = ESPCMD_SETATN; \
		sc->sc_msgpriq |= (m); \
		ESPD(("Mq%02x ", sc->sc_msgpriq)); \
	} while (0)

#define IS1BYTEMSG(m) (((m) != 1 && ((unsigned)(m)) < 0x20) || (m) & 0x80)
#define IS2BYTEMSG(m) (((m) & 0xf0) == 0x20)
#define ISEXTMSG(m) ((m) == 1)

/*
 * Get an incoming message as initiator.
 *
 * The SCSI bus must already be in MESSAGE_IN_PHASE and there is a
 * byte in the FIFO
 */
void
esp_msgin(sc)
	register struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;
	struct ecb *ecb;
	int extlen;
	int x, i;
	
	ESP_TRACE(("esp_msgin "));

	/* is something wrong ? */
	if (sc->sc_phase != MESSAGE_IN_PHASE) {
		printf("%s: not MESSAGE_IN_PHASE\n", sc->sc_dev.dv_xname);
		return;
	}

	ESPD(("MSGIN:%d ", espr->espr_fflag & ESPFIFO_FF));

#ifdef fixme
	/*
	 * Prepare for a new message.  A message should (according
	 * to the SCSI standard) be transmitted in one single
	 * MESSAGE_IN_PHASE. If we have been in some other phase,
	 * then this is a new message.
	 */
	if (sc->sc_prevphase != MESSAGE_IN_PHASE) {
		sc->sc_flags &= ~ESPF_DROP_MSGI;
		if (sc->sc_imlen > 0) {
			printf("%s: message type %02x",
			    sc->sc_dev.dv_xname, sc->sc_imess[0]);
			if (!IS1BYTEMSG(sc->sc_imess[0]))
				printf(" %02x", sc->sc_imess[1]);
			printf(" was dropped\n");
		}
		sc->sc_imlen = 0;
	}
#endif

	/*
	 * Which target is reselecting us? (The ID bit really)
	 * On a reselection, there should be the reselecting target's
	 * id and an identify message in the fifo.
	 */
	if (sc->sc_state == ESPS_RESELECTED && sc->sc_imlen == 0) {
		x = espgetbyte(sc, 0);
		if (x == -1) {
			printf("%s: msgin reselection found fifo empty\n",
			    sc->sc_dev.dv_xname);
			return;
		}
		ESPD(("ID=%02x ", x));
		sc->sc_selid = (u_char)x & ~(1<<sc->sc_id);
		ESP_MISC(("selid=0x%02x ", sc->sc_selid));
	}

	/*
	 * If parity errors just dump everything on the floor
	 */
	if (sc->sc_espstat & ESPSTAT_PE) {
		printf("%s: SCSI bus parity error\n",
		    sc->sc_dev.dv_xname);
		esp_sched_msgout(SEND_PARITY_ERROR);
		DELAY(1);
		sc->sc_flags |= ESPF_DROP_MSGI;
	}

	/*
	 * If we're going to reject the message, don't bother storing
	 * the incoming bytes.  But still, we need to ACK them.
	 * XXX: the above comment might be true -- this is not being
	 * done though!
	 */
	if ((sc->sc_flags & ESPF_DROP_MSGI) == 0) {
		x = espgetbyte(sc, 1);
		if (x == -1) {
			printf("%s: msgin fifo empty at %d\n",
			    sc->sc_dev.dv_xname, sc->sc_imlen);
			if (sc->sc_espintr & ESPINTR_BS) {
				espr->espr_cmd = ESPCMD_TRANS;
				ESPD(("MSI:y "));
			} else
				ESPD(("MSI:n "));
			return;
		}
		ESPD(("{%02x} ", (u_char)x));
		sc->sc_imess[sc->sc_imlen] = (u_char)x;
		if (sc->sc_imlen >= ESP_MSGLEN_MAX) {
			esp_sched_msgout(SEND_REJECT);
			DELAY(1);
			sc->sc_flags |= ESPF_DROP_MSGI;
		} else {
			sc->sc_imlen++;
			/* 
			 * This testing is suboptimal, but most
			 * messages will be of the one byte variety, so
			 * it should not effect performance
			 * significantly.
			 */
			if (sc->sc_imlen == 1 && IS1BYTEMSG(sc->sc_imess[0]))
				goto gotit;
			if (sc->sc_imlen == 2 && IS2BYTEMSG(sc->sc_imess[0]))
				goto gotit;
			if (sc->sc_imlen >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
			    sc->sc_imlen == sc->sc_imess[1] + 2)
				goto gotit;
		}
	}
	goto done;

gotit:
	ESPD(("MP["));
	for (i=0; i<sc->sc_imlen; i++)
		ESPD(("%02x%c", sc->sc_imess[i],
		    (i==sc->sc_imlen-1) ? ']' : ' '));
	ESPD((" "));

	sc->sc_imlen = 0;	/* message is fully received */

	ESP_MISC(("gotmsg "));
	/*
	 * Now we should have a complete message (1 byte, 2 byte
	 * and moderately long extended messages).  We only handle
	 * extended messages which total length is shorter than
	 * ESP_MSGLEN_MAX.  Longer messages will be amputated.
	 */
	if (sc->sc_state == ESPS_NEXUS ||
	    sc->sc_state == ESPS_DOINGMSGIN ||
	    sc->sc_state == ESPS_DOINGSTATUS) {
		struct esp_tinfo *ti;

		ecb = sc->sc_nexus;
		ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

		sc->sc_state = ESPS_NEXUS;	/* where we go after this */

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			ESP_MISC(("cmdcomplete "));
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
				printf("%s: CMDCOMPLETE but no command?\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			if (sc->sc_dleft < 0) {
				sc_print_addr(ecb->xs->sc_link);
				printf("%d extra bytes\n", -sc->sc_dleft);
				ecb->dleft = 0;
			}
			ecb->xs->resid = ecb->dleft = sc->sc_dleft;
			ecb->flags |= ECB_DONE;
			break;
		case MSG_MESSAGE_REJECT:
			if (esp_debug & ESP_SHOWMISC)
				printf("%s targ %d: our msg rejected by target\n",
				    sc->sc_dev.dv_xname,
				    ecb->xs->sc_link->target);
			if (sc->sc_flags & ESPF_SYNCHNEGO) {
				/*
				 * Device doesn't even know what sync is
				 * (right?)
				 */
				ti->offset = 0;
				ti->period = sc->sc_minsync;
				ti->synctp = 250 / ti->period;
				sc->sc_flags &= ~ESPF_SYNCHNEGO;
				ti->flags &= ~DO_NEGOTIATE;
			}
			/* Not all targets understand INITIATOR_DETECTED_ERR */
			if (sc->sc_msgout == SEND_INIT_DET_ERR) {
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
			}
			break;
		case MSG_NOOP:
			break;
		case MSG_DISCONNECT:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
				printf("%s: nothing to DISCONNECT\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			ti->dconns++;
			TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
			ecb = sc->sc_nexus = NULL;
			sc->sc_state = ESPS_EXPECTDISC;
			sc->sc_flags |= ESPF_MAYDISCON;
			break;
		case MSG_SAVEDATAPOINTER:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
				printf("%s: no DATAPOINTERs to save\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			ESPD(("DL%d/%d ", sc->sc_dleft, ecb->dleft));
			ecb->dleft = sc->sc_dleft;
			ecb->daddr = sc->sc_dp;
			break;
		case MSG_RESTOREPOINTERS:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
				printf("%s: no DATAPOINTERs to restore\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;
		case MSG_EXTENDED:
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				sc->sc_flags &= ~ESPF_SYNCHNEGO;
				ti->flags &= ~DO_NEGOTIATE;
				if (ti->offset == 0 ||
				    ti->offset > ESP_SYNC_MAXOFF ||
				    ti->period < sc->sc_minsync) {
					ti->offset = 0;
					ti->period = sc->sc_minsync;
					ti->synctp = 250 / ti->period;
					break;
				}
				printf("%s targ %d: sync, offset %d, period %dnsec\n",
				    sc->sc_dev.dv_xname, ecb->xs->sc_link->target,
				    ti->offset, ti->period * 4);
				ti->synctp = 250 / ti->period;
				break;
			default:
				/* Extended messages we don't handle */
				esp_sched_msgout(SEND_REJECT);
				DELAY(1);
				break;
			}
			break;
		default:
			/* thanks for that ident... XXX: we should check it? */
			if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
				esp_sched_msgout(SEND_REJECT);
				DELAY(1);
			}
			break;
		}
	} else if (sc->sc_state == ESPS_RESELECTED) {
		struct scsi_link *sc_link;
		u_char lunit;

		if (MSG_ISIDENTIFY(sc->sc_imess[0])) { 	/* Identify? */
			ESP_MISC(("searching "));
			/*
			 * Search wait queue for disconnected cmd
			 * The list should be short, so I haven't bothered with
			 * any more sophisticated structures than a simple
			 * singly linked list. 
			 */
			lunit = sc->sc_imess[0] & 0x07;
			for (ecb = sc->nexus_list.tqh_first; ecb;
			    ecb = ecb->chain.tqe_next) {
				sc_link = ecb->xs->sc_link;
				if (sc_link->lun == lunit &&
				    sc->sc_selid == (1<<sc_link->target)) {
					TAILQ_REMOVE(&sc->nexus_list, ecb,
					    chain);
					break;
				}
			}

			if (!ecb) {
				/* Invalid reselection! */
				esp_sched_msgout(SEND_ABORT);
				DELAY(1);
				printf("esp: invalid reselect (idbit=0x%2x)\n",
				    sc->sc_selid);
			} else {
				/*
				 * Reestablish nexus:
				 * Setup driver data structures and
				 * do an implicit RESTORE POINTERS
				 */
				sc->sc_nexus = ecb;
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				sc->sc_tinfo[sc_link->target].lubusy |=
				    (1<<sc_link->lun);
				espr->espr_syncoff =
				    sc->sc_tinfo[sc_link->target].offset;
				espr->espr_synctp = 250 /
				    sc->sc_tinfo[sc_link->target].period;
				ESP_MISC(("... found ecb"));
				sc->sc_state = ESPS_NEXUS;
			}
		} else {
			printf("%s: bogus reselect (no IDENTIFY) %0x2x\n",
			    sc->sc_dev.dv_xname, sc->sc_selid);
			esp_sched_msgout(SEND_DEV_RESET);
			DELAY(1);
		}
	} else { /* Neither ESP_HASNEXUS nor ESP_RESELECTED! */
		printf("%s: unexpected message in; will send DEV_RESET\n",
		    sc->sc_dev.dv_xname);
		esp_sched_msgout(SEND_DEV_RESET);
		DELAY(1);
	}
done:
	espr->espr_cmd = ESPCMD_MSGOK;
	espr->espr_cmd = ESPCMD_NOP;
}


void
esp_makemsg(sc)
	register struct esp_softc *sc;
{
	struct esp_tinfo *ti;
	struct ecb *ecb = sc->sc_nexus;
	int i;

	sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
	ESPD(("MQ%02x/%02x ", sc->sc_msgpriq, sc->sc_msgout));

	sc->sc_omlen = 1;		/* "Default" message len */
	switch (sc->sc_msgout) {
	case SEND_SDTR:		/* implies an IDENTIFY message */
		sc->sc_flags |= ESPF_SYNCHNEGO;
		ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
		sc->sc_omess[0] = MSG_IDENTIFY(ecb->xs->sc_link->lun,
		    !(ecb->xs->flags & SCSI_POLL));
		sc->sc_omess[1] = MSG_EXTENDED;
		sc->sc_omess[2] = 3;
		sc->sc_omess[3] = MSG_EXT_SDTR;
		sc->sc_omess[4] = ti->period;
		sc->sc_omess[5] = ti->offset;
		sc->sc_omlen = 6;
		/* Fallthrough! */
	case SEND_IDENTIFY:
		sc->sc_omess[0] = MSG_IDENTIFY(ecb->xs->sc_link->lun,
		    !(ecb->xs->flags & SCSI_POLL));
		break;
	case SEND_DEV_RESET:
		sc->sc_omess[0] = MSG_BUS_DEV_RESET;
		if (sc->sc_nexus)
			sc->sc_nexus->flags |= ECB_DONE;
		break;
	case SEND_PARITY_ERROR:
		sc->sc_omess[0] = MSG_PARITY_ERROR;
		break;
	case SEND_ABORT:
		sc->sc_omess[0] = MSG_ABORT;
		if (sc->sc_nexus)
			sc->sc_nexus->flags |= ECB_DONE;
		break;
	case SEND_INIT_DET_ERR:
		sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
		break;
	case SEND_REJECT:
		sc->sc_omess[0] = MSG_MESSAGE_REJECT;
		break;
	default:
		sc->sc_omess[0] = MSG_NOOP;
		break;
	}
	sc->sc_omp = sc->sc_omess;
}


/*
 * Send the highest priority, scheduled message
 */
void
esp_msgout(sc)
	register struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;
	struct esp_tinfo *ti;
	struct ecb *ecb;
	int i;

	/* Pick up highest priority message */
	sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
	esp_makemsg(sc);

	for (i = 0; i < sc->sc_omlen; i++)
		espr->espr_fifo = sc->sc_omp[i];
	espr->espr_cmd = ESPCMD_TRANS;
}


void
esp_timeout(arg)
	void *arg;
{
	struct ecb *ecb = (struct ecb *)arg;
	struct esp_softc *sc;

	sc = ecb->xs->sc_link->adapter_softc;
	sc_print_addr(ecb->xs->sc_link);
	printf("timed out at %d %d\n", sc->sc_state, sc->sc_phase);

	ecb->xs->error = XS_TIMEOUT;
	esp_reset(sc);
	esp_done(ecb);
	printf("new ecb %08x\n", sc->sc_nexus);
}

/*
 * Read the ESP registers, and save their contents for later use.
 * ESP_STAT, ESP_STEP & ESP_INTR are mostly zeroed out when reading
 * ESP_INTR - so make sure it is the last read.
 *
 * XXX: TDR: this logic seems unsound
 * I think that (from reading the docs) most bits in these registers
 * only make sense when the DMA CSR has an interrupt showing. So I have
 * coded this to not do anything if there is no interrupt or error
 * pending.
 */
void
espreadregs(sc)
	struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;

	/* they mean nothing if the is no pending interrupt ??? */
	if (!(dmapending(sc->sc_dma)))
		return;

	/* Only the stepo bits are of interest */
	sc->sc_espstep = espr->espr_step & ESPSTEP_MASK;
	sc->sc_espstat = espr->espr_stat;
	sc->sc_espintr = espr->espr_intr;

	ESP_MISC(("regs[intr=%02x,stat=%02x,step=%02x] ", sc->sc_espintr,
	    sc->sc_espstat, sc->sc_espstep));
}

/*
 * Whatever we do, we must generate an interrupt if we expect to go
 * to the next state.
 * Note: this increments the events even if called from esp_poll()
 */
int 
espintr(sc)
	register struct esp_softc *sc;
{
	struct espregs *espr = sc->sc_regs;
	register struct ecb *ecb = sc->sc_nexus;
	register struct scsi_link *sc_link;
	u_char *cmd;
	struct esp_tinfo *ti;
	int dmaintrwas, i;

	/*
	 * Revision 1 DMA's must have their interrupts disabled,
	 * otherwise we can get bus timeouts while reading ESP
	 * registers.
	 */
	if (sc->sc_dma->sc_rev == DMAREV_1)
		dmaintrwas = dmadisintr(sc->sc_dma);

	espreadregs(sc);

	/*
	 * If either of these two things is true, we caused an interrupt.
	 * If we didn't, let's get out of here.
	 */
	if ((sc->sc_espintr | dmapending(sc->sc_dma)) == 0) {
		if (sc->sc_dma->sc_rev == DMAREV_1 && dmaintrwas)
			dmaenintr(sc->sc_dma);
		return (0);
	}

	sc->sc_phase = espphase(sc);

	ESPD(("I%02x/%02x/%02x ", sc->sc_espintr, sc->sc_state, sc->sc_phase));

	if (sc->sc_espintr & ESPINTR_SBR) {
		espflush(sc);
		printf("%s: scsi bus reset\n", sc->sc_dev.dv_xname);
		esp_init(sc, 0);	/* Restart everything */
		/*
		 * No interrupt expected, since there are now no
		 * ecb's in progress.
		 */
		goto done;
	}

	if (sc->sc_espintr & ESPINTR_ILL) {
		printf("%s: illegal command %02x\n",
		    sc->sc_dev.dv_xname, espr->espr_cmd);
		if (sc->sc_state == ESPS_NEXUS) {
			ecb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, ecb);
			esp_done(ecb);
		}
		esp_reset(sc);
		esp_sched(sc);		/* start next command */
		goto done;
	}

	if (sc->sc_espstat & ESPSTAT_GE) {
		/* no target ? */
		espflush(sc);
		printf("%s: gross error\n", sc->sc_dev.dv_xname);
		if (sc->sc_state == ESPS_NEXUS) {
			ecb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, ecb);
			esp_done(ecb);
		}
		esp_reset(sc);
		esp_sched(sc);
		/* started next command, which will interrupt us */
		goto done;
	}

	/*
	 * The `bad interrupts' have already been checked.
	 */
states:
	switch (sc->sc_state) {
	case ESPS_IDLE:
		ESPD(("I "));

		sc->sc_nexus = NULL;

		if (sc->sc_espintr & ESPINTR_RESEL) {
			sc->sc_state = ESPS_RESELECTED;
			goto states;
		}
		esp_sched(sc);

		/*
		 * We will get an interrupt if esp_sched() queues
		 * anything to the scsi bus.
		 */
		goto done;
	case ESPS_SELECTING:
		/*
		 * For a simple select, we sent either
		 * ESPCMD_SELATN or ESPCMD_SELNATN
		 */
		ESPD(("S "));

		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;	/* will cleanup */
			goto states;
		}

		if (sc->sc_espintr & ESPINTR_RESEL) {
			/*
			 * If we were trying to select a target,
			 * push our command back into the ready list.
			 */
			if (sc->sc_state == ESPS_SELECTING) {
				TAILQ_INSERT_HEAD(&sc->ready_list,
				    sc->sc_nexus, chain);
				ecb = sc->sc_nexus = NULL;
			}
			sc->sc_state = ESPS_RESELECTED;
			goto states;
		}

		if (sc->sc_espintr == (ESPINTR_FC|ESPINTR_BS)) {
			ESPD(("STEP%d ", sc->sc_espstep));
			if (sc->sc_espstep == 0) {
				/* ATN: cannot happen */
				/* NATN: target did not assert message phase */
				sc->sc_state = ESPS_IDLE;
				goto states;
			} else if (sc->sc_espstep == 2) {
				/* BOTH: target did not assert command phase */
				sc->sc_state = ESPS_IDLE;
				goto states;
			} else if (sc->sc_espstep == 3) {
				/* BOTH: changed phase during cmd transfer */
				ESPD(("FF%d ", espr->espr_fflag & ESPFIFO_FF));
				if (espr->espr_fflag & ESPFIFO_FF) {
					sc->sc_state = ESPS_IDLE;
					goto states;
				}
			}

			espr->espr_cmd = ESPCMD_NOP;	/* unlatch fifo counter */
			if (sc->sc_phase != DATA_IN_PHASE &&
			    sc->sc_tinfo[ecb->xs->sc_link->target].offset == 0) {
				DELAY(1);
				espflush(sc);
			}

			ecb = sc->sc_nexus;
			if (!ecb)
				panic("esp: not nexus at sc->sc_nexus");
			sc_link = ecb->xs->sc_link;
			ti = &sc->sc_tinfo[sc_link->target];
			sc->sc_flags = 0;

			/* Clearly we succeeded at sending the message */
			sc->sc_msgpriq &= ~sc->sc_msgout;
			sc->sc_msgout = 0;
			if (sc->sc_msgpriq)
				espr->espr_cmd = ESPCMD_SETATN;

			sc->sc_dp = ecb->daddr;	/* implicit RESTOREDATAPOINTERS */
			sc->sc_dleft = ecb->dleft;
			ti->lubusy |= (1<<sc_link->lun);

			sc->sc_state = ESPS_NEXUS;
		}
		break;		/* handle the phase */
	case ESPS_SELECTSTOP:
		/*
		 * We wanted to select with multiple message bytes.
		 * As a result, we needed to use SELATNS. One
		 * message byte has been sent at this point. We need
		 * to send the other bytes, then the target will either
		 * switch to command phase to fetch the command or
		 * send us a message.
		 */
		ESPD(("SS%d ", sc->sc_espstep));

		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;
			goto states;
		}

		if (sc->sc_espintr & ESPINTR_RESEL) {
			/*
			 * If we were trying to select a target,
			 * push our command back into the ready list.
			 */
			if (sc->sc_state == ESPS_SELECTING) {
				TAILQ_INSERT_HEAD(&sc->ready_list,
				    sc->sc_nexus, chain);
				ecb = sc->sc_nexus = NULL;
			}
			sc->sc_state = ESPS_RESELECTED;
			goto states;
		}

		if (sc->sc_espintr & (ESPINTR_BS|ESPINTR_FC)) {
			int i;

			/*
			 * Shove the remainder of the message bytes
			 * into the fifo. After we send them, command
			 * phase will request the command...
			 */
			espflush(sc);
			for (i = 1; i < sc->sc_omlen; i++)
				espr->espr_fifo = sc->sc_omp[i];
			espr->espr_cmd = ESPCMD_TRANS;
			sc->sc_state = ESPS_MULTMSGEND;
			goto done;
		}
		goto done;
	case ESPS_MULTMSGEND:
		ESPD(("MME "));
		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;
			goto states;
		}

		sc->sc_msgpriq &= ~sc->sc_msgout;
		sc->sc_msgout = 0;

		ESPD(("F%d ", espr->espr_fflag & ESPFIFO_FF));
		if (espr->espr_fflag & ESPFIFO_FF) {
			espflush(sc);
			DELAY(1);
		}

		/*
		 * If there are more messages, re-assert
		 * ATN so that we will enter MSGOUT phase
		 * again. Also, pause till that interrupt is
		 * done to see if the phase changes.
		 */
		if (sc->sc_msgpriq)
			espr->espr_cmd = ESPCMD_SETATN;
		sc->sc_state = ESPS_NEXUS;
		goto states;
	case ESPS_RESELECTED:
		ESPD(("RS "));

		if (sc->sc_phase != MESSAGE_IN_PHASE) {
			printf("%s: target didn't identify\n",
			    sc->sc_dev.dv_xname);
			esp_init(sc, 1);
			goto done;
		}
		esp_msgin(sc);
		if (sc->sc_state != ESPS_NEXUS) {
			/* IDENTIFY failed?! */
			printf("%s: identify failed\n", sc->sc_dev.dv_xname);
			esp_init(sc, 1);
			goto done;
		}

		/*
		 * We will get an interrupt because of the ESPCMD_MSGOK
		 * interrupt inside esp_msgin()
		 */
		goto done;
	case ESPS_DOINGDMA:
		/*
		 * Call if DMA is still active. If dmaintr() has finished,
		 * delay a while to let things settle, and then re-read the
		 * phase.
		 * 
		 * XXX: should check ESPINTR_DIS
		 */
		if (sc->sc_espstat & ESPSTAT_PE) {
			printf("%s: SCSI bus parity error\n",
			    sc->sc_dev.dv_xname);
			esp_sched_msgout(SEND_INIT_DET_ERR);
			/* XXX: anything else to do? */
		}

		if (sc->sc_espintr & ESPINTR_BS) {
			/*
			 * ESP says we have changed phase, or have
			 * finished transferring all the bytes. It
			 * doesn't matter which case, either way we
			 * will let the phase determine what happens
			 * next.
			 */
			ESPD(("DOINGDMA:BS:%02x/%02x ", sc->sc_espstat,
			    espr->espr_tcl));
			if (sc->sc_dma->sc_active) {
				dmaintr(sc->sc_dma, 0);
				if (sc->sc_dma->sc_active)
					goto done;
			}
		} else {
			ESPD(("DOINGDMA:cont "));
			if (sc->sc_dma->sc_active) {
				dmaintr(sc->sc_dma, 1);
				if (sc->sc_dma->sc_active)
					goto done;
			}
		}

		/* finished a DMA transaction */
		sc->sc_flags = 0;

		ecb->daddr = sc->sc_dp;		/* implicit SAVEDATAPOINTERS */
		ecb->dleft = sc->sc_dleft;

		/*
		 * ESP100 strangeness -- need to fetch a new phase --
		 * why?
		 */
		if (sc->sc_rev==ESP100 &&
		    sc->sc_tinfo[ecb->xs->sc_link->target].offset) {
			sc->sc_espstat = espr->espr_stat & ESPSTAT_PHASE;
			sc->sc_phase = espphase(sc);
			/* XXX should check if sync under/overran */
		}
		sc->sc_state = ESPS_NEXUS;
		break;		/* handle the phase */
	case ESPS_DOINGMSGOUT:
		ESPD(("DMO "));
		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;	/* will cleanup */
			goto states;
		}

		if (sc->sc_espintr & ESPINTR_BS) {
			sc->sc_msgpriq &= ~sc->sc_msgout;
			sc->sc_msgout = 0;

			ESPD(("F%d ", espr->espr_fflag & ESPFIFO_FF));
			if (espr->espr_fflag & ESPFIFO_FF) {
				espflush(sc);
				DELAY(1);
			}

			/*
			 * If there are more messages, re-assert
			 * ATN so that we will enter MSGOUT phase
			 * again. Also, pause till that interrupt is
			 * done to see if the phase changes.
			 */
			if (sc->sc_msgpriq)
				espr->espr_cmd = ESPCMD_SETATN;
			break;
		}

#if 0
		if (sc->sc_espintr & ESPINTR_FC) {
			/*
			 * XXX: the book claims that an ESPINTR_BS is
			 * generated, not ESPINTR_FC. Check if that is
			 * true.
			 */
		}
#endif
		break;		/* handle the phase */
	case ESPS_DOINGMSGIN:
		/*
		 * We receive two interrupts per message byte here: one for
		 * the MSGOK command in esp_msgin(), and one for the TRANS
		 * command below.
		 */
		ESPD(("DMI "));

		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;	/* will cleanup */
			goto states;
		}

		if (sc->sc_espstat & ESPSTAT_PE) {
			printf("%s: SCSI bus parity error\n",
			    sc->sc_dev.dv_xname);
			esp_sched_msgout(SEND_PARITY_ERROR);
			espflush(sc);
			sc->sc_state = ESPS_NEXUS;
			/* XXX: are we missing some fifo handling? */
			goto done;	/* will be interrupted for MSGOUT */
		}

		/*
		 * The two interrupts we receive per byte will alternate
		 * between entering the following two if() statements.
		 * XXX: just incase, are these two if()'s in the correct
		 * order?
		 */
		if (sc->sc_espintr & ESPINTR_BS) {
			/*
			 * interrupted by ESPCMD_MSGOK in esp_msgin()
			 */
			if (sc->sc_phase == MESSAGE_IN_PHASE)
				espr->espr_cmd = ESPCMD_TRANS;
			else {
				ESPD(("PC "));
				/*
				 * XXX: The phase has been changed on us. 
				 * This should lead us to discard the
				 * current (incomplete) message, perhaps
				 * assert an error(?), and go handle whatever
				 * new phase we are now in.
				 */
				break;
			}
			goto done;
		}
		if (sc->sc_espintr & ESPINTR_FC) {
			/*
			 * interrupted by ESPCMD_TRANS a few lines above.
			 */
			esp_msgin(sc);
			goto done;
		}
		goto done;
	case ESPS_DOINGSTATUS:
		if (sc->sc_espintr & ESPINTR_DIS) {
			sc->sc_state = ESPS_NEXUS;	/* will cleanup */
			goto states;
		}

		if (sc->sc_espintr & (ESPINTR_FC|ESPINTR_BS)) {
			i = espgetbyte(sc, 0);
			if (i == -1) {
				printf("%s: ESPS_DOINGSTATUS fifo empty\n",
				    sc->sc_dev.dv_xname);
				/* XXX: how do we cleanup from this error? */
				goto done;
			}
			if (sc->sc_espstat & ESPSTAT_PE) {
				printf("%s: SCSI bus parity error\n",
				    sc->sc_dev.dv_xname);
				/*
				 * Can't tell what the real status should
				 * be, so force a later check.
				 */
				i = SCSI_CHECK;
			}
			ecb->stat = (u_char)i;
			ESPD(("status=%02x ", (u_char)i));

			if (sc->sc_espintr & ESPINTR_FC) {
				/*
				 * XXX assumes a single byte message --
				 * and what about parity errors and other
				 * possible botches inside esp_msgin?
				 */
				esp_msgin(sc);
				sc->sc_state = ESPS_NEXUS;
			} else
				sc->sc_state = ESPS_DOINGMSGIN;
			goto done;
		}
		goto done;
	case ESPS_EXPECTDISC:
		/*
		 * We were told to expect a disconnection interrupt.
		 * When we get it, we can go back to other work.
		 */
		ESPD(("ED "));
		if (sc->sc_espintr & ESPINTR_DIS) {
			espflush(sc);
			DELAY(1);
			espr->espr_cmd = ESPCMD_ENSEL; /* within 250ms of disconnect */
			sc->sc_state = ESPS_IDLE;
		}
		goto states;
	case ESPS_NEXUS:
		ESPD(("NX "));

		if (sc->sc_espintr & ESPINTR_DIS) {
			espflush(sc);
			DELAY(1);
			espr->espr_cmd = ESPCMD_ENSEL; /* within 250ms of disconnect */
			sc->sc_msgpriq = sc->sc_msgout = 0;

			ESPD(("DD:"));
			if (ecb->flags & ECB_DONE) {
				ESPD(("D "));
				/* successfully finished a transaction */
				untimeout(esp_timeout, ecb);
				esp_done(ecb);
			} else if (sc->sc_flags & ESPF_MAYDISCON) {
				ESPD(("M "));
				/* legal discon/recon is happening */
				TAILQ_INSERT_HEAD(&sc->nexus_list,
				    ecb, chain);
				sc->sc_nexus = NULL;
				sc->sc_state = ESPS_IDLE;
				sc->sc_flags &= ~ESPF_MAYDISCON;
				goto states;
			} else {
				ESPD(("U "));
				/* unexpected disconnection! fail. */
				ecb->xs->error = XS_TIMEOUT;
				untimeout(esp_timeout, ecb);
				esp_done(ecb);
			}
			goto done;
		}
		break;		/* handle the phase */
	default:
		printf("%s: unknown state %d\n",
		    sc->sc_dev.dv_xname, sc->sc_state);
		panic("cannot continue");
	}

	ecb = sc->sc_nexus;

	switch (sc->sc_phase) {
	case MESSAGE_OUT_PHASE:
		/*
		 * ATN was asserted, and the bus changed into MSGOUT phase.
		 * Send a message.
		 */
		esp_msgout(sc);				/* cause intr */
		sc->sc_state = ESPS_DOINGMSGOUT;
		break;
	case COMMAND_PHASE:
		/*
		 * Apparently we need to repeat the previous command.
		 */
		espflush(sc);
		DELAY(1);
		cmd = (u_char *)&ecb->cmd;

		ESPD(("CMD["));
		for (i = 0; i < ecb->clen; i++)
			ESPD(("%02x%c", cmd[i],
			    (i == ecb->clen-1) ? ']' : ' '));
		ESPD((" "));

		/* Now the command into the FIFO */
		for (i = 0; i < ecb->clen; i++)
			espr->espr_fifo = cmd[i];

		espr->espr_cmd = ESPCMD_TRANS;		/* cause intr */
		sc->sc_state = ESPS_NEXUS;
		break;
	case DATA_OUT_PHASE:
		/*
		 * We can feed the device the data now.
		 */
		espflush(sc);
		DELAY(1);
		dmastart(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft, 0,
		    ecb->xs->flags & SCSI_POLL);	/* cause intr */
		sc->sc_state = ESPS_DOINGDMA;
		break;
	case DATA_IN_PHASE:
		/*
		 * The device is ready to give us the data.
		 */
		dmadrain(sc->sc_dma);
		dmastart(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft, 1,
		    ecb->xs->flags & SCSI_POLL);	/* cause intr */
		sc->sc_state = ESPS_DOINGDMA;
		break;
	case MESSAGE_IN_PHASE:
		espflush(sc);
		DELAY(1);
		espr->espr_cmd = ESPCMD_TRANS;		/* cause intr */
		sc->sc_state = ESPS_DOINGMSGIN;
		break;
	case STATUS_PHASE:
		espflush(sc);
		DELAY(1);
		espr->espr_cmd = ESPCMD_ICCS;		/* cause intr */
		sc->sc_state = ESPS_DOINGSTATUS;
		break;
	default:
		printf("%s: bogus bus phase %d\n",
		    sc->sc_dev.dv_xname);
		break;
	}

done:
	if (sc->sc_dma->sc_rev == DMAREV_1 && dmaintrwas)
		dmaenintr(sc->sc_dma);
	sc->sc_intrcnt.ev_count++;
	return (1);
}
