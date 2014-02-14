/*	$OpenBSD: qle.c,v 1.3 2014/02/14 12:04:16 jmatthew Exp $ */

/*
 * Copyright (c) 2013, 2014 Jonathan Matthew <jmatthew@openbsd.org>
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sensors.h>
#include <sys/rwlock.h>
#include <sys/task.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#endif

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/pci/qlereg.h>

/* firmware */
#include <dev/microcode/isp/asm_2400.h>
#include <dev/microcode/isp/asm_2500.h>

#define QLE_PCI_MEM_BAR		0x14
#define QLE_PCI_IO_BAR		0x10


#define QLE_DEFAULT_PORT_NAME		0x400000007F000003ULL /* from isp(4) */

#define QLE_WAIT_FOR_LOOP		10

/* rounded up range of assignable handles */
#define QLE_MAX_TARGETS			2048

/* maximum number of segments allowed for in a single io */
#define QLE_MAX_SEGS			16

enum qle_isp_gen {
	QLE_GEN_ISP24XX = 1,
	QLE_GEN_ISP25XX
};

enum qle_isp_type {
	QLE_ISP2422 = 1,
	QLE_ISP2432,
	QLE_ISP2512,
	QLE_ISP2522,
	QLE_ISP2532
};

/* port database things */
#define QLE_SCRATCH_SIZE		0x1000

enum qle_port_disp {
	QLE_PORT_DISP_NEW,
	QLE_PORT_DISP_GONE,
	QLE_PORT_DISP_SAME,
	QLE_PORT_DISP_CHANGED,
	QLE_PORT_DISP_MOVED,
	QLE_PORT_DISP_DUP
};

#define QLE_LOCATION_LOOP		(1 << 24)
#define QLE_LOCATION_FABRIC		(2 << 24)
#define QLE_LOCATION_LOOP_ID(l)		(l | QLE_LOCATION_LOOP)
#define QLE_LOCATION_PORT_ID(p)		(p | QLE_LOCATION_FABRIC)

struct qle_fc_port {
	TAILQ_ENTRY(qle_fc_port) ports;
	TAILQ_ENTRY(qle_fc_port) update;

	u_int64_t	node_name;
	u_int64_t	port_name;
	u_int32_t	location;	/* port id or loop id */

	int		flags;
#define QLE_PORT_FLAG_IS_TARGET		1
#define QLE_PORT_FLAG_NEEDS_LOGIN	2

	u_int32_t	portid;
	u_int16_t	loopid;
};


/* request/response queue stuff */
#define QLE_QUEUE_ENTRY_SIZE		64

struct qle_ccb {
	struct qle_softc 	*ccb_sc;
	int			ccb_id;
	struct scsi_xfer	*ccb_xs;

	bus_dmamap_t		ccb_dmamap;

	struct qle_iocb_seg	*ccb_segs;
	u_int64_t		ccb_seg_offset;

	SIMPLEQ_ENTRY(qle_ccb)	ccb_link;
};

SIMPLEQ_HEAD(qle_ccb_list, qle_ccb);

struct qle_dmamem {
	bus_dmamap_t		qdm_map;
	bus_dma_segment_t	qdm_seg;
	size_t			qdm_size;
	caddr_t			qdm_kva;
};
#define QLE_DMA_MAP(_qdm)	((_qdm)->qdm_map)
#define QLE_DMA_LEN(_qdm)	((_qdm)->qdm_size)
#define QLE_DMA_DVA(_qdm)	((u_int64_t)(_qdm)->qdm_map->dm_segs[0].ds_addr)
#define QLE_DMA_KVA(_qdm)	((void *)(_qdm)->qdm_kva)

struct qle_softc {
	struct device		sc_dev;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	void			*sc_ih;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	struct scsi_link        sc_link;

	struct scsibus_softc	*sc_scsibus;

	enum qle_isp_type	sc_isp_type;
	enum qle_isp_gen	sc_isp_gen;
	int			sc_port;

	int			sc_mbox_base;
	u_int16_t		sc_mbox[QLE_MBOX_COUNT];
	int			sc_mbox_pending;
	struct mutex		sc_mbox_mtx;

	int			sc_loop_up;
	int			sc_topology;
	int			sc_loop_id;
	int			sc_port_id;
	int			sc_loop_max_id;
	u_int64_t		sc_sns_port_name;

	struct mutex		sc_port_mtx;
	TAILQ_HEAD(, qle_fc_port) sc_ports;
	TAILQ_HEAD(, qle_fc_port) sc_ports_new;
	TAILQ_HEAD(, qle_fc_port) sc_ports_gone;
	TAILQ_HEAD(, qle_fc_port) sc_ports_found;
	struct qle_fc_port	*sc_targets[QLE_MAX_TARGETS];

	struct taskq		*sc_update_taskq;
	struct task		sc_update_task;
	int			sc_update;
	int			sc_update_tasks;
#define	QLE_UPDATE_TASK_CLEAR_ALL	0x00000001
#define QLE_UPDATE_TASK_SOFTRESET	0x00000002
#define QLE_UPDATE_TASK_DETACH_TARGET	0x00000004
#define QLE_UPDATE_TASK_ATTACH_TARGET	0x00000008
#define QLE_UPDATE_TASK_UPDATE_TOPO	0x00000010
#define QLE_UPDATE_TASK_SCAN_LOOP	0x00000020
#define QLE_UPDATE_TASK_SCANNING_LOOP	0x00000040
#define QLE_UPDATE_TASK_SCAN_FABRIC	0x00000080
#define QLE_UPDATE_TASK_SCANNING_FABRIC	0x00000100
#define QLE_UPDATE_TASK_FABRIC_LOGIN	0x00000200
#define QLE_UPDATE_TASK_FABRIC_RELOGIN	0x00000400

	int			sc_maxcmds;
	struct qle_dmamem	*sc_requests;
	struct qle_dmamem	*sc_responses;
	struct qle_dmamem	*sc_segments;
	struct qle_dmamem	*sc_pri_requests;
	struct qle_dmamem	*sc_scratch;
	struct qle_dmamem	*sc_fcp_cmnds;
	struct qle_ccb		*sc_ccbs;
	struct qle_ccb_list	sc_ccb_free;
	struct mutex		sc_ccb_mtx;
	struct mutex		sc_queue_mtx;
	struct scsi_iopool	sc_iopool;
	u_int32_t		sc_next_req_id;
	u_int32_t		sc_last_resp_id;
	int			sc_marker_required;
	int			sc_fabric_pending;

	struct qle_nvram	sc_nvram;
	int			sc_nvram_valid;
};
#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

int	qle_intr(void *);

int	qle_match(struct device *, void *, void *);
void	qle_attach(struct device *, struct device *, void *);
int	qle_detach(struct device *, int);

struct cfattach qle_ca = {
	sizeof(struct qle_softc),
	qle_match,
	qle_attach,
	qle_detach
};

struct cfdriver qle_cd = {
	NULL,
	"qle",
	DV_DULL
};

void		qle_scsi_cmd(struct scsi_xfer *);
struct qle_ccb *qle_scsi_cmd_poll(struct qle_softc *);
int		qle_scsi_probe(struct scsi_link *);


struct scsi_adapter qle_switch = {
	qle_scsi_cmd,
	scsi_minphys,
	qle_scsi_probe,
	NULL,	/* scsi_free */
	NULL	/* ioctl */
};

u_int32_t	qle_read(struct qle_softc *, int);
void		qle_write(struct qle_softc *, int, u_int32_t);
void		qle_host_cmd(struct qle_softc *sc, u_int32_t);

int		qle_mbox(struct qle_softc *, int, int);
int		qle_ct_pass_through(struct qle_softc *sc,
		    u_int32_t port_handle, struct qle_dmamem *mem,
		    size_t req_size, size_t resp_size);
void		qle_mbox_putaddr(u_int16_t *, struct qle_dmamem *);
u_int16_t	qle_read_mbox(struct qle_softc *, int);
void		qle_write_mbox(struct qle_softc *, int, u_int16_t);

void		qle_handle_intr(struct qle_softc *, u_int16_t, u_int16_t);
void		qle_set_ints(struct qle_softc *, int);
int		qle_read_isr(struct qle_softc *, u_int16_t *, u_int16_t *);
void		qle_clear_isr(struct qle_softc *, u_int16_t);

void		qle_put_marker(struct qle_softc *, void *);
void		qle_put_cmd(struct qle_softc *, void *, struct scsi_xfer *,
		    struct qle_ccb *, u_int32_t);
struct qle_ccb *qle_handle_resp(struct qle_softc *, u_int16_t);
void		qle_put_data_seg(struct qle_iocb_seg *, bus_dmamap_t, int);

struct qle_fc_port *qle_next_fabric_port(struct qle_softc *, u_int32_t *,
		    u_int32_t *);
int		qle_get_port_db(struct qle_softc *, u_int16_t,
		    struct qle_dmamem *);
int		qle_add_loop_port(struct qle_softc *, u_int16_t);
int		qle_add_fabric_port(struct qle_softc *, struct qle_fc_port *);
int		qle_classify_port(struct qle_softc *, u_int32_t, u_int64_t,
		    u_int64_t, struct qle_fc_port **);
int		qle_get_loop_id(struct qle_softc *sc);
void		qle_clear_port_lists(struct qle_softc *);
void		qle_ports_gone(struct qle_softc *, u_int32_t);
int		qle_softreset(struct qle_softc *);
void		qle_update_topology(struct qle_softc *);
int		qle_update_fabric(struct qle_softc *);
int		qle_fabric_plogi(struct qle_softc *, struct qle_fc_port *);
void		qle_fabric_plogo(struct qle_softc *, struct qle_fc_port *);

void		qle_update_start(struct qle_softc *, int);
void		qle_update_done(struct qle_softc *, int);
void		qle_do_update(void *, void *);
int		qle_async(struct qle_softc *, u_int16_t);

int		qle_load_fwchunk(struct qle_softc *,
		    struct qle_dmamem *, const u_int32_t *);
int		qle_load_firmware_chunks(struct qle_softc *, const u_int32_t *);
int		qle_read_nvram(struct qle_softc *);

struct qle_dmamem *qle_dmamem_alloc(struct qle_softc *, size_t);
void		qle_dmamem_free(struct qle_softc *, struct qle_dmamem *);

int		qle_alloc_ccbs(struct qle_softc *);
void		qle_free_ccbs(struct qle_softc *);
void		*qle_get_ccb(void *);
void		qle_put_ccb(void *, void *);

void		qle_dump_stuff(struct qle_softc *, void *, int);
void		qle_dump_iocb(struct qle_softc *, void *);
void		qle_dump_iocb_segs(struct qle_softc *, void *, int);


static const struct pci_matchid qle_devices[] = {
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP2422 },
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP2432 },
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP2512 },
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP2522 },
	{ PCI_VENDOR_QLOGIC,	PCI_PRODUCT_QLOGIC_ISP2532 },
};

int
qle_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, qle_devices, nitems(qle_devices)));
}

