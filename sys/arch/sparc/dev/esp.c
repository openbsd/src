/*	$NetBSD: esp.c,v 1.26 1995/09/14 20:38:53 pk Exp $ */

/*
 * Copyright (c) 1994 Peter Galbavy
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products 
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
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

#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espreg.h>
#include <sparc/dev/espvar.h>

int esp_debug = 0; /*ESP_SHOWPHASE|ESP_SHOWMISC|ESP_SHOWTRAC|ESP_SHOWCMDS;*/ 

/*static*/ void	espattach	__P((struct device *, struct device *, void *));
/*static*/ int	espmatch	__P((struct device *, void *, void *));
/*static*/ int  espprint	__P((void *, char *));
/*static*/ u_int	esp_adapter_info __P((struct esp_softc *));
/*static*/ void	espreadregs	__P((struct esp_softc *));
/*static*/ int	espgetbyte	__P((struct esp_softc *, u_char *));
/*static*/ void	espselect	__P((struct esp_softc *,
				     u_char, u_char, caddr_t, u_char));
/*static*/ void	esp_scsi_reset	__P((struct esp_softc *));
/*static*/ void	esp_reset	__P((struct esp_softc *));
/*static*/ void	esp_init	__P((struct esp_softc *, int));
/*static*/ int	esp_scsi_cmd	__P((struct scsi_xfer *));
/*static*/ int	esp_poll	__P((struct esp_softc *, struct ecb *));
/*static*/ int	espphase	__P((struct esp_softc *));
/*static*/ void	esp_sched	__P((struct esp_softc *));
/*static*/ void	esp_done	__P((struct ecb *));
/*static*/ void	esp_msgin	__P((struct esp_softc *));
/*static*/ void	esp_msgout	__P((struct esp_softc *));
/*static*/ int	espintr		__P((struct esp_softc *));
/*static*/ void	esp_timeout	__P((void *arg));

/* Linkup to the rest of the kernel */
struct cfdriver espcd = {
	NULL, "esp", espmatch, espattach,
	DV_DULL, sizeof(struct esp_softc)
};

struct scsi_adapter esp_switch = {
	esp_scsi_cmd,
	minphys,		/* no max at this level; handled by DMA code */
	NULL,
	NULL,
};

struct scsi_device esp_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

/*
 * Read the ESP registers, and save their contents for later use.
 * ESP_STAT, ESP_STEP & ESP_INTR are mostly zeroed out when reading
 * ESP_INTR - so make sure it is the last read.
 *
 * I think that (from reading the docs) most bits in these registers
 * only make sense when he DMA CSR has an interrupt showing. So I have
 * coded this to not do anything if there is no interrupt or error
 * pending.
 */
void
espreadregs(sc)
	struct esp_softc *sc;
{
	volatile caddr_t esp = sc->sc_reg;

	/* they mean nothing if the is no pending interrupt ??? */
	if (!(DMA_ISINTR(sc->sc_dma)))
		return;

	/* Only the stepo bits are of interest */
	sc->sc_espstep = esp[ESP_STEP] & ESPSTEP_MASK;
	sc->sc_espstat = esp[ESP_STAT];
	sc->sc_espintr = esp[ESP_INTR];

	ESP_MISC(("regs[intr=%02x,stat=%02x,step=%02x] ", sc->sc_espintr,
	    sc->sc_espstat, sc->sc_espstep));
}

/*
 * no error checking ouch
 */
int
espgetbyte(sc, v)
	struct esp_softc *sc;
	u_char *v;
{
	volatile caddr_t esp = sc->sc_reg;

	if (!(esp[ESP_FFLAG] & ESPFIFO_FF)) {
ESPCMD(sc, ESPCMD_FLUSH);
DELAY(1);
		ESPCMD(sc, ESPCMD_TRANS);
		while (!DMA_ISINTR(sc->sc_dma))
			DELAY(1);
		/*
		 * If we read something, then clear the outstanding
		 * interrupts
		 */
		espreadregs(sc);
		if (sc->sc_espintr & ESPINTR_ILL) /* Oh, why? */
			return -1;
	}
	if (!(esp[ESP_FFLAG] & ESPFIFO_FF)) {
		printf("error... ");
		return -1;
	}
	*v = esp[ESP_FIFO];
	return 0;
}

/*
 * Send a command to a target, set the driver state to ESP_SELECTING
 * and let the caller take care of the rest.
 *
 * Keeping this as a function allows me to say that this may be done
 * by DMA instead of programmed I/O soon.
 */
