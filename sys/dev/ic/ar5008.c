/*	$OpenBSD: ar5008.c,v 1.4 2010/05/16 08:55:39 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2008-2009 Atheros Communications Inc.
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

/*
 * Driver for Atheros 802.11a/g/n chipsets.
 * Routines common to AR5008, AR9001 and AR9002 families.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/stdint.h>	/* uintptr_t */

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/athnreg.h>
#include <dev/ic/athnvar.h>

#include <dev/ic/ar5008reg.h>

int	ar5008_attach(struct athn_softc *);
int	ar5008_read_rom_word(struct athn_softc *, uint32_t, uint16_t *);
int	ar5008_read_rom(struct athn_softc *);
void	ar5008_swap_rom(struct athn_softc *);
int	ar5008_gpio_read(struct athn_softc *, int);
void	ar5008_gpio_write(struct athn_softc *, int, int);
void	ar5008_gpio_config_input(struct athn_softc *, int);
void	ar5008_gpio_config_output(struct athn_softc *, int, int);
void	ar5008_rfsilent_init(struct athn_softc *);
int	ar5008_dma_alloc(struct athn_softc *);
void	ar5008_dma_free(struct athn_softc *);
int	ar5008_tx_alloc(struct athn_softc *);
void	ar5008_tx_free(struct athn_softc *);
int	ar5008_rx_alloc(struct athn_softc *);
void	ar5008_rx_free(struct athn_softc *);
void	ar5008_rx_enable(struct athn_softc *);
void	ar5008_rx_radiotap(struct athn_softc *, struct mbuf *,
	    struct ar_rx_desc *);
void	ar5008_rx_intr(struct athn_softc *);
int	ar5008_tx_process(struct athn_softc *, int);
void	ar5008_tx_intr(struct athn_softc *);
int	ar5008_intr(struct athn_softc *);
int	ar5008_tx(struct athn_softc *, struct mbuf *, struct ieee80211_node *);
void	ar5008_set_rf_mode(struct athn_softc *, struct ieee80211_channel *);
int	ar5008_rf_bus_request(struct athn_softc *);
void	ar5008_rf_bus_release(struct athn_softc *);
void	ar5008_set_phy(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
void	ar5008_set_delta_slope(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
void	ar5008_enable_antenna_diversity(struct athn_softc *);
void	ar5008_init_baseband(struct athn_softc *);
void	ar5008_disable_phy(struct athn_softc *);
void	ar5008_init_chains(struct athn_softc *);
void	ar5008_set_rxchains(struct athn_softc *);
void	ar5008_read_noisefloor(struct athn_softc *, int16_t *, int16_t *);
void	ar5008_write_noisefloor(struct athn_softc *, int16_t *, int16_t *);
void	ar5008_get_noisefloor(struct athn_softc *, struct ieee80211_channel *);
void	ar5008_bb_load_noisefloor(struct athn_softc *);
void	ar5008_noisefloor_calib(struct athn_softc *);
void	ar5008_do_noisefloor_calib(struct athn_softc *);
void	ar5008_do_calib(struct athn_softc *);
void	ar5008_next_calib(struct athn_softc *);
void	ar5008_calib_iq(struct athn_softc *);
void	ar5008_calib_adc_gain(struct athn_softc *);
void	ar5008_calib_adc_dc_off(struct athn_softc *);
void	ar5008_write_txpower(struct athn_softc *, int16_t power[]);
void	ar5008_set_viterbi_mask(struct athn_softc *, int);
void	ar5008_hw_init(struct athn_softc *, struct ieee80211_channel *,
	    struct ieee80211_channel *);
uint8_t	ar5008_get_vpd(uint8_t, const uint8_t *, const uint8_t *, int);
void	ar5008_get_pdadcs(struct athn_softc *, uint8_t, struct athn_pier *,
	    struct athn_pier *, int, int, uint8_t, uint8_t *, uint8_t *);
void	ar5008_get_lg_tpow(struct athn_softc *, struct ieee80211_channel *,
	    uint8_t, const struct ar_cal_target_power_leg *, int, uint8_t[]);
void	ar5008_get_ht_tpow(struct athn_softc *, struct ieee80211_channel *,
	    uint8_t, const struct ar_cal_target_power_ht *, int, uint8_t[]);
void	ar5008_set_noise_immunity_level(struct athn_softc *, int);
void	ar5008_enable_ofdm_weak_signal(struct athn_softc *);
void	ar5008_disable_ofdm_weak_signal(struct athn_softc *);
void	ar5008_set_cck_weak_signal(struct athn_softc *, int);
void	ar5008_set_firstep_level(struct athn_softc *, int);
void	ar5008_set_spur_immunity_level(struct athn_softc *, int);

/* Extern functions. */
void	athn_stop(struct ifnet *, int);
int	athn_interpolate(int, int, int, int, int);
int	athn_txtime(struct athn_softc *, int, int, u_int);
void	athn_inc_tx_trigger_level(struct athn_softc *);
void	athn_get_delta_slope(uint32_t, uint32_t *, uint32_t *);
void	athn_config_pcie(struct athn_softc *);
void	athn_config_nonpcie(struct athn_softc *);
uint8_t	athn_chan2fbin(struct ieee80211_channel *);
uint8_t	ar5416_get_rf_rev(struct athn_softc *);
void	ar5416_reset_addac(struct athn_softc *, struct ieee80211_channel *);
void	ar5416_rf_reset(struct athn_softc *, struct ieee80211_channel *);
void	ar5416_reset_bb_gain(struct athn_softc *, struct ieee80211_channel *);
void	ar9280_reset_rx_gain(struct athn_softc *, struct ieee80211_channel *);
void	ar9280_reset_tx_gain(struct athn_softc *, struct ieee80211_channel *);


int
ar5008_attach(struct athn_softc *sc)
{
	struct athn_ops *ops = &sc->ops;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ar_base_eep_header *base;
	uint8_t eep_ver, kc_entries_log;
	int error;

	/* Set callbacks for AR5008, AR9001 and AR9002 families. */
	ops->gpio_read = ar5008_gpio_read;
	ops->gpio_write = ar5008_gpio_write;
	ops->gpio_config_input = ar5008_gpio_config_input;
	ops->gpio_config_output = ar5008_gpio_config_output;
	ops->rfsilent_init = ar5008_rfsilent_init;

	ops->dma_alloc = ar5008_dma_alloc;
	ops->dma_free = ar5008_dma_free;
	ops->rx_enable = ar5008_rx_enable;
	ops->intr = ar5008_intr;
	ops->tx = ar5008_tx;

	ops->set_rf_mode = ar5008_set_rf_mode;
	ops->rf_bus_request = ar5008_rf_bus_request;
	ops->rf_bus_release = ar5008_rf_bus_release;
	ops->set_phy = ar5008_set_phy;
	ops->set_delta_slope = ar5008_set_delta_slope;
	ops->enable_antenna_diversity = ar5008_enable_antenna_diversity;
	ops->init_baseband = ar5008_init_baseband;
	ops->disable_phy = ar5008_disable_phy;
	ops->set_rxchains = ar5008_set_rxchains;
	ops->noisefloor_calib = ar5008_do_noisefloor_calib;
	ops->do_calib = ar5008_do_calib;
	ops->next_calib = ar5008_next_calib;
	ops->hw_init = ar5008_hw_init;

	ops->set_noise_immunity_level = ar5008_set_noise_immunity_level;
	ops->enable_ofdm_weak_signal = ar5008_enable_ofdm_weak_signal;
	ops->disable_ofdm_weak_signal = ar5008_disable_ofdm_weak_signal;
	ops->set_cck_weak_signal = ar5008_set_cck_weak_signal;
	ops->set_firstep_level = ar5008_set_firstep_level;
	ops->set_spur_immunity_level = ar5008_set_spur_immunity_level;

	/* Set MAC registers offsets. */
	sc->obs_off = AR_OBS;
	sc->gpio_input_en_off = AR_GPIO_INPUT_EN_VAL;

	if (!(sc->flags & ATHN_FLAG_PCIE))
		athn_config_nonpcie(sc);
	else
		athn_config_pcie(sc);

	/* Read entire ROM content in memory. */
	if ((error = ar5008_read_rom(sc)) != 0) {
		printf(": could not read ROM\n");
		return (error);
	}

	/* Get RF revision. */
	sc->rf_rev = ar5416_get_rf_rev(sc);

	base = sc->eep;
	eep_ver = (base->version >> 12) & 0xf;
	sc->eep_rev = (base->version & 0xfff);
	if (eep_ver != AR_EEP_VER || sc->eep_rev == 0) {
		printf(": unsupported ROM version %d.%d\n",  eep_ver,
		    sc->eep_rev);
		return (EINVAL);
	}

	if (base->opCapFlags & AR_OPFLAGS_11A)
		sc->flags |= ATHN_FLAG_11A;
	if (base->opCapFlags & AR_OPFLAGS_11G)
		sc->flags |= ATHN_FLAG_11G;
	if (base->opCapFlags & AR_OPFLAGS_11N)
		sc->flags |= ATHN_FLAG_11N;

	IEEE80211_ADDR_COPY(ic->ic_myaddr, base->macAddr);

	/* Check if we have a hardware radio switch. */
	if (base->rfSilent & AR_EEP_RFSILENT_ENABLED) {
		sc->flags |= ATHN_FLAG_RFSILENT;
		/* Get GPIO pin used by hardware radio switch. */
		sc->rfsilent_pin = MS(base->rfSilent,
		    AR_EEP_RFSILENT_GPIO_SEL);
		/* Get polarity of hardware radio switch. */
		if (base->rfSilent & AR_EEP_RFSILENT_POLARITY)
			sc->flags |= ATHN_FLAG_RFSILENT_REVERSED;
	}

	/* Get the number of HW key cache entries. */
	kc_entries_log = MS(base->deviceCap, AR_EEP_DEVCAP_KC_ENTRIES);
	sc->kc_entries = (kc_entries_log != 0) ?
	    1 << kc_entries_log : AR_KEYTABLE_SIZE;

	sc->txchainmask = base->txMask;
	if (sc->mac_ver == AR_SREV_VERSION_5416_PCI &&
	    !(base->opCapFlags & AR_OPFLAGS_11A)) {
		/* For single-band AR5416 PCI, use GPIO pin 0. */
		sc->rxchainmask = ar5008_gpio_read(sc, 0) ? 0x5 : 0x7;
	} else
		sc->rxchainmask = base->rxMask;

	ops->setup(sc);
	return (0);
}

/*
 * Read 16-bit value from ROM.
 */
int
ar5008_read_rom_word(struct athn_softc *sc, uint32_t addr, uint16_t *val)
{
	uint32_t reg;
	int ntries;

	reg = AR_READ(sc, AR_EEPROM_OFFSET(addr));
	for (ntries = 0; ntries < 1000; ntries++) {
		reg = AR_READ(sc, AR_EEPROM_STATUS_DATA);
		if (!(reg & (AR_EEPROM_STATUS_DATA_BUSY |
		    AR_EEPROM_STATUS_DATA_PROT_ACCESS))) {
			*val = MS(reg, AR_EEPROM_STATUS_DATA_VAL);
			return (0);
		}
		DELAY(10);
	}
	*val = 0xffff;
	return (ETIMEDOUT);
}

int
ar5008_read_rom(struct athn_softc *sc)
{
	uint32_t addr, end;
	uint16_t magic, sum, *eep;
	int need_swap = 0;
	int error;

	/* Determine ROM endianness. */
	error = ar5008_read_rom_word(sc, AR_EEPROM_MAGIC_OFFSET, &magic);
	if (error != 0)
		return (error);
	if (magic != AR_EEPROM_MAGIC) {
		if (magic != swap16(AR_EEPROM_MAGIC)) {
			DPRINTF(("invalid ROM magic 0x%x != 0x%x\n",
			    magic, AR_EEPROM_MAGIC));
			return (EIO);
		}
		DPRINTF(("non-native ROM endianness\n"));
		need_swap = 1;
	}

	/* Allocate space to store ROM in host memory. */
	sc->eep = malloc(sc->eep_size, M_DEVBUF, M_NOWAIT);
	if (sc->eep == NULL)
		return (ENOMEM);

	/* Read entire ROM and compute checksum. */
	sum = 0;
	eep = sc->eep;
	end = sc->eep_base + sc->eep_size / sizeof(uint16_t);
	for (addr = sc->eep_base; addr < end; addr++, eep++) {
		if ((error = ar5008_read_rom_word(sc, addr, eep)) != 0) {
			DPRINTF(("could not read ROM at 0x%x\n", addr));
			return (error);
		}
		if (need_swap)
			*eep = swap16(*eep);
		sum ^= *eep;
	}
	if (sum != 0xffff) {
		printf("%s: bad ROM checksum 0x%04x\n",
		    sc->sc_dev.dv_xname, sum);
		return (EIO);
	}
	if (need_swap)
		ar5008_swap_rom(sc);

	return (0);
}

void
ar5008_swap_rom(struct athn_softc *sc)
{
	struct ar_base_eep_header *base = sc->eep;

	/* Swap common fields first. */
	base->length = swap16(base->length);
	base->version = swap16(base->version);
	base->regDmn[0] = swap16(base->regDmn[0]);
	base->regDmn[1] = swap16(base->regDmn[1]);
	base->rfSilent = swap16(base->rfSilent);
	base->blueToothOptions = swap16(base->blueToothOptions);
	base->deviceCap = swap16(base->deviceCap);

	/* Swap device-dependent fields. */
	sc->ops.swap_rom(sc);
}

/*
 * Access to General Purpose Input/Output ports.
 */
int
ar5008_gpio_read(struct athn_softc *sc, int pin)
{
	KASSERT(pin < sc->ngpiopins);
	return ((AR_READ(sc, AR_GPIO_IN_OUT) >> (sc->ngpiopins + pin)) & 1);
}

void
ar5008_gpio_write(struct athn_softc *sc, int pin, int set)
{
	uint32_t reg;

	KASSERT(pin < sc->ngpiopins);
	reg = AR_READ(sc, AR_GPIO_IN_OUT);
	if (set)
		reg |= 1 << pin;
	else
		reg &= ~(1 << pin);
	AR_WRITE(sc, AR_GPIO_IN_OUT, reg);
}

void
ar5008_gpio_config_input(struct athn_softc *sc, int pin)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV_M << (pin * 2));
	reg |= AR_GPIO_OE_OUT_DRV_NO << (pin * 2);
	AR_WRITE(sc, AR_GPIO_OE_OUT, reg);
}

