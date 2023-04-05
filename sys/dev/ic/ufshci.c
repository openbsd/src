/*	$OpenBSD: ufshci.c,v 1.3 2023/04/05 17:23:30 mglocker Exp $ */

/*
 * Copyright (c) 2022 Marcus Glocker <mglocker@openbsd.org>
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
 * Universal Flash Storage Host Controller Interface (UFSHCI) 2.1 driver based
 * on the JEDEC JESD223C.pdf and JESD220C-2_1.pdf specifications.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/pool.h>

#include <sys/atomic.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/ufshcivar.h>
#include <dev/ic/ufshcireg.h>

//#define UFSHCI_DEBUG 1

#ifdef UFSHCI_DEBUG
int ufshci_dbglvl = 1;
#define DPRINTF(x...)	do { printf(x); } while (0)
#else
#define DPRINTF(x...)
#endif

struct cfdriver ufshci_cd = {
	NULL, "ufshci", DV_DULL
};

int			 ufshci_reset(struct ufshci_softc *);
int			 ufshci_uccs_poll(struct ufshci_softc *);
struct ufshci_dmamem	*ufshci_dmamem_alloc(struct ufshci_softc *, size_t);
void			 ufshci_dmamem_free(struct ufshci_softc *,
			     struct ufshci_dmamem *);
int			 ufshci_init(struct ufshci_softc *);
int			 ufshci_doorbell_get_free(struct ufshci_softc *);
int			 ufshci_doorbell_read(struct ufshci_softc *);
int			 ufshci_doorbell_poll(struct ufshci_softc *, int);
void			 ufshci_doorbell_set(struct ufshci_softc *, int);
uint8_t			 ufshci_get_taskid(struct ufshci_softc *);
int			 ufshci_utr_cmd_nop(struct ufshci_softc *);
int			 ufshci_utr_cmd_lun(struct ufshci_softc *,
			     struct ufshci_ccb *, int);
int			 ufshci_utr_cmd_inquiry(struct ufshci_softc *,
			     struct ufshci_ccb *, int, int);
int			 ufshci_utr_cmd_capacity16(struct ufshci_softc *,
			     struct ufshci_ccb *, int, int);
int			 ufshci_utr_cmd_capacity(struct ufshci_softc *,
			     struct ufshci_ccb *, int, int);
int			 ufshci_utr_cmd_read(struct ufshci_softc *,
			     struct ufshci_ccb *, int, int, uint32_t, uint16_t);
int			 ufshci_utr_cmd_write(struct ufshci_softc *,
			     struct ufshci_ccb *, int, int, uint32_t, uint16_t);
int			 ufshci_utr_cmd_sync(struct ufshci_softc *,
			     struct ufshci_ccb *, int, uint32_t, uint16_t);
int			 ufshci_xfer_complete(struct ufshci_softc *);
void			 ufshci_hexdump(void *, int, char *, int);

/* SCSI */
int			 ufshci_ccb_alloc(struct ufshci_softc *, int);
void			*ufshci_ccb_get(void *);
void			 ufshci_ccb_put(void *, void *);
void			 ufshci_ccb_free(struct ufshci_softc*, int);

void			 ufshci_scsi_cmd(struct scsi_xfer *);
void			 ufshci_minphys(struct buf *, struct scsi_link *);
int			 ufshci_scsi_probe(struct scsi_link *);
void			 ufshci_scsi_free(struct scsi_link *);

void			 ufshci_scsi_inquiry(struct scsi_xfer *);
void			 ufshci_scsi_capacity16(struct scsi_xfer *);
void			 ufshci_scsi_capacity(struct scsi_xfer *);
void			 ufshci_scsi_sync(struct scsi_xfer *);
void			 ufshci_scsi_io(struct scsi_xfer *, int);
void			 ufshci_scsi_io_done(struct ufshci_softc *,
			     struct ufshci_ccb *);
void			 ufshci_scsi_done(struct ufshci_softc *,
			     struct ufshci_ccb *);

#if 0
const struct scsi_adapter ufshci_switch = {
	ufshci_scsi_cmd, ufshci_minphys, ufshci_scsi_probe, ufshci_scsi_free,
	NULL
};
#endif
const struct scsi_adapter ufshci_switch = {
	ufshci_scsi_cmd, NULL, NULL, NULL, NULL
};

int
ufshci_intr(void *arg)
{
	struct ufshci_softc *sc = arg;
	uint32_t status;
	int handled = 0;

	status = UFSHCI_READ_4(sc, UFSHCI_REG_IS);
	DPRINTF("%s: status=0x%08x\n", __func__, status);

	if (status == 0)
		return 0;

	if (status & UFSHCI_REG_IS_UCCS) {
		DPRINTF("%s: UCCS interrupt\n", __func__);
		handled = 1;
	}
	if (status & UFSHCI_REG_IS_UTRCS) {
	  	DPRINTF("%s: UTRCS interrupt\n", __func__);

		ufshci_xfer_complete(sc);

		/* Reset Interrupt Aggregation Counter and Timer. */
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN | UFSHCI_REG_UTRIACR_CTR);

		handled = 1;
	}

	if (handled == 0) {
		printf("%s: UNKNOWN interrupt, status=0x%08x\n",
		    sc->sc_dev.dv_xname, status);
	}

	/* ACK interrupt */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_IS, status);

	return 1;
}

