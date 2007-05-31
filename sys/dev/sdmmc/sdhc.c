/*	$OpenBSD: sdhc.c,v 1.21 2007/05/31 23:37:21 uwe Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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
 * SD Host Controller driver based on the SD Host Controller Standard
 * Simplified Specification Version 1.00 (www.sdcard.com).
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#define SDHC_COMMAND_TIMEOUT	hz
#define SDHC_BUFFER_TIMEOUT	hz
#define SDHC_TRANSFER_TIMEOUT	hz

struct sdhc_host {
	struct sdhc_softc *sc;		/* host controller device */
	struct device *sdmmc;		/* generic SD/MMC device */
	bus_space_tag_t iot;		/* host register set tag */
	bus_space_handle_t ioh;		/* host register set handle */
	u_int clkbase;			/* base clock frequency in KHz */
	int maxblklen;			/* maximum block length */
	int flags;			/* flags for this host */
	u_int32_t ocr;			/* OCR value from capabilities */
	u_int8_t regs[14];		/* host controller state */
	u_int16_t intr_status;		/* soft interrupt status */
	u_int16_t intr_error_status;	/* soft error status */
};

#define HDEVNAME(hp)	((hp)->sc->sc_dev.dv_xname)

/* flag values */
#define SHF_USE_DMA		0x0001

#define HREAD1(hp, reg)							\
	(bus_space_read_1((hp)->iot, (hp)->ioh, (reg)))
#define HREAD2(hp, reg)							\
	(bus_space_read_2((hp)->iot, (hp)->ioh, (reg)))
#define HREAD4(hp, reg)							\
	(bus_space_read_4((hp)->iot, (hp)->ioh, (reg)))
#define HWRITE1(hp, reg, val)						\
	bus_space_write_1((hp)->iot, (hp)->ioh, (reg), (val))
#define HWRITE2(hp, reg, val)						\
	bus_space_write_2((hp)->iot, (hp)->ioh, (reg), (val))
#define HWRITE4(hp, reg, val)						\
	bus_space_write_4((hp)->iot, (hp)->ioh, (reg), (val))
#define HCLR1(hp, reg, bits)						\
	HWRITE1((hp), (reg), HREAD1((hp), (reg)) & ~(bits))
#define HCLR2(hp, reg, bits)						\
	HWRITE2((hp), (reg), HREAD2((hp), (reg)) & ~(bits))
#define HSET1(hp, reg, bits)						\
	HWRITE1((hp), (reg), HREAD1((hp), (reg)) | (bits))
#define HSET2(hp, reg, bits)						\
	HWRITE2((hp), (reg), HREAD2((hp), (reg)) | (bits))

int	sdhc_host_reset(sdmmc_chipset_handle_t);
u_int32_t sdhc_host_ocr(sdmmc_chipset_handle_t);
int	sdhc_host_maxblklen(sdmmc_chipset_handle_t);
int	sdhc_card_detect(sdmmc_chipset_handle_t);
int	sdhc_bus_power(sdmmc_chipset_handle_t, u_int32_t);
int	sdhc_bus_clock(sdmmc_chipset_handle_t, int);
void	sdhc_card_intr_mask(sdmmc_chipset_handle_t, int);
void	sdhc_card_intr_ack(sdmmc_chipset_handle_t);
void	sdhc_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);
int	sdhc_start_command(struct sdhc_host *, struct sdmmc_command *);
int	sdhc_wait_state(struct sdhc_host *, u_int32_t, u_int32_t);
int	sdhc_soft_reset(struct sdhc_host *, int);
int	sdhc_wait_intr(struct sdhc_host *, int, int);
void	sdhc_transfer_data(struct sdhc_host *, struct sdmmc_command *);
void	sdhc_read_data(struct sdhc_host *, u_char *, int);
void	sdhc_write_data(struct sdhc_host *, u_char *, int);

#ifdef SDHC_DEBUG
int sdhcdebug = 0;
#define DPRINTF(n,s)	do { if ((n) <= sdhcdebug) printf s; } while (0)
void	sdhc_dump_regs(struct sdhc_host *);
#else
#define DPRINTF(n,s)	do {} while(0)
#endif