void
ar5008_gpio_config_output(struct athn_softc *sc, int pin, int type)
{
	uint32_t reg;
	int mux, off;

	mux = pin / 6;
	off = pin % 6;

	reg = AR_READ(sc, AR_GPIO_OUTPUT_MUX(mux));
	if (!AR_SREV_9280_20_OR_LATER(sc) && mux == 0)
		reg = (reg & ~0x1f0) | (reg & 0x1f0) << 1;
	reg &= ~(0x1f << (off * 5));
	reg |= (type & 0x1f) << (off * 5);
	AR_WRITE(sc, AR_GPIO_OUTPUT_MUX(mux), reg);

	reg = AR_READ(sc, AR_GPIO_OE_OUT);
	reg &= ~(AR_GPIO_OE_OUT_DRV_M << (pin * 2));
	reg |= AR_GPIO_OE_OUT_DRV_ALL << (pin * 2);
	AR_WRITE(sc, AR_GPIO_OE_OUT, reg);
}

void
ar5008_rfsilent_init(struct athn_softc *sc)
{
	uint32_t reg;

	/* Configure hardware radio switch. */
	AR_SETBITS(sc, AR_GPIO_INPUT_EN_VAL, AR_GPIO_INPUT_EN_VAL_RFSILENT_BB);
	reg = AR_READ(sc, AR_GPIO_INPUT_MUX2);
	reg = RW(reg, AR_GPIO_INPUT_MUX2_RFSILENT, 0);
	AR_WRITE(sc, AR_GPIO_INPUT_MUX2, reg);
	ar5008_gpio_config_input(sc, sc->rfsilent_pin);
	AR_SETBITS(sc, AR_PHY_TEST, AR_PHY_TEST_RFSILENT_BB);
	if (!(sc->flags & ATHN_FLAG_RFSILENT_REVERSED)) {
		AR_SETBITS(sc, AR_GPIO_INTR_POL,
		    AR_GPIO_INTR_POL_PIN(sc->rfsilent_pin));
	}
}

int
ar5008_dma_alloc(struct athn_softc *sc)
{
	int error;

	error = ar5008_tx_alloc(sc);
	if (error != 0)
		return (error);

	error = ar5008_rx_alloc(sc);
	if (error != 0)
		return (error);

	return (0);
}

void
ar5008_dma_free(struct athn_softc *sc)
{
	ar5008_tx_free(sc);
	ar5008_rx_free(sc);
}