/* XXX: Only for testing */
void
ufshci_attach_hook(struct device *self)
{
	struct ufshci_softc *sc = (struct ufshci_softc *)self;

	ufshci_attach(sc);
}

int
ufshci_attach(struct ufshci_softc *sc)
{
	struct scsibus_attach_args saa;

	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	SIMPLEQ_INIT(&sc->sc_ccb_list);
	scsi_iopool_init(&sc->sc_iopool, sc, ufshci_ccb_get, ufshci_ccb_put);

	ufshci_reset(sc);

	sc->sc_ver = UFSHCI_READ_4(sc, UFSHCI_REG_VER);
	printf(", UFSHCI %d.%d%d\n",
	    UFSHCI_REG_VER_MAJOR(sc->sc_ver),
	    UFSHCI_REG_VER_MINOR(sc->sc_ver),
	    UFSHCI_REG_VER_SUFFIX(sc->sc_ver));

	sc->sc_cap = UFSHCI_READ_4(sc, UFSHCI_REG_CAP);
	sc->sc_hcpid = UFSHCI_READ_4(sc, UFSHCI_REG_HCPID);
	sc->sc_hcmid = UFSHCI_READ_4(sc, UFSHCI_REG_HCMID);
	sc->sc_nutmrs = UFSHCI_REG_CAP_NUTMRS(sc->sc_cap) + 1;
	sc->sc_rtt = UFSHCI_REG_CAP_RTT(sc->sc_cap) + 1;
	sc->sc_nutrs = UFSHCI_REG_CAP_NUTRS(sc->sc_cap) + 1;

#if UFSHCI_DEBUG
	printf("Capabilities (0x%08x):\n", sc->sc_cap);
	printf(" CS=%d\n", sc->sc_cap & UFSHCI_REG_CAP_CS ? 1 : 0);
	printf(" UICDMETMS=%d\n",
	    sc->sc_cap & UFSHCI_REG_CAP_UICDMETMS ? 1 : 0);
	printf(" OODDS=%d\n", sc->sc_cap & UFSHCI_REG_CAP_OODDS ? 1 : 0);
	printf(" 64AS=%d\n", sc->sc_cap & UFSHCI_REG_CAP_64AS ? 1 : 0);
	printf(" AUTOH8=%d\n", sc->sc_cap & UFSHCI_REG_AUTOH8 ? 1 : 0);
	printf(" NUTMRS=%d\n", sc->sc_nutmrs);
	printf(" RTT=%d\n", sc->sc_rtt);
	printf(" NUTRS=%d\n", sc->sc_nutrs);
	printf("HCPID=0x%08x:\n", sc->sc_hcpid);
	printf("HCMID (0x%08x):\n", sc->sc_hcmid);
	printf(" BI=0x%04x\n", UFSHCI_REG_HCMID_BI(sc->sc_hcmid));
	printf(" MIC=0x%04x\n", UFSHCI_REG_HCMID_MIC(sc->sc_hcmid));
#endif

	if (sc->sc_nutrs > 32) {
		printf("%s: NUTRS can't be >32 (is %d)!\n",
		    sc->sc_dev.dv_xname, sc->sc_nutrs);
		return 1;
	}

	ufshci_init(sc);

	if (ufshci_ccb_alloc(sc, sc->sc_nutrs) != 0) {
		printf("%s: %s: Can't allocate CCBs\n",
		    sc->sc_dev.dv_xname, __func__);
		return 1;
	}

	/* Attach to SCSI layer */
	saa.saa_adapter = &ufshci_switch;
	saa.saa_adapter_softc = sc;
	saa.saa_adapter_buswidth = 2; /* XXX: What's the right value? */
	saa.saa_luns = 1; /* XXX: Should we use ufshci_utr_cmd_lun() */
	saa.saa_adapter_target = 0;
	saa.saa_openings = sc->sc_nutrs;
	saa.saa_pool = &sc->sc_iopool;
	saa.saa_quirks = saa.saa_flags = 0;
	saa.saa_wwpn = saa.saa_wwnn = 0;

	config_found(&sc->sc_dev, &saa, scsiprint);

	return 0;
}

int
ufshci_reset(struct ufshci_softc *sc)
{
	int i;
	int retry = 10;
	uint32_t hce;

	/*
	 * 7.1.1 Host Controller Initialization: 2)
	 * Reset and enable host controller
	 */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_HCE, UFSHCI_REG_HCE_HCE);
	/* 7.1.1 Host Controller Initialization: 3) */
	for (i = 0; i < retry; i++) {
		hce = UFSHCI_READ_4(sc, UFSHCI_REG_HCE);
		if (hce == 1)
			break;
		delay(1);
	}
	if (i == retry) {
		printf("%s: Enabling Host Controller failed!\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}

	DPRINTF("\n%s: Host Controller enabled (i=%d)\n", __func__, i);

	return 0;
}

int
ufshci_uccs_poll(struct ufshci_softc *sc)
{
	uint32_t status;
	int i, retry = 25;

	DPRINTF("%s\n", __func__);

	for (i = 0; i < retry; i++) {
		status = UFSHCI_READ_4(sc, UFSHCI_REG_IS);
		if (status & UFSHCI_REG_IS_UCCS)
			break;
		delay(10);
	}
	if (i == retry) {
		printf("%s: %s: timeout\n", sc->sc_dev.dv_xname, __func__);
		return -1;
	}
	DPRINTF("%s: completed after %d retries\n", __func__, i);

	/* ACK interrupt */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_IS, status);

	return 0;
}

