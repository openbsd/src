/*	$OpenBSD: sdmmc_io.c,v 1.1 2006/05/28 17:21:14 uwe Exp $	*/

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

/* Routines for SD I/O cards. */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#undef SDMMC_DEBUG

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

/*
 * Initialize SD I/O card functions (before memory cards).  The host
 * system and controller must support card interrupts in order to use
 * I/O functions.
 */
int
sdmmc_io_enable(struct sdmmc_softc *sc)
{
	u_int32_t host_ocr;
	u_int32_t card_ocr;

	/* Set host mode to SD "combo" card. */
	SET(sc->sc_flags, SMF_SD_MODE|SMF_IO_MODE|SMF_MEM_MODE);

	/* Reset I/O functions (*must* do that before CMD5). */
	sdmmc_io_reset(sc);

	/*
	 * Read the I/O OCR value, determine the number of I/O
	 * functions and whether memory is also present (a "combo
	 * card") by issuing CMD5.  SD memory-only and MMC cards
	 * do not respond to CMD5.
	 */
	if (sdmmc_io_send_op_cond(sc, 0, &card_ocr) != 0) {
		/* No SDIO card; switch to SD memory-only mode. */
		CLR(sc->sc_flags, SMF_IO_MODE);
		return 0;
	}

	if (SD_IO_OCR_NF(card_ocr) == 0) {
		/* No I/O functions. */
		DPRINTF(("%s: no I/O functions\n", SDMMCDEVNAME(sc)));
		return 0;
	}

	/* Set the lowest voltage supported by the card and host. */
	host_ocr = sdmmc_chip_host_ocr(sc->sct, sc->sch);
	if (sdmmc_set_bus_power(sc, host_ocr, card_ocr) != 0) {
		printf("%s: can't supply voltage requested by card\n",
		    SDMMCDEVNAME(sc));
		return 1;
	}

	/* Reset I/O functions (again). */
	sdmmc_io_reset(sc);

	/* Send the new OCR value until all cards are ready. */
	if (sdmmc_io_send_op_cond(sc, host_ocr, NULL) != 0) {
		printf("%s: can't send I/O OCR\n", SDMMCDEVNAME(sc));
		return 1;
	}
	return 0;
}

/*
 * Send the "I/O RESET" command.
 */
void
sdmmc_io_reset(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = SD_IO_RESET;
	cmd.c_flags = SCF_CMD_BC | SCF_RSP_R0;

	(void)sdmmc_mmc_command(sc, &cmd);
}

/*
 * Get or set the card's I/O OCR value (SDIO).
 */
int
sdmmc_io_send_op_cond(struct sdmmc_softc *sc, u_int32_t ocr, u_int32_t *ocrp)
{
	struct sdmmc_command cmd;
	int error;
	int i;

	/*
	 * If we change the OCR value, retry the command until the OCR
	 * we receive in response has the "CARD BUSY" bit set, meaning
	 * that all cards are ready for card identification.
	 */
	for (i = 0; i < 100; i++) {
		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = SD_IO_SEND_OP_COND;
		cmd.c_arg = ocr;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R4;

		error = sdmmc_mmc_command(sc, &cmd);
		if (error != 0)
			break;
		if (ISSET(MMC_R4(cmd.c_resp), SD_IO_OCR_MEM_READY) ||
		    ocr == 0)
			break;
		error = ETIMEDOUT;
		sdmmc_delay(10000);
	}
	if (error == 0)
		printf("ocr: %x\n", MMC_R4(cmd.c_resp));
	if (error == 0 && ocrp != NULL)
		*ocrp = MMC_R4(cmd.c_resp);
	return error;
}
