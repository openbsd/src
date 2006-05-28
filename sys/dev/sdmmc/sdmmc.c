/*	$OpenBSD: sdmmc.c,v 1.2 2006/05/28 18:45:23 uwe Exp $	*/

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
 * Host controller independent SD/MMC bus driver based on information
 * from SanDisk SD Card Product Manual Revision 2.2 (SanDisk), SDIO
 * Simple Specification Version 1.0 (SDIO) and the Linux "mmc" driver.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/sdmmc/sdmmc_scsi.h>
#include <dev/sdmmc/sdmmcchip.h>
#ifdef notyet
#include <dev/sdmmc/sdmmcdevs.h>
#endif
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

int	sdmmc_match(struct device *, void *, void *);
void	sdmmc_attach(struct device *, struct device *, void *);

int	sdmmc_set_relative_addr(struct sdmmc_softc *, struct sdmmc_card *);
int	sdmmc_set_bus_width(struct sdmmc_softc *, struct sdmmc_card *);
int	sdmmc_enable(struct sdmmc_softc *);
void	sdmmc_disable(struct sdmmc_softc *);
int	sdmmc_decode_csd(struct sdmmc_softc *, sdmmc_response,
	    struct sdmmc_card *);
int	sdmmc_decode_cid(struct sdmmc_softc *, sdmmc_response,
	    struct sdmmc_card *);
void	sdmmc_print_cid(struct sdmmc_cid *);
void	sdmmc_identify_all(struct sdmmc_softc *);

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

struct cfattach sdmmc_ca = {
	sizeof(struct sdmmc_softc), sdmmc_match, sdmmc_attach
};

struct cfdriver sdmmc_cd = {
	NULL, "sdmmc", DV_DULL
};

int
sdmmc_match(struct device *parent, void *match, void *aux)
{
	struct cfdata *cf = match;
	struct sdmmcbus_attach_args *saa = aux;

	return strcmp(saa->saa_busname, cf->cf_driver->cd_name) == 0;
}

void
sdmmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
	struct sdmmcbus_attach_args *saa = aux;

	printf("\n");

	sc->sct = saa->sct;
	sc->sch = saa->sch;

	SIMPLEQ_INIT(&sc->cs_head);
}

/*
 * Called from the host driver when a card, or a stack of cards are
 * inserted.  Return zero if any card drivers have been attached.
 */
int
sdmmc_card_attach(struct device *dev)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)dev;
	struct sdmmc_card *cs;

	DPRINTF(("%s: attach card\n", SDMMCDEVNAME(sc)));

	/* Power up the card (or card stack). */
	if (sdmmc_enable(sc) != 0) {
		printf("%s: can't enable card\n", SDMMCDEVNAME(sc));
		return 1;
	}

	/* Scan for cards and allocate a card structure for each. */
	sdmmc_identify_all(sc);

	/* There should be at least one card now; otherwise, bail out. */
	if (SIMPLEQ_EMPTY(&sc->cs_head)) {
		printf("%s: can't identify card\n", SDMMCDEVNAME(sc));
		sdmmc_disable(sc);
		return 1;
	}

	/* Initialize all identified cards. */
	SIMPLEQ_FOREACH(cs, &sc->cs_head, cs_list) {
		/* Boost the bus width. */
		(void)sdmmc_set_bus_width(sc, cs); /* XXX */

		if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
		    sdmmc_mem_init(sc, cs) != 0)
			printf("%s: init failed\n", SDMMCDEVNAME(sc));
	}

	/* XXX attach SDIO driver(s) */

	/* Attach SCSI emulation for memory cards. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_scsi_attach(sc);
	return 0;
}

/*
 * Called from host driver with DETACH_* flags from <sys/device.h>
 * when cards are gone.
 */
void
sdmmc_card_detach(struct device *dev, int flags)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)dev;
	struct sdmmc_card *cs, *csnext;

	DPRINTF(("%s: detach card\n", SDMMCDEVNAME(sc)));

	/* Power down. */
	sdmmc_disable(sc);

	/* Detach SCSI emulation. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_scsi_detach(sc);

	/* XXX detach SDIO driver(s) */

	/* Free all card structures. */
	for (cs = SIMPLEQ_FIRST(&sc->cs_head); cs != NULL; cs = csnext) {
		csnext = SIMPLEQ_NEXT(cs, cs_list);
		FREE(cs, M_DEVBUF);
	}
	SIMPLEQ_INIT(&sc->cs_head);
}

