/*	$OpenBSD: ahcivar.h,v 1.5 2014/03/31 03:38:46 dlg Exp $ */

/*
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2010 Conformal Systems LLC <info@conformal.com>
 * Copyright (c) 2010 Jonathan Matthew <jonathan@d14n.org>
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

#include <sys/timeout.h>
#include <dev/ata/atascsi.h>
#include <dev/ata/pmreg.h>

/* change to AHCI_DEBUG for dmesg spam */
#define NO_AHCI_DEBUG

struct ahci_cmd_hdr {
	u_int16_t		flags;
#define AHCI_CMD_LIST_FLAG_CFL		0x001f /* Command FIS Length */
#define AHCI_CMD_LIST_FLAG_A		(1<<5) /* ATAPI */
#define AHCI_CMD_LIST_FLAG_W		(1<<6) /* Write */
#define AHCI_CMD_LIST_FLAG_P		(1<<7) /* Prefetchable */
#define AHCI_CMD_LIST_FLAG_R		(1<<8) /* Reset */
#define AHCI_CMD_LIST_FLAG_B		(1<<9) /* BIST */
#define AHCI_CMD_LIST_FLAG_C		(1<<10) /* Clear Busy upon R_OK */
#define AHCI_CMD_LIST_FLAG_PMP		0xf000 /* Port Multiplier Port */
#define AHCI_CMD_LIST_FLAG_PMP_SHIFT	12
	u_int16_t		prdtl; /* sgl len */

	u_int32_t		prdbc; /* transferred byte count */

	u_int32_t		ctba_lo;
	u_int32_t		ctba_hi;

	u_int32_t		reserved[4];
} __packed;

struct ahci_rfis {
	u_int8_t		dsfis[28];
	u_int8_t		reserved1[4];
	u_int8_t		psfis[24];
	u_int8_t		reserved2[8];
	u_int8_t		rfis[24];
	u_int8_t		reserved3[4];
	u_int8_t		sdbfis[4];
	u_int8_t		ufis[64];
	u_int8_t		reserved4[96];
} __packed;

struct ahci_prdt {
	u_int64_t		dba;
	u_int32_t		reserved;
	u_int32_t		flags;
#define AHCI_PRDT_FLAG_INTR		(1<<31) /* interrupt on completion */
} __packed __aligned(8);

/* this makes ahci_cmd_table 512 bytes, supporting 128-byte alignment */
#define AHCI_MAX_PRDT		24

struct ahci_cmd_table {
	u_int8_t		cfis[64];	/* Command FIS */
	u_int8_t		acmd[16];	/* ATAPI Command */
	u_int8_t		reserved[48];

	struct ahci_prdt	prdt[AHCI_MAX_PRDT];
} __packed;

#define AHCI_MAX_PORTS		32

struct ahci_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};
#define AHCI_DMA_MAP(_adm)	((_adm)->adm_map)
#define AHCI_DMA_DVA(_adm)	((u_int64_t)(_adm)->adm_map->dm_segs[0].ds_addr)
#define AHCI_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct ahci_softc;
struct ahci_port;

struct ahci_ccb {
	/* ATA xfer associated with this CCB.  Must be 1st struct member. */
	struct ata_xfer		ccb_xa;

	int			ccb_slot;
	struct ahci_port	*ccb_port;

	bus_dmamap_t		ccb_dmamap;
	struct ahci_cmd_hdr	*ccb_cmd_hdr;
	struct ahci_cmd_table	*ccb_cmd_table;

	void			(*ccb_done)(struct ahci_ccb *);

	TAILQ_ENTRY(ahci_ccb)	ccb_entry;
};

struct ahci_port {
	struct ahci_softc	*ap_sc;
	bus_space_handle_t	ap_ioh;

#ifdef AHCI_COALESCE
	int			ap_num;
#endif

	struct ahci_rfis	*ap_rfis;
	struct ahci_dmamem	*ap_dmamem_rfis;

	struct ahci_dmamem	*ap_dmamem_cmd_list;
	struct ahci_dmamem	*ap_dmamem_cmd_table;

	volatile u_int32_t	ap_active;
	volatile u_int32_t	ap_active_cnt;
	volatile u_int32_t	ap_sactive;
	volatile u_int32_t	ap_pmp_ncq_port;
	struct ahci_ccb		*ap_ccbs;

	TAILQ_HEAD(, ahci_ccb)	ap_ccb_free;
	TAILQ_HEAD(, ahci_ccb)	ap_ccb_pending;
	struct mutex		ap_ccb_mtx;
	struct ahci_ccb		*ap_ccb_err;

	u_int32_t		ap_state;
#define AP_S_NORMAL			0
#define AP_S_PMP_PROBE			1
#define AP_S_PMP_PORT_PROBE		2
#define AP_S_ERROR_RECOVERY		3
#define AP_S_FATAL_ERROR		4

	int			ap_pmp_ports;
	int			ap_port;
	int			ap_pmp_ignore_ifs;

	/* For error recovery. */
#ifdef DIAGNOSTIC
	int			ap_err_busy;
#endif
	u_int32_t		ap_err_saved_sactive;
	u_int32_t		ap_err_saved_active;
	u_int32_t		ap_err_saved_active_cnt;

	u_int8_t		*ap_err_scratch;

#ifdef AHCI_DEBUG
	char			ap_name[16];
#define PORTNAME(_ap)	((_ap)->ap_name)
#else
#define PORTNAME(_ap)	DEVNAME((_ap)->ap_sc)
#endif
};

struct ahci_softc {
	struct device		sc_dev;

	void			*sc_ih;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	int			sc_flags;
#define AHCI_F_NO_NCQ			(1<<0)
#define AHCI_F_IPMS_PROBE		(1<<1)	/* IPMS on failed PMP probe */
#define AHCI_F_NO_PMP			(1<<2)	/* ignore PMP capability */
#define AHCI_F_NO_MSI			(1<<3)	/* disable MSI */

	u_int			sc_ncmds;

	struct ahci_port	*sc_ports[AHCI_MAX_PORTS];

	struct atascsi		*sc_atascsi;

	u_int32_t		sc_cap;

#ifdef AHCI_COALESCE
	u_int32_t		sc_ccc_mask;
	u_int32_t		sc_ccc_ports;
	u_int32_t		sc_ccc_ports_cur;
#endif
};

#define DEVNAME(_s)		((_s)->sc_dev.dv_xname)

int			ahci_attach(struct ahci_softc *);
int			ahci_detach(struct ahci_softc *, int);
int			ahci_activate(struct device *, int);

int			ahci_intr(void *);
