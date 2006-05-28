/*	$OpenBSD: sdhc.c,v 1.2 2006/05/28 18:45:23 uwe Exp $	*/

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
#include <sys/timeout.h>

#include <dev/sdmmc/sdhcreg.h>
#include <dev/sdmmc/sdhcvar.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#define SDHC_COMMAND_TIMEOUT	(hz*2)	/* 2 seconds */
#define SDHC_DATA_TIMEOUT	(hz*2)	/* 2 seconds */

struct sdhc_host {
	struct sdhc_softc *sc;		/* host controller device */
	struct device *sdmmc;		/* generic SD/MMC device */
	bus_space_tag_t iot;		/* host register set tag */
	bus_space_handle_t ioh;		/* host register set handle */
	u_int clkbase;			/* base clock frequency in KHz */
	int maxblklen;			/* maximum block length */
	int flags;			/* flags for this host */
	u_int32_t ocr;			/* OCR value from capabilities */
	struct proc *event_thread;	/* event processing thread */
	struct sdmmc_command *cmd;	/* current command or NULL */
	struct timeout cmd_to;		/* command timeout */
};

#define HDEVNAME(hp)	((hp)->sdmmc->dv_xname)

/* flag values */
#define SHF_USE_DMA		0x0001
#define SHF_CARD_PRESENT	0x0002
#define SHF_CARD_ATTACHED	0x0004

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

void	sdhc_create_event_thread(void *);
void	sdhc_event_thread(void *);
void	sdhc_event_process(struct sdhc_host *);

int	sdhc_host_reset(sdmmc_chipset_handle_t);
u_int32_t sdhc_host_ocr(sdmmc_chipset_handle_t);
int	sdhc_host_maxblklen(sdmmc_chipset_handle_t);
int	sdhc_card_detect(sdmmc_chipset_handle_t);
int	sdhc_bus_power(sdmmc_chipset_handle_t, u_int32_t);
int	sdhc_bus_clock(sdmmc_chipset_handle_t, int);
int	sdhc_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);
int	sdhc_wait_state(struct sdhc_host *, u_int32_t, u_int32_t);
int	sdhc_start_command(struct sdhc_host *, struct sdmmc_command *);
int	sdhc_wait_command(struct sdhc_host *, int);
int	sdhc_finish_command(struct sdhc_host *);
void	sdhc_transfer_data(struct sdhc_host *);
void	sdhc_read_data(struct sdhc_host *, u_char *, int);
void	sdhc_write_data(struct sdhc_host *, u_char *, int);
void	sdhc_command_timeout(void *);

#ifdef SDHC_DEBUG
void	sdhc_dump_regs(struct sdhc_host *);
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
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
	sdhc_exec_command
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
	MALLOC(hp, struct sdhc_host *, sizeof(struct sdhc_host) *
	    sc->sc_nhosts, M_DEVBUF, M_WAITOK);
	if (sc->sc_host != NULL) {
		bcopy(sc->sc_host, hp, sizeof(struct sdhc_host) *
		    (sc->sc_nhosts-1));
		FREE(sc->sc_host, M_DEVBUF);
	}
	sc->sc_host = hp;

	/* Fill in the new host structure. */
	hp = &sc->sc_host[sc->sc_nhosts-1];
	bzero(hp, sizeof(struct sdhc_host));
	hp->sc = sc;
	hp->iot = iot;
	hp->ioh = ioh;
	timeout_set(&hp->cmd_to, sdhc_command_timeout, hp);

	/*
	 * Reset the host controller and enable interrupts.
	 */
	if (sdhc_host_reset(hp) != 0) {
		printf("%s: host reset failed\n", sc->sc_dev.dv_xname);
		goto err;
	}

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
		/* SDHC 1.0 supports only 10-63 Mhz. */
		printf("%s: base clock frequency out of range: %u MHz\n",
		    sc->sc_dev.dv_xname, hp->clkbase);
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
		printf("%s: can't attach bus\n", sc->sc_dev.dv_xname);
		goto err;
	}

	/*
	 * Create the event thread that will attach and detach cards
	 * and perform other lengthy operations.
	 */
