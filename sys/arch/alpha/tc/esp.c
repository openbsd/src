/*	$NetBSD: esp.c,v 1.6 1995/12/20 00:40:21 cgd Exp $	*/

/*
 * Copyright (c) 1994 Peter Galbavy
 * Copyright (c) 1995 Paul Kranenburg
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
#include <scsi/scsi_message.h>

#include <machine/cpu.h>
#ifdef SPARC_DRIVER
#include <machine/autoconf.h>
#include <sparc/dev/sbusvar.h>
#include <sparc/dev/dmareg.h>
#include <sparc/dev/dmavar.h>
#include <sparc/dev/espreg.h>
#include <sparc/dev/espvar.h>
#else
#include <dev/tc/tcvar.h>
#include <alpha/tc/tcdsvar.h>
#include <alpha/tc/espreg.h>
#include <alpha/tc/espvar.h>
#endif

int esp_debug = 0; /*ESP_SHOWPHASE|ESP_SHOWMISC|ESP_SHOWTRAC|ESP_SHOWCMDS;*/

/*static*/ void	espattach	__P((struct device *, struct device *, void *));
/*static*/ int	espmatch	__P((struct device *, void *, void *));
/*static*/ int	espprint	__P((void *, char *));
/*static*/ void	espreadregs	__P((struct esp_softc *));
/*static*/ void	espselect	__P((struct esp_softc *,
				     u_char, u_char, u_char *, u_char));
/*static*/ void	esp_scsi_reset	__P((struct esp_softc *));
/*static*/ void	esp_reset	__P((struct esp_softc *));
/*static*/ void	esp_init	__P((struct esp_softc *, int));
/*static*/ int	esp_scsi_cmd	__P((struct scsi_xfer *));
/*static*/ int	esp_poll	__P((struct esp_softc *, struct ecb *));
/*static*/ void	esp_sched	__P((struct esp_softc *));
/*static*/ void	esp_done	__P((struct ecb *));
/*static*/ void	esp_msgin	__P((struct esp_softc *));
/*static*/ void	esp_msgout	__P((struct esp_softc *));
/*static*/ int	espintr		__P((struct esp_softc *));
/*static*/ void	esp_timeout	__P((void *arg));
/*static*/ void	esp_abort	__P((struct esp_softc *, struct ecb *));

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

int
espprint(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("scsibus at %s", name);
	return UNCONF;
}

int
espmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
#ifdef SPARC_DRIVER
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);
	if (ca->ca_bustype == BUS_SBUS)
		return (1);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
#else
	struct tcdsdev_attach_args *tcdsdev = aux;

	if (strncmp(tcdsdev->tcdsda_modname, "PMAZ-AA ", TC_ROM_LLEN))
		return (0);
	return (!tc_badaddr(tcdsdev->tcdsda_addr));
#endif
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
espattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#ifdef SPARC_DRIVER
	register struct confargs *ca = aux;
#else
	register struct tcdsdev_attach_args *tcdsdev = aux;
#endif
	struct esp_softc *sc = (void *)self;
#ifdef SPARC_DRIVER
	struct bootpath *bp;
	int dmachild = strncmp(parent->dv_xname, "espdma", 6) == 0;
#endif

#ifdef SPARC_DRIVER
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
		sc->sc_reg = (volatile u_char *) ca->ca_ra.ra_vaddr;
	else {
		sc->sc_reg = (volatile u_char *)
		    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len, ca->ca_bustype);
	}
#else
	sc->sc_reg = (volatile u_int32_t *)tcdsdev->tcdsda_tc.tcda_addr;
	sc->sc_cookie = tcdsdev->tcdsda_tc.tcda_cookie;
	sc->sc_dma = tcdsdev->tcdsda_sc;

	printf(": address %x", sc->sc_reg);
	tcds_intr_establish(parent, sc->sc_cookie, TC_IPL_BIO,
	    (int (*)(void *))espintr, sc);
#endif

#ifdef SPARC_DRIVER
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
#else
	if (parent->dv_cfdata->cf_driver == &tcdscd) {
		sc->sc_id = tcdsdev->tcdsda_id;
		sc->sc_freq = tcdsdev->tcdsda_freq;
	} else {
		/* XXX */
		sc->sc_id = 7;
		sc->sc_freq = 24000000;
	}
#endif

	/* gimme Mhz */
	sc->sc_freq /= 1000000;

#ifdef SPARC_DRIVER
	if (dmachild) {
		sc->sc_dma = (struct dma_softc *)parent;
		sc->sc_dma->sc_esp = sc;
	} else {
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
		sc->sc_dma = (struct dma_softc *)
			getdevunit("dma", sc->sc_dev.dv_unit);

		/*
		 * and a back pointer to us, for DMA
		 */
		if (sc->sc_dma)
			sc->sc_dma->sc_esp = sc;
	}
#else
	sc->sc_dma->sc_esp = sc;		/* XXX */
