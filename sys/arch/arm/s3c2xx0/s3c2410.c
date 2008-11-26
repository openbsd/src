/*	$OpenBSD: s3c2410.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
/*	$NetBSD: s3c2410.c,v 1.10 2005/12/11 12:16:51 christos Exp $ */

/*
 * Copyright (c) 2003, 2005  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
//__KERNEL_RCSID(0, "$NetBSD: s3c2410.c,v 1.10 2005/12/11 12:16:51 christos Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/reboot.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <arm/cpufunc.h>
#include <arm/mainbus/mainbus.h>
#include <arm/s3c2xx0/s3c2410reg.h>
#include <arm/s3c2xx0/s3c2410var.h>

/* prototypes */
int	s3c2410_match(struct device *, void *, void *);
void	s3c2410_attach(struct device *, struct device *, void *);
int	s3c2410_search(struct device *, void *, void *);

/* attach structures */
/*
#if 0
CFATTACH_DECL(ssio, sizeof(struct s3c24x0_softc), s3c2410_match, s3c2410_attach,
    NULL, NULL);
#endif
*/

struct cfattach ssio_ca = {
        sizeof(struct s3c24x0_softc), s3c2410_match, s3c2410_attach
};

struct cfdriver ssio_cd = {
	NULL, "ssio", DV_DULL
};


extern struct bus_space s3c2xx0_bs_tag;

struct s3c2xx0_softc *s3c2xx0_softc;

#ifdef DEBUG_PORTF
volatile uint8_t *portf;	/* for debug */
#endif

static int
s3c2410_print(void *aux, const char *name)
{
	struct s3c2xx0_attach_args *sa = (struct s3c2xx0_attach_args *) aux;

	if (sa->sa_size)
		printf(" addr 0x%lx", sa->sa_addr);
	if (sa->sa_size > 1)
		printf("-0x%lx", sa->sa_addr + sa->sa_size - 1);
	if (sa->sa_intr != -1)
		printf(" intr %d", sa->sa_intr);
	if (sa->sa_index != -1)
		printf(" unit %d", sa->sa_index);

	return (UNCONF);
}

int
s3c2410_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
s3c2410_attach(struct device *parent, struct device *self, void *aux)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) self;
	bus_space_tag_t iot;
	const char *which_registers;	/* for panic message */

#define FAIL(which)  do { \
	which_registers=(which); goto abort; }while(/*CONSTCOND*/0)

	s3c2xx0_softc = &(sc->sc_sx);
	sc->sc_sx.sc_iot = iot = &s3c2xx0_bs_tag;

	if (bus_space_map(iot,
		S3C2410_INTCTL_BASE, S3C2410_INTCTL_SIZE,
		BUS_SPACE_MAP_LINEAR, &sc->sc_sx.sc_intctl_ioh))
		FAIL("intc");
	/* tell register addresses to interrupt handler */
	s3c2410_intr_init(sc);

	/* Map the GPIO registers */
	if (bus_space_map(iot, S3C2410_GPIO_BASE, S3C2410_GPIO_SIZE,
		0, &sc->sc_sx.sc_gpio_ioh))
		FAIL("GPIO");
#ifdef DEBUG_PORTF
	{
		extern volatile uint8_t *portf;
		/* make all ports output */
		bus_space_write_2(iot, sc->sc_sx.sc_gpio_ioh, GPIO_PCONF, 0x5555);
		portf = (volatile uint8_t *)
			((char *)bus_space_vaddr(iot, sc->sc_sx.sc_gpio_ioh) + GPIO_PDATF);
	}
#endif

#if 0
	/* Map the DMA controller registers */
	if (bus_space_map(iot, S3C2410_DMAC_BASE, S3C2410_DMAC_SIZE,
		0, &sc->sc_sx.sc_dmach))
		FAIL("DMAC");
#endif

	/* Memory controller */
	if (bus_space_map(iot, S3C2410_MEMCTL_BASE,
		S3C24X0_MEMCTL_SIZE, 0, &sc->sc_sx.sc_memctl_ioh))
		FAIL("MEMC");
	/* Clock manager */
	if (bus_space_map(iot, S3C2410_CLKMAN_BASE,
		S3C24X0_CLKMAN_SIZE, 0, &sc->sc_sx.sc_clkman_ioh))
		FAIL("CLK");