#ifdef DO_CONFIG_PENDING
	config_pending_incr();
#endif
	kthread_create_deferred(sdhc_create_event_thread, hp);
	
	return 0;

err:
	/* XXX: Leaking one sdhc_host structure here. */
	sc->sc_nhosts--;
	return 1;
}

void
sdhc_create_event_thread(void *arg)
{
	struct sdhc_host *hp = arg;

	/* If there's a card, attach it. */
	sdhc_event_process(hp);

	if (kthread_create(sdhc_event_thread, hp, &hp->event_thread,
	    HDEVNAME(hp)) != 0)
		printf("%s: can't create event thread\n", HDEVNAME(hp));

#ifdef DO_CONFIG_PENDING
	config_pending_decr();
#endif
}

void
sdhc_event_thread(void *arg)
{
	struct sdhc_host *hp = arg;

	for (;;) {
		//DPRINTF(("%s: tsleep sdhcev\n", HDEVNAME(hp)));
		(void)tsleep((caddr_t)hp, PWAIT, "sdhcev", 0);
		sdhc_event_process(hp);
	}
}

void
sdhc_event_process(struct sdhc_host *hp)
{
	//DPRINTF(("%s: event process\n", HDEVNAME(hp)));

	/* If there's a card, attach it, if it went away, detach it. */
	if (sdhc_card_detect(hp)) {
		if (!ISSET(hp->flags, SHF_CARD_PRESENT)) {
			SET(hp->flags, SHF_CARD_PRESENT);
			if (sdmmc_card_attach(hp->sdmmc) == 0)
				SET(hp->flags, SHF_CARD_ATTACHED);
		}
	} else {
		/* XXX If a command was in progress, abort it. */
		int s = splsdmmc();
		if (hp->cmd != NULL) {
			timeout_del(&hp->cmd_to);
			printf("%s: interrupted command %u\n",
			    HDEVNAME(hp), hp->cmd->c_opcode);
			hp->cmd = NULL;
		}
		splx(s);

		if (ISSET(hp->flags, SHF_CARD_PRESENT)) {
			CLR(hp->flags, SHF_CARD_PRESENT);
			if (ISSET(hp->flags, SHF_CARD_ATTACHED)) {
				sdmmc_card_detach(hp->sdmmc, DETACH_FORCE);
				CLR(hp->flags, SHF_CARD_ATTACHED);
			}
		}
	}
}

/*
 * Power hook established by or called from attachment driver.
 */
void
sdhc_power(int why, void *arg)
{
	/* struct sdhc_softc *sc = arg; */

	switch(why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		/* XXX suspend or detach cards */
		break;
	case PWR_RESUME:
		/* XXX resume or reattach cards */
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
		hp = &sc->sc_host[i];
		sdhc_host_reset(hp);
	}
}

/*
 * Reset the host controller.  Called during initialization, when
 * cards are removed and during error recovery.
 */