#endif

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the esp_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | ESPCFG1_PARENB;
#ifdef SPARC_DRIVER
	sc->sc_cfg2 = ESPCFG2_SCSI2 | ESPCFG2_RPE;
	sc->sc_cfg3 = ESPCFG3_CDB;
	ESP_WRITE_REG(sc, ESP_CFG2, sc->sc_cfg2);

	if ((ESP_READ_REG(sc, ESP_CFG2) & ~ESPCFG2_RSVD) != (ESPCFG2_SCSI2 | ESPCFG2_RPE)) {
		printf(": ESP100");
		sc->sc_rev = ESP100;
	} else {
		sc->sc_cfg2 = ESPCFG2_SCSI2 | ESPCFG2_FE;
		ESP_WRITE_REG(sc, ESP_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (ESPCFG3_CDB | ESPCFG3_FCLK);
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
		if (ESP_READ_REG(sc, ESP_CFG3) != (ESPCFG3_CDB | ESPCFG3_FCLK)) {
			printf(": ESP100A");
			sc->sc_rev = ESP100A;
		} else {
			sc->sc_cfg3 = 0;
			ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
			printf(": ESP200");
			sc->sc_rev = ESP200;
		}
	}
#else
	sc->sc_cfg2 = ESPCFG2_SCSI2;
	sc->sc_cfg3 = 0x4;		/* Save residual byte. XXX??? */
	printf(": NCR53C94");
	sc->sc_rev = NCR53C94;
#endif

	/*
	 * This is the value used to start sync negotiations
	 * Note that the ESP register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

#ifdef SPARC_DRIVER
	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits
	 * in config register 3...
	 */
	switch (sc->sc_rev) {
	case ESP100:
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;
	case ESP100A:
		sc->sc_minsync = esp_cpb2stp(sc, 5); /* Min clocks/byte is 5 */
		break;
	case ESP200:
		/* XXX - do actually set FAST* bits */
	}
#endif

	sc->sc_ccf = FREQTOCCF(sc->sc_freq);

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
	 * Since CCF has a linear relation to CLK, this generally computes
	 * to the constant of 153.
	 */
	sc->sc_timeout = ((250 * 1000) * sc->sc_freq) / (8192 * sc->sc_ccf);

	/* CCF register only has 3 bits; 0 is actually 8 */
	sc->sc_ccf &= 7;

	/* Reset state & bus */
	sc->sc_state = 0;
	esp_init(sc, 1);

	printf(" %dMhz, target %d\n", sc->sc_freq, sc->sc_id);

#ifdef SPARC_DRIVER
	/* add me to the sbus structures */
	sc->sc_sd.sd_reset = (void *) esp_reset;
#if defined(SUN4C) || defined(SUN4M)
	if (ca->ca_bustype == BUS_SBUS) {
		if (dmachild)
			sbus_establish(&sc->sc_sd, sc->sc_dev.dv_parent);
		else
			sbus_establish(&sc->sc_sd, &sc->sc_dev);
	}
#endif /* SUN4C || SUN4M */
#endif

#ifdef SPARC_DRIVER
	/* and the interuppts */
	sc->sc_ih.ih_fun = (void *) espintr;
	sc->sc_ih.ih_arg = sc;
	intr_establish(sc->sc_pri, &sc->sc_ih);
#endif
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
#ifdef SPARC_DRIVER
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
#endif

	/*
	 * Now try to attach all the sub-devices
	 */
	config_found(self, &sc->sc_link, espprint);

#ifdef SPARC_DRIVER
	bootpath_store(1, NULL);
#endif
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

	/* reset DMA first */
	DMA_RESET(sc->sc_dma);

	/* reset SCSI chip */
	ESPCMD(sc, ESPCMD_RSTCHIP);
	ESPCMD(sc, ESPCMD_NOP);
	DELAY(500);

	/* do these backwards, and fall through */
	switch (sc->sc_rev) {
#ifndef SPARC_DRIVER
	case NCR53C94:
#endif
	case ESP200:
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
	case ESP100A:
		ESP_WRITE_REG(sc, ESP_CFG2, sc->sc_cfg2);
	case ESP100:
		ESP_WRITE_REG(sc, ESP_CFG1, sc->sc_cfg1);
		ESP_WRITE_REG(sc, ESP_CCF, sc->sc_ccf);
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_TIMEOUT, sc->sc_timeout);
		break;
	default:
		printf("%s: unknown revision code, assuming ESP100\n",
		    sc->sc_dev.dv_xname);
		ESP_WRITE_REG(sc, ESP_CFG1, sc->sc_cfg1);
		ESP_WRITE_REG(sc, ESP_CCF, sc->sc_ccf);
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_TIMEOUT, sc->sc_timeout);
	}
}

/*
 * Reset the SCSI bus, but not the chip
 */
void
esp_scsi_reset(sc)
	struct esp_softc *sc;
{
#ifdef SPARC_DRIVER
	/* stop DMA first, as the chip will return to Bus Free phase */
	DMACSR(sc->sc_dma) &= ~D_EN_DMA;
#else
	/*
	 * XXX STOP DMA FIRST
	 */
#endif

	printf("esp: resetting SCSI bus\n");
	ESPCMD(sc, ESPCMD_RSTSCSI);
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

	ESP_TRACE(("[ESP_INIT(%d)] ", doreset));

	if (sc->sc_state == 0) {	/* First time through */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		ecb = sc->sc_ecb;
		bzero(ecb, sizeof(sc->sc_ecb));
		for (r = 0; r < sizeof(sc->sc_ecb) / sizeof(*ecb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QFREE);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		sc->sc_flags |= ESP_ABORTING;
		sc->sc_state = ESP_IDLE;
		if (sc->sc_nexus != NULL) {
			sc->sc_nexus->xs->error = XS_TIMEOUT;
			esp_done(sc->sc_nexus);
		}
		sc->sc_nexus = NULL;
		while ((ecb = sc->nexus_list.tqh_first) != NULL) {
			ecb->xs->error = XS_TIMEOUT;
			esp_done(ecb);
		}
	}

	/*
	 * reset the chip to a known state
	 */
	esp_reset(sc);

	sc->sc_phase = sc->sc_prevphase = INVALID_PHASE;
	for (r = 0; r < 8; r++) {
		struct esp_tinfo *tp = &sc->sc_tinfo[r];

		tp->flags = (sc->sc_minsync ? T_NEGOTIATE : 0) |
				T_NEED_TO_RESET | (tp->flags & T_XXX);
		tp->period = sc->sc_minsync;
		tp->offset = 0;
	}
	sc->sc_flags &= ~ESP_ABORTING;

	if (doreset) {
		sc->sc_state = ESP_SBR;
		ESPCMD(sc, ESPCMD_RSTSCSI);
		return;
	}
	
	sc->sc_state = ESP_IDLE;
	esp_sched(sc);
	return;
}

/*
 * Read the ESP registers, and save their contents for later use.
 * ESP_STAT, ESP_STEP & ESP_INTR are mostly zeroed out when reading
 * ESP_INTR - so make sure it is the last read.
 *
 * I think that (from reading the docs) most bits in these registers
 * only make sense when he DMA CSR has an interrupt showing. Call only
 * if an interrupt is pending.
 */
void
espreadregs(sc)
	struct esp_softc *sc;
{

