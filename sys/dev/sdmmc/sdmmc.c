/*	$OpenBSD: sdmmc.c,v 1.8 2006/11/29 00:46:52 uwe Exp $	*/

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
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/sdmmc/sdmmc_ioreg.h>
#include <dev/sdmmc/sdmmc_scsi.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>
#include <dev/sdmmc/sdmmcvar.h>

#ifdef SDMMC_IOCTL
#include "bio.h"
#if NBIO < 1
#undef SDMMC_IOCTL
#endif
#include <dev/biovar.h>
#endif

int	sdmmc_match(struct device *, void *, void *);
void	sdmmc_attach(struct device *, struct device *, void *);
int	sdmmc_detach(struct device *, int);
void	sdmmc_create_thread(void *);
void	sdmmc_task_thread(void *);
void	sdmmc_discover_task(void *);
void	sdmmc_card_attach(struct sdmmc_softc *);
void	sdmmc_card_detach(struct sdmmc_softc *, int);
int	sdmmc_enable(struct sdmmc_softc *);
void	sdmmc_disable(struct sdmmc_softc *);
int	sdmmc_scan(struct sdmmc_softc *);
int	sdmmc_init(struct sdmmc_softc *);
int	sdmmc_set_bus_width(struct sdmmc_function *);
#ifdef SDMMC_IOCTL
int	sdmmc_ioctl(struct device *, u_long, caddr_t);
#endif

#define DEVNAME(sc)	SDMMCDEVNAME(sc)

#ifdef SDMMC_DEBUG
int sdmmcdebug = 0;
extern int sdhcdebug;	/* XXX should have a sdmmc_chip_debug() function */
#define DPRINTF(n,s)	do { if ((n) <= sdmmcdebug) printf s; } while (0)
#else
#define DPRINTF(n,s)	do {} while (0)
#endif

struct cfattach sdmmc_ca = {
	sizeof(struct sdmmc_softc), sdmmc_match, sdmmc_attach, sdmmc_detach
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

	SIMPLEQ_INIT(&sc->sf_head);
	TAILQ_INIT(&sc->sc_tskq);
	sdmmc_init_task(&sc->sc_discover_task, sdmmc_discover_task, sc);
	lockinit(&sc->sc_lock, PRIBIO, DEVNAME(sc), 0, LK_CANRECURSE);

#ifdef SDMMC_IOCTL
	if (bio_register(self, sdmmc_ioctl) != 0)
		printf("%s: unable to register ioctl\n", DEVNAME(sc));
#endif

	/*
	 * Create the event thread that will attach and detach cards
	 * and perform other lengthy operations.
	 */
#ifdef DO_CONFIG_PENDING
	config_pending_incr();
#endif
	kthread_create_deferred(sdmmc_create_thread, sc);
}

int
sdmmc_detach(struct device *self, int flags)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;

	sc->sc_dying = 1;
	while (sc->sc_task_thread != NULL) {
		wakeup(&sc->sc_tskq);
		tsleep(sc, PWAIT, "mmcdie", 0);
	}
	return 0;
}

void
sdmmc_create_thread(void *arg)
{
	struct sdmmc_softc *sc = arg;

	if (kthread_create(sdmmc_task_thread, sc, &sc->sc_task_thread,
	    "%s", DEVNAME(sc)) != 0)
		printf("%s: can't create task thread\n", DEVNAME(sc));

#ifdef DO_CONFIG_PENDING
	config_pending_decr();
#endif
}

void
sdmmc_task_thread(void *arg)
{
	struct sdmmc_softc *sc = arg;
	struct sdmmc_task *task;
	int s;

	sdmmc_needs_discover(&sc->sc_dev);

	s = splsdmmc();
	while (!sc->sc_dying) {
		for (task = TAILQ_FIRST(&sc->sc_tskq); task != NULL;
		     task = TAILQ_FIRST(&sc->sc_tskq)) {
			splx(s);
			sdmmc_del_task(task);
			task->func(task->arg);
			s = splsdmmc();
		}
		tsleep(&sc->sc_tskq, PWAIT, "mmctsk", 0);
	}
	splx(s);

	if (ISSET(sc->sc_flags, SMF_CARD_PRESENT))
		sdmmc_card_detach(sc, DETACH_FORCE);

	sc->sc_task_thread = NULL;
	wakeup(sc);
	kthread_exit(0);
}