void
qle_attach(struct device *parent, struct device *self, void *aux)
{
	struct qle_softc *sc = (void *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	u_int32_t pcictl;
	struct scsibus_attach_args saa;
	struct qle_init_cb *icb;

	pcireg_t bars[] = { QLE_PCI_MEM_BAR, QLE_PCI_IO_BAR };
	pcireg_t memtype;
	int r, i, rv;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_ih = NULL;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_ios = 0;

	for (r = 0; r < nitems(bars); r++) {
		memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, bars[r]);
		if (pci_mapreg_map(pa, bars[r], memtype, 0,
		    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_ios, 0) == 0)
			break;

		sc->sc_ios = 0;
	}
	if (sc->sc_ios == 0) {
		printf(": unable to map registers\n");
		return;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": unable to map interrupt\n");
		goto unmap;
	}
	printf(": %s\n", pci_intr_string(sc->sc_pc, ih));

	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
	    qle_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf("%s: unable to establish interrupt\n");
		goto deintr;
	}

	pcictl = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pcictl |= PCI_COMMAND_INVALIDATE_ENABLE |
	    PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, pcictl);

	pcictl = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	pcictl &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	pcictl &= ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT);
	pcictl |= (0x80 << PCI_LATTIMER_SHIFT);
	pcictl |= (0x10 << PCI_CACHELINE_SHIFT);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, pcictl);

	pcictl = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pcictl &= ~1;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, pcictl);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_QLOGIC_ISP2422:
		sc->sc_isp_type = QLE_ISP2422;
		sc->sc_isp_gen = QLE_GEN_ISP24XX;
		sc->sc_mbox_base = QLE_MBOX_BASE_24XX;
		break;
	case PCI_PRODUCT_QLOGIC_ISP2432:
		sc->sc_isp_type = QLE_ISP2432;
		sc->sc_isp_gen = QLE_GEN_ISP24XX;
		sc->sc_mbox_base = QLE_MBOX_BASE_24XX;
		break;
	case PCI_PRODUCT_QLOGIC_ISP2512:
		sc->sc_isp_type = QLE_ISP2512;
		sc->sc_isp_gen = QLE_GEN_ISP25XX;
		sc->sc_mbox_base = QLE_MBOX_BASE_24XX;
		break;
	case PCI_PRODUCT_QLOGIC_ISP2522:
		sc->sc_isp_type = QLE_ISP2522;
		sc->sc_isp_gen = QLE_GEN_ISP25XX;
		sc->sc_mbox_base = QLE_MBOX_BASE_24XX;
		break;
	case PCI_PRODUCT_QLOGIC_ISP2532:
		sc->sc_isp_type = QLE_ISP2532;
		sc->sc_isp_gen = QLE_GEN_ISP25XX;
		sc->sc_mbox_base = QLE_MBOX_BASE_24XX;
		break;

	default:
		printf("unknown pci id %x", pa->pa_id);
		goto deintr;
	}

	sc->sc_port = pa->pa_function;

	TAILQ_INIT(&sc->sc_ports);
	TAILQ_INIT(&sc->sc_ports_new);
	TAILQ_INIT(&sc->sc_ports_gone);
	TAILQ_INIT(&sc->sc_ports_found);

	/* after reset, mbox regs 1 and 2 contain the string "ISP " */
	if (qle_read_mbox(sc, 1) != 0x4953 ||
	    qle_read_mbox(sc, 2) != 0x5020) {
		/* try releasing the risc processor */
		printf("%s: bad startup mboxes: %x %x\n", DEVNAME(sc),
		    qle_read_mbox(sc, 1), qle_read_mbox(sc, 2));
		qle_host_cmd(sc, QLE_HOST_CMD_RELEASE);
	}

	qle_host_cmd(sc, QLE_HOST_CMD_PAUSE);
	if (qle_softreset(sc) != 0) {
		printf("softreset failed\n");
		goto deintr;
	}

	if (qle_read_nvram(sc) == 0)
		sc->sc_nvram_valid = 1;

	switch (sc->sc_isp_gen) {
	case QLE_GEN_ISP24XX:
		if (qle_load_firmware_chunks(sc, isp_2400_risc_code)) {
			printf("firmware load failed\n");
			goto deintr;
		}
		break;
	case QLE_GEN_ISP25XX:
		if (qle_load_firmware_chunks(sc, isp_2500_risc_code)) {
			printf("firmware load failed\n");
			goto deintr;
		}
		break;
	}

	/* execute firmware */
	sc->sc_mbox[0] = QLE_MBOX_EXEC_FIRMWARE;
	sc->sc_mbox[1] = QLE_2400_CODE_ORG >> 16;
	sc->sc_mbox[2] = QLE_2400_CODE_ORG & 0xffff;
	sc->sc_mbox[3] = 0;
	sc->sc_mbox[4] = 0;
	if (qle_mbox(sc, 0x001f, 0x0001)) {
		printf("ISP couldn't exec firmware: %x\n", sc->sc_mbox[0]);
		goto deintr;
	}

	delay(250000);		/* from isp(4) */

	sc->sc_mbox[0] = QLE_MBOX_ABOUT_FIRMWARE;
	if (qle_mbox(sc, QLE_MBOX_ABOUT_FIRMWARE_IN,
	    QLE_MBOX_ABOUT_FIRMWARE_OUT)) {
		printf("ISP not talking after firmware exec: %x\n",
		    sc->sc_mbox[0]);
		goto deintr;
	}
	printf("firmware v%d.%d.%d, attrs %x\n", sc->sc_mbox[1], sc->sc_mbox[2],
	    sc->sc_mbox[3], sc->sc_mbox[6]);

	sc->sc_maxcmds = 4096;

	/* reserve queue slots for markers and fabric ops */
	sc->sc_maxcmds -= 2;

	if (qle_alloc_ccbs(sc)) {
		/* error already printed */
		goto deintr;
	}
	sc->sc_scratch = qle_dmamem_alloc(sc, QLE_SCRATCH_SIZE);
	if (sc->sc_scratch == NULL) {
		printf("%s: unable to allocate scratch\n", DEVNAME(sc));
		goto free_ccbs;
	}

	/* build init buffer thing */
	icb = (struct qle_init_cb *)QLE_DMA_KVA(sc->sc_scratch);
	memset(icb, 0, sizeof(*icb));
	icb->icb_version = QLE_ICB_VERSION;
	if (sc->sc_nvram_valid) {
		icb->icb_max_frame_len = sc->sc_nvram.frame_payload_size;
		icb->icb_exec_throttle = sc->sc_nvram.execution_throttle;
		icb->icb_hardaddr = sc->sc_nvram.hard_address;
		icb->icb_portname = sc->sc_nvram.port_name;
		icb->icb_nodename = sc->sc_nvram.node_name;
		icb->icb_login_retry = sc->sc_nvram.login_retry;
		icb->icb_login_timeout = sc->sc_nvram.login_timeout;
		icb->icb_fwoptions1 = sc->sc_nvram.fwoptions1;
		icb->icb_fwoptions2 = sc->sc_nvram.fwoptions2;
		icb->icb_fwoptions3 = sc->sc_nvram.fwoptions3;
	} else {
		/* defaults copied from isp(4) */
		icb->icb_max_frame_len = htole16(1024);
		icb->icb_exec_throttle = htole16(16);
		icb->icb_portname = htobe64(QLE_DEFAULT_PORT_NAME);
		icb->icb_nodename = 0;
		icb->icb_login_retry = 3;

		icb->icb_fwoptions1 = htole16(QLE_ICB_FW1_FAIRNESS |
		    QLE_ICB_FW1_HARD_ADDR |
		    QLE_ICB_FW1_FULL_DUPLEX);
		icb->icb_fwoptions2 = htole16(QLE_ICB_FW2_LOOP_PTP);
		icb->icb_fwoptions3 = htole16(QLE_ICB_FW3_FCP_RSP_24_0 |
		    QLE_ICB_FW3_AUTONEG);
	}

	icb->icb_exchange_count = 0;

	icb->icb_req_out = 0;
	icb->icb_resp_in = 0;
	icb->icb_pri_req_out = 0;
	icb->icb_req_queue_len = htole16(sc->sc_maxcmds);
	icb->icb_resp_queue_len = htole16(sc->sc_maxcmds);
	icb->icb_pri_req_queue_len = htole16(8); /* apparently the minimum */
	icb->icb_req_queue_addr = htole64(QLE_DMA_DVA(sc->sc_requests));
	icb->icb_resp_queue_addr = htole64(QLE_DMA_DVA(sc->sc_responses));
	icb->icb_pri_req_queue_addr =
	    htole64(QLE_DMA_DVA(sc->sc_pri_requests));

	icb->icb_link_down_nos = htole16(200);
	icb->icb_int_delay = 0;
	icb->icb_login_timeout = 0;

	sc->sc_mbox[0] = QLE_MBOX_INIT_FIRMWARE;
	sc->sc_mbox[4] = 0;
	sc->sc_mbox[5] = 0;
	qle_mbox_putaddr(sc->sc_mbox, sc->sc_scratch);
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_scratch), 0,
	    sizeof(*icb), BUS_DMASYNC_PREWRITE);
	rv = qle_mbox(sc, QLE_MBOX_INIT_FIRMWARE_IN, 0x0001);
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_scratch), 0,
	    sizeof(*icb), BUS_DMASYNC_POSTWRITE);

	if (rv != 0) {
		printf("%s: ISP firmware init failed: %x\n", DEVNAME(sc),
		    sc->sc_mbox[0]);
		goto free_scratch;
	}

	/* enable some more notifications */
	sc->sc_mbox[0] = QLE_MBOX_SET_FIRMWARE_OPTIONS;
	sc->sc_mbox[1] = QLE_FW_OPTION1_ASYNC_LIP_F8 |
	    QLE_FW_OPTION1_ASYNC_LIP_RESET |
	    QLE_FW_OPTION1_ASYNC_LIP_ERROR |
	    QLE_FW_OPTION1_ASYNC_LOGIN_RJT;
	sc->sc_mbox[2] = 0;
	sc->sc_mbox[3] = 0;
	if (qle_mbox(sc, QLE_MBOX_SET_FIRMWARE_OPTIONS_IN, 0x0001)) {
		printf("%s: setting firmware options failed: %x\n",
		    DEVNAME(sc), sc->sc_mbox[0]);
		goto free_scratch;
	}

	sc->sc_update_taskq = taskq_create(DEVNAME(sc), 1, IPL_BIO);
	task_set(&sc->sc_update_task, qle_do_update, sc, NULL);

	/* wait a bit for link to come up so we can scan and attach devices */
	for (i = 0; i < QLE_WAIT_FOR_LOOP * 10000; i++) {
		u_int16_t isr, info;

		delay(100);

		if (qle_read_isr(sc, &isr, &info) == 0)
			continue;

		qle_handle_intr(sc, isr, info);

		if (sc->sc_loop_up)
			break;
	}

	if (sc->sc_loop_up) {
		qle_do_update(sc, NULL);
	} else {
		printf("%s: loop still down, giving up\n", DEVNAME(sc));
	}

	/* we should be good to go now, attach scsibus */
	sc->sc_link.adapter = &qle_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = QLE_MAX_TARGETS;
	sc->sc_link.adapter_buswidth = QLE_MAX_TARGETS;
	sc->sc_link.openings = sc->sc_maxcmds;
	sc->sc_link.pool = &sc->sc_iopool;
	if (sc->sc_nvram_valid) {
		sc->sc_link.port_wwn = betoh64(sc->sc_nvram.port_name);
		sc->sc_link.node_wwn = betoh64(sc->sc_nvram.node_name);
	} else {
		sc->sc_link.port_wwn = QLE_DEFAULT_PORT_NAME;
		sc->sc_link.node_wwn = 0;
	}
	if (sc->sc_link.node_wwn == 0) {
		/*
		 * mask out the port number from the port name to get
		 * the node name.
		 */
		sc->sc_link.node_wwn = sc->sc_link.port_wwn;
		sc->sc_link.node_wwn &= ~(0xfULL << 56);
	}

	memset(&saa, 0, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	/* config_found() returns the scsibus attached to us */
	sc->sc_scsibus = (struct scsibus_softc *)config_found(&sc->sc_dev,
	    &saa, scsiprint);

	return;

free_scratch:
	qle_dmamem_free(sc, sc->sc_scratch);
free_ccbs:
	qle_free_ccbs(sc);
deintr:
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	sc->sc_ih = NULL;
unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
qle_detach(struct device *self, int flags)
{
	struct qle_softc *sc = (struct qle_softc *)self;

	if (sc->sc_ih == NULL) {
		/* we didnt attach properly, so nothing to detach */
		return (0);
	}

	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	sc->sc_ih = NULL;

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;

	return (0);
}

int
qle_classify_port(struct qle_softc *sc, u_int32_t location,
    u_int64_t port_name, u_int64_t node_name, struct qle_fc_port **prev)
{
	struct qle_fc_port *port, *locmatch, *wwnmatch;
	locmatch = NULL;
	wwnmatch = NULL;

	/* make sure we don't try to add a port or location twice */
	TAILQ_FOREACH(port, &sc->sc_ports_new, update) {
		if ((port->port_name == port_name &&
		    port->node_name == node_name) ||
		    port->location == location) {
			*prev = port;
			return (QLE_PORT_DISP_DUP);
		}
	}

	/* if we're attaching, everything is new */
	if (sc->sc_scsibus == NULL) {
		*prev = NULL;
		return (QLE_PORT_DISP_NEW);
	}

	TAILQ_FOREACH(port, &sc->sc_ports_gone, update) {
		if (port->location == location)
			locmatch = port;

		if (port->port_name == port_name &&
		    port->node_name == node_name)
			wwnmatch = port;
	}

	if (locmatch == NULL && wwnmatch == NULL) {
		*prev = NULL;
		return (QLE_PORT_DISP_NEW);
	} else if (locmatch == wwnmatch) {
		*prev = locmatch;
		return (QLE_PORT_DISP_SAME);
	} else if (wwnmatch != NULL) {
		*prev = wwnmatch;
		return (QLE_PORT_DISP_MOVED);
	} else {
		*prev = locmatch;
		return (QLE_PORT_DISP_CHANGED);
	}
}

int
qle_get_loop_id(struct qle_softc *sc)
{
	int i, last;

	i = QLE_MIN_HANDLE;
	last = QLE_MAX_HANDLE;
	for (; i <= last; i++) {
		if (sc->sc_targets[i] == NULL)
			return (i);
	}

	return (-1);
}

int
qle_get_port_db(struct qle_softc *sc, u_int16_t loopid, struct qle_dmamem *mem)
{
	sc->sc_mbox[0] = QLE_MBOX_GET_PORT_DB;
	sc->sc_mbox[1] = loopid;
	qle_mbox_putaddr(sc->sc_mbox, mem);
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0,
	    sizeof(struct qle_get_port_db), BUS_DMASYNC_PREREAD);
	if (qle_mbox(sc, 0x00cf, 0x0001)) {
		printf("%s: get port db for %x failed: %x\n",
		    DEVNAME(sc), loopid, sc->sc_mbox[0]);
		return (1);
	}

	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0,
	    sizeof(struct qle_get_port_db), BUS_DMASYNC_POSTREAD);
	return (0);
}

