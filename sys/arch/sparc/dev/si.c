/*	$NetBSD: si.c,v 1.6 1995/09/14 20:38:56 pk Exp $	*/

/*
 * Copyright (C) 1994 Adam Glass, Gordon W. Ross
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define DEBUG 1

/* XXX - Need to add support for real DMA. -gwr */
/* #define PSEUDO_DMA 1 (broken) */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_debug.h>
#include <scsi/scsiconf.h>

#include <dev/ic/ncr5380reg.h>
#include <dev/ic/ncr5380var.h>
#include <sparc/dev/sireg.h>

#ifdef	DEBUG
static int ncr5380_debug = 0;
static int si_flags = 0 /* | SDEV_DB2 */ ;
#endif

static int	si_match __P((struct device *, void *, void *));
static void	si_attach __P((struct device *, struct device *, void *));
static int	si_print __P((void *, char *));
static int	si_intr __P((void *));
static void	si_dma_intr __P((struct ncr5380_softc *));
static int	reset_adapter __P((struct ncr5380_softc *));


static char scsi_name[] = "si";

struct scsi_adapter	ncr5380_switch = {
	ncr5380_scsi_cmd,		/* scsi_cmd()		*/
	minphys,			/* scsi_minphys()	*/
	NULL,				/* open_target_lu()	*/
	NULL,				/* close_target_lu()	*/
};

/* This is copied from julian's bt driver */
/* "so we have a default dev struct for our link struct." */
struct scsi_device si_dev = {
	NULL,		/* Use default error handler. */
	NULL,		/* Use default start handler. */
	NULL,		/* Use default async handler. */
	NULL,		/* Use default "done" routine.*/
};

struct cfdriver sicd = {
	NULL, "si", si_match, si_attach, DV_DULL,
	sizeof(struct ncr5380_softc), NULL, 0,
};

/*
 * An `sw' is just an `si' behind a different DMA engine.
 * This driver doesn't currently do DMA, so we can more or less
 * handle it here.  (It's really not much different than the
 * Sun 3/50 SCSI controller, if I understand it right.)
 */
struct cfdriver swcd = {
	NULL, "sw", si_match, si_attach, DV_DULL,
	sizeof(struct ncr5380_softc), NULL, 0,
};

