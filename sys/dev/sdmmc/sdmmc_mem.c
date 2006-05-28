/*	$OpenBSD: sdmmc_mem.c,v 1.2 2006/05/28 18:45:23 uwe Exp $	*/

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

/* Routines for SD/MMC memory cards. */

#include <sys/param.h>
#include <sys/systm.h>

#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

int	sdmmc_mem_send_op_cond(struct sdmmc_softc *, u_int32_t, u_int32_t *);
int	sdmmc_mem_set_blocklen(struct sdmmc_softc *, struct sdmmc_card *);

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

/*
 * Initialize SD/MMC memory cards and memory in SDIO "combo" cards.
 */
int
sdmmc_mem_enable(struct sdmmc_softc *sc)
{
	u_int32_t host_ocr;
	u_int32_t card_ocr;

	/* Set host mode to SD "combo" card or SD memory-only. */
	SET(sc->sc_flags, SMF_SD_MODE|SMF_MEM_MODE);

	/* Reset memory (*must* do that before CMD55 or CMD1). */
	sdmmc_go_idle_state(sc);

	/*
	 * Read the SD/MMC memory OCR value by issuing CMD55 followed
	 * by ACMD41 to read the OCR value from memory-only SD cards.
	 * MMC cards will not respond to CMD55 or ACMD41 and this is
	 * how we distinguish them from SD cards.
	 */
 mmc_mode:
	if (sdmmc_mem_send_op_cond(sc, 0, &card_ocr) != 0) {
		DPRINTF(("flags %x\n", sc->sc_flags));
		if (ISSET(sc->sc_flags, SMF_SD_MODE) &&
		    !ISSET(sc->sc_flags, SMF_IO_MODE)) {
			/* Not a SD card, switch to MMC mode. */
			CLR(sc->sc_flags, SMF_SD_MODE);
			goto mmc_mode;
		}
		if (!ISSET(sc->sc_flags, SMF_SD_MODE)) {
			printf("%s: can't read memory OCR\n",
			    SDMMCDEVNAME(sc));
			return 1;
		} else {
			/* Not a "combo" card. */
			CLR(sc->sc_flags, SMF_MEM_MODE);
			return 0;
		}
	}

	/* Set the lowest voltage supported by the card and host. */
	host_ocr = sdmmc_chip_host_ocr(sc->sct, sc->sch);
	if (sdmmc_set_bus_power(sc, host_ocr, card_ocr) != 0) {
		printf("%s: can't supply voltage requested by card\n",
		    SDMMCDEVNAME(sc));
		return 1;
	}

	/* Tell the card(s) to enter the idle state (again). */
	sdmmc_go_idle_state(sc);

	/* Send the new OCR value until all cards are ready. */
	if (sdmmc_mem_send_op_cond(sc, host_ocr, NULL) != 0) {
		printf("%s: can't send memory OCR\n", SDMMCDEVNAME(sc));
		return 1;
	}
	return 0;
}

/*
 * Initialize a SD/MMC memory card.
 */
int
sdmmc_mem_init(struct sdmmc_softc *sc, struct sdmmc_card *cs)
{
	if (sdmmc_select_card(sc, cs) != 0 ||
	    sdmmc_mem_set_blocklen(sc, cs) != 0)
		return 1;
	return 0;
}

/*
 * Get or set the card's memory OCR value (SD or MMC).
 */
int
sdmmc_mem_send_op_cond(struct sdmmc_softc *sc, u_int32_t ocr,
    u_int32_t *ocrp)
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
		cmd.c_arg = ocr;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R3;

		if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
			cmd.c_opcode = SD_APP_OP_COND;
			error = sdmmc_app_command(sc, &cmd);
		} else {
			cmd.c_opcode = MMC_SEND_OP_COND;
			error = sdmmc_mmc_command(sc, &cmd);
		}
		if (error != 0)
			break;
		if (ISSET(MMC_R3(cmd.c_resp), MMC_OCR_MEM_READY) ||
		    ocr == 0)
			break;
		error = ETIMEDOUT;
		sdmmc_delay(10000);
	}
	if (error == 0 && ocrp != NULL)
		*ocrp = MMC_R3(cmd.c_resp);
	return error;
}

/*
 * Set the read block length appropriately for this card, according to
 * the card CSD register value.
 */
int
sdmmc_mem_set_blocklen(struct sdmmc_softc *sc, struct sdmmc_card *cs)
{
	struct sdmmc_command cmd;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_SET_BLOCKLEN;
	cmd.c_arg = cs->csd.sector_size;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	DPRINTF(("%s: read_bl_len=%d sector_size=%d\n", SDMMCDEVNAME(sc),
	    1 << cs->csd.read_bl_len, cs->csd.sector_size));

	return sdmmc_mmc_command(sc, &cmd);
}

int
sdmmc_mem_read_block(struct sdmmc_softc *sc, struct sdmmc_card *cs,
    int blkno, u_char *data, size_t datalen)
{
	struct sdmmc_command cmd;
	int error;

	if ((error = sdmmc_select_card(sc, cs)) != 0)
		return error;

	bzero(&cmd, sizeof cmd);
	cmd.c_data = data;
	cmd.c_datalen = datalen;
	cmd.c_blklen = cs->csd.sector_size;
	cmd.c_opcode = (datalen / cmd.c_blklen) > 1 ?
	    MMC_READ_BLOCK_MULTIPLE : MMC_READ_BLOCK_SINGLE;
	cmd.c_arg = blkno << 9;
	cmd.c_flags = SCF_CMD_ADTC | SCF_CMD_READ | SCF_RSP_R1;

	error = sdmmc_mmc_command(sc, &cmd);
	if (error != 0)
		return error;

	do {
		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = MMC_SEND_STATUS;
		cmd.c_arg = MMC_ARG_RCA(cs->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
		error = sdmmc_mmc_command(sc, &cmd);
		if (error != 0)
			break;
		/* XXX time out */
	} while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

	return error;
}

int
sdmmc_mem_write_block(struct sdmmc_softc *sc, struct sdmmc_card *cs,
    int blkno, u_char *data, size_t datalen)
{
	struct sdmmc_command cmd;
	int error;

	if ((error = sdmmc_select_card(sc, cs)) != 0)
		return error;

	bzero(&cmd, sizeof cmd);
	cmd.c_data = data;
	cmd.c_datalen = datalen;
	cmd.c_blklen = cs->csd.sector_size;
	cmd.c_opcode = (datalen / cmd.c_blklen) > 1 ?
	    MMC_WRITE_BLOCK_MULTIPLE : MMC_WRITE_BLOCK_SINGLE;
	cmd.c_arg = blkno << 9;
	cmd.c_flags = SCF_CMD_ADTC | SCF_RSP_R1;

	error = sdmmc_mmc_command(sc, &cmd);
	if (error != 0)
		return error;

	do {
		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = MMC_SEND_STATUS;
		cmd.c_arg = MMC_ARG_RCA(cs->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
		error = sdmmc_mmc_command(sc, &cmd);
		if (error != 0)
			break;
		/* XXX time out */
	} while (!ISSET(MMC_R1(cmd.c_resp), MMC_R1_READY_FOR_DATA));

	return error;
}