int
qle_add_loop_port(struct qle_softc *sc, u_int16_t loopid)
{
	struct qle_get_port_db *pdb;
	struct qle_fc_port *port, *pport;
	int disp;

	if (qle_get_port_db(sc, loopid, sc->sc_scratch) != 0) {
		return (1);
	}
	pdb = QLE_DMA_KVA(sc->sc_scratch);

	port = malloc(sizeof(*port), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (port == NULL) {
		printf("%s: failed to allocate a port structure\n",
		    DEVNAME(sc));
		return (1);
	}

	if (letoh16(pdb->prli_svc_word3) & QLE_SVC3_TARGET_ROLE)
		port->flags |= QLE_PORT_FLAG_IS_TARGET;

	port->port_name = betoh64(pdb->port_name);
	port->node_name = betoh64(pdb->node_name);
	port->location = QLE_LOCATION_LOOP_ID(loopid);
	port->loopid = loopid;
	port->portid = (pdb->port_id[0] << 16) | (pdb->port_id[1] << 8) |
	    pdb->port_id[2];

	mtx_enter(&sc->sc_port_mtx);
	disp = qle_classify_port(sc, port->location, port->port_name,
	    port->node_name, &pport);
	switch (disp) {
	case QLE_PORT_DISP_CHANGED:
	case QLE_PORT_DISP_MOVED:
	case QLE_PORT_DISP_NEW:
		TAILQ_INSERT_TAIL(&sc->sc_ports_new, port, update);
		sc->sc_targets[loopid] = port;
		break;
	case QLE_PORT_DISP_DUP:
		free(port, M_DEVBUF);
		break;
	case QLE_PORT_DISP_SAME:
		TAILQ_REMOVE(&sc->sc_ports_gone, pport, update);
		free(port, M_DEVBUF);
		break;
	}
	mtx_leave(&sc->sc_port_mtx);

	switch (disp) {
	case QLE_PORT_DISP_CHANGED:
	case QLE_PORT_DISP_MOVED:
	case QLE_PORT_DISP_NEW:
		printf("%s: %s %d; name %llx\n",
		    DEVNAME(sc), ISSET(port->flags, QLE_PORT_FLAG_IS_TARGET) ?
		    "target" : "non-target", loopid, betoh64(pdb->port_name));
		break;
	default:
		break;
	}
	return (0);
}

int
qle_add_fabric_port(struct qle_softc *sc, struct qle_fc_port *port)
{
	struct qle_get_port_db *pdb;

	if (qle_get_port_db(sc, port->loopid, sc->sc_scratch) != 0) {
		free(port, M_DEVBUF);
		return (1);
	}
	pdb = QLE_DMA_KVA(sc->sc_scratch);

	if (letoh16(pdb->prli_svc_word3) & QLE_SVC3_TARGET_ROLE)
		port->flags |= QLE_PORT_FLAG_IS_TARGET;

	/* compare port and node name with what's in the port db now */

	mtx_enter(&sc->sc_port_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_ports_new, port, update);
	sc->sc_targets[port->loopid] = port;
	mtx_leave(&sc->sc_port_mtx);

	printf("%s: %s %d; name %llx\n",
	    DEVNAME(sc), ISSET(port->flags, QLE_PORT_FLAG_IS_TARGET) ?
	    "target" : "non-target", port->loopid, port->port_name);
	return (0);
}

struct qle_ccb *
qle_handle_resp(struct qle_softc *sc, u_int16_t id)
{
	struct qle_ccb *ccb;
	struct qle_iocb_status *status;
	struct qle_iocb_req6 *req;
	struct scsi_xfer *xs;
	u_int32_t handle;
	u_int16_t completion;
	u_int8_t *entry;
	u_int8_t *data;

	ccb = NULL;
	entry = QLE_DMA_KVA(sc->sc_responses) + (id * QLE_QUEUE_ENTRY_SIZE);
	
	bus_dmamap_sync(sc->sc_dmat,
	    QLE_DMA_MAP(sc->sc_responses), id * QLE_QUEUE_ENTRY_SIZE,
	    QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_POSTREAD);

	/*qle_dump_iocb(sc, entry);*/
	switch(entry[0]) {
	case QLE_IOCB_STATUS:
		status = (struct qle_iocb_status *)entry;
		handle = status->handle;
		if (handle > sc->sc_maxcmds) {
			panic("bad completed command handle: %d (> %d)",
			    handle, sc->sc_maxcmds);
		}

		ccb = &sc->sc_ccbs[handle];
		xs = ccb->ccb_xs;
		if (xs == NULL) {
			printf("%s: got status for inactive ccb %d\n",
			    DEVNAME(sc), handle);
			ccb = NULL;
			break;
		}
		if (xs->io != ccb) {
			panic("completed command handle doesn't match xs "
			    "(handle %d, ccb %p, xs->io %p)", handle, ccb,
			    xs->io);
		}
		/*qle_dump_iocb(sc, status);*/

		if (xs->datalen > 0) {
			if (ccb->ccb_dmamap->dm_nsegs >
			    QLE_IOCB_SEGS_PER_CMD) {
				bus_dmamap_sync(sc->sc_dmat,
				    QLE_DMA_MAP(sc->sc_segments),
				    ccb->ccb_seg_offset,
				    sizeof(*ccb->ccb_segs) *
				    ccb->ccb_dmamap->dm_nsegs,
				    BUS_DMASYNC_POSTWRITE);
			}

			bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
			    ccb->ccb_dmamap->dm_mapsize,
			    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
		}

		xs->status = letoh16(status->scsi_status) & 0x0f;
		completion = letoh16(status->completion);
		switch (completion) {
		case QLE_IOCB_STATUS_DATA_OVERRUN:
		case QLE_IOCB_STATUS_DATA_UNDERRUN:
		case QLE_IOCB_STATUS_COMPLETE:
			if (completion == QLE_IOCB_STATUS_COMPLETE) {
				xs->resid = 0;
			} else {
				xs->resid = letoh32(status->resid);
			}

			if (letoh16(status->scsi_status) &
			    QLE_SCSI_STATUS_SENSE_VALID) {
				u_int32_t *pp;
				int sr;
				data = status->data +
				    letoh32(status->fcp_rsp_len);
				memcpy(&xs->sense, data,
				    letoh32(status->fcp_sense_len));
				xs->error = XS_SENSE;
				pp = (u_int32_t *)&xs->sense;
				for (sr = 0; sr < sizeof(xs->sense)/4; sr++) {
					pp[sr] = swap32(pp[sr]);
				}
			} else {
				xs->error = XS_NOERROR;
			}
			break;

		case QLE_IOCB_STATUS_DMA_ERROR:
			printf("%s: dma error\n", DEVNAME(sc));
			/* set resid apparently? */
			break;

		case QLE_IOCB_STATUS_RESET:
			printf("%s: reset destroyed command\n", DEVNAME(sc));
			sc->sc_marker_required = 1;
			xs->error = XS_RESET;
			break;

		case QLE_IOCB_STATUS_ABORTED:
			printf("%s: aborted\n", DEVNAME(sc));
			sc->sc_marker_required = 1;
			xs->error = XS_DRIVER_STUFFUP;
			break;
		
		case QLE_IOCB_STATUS_TIMEOUT:
			printf("%s: command timed out\n", DEVNAME(sc));
			xs->error = XS_TIMEOUT;
			break;

		case QLE_IOCB_STATUS_QUEUE_FULL:
			printf("%s: queue full\n", DEVNAME(sc));
			xs->error = XS_BUSY;
			break;

		case QLE_IOCB_STATUS_PORT_UNAVAIL:
		case QLE_IOCB_STATUS_PORT_LOGGED_OUT:
		case QLE_IOCB_STATUS_PORT_CHANGED:
			printf("%s: dev gone\n", DEVNAME(sc));
			xs->error = XS_SELTIMEOUT;
			break;

		default:
			printf("%s: unexpected completion status %x\n",
			    DEVNAME(sc), status->completion);
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case QLE_IOCB_STATUS_CONT:
		printf("%s: ignoring status continuation iocb\n",
		    DEVNAME(sc));
		break;

	case QLE_IOCB_PLOGX:
	case QLE_IOCB_CT_PASSTHROUGH:
		if (sc->sc_fabric_pending) {
			/*qle_dump_iocb(sc, entry);*/
			sc->sc_fabric_pending = 2;
			wakeup(sc->sc_scratch);
		} else {
			printf("%s: unexpected fabric response %x\n",
			    DEVNAME(sc), entry[0]);
		}
		break;

	case QLE_IOCB_MARKER:
		break;

	case QLE_IOCB_CMD_TYPE_6:
	case QLE_IOCB_CMD_TYPE_7:
		printf("%s: request bounced back\n", DEVNAME(sc));
		req = (struct qle_iocb_req6 *)entry;
		handle = req->req_handle;
		if (handle > sc->sc_maxcmds) {
			panic("bad bounced command handle: %d (> %d)",
			    handle, sc->sc_maxcmds);
		}

		ccb = &sc->sc_ccbs[handle];
		xs = ccb->ccb_xs;
		xs->error = XS_DRIVER_STUFFUP;
		break;
	default:
		printf("%s: unexpected response entry type %x\n",
		    DEVNAME(sc), entry[0]);
		break;
	}

	return (ccb);
}

void
qle_handle_intr(struct qle_softc *sc, u_int16_t isr, u_int16_t info)
{
	int i;
	u_int16_t rspin;
	struct qle_ccb *ccb;

	switch (isr) {
	case QLE_INT_TYPE_ASYNC:
		qle_async(sc, info);
		break;

	case QLE_INT_TYPE_IO:
		rspin = qle_read(sc, QLE_RESP_IN);
		if (rspin == sc->sc_last_resp_id) {
			/* isp(4) has some weird magic for this case */
			printf("%s: nonsense interrupt (%x)\n", DEVNAME(sc),
			    rspin);
		} else {
			while (sc->sc_last_resp_id != rspin) {
				ccb = qle_handle_resp(sc, sc->sc_last_resp_id);
				if (ccb)
					scsi_done(ccb->ccb_xs);

				sc->sc_last_resp_id++;
				if (sc->sc_last_resp_id == sc->sc_maxcmds)
					sc->sc_last_resp_id = 0;
			}

			qle_write(sc, QLE_RESP_OUT, sc->sc_last_resp_id);
		}
		break;

	case QLE_INT_TYPE_MBOX:
		mtx_enter(&sc->sc_mbox_mtx);
		if (sc->sc_mbox_pending) {
			sc->sc_mbox[0] = info;
			if (info == QLE_MBOX_COMPLETE) {
				for (i = 1; i < nitems(sc->sc_mbox); i++) {
					sc->sc_mbox[i] = qle_read_mbox(sc, i);
				}
			}
			sc->sc_mbox_pending = 2;
			wakeup(sc->sc_mbox);
			mtx_leave(&sc->sc_mbox_mtx);
		} else {
			mtx_leave(&sc->sc_mbox_mtx);
			printf("%s: unexpected mbox interrupt: %x\n",
			    DEVNAME(sc), info);
		}
		break;

	default:
		/* maybe log something? */
		break;
	}

	qle_clear_isr(sc, isr);
}

int
qle_intr(void *xsc)
{
	struct qle_softc *sc = xsc;
	u_int16_t isr;
	u_int16_t info;

	if (qle_read_isr(sc, &isr, &info) == 0)
		return (0);

	qle_handle_intr(sc, isr, info);
	return (1);
}

int
qle_scsi_probe(struct scsi_link *link)
{
	struct qle_softc *sc = link->adapter_softc;
	int rv = 0;

	mtx_enter(&sc->sc_port_mtx);
	if (sc->sc_targets[link->target] == NULL)
		rv = ENXIO;
	else if (!ISSET(sc->sc_targets[link->target]->flags,
	    QLE_PORT_FLAG_IS_TARGET))
		rv = ENXIO;
	mtx_leave(&sc->sc_port_mtx);

	return (rv);
}

void
qle_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct qle_softc	*sc = link->adapter_softc;
	struct qle_ccb		*ccb;
	void			*iocb;
	struct qle_ccb_list	list;
	u_int16_t		req;
	u_int32_t		portid;
	int			offset, error;
	bus_dmamap_t		dmap;

	if (xs->cmdlen > 16) {
		printf("%s: too fat (%d)\n", DEVNAME(sc), xs->cmdlen);
		memset(&xs->sense, 0, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | SSD_ERRCODE_CURRENT;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20;
		xs->error = XS_SENSE;
		scsi_done(xs);
		return;
	}

	portid = 0xffffffff;
	mtx_enter(&sc->sc_port_mtx);
	if (sc->sc_targets[xs->sc_link->target] != NULL) {
		portid = sc->sc_targets[xs->sc_link->target]->portid;
	}
	mtx_leave(&sc->sc_port_mtx);
	if (portid == 0xffffffff) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	ccb = xs->io;
	dmap = ccb->ccb_dmamap;
	if (xs->datalen > 0) {
		error = bus_dmamap_load(sc->sc_dmat, dmap, xs->data,
		    xs->datalen, NULL, (xs->flags & SCSI_NOSLEEP) ?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
		if (error) {
			xs->error = XS_DRIVER_STUFFUP;
			scsi_done(xs);
			return;
		}

		bus_dmamap_sync(sc->sc_dmat, dmap, 0,
		    dmap->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);
	}

	mtx_enter(&sc->sc_queue_mtx);

	/* put in a sync marker if required */
	if (sc->sc_marker_required) {
		req = sc->sc_next_req_id++;
		if (sc->sc_next_req_id == sc->sc_maxcmds)
			sc->sc_next_req_id = 0;

		printf("%s: writing marker at request %d\n", DEVNAME(sc), req);
		offset = (req * QLE_QUEUE_ENTRY_SIZE);
		iocb = QLE_DMA_KVA(sc->sc_requests) + offset;
		bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_requests),
		    offset, QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_POSTWRITE);
		qle_put_marker(sc, iocb);
		qle_write(sc, QLE_REQ_IN, sc->sc_next_req_id);
		sc->sc_marker_required = 0;
	}

	req = sc->sc_next_req_id++;
	if (sc->sc_next_req_id == sc->sc_maxcmds)
		sc->sc_next_req_id = 0;

	offset = (req * QLE_QUEUE_ENTRY_SIZE);
	iocb = QLE_DMA_KVA(sc->sc_requests) + offset;
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_requests), offset,
	    QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_POSTWRITE);
	    
	ccb->ccb_xs = xs;

	qle_put_cmd(sc, iocb, xs, ccb, portid);

	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_requests), offset,
	    QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_PREREAD);
	qle_write(sc, QLE_REQ_IN, sc->sc_next_req_id);

	if (!ISSET(xs->flags, SCSI_POLL)) {
		mtx_leave(&sc->sc_queue_mtx);
		return;
	}

	SIMPLEQ_INIT(&list);
	do {
		ccb = qle_scsi_cmd_poll(sc);
		SIMPLEQ_INSERT_TAIL(&list, ccb, ccb_link);
	} while (xs->io != ccb);

	mtx_leave(&sc->sc_queue_mtx);

	while ((ccb = SIMPLEQ_FIRST(&list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&list, ccb_link);
		scsi_done(ccb->ccb_xs);
	}
}