int
ar5008_tx_alloc(struct athn_softc *sc)
{
	struct athn_tx_buf *bf;
	bus_size_t size;
	int error, nsegs, i;

	/*
	 * Allocate a pool of Tx descriptors shared between all Tx queues.
	 */
	size = ATHN_NTXBUFS * AR5008_MAX_SCATTER * sizeof(struct ar_tx_desc);

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &sc->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(sc->sc_dmat, size, 4, 0, &sc->seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(sc->sc_dmat, &sc->seg, 1, size,
	    (caddr_t *)&sc->descs, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load_raw(sc->sc_dmat, sc->map, &sc->seg, 1, size,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	SIMPLEQ_INIT(&sc->txbufs);
	for (i = 0; i < ATHN_NTXBUFS; i++) {
		bf = &sc->txpool[i];

		error = bus_dmamap_create(sc->sc_dmat, ATHN_TXBUFSZ,
		    AR5008_MAX_SCATTER, ATHN_TXBUFSZ, 0, BUS_DMA_NOWAIT,
		    &bf->bf_map);
		if (error != 0) {
			printf("%s: could not create Tx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		bf->bf_descs =
		    &((struct ar_tx_desc *)sc->descs)[i * AR5008_MAX_SCATTER];
		bf->bf_daddr = sc->map->dm_segs[0].ds_addr +
		    i * AR5008_MAX_SCATTER * sizeof(struct ar_tx_desc);

		SIMPLEQ_INSERT_TAIL(&sc->txbufs, bf, bf_list);
	}
	return (0);
 fail:
	ar5008_tx_free(sc);
	return (error);
}

void
ar5008_tx_free(struct athn_softc *sc)
{
	struct athn_tx_buf *bf;
	int i;

	for (i = 0; i < ATHN_NTXBUFS; i++) {
		bf = &sc->txpool[i];

		if (bf->bf_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_map);
	}
	/* Free Tx descriptors. */
	if (sc->map != NULL) {
		if (sc->descs != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->descs,
			    ATHN_NTXBUFS * AR5008_MAX_SCATTER *
			    sizeof(struct ar_tx_desc));
			bus_dmamem_free(sc->sc_dmat, &sc->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->map);
	}
}

int
ar5008_rx_alloc(struct athn_softc *sc)
{
	struct athn_rxq *rxq = &sc->rxq[0];
	struct athn_rx_buf *bf;
	struct ar_rx_desc *ds;
	bus_size_t size;
	int error, nsegs, i;

	rxq->bf = malloc(ATHN_NRXBUFS * sizeof(*bf), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (rxq->bf == NULL)
		return (ENOMEM);

	size = ATHN_NRXBUFS * sizeof(struct ar_rx_desc);

	error = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &rxq->map);
	if (error != 0)
		goto fail;

	error = bus_dmamem_alloc(sc->sc_dmat, size, 0, 0, &rxq->seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (error != 0)
		goto fail;

	error = bus_dmamem_map(sc->sc_dmat, &rxq->seg, 1, size,
	    (caddr_t *)&rxq->descs, BUS_DMA_NOWAIT | BUS_DMA_COHERENT);
	if (error != 0)
		goto fail;

	error = bus_dmamap_load_raw(sc->sc_dmat, rxq->map, &rxq->seg, 1,
	    size, BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;

	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];
		ds = &((struct ar_rx_desc *)rxq->descs)[i];

		error = bus_dmamap_create(sc->sc_dmat, ATHN_RXBUFSZ, 1,
		    ATHN_RXBUFSZ, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &bf->bf_map);
		if (error != 0) {
			printf("%s: could not create Rx buf DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		/*
		 * Assumes MCLGETI returns cache-line-size aligned buffers.
		 */
		bf->bf_m = MCLGETI(NULL, M_DONTWAIT, NULL, ATHN_RXBUFSZ);
		if (bf->bf_m == NULL) {
			printf("%s: could not allocate Rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOBUFS;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, bf->bf_map,
		    mtod(bf->bf_m, void *), ATHN_RXBUFSZ, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		if (error != 0) {
			printf("%s: could not DMA map Rx buffer\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0,
		    bf->bf_map->dm_mapsize, BUS_DMASYNC_PREREAD);

		bf->bf_desc = ds;
		bf->bf_daddr = rxq->map->dm_segs[0].ds_addr +
		    i * sizeof(struct ar_rx_desc);
	}
	return (0);
 fail:
	ar5008_rx_free(sc);
	return (error);
}

void
ar5008_rx_free(struct athn_softc *sc)
{
	struct athn_rxq *rxq = &sc->rxq[0];
	struct athn_rx_buf *bf;
	int i;

	if (rxq->bf == NULL)
		return;
	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];

		if (bf->bf_map != NULL)
			bus_dmamap_destroy(sc->sc_dmat, bf->bf_map);
		if (bf->bf_m != NULL)
			m_freem(bf->bf_m);
	}
	free(rxq->bf, M_DEVBUF);

	/* Free Rx descriptors. */
	if (rxq->descs != NULL) {
		if (rxq->descs != NULL) {
			bus_dmamap_unload(sc->sc_dmat, rxq->map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)rxq->descs,
			    ATHN_NRXBUFS * sizeof(struct ar_rx_desc));
			bus_dmamem_free(sc->sc_dmat, &rxq->seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, rxq->map);
	}
}

void
ar5008_rx_enable(struct athn_softc *sc)
{
	struct athn_rxq *rxq = &sc->rxq[0];
	struct athn_rx_buf *bf;
	struct ar_rx_desc *ds;
	int i;

	/* Setup and link Rx descriptors. */
	SIMPLEQ_INIT(&rxq->head);
	rxq->lastds = NULL;
	for (i = 0; i < ATHN_NRXBUFS; i++) {
		bf = &rxq->bf[i];
		ds = bf->bf_desc;

		memset(ds, 0, sizeof(*ds));
		ds->ds_data = bf->bf_map->dm_segs[0].ds_addr;
		ds->ds_ctl1 = SM(AR_RXC1_BUF_LEN, ATHN_RXBUFSZ);

		if (rxq->lastds != NULL) {
			((struct ar_rx_desc *)rxq->lastds)->ds_link =
			    bf->bf_daddr;
		}
		SIMPLEQ_INSERT_TAIL(&rxq->head, bf, bf_list);
		rxq->lastds = ds;
	}
	bus_dmamap_sync(sc->sc_dmat, rxq->map, 0, rxq->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* Enable Rx. */
	AR_WRITE(sc, AR_RXDP, SIMPLEQ_FIRST(&rxq->head)->bf_daddr);
	AR_WRITE(sc, AR_CR, AR_CR_RXE);
}

#if NBPFILTER > 0
void
ar5008_rx_radiotap(struct athn_softc *sc, struct mbuf *m,
    struct ar_rx_desc *ds)
{
#define IEEE80211_RADIOTAP_F_SHORTGI	0x80	/* XXX from FBSD */

	struct athn_rx_radiotap_header *tap = &sc->sc_rxtap;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf mb;
	uint64_t tsf;
	uint32_t tstamp;
	uint8_t rate;

	/* Extend the 15-bit timestamp from Rx descriptor to 64-bit TSF. */
	tstamp = ds->ds_status2;
	tsf = AR_READ(sc, AR_TSF_U32);
	tsf = tsf << 32 | AR_READ(sc, AR_TSF_L32);
	if ((tsf & 0x7fff) < tstamp)
		tsf -= 0x8000;
	tsf = (tsf & ~0x7fff) | tstamp;

	tap->wr_flags = IEEE80211_RADIOTAP_F_FCS;
	tap->wr_tsft = htole64(tsf);
	tap->wr_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
	tap->wr_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
	tap->wr_dbm_antsignal = MS(ds->ds_status4, AR_RXS4_RSSI_COMBINED);
	/* XXX noise. */
	tap->wr_antenna = MS(ds->ds_status3, AR_RXS3_ANTENNA);
	tap->wr_rate = 0;	/* In case it can't be found below. */
	if (AR_SREV_5416_20_OR_LATER(sc))
		rate = MS(ds->ds_status0, AR_RXS0_RATE);
	else
		rate = MS(ds->ds_status3, AR_RXS3_RATE);
	if (rate & 0x80) {		/* HT. */
		/* Bit 7 set means HT MCS instead of rate. */
		tap->wr_rate = rate;
		if (!(ds->ds_status3 & AR_RXS3_GI))
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;

	} else if (rate & 0x10) {	/* CCK. */
		if (rate & 0x04)
			tap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		switch (rate & ~0x14) {
		case 0xb: tap->wr_rate =   2; break;
		case 0xa: tap->wr_rate =   4; break;
		case 0x9: tap->wr_rate =  11; break;
		case 0x8: tap->wr_rate =  22; break;
		}
	} else {			/* OFDM. */
		switch (rate) {
		case 0xb: tap->wr_rate =  12; break;
		case 0xf: tap->wr_rate =  18; break;
		case 0xa: tap->wr_rate =  24; break;
		case 0xe: tap->wr_rate =  36; break;
		case 0x9: tap->wr_rate =  48; break;
		case 0xd: tap->wr_rate =  72; break;
		case 0x8: tap->wr_rate =  96; break;
		case 0xc: tap->wr_rate = 108; break;
		}
	}
	mb.m_data = (caddr_t)tap;
	mb.m_len = sc->sc_rxtap_len;
	mb.m_next = m;
	mb.m_nextpkt = NULL;
	mb.m_type = 0;
	mb.m_flags = 0;
	bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_IN);
}
#endif

static __inline int
ar5008_rx_process(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct athn_rxq *rxq = &sc->rxq[0];
	struct athn_rx_buf *bf, *nbf;
	struct ar_rx_desc *ds;
	struct ieee80211_frame *wh;
	struct ieee80211_rxinfo rxi;
	struct ieee80211_node *ni;
	struct mbuf *m, *m1;
	int error, len;

	bf = SIMPLEQ_FIRST(&rxq->head);
	if (__predict_false(bf == NULL)) {	/* Should not happen. */
		printf("%s: Rx queue is empty!\n", sc->sc_dev.dv_xname);
		return (ENOENT);
	}
	ds = bf->bf_desc;

	if (!(ds->ds_status8 & AR_RXS8_DONE)) {
		/*
		 * On some parts, the status words can get corrupted
		 * (including the "done" bit), so we check the next
		 * descriptor "done" bit.  If it is set, it is a good
		 * indication that the status words are corrupted, so
		 * we skip this descriptor and drop the frame.
		 */
		nbf = SIMPLEQ_NEXT(bf, bf_list);
		if (nbf != NULL &&
		    (((struct ar_rx_desc *)nbf->bf_desc)->ds_status8 &
		     AR_RXS8_DONE)) {
			DPRINTF(("corrupted descriptor status=0x%x\n",
			    ds->ds_status8));
			/* HW will not "move" RXDP in this case, so do it. */
			AR_WRITE(sc, AR_RXDP, nbf->bf_daddr);
			ifp->if_ierrors++;
			goto skip;
		}
		return (EBUSY);
	}

	if (__predict_false(ds->ds_status1 & AR_RXS1_MORE)) {
		/* Drop frames that span multiple Rx descriptors. */
		DPRINTF(("dropping split frame\n"));
		ifp->if_ierrors++;
		goto skip;
	}
	if (!(ds->ds_status8 & AR_RXS8_FRAME_OK)) {
		if (ds->ds_status8 & AR_RXS8_CRC_ERR)
			DPRINTFN(6, ("CRC error\n"));
		else if (ds->ds_status8 & AR_RXS8_PHY_ERR)
			DPRINTFN(6, ("PHY error=0x%x\n",
			    MS(ds->ds_status8, AR_RXS8_PHY_ERR_CODE)));
		else if (ds->ds_status8 & AR_RXS8_DECRYPT_CRC_ERR)
			DPRINTFN(6, ("Decryption CRC error\n"));
		else if (ds->ds_status8 & AR_RXS8_MICHAEL_ERR) {
			DPRINTFN(2, ("Michael MIC failure\n"));
			/* Report Michael MIC failures to net80211. */
			ic->ic_stats.is_rx_locmicfail++;
			ieee80211_michael_mic_failure(ic, 0);
			/*
			 * XXX Check that it is not a control frame
			 * (invalid MIC failures on valid ctl frames.)
			 */
		}
		ifp->if_ierrors++;
		goto skip;
	}

	len = MS(ds->ds_status1, AR_RXS1_DATA_LEN);
	if (__predict_false(len == 0 || len > ATHN_RXBUFSZ)) {
		DPRINTF(("corrupted descriptor length=%d\n", len));
		ifp->if_ierrors++;
		goto skip;
	}

	/* Allocate a new Rx buffer. */
	m1 = MCLGETI(NULL, M_DONTWAIT, NULL, ATHN_RXBUFSZ);
	if (__predict_false(m1 == NULL)) {
		ic->ic_stats.is_rx_nombuf++;
		ifp->if_ierrors++;
		goto skip;
	}

	/* Sync and unmap the old Rx buffer. */
	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, ATHN_RXBUFSZ,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, bf->bf_map);

	/* Map the new Rx buffer. */
	error = bus_dmamap_load(sc->sc_dmat, bf->bf_map, mtod(m1, void *),
	    ATHN_RXBUFSZ, NULL, BUS_DMA_NOWAIT | BUS_DMA_READ);
	if (__predict_false(error != 0)) {
		m_freem(m1);

		/* Remap the old Rx buffer or panic. */
		error = bus_dmamap_load(sc->sc_dmat, bf->bf_map,
		    mtod(bf->bf_m, void *), ATHN_RXBUFSZ, NULL,
		    BUS_DMA_NOWAIT | BUS_DMA_READ);
		KASSERT(error != 0);
		ifp->if_ierrors++;
		goto skip;
	}

	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, ATHN_RXBUFSZ,
	    BUS_DMASYNC_PREREAD);

	/* Write physical address of new Rx buffer. */
	ds->ds_data = bf->bf_map->dm_segs[0].ds_addr;

	m = bf->bf_m;
	bf->bf_m = m1;

	/* Finalize mbuf. */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = len;

	/* Grab a reference to the source node. */
	wh = mtod(m, struct ieee80211_frame *);
	ni = ieee80211_find_rxnode(ic, wh);

	/* Remove any HW padding after the 802.11 header. */
	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_CTL)) {
		u_int hdrlen = ieee80211_get_hdrlen(wh);
		if (hdrlen & 3) {
			ovbcopy(wh, (caddr_t)wh + 2, hdrlen);
			m_adj(m, 2);	/* XXX sure? */
		}
	}
#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL))
		ar5008_rx_radiotap(sc, m, ds);
#endif
	/* Trim 802.11 FCS after radiotap. */
	m_adj(m, -IEEE80211_CRC_LEN);

	/* Send the frame to the 802.11 layer. */
	rxi.rxi_flags = 0;	/* XXX */
	rxi.rxi_rssi = MS(ds->ds_status4, AR_RXS4_RSSI_COMBINED);
	rxi.rxi_tstamp = ds->ds_status2;
	ieee80211_input(ifp, m, ni, &rxi);

	/* Node is no longer needed. */
	ieee80211_release_node(ic, ni);

 skip:
	/* Unlink this descriptor from head. */
	SIMPLEQ_REMOVE_HEAD(&rxq->head, bf_list);
	memset(&ds->ds_status0, 0, 36);	/* XXX Really needed? */
	ds->ds_status8 &= ~AR_RXS8_DONE;
	ds->ds_link = 0;

	/* Re-use this descriptor and link it to tail. */
	if (__predict_true(!SIMPLEQ_EMPTY(&rxq->head)))
		((struct ar_rx_desc *)rxq->lastds)->ds_link = bf->bf_daddr;
	else
		AR_WRITE(sc, AR_RXDP, bf->bf_daddr);
	SIMPLEQ_INSERT_TAIL(&rxq->head, bf, bf_list);
	rxq->lastds = ds;

	/* Re-enable Rx. */
	AR_WRITE(sc, AR_CR, AR_CR_RXE);
	return (0);
}

void
ar5008_rx_intr(struct athn_softc *sc)
{
	while (ar5008_rx_process(sc) == 0);
}

int
ar5008_tx_process(struct athn_softc *sc, int qid)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct athn_txq *txq = &sc->txq[qid];
	struct athn_node *an;
	struct athn_tx_buf *bf;
	struct ar_tx_desc *ds;
	uint8_t failcnt;

	bf = SIMPLEQ_FIRST(&txq->head);
	if (__predict_false(bf == NULL))
		return (ENOENT);
	/* Get descriptor of last DMA segment. */
	ds = &((struct ar_tx_desc *)bf->bf_descs)[bf->bf_map->dm_nsegs - 1];

	if (!(ds->ds_status9 & AR_TXS9_DONE))
		return (EBUSY);

	SIMPLEQ_REMOVE_HEAD(&txq->head, bf_list);
	ifp->if_opackets++;

	sc->sc_tx_timer = 0;

	if (ds->ds_status1 & AR_TXS1_EXCESSIVE_RETRIES)
		ifp->if_oerrors++;

	if (ds->ds_status1 & AR_TXS1_UNDERRUN)
		athn_inc_tx_trigger_level(sc);

	an = (struct athn_node *)bf->bf_ni;
	/*
	 * NB: the data fail count contains the number of un-acked tries
	 * for the final series used.  We must add the number of tries for
	 * each series that was fully processed.
	 */
	failcnt  = MS(ds->ds_status1, AR_TXS1_DATA_FAIL_CNT);
	/* NB: Assume two tries per series. */
	failcnt += MS(ds->ds_status9, AR_TXS9_FINAL_IDX) * 2;

	/* Update rate control statistics. */
	an->amn.amn_txcnt++;
	if (failcnt > 0)
		an->amn.amn_retrycnt++;

	DPRINTFN(5, ("Tx done qid=%d status1=%d fail count=%d\n",
	    qid, ds->ds_status1, failcnt));

	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, bf->bf_map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, bf->bf_map);

	m_freem(bf->bf_m);
	bf->bf_m = NULL;
	ieee80211_release_node(ic, bf->bf_ni);
	bf->bf_ni = NULL;

	/* Link Tx buffer back to global free list. */
	SIMPLEQ_INSERT_TAIL(&sc->txbufs, bf, bf_list);
	return (0);
}

void
ar5008_tx_intr(struct athn_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	uint16_t mask = 0;
	uint32_t reg;
	int qid;

	reg = AR_READ(sc, AR_ISR_S0_S);
	mask |= MS(reg, AR_ISR_S0_QCU_TXOK);
	mask |= MS(reg, AR_ISR_S0_QCU_TXDESC);

	reg = AR_READ(sc, AR_ISR_S1_S);
	mask |= MS(reg, AR_ISR_S1_QCU_TXERR);
	mask |= MS(reg, AR_ISR_S1_QCU_TXEOL);

	DPRINTFN(4, ("Tx interrupt mask=0x%x\n", mask));
	for (qid = 0; mask != 0; mask >>= 1, qid++) {
		if (mask & 1)
			while (ar5008_tx_process(sc, qid) == 0);
	}
	if (!SIMPLEQ_EMPTY(&sc->txbufs)) {
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_start(ifp);
	}
}

int
ar5008_intr(struct athn_softc *sc)
{
	uint32_t intr, intr2, intr5, sync;

	/* Get pending interrupts. */
	intr = AR_READ(sc, AR_INTR_ASYNC_CAUSE);
	if (!(intr & AR_INTR_MAC_IRQ) || intr == AR_INTR_SPURIOUS) {
		intr = AR_READ(sc, AR_INTR_SYNC_CAUSE);
		if (intr == AR_INTR_SPURIOUS || (intr & sc->isync) == 0)
			return (0);	/* Not for us. */
	}

	if ((AR_READ(sc, AR_INTR_ASYNC_CAUSE) & AR_INTR_MAC_IRQ) &&
	    (AR_READ(sc, AR_RTC_STATUS) & AR_RTC_STATUS_M) == AR_RTC_STATUS_ON)
		intr = AR_READ(sc, AR_ISR);
	else
		intr = 0;
	sync = AR_READ(sc, AR_INTR_SYNC_CAUSE) & sc->isync;
	if (intr == 0 && sync == 0)
		return (0);	/* Not for us. */

	if (intr != 0) {
		if (intr & AR_ISR_BCNMISC) {
			intr2 = AR_READ(sc, AR_ISR_S2);
			if (intr2 & AR_ISR_S2_TIM)
				/* TBD */;
			if (intr2 & AR_ISR_S2_TSFOOR)
				/* TBD */;
		}
		intr = AR_READ(sc, AR_ISR_RAC);
		if (intr == AR_INTR_SPURIOUS)
			return (1);

		if (intr & (AR_ISR_RXMINTR | AR_ISR_RXINTM))
			ar5008_rx_intr(sc);
		if (intr & (AR_ISR_RXOK | AR_ISR_RXERR | AR_ISR_RXORN))
			ar5008_rx_intr(sc);

		if (intr & (AR_ISR_TXOK | AR_ISR_TXDESC |
		    AR_ISR_TXERR | AR_ISR_TXEOL))
			ar5008_tx_intr(sc);

		if (intr & AR_ISR_GENTMR) {
			intr5 = AR_READ(sc, AR_ISR_S5_S);
			if (intr5 & AR_ISR_GENTMR) {
				DPRINTF(("GENTMR trigger=%d thresh=%d\n",
				    MS(intr5, AR_ISR_S5_GENTIMER_TRIG),
				    MS(intr5, AR_ISR_S5_GENTIMER_THRESH)));
			}
		}

		intr5 = AR_READ(sc, AR_ISR_S5_S);
		if (intr5 & AR_ISR_S5_TIM_TIMER)
			/* TBD */;
	}
	if (sync != 0) {
		if (sync & (AR_INTR_SYNC_HOST1_FATAL |
		    AR_INTR_SYNC_HOST1_PERR))
			/* TBD */;

		if (sync & AR_INTR_SYNC_RADM_CPL_TIMEOUT) {
			AR_WRITE(sc, AR_RC, AR_RC_HOSTIF);
			AR_WRITE(sc, AR_RC, 0);
		}

		if ((sc->flags & ATHN_FLAG_RFSILENT) &&
		    (sync & AR_INTR_SYNC_GPIO_PIN(sc->rfsilent_pin))) {
			struct ifnet *ifp = &sc->sc_ic.ic_if;

			printf("%s: radio switch turned off\n",
			    sc->sc_dev.dv_xname);
			/* Turn the interface down. */
			ifp->if_flags &= ~IFF_UP;
			athn_stop(ifp, 1);
			return (1);
		}

		AR_WRITE(sc, AR_INTR_SYNC_CAUSE, sync);
		(void)AR_READ(sc, AR_INTR_SYNC_CAUSE);
	}
	return (1);
}

int
ar5008_tx(struct athn_softc *sc, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_key *k = NULL;
	struct ieee80211_frame *wh;
	struct athn_series series[4];
	struct ar_tx_desc *ds, *lastds;
	struct athn_txq *txq;
	struct athn_tx_buf *bf;
	struct athn_node *an = (void *)ni;
	struct mbuf *m1;
	uintptr_t entry;
	uint16_t qos;
	uint8_t txpower, type, encrtype, tid, ridx[4];
	int i, error, totlen, hasqos, qid;

	/* Grab a Tx buffer from our global free list. */
	bf = SIMPLEQ_FIRST(&sc->txbufs);
	KASSERT(bf != NULL);

	/* Map 802.11 frame type to hardware frame type. */
	wh = mtod(m, struct ieee80211_frame *);
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	    IEEE80211_FC0_TYPE_MGT) {
		if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_BEACON)
			type = AR_FRAME_TYPE_BEACON;
		else if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_PROBE_RESP)
			type = AR_FRAME_TYPE_PROBE_RESP;
		else if ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		    IEEE80211_FC0_SUBTYPE_ATIM)
			type = AR_FRAME_TYPE_ATIM;
		else
			type = AR_FRAME_TYPE_NORMAL;
	} else if ((wh->i_fc[0] &
	    (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_MASK)) ==
	    (IEEE80211_FC0_TYPE_CTL  | IEEE80211_FC0_SUBTYPE_PS_POLL)) {
		type = AR_FRAME_TYPE_PSPOLL;
	} else
		type = AR_FRAME_TYPE_NORMAL;

	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		k = ieee80211_get_txkey(ic, wh, ni);
		if ((m = ieee80211_encrypt(ic, m, k)) == NULL)
			return (ENOBUFS);
		wh = mtod(m, struct ieee80211_frame *);
	}

	/* XXX 2-byte padding for QoS and 4-addr headers. */

	/* Select the HW Tx queue to use for this frame. */
	if ((hasqos = ieee80211_has_qos(wh))) {
		qos = ieee80211_get_qos(wh);
		tid = qos & IEEE80211_QOS_TID;
		qid = athn_ac2qid[ieee80211_up_to_ac(ic, tid)];
	} else if (type == AR_FRAME_TYPE_BEACON) {
		qid = ATHN_QID_BEACON;
	} else if (type == AR_FRAME_TYPE_PSPOLL) {
		qid = ATHN_QID_PSPOLL;
	} else
		qid = ATHN_QID_AC_BE;
	txq = &sc->txq[qid];

	/* Select the transmit rates to use for this frame. */
	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	    IEEE80211_FC0_TYPE_DATA) {
		/* Use lowest rate for all tries. */
		ridx[0] = ridx[1] = ridx[2] = ridx[3] =
		    (ic->ic_curmode == IEEE80211_MODE_11A) ?
			ATHN_RIDX_OFDM6 : ATHN_RIDX_CCK1;
	} else if (ic->ic_fixed_rate != -1) {
		/* Use same fixed rate for all tries. */
		ridx[0] = ridx[1] = ridx[2] = ridx[3] =
		    sc->fixed_ridx;
	} else {
		int txrate = ni->ni_txrate;
		/* Use fallback table of the node. */
		for (i = 0; i < 4; i++) {
			ridx[i] = an->ridx[txrate];
			txrate = an->fallback[txrate];
		}
	}

