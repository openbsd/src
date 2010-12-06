/*	$OpenBSD: ueaglevar.h,v 1.3 2010/12/06 04:41:40 jakemsr Exp $	*/

/*-
 * Copyright (c) 2003-2005
 *	Damien Bergamini <damien.bergamini@free.fr>
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

#define UEAGLE_NISOREQS	6
#define UEAGLE_NISOFRMS	4

#ifndef UEAGLE_INTR_INTERVAL
#define UEAGLE_INTR_INTERVAL	10	/* ms */
#endif

#define UEAGLE_TX_LIST_CNT	1

#define UEAGLE_IDMA_TIMEOUT	1000
#define UEAGLE_TX_TIMEOUT	10000

#define CRC_INITIAL	0xffffffff
#define CRC_MAGIC	0xc704dd7b

#define ATM_CELL_SIZE		53
#define ATM_CELL_HEADER_SIZE	5
#define ATM_CELL_PAYLOAD_SIZE	(ATM_CELL_SIZE - ATM_CELL_HEADER_SIZE)

#define AAL5_TRAILER_SIZE	8

/*-
 *    ATM cell header:
 *
 *    0                 4                 8
 *    +-----------------+-----------------+
 *    |       GFC       |       VPI       |
 *    +-----------------+-----------------+
 *    |       VPI       |       VCI       |
 *    +-----------------+-----------------+
 *    |                VCI                |
 *    +-----------------+-----------+-----+
 *    |       VCI       |   PT (3)  | CLP |
 *    +-----------------+-----------+-----+
 *    |                HEC                |
 *    +-----------------------------------+
 */
#define ATM_CH_FILL(x, gfc, vpi, vci, pt, clp, hec) do {		\
	(x)[0] = ((gfc) & 0xf) << 4 | ((vpi) & 0xf0) >> 4;		\
	(x)[1] = ((vpi) & 0xf) << 4 | ((vci) & 0xf000) >> 12;		\
	(x)[2] = ((vci) & 0xff0) >> 4;  				\
	(x)[3] = ((vci) & 0xf) << 4 | ((pt) & 0x7) << 1 | ((clp) & 0x1);\
	(x)[4] = (uint8_t)(hec);					\
} while (/* CONSTCOND */0)

#define ATM_CH_SETPTFLAGS(x, v)	((x)[3] |= ((v) & 0x7) << 1)
#define ATM_CH_GETPTFLAGS(x)	(((x)[3] >> 1) & 0x7)
#define ATM_CH_GETVPI(x)	((x)[0] << 4 | (x)[1] >> 4)
#define ATM_CH_GETVCI(x) \
	(((x)[1] & 0xf) << 12 | (x)[2] << 4 | ((x)[3] & 0xf0) >> 4)

/* optimized shortcut for (ATM_CH_GETPTFLAGS(x) & 1) */
#define ATM_CH_ISLASTCELL(x)	((x)[3] & 0x2)

#define AAL5_TR_SETCPSUU(x, v)	((x)[45] = (uint8_t)(v))
#define AAL5_TR_SETCPI(x, v)	((x)[46] = (uint8_t)(v))
#define AAL5_TR_SETPDULEN(x, v) do {					\
	(x)[47] = (uint8_t)((v) >> 8);					\
	(x)[48] = (uint8_t)(v);						\
} while (/* CONSTCOND */0)

#define AAL5_TR_GETPDULEN(x)	(uint16_t)((x)[47] << 8 | (x)[48])
#define AAL5_TR_SETCRC(x, v) do {					\
	(x)[49] = (uint8_t)((v) >> 24);					\
	(x)[50] = (uint8_t)((v) >> 16);					\
	(x)[51] = (uint8_t)((v) >> 8); 					\
	(x)[52] = (uint8_t)(v);						\
} while (/* CONSTCOND */0)

#define UEAGLE_IFMTU		1500
#define UEAGLE_TXBUFLEN							\
	(((UEAGLE_IFMTU / ATM_CELL_PAYLOAD_SIZE) + 2) * ATM_CELL_SIZE)

struct ueagle_vcc {
	uint16_t		vci;
	uint8_t			vpi;
	uint8_t			ch[ATM_CELL_HEADER_SIZE];
	void 			*rxhand;
	struct mbuf		*m;
	uint8_t			*dst;
	uint8_t			*limit;
	struct atm_pseudohdr	aph;
	int			flags;
#define UEAGLE_VCC_ACTIVE	(1 << 0)
#define UEAGLE_VCC_DROP		(1 << 1)
};

struct ueagle_softc;

struct ueagle_isoreq {
	struct ueagle_softc	*sc;
	usbd_xfer_handle	xfer;
	uint16_t		frlengths[UEAGLE_NISOFRMS];
	uint8_t			*offsets[UEAGLE_NISOFRMS];
};

struct ueagle_txreq {
	struct ueagle_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
};

struct ueagle_stats {
	struct {
		uint32_t	status;
		uint32_t	flags;
		uint32_t	vidcpe;
		uint32_t	vidco;
		uint32_t	dsrate;
		uint32_t	usrate;
		uint32_t	dserror;
		uint32_t	userror;
		uint32_t	dsunc;
		uint32_t	usunc;
		uint32_t	txflow;
		uint32_t	rxflow;
		uint32_t	attenuation;
		uint32_t	dsmargin;
		uint32_t	usmargin;
	} phy;

	struct {
		uint32_t	cells_transmitted;
		uint32_t	cells_received;
		uint32_t	cells_crc_errors;
		uint32_t	cells_dropped;
		uint32_t	vcc_no_conn;
		uint32_t	cspdus_transmitted;
		uint32_t	cspdus_received;
		uint32_t	cspdus_crc_errors;
		uint32_t	cspdus_dropped;
	} atm;
};

#define UEAGLE_COND_CMV(sc)	((char *)(sc) + 1)
#define UEAGLE_COND_READY(sc)	((char *)(sc) + 2)
#define UEAGLE_COND_SYNC(sc)	((char *)(sc) + 3)

struct ueagle_softc {
	struct device		sc_dev;
	struct ifnet		sc_if;

	usbd_device_handle	sc_udev;

	struct proc		*stat_thread;
	struct usb_task		sc_swap_task;
	uint16_t		pageno;
	uint16_t		ovl;

	const char		*fw;
	uint8_t			*dsp;

	struct usb_task		sc_init_task;

	usbd_pipe_handle	pipeh_tx;
	usbd_pipe_handle	pipeh_rx;
	usbd_pipe_handle	pipeh_idma;
	usbd_pipe_handle	pipeh_intr;

	struct ueagle_isoreq	isoreqs[UEAGLE_NISOREQS];
	struct ueagle_txreq	txreqs[UEAGLE_TX_LIST_CNT];
	struct ueagle_vcc	vcc;
	struct ueagle_stats	stats;

	uint16_t		isize;
	char			ibuf[32];

	uint16_t		index;
	uint32_t		data;
};