struct qle_ccb *
qle_scsi_cmd_poll(struct qle_softc *sc)
{
	u_int16_t rspin;
	struct qle_ccb *ccb = NULL;

	while (ccb == NULL) {
		u_int16_t isr, info;

		delay(100);

		if (qle_read_isr(sc, &isr, &info) == 0) {
			continue;
		}

		if (isr != QLE_INT_TYPE_IO) {
			qle_handle_intr(sc, isr, info);
			continue;
		}

		rspin = qle_read(sc, QLE_RESP_IN);
		if (rspin != sc->sc_last_resp_id) {
			ccb = qle_handle_resp(sc, sc->sc_last_resp_id);

			sc->sc_last_resp_id++;
			if (sc->sc_last_resp_id == sc->sc_maxcmds)
				sc->sc_last_resp_id = 0;

			qle_write(sc, QLE_RESP_OUT, sc->sc_last_resp_id);
		}

		qle_clear_isr(sc, isr);
	}

	return (ccb);
}

u_int32_t
qle_read(struct qle_softc *sc, int offset)
{
	u_int32_t v;
	v = bus_space_read_4(sc->sc_iot, sc->sc_ioh, offset);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, offset, 4,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return (v);
}

void
qle_write(struct qle_softc *sc, int offset, u_int32_t value)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, offset, value);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, offset, 4,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
}

u_int16_t
qle_read_mbox(struct qle_softc *sc, int mbox)
{
	u_int16_t v;
	bus_size_t offset = sc->sc_mbox_base + (mbox * 2);
	v = bus_space_read_2(sc->sc_iot, sc->sc_ioh, offset);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, offset, 2,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return (v);
}

void
qle_write_mbox(struct qle_softc *sc, int mbox, u_int16_t value)
{
	bus_size_t offset = sc->sc_mbox_base + (mbox * 2);
	bus_space_write_2(sc->sc_iot, sc->sc_ioh, offset, value);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, offset, 2,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
}

void
qle_host_cmd(struct qle_softc *sc, u_int32_t cmd)
{
	qle_write(sc, QLE_HOST_CMD_CTRL, cmd << QLE_HOST_CMD_SHIFT);
}

#define MBOX_COMMAND_TIMEOUT	400000

int
qle_mbox(struct qle_softc *sc, int maskin, int maskout)
{
	int i;
	int result = 0;
	int rv;

	for (i = 0; i < nitems(sc->sc_mbox); i++) {
		if (maskin & (1 << i)) {
			qle_write_mbox(sc, i, sc->sc_mbox[i]);
		}
	}
	qle_host_cmd(sc, QLE_HOST_CMD_SET_HOST_INT);

	if (sc->sc_scsibus == NULL) {
		for (i = 0; i < MBOX_COMMAND_TIMEOUT && result == 0; i++) {
			u_int16_t isr, info;

			delay(100);

			if (qle_read_isr(sc, &isr, &info) == 0)
				continue;

			switch (isr) {
			case QLE_INT_TYPE_MBOX:
				result = info;
				break;

			default:
				qle_handle_intr(sc, isr, info);
				break;
			}
		}
	} else {
		mtx_enter(&sc->sc_mbox_mtx);
		sc->sc_mbox_pending = 1;
		while (sc->sc_mbox_pending == 1) {
			msleep(sc->sc_mbox, &sc->sc_mbox_mtx, PRIBIO,
			    "qlembox", 0);
		}
		result = sc->sc_mbox[0];
		sc->sc_mbox_pending = 0;
		mtx_leave(&sc->sc_mbox_mtx);
		return (result == QLE_MBOX_COMPLETE ? 0 : result);
	}

	switch (result) {
	case QLE_MBOX_COMPLETE:
		for (i = 1; i < nitems(sc->sc_mbox); i++) {
			sc->sc_mbox[i] = (maskout & (1 << i)) ?
			    qle_read_mbox(sc, i) : 0;
		}
		rv = 0;
		break;

	case 0:
		/* timed out; do something? */
		printf("mbox timed out\n");
		rv = 1;
		break;

	default:
		/* log a thing? */
		sc->sc_mbox[0] = result;
		rv = result;
		break;
	}

	qle_clear_isr(sc, QLE_INT_TYPE_MBOX);
	return (rv);
}