struct sdmmc_chip_functions sdhc_functions = {
	/* host controller reset */
	sdhc_host_reset,
	/* host controller capabilities */
	sdhc_host_ocr,
	sdhc_host_maxblklen,
	/* card detection */
	sdhc_card_detect,
	/* bus power and clock frequency */
	sdhc_bus_power,
	sdhc_bus_clock,
	/* command execution */
	sdhc_exec_command,
	/* card interrupt */
	sdhc_card_intr_mask,
	sdhc_card_intr_ack
};

struct cfdriver sdhc_cd = {
	NULL, "sdhc", DV_DULL
};

/*
 * Called by attachment driver.  For each SD card slot there is one SD
 * host controller standard register set. (1.3)
 */
int
sdhc_host_found(struct sdhc_softc *sc, bus_space_tag_t iot,
    bus_space_handle_t ioh, bus_size_t iosize, int usedma)
{
	struct sdmmcbus_attach_args saa;
	struct sdhc_host *hp;
	u_int32_t caps;
	int error = 1;
#ifdef SDHC_DEBUG
	u_int16_t version;

	version = bus_space_read_2(iot, ioh, SDHC_HOST_CTL_VERSION);
	printf("%s: SD Host Specification/Vendor Version ",
	    sc->sc_dev.dv_xname);
	switch(SDHC_SPEC_VERSION(version)) {
	case 0x00:
		printf("1.0/%u\n", SDHC_VENDOR_VERSION(version));
		break;
	default:
		printf(">1.0/%u\n", SDHC_VENDOR_VERSION(version));
		break;
	}
#endif

	/* Allocate one more host structure. */
	sc->sc_nhosts++;
	MALLOC(hp, struct sdhc_host *, sizeof(struct sdhc_host),
	    M_DEVBUF, M_WAITOK);
	sc->sc_host[sc->sc_nhosts - 1] = hp;

	/* Fill in the new host structure. */
	bzero(hp, sizeof(struct sdhc_host));
	hp->sc = sc;
	hp->iot = iot;
	hp->ioh = ioh;

	/*
	 * Reset the host controller and enable interrupts.
	 */
	(void)sdhc_host_reset(hp);

	/* Determine host capabilities. */
	caps = HREAD4(hp, SDHC_CAPABILITIES);

	/* Use DMA if the host system and the controller support it. */
	if (usedma && ISSET(caps, SDHC_DMA_SUPPORT))
		SET(hp->flags, SHF_USE_DMA);

	/*
	 * Determine the base clock frequency. (2.2.24)
	 */
	if (SDHC_BASE_FREQ_KHZ(caps) != 0)
		hp->clkbase = SDHC_BASE_FREQ_KHZ(caps);
	if (hp->clkbase == 0) {
		/* The attachment driver must tell us. */
		printf("%s: base clock frequency unknown\n",
		    sc->sc_dev.dv_xname);
		goto err;
	} else if (hp->clkbase < 10000 || hp->clkbase > 63000) {
		/* SDHC 1.0 supports only 10-63 MHz. */
		printf("%s: base clock frequency out of range: %u MHz\n",
		    sc->sc_dev.dv_xname, hp->clkbase / 1000);
		goto err;
	}

	/*
	 * XXX Set the data timeout counter value according to
	 * capabilities. (2.2.15)
	 */

	/*
	 * Determine SD bus voltage levels supported by the controller.
	 */
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_1_8V))
		SET(hp->ocr, MMC_OCR_1_7V_1_8V | MMC_OCR_1_8V_1_9V);
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_3_0V))
		SET(hp->ocr, MMC_OCR_2_9V_3_0V | MMC_OCR_3_0V_3_1V);
	if (ISSET(caps, SDHC_VOLTAGE_SUPP_3_3V))
		SET(hp->ocr, MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V);

	/*
	 * Determine the maximum block length supported by the host
	 * controller. (2.2.24)
	 */
	switch((caps >> SDHC_MAX_BLK_LEN_SHIFT) & SDHC_MAX_BLK_LEN_MASK) {
	case SDHC_MAX_BLK_LEN_512:
		hp->maxblklen = 512;
		break;
	case SDHC_MAX_BLK_LEN_1024:
		hp->maxblklen = 1024;
		break;
	case SDHC_MAX_BLK_LEN_2048:
		hp->maxblklen = 2048;
		break;
	default:
		hp->maxblklen = 1;
		break;
	}

	/*
	 * Attach the generic SD/MMC bus driver.  (The bus driver must
	 * not invoke any chipset functions before it is attached.)
	 */
	bzero(&saa, sizeof(saa));
	saa.saa_busname = "sdmmc";
	saa.sct = &sdhc_functions;
	saa.sch = hp;

	hp->sdmmc = config_found(&sc->sc_dev, &saa, NULL);
	if (hp->sdmmc == NULL) {
		error = 0;
		goto err;
	}
	
	return 0;