static int
si_print(aux, name)
	void *aux;
	char *name;
{
	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

static int
si_match(parent, vcf, aux)
	struct device	*parent;
	void		*vcf;
	void		*aux;
{
	struct cfdata	*cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	/* Are we looking for the right thing? */
	if (strcmp(cf->cf_driver->cd_name, ra->ra_name))
		return (0);

	/* Nothing but a Sun 4 is going to have these devices. */
	if (cputyp != CPU_SUN4)
		return (0);

	/* Figure out the bus type and look for the appropriate adapter. */
	switch (ca->ca_bustype) {
	case BUS_VME16:
		break;

	case BUS_OBIO:
		/* An `sw' can only exist on the 4/100 obio. */
		if (cpumod != SUN4_100)
			return (0);
		break;
	}

	/* Default interrupt priority always splbio == 2 */
	if (ra->ra_intr[0].int_pri == -1)
		ra->ra_intr[0].int_pri == 2;

	/* Make sure there is something there... */
	if (probeget(ra->ra_vaddr + 1, 1) == -1)
		return (0);

	/*
	 * If we're looking for an `si', we have to determine whether
	 * it is an `sc' (Sun2) or `si' (Sun3) SCSI board.  This can be
	 * determined using the fact that the `sc' board occupies 4K bytes
	 * in VME space but the `si' board occupies 2K bytes.
	 * Note that the `si' board should NOT respond to this.
	 */
	if (strcmp(cf->cf_driver->cd_name, "si") == 0)
		if (probeget(ra->ra_vaddr + 0x801, 1) != -1)
			return(0);

	return (1);
}

static void
si_attach(parent, self, aux)
	struct device	*parent, *self;
	void		*aux;
{
	struct ncr5380_softc *ncr5380 = (struct ncr5380_softc *) self;
	volatile struct si_regs *regs;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;
	struct bootpath *bp;

	/* Map the controller registers. */
	regs = (struct si_regs *)mapiodev(ra->ra_paddr,
	    sizeof(struct si_regs), ca->ca_bustype);

	/* Establish the interrupt. */
	ncr5380->sc_ih.ih_fun = si_intr;
	ncr5380->sc_ih.ih_arg = ncr5380;

	switch (ca->ca_bustype) {
	case BUS_VME16:
		/*
		 * This will be an `si'.
		 */
		vmeintr_establish(ra->ra_intr[0].int_vec,
		    ra->ra_intr[0].int_pri, &ncr5380->sc_ih);
		ncr5380->sc_adapter_iv_am =
		    VME_SUPV_DATA_24 | (ra->ra_intr[0].int_vec & 0xFF);
		break;

	case BUS_OBIO:
		/*
		 * This will be an `sw'.
		 */
		intr_establish(ra->ra_intr[0].int_pri, &ncr5380->sc_ih);
		break;
	}

	ncr5380->sc_adapter_type = ca->ca_bustype;
	ncr5380->sc_regs = regs;

	/*
	 * fill in the prototype scsi_link.
	 */
	ncr5380->sc_link.adapter_softc = ncr5380;
	ncr5380->sc_link.adapter_target = 7;
	ncr5380->sc_link.adapter = &ncr5380_switch;
	ncr5380->sc_link.device = &si_dev;
	ncr5380->sc_link.openings = 2;
#ifdef	DEBUG
	ncr5380->sc_link.flags |= si_flags;
#endif

	printf(" pri %d\n", ra->ra_intr[0].int_pri);
	reset_adapter(ncr5380);
	ncr5380_reset_scsibus(ncr5380);

	/*
	 * If the boot path is "sw" or "si" at the moment and it's me, then
	 * walk out pointer to the sub-device, ready for the config
	 * below.
	 */
	bp = ra->ra_bp;
	if (bp != NULL && strcmp(bp->name, ra->ra_name) == 0 &&
	    bp->val[0] == -1 && bp->val[1] == ncr5380->sc_dev.dv_unit)
		bootpath_store(1, bp + 1);

	/* Configure sub-devices */
	config_found(self, &(ncr5380->sc_link), si_print);

	bootpath_store(1, NULL);
}

static void
si_dma_intr(ncr5380)
	struct ncr5380_softc *ncr5380;
{
	volatile struct si_regs *regs = ncr5380->sc_regs;

#ifdef	DEBUG
	printf (" si_dma_intr\n");
#endif
}

static int
si_intr(arg)
	void *arg;
{
	struct ncr5380_softc *ncr5380 = arg;
	volatile struct si_regs *si = ncr5380->sc_regs;
	int rv = 0;

	switch (ncr5380->sc_adapter_type) {
	case BUS_VME16:
		/* Interrupts not enabled?  Can not be for us. */
		if ((si->si_csr & SI_CSR_INTR_EN) == 0)
			return rv;

		if (si->si_csr & SI_CSR_DMA_IP) {
			si_dma_intr(ncr5380);
			rv++;
		}
		if (si->si_csr & SI_CSR_SBC_IP) {
			ncr5380_sbc_intr(ncr5380);
			rv++;
		}
		break;

	case BUS_OBIO:
		/* Interrupts not enabled?  Can not be for us. */
		if ((si->sw_csr & SI_CSR_INTR_EN) == 0)
			return rv;

		if (si->sw_csr & SI_CSR_DMA_IP) {
			si_dma_intr(ncr5380);
			rv++;
		}
		if (si->sw_csr & SI_CSR_SBC_IP) {
			ncr5380_sbc_intr(ncr5380);
			rv++;
		}
		break;
	}

	return rv;
}

static int
reset_adapter(sc)
	struct ncr5380_softc *sc;
{
	volatile struct si_regs *si = sc->sc_regs;

#ifdef	DEBUG
	if (ncr5380_debug) {
		printf("reset_adapter\n");
	}
#endif

	switch(sc->sc_adapter_type) {
	case BUS_VME16:
		/* The reset bits in the CSR are active low. */
		si->si_csr = 0;
		delay(20);
		si->si_csr = SI_CSR_FIFO_RES | SI_CSR_SCSI_RES;
		si->fifo_count = 0;
		si->dma_addrh = 0;
		si->dma_addrl = 0;
		si->dma_counth = 0;
		si->dma_countl = 0;
		si->iv_am = sc->sc_adapter_iv_am;
		break;

	case BUS_OBIO:
		si->sw_csr = 0;
		delay(20);
		si->sw_csr = SI_CSR_FIFO_RES | SI_CSR_SCSI_RES;
		si->dma_addr = 0;
		si->dma_count = 0;
		break;
	}
}

#include <dev/ic/ncr5380.c>

