/* $NetBSD: if_awi_pcmcia.c,v 1.13 2000/03/22 11:22:20 onoe Exp $ */
/* $OpenBSD: if_awi_pcmcia.c,v 1.13 2004/11/23 21:12:23 fgsch Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PCMCIA attachment for BayStack 650 802.11FH PCMCIA card,
 * based on the AMD 79c930 802.11 controller chip.
 *
 * This attachment can probably be trivially adapted for other FH and
 * DS cards based on the same chipset.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

struct awi_pcmcia_softc {
	struct awi_softc sc_awi;		/* real "awi" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	struct pcmcia_mem_handle sc_memh;	/* PCMCIA memory space info */
	int sc_io_window;			/* our i/o window */
	int sc_mem_window;			/* our memory window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
	void *sc_powerhook;			/* power hook descriptor */
};

static int awi_pcmcia_match(struct device *, void *, void *);
static void awi_pcmcia_attach(struct device *, struct device *, void *);
static int awi_pcmcia_detach(struct device *, int);
static int awi_pcmcia_enable(struct awi_softc *);
static void awi_pcmcia_disable(struct awi_softc *);
static void awi_pcmcia_powerhook(int, void *);

static int	awi_pcmcia_find(struct awi_pcmcia_softc *,
    struct pcmcia_attach_args *, struct pcmcia_config_entry *);

struct cfattach awi_pcmcia_ca = {
	sizeof(struct awi_pcmcia_softc), awi_pcmcia_match, awi_pcmcia_attach,
	awi_pcmcia_detach, awi_activate
};

static struct awi_pcmcia_product {
	u_int16_t	app_vendor;	/* vendor ID */
	u_int16_t	app_product;	/* product ID */
	const char	*app_cisinfo[4]; /* CIS information */
	const char	*app_name;	/* product name */
} awi_pcmcia_products[] = {
	{ PCMCIA_VENDOR_BAY,		PCMCIA_PRODUCT_BAY_STACK_650,
	  PCMCIA_CIS_BAY_STACK_650,	"BayStack 650" },

	{ PCMCIA_VENDOR_BAY,		PCMCIA_PRODUCT_BAY_STACK_660,
	  PCMCIA_CIS_BAY_STACK_660,	"BayStack 660" },

	{ PCMCIA_VENDOR_BAY,		PCMCIA_PRODUCT_BAY_SURFER_PRO,
	  PCMCIA_CIS_BAY_SURFER_PRO,	"AirSurfer Pro" },

	{ PCMCIA_VENDOR_AMD,		PCMCIA_PRODUCT_AMD_AM79C930,
	  PCMCIA_CIS_AMD_AM79C930,	"AMD AM79C930" },

	{ PCMCIA_VENDOR_ICOM,		PCMCIA_PRODUCT_ICOM_SL200,
	  PCMCIA_CIS_ICOM_SL200,	"Icom SL-200" },

	{ PCMCIA_VENDOR_NOKIA,		PCMCIA_PRODUCT_NOKIA_C020_WLAN,
	  PCMCIA_CIS_NOKIA_C020_WLAN,	"Nokia C020" },

	{ PCMCIA_VENDOR_FARALLON,	PCMCIA_PRODUCT_FARALLON_SKYLINE,
	  PCMCIA_CIS_FARALLON_SKYLINE,	"SkyLINE Wireless" },

	{ PCMCIA_VENDOR_ZOOM,		PCMCIA_PRODUCT_ZOOM_AIR4000,
	  PCMCIA_CIS_ZOOM_AIR4000,	"Zoom Air-4000" },

/*	{ PCMCIA_VENDOR_BREEZECOM,	PCMCIA_PRODUCT_BREEZECOM_BREEZENET,
	  PCMCIA_CIS_BREEZECOM_BREEZENET,	"BreezeNet SC-PX" },
*/
	{ 0,				0,
	  { NULL, NULL, NULL, NULL },	NULL },
};

static struct awi_pcmcia_product *
	awi_pcmcia_lookup(struct pcmcia_attach_args *);