struct ufshci_dmamem *
ufshci_dmamem_alloc(struct ufshci_softc *sc, size_t size)
{
	struct ufshci_dmamem *udm;
	int nsegs;

	udm = malloc(sizeof(*udm), M_DEVBUF, M_WAITOK | M_ZERO);
	if (udm == NULL)
		return NULL;

	udm->udm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &udm->udm_map) != 0)
		goto udmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &udm->udm_seg,
	    1, &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &udm->udm_seg, nsegs, size,
	    &udm->udm_kva, BUS_DMA_WAITOK) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, udm->udm_map, udm->udm_kva, size,
	    NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	DPRINTF("%s: size=%lu, page_size=%d, nsegs=%d\n",
	    __func__, size, PAGE_SIZE, nsegs);

	return udm;

unmap:
	bus_dmamem_unmap(sc->sc_dmat, udm->udm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &udm->udm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, udm->udm_map);
udmfree:
	free(udm, M_DEVBUF, sizeof(*udm));

	return NULL;
}

void
ufshci_dmamem_free(struct ufshci_softc *sc, struct ufshci_dmamem *udm)
{
	bus_dmamap_unload(sc->sc_dmat, udm->udm_map);
	bus_dmamem_unmap(sc->sc_dmat, udm->udm_kva, udm->udm_size);
	bus_dmamem_free(sc->sc_dmat, &udm->udm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, udm->udm_map);
	free(udm, M_DEVBUF, sizeof(*udm));
}