	sc->sc_espstat = ESP_READ_REG(sc, ESP_STAT);
	/* Only the stepo bits are of interest */
	sc->sc_espstep = ESP_READ_REG(sc, ESP_STEP) & ESPSTEP_MASK;
	sc->sc_espintr = ESP_READ_REG(sc, ESP_INTR);
#ifndef SPARC_DRIVER
	/* Clear the TCDS interrupt bit. */
	(void)tcds_scsi_isintr(sc->sc_dma, 1);
#endif

	/*
	 * Determine the SCSI bus phase, return either a real SCSI bus phase
	 * or some pseudo phase we use to detect certain exceptions.
	 */

	sc->sc_phase = (sc->sc_espintr & ESPINTR_DIS)
			? /* Disconnected */ BUSFREE_PHASE
			: sc->sc_espstat & ESPSTAT_PHASE;

	ESP_MISC(("regs[intr=%02x,stat=%02x,step=%02x] ",
		sc->sc_espintr, sc->sc_espstat, sc->sc_espstep));
}

/*
 * Convert Synchronous Transfer Period to chip register Clock Per Byte value.
 */
esp_stp2cpb(sc, period)
	struct esp_softc *sc;
{
	int v;
	v = (sc->sc_freq * period) / 250;
	if (esp_cpb2stp(sc, v) < period)
		/* Correct round-down error */
		v++;
	return v;
}

/*
 * Convert chip register Clock Per Byte value to Synchronous Transfer Period.
 */
esp_cpb2stp(sc, cpb)
	struct esp_softc *sc;
	int cpb;
{
	return ((250 * cpb) / sc->sc_freq);
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
	u_char *cmd;
	u_char clen;
{
	struct esp_tinfo *ti = &sc->sc_tinfo[target];
	int i;

	ESP_TRACE(("[espselect(t%d,l%d,cmd:%x)] ", target, lun, *(u_char *)cmd));

	/* new state ESP_SELECTING */
	sc->sc_state = ESP_SELECTING;

	/*
	 * The docs say the target register is never reset, and I
	 * can't think of a better place to set it
	 */
	ESP_WRITE_REG(sc, ESP_SELID, target);
	if (ti->flags & T_SYNCMODE) {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, ti->offset);
		ESP_WRITE_REG(sc, ESP_SYNCTP, esp_stp2cpb(sc, ti->period));
	} else {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_SYNCTP, 0);
	}

	/*
	 * Who am I. This is where we tell the target that we are
	 * happy for it to disconnect etc.
	 */
	ESP_WRITE_REG(sc, ESP_FIFO, MSG_IDENTIFY(lun, 1));

	if (ti->flags & T_NEGOTIATE) {
		/* Arbitrate, select and stop after IDENTIFY message */
		ESPCMD(sc, ESPCMD_SELATNS);
		return;
	}

	/* Now the command into the FIFO */
	for (i = 0; i < clen; i++)
		ESP_WRITE_REG(sc, ESP_FIFO, *cmd++);

	/* And get the targets attention */
	ESPCMD(sc, ESPCMD_SELATN);
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
	