void
sdmmc_add_task(struct sdmmc_softc *sc, struct sdmmc_task *task)
{
	int s;

	s = splsdmmc();
	TAILQ_INSERT_TAIL(&sc->sc_tskq, task, next);
	task->onqueue = 1;
	task->sc = sc;
	wakeup(&sc->sc_tskq);
	splx(s);
}

void
sdmmc_del_task(struct sdmmc_task *task)
{
	struct sdmmc_softc *sc = task->sc;
	int s;

	if (sc == NULL)
		return;

	s = splsdmmc();
	task->sc = NULL;
	task->onqueue = 0;
	TAILQ_REMOVE(&sc->sc_tskq, task, next);
	splx(s);
}

void
sdmmc_needs_discover(struct device *self)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;

	if (!sdmmc_task_pending(&sc->sc_discover_task))
		sdmmc_add_task(sc, &sc->sc_discover_task);
}

void
sdmmc_discover_task(void *arg)
{
	struct sdmmc_softc *sc = arg;

	if (sdmmc_chip_card_detect(sc->sct, sc->sch)) {
		if (!ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
			SET(sc->sc_flags, SMF_CARD_PRESENT);
			sdmmc_card_attach(sc);
		}
	} else {
		if (ISSET(sc->sc_flags, SMF_CARD_PRESENT)) {
			CLR(sc->sc_flags, SMF_CARD_PRESENT);
			sdmmc_card_detach(sc, DETACH_FORCE);
		}
	}
}

/*
 * Called from process context when a card is present.
 */
void
sdmmc_card_attach(struct sdmmc_softc *sc)
{
	DPRINTF(1,("%s: attach card\n", DEVNAME(sc)));

	SDMMC_LOCK(sc);
	CLR(sc->sc_flags, SMF_CARD_ATTACHED);

	/*
	 * Power up the card (or card stack).
	 */
	if (sdmmc_enable(sc) != 0) {
		printf("%s: can't enable card\n", DEVNAME(sc));
		goto err;
	}

	/*
	 * Scan for I/O functions and memory cards on the bus,
	 * allocating a sdmmc_function structure for each.
	 */
	if (sdmmc_scan(sc) != 0) {
		printf("%s: no functions\n", DEVNAME(sc));
		goto err;
	}

	/*
	 * Initialize the I/O functions and memory cards.
	 */
	if (sdmmc_init(sc) != 0) {
		printf("%s: init failed\n", DEVNAME(sc));
		goto err;
	}

	/* Attach SCSI emulation for memory cards. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_scsi_attach(sc);

	/* Attach I/O function drivers. */
	if (ISSET(sc->sc_flags, SMF_IO_MODE))
		sdmmc_io_attach(sc);

	SET(sc->sc_flags, SMF_CARD_ATTACHED);
	SDMMC_UNLOCK(sc);
	return;
err:
	sdmmc_card_detach(sc, DETACH_FORCE);
	SDMMC_UNLOCK(sc);
}

/*
 * Called from process context with DETACH_* flags from <sys/device.h>
 * when cards are gone.
 */