#if NBPFILTER > 0
	if (__predict_false(sc->sc_drvbpf != NULL)) {
		struct athn_tx_radiotap_header *tap = &sc->sc_txtap;
		struct mbuf mb;

		tap->wt_flags = 0;
		/* Use initial transmit rate. */
		tap->wt_rate = athn_rates[ridx[0]].rate;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);
		tap->wt_hwqueue = qid;
		if (ridx[0] != ATHN_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			tap->wt_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_nextpkt = NULL;
		mb.m_type = 0;
		mb.m_flags = 0;
		bpf_mtap(sc->sc_drvbpf, &mb, BPF_DIRECTION_OUT);
	}
#endif

	/* DMA map mbuf. */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_map, m,
	    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
	if (__predict_false(error != 0)) {
		if (error != EFBIG) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return (error);
		}
		/*
		 * DMA mapping requires too many DMA segments; linearize
		 * mbuf in kernel virtual address space and retry.
		 */
		MGETHDR(m1, M_DONTWAIT, MT_DATA);
		if (m1 == NULL) {
			m_freem(m);
			return (ENOBUFS);
		}
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m1, M_DONTWAIT);
			if (!(m1->m_flags & M_EXT)) {
				m_freem(m);
				m_freem(m1);
				return (ENOBUFS);
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m1, caddr_t));
		m1->m_pkthdr.len = m1->m_len = m->m_pkthdr.len;
		m_freem(m);
		m = m1;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, bf->bf_map, m,
		    BUS_DMA_NOWAIT | BUS_DMA_WRITE);
		if (error != 0) {
			printf("%s: can't map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return (error);
		}
	}
	bf->bf_m = m;
	bf->bf_ni = ni;

	wh = mtod(m, struct ieee80211_frame *);

	totlen = m->m_pkthdr.len + IEEE80211_CRC_LEN;

	/* Clear all Tx descriptors that we will use. */
	memset(bf->bf_descs, 0, bf->bf_map->dm_nsegs * sizeof(*ds));

	/* Setup first Tx descriptor. */
	ds = bf->bf_descs;

	ds->ds_ctl0 = AR_TXC0_INTR_REQ | AR_TXC0_CLR_DEST_MASK;
	txpower = AR_MAX_RATE_POWER;	/* Get from per-rate registers. */
	ds->ds_ctl0 |= SM(AR_TXC0_XMIT_POWER, txpower);

	ds->ds_ctl1 = SM(AR_TXC1_FRAME_TYPE, type);

	if (IEEE80211_IS_MULTICAST(wh->i_addr1) ||
	    (hasqos && (qos & IEEE80211_QOS_ACK_POLICY_MASK) ==
	     IEEE80211_QOS_ACK_POLICY_NOACK))
		ds->ds_ctl1 |= AR_TXC1_NO_ACK;

	if (0 && wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		/* Retrieve key for encryption. */
		k = ieee80211_get_txkey(ic, wh, ni);
		/*
		 * Map 802.11 cipher to hardware encryption type and
		 * compute crypto overhead.
		 */
		switch (k->k_cipher) {
		case IEEE80211_CIPHER_WEP40:
		case IEEE80211_CIPHER_WEP104:
			encrtype = AR_ENCR_TYPE_WEP;
			totlen += 8;
			break;
		case IEEE80211_CIPHER_TKIP:
			encrtype = AR_ENCR_TYPE_TKIP;
			totlen += 20;
			break;
		case IEEE80211_CIPHER_CCMP:
			encrtype = AR_ENCR_TYPE_AES;
			totlen += 16;
			break;
		default:
			panic("unsupported cipher");	/* XXX BIP? */
		}
		/*
		 * NB: The key cache entry index is stored in the key
		 * private field when the key is installed.
		 */
		entry = (uintptr_t)k->k_priv;
		ds->ds_ctl1 |= SM(AR_TXC1_DEST_IDX, entry);
		ds->ds_ctl0 |= AR_TXC0_DEST_IDX_VALID;
	} else
		encrtype = AR_ENCR_TYPE_CLEAR;
	ds->ds_ctl6 = SM(AR_TXC6_ENCR_TYPE, encrtype);

	/* Check if frame must be protected using RTS/CTS or CTS-to-self. */
	if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* NB: Group frames are sent using CCK in 802.11b/g. */
		if (totlen > ic->ic_rtsthreshold) {
			ds->ds_ctl0 |= AR_TXC0_RTS_ENABLE;
		} else if ((ic->ic_flags & IEEE80211_F_USEPROT) &&
		    athn_rates[ridx[0]].phy == IEEE80211_T_OFDM) {
			if (ic->ic_protmode == IEEE80211_PROT_RTSCTS)
				ds->ds_ctl0 |= AR_TXC0_RTS_ENABLE;
			else if (ic->ic_protmode == IEEE80211_PROT_CTSONLY)
				ds->ds_ctl0 |= AR_TXC0_CTS_ENABLE;
		}
	}
	if (ds->ds_ctl0 & (AR_TXC0_RTS_ENABLE | AR_TXC0_CTS_ENABLE)) {
		/* Disable multi-rate retries when protection is used. */
		ridx[1] = ridx[2] = ridx[3] = ridx[0];
	}
	/* Setup multi-rate retries. */
	for (i = 0; i < 4; i++) {
		series[i].hwrate = athn_rates[ridx[i]].hwrate;
		if (athn_rates[ridx[i]].phy == IEEE80211_T_DS &&
		    ridx[i] != ATHN_RIDX_CCK1 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			series[i].hwrate |= 0x04;
		series[i].dur = 0;
	}
	if (!(ds->ds_ctl1 & AR_TXC1_NO_ACK)) {
		/* Compute duration for each series. */
		for (i = 0; i < 4; i++) {
			series[i].dur = athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[ridx[i]].rspridx, ic->ic_flags);
		}
	}

	/* Write number of tries for each series. */
	ds->ds_ctl2 =
	    SM(AR_TXC2_XMIT_DATA_TRIES0, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES1, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES2, 2) |
	    SM(AR_TXC2_XMIT_DATA_TRIES3, 4);

	/* Tell HW to update duration field in 802.11 header. */
	if (type != AR_FRAME_TYPE_PSPOLL)
		ds->ds_ctl2 |= AR_TXC2_DUR_UPDATE_ENA;

	/* Write Tx rate for each series. */
	ds->ds_ctl3 =
	    SM(AR_TXC3_XMIT_RATE0, series[0].hwrate) |
	    SM(AR_TXC3_XMIT_RATE1, series[1].hwrate) |
	    SM(AR_TXC3_XMIT_RATE2, series[2].hwrate) |
	    SM(AR_TXC3_XMIT_RATE3, series[3].hwrate);

	/* Write duration for each series. */
	ds->ds_ctl4 =
	    SM(AR_TXC4_PACKET_DUR0, series[0].dur) |
	    SM(AR_TXC4_PACKET_DUR1, series[1].dur);
	ds->ds_ctl5 =
	    SM(AR_TXC5_PACKET_DUR2, series[2].dur) |
	    SM(AR_TXC5_PACKET_DUR3, series[3].dur);

	/* Use the same Tx chains for all tries. */
	ds->ds_ctl7 =
	    SM(AR_TXC7_CHAIN_SEL0, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL1, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL2, sc->txchainmask) |
	    SM(AR_TXC7_CHAIN_SEL3, sc->txchainmask);
