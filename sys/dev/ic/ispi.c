/* $OpenBSD: ispi.c,v 1.1 2025/11/14 01:55:07 jcs Exp $ */
/*
 * Intel LPSS SPI controller
 *
 * Copyright (c) 2015-2019 joshua stein <jcs@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/spi/spivar.h>

#include <dev/pci/lpssreg.h>
#include <dev/ic/ispivar.h>

#define SSCR0			0x0	/* SSP Control Register 0 */
#define   SSCR0_EDSS_0		(0 << 20)
#define   SSCR0_EDSS_1		(1 << 20)
#define   SSCR0_SCR_SHIFT	(8)
#define   SSCR0_SCR_MASK	(0xFFF)
#define   SSCR0_SSE		(1 << 7)
#define   SSCR0_ECS_ON_CHIP	(0 << 6)
#define   SSCR0_FRF_MOTOROLA	(0 << 4)
#define   SSCR0_DSS_SHIFT	(0)
#define   SSCR0_DSS_MASK	(0xF)
#define SSCR1			0x4	/* SSP Control Register 1 */
#define   SSCR1_RIE		(1 << 0) /* Receive FIFO Interrupt Enable */
#define   SSCR1_TIE		(1 << 1) /* Transmit FIFO Interrupt Enable */
#define   SSCR1_LBM		(1 << 2) /* Loop-Back Mode */
#define   SSCR1_SPO_LOW		(0 << 3) /* Motorola SPI SSPSCLK polarity setting */
#define   SSCR1_SPO_HIGH	(1 << 3)
#define   SSCR1_SPH_FIRST	(0 << 4) /* Motorola SPI SSPSCLK phase setting */
#define   SSCR1_SPH_SECOND	(1 << 4)
#define   SSCR1_MWDS		(1 << 5) /* Microwire Transmit Data Size */
#define   SSCR1_IFS_LOW		(0 << 16)
#define   SSCR1_IFS_HIGH	(1 << 16)
#define   SSCR1_PINTE		(1 << 18) /* Peripheral Trailing Byte Interrupt Enable */
#define   SSCR1_TINTE		(1 << 19) /* Receiver Time-out Interrupt enable */
#define   SSCR1_RX_THRESH_DEF	8
#define   SSCR1_RX_THRESH(x)	(((x) - 1) << 10)
#define   SSCR1_RFT		(0x00003c00)	/* Receive FIFO Threshold (mask) */
#define   SSCR1_TX_THRESH_DEF	8
#define   SSCR1_TX_THRESH(x)	(((x) - 1) << 6)
#define   SSCR1_TFT		(0x000003c0)	/* Transmit FIFO Threshold (mask) */
#define SSSR			0x8	/* SSP Status Register */
#define   SSSR_TUR		(1 << 21)  /* Tx FIFO underrun */
#define   SSSR_TINT		(1 << 19)  /* Rx Time-out interrupt */
#define   SSSR_PINT		(1 << 18)  /* Peripheral trailing byte interrupt */
#define   SSSR_ROR		(1 << 7)   /* Rx FIFO Overrun */
#define   SSSR_BSY		(1 << 4)   /* SSP Busy */
#define   SSSR_RNE		(1 << 3)   /* Receive FIFO not empty */
#define   SSSR_TNF		(1 << 2)   /* Transmit FIFO not full */
#define SSDR			0x10	/* SSP Data Register */
#define SSTO			0x28	/* SSP Time out */
#define SSPSP			0x2C	/* SSP Programmable Serial Protocol */
#define SSITF			0x44	/* SPI Transmit FIFO */
#define   SSITF_LEVEL_SHIFT	(16)
#define   SSITF_LEVEL_MASK	(0x3f)
#define   SSITF_TX_LO_THRESH(x)	(((x) - 1) << 8)
#define   SSITF_TX_HI_THRESH(x)	((x) - 1)
#define SSIRF			0x48	/* SPI Receive FIFO */
#define   SSIRF_LEVEL_SHIFT	(8)
#define   SSIRF_LEVEL_MASK	(0x3f)
#define   SSIRF_RX_THRESH(x)	((x) - 1)

void		ispi_cs_change(struct ispi_softc *, int);
uint32_t	ispi_lpss_read(struct ispi_softc *, int);
void		ispi_lpss_write(struct ispi_softc *, int, uint32_t);
int		ispi_rx_fifo_empty(struct ispi_softc *);
int		ispi_tx_fifo_full(struct ispi_softc *);