int
sdmmc_enable(struct sdmmc_softc *sc)
{
	u_int32_t host_ocr;
	int error;

	/*
	 * Calculate the equivalent of the card OCR from the host
	 * capabilities and select the maximum supported bus voltage.
	 */
	host_ocr = sdmmc_chip_host_ocr(sc->sct, sc->sch);
	error = sdmmc_chip_bus_power(sc->sct, sc->sch, host_ocr);
	if (error != 0) {
		printf("%s: can't supply bus power\n", SDMMCDEVNAME(sc));
		goto err;
	}

	/*
	 * Select the minimum clock frequency.
	 */
	error = sdmmc_chip_bus_clock(sc->sct, sc->sch, SDMMC_SDCLK_400KHZ);
	if (error != 0) {
		printf("%s: can't supply clock\n", SDMMCDEVNAME(sc));
		goto err;
	}

	/* Initialize SD I/O card function(s). */
	if ((error = sdmmc_io_enable(sc)) != 0)
		goto err;

	/* Initialize SD/MMC memory card(s). */
	if ((error = sdmmc_mem_enable(sc)) != 0)
		goto err;

	/* XXX */
	if (ISSET(sc->sc_flags, SMF_SD_MODE))
		(void)sdmmc_chip_bus_clock(sc->sct, sc->sch,
		    SDMMC_SDCLK_25MHZ);

 err:
	if (error != 0)
		sdmmc_disable(sc);
	return error;
}

void
sdmmc_disable(struct sdmmc_softc *sc)
{
	/* XXX complete commands if card is still present. */

	/* Deselect all cards. */
	(void)sdmmc_select_card(sc, NULL);

	/* Turn off bus power and clock. */
	(void)sdmmc_chip_bus_clock(sc->sct, sc->sch, SDMMC_SDCLK_OFF);
	(void)sdmmc_chip_bus_power(sc->sct, sc->sch, 0);
}

void
sdmmc_delay(u_int usecs)
{
	int ticks = usecs / (1000000 / hz);

	if (ticks > 0)
		(void)tsleep(&sdmmc_delay, PWAIT, "sdwait", ticks);
	else
		delay(usecs);
}

/*
 * Set the lowest bus voltage supported by the card and the host.
 */
int
sdmmc_set_bus_power(struct sdmmc_softc *sc, u_int32_t host_ocr,
    u_int32_t card_ocr)
{
	u_int32_t bit;

	/* Mask off unsupported voltage levels and select the lowest. */
	DPRINTF(("%s: host_ocr=%x ", SDMMCDEVNAME(sc), host_ocr));
	host_ocr &= card_ocr;
	for (bit = 4; bit < 23; bit++) {
		if (ISSET(host_ocr, 1<<bit)) {
			host_ocr &= 3<<bit;
			break;
		}
	}
	DPRINTF(("card_ocr=%x new_ocr=%x\n", card_ocr, host_ocr));

	if (host_ocr == 0 ||
	    sdmmc_chip_bus_power(sc->sct, sc->sch, host_ocr) != 0)
		return 1;
	return 0;
}

/*
 * Read the CSD and CID from all cards and assign each card a unique
 * relative card address (RCA).
 */