void
sdmmc_card_detach(struct sdmmc_softc *sc, int flags)
{
	struct sdmmc_function *sf, *sfnext;

	DPRINTF(1,("%s: detach card\n", DEVNAME(sc)));

	if (ISSET(sc->sc_flags, SMF_CARD_ATTACHED)) {
		/* Detach I/O function drivers. */
		if (ISSET(sc->sc_flags, SMF_IO_MODE))
			sdmmc_io_detach(sc);

		/* Detach the SCSI emulation for memory cards. */
		if (ISSET(sc->sc_flags, SMF_MEM_MODE))
			sdmmc_scsi_detach(sc);

		CLR(sc->sc_flags, SMF_CARD_ATTACHED);
	}

	/* Power down. */
	sdmmc_disable(sc);

	/* Free all sdmmc_function structures. */
	for (sf = SIMPLEQ_FIRST(&sc->sf_head); sf != NULL; sf = sfnext) {
		sfnext = SIMPLEQ_NEXT(sf, sf_list);
		sdmmc_function_free(sf);
	}
	SIMPLEQ_INIT(&sc->sf_head);
	sc->sc_function_count = 0;
	sc->sc_fn0 = NULL;
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
		printf("%s: can't supply bus power\n", DEVNAME(sc));
		goto err;
	}

	/*
	 * Select the minimum clock frequency.
	 */
	error = sdmmc_chip_bus_clock(sc->sct, sc->sch, SDMMC_SDCLK_400KHZ);
	if (error != 0) {
		printf("%s: can't supply clock\n", DEVNAME(sc));
		goto err;
	}

	/* XXX wait for card to power up */
	sdmmc_delay(100000);

	/* Initialize SD I/O card function(s). */
	if ((error = sdmmc_io_enable(sc)) != 0)
		goto err;

	/* Initialize SD/MMC memory card(s). */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
	    (error = sdmmc_mem_enable(sc)) != 0)
		goto err;

	/* XXX respect host and card capabilities */
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

	/* Make sure no card is still selected. */
	(void)sdmmc_select_card(sc, NULL);

	/* Turn off bus power and clock. */
	(void)sdmmc_chip_bus_clock(sc->sct, sc->sch, SDMMC_SDCLK_OFF);
	(void)sdmmc_chip_bus_power(sc->sct, sc->sch, 0);
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
	DPRINTF(1,("%s: host_ocr=%x ", DEVNAME(sc), host_ocr));
	host_ocr &= card_ocr;
	for (bit = 4; bit < 23; bit++) {
		if (ISSET(host_ocr, 1<<bit)) {
			host_ocr &= 3<<bit;
			break;
		}
	}
	DPRINTF(1,("card_ocr=%x new_ocr=%x\n", card_ocr, host_ocr));

	if (host_ocr == 0 ||
	    sdmmc_chip_bus_power(sc->sct, sc->sch, host_ocr) != 0)
		return 1;
	return 0;
}

struct sdmmc_function *
sdmmc_function_alloc(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;

	MALLOC(sf, struct sdmmc_function *, sizeof *sf, M_DEVBUF,
	    M_WAITOK);
	bzero(sf, sizeof *sf);
	sf->sc = sc;
	sf->number = -1;
	sf->cis.manufacturer = SDMMC_VENDOR_INVALID;
	sf->cis.product = SDMMC_PRODUCT_INVALID;
	sf->cis.function = SDMMC_FUNCTION_INVALID;
	return sf;
}

void
sdmmc_function_free(struct sdmmc_function *sf)
{
	FREE(sf, M_DEVBUF);
}

/*
 * Scan for I/O functions and memory cards on the bus, allocating a
 * sdmmc_function structure for each.
 */
int
sdmmc_scan(struct sdmmc_softc *sc)
{
	/* Scan for I/O functions. */
	if (ISSET(sc->sc_flags, SMF_IO_MODE))
		sdmmc_io_scan(sc);

	/* Scan for memory cards on the bus. */
	if (ISSET(sc->sc_flags, SMF_MEM_MODE))
		sdmmc_mem_scan(sc);

	/* There should be at least one function now. */
	if (SIMPLEQ_EMPTY(&sc->sf_head)) {
		printf("%s: can't identify card\n", DEVNAME(sc));
		return 1;
	}
	return 0;
}

/*
 * Initialize all the distinguished functions of the card, be it I/O
 * or memory functions.
 */
int
sdmmc_init(struct sdmmc_softc *sc)
{
	struct sdmmc_function *sf;

	/* Initialize all identified card functions. */
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (ISSET(sc->sc_flags, SMF_IO_MODE) &&
		    sdmmc_io_init(sc, sf) != 0)
			printf("%s: i/o init failed\n", DEVNAME(sc));

		if (ISSET(sc->sc_flags, SMF_MEM_MODE) &&
		    sdmmc_mem_init(sc, sf) != 0)
			printf("%s: mem init failed\n", DEVNAME(sc));
	}

	/* Any good functions left after initialization? */
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (!ISSET(sf->flags, SFF_ERROR))
			return 0;
	}
	/* No, we should probably power down the card. */
	return 1;
}

void
sdmmc_delay(u_int usecs)
{
	int ticks = usecs / (1000000 / hz);

	if (ticks > 0)
		tsleep(&sdmmc_delay, PWAIT, "mmcdly", ticks);
	else
		delay(usecs);
}