void
qle_mbox_putaddr(u_int16_t *mbox, struct qle_dmamem *mem)
{
	mbox[2] = (QLE_DMA_DVA(mem) >> 16) & 0xffff;
	mbox[3] = (QLE_DMA_DVA(mem) >> 0) & 0xffff;
	mbox[6] = (QLE_DMA_DVA(mem) >> 48) & 0xffff;
	mbox[7] = (QLE_DMA_DVA(mem) >> 32) & 0xffff;
}

void
qle_set_ints(struct qle_softc *sc, int enabled)
{
	u_int32_t v = enabled ? QLE_INT_CTRL_ENABLE : 0;
	qle_write(sc, QLE_INT_CTRL, v);
}

int
qle_read_isr(struct qle_softc *sc, u_int16_t *isr, u_int16_t *info)
{
	u_int32_t v;

	switch (sc->sc_isp_gen) {
	case QLE_GEN_ISP24XX:
	case QLE_GEN_ISP25XX:
		if ((qle_read(sc, QLE_INT_STATUS) & QLE_RISC_INT_REQ) == 0)
			return (0);

		v = qle_read(sc, QLE_RISC_STATUS);

		switch (v & QLE_INT_STATUS_MASK) {
		case QLE_24XX_INT_ROM_MBOX:
		case QLE_24XX_INT_ROM_MBOX_FAIL:
		case QLE_24XX_INT_MBOX:
		case QLE_24XX_INT_MBOX_FAIL:
			*isr = QLE_INT_TYPE_MBOX;
			break;

		case QLE_24XX_INT_ASYNC:
			*isr = QLE_INT_TYPE_ASYNC;
			break;

		case QLE_24XX_INT_RSPQ:
			*isr = QLE_INT_TYPE_IO;
			break;

		default:
			*isr = QLE_INT_TYPE_OTHER;
			break;
		}

		*info = (v >> QLE_INT_INFO_SHIFT);
		return (1);

	default:
		return (0);
	}
}

void
qle_clear_isr(struct qle_softc *sc, u_int16_t isr)
{
	qle_host_cmd(sc, QLE_HOST_CMD_CLR_RISC_INT);
}

void
qle_update_done(struct qle_softc *sc, int task)
{
	atomic_clearbits_int(&sc->sc_update_tasks, task);
}

void
qle_update_start(struct qle_softc *sc, int task)
{
	atomic_setbits_int(&sc->sc_update_tasks, task);
	task_add(sc->sc_update_taskq, &sc->sc_update_task);
}

void
qle_clear_port_lists(struct qle_softc *sc)
{
	struct qle_fc_port *p;
	while (!TAILQ_EMPTY(&sc->sc_ports_found)) {
		p = TAILQ_FIRST(&sc->sc_ports_found);
		TAILQ_REMOVE(&sc->sc_ports_found, p, update);
		free(p, M_DEVBUF);
	}

	while (!TAILQ_EMPTY(&sc->sc_ports_new)) {
		p = TAILQ_FIRST(&sc->sc_ports_new);
		TAILQ_REMOVE(&sc->sc_ports_new, p, update);
		free(p, M_DEVBUF);
	}

	while (!TAILQ_EMPTY(&sc->sc_ports_gone)) {
		p = TAILQ_FIRST(&sc->sc_ports_gone);
		TAILQ_REMOVE(&sc->sc_ports_gone, p, update);
	}
}

void
qle_ports_gone(struct qle_softc *sc, u_int32_t location)
{
	struct qle_fc_port *port;
	TAILQ_FOREACH(port, &sc->sc_ports, ports) {
		if ((port->location & location) != 0)
			TAILQ_INSERT_TAIL(&sc->sc_ports_gone, port, update);
	}
}

int
qle_softreset(struct qle_softc *sc)
{
	int i;
	qle_set_ints(sc, 0);

	/* set led control bits, stop dma */
	qle_write(sc, QLE_GPIO_DATA, 0);
	qle_write(sc, QLE_CTRL_STATUS, QLE_CTRL_DMA_SHUTDOWN);
	while (qle_read(sc, QLE_CTRL_STATUS) & QLE_CTRL_DMA_ACTIVE) {
		printf("%s: dma still active\n", DEVNAME(sc));
		delay(100);
	}

	/* reset */
	qle_write(sc, QLE_CTRL_STATUS, QLE_CTRL_RESET | QLE_CTRL_DMA_SHUTDOWN);
	delay(100);
	/* clear data and control dma engines? */

	/* wait for soft reset to clear */
	for (i = 0; i < 1000; i++) {
		if (qle_read_mbox(sc, 0) == 0x0000)
			break;

		delay(100);
	}

	if (i == 1000) {
		printf("%s: reset mbox didn't clear\n", DEVNAME(sc));
		qle_set_ints(sc, 0);
		return (ENXIO);
	}

	for (i = 0; i < 500000; i++) {
		if ((qle_read(sc, QLE_CTRL_STATUS) & QLE_CTRL_RESET) == 0)
			break;
		delay(5);
	}
	if (i == 500000) {
		printf("%s: reset status didn't clear\n", DEVNAME(sc));
		return (ENXIO);
	}

	/* reset risc processor */
	qle_host_cmd(sc, QLE_HOST_CMD_RESET);
	qle_host_cmd(sc, QLE_HOST_CMD_RELEASE);
	qle_host_cmd(sc, QLE_HOST_CMD_CLEAR_RESET);

	/* wait for reset to clear */
	for (i = 0; i < 1000; i++) {
		if (qle_read_mbox(sc, 0) == 0x0000)
			break;
		delay(100);
	}
	if (i == 1000) {
		printf("%s: risc not ready after reset\n", DEVNAME(sc));
		return (ENXIO);
	}

	/* reset queue pointers */
	qle_write(sc, QLE_REQ_IN, 0);
	qle_write(sc, QLE_REQ_OUT, 0);
	qle_write(sc, QLE_RESP_IN, 0);
	qle_write(sc, QLE_RESP_OUT, 0);

	qle_set_ints(sc, 1);

	/* do a basic mailbox operation to check we're alive */
	sc->sc_mbox[0] = QLE_MBOX_NOP;
	if (qle_mbox(sc, 0x0001, 0x0001)) {
		printf("ISP not responding after reset\n");
		return (ENXIO);
	}

	return (0);
}

void
qle_update_topology(struct qle_softc *sc)
{
	sc->sc_mbox[0] = QLE_MBOX_GET_ID;
	if (qle_mbox(sc, 0x0001, QLE_MBOX_GET_LOOP_ID_OUT)) {
		printf("%s: unable to get loop id\n", DEVNAME(sc));
		sc->sc_topology = QLE_TOPO_N_PORT_NO_TARGET;
	} else {
		sc->sc_topology = sc->sc_mbox[6];
		sc->sc_loop_id = sc->sc_mbox[1];

		switch (sc->sc_topology) {
		case QLE_TOPO_NL_PORT:
		case QLE_TOPO_N_PORT:
			printf("%s: loop id %d\n", DEVNAME(sc),
			    sc->sc_loop_id);
			break;

		case QLE_TOPO_FL_PORT:
		case QLE_TOPO_F_PORT:
			sc->sc_port_id = sc->sc_mbox[2] |
			    (sc->sc_mbox[3] << 16);
			printf("%s: fabric port id %06x\n", DEVNAME(sc), 
			    sc->sc_port_id);
			break;

		case QLE_TOPO_N_PORT_NO_TARGET:
		default:
			printf("%s: not useful\n", DEVNAME(sc));
			break;
		}

		switch (sc->sc_topology) {
		case QLE_TOPO_NL_PORT:
		case QLE_TOPO_FL_PORT:
			sc->sc_loop_max_id = 126;
			break;

		case QLE_TOPO_N_PORT:
			sc->sc_loop_max_id = 2;
			break;

		default:
			sc->sc_loop_max_id = 0;
			break;
		}
	}
}

int
qle_update_fabric(struct qle_softc *sc)
{
	/*struct qle_sns_rft_id *rft;*/

	switch (sc->sc_topology) {
	case QLE_TOPO_F_PORT:
	case QLE_TOPO_FL_PORT:
		break;

	default:
		return (0);
	}

	/* get the name server's port db entry */
	sc->sc_mbox[0] = QLE_MBOX_GET_PORT_DB;
	sc->sc_mbox[1] = QLE_F_PORT_HANDLE;
	qle_mbox_putaddr(sc->sc_mbox, sc->sc_scratch);
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_scratch), 0,
	    sizeof(struct qle_get_port_db), BUS_DMASYNC_PREREAD);
	if (qle_mbox(sc, 0x00cf, 0x0001)) {
		printf("%s: get port db for SNS failed: %x\n",
		    DEVNAME(sc), sc->sc_mbox[0]);
		sc->sc_sns_port_name = 0;
	} else {
		struct qle_get_port_db *pdb;
		bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_scratch), 0,
		    sizeof(struct qle_get_port_db), BUS_DMASYNC_POSTREAD);
		pdb = QLE_DMA_KVA(sc->sc_scratch);
		printf("%s: SNS port name %llx\n", DEVNAME(sc),
		    betoh64(pdb->port_name));
		sc->sc_sns_port_name = betoh64(pdb->port_name);
	}

	/*
	 * register fc4 types with the fabric
	 * some switches do this automatically, but apparently
	 * some don't.
	 */
	/*
	rft = QLE_DMA_KVA(sc->sc_scratch);
	memset(rft, 0, sizeof(*rft) + sizeof(struct qle_sns_req_hdr));
	rft->subcmd = htole16(QLE_SNS_RFT_ID);
	rft->max_word = htole16(sizeof(struct qle_sns_req_hdr) / 4);
	rft->port_id = htole32(sc->sc_port_id);
	rft->fc4_types[0] = (1 << QLE_FC4_SCSI);
	if (qle_sns_req(sc, sc->sc_scratch, sizeof(*rft))) {
		printf("%s: RFT_ID failed\n", DEVNAME(sc));
		/ * we might be able to continue after this fails * /
	}
	*/

	return (1);
}

int
qle_ct_pass_through(struct qle_softc *sc, u_int32_t port_handle,
    struct qle_dmamem *mem, size_t req_size, size_t resp_size)
{
	struct qle_iocb_ct_passthrough *iocb;
	u_int16_t req;
	u_int64_t offset;
	int rv;
	
	mtx_enter(&sc->sc_queue_mtx);

	req = sc->sc_next_req_id++;
	if (sc->sc_next_req_id == sc->sc_maxcmds)
		sc->sc_next_req_id = 0;