#ifdef notyet
#ifndef IEEE80211_NO_HT
	/* Use the same short GI setting for all tries. */
	if (ic->ic_flags & IEEE80211_F_SHGI)
		ds->ds_ctl7 |= AR_TXC7_GI0123;
	/* Use the same channel width for all tries. */
	if (ic->ic_flags & IEEE80211_F_CBW40)
		ds->ds_ctl7 |= AR_TXC7_2040_0123;
#endif
#endif

	if (ds->ds_ctl0 & (AR_TXC0_RTS_ENABLE | AR_TXC0_CTS_ENABLE)) {
		uint8_t protridx, hwrate;
		uint16_t dur = 0;

		/* Use the same protection mode for all tries. */
		if (ds->ds_ctl0 & AR_TXC0_RTS_ENABLE) {
			ds->ds_ctl4 |= AR_TXC4_RTSCTS_QUAL01;
			ds->ds_ctl5 |= AR_TXC5_RTSCTS_QUAL23;
		}
		/* Select protection rate (suboptimal but ok.) */
		protridx = (ic->ic_curmode == IEEE80211_MODE_11A) ?
		    ATHN_RIDX_OFDM6 : ATHN_RIDX_CCK2;
		if (ds->ds_ctl0 & AR_TXC0_RTS_ENABLE) {
			/* Account for CTS duration. */
			dur += athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[protridx].rspridx, ic->ic_flags);
		}
		dur += athn_txtime(sc, totlen, ridx[0], ic->ic_flags);
		if (!(ds->ds_ctl1 & AR_TXC1_NO_ACK)) {
			/* Account for ACK duration. */
			dur += athn_txtime(sc, IEEE80211_ACK_LEN,
			    athn_rates[ridx[0]].rspridx, ic->ic_flags);
		}
		/* Write protection frame duration and rate. */
		ds->ds_ctl2 |= SM(AR_TXC2_BURST_DUR, dur);
		hwrate = athn_rates[protridx].hwrate;
		if (protridx == ATHN_RIDX_CCK2 &&
		    (ic->ic_flags & IEEE80211_F_SHPREAMBLE))
			hwrate |= 0x04;
		ds->ds_ctl7 |= SM(AR_TXC7_RTSCTS_RATE, hwrate);
	}

	/* Finalize first Tx descriptor and fill others (if any.) */
	ds->ds_ctl0 |= SM(AR_TXC0_FRAME_LEN, totlen);

	for (i = 0; i < bf->bf_map->dm_nsegs; i++, ds++) {
		ds->ds_data = bf->bf_map->dm_segs[i].ds_addr;
		ds->ds_ctl1 |= SM(AR_TXC1_BUF_LEN,
		    bf->bf_map->dm_segs[i].ds_len);

		if (i != bf->bf_map->dm_nsegs - 1)
			ds->ds_ctl1 |= AR_TXC1_MORE;
		ds->ds_link = 0;

		/* Chain Tx descriptor. */
		if (i != 0)
			lastds->ds_link = bf->bf_daddr + i * sizeof(*ds);
		lastds = ds;
	}
	bus_dmamap_sync(sc->sc_dmat, bf->bf_map, 0, bf->bf_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if (!SIMPLEQ_EMPTY(&txq->head))
		((struct ar_tx_desc *)txq->lastds)->ds_link = bf->bf_daddr;
	else
		AR_WRITE(sc, AR_QTXDP(qid), bf->bf_daddr);
	txq->lastds = lastds;
	SIMPLEQ_REMOVE_HEAD(&sc->txbufs, bf_list);
	SIMPLEQ_INSERT_TAIL(&txq->head, bf, bf_list);

	ds = bf->bf_descs;
	DPRINTFN(6, ("Tx qid=%d nsegs=%d ctl0=0x%x ctl1=0x%x ctl3=0x%x\n",
	    qid, bf->bf_map->dm_nsegs, ds->ds_ctl0, ds->ds_ctl1, ds->ds_ctl3));

	/* Kick Tx. */
	AR_WRITE(sc, AR_Q_TXE, 1 << qid);
	return (0);
}

void
ar5008_set_rf_mode(struct athn_softc *sc, struct ieee80211_channel *c)
{
	uint32_t reg;

	reg = IEEE80211_IS_CHAN_2GHZ(c) ?
	    AR_PHY_MODE_DYNAMIC : AR_PHY_MODE_OFDM;
	if (!AR_SREV_9280_10_OR_LATER(sc)) {
		reg |= IEEE80211_IS_CHAN_2GHZ(c) ?
		    AR_PHY_MODE_RF2GHZ : AR_PHY_MODE_RF5GHZ;
	} else if (IEEE80211_IS_CHAN_5GHZ(c) &&
	    (sc->flags & ATHN_FLAG_FAST_PLL_CLOCK)) {
		reg |= AR_PHY_MODE_DYNAMIC | AR_PHY_MODE_DYN_CCK_DISABLE;
	}
	AR_WRITE(sc, AR_PHY_MODE, reg);
}

static __inline uint32_t
ar5008_synth_delay(struct athn_softc *sc)
{
	uint32_t delay;

	delay = MS(AR_READ(sc, AR_PHY_RX_DELAY), AR_PHY_RX_DELAY_DELAY);
	if (sc->sc_ic.ic_curmode == IEEE80211_MODE_11B)
		delay = (delay * 4) / 22;
	else
		delay = delay / 10;	/* in 100ns steps */
	return (delay);
}

int
ar5008_rf_bus_request(struct athn_softc *sc)
{
	int ntries;

	/* Request RF Bus grant. */
	AR_WRITE(sc, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
	for (ntries = 0; ntries < 10000; ntries++) {
		if (AR_READ(sc, AR_PHY_RFBUS_GRANT) & AR_PHY_RFBUS_GRANT_EN)
			return (0);
		DELAY(10);
	}
	DPRINTF(("could not kill baseband Rx"));
	return (ETIMEDOUT);
}

void
ar5008_rf_bus_release(struct athn_softc *sc)
{
	/* Wait for the synthesizer to settle. */
	DELAY(AR_BASE_PHY_ACTIVE_DELAY + ar5008_synth_delay(sc));

	/* Release the RF Bus grant. */
	AR_WRITE(sc, AR_PHY_RFBUS_REQ, 0);
}

void
ar5008_set_phy(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint32_t phy;

	if (AR_SREV_9285_10_OR_LATER(sc))
		phy = AR_READ(sc, AR_PHY_TURBO) & AR_PHY_FC_ENABLE_DAC_FIFO;
	else
		phy = 0;
	phy |= AR_PHY_FC_HT_EN | AR_PHY_FC_SHORT_GI_40 |
	    AR_PHY_FC_SINGLE_HT_LTF1 | AR_PHY_FC_WALSH;
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		phy |= AR_PHY_FC_DYN2040_EN;
		if (extc > c)	/* XXX */
			phy |= AR_PHY_FC_DYN2040_PRI_CH;
	}
#endif
	AR_WRITE(sc, AR_PHY_TURBO, phy);

	AR_WRITE(sc, AR_2040_MODE,
	    (extc != NULL) ? AR_2040_JOINED_RX_CLEAR : 0);

	/* Set global transmit timeout. */
	AR_WRITE(sc, AR_GTXTO, SM(AR_GTXTO_TIMEOUT_LIMIT, 25));
	/* Set carrier sense timeout. */
	AR_WRITE(sc, AR_CST, SM(AR_CST_TIMEOUT_LIMIT, 15));
}

void
ar5008_set_delta_slope(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	uint32_t coeff, exp, man, reg;

	/* Set Delta Slope (exponent and mantissa). */
	coeff = (100 << 24) / c->ic_freq;
	athn_get_delta_slope(coeff, &exp, &man);
	DPRINTFN(5, ("delta slope coeff exp=%u man=%u\n", exp, man));

	reg = AR_READ(sc, AR_PHY_TIMING3);
	reg = RW(reg, AR_PHY_TIMING3_DSC_EXP, exp);
	reg = RW(reg, AR_PHY_TIMING3_DSC_MAN, man);
	AR_WRITE(sc, AR_PHY_TIMING3, reg);

	/* For Short GI, coeff is 9/10 that of normal coeff. */
	coeff = (9 * coeff) / 10;
	athn_get_delta_slope(coeff, &exp, &man);
	DPRINTFN(5, ("delta slope coeff exp=%u man=%u\n", exp, man));

	reg = AR_READ(sc, AR_PHY_HALFGI);
	reg = RW(reg, AR_PHY_HALFGI_DSC_EXP, exp);
	reg = RW(reg, AR_PHY_HALFGI_DSC_MAN, man);
	AR_WRITE(sc, AR_PHY_HALFGI, reg);
}