int
sdhc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct sdhc_host *hp = sch;
	u_int16_t imask;
	int error = 0;
	int timo;
	int s;

	s = splsdmmc();

	/* Disable all interrupts. */
	HWRITE2(hp, SDHC_NINTR_SIGNAL_EN, 0);

	/*
	 * Reset the entire host controller and wait up to 100ms.
	 */
	HWRITE1(hp, SDHC_SOFTWARE_RESET, SDHC_RESET_MASK);
	for (timo = 10; timo > 0; timo--) {
		if (!ISSET(HREAD1(hp, SDHC_SOFTWARE_RESET), SDHC_RESET_MASK))
			break;
		sdmmc_delay(10000);
	}
	if (timo == 0) {
		HWRITE1(hp, SDHC_SOFTWARE_RESET, 0);
		error = ETIMEDOUT;
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
	return error;
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
		if (sdhc_host_reset(hp) != 0)
			printf("%s: host reset failed\n", HDEVNAME(hp));
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

/*
 * Send a command and data to the card and return the command response
 * and data from the card.
 *
 * If no callback function is specified, execute the command
 * synchronously; otherwise, return immediately and call the function
 * from the event thread after the command has completed.
 */
int
sdhc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct sdhc_host *hp = sch;
	int error;

	if (hp->cmd != NULL)
		return EBUSY;

	error = sdhc_start_command(hp, cmd);
	if (error != 0)
		goto err;
	if (cmd->c_done != NULL)
		/* Execute this command asynchronously. */
		return error;

	error = sdhc_wait_command(hp, SCF_DONE|SCF_CMD_DONE);
	if (error != 0)
		goto err;
	return sdhc_finish_command(hp);
 err:
	cmd->c_error = error;
	SET(cmd->c_flags, SCF_DONE);
	return sdhc_finish_command(hp);
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
	DPRINTF(("%s: timeout waiting for %x (state=%b)\n", HDEVNAME(hp),
	    value, state, SDHC_PRESENT_STATE_BITS));
	return ETIMEDOUT;
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
	
	DPRINTF(("%s: start cmd %u\n", HDEVNAME(hp), cmd->c_opcode));
	hp->cmd = cmd;

	/* If the card went away, finish the command immediately. */
	if (!ISSET(hp->flags, SHF_CARD_PRESENT))
		return ETIMEDOUT;

	/*
	 * The maximum block length for commands should be the minimum
	 * of the host buffer size and the card buffer size. (1.7.2)
	 */

	/* Fragment the data into proper blocks. */
	if (cmd->c_datalen > 0) {
		blksize = cmd->c_blklen;
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

	/*
	 * Start a CPU data transfer.  Writing to the high order byte
	 * of the SDHC_COMMAND register triggers the SD command. (1.5)
	 */
	HWRITE2(hp, SDHC_BLOCK_SIZE, blksize);
	HWRITE2(hp, SDHC_BLOCK_COUNT, blkcount);
	HWRITE4(hp, SDHC_ARGUMENT, cmd->c_arg);
	HWRITE2(hp, SDHC_TRANSFER_MODE, mode);
	HWRITE2(hp, SDHC_COMMAND, command);

	/*
	 * Start a software timeout.  In the unlikely event that the
	 * controller's own timeout detection mechanism fails we will
	 * abort the transfer in software.
	 */
	timeout_add(&hp->cmd_to, SDHC_COMMAND_TIMEOUT);

	splx(s);
	return 0;
}

int
sdhc_wait_command(struct sdhc_host *hp, int flags)
{
	int s;

	for (;;) {
		/* Return if the command was aborted. */
		if (hp->cmd == NULL)
			return EIO;

		s = splsdmmc();

		/* Return if the command has reached the awaited state. */
		if (ISSET(hp->cmd->c_flags, flags)) {
			splx(s);
			return 0;
		}

		DPRINTF(("%s: tsleep sdhccmd (flags=%#x)\n",
		    HDEVNAME(hp), flags));
		(void)tsleep((caddr_t)hp, PWAIT, "sdhccmd", 0);

		/* Process card events. */
		sdhc_event_process(hp);

		splx(s);
	}
}

int
sdhc_finish_command(struct sdhc_host *hp)
{
	struct sdmmc_command *cmd = hp->cmd;
	int error;

	if (cmd == NULL) {
		DPRINTF(("%s: finish NULL cmd\n", HDEVNAME(hp)));
		return 0;
	}

	/* Cancel command timeout. */
	timeout_del(&hp->cmd_to);

	DPRINTF(("%s: finish cmd %u (flags=%#x error=%d)\n",
	    HDEVNAME(hp), cmd->c_opcode, cmd->c_flags, cmd->c_error));

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

	if (cmd->c_error == 0 && cmd->c_data != NULL) {
		timeout_add(&hp->cmd_to, SDHC_DATA_TIMEOUT);
		sdhc_transfer_data(hp);
	}

	/* Turn off the LED. */
	HCLR1(hp, SDHC_HOST_CTL, SDHC_LED_ON);

	error = cmd->c_error;
	hp->cmd = NULL;

	SET(cmd->c_flags, SCF_DONE);
	DPRINTF(("%s: cmd %u done (flags=%#x error=%d)\n",
	    HDEVNAME(hp), cmd->c_opcode, cmd->c_flags, error));
	if (cmd->c_done != NULL)
		cmd->c_done(hp->sdmmc, cmd);
	return error;
}

void
sdhc_transfer_data(struct sdhc_host *hp)
{
	struct sdmmc_command *cmd = hp->cmd;
	u_char *datap = cmd->c_data;
	int i, datalen;
	int mask;
	int error;

	mask = ISSET(cmd->c_flags, SCF_CMD_READ) ?
	    SDHC_BUFFER_READ_ENABLE : SDHC_BUFFER_WRITE_ENABLE;
	error = 0;
	datalen = cmd->c_datalen;

	DPRINTF(("%s: resp=%#x ", HDEVNAME(hp), MMC_R1(cmd->c_resp)));
	DPRINTF(("datalen %u\n", datalen));
	while (datalen > 0) {
		error = sdhc_wait_command(hp, SCF_DONE|SCF_BUF_READY);
		if (error != 0)
			break;

		error = sdhc_wait_state(hp, mask, mask);
		if (error != 0)
			break;

		i = MIN(datalen, cmd->c_blklen);
		if (ISSET(cmd->c_flags, SCF_CMD_READ))
			sdhc_read_data(hp, datap, i);
		else
			sdhc_write_data(hp, datap, i);

		datap += i;
		datalen -= i;
		CLR(cmd->c_flags, SCF_BUF_READY);
	}

	if (error == 0)
		error = sdhc_wait_command(hp, SCF_DONE|SCF_XFR_DONE);

	timeout_del(&hp->cmd_to);

	if (cmd->c_error == 0) {
		cmd->c_error = error;
		SET(cmd->c_flags, SCF_DONE);
	}

	DPRINTF(("%s: data transfer done (error=%d)\n",
	    HDEVNAME(hp), cmd->c_error));
}

void
sdhc_read_data(struct sdhc_host *hp, u_char *datap, int datalen)
{
	while (datalen > 0) {
		if (datalen > 3) {
			*((u_int32_t *)datap)++ = HREAD4(hp, SDHC_DATA);
			datalen -= 4;
		} else if (datalen > 1) {
			*((u_int16_t *)datap)++ = HREAD2(hp, SDHC_DATA);
			datalen -= 2;
		} else {
			*datap++ = HREAD1(hp, SDHC_DATA);
			datalen -= 1;
		}
	}
}

void
sdhc_write_data(struct sdhc_host *hp, u_char *datap, int datalen)
{
	while (datalen > 0) {
		if (datalen > 3) {
			HWRITE4(hp, SDHC_DATA, *(u_int32_t *)datap);
			datap += 4;
			datalen -= 4;
		} else if (datalen > 1) {
			HWRITE2(hp, SDHC_DATA, *(u_int16_t *)datap);
			datap += 2;
			datalen -= 2;
		} else {
			HWRITE1(hp, SDHC_DATA, *datap);
			datap++;
			datalen -= 1;
		}
	}
}

void
sdhc_command_timeout(void *arg)
{
	struct sdhc_host *hp = arg;
	struct sdmmc_command *cmd = hp->cmd;
	int s;

	if (cmd == NULL)
		return;

	s = splsdmmc();
	if (!ISSET(cmd->c_flags, SCF_DONE)) {
		DPRINTF(("%s: timeout cmd %u, resetting...\n",
		    HDEVNAME(hp), cmd->c_opcode));
		cmd->c_error = ETIMEDOUT;
		SET(cmd->c_flags, SCF_DONE);
		HWRITE1(hp, SDHC_SOFTWARE_RESET, SDHC_RESET_DAT|
		    SDHC_RESET_CMD);
		timeout_add(&hp->cmd_to, hz/2);
	} else {
		DPRINTF(("%s: timeout cmd %u, resetting...done\n",
		    HDEVNAME(hp), cmd->c_opcode));
		wakeup(hp);
	}
	splx(s);
}

/*
 * Established by attachment driver at interrupt priority IPL_SDMMC.
 */
int
sdhc_intr(void *arg)
{
	struct sdhc_softc *sc = arg;
	int done = 0;
	int host;

	/* We got an interrupt, but we don't know from which slot. */
	for (host = 0; host < sc->sc_nhosts; host++) {
		struct sdhc_host *hp = &sc->sc_host[host];
		u_int16_t status;

		if (hp == NULL)
			continue;

		status = HREAD2(hp, SDHC_NINTR_STATUS);
		if (!ISSET(status, SDHC_NINTR_STATUS_MASK))
			continue;

		/* Clear interrupts we are about to handle. */
		HWRITE2(hp, SDHC_NINTR_STATUS, status);
#ifdef SDHC_DEBUG
		printf("%s: interrupt status=%b\n", HDEVNAME(hp),
		    status, SDHC_NINTR_STATUS_BITS);
#endif

		/*
		 * Wake up the event thread to service the interrupt(s).
		 */
		if (ISSET(status, SDHC_BUFFER_READ_READY|
		    SDHC_BUFFER_WRITE_READY)) {
			if (hp->cmd != NULL &&
			    !ISSET(hp->cmd->c_flags, SCF_DONE)) {
				SET(hp->cmd->c_flags, SCF_BUF_READY);
				wakeup(hp);
			}
			done++;
		}
		if (ISSET(status, SDHC_COMMAND_COMPLETE)) {
			if (hp->cmd != NULL &&
			    !ISSET(hp->cmd->c_flags, SCF_DONE)) {
				SET(hp->cmd->c_flags, SCF_CMD_DONE);
				wakeup(hp);
			}
			done++;
		}
		if (ISSET(status, SDHC_TRANSFER_COMPLETE)) {
			if (hp->cmd != NULL &&
			    !ISSET(hp->cmd->c_flags, SCF_DONE)) {
				SET(hp->cmd->c_flags, SCF_XFR_DONE);
				wakeup(hp);
			}
			done++;
		}
		if (ISSET(status, SDHC_CARD_REMOVAL|SDHC_CARD_INSERTION)) {
			wakeup(hp);
			done++;
		}

		if (ISSET(status, SDHC_ERROR_INTERRUPT)) {
			u_int16_t error;

			error = HREAD2(hp, SDHC_EINTR_STATUS);
			HWRITE2(hp, SDHC_EINTR_STATUS, error);

			DPRINTF(("%s: error interrupt, status=%b\n",
			    HDEVNAME(hp), error, SDHC_EINTR_STATUS_BITS));

			if (ISSET(error, SDHC_CMD_TIMEOUT_ERROR|
			    SDHC_DATA_TIMEOUT_ERROR) && hp->cmd != NULL &&
			    !ISSET(hp->cmd->c_flags, SCF_DONE)) {
				hp->cmd->c_error = ETIMEDOUT;
				SET(hp->cmd->c_flags, SCF_DONE);
				/* XXX can this reset be avoided? */
				HWRITE1(hp, SDHC_SOFTWARE_RESET,
				    SDHC_RESET_DAT|SDHC_RESET_CMD);
				timeout_add(&hp->cmd_to, hz/2);
			}
			done++;
		}

		if (ISSET(status, SDHC_CARD_INTERRUPT)) {
			HCLR2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
			/* XXX service card interrupt */
			printf("%s: card interrupt\n", HDEVNAME(hp));
			HSET2(hp, SDHC_NINTR_STATUS_EN, SDHC_CARD_INTERRUPT);
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