	offset = (req * QLE_QUEUE_ENTRY_SIZE);
	iocb = QLE_DMA_KVA(sc->sc_requests) + offset;
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_requests), offset,
	    QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_POSTWRITE);
	    
	memset(iocb, 0, QLE_QUEUE_ENTRY_SIZE);
	iocb->entry_type = QLE_IOCB_CT_PASSTHROUGH;
	iocb->entry_count = 1;

	iocb->req_handle = 9;
	iocb->req_nport_handle = htole16(port_handle);
	iocb->req_dsd_count = htole16(1);
	iocb->req_resp_dsd_count = htole16(1);
	iocb->req_cmd_byte_count = htole32(req_size);
	iocb->req_resp_byte_count = htole32(resp_size);
	iocb->req_cmd_seg.seg_addr = htole64(QLE_DMA_DVA(mem));
	iocb->req_cmd_seg.seg_len = htole32(req_size);
	iocb->req_resp_seg.seg_addr = htole64(QLE_DMA_DVA(mem) + req_size);
	iocb->req_resp_seg.seg_len = htole32(resp_size);

	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0, QLE_DMA_LEN(mem),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	qle_write(sc, QLE_REQ_IN, sc->sc_next_req_id);
	sc->sc_fabric_pending = 1;
	mtx_leave(&sc->sc_queue_mtx);

	/* maybe put a proper timeout on this */
	rv = 0;
	while (sc->sc_fabric_pending == 1) {
		if (sc->sc_scsibus == NULL) {
			u_int16_t isr, info;

			delay(100);
			if (qle_read_isr(sc, &isr, &info) != 0)
				qle_handle_intr(sc, isr, info);
		} else {
			tsleep(sc->sc_scratch, PRIBIO, "qle_fabric", 100);
		}
	}
	if (rv == 0)
		bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0,
		    QLE_DMA_LEN(mem), BUS_DMASYNC_POSTREAD |
		    BUS_DMASYNC_POSTWRITE);

	sc->sc_fabric_pending = 0;

	return (rv);
}

struct qle_fc_port *
qle_next_fabric_port(struct qle_softc *sc, u_int32_t *firstport,
    u_int32_t *lastport)
{
	struct qle_ct_ga_nxt_req *ga;
	struct qle_ct_ga_nxt_resp *gar;
	struct qle_fc_port *fport;
	int result;

	/* get the next port from the fabric nameserver */
	ga = QLE_DMA_KVA(sc->sc_scratch);
	memset(ga, 0, sizeof(*ga) + sizeof(*gar));
	ga->header.ct_revision = 0x01;
	ga->header.ct_gs_type = 0xfc;
	ga->header.ct_gs_subtype = 0x02;
	ga->subcmd = htobe16(QLE_SNS_GA_NXT);
	ga->max_word = htobe16((sizeof(*gar) - 16) / 4);
	ga->port_id = htobe32(*lastport);
	result = qle_ct_pass_through(sc, QLE_SNS_HANDLE, sc->sc_scratch,
	    sizeof(*ga), sizeof(*gar));
	if (result) {
		printf("%s: GA_NXT %x failed: %x\n", DEVNAME(sc), lastport,
		    result);
		*lastport = 0xffffffff;
		return (NULL);
	}

	gar = (struct qle_ct_ga_nxt_resp *)(ga + 1);
	/* if the response is all zeroes, try again */
	if (gar->port_type_id == 0 && gar->port_name == 0 &&
	    gar->node_name == 0) {
		printf("%s: GA_NXT returned junk\n", DEVNAME(sc));
		return (NULL);
	}

	/* are we back at the start? */
	*lastport = betoh32(gar->port_type_id) & 0xffffff;
	if (*lastport == *firstport) {
		printf("%s: got %06x again\n", DEVNAME(sc), *lastport);
		*lastport = 0xffffffff;
		return (NULL);
	}
	if (*firstport == 0xffffffff)
		*firstport = *lastport;

	printf("%s: GA_NXT: port type/id: %x, wwpn %llx, wwnn %llx\n",
	    DEVNAME(sc), *lastport, betoh64(gar->port_name),
	    betoh64(gar->node_name));

	/* don't try to log in to ourselves */
	if (*lastport == sc->sc_port_id) {
		return (NULL);
	}

	fport = malloc(sizeof(*fport), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (fport == NULL) {
		printf("%s: failed to allocate a port struct\n",
		    DEVNAME(sc));
		*lastport = 0xffffffff;
		return (NULL);
	}
	fport->port_name = betoh64(gar->port_name);
	fport->node_name = betoh64(gar->node_name);
	fport->location = QLE_LOCATION_PORT_ID(*lastport);
	fport->portid = *lastport;
	return (fport);
}


int
qle_fabric_plogi(struct qle_softc *sc, struct qle_fc_port *port)
{
	struct qle_iocb_plogx *iocb;
	u_int16_t req;
	u_int64_t offset;
	int rv;
	int loopid;

	mtx_enter(&sc->sc_port_mtx);
	loopid = qle_get_loop_id(sc);
	mtx_leave(&sc->sc_port_mtx);
	if (loopid == -1) {
		printf("%s: ran out of loop ids\n", DEVNAME(sc));
		return (1);
	}
	
	mtx_enter(&sc->sc_queue_mtx);

	req = sc->sc_next_req_id++;
	if (sc->sc_next_req_id == sc->sc_maxcmds)
		sc->sc_next_req_id = 0;

	offset = (req * QLE_QUEUE_ENTRY_SIZE);
	iocb = QLE_DMA_KVA(sc->sc_requests) + offset;
	bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(sc->sc_requests), offset,
	    QLE_QUEUE_ENTRY_SIZE, BUS_DMASYNC_POSTWRITE);
	    
	memset(iocb, 0, QLE_QUEUE_ENTRY_SIZE);
	iocb->entry_type = QLE_IOCB_PLOGX;
	iocb->entry_count = 1;

	iocb->req_handle = 7;
	iocb->req_nport_handle = htole16(loopid);
	iocb->req_port_id_lo = htole16(port->portid & 0xffff);
	iocb->req_port_id_hi = htole16(port->portid >> 16);
	iocb->req_flags = 0;

	/*qle_dump_iocb(sc, iocb);*/

	qle_write(sc, QLE_REQ_IN, sc->sc_next_req_id);
	sc->sc_fabric_pending = 1;
	mtx_leave(&sc->sc_queue_mtx);

	/* maybe put a proper timeout on this */
	rv = 0;
	while (sc->sc_fabric_pending == 1) {
		if (sc->sc_scsibus == NULL) {
			u_int16_t isr, info;

			delay(100);
			if (qle_read_isr(sc, &isr, &info) != 0)
				qle_handle_intr(sc, isr, info);
		} else {
			tsleep(sc->sc_scratch, PRIBIO, "qle_fabric", 100);
		}
	}
	sc->sc_fabric_pending = 0;

	port->loopid = loopid;
	return (rv);
}

void
qle_fabric_plogo(struct qle_softc *sc, struct qle_fc_port *port)
{
#if 0
	sc->sc_mbox[0] = QLE_MBOX_FABRIC_PLOGO;
	sc->sc_mbox[1] = port->loopid;
	sc->sc_mbox[10] = 0;

	if (qle_mbox(sc, 0x0403, 0x03))
		printf("%s: PLOGO %x failed\n", DEVNAME(sc), port->loopid);
#endif
}

void
qle_do_update(void *xsc, void *x)
{
	struct qle_softc *sc = xsc;
	int step, firstport, lastport;
	struct qle_fc_port *port;

	printf("%s: updating\n", DEVNAME(sc));
	while (sc->sc_update_tasks != 0) {
		if (sc->sc_update_tasks & QLE_UPDATE_TASK_CLEAR_ALL) {
			TAILQ_HEAD(, qle_fc_port) detach;
			printf("%s: detaching everything\n", DEVNAME(sc));

			mtx_enter(&sc->sc_port_mtx);
			qle_clear_port_lists(sc);
			TAILQ_INIT(&detach);
			while (!TAILQ_EMPTY(&sc->sc_ports)) {
				port = TAILQ_FIRST(&sc->sc_ports);
				TAILQ_REMOVE(&sc->sc_ports, port, ports);
				TAILQ_INSERT_TAIL(&detach, port, ports);
				if (port->flags & QLE_PORT_FLAG_IS_TARGET) {
					sc->sc_targets[port->loopid] = NULL;
				}
			}
			mtx_leave(&sc->sc_port_mtx);

			while (!TAILQ_EMPTY(&detach)) {
				port = TAILQ_FIRST(&detach);
				TAILQ_REMOVE(&detach, port, ports);
				if (port->flags & QLE_PORT_FLAG_IS_TARGET) {
					scsi_detach_target(sc->sc_scsibus,
					    port->loopid, -1);
					sc->sc_targets[port->loopid] = NULL;
				}
				if (port->location & QLE_LOCATION_FABRIC)
					qle_fabric_plogo(sc, port);

				free(port, M_DEVBUF);
			}

			qle_update_done(sc, QLE_UPDATE_TASK_CLEAR_ALL);
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_SOFTRESET) {
			printf("%s: attempting softreset\n", DEVNAME(sc));
			if (qle_softreset(sc) != 0) {
				printf("%s: couldn't softreset\n", DEVNAME(sc));
			}
			qle_update_done(sc, QLE_UPDATE_TASK_SOFTRESET);
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_DETACH_TARGET) {
			mtx_enter(&sc->sc_port_mtx);
			port = TAILQ_FIRST(&sc->sc_ports_gone);
			if (port != NULL) {
				sc->sc_targets[port->loopid] = NULL;
				TAILQ_REMOVE(&sc->sc_ports_gone, port, update);
				TAILQ_REMOVE(&sc->sc_ports, port, ports);
			}
			mtx_leave(&sc->sc_port_mtx);

			if (port != NULL) {
				printf("%s: detaching port %06x\n", DEVNAME(sc),
				    port->portid);
				if (sc->sc_scsibus != NULL)
					scsi_detach_target(sc->sc_scsibus,
					    port->loopid, -1);

				if (port->location & QLE_LOCATION_FABRIC)
					qle_fabric_plogo(sc, port);

				free(port, M_DEVBUF);
			} else {
				printf("%s: nothing to detach\n", DEVNAME(sc));
				qle_update_done(sc,
				    QLE_UPDATE_TASK_DETACH_TARGET);
			}
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_ATTACH_TARGET) {
			mtx_enter(&sc->sc_port_mtx);
			port = TAILQ_FIRST(&sc->sc_ports_new);
			if (port != NULL) {
				TAILQ_REMOVE(&sc->sc_ports_new, port, update);
				TAILQ_INSERT_TAIL(&sc->sc_ports, port, ports);
			}
			mtx_leave(&sc->sc_port_mtx);

			if (port != NULL) {
				if (sc->sc_scsibus != NULL)
					scsi_probe_target(sc->sc_scsibus,
					    port->loopid);
			} else {
				qle_update_done(sc,
				    QLE_UPDATE_TASK_ATTACH_TARGET);
			}
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_UPDATE_TOPO) {
			printf("%s: updating topology\n", DEVNAME(sc));
			qle_update_topology(sc);
			qle_update_done(sc, QLE_UPDATE_TASK_UPDATE_TOPO);
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_SCAN_LOOP) {
			printf("%s: starting loop scan\n", DEVNAME(sc));
			qle_clear_port_lists(sc);
			qle_update_start(sc, QLE_UPDATE_TASK_SCANNING_LOOP);
			qle_update_done(sc, QLE_UPDATE_TASK_SCAN_LOOP);
			step = 0;
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_SCANNING_LOOP) {
			printf("%s: scanning loop id %x\n", DEVNAME(sc), step);
			qle_add_loop_port(sc, step);
			if (step == sc->sc_loop_max_id) {
				qle_update_done(sc,
				    QLE_UPDATE_TASK_SCANNING_LOOP);
				qle_update_start(sc,
				    QLE_UPDATE_TASK_ATTACH_TARGET |
				    QLE_UPDATE_TASK_DETACH_TARGET);
			} else {
				step++;
			}
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_SCAN_FABRIC) {
			printf("%s: starting fabric scan\n", DEVNAME(sc));
			qle_clear_port_lists(sc);
			qle_ports_gone(sc, QLE_LOCATION_FABRIC);
			lastport = 0;
			firstport = 0xffffffff;
			step = 0;
			if (qle_update_fabric(sc))
				qle_update_start(sc,
				    QLE_UPDATE_TASK_SCANNING_FABRIC);

			qle_update_done(sc, QLE_UPDATE_TASK_SCAN_FABRIC);
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_SCANNING_FABRIC) {
			port = qle_next_fabric_port(sc, &firstport, &lastport);
			if (port != NULL) {
				struct qle_fc_port *pport = NULL;
				int disp;

				mtx_enter(&sc->sc_port_mtx);
				disp = qle_classify_port(sc, port->location,
				    port->port_name, port->node_name, &pport);
				switch (disp) {
				case QLE_PORT_DISP_CHANGED:
				case QLE_PORT_DISP_MOVED:
					/* pport cleaned up later */
				case QLE_PORT_DISP_NEW:
					printf("%s: new port %06x\n",
					    DEVNAME(sc), port->portid);
					TAILQ_INSERT_TAIL(&sc->sc_ports_found,
					    port, update);
					break;
				case QLE_PORT_DISP_DUP:
					free(port, M_DEVBUF);
					port = NULL;
					break;
				case QLE_PORT_DISP_SAME:
					printf("%s: existing port %06x\n",
					    DEVNAME(sc), port->portid);
					TAILQ_REMOVE(&sc->sc_ports_gone, pport,
					    update);
					free(port, M_DEVBUF);
					port = NULL;
					break;
				}
				mtx_leave(&sc->sc_port_mtx);
			}
			if (lastport == 0xffffffff) {
				printf("%s: finished\n", DEVNAME(sc));
				qle_update_done(sc,
				    QLE_UPDATE_TASK_SCANNING_FABRIC);
				qle_update_start(sc,
				    QLE_UPDATE_TASK_FABRIC_LOGIN);
			}
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_FABRIC_LOGIN) {
			mtx_enter(&sc->sc_port_mtx);
			port = TAILQ_FIRST(&sc->sc_ports_found);
			if (port != NULL) {
				TAILQ_REMOVE(&sc->sc_ports_found, port, update);
			}
			mtx_leave(&sc->sc_port_mtx);

			if (port != NULL) {
				printf("%s: found port %06x\n", DEVNAME(sc),
				    port->portid);
				if (qle_fabric_plogi(sc, port) == 0) {
					qle_add_fabric_port(sc, port);
				} else {
					printf("%s: plogi %x failed\n",
					    DEVNAME(sc));
					free(port, M_DEVBUF);
				}
			} else {
				printf("%s: done with logins\n", DEVNAME(sc));
				qle_update_done(sc,
				    QLE_UPDATE_TASK_FABRIC_LOGIN);
				qle_update_start(sc,
				    QLE_UPDATE_TASK_ATTACH_TARGET |
				    QLE_UPDATE_TASK_DETACH_TARGET);
			}
			continue;
		}

		if (sc->sc_update_tasks & QLE_UPDATE_TASK_FABRIC_RELOGIN) {
			/* loop across all fabric targets and redo login */
			qle_update_done(sc, QLE_UPDATE_TASK_FABRIC_RELOGIN);
			continue;
		}
	}

	printf("%s: done updating\n", DEVNAME(sc));
}