int
ufshci_init(struct ufshci_softc *sc)
{
	uint32_t reg;
	uint64_t dva;

	/*
	 * 7.1.1 Host Controller Initialization: 4)
	 * TODO: Do we need to set DME_SET?
	 */

	/* 7.1.1 Host Controller Initialization: 5) */
	//UFSHCI_WRITE_4(sc, UFSHCI_REG_IE, UFSHCI_REG_IE_UCCE |
	UFSHCI_WRITE_4(sc, UFSHCI_REG_IE,
	    UFSHCI_REG_IE_UTRCE | UFSHCI_REG_IE_UTMRCE);

	/* 7.1.1 Host Controller Initialization: 6) */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UICCMD,
	    UFSHCI_REG_UICCMD_CMDOP_DME_LINKSTARTUP);
	if (ufshci_uccs_poll(sc) != 0)
		return -1;

	/*
	 * 7.1.1 Host Controller Initialization: 7), 8), 9)
	 * TODO: Implement retry in case UFSHCI_REG_HCS returns 0
	 */
	reg = UFSHCI_READ_4(sc, UFSHCI_REG_HCS);
	if (reg & UFSHCI_REG_HCS_DP)
		DPRINTF("%s: Device Presence SET\n", __func__);
	else
		DPRINTF("%s: Device Presence NOT SET\n", __func__);

	/*
	 * 7.1.1 Host Controller Initialization: 10)
	 * TODO: Enable additional interrupt on the IE register
	 */

	/* 7.1.1 Host Controller Initialization: 11) */
	reg = UFSHCI_READ_4(sc, UFSHCI_REG_UTRIACR);
	DPRINTF("%s: UTRIACR=0x%08x\n", __func__, reg);
	/*
	 * Only enable interrupt aggregation when interrupts are available.
	 * Otherwise, the interrupt aggregation counter already starts to
	 * count completed commands, and will keep interrupts disabled once
	 * reaching the threshold.  We only issue the interrupt aggregation
	 * counter reset in the interrupt handler during runtime, so we would
	 * have a kind of chicken/egg problem.
	 */
	if (!cold) {
		DPRINTF("%s: Enable interrupt aggregation\n", __func__);
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_CTR |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
		sc->sc_intraggr_enabled = 1;
	}

	/*
	 * 7.1.1 Host Controller Initialization: 12)
	 * TODO: More UIC commands to issue?
	 */

	/* 7.1.1 Host Controller Initialization: 13) */
	sc->sc_dmamem_utmrd = ufshci_dmamem_alloc(sc,
	    sizeof(struct ufshci_utmrd) * sc->sc_nutmrs);
	if (sc->sc_dmamem_utmrd == NULL) {
		printf("%s: Can't allocate DMA memory for UTMRD\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}
	/* 7.1.1 Host Controller Initialization: 14) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_utmrd);
	DPRINTF("%s: utmrd dva=%llu\n", __func__, dva);
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTMRLBA, (uint32_t)dva);
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTMRLBAU, (uint32_t)(dva >> 32));

	/* 7.1.1 Host Controller Initialization: 15) */
	sc->sc_dmamem_utrd = ufshci_dmamem_alloc(sc,
	    sizeof(struct ufshci_utrd) * sc->sc_nutrs);
	if (sc->sc_dmamem_utrd == NULL) {
		printf("%s: Can't allocate DMA memory for UTRD\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}
	/* 7.1.1 Host Controller Initialization: 16) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_utrd);
	DPRINTF("%s: utrd dva=%llu\n", __func__, dva);
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRLBA, (uint32_t)dva);
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRLBAU, (uint32_t)(dva >> 32));


	/* Allocate UCDs. */
	sc->sc_dmamem_ucd = ufshci_dmamem_alloc(sc,
	    sizeof(struct ufshci_ucd) * sc->sc_nutrs);
	if (sc->sc_dmamem_ucd == NULL) {
		printf("%s: Can't allocate DMA memory for UCD\n",
		    sc->sc_dev.dv_xname);
		return -1;
	}

	/* 7.1.1 Host Controller Initialization: 17) */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTMRLRSR, UFSHCI_REG_UTMRLRSR_START);

	/* 7.1.1 Host Controller Initialization: 18) */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRLRSR, UFSHCI_REG_UTRLRSR_START);

	/* 7.1.1 Host Controller Initialization: 19) */
	/* TODO: bMaxNumOfRTT will be set as the minimum value of
	 * bDeviceRTTCap and NORTT. ???
	 */

	return 0;
}

int
ufshci_doorbell_get_free(struct ufshci_softc *sc)
{
	int slot;
	uint32_t reg;

	reg = UFSHCI_READ_4(sc, UFSHCI_REG_UTRLDBR);

	for (slot = 0; slot < sc->sc_nutrs; slot++) {
		if ((reg & (1 << slot)) == 0)
			return slot;
	}

	return -1;
}

int
ufshci_doorbell_read(struct ufshci_softc *sc)
{
	uint32_t reg;

	reg = UFSHCI_READ_4(sc, UFSHCI_REG_UTRLDBR);

	return reg;
}

int
ufshci_doorbell_poll(struct ufshci_softc *sc, int slot)
{
	uint32_t reg;
	int i, retry = 25;

	DPRINTF("%s\n", __func__);

	for (i = 0; i < retry; i++) {
		reg = UFSHCI_READ_4(sc, UFSHCI_REG_UTRLDBR);
		if ((reg & (1 << slot)) == 0)
			break;
		delay(10);
	}
	if (i == retry) {
		printf("%s: %s: timeout\n", sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	return 0;
}

void
ufshci_doorbell_set(struct ufshci_softc *sc, int slot)
{
	uint32_t reg;

	reg = (1 << slot);

	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRLDBR, reg);
}

uint8_t
ufshci_get_taskid(struct ufshci_softc *sc)
{
	if (sc->sc_taskid == 255)
		sc->sc_taskid = 0;
	else
		sc->sc_taskid++;

	return sc->sc_taskid;
}

int
ufshci_utr_cmd_nop(struct ufshci_softc *sc)
{
	int slot, off, len;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_NO;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_INT;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_NOP_OUT;
	ucd->cmd.hdr.flags = 0;
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(0); /* No data xfer */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
	    UFSHCI_REG_UTRIACR_IAEN |
	    UFSHCI_REG_UTRIACR_IAPWEN |
	    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
	    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return 0;
}

int
ufshci_utr_cmd_lun(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_T2I;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 6); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = REPORT_LUNS;
	ucd->cmd.cdb[6] = 0;
	ucd->cmd.cdb[7] = 0;
	ucd->cmd.cdb[8] = 0;
	ucd->cmd.cdb[9] = rsp_size;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
	    UFSHCI_REG_UTRIACR_IAEN |
	    UFSHCI_REG_UTRIACR_IAPWEN |
	    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
	    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return 0;
}

int
ufshci_utr_cmd_inquiry(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size, int flags)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_T2I;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 6); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = INQUIRY; /* 0x12 */
	ucd->cmd.cdb[3] = 0;
	ucd->cmd.cdb[4] = rsp_size;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_utr_cmd_capacity16(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size, int flags)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_T2I;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 6); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = READ_CAPACITY_16; /* 0x9e */
	ucd->cmd.cdb[1] = 0x10; /* Service Action */
	/* Logical Block Address = 0 for UFS */
	ucd->cmd.cdb[10] = 0;
	ucd->cmd.cdb[11] = 0;
	ucd->cmd.cdb[12] = 0;
	ucd->cmd.cdb[13] = rsp_size;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_utr_cmd_capacity(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size, int flags)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_T2I;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 6); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = READ_CAPACITY; /* 0x25 */
	/* Logical Block Address = 0 for UFS */
	ucd->cmd.cdb[2] = 0;
	ucd->cmd.cdb[3] = 0;
	ucd->cmd.cdb[4] = 0;
	ucd->cmd.cdb[5] = 0;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_utr_cmd_read(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size, int flags, uint32_t lba, uint16_t blocks)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_T2I;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 6); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = READ_10; /* 0x28 */
	//ucd->cmd.cdb[1] = (1 << 3); /* FUA: Force Unit Access */
	ucd->cmd.cdb[2] = (lba >> 24) & 0xff;
	ucd->cmd.cdb[3] = (lba >> 16) & 0xff;
	ucd->cmd.cdb[4] = (lba >>  8) & 0xff;
	ucd->cmd.cdb[5] = (lba >>  0) & 0xff;
	ucd->cmd.cdb[7] = (blocks >> 8) & 0xff;
	ucd->cmd.cdb[8] = (blocks >> 0) & 0xff;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_utr_cmd_write(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int rsp_size, int flags, uint32_t lba, uint16_t blocks)
{
	int slot, off, len, i;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	bus_dmamap_t dmap = ccb->ccb_dmamap;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_I2T;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = (1 << 5); /* Bit-5 = Write, Bit-6 = Read */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(rsp_size);

	ucd->cmd.cdb[0] = WRITE_10; /* 0x2a */
	ucd->cmd.cdb[1] = (1 << 3); /* FUA: Force Unit Access */
	ucd->cmd.cdb[2] = (lba >> 24) & 0xff;
	ucd->cmd.cdb[3] = (lba >> 16) & 0xff;
	ucd->cmd.cdb[4] = (lba >>  8) & 0xff;
	ucd->cmd.cdb[5] = (lba >>  0) & 0xff;
	ucd->cmd.cdb[7] = (blocks >> 8) & 0xff;
	ucd->cmd.cdb[8] = (blocks >> 0) & 0xff;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(dmap->dm_nsegs);

	/* Build PRDT data segment. */
	for (i = 0; i < dmap->dm_nsegs; i++) {
		dva = dmap->dm_segs[i].ds_addr;
		ucd->prdt[i].dw0 = (uint32_t)dva;
		ucd->prdt[i].dw1 = (uint32_t)(dva >> 32);
		ucd->prdt[i].dw2 = 0;
		ucd->prdt[i].dw3 = dmap->dm_segs[i].ds_len - 1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_COUNT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_utr_cmd_sync(struct ufshci_softc *sc, struct ufshci_ccb *ccb,
    int flags, uint32_t lba, uint16_t blocks)
{
	int slot, off, len;
	uint64_t dva;
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 1) */
	slot = ufshci_doorbell_get_free(sc);
	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	memset(utrd, 0, sizeof(*utrd));
	DPRINTF("%s: slot=%d\n", __func__, slot);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2a) */
	utrd->dw0 = UFSHCI_UTRD_DW0_CT_UFS;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2b) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_DD_I2T;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2c) */
	utrd->dw0 |= UFSHCI_UTRD_DW0_I_REG;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2d) */
	utrd->dw2 = UFSHCI_UTRD_DW2_OCS_IOV;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2e) */
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	memset(ucd, 0, sizeof(*ucd));

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2f) */
	ucd->cmd.hdr.tc = UPIU_TC_I2T_COMMAND;
	ucd->cmd.hdr.flags = 0; /* No data transfer */
	ucd->cmd.hdr.lun = 0;
	ucd->cmd.hdr.taskid = ufshci_get_taskid(sc);
	ucd->cmd.hdr.cmd_set_type = 0; /* SCSI command */
	ucd->cmd.hdr.query = 0;
	ucd->cmd.hdr.response = 0;
	ucd->cmd.hdr.status = 0;
	ucd->cmd.hdr.ehs_len = 0;
	ucd->cmd.hdr.device_info = 0;
	ucd->cmd.hdr.ds_len = 0;

	ucd->cmd.expected_xfer_len = htobe32(0); /* No data transfer */

	ucd->cmd.cdb[0] = SYNCHRONIZE_CACHE; /* 0x35 */
	ucd->cmd.cdb[2] = (lba >> 24) & 0xff;
	ucd->cmd.cdb[3] = (lba >> 16) & 0xff;
	ucd->cmd.cdb[4] = (lba >>  8) & 0xff;
	ucd->cmd.cdb[5] = (lba >>  0) & 0xff;
	ucd->cmd.cdb[7] = (blocks >> 8) & 0xff;
	ucd->cmd.cdb[8] = (blocks >> 0) & 0xff;

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 2g) */
	/* Already done with above memset */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 3) */
	dva = UFSHCI_DMA_DVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);
	DPRINTF("%s: ucd dva=%llu\n", __func__, dva);
	utrd->dw4 = (uint32_t)dva;
	utrd->dw5 = (uint32_t)(dva >> 32);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 4) */
	off = sizeof(struct upiu_command) / 4; /* DWORD offset */
	utrd->dw6 = UFSHCI_UTRD_DW6_RUO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 5) */
	len = sizeof(struct upiu_response) / 4; /* DWORD length */
	utrd->dw6 |= UFSHCI_UTRD_DW6_RUL(len);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 6) */
	off = (sizeof(struct upiu_command) + sizeof(struct upiu_response)) / 4;
	utrd->dw7 = UFSHCI_UTRD_DW7_PRDTO(off);

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 7) */
	utrd->dw7 |= UFSHCI_UTRD_DW7_PRDTL(0); /* No data xfer */

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 9) */
	if (UFSHCI_READ_4(sc, UFSHCI_REG_UTRLRSR) != 1) {
		printf("%s: %s: UTRLRSR not set\n",
		    sc->sc_dev.dv_xname, __func__);
		return -1;
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 10) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 11) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 12) */
	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 13) */
	if (!ISSET(flags, SCSI_POLL)) {
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
	}

	/* 7.2.1 Basic Steps when Building a UTP Transfer Request: 14) */
	ufshci_doorbell_set(sc, slot);

	return slot;
}