err:
	FREE(hp, M_DEVBUF);
	sc->sc_host[sc->sc_nhosts - 1] = NULL;
	sc->sc_nhosts--;
	return (error);
}

/*
 * Power hook established by or called from attachment driver.
 */
void
sdhc_power(int why, void *arg)
{
	struct sdhc_softc *sc = arg;
	struct sdhc_host *hp;
	int n, i;

	switch(why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		/* XXX poll for command completion or suspend command
		 * in progress */

		/* Save the host controller state. */
		for (n = 0; n < sc->sc_nhosts; n++) {
			hp = sc->sc_host[n];
			for (i = 0; i < sizeof hp->regs; i++)
				hp->regs[i] = HREAD1(hp, i);
		}
		break;

	case PWR_RESUME:
		/* Restore the host controller state. */
		for (n = 0; n < sc->sc_nhosts; n++) {
			hp = sc->sc_host[n];
			(void)sdhc_host_reset(hp);
			for (i = 0; i < sizeof hp->regs; i++)
				HWRITE1(hp, i, hp->regs[i]);
		}
		break;
	}
}

/*
 * Shutdown hook established by or called from attachment driver.
 */
void
sdhc_shutdown(void *arg)
{
	struct sdhc_softc *sc = arg;
	struct sdhc_host *hp;
	int i;

	/* XXX chip locks up if we don't disable it before reboot. */
	for (i = 0; i < sc->sc_nhosts; i++) {
		hp = sc->sc_host[i];
		(void)sdhc_host_reset(hp);
	}
}

/*
 * Reset the host controller.  Called during initialization, when
 * cards are removed, upon resume, and during error recovery.
 */
int
sdhc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;
	u_int16_t imask;
	int error;
	int s;

	s = splsdmmc();

	/* Disable all interrupts. */
	HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, 0);

	/*
	 * Reset the entire host controller and wait up to 100ms for
	 * the controller to clear the reset bit.
	 */
	if ((error = sdhc_soft_reset(hp, SDHC_RESET_ALL)) != 0) {
		splx(s);
		return (error);
	}	

	/* Set data timeout counter value to max for now. */
	HWRITE1(hp, SDHC_TIMEOUT_CTL, SDHC_TIMEOUT_MAX);

	/* Enable interrupts. */
	imask = SDHC_CARD_REMOVAL | SDHC_CARD_INSERTION |
	    SDHC_BUFFER_READ_READY | SDHC_BUFFER_WRITE_READY |
	    SDHC_DMA_INTERRUPT | SDHC_BLOCK_GAP_EVENT |
	    SDHC_TRANSFER_COMPLETE | SDHC_COMMAND_COMPLETE;

	HWRITE2(hp, SDHC_NINTR_STATUS_EN, imask);
	HWRITE2(hp, SDHC_EINTR_STATUS_EN, SDHC_EINTR_STATUS_MASK);
	HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, imask);
	HWRITE2(hp, SDHC_EINTR_SIGNAL_EN, SDHC_EINTR_SIGNAL_MASK);

	splx(s);
	return 0;
}

u_int32_t
sdhc_host_ocr(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;
	return hp->ocr;
}

int
sdhc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;
	return hp->maxblklen;
}

/*
 * Return non-zero if the card is currently inserted.
 */
int
sdhc_card_detect(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;
	return ISSET(HREAD4(hp, SDHC_PRESENT_STATE), SDHC_CARD_INSERTED) ?
	    1 : 0;
}

/*
 * Set or change SD bus voltage and enable or disable SD bus power.
 * Return zero on success.
 */