int
qle_async(struct qle_softc *sc, u_int16_t info)
{
	switch (info) {
	case QLE_ASYNC_SYSTEM_ERROR:
		qle_update_start(sc, QLE_UPDATE_TASK_SOFTRESET);
		break;

	case QLE_ASYNC_REQ_XFER_ERROR:
		qle_update_start(sc, QLE_UPDATE_TASK_SOFTRESET);
		break;

	case QLE_ASYNC_RSP_XFER_ERROR:
		qle_update_start(sc, QLE_UPDATE_TASK_SOFTRESET);
		break;

	case QLE_ASYNC_LIP_OCCURRED:
		printf("%s: lip occurred\n", DEVNAME(sc));
		break;

	case QLE_ASYNC_LOOP_UP:
		printf("%s: loop up\n", DEVNAME(sc));
		sc->sc_loop_up = 1;
		sc->sc_marker_required = 1;
		qle_update_start(sc, QLE_UPDATE_TASK_UPDATE_TOPO |
		    QLE_UPDATE_TASK_SCAN_LOOP |
		    QLE_UPDATE_TASK_SCAN_FABRIC);
		break;

	case QLE_ASYNC_LOOP_DOWN:
		printf("%s: loop down\n", DEVNAME(sc));
		sc->sc_loop_up = 0;
		qle_update_start(sc, QLE_UPDATE_TASK_CLEAR_ALL);
		break;

	case QLE_ASYNC_LIP_RESET:
		printf("%s: lip reset\n", DEVNAME(sc));
		sc->sc_marker_required = 1;
		qle_update_start(sc, QLE_UPDATE_TASK_FABRIC_RELOGIN);
		break;

	case QLE_ASYNC_PORT_DB_CHANGE:
		printf("%s: port db changed %x\n", DEVNAME(sc),
		    qle_read_mbox(sc, 1));
		qle_update_start(sc, QLE_UPDATE_TASK_SCAN_LOOP);
		break;

	case QLE_ASYNC_CHANGE_NOTIFY:
		printf("%s: name server change (%02x:%02x)\n", DEVNAME(sc),
		    qle_read_mbox(sc, 1), qle_read_mbox(sc, 2));
		qle_update_start(sc, QLE_UPDATE_TASK_SCAN_FABRIC);
		break;

	case QLE_ASYNC_LIP_F8:
		printf("%s: lip f8\n", DEVNAME(sc));
		break;

	case QLE_ASYNC_LOOP_INIT_ERROR:
		printf("%s: loop initialization error: %x", DEVNAME(sc), 
		    qle_read_mbox(sc, 1));
		break;

	case QLE_ASYNC_POINT_TO_POINT:
		printf("%s: connected in point-to-point mode\n", DEVNAME(sc));
		break;

	case QLE_ASYNC_ZIO_RESP_UPDATE:
		/* shouldn't happen, we don't do zio */
		break;

	default:
		printf("%s: unknown async %x\n", DEVNAME(sc), info);
		break;
	}
	return (1);
}

void
qle_dump_stuff(struct qle_softc *sc, void *buf, int n)
{
	u_int8_t *d = buf;
	int l;

	printf("%s: stuff\n", DEVNAME(sc));
	for (l = 0; l < n; l++) {
		printf(" %2.2x", d[l]);
		if (l % 16 == 15)
			printf("\n");
	}
	if (n % 16 != 0)
		printf("\n");
}

void
qle_dump_iocb(struct qle_softc *sc, void *buf)
{
	u_int8_t *iocb = buf;
	int l;
	int b;

	printf("%s: iocb:\n", DEVNAME(sc));
	for (l = 0; l < 4; l++) {
		for (b = 0; b < 16; b++) {
			printf(" %2.2x", iocb[(l*16)+b]);
		}
		printf("\n");
	}
}

void
qle_dump_iocb_segs(struct qle_softc *sc, void *segs, int n)
{
	u_int8_t *buf = segs;
	int s, b;
	printf("%s: iocb segs:\n", DEVNAME(sc));
	for (s = 0; s < n; s++) {
		for (b = 0; b < sizeof(struct qle_iocb_seg); b++) {
			printf(" %2.2x", buf[(s*(sizeof(struct qle_iocb_seg)))
			    + b]);
		}
		printf("\n");
	}
}

void
qle_put_marker(struct qle_softc *sc, void *buf)
{
	struct qle_iocb_marker *marker = buf;

	marker->entry_type = QLE_IOCB_MARKER;
	marker->entry_count = 1;
	marker->seqno = 0;
	marker->flags = 0;

	/* could be more specific here; isp(4) isn't */
	marker->target = 0;
	marker->modifier = QLE_IOCB_MARKER_SYNC_ALL;
}

void
qle_put_data_seg(struct qle_iocb_seg *seg, bus_dmamap_t dmap, int num)
{
	seg->seg_addr = htole64(dmap->dm_segs[num].ds_addr);
	seg->seg_len = htole32(dmap->dm_segs[num].ds_len);
}

void
qle_put_cmd(struct qle_softc *sc, void *buf, struct scsi_xfer *xs,
    struct qle_ccb *ccb, u_int32_t target_port)
{
	struct qle_iocb_req6 *req = buf;
	struct qle_fcp_cmnd *cmnd;
	u_int64_t fcp_cmnd_offset;
	u_int32_t fcp_dl;
	int seg;
	int target = xs->sc_link->target;
	int lun = xs->sc_link->lun;

	memset(req, 0, sizeof(*req));
	req->entry_type = QLE_IOCB_CMD_TYPE_6;
	req->entry_count = 1;

	req->req_handle = ccb->ccb_id;
	req->req_nport_handle = htole16(target);
	
	/*
	 * timeout is in seconds.  make sure it's at least 1 if a timeout
	 * was specified in xs
	 */
	if (xs->timeout != 0)
		req->req_timeout = htole16(MAX(1, xs->timeout/1000));

	if (xs->datalen > 0) {
		req->req_data_seg_count = htole16(ccb->ccb_dmamap->dm_nsegs);
		req->req_ctrl_flags = htole16(xs->flags & SCSI_DATA_IN ?
		    QLE_IOCB_CTRL_FLAG_READ : QLE_IOCB_CTRL_FLAG_WRITE);
		if (ccb->ccb_dmamap->dm_nsegs == 1) {
			qle_put_data_seg(&req->req_data_seg,
			    ccb->ccb_dmamap, 0);
		} else {
			req->req_ctrl_flags |=
			    htole16(QLE_IOCB_CTRL_FLAG_EXT_SEG);
			req->req_data_seg.seg_addr =
			    htole64(QLE_DMA_DVA(sc->sc_segments) +
			    ccb->ccb_seg_offset);
			req->req_data_seg.seg_len = (ccb->ccb_dmamap->dm_nsegs
			    + 1) * sizeof(struct qle_iocb_seg);
			for (seg = 0; seg < ccb->ccb_dmamap->dm_nsegs; seg++) {
				qle_put_data_seg(&ccb->ccb_segs[seg],
				    ccb->ccb_dmamap, seg);
			}
			ccb->ccb_segs[ccb->ccb_dmamap->dm_nsegs].seg_addr = 0;
			ccb->ccb_segs[ccb->ccb_dmamap->dm_nsegs].seg_len = 0;
			bus_dmamap_sync(sc->sc_dmat,
			    QLE_DMA_MAP(sc->sc_segments), ccb->ccb_seg_offset,
			    sizeof(*ccb->ccb_segs) * ccb->ccb_dmamap->dm_nsegs,
			    BUS_DMASYNC_PREWRITE);
		}
		req->req_data_len = htole32(xs->datalen);
	}
	req->req_fcp_lun[0] = htole16((lun >> 16) & 0xffff);
	req->req_fcp_lun[1] = htole16(lun & 0xffff);