void
ar5008_enable_antenna_diversity(struct athn_softc *sc)
{
	AR_SETBITS(sc, AR_PHY_CCK_DETECT,
	    AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);
}

void
ar5008_init_baseband(struct athn_softc *sc)
{
	uint32_t synth_delay;

	synth_delay = ar5008_synth_delay(sc);
	/* Activate the PHY (includes baseband activate and synthesizer on). */
	AR_WRITE(sc, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
	DELAY(AR_BASE_PHY_ACTIVE_DELAY + synth_delay);
}

void
ar5008_disable_phy(struct athn_softc *sc)
{
	AR_WRITE(sc, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);
}

void
ar5008_init_chains(struct athn_softc *sc)
{
	if (sc->rxchainmask == 0x5 || sc->txchainmask == 0x5)
		AR_SETBITS(sc, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);

	/* Setup chain masks. */
	if (sc->mac_ver <= AR_SREV_VERSION_9160 &&
	    (sc->rxchainmask == 0x3 || sc->rxchainmask == 0x5)) {
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  0x7);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, 0x7);
	} else {
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  sc->rxchainmask);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, sc->rxchainmask);
	}
	AR_WRITE(sc, AR_SELFGEN_MASK, sc->txchainmask);
}

void
ar5008_set_rxchains(struct athn_softc *sc)
{
	if (sc->rxchainmask == 0x3 || sc->rxchainmask == 0x5) {
		AR_WRITE(sc, AR_PHY_RX_CHAINMASK,  sc->rxchainmask);
		AR_WRITE(sc, AR_PHY_CAL_CHAINMASK, sc->rxchainmask);
	}
}

void
ar5008_read_noisefloor(struct athn_softc *sc, int16_t *nf, int16_t *nf_ext)
{
/* Sign-extend 9-bit value to 16-bit. */
#define SIGN_EXT(v)	((((int16_t)(v)) << 7) >> 7)
	uint32_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		reg = AR_READ(sc, AR_PHY_CCA(i));
		if (AR_SREV_9280_10_OR_LATER(sc))
			nf[i] = MS(reg, AR9280_PHY_MINCCA_PWR);
		else
			nf[i] = MS(reg, AR_PHY_MINCCA_PWR);
		nf[i] = SIGN_EXT(nf[i]);

		reg = AR_READ(sc, AR_PHY_EXT_CCA(i));
		if (AR_SREV_9280_10_OR_LATER(sc))
			nf_ext[i] = MS(reg, AR9280_PHY_EXT_MINCCA_PWR);
		else
			nf_ext[i] = MS(reg, AR_PHY_EXT_MINCCA_PWR);
		nf_ext[i] = SIGN_EXT(nf_ext[i]);
	}
#undef SIGN_EXT
}

void
ar5008_write_noisefloor(struct athn_softc *sc, int16_t *nf, int16_t *nf_ext)
{
	uint32_t reg;
	int i;

	for (i = 0; i < sc->nrxchains; i++) {
		reg = AR_READ(sc, AR_PHY_CCA(i));
		reg = RW(reg, AR_PHY_MAXCCA_PWR, nf[i]);
		AR_WRITE(sc, AR_PHY_CCA(i), reg);

		reg = AR_READ(sc, AR_PHY_EXT_CCA(i));
		reg = RW(reg, AR_PHY_EXT_MAXCCA_PWR, nf_ext[i]);
		AR_WRITE(sc, AR_PHY_EXT_CCA(i), reg);
	}
}