	ESP_TRACE(("[esp_scsi_cmd] "));
	ESP_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen, 
	    sc_link->target));

	flags = xs->flags;

	/* Get a esp command block */
	s = splbio();
	ecb = sc->free_list.tqh_first;
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
		ECB_SETQ(ecb, ECB_QNONE);
	}
	splx(s);
		
	if (ecb == NULL) {
		ESP_MISC(("TRY_AGAIN_LATER"));
		return TRY_AGAIN_LATER;
	}

	/* Initialize ecb */
	ecb->xs = xs;
	bcopy(xs->cmd, &ecb->cmd, xs->cmdlen);
	ecb->clen = xs->cmdlen;
	ecb->daddr = xs->data;
	ecb->dleft = xs->datalen;
	ecb->stat = 0;
	
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->ready_list, ecb, chain);
	ECB_SETQ(ecb, ECB_QREADY);
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
	int count = xs->timeout * 100;

	ESP_TRACE(("[esp_poll] "));
	while (count) {
		if (DMA_ISINTR(sc->sc_dma)) {
			espintr(sc);
		}
#if alternatively
		if (ESP_READ_REG(sc, ESP_STAT) & ESPSTAT_INT)
			espintr(sc);
#endif
		if (xs->flags & ITSDONE)
			break;
		DELAY(10);
		if (sc->sc_state == ESP_IDLE) {
			ESP_TRACE(("[esp_poll: rescheduling] "));
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
	
	ESP_TRACE(("[esp_sched] "));
	if (sc->sc_state != ESP_IDLE)
		panic("esp_sched: not IDLE (state=%d)", sc->sc_state);

	if (sc->sc_flags & ESP_ABORTING)
		return;

	/*
	 * Find first ecb in ready queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	for (ecb = sc->ready_list.tqh_first; ecb; ecb = ecb->chain.tqe_next) {
		sc_link = ecb->xs->sc_link;
		t = sc_link->target;
		if (!(sc->sc_tinfo[t].lubusy & (1 << sc_link->lun))) {
			struct esp_tinfo *ti = &sc->sc_tinfo[t];

			if ((ecb->flags & ECB_QBITS) != ECB_QREADY)
				panic("esp: busy entry on ready list");
			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QNONE);
			sc->sc_nexus = ecb;
			sc->sc_flags = 0;
			sc->sc_prevphase = INVALID_PHASE;
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			ti->lubusy |= (1<<sc_link->lun);
/*XXX*/if (sc->sc_msgpriq) {
	printf("esp: message queue not empty: %x!\n", sc->sc_msgpriq);
}
/*XXX*/sc->sc_msgpriq = sc->sc_msgout = 0;
			espselect(sc, t, sc_link->lun,
				  (u_char *)&ecb->cmd, ecb->clen);
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
	struct esp_tinfo *ti = &sc->sc_tinfo[sc_link->target];

	ESP_TRACE(("[esp_done(error:%x)] ", xs->error));

	untimeout(esp_timeout, ecb);

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the 
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and 
	 * push the command back into the ready queue *before* any other 
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if ((ecb->flags & ECB_ABORTED) != 0) {
			xs->error = XS_TIMEOUT;
		} else if ((ecb->flags & ECB_CHKSENSE) != 0) {
			xs->error = XS_SENSE;
		} else if ((ecb->stat & ST_MASK) == SCSI_CHECK) {
			struct scsi_sense *ss = (void *)&ecb->cmd;
			ESP_MISC(("requesting sense "));
			/* First, save the return values */
			xs->resid = ecb->dleft;
			xs->status = ecb->stat;
			/* Next, setup a request sense command block */
			bzero(ss, sizeof(*ss));
			ss->opcode = REQUEST_SENSE;
			/*ss->byte2 = sc_link->lun << 5;*/
			ss->length = sizeof(struct scsi_sense_data);
			ecb->clen = sizeof(*ss);
			ecb->daddr = (char *)&xs->sense;
			ecb->dleft = sizeof(struct scsi_sense_data);
			ecb->flags |= ECB_CHKSENSE;
/*XXX - must take off queue here */
			TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
			ECB_SETQ(ecb, ECB_QREADY);
			ti->lubusy &= ~(1<<sc_link->lun);
			ti->senses++;
			timeout(esp_timeout, ecb, (xs->timeout*hz)/1000);
			if (sc->sc_nexus == ecb) {
				sc->sc_nexus = NULL;
				sc->sc_state = ESP_IDLE;
				esp_sched(sc);
			}
			return;
		} else {
			xs->resid = ecb->dleft;
		}
	}

	xs->flags |= ITSDONE;

#ifdef ESP_DEBUG
	if (esp_debug & ESP_SHOWMISC) {
		printf("err=0x%02x ",xs->error);
		if (xs->error == XS_SENSE) {
			printf("sense=%2x; ", xs->sense.error_code);
			if (xs->sense.error_code == 0x70)
				printf("extcode: %x; ", xs->sense.extended_flags);
		}
	}
	if ((xs->resid || xs->error > XS_SENSE) && esp_debug & ESP_SHOWMISC) {
		if (xs->resid)
			printf("esp_done: resid=%d\n", xs->resid);
		if (xs->error)
			printf("esp_done: error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ECB from whatever queue it's on.
	 */
	switch (ecb->flags & ECB_QBITS) {
	case ECB_QNONE:
		if (ecb != sc->sc_nexus) {
			panic("%s: floating ecb", sc->sc_dev.dv_xname);
		}
		sc->sc_state = ESP_IDLE;
		ti->lubusy &= ~(1<<sc_link->lun);
		esp_sched(sc);
		break;
	case ECB_QREADY:
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
		break;
	case ECB_QNEXUS:
		TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
		ti->lubusy &= ~(1<<sc_link->lun);
		break;
	case ECB_QFREE:
		panic("%s: busy ecb on free list", sc->sc_dev.dv_xname);
		break;
	}

	/* Put it on the free list, and clear flags. */
	TAILQ_INSERT_HEAD(&sc->free_list, ecb, chain);
	ecb->flags = ECB_QFREE;

	ti->cmds++;
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
	register int v;
	
	ESP_TRACE(("[esp_msgin(curmsglen:%d)] ", sc->sc_imlen));

	if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) == 0) {
		printf("%s: msgin: no msg byte available\n",
			sc->sc_dev.dv_xname);
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

	v = ESP_READ_REG(sc, ESP_FIFO);
	ESP_MISC(("<msgbyte:0x%02x>", v));

#if 0
	if (sc->sc_state == ESP_RESELECTED && sc->sc_imlen == 0) {
		/*
		 * Which target is reselecting us? (The ID bit really)
		 */
		sc->sc_selid = v;
		sc->sc_selid &= ~(1<<sc->sc_id);
		ESP_MISC(("selid=0x%2x ", sc->sc_selid));
		return;
	}
#endif

	sc->sc_imess[sc->sc_imlen] = v;

	/*
	 * If we're going to reject the message, don't bother storing
	 * the incoming bytes.  But still, we need to ACK them.
	 */

	if ((sc->sc_flags & ESP_DROP_MSGI)) {
		ESPCMD(sc, ESPCMD_SETATN);
		ESPCMD(sc, ESPCMD_MSGOK);
		printf("<dropping msg byte %x>",
			sc->sc_imess[sc->sc_imlen]);
		return;
	}

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
			goto gotit;
		if (sc->sc_imlen == 2 && IS2BYTEMSG(sc->sc_imess[0]))
			goto gotit;
		if (sc->sc_imlen >= 3 && ISEXTMSG(sc->sc_imess[0]) &&
		    sc->sc_imlen == sc->sc_imess[1] + 2)
			goto gotit;
	}
	/* Ack what we have so far */
	ESPCMD(sc, ESPCMD_MSGOK);
	return;

gotit:
	ESP_MSGS(("gotmsg(%x)", sc->sc_imess[0]));
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
			ESP_MSGS(("cmdcomplete "));
			if (sc->sc_dleft < 0) {
				struct scsi_link *sc_link = ecb->xs->sc_link;
				printf("esp: %d extra bytes from %d:%d\n",
					-sc->sc_dleft,
					sc_link->target, sc_link->lun);
				sc->sc_dleft = 0;
			}
			ecb->xs->resid = ecb->dleft = sc->sc_dleft;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;

		case MSG_MESSAGE_REJECT:
			if (esp_debug & ESP_SHOWMSGS)
				printf("%s: our msg rejected by target\n",
				    sc->sc_dev.dv_xname);
#if 0 /* XXX - must remember last message */
printf("<<target%d: MSG_MESSAGE_REJECT>>", ecb->xs->sc_link->target);
ti->period = ti->offset = 0;
sc->sc_flags &= ~ESP_SYNCHNEGO;
ti->flags &= ~T_NEGOTIATE;
#endif
			if (sc->sc_flags & ESP_SYNCHNEGO) {
				ti->period = ti->offset = 0;
				sc->sc_flags &= ~ESP_SYNCHNEGO;
				ti->flags &= ~T_NEGOTIATE;
			}
			/* Not all targets understand INITIATOR_DETECTED_ERR */
			if (sc->sc_msgout == SEND_INIT_DET_ERR)
				esp_sched_msgout(SEND_ABORT);
			break;
		case MSG_NOOP:
			ESP_MSGS(("noop "));
			break;
		case MSG_DISCONNECT:
			ESP_MSGS(("disconnect "));
			ti->dconns++;
			sc->sc_flags |= ESP_DISCON;
			sc->sc_flags |= ESP_BUSFREE_OK;
			break;
		case MSG_SAVEDATAPOINTER:
			ESP_MSGS(("save datapointer "));
			ecb->dleft = sc->sc_dleft;
			ecb->daddr = sc->sc_dp;
			break;
		case MSG_RESTOREPOINTERS:
			ESP_MSGS(("restore datapointer "));
			if (!ecb) {
				esp_sched_msgout(SEND_ABORT);
				printf("%s: no DATAPOINTERs to restore\n",
				    sc->sc_dev.dv_xname);
				break;
			}
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;
		case MSG_PARITY_ERROR:
printf("<<esp:target%d: MSG_PARITY_ERROR>>", ecb->xs->sc_link->target);
			break;
		case MSG_EXTENDED:
			ESP_MSGS(("extended(%x) ", sc->sc_imess[2]));
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				ESP_MSGS(("SDTR period %d, offset %d ",
					sc->sc_imess[3], sc->sc_imess[4]));
	/*XXX*/if (ti->flags & T_XXX) {
		printf("%s:%d: rejecting SDTR\n",
			"esp", ecb->xs->sc_link->target);
		sc->sc_flags &= ~ESP_SYNCHNEGO;
		esp_sched_msgout(SEND_REJECT);
		break;
	}
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				if (sc->sc_minsync == 0) {
					/* We won't do synch */
					ti->offset = 0;
					esp_sched_msgout(SEND_SDTR);
				} else if (ti->offset == 0) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					ti->offset = 0;
					sc->sc_flags &= ~ESP_SYNCHNEGO;
				} else if (ti->period > 124) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					ti->offset = 0;
					esp_sched_msgout(SEND_SDTR);
				} else {
					int r = 250/ti->period;
					int s = (100*250)/ti->period - 100*r;
					int p;
					p =  esp_stp2cpb(sc, ti->period);
					ti->period = esp_cpb2stp(sc, p);
					if ((sc->sc_flags&ESP_SYNCHNEGO) == 0) {
						/* Target initiated negotiation */
						sc->sc_flags |= ESP_SYNCHNEGO;
						if (ti->flags & T_SYNCMODE) {
							sc_print_addr(ecb->xs->sc_link);
#ifdef ESP_DEBUG
							printf("%s:%d: dropping out of sync mode\n");
#endif
						}
						ti->flags &= ~T_SYNCMODE;
						ESP_WRITE_REG(sc,
						    ESP_SYNCOFF, 0);
						esp_sched_msgout(SEND_SDTR);
					} else {
						/* we are sync */
						sc->sc_flags &= ~ESP_SYNCHNEGO;
						ESP_WRITE_REG(sc,
						    ESP_SYNCOFF, ti->offset);
						ESP_WRITE_REG(sc,
						    ESP_SYNCTP, p);
						ti->flags |= T_SYNCMODE;
						sc_print_addr(ecb->xs->sc_link);
#ifdef ESP_DEBUG
						printf("max sync "
						       "rate %d.%02dMb/s\n",
						        r, s);
#endif
					}
				}
				ti->flags &= ~T_NEGOTIATE;
				break;
			default: /* Extended messages we don't handle */
				ESPCMD(sc, ESPCMD_SETATN);
				break;
			}
			break;
		default:
			ESP_MSGS(("ident "));
			/* thanks for that ident... */
			if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
				ESP_MISC(("unknown "));
