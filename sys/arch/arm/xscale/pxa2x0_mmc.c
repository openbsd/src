/*	$OpenBSD: pxa2x0_mmc.c,v 1.10 2011/04/07 15:30:15 miod Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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
 * MMC/SD/SDIO controller driver for Intel PXA27x processors
 *
 * Power management is beyond control of the processor's SD/SDIO/MMC
 * block, so this driver depends on the attachment driver to provide
 * us with some callback functions via the "tag" member in our softc.
 * Bus power management calls are then dispatched to the attachment
 * driver.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <arch/arm/xscale/pxa2x0_gpio.h>
#include <arch/arm/xscale/pxa2x0reg.h>
#include <arch/arm/xscale/pxa2x0var.h>
#include <arch/arm/xscale/pxammcvar.h>

#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

/* GPIO pins */
#define PXAMMC_MMCLK		32
#define PXAMMC_MMCMD		112
#define PXAMMC_MMDAT0		92
#define PXAMMC_MMDAT1		109
#define PXAMMC_MMDAT2		110
#define PXAMMC_MMDAT3		111

int	pxammc_host_reset(sdmmc_chipset_handle_t);
u_int32_t pxammc_host_ocr(sdmmc_chipset_handle_t);
int	pxammc_host_maxblklen(sdmmc_chipset_handle_t);
int	pxammc_card_detect(sdmmc_chipset_handle_t);
int	pxammc_bus_power(sdmmc_chipset_handle_t, u_int32_t);
int	pxammc_bus_clock(sdmmc_chipset_handle_t, int);
void	pxammc_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);
void	pxammc_clock_stop(struct pxammc_softc *);
void	pxammc_clock_start(struct pxammc_softc *);
int	pxammc_card_intr(void *);
int	pxammc_intr(void *);
inline void	pxammc_intr_cmd(struct pxammc_softc *);
inline void	pxammc_intr_data(struct pxammc_softc *);
void	pxammc_intr_done(struct pxammc_softc *);