void
espselect(sc, target, lun, cmd, clen)
	struct esp_softc *sc;
	u_char target, lun;
	caddr_t cmd;
	u_char clen;
{
	volatile caddr_t esp = sc->sc_reg;
	int i;

	/*
	 * The docs say the target register is never reset, and I
	 * can't think of a better place to set it
	 */
	esp[ESP_SELID] = target;
	esp[ESP_SYNCOFF] = sc->sc_tinfo[target].offset;
	esp[ESP_SYNCTP] = 250 / sc->sc_tinfo[target].period;

	/*
	 * Who am I. This is where we tell the target that we are
	 * happy for it to disconnect etc.
	 */
#if 1
	if ((sc->sc_tinfo[target].flags & T_XXX) == 0)
#endif
		esp[ESP_FIFO] = ESP_MSG_IDENTIFY(lun);

	/* Now the command into the FIFO */
	for (i = 0; i < clen; i++)
		esp[ESP_FIFO] = *cmd++;

	/* And get the targets attention */
#if 1
	if ((sc->sc_tinfo[target].flags & T_XXX) == 0)
#endif
		ESPCMD(sc, ESPCMD_SELATN);
#if 1
	else
		ESPCMD(sc, ESPCMD_SELNATN);
#endif

	/* new state ESP_SELECTING */
	sc->sc_state = ESP_SELECTING;
}