void
sdmmc_identify_all(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;
	struct sdmmc_card *cs;
	u_int16_t next_rca;
	int error;
	int i;

	/*
	 * CMD2 is a broadcast command understood by SD cards and MMC
	 * cards.  All cards begin to respond to the command, but back
	 * off if another card drives the CMD line to a different level.
	 * Only one card will get its entire response through.  That
	 * card remains silent once it has been assigned a RCA.
	 */
	for (i = 0; i < 100; i++) {
		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = MMC_ALL_SEND_CID;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R2;

		error = sdmmc_mmc_command(sc, &cmd);
		if (error == ETIMEDOUT) {
			/* No more cards there. */
			break;
		} else if (error != 0) {
			DPRINTF(("%s: can't read CID\n", SDMMCDEVNAME(sc)));
			break;
		}

		/* In MMC mode, find the next available RCA. */
		next_rca = 0;
		if (!ISSET(sc->sc_flags, SMF_SD_MODE))
			SIMPLEQ_FOREACH(cs, &sc->cs_head, cs_list)
				next_rca++;

		/* Allocate a card structure. */
		MALLOC(cs, struct sdmmc_card *, sizeof *cs, M_DEVBUF,
		    M_WAITOK);
		bzero(cs, sizeof *cs);
		cs->rca = next_rca;

		/*
		 * Remember the CID returned in the CMD2 response for
		 * later decoding.
		 */
		bcopy(cmd.c_resp, cs->raw_cid, sizeof cs->raw_cid);

		/*
		 * Silence the card by assigning it a unique RCA, or
		 * querying it for its RCA in case of SD.
		 */
		if (sdmmc_set_relative_addr(sc, cs) != 0) {
			printf("%s: can't set RCA\n", SDMMCDEVNAME(sc));
			FREE(cs, M_DEVBUF);
			break;
		}

		SIMPLEQ_INSERT_TAIL(&sc->cs_head, cs, cs_list);
	}

	/*
	 * All cards are either inactive or awaiting further commands.
	 * Read the CSDs and decode the raw CID for each card.
	 */
	SIMPLEQ_FOREACH(cs, &sc->cs_head, cs_list) {
		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = MMC_SEND_CSD;
		cmd.c_arg = MMC_ARG_RCA(cs->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R2;

		if (sdmmc_mmc_command(sc, &cmd) != 0) {
			SET(cs->flags, SDMMCF_CARD_ERROR);
			continue;
		}

		if (sdmmc_decode_csd(sc, cmd.c_resp, cs) != 0 ||
		    sdmmc_decode_cid(sc, cs->raw_cid, cs) != 0) {
			SET(cs->flags, SDMMCF_CARD_ERROR);
			continue;
		}

#ifdef SDMMC_DEBUG
		printf("%s: CID: ", SDMMCDEVNAME(sc));
		sdmmc_print_cid(&cs->cid);
#endif
	}
}

int
sdmmc_app_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	struct sdmmc_command acmd;
	int error;

	bzero(&acmd, sizeof acmd);
	acmd.c_opcode = MMC_APP_CMD;
	acmd.c_arg = 0;
	acmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;

	error = sdmmc_mmc_command(sc, &acmd);
	if (error != 0)
		return error;

	if (!ISSET(MMC_R1(acmd.c_resp), MMC_R1_APP_CMD))
		/* Card does not support application commands. */
		return ENODEV;

	return sdmmc_mmc_command(sc, cmd);
}

int
sdmmc_mmc_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	return sdmmc_chip_exec_command(sc->sct, sc->sch, cmd);
}

/*
 * Send the "GO IDLE STATE" command.
 */
void
sdmmc_go_idle_state(struct sdmmc_softc *sc)
{
	struct sdmmc_command cmd;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_GO_IDLE_STATE;
	cmd.c_flags = SCF_CMD_BC | SCF_RSP_R0;

	(void)sdmmc_mmc_command(sc, &cmd);
}

/*
 * Retrieve (SD) or set (MMC) the relative card address (RCA).
 */
int
sdmmc_set_relative_addr(struct sdmmc_softc *sc, struct sdmmc_card *cs)
{
	struct sdmmc_command cmd;

	bzero(&cmd, sizeof cmd);

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R6;
	} else {
		cmd.c_opcode = MMC_SET_RELATIVE_ADDR;
		cmd.c_arg = MMC_ARG_RCA(cs->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	}

	if (sdmmc_mmc_command(sc, &cmd) != 0)
		return 1;

	if (ISSET(sc->sc_flags, SMF_SD_MODE))
		cs->rca = SD_R6_RCA(cmd.c_resp);
	return 0;
}

/*
 * Set the maximum supported bus width.
 */
int
sdmmc_set_bus_width(struct sdmmc_softc *sc, struct sdmmc_card *cs)
{
	struct sdmmc_command cmd;
	int error;

	if (!ISSET(sc->sc_flags, SMF_SD_MODE))
		return EOPNOTSUPP;

	if ((error = sdmmc_select_card(sc, cs)) != 0)
		return error;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = SD_APP_SET_BUS_WIDTH;
	cmd.c_arg = SD_ARG_BUS_WIDTH_4;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;

	return sdmmc_app_command(sc, &cmd);
}