int
ufshci_xfer_complete(struct ufshci_softc *sc)
{
	struct ufshci_ccb *ccb;
	uint32_t reg;
	int i;

	reg = ufshci_doorbell_read(sc);

	for (i = 0; i < sc->sc_nutrs; i++) {
		ccb = &sc->sc_ccbs[i];

		if (ccb->ccb_slot == -1)
			/* CCB isn't used. */
			continue;

		if (reg & (1 << ccb->ccb_slot))
			/* Transfer is still in progress. */
			continue;

		/* Transfer has completed. */
		if (ccb->ccb_done == NULL)
			panic("ccb_done not defined");
		ccb->ccb_done(sc, ccb);
	}

	return 0;
}

#ifdef UFSHCI_DEBUG
void
ufshci_hexdump(void *buf, int len, char *title, int dbglvl)
{
	u_char b[16];
	int i, j, l;

	if (dbglvl > ufshci_dbglvl)
		return;

	printf("hexdump for %s (size=%d bytes)\n", title, len);

	for (i = 0; i < len; i += l) {
		printf("%4i:", i);
		l = min(sizeof(b), len - i);
		bcopy(buf + i, b, l);

		for (j = 0; j < sizeof(b); j++) {
			if (j % 2 == 0)
				printf(" "); 
			if (j % 8 == 0)
				printf(" ");
			if (j < l)
				printf("%02x", (int)b[j]);
			else
				printf("  ");
		}
		printf("  |");
		for (j = 0; j < l; j++) {
			if (b[j] >= 0x20 && b[j] <= 0x7e)
				printf("%c", b[j]);
			else
				printf(".");
		}
		printf("|\n");
	}
}
#else
void
ufshci_hexdump(void *buf, int len, char *title, int dbglvl)
{

}
#endif