int
sdhc_bus_power(sdmmc_chipset_handle_t sch, u_int32_t ocr)
{
	struct sdhc_host *hp = sch;
	u_int8_t vdd;
	int s;

	s = splsdmmc();

	/*
	 * Disable bus power before voltage change.
	 */
	HWRITE1(hp, SDHC_POWER_CTL, 0);

	/* If power is disabled, reset the host and return now. */
	if (ocr == 0) {
		splx(s);
		(void)sdhc_host_reset(hp);
		return 0;
	}

	/*
	 * Select the maximum voltage according to capabilities.
	 */
	ocr &= hp->ocr;
	if (ISSET(ocr, MMC_OCR_3_2V_3_3V|MMC_OCR_3_3V_3_4V))
		vdd = SDHC_VOLTAGE_3_3V;
	else if (ISSET(ocr, MMC_OCR_2_9V_3_0V|MMC_OCR_3_0V_3_1V))
		vdd = SDHC_VOLTAGE_3_0V;
	else if (ISSET(ocr, MMC_OCR_1_7V_1_8V|MMC_OCR_1_8V_1_9V))
		vdd = SDHC_VOLTAGE_1_8V;
	else {
		/* Unsupported voltage level requested. */
		splx(s);
		return EINVAL;
	}

	/*
	 * Enable bus power.  Wait at least 1 ms (or 74 clocks) plus
	 * voltage ramp until power rises.
	 */
	HWRITE1(hp, SDHC_POWER_CTL, (vdd << SDHC_VOLTAGE_SHIFT) |
	    SDHC_BUS_POWER);
	sdmmc_delay(10000);

	/*
	 * The host system may not power the bus due to battery low,
	 * etc.  In that case, the host controller should clear the
	 * bus power bit.
	 */
	if (!ISSET(HREAD1(hp, SDHC_POWER_CTL), SDHC_BUS_POWER)) {
		splx(s);
		return ENXIO;
	}

	splx(s);
	return 0;
}

/*
 * Return the smallest possible base clock frequency divisor value
 * for the CLOCK_CTL register to produce `freq' (KHz).
 */
static int
sdhc_clock_divisor(struct sdhc_host *hp, u_int freq)
{
	int div;

	for (div = 1; div <= 256; div *= 2)
		if ((hp->clkbase / div) <= freq)
			return (div / 2);
	/* No divisor found. */
	return -1;
}

/*
 * Set or change SDCLK frequency or disable the SD clock.
 * Return zero on success.
 */
int
sdhc_bus_clock(sdmmc_chipset_handle_t sch, int freq)
{
	struct sdhc_host *hp = sch;
	int s;
	int div;
	int timo;
	int error = 0;

	s = splsdmmc();

#ifdef DIAGNOSTIC
	/* Must not stop the clock if commands are in progress. */
	if (ISSET(HREAD4(hp, SDHC_PRESENT_STATE), SDHC_CMD_INHIBIT_MASK) &&
	    sdhc_card_detect(hp))
		printf("sdhc_sdclk_frequency_select: command in progress\n");
#endif

	/*
	 * Stop SD clock before changing the frequency.
	 */
	HWRITE2(hp, SDHC_CLOCK_CTL, 0);
	if (freq == SDMMC_SDCLK_OFF)
		goto ret;

	/*
	 * Set the minimum base clock frequency divisor.
	 */
	if ((div = sdhc_clock_divisor(hp, freq)) < 0) {
		/* Invalid base clock frequency or `freq' value. */
		error = EINVAL;
		goto ret;
	}
	HWRITE2(hp, SDHC_CLOCK_CTL, div << SDHC_SDCLK_DIV_SHIFT);

	/*
	 * Start internal clock.  Wait 10ms for stabilization.
	 */
	HSET2(hp, SDHC_CLOCK_CTL, SDHC_INTCLK_ENABLE);
	for (timo = 1000; timo > 0; timo--) {
		if (ISSET(HREAD2(hp, SDHC_CLOCK_CTL), SDHC_INTCLK_STABLE))
			break;
		sdmmc_delay(10);
	}
	if (timo == 0) {
		error = ETIMEDOUT;
		goto ret;
	}

	/*
	 * Enable SD clock.
	 */
	HSET2(hp, SDHC_CLOCK_CTL, SDHC_SDCLK_ENABLE);

ret:
	splx(s);
	return error;
}