#define CSR_READ_1(sc, reg) \
	bus_space_read_1((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define CSR_WRITE_1(sc, reg, val) \
	bus_space_write_1((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_READ_4(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define CSR_WRITE_4(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define CSR_SET_4(sc, reg, bits) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) | (bits))
#define CSR_CLR_4(sc, reg, bits) \
	CSR_WRITE_4((sc), (reg), CSR_READ_4((sc), (reg)) & ~(bits))

struct sdmmc_chip_functions pxammc_functions = {
	/* host controller reset */
	pxammc_host_reset,
	/* host controller capabilities */
	pxammc_host_ocr,
	pxammc_host_maxblklen,
	/* card detection */
	pxammc_card_detect,
	/* bus power and clock frequency */
	pxammc_bus_power,
	pxammc_bus_clock,
	/* command execution */
	pxammc_exec_command
};

struct cfdriver pxammc_cd = {
	NULL, "pxammc", DV_DULL
};

#ifdef SDMMC_DEBUG
int sdhcdebug = 0;	/* XXX must be named sdhcdebug for sdmmc.c */
#define DPRINTF(n,s)	do { if ((n) <= sdhcdebug) printf s; } while (0)
#else
#define DPRINTF(n,s)	do {} while (0)
#endif

int
pxammc_match(void)
{
	return (cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X;
}

void
pxammc_attach(struct pxammc_softc *sc, void *aux)
{
	struct pxaip_attach_args *pxa = aux;
	struct sdmmcbus_attach_args saa;
	int s;

	/* Enable the clocks to the MMC controller. */
	pxa2x0_clkman_config(CKEN_MMC, 1);

	sc->sc_iot = pxa->pxa_sa.sa_iot;
	if (bus_space_map(sc->sc_iot, PXA2X0_MMC_BASE, PXA2X0_MMC_SIZE, 0,
	    &sc->sc_ioh) != 0) {
		printf(": can't map regs\n");
		goto fail;
	}

	/*
	 * Establish the card detection and MMC interrupt handlers and
	 * mask all interrupts until we are prepared to handle them.
	 */
	s = splsdmmc();

	pxa2x0_gpio_set_function(sc->sc_gpio_detect, GPIO_IN);
	sc->sc_card_ih = pxa2x0_gpio_intr_establish(sc->sc_gpio_detect,
	    IST_EDGE_BOTH, IPL_SDMMC, pxammc_card_intr, sc, "mmccd");
	if (sc->sc_card_ih == NULL) {
		splx(s);
		printf(": can't establish card interrupt\n");
		goto fail;
	}
	pxa2x0_gpio_intr_mask(sc->sc_card_ih);

	sc->sc_ih = pxa2x0_intr_establish(PXA2X0_INT_MMC, IPL_SDMMC,
	    pxammc_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		splx(s);
		printf(": can't establish MMC interrupt\n");
		goto fail;
	}
	CSR_WRITE_4(sc, MMC_I_MASK, 0xffffffff);

	splx(s);

	printf(": MMC/SD/SDIO controller\n");

	/*
	 * Configure the GPIO pins.  In SD/MMC mode, all pins except
	 * MMCLK are bidirectional and the direction is controlled in
	 * hardware without our assistence.
	 */
	pxa2x0_gpio_set_function(PXAMMC_MMCLK, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(PXAMMC_MMCMD, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(PXAMMC_MMDAT0, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(PXAMMC_MMDAT1, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(PXAMMC_MMDAT2, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(PXAMMC_MMDAT3, GPIO_ALT_FN_1_IN);

	/*
	 * Reset the host controller and unmask normal interrupts.
	 */
	(void)pxammc_host_reset(sc);

	/*
	 * Attach the generic sdmmc bus driver.
	 */
	bzero(&saa, sizeof saa);
	saa.saa_busname = "sdmmc";
	saa.sct = &pxammc_functions;
	saa.sch = sc;
	saa.flags = SMF_STOP_AFTER_MULTIPLE;
	saa.max_xfer = 1;

	sc->sc_sdmmc = config_found(&sc->sc_dev, &saa, NULL);
	if (sc->sc_sdmmc == NULL) {
		printf("%s: can't attach bus\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	/* Enable card detection interrupt. */
	pxa2x0_gpio_intr_unmask(sc->sc_card_ih);
	return;

fail:
	if (sc->sc_ih != NULL) {
		pxa2x0_intr_disestablish(sc->sc_ih);
		sc->sc_ih = NULL;
	}
	if (sc->sc_card_ih != NULL) {
		pxa2x0_gpio_intr_disestablish(sc->sc_card_ih);
		sc->sc_card_ih = NULL;
	}
	if (sc->sc_ioh != 0) {
		bus_space_unmap(sc->sc_iot, sc->sc_ioh, PXA2X0_MMC_SIZE);
		sc->sc_ioh = 0;
	}
	pxa2x0_clkman_config(CKEN_MMC, 0);
}

int
pxammc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct pxammc_softc *sc = sch;
	int s = splsdmmc();

	/* Make sure to initialize the card before the next command. */
	CLR(sc->sc_flags, PMF_CARD_INITED);

	/* Disable SPI mode (we don't support SPI). */
	CSR_WRITE_4(sc, MMC_SPI, 0);

	/* Set response timeout to maximum. */
	CSR_WRITE_4(sc, MMC_RESTO, 0x7f);

	/* Enable all interrupts. */
	CSR_WRITE_4(sc, MMC_I_MASK, 0);

	splx(s);
	return 0;
}

int
pxammc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	return 2048;
}

u_int32_t
pxammc_host_ocr(sdmmc_chipset_handle_t sch)
{
	struct pxammc_softc *sc = sch;

	if (sc->tag.get_ocr != NULL)
		return sc->tag.get_ocr(sc->tag.cookie);

	DPRINTF(0,("%s: driver lacks get_ocr() function\n",
	    sc->sc_dev.dv_xname));
	return ENXIO;
}

int
pxammc_card_detect(sdmmc_chipset_handle_t sch)
{
	struct pxammc_softc *sc = sch;
	return !pxa2x0_gpio_get_bit(sc->sc_gpio_detect);
}

int
pxammc_bus_power(sdmmc_chipset_handle_t sch, u_int32_t ocr)
{
	struct pxammc_softc *sc = sch;

	/*
	 * Bus power management is beyond control of the SD/SDIO/MMC
	 * block of the PXA2xx processors, so we have to hand this
	 * task off to the attachment driver.
	 */
	if (sc->tag.set_power != NULL)
		return sc->tag.set_power(sc->tag.cookie, ocr);

	DPRINTF(0,("%s: driver lacks set_power() function\n",
	    sc->sc_dev.dv_xname));
	return ENXIO;
}

int
pxammc_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct pxammc_softc *sc = sch;
	int actfreq = 19500; /* KHz */
	int div = 0;
	int s;

	s = splsdmmc();

	/* Stop the clock and wait for the interrupt. */
	pxammc_clock_stop(sc);

	/* Just stop the clock. */
	if (freq == 0) {
		splx(s);
		return 0;
	}

	/*
	 * PXA27x Errata...
	 *
	 * <snip>
	 * E40. SDIO: SDIO Devices Not Working at 19.5 Mbps
	 *
	 * SD/SDIO controller can only support up to 9.75 Mbps data
	 * transfer rate for SDIO card.
	 * </snip>
	 *
	 * If we don't limit the frequency, CRC errors will be
	 * reported by the controller after we set the bus speed.
	 * XXX slow down incrementally.
	 */
	if (freq > 9750)
		freq = 9750;

	/*
	 * Pick the smallest divider that produces a frequency not
	 * more than `freq' KHz.
	 */
	while (div < 7) {
		if (actfreq <= freq)
			break;
		actfreq /= 2;
		div++;
	}
	if (div == 7) {
		splx(s);
		printf("%s: unsupported bus frequency of %d KHz\n",
		    sc->sc_dev.dv_xname, freq);
		return -1;
	}

	DPRINTF(1,("pxammc_bus_clock freq=%d actfreq=%d div=%d\n",
	    freq, actfreq, div));
	sc->sc_clkdiv = div;
	pxammc_clock_start(sc);
	splx(s);
	return 0;
}

void
pxammc_exec_command(sdmmc_chipset_handle_t sch,
    struct sdmmc_command *cmd)
{
	struct pxammc_softc *sc = sch;
	u_int32_t cmdat;
	int timo;
	int s;

	DPRINTF(1,("%s: cmd %u arg=%#x data=%#x dlen=%d flags=%#x "
	    "proc=\"%s\"\n", sc->sc_dev.dv_xname, cmd->c_opcode,
	    cmd->c_arg, cmd->c_data, cmd->c_datalen, cmd->c_flags,
	    curproc ? curproc->p_comm : ""));

	s = splsdmmc();

	/* Stop the bus clock (MMCLK). [15.8.3] */
	pxammc_clock_stop(sc);

	/* Set the command and argument. */
	CSR_WRITE_4(sc, MMC_CMD, cmd->c_opcode);
	CSR_WRITE_4(sc, MMC_ARGH, (cmd->c_arg >> 16) & 0xffff);
	CSR_WRITE_4(sc, MMC_ARGL, cmd->c_arg & 0xffff);

	/* Set response characteristics for this command. */
	if (!ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		cmdat = CMDAT_RESPONSE_FORMAT_NO;
	else if (ISSET(cmd->c_flags, SCF_RSP_136))
		cmdat = CMDAT_RESPONSE_FORMAT_R2;
	else if (!ISSET(cmd->c_flags, SCF_RSP_CRC))
		cmdat = CMDAT_RESPONSE_FORMAT_R3;
	else
		cmdat = CMDAT_RESPONSE_FORMAT_R1;

	if (ISSET(cmd->c_flags, SCF_RSP_BSY))
		cmdat |= CMDAT_BUSY;

	if (!ISSET(cmd->c_flags, SCF_CMD_READ))
		cmdat |= CMDAT_WRITE;

	/* Fragment the data into proper blocks. */
	if (cmd->c_datalen > 0) {
		int blklen = MIN(cmd->c_datalen, cmd->c_blklen);
		int numblk = cmd->c_datalen / blklen;

		if (cmd->c_datalen % blklen > 0) {
			/* XXX: Split this command. (1.7.4) */
			printf("%s: data not a multiple of %d bytes\n",
			    sc->sc_dev.dv_xname, blklen);
			cmd->c_error = EINVAL;
			splx(s);
			return;
		}

		CSR_WRITE_4(sc, MMC_BLKLEN, blklen);
		CSR_WRITE_4(sc, MMC_NUMBLK, numblk);

		/* Enable data interrupts. */
		CSR_CLR_4(sc, MMC_I_MASK,
		    MMC_I_RXFIFO_RD_REQ | MMC_I_TXFIFO_WR_REQ |
		    MMC_I_DAT_ERR);

		cmd->c_resid = cmd->c_datalen;
		cmd->c_buf = cmd->c_data;

		cmdat |= CMDAT_DATA_EN;
	} else {
		cmd->c_resid = 0;
		cmd->c_buf = NULL;
	}

	/*
	 * "After reset, the MMC card must be initialized by sending
	 * 80 clocks to it on the MMCLK signal." [15.4.3.2]
	 */
	if (!ISSET(sc->sc_flags, PMF_CARD_INITED)) {
		DPRINTF(1,("%s: first command\n", sc->sc_dev.dv_xname));
		cmdat |= CMDAT_INIT;
		SET(sc->sc_flags, PMF_CARD_INITED);
	}

	/* Begin the transfer and start the bus clock. */
	CSR_WRITE_4(sc, MMC_CMDAT, cmdat);
	pxammc_clock_start(sc);

	/* Wait for it to complete (in no more than 2 seconds). */
	CSR_CLR_4(sc, MMC_I_MASK, MMC_I_END_CMD_RES | MMC_I_RES_ERR);
	timo = 2;
	sc->sc_cmd = cmd;
	do { tsleep(sc, PWAIT, "mmcmd", hz); }
	while (sc->sc_cmd == cmd && timo-- > 0);

	/* If it completed in time, SCF_ITSDONE is already set. */
	if (sc->sc_cmd == cmd) {
		sc->sc_cmd = NULL;
		cmd->c_error = ETIMEDOUT;
		SET(cmd->c_flags, SCF_ITSDONE);
	}
	splx(s);
}

void
pxammc_clock_stop(struct pxammc_softc *sc)
{
	if (ISSET(CSR_READ_4(sc, MMC_STAT), STAT_CLK_EN)) {
		CSR_CLR_4(sc, MMC_I_MASK, MMC_I_CLK_IS_OFF);
		CSR_WRITE_4(sc, MMC_STRPCL, STRPCL_STOP);
		while (ISSET(CSR_READ_4(sc, MMC_STAT), STAT_CLK_EN))
			tsleep(sc, PWAIT, "mmclk", 0);
	}
}

void
pxammc_clock_start(struct pxammc_softc *sc)
{
	CSR_WRITE_4(sc, MMC_CLKRT, sc->sc_clkdiv);
	CSR_WRITE_4(sc, MMC_STRPCL, STRPCL_START);
}

int
pxammc_card_intr(void *arg)
{
	struct pxammc_softc *sc = arg;

	DPRINTF(1,("%s: card intr\n", sc->sc_dev.dv_xname));

	/* Scan for inserted or removed cards. */
	sdmmc_needs_discover(sc->sc_sdmmc);

	return 1;
}

int
pxammc_intr(void *arg)
{
	struct pxammc_softc *sc = arg;
	int status;
#ifdef DIAGNOSTIC
	int wstatus;
#endif

#define MMC_I_REG_STR	"\20\001DATADONE\002PRGDONE\003ENDCMDRES"	\
			"\004STOPCMD\005CLKISOFF\006RXFIFO\007TXFIFO"	\
			"\011DATERR\012RESERR\014SDIO"

#ifdef DIAGNOSTIC
	wstatus =
#endif
	status = CSR_READ_4(sc, MMC_I_REG) & ~CSR_READ_4(sc, MMC_I_MASK);
	DPRINTF(1,("%s: intr %b\n", sc->sc_dev.dv_xname, status,
	    MMC_I_REG_STR));

	/*
	 * Notify the process waiting in pxammc_clock_stop() when
	 * the clock has really stopped.
	 */
	if (ISSET(status, MMC_I_CLK_IS_OFF)) {
		DPRINTF(2,("%s: clock is now off\n", sc->sc_dev.dv_xname));
		wakeup(sc);
		CSR_SET_4(sc, MMC_I_MASK, MMC_I_CLK_IS_OFF);
		CLR(status, MMC_I_CLK_IS_OFF);
	}

	if (sc->sc_cmd == NULL)
		goto end;

	if (ISSET(status, MMC_I_RES_ERR)) {
		CSR_SET_4(sc, MMC_I_MASK, MMC_I_RES_ERR);
		CLR(status, MMC_I_RES_ERR | MMC_I_END_CMD_RES);
		sc->sc_cmd->c_error = ENOEXEC;
		pxammc_intr_done(sc);
		goto end;
	}

	if (ISSET(status, MMC_I_END_CMD_RES)) {
		pxammc_intr_cmd(sc);
		CSR_SET_4(sc, MMC_I_MASK, MMC_I_END_CMD_RES);
		CLR(status, MMC_I_END_CMD_RES);
		/* ignore programming done condition */
		if (ISSET(status, MMC_I_PRG_DONE)) {
			CSR_SET_4(sc, MMC_I_MASK, MMC_I_PRG_DONE);
			CLR(status, MMC_I_PRG_DONE);
		}
		if (sc->sc_cmd == NULL)
			goto end;
	}

	if (ISSET(status, MMC_I_DAT_ERR)) {
		sc->sc_cmd->c_error = EIO;
		pxammc_intr_done(sc);
		CSR_SET_4(sc, MMC_I_MASK, MMC_I_DAT_ERR);
		CLR(status, MMC_I_DAT_ERR);
		/* ignore transmission done condition */
		if (ISSET(status, MMC_I_DATA_TRAN_DONE)) {
			CSR_SET_4(sc, MMC_I_MASK, MMC_I_DATA_TRAN_DONE);
			CLR(status, MMC_I_DATA_TRAN_DONE);
		}
		goto end;
	}

	if (ISSET(status, MMC_I_DATA_TRAN_DONE)) {
		pxammc_intr_done(sc);
		CSR_SET_4(sc, MMC_I_MASK, MMC_I_DATA_TRAN_DONE);
		CLR(status, MMC_I_DATA_TRAN_DONE);
	}

	if (ISSET(status, MMC_I_TXFIFO_WR_REQ | MMC_I_RXFIFO_RD_REQ)) {
		pxammc_intr_data(sc);
		CLR(status, MMC_I_TXFIFO_WR_REQ | MMC_I_RXFIFO_RD_REQ);
	}

end:
	/* Avoid further unhandled interrupts. */
	if (status != 0) {
#ifdef DIAGNOSTIC
		printf("%s: unhandled interrupt %b out of %b\n",
		    sc->sc_dev.dv_xname, status, MMC_I_REG_STR,
		    wstatus, MMC_I_REG_STR);
#endif
		CSR_SET_4(sc, MMC_I_MASK, status);
	}

	return 1;
}

inline void
pxammc_intr_cmd(struct pxammc_softc *sc)
{
	struct sdmmc_command *cmd = sc->sc_cmd;
	u_int32_t status;
	int i;

#define MMC_STAT_STR	"\20\001READ_TIME_OUT\002TIMEOUT_RESPONSE"	\
			"\003CRC_WRITE_ERROR\004CRC_READ_ERROR"		\
			"\005SPI_READ_ERROR_TOKEN\006RES_CRC_ERR"	\
			"\007XMIT_FIFO_EMPTY\010RECV_FIFO_FULL"		\
			"\011CLK_EN\012FLASH_ERR\013SPI_WR_ERR"		\
			"\014DATA_TRAN_DONE\015PRG_DONE\016END_CMD_RES"	\
			"\017RD_STALLED\020SDIO_INT\021SDIO_SUSPEND_ACK"

#define STAT_ERR	(STAT_READ_TIME_OUT | STAT_TIMEOUT_RESPONSE |	\
			 STAT_CRC_WRITE_ERROR | STAT_CRC_READ_ERROR |	\
			 STAT_SPI_READ_ERROR_TOKEN | STAT_RES_CRC_ERR)

	if (ISSET(cmd->c_flags, SCF_RSP_136)) {
		for (i = 3; i >= 0; i--) {
			u_int32_t h = CSR_READ_4(sc, MMC_RES) & 0xffff;
			u_int32_t l = CSR_READ_4(sc, MMC_RES) & 0xffff;
			cmd->c_resp[i] = (h << 16) | l;
		}
		cmd->c_error = 0;
	} else if (ISSET(cmd->c_flags, SCF_RSP_PRESENT)) {
		/*
		 * Grrr... The processor manual is not clear about
		 * the layout of the response FIFO.  It just states
		 * that the FIFO is 16 bits wide, has a depth of 8,
		 * and that the CRC is not copied into the FIFO.
		 *
		 * A 16-bit word in the FIFO is filled from highest
		 * to lowest bit as the response comes in.  The two
		 * start bits and the 6 command index bits are thus
		 * stored in the upper 8 bits of the first 16-bit
		 * word that we read back from the FIFO.
		 *
		 * Since the sdmmc(4) framework expects the host
		 * controller to discard the first 8 bits of the
		 * response, what we must do is discard the upper
		 * byte of the first 16-bit word.
		 */
		u_int32_t h = CSR_READ_4(sc, MMC_RES) & 0xffff;
		u_int32_t m = CSR_READ_4(sc, MMC_RES) & 0xffff;
		u_int32_t l = CSR_READ_4(sc, MMC_RES) & 0xffff;
		cmd->c_resp[0] = h << 24 | m << 8 | l >> 8;
		for (i = 1; i < 4; i++)
			cmd->c_resp[i] = 0;
		cmd->c_error = 0;
	}

	status = CSR_READ_4(sc, MMC_STAT);

	if (!ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		status &= ~STAT_TIMEOUT_RESPONSE;

	/* XXX only for R6, not for R2 */
	if (!ISSET(cmd->c_flags, SCF_RSP_IDX))
		status &= ~STAT_RES_CRC_ERR;

	if (ISSET(status, STAT_TIMEOUT_RESPONSE))
		cmd->c_error = ETIMEDOUT;
	else if (ISSET(status, STAT_ERR))
		cmd->c_error = EIO;

	if (cmd->c_error || cmd->c_datalen < 1)
		pxammc_intr_done(sc);
}

inline void
pxammc_intr_data(struct pxammc_softc *sc)
{
	struct sdmmc_command *cmd = sc->sc_cmd;
	int n;

	n = MIN(32, cmd->c_resid);
	cmd->c_resid -= n;

	DPRINTF(2,("%s: cmd %p resid %d\n", sc->sc_dev.dv_xname,
	    cmd, cmd->c_resid));

	if (ISSET(cmd->c_flags, SCF_CMD_READ)) {
		while (n-- > 0)
			*cmd->c_buf++ = CSR_READ_1(sc, MMC_RXFIFO);

		if (cmd->c_resid > 0)
			CSR_CLR_4(sc, MMC_I_MASK, MMC_I_RXFIFO_RD_REQ);
		else {
			CSR_SET_4(sc, MMC_I_MASK, MMC_I_RXFIFO_RD_REQ);
			CSR_CLR_4(sc, MMC_I_MASK, MMC_I_DATA_TRAN_DONE);
		}
	} else {
		int short_xfer = (n != 0 && n != 32);

		while (n-- > 0)
			CSR_WRITE_1(sc, MMC_TXFIFO, *cmd->c_buf++);
		if (short_xfer)
			CSR_WRITE_4(sc, MMC_PRTBUF, 1);

		if (cmd->c_resid > 0)
			CSR_CLR_4(sc, MMC_I_MASK, MMC_I_TXFIFO_WR_REQ);
		else {
			CSR_SET_4(sc, MMC_I_MASK, MMC_I_TXFIFO_WR_REQ);
			CSR_CLR_4(sc, MMC_I_MASK, MMC_I_DATA_TRAN_DONE);
		}
	}
}

/*
 * Wake up the process sleeping in pxammc_exec_command().
 */
void
pxammc_intr_done(struct pxammc_softc *sc)
{
	DPRINTF(1,("%s: status %b\n", sc->sc_dev.dv_xname,
	    CSR_READ_4(sc, MMC_STAT), MMC_STAT_STR));

	CSR_SET_4(sc, MMC_I_MASK, MMC_I_TXFIFO_WR_REQ |
	    MMC_I_RXFIFO_RD_REQ | MMC_I_DATA_TRAN_DONE |
	    MMC_I_END_CMD_RES | MMC_I_RES_ERR | MMC_I_DAT_ERR);
	
	SET(sc->sc_cmd->c_flags, SCF_ITSDONE);
	sc->sc_cmd = NULL;
	wakeup(sc);
}