/* SCSI */

int
ufshci_ccb_alloc(struct ufshci_softc *sc, int nccbs)
{
	struct ufshci_ccb *ccb;
	int i;

	DPRINTF("%s: nccbs=%d, dma_size=%d, dma_nsegs=%d, "
	    "dma_segmaxsize=%d\n",
	    __func__, nccbs, UFSHCI_UCD_PRDT_MAX_XFER, UFSHCI_UCD_PRDT_MAX_SEGS,
	    UFSHCI_UCD_PRDT_MAX_XFER);

	sc->sc_ccbs = mallocarray(nccbs, sizeof(*ccb), M_DEVBUF,
	    M_WAITOK | M_CANFAIL);
	if (sc->sc_ccbs == NULL)
		return 1;

	for (i = 0; i < nccbs; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, UFSHCI_UCD_PRDT_MAX_XFER,
		    UFSHCI_UCD_PRDT_MAX_SEGS, UFSHCI_UCD_PRDT_MAX_XFER, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &ccb->ccb_dmamap) != 0)
			goto free_maps;

		ccb->ccb_cookie = NULL;
		ccb->ccb_slot = -1;

		SIMPLEQ_INSERT_TAIL(&sc->sc_ccb_list, ccb, ccb_entry);
	}

	return 0;

free_maps:
	ufshci_ccb_free(sc, nccbs);
	return 1;
}

void *
ufshci_ccb_get(void *cookie)
{
	struct ufshci_softc *sc = cookie;
	struct ufshci_ccb *ccb;

	DPRINTF("%s\n", __func__);

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = SIMPLEQ_FIRST(&sc->sc_ccb_list);
	if (ccb != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_list, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);

	return ccb;
}

void
ufshci_ccb_put(void *cookie, void *io)
{
	struct ufshci_softc *sc = cookie;
	struct ufshci_ccb *ccb = io;

	DPRINTF("%s\n", __func__);

	mtx_enter(&sc->sc_ccb_mtx);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_list, ccb, ccb_entry);
	mtx_leave(&sc->sc_ccb_mtx);
}