int
espprint(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
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
	struct bootpath *bp;

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
		sc->sc_reg = (volatile caddr_t) ca->ca_ra.ra_vaddr;
	else {
		sc->sc_reg = (volatile caddr_t)
		    mapiodev(ca->ca_ra.ra_paddr, ca->ca_ra.ra_len, ca->ca_bustype);
	}

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

	/* gimme Mhz */
	sc->sc_freq /= 1000000;

	/*
	 * This is the value used to start sync negotiations
	 * For a 25Mhz clock, this gives us 40, or 160nS, or
	 * 6.25Mb/s. It is constant for each adapter.
	 *
	 * In turn, notice that the ESP register "SYNCTP" is
	 * = (250 / the negotiated period). It works, try it
	 * on paper.
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	/* 0 is actually 8, even though the register only has 3 bits */
	sc->sc_ccf = FREQTOCCF(sc->sc_freq) & 0x07;

	/* The value *must not* be == 1. Make it 2 */
	if (sc->sc_ccf == 1)
		sc->sc_ccf = 2;

	/*
	 * The recommended timeout is 250ms. This register is loaded
	 * with a value calculated as follows, from the docs:
	 *
	 *		(timout period) x (CLK frequency)
	 *	reg = -------------------------------------
	 *		 8192 x (Clock Conversion Factor)
	 *
	 * We have the CCF from above, so the sum is simple, and generally
	 * gives us a constant of 153. Try working out a few and see.
	 */
	sc->sc_timeout = ESP_DEF_TIMEOUT;

	/*
	 * find the DMA by poking around the dma device structures
	 *
	 * What happens here is that if the dma driver has not been
	 * configured, then this returns a NULL pointer. Then when the
	 * dma actually gets configured, it does the opposing test, and
	 * if the sc->sc_esp field in it's softc is NULL, then tries to
	 * find the matching esp driver.
	 *
	 */
	sc->sc_dma = ((struct dma_softc *)getdevunit("dma",
	    sc->sc_dev.dv_unit));

	/*
	 * and a back pointer to us, for DMA
	 */
	if (sc->sc_dma)
		sc->sc_dma->sc_esp = sc;

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the esp_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | ESPCFG1_PARENB;
	sc->sc_cfg2 = ESPCFG2_SCSI2 | ESPCFG2_RPE;
	sc->sc_cfg3 = ESPCFG3_CDB;
	sc->sc_reg[ESP_CFG2] = sc->sc_cfg2;

	if ((sc->sc_reg[ESP_CFG2] & ~ESPCFG2_RSVD) != (ESPCFG2_SCSI2 | ESPCFG2_RPE)) {
		printf(": ESP100");
		sc->sc_rev = ESP100;
	} else {
		sc->sc_cfg2 = 0;
		sc->sc_reg[ESP_CFG2] = sc->sc_cfg2;
		sc->sc_cfg3 = 0;
		sc->sc_reg[ESP_CFG3] = sc->sc_cfg3;
		sc->sc_cfg3 = 5;
		sc->sc_reg[ESP_CFG3] = sc->sc_cfg3;
		if (sc->sc_reg[ESP_CFG3] != 5) {
			printf(": ESP100A");
			sc->sc_rev = ESP100A;
		} else {
			sc->sc_cfg3 = 0;
			sc->sc_reg[ESP_CFG3] = sc->sc_cfg3;
			printf(": ESP200");
			sc->sc_rev = ESP200;
		}
	}

	sc->sc_state = 0;
	esp_init(sc, 1);

	printf(" %dMhz, target %d\n", sc->sc_freq, sc->sc_id);

	/* add me to the sbus structures */
	sc->sc_sd.sd_reset = (void *) esp_reset;
#if defined(SUN4C) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);
#endif /* SUN4C || SUN4M */

	/* and the interuppts */
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
	 * If the boot path is "esp" at the moment and it's me, then
	 * walk our pointer to the sub-device, ready for the config
	 * below.
	 */
	bp = ca->ca_ra.ra_bp;
	switch (ca->ca_bustype) {
	case BUS_SBUS:
		if (bp != NULL && strcmp(bp->name, "esp") == 0 &&
		    SAME_ESP(sc, bp, ca))
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
 * This is the generic esp reset function. It does not reset the SCSI bus,
 * only this controllers, but kills any on-going commands, and also stops
 * and resets the DMA.
 *
 * After reset, registers are loaded with the defaults from the attach
 * routine above.
 */
void 
esp_reset(sc)
	struct esp_softc *sc;
{
	volatile caddr_t esp = sc->sc_reg; 

	/* reset DMA first */
	DMA_RESET(sc->sc_dma);

	ESPCMD(sc, ESPCMD_RSTCHIP);		/* reset chip */
	ESPCMD(sc, ESPCMD_NOP);
	DELAY(500);

	/* do these backwards, and fall through */
	switch (sc->sc_rev) {
	case ESP200:
		esp[ESP_CFG3] = sc->sc_cfg3;
	case ESP100A:
		esp[ESP_CFG2] = sc->sc_cfg2;
	case ESP100:
		esp[ESP_CFG1] = sc->sc_cfg1;
		esp[ESP_CCF] = sc->sc_ccf;
		esp[ESP_SYNCOFF] = 0;
		esp[ESP_TIMEOUT] = sc->sc_timeout;
		break;
	default:
		printf("%s: unknown revision code, assuming ESP100\n",
		    sc->sc_dev.dv_xname);
		esp[ESP_CFG1] = sc->sc_cfg1;
		esp[ESP_CCF] = sc->sc_ccf;
		esp[ESP_SYNCOFF] = 0;
		esp[ESP_TIMEOUT] = sc->sc_timeout;
	}
}

/*
 * Reset the SCSI bus, but not the chip
 */
void
esp_scsi_reset(sc)
	struct esp_softc *sc;
{
	printf("esp: resetting SCSI bus\n");
	ESPCMD(sc, ESPCMD_RSTSCSI);
	DELAY(50);
}

/*
 * Initialize esp state machine
 */
void
esp_init(sc, doreset)
	struct esp_softc *sc;
	int doreset;
{
	struct ecb *ecb;
	int r;
	
	/*
	 * reset the chip to a known state
	 */
	esp_reset(sc);

	if (doreset) {
		ESPCMD(sc, ESPCMD_RSTSCSI);
		DELAY(50);
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
		for (r = 0; r < sizeof(sc->sc_ecb) / sizeof(*ecb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		sc->sc_state = ESP_IDLE;
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
	
	sc->sc_phase = sc->sc_prevphase = INVALID_PHASE;
	for (r = 0; r < 8; r++) {
		struct esp_tinfo *tp = &sc->sc_tinfo[r];

		tp->flags = DO_NEGOTIATE | NEED_TO_RESET;
		tp->period = sc->sc_minsync;
		tp->offset = ESP_SYNC_REQ_ACK_OFS;
	}
	sc->sc_state = ESP_IDLE;
	return;
}

/*
 * DRIVER FUNCTIONS CALLABLE FROM HIGHER LEVEL DRIVERS
 */

/*
 * Start a SCSI-command
 * This function is called by the higher level SCSI-driver to queue/run
 * SCSI-commands.
 */
int 
esp_scsi_cmd(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;
	struct ecb 	*ecb;
	int s, flags;
	
	ESP_TRACE(("esp_scsi_cmd\n"));
	ESP_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen, 
	    sc_link->target));

	flags = xs->flags;

	/* Get a esp command block */
	s = splbio();
	ecb = sc->free_list.tqh_first;
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
	}
	splx(s);
		
	if (ecb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		ESP_MISC(("TRY_AGAIN_LATER"));
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
	timeout(esp_timeout, ecb, (xs->timeout*hz)/1000);

	if (sc->sc_state == ESP_IDLE)
		esp_sched(sc);

	splx(s);

	if (flags & SCSI_POLL) {
		/* Not allowed to use interrupts, use polling instead */
		return esp_poll(sc, ecb);
	}

	ESP_MISC(("SUCCESSFULLY_QUEUED"));
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
	int count = xs->timeout * 10;

	ESP_TRACE(("esp_poll\n"));
	while (count) {
		if (DMA_ISINTR(sc->sc_dma)) {
			espintr(sc);
		}
		if (xs->flags & ITSDONE)
			break;
		DELAY(5);
		if (sc->sc_state == ESP_IDLE) {
			ESP_TRACE(("esp_poll: rescheduling"));
			esp_sched(sc);
		}
		count--;
	}

	if (count == 0) {
		ESP_MISC(("esp_poll: timeout"));
		esp_timeout((caddr_t)ecb);
	}

	return COMPLETE;
}


/*
 * LOW LEVEL SCSI UTILITIES
 */

/*
 * Determine the SCSI bus phase, return either a real SCSI bus phase
 * or some pseudo phase we use to detect certain exceptions.
 *
 * Notice that we do not read the live register on an ESP100. On the
 * ESP100A and above the FE (Feature Enable) bit in config 2 latches
 * the phase in the register so it is safe to read.
 */
int
espphase(sc)
	struct esp_softc *sc;
{
	
	if (sc->sc_espintr & ESPINTR_DIS)	/* Disconnected */
		return BUSFREE_PHASE;

	if (sc->sc_rev > ESP100)
		return (sc->sc_reg[ESP_STAT] & ESPSTAT_PHASE);

	return (sc->sc_espstat & ESPSTAT_PHASE);
}


/*
 * Schedule a scsi operation.  This has now been pulled out of the interrupt
 * handler so that we may call it from esp_scsi_cmd and esp_done.  This may
 * save us an unecessary interrupt just to get things going.  Should only be
 * called when state == ESP_IDLE and at bio pl.
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
			struct esp_tinfo *ti = &sc->sc_tinfo[t];

			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			sc->sc_nexus = ecb;
			sc->sc_flags = 0;
			sc->sc_prevphase = INVALID_PHASE;
			sc_link = ecb->xs->sc_link;
			espselect(sc, t, sc_link->lun, cmd, ecb->clen);
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
	if (xs->error == XS_NOERROR && !(ecb->flags & ECB_CHKSENSE)) {
		if ((ecb->stat & ST_MASK)==SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&ecb->cmd;
			ESP_MISC(("requesting sense "));
			/* First, save the return values */
			xs->resid = ecb->dleft;
			xs->status = ecb->stat;
			/* Next, setup a request sense command block */
			bzero(ss, sizeof(*ss));
			ss->opcode = REQUEST_SENSE;
			ss->byte2 = sc_link->lun << 5;
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
			if (sc->sc_nexus == ecb) {
				sc->sc_nexus = NULL;
				sc->sc_state = ESP_IDLE;
				esp_sched(sc);
			}
			return;
		}
	}
	
	if (xs->error == XS_NOERROR && (ecb->flags & ECB_CHKSENSE)) {
		xs->error = XS_SENSE;
	} else {
		xs->resid = ecb->dleft;
	}
	xs->flags |= ITSDONE;

#ifdef ESP_DEBUG
	if (esp_debug & ESP_SHOWMISC) {
		printf("err=0x%02x ",xs->error);
		if (xs->error == XS_SENSE)
			printf("sense=%2x\n", xs->sense.error_code);
	}
	if ((xs->resid || xs->error > XS_SENSE) && esp_debug & ESP_SHOWMISC) {
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
		sc->sc_state = ESP_IDLE;
		sc->sc_tinfo[sc_link->target].lubusy &= ~(1<<sc_link->lun);
		esp_sched(sc);
	} else if (sc->ready_list.tqh_last == &ecb->chain.tqe_next) {
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
	} else {
		register struct ecb *ecb2;
		for (ecb2 = sc->nexus_list.tqh_first; ecb2;
		    ecb2 = ecb2->chain.tqe_next)
			if (ecb2 == ecb) {
				TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
				sc->sc_tinfo[sc_link->target].lubusy
					&= ~(1<<sc_link->lun);
				break;
			}
		if (ecb2)
			;
		else if (ecb->chain.tqe_next) {
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
	scsi_done(xs);
	return;
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
	do {				\
		ESP_MISC(("esp_sched_msgout %d ", m)); \
		ESPCMD(sc, ESPCMD_SETATN);	\
		sc->sc_msgpriq |= (m);	\
	} while (0)

#define IS1BYTEMSG(m) (((m) != 1 && (m) < 0x20) || (m) & 0x80)
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
	volatile caddr_t esp = sc->sc_reg;
	int extlen;
	
	ESP_TRACE(("esp_msgin "));

	/* is something wrong ? */
	if (sc->sc_phase != MESSAGE_IN_PHASE) {
		printf("%s: not MESSAGE_IN_PHASE\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Prepare for a new message.  A message should (according
	 * to the SCSI standard) be transmitted in one single
	 * MESSAGE_IN_PHASE. If we have been in some other phase,
	 * then this is a new message.
	 */
	if (sc->sc_prevphase != MESSAGE_IN_PHASE) {
		sc->sc_flags &= ~ESP_DROP_MSGI;
		sc->sc_imlen = 0;
	}

	if (sc->sc_state == ESP_RESELECTED && sc->sc_imlen == 0) {
		/*
		 * Which target is reselecting us? (The ID bit really)
		 */
		(void)espgetbyte(sc, &sc->sc_selid);
		sc->sc_selid &= ~(1<<sc->sc_id);
		ESP_MISC(("selid=0x%2x ", sc->sc_selid));
	}

	for (;;) {
		/*
		 * If parity errors just dump everything on the floor
		 */
		if (sc->sc_espstat & ESPSTAT_PE) {
			esp_sched_msgout(SEND_PARITY_ERROR);
			sc->sc_flags |= ESP_DROP_MSGI;
		}

		/*
		 * If we're going to reject the message, don't bother storing
		 * the incoming bytes.  But still, we need to ACK them.
		 */
		if ((sc->sc_flags & ESP_DROP_MSGI) == 0) {
			if (espgetbyte(sc, &sc->sc_imess[sc->sc_imlen])) {
				/*
				 * XXX - hack alert.
				 * Apparently, the chip didn't grok a multibyte
				 * message from the target; set a flag that
				 * will cause selection w.o. ATN when we retry
				 * (after a SCSI reset).
				 * Set NOLUNS quirk as we won't be asking for
				 * a lun to identify.
				 */
				struct scsi_link *sc_link = sc->sc_nexus->xs->sc_link;
				printf("%s(%d,%d): "
					"MSGIN failed; trying alt selection\n",
					sc->sc_dev.dv_xname,
					sc_link->target, sc_link->lun);
				esp_sched_msgout(SEND_REJECT);
				sc->sc_tinfo[sc_link->target].flags |= T_XXX;
				sc_link->quirks |= SDEV_NOLUNS;
				if (sc->sc_state == ESP_HASNEXUS) {
					TAILQ_INSERT_HEAD(&sc->ready_list,
					    sc->sc_nexus, chain);
					sc->sc_nexus = NULL;
					sc->sc_tinfo[sc_link->target].lubusy
						&= ~(1<<sc_link->lun);
				}
				esp_scsi_reset(sc);
				sc->sc_state = ESP_IDLE;
				return;
			}
			ESP_MISC(("0x%02x ", sc->sc_imess[sc->sc_imlen]));
			if (sc->sc_imlen >= ESP_MAX_MSG_LEN) {
				esp_sched_msgout(SEND_REJECT);
				sc->sc_flags |= ESP_DROP_MSGI;
			} else {
				sc->sc_imlen++;
				/* 
				 * This testing is suboptimal, but most
				 * messages will be of the one byte variety, so
				 * it should not effect performance
				 * significantly.
				 */
				if (sc->sc_imlen == 1 && IS1BYTEMSG(sc->sc_imess[0]))
					break;
				if (sc->sc_imlen == 2 && IS2BYTEMSG(sc->sc_imess[0]))
					break;
				if (sc->sc_imlen >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
				    sc->sc_imlen == sc->sc_imess[1] + 2)
					break;
			}
		}
	}

	ESP_MISC(("gotmsg "));
	/*
	 * Now we should have a complete message (1 byte, 2 byte
	 * and moderately long extended messages).  We only handle
	 * extended messages which total length is shorter than
	 * ESP_MAX_MSG_LEN.  Longer messages will be amputated.
	 */
	if (sc->sc_state == ESP_HASNEXUS) {
		struct ecb *ecb = sc->sc_nexus;
		struct esp_tinfo *ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			ESP_MISC(("cmdcomplete "));
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: CMDCOMPLETE but no command?\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			if (sc->sc_dleft < 0) {
				struct scsi_link *sc_link = ecb->xs->sc_link;
				printf("esp: %d extra bytes from %d:%d\n",
				    -sc->sc_dleft, sc_link->target, sc_link->lun);
				sc->sc_dleft = 0;
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			ecb->xs->resid = ecb->dleft = sc->sc_dleft;
			sc->sc_flags |= ESP_BUSFREE_OK;
			return;

		case MSG_MESSAGE_REJECT:
			if (esp_debug & ESP_SHOWMISC)
				printf("%s: our msg rejected by target\n",
				    sc->sc_dev.dv_xname);
			if (sc->sc_flags & ESP_SYNCHNEGO) {
				ti->period = ti->offset = 0;
				sc->sc_flags &= ~ESP_SYNCHNEGO;
				ti->flags &= ~DO_NEGOTIATE;
			}
			/* Not all targets understand INITIATOR_DETECTED_ERR */
			if (sc->sc_msgout == SEND_INIT_DET_ERR)
				esp_sched_msgout(SEND_ABORT);
			ESPCMD(sc, ESPCMD_MSGOK);
			break;
		case MSG_NOOP:
			ESPCMD(sc, ESPCMD_MSGOK);
			break;
		case MSG_DISCONNECT:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: nothing to DISCONNECT\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			ti->dconns++;
			TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
			ecb = sc->sc_nexus = NULL;
			sc->sc_state = ESP_IDLE;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;
		case MSG_SAVEDATAPOINTER:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: no DATAPOINTERs to save\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			ecb->dleft = sc->sc_dleft;
			ecb->daddr = sc->sc_dp;
			break;
		case MSG_RESTOREPOINTERS:
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: no DATAPOINTERs to restore\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;
		case MSG_EXTENDED:
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				if (ti->offset == 0) {
					printf("%s: async\n", TARGETNAME(ecb));
					ti->offset = 0;
				} else if (ti->period > 124) {
					printf("%s: async\n", TARGETNAME(ecb));
					ti->offset = 0;
					esp_sched_msgout(SEND_SDTR);
				} else {	/* we are sync */
					printf("%s: sync rate %2fMb/s\n",
					    TARGETNAME(ecb),
					    sc->sc_freq/ti->period);
				}
				break;
			default: /* Extended messages we don't handle */
				ESPCMD(sc, ESPCMD_SETATN);
				break;
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			break;
		default:
			/* thanks for that ident... */
			if (!ESP_MSG_ISIDENT(sc->sc_imess[0])) {
				ESP_MISC(("unknown "));
				ESPCMD(sc, ESPCMD_SETATN);
			}
			ESPCMD(sc, ESPCMD_MSGOK);
			break;
		}
	} else if (sc->sc_state == ESP_RESELECTED) {
		struct scsi_link *sc_link;
		struct ecb *ecb;
		u_char lunit;
		if (ESP_MSG_ISIDENT(sc->sc_imess[0])) { 	/* Identify? */
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

			if (!ecb) {		/* Invalid reselection! */
				esp_sched_msgout(SEND_ABORT);
				printf("esp: invalid reselect (idbit=0x%2x)\n",
				    sc->sc_selid);
			} else {		/* Reestablish nexus */
				/*
				 * Setup driver data structures and
				 * do an implicit RESTORE POINTERS
				 */
				sc->sc_nexus = ecb;
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				sc->sc_tinfo[sc_link->target].lubusy
					|= (1<<sc_link->lun);
				esp[ESP_SYNCOFF] =
				    sc->sc_tinfo[sc_link->target].offset;
				esp[ESP_SYNCTP] =
				    250 / sc->sc_tinfo[sc_link->target].period;
				ESP_MISC(("... found ecb"));
				sc->sc_state = ESP_HASNEXUS;
			}
		} else {
			printf("%s: bogus reselect (no IDENTIFY) %0x2x\n",
			    sc->sc_dev.dv_xname, sc->sc_selid);
			esp_sched_msgout(SEND_DEV_RESET);
		}
	} else { /* Neither ESP_HASNEXUS nor ESP_RESELECTED! */
		printf("%s: unexpected message in; will send DEV_RESET\n",
		    sc->sc_dev.dv_xname);
		esp_sched_msgout(SEND_DEV_RESET);
	}
}


/*
 * Send the highest priority, scheduled message
 */
void
esp_msgout(sc)
	register struct esp_softc *sc;
{
	volatile caddr_t esp = sc->sc_reg;
	struct esp_tinfo *ti;
	struct ecb *ecb;

	if (sc->sc_prevphase != MESSAGE_OUT_PHASE) {
		/* Pick up highest priority message */
		sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_omlen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:	/* Also implies an IDENTIFY message */
			ecb = sc->sc_nexus;
			sc->sc_flags |= ESP_SYNCHNEGO;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			sc->sc_omess[1] = MSG_EXTENDED;
			sc->sc_omess[2] = 3;
			sc->sc_omess[3] = MSG_EXT_SDTR;
			sc->sc_omess[4] = ti->period;
			sc->sc_omess[5] = ti->offset;
			sc->sc_omlen = 6;
			/* Fallthrough! */
		case SEND_IDENTIFY:
			if (sc->sc_state != ESP_HASNEXUS) {
				printf("esp at line %d: no nexus", __LINE__);
				Debugger();
			}
			ecb = sc->sc_nexus;
			sc->sc_omess[0] = ESP_MSG_IDENTIFY(ecb->xs->sc_link->lun);
			break;
		case SEND_DEV_RESET:
			sc->sc_omess[0] = MSG_BUS_DEV_RESET;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omess[0] = MSG_PARITY_ERR;
			break;
		case SEND_ABORT:
			sc->sc_omess[0] = MSG_ABORT;
			sc->sc_flags |= ESP_BUSFREE_OK;
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

	/* (re)send the message */
	DMA_START(sc->sc_dma, &sc->sc_omp, &sc->sc_omlen, 0);
}

/*
 * This is the most critical part of the driver, and has to know
 * how to deal with *all* error conditions and phases from the SCSI
 * bus. If there are no errors and the DMA was active, then call the
 * DMA pseudo-interrupt handler. If this returns 1, then that was it
 * and we can return from here without further processing.
 *
 * Most of this needs verifying.
 */
int 
espintr(sc)
	register struct esp_softc *sc;
{
	register struct ecb *ecb = sc->sc_nexus;
	register struct scsi_link *sc_link;
	volatile caddr_t esp = sc->sc_reg;
	struct esp_tinfo *ti;
	caddr_t cmd;
	int loop;

	ESP_TRACE(("espintr\n"));
	
	/*
	 * I have made some (maybe seriously flawed) assumptions here,
	 * but basic testing (uncomment the printf() below), show that
	 * certainly something happens when this loop is here.
	 *
	 * The idea is that many of the SCSI operations take very little
	 * time, and going away and getting interrupted is too high an
	 * overhead to pay. For example, selecting, sending a message
	 * and command and then doing some work can be done in one "pass".
	 *
	 * The DELAY is not variable because I do not understand that the
	 * DELAY loop should be fixed-time regardless of CPU speed, but
	 * I am *assuming* that the faster SCSI processors get things done
	 * quicker (sending a command byte etc), and so there is no
	 * need to be too slow.
	 *
	 * This is a heuristic. It is 2 when at 20Mhz, 2 at 25Mhz and 1
	 * at 40Mhz. This needs testing.
	 */
	for (loop = 0; 1;loop++, DELAY(50/sc->sc_freq)) {
		/* a feeling of deja-vu */
		if (!DMA_ISINTR(sc->sc_dma) && loop)
			return 1;
#if 0
		if (loop)
			printf("*");
#endif

		/* and what do the registers say... */
		espreadregs(sc);

		if (sc->sc_state == ESP_IDLE) {
			printf("%s: stray interrupt\n", sc->sc_dev.dv_xname);
			return 0;
		}

		sc->sc_intrcnt.ev_count++;

		/*
		 * What phase are we in when we *entered* the
		 * interrupt handler ?
		 *
		 * On laster ESP chips (ESP236 and up) the FE (features
		 * enable) bit in config 2 latches the phase bits
		 * at each "command completion".
		 */
		sc->sc_phase = espphase(sc);

		/*
		 * At the moment, only a SCSI Bus Reset or Illegal
		 * Command are classed as errors. A diconnect is a
		 * valid condition, and we let the code check is the
		 * "ESP_BUSFREE_OK" flag was set before declaring it
		 * and error.
		 *
		 * Also, the status register tells us about "Gross
		 * Errors" and "Parity errors". Only the Gross Error
		 * is really bad, and the parity errors are dealt
		 * with later
		 *
		 * TODO
		 *	If there are too many parity error, go to slow
		 *	cable mode ?
		 */
#define ESPINTR_ERR (ESPINTR_SBR|ESPINTR_ILL)

		if (sc->sc_espintr & ESPINTR_ERR ||
		    sc->sc_espstat & ESPSTAT_GE) {
			/* SCSI Reset */
			if (sc->sc_espintr & ESPINTR_SBR) {
				if (esp[ESP_FFLAG] & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				printf("%s: SCSI bus reset\n",
				    sc->sc_dev.dv_xname);
				esp_init(sc, 0); /* Restart everything */
				return 1;
			}

			if (sc->sc_espstat & ESPSTAT_GE) {
				/* no target ? */
				if (esp[ESP_FFLAG] & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				DELAY(1);
				if (sc->sc_state == ESP_HASNEXUS) {
					ecb->xs->error = XS_DRIVER_STUFFUP;
					untimeout(esp_timeout, ecb);
					espreadregs(sc);
					esp_done(ecb);
				}
				return 1;
			}

			if (sc->sc_espintr & ESPINTR_ILL) {
				/* illegal command, out of sync ? */
				printf("%s: illegal command ",
				    sc->sc_dev.dv_xname);
				if (esp[ESP_FFLAG] & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				if (sc->sc_state == ESP_HASNEXUS) {
					ecb->xs->error = XS_DRIVER_STUFFUP;
					untimeout(esp_timeout, ecb);
					esp_done(ecb);
				}
				esp_reset(sc);		/* so start again */
				return 1;
			}
		}

		/*
		 * Call if DMA is active.
		 *
		 * If DMA_INTR returns true, then maybe go 'round the loop
		 * again in case there is no more DMA queued, but a phase
		 * change is expected.
		 */
		if (sc->sc_dma->sc_active && DMA_INTR(sc->sc_dma)) {
			/* If DMA active here, then go back to work... */
			if (sc->sc_dma->sc_active)
				return 1;
			DELAY(50/sc->sc_freq);
			continue;
		}

		/*
		 * check for less serious errors
		 */
		if (sc->sc_espstat & ESPSTAT_PE) {
			printf("esp: SCSI bus parity error\n");
			if (sc->sc_prevphase == MESSAGE_IN_PHASE)
				esp_sched_msgout(SEND_PARITY_ERROR);
			else 
				esp_sched_msgout(SEND_INIT_DET_ERR);
		}

		if (sc->sc_espintr & ESPINTR_DIS) {
			ESP_MISC(("disc "));
			if (esp[ESP_FFLAG] & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/*
			 * This command must (apparently) be issued within
			 * 250mS of a disconnect. So here you are...
			 */
			ESPCMD(sc, ESPCMD_ENSEL);
			if (sc->sc_state != ESP_IDLE) {
				/* it may be OK to disconnect */
				if (!(sc->sc_flags & ESP_BUSFREE_OK))
					ecb->xs->error = XS_TIMEOUT;
				untimeout(esp_timeout, ecb);
				esp_done(ecb);
				return 1;
			}
		}

		/* did a message go out OK ? This must be broken */
		if (sc->sc_prevphase == MESSAGE_OUT_PHASE &&
		    sc->sc_phase != MESSAGE_OUT_PHASE) {
			/* we have sent it */
			sc->sc_msgpriq &= ~sc->sc_msgout;
			sc->sc_msgout = 0;
		}

		switch (sc->sc_state) {

		case ESP_RESELECTED:
			/*
			 * we must be continuing a message ?
			 */
			if (sc->sc_phase != MESSAGE_IN_PHASE) {
				printf("%s: target didn't identify\n",
				    sc->sc_dev.dv_xname);
				esp_init(sc, 1);
				return 1;
			}
			esp_msgin(sc);
			if (sc->sc_state != ESP_HASNEXUS) {
				/* IDENTIFY fail?! */
				printf("%s: identify failed\n",
				    sc->sc_dev.dv_xname);
				esp_init(sc, 1);
				return 1;
			}
			break;

		case ESP_IDLE:
		case ESP_SELECTING:

			if (sc->sc_espintr & ESPINTR_RESEL) {
				/*
				 * If we're trying to select a
				 * target ourselves, push our command
				 * back into the ready list.
				 */
				if (sc->sc_state == ESP_SELECTING) {
					ESP_MISC(("backoff selector "));
					TAILQ_INSERT_HEAD(&sc->ready_list,
					    sc->sc_nexus, chain);
			sc->sc_tinfo[sc->sc_nexus->xs->sc_link->target].lubusy
						&= ~(1<<sc_link->lun);
					sc->sc_nexus = NULL;
				}
				sc->sc_state = ESP_RESELECTED;
				if (sc->sc_phase != MESSAGE_IN_PHASE) {
					/*
					 * Things are seriously fucked up.
					 * Pull the brakes, i.e. reset
					 */
					printf("%s: target didn't identify\n", 
					    sc->sc_dev.dv_xname);
					esp_init(sc, 1);
					return 1;
				}
				esp_msgin(sc);	/* Handle identify message */
				if (sc->sc_state != ESP_HASNEXUS) {
					/* IDENTIFY fail?! */
					printf("%s: identify failed\n",
					    sc->sc_dev.dv_xname);
					esp_init(sc, 1);
					return 1;
				}
				break;
			}

#define	ESPINTR_DONE	(ESPINTR_FC|ESPINTR_BS)
			if ((sc->sc_espintr & ESPINTR_DONE) == ESPINTR_DONE) {
				ecb = sc->sc_nexus;
				if (!ecb)
					panic("esp: not nexus at sc->sc_nexus");
				sc_link = ecb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->target];
				if (ecb->xs->flags & SCSI_RESET)
					sc->sc_msgpriq = SEND_DEV_RESET;
				else if (ti->flags & DO_NEGOTIATE)
					sc->sc_msgpriq =
					    SEND_IDENTIFY | SEND_SDTR;
				else
					sc->sc_msgpriq = SEND_IDENTIFY;
				sc->sc_state = ESP_HASNEXUS;
				sc->sc_flags = 0;
				sc->sc_prevphase = INVALID_PHASE;
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				ti->lubusy |= (1<<sc_link->lun);
				break;
			} else if (sc->sc_espintr & ESPINTR_FC) {
				if (sc->sc_espstep != ESPSTEP_DONE)
					if (esp[ESP_FFLAG] & ESPFIFO_FF) {
						ESPCMD(sc, ESPCMD_FLUSH);
						DELAY(1);
					}
			}
			/* We aren't done yet, but expect to be soon */
			DELAY(50/sc->sc_freq);
			continue;

		case ESP_HASNEXUS:
			break;
		default:
			panic("esp unknown state");
		}

		/*
		 * Driver is now in state ESP_HASNEXUS, i.e. we
		 * have a current command working the SCSI bus.
		 */
		cmd = (caddr_t) &ecb->cmd;
		if (sc->sc_state != ESP_HASNEXUS || ecb == NULL) {
			panic("esp no nexus");
		}

		switch (sc->sc_phase) {
		case MESSAGE_OUT_PHASE:
			ESP_PHASE(("MESSAGE_OUT_PHASE "));
			esp_msgout(sc);
			sc->sc_prevphase = MESSAGE_OUT_PHASE;
			break;
		case MESSAGE_IN_PHASE:
			ESP_PHASE(("MESSAGE_IN_PHASE "));
			esp_msgin(sc);
			sc->sc_prevphase = MESSAGE_IN_PHASE;
			break;
		case COMMAND_PHASE:
			/* well, this means send the command again */
			ESP_PHASE(("COMMAND_PHASE 0x%02x (%d) ",
				ecb->cmd.opcode, ecb->clen));
			if (esp[ESP_FFLAG] & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			espselect(sc, ecb->xs->sc_link->target,
			    ecb->xs->sc_link->lun, (caddr_t)&ecb->cmd,
			    ecb->clen);
			sc->sc_prevphase = COMMAND_PHASE;
			break;
		case DATA_OUT_PHASE:
			ESP_PHASE(("DATA_OUT_PHASE [%d] ",  sc->sc_dleft));
			ESPCMD(sc, ESPCMD_FLUSH);
			DMA_START(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft, 0);
			sc->sc_prevphase = DATA_OUT_PHASE;
			break;
		case DATA_IN_PHASE:
			ESP_PHASE(("DATA_IN_PHASE "));
			ESPCMD(sc, ESPCMD_FLUSH);
			DMA_DRAIN(sc->sc_dma);
			DMA_START(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft,
			    D_WRITE);
			sc->sc_prevphase = DATA_IN_PHASE;
			break;
		case STATUS_PHASE:
			ESP_PHASE(("STATUS_PHASE "));
			ESPCMD(sc, ESPCMD_ICCS);
			(void)espgetbyte(sc, &ecb->stat);
			ESP_PHASE(("0x%02x ", ecb->stat));
			sc->sc_prevphase = STATUS_PHASE;
			break;
		case INVALID_PHASE:
			break;
		case BUSFREE_PHASE:
			if (sc->sc_flags & ESP_BUSFREE_OK) {
				/*It's fun the 1st time.. */
				sc->sc_flags &= ~ESP_BUSFREE_OK;
			}
			break;
		default:
			panic("esp: bogus bus phase\n");
		}
	}
}

void
esp_timeout(arg)
	void *arg;
{
	int s = splbio();
	struct ecb *ecb = (struct ecb *)arg;
	struct esp_softc *sc;

	sc = ecb->xs->sc_link->adapter_softc;
	sc_print_addr(ecb->xs->sc_link);
	ecb->xs->error = XS_TIMEOUT;
	printf("timed out\n");

	esp_done(ecb);
	esp_reset(sc);
	splx(s);
}