struct cfdriver ispi_cd = {
	NULL, "ispi", DV_DULL
};

int
ispi_activate(struct device *self, int act)
{
	return config_activate_children(self, act);
}

int
ispi_spi_print(void *aux, const char *pnp)
{
	struct spi_attach_args *sa = aux;

	if (pnp != NULL)
		printf("\"%s\" at %s", sa->sa_name, pnp);

	return UNCONF;
}

int
ispi_acquire_bus(void *cookie, int flags)
{
	struct ispi_softc *sc = cookie;

	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));

	rw_enter(&sc->sc_buslock, RW_WRITE);
	ispi_cs_change(sc, 1);

	return 0;
}

void
ispi_release_bus(void *cookie, int flags)
{
	struct ispi_softc *sc = cookie;

	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));

	ispi_cs_change(sc, 0);
	rw_exit(&sc->sc_buslock);
}

void
ispi_init(struct ispi_softc *sc)
{
	uint32_t csctrl;

	if (!sc->sc_rx_threshold)
		sc->sc_rx_threshold = SSCR1_RX_THRESH_DEF;
	if (!sc->sc_tx_threshold)
		sc->sc_tx_threshold = SSCR1_TX_THRESH_DEF;

	ispi_write(sc, SSCR0, 0);
	ispi_write(sc, SSCR1, 0);
	ispi_write(sc, SSTO, 0);
	ispi_write(sc, SSPSP, 0);

	/* lpss: enable software chip select control */
	csctrl = ispi_lpss_read(sc, sc->sc_reg_cs_ctrl);
	csctrl &= ~(LPSS_CS_CONTROL_SW_MODE | LPSS_CS_CONTROL_CS_HIGH);
	csctrl |= LPSS_CS_CONTROL_SW_MODE | LPSS_CS_CONTROL_CS_HIGH;
	ispi_lpss_write(sc, sc->sc_reg_cs_ctrl, csctrl);

	ispi_cs_change(sc, 0);
}

uint32_t
ispi_read(struct ispi_softc *sc, int reg)
{
	uint32_t val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, reg);
	DPRINTF(("%s: %s(0x%x) = 0x%x\n", sc->sc_dev.dv_xname, __func__, reg,
	    val));
	return val;
}

void
ispi_write(struct ispi_softc *sc, int reg, uint32_t val)
{
	DPRINTF(("%s: %s(0x%x, 0x%x)\n", sc->sc_dev.dv_xname, __func__, reg,
	    val));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, reg, val);
}

uint32_t
ispi_lpss_read(struct ispi_softc *sc, int reg)
{
	return ispi_read(sc, sc->sc_lpss_reg_offset + reg);
}

void
ispi_lpss_write(struct ispi_softc *sc, int reg, uint32_t val)
{
	ispi_write(sc, sc->sc_lpss_reg_offset + reg, val);
}

void
ispi_config(void *cookie, struct spi_config *conf)
{
	struct ispi_softc *sc = cookie;
	uint32_t sscr0, sscr1;
	unsigned int rate;
	int rx_threshold, tx_threshold;

	DPRINTF(("%s: %s: ssp clk %ld sc_freq %d\n", sc->sc_dev.dv_xname,
	    __func__, sc->sc_ssp_clk, conf->sc_freq));

	rate = min(sc->sc_ssp_clk, conf->sc_freq);
	if (rate <= 1)
		rate = sc->sc_ssp_clk;

	sscr0 = ((sc->sc_ssp_clk / rate) - 1) & 0xfff;
	sscr0 |= SSCR0_FRF_MOTOROLA;
	sscr0 |= (conf->sc_bpw - 1);
	sscr0 |= SSCR0_SSE;
	sscr0 |= (conf->sc_bpw > 16 ? SSCR0_EDSS_1 : SSCR0_EDSS_0);

	ispi_clear_status(sc);

	rx_threshold = SSIRF_RX_THRESH(sc->sc_rx_threshold);
	tx_threshold = SSITF_TX_LO_THRESH(sc->sc_tx_threshold) |
	    SSITF_TX_HI_THRESH(sc->sc_tx_threshold_hi);

	if ((ispi_read(sc, SSIRF) & 0xff) != rx_threshold)
		ispi_write(sc, SSIRF, rx_threshold);

	if ((ispi_read(sc, SSITF) & 0xffff) != tx_threshold)
		ispi_write(sc, SSITF, tx_threshold);

	ispi_write(sc, SSCR0, sscr0 & ~SSCR0_SSE);

	ispi_write(sc, SSTO, 1000); /* timeout */

	sscr1 = (SSCR1_RX_THRESH(sc->sc_rx_threshold) & SSCR1_RFT) |
	    (SSCR1_TX_THRESH(sc->sc_tx_threshold) & SSCR1_TFT);

#if 0
	/* enable interrupts */
	sscr1 |= (SSCR1_TIE | SSCR1_RIE | SSCR1_TINTE);
#endif
	ispi_write(sc, SSCR1, sscr1);

	/* restart SSP */
	ispi_write(sc, SSCR0, sscr0);
	ispi_write(sc, SSCR1, sscr1);
}

