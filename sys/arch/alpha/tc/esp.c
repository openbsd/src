/*	$OpenBSD: esp.c,v 1.10 2000/02/04 17:30:07 deraadt Exp $	*/
/*	$NetBSD: esp.c,v 1.26 1996/12/05 01:39:40 cgd Exp $	*/

#ifdef __sparc__
#define	SPARC_DRIVER
#endif

/*
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
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
#include <machine/autoconf.h>	/* badaddr() prototype */
#include <dev/tc/tcvar.h>
#include <alpha/tc/tcdsvar.h>
#include <alpha/tc/espreg.h>
#include <alpha/tc/espvar.h>
#endif

int esp_debug = 0; /*ESP_SHOWPHASE|ESP_SHOWMISC|ESP_SHOWTRAC|ESP_SHOWCMDS;*/

/*static*/ void	espattach	__P((struct device *, struct device *,
				    void *));
/*static*/ int	espprint	__P((void *, const char *));
#ifdef __BROKEN_INDIRECT_CONFIG
/*static*/ int	espmatch	__P((struct device *, void *, void *));
#else
/*static*/ int	espmatch	__P((struct device *, struct cfdata *,
				    void *));
#endif
/*static*/ u_int	esp_adapter_info __P((struct esp_softc *));
/*static*/ void	espreadregs	__P((struct esp_softc *));
/*static*/ void	esp_select	__P((struct esp_softc *, struct esp_ecb *));
/*static*/ int esp_reselect	__P((struct esp_softc *, int));
/*static*/ void	esp_scsi_reset	__P((struct esp_softc *));
/*static*/ void	esp_reset	__P((struct esp_softc *));
/*static*/ void	espinit	__P((struct esp_softc *, int));
/*static*/ int	esp_scsi_cmd	__P((struct scsi_xfer *));
/*static*/ int	esp_poll	__P((struct esp_softc *, struct scsi_xfer *,
				    int));
/*static*/ void	esp_sched	__P((struct esp_softc *));
/*static*/ void	esp_done	__P((struct esp_softc *, struct esp_ecb *));
/*static*/ void	esp_msgin	__P((struct esp_softc *));
/*static*/ void	esp_msgout	__P((struct esp_softc *));
/*static*/ int	espintr		__P((struct esp_softc *));
/*static*/ void	esp_timeout	__P((void *arg));
/*static*/ void	esp_abort	__P((struct esp_softc *, struct esp_ecb *));
/*static*/ void esp_dequeue	__P((struct esp_softc *, struct esp_ecb *));
void esp_sense __P((struct esp_softc *, struct esp_ecb *));
void esp_free_ecb __P((struct esp_softc *, struct esp_ecb *, int));
struct esp_ecb *esp_get_ecb __P((struct esp_softc *, int));
static inline int esp_stp2cpb __P((struct esp_softc *, int));
static inline int esp_cpb2stp __P((struct esp_softc *, int));
static inline void esp_setsync __P((struct esp_softc *, struct esp_tinfo *));

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch, espattach
};

