/*	$OpenBSD: pxa2x0.c,v 1.19 2014/10/18 12:21:56 miod Exp $ */
/*	$NetBSD: pxa2x0.c,v 1.5 2003/12/12 16:42:44 thorpej Exp $ */

/*
 * Copyright (c) 2002  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
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
 *	This product includes software developed for the NetBSD Project by
 *	Genetec Corporation.
 * 4. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Autoconfiguration support for the Intel PXA2[15]0 application
 * processor. This code is derived from arm/sa11x0/sa11x0.c
 */

/*-
 * Copyright (c) 2001, The NetBSD Foundation, Inc.  All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro and Ichiro FUKUHARA.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
/*-
 * Copyright (c) 1999
 *         Shin Takemura and PocketBSD Project. All rights reserved.
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
 *	This product includes software developed by the PocketBSD project
 *	and its contributors.
 * 4. Neither the name of the project nor the names of its contributors
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

#include "pxagpio.h"
#include "pxadmac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/timetc.h>

#include <machine/cpu.h>
#include <machine/bus.h>

#include <arm/cpufunc.h>
#include <arm/mainbus/mainbus.h>
#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>

struct pxaip_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_dma_tag_t sc_dmat;
	bus_space_handle_t sc_bush_clk;
	bus_space_handle_t sc_bush_rtc;
};

/* prototypes */
int	pxaip_match(struct device *, void *, void *);
void	pxaip_attach(struct device *, struct device *, void *);
int 	pxaip_search(struct device *, void *, void *);
void	pxaip_attach_critical(struct pxaip_softc *);
int	pxaip_print(void *, const char *);

int	pxaip_measure_cpuclock(struct pxaip_softc *);

/* attach structures */
#ifdef __NetBSD__
CFATTACH_DECL(pxaip, sizeof(struct pxaip_softc),
    pxaip_match, pxaip_attach, NULL, NULL);
#else
struct cfattach pxaip_ca = {
	sizeof(struct pxaip_softc), pxaip_match, pxaip_attach
};

struct cfdriver pxaip_cd = {
	NULL, "pxaip", DV_DULL
};
#endif

struct pxaip_softc *pxaip_sc;

int
pxaip_match(struct device *parent, void *match, void *aux)
{

	return 1;
}

void
pxaip_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxaip_softc *sc = (struct pxaip_softc *)self;
#ifdef __APM__
	extern int freq;
#endif
	int cpuclock;

	pxaip_sc = sc;
	sc->sc_bust = &pxa2x0_bs_tag;
	sc->sc_dmat = &pxa2x0_bus_dma_tag;

	if (bus_space_map(sc->sc_bust, PXA2X0_CLKMAN_BASE, PXA2X0_CLKMAN_SIZE,
	    0, &sc->sc_bush_clk))
		panic("pxaip_attach: failed to map CLKMAN");

	if (bus_space_map(sc->sc_bust, PXA2X0_RTC_BASE, PXA2X0_RTC_SIZE,
	    0, &sc->sc_bush_rtc))
		panic("pxaip_attach: failed to map RTC");

	/*
	 * Calculate clock speed
	 * This takes 2 secs at most.
	 */
	cpuclock = pxaip_measure_cpuclock(sc) / 1000;
	if (cpuclock % 1000 > 500)
		cpuclock = cpuclock + 1000 - cpuclock % 1000;
#ifdef __APM__
	freq = cpuclock / 1000;
#endif

	printf(": CPU clock = %d.%03d MHz\n", cpuclock/1000, cpuclock%1000);

	/*
	 * Attach critical devices
	 */
	pxaip_attach_critical(sc);

	/*
	 * Attach all other devices
	 */
	config_search(pxaip_search, self, sc);

}

int
pxaip_search(struct device *parent, void *c, void *aux)
{
	struct pxaip_softc *sc = aux;
	struct pxaip_attach_args aa;
	struct cfdata	*cf = c;

        aa.pxa_iot = sc->sc_bust;
        aa.pxa_dmat = sc->sc_dmat;
#if 0
        aa.pxa_addr = cf->cf_addr;
        aa.pxa_size = cf->cf_size;
        aa.pxa_intr = cf->cf_intr;
	aa.pxa_index = cf->cf_index;
#else
        aa.pxa_addr = (cf->cf_loc)[0];
        aa.pxa_size = (cf->cf_loc)[1];
        aa.pxa_intr = (cf->cf_loc)[2];
	aa.pxa_index = (cf->cf_loc)[3];
#endif

	config_found(parent, &aa, pxaip_print);

        return 0;
}