int
ispi_flush(struct ispi_softc *sc)
{
	int tries = 500;

	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));

	do {
		while (!ispi_rx_fifo_empty(sc))
			ispi_read(sc, SSDR);
	} while ((ispi_read(sc, SSSR) & SSSR_BSY) && --tries);

	DPRINTF(("%s: flushed with %d left\n", sc->sc_dev.dv_xname, tries));

	ispi_write(sc, SSSR, SSSR_ROR);

	return (tries != 0);
}

void
ispi_cs_change(struct ispi_softc *sc, int cs_assert)
{
	uint32_t t;
	int tries = 500;

	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));

	if (!cs_assert) {
		/* wait until idle */
		while ((ispi_read(sc, SSSR) & SSSR_BSY) && --tries)
			;
	}

	t = ispi_lpss_read(sc, sc->sc_reg_cs_ctrl);
	if (cs_assert)
		t &= ~LPSS_CS_CONTROL_CS_HIGH;
	else
		t |= LPSS_CS_CONTROL_CS_HIGH;
	ispi_lpss_write(sc, sc->sc_reg_cs_ctrl, t);

	DELAY(10);
}

int
ispi_transfer(void *cookie, char *out, char *in, int len, int flags)
{
	struct ispi_softc *sc = cookie;
	int s = spltty();

	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));

	sc->sc_ridx = sc->sc_widx = 0;

	/* drain input buffer */
	ispi_flush(sc);

	while (sc->sc_ridx < len || sc->sc_widx < len) {
		while (!ispi_rx_fifo_empty(sc) && sc->sc_ridx < len) {
			if (in)
				in[sc->sc_ridx] = ispi_read(sc, SSDR) & 0xff;
			else
				ispi_read(sc, SSDR);

			sc->sc_ridx++;
		}

		while (!ispi_tx_fifo_full(sc) && sc->sc_widx < len) {
			if (out)
				ispi_write(sc, SSDR, out[sc->sc_widx]);
			else
				ispi_write(sc, SSDR, 0);

			sc->sc_widx++;
		}
	}

	DPRINTF(("%s: %s: done transmitting %s %d\n", sc->sc_dev.dv_xname,
	    __func__, (out ? "out" : "in"), len));

	splx(s);
	return 0;
}

int
ispi_status(struct ispi_softc *sc)
{
	return ispi_read(sc, SSSR);
}

void
ispi_clear_status(struct ispi_softc *sc)
{
	ispi_write(sc, SSSR, SSSR_TUR | SSSR_TINT | SSSR_PINT | SSSR_ROR);
}

int
ispi_rx_fifo_empty(struct ispi_softc *sc)
{
	return !(ispi_status(sc) & SSSR_RNE);
}

int
ispi_tx_fifo_full(struct ispi_softc *sc)
{
	return !(ispi_status(sc) & SSSR_TNF);
}

int
ispi_rx_fifo_overrun(struct ispi_softc *sc)
{
	if (ispi_status(sc) & SSSR_ROR) {
		printf("%s: %s\n", sc->sc_dev.dv_xname, __func__);
		return 1;
	}

	return 0;
}

int
ispi_intr(void *arg)
{
#ifdef ISPI_DEBUG
	struct ispi_softc *sc = arg;
	DPRINTF(("%s: %s\n", sc->sc_dev.dv_xname, __func__));
#endif
	return 1;
}
