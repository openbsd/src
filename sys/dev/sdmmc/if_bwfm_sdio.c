/* $OpenBSD: if_bwfm_sdio.c,v 1.3 2018/02/07 22:08:24 patrick Exp $ */
/*
 * Copyright (c) 2010-2016 Broadcom Corporation
 * Copyright (c) 2016,2017 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/socket.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/sdmmc/sdmmcvar.h>

#include <dev/ic/bwfmvar.h>
#include <dev/ic/bwfmreg.h>
#include <dev/sdmmc/if_bwfm_sdio.h>

#define BWFM_SDIO_CCCR_BRCM_CARDCAP			0xf0
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD14_SUPPORT	0x02
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD14_EXT		0x04
#define  BWFM_SDIO_CCCR_BRCM_CARDCAP_CMD_NODEC		0x08
#define BWFM_SDIO_CCCR_BRCM_CARDCTRL			0xf1
#define  BWFM_SDIO_CCCR_BRCM_CARDCTRL_WLANRESET		0x02
#define BWFM_SDIO_CCCR_BRCM_SEPINT			0xf2

#ifdef BWFM_DEBUG
#define DPRINTF(x)	do { if (bwfm_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (bwfm_debug >= (n)) printf x; } while (0)
static int bwfm_debug = 1;
#else
#define DPRINTF(x)	do { ; } while (0)
#define DPRINTFN(n, x)	do { ; } while (0)
#endif

#undef DEVNAME
#define DEVNAME(sc)	((sc)->sc_sc.sc_dev.dv_xname)

struct bwfm_sdio_softc {
	struct bwfm_softc	  sc_sc;
	struct sdmmc_function	**sc_sf;
	uint32_t		  sc_bar0;
};

int		 bwfm_sdio_match(struct device *, void *, void *);
void		 bwfm_sdio_attach(struct device *, struct device *, void *);
int		 bwfm_sdio_detach(struct device *, int);

void		 bwfm_sdio_backplane(struct bwfm_sdio_softc *, uint32_t);
uint8_t		 bwfm_sdio_read_1(struct bwfm_sdio_softc *, uint32_t);
uint32_t	 bwfm_sdio_read_4(struct bwfm_sdio_softc *, uint32_t);
void		 bwfm_sdio_write_1(struct bwfm_sdio_softc *, uint32_t,
		     uint8_t);
void		 bwfm_sdio_write_4(struct bwfm_sdio_softc *, uint32_t,
		     uint32_t);

uint32_t	 bwfm_sdio_buscore_read(struct bwfm_softc *, uint32_t);
void		 bwfm_sdio_buscore_write(struct bwfm_softc *, uint32_t,
		     uint32_t);
int		 bwfm_sdio_buscore_prepare(struct bwfm_softc *);
void		 bwfm_sdio_buscore_activate(struct bwfm_softc *, uint32_t);

int		 bwfm_sdio_txdata(struct bwfm_softc *, struct mbuf *);
int		 bwfm_sdio_txctl(struct bwfm_softc *, char *, size_t);
int		 bwfm_sdio_rxctl(struct bwfm_softc *, char *, size_t *);

struct bwfm_bus_ops bwfm_sdio_bus_ops = {
	.bs_init = NULL,
	.bs_stop = NULL,
	.bs_txdata = bwfm_sdio_txdata,
	.bs_txctl = bwfm_sdio_txctl,
	.bs_rxctl = bwfm_sdio_rxctl,
};

struct bwfm_buscore_ops bwfm_sdio_buscore_ops = {
	.bc_read = bwfm_sdio_buscore_read,
	.bc_write = bwfm_sdio_buscore_write,
	.bc_prepare = bwfm_sdio_buscore_prepare,
	.bc_reset = NULL,
	.bc_setup = NULL,
	.bc_activate = bwfm_sdio_buscore_activate,
};

struct cfattach bwfm_sdio_ca = {
	sizeof(struct bwfm_sdio_softc),
	bwfm_sdio_match,
	bwfm_sdio_attach,
	bwfm_sdio_detach,
};

int
bwfm_sdio_match(struct device *parent, void *match, void *aux)
{
	struct sdmmc_attach_args *saa = aux;
	struct sdmmc_function *sf = saa->sf;
	struct sdmmc_cis *cis;

	/* Not SDIO. */
	if (sf == NULL)
		return 0;

	/* Look for Broadcom 433[04]. */
	cis = &sf->sc->sc_fn0->cis;
	if (cis->manufacturer != 0x02d0 || (cis->product != 0x4330 &&
	    cis->product != 0x4334))
		return 0;

	/* We need both functions, but ... */
	if (sf->sc->sc_function_count <= 1)
		return 0;

	/* ... only attach for one. */
	if (sf->number != 1)
		return 0;

	return 1;
}