void
ar5008_get_noisefloor(struct athn_softc *sc, struct ieee80211_channel *c)
{
	int16_t nf[AR_MAX_CHAINS], nf_ext[AR_MAX_CHAINS];
	int i;

	if (AR_READ(sc, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
		/* Noisefloor calibration not finished. */
		return;
	}
	/* Noisefloor calibration is finished. */
	ar5008_read_noisefloor(sc, nf, nf_ext);

	/* Update noisefloor history. */
	for (i = 0; i < sc->nrxchains; i++) {
		sc->nf_hist[sc->nf_hist_cur].nf[i] = nf[i];
		sc->nf_hist[sc->nf_hist_cur].nf_ext[i] = nf_ext[i];
	}
	if (++sc->nf_hist_cur >= ATHN_NF_CAL_HIST_MAX)
		sc->nf_hist_cur = 0;
}

void
ar5008_bb_load_noisefloor(struct athn_softc *sc)
{
	int16_t nf[AR_MAX_CHAINS], nf_ext[AR_MAX_CHAINS];
	int i, ntries;

	/* Write filtered noisefloor values. */
	for (i = 0; i < sc->nrxchains; i++) {
		nf[i] = sc->nf_priv[i] * 2;
		nf_ext[i] = sc->nf_ext_priv[i] * 2;
	}
	ar5008_write_noisefloor(sc, nf, nf_ext);

	/* Load filtered noisefloor values into baseband. */
	AR_CLRBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
	AR_CLRBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
	/* Wait for load to complete. */
	for (ntries = 0; ntries < 5; ntries++) {
		if (!(AR_READ(sc, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF))
			break;
		DELAY(50);
	}
#ifdef ATHN_DEBUG
	if (ntries == 5 && athn_debug > 0)
		printf("failed to load noisefloor values\n");
#endif

	/* Restore noisefloor values to initial (max) values. */
	for (i = 0; i < AR_MAX_CHAINS; i++)
		nf[i] = nf_ext[i] = -50 * 2;
	ar5008_write_noisefloor(sc, nf, nf_ext);
}

void
ar5008_noisefloor_calib(struct athn_softc *sc)
{
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
}

void
ar5008_do_noisefloor_calib(struct athn_softc *sc)
{
	AR_SETBITS(sc, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
}

void
ar5008_do_calib(struct athn_softc *sc)
{
	int log = AR_MAX_LOG_CAL;	/* XXX */
	uint32_t mode = 0, reg;

	reg = AR_READ(sc, AR_PHY_TIMING_CTRL4_0);
	reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX, log);
	AR_WRITE(sc, AR_PHY_TIMING_CTRL4_0, reg);

	if (sc->calib_mask & ATHN_CAL_ADC_GAIN)
		mode = AR_PHY_CALMODE_ADC_GAIN;
	else if (sc->calib_mask & ATHN_CAL_ADC_DC)
		mode = AR_PHY_CALMODE_ADC_DC_PER;
	else if (sc->calib_mask & ATHN_CAL_IQ)
		mode = AR_PHY_CALMODE_IQ;
	AR_WRITE(sc, AR_PHY_CALMODE, mode);

	AR_SETBITS(sc, AR_PHY_TIMING_CTRL4_0, AR_PHY_TIMING_CTRL4_DO_CAL);
}

void
ar5008_next_calib(struct athn_softc *sc)
{
	if (AR_READ(sc, AR_PHY_TIMING_CTRL4_0) & AR_PHY_TIMING_CTRL4_DO_CAL) {
		/* Calibration in progress, come back later. */
		return;
	}
	if (sc->calib_mask & ATHN_CAL_ADC_GAIN)
		ar5008_calib_iq(sc);
	else if (sc->calib_mask & ATHN_CAL_ADC_DC)
		ar5008_calib_adc_gain(sc);
	else if (sc->calib_mask & ATHN_CAL_IQ)
		ar5008_calib_adc_dc_off(sc);
}

void
ar5008_calib_iq(struct athn_softc *sc)
{
	struct athn_iq_cal *cal;
	uint32_t reg, i_coff_denom, q_coff_denom;
	int32_t i_coff, q_coff;
	int i, iq_corr_neg;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.iq[i];

		/* Accumulate IQ calibration measures (clear on read). */
		cal->pwr_meas_i += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_q += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->iq_corr_meas +=
		    (int32_t)AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		ar5008_do_calib(sc);
		return;
	}

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.iq[i];

		if (cal->pwr_meas_q == 0)
			continue;

		if ((iq_corr_neg = cal->iq_corr_meas < 0))
			cal->iq_corr_meas = -cal->iq_corr_meas;

		i_coff_denom =
		    (cal->pwr_meas_i / 2 + cal->pwr_meas_q / 2) / 128;
		q_coff_denom = cal->pwr_meas_q / 64;

		if (i_coff_denom == 0 || q_coff_denom == 0)
			continue;	/* Prevents division by zero. */

		i_coff = cal->iq_corr_meas / i_coff_denom;
		q_coff = (cal->pwr_meas_i / q_coff_denom) - 64;

		/* Negate i_coff if iq_corr_meas is positive. */
		if (!iq_corr_neg)
			i_coff = 0x40 - (i_coff & 0x3f);
		if (q_coff > 15)
			q_coff = 15;
		else if (q_coff <= -16)
			q_coff = -16;	/* XXX Linux has a bug here? */

		DPRINTFN(2, ("IQ calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_TIMING_CTRL4(i));
		reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF, i_coff);
		reg = RW(reg, AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF, q_coff);
		AR_WRITE(sc, AR_PHY_TIMING_CTRL4(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_TIMING_CTRL4_0,
	    AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);
}

void
ar5008_calib_adc_gain(struct athn_softc *sc)
{
	struct athn_adc_cal *cal;
	uint32_t reg, gain_mismatch_i, gain_mismatch_q;
	int i;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.adc_gain[i];

		/* Accumulate ADC gain measures (clear on read). */
		cal->pwr_meas_odd_i  += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_even_i += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->pwr_meas_odd_q  += AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
		cal->pwr_meas_even_q += AR_READ(sc, AR_PHY_CAL_MEAS_3(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		ar5008_do_calib(sc);
		return;
	}

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.adc_gain[i];

		if (cal->pwr_meas_odd_i == 0 || cal->pwr_meas_even_q == 0)
			continue;	/* Prevents division by zero. */

		gain_mismatch_i =
		    (cal->pwr_meas_even_i * 32) / cal->pwr_meas_odd_i;
		gain_mismatch_q =
		    (cal->pwr_meas_odd_q * 32) / cal->pwr_meas_even_q;

		DPRINTFN(2, ("ADC gain calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_IGAIN, gain_mismatch_i);
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_QGAIN, gain_mismatch_q);
		AR_WRITE(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
	    AR_PHY_NEW_ADC_GAIN_CORR_ENABLE);
}

void
ar5008_calib_adc_dc_off(struct athn_softc *sc)
{
	struct athn_adc_cal *cal;
	int32_t dc_offset_mismatch_i, dc_offset_mismatch_q;
	uint32_t reg;
	int count, i;

	for (i = 0; i < AR_MAX_CHAINS; i++) {
		cal = &sc->calib.adc_dc_offset[i];

		/* Accumulate ADC DC offset measures (clear on read). */
		cal->pwr_meas_odd_i  += AR_READ(sc, AR_PHY_CAL_MEAS_0(i));
		cal->pwr_meas_even_i += AR_READ(sc, AR_PHY_CAL_MEAS_1(i));
		cal->pwr_meas_odd_q  += AR_READ(sc, AR_PHY_CAL_MEAS_2(i));
		cal->pwr_meas_even_q += AR_READ(sc, AR_PHY_CAL_MEAS_3(i));
	}
	if (++sc->calib.nsamples < AR_CAL_SAMPLES) {
		/* Not enough samples accumulated, continue. */
		ar5008_do_calib(sc);
		return;
	}

	count = (1 << (AR_MAX_LOG_CAL + 5)) * sc->calib.nsamples;

	for (i = 0; i < sc->nrxchains; i++) {
		cal = &sc->calib.adc_dc_offset[i];

		dc_offset_mismatch_i =
		    (cal->pwr_meas_even_i - cal->pwr_meas_odd_i * 2) / count;
		dc_offset_mismatch_q =
		    (cal->pwr_meas_odd_q - cal->pwr_meas_even_q * 2) / count;

		DPRINTFN(2, ("ADC DC offset calibration for chain %d\n", i));
		reg = AR_READ(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_QDC,
		    dc_offset_mismatch_q);
		reg = RW(reg, AR_PHY_NEW_ADC_DC_GAIN_IDC,
		    dc_offset_mismatch_i);
		AR_WRITE(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), reg);
	}

	AR_SETBITS(sc, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
	    AR_PHY_NEW_ADC_DC_OFFSET_CORR_ENABLE);
}

void
ar5008_write_txpower(struct athn_softc *sc, int16_t power[ATHN_POWER_COUNT])
{
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE1,
	    (power[ATHN_POWER_OFDM18  ] & 0x3f) << 24 |
	    (power[ATHN_POWER_OFDM12  ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM9   ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_OFDM6   ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE2,
	    (power[ATHN_POWER_OFDM54  ] & 0x3f) << 24 |
	    (power[ATHN_POWER_OFDM48  ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM36  ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_OFDM24  ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE3,
	    (power[ATHN_POWER_CCK2_SP ] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK2_LP ] & 0x3f) << 16 |
	    (power[ATHN_POWER_XR      ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK1_LP ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE4,
	    (power[ATHN_POWER_CCK11_SP] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK11_LP] & 0x3f) << 16 |
	    (power[ATHN_POWER_CCK55_SP] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK55_LP] & 0x3f));
#ifndef IEEE80211_NO_HT
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE5,
	    (power[ATHN_POWER_HT20(3) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT20(2) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT20(1) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT20(0) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE6,
	    (power[ATHN_POWER_HT20(7) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT20(6) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT20(5) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT20(4) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE7,
	    (power[ATHN_POWER_HT40(3) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT40(2) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT40(1) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT40(0) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE8,
	    (power[ATHN_POWER_HT40(7) ] & 0x3f) << 24 |
	    (power[ATHN_POWER_HT40(6) ] & 0x3f) << 16 |
	    (power[ATHN_POWER_HT40(5) ] & 0x3f) <<  8 |
	    (power[ATHN_POWER_HT40(4) ] & 0x3f));
	AR_WRITE(sc, AR_PHY_POWER_TX_RATE9,
	    (power[ATHN_POWER_OFDM_EXT] & 0x3f) << 24 |
	    (power[ATHN_POWER_CCK_EXT ] & 0x3f) << 16 |
	    (power[ATHN_POWER_OFDM_DUP] & 0x3f) <<  8 |
	    (power[ATHN_POWER_CCK_DUP ] & 0x3f));
#endif
}

void
ar5008_set_viterbi_mask(struct athn_softc *sc, int bin)
{
	uint32_t mask[4], reg;
	uint8_t m[62], p[62];	/* XXX use bit arrays? */
	int i, bit, cur;

	/* Compute pilot mask. */
	cur = -6000;
	for (i = 0; i < 4; i++) {
		mask[i] = 0;
		for (bit = 0; bit < 30; bit++) {
			if (abs(cur - bin) < 100)
				mask[i] |= 1 << bit;
			cur += 100;
		}
		if (cur == 0)	/* Skip entry "0". */
			cur = 100;
	}
	/* Write entries from -6000 to -3100. */
	AR_WRITE(sc, AR_PHY_TIMING7, mask[0]);
	AR_WRITE(sc, AR_PHY_TIMING9, mask[0]);
	/* Write entries from -3000 to -100. */
	AR_WRITE(sc, AR_PHY_TIMING8, mask[1]);
	AR_WRITE(sc, AR_PHY_TIMING10, mask[1]);
	/* Write entries from 100 to 3000. */
	AR_WRITE(sc, AR_PHY_PILOT_MASK_01_30, mask[2]);
	AR_WRITE(sc, AR_PHY_CHANNEL_MASK_01_30, mask[2]);
	/* Write entries from 3100 to 6000. */
	AR_WRITE(sc, AR_PHY_PILOT_MASK_31_60, mask[3]);
	AR_WRITE(sc, AR_PHY_CHANNEL_MASK_31_60, mask[3]);

	/* Compute viterbi mask. */
	for (cur = 6100; cur >= 0; cur -= 100)
		p[+cur / 100] = abs(cur - bin) < 75;
	for (cur = -100; cur >= -6100; cur -= 100)
		m[-cur / 100] = abs(cur - bin) < 75;

	/* Write viterbi mask (XXX needs to be reworked.) */
	reg =
	    m[46] << 30 | m[47] << 28 | m[48] << 26 | m[49] << 24 |
	    m[50] << 22 | m[51] << 20 | m[52] << 18 | m[53] << 16 |
	    m[54] << 14 | m[55] << 12 | m[56] << 10 | m[57] <<  8 |
	    m[58] <<  6 | m[59] <<  4 | m[60] <<  2 | m[61] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_1, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_46_61, reg);

	/* XXX m[48] should be m[38] ? */
	reg =             m[31] << 28 | m[32] << 26 | m[33] << 24 |
	    m[34] << 22 | m[35] << 20 | m[36] << 18 | m[37] << 16 |
	    m[48] << 14 | m[39] << 12 | m[40] << 10 | m[41] <<  8 |
	    m[42] <<  6 | m[43] <<  4 | m[44] <<  2 | m[45] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_2, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_31_45, reg);

	/* XXX This one is weird too. */
	reg =
	    m[16] << 30 | m[16] << 28 | m[18] << 26 | m[18] << 24 |
	    m[20] << 22 | m[20] << 20 | m[22] << 18 | m[22] << 16 |
	    m[24] << 14 | m[24] << 12 | m[25] << 10 | m[26] <<  8 |
	    m[27] <<  6 | m[28] <<  4 | m[29] <<  2 | m[30] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK_3, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_16_30, reg);

	reg =
	    m[ 0] << 30 | m[ 1] << 28 | m[ 2] << 26 | m[ 3] << 24 |
	    m[ 4] << 22 | m[ 5] << 20 | m[ 6] << 18 | m[ 7] << 16 |
	    m[ 8] << 14 | m[ 9] << 12 | m[10] << 10 | m[11] <<  8 |
	    m[12] <<  6 | m[13] <<  4 | m[14] <<  2 | m[15] <<  0;
	AR_WRITE(sc, AR_PHY_MASK_CTL, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_M_00_15, reg);

	reg =             p[15] << 28 | p[14] << 26 | p[13] << 24 |
	    p[12] << 22 | p[11] << 20 | p[10] << 18 | p[ 9] << 16 |
	    p[ 8] << 14 | p[ 7] << 12 | p[ 6] << 10 | p[ 5] <<  8 |
	    p[ 4] <<  6 | p[ 3] <<  4 | p[ 2] <<  2 | p[ 1] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_1, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_15_01, reg);

	reg =             p[30] << 28 | p[29] << 26 | p[28] << 24 |
	    p[27] << 22 | p[26] << 20 | p[25] << 18 | p[24] << 16 |
	    p[23] << 14 | p[22] << 12 | p[21] << 10 | p[20] <<  8 |
	    p[19] <<  6 | p[18] <<  4 | p[17] <<  2 | p[16] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_2, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_30_16, reg);

	reg =             p[45] << 28 | p[44] << 26 | p[43] << 24 |
	    p[42] << 22 | p[41] << 20 | p[40] << 18 | p[39] << 16 |
	    p[38] << 14 | p[37] << 12 | p[36] << 10 | p[35] <<  8 |
	    p[34] <<  6 | p[33] <<  4 | p[32] <<  2 | p[31] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_3, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_45_31, reg);

	reg =
	    p[61] << 30 | p[60] << 28 | p[59] << 26 | p[58] << 24 |
	    p[57] << 22 | p[56] << 20 | p[55] << 18 | p[54] << 16 |
	    p[53] << 14 | p[52] << 12 | p[51] << 10 | p[50] <<  8 |
	    p[49] <<  6 | p[48] <<  4 | p[47] <<  2 | p[46] <<  0;
	AR_WRITE(sc, AR_PHY_BIN_MASK2_4, reg);
	AR_WRITE(sc, AR_PHY_VIT_MASK2_P_61_46, reg);
}

void
ar5008_hw_init(struct athn_softc *sc, struct ieee80211_channel *c,
    struct ieee80211_channel *extc)
{
	struct athn_ops *ops = &sc->ops;
	const struct athn_ini *ini = sc->ini;
	const uint32_t *pvals;
	int i;

	AR_WRITE(sc, AR_PHY(0), 0x00000007);
	AR_WRITE(sc, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_EXTERNAL_RADIO);

	if (!AR_SINGLE_CHIP(sc))
		ar5416_reset_addac(sc, c);

	AR_WRITE(sc, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_INTERNAL_ADDAC);

	/* First initialization step (depends on channel band/bandwidth). */
#ifndef IEEE80211_NO_HT
	if (extc != NULL) {
		if (IEEE80211_IS_CHAN_2GHZ(c))
			pvals = ini->vals_2g40;
		else
			pvals = ini->vals_5g40;
	} else
#endif
	{
		if (IEEE80211_IS_CHAN_2GHZ(c))
			pvals = ini->vals_2g20;
		else
			pvals = ini->vals_5g20;
	}
	DPRINTFN(4, ("writing modal init vals\n"));
	for (i = 0; i < ini->nregs; i++) {
		AR_WRITE(sc, ini->regs[i], pvals[i]);
		if (AR_IS_ANALOG_REG(ini->regs[i]))
			DELAY(100);
		if ((i & 0x1f) == 0)
			DELAY(1);
	}

	if (sc->rx_gain != NULL)
		ar9280_reset_rx_gain(sc, c);
	if (sc->tx_gain != NULL)
		ar9280_reset_tx_gain(sc, c);

	/* Second initialization step (common to all channels). */
	DPRINTFN(4, ("writing common init vals\n"));
	for (i = 0; i < ini->ncmregs; i++) {
		AR_WRITE(sc, ini->cmregs[i], ini->cmvals[i]);
		if (AR_IS_ANALOG_REG(ini->cmregs[i]))
			DELAY(100);
		if ((i & 0x1f) == 0)
			DELAY(1);
	}

	if (!AR_SINGLE_CHIP(sc))
		ar5416_reset_bb_gain(sc, c);

	if (IEEE80211_IS_CHAN_5GHZ(c) &&
	    (sc->flags & ATHN_FLAG_FAST_PLL_CLOCK)) {
		/* Update modal values for fast PLL clock. */
#ifndef IEEE80211_NO_HT
		if (extc != NULL)
			pvals = ini->fastvals_5g40;
		else
#endif
			pvals = ini->fastvals_5g20;
		DPRINTFN(4, ("writing fast pll clock init vals\n"));
		for (i = 0; i < ini->nfastregs; i++) {
			AR_WRITE(sc, ini->fastregs[i], pvals[i]);
			if (AR_IS_ANALOG_REG(ini->fastregs[i]))
				DELAY(100);
			if ((i & 0x1f) == 0)
				DELAY(1);
		}
	}

	/*
	 * Set the RX_ABORT and RX_DIS bits to prevent frames with corrupted
	 * descriptor status.
	 */
	AR_SETBITS(sc, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);

	/* Hardware workarounds for occasional Rx data corruption. */
	if (AR_SREV_9287_10_OR_LATER(sc))
		AR_CLRBITS(sc, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_HWWAR1);
	else if (AR_SREV_9280_10_OR_LATER(sc))
		AR_CLRBITS(sc, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_HWWAR1 |
		    AR_PCU_MISC_MODE2_HWWAR2);

	if (AR_SREV_5416_20_OR_LATER(sc) && !AR_SREV_9280_10_OR_LATER(sc)) {
		/* Disable baseband clock gating. */
		AR_WRITE(sc, AR_PHY(651), 0x11);

		if (AR_SREV_9160(sc)) {
			/* Disable RIFS search to fix baseband hang. */
			AR_CLRBITS(sc, AR_PHY_HEAVY_CLIP_FACTOR_RIFS,
			    AR_PHY_RIFS_INIT_DELAY_M);
		}
	}

	ar5008_set_phy(sc, c, extc);
	ar5008_init_chains(sc);

	if (sc->flags & ATHN_FLAG_OLPC)
		ops->olpc_init(sc);
	ops->set_txpower(sc, c, extc);

	if (!AR_SINGLE_CHIP(sc))
		ar5416_rf_reset(sc, c);
}

uint8_t
ar5008_get_vpd(uint8_t pwr, const uint8_t *pwrPdg, const uint8_t *vpdPdg,
    int nicepts)
{
	uint8_t vpd;
	int i, lo, hi;

	for (i = 0; i < nicepts; i++)
		if (pwrPdg[i] > pwr)
			break;
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nicepts)
		hi = lo;

	vpd = athn_interpolate(pwr, pwrPdg[lo], vpdPdg[lo],
	    pwrPdg[hi], vpdPdg[hi]);
	return (vpd);
}

void
ar5008_get_pdadcs(struct athn_softc *sc, uint8_t fbin,
    struct athn_pier *lopier, struct athn_pier *hipier, int nxpdgains,
    int nicepts, uint8_t overlap, uint8_t *boundaries, uint8_t *pdadcs)
{
#define DB(x)	((x) / 2)	/* Convert half dB to dB. */
	uint8_t minpwr[AR_PD_GAINS_IN_MASK], maxpwr[AR_PD_GAINS_IN_MASK];
	uint8_t vpd[AR_MAX_PWR_RANGE_IN_HALF_DB], pwr;
	uint8_t lovpd, hivpd, boundary;
	int16_t ss, delta, vpdstep, val;
	int i, j, npdadcs, nvpds, maxidx, tgtidx;

	/* Compute min and max power in half dB for each pdGain. */
	for (i = 0; i < nxpdgains; i++) {
		minpwr[i] = MAX(lopier->pwr[i][0], hipier->pwr[i][0]);
		maxpwr[i] = MIN(lopier->pwr[i][nicepts - 1],
		    hipier->pwr[i][nicepts - 1]);
	}

	npdadcs = 0;
	for (i = 0; i < nxpdgains; i++) {
		if (i != nxpdgains - 1)
			boundaries[i] = DB(maxpwr[i] + minpwr[i + 1]) / 2;
		else
			boundaries[i] = DB(maxpwr[i]);
		if (boundaries[i] > AR_MAX_RATE_POWER)
			boundaries[i] = AR_MAX_RATE_POWER;

		if (i == 0 && !AR_SREV_5416_20_OR_LATER(sc)) {
			/* Fix the gain delta (AR5416 1.0 only.) */
			delta = boundaries[0] - 23;
			boundaries[0] = 23;
		} else
			delta = 0;

		/* Find starting index for this pdGain. */
		if (i != 0) {
			ss = boundaries[i - 1] - DB(minpwr[i]) -
			    overlap + 1 + delta;
		} else if (AR_SREV_9280_10_OR_LATER(sc)) {
			ss = -DB(minpwr[i]);
		} else
			ss = 0;

		/* Compute Vpd table for this pdGain. */
		nvpds = DB(maxpwr[i] - minpwr[i]) + 1;
		memset(vpd, 0, sizeof(vpd));
		pwr = minpwr[i];
		for (j = 0; j < nvpds; j++) {
			/* Get lower and higher Vpd. */
			lovpd = ar5008_get_vpd(pwr, lopier->pwr[i],
			    lopier->vpd[i], nicepts);
			hivpd = ar5008_get_vpd(pwr, hipier->pwr[i],
			    hipier->vpd[i], nicepts);

			/* Interpolate the final Vpd. */
			vpd[j] = athn_interpolate(fbin,
			    lopier->fbin, lovpd, hipier->fbin, hivpd);

			pwr += 2;	/* In half dB. */
		}

		/* Extrapolate data for ss < 0. */
		if (vpd[1] > vpd[0])
			vpdstep = vpd[1] - vpd[0];
		else
			vpdstep = 1;
		while (ss < 0 && npdadcs < AR_NUM_PDADC_VALUES - 1) {
			val = vpd[0] + ss * vpdstep;
			pdadcs[npdadcs++] = MAX(val, 0);
			ss++;
		}

		tgtidx = boundaries[i] + overlap - DB(minpwr[i]);
		maxidx = MIN(tgtidx, nvpds);
		while (ss < maxidx && npdadcs < AR_NUM_PDADC_VALUES - 1)
			pdadcs[npdadcs++] = vpd[ss++];

		if (tgtidx <= maxidx)
			continue;

		/* Extrapolate data for maxidx <= ss <= tgtidx. */
		if (vpd[nvpds - 1] > vpd[nvpds - 2])
			vpdstep = vpd[nvpds - 1] - vpd[nvpds - 2];
		else
			vpdstep = 1;
		while (ss <= tgtidx && npdadcs < AR_NUM_PDADC_VALUES - 1) {
			val = vpd[nvpds - 1] + (ss - maxidx + 1) * vpdstep;
			pdadcs[npdadcs++] = MIN(val, 255);
			ss++;
		}
	}

	/* Fill remaining PDADC and boundaries entries. */
	if (AR_SREV_9285(sc))
		boundary = AR9285_PD_GAIN_BOUNDARY_DEFAULT;
	else	/* Fill with latest. */
		boundary = boundaries[nxpdgains - 1];

	for (; nxpdgains < AR_PD_GAINS_IN_MASK; nxpdgains++)
		boundaries[nxpdgains] = boundary;

	for (; npdadcs < AR_NUM_PDADC_VALUES; npdadcs++)
		pdadcs[npdadcs] = pdadcs[npdadcs - 1];
#undef DB
}

void
ar5008_get_lg_tpow(struct athn_softc *sc, struct ieee80211_channel *c,
    uint8_t ctl, const struct ar_cal_target_power_leg *tgt, int nchans,
    uint8_t tpow[4])
{
	uint8_t fbin;
	int i, lo, hi;

	/* Find interval (lower and upper indices.) */
	fbin = athn_chan2fbin(c);
	for (i = 0; i < nchans; i++) {
		if (tgt[i].bChannel == AR_BCHAN_UNUSED ||
		    tgt[i].bChannel > fbin)
			break;
	}
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nchans || tgt[hi].bChannel == AR_BCHAN_UNUSED)
		hi = lo;

	/* Interpolate values. */
	for (i = 0; i < 4; i++) {
		tpow[i] = athn_interpolate(fbin,
		    tgt[lo].bChannel, tgt[lo].tPow2x[i],
		    tgt[hi].bChannel, tgt[hi].tPow2x[i]);
	}
	/* XXX Apply conformance test limit. */
}

#ifndef IEEE80211_NO_HT
void
ar5008_get_ht_tpow(struct athn_softc *sc, struct ieee80211_channel *c,
    uint8_t ctl, const struct ar_cal_target_power_ht *tgt, int nchans,
    uint8_t tpow[8])
{
	uint8_t fbin;
	int i, lo, hi;

	/* Find interval (lower and upper indices.) */
	fbin = athn_chan2fbin(c);
	for (i = 0; i < nchans; i++) {
		if (tgt[i].bChannel == AR_BCHAN_UNUSED ||
		    tgt[i].bChannel > fbin)
			break;
	}
	hi = i;
	lo = hi - 1;
	if (lo == -1)
		lo = hi;
	else if (hi == nchans || tgt[hi].bChannel == AR_BCHAN_UNUSED)
		hi = lo;

	/* Interpolate values. */
	for (i = 0; i < 8; i++) {
		tpow[i] = athn_interpolate(fbin,
		    tgt[lo].bChannel, tgt[lo].tPow2x[i],
		    tgt[hi].bChannel, tgt[hi].tPow2x[i]);
	}
	/* XXX Apply conformance test limit. */
}
#endif

/*
 * Adaptive noise immunity.
 */
void
ar5008_set_noise_immunity_level(struct athn_softc *sc, int level)
{
	int high = level == 4;
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_DESIRED_SZ);
	reg = RW(reg, AR_PHY_DESIRED_SZ_TOT_DES, high ? -62 : -55);
	AR_WRITE(sc, AR_PHY_DESIRED_SZ, reg);

	reg = AR_READ(sc, AR_PHY_AGC_CTL1);
	reg = RW(reg, AR_PHY_AGC_CTL1_COARSE_LOW, high ? -70 : -64);
	reg = RW(reg, AR_PHY_AGC_CTL1_COARSE_HIGH, high ? -12 : -14);
	AR_WRITE(sc, AR_PHY_AGC_CTL1, reg);

	reg = AR_READ(sc, AR_PHY_FIND_SIG);
	reg = RW(reg, AR_PHY_FIND_SIG_FIRPWR, high ? -80 : -78);
	AR_WRITE(sc, AR_PHY_FIND_SIG, reg);
}

void
ar5008_enable_ofdm_weak_signal(struct athn_softc *sc)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_SFCORR_LOW);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M1_THRESH_LOW, 50);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2_THRESH_LOW, 40);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, 48);
	AR_WRITE(sc, AR_PHY_SFCORR_LOW, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR);
	reg = RW(reg, AR_PHY_SFCORR_M1_THRESH, 77);
	reg = RW(reg, AR_PHY_SFCORR_M2_THRESH, 64);
	reg = RW(reg, AR_PHY_SFCORR_M2COUNT_THR, 16);
	AR_WRITE(sc, AR_PHY_SFCORR, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR_EXT);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH_LOW, 50);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH_LOW, 40);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH, 77);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH, 64);
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, reg);

	AR_SETBITS(sc, AR_PHY_SFCORR_LOW,
	    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
}