int
sdmmc_select_card(struct sdmmc_softc *sc, struct sdmmc_card *cs)
{
	struct sdmmc_command cmd;
	int error;

	if (sc->sc_card == cs)
		return 0;

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_SELECT_CARD;
	cmd.c_arg = cs == NULL ? 0 : MMC_ARG_RCA(cs->rca);
	cmd.c_flags = SCF_CMD_AC | (cs == NULL ? SCF_RSP_R0 : SCF_RSP_R1);
	error = sdmmc_mmc_command(sc, &cmd);
	if (error == 0 || cs == NULL)
		sc->sc_card = cs;
	return error;
}

int
sdmmc_decode_csd(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_card *cs)
{
	struct sdmmc_csd *csd = &cs->csd;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		/*
		 * CSD version 1.0 corresponds to SD system
		 * specification version 1.0 - 1.10. (SanDisk, 3.5.3)
		 */
		csd->csdver = SD_CSD_CSDVER(resp);
		if (csd->csdver != SD_CSD_CSDVER_1_0) {
			printf("%s: unknown SD CSD structure version 0x%x\n",
			    SDMMCDEVNAME(sc), csd->csdver);
			return 1;
		}

		csd->capacity = SD_CSD_CAPACITY(resp);
		csd->read_bl_len = SD_CSD_READ_BL_LEN(resp);
	} else {
		csd->csdver = MMC_CSD_CSDVER(resp);
		if (csd->csdver != MMC_CSD_CSDVER_1_0 &&
		    csd->csdver != MMC_CSD_CSDVER_2_0) {
			printf("%s: unknown MMC CSD structure version 0x%x\n",
			    SDMMCDEVNAME(sc), csd->csdver);
			return 1;
		}

		csd->mmcver = MMC_CSD_MMCVER(resp);
		csd->capacity = MMC_CSD_CAPACITY(resp);
		csd->read_bl_len = MMC_CSD_READ_BL_LEN(resp);
		csd->sector_size = 1 << csd->read_bl_len;
	}
	csd->sector_size = MIN(1 << csd->read_bl_len,
	    sdmmc_chip_host_maxblklen(sc->sct, sc->sch));
	return 0;
}

int
sdmmc_decode_cid(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_card *cs)
{
	struct sdmmc_cid *cid = &cs->cid;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cid->mid = SD_CID_MID(resp);
		cid->oid = SD_CID_OID(resp);
		SD_CID_PNM_CPY(resp, cid->pnm);
		cid->rev = SD_CID_REV(resp);
		cid->psn = SD_CID_PSN(resp);
		cid->mdt = SD_CID_MDT(resp);
	} else {
		switch(cs->csd.mmcver) {
		case MMC_CSD_MMCVER_1_0:
		case MMC_CSD_MMCVER_1_4:
			cid->mid = MMC_CID_MID_V1(resp);
			MMC_CID_PNM_V1_CPY(resp, cid->pnm);
			cid->rev = MMC_CID_REV_V1(resp);
			cid->psn = MMC_CID_PSN_V1(resp);
			cid->mdt = MMC_CID_MDT_V1(resp);
			break;
		case MMC_CSD_MMCVER_2_0:
		case MMC_CSD_MMCVER_3_1:
		case MMC_CSD_MMCVER_4_0:
			cid->mid = MMC_CID_MID_V2(resp);
			cid->oid = MMC_CID_OID_V2(resp);
			MMC_CID_PNM_V2_CPY(resp, cid->pnm);
			cid->psn = MMC_CID_PSN_V2(resp);
			break;
		default:
			printf("%s: unknown MMC version %d\n",
			    SDMMCDEVNAME(sc), cs->csd.mmcver);
			return 1;
		}
	}
	return 0;
}

void
sdmmc_print_cid(struct sdmmc_cid *cid)
{
	printf("mid=0x%02x oid=0x%04x pnm=\"%s\" rev=0x%02x psn=0x%08x"
	    " mdt=%03x\n", cid->mid, cid->oid, cid->pnm, cid->rev, cid->psn,
	    cid->mdt);
}