static struct awi_pcmcia_product *
awi_pcmcia_lookup(pa)
	struct pcmcia_attach_args *pa;
{
	struct awi_pcmcia_product *app;

	for (app = awi_pcmcia_products; app->app_name != NULL; app++) {
		/* match by vendor/product id */
		if (pa->manufacturer != PCMCIA_VENDOR_INVALID &&
		    pa->manufacturer == app->app_vendor &&
		    pa->product != PCMCIA_PRODUCT_INVALID &&
		    pa->product == app->app_product)
			return (app);

		/* match by CIS information */
		if (pa->card->cis1_info[0] != NULL &&
		    app->app_cisinfo[0] != NULL &&
		    strcmp(pa->card->cis1_info[0], app->app_cisinfo[0]) == 0 &&
		    pa->card->cis1_info[1] != NULL &&
		    app->app_cisinfo[1] != NULL &&
		    strcmp(pa->card->cis1_info[1], app->app_cisinfo[1]) == 0)
			return (app);
	}

	return (NULL);
}

static int
awi_pcmcia_enable(sc)
	struct awi_softc *sc;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *)sc;
	struct pcmcia_function *pf = psc->sc_pf;

	/* establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(pf, IPL_NET, awi_intr,
	    sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	if (pcmcia_function_enable(pf)) {
		pcmcia_intr_disestablish(pf, sc->sc_ih);
		return (1);
	}
	DELAY(1000);

	return (0);
}

static void
awi_pcmcia_disable(sc)
	struct awi_softc *sc;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *)sc;
	struct pcmcia_function *pf = psc->sc_pf;

	pcmcia_intr_disestablish (pf, sc->sc_ih);
	pcmcia_function_disable (pf);
}

static int
awi_pcmcia_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (awi_pcmcia_lookup(pa) != NULL)
		return (1);

	return (0);
}

static int
awi_pcmcia_find(psc, pa, cfe)
	struct awi_pcmcia_softc *psc;
	struct pcmcia_attach_args *pa;
	struct pcmcia_config_entry *cfe;
{
	struct awi_softc *sc = &psc->sc_awi;
	int fail = 0;
	u_int8_t version[AWI_BANNER_LEN];

	/*
	 * see if we can read the firmware version sanely
	 * through the i/o ports.
	 * if not, try a different CIS string..
	 */
	if (pcmcia_io_alloc(psc->sc_pf, cfe->iospace[0].start,
	    cfe->iospace[0].length, AM79C930_IO_ALIGN,
	    &psc->sc_pcioh) != 0)
		goto fail;

	if (pcmcia_io_map(psc->sc_pf, PCMCIA_WIDTH_AUTO, 0, psc->sc_pcioh.size,
	    &psc->sc_pcioh, &psc->sc_io_window))
		goto fail_io_free;

	/* Enable the card. */
	pcmcia_function_init(psc->sc_pf, cfe);
	if (pcmcia_function_enable(psc->sc_pf))
		goto fail_io_unmap;

	sc->sc_chip.sc_bustype = AM79C930_BUS_PCMCIA;
	sc->sc_chip.sc_iot = psc->sc_pcioh.iot;
	sc->sc_chip.sc_ioh = psc->sc_pcioh.ioh;
	am79c930_chip_init(&sc->sc_chip, 0);

	DELAY(1000);

	awi_read_bytes(sc, AWI_BANNER, version, AWI_BANNER_LEN);

	if (memcmp(version, "PCnetMobile:", 12) == 0)
		return (0);

	fail++;
	pcmcia_function_disable(psc->sc_pf);

 fail_io_unmap:
	fail++;
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);

 fail_io_free:
	fail++;
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);
 fail:
	fail++;
	psc->sc_io_window = -1;
	return (fail);
}

static void
awi_pcmcia_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct awi_pcmcia_softc *psc = (void *)self;
	struct awi_softc *sc = &psc->sc_awi;
	struct awi_pcmcia_product *app;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
#if 0
	bus_addr_t memoff;