printf("%s: unimplemented message: %d\n", sc->sc_dev.dv_xname, sc->sc_imess[0]);
				ESPCMD(sc, ESPCMD_SETATN);
			}
			break;
		}
	} else if (sc->sc_state == ESP_RESELECTED) {
		struct scsi_link *sc_link;
		struct ecb *ecb;
		struct esp_tinfo *ti;
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
					ECB_SETQ(ecb, ECB_QNONE);
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
				ti = &sc->sc_tinfo[sc_link->target];
				sc->sc_nexus = ecb;
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				sc->sc_tinfo[sc_link->target].lubusy
					|= (1<<sc_link->lun);
				ESP_WRITE_REG(sc, ESP_SYNCOFF, ti->offset);
				ESP_WRITE_REG(sc, ESP_SYNCTP,
				    esp_stp2cpb(sc, ti->period));
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

	/* Ack last message byte */
	ESPCMD(sc, ESPCMD_MSGOK);

	/* Done, reset message pointer. */
	sc->sc_flags &= ~ESP_DROP_MSGI;
	sc->sc_imlen = 0;
}


/*
 * Send the highest priority, scheduled message
 */
void
esp_msgout(sc)
	register struct esp_softc *sc;
{
	struct esp_tinfo *ti;
	struct ecb *ecb;

	ESP_TRACE(("[esp_msgout(priq:%x, prevphase:%x)]", sc->sc_msgpriq, sc->sc_prevphase));

	if (sc->sc_prevphase != MESSAGE_OUT_PHASE) {
		/* Pick up highest priority message */
		sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_omlen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			ecb = sc->sc_nexus;
			sc->sc_flags |= ESP_SYNCHNEGO;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			sc->sc_omess[0] = MSG_EXTENDED;
			sc->sc_omess[1] = 3;
			sc->sc_omess[2] = MSG_EXT_SDTR;
			sc->sc_omess[3] = ti->period;
			sc->sc_omess[4] = ti->offset;
			sc->sc_omlen = 5;
			break;
		case SEND_IDENTIFY:
			if (sc->sc_state != ESP_HASNEXUS) {
				printf("esp at line %d: no nexus", __LINE__);
			}
			ecb = sc->sc_nexus;
			sc->sc_omess[0] = MSG_IDENTIFY(ecb->xs->sc_link->lun,0);
			break;
		case SEND_DEV_RESET:
			sc->sc_omess[0] = MSG_BUS_DEV_RESET;
			sc->sc_flags |= ESP_BUSFREE_OK;
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			ti->flags &= ~T_SYNCMODE;
			ti->flags |= T_NEGOTIATE;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omess[0] = MSG_PARITY_ERROR;
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

#if 1
	/* (re)send the message */
	DMA_START(sc->sc_dma, &sc->sc_omp, &sc->sc_omlen, 0);
#else
	{	int i;
		ESPCMD(sc, ESPCMD_FLUSH);
		for (i = 0; i < sc->sc_omlen; i++)
			ESP_WRITE_REG(sc, FIFO, sc->sc_omess[i]);
		ESPCMD(sc, ESPCMD_TRANS);
#if test_stuck_on_msgout
printf("<<XXXmsgoutdoneXXX>>");
#endif
	}
#endif
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
	register struct ecb *ecb;
	register struct scsi_link *sc_link;
	struct esp_tinfo *ti;
	int loop;

	ESP_TRACE(("[espintr]"));
	
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
	for (loop = 0; ; loop++, DELAY(50/sc->sc_freq)) {
		/* a feeling of deja-vu */
		if (!DMA_ISINTR(sc->sc_dma))
			return (loop != 0);
#if 0
		if (loop)
			printf("*");
#endif

		/* and what do the registers say... */
		espreadregs(sc);

		sc->sc_intrcnt.ev_count++;

		/*
		 * At the moment, only a SCSI Bus Reset or Illegal
		 * Command are classed as errors. A disconnect is a
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

		/* SCSI Reset */
		if (sc->sc_espintr & ESPINTR_SBR) {
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			if (sc->sc_state != ESP_SBR) {
				printf("%s: SCSI bus reset\n",
					sc->sc_dev.dv_xname);
				esp_init(sc, 0); /* Restart everything */
				return 1;
			}
#if 0
	/*XXX*/		printf("<expected bus reset: "
				"[intr %x, stat %x, step %d]>\n",
				sc->sc_espintr, sc->sc_espstat,
				sc->sc_espstep);
#endif
			if (sc->sc_nexus)
				panic("%s: nexus in reset state",
				      sc->sc_dev.dv_xname);
			sc->sc_state = ESP_IDLE;
			esp_sched(sc);
			return 1;
		}

		ecb = sc->sc_nexus;

#define ESPINTR_ERR (ESPINTR_SBR|ESPINTR_ILL)
		if (sc->sc_espintr & ESPINTR_ERR ||
		    sc->sc_espstat & ESPSTAT_GE) {

			if (sc->sc_espstat & ESPSTAT_GE) {
				/* no target ? */
				if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				if (sc->sc_state == ESP_HASNEXUS ||
				    sc->sc_state == ESP_SELECTING) {
					ecb->xs->error = XS_DRIVER_STUFFUP;
					esp_done(ecb);
				}
				return 1;
			}

			if (sc->sc_espintr & ESPINTR_ILL) {
				/* illegal command, out of sync ? */
				printf("%s: illegal command: 0x%x (state %d, phase %x, prevphase %x)\n",
					sc->sc_dev.dv_xname, sc->sc_lastcmd,
					sc->sc_state, sc->sc_phase,
					sc->sc_prevphase);
				if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
					ESPCMD(sc, ESPCMD_FLUSH);
					DELAY(1);
				}
				esp_init(sc, 0); /* Restart everything */
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
			if (!(sc->sc_espstat & ESPSTAT_TC))
				printf("%s: !TC\n", sc->sc_dev.dv_xname);
			continue;
		}

		/*
		 * check for less serious errors
		 */
		if (sc->sc_espstat & ESPSTAT_PE) {
			printf("%s: SCSI bus parity error\n",
				sc->sc_dev.dv_xname);
			if (sc->sc_prevphase == MESSAGE_IN_PHASE)
				esp_sched_msgout(SEND_PARITY_ERROR);
			else 
				esp_sched_msgout(SEND_INIT_DET_ERR);
		}

		if (sc->sc_espintr & ESPINTR_DIS) {
			ESP_MISC(("<DISC [intr %x, stat %x, step %d]>", sc->sc_espintr, sc->sc_espstat, sc->sc_espstep));
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/*
			 * This command must (apparently) be issued within
			 * 250mS of a disconnect. So here you are...
			 */
			ESPCMD(sc, ESPCMD_ENSEL);
			if (sc->sc_state != ESP_IDLE) {
				if ((sc->sc_flags & ESP_SYNCHNEGO)) {
					printf("%s: sync nego not completed!\n",
						sc->sc_dev.dv_xname);
				}
			/*XXX*/sc->sc_msgpriq = sc->sc_msgout = 0;

				/* it may be OK to disconnect */
				if (!(sc->sc_flags & ESP_BUSFREE_OK))
					ecb->xs->error = XS_TIMEOUT;
				else if (sc->sc_flags & ESP_DISCON) {
					TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
					ECB_SETQ(ecb, ECB_QNEXUS);
					sc->sc_nexus = NULL;
					sc->sc_flags &= ~ESP_DISCON;
					sc->sc_state = ESP_IDLE;
					esp_sched(sc);
					return 1;
				}

				esp_done(ecb);
				return 1;
			}
			printf("DISCONNECT in IDLE state!\n");
		}

		/* did a message go out OK ? This must be broken */
		if (sc->sc_prevphase == MESSAGE_OUT_PHASE &&
		    sc->sc_phase != MESSAGE_OUT_PHASE) {
			/* we have sent it */
			sc->sc_msgpriq &= ~sc->sc_msgout;
			sc->sc_msgout = 0;
		}

		switch (sc->sc_state) {

		case ESP_SBR:
			printf("%s: waiting for SCSI Bus Reset to happen\n",
				sc->sc_dev.dv_xname);
			return 1;

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
printf("<<RESELECT CONT'd>>");
#if XXXX
			esp_msgin(sc);
			if (sc->sc_state != ESP_HASNEXUS) {
				/* IDENTIFY fail?! */
				printf("%s: identify failed\n",
					sc->sc_dev.dv_xname);
				esp_init(sc, 1);
				return 1;
			}
#endif
			break;

		case ESP_IDLE:
if (sc->sc_flags & ESP_ICCS) printf("[[esp: BUMMER]]");
		case ESP_SELECTING:

			if (sc->sc_espintr & ESPINTR_RESEL) {
				/*
				 * If we're trying to select a
				 * target ourselves, push our command
				 * back into the ready list.
				 */
				if (sc->sc_state == ESP_SELECTING) {
					ESP_MISC(("backoff selector "));
					sc_link = sc->sc_nexus->xs->sc_link;
					ti = &sc->sc_tinfo[sc_link->target];
					TAILQ_INSERT_HEAD(&sc->ready_list,
					    sc->sc_nexus, chain);
					ECB_SETQ(sc->sc_nexus, ECB_QREADY);
					ti->lubusy &= ~(1<<sc_link->lun);
					ecb = sc->sc_nexus = NULL;
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
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
printf("<RESELECT: %d bytes in FIFO>", ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF);
				}
				sc->sc_selid = ESP_READ_REG(sc, ESP_FIFO);
				sc->sc_selid &= ~(1<<sc->sc_id);
				ESP_MISC(("selid=0x%2x ", sc->sc_selid));
				esp_msgin(sc);	/* Handle identify message */
				if (sc->sc_state != ESP_HASNEXUS) {
					/* IDENTIFY fail?! */
					printf("%s: identify failed\n",
						sc->sc_dev.dv_xname);
					esp_init(sc, 1);
					return 1;
				}
				continue; /* ie. next phase expected soon */
			}

#define	ESPINTR_DONE	(ESPINTR_FC|ESPINTR_BS)
			if ((sc->sc_espintr & ESPINTR_DONE) == ESPINTR_DONE) {
				ecb = sc->sc_nexus;
				if (!ecb)
					panic("esp: not nexus at sc->sc_nexus");

				sc_link = ecb->xs->sc_link;
				ti = &sc->sc_tinfo[sc_link->target];

				switch (sc->sc_espstep) {
				case 0:
					printf("%s: select timeout/no disconnect\n",
						sc->sc_dev.dv_xname);
					esp_abort(sc, ecb);
					return 1;
				case 1:
					if ((ti->flags & T_NEGOTIATE) == 0) {
						printf("%s: step 1 & !NEG\n",
							sc->sc_dev.dv_xname);
						esp_abort(sc, ecb);
						return 1;
					}
					if (sc->sc_phase != MESSAGE_OUT_PHASE) {
						printf("%s: !MSGOUT\n",
							sc->sc_dev.dv_xname);
						esp_abort(sc, ecb);
						return 1;
					}
					/* Start negotiating */
					ti->period = sc->sc_minsync;
					ti->offset = 15;
					sc->sc_msgpriq = SEND_SDTR;
					break;
				case 3:
					/*
					 * Grr, this is supposed to mean
					 * "target left command phase
					 *  prematurely". It seems to happen
					 * regularly when sync mode is on.
					 * Look at FIFO to see if command
					 * went out.
					 * (Timing problems?)
					 */
					if ((ESP_READ_REG(sc, ESP_FFLAG)&ESPFIFO_FF) == 0) {
						/* Hope for the best.. */
						break;
					}
					printf("(%s:%d:%d): selection failed;"
						" %d left in FIFO "
						"[intr %x, stat %x, step %d]\n",
						sc->sc_dev.dv_xname,
						sc_link->target,
						sc_link->lun,
						ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
					sc->sc_flags |= ESP_ABORTING;
					esp_sched_msgout(SEND_ABORT);
					return 1;
				case 2:
					/* Select stuck at Command Phase */
					ESPCMD(sc, ESPCMD_FLUSH);
				case 4:
					/* So far, everything went fine */
					sc->sc_msgpriq = 0;
					break;
				}
#if 0
/* Why set msgpriq? (and not raise ATN) */
				if (ecb->xs->flags & SCSI_RESET)
					sc->sc_msgpriq = SEND_DEV_RESET;
				else if (ti->flags & T_NEGOTIATE)
					sc->sc_msgpriq =
					    SEND_IDENTIFY | SEND_SDTR;
				else
					sc->sc_msgpriq = SEND_IDENTIFY;
#endif
				sc->sc_state = ESP_HASNEXUS;
				/*???sc->sc_flags = 0; */
				sc->sc_prevphase = INVALID_PHASE; /* ?? */
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;
				ti->lubusy |= (1<<sc_link->lun);
				break;
			} else {
				printf("%s: unexpected status after select"
					": [intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
				esp_abort(sc, ecb);
			}
			if (sc->sc_state == ESP_IDLE) {
				printf("%s: stray interrupt\n", sc->sc_dev.dv_xname);
					return 0;
			}
			break;

		case ESP_HASNEXUS:
			if (sc->sc_flags & ESP_ICCS) {
				unsigned char msg;

				sc->sc_flags &= ~ESP_ICCS;

				if (!(sc->sc_espintr & ESPINTR_DONE)) {
					printf("%s: ICCS: "
					      ": [intr %x, stat %x, step %x]\n",
						sc->sc_dev.dv_xname,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
				}
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
					printf("%s: ICCS: expected 2, got %d "
					      ": [intr %x, stat %x, step %x]\n",
						sc->sc_dev.dv_xname,
						ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
					ESPCMD(sc, ESPCMD_FLUSH);
					esp_abort(sc, ecb);
					return 1;
				}
				ecb->stat = ESP_READ_REG(sc, ESP_FIFO);
				msg = ESP_READ_REG(sc, ESP_FIFO);
				ESP_PHASE(("<stat:(%x,%x)>", ecb->stat, msg));
				if (msg == MSG_CMDCOMPLETE) {
					sc->sc_flags |= ESP_BUSFREE_OK;
					ecb->xs->resid = ecb->dleft = sc->sc_dleft;
				} else
					printf("%s: STATUS_PHASE: msg %d\n",
						sc->sc_dev.dv_xname, msg);
				ESPCMD(sc, ESPCMD_MSGOK);
				continue; /* ie. wait for disconnect */
			}
			break;
		default:
			panic("%s: invalid state: %d",
			      sc->sc_dev.dv_xname,
			      sc->sc_state);
		}

		/*
		 * Driver is now in state ESP_HASNEXUS, i.e. we
		 * have a current command working the SCSI bus.
		 */
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
			if (sc->sc_espintr & ESPINTR_BS) {
				ESPCMD(sc, ESPCMD_FLUSH);
				sc->sc_flags |= ESP_WAITI;
				ESPCMD(sc, ESPCMD_TRANS);
			} else if (sc->sc_espintr & ESPINTR_FC) {
				if ((sc->sc_flags & ESP_WAITI) == 0) {
					printf("%s: MSGIN: unexpected FC bit: "
						"[intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				}
				sc->sc_flags &= ~ESP_WAITI;
				esp_msgin(sc);
			} else {
				printf("%s: MSGIN: weird bits: "
					"[intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
			}
			sc->sc_prevphase = MESSAGE_IN_PHASE;
			break;
		case COMMAND_PHASE: {
			/* well, this means send the command again */
			u_char *cmd = (u_char *)&ecb->cmd;
			int i;

			ESP_PHASE(("COMMAND_PHASE 0x%02x (%d) ",
				ecb->cmd.opcode, ecb->clen));
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/* Now the command into the FIFO */
			for (i = 0; i < ecb->clen; i++)
				ESP_WRITE_REG(sc, ESP_FIFO, *cmd++);
			ESPCMD(sc, ESPCMD_TRANS);
			sc->sc_prevphase = COMMAND_PHASE;
			}
			break;
		case DATA_OUT_PHASE:
			ESP_PHASE(("DATA_OUT_PHASE [%d] ",  sc->sc_dleft));
			ESPCMD(sc, ESPCMD_FLUSH);
			DMA_START(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft, 0);
			sc->sc_prevphase = DATA_OUT_PHASE;
			return 1;
		case DATA_IN_PHASE:
			ESP_PHASE(("DATA_IN_PHASE "));
			if (sc->sc_rev == ESP100)
				ESPCMD(sc, ESPCMD_FLUSH);
#if 0 /* Why is the fifo sometimes full after re-connect? */
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				static int xxx;
				if (xxx <= 3) {
					printf("%s: lost %d bytes from FIFO ",
						sc->sc_dev.dv_xname,
						ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF);
					printf(" previous phase %x\n",
						sc->sc_prevphase);
					if (xxx == 3)
						printf("(stopped logging)");
					++xxx;
				}
			}
#endif
			DMA_DRAIN(sc->sc_dma);
			DMA_START(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft, 1);
			sc->sc_prevphase = DATA_IN_PHASE;
			return 1;
		case STATUS_PHASE:
			ESP_PHASE(("STATUS_PHASE "));
			sc->sc_flags |= ESP_ICCS;
			ESPCMD(sc, ESPCMD_ICCS);
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
	panic("esp: should not get here..");
}

void
esp_abort(sc, ecb)
	struct esp_softc *sc;
	struct ecb *ecb;
{
	if (ecb == sc->sc_nexus) {
/*XXX*/sc->sc_tinfo[ecb->xs->sc_link->target].flags |= T_XXX;
		if (sc->sc_state == ESP_HASNEXUS) {
			sc->sc_flags |= ESP_ABORTING;
			esp_sched_msgout(SEND_ABORT);
		}
	} else {
		if (sc->sc_state == ESP_IDLE)
			esp_sched(sc);
	}
}

void
esp_timeout(arg)
	void *arg;
{
	int s = splbio();
	struct ecb *ecb = (struct ecb *)arg;
	struct esp_softc *sc;
	struct scsi_xfer *xs = ecb->xs;

	sc = xs->sc_link->adapter_softc;
	sc_print_addr(xs->sc_link);
again:
	printf("timed out (ecb 0x%x (flags 0x%x, dleft %x), state %d, phase %d, msgpriq %x, msgout %x)",
		ecb, ecb->flags, ecb->dleft, sc->sc_state, sc->sc_phase,
		sc->sc_msgpriq, sc->sc_msgout);

	if (ecb->flags == ECB_QFREE) {
		printf("note: ecb already on free list\n");
		splx(s);
		return;
	}
	if (ecb->flags & ECB_ABORTED) {
		/* abort timed out */
		printf(" AGAIN\n");
		xs->retries = 0;
		esp_init(sc, 1);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		xs->error = XS_TIMEOUT;
		ecb->flags |= ECB_ABORTED;
#if 0
if (sc->sc_dma->sc_active) {
	int x = esp_debug; esp_debug=0x3ff;
	DMA_INTR(sc->sc_dma);
	esp_debug = x;
}
#endif
		esp_abort(sc, ecb);
		/* 2 secs for the abort */
		if ((xs->flags & SCSI_POLL) == 0)
			timeout(esp_timeout, ecb, 2 * hz);
		else {
			int count = 200000;
			while (count) {
				if (DMA_ISINTR(sc->sc_dma)) {
					espintr(sc);
				}
				if (xs->flags & ITSDONE)
					break;
				DELAY(10);
				--count;
			}
			if (count == 0)
				goto again;
		}
	}
	splx(s);
}