	req->req_target_id = htole32(target_port & 0xffffff);

	fcp_cmnd_offset = ccb->ccb_id * sizeof(*cmnd);
	req->req_fcp_cmnd_addr = htole64(QLE_DMA_DVA(sc->sc_fcp_cmnds)
	    + fcp_cmnd_offset);

	/* set up FCP_CMND */
	cmnd = (struct qle_fcp_cmnd *)QLE_DMA_KVA(sc->sc_fcp_cmnds) +
	    ccb->ccb_id;

	memset(cmnd, 0, sizeof(*cmnd));
	memcpy(cmnd->fcp_lun, req->req_fcp_lun, sizeof(cmnd->fcp_lun));
	/* cmnd->fcp_task_attr = TSK_SIMPLE; */
	/* cmnd->fcp_task_mgmt = 0; */
	memcpy(cmnd->fcp_cdb, xs->cmd, xs->cmdlen);

	/* FCP_DL goes after the cdb */
	fcp_dl = htobe32(xs->datalen);
	if (xs->cmdlen > 16) {
		req->req_fcp_cmnd_len = htole16(12 + xs->cmdlen + 4);
		cmnd->fcp_add_cdb_len = xs->cmdlen - 16;
		memcpy(cmnd->fcp_cdb + xs->datalen, &fcp_dl, sizeof(fcp_dl));
	} else {
		req->req_fcp_cmnd_len = htole16(12 + 16 + 4);
		cmnd->fcp_add_cdb_len = 0;
		memcpy(cmnd->fcp_cdb + 16, &fcp_dl, sizeof(fcp_dl));
	}
	if (xs->datalen > 0)
		cmnd->fcp_add_cdb_len |= (xs->flags & SCSI_DATA_IN) ? 2 : 1;
	
	bus_dmamap_sync(sc->sc_dmat,
	    QLE_DMA_MAP(sc->sc_fcp_cmnds), fcp_cmnd_offset,
	    sizeof(*cmnd), BUS_DMASYNC_PREWRITE);
}

int
qle_load_fwchunk(struct qle_softc *sc, struct qle_dmamem *mem,
    const u_int32_t *src)
{
	u_int32_t dest, done, total;
	int i;

	dest = src[2];
	done = 0;
	total = src[3];

	while (done < total) {
		u_int32_t *copy;
		u_int32_t words;

		/* limit transfer size otherwise it just doesn't work */
		words = MIN(total - done, 1 << 10);
		copy = QLE_DMA_KVA(mem);
		for (i = 0; i < words; i++) {
			copy[i] = htole32(src[done++]);
		}
		bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0, words * 4,
		    BUS_DMASYNC_PREWRITE);

		sc->sc_mbox[0] = QLE_MBOX_LOAD_RISC_RAM;
		sc->sc_mbox[1] = dest;
		sc->sc_mbox[4] = words >> 16;
		sc->sc_mbox[5] = words & 0xffff;
		sc->sc_mbox[8] = dest >> 16;
		qle_mbox_putaddr(sc->sc_mbox, mem);
		if (qle_mbox(sc, 0x01ff, 0x0001)) {
			printf("firmware load failed\n");
			return (1);
		}
		bus_dmamap_sync(sc->sc_dmat, QLE_DMA_MAP(mem), 0, words * 4,
		    BUS_DMASYNC_POSTWRITE);

		dest += words;
	}

	sc->sc_mbox[0] = QLE_MBOX_VERIFY_CSUM;
	sc->sc_mbox[1] = src[2] >> 16;
	sc->sc_mbox[2] = src[2];
	if (qle_mbox(sc, 0x0007, 0x0007)) {
		printf("verification of chunk at %x failed: %x %x\n", src[2],
		    sc->sc_mbox[1], sc->sc_mbox[2]);
		return (1);
	}

	return (0);
}

int
qle_load_firmware_chunks(struct qle_softc *sc, const u_int32_t *fw)
{
	struct qle_dmamem *mem;

	mem = qle_dmamem_alloc(sc, 65536);
	for (;;) {
		qle_load_fwchunk(sc, mem, fw);
		if (fw[1] == 0)
			break;
		fw += fw[3];
	}

	qle_dmamem_free(sc, mem);
	return (0);
}

int
qle_read_nvram(struct qle_softc *sc)
{
	u_int32_t data[sizeof(sc->sc_nvram) / 4];
	u_int32_t csum, tmp, v;
	int i, base, l;

	switch (sc->sc_isp_gen) {
	case QLE_GEN_ISP24XX:
		base = 0x7ffe0080;
		break;
	case QLE_GEN_ISP25XX:
		base = 0x7ff48080;
		break;
	}
	base += sc->sc_port * 0x100;
	
	csum = 0;
	for (i = 0; i < nitems(data); i++) {
		data[i] = 0xffffffff;
		qle_write(sc, QLE_FLASH_NVRAM_ADDR, base + i);
		for (l = 0; l < 5000; l++) {
			delay(10);
			tmp = qle_read(sc, QLE_FLASH_NVRAM_ADDR);
			if (tmp & (1U << 31)) {
				v = qle_read(sc, QLE_FLASH_NVRAM_DATA);
				csum += v;
				data[i] = letoh32(v);
				break;
			}
		}
	}

	bcopy(data, &sc->sc_nvram, sizeof(sc->sc_nvram));
	/* id field should be 'ISP' */
	if (sc->sc_nvram.id[0] != 'I' || sc->sc_nvram.id[1] != 'S' ||
	    sc->sc_nvram.id[2] != 'P' || csum != 0) {
		printf("%s: nvram corrupt\n", DEVNAME(sc));
		return (1);
	}
	return (0);
}

struct qle_dmamem *
qle_dmamem_alloc(struct qle_softc *sc, size_t size)
{
	struct qle_dmamem *m;
	int nsegs;

	m = malloc(sizeof(*m), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (m == NULL)
		return (NULL);

	m->qdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &m->qdm_map) != 0)
		goto qdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &m->qdm_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &m->qdm_seg, nsegs, size, &m->qdm_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, m->qdm_map, m->qdm_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (m);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, m->qdm_kva, m->qdm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &m->qdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, m->qdm_map);
qdmfree:
	free(m, M_DEVBUF);

	return (NULL);
}

void
qle_dmamem_free(struct qle_softc *sc, struct qle_dmamem *m)
{
	bus_dmamap_unload(sc->sc_dmat, m->qdm_map);
	bus_dmamem_unmap(sc->sc_dmat, m->qdm_kva, m->qdm_size);
	bus_dmamem_free(sc->sc_dmat, &m->qdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, m->qdm_map);
	free(m, M_DEVBUF);
}

int
qle_alloc_ccbs(struct qle_softc *sc)
{
	struct qle_ccb		*ccb;
	u_int8_t		*cmd;
	int			i;

	SIMPLEQ_INIT(&sc->sc_ccb_free);
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	mtx_init(&sc->sc_queue_mtx, IPL_BIO);
	mtx_init(&sc->sc_port_mtx, IPL_BIO);
	mtx_init(&sc->sc_mbox_mtx, IPL_BIO);

	sc->sc_ccbs = malloc(sizeof(struct qle_ccb) * sc->sc_maxcmds,
	    M_DEVBUF, M_WAITOK | M_CANFAIL | M_ZERO);
	if (sc->sc_ccbs == NULL) {
		printf("%s: unable to allocate ccbs\n", DEVNAME(sc));
		return (1);
	}

	sc->sc_requests = qle_dmamem_alloc(sc, sc->sc_maxcmds *
	    QLE_QUEUE_ENTRY_SIZE);
	if (sc->sc_requests == NULL) {
		printf("%s: unable to allocate ccb dmamem\n", DEVNAME(sc));
		goto free_ccbs;
	}
	sc->sc_responses = qle_dmamem_alloc(sc, sc->sc_maxcmds *
	    QLE_QUEUE_ENTRY_SIZE);
	if (sc->sc_responses == NULL) {
		printf("%s: unable to allocate rcb dmamem\n", DEVNAME(sc));
		goto free_req;
	}
	sc->sc_pri_requests = qle_dmamem_alloc(sc, 8 * QLE_QUEUE_ENTRY_SIZE);
	if (sc->sc_pri_requests == NULL) {
		printf("%s: unable to allocate pri ccb dmamem\n", DEVNAME(sc));
		goto free_pri;
	}
	sc->sc_segments = qle_dmamem_alloc(sc, sc->sc_maxcmds * QLE_MAX_SEGS *
	    sizeof(struct qle_iocb_seg));
	if (sc->sc_segments == NULL) {
		printf("%s: unable to allocate iocb segments\n", DEVNAME(sc));
		goto free_res;
	}

	sc->sc_fcp_cmnds = qle_dmamem_alloc(sc, sc->sc_maxcmds *
	    sizeof(struct qle_fcp_cmnd));
	if (sc->sc_fcp_cmnds == NULL) {
		printf("%s: unable to allocate FCP_CMNDs\n", DEVNAME(sc));
		goto free_seg;
	}

	cmd = QLE_DMA_KVA(sc->sc_requests);
	memset(cmd, 0, QLE_QUEUE_ENTRY_SIZE * sc->sc_maxcmds);
	for (i = 0; i < sc->sc_maxcmds; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS,
		    QLE_MAX_SEGS, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dma map\n", DEVNAME(sc));
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_id = i;

		ccb->ccb_seg_offset = i * QLE_MAX_SEGS *
		    sizeof(struct qle_iocb_seg);
		ccb->ccb_segs = QLE_DMA_KVA(sc->sc_segments) +
		    ccb->ccb_seg_offset;

		qle_put_ccb(sc, ccb);
	}

	scsi_iopool_init(&sc->sc_iopool, sc, qle_get_ccb, qle_put_ccb);
	return (0);

free_maps:
	while ((ccb = qle_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	qle_dmamem_free(sc, sc->sc_fcp_cmnds);
free_seg:
	qle_dmamem_free(sc, sc->sc_segments);
free_pri:
	qle_dmamem_free(sc, sc->sc_pri_requests);
free_res:
	qle_dmamem_free(sc, sc->sc_responses);
free_req:
	qle_dmamem_free(sc, sc->sc_requests);
free_ccbs:
	free(sc->sc_ccbs, M_DEVBUF);

	return (1);
}

void
qle_free_ccbs(struct qle_softc *sc)
{
	struct qle_ccb		*ccb;

	scsi_iopool_destroy(&sc->sc_iopool);
	while ((ccb = qle_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	qle_dmamem_free(sc, sc->sc_segments);
	qle_dmamem_free(sc, sc->sc_responses);
	qle_dmamem_free(sc, sc->sc_requests);
	free(sc->sc_ccbs, M_DEVBUF);
}

void *
qle_get_ccb(void *xsc)
{
	struct qle_softc 	*sc = xsc;
	struct qle_ccb		*ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = SIMPLEQ_FIRST(&sc->sc_ccb_free);
	if (ccb != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_free, ccb_link);
	}
	mtx_leave(&sc->sc_ccb_mtx);
	return (ccb);
}

void
qle_put_ccb(void *xsc, void *io)
{
	struct qle_softc	*sc = xsc;
	struct qle_ccb		*ccb = io;

	ccb->ccb_xs = NULL;
	mtx_enter(&sc->sc_ccb_mtx);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_free, ccb, ccb_link);
	mtx_leave(&sc->sc_ccb_mtx);
}