#endif

	app = awi_pcmcia_lookup(pa);
	if (app == NULL)
		panic("awi_pcmcia_attach: impossible");

	psc->sc_pf = pa->pf;

	for (cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head); cfe != NULL;
	     cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {
		if (cfe->iftype != PCMCIA_IFTYPE_IO)
			continue;
		if (cfe->num_iospace < 1)
			continue;
		if (cfe->iospace[0].length < AM79C930_IO_SIZE)
			continue;

		if (awi_pcmcia_find(psc, pa, cfe) == 0)
			break;
	}
	if (cfe == NULL) {
		printf(": no suitable CIS info found\n");
		goto no_config_entry;
	}

	sc->sc_enabled = 1;
	printf(": %s\n", app->app_name);

	psc->sc_mem_window = -1;
#if 0	/* if memory mode is disabled it futhers futher */
	if (pcmcia_mem_alloc(psc->sc_pf, AM79C930_MEM_SIZE,
	    &psc->sc_memh) != 0) {
		printf("%s: unable to allocate memory space; using i/o only\n",
		    sc->sc_dev.dv_xname);
	} else if (pcmcia_mem_map(psc->sc_pf,
	    PCMCIA_MEM_COMMON, AM79C930_MEM_BASE,
	    AM79C930_MEM_SIZE, &psc->sc_memh, &memoff, &psc->sc_mem_window)) {
		printf("%s: unable to map memory space; using i/o only\n",
		    sc->sc_dev.dv_xname);
		pcmcia_mem_free(psc->sc_pf, &psc->sc_memh);
	} else {
		sc->sc_chip.sc_memt = psc->sc_memh.memt;
		sc->sc_chip.sc_memh = psc->sc_memh.memh;
		am79c930_chip_init(&sc->sc_chip, 1);
	}
#endif

	sc->sc_enable = awi_pcmcia_enable;
	sc->sc_disable = awi_pcmcia_disable;

	/* establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(psc->sc_pf, IPL_NET,
	    awi_intr, sc, "");
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		goto no_interrupt;
	}
	sc->sc_ifp = &sc->sc_arpcom.ac_if;
	sc->sc_cansleep = 1;

	if (awi_attach(sc) != 0) {
		printf("%s: failed to attach controller\n",
		    sc->sc_dev.dv_xname);
		goto attach_failed;
	}
	psc->sc_powerhook = powerhook_establish(awi_pcmcia_powerhook, psc);

	sc->sc_enabled = 0;
	/* disable device and disestablish the interrupt */
	awi_pcmcia_disable(sc);
	return;

 attach_failed:
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);

 no_interrupt:
	/* Unmap our memory window and space */
	if (psc->sc_mem_window != -1) {
		pcmcia_mem_unmap(psc->sc_pf, psc->sc_mem_window);
		pcmcia_mem_free(psc->sc_pf, &psc->sc_memh);
	}

	/* Unmap our i/o window and space */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

	/* Disable the function */
	pcmcia_function_disable(psc->sc_pf);

 no_config_entry:
	psc->sc_io_window = -1;
}


static int
awi_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *)self;
	int error;

	if (psc->sc_io_window == -1)
		/* Nothing to detach. */
		return (0);

	if (psc->sc_powerhook != NULL)
		powerhook_disestablish(psc->sc_powerhook);

	error = awi_detach(&psc->sc_awi);
	if (error != 0)
		return (error);

	/* Unmap our memory window and free memory space */
	if (psc->sc_mem_window != -1) {
		pcmcia_mem_unmap(psc->sc_pf, psc->sc_mem_window);
		pcmcia_mem_free(psc->sc_pf, &psc->sc_memh);
	}

	/* Unmap our i/o window. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);

	/* Free our i/o space. */
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);
	return (0);
}

static void
awi_pcmcia_powerhook(why, arg)
	int why;
	void *arg;
{
	struct awi_pcmcia_softc *psc = arg;
	struct awi_softc *sc = &psc->sc_awi;

	awi_power(sc, why);
}