int
sdmmc_app_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	struct sdmmc_command acmd;
	int error;

	SDMMC_LOCK(sc);

	bzero(&acmd, sizeof acmd);
	acmd.c_opcode = MMC_APP_CMD;
	acmd.c_arg = 0;
	acmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;

	error = sdmmc_mmc_command(sc, &acmd);
	if (error != 0) {
		SDMMC_UNLOCK(sc);
		return error;
	}

	if (!ISSET(MMC_R1(acmd.c_resp), MMC_R1_APP_CMD)) {
		/* Card does not support application commands. */
		SDMMC_UNLOCK(sc);
		return ENODEV;
	}

	error = sdmmc_mmc_command(sc, cmd);
	SDMMC_UNLOCK(sc);
	return error;
}

/*
 * Execute MMC command and data transfers.  All interactions with the
 * host controller to complete the command happen in the context of
 * the current process.
 */
int
sdmmc_mmc_command(struct sdmmc_softc *sc, struct sdmmc_command *cmd)
{
	int error;

	SDMMC_LOCK(sc);

	sdmmc_chip_exec_command(sc->sct, sc->sch, cmd);

	DPRINTF(2,("%s: mmc cmd=%p opcode=%d proc=\"%s\" (error %d)\n",
	    DEVNAME(sc), cmd, cmd->c_opcode, curproc ? curproc->p_comm :
	    "", cmd->c_error));

	error = cmd->c_error;
	wakeup(cmd);

	SDMMC_UNLOCK(sc);
	return error;
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
sdmmc_set_relative_addr(struct sdmmc_softc *sc,
    struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;

	bzero(&cmd, sizeof cmd);

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cmd.c_opcode = SD_SEND_RELATIVE_ADDR;
		cmd.c_flags = SCF_CMD_BCR | SCF_RSP_R6;
	} else {
		cmd.c_opcode = MMC_SET_RELATIVE_ADDR;
		cmd.c_arg = MMC_ARG_RCA(sf->rca);
		cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	}

	if (sdmmc_mmc_command(sc, &cmd) != 0)
		return 1;

	if (ISSET(sc->sc_flags, SMF_SD_MODE))
		sf->rca = SD_R6_RCA(cmd.c_resp);
	return 0;
}

/*
 * Switch card and host to the maximum supported bus width.
 */
int
sdmmc_set_bus_width(struct sdmmc_function *sf)
{
	struct sdmmc_softc *sc = sf->sc;
	struct sdmmc_command cmd;
	int error;

	SDMMC_LOCK(sc);

	if (!ISSET(sc->sc_flags, SMF_SD_MODE)) {
		SDMMC_UNLOCK(sc);
		return EOPNOTSUPP;
	}

	if ((error = sdmmc_select_card(sc, sf)) != 0) {
		SDMMC_UNLOCK(sc);
		return error;
	}

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = SD_APP_SET_BUS_WIDTH;
	cmd.c_arg = SD_ARG_BUS_WIDTH_4;
	cmd.c_flags = SCF_CMD_AC | SCF_RSP_R1;
	error = sdmmc_app_command(sc, &cmd);
	SDMMC_UNLOCK(sc);
	return error;
}

int
sdmmc_select_card(struct sdmmc_softc *sc, struct sdmmc_function *sf)
{
	struct sdmmc_command cmd;
	int error;

	if (sc->sc_card == sf || (sf && sc->sc_card &&
	    sc->sc_card->rca == sf->rca)) {
		sc->sc_card = sf;
		return 0;
	}

	bzero(&cmd, sizeof cmd);
	cmd.c_opcode = MMC_SELECT_CARD;
	cmd.c_arg = sf == NULL ? 0 : MMC_ARG_RCA(sf->rca);
	cmd.c_flags = SCF_CMD_AC | (sf == NULL ? SCF_RSP_R0 : SCF_RSP_R1);
	error = sdmmc_mmc_command(sc, &cmd);
	if (error == 0 || sf == NULL)
		sc->sc_card = sf;
	return error;
}