void
bwfm_sdio_attach(struct device *parent, struct device *self, void *aux)
{
	struct bwfm_sdio_softc *sc = (struct bwfm_sdio_softc *)self;
	struct sdmmc_attach_args *saa = aux;
	struct sdmmc_function *sf = saa->sf;
	struct bwfm_core *core;

	printf("\n");

	rw_assert_wrlock(&sf->sc->sc_lock);

	sc->sc_sf = mallocarray(sf->sc->sc_function_count + 1,
	    sizeof(struct sdmmc_function *), M_DEVBUF, M_WAITOK);

	/* Copy all function pointers. */
	SIMPLEQ_FOREACH(sf, &saa->sf->sc->sf_head, sf_list) {
		sc->sc_sf[sf->number] = sf;
	}
	sf = saa->sf;

	/*
	 * TODO: set block size to 64 for func 1, 512 for func 2.
	 * We might need to work on the SDMMC stack to be able to set
	 * a block size per function.  Currently the IO code uses the
	 * SDHC controller's maximum block length.
	 */

	/* Enable Function 1. */
	if (sdmmc_io_function_enable(sc->sc_sf[1]) != 0) {
		printf("%s: cannot enable function 1\n", DEVNAME(sc));
		goto err;
	}

	DPRINTF(("%s: F1 signature read @0x18000000=%x\n", DEVNAME(sc),
	    bwfm_sdio_read_4(sc, 0x18000000)));

	/* Force PLL off */
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR,
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL_REQ);

	sc->sc_sc.sc_buscore_ops = &bwfm_sdio_buscore_ops;
	if (bwfm_chip_attach(&sc->sc_sc) != 0) {
		printf("%s: cannot attach chip\n", DEVNAME(sc));
		goto err;
	}

	/* TODO: drive strength */

	bwfm_sdio_write_1(sc, BWFM_SDIO_CCCR_BRCM_CARDCTRL,
	    bwfm_sdio_read_1(sc, BWFM_SDIO_CCCR_BRCM_CARDCTRL) |
	    BWFM_SDIO_CCCR_BRCM_CARDCTRL_WLANRESET);

	core = bwfm_chip_get_pmu(&sc->sc_sc);
	bwfm_sdio_write_4(sc, core->co_base + BWFM_CHIP_REG_PMUCONTROL,
	    bwfm_sdio_read_4(sc, core->co_base + BWFM_CHIP_REG_PMUCONTROL) |
	    (BWFM_CHIP_REG_PMUCONTROL_RES_RELOAD <<
	     BWFM_CHIP_REG_PMUCONTROL_RES_SHIFT));

	sc->sc_sc.sc_bus_ops = &bwfm_sdio_bus_ops;
	sc->sc_sc.sc_proto_ops = &bwfm_proto_bcdc_ops;
	bwfm_attach(&sc->sc_sc);

	return;

err:
	free(sc->sc_sf, M_DEVBUF, 0);
}

int
bwfm_sdio_detach(struct device *self, int flags)
{
	struct bwfm_sdio_softc *sc = (struct bwfm_sdio_softc *)self;

	bwfm_detach(&sc->sc_sc, flags);

	free(sc->sc_sf, M_DEVBUF, 0);

	return 0;
}

void
bwfm_sdio_backplane(struct bwfm_sdio_softc *sc, uint32_t bar0)
{
	if (sc->sc_bar0 == bar0)
		return;

	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRLOW,
	    (bar0 >>  8) & 0x80);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRMID,
	    (bar0 >> 16) & 0xff);
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SBADDRHIGH,
	    (bar0 >> 24) & 0xff);
	sc->sc_bar0 = bar0;
}

uint8_t
bwfm_sdio_read_1(struct bwfm_sdio_softc *sc, uint32_t addr)
{
	struct sdmmc_function *sf;
	uint8_t rv;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	rv = sdmmc_io_read_1(sf, addr);
	return rv;
}

uint32_t
bwfm_sdio_read_4(struct bwfm_sdio_softc *sc, uint32_t addr)
{
	struct sdmmc_function *sf;
	uint32_t bar0 = addr & ~BWFM_SDIO_SB_OFT_ADDR_MASK;
	uint32_t rv;

	bwfm_sdio_backplane(sc, bar0);

	addr &= BWFM_SDIO_SB_OFT_ADDR_MASK;
	addr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	rv = sdmmc_io_read_4(sf, addr);
	return rv;
}