void
pxaip_attach_critical(struct pxaip_softc *sc)
{
	struct pxaip_attach_args aa;

        aa.pxa_iot = sc->sc_bust;
        aa.pxa_dmat = sc->sc_dmat;
        aa.pxa_addr = PXA2X0_INTCTL_BASE;
        aa.pxa_size = PXA2X0_INTCTL_SIZE;
        aa.pxa_intr = -1;
	if (config_found(&sc->sc_dev, &aa, pxaip_print) == NULL)
		panic("pxaip_attach_critical: failed to attach INTC!");

#if NPXAGPIO > 0
        aa.pxa_iot = sc->sc_bust;
        aa.pxa_dmat = sc->sc_dmat;
        aa.pxa_addr = PXA2X0_GPIO_BASE;
        aa.pxa_size = PXA2X0_GPIO_SIZE;
        aa.pxa_intr = -1;
	if (config_found(&sc->sc_dev, &aa, pxaip_print) == NULL)
		panic("pxaip_attach_critical: failed to attach GPIO!");
#endif

#if NPXADMAC > 0
        aa.pxa_iot = sc->sc_bust;
        aa.pxa_dmat = sc->sc_dmat;
        aa.pxa_addr = PXA2X0_DMAC_BASE;
        aa.pxa_size = PXA2X0_DMAC_SIZE;
        aa.pxa_intr = PXA2X0_INT_DMA;
	if (config_found(&sc->sc_dev, &aa, pxaip_print) == NULL)
		panic("pxaip_attach_critical: failed to attach DMAC!");
#endif
}

int
pxaip_print(void *aux, const char *name)
{
	struct pxaip_attach_args *sa = (struct pxaip_attach_args*)aux;

	if (sa->pxa_addr != -1) {
                printf(" addr 0x%lx", sa->pxa_addr);
	        if (sa->pxa_size > -1)
	                printf("-0x%lx", sa->pxa_addr + sa->pxa_size-1);
	}
        if (sa->pxa_intr != -1)
                printf(" intr %d", sa->pxa_intr);

        return (UNCONF);
}

static inline uint32_t
read_clock_counter(void)
{
  uint32_t x;
  __asm volatile("mrc	p14, 0, %0, c1, c1, 0" : "=r" (x));

  return x;
}

int
pxaip_measure_cpuclock(struct pxaip_softc *sc)
{
	uint32_t rtc0, rtc1, start, end;
	uint32_t pmcr_save;
	bus_space_handle_t ioh;
	int irq;

	ioh = sc->sc_bush_rtc;
	irq = disable_interrupts(I32_bit|F32_bit);

	__asm volatile( "mrc p14, 0, %0, c0, c1, 0" : "=r" (pmcr_save));
	/* Enable clock counter */
	__asm volatile( "mcr p14, 0, %0, c0, c1, 0" : : "r" (0x0001));

	rtc0 = bus_space_read_4(sc->sc_bust, ioh, RTC_RCNR);
	/* Wait for next second starts */
	while ((rtc1 = bus_space_read_4(sc->sc_bust, ioh, RTC_RCNR)) == rtc0)
		;
	start = read_clock_counter();
	while(rtc1 == bus_space_read_4(sc->sc_bust, ioh, RTC_RCNR))
		;		/* Wait for 1sec */
	end = read_clock_counter();

	__asm volatile( "mcr p14, 0, %0, c0, c1, 0" : : "r" (pmcr_save));
	restore_interrupts(irq);

	return end - start;
}

void
pxa2x0_turbo_mode(int f)
{
	__asm volatile("mcr p14, 0, %0, c6, c0, 0" : : "r" (f));
}