int
sdmmc_decode_csd(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_function *sf)
{
	struct sdmmc_csd *csd = &sf->csd;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		/*
		 * CSD version 1.0 corresponds to SD system
		 * specification version 1.0 - 1.10. (SanDisk, 3.5.3)
		 */
		csd->csdver = SD_CSD_CSDVER(resp);
		if (csd->csdver != SD_CSD_CSDVER_1_0) {
			printf("%s: unknown SD CSD structure version 0x%x\n",
			    DEVNAME(sc), csd->csdver);
			return 1;
		}

		csd->capacity = SD_CSD_CAPACITY(resp);
		csd->read_bl_len = SD_CSD_READ_BL_LEN(resp);
	} else {
		csd->csdver = MMC_CSD_CSDVER(resp);
		if (csd->csdver != MMC_CSD_CSDVER_1_0 &&
		    csd->csdver != MMC_CSD_CSDVER_2_0) {
			printf("%s: unknown MMC CSD structure version 0x%x\n",
			    DEVNAME(sc), csd->csdver);
			return 1;
		}

		csd->mmcver = MMC_CSD_MMCVER(resp);
		csd->capacity = MMC_CSD_CAPACITY(resp);
		csd->read_bl_len = MMC_CSD_READ_BL_LEN(resp);
	}
	csd->sector_size = MIN(1 << csd->read_bl_len,
	    sdmmc_chip_host_maxblklen(sc->sct, sc->sch));
	if (csd->sector_size < (1<<csd->read_bl_len))
		csd->capacity *= (1<<csd->read_bl_len) /
		    csd->sector_size;

	return 0;
}

int
sdmmc_decode_cid(struct sdmmc_softc *sc, sdmmc_response resp,
    struct sdmmc_function *sf)
{
	struct sdmmc_cid *cid = &sf->cid;

	if (ISSET(sc->sc_flags, SMF_SD_MODE)) {
		cid->mid = SD_CID_MID(resp);
		cid->oid = SD_CID_OID(resp);
		SD_CID_PNM_CPY(resp, cid->pnm);
		cid->rev = SD_CID_REV(resp);
		cid->psn = SD_CID_PSN(resp);
		cid->mdt = SD_CID_MDT(resp);
	} else {
		switch(sf->csd.mmcver) {
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
			    DEVNAME(sc), sf->csd.mmcver);
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

#ifdef SDMMC_IOCTL
int
sdmmc_ioctl(struct device *self, u_long request, caddr_t addr)
{
	struct sdmmc_softc *sc = (struct sdmmc_softc *)self;
	struct sdmmc_command *ucmd;
	struct sdmmc_command cmd;
	void *data;
	int error;

	switch (request) {
#ifdef SDMMC_DEBUG
	case SDIOCSETDEBUG:
		sdmmcdebug = (((struct bio_sdmmc_debug *)addr)->debug) & 0xff;
		sdhcdebug = (((struct bio_sdmmc_debug *)addr)->debug >> 8) & 0xff;
		break;
#endif

	case SDIOCEXECMMC:
	case SDIOCEXECAPP:
		ucmd = &((struct bio_sdmmc_command *)addr)->cmd;

		/* Refuse to transfer more than 512K per command. */
		if (ucmd->c_datalen > 524288)
			return ENOMEM;

		/* Verify that the data buffer is safe to copy. */
		if ((ucmd->c_datalen > 0 && ucmd->c_data == NULL) ||
		    (ucmd->c_datalen < 1 && ucmd->c_data != NULL) ||
		    ucmd->c_datalen < 0)
			return EINVAL;

		bzero(&cmd, sizeof cmd);
		cmd.c_opcode = ucmd->c_opcode;
		cmd.c_arg = ucmd->c_arg;
		cmd.c_flags = ucmd->c_flags;
		cmd.c_blklen = ucmd->c_blklen;

		if (ucmd->c_data) {
			data = malloc(ucmd->c_datalen, M_DEVBUF,
			    M_WAITOK | M_CANFAIL);
			if (data == NULL)
				return ENOMEM;
			if (copyin(ucmd->c_data, data, ucmd->c_datalen))
				return EFAULT;

			cmd.c_data = data;
			cmd.c_datalen = ucmd->c_datalen;
		}

		if (request == SDIOCEXECMMC)
			error = sdmmc_mmc_command(sc, &cmd);
		else
			error = sdmmc_app_command(sc, &cmd);
		if (error && !cmd.c_error)
			cmd.c_error = error;

		bcopy(&cmd.c_resp, ucmd->c_resp, sizeof cmd.c_resp);
		ucmd->c_flags = cmd.c_flags;
		ucmd->c_error = cmd.c_error;

		if (ucmd->c_data && copyout(data, ucmd->c_data,
		    ucmd->c_datalen))
			return EFAULT;

		if (ucmd->c_data)
			free(data, M_DEVBUF);
		break;

	default:
		return ENOTTY;
	}
	return 0;
}
#endif