void
ufshci_ccb_free(struct ufshci_softc *sc, int nccbs)
{
	struct ufshci_ccb *ccb;

	DPRINTF("%s\n", __func__);

	while ((ccb = SIMPLEQ_FIRST(&sc->sc_ccb_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_list, ccb_entry);
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	}

	ufshci_dmamem_free(sc, sc->sc_dmamem_utrd);
	free(sc->sc_ccbs, M_DEVBUF, nccbs * sizeof(*ccb));
}

void
ufshci_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;

	DPRINTF("%s: cmd=0x%x\n", __func__, xs->cmd.opcode);

	if (!cold && !sc->sc_intraggr_enabled) {
		DPRINTF("%s: Enable interrupt aggregation\n", __func__);
		UFSHCI_WRITE_4(sc, UFSHCI_REG_UTRIACR,
		    UFSHCI_REG_UTRIACR_IAEN |
		    UFSHCI_REG_UTRIACR_IAPWEN |
		    UFSHCI_REG_UTRIACR_CTR |
		    UFSHCI_REG_UTRIACR_IACTH(UFSHCI_INTR_AGGR_COUNT) |
		    UFSHCI_REG_UTRIACR_IATOVAL(UFSHCI_INTR_AGGR_TIMEOUT));
		sc->sc_intraggr_enabled = 1;
	}

	switch (xs->cmd.opcode) {

	case READ_COMMAND:
	case READ_10:
	case READ_12:
	case READ_16:
		ufshci_scsi_io(xs, SCSI_DATA_IN);
		return;

	case WRITE_COMMAND:
	case WRITE_10:
	case WRITE_12:
	case WRITE_16:
		ufshci_scsi_io(xs, SCSI_DATA_OUT);
		return;

	case SYNCHRONIZE_CACHE:
		ufshci_scsi_sync(xs);
		return;

	case INQUIRY:
		ufshci_scsi_inquiry(xs);
		return;

	case READ_CAPACITY_16:
		ufshci_scsi_capacity16(xs);
		return;
	case READ_CAPACITY:
		ufshci_scsi_capacity(xs);
		return;

	case TEST_UNIT_READY:
	case PREVENT_ALLOW:
	case START_STOP:
		xs->error = XS_NOERROR;
		scsi_done(xs);
		return;
	default:
		DPRINTF("%s: unhandled scsi command 0x%02x\n",
		    __func__, xs->cmd.opcode);
		break;
	}

	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_minphys(struct buf *bp, struct scsi_link *link)
{
	DPRINTF("%s\n", __func__);
}

int
ufshci_scsi_probe(struct scsi_link *link)
{
	DPRINTF("%s\n", __func__);

	return 0;
}

void
ufshci_scsi_free(struct scsi_link *link)
{
	DPRINTF("%s\n", __func__);
}

void
ufshci_scsi_inquiry(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;
	struct ufshci_ccb *ccb = xs->io;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error;

	DPRINTF("%s: INQUIRY (%s)\n",
	    __func__, ISSET(xs->flags, SCSI_POLL) ? "poll"  : "no poll");

	if (xs->datalen > UPIU_SCSI_RSP_INQUIRY_SIZE) {
		DPRINTF("%s: request len too large\n", __func__);
		goto error1;
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap, xs->data, xs->datalen, NULL,
	    ISSET(xs->flags, SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: bus_dmamap_load error=%d\n", __func__, error);
		goto error1;
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	ccb->ccb_cookie = xs;
	ccb->ccb_done = ufshci_scsi_io_done;

	/* Response length should be UPIU_SCSI_RSP_INQUIRY_SIZE. */
	ccb->ccb_slot = ufshci_utr_cmd_inquiry(sc, ccb, xs->datalen, xs->flags);
	if (ccb->ccb_slot == -1)
		goto error2;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (ufshci_doorbell_poll(sc, ccb->ccb_slot) == 0) {
			ccb->ccb_done(sc, ccb);
			return;
		}
		goto error2;
        }

	return;

error2:
	bus_dmamap_unload(sc->sc_dmat, dmap);
	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;
error1:
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_scsi_capacity16(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;
	struct ufshci_ccb *ccb = xs->io;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error;

	DPRINTF("%s: CAPACITY16 (%s)\n",
	    __func__, ISSET(xs->flags, SCSI_POLL) ? "poll"  : "no poll");

	if (xs->datalen > UPIU_SCSI_RSP_CAPACITY16_SIZE) {
		DPRINTF("%s: request len too large\n", __func__);
		goto error1;
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap, xs->data, xs->datalen, NULL,
	    ISSET(xs->flags, SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: bus_dmamap_load error=%d\n", __func__, error);
		goto error1;
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	ccb->ccb_cookie = xs;
	ccb->ccb_done = ufshci_scsi_io_done;

	/* Response length should be UPIU_SCSI_RSP_CAPACITY16_SIZE. */
	ccb->ccb_slot = ufshci_utr_cmd_capacity16(sc, ccb, xs->datalen,
	    xs->flags);
	if (ccb->ccb_slot == -1)
		goto error2;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (ufshci_doorbell_poll(sc, ccb->ccb_slot) == 0) {
			ccb->ccb_done(sc, ccb);
			return;
		}
		goto error2;
	}

	return;

error2:
	bus_dmamap_unload(sc->sc_dmat, dmap);
	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;
error1:
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_scsi_capacity(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;
	struct ufshci_ccb *ccb = xs->io;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error;

	DPRINTF("%s: CAPACITY (%s)\n",
	    __func__, ISSET(xs->flags, SCSI_POLL) ? "poll"  : "no poll");

	if (xs->datalen > UPIU_SCSI_RSP_CAPACITY_SIZE) {
		DPRINTF("%s: request len too large\n", __func__);
		goto error1;
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap, xs->data, xs->datalen, NULL,
	    ISSET(xs->flags, SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: bus_dmamap_load error=%d\n", __func__, error);
		goto error1;
        }

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	ccb->ccb_cookie = xs;
	ccb->ccb_done = ufshci_scsi_io_done;

	/* Response length should be UPIU_SCSI_RSP_CAPACITY_SIZE */
	ccb->ccb_slot = ufshci_utr_cmd_capacity(sc, ccb, xs->datalen,
	    xs->flags);
	if (ccb->ccb_slot == -1)
		goto error2;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (ufshci_doorbell_poll(sc, ccb->ccb_slot) == 0) {
			ccb->ccb_done(sc, ccb);
			return;
		}
		goto error2;
	}
 
	return;

error2:
	bus_dmamap_unload(sc->sc_dmat, dmap);
	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;
error1:
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_scsi_sync(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;
	struct ufshci_ccb *ccb = xs->io;
	uint64_t lba;
	uint32_t blocks;

	/* lba = 0, blocks = 0: Synchronize all logical blocks. */
	lba = 0; blocks = 0;

	DPRINTF("%s: SYNC, lba=%llu, blocks=%u (%s)\n",
	    __func__, lba, blocks,
	    ISSET(xs->flags, SCSI_POLL) ? "poll"  : "no poll");

	ccb->ccb_cookie = xs;
	ccb->ccb_done = ufshci_scsi_done;

	ccb->ccb_slot = ufshci_utr_cmd_sync(sc, ccb, xs->flags, (uint32_t)lba,
	    (uint16_t)blocks);
	if (ccb->ccb_slot == -1)
		goto error;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (ufshci_doorbell_poll(sc, ccb->ccb_slot) == 0) {
			ccb->ccb_done(sc, ccb);
			return;
		}
		goto error;
	}

	return;

error:
        ccb->ccb_cookie = NULL;
        ccb->ccb_slot = -1;
        ccb->ccb_done = NULL;

	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_scsi_io(struct scsi_xfer *xs, int dir)
{
	struct scsi_link *link = xs->sc_link;
	struct ufshci_softc *sc = link->bus->sb_adapter_softc;
	struct ufshci_ccb *ccb = xs->io;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	uint64_t lba;
	uint32_t blocks;
	int error;

	if ((xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) != dir)
		goto error1;

	scsi_cmd_rw_decode(&xs->cmd, &lba, &blocks);

	DPRINTF("%s: %s, lba=%llu, blocks=%u, datalen=%d (%s)\n",
	    __func__,
	    ISSET(xs->flags, SCSI_DATA_IN) ? "READ" : "WRITE",
	    lba, blocks, xs->datalen,
	    ISSET(xs->flags, SCSI_POLL) ? "poll"  : "no poll");

	error = bus_dmamap_load(sc->sc_dmat, dmap, xs->data, xs->datalen, NULL,
	    ISSET(xs->flags, SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error != 0) {
		printf("%s: bus_dmamap_load error=%d\n", __func__, error);
		goto error1;
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    ISSET(xs->flags, SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	ccb->ccb_cookie = xs;
	ccb->ccb_done = ufshci_scsi_io_done;

	if (dir == SCSI_DATA_IN) {
		ccb->ccb_slot = ufshci_utr_cmd_read(sc, ccb, xs->datalen,
		    xs->flags, (uint32_t)lba, (uint16_t)blocks);
	} else {
		ccb->ccb_slot = ufshci_utr_cmd_write(sc, ccb, xs->datalen,
		    xs->flags, (uint32_t)lba, (uint16_t)blocks);
	}

	if (ccb->ccb_slot == -1)
		goto error2;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (ufshci_doorbell_poll(sc, ccb->ccb_slot) == 0) {
			ccb->ccb_done(sc, ccb);
			return;
		}
		goto error2;
	}

	return;

error2:
	bus_dmamap_unload(sc->sc_dmat, dmap);
	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;
error1:
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
ufshci_scsi_io_done(struct ufshci_softc *sc, struct ufshci_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ccb_cookie;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
#if 0
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	int slot = ccb->ccb_slot;
#endif
	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    ISSET(xs->flags, SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
	    BUS_DMASYNC_POSTWRITE);
#if 0
	ufshci_hexdump(xs->data, xs->datalen, "xs->data", 1);

	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);

	printf("ucd rsp tc=0x%02x\n", ucd->rsp.hdr.tc);
	printf("ucd rsp flags=0x%02x\n", ucd->rsp.hdr.flags);
	printf("ucd rsp lun=%d\n", ucd->rsp.hdr.lun);
	printf("ucd rsp taskid=%d\n", ucd->rsp.hdr.taskid);
	printf("ucd rsp cmd_set_type=0x%02x\n", ucd->rsp.hdr.cmd_set_type);
	printf("ucd rsp query=0x%02x\n", ucd->rsp.hdr.query);
	printf("ucd rsp response=0x%02x\n", ucd->rsp.hdr.response);
	printf("ucd rsp status=0x%02x\n", ucd->rsp.hdr.status);
	printf("ucd rsp ehs_len=%d\n", ucd->rsp.hdr.ehs_len);
	printf("ucd rsp device_info=0x%02x\n", ucd->rsp.hdr.device_info);
	printf("ucd rsp ds_len=%d\n", ucd->rsp.hdr.ds_len);
	printf("ucd rsp rxl=%d\n", be32toh(ucd->rsp.residual_xfer_len));
#endif
	bus_dmamap_unload(sc->sc_dmat, dmap);

	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;

	xs->error = XS_NOERROR;
	xs->status = SCSI_OK;
	xs->resid = 0;
	scsi_done(xs);
}

void
ufshci_scsi_done(struct ufshci_softc *sc, struct ufshci_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ccb_cookie;
#if 0
	struct ufshci_utrd *utrd;
	struct ufshci_ucd *ucd;
	int slot = ccb->ccb_slot;

	ufshci_hexdump(xs->data, xs->datalen, "xs->data", 1);

	utrd = UFSHCI_DMA_KVA(sc->sc_dmamem_utrd) + (sizeof(*utrd) * slot);
	ucd = UFSHCI_DMA_KVA(sc->sc_dmamem_ucd) + (sizeof(*ucd) * slot);

	printf("ucd rsp tc=0x%02x\n", ucd->rsp.hdr.tc);
	printf("ucd rsp flags=0x%02x\n", ucd->rsp.hdr.flags);
	printf("ucd rsp lun=%d\n", ucd->rsp.hdr.lun);
	printf("ucd rsp taskid=%d\n", ucd->rsp.hdr.taskid);
	printf("ucd rsp cmd_set_type=0x%02x\n", ucd->rsp.hdr.cmd_set_type);
	printf("ucd rsp query=0x%02x\n", ucd->rsp.hdr.query);
	printf("ucd rsp response=0x%02x\n", ucd->rsp.hdr.response);
	printf("ucd rsp status=0x%02x\n", ucd->rsp.hdr.status);
	printf("ucd rsp ehs_len=%d\n", ucd->rsp.hdr.ehs_len);
	printf("ucd rsp device_info=0x%02x\n", ucd->rsp.hdr.device_info);
	printf("ucd rsp ds_len=%d\n", ucd->rsp.hdr.ds_len);
	printf("ucd rsp rxl=%d\n", be32toh(ucd->rsp.residual_xfer_len));
#endif
	ccb->ccb_cookie = NULL;
	ccb->ccb_slot = -1;
	ccb->ccb_done = NULL;

	xs->error = XS_NOERROR;
	xs->status = SCSI_OK;
	xs->resid = 0;
	scsi_done(xs);
}