void
sdhc_card_intr_mask(sdmmc_chipset_handle_t sch, int enable)
{
	struct sdhc_host *hp = sch;

	if (enable) {
		HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
		HSET2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_CARD_INTERRUPT);
	} else {
		HCLR2(hp, SDHC_NINTR_SIGNAL_EN, SDHC_CARD_INTERRUPT);
		HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
	}
}

void
sdhc_card_intr_ack(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;

	HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
}

int
sdhc_wait_state(struct sdhc_host *hp, u_int32_t mask, u_int32_t value)
{
	u_int32_t state;
	int timeout;

	for (timeout = 10; timeout > 0; timeout--) {
		if (((state = HREAD4(hp, SDHC_PRESENT_STATE)) & mask)
		    == value)
			return 0;
		sdmmc_delay(10000);
	}
	DPRINTF(0,("%s: timeout waiting for %x (state=%b)\n", HDEVNAME(hp),
	    value, state, SDHC_PRESENT_STATE_BITS));
	return ETIMEDOUT;
}

void
sdhc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct sdhc_host *hp = sch;
	int error;

	/*
	 * Start the MMC command, or mark `cmd' as failed and return.
	 */
	error = sdhc_start_command(hp, cmd);
	if (error != 0) {
		cmd->c_error = error;
		SET(cmd->c_flags, SCF_ITSDONE);
		return;
	}

	/*
	 * Wait until the command phase is done, or until the command
	 * is marked done for any other reason.
	 */
	if (!sdhc_wait_intr(hp, SDHC_COMMAND_COMPLETE,
	    SDHC_COMMAND_TIMEOUT)) {
		cmd->c_error = ETIMEDOUT;
		SET(cmd->c_flags, SCF_ITSDONE);
		return;
	}

	/*
	 * The host controller removes bits [0:7] from the response
	 * data (CRC) and we pass the data up unchanged to the bus
	 * driver (without padding).
	 */
	if (cmd->c_error == 0 && ISSET(cmd->c_flags, SCF_RSP_PRESENT)) {
		if (ISSET(cmd->c_flags, SCF_RSP_136)) {
			u_char *p = (u_char *)cmd->c_resp;
			int i;

			for (i = 0; i < 15; i++)
				*p++ = HREAD1(hp, SDHC_RESPONSE + i);
		} else
			cmd->c_resp[0] = HREAD4(hp, SDHC_RESPONSE);
	}

	/*
	 * If the command has data to transfer in any direction,
	 * execute the transfer now.
	 */
	if (cmd->c_error == 0 && cmd->c_data != NULL)
		sdhc_transfer_data(hp, cmd);

	/* Turn off the LED. */
	HCLR1(hp, SDHC_HOST_CTL, SDHC_LED_ON);

	DPRINTF(1,("%s: cmd %u done (flags=%#x error=%d)\n",
	    HDEVNAME(hp), cmd->c_opcode, cmd->c_flags, cmd->c_error));
	SET(cmd->c_flags, SCF_ITSDONE);
}