void
ar5008_disable_ofdm_weak_signal(struct athn_softc *sc)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_SFCORR_LOW);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M1_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, 63);
	AR_WRITE(sc, AR_PHY_SFCORR_LOW, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR);
	reg = RW(reg, AR_PHY_SFCORR_M1_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_M2_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_M2COUNT_THR, 31);
	AR_WRITE(sc, AR_PHY_SFCORR, reg);

	reg = AR_READ(sc, AR_PHY_SFCORR_EXT);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH_LOW, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M1_THRESH, 127);
	reg = RW(reg, AR_PHY_SFCORR_EXT_M2_THRESH, 127);
	AR_WRITE(sc, AR_PHY_SFCORR_EXT, reg);

	AR_CLRBITS(sc, AR_PHY_SFCORR_LOW,
	    AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
}

void
ar5008_set_cck_weak_signal(struct athn_softc *sc, int high)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_CCK_DETECT);
	reg = RW(reg, AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK, high ? 6 : 8);
	AR_WRITE(sc, AR_PHY_CCK_DETECT, reg);
}

void
ar5008_set_firstep_level(struct athn_softc *sc, int level)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_FIND_SIG);
	reg = RW(reg, AR_PHY_FIND_SIG_FIRSTEP, level * 4);
	AR_WRITE(sc, AR_PHY_FIND_SIG, reg);
}

void
ar5008_set_spur_immunity_level(struct athn_softc *sc, int level)
{
	uint32_t reg;

	reg = AR_READ(sc, AR_PHY_TIMING5);
	reg = RW(reg, AR_PHY_TIMING5_CYCPWR_THR1, (level + 1) * 2);
	AR_WRITE(sc, AR_PHY_TIMING5, reg);
}
