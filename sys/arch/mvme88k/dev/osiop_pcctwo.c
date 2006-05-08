/*	$OpenBSD: osiop_pcctwo.c,v 1.5 2006/05/08 14:36:10 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <machine/bugio.h>
#include <machine/prom.h>

#include <dev/ic/osiopreg.h>
#include <dev/ic/osiopvar.h>

#include <mvme88k/dev/pcctworeg.h>
#include <mvme88k/dev/pcctwovar.h>

void	osiop_pcctwo_attach(struct device *, struct device *, void *);
int	osiop_pcctwo_intr(void *);
int	osiop_pcctwo_match(struct device *, void *, void *);

struct osiop_pcctwo_softc {
	struct osiop_softc sc_osiop;
	struct intrhand sc_ih;
};

struct cfattach osiop_pcctwo_ca = {
	sizeof(struct osiop_pcctwo_softc),
	    osiop_pcctwo_match, osiop_pcctwo_attach
};

int
osiop_pcctwo_match(struct device *parent, void *cf, void *aux)
{
	struct confargs *ca = aux;
	bus_space_handle_t ioh;
	int rc;

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, OSIOP_NREGS, 0, &ioh) != 0)
		return (0);
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	if (rc == 0) {
		bus_space_unmap(ca->ca_iot, ioh, OSIOP_NREGS);
		return (1);
	}

	/*
	 * For some reason, if the SCSI hardware is not ``warmed'' by the
	 * BUG (netboot or boot from external SCSI controller), badaddr()
	 * will always fail, although the hardware is there.
	 * Since the BUG will do the right thing, we'll defer a dummy read
	 * from the controller and retry.
	 */
	if (brdtyp == BRD_187 || brdtyp == BRD_8120 || brdtyp == BRD_197) {
		struct mvmeprom_dskio dio;
		char buf[MVMEPROM_BLOCK_SIZE];

#ifdef DEBUG
		printf("osiop_pcctwo_match: trying to warm up controller\n");
#endif
		bzero(&dio, sizeof dio);
		dio.pbuffer = buf;
		dio.blk_cnt = 1;
		bugdiskrd(&dio);

		rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 4);
	}

	bus_space_unmap(ca->ca_iot, ioh, OSIOP_NREGS);
	return (rc == 0);
}

void
osiop_pcctwo_attach(struct device *parent, struct device *self, void *aux)
{
	struct pcctwosoftc *pcctwo = (struct pcctwosoftc *)parent;
	struct osiop_softc *sc = (struct osiop_softc *)self;
	struct osiop_pcctwo_softc *psc = (struct osiop_pcctwo_softc *)self;
	struct confargs *ca = aux;
	int tmp;
	extern int cpuspeed;

	sc->sc_bst = ca->ca_iot;
	sc->sc_dmat = ca->ca_dmat;

	if (bus_space_map(sc->sc_bst, ca->ca_paddr, OSIOP_NREGS, 0,
	    &sc->sc_reg) != 0) {
		printf(": couldn't map I/O ports\n");
		return;
	}

	switch (brdtyp) {
#ifdef MVME197
	case BRD_197:
		sc->sc_clock_freq = cpuspeed;
		break;
#endif
#ifdef MVME187
	case BRD_187:
	case BRD_8120:
		sc->sc_clock_freq = cpuspeed * 2;
		break;
#endif
	default:
		sc->sc_clock_freq = 50;	/* wild guess */
		break;
	}

	sc->sc_dcntl = OSIOP_DCNTL_EA;
	sc->sc_ctest7 = OSIOP_CTEST7_TT1;	/* no snooping */
	sc->sc_dmode = OSIOP_DMODE_BL4;	/* burst length = 4 */
	sc->sc_flags = 0;
	sc->sc_id = 7;	/* XXX should read from CNFG block in nvram */

	tmp = bootpart;
	if (ca->ca_paddr != bootaddr)
		bootpart = -1;	/* never match */

	osiop_attach(sc);

	bootpart = tmp;

	psc->sc_ih.ih_fn = osiop_pcctwo_intr;
	psc->sc_ih.ih_arg = sc;
	psc->sc_ih.ih_wantframe = 0;
	psc->sc_ih.ih_ipl = ca->ca_ipl;

	/* enable device interrupts */
	pcctwointr_establish(PCC2V_SCSI, &psc->sc_ih, self->dv_xname);
	bus_space_write_1(pcctwo->sc_iot, pcctwo->sc_ioh,
	    PCCTWO_SCSIICR, PCC2_IRQ_IEN | (ca->ca_ipl & PCC2_IRQ_IPL));
}

int
osiop_pcctwo_intr(void *arg)
{
	struct osiop_softc *sc = arg;
	u_int8_t istat;

	if (sc->sc_flags & OSIOP_INTSOFF)
		return 0;

	istat = osiop_read_1(sc, OSIOP_ISTAT);
	if ((istat & (OSIOP_ISTAT_SIP | OSIOP_ISTAT_DIP)) == 0)
		return 0;

	/*
	 * 53c710 manual recommends reading dstat and sstat0 at least
	 * 12 clk cycles apart if reading as bytes (which is what
	 * pcc2 permits). Stick in a 1us delay between accessing dstat and
	 * sstat0 below.
	 *
	 * save interrupt status, DMA status, and SCSI status 0
	 * (may need to deal with stacked interrupts?)
	 */
	sc->sc_istat = istat;
	sc->sc_sstat0 = osiop_read_1(sc, OSIOP_SSTAT0);
	DELAY(25);
	sc->sc_dstat = osiop_read_1(sc, OSIOP_DSTAT);

	osiop_intr(sc);

	return 1;
}