void
bwfm_sdio_write_1(struct bwfm_sdio_softc *sc, uint32_t addr, uint8_t data)
{
	struct sdmmc_function *sf;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	sdmmc_io_write_1(sf, addr, data);
}

void
bwfm_sdio_write_4(struct bwfm_sdio_softc *sc, uint32_t addr, uint32_t data)
{
	struct sdmmc_function *sf;
	uint32_t bar0 = addr & ~BWFM_SDIO_SB_OFT_ADDR_MASK;

	bwfm_sdio_backplane(sc, bar0);

	addr &= BWFM_SDIO_SB_OFT_ADDR_MASK;
	addr |= BWFM_SDIO_SB_ACCESS_2_4B_FLAG;

	/*
	 * figure out how to read the register based on address range
	 * 0x00 ~ 0x7FF: function 0 CCCR and FBR
	 * 0x10000 ~ 0x1FFFF: function 1 miscellaneous registers
	 * The rest: function 1 silicon backplane core registers
	 */
	if ((addr & ~0x7ff) == 0)
		sf = sc->sc_sf[0];
	else
		sf = sc->sc_sf[1];

	sdmmc_io_write_4(sf, addr, data);
}

uint32_t
bwfm_sdio_buscore_read(struct bwfm_softc *bwfm, uint32_t reg)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;
	uint32_t val;

	val = bwfm_sdio_read_4(sc, reg);
	/* TODO: Workaround for 4335/4339 */

	return val;
}

void
bwfm_sdio_buscore_write(struct bwfm_softc *bwfm, uint32_t reg, uint32_t val)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;
	bwfm_sdio_write_4(sc, reg, val);
}

int
bwfm_sdio_buscore_prepare(struct bwfm_softc *bwfm)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;
	uint8_t clkval, clkset, clkmask;
	int i;

	clkset = BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL_REQ |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF;
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, clkset);

	clkmask = BWFM_SDIO_FUNC1_CHIPCLKCSR_ALP_AVAIL |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_HT_AVAIL;
	clkval = bwfm_sdio_read_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR);

	if ((clkval & ~clkmask) != clkset) {
		printf("%s: wrote 0x%02x read 0x%02x\n", DEVNAME(sc),
		    clkset, clkval);
		return 1;
	}

	for (i = 1000; i > 0; i--) {
		clkval = bwfm_sdio_read_1(sc,
		    BWFM_SDIO_FUNC1_CHIPCLKCSR);
		if (clkval & clkmask)
			break;
	}
	if (i == 0) {
		printf("%s: timeout on ALPAV wait, clkval 0x%02x\n",
		    DEVNAME(sc), clkval);
		return 1;
	}

	clkset = BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_HW_CLKREQ_OFF |
	    BWFM_SDIO_FUNC1_CHIPCLKCSR_FORCE_ALP;
	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_CHIPCLKCSR, clkset);
	delay(65);

	bwfm_sdio_write_1(sc, BWFM_SDIO_FUNC1_SDIOPULLUP, 0);

	return 0;
}

void
bwfm_sdio_buscore_activate(struct bwfm_softc *bwfm, uint32_t rstvec)
{
	struct bwfm_sdio_softc *sc = (void *)bwfm;
	struct bwfm_core *core;

	core = bwfm_chip_get_core(&sc->sc_sc, BWFM_AGENT_CORE_SDIO_DEV);
	bwfm_sdio_buscore_write(&sc->sc_sc,
	    core->co_base + BWFM_SDPCMD_INTSTATUS, 0xFFFFFFFF);

#if notyet
	if (rstvec)
		bwfm_sdio_ram_write(&sc->sc_sc, 0, &rstvec, sizeof(rstvec));
#endif
}

int
bwfm_sdio_txdata(struct bwfm_softc *bwfm, struct mbuf *m)
{
#ifdef BWFM_DEBUG
	struct bwfm_sdio_softc *sc = (void *)bwfm;
#endif
	int ret = 1;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	return ret;
}

int
bwfm_sdio_txctl(struct bwfm_softc *bwfm, char *buf, size_t len)
{
#ifdef BWFM_DEBUG
	struct bwfm_sdio_softc *sc = (void *)bwfm;
#endif
	int ret = 1;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	return ret;
}

int
bwfm_sdio_rxctl(struct bwfm_softc *bwfm, char *buf, size_t *len)
{
#ifdef BWFM_DEBUG
	struct bwfm_sdio_softc *sc = (void *)bwfm;
#endif
	int ret = 1;

	DPRINTF(("%s: %s\n", DEVNAME(sc), __func__));

	return ret;
}