struct cfdriver esp_cd = {
	NULL, "esp", DV_DULL
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
 * XXX should go when new generic scsiprint finds its way here
 */
int
espprint(aux, name)
	void *aux;
	const char *name;
{
	if (name != NULL)
		printf("scsibus at %s", name);
	return UNCONF;
}

int
#ifdef __BROKEN_INDIRECT_CONFIG
espmatch(parent, vcf, aux)
#else
espmatch(parent, cf, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *vcf;
#else
	struct cfdata *cf;
#endif
	void *aux;
{
#ifdef SPARC_DRIVER
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *cf = vcf;
#endif
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
	int dmachild = strncmp(parent->dv_xname, "dma", 3) == 0;
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
	sc->sc_reg = (volatile u_int32_t *)tcdsdev->tcdsda_addr;
	sc->sc_cookie = tcdsdev->tcdsda_cookie;
	sc->sc_dma = tcdsdev->tcdsda_sc;

	printf(": address %p", sc->sc_reg);
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
	if (parent->dv_cfdata->cf_driver == &tcds_cd) {
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
		else
			panic("espattach: no dma found");
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
		sc->sc_cfg2 = ESPCFG2_SCSI2;
		ESP_WRITE_REG(sc, ESP_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (ESPCFG3_CDB | ESPCFG3_FCLK);
		ESP_WRITE_REG(sc, ESP_CFG3, sc->sc_cfg3);
		if (ESP_READ_REG(sc, ESP_CFG3) != (ESPCFG3_CDB | ESPCFG3_FCLK)) {
			printf(": ESP100A");
			sc->sc_rev = ESP100A;
		} else {
			/* ESPCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= ESPCFG2_FE;
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
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;
	case ESP100A:
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = esp_cpb2stp(sc, 5); /* Min clocks/byte is 5 */
		break;
	case ESP200:
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* XXX - do actually set FAST* bits */
	}
#else
	sc->sc_maxxfer = 64 * 1024;
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
	espinit(sc, 1);

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
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);
#endif

	/*
	 * fill in the prototype scsi_link.
	 */
	/* sc->sc_link.channel = SCSI_CHANNEL_ONLY_ONE; */
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
	config_found(self, &sc->sc_link, /* scsiprint */ espprint);	/* XXX */

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
espinit(sc, doreset)
	struct esp_softc *sc;
	int doreset;
{
	struct esp_ecb *ecb;
	int r;

	ESP_TRACE(("[ESPINIT(%d)] ", doreset));

	if (sc->sc_state == 0) {
		/* First time through; initialize. */
		TAILQ_INIT(&sc->ready_list);
		TAILQ_INIT(&sc->nexus_list);
		TAILQ_INIT(&sc->free_list);
		sc->sc_nexus = NULL;
		ecb = sc->sc_ecb;
		bzero(ecb, sizeof(sc->sc_ecb));
		for (r = 0; r < sizeof(sc->sc_ecb) / sizeof(*ecb); r++) {
			TAILQ_INSERT_TAIL(&sc->free_list, ecb, chain);
			ecb++;
		}
		bzero(sc->sc_tinfo, sizeof(sc->sc_tinfo));
	} else {
		/* Cancel any active commands. */
		sc->sc_state = ESP_CLEANING;
		if ((ecb = sc->sc_nexus) != NULL) {
			ecb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, ecb);
			esp_done(sc, ecb);
		}
		while ((ecb = sc->nexus_list.tqh_first) != NULL) {
			ecb->xs->error = XS_DRIVER_STUFFUP;
			untimeout(esp_timeout, ecb);
			esp_done(sc, ecb);
		}
	}

	/*
	 * reset the chip to a known state
	 */
	esp_reset(sc);

	sc->sc_phase = sc->sc_prevphase = INVALID_PHASE;
	for (r = 0; r < 8; r++) {
		struct esp_tinfo *ti = &sc->sc_tinfo[r];
/* XXX - config flags per target: low bits: no reselect; high bits: no synch */
		int fl = sc->sc_dev.dv_cfdata->cf_flags;

		ti->flags = ((sc->sc_minsync && !(fl & (1<<(r+8))))
				? T_NEGOTIATE : 0) |
				((fl & (1<<r)) ? T_RSELECTOFF : 0) |
				T_NEED_TO_RESET;
		ti->period = sc->sc_minsync;
		ti->offset = 0;
	}

	if (doreset) {
		sc->sc_state = ESP_SBR;
		ESPCMD(sc, ESPCMD_RSTSCSI);
	} else {
		sc->sc_state = ESP_IDLE;
	}
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
 * Convert chip register Clock Per Byte value to Synchronous Transfer Period.
 */
static inline int
esp_cpb2stp(sc, cpb)
	struct esp_softc *sc;
	int cpb;
{
	return ((250 * cpb) / sc->sc_freq);
}

/*
 * Convert Synchronous Transfer Period to chip register Clock Per Byte value.
 */
static inline int
esp_stp2cpb(sc, period)
	struct esp_softc *sc;
	int period;
{
	int v;
	v = (sc->sc_freq * period) / 250;
	if (esp_cpb2stp(sc, v) < period)
		/* Correct round-down error */
		v++;
	return v;
}

static inline void
esp_setsync(sc, ti)
	struct esp_softc *sc;
	struct esp_tinfo *ti;
{

	if (ti->flags & T_SYNCMODE) {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, ti->offset);
		ESP_WRITE_REG(sc, ESP_SYNCTP, esp_stp2cpb(sc, ti->period));
	} else {
		ESP_WRITE_REG(sc, ESP_SYNCOFF, 0);
		ESP_WRITE_REG(sc, ESP_SYNCTP, 0);
	}
}

/*
 * Send a command to a target, set the driver state to ESP_SELECTING
 * and let the caller take care of the rest.
 *
 * Keeping this as a function allows me to say that this may be done
 * by DMA instead of programmed I/O soon.
 */
void
esp_select(sc, ecb)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
{
	struct scsi_link *sc_link = ecb->xs->sc_link;
	int target = sc_link->target;
	struct esp_tinfo *ti = &sc->sc_tinfo[target];
	u_char *cmd;
	int clen;

	ESP_TRACE(("[esp_select(t%d,l%d,cmd:%x)] ", sc_link->target, sc_link->lun, ecb->cmd.opcode));

	/* new state ESP_SELECTING */
	sc->sc_state = ESP_SELECTING;

	ESPCMD(sc, ESPCMD_FLUSH);

	/*
	 * The docs say the target register is never reset, and I
	 * can't think of a better place to set it
	 */
	ESP_WRITE_REG(sc, ESP_SELID, target);
	esp_setsync(sc, ti);

	/*
	 * Who am I. This is where we tell the target that we are
	 * happy for it to disconnect etc.
	 */
	ESP_WRITE_REG(sc, ESP_FIFO,
		MSG_IDENTIFY(sc_link->lun, (ti->flags & T_RSELECTOFF)?0:1));

	if (ti->flags & T_NEGOTIATE) {
		/* Arbitrate, select and stop after IDENTIFY message */
		ESPCMD(sc, ESPCMD_SELATNS);
		return;
	}

	/* Now the command into the FIFO */
	cmd = (u_char *)&ecb->cmd;
	clen = ecb->clen;
	while (clen--)
		ESP_WRITE_REG(sc, ESP_FIFO, *cmd++);

	/* And get the targets attention */
	ESPCMD(sc, ESPCMD_SELATN);
}

void
esp_free_ecb(sc, ecb, flags)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
	int flags;
{
	int s;

	s = splbio();

	ecb->flags = 0;
	TAILQ_INSERT_HEAD(&sc->free_list, ecb, chain);

	/*
	 * If there were none, wake anybody waiting for one to come free,
	 * starting with queued entries.
	 */
	if (ecb->chain.tqe_next == 0)
		wakeup(&sc->free_list);

	splx(s);
}

struct esp_ecb *
esp_get_ecb(sc, flags)
	struct esp_softc *sc;
	int flags;
{
	struct esp_ecb *ecb;
	int s;

	s = splbio();

	while ((ecb = sc->free_list.tqh_first) == NULL &&
	       (flags & SCSI_NOSLEEP) == 0)
		tsleep(&sc->free_list, PRIBIO, "especb", 0);
	if (ecb) {
		TAILQ_REMOVE(&sc->free_list, ecb, chain);
		ecb->flags |= ECB_ALLOC;
	}

	splx(s);
	return ecb;
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
	struct esp_ecb *ecb;
	int s, flags;

	ESP_TRACE(("[esp_scsi_cmd] "));
	ESP_CMDS(("[0x%x, %d]->%d ", (int)xs->cmd->opcode, xs->cmdlen,
	    sc_link->target));

	flags = xs->flags;
	if ((ecb = esp_get_ecb(sc, flags)) == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return TRY_AGAIN_LATER;
	}

	/* Initialize ecb */
	ecb->xs = xs;
	ecb->timeout = xs->timeout;

	if (xs->flags & SCSI_RESET) {
		ecb->flags |= ECB_RESET;
		ecb->clen = 0;
		ecb->dleft = 0;
	} else {
		bcopy(xs->cmd, &ecb->cmd, xs->cmdlen);
		ecb->clen = xs->cmdlen;
		ecb->daddr = xs->data;
		ecb->dleft = xs->datalen;
	}
	ecb->stat = 0;

	s = splbio();

	TAILQ_INSERT_TAIL(&sc->ready_list, ecb, chain);
	if (sc->sc_state == ESP_IDLE)
		esp_sched(sc);

	splx(s);

	if ((flags & SCSI_POLL) == 0)
		return SUCCESSFULLY_QUEUED;

	/* Not allowed to use interrupts, use polling instead */
	if (esp_poll(sc, xs, ecb->timeout)) {
		esp_timeout(ecb);
		if (esp_poll(sc, xs, ecb->timeout))
			esp_timeout(ecb);
	}
	return COMPLETE;
}

/*
 * Used when interrupt driven I/O isn't allowed, e.g. during boot.
 */
int
esp_poll(sc, xs, count)
	struct esp_softc *sc;
	struct scsi_xfer *xs;
	int count;
{

	ESP_TRACE(("[esp_poll] "));
	while (count) {
		if (DMA_ISINTR(sc->sc_dma)) {
			espintr(sc);
		}
#if alternatively
		if (ESP_READ_REG(sc, ESP_STAT) & ESPSTAT_INT)
			espintr(sc);
#endif
		if ((xs->flags & ITSDONE) != 0)
			return 0;
		if (sc->sc_state == ESP_IDLE) {
			ESP_TRACE(("[esp_poll: rescheduling] "));
			esp_sched(sc);
		}
		DELAY(1000);
		count--;
	}
	return 1;
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
	struct esp_ecb *ecb;
	struct scsi_link *sc_link;
	struct esp_tinfo *ti;

	ESP_TRACE(("[esp_sched] "));
	if (sc->sc_state != ESP_IDLE)
		panic("esp_sched: not IDLE (state=%d)", sc->sc_state);

	/*
	 * Find first ecb in ready queue that is for a target/lunit
	 * combinations that is not busy.
	 */
	for (ecb = sc->ready_list.tqh_first; ecb; ecb = ecb->chain.tqe_next) {
		sc_link = ecb->xs->sc_link;
		ti = &sc->sc_tinfo[sc_link->target];
		if ((ti->lubusy & (1 << sc_link->lun)) == 0) {
			TAILQ_REMOVE(&sc->ready_list, ecb, chain);
			sc->sc_nexus = ecb;
			esp_select(sc, ecb);
			break;
		} else
			ESP_MISC(("%d:%d busy\n",
			    sc_link->target, sc_link->lun));
	}
}

void
esp_sense(sc, ecb)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_tinfo *ti = &sc->sc_tinfo[sc_link->target];
	struct scsi_sense *ss = (void *)&ecb->cmd;

	ESP_MISC(("requesting sense "));
	/* Next, setup a request sense command block */
	bzero(ss, sizeof(*ss));
	ss->opcode = REQUEST_SENSE;
	ss->byte2 = sc_link->lun << 5;
	ss->length = sizeof(struct scsi_sense_data);
	ecb->clen = sizeof(*ss);
	ecb->daddr = (char *)&xs->sense;
	ecb->dleft = sizeof(struct scsi_sense_data);
	ecb->flags |= ECB_SENSE;
	ti->senses++;
	if (ecb->flags & ECB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (ecb == sc->sc_nexus) {
		esp_select(sc, ecb);
	} else {
		esp_dequeue(sc, ecb);
		TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
		if (sc->sc_state == ESP_IDLE)
			esp_sched(sc);
	}
}

/*
 * POST PROCESSING OF SCSI_CMD (usually current)
 */
void
esp_done(sc, ecb)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
{
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_tinfo *ti = &sc->sc_tinfo[sc_link->target];

	ESP_TRACE(("[esp_done(error:%x)] ", xs->error));

	/*
	 * Now, if we've come here with no error code, i.e. we've kept the
	 * initial XS_NOERROR, and the status code signals that we should
	 * check sense, we'll need to set up a request sense cmd block and
	 * push the command back into the ready queue *before* any other
	 * commands for this target/lunit, else we lose the sense info.
	 * We don't support chk sense conditions for the request sense cmd.
	 */
	if (xs->error == XS_NOERROR) {
		if ((ecb->flags & ECB_ABORT) != 0) {
			xs->error = XS_DRIVER_STUFFUP;
		} else if ((ecb->flags & ECB_SENSE) != 0) {
			xs->error = XS_SENSE;
		} else if ((ecb->stat & ST_MASK) == SCSI_CHECK) {
			/* First, save the return values */
			xs->resid = ecb->dleft;
			xs->status = ecb->stat;
			esp_sense(sc, ecb);
			return;
		} else {
			xs->resid = ecb->dleft;
		}
	}

	xs->flags |= ITSDONE;

#ifdef ESP_DEBUG
	if (esp_debug & ESP_SHOWMISC) {
		if (xs->resid != 0)
			printf("resid=%lu ", xs->resid);
		if (xs->error == XS_SENSE)
			printf("sense=0x%02x\n", xs->sense.error_code);
		else
			printf("error=%d\n", xs->error);
	}
#endif

	/*
	 * Remove the ECB from whatever queue it's on.
	 */
	if (ecb->flags & ECB_NEXUS)
		ti->lubusy &= ~(1 << sc_link->lun);
	if (ecb == sc->sc_nexus) {
		sc->sc_nexus = NULL;
		sc->sc_state = ESP_IDLE;
		esp_sched(sc);
	} else
		esp_dequeue(sc, ecb);
		
	esp_free_ecb(sc, ecb, xs->flags);
	ti->cmds++;
	scsi_done(xs);
}

void
esp_dequeue(sc, ecb)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
{

	if (ecb->flags & ECB_NEXUS) {
		TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
	} else {
		TAILQ_REMOVE(&sc->ready_list, ecb, chain);
	}
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
	do {						\
		ESP_MISC(("esp_sched_msgout %d ", m));	\
		ESPCMD(sc, ESPCMD_SETATN);		\
		sc->sc_flags |= ESP_ATN;		\
		sc->sc_msgpriq |= (m);			\
	} while (0)

int
esp_reselect(sc, message)
	struct esp_softc *sc;
	int message;
{
	u_char selid, target, lun;
	struct esp_ecb *ecb;
	struct scsi_link *sc_link;
	struct esp_tinfo *ti;

	/*
	 * The SCSI chip made a snapshot of the data bus while the reselection
	 * was being negotiated.  This enables us to determine which target did
	 * the reselect.
	 */
	selid = sc->sc_selid & ~(1 << sc->sc_id);
	if (selid & (selid - 1)) {
		printf("%s: reselect with invalid selid %02x; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname, selid);
		goto reset;
	}

	/*
	 * Search wait queue for disconnected cmd
	 * The list should be short, so I haven't bothered with
	 * any more sophisticated structures than a simple
	 * singly linked list.
	 */
	target = ffs(selid) - 1;
	lun = message & 0x07;
	for (ecb = sc->nexus_list.tqh_first; ecb != NULL;
	     ecb = ecb->chain.tqe_next) {
		sc_link = ecb->xs->sc_link;
		if (sc_link->target == target && sc_link->lun == lun)
			break;
	}
	if (ecb == NULL) {
		printf("%s: reselect from target %d lun %d with no nexus; sending ABORT\n",
		    sc->sc_dev.dv_xname, target, lun);
		goto abort;
	}

	/* Make this nexus active again. */
	TAILQ_REMOVE(&sc->nexus_list, ecb, chain);
	sc->sc_state = ESP_CONNECTED;
	sc->sc_nexus = ecb;
	ti = &sc->sc_tinfo[target];
	ti->lubusy |= (1 << lun);
	esp_setsync(sc, ti);

	if (ecb->flags & ECB_RESET)
		esp_sched_msgout(SEND_DEV_RESET);
	else if (ecb->flags & ECB_ABORT)
		esp_sched_msgout(SEND_ABORT);

	/* Do an implicit RESTORE POINTERS. */
	sc->sc_dp = ecb->daddr;
	sc->sc_dleft = ecb->dleft;

	return (0);

reset:
	esp_sched_msgout(SEND_DEV_RESET);
	return (1);

abort:
	esp_sched_msgout(SEND_ABORT);
	return (1);
}

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

	ESP_TRACE(("[esp_msgin(curmsglen:%ld)] ", (long)sc->sc_imlen));

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
	switch (sc->sc_state) {
		struct esp_ecb *ecb;
		struct esp_tinfo *ti;

	case ESP_CONNECTED:
		ecb = sc->sc_nexus;
		ti = &sc->sc_tinfo[ecb->xs->sc_link->target];

		switch (sc->sc_imess[0]) {
		case MSG_CMDCOMPLETE:
			ESP_MSGS(("cmdcomplete "));
			if (sc->sc_dleft < 0) {
				struct scsi_link *sc_link = ecb->xs->sc_link;
				printf("%s: %ld extra bytes from %d:%d\n",
				    sc->sc_dev.dv_xname, -(long)sc->sc_dleft,
				    sc_link->target, sc_link->lun);
				sc->sc_dleft = 0;
			}
			ecb->xs->resid = ecb->dleft = sc->sc_dleft;
			sc->sc_state = ESP_CMDCOMPLETE;
			break;

		case MSG_MESSAGE_REJECT:
			if (esp_debug & ESP_SHOWMSGS)
				printf("%s: our msg rejected by target\n",
				    sc->sc_dev.dv_xname);
			switch (sc->sc_msgout) {
			case SEND_SDTR:
				sc->sc_flags &= ~ESP_SYNCHNEGO;
				ti->flags &= ~(T_NEGOTIATE | T_SYNCMODE);
				esp_setsync(sc, ti);
				break;
			case SEND_INIT_DET_ERR:
				goto abort;
			}
			break;

		case MSG_NOOP:
			ESP_MSGS(("noop "));
			break;

		case MSG_DISCONNECT:
			ESP_MSGS(("disconnect "));
			ti->dconns++;
			sc->sc_state = ESP_DISCONNECT;
			if ((ecb->xs->sc_link->quirks & SDEV_AUTOSAVE) == 0)
				break;
			/*FALLTHROUGH*/

		case MSG_SAVEDATAPOINTER:
			ESP_MSGS(("save datapointer "));
			ecb->daddr = sc->sc_dp;
			ecb->dleft = sc->sc_dleft;
			break;

		case MSG_RESTOREPOINTERS:
			ESP_MSGS(("restore datapointer "));
			sc->sc_dp = ecb->daddr;
			sc->sc_dleft = ecb->dleft;
			break;

		case MSG_EXTENDED:
			ESP_MSGS(("extended(%x) ", sc->sc_imess[2]));
			switch (sc->sc_imess[2]) {
			case MSG_EXT_SDTR:
				ESP_MSGS(("SDTR period %d, offset %d ",
					sc->sc_imess[3], sc->sc_imess[4]));
				if (sc->sc_imess[1] != 3)
					goto reject;
				ti->period = sc->sc_imess[3];
				ti->offset = sc->sc_imess[4];
				ti->flags &= ~T_NEGOTIATE;
				if (sc->sc_minsync == 0 ||
				    ti->offset == 0 ||
				    ti->period > 124) {
					printf("%s:%d: async\n", "esp",
						ecb->xs->sc_link->target);
					if ((sc->sc_flags&ESP_SYNCHNEGO) == 0) {
						/* target initiated negotiation */
						ti->offset = 0;
						ti->flags &= ~T_SYNCMODE;
						esp_sched_msgout(SEND_SDTR);
					} else {
						/* we are async */
						ti->flags &= ~T_SYNCMODE;
					}
				} else {
					int r = 250/ti->period;
					int s = (100*250)/ti->period - 100*r;
					int p;

					p =  esp_stp2cpb(sc, ti->period);
					ti->period = esp_cpb2stp(sc, p);
#ifdef ESP_DEBUG
					sc_print_addr(ecb->xs->sc_link);
					printf("max sync rate %d.%02dMb/s\n",
						r, s);
#endif
					if ((sc->sc_flags&ESP_SYNCHNEGO) == 0) {
						/* target initiated negotiation */
						if (ti->period < sc->sc_minsync)
							ti->period = sc->sc_minsync;
						if (ti->offset > 15)
							ti->offset = 15;
						ti->flags &= ~T_SYNCMODE;
						esp_sched_msgout(SEND_SDTR);
					} else {
						/* we are sync */
						ti->flags |= T_SYNCMODE;
					}
				}
				sc->sc_flags &= ~ESP_SYNCHNEGO;
				esp_setsync(sc, ti);
				break;

			default:
				printf("%s: unrecognized MESSAGE EXTENDED; sending REJECT\n",
				    sc->sc_dev.dv_xname);
				goto reject;
			}
			break;

		default:
			ESP_MSGS(("ident "));
			printf("%s: unrecognized MESSAGE; sending REJECT\n",
			    sc->sc_dev.dv_xname);
		reject:
			esp_sched_msgout(SEND_REJECT);
			break;
		}
		break;

	case ESP_RESELECTED:
		if (!MSG_ISIDENTIFY(sc->sc_imess[0])) {
			printf("%s: reselect without IDENTIFY; sending DEVICE RESET\n",
			    sc->sc_dev.dv_xname);
			goto reset;
		}

		(void) esp_reselect(sc, sc->sc_imess[0]);
		break;

	default:
		printf("%s: unexpected MESSAGE IN; sending DEVICE RESET\n",
		    sc->sc_dev.dv_xname);
	reset:
		esp_sched_msgout(SEND_DEV_RESET);
		break;

	abort:
		esp_sched_msgout(SEND_ABORT);
		break;
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
	struct esp_ecb *ecb;
	size_t size;

	ESP_TRACE(("[esp_msgout(priq:%x, prevphase:%x)]", sc->sc_msgpriq, sc->sc_prevphase));

	if (sc->sc_flags & ESP_ATN) {
		if (sc->sc_prevphase != MESSAGE_OUT_PHASE) {
		new:
			ESPCMD(sc, ESPCMD_FLUSH);
			DELAY(1);
			sc->sc_msgoutq = 0;
			sc->sc_omlen = 0;
		}
	} else {
		if (sc->sc_prevphase == MESSAGE_OUT_PHASE) {
			esp_sched_msgout(sc->sc_msgoutq);
			goto new;
		} else {
			printf("esp at line %d: unexpected MESSAGE OUT phase\n", __LINE__);
		}
	}
			
	if (sc->sc_omlen == 0) {
		/* Pick up highest priority message */
		sc->sc_msgout = sc->sc_msgpriq & -sc->sc_msgpriq;
		sc->sc_msgoutq |= sc->sc_msgout;
		sc->sc_msgpriq &= ~sc->sc_msgout;
		sc->sc_omlen = 1;		/* "Default" message len */
		switch (sc->sc_msgout) {
		case SEND_SDTR:
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			sc->sc_omess[0] = MSG_EXTENDED;
			sc->sc_omess[1] = 3;
			sc->sc_omess[2] = MSG_EXT_SDTR;
			sc->sc_omess[3] = ti->period;
			sc->sc_omess[4] = ti->offset;
			sc->sc_omlen = 5;
			if ((sc->sc_flags & ESP_SYNCHNEGO) == 0) {
				ti->flags |= T_SYNCMODE;
				esp_setsync(sc, ti);
			}
			break;
		case SEND_IDENTIFY:
			if (sc->sc_state != ESP_CONNECTED) {
				printf("esp at line %d: no nexus\n", __LINE__);
			}
			ecb = sc->sc_nexus;
			sc->sc_omess[0] = MSG_IDENTIFY(ecb->xs->sc_link->lun,0);
			break;
		case SEND_DEV_RESET:
			sc->sc_flags |= ESP_ABORTING;
			sc->sc_omess[0] = MSG_BUS_DEV_RESET;
			ecb = sc->sc_nexus;
			ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
			ti->flags &= ~T_SYNCMODE;
			ti->flags |= T_NEGOTIATE;
			break;
		case SEND_PARITY_ERROR:
			sc->sc_omess[0] = MSG_PARITY_ERROR;
			break;
		case SEND_ABORT:
			sc->sc_flags |= ESP_ABORTING;
			sc->sc_omess[0] = MSG_ABORT;
			break;
		case SEND_INIT_DET_ERR:
			sc->sc_omess[0] = MSG_INITIATOR_DET_ERR;
			break;
		case SEND_REJECT:
			sc->sc_omess[0] = MSG_MESSAGE_REJECT;
			break;
		default:
			ESPCMD(sc, ESPCMD_RSTATN);
			sc->sc_flags &= ~ESP_ATN;
			sc->sc_omess[0] = MSG_NOOP;
			break;
		}
		sc->sc_omp = sc->sc_omess;
	}

#if 1
	/* (re)send the message */
	size = min(sc->sc_omlen, sc->sc_maxxfer);
	DMA_SETUP(sc->sc_dma, &sc->sc_omp, &sc->sc_omlen, 0, &size);
	/* Program the SCSI counter */
	ESP_WRITE_REG(sc, ESP_TCL, size);
	ESP_WRITE_REG(sc, ESP_TCM, size >> 8);
	if (sc->sc_cfg2 & ESPCFG2_FE) {
		ESP_WRITE_REG(sc, ESP_TCH, size >> 16);
	}
	/* load the count in */
	ESPCMD(sc, ESPCMD_NOP|ESPCMD_DMA);
	ESPCMD(sc, ESPCMD_TRANS|ESPCMD_DMA);
	DMA_GO(sc->sc_dma);
#else
	{	int i;
		for (i = 0; i < sc->sc_omlen; i++)
			ESP_WRITE_REG(sc, FIFO, sc->sc_omess[i]);
		ESPCMD(sc, ESPCMD_TRANS);
		sc->sc_omlen = 0;
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
	register struct esp_ecb *ecb;
	register struct scsi_link *sc_link;
	struct esp_tinfo *ti;
	int loop;
	size_t size;

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
	for (loop = 0; 1;loop++, DELAY(50/sc->sc_freq)) {
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
				espinit(sc, 0); /* Restart everything */
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
			goto sched;
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
				if (sc->sc_state == ESP_CONNECTED ||
				    sc->sc_state == ESP_SELECTING) {
					ecb->xs->error = XS_DRIVER_STUFFUP;
					esp_done(sc, ecb);
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
				espinit(sc, 0); /* Restart everything */
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
		if (DMA_ISACTIVE(sc->sc_dma)) {
			int r = DMA_INTR(sc->sc_dma);
			if (r == -1) {
				printf("%s: DMA error; resetting\n",
					sc->sc_dev.dv_xname);
				espinit(sc, 1);
			}
			/* If DMA active here, then go back to work... */
			if (DMA_ISACTIVE(sc->sc_dma))
				return 1;

			if (sc->sc_dleft == 0 &&
			    (sc->sc_espstat & ESPSTAT_TC) == 0)
				printf("%s: !TC [intr %x, stat %x, step %d]"
				       " prevphase %x, resid %lx\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr,
					sc->sc_espstat,
					sc->sc_espstep,
					sc->sc_prevphase,
					ecb?ecb->dleft:-1);
		}

#if 0	/* Unreliable on some ESP revisions? */
		if ((sc->sc_espstat & ESPSTAT_INT) == 0) {
			printf("%s: spurious interrupt\n", sc->sc_dev.dv_xname);
			return 1;
		}
#endif

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
			ESP_MISC(("<DISC [intr %x, stat %x, step %d]>",
				sc->sc_espintr,sc->sc_espstat,sc->sc_espstep));
			if (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) {
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
			}
			/*
			 * This command must (apparently) be issued within
			 * 250mS of a disconnect. So here you are...
			 */
			ESPCMD(sc, ESPCMD_ENSEL);
			switch (sc->sc_state) {
			case ESP_RESELECTED:
				goto sched;

			case ESP_SELECTING:
				ecb->xs->error = XS_SELTIMEOUT;
				goto finish;

			case ESP_CONNECTED:
				if ((sc->sc_flags & ESP_SYNCHNEGO)) {
#ifdef ESP_DEBUG
					if (ecb)
						sc_print_addr(ecb->xs->sc_link);
					printf("sync nego not completed!\n");
#endif
					ti = &sc->sc_tinfo[ecb->xs->sc_link->target];
					sc->sc_flags &= ~ESP_SYNCHNEGO;
					ti->flags &= ~(T_NEGOTIATE | T_SYNCMODE);
				}

				/* it may be OK to disconnect */
				if ((sc->sc_flags & ESP_ABORTING) == 0) {
					/*  
					 * Section 5.1.1 of the SCSI 2 spec
					 * suggests issuing a REQUEST SENSE
					 * following an unexpected disconnect.
					 * Some devices go into a contingent
					 * allegiance condition when
					 * disconnecting, and this is necessary
					 * to clean up their state.
					 */     
					printf("%s: unexpected disconnect; ",
					    sc->sc_dev.dv_xname);
					if (ecb->flags & ECB_SENSE) {
						printf("resetting\n");
						goto reset;
					}
					printf("sending REQUEST SENSE\n");
					esp_sense(sc, ecb);
					goto out;
				}

				ecb->xs->error = XS_DRIVER_STUFFUP;
				goto finish;

			case ESP_DISCONNECT:
				TAILQ_INSERT_HEAD(&sc->nexus_list, ecb, chain);
				sc->sc_nexus = NULL;
				goto sched;

			case ESP_CMDCOMPLETE:
				goto finish;
			}
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
				espinit(sc, 1);
				return 1;
			}
printf("<<RESELECT CONT'd>>");
#if XXXX
			esp_msgin(sc);
			if (sc->sc_state != ESP_CONNECTED) {
				/* IDENTIFY fail?! */
				printf("%s: identify failed\n",
					sc->sc_dev.dv_xname);
				espinit(sc, 1);
				return 1;
			}
#endif
			break;

		case ESP_IDLE:
if (sc->sc_flags & ESP_ICCS) printf("[[esp: BUMMER]]");
		case ESP_SELECTING:
			sc->sc_msgpriq = sc->sc_msgout = sc->sc_msgoutq = 0;
			sc->sc_flags = 0;

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
					espinit(sc, 1);
					return 1;
				}
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
					printf("%s: RESELECT: %d bytes in FIFO!\n",
						sc->sc_dev.dv_xname,
						ESP_READ_REG(sc, ESP_FFLAG) &
						ESPFIFO_FF);
					espinit(sc, 1);
					return 1;
				}
				sc->sc_selid = ESP_READ_REG(sc, ESP_FIFO);
				ESP_MISC(("selid=0x%2x ", sc->sc_selid));
				esp_msgin(sc);	/* Handle identify message */
				if (sc->sc_state != ESP_CONNECTED) {
					/* IDENTIFY fail?! */
					printf("%s: identify failed\n",
						sc->sc_dev.dv_xname);
					espinit(sc, 1);
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
					ecb->xs->error = XS_SELTIMEOUT;
					goto finish;
				case 1:
					if ((ti->flags & T_NEGOTIATE) == 0) {
						printf("%s: step 1 & !NEG\n",
							sc->sc_dev.dv_xname);
						goto reset;
					}
					if (sc->sc_phase != MESSAGE_OUT_PHASE) {
						printf("%s: !MSGOUT\n",
							sc->sc_dev.dv_xname);
						goto reset;
					}
					/* Start negotiating */
					ti->period = sc->sc_minsync;
					ti->offset = 15;
					sc->sc_flags |= ESP_SYNCHNEGO;
					esp_sched_msgout(SEND_SDTR);
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
					ESPCMD(sc, ESPCMD_FLUSH);
					esp_sched_msgout(SEND_ABORT);
					return 1;
				case 2:
					/* Select stuck at Command Phase */
					ESPCMD(sc, ESPCMD_FLUSH);
				case 4:
					/* So far, everything went fine */
					break;
				}
#if 0
				if (ecb->xs->flags & SCSI_RESET)
					esp_sched_msgout(SEND_DEV_RESET);
				else if (ti->flags & T_NEGOTIATE)
					esp_sched_msgout(
					    SEND_IDENTIFY | SEND_SDTR);
				else
					esp_sched_msgout(SEND_IDENTIFY);
#endif

				ecb->flags |= ECB_NEXUS;
				ti->lubusy |= (1 << sc_link->lun);

				sc->sc_prevphase = INVALID_PHASE; /* ?? */
				/* Do an implicit RESTORE POINTERS. */
				sc->sc_dp = ecb->daddr;
				sc->sc_dleft = ecb->dleft;

				/* On our first connection, schedule a timeout. */
				if ((ecb->xs->flags & SCSI_POLL) == 0)
					timeout(esp_timeout, ecb, (ecb->timeout * hz) / 1000);

				sc->sc_state = ESP_CONNECTED;
				break;
			} else {
				printf("%s: unexpected status after select"
					": [intr %x, stat %x, step %x]\n",
					sc->sc_dev.dv_xname,
					sc->sc_espintr, sc->sc_espstat,
					sc->sc_espstep);
				ESPCMD(sc, ESPCMD_FLUSH);
				DELAY(1);
				goto reset;
			}
			if (sc->sc_state == ESP_IDLE) {
				printf("%s: stray interrupt\n", sc->sc_dev.dv_xname);
					return 0;
			}
			break;

		case ESP_CONNECTED:
			if (sc->sc_flags & ESP_ICCS) {
				u_char msg;

				sc->sc_flags &= ~ESP_ICCS;

				if (!(sc->sc_espintr & ESPINTR_DONE)) {
					printf("%s: ICCS: "
					      ": [intr %x, stat %x, step %x]\n",
						sc->sc_dev.dv_xname,
						sc->sc_espintr, sc->sc_espstat,
						sc->sc_espstep);
				}
				if ((ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) != 2) {
					int i = (ESP_READ_REG(sc, ESP_FFLAG) & ESPFIFO_FF) - 2;
					while (i--)
						(void) ESP_READ_REG(sc, ESP_FIFO);
				}
				ecb->stat = ESP_READ_REG(sc, ESP_FIFO);
				msg = ESP_READ_REG(sc, ESP_FIFO);
				ESP_PHASE(("<stat:(%x,%x)>", ecb->stat, msg));
				if (msg == MSG_CMDCOMPLETE) {
					ecb->xs->resid = ecb->dleft = sc->sc_dleft;
					sc->sc_state = ESP_CMDCOMPLETE;
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
		 * Driver is now in state ESP_CONNECTED, i.e. we
		 * have a current command working the SCSI bus.
		 */
		if (sc->sc_state != ESP_CONNECTED || ecb == NULL) {
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
			ESP_PHASE(("DATA_OUT_PHASE [%ld] ",(long)sc->sc_dleft));
			ESPCMD(sc, ESPCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			DMA_SETUP(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft,
				  0, &size);
			sc->sc_prevphase = DATA_OUT_PHASE;
			goto setup_xfer;
		case DATA_IN_PHASE:
			ESP_PHASE(("DATA_IN_PHASE "));
			if (sc->sc_rev == ESP100)
				ESPCMD(sc, ESPCMD_FLUSH);
			size = min(sc->sc_dleft, sc->sc_maxxfer);
			DMA_SETUP(sc->sc_dma, &sc->sc_dp, &sc->sc_dleft,
				  1, &size);
			sc->sc_prevphase = DATA_IN_PHASE;
		setup_xfer:
			/* Program the SCSI counter */
			ESP_WRITE_REG(sc, ESP_TCL, size);
			ESP_WRITE_REG(sc, ESP_TCM, size >> 8);
			if (sc->sc_cfg2 & ESPCFG2_FE) {
				ESP_WRITE_REG(sc, ESP_TCH, size >> 16);
			}
			/* load the count in */
			ESPCMD(sc, ESPCMD_NOP|ESPCMD_DMA);

			/*
			 * Note that if `size' is 0, we've already transceived
			 * all the bytes we want but we're still in DATA PHASE.
			 * Apparently, the device needs padding. Also, a
			 * transfer size of 0 means "maximum" to the chip
			 * DMA logic.
			 */
			ESPCMD(sc,
			       (size==0?ESPCMD_TRPAD:ESPCMD_TRANS)|ESPCMD_DMA);
			DMA_GO(sc->sc_dma);
			return 1;
		case STATUS_PHASE:
			ESP_PHASE(("STATUS_PHASE "));
			sc->sc_flags |= ESP_ICCS;
			ESPCMD(sc, ESPCMD_ICCS);
			sc->sc_prevphase = STATUS_PHASE;
			break;
		case INVALID_PHASE:
			break;
		default:
			printf("%s: unexpected bus phase; resetting\n",
			    sc->sc_dev.dv_xname);
			goto reset;
		}
	}
	panic("esp: should not get here..");

reset:
	espinit(sc, 1);
	return 1;

finish:
	untimeout(esp_timeout, ecb);
	esp_done(sc, ecb);
	goto out;

sched:
	sc->sc_state = ESP_IDLE;
	esp_sched(sc);
	goto out;

out:
	return 1;
}

void
esp_abort(sc, ecb)
	struct esp_softc *sc;
	struct esp_ecb *ecb;
{

	/* 2 secs for the abort */
	ecb->timeout = ESP_ABORT_TIMEOUT;
	ecb->flags |= ECB_ABORT;

	if (ecb == sc->sc_nexus) {
		/*
		 * If we're still selecting, the message will be scheduled
		 * after selection is complete.
		 */
		if (sc->sc_state == ESP_CONNECTED)
			esp_sched_msgout(SEND_ABORT);

		/*
		 * Reschedule timeout. First, cancel a queued timeout (if any)
		 * in case someone decides to call esp_abort() from elsewhere.
		 */
		untimeout(esp_timeout, ecb);
		timeout(esp_timeout, ecb, (ecb->timeout * hz) / 1000);
	} else {
		esp_dequeue(sc, ecb);
		TAILQ_INSERT_HEAD(&sc->ready_list, ecb, chain);
		if (sc->sc_state == ESP_IDLE)
			esp_sched(sc);
	}
}

void
esp_timeout(arg)
	void *arg;
{
	struct esp_ecb *ecb = arg;
	struct scsi_xfer *xs = ecb->xs;
	struct scsi_link *sc_link = xs->sc_link;
	struct esp_softc *sc = sc_link->adapter_softc;
	int s;

	sc_print_addr(sc_link);
	printf("%s: timed out [ecb %p (flags 0x%x, dleft %x, stat %x)], "
	       "<state %d, nexus %p, phase(c %x, p %x), resid %lx, msg(q %x,o %x) %s>",
		sc->sc_dev.dv_xname,
		ecb, ecb->flags, ecb->dleft, ecb->stat,
		sc->sc_state, sc->sc_nexus, sc->sc_phase, sc->sc_prevphase,
		(long)sc->sc_dleft, sc->sc_msgpriq, sc->sc_msgout,
		DMA_ISACTIVE(sc->sc_dma) ? "DMA active" : "");
#if ESP_DEBUG > 0
	printf("TRACE: %s.", ecb->trace);
#endif

	s = splbio();

	if (ecb->flags & ECB_ABORT) {
		/* abort timed out */
		printf(" AGAIN\n");
		espinit(sc, 1);
	} else {
		/* abort the operation that has timed out */
		printf("\n");
		xs->error = XS_TIMEOUT;
		esp_abort(sc, ecb);
	}

	splx(s);
}