#if 0
	/* Real time clock */
	if (bus_space_map(iot, S3C2410_RTC_BASE,
		S3C24X0_RTC_SIZE, 0, &sc->sc_sx.sc_rtc_ioh))
		FAIL("RTC");
#endif

	if (bus_space_map(iot, S3C2410_TIMER_BASE,
		S3C24X0_TIMER_SIZE, 0, &sc->sc_timer_ioh))
		FAIL("TIMER");

	/* calculate current clock frequency */
	s3c24x0_clock_freq(&sc->sc_sx);
	printf(": fclk %d MHz hclk %d MHz pclk %d MHz\n",
	       sc->sc_sx.sc_fclk / 1000000, sc->sc_sx.sc_hclk / 1000000,
	       sc->sc_sx.sc_pclk / 1000000);

	/* get busdma tag for the platform */
	sc->sc_sx.sc_dmat = s3c2xx0_bus_dma_init(&s3c2xx0_bus_dma);

	/*
	 *  Attach devices.
	 */
	config_search(s3c2410_search, self, sc);

	return;

abort:
	panic("%s: unable to map %s registers",
	    self->dv_xname, which_registers);

#undef FAIL
}

int
s3c2410_search(struct device * parent, void * c, void *aux)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) parent;
	struct s3c2xx0_attach_args aa;
	struct cfdata   *cf = c;


	aa.sa_sc = sc;
	aa.sa_iot = sc->sc_sx.sc_iot;
	#if 0
	aa.sa_addr = cf->cf_loc[SSIOCF_ADDR];
	aa.sa_size = cf->cf_loc[SSIOCF_SIZE];
	aa.sa_index = cf->cf_loc[SSIOCF_INDEX];
	aa.sa_intr = cf->cf_loc[SSIOCF_INTR];
	#else
	aa.sa_addr = cf->cf_loc[0];
	aa.sa_size = cf->cf_loc[1];
	aa.sa_index = cf->cf_loc[2];
	aa.sa_intr = cf->cf_loc[3];
	#endif

	aa.sa_dmat = sc->sc_sx.sc_dmat;

        config_found(parent, &aa, s3c2410_print);

	return 0;
}

/*
 * fill sc_pclk, sc_hclk, sc_fclk from values of clock controller register.
 *
 * s3c24x0_clock_freq2() is meant to be called from kernel startup routines.
 * s3c24x0_clock_freq() is for after kernel initialization is done.
 */
void
s3c24x0_clock_freq2(vaddr_t clkman_base, int *fclk, int *hclk, int *pclk)
{
	uint32_t pllcon, divn;
	int mdiv, pdiv, sdiv;
	int f, h, p;

	pllcon = *(volatile uint32_t *)(clkman_base + CLKMAN_MPLLCON);
	divn = *(volatile uint32_t *)(clkman_base + CLKMAN_CLKDIVN);

	mdiv = (pllcon & PLLCON_MDIV_MASK) >> PLLCON_MDIV_SHIFT;
	pdiv = (pllcon & PLLCON_PDIV_MASK) >> PLLCON_PDIV_SHIFT;
	sdiv = (pllcon & PLLCON_SDIV_MASK) >> PLLCON_SDIV_SHIFT;

	f = ((mdiv + 8) * S3C2XX0_XTAL_CLK) / ((pdiv + 2) * (1 << sdiv));
	h = f;
	if (divn & CLKDIVN_HDIVN)
		h /= 2;
	p = h;
	if (divn & CLKDIVN_PDIVN)
		p /= 2;

	if (fclk) *fclk = f;
	if (hclk) *hclk = h;
	if (pclk) *pclk = p;

}

void
s3c24x0_clock_freq(struct s3c2xx0_softc *sc)
{
	s3c24x0_clock_freq2(
		(vaddr_t)bus_space_vaddr(sc->sc_iot, sc->sc_clkman_ioh),
		&sc->sc_fclk, &sc->sc_hclk, &sc->sc_pclk);
}

/*
 * Issue software reset command.
 * called with MMU off.
 *
 * S3C2410 doesn't have sowtware reset bit like S3C2800.
 * use watch dog timer and make it fire immediately.
 */
void
s3c2410_softreset(void)
{
	disable_interrupts(I32_bit|F32_bit);

	*(volatile unsigned int *)(S3C2410_WDT_BASE + WDT_WTCON)
		= (0 << WTCON_PRESCALE_SHIFT) | WTCON_ENABLE |
		WTCON_CLKSEL_16 | WTCON_ENRST;
}