int
sdhc_start_command(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	u_int16_t blksize = 0;
	u_int16_t blkcount = 0;
	u_int16_t mode;
	u_int16_t command;
	int error;
	int s;
	
	DPRINTF(1,("%s: start cmd %u arg=%#x data=%#x dlen=%d flags=%#x "
	    "proc=\"%s\"\n", HDEVNAME(hp), cmd->c_opcode, cmd->c_arg,
	    cmd->c_data, cmd->c_datalen, cmd->c_flags, curproc ?
	    curproc->p_comm : ""));

	/*
	 * The maximum block length for commands should be the minimum
	 * of the host buffer size and the card buffer size. (1.7.2)
	 */

	/* Fragment the data into proper blocks. */
	if (cmd->c_datalen > 0) {
		blksize = MIN(cmd->c_datalen, cmd->c_blklen);
		blkcount = cmd->c_datalen / blksize;
		if (cmd->c_datalen % blksize > 0) {
			/* XXX: Split this command. (1.7.4) */
			printf("%s: data not a multiple of %d bytes\n",
			    HDEVNAME(hp), blksize);
			return EINVAL;
		}
	}

	/* Check limit imposed by 9-bit block count. (1.7.2) */
	if (blkcount > SDHC_BLOCK_COUNT_MAX) {
		printf("%s: too much data\n", HDEVNAME(hp));
		return EINVAL;
	}

	/* Prepare transfer mode register value. (2.2.5) */
	mode = 0;
	if (ISSET(cmd->c_flags, SCF_CMD_READ))
		mode |= SDHC_READ_MODE;
	if (blkcount > 0) {
		mode |= SDHC_BLOCK_COUNT_ENABLE;
		if (blkcount > 1) {
			mode |= SDHC_MULTI_BLOCK_MODE;
			/* XXX only for memory commands? */
			mode |= SDHC_AUTO_CMD12_ENABLE;
		}
	}
#ifdef notyet
	if (ISSET(hp->flags, SHF_USE_DMA))
		mode |= SDHC_DMA_ENABLE;
#endif

	/*
	 * Prepare command register value. (2.2.6)
	 */
	command = (cmd->c_opcode & SDHC_COMMAND_INDEX_MASK) <<
	    SDHC_COMMAND_INDEX_SHIFT;

	if (ISSET(cmd->c_flags, SCF_RSP_CRC))
		command |= SDHC_CRC_CHECK_ENABLE;
	if (ISSET(cmd->c_flags, SCF_RSP_IDX))
		command |= SDHC_INDEX_CHECK_ENABLE;
	if (cmd->c_data != NULL)
		command |= SDHC_DATA_PRESENT_SELECT;

	if (!ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		command |= SDHC_NO_RESPONSE;
	else if (ISSET(cmd->c_flags, SCF_RSP_136))
		command |= SDHC_RESP_LEN_136;
	else if (ISSET(cmd->c_flags, SCF_RSP_BSY))
		command |= SDHC_RESP_LEN_48_CHK_BUSY;
	else
		command |= SDHC_RESP_LEN_48;

	/* Wait until command and data inhibit bits are clear. (1.5) */
	if ((error = sdhc_wait_state(hp, SDHC_CMD_INHIBIT_MASK, 0)) != 0)
		return error;

	s = splsdmmc();

	/* Alert the user not to remove the card. */
	HSET1(hp, SDHC_HOST_CTL, SDHC_LED_ON);

	/* XXX: Set DMA start address if SHF_USE_DMA is set. */

	DPRINTF(1,("%s: cmd=%#x mode=%#x blksize=%d blkcount=%d\n",
	    HDEVNAME(hp), command, mode, blksize, blkcount));

	/*
	 * Start a CPU data transfer.  Writing to the high order byte
	 * of the SDHC_COMMAND register triggers the SD command. (1.5)
	 */
	HWRITE2(hp, SDHC_TRANSFER_MODE, mode);
	HWRITE2(hp, SDHC_BLOCK_SIZE, blksize);
	if (blkcount > 1)
		HWRITE2(hp, SDHC_BLOCK_COUNT, blkcount);
	HWRITE4(hp, SDHC_ARGUMENT, cmd->c_arg);
	HWRITE2(hp, SDHC_COMMAND, command);

	splx(s);
	return 0;
}

void
sdhc_transfer_data(struct sdhc_host *hp, struct sdmmc_command *cmd)
{
	u_char *datap = cmd->c_data;
	int i, datalen;
	int mask;
	int error;

	mask = ISSET(cmd->c_flags, SCF_CMD_READ) ?
	    SDHC_BUFFER_READ_ENABLE : SDHC_BUFFER_WRITE_ENABLE;
	error = 0;
	datalen = cmd->c_datalen;

	DPRINTF(1,("%s: resp=%#x datalen=%d\n", HDEVNAME(hp),
	    MMC_R1(cmd->c_resp), datalen));

#ifdef SDHC_DEBUG
	/* XXX I forgot why I wanted to know when this happens :-( */
	if ((cmd->c_opcode == 52 || cmd->c_opcode == 53) &&
	    ISSET(MMC_R1(cmd->c_resp), 0xcb00))
		printf("%s: CMD52/53 error response flags %#x\n",
		    HDEVNAME(hp), MMC_R1(cmd->c_resp) & 0xff00);
#endif

	while (datalen > 0) {
		if (!sdhc_wait_intr(hp, SDHC_BUFFER_READ_READY|
		    SDHC_BUFFER_WRITE_READY, SDHC_BUFFER_TIMEOUT)) {
			error = ETIMEDOUT;
			break;
		}

		if ((error = sdhc_wait_state(hp, mask, mask)) != 0)
			break;

		i = MIN(datalen, cmd->c_blklen);
		if (ISSET(cmd->c_flags, SCF_CMD_READ))
			sdhc_read_data(hp, datap, i);
		else
			sdhc_write_data(hp, datap, i);

		datap += i;
		datalen -= i;
	}

	if (error == 0 && !sdhc_wait_intr(hp, SDHC_TRANSFER_COMPLETE,
	    SDHC_TRANSFER_TIMEOUT))
		error = ETIMEDOUT;

	if (error != 0)
		cmd->c_error = error;
	SET(cmd->c_flags, SCF_ITSDONE);

	DPRINTF(1,("%s: data transfer done (error=%d)\n",
	    HDEVNAME(hp), cmd->c_error));
}

void
sdhc_read_data(struct sdhc_host *hp, u_char *datap, int datalen)
{
	while (datalen > 3) {
		*(u_int32_t *)datap = HREAD4(hp, SDHC_DATA);
		datap += 4;
		datalen -= 4;
	}
	if (datalen > 0) {
		u_int32_t rv = HREAD4(hp, SDHC_DATA);
		do {
			*datap++ = rv & 0xff;
			rv = rv >> 8;
		} while (--datalen > 0);
	}
}

void
sdhc_write_data(struct sdhc_host *hp, u_char *datap, int datalen)
{
	while (datalen > 3) {
		DPRINTF(3,("%08x\n", *(u_int32_t *)datap));
		HWRITE4(hp, SDHC_DATA, *((u_int32_t *)datap)++);
		datalen -= 4;
	}
	if (datalen > 0) {
		u_int32_t rv = *datap++;
		if (datalen > 1)
			rv |= *datap++ << 8;
		if (datalen > 2)
			rv |= *datap++ << 16;
		DPRINTF(3,("rv %08x\n", rv));
		HWRITE4(hp, SDHC_DATA, rv);
	}
}

/* Prepare for another command. */
int
sdhc_soft_reset(struct sdhc_host *hp, int mask)
{
	int timo;

	DPRINTF(1,("%s: software reset reg=%#x\n", HDEVNAME(hp), mask));

	HWRITE1(hp, SDHC_SOFTWARE_RESET, mask);
	for (timo = 10; timo > 0; timo--) {
		if (!ISSET(HREAD1(hp, SDHC_SOFTWARE_RESET), mask))
			break;
		sdmmc_delay(10000);
		HWRITE1(hp, SDHC_SOFTWARE_RESET, 0);
	}
	if (timo == 0) {
		DPRINTF(1,("%s: timeout reg=%#x\n", HDEVNAME(hp),
		    HREAD1(hp, SDHC_SOFTWARE_RESET)));
		HWRITE1(hp, SDHC_SOFTWARE_RESET, 0);
		return (ETIMEDOUT);
	}

	return (0);
}

int
sdhc_wait_intr(struct sdhc_host *hp, int mask, int timo)
{
	int status;
	int s;

	mask |= SDHC_ERROR_INTERRUPT;

	s = splsdmmc();
	status = hp->intr_status & mask;
	while (status == 0) {
		if (tsleep(&hp->intr_status, PWAIT, "hcintr", timo)
		    == EWOULDBLOCK) {
			status |= SDHC_ERROR_INTERRUPT;
			break;
		}
		status = hp->intr_status & mask;
	}
	hp->intr_status &= ~status;

	DPRINTF(2,("%s: intr status %#x error %#x\n", HDEVNAME(hp), status,
	    hp->intr_error_status));
	
	/* Command timeout has higher priority than command complete. */
	if (ISSET(status, SDHC_ERROR_INTERRUPT)) {
		hp->intr_error_status = 0;
		(void)sdhc_soft_reset(hp, SDHC_RESET_DAT|SDHC_RESET_CMD);
		status = 0;
	}

	splx(s);
	return status;
}

/*
 * Established by attachment driver at interrupt priority IPL_SDMMC.
 */
int
sdhc_intr(void *arg)
{
	struct sdhc_softc *sc = arg;
	int host;
	int done = 0;

	/* We got an interrupt, but we don't know from which slot. */
	for (host = 0; host < sc->sc_nhosts; host++) {
		struct sdhc_host *hp = sc->sc_host[host];
		u_int16_t status;

		if (hp == NULL)
			continue;

		/* Find out which interrupts are pending. */
		status = HREAD2(hp, SDHC_NINTR_STATUS);
		if (!ISSET(status, SDHC_NINTR_STATUS_MASK))
			continue; /* no interrupt for us */

		/* Acknowledge the interrupts we are about to handle. */
		HWRITE2(hp, SDHC_NINTR_STATUS, status);
		DPRINTF(2,("%s: interrupt status=%b\n", HDEVNAME(hp),
		    status, SDHC_NINTR_STATUS_BITS));

		/* Claim this interrupt. */
		done = 1;

		/*
		 * Service error interrupts.
		 */
		if (ISSET(status, SDHC_ERROR_INTERRUPT)) {
			u_int16_t error;

			/* Acknowledge error interrupts. */
			error = HREAD2(hp, SDHC_EINTR_STATUS);
			HWRITE2(hp, SDHC_EINTR_STATUS, error);
			DPRINTF(2,("%s: error interrupt, status=%b\n",
			    HDEVNAME(hp), error, SDHC_EINTR_STATUS_BITS));

			if (ISSET(error, SDHC_CMD_TIMEOUT_ERROR|
			    SDHC_DATA_TIMEOUT_ERROR)) {
				hp->intr_error_status |= error;
				hp->intr_status |= status;
				wakeup(&hp->intr_status);
			}
		}

		/*
		 * Wake up the sdmmc event thread to scan for cards.
		 */
		if (ISSET(status, SDHC_CARD_REMOVAL|SDHC_CARD_INSERTION))
			sdmmc_needs_discover(hp->sdmmc);

		/*
		 * Wake up the blocking process to service command
		 * related interrupt(s).
		 */
		if (ISSET(status, SDHC_BUFFER_READ_READY|
		    SDHC_BUFFER_WRITE_READY|SDHC_COMMAND_COMPLETE|
		    SDHC_TRANSFER_COMPLETE)) {
			hp->intr_status |= status;
			wakeup(&hp->intr_status);
		}

		/*
		 * Service SD card interrupts.
		 */
		if (ISSET(status, SDHC_CARD_INTERRUPT)) {
			DPRINTF(0,("%s: card interrupt\n", HDEVNAME(hp)));
			HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
			sdmmc_card_intr(hp->sdmmc);
		}
	}
	return done;
}

#ifdef SDHC_DEBUG
void
sdhc_dump_regs(struct sdhc_host *hp)
{
	printf("0x%02x PRESENT_STATE:    %b\n", SDHC_PRESENT_STATE,
	    HREAD4(hp, SDHC_PRESENT_STATE), SDHC_PRESENT_STATE_BITS);
	printf("0x%02x POWER_CTL:        %x\n", SDHC_POWER_CTL,
	    HREAD1(hp, SDHC_POWER_CTL));
	printf("0x%02x NINTR_STATUS:     %x\n", SDHC_NINTR_STATUS,
	    HREAD2(hp, SDHC_NINTR_STATUS));
	printf("0x%02x EINTR_STATUS:     %x\n", SDHC_EINTR_STATUS,
	    HREAD2(hp, SDHC_EINTR_STATUS));
	printf("0x%02x NINTR_STATUS_EN:  %x\n", SDHC_NINTR_STATUS_EN,
	    HREAD2(hp, SDHC_NINTR_STATUS_EN));
	printf("0x%02x EINTR_STATUS_EN:  %x\n", SDHC_EINTR_STATUS_EN,
	    HREAD2(hp, SDHC_EINTR_STATUS_EN));
	printf("0x%02x NINTR_SIGNAL_EN:  %x\n", SDHC_NINTR_SIGNAL_EN,
	    HREAD2(hp, SDHC_NINTR_SIGNAL_EN));
	printf("0x%02x EINTR_SIGNAL_EN:  %x\n", SDHC_EINTR_SIGNAL_EN,
	    HREAD2(hp, SDHC_EINTR_SIGNAL_EN));
	printf("0x%02x CAPABILITIES:     %x\n", SDHC_CAPABILITIES,
	    HREAD4(hp, SDHC_CAPABILITIES));
	printf("0x%02x MAX_CAPABILITIES: %x\n", SDHC_MAX_CAPABILITIES,
	    HREAD4(hp, SDHC_MAX_CAPABILITIES));
}
#endif
