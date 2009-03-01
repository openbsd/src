/*	$OpenBSD: ofobio.c,v 1.1 2009/03/01 22:08:13 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * MVME141 On-board resources (One-Four-One OBIO)
 *
 * Unlike other MVME boards supported under OpenBSD/mvme68k, there is no
 * specific ASIC (lrc, mc2, pcc, pcc2...) providing a solid base to which
 * all other devices are related.
 *
 * Instead, we have four registers, and no timers. The onboard devices
 * (MC6861 providing serial ports and the clock, VMEchip, VSBchip and
 * NVRAM) may as well attach to mainbus. We nevertheless attach them at
 * ofobio to make things simpler in the kernel.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/ofobioreg.h>
#include <mvme68k/dev/dartreg.h>

struct ofobioreg *sys_ofobio;

struct ofobiosoftc {
	struct device	  sc_dev;
	struct ofobioreg *sc_regs;
	struct intrhand	  sc_abortih;
};

int	ofobioabort(void *);
void	ofobioattach(struct device *, struct device *, void *);
int	ofobiomatch(struct device *, void *, void *);
int	ofobioprint(void *, const char *);
int	ofobioscan(struct device *, void *, void *);

struct cfattach ofobio_ca = {
	sizeof(struct ofobiosoftc), ofobiomatch, ofobioattach
};

struct cfdriver ofobio_cd = {
	NULL, "ofobio", DV_DULL
};

int
ofobiomatch(struct device *parent, void *cf, void *aux)
{
	if (cputyp != CPU_141 || sys_ofobio != NULL)
		return (0);

	return (1);
}

void
ofobioattach(struct device *parent, struct device *self, void *aux)
{
#if 0
	struct confargs *ca = aux;
#endif
	struct ofobiosoftc *sc = (struct ofobiosoftc *)self;

	sys_ofobio = sc->sc_regs = (struct ofobioreg *)IIOV(OFOBIO_CSR_ADDR);

	/* disable VSB and timer interrupts */
	sc->sc_regs->csr_b |= OFO_CSRB_VSB_INTDIS | OFO_CSRB_TIMER_INTDIS;
	/* enable A32 addressing and device interrupts */
	sc->sc_regs->csr_b &= ~(OFO_CSRB_GLOBAL_INTDIS | OFO_CSRB_DISABLE_A24);

	/*
	 * A note regarding the board cache: the on-board 32KB are used
	 * during early BUG bootstrap, until off-board memory is found,
	 * and is intended to be used by the operating system as cache
	 * memory (in addition to the small instruction cache internal to
	 * the 68030).
	 *
	 * The cache flush and cache invalidation routines in locore
	 * are not modified to invalidate the board cache as well,
	 * since its snooping appears to be good enough to make it
	 * completely transparent (the fact that vsbic(4) works, relying
	 * to proper invalidate behaviour, is a good omen).
	 */

	/* clear board cache, then enable it */
	sc->sc_regs->csr_a |= OFO_CSRA_CACHE_WDIS | OFO_CSRA_CACHE_RDIS;
	sc->sc_regs->csr_a |= OFO_CSRA_CACHE_CLEAR;
	sc->sc_regs->csr_a = OFO_CSRA_CACHE_MONITOR;

	printf(": board cache enabled\n");

	sc->sc_abortih.ih_fn = ofobioabort;
	sc->sc_abortih.ih_ipl = IPL_HIGH;
	sc->sc_abortih.ih_wantframe = 1;
	intr_establish(OFOBIOVEC_ABORT, &sc->sc_abortih, self->dv_xname);

	config_search(ofobioscan, self, aux);
}

int
ofobioprint(void *aux, const char *pnp)
{
	struct confargs *ca = aux;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
ofobioscan(struct device *parent, void *self, void *aux)
{
	struct cfdata *cf = self;
#if 0
	struct ofobiosoftc *sc = (struct ofobiosoftc *)parent;
#endif
	struct confargs *ca = aux;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = ca->ca_iot;
	oca.ca_dmat = ca->ca_dmat;
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if (oca.ca_offset != -1) {
		oca.ca_paddr = ca->ca_paddr + oca.ca_offset;
		oca.ca_vaddr = IIOV(oca.ca_paddr);
	} else {
		oca.ca_vaddr = (vaddr_t)-1;
		oca.ca_paddr = (paddr_t)-1;
	}
	oca.ca_bustype = BUS_OFOBIO;
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, ofobioprint);

	return (1);
}

int
ofobioabort(void *frame)
{
	/*
	 * Latch the condition; this will debounce the interrupt,
	 * acknowledge it, and both the CSRD_ABORT and CRSD_ABORT_LATCH
	 * bits will clear.
	 */
	sys_ofobio->csr_d |= OFO_CSRD_ABORT_LATCH;

	nmihand(frame);

	return (1);
}

void
ofobio_clocksetup()
{
	volatile uint8_t *dartregs =
	    (volatile uint8_t *)IIOV(MVME141_DART_BASE);
	uint limit;
	uint8_t dummy;

	/*
	 * Note that the dart(4) driver already may have programmed the
	 * OPCR register during attachment.
	 * It is very unfortunate that we have to override this. However
	 * since dart(4) never needs to change the setting of this register,
	 * ve'll simply keep the serial-related bits and add our own.
	 */

	/* disable timer output during programming */
	dartregs[DART_OPCR] = OPSET;

	/*
	 * The DUART runs at 3.6864 MHz; by using the second waveform
	 * generator we'll get a CLK/32 master rate.
	 */
	limit = (3686400 / 32) / hz;

	dartregs[DART_CTUR] = limit >> 8;
	dartregs[DART_CTLR] = limit & 0xff;
	dartregs[DART_ACR] = BDSET2 | CCLK1 | IPDCDB | IPDCDA;
	dartregs[DART_OPCR] = OPSETTO;
	/* start counter */
	dummy = dartregs[DART_CTSTART];

	/* enable timer interrupts */
	sys_ofobio->csr_b &= ~OFO_CSRB_TIMER_INTDIS;
}
