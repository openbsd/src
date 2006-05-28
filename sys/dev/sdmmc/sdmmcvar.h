/*	$OpenBSD: sdmmcvar.h,v 1.1 2006/05/28 17:21:14 uwe Exp $	*/

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

#ifndef _SDMMCVAR_H_
#define _SDMMCVAR_H_

#include <sys/queue.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcreg.h>

struct sdmmc_csd {
	int	csdver;		/* CSD structure format */
	int	mmcver;		/* MMC version (for CID format) */
	int	capacity;	/* total number of sectors */
	int	sector_size;	/* sector size in bytes */
	int	read_bl_len;	/* block length for reads */
	/* ... */
};

struct sdmmc_cid {
	int	mid;		/* manufacturer identification number */
	int	oid;		/* OEM/product identification number */
	char	pnm[8];		/* product name (MMC v1 has the longest) */
	int	rev;		/* product revision */
	int	psn;		/* product serial number */
	int	mdt;		/* manufacturing date */
};

struct sdmmc_command;

typedef u_int32_t sdmmc_response[4];
typedef void (*sdmmc_callback)(struct device *, struct sdmmc_command *);

struct sdmmc_command {
	u_int16_t	 c_opcode;	/* SD or MMC command index */
	u_int32_t	 c_arg;		/* SD/MMC command argument */
	sdmmc_response	 c_resp;	/* response buffer */
	void		*c_data;	/* buffer to send or read into */
	int		 c_datalen;	/* length of data buffer */
	int		 c_blklen;	/* block length */
	int		 c_flags;	/* see below */
#define SCF_DONE	 0x0001		/* command is finished */
#define SCF_BUF_READY	 0x0002		/* buffer ready int occurred */
#define SCF_CMD_DONE	 0x0004		/* cmd complete int occurred */
#define SCF_XFR_DONE	 0x0008		/* transfer complete int occurred */
#define SCF_CMD_AC	 0x0000
#define SCF_CMD_ADTC	 0x0010
#define SCF_CMD_BC	 0x0020
#define SCF_CMD_BCR	 0x0030
#define SCF_CMD_READ	 0x0040		/* read command (data expected) */
#define SCF_RSP_BSY	 0x0100
#define SCF_RSP_136	 0x0200
#define SCF_RSP_CRC	 0x0400
#define SCF_RSP_IDX	 0x0800
#define SCF_RSP_PRESENT	 0x1000
/* response types */
#define SCF_RSP_R0	 0 /* none */
#define SCF_RSP_R1	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_IDX)
#define SCF_RSP_R2	 (SCF_RSP_PRESENT|SCF_RSP_CRC|SCF_RSP_136)
#define SCF_RSP_R3	 (SCF_RSP_PRESENT)
#define SCF_RSP_R4	 (SCF_RSP_PRESENT)
#define SCF_RSP_R6	 (SCF_RSP_PRESENT|SCF_RSP_CRC)
	sdmmc_callback	 c_done;	/* callback function */
	int		 c_error;	/* errno value on completion */
};

struct sdmmc_card {
	u_int16_t rca;
	int flags;
#define SDMMCF_CARD_ERROR	0x0010	/* card in error state */
	sdmmc_response raw_cid;
	struct sdmmc_cid cid;
	struct sdmmc_csd csd;
	SIMPLEQ_ENTRY(sdmmc_card) cs_list;
};

struct sdmmc_softc {
	struct device sc_dev;		/* base device */
#define SDMMCDEVNAME(sc)	((sc)->sc_dev.dv_xname)
	sdmmc_chipset_tag_t sct;	/* host controller chipset tag */
	sdmmc_chipset_handle_t sch;	/* host controller chipset handle */
	int sc_flags;
#define SMF_SD_MODE		0x0001	/* host in SD mode (MMC otherwise) */
#define SMF_IO_MODE		0x0002	/* host in I/O mode (SD only) */
#define SMF_MEM_MODE		0x0004	/* host in memory mode (SD or MMC) */
	SIMPLEQ_HEAD(, sdmmc_card) cs_head;
	struct sdmmc_card *sc_card;	/* selected card */
	void *sc_scsibus;		/* SCSI bus emulation softc */
};

#define IPL_SDMMC	IPL_BIO
#define splsdmmc()	splbio()

void	sdmmc_delay(u_int);
int	sdmmc_set_bus_power(struct sdmmc_softc *, u_int32_t, u_int32_t);
int	sdmmc_mmc_command(struct sdmmc_softc *, struct sdmmc_command *);
int	sdmmc_app_command(struct sdmmc_softc *, struct sdmmc_command *);
void	sdmmc_go_idle_state(struct sdmmc_softc *);
int	sdmmc_select_card(struct sdmmc_softc *, struct sdmmc_card *);

int	sdmmc_io_enable(struct sdmmc_softc *);
void	sdmmc_io_reset(struct sdmmc_softc *);
int	sdmmc_io_send_op_cond(struct sdmmc_softc *, u_int32_t, u_int32_t *);

int	sdmmc_mem_enable(struct sdmmc_softc *);
int	sdmmc_mem_init(struct sdmmc_softc *, struct sdmmc_card *);
int	sdmmc_mem_read_block(struct sdmmc_softc *, struct sdmmc_card *,
	    int, u_char *, size_t);
int	sdmmc_mem_write_block(struct sdmmc_softc *, struct sdmmc_card *,
	    int, u_char *, size_t);

#endif