void
pxa2x0_probe_sdram(vaddr_t memctl_va, paddr_t *start, paddr_t *size)
{
	u_int32_t mdcnfg, dwid, dcac, drac, dnb;
	int i;

	mdcnfg = *((volatile u_int32_t *)(memctl_va + MEMCTL_MDCNFG));

	/*
	 * Scan all 4 SDRAM banks
	 */
	for (i = 0; i < PXA2X0_SDRAM_BANKS; i++) {
		start[i] = 0;
		size[i] = 0;

		switch (i) {
		case 0:
		case 1:
			if ((i == 0 && (mdcnfg & MDCNFG_DE0) == 0) ||
			    (i == 1 && (mdcnfg & MDCNFG_DE1) == 0))
				continue;
			dwid = mdcnfg >> MDCNFD_DWID01_SHIFT;
			dcac = mdcnfg >> MDCNFD_DCAC01_SHIFT;
			drac = mdcnfg >> MDCNFD_DRAC01_SHIFT;
			dnb = mdcnfg >> MDCNFD_DNB01_SHIFT;
			break;

		case 2:
		case 3:
			if ((i == 2 && (mdcnfg & MDCNFG_DE2) == 0) ||
			    (i == 3 && (mdcnfg & MDCNFG_DE3) == 0))
				continue;
			dwid = mdcnfg >> MDCNFD_DWID23_SHIFT;
			dcac = mdcnfg >> MDCNFD_DCAC23_SHIFT;
			drac = mdcnfg >> MDCNFD_DRAC23_SHIFT;
			dnb = mdcnfg >> MDCNFD_DNB23_SHIFT;
			break;
		default:
			panic("pxa2x0_probe_sdram: impossible");
		}

		dwid = 2 << (1 - (dwid & MDCNFD_DWID_MASK));  /* 16/32 width */
		dcac = 1 << ((dcac & MDCNFD_DCAC_MASK) + 8);  /* 8-11 columns */
		drac = 1 << ((drac & MDCNFD_DRAC_MASK) + 11); /* 11-13 rows */
		dnb = 2 << (dnb & MDCNFD_DNB_MASK);	      /* # of banks */

		size[i] = (paddr_t)(dwid * dcac * drac * dnb);
		start[i] = PXA2X0_SDRAM0_START + (i * PXA2X0_SDRAM_BANK_SIZE);
	}
}

void
pxa2x0_clkman_config(u_int clk, int enable)
{
	struct pxaip_softc *sc;
	u_int32_t rv;

	KDASSERT(pxaip_sc != NULL);
	sc = pxaip_sc;

	rv = bus_space_read_4(sc->sc_bust, sc->sc_bush_clk, CLKMAN_CKEN);
	rv &= ~clk;

	if (enable)
		rv |= clk;

	bus_space_write_4(sc->sc_bust, sc->sc_bush_clk, CLKMAN_CKEN, rv);
}

void
pxa2x0_rtc_setalarm(u_int32_t secs)
{
	struct pxaip_softc *sc;
	u_int32_t rv;
	int s;

	KDASSERT(pxaip_sc != NULL);
	sc = pxaip_sc;

	s = splhigh();
	bus_space_write_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RTAR, secs);
	rv = bus_space_read_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RTSR);
	if (secs == 0)
		bus_space_write_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RTSR,
		    (rv | RTSR_AL) & ~RTSR_ALE);
	else
		bus_space_write_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RTSR,
		    (rv | RTSR_AL | RTSR_ALE));
	splx(s);
}

u_int32_t
pxa2x0_rtc_getalarm(void)
{
	struct pxaip_softc *sc;

	KDASSERT(pxaip_sc != NULL);
	sc = pxaip_sc;

	return (bus_space_read_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RTAR));
}

u_int32_t
pxa2x0_rtc_getsecs(void)
{
	struct pxaip_softc *sc;

	KDASSERT(pxaip_sc != NULL);
	sc = pxaip_sc;

	return (bus_space_read_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RCNR));
}

void
resettodr(void)
{
	struct pxaip_softc *sc = pxaip_sc;
	struct timeval tv;

	microtime(&tv);

	bus_space_write_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RCNR,
	    (u_int32_t)tv.tv_sec);
}

void
inittodr(time_t base)
{
	struct pxaip_softc *sc = pxaip_sc;
	struct timespec ts;
	u_int32_t rcnr;

	/* XXX decide if RCNR can be valid, based on the last reset
	 * XXX reason, i.e. RCSR. */
	rcnr = bus_space_read_4(sc->sc_bust, sc->sc_bush_rtc, RTC_RCNR);

	/* XXX check how much RCNR differs from the filesystem date. */
	if (rcnr > base)
		ts.tv_sec = rcnr;
	else {
		printf("WARNING: using filesystem date -- CHECK AND RESET THE DATE!\n");
		ts.tv_sec = base;
	}

	ts.tv_nsec = 0;
	tc_setclock(&ts);
}
