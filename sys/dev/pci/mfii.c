/* $OpenBSD: mfii.c,v 1.51 2018/05/18 05:11:53 jmatthew Exp $ */

/*
 * Copyright (c) 2012 David Gwynne <dlg@openbsd.org>
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
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/task.h>
#include <sys/atomic.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/ic/mfireg.h>
#include <dev/pci/mpiireg.h>

#define	MFII_BAR		0x14
#define MFII_BAR_35		0x10
#define	MFII_PCI_MEMSIZE	0x2000 /* 8k */

#define MFII_OSTS_INTR_VALID	0x00000009
#define MFII_RPI		0x6c /* reply post host index */
#define MFII_OSP2		0xb4 /* outbound scratch pad 2 */
#define MFII_OSP3		0xb8 /* outbound scratch pad 3 */

#define MFII_REQ_TYPE_SCSI	MPII_REQ_DESCR_SCSI_IO
#define MFII_REQ_TYPE_LDIO	(0x7 << 1)
#define MFII_REQ_TYPE_MFA	(0x1 << 1)
#define MFII_REQ_TYPE_NO_LOCK	(0x2 << 1)
#define MFII_REQ_TYPE_HI_PRI	(0x6 << 1)

#define MFII_REQ_MFA(_a)	htole64((_a) | MFII_REQ_TYPE_MFA)

#define MFII_FUNCTION_PASSTHRU_IO			(0xf0)
#define MFII_FUNCTION_LDIO_REQUEST			(0xf1)

#define MFII_MAX_CHAIN_UNIT	0x00400000
#define MFII_MAX_CHAIN_MASK	0x000003E0
#define MFII_MAX_CHAIN_SHIFT	5

#define MFII_256K_IO		128
#define MFII_1MB_IO		(MFII_256K_IO * 4)

#define MFII_CHAIN_FRAME_MIN	1024

struct mfii_request_descr {
	u_int8_t	flags;
	u_int8_t	msix_index;
	u_int16_t	smid;

	u_int16_t	lmid;
	u_int16_t	dev_handle;
} __packed;

#define MFII_RAID_CTX_IO_TYPE_SYSPD	(0x1 << 4)
#define MFII_RAID_CTX_TYPE_CUDA		(0x2 << 4)

struct mfii_raid_context {
	u_int8_t	type_nseg;
	u_int8_t	_reserved1;
	u_int16_t	timeout_value;

	u_int16_t	reg_lock_flags;
#define MFII_RAID_CTX_RL_FLAGS_SEQNO_EN	(0x08)
#define MFII_RAID_CTX_RL_FLAGS_CPU0	(0x00)
#define MFII_RAID_CTX_RL_FLAGS_CPU1	(0x10)
#define MFII_RAID_CTX_RL_FLAGS_CUDA	(0x80)

#define MFII_RAID_CTX_ROUTING_FLAGS_SQN	(1 << 4)
#define MFII_RAID_CTX_ROUTING_FLAGS_CPU0 0
	u_int16_t	virtual_disk_target_id;

	u_int64_t	reg_lock_row_lba;

	u_int32_t	reg_lock_length;

	u_int16_t	next_lm_id;
	u_int8_t	ex_status;
	u_int8_t	status;

	u_int8_t	raid_flags;
	u_int8_t	num_sge;
	u_int16_t	config_seq_num;

	u_int8_t	span_arm;
	u_int8_t	_reserved3[3];
} __packed;

struct mfii_sge {
	u_int64_t	sg_addr;
	u_int32_t	sg_len;
	u_int16_t	_reserved;
	u_int8_t	sg_next_chain_offset;
	u_int8_t	sg_flags;
} __packed;

#define MFII_SGE_ADDR_MASK		(0x03)
#define MFII_SGE_ADDR_SYSTEM		(0x00)
#define MFII_SGE_ADDR_IOCDDR		(0x01)
#define MFII_SGE_ADDR_IOCPLB		(0x02)
#define MFII_SGE_ADDR_IOCPLBNTA		(0x03)
#define MFII_SGE_END_OF_LIST		(0x40)
#define MFII_SGE_CHAIN_ELEMENT		(0x80)

#define MFII_REQUEST_SIZE	256

#define MR_DCMD_LD_MAP_GET_INFO			0x0300e101

#define MFII_MAX_ROW		32
#define MFII_MAX_ARRAY		128

struct mfii_array_map {
	uint16_t		mam_pd[MFII_MAX_ROW];
} __packed;

struct mfii_dev_handle {
	uint16_t		mdh_cur_handle;
	uint8_t			mdh_valid;
	uint8_t			mdh_reserved;
	uint16_t		mdh_handle[2];
} __packed;

struct mfii_ld_map {
	uint32_t		mlm_total_size;
	uint32_t		mlm_reserved1[5];
	uint32_t		mlm_num_lds;
	uint32_t		mlm_reserved2;
	uint8_t			mlm_tgtid_to_ld[2 * MFI_MAX_LD];
	uint8_t			mlm_pd_timeout;
	uint8_t			mlm_reserved3[7];
	struct mfii_array_map	mlm_am[MFII_MAX_ARRAY];
	struct mfii_dev_handle	mlm_dev_handle[MFI_MAX_PD];
} __packed;

struct mfii_task_mgmt {
	union {
		uint8_t			request[128];
		struct mpii_msg_scsi_task_request
					mpii_request;
	} __packed __aligned(8);

	union {
		uint8_t			reply[128];
		uint32_t		flags;
#define MFII_TASK_MGMT_FLAGS_LD				(1 << 0)
#define MFII_TASK_MGMT_FLAGS_PD				(1 << 1)
		struct mpii_msg_scsi_task_reply
					mpii_reply;
	} __packed __aligned(8);
} __packed __aligned(8);

struct mfii_dmamem {
	bus_dmamap_t		mdm_map;
	bus_dma_segment_t	mdm_seg;
	size_t			mdm_size;
	caddr_t			mdm_kva;
};
#define MFII_DMA_MAP(_mdm)	((_mdm)->mdm_map)
#define MFII_DMA_LEN(_mdm)	((_mdm)->mdm_size)
#define MFII_DMA_DVA(_mdm)	((u_int64_t)(_mdm)->mdm_map->dm_segs[0].ds_addr)
#define MFII_DMA_KVA(_mdm)	((void *)(_mdm)->mdm_kva)

struct mfii_softc;

struct mfii_ccb {
	void			*ccb_request;
	u_int64_t		ccb_request_dva;
	bus_addr_t		ccb_request_offset;

	void			*ccb_mfi;
	u_int64_t		ccb_mfi_dva;
	bus_addr_t		ccb_mfi_offset;

	struct mfi_sense	*ccb_sense;
	u_int64_t		ccb_sense_dva;
	bus_addr_t		ccb_sense_offset;

	struct mfii_sge		*ccb_sgl;
	u_int64_t		ccb_sgl_dva;
	bus_addr_t		ccb_sgl_offset;
	u_int			ccb_sgl_len;

	struct mfii_request_descr ccb_req;

	bus_dmamap_t		ccb_dmamap;

	/* data for sgl */
	void			*ccb_data;
	size_t			ccb_len;

	int			ccb_direction;
#define MFII_DATA_NONE			0
#define MFII_DATA_IN			1
#define MFII_DATA_OUT			2

	void			*ccb_cookie;
	void			(*ccb_done)(struct mfii_softc *,
				    struct mfii_ccb *);

	u_int32_t		ccb_flags;
#define MFI_CCB_F_ERR			(1<<0)
	u_int			ccb_smid;
	u_int			ccb_refcnt;
	SIMPLEQ_ENTRY(mfii_ccb)	ccb_link;
};
SIMPLEQ_HEAD(mfii_ccb_list, mfii_ccb);

struct mfii_pd_softc {
	struct scsi_link	pd_link;
	struct scsibus_softc	*pd_scsibus;
	struct srp		pd_dev_handles;
	uint8_t			pd_timeout;
};

struct mfii_iop {
	int bar;
	int num_sge_loc;
#define MFII_IOP_NUM_SGE_LOC_ORIG	0
#define MFII_IOP_NUM_SGE_LOC_35		1
	u_int16_t ldio_ctx_reg_lock_flags;
	u_int8_t ldio_req_type;
	u_int8_t ldio_ctx_type_nseg;
	u_int8_t sge_flag_chain;
	u_int8_t sge_flag_eol;
};

struct mfii_softc {
	struct device		sc_dev;
	const struct mfii_iop	*sc_iop;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	void			*sc_ih;

	struct mutex		sc_ccb_mtx;
	struct mutex		sc_post_mtx;

	u_int			sc_max_fw_cmds;
	u_int			sc_max_cmds;
	u_int			sc_max_sgl;

	u_int			sc_reply_postq_depth;
	u_int			sc_reply_postq_index;
	struct mutex		sc_reply_postq_mtx;
	struct mfii_dmamem	*sc_reply_postq;

	struct mfii_dmamem	*sc_requests;
	struct mfii_dmamem	*sc_mfi;
	struct mfii_dmamem	*sc_sense;
	struct mfii_dmamem	*sc_sgl;

	struct mfii_ccb		*sc_ccb;
	struct mfii_ccb_list	sc_ccb_freeq;

	struct mfii_ccb		*sc_aen_ccb;
	struct task		sc_aen_task;

	struct mutex		sc_abort_mtx;
	struct mfii_ccb_list	sc_abort_list;
	struct task		sc_abort_task;

	struct scsi_link	sc_link;
	struct scsibus_softc	*sc_scsibus;
	struct mfii_pd_softc	*sc_pd;
	struct scsi_iopool	sc_iopool;

	struct mfi_ctrl_info	sc_info;
};

int		mfii_match(struct device *, void *, void *);
void		mfii_attach(struct device *, struct device *, void *);
int		mfii_detach(struct device *, int);

struct cfattach mfii_ca = {
	sizeof(struct mfii_softc),
	mfii_match,
	mfii_attach,
	mfii_detach
};

struct cfdriver mfii_cd = {
	NULL,
	"mfii",
	DV_DULL
};

void		mfii_scsi_cmd(struct scsi_xfer *);
void		mfii_scsi_cmd_done(struct mfii_softc *, struct mfii_ccb *);

struct scsi_adapter mfii_switch = {
	mfii_scsi_cmd,
	scsi_minphys,
	NULL, /* probe */
	NULL, /* unprobe */
	NULL  /* ioctl */
};

void		mfii_pd_scsi_cmd(struct scsi_xfer *);
int		mfii_pd_scsi_probe(struct scsi_link *);

struct scsi_adapter mfii_pd_switch = {
	mfii_pd_scsi_cmd,
	scsi_minphys,
	mfii_pd_scsi_probe
};

#define DEVNAME(_sc)		((_sc)->sc_dev.dv_xname)

u_int32_t		mfii_read(struct mfii_softc *, bus_size_t);
void			mfii_write(struct mfii_softc *, bus_size_t, u_int32_t);

struct mfii_dmamem *	mfii_dmamem_alloc(struct mfii_softc *, size_t);
void			mfii_dmamem_free(struct mfii_softc *,
			    struct mfii_dmamem *);

void *			mfii_get_ccb(void *);
void			mfii_put_ccb(void *, void *);
int			mfii_init_ccb(struct mfii_softc *);
void			mfii_scrub_ccb(struct mfii_ccb *);

int			mfii_transition_firmware(struct mfii_softc *);
int			mfii_initialise_firmware(struct mfii_softc *);
int			mfii_get_info(struct mfii_softc *);
int			mfii_syspd(struct mfii_softc *);

void			mfii_start(struct mfii_softc *, struct mfii_ccb *);
void			mfii_done(struct mfii_softc *, struct mfii_ccb *);
int			mfii_poll(struct mfii_softc *, struct mfii_ccb *);
void			mfii_poll_done(struct mfii_softc *, struct mfii_ccb *);
int			mfii_exec(struct mfii_softc *, struct mfii_ccb *);
void			mfii_exec_done(struct mfii_softc *, struct mfii_ccb *);
int			mfii_my_intr(struct mfii_softc *);
int			mfii_intr(void *);
void			mfii_postq(struct mfii_softc *);

int			mfii_load_ccb(struct mfii_softc *, struct mfii_ccb *,
			    void *, int);
int			mfii_load_mfa(struct mfii_softc *, struct mfii_ccb *,
			    void *, int);

int			mfii_mfa_poll(struct mfii_softc *, struct mfii_ccb *);

int			mfii_mgmt(struct mfii_softc *, struct mfii_ccb *,
			    u_int32_t, const union mfi_mbox *,
			    void *, size_t, int);
void			mfii_empty_done(struct mfii_softc *, struct mfii_ccb *);

int			mfii_scsi_cmd_io(struct mfii_softc *,
			    struct scsi_xfer *);
int			mfii_scsi_cmd_cdb(struct mfii_softc *,
			    struct scsi_xfer *);
int			mfii_pd_scsi_cmd_cdb(struct mfii_softc *,
			    struct scsi_xfer *);
void			mfii_scsi_cmd_tmo(void *);

int			mfii_dev_handles_update(struct mfii_softc *sc);
void			mfii_dev_handles_dtor(void *, void *);

void			mfii_abort_task(void *);
void			mfii_abort(struct mfii_softc *, struct mfii_ccb *,
			    uint16_t, uint16_t, uint8_t, uint32_t);
void			mfii_scsi_cmd_abort_done(struct mfii_softc *,
			    struct mfii_ccb *);

int			mfii_aen_register(struct mfii_softc *);
void			mfii_aen_start(struct mfii_softc *, struct mfii_ccb *,
			    struct mfii_dmamem *, uint32_t);
void			mfii_aen_done(struct mfii_softc *, struct mfii_ccb *);
void			mfii_aen(void *);
void			mfii_aen_unregister(struct mfii_softc *);

void			mfii_aen_pd_insert(struct mfii_softc *,
			    const struct mfi_evtarg_pd_address *);
void			mfii_aen_pd_remove(struct mfii_softc *,
			    const struct mfi_evtarg_pd_address *);
void			mfii_aen_pd_state_change(struct mfii_softc *,
			    const struct mfi_evtarg_pd_state *);

/*
 * mfii boards support asynchronous (and non-polled) completion of
 * dcmds by proxying them through a passthru mpii command that points
 * at a dcmd frame. since the passthru command is submitted like
 * the scsi commands using an SMID in the request descriptor,
 * ccb_request memory * must contain the passthru command because
 * that is what the SMID refers to. this means ccb_request cannot
 * contain the dcmd. rather than allocating separate dma memory to
 * hold the dcmd, we reuse the sense memory buffer for it.
 */

void			mfii_dcmd_start(struct mfii_softc *,
			    struct mfii_ccb *);

static inline void
mfii_dcmd_scrub(struct mfii_ccb *ccb)
{
	memset(ccb->ccb_sense, 0, sizeof(*ccb->ccb_sense));
}

static inline struct mfi_dcmd_frame *
mfii_dcmd_frame(struct mfii_ccb *ccb)
{
	CTASSERT(sizeof(struct mfi_dcmd_frame) <= sizeof(*ccb->ccb_sense));
	return ((struct mfi_dcmd_frame *)ccb->ccb_sense);
}

static inline void
mfii_dcmd_sync(struct mfii_softc *sc, struct mfii_ccb *ccb, int flags)
{
	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_sense),
	    ccb->ccb_sense_offset, sizeof(*ccb->ccb_sense), flags);
}

#define mfii_fw_state(_sc) mfii_read((_sc), MFI_OSP)

const struct mfii_iop mfii_iop_thunderbolt = {
	MFII_BAR,
	MFII_IOP_NUM_SGE_LOC_ORIG,
	0,
	MFII_REQ_TYPE_LDIO,
	0,
	MFII_SGE_CHAIN_ELEMENT | MFII_SGE_ADDR_IOCPLBNTA,
	0
};

/*
 * a lot of these values depend on us not implementing fastpath yet.
 */
const struct mfii_iop mfii_iop_25 = {
	MFII_BAR,
	MFII_IOP_NUM_SGE_LOC_ORIG,
	MFII_RAID_CTX_RL_FLAGS_CPU0, /* | MFII_RAID_CTX_RL_FLAGS_SEQNO_EN */
	MFII_REQ_TYPE_NO_LOCK,
	MFII_RAID_CTX_TYPE_CUDA | 0x1,
	MFII_SGE_CHAIN_ELEMENT,
	MFII_SGE_END_OF_LIST
};

const struct mfii_iop mfii_iop_35 = {
	MFII_BAR_35,
	MFII_IOP_NUM_SGE_LOC_35,
	MFII_RAID_CTX_ROUTING_FLAGS_CPU0, /* | MFII_RAID_CTX_ROUTING_FLAGS_SQN */
	MFII_REQ_TYPE_NO_LOCK,
	MFII_RAID_CTX_TYPE_CUDA | 0x1,
	MFII_SGE_CHAIN_ELEMENT,
	MFII_SGE_END_OF_LIST
};

struct mfii_device {
	pcireg_t		mpd_vendor;
	pcireg_t		mpd_product;
	const struct mfii_iop	*mpd_iop;
};

const struct mfii_device mfii_devices[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_2208,
	    &mfii_iop_thunderbolt },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3008,
	    &mfii_iop_25 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3108,
	    &mfii_iop_25 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3404,
	    &mfii_iop_35 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3504,
	    &mfii_iop_35 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3408,
	    &mfii_iop_35 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3508,
	    &mfii_iop_35 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3416,
	    &mfii_iop_35 },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_3516,
	    &mfii_iop_35 }
};

const struct mfii_iop *mfii_find_iop(struct pci_attach_args *);

const struct mfii_iop *
mfii_find_iop(struct pci_attach_args *pa)
{
	const struct mfii_device *mpd;
	int i;

	for (i = 0; i < nitems(mfii_devices); i++) {
		mpd = &mfii_devices[i];

		if (mpd->mpd_vendor == PCI_VENDOR(pa->pa_id) &&
		    mpd->mpd_product == PCI_PRODUCT(pa->pa_id))
			return (mpd->mpd_iop);
	}

	return (NULL);
}

int
mfii_match(struct device *parent, void *match, void *aux)
{
	return ((mfii_find_iop(aux) != NULL) ? 1 : 0);
}

void
mfii_attach(struct device *parent, struct device *self, void *aux)
{
	struct mfii_softc *sc = (struct mfii_softc *)self;
	struct pci_attach_args *pa = aux;
	pcireg_t memtype;
	pci_intr_handle_t ih;
	struct scsibus_attach_args saa;
	u_int32_t status, scpad2, scpad3;
	int chain_frame_sz, nsge_in_io, nsge_in_chain;

	/* init sc */
	sc->sc_iop = mfii_find_iop(aux);
	sc->sc_dmat = pa->pa_dmat;
	SIMPLEQ_INIT(&sc->sc_ccb_freeq);
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);
	mtx_init(&sc->sc_post_mtx, IPL_BIO);
	mtx_init(&sc->sc_reply_postq_mtx, IPL_BIO);
	scsi_iopool_init(&sc->sc_iopool, sc, mfii_get_ccb, mfii_put_ccb);

	sc->sc_aen_ccb = NULL;
	task_set(&sc->sc_aen_task, mfii_aen, sc);

	mtx_init(&sc->sc_abort_mtx, IPL_BIO);
	SIMPLEQ_INIT(&sc->sc_abort_list);
	task_set(&sc->sc_abort_task, mfii_abort_task, sc);

	/* wire up the bus shizz */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, sc->sc_iop->bar);
	if (pci_mapreg_map(pa, sc->sc_iop->bar, memtype, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &sc->sc_ios, MFII_PCI_MEMSIZE)) {
		printf(": unable to map registers\n");
		return;
	}

	/* disable interrupts */
	mfii_write(sc, MFI_OMSK, 0xffffffff);

	if (pci_intr_map_msi(pa, &ih) != 0 && pci_intr_map(pa, &ih) != 0) {
		printf(": unable to map interrupt\n");
		goto pci_unmap;
	}
	printf(": %s\n", pci_intr_string(pa->pa_pc, ih));

	/* lets get started */
	if (mfii_transition_firmware(sc))
		goto pci_unmap;

	/* determine max_cmds (refer to the Linux megaraid_sas driver) */
	scpad3 = mfii_read(sc, MFII_OSP3);
	status = mfii_fw_state(sc);
	sc->sc_max_fw_cmds = scpad3 & MFI_STATE_MAXCMD_MASK;
	if (sc->sc_max_fw_cmds == 0)
		sc->sc_max_fw_cmds = status & MFI_STATE_MAXCMD_MASK;
	/*
	 * reduce max_cmds by 1 to ensure that the reply queue depth does not
	 * exceed FW supplied max_fw_cmds.
	 */
	sc->sc_max_cmds = min(sc->sc_max_fw_cmds, 1024) - 1;

	/* determine max_sgl (refer to the Linux megaraid_sas driver) */
	scpad2 = mfii_read(sc, MFII_OSP2);
	chain_frame_sz =
		((scpad2 & MFII_MAX_CHAIN_MASK) >> MFII_MAX_CHAIN_SHIFT) *
		((scpad2 & MFII_MAX_CHAIN_UNIT) ? MFII_1MB_IO : MFII_256K_IO);
	if (chain_frame_sz < MFII_CHAIN_FRAME_MIN)
		chain_frame_sz = MFII_CHAIN_FRAME_MIN;

	nsge_in_io = (MFII_REQUEST_SIZE -
		sizeof(struct mpii_msg_scsi_io) -
		sizeof(struct mfii_raid_context)) / sizeof(struct mfii_sge);
	nsge_in_chain = chain_frame_sz / sizeof(struct mfii_sge);

	/* round down to nearest power of two */
	sc->sc_max_sgl = 1;
	while ((sc->sc_max_sgl << 1) <= (nsge_in_io + nsge_in_chain))
		sc->sc_max_sgl <<= 1;

#ifdef MFI_DEBUG
	printf("%s: OSP 0x%08x, OSP2 0x%08x, OSP3 0x%08x\n",
	    DEVNAME(sc), status, scpad2, scpad3);
	printf("%s: max_fw_cmds %d, max_cmds %d\n",
	    DEVNAME(sc), sc->sc_max_fw_cmds, sc->sc_max_cmds);
	printf("%s: nsge_in_io %d, nsge_in_chain %d, "
	    "max_sgl %d\n", DEVNAME(sc), nsge_in_io, nsge_in_chain,
	    sc->sc_max_sgl);
#endif

	/* sense memory */
	CTASSERT(sizeof(struct mfi_sense) == MFI_SENSE_SIZE);
	sc->sc_sense = mfii_dmamem_alloc(sc, sc->sc_max_cmds * MFI_SENSE_SIZE);
	if (sc->sc_sense == NULL) {
		printf("%s: unable to allocate sense memory\n", DEVNAME(sc));
		goto pci_unmap;
	}

	/* reply post queue */
	sc->sc_reply_postq_depth = roundup(sc->sc_max_fw_cmds, 16);

	sc->sc_reply_postq = mfii_dmamem_alloc(sc,
	    sc->sc_reply_postq_depth * sizeof(struct mpii_reply_descr));
	if (sc->sc_reply_postq == NULL)
		goto free_sense;

	memset(MFII_DMA_KVA(sc->sc_reply_postq), 0xff,
	    MFII_DMA_LEN(sc->sc_reply_postq));

	/* MPII request frame array */
	sc->sc_requests = mfii_dmamem_alloc(sc,
	    MFII_REQUEST_SIZE * (sc->sc_max_cmds + 1));
	if (sc->sc_requests == NULL)
		goto free_reply_postq;

	/* MFI command frame array */
	sc->sc_mfi = mfii_dmamem_alloc(sc, sc->sc_max_cmds * MFI_FRAME_SIZE);
	if (sc->sc_mfi == NULL)
		goto free_requests;

	/* MPII SGL array */
	sc->sc_sgl = mfii_dmamem_alloc(sc, sc->sc_max_cmds *
	    sizeof(struct mfii_sge) * sc->sc_max_sgl);
	if (sc->sc_sgl == NULL)
		goto free_mfi;

	if (mfii_init_ccb(sc) != 0) {
		printf("%s: could not init ccb list\n", DEVNAME(sc));
		goto free_sgl;
	}

	/* kickstart firmware with all addresses and pointers */
	if (mfii_initialise_firmware(sc) != 0) {
		printf("%s: could not initialize firmware\n", DEVNAME(sc));
		goto free_sgl;
	}

	if (mfii_get_info(sc) != 0) {
		printf("%s: could not retrieve controller information\n",
		    DEVNAME(sc));
		goto free_sgl;
	}

	printf("%s: \"%s\", firmware %s", DEVNAME(sc),
	    sc->sc_info.mci_product_name, sc->sc_info.mci_package_version);
	if (letoh16(sc->sc_info.mci_memory_size) > 0)
		printf(", %uMB cache", letoh16(sc->sc_info.mci_memory_size));
	printf("\n");

	sc->sc_ih = pci_intr_establish(sc->sc_pc, ih, IPL_BIO,
	    mfii_intr, sc, DEVNAME(sc));
	if (sc->sc_ih == NULL)
		goto free_sgl;

	sc->sc_link.openings = sc->sc_max_cmds;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &mfii_switch;
	sc->sc_link.adapter_target = sc->sc_info.mci_max_lds;
	sc->sc_link.adapter_buswidth = sc->sc_info.mci_max_lds;
	sc->sc_link.pool = &sc->sc_iopool;

	memset(&saa, 0, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dev, &saa, scsiprint);

	mfii_syspd(sc);

	if (mfii_aen_register(sc) != 0) {
		/* error printed by mfii_aen_register */
		goto intr_disestablish;
	}

	/* enable interrupts */
	mfii_write(sc, MFI_OSTS, 0xffffffff);
	mfii_write(sc, MFI_OMSK, ~MFII_OSTS_INTR_VALID);

	return;
intr_disestablish:
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
free_sgl:
	mfii_dmamem_free(sc, sc->sc_sgl);
free_mfi:
	mfii_dmamem_free(sc, sc->sc_mfi);
free_requests:
	mfii_dmamem_free(sc, sc->sc_requests);
free_reply_postq:
	mfii_dmamem_free(sc, sc->sc_reply_postq);
free_sense:
	mfii_dmamem_free(sc, sc->sc_sense);
pci_unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
}

struct srp_gc mfii_dev_handles_gc =
    SRP_GC_INITIALIZER(mfii_dev_handles_dtor, NULL);

static inline uint16_t
mfii_dev_handle(struct mfii_softc *sc, uint16_t target)
{
	struct srp_ref sr;
	uint16_t *map, handle;

	map = srp_enter(&sr, &sc->sc_pd->pd_dev_handles);
	handle = map[target];
	srp_leave(&sr);

	return (handle);
}

int
mfii_dev_handles_update(struct mfii_softc *sc)
{
	struct mfii_ld_map *lm;
	uint16_t *dev_handles = NULL;
	struct mfii_ccb *ccb;
	int i;
	int rv = 0;

	lm = malloc(sizeof(*lm), M_TEMP, M_WAITOK|M_ZERO);
	ccb = scsi_io_get(&sc->sc_iopool, 0);

	rv = mfii_mgmt(sc, ccb, MR_DCMD_LD_MAP_GET_INFO, NULL,
	    lm, sizeof(*lm), SCSI_DATA_IN|SCSI_NOSLEEP);

	scsi_io_put(&sc->sc_iopool, ccb);
	if (rv != 0) {
		rv = EIO;
		goto free_lm;
	}

	dev_handles = mallocarray(MFI_MAX_PD, sizeof(*dev_handles),
	    M_DEVBUF, M_WAITOK);

	for (i = 0; i < MFI_MAX_PD; i++)
		dev_handles[i] = lm->mlm_dev_handle[i].mdh_cur_handle;

	/* commit the updated info */
	sc->sc_pd->pd_timeout = lm->mlm_pd_timeout;
	srp_update_locked(&mfii_dev_handles_gc,
	    &sc->sc_pd->pd_dev_handles, dev_handles);

free_lm:
	free(lm, M_TEMP, sizeof(*lm));

	return (rv);
}

void
mfii_dev_handles_dtor(void *null, void *v)
{
	uint16_t *dev_handles = v;

	free(dev_handles, M_DEVBUF, sizeof(*dev_handles) * MFI_MAX_PD);
}

int
mfii_syspd(struct mfii_softc *sc)
{
	struct scsibus_attach_args saa;
	struct scsi_link *link;

	sc->sc_pd = malloc(sizeof(*sc->sc_pd), M_DEVBUF, M_WAITOK|M_ZERO);
	if (sc->sc_pd == NULL)
		return (1);

	srp_init(&sc->sc_pd->pd_dev_handles);
	if (mfii_dev_handles_update(sc) != 0)
		goto free_pdsc;

	link = &sc->sc_pd->pd_link;
	link->adapter = &mfii_pd_switch;
	link->adapter_softc = sc;
	link->adapter_buswidth = MFI_MAX_PD;
	link->adapter_target = -1;
	link->openings = sc->sc_max_cmds - 1;
	link->pool = &sc->sc_iopool;

	memset(&saa, 0, sizeof(saa));
	saa.saa_sc_link = link;

	sc->sc_pd->pd_scsibus = (struct scsibus_softc *)
	    config_found(&sc->sc_dev, &saa, scsiprint);

	return (0);

free_pdsc:
	free(sc->sc_pd, M_DEVBUF, sizeof(*sc->sc_pd));
	return (1);
}

int
mfii_detach(struct device *self, int flags)
{
	struct mfii_softc *sc = (struct mfii_softc *)self;

	if (sc->sc_ih == NULL)
		return (0);

	mfii_aen_unregister(sc);
	pci_intr_disestablish(sc->sc_pc, sc->sc_ih);
	mfii_dmamem_free(sc, sc->sc_sgl);
	mfii_dmamem_free(sc, sc->sc_mfi);
	mfii_dmamem_free(sc, sc->sc_requests);
	mfii_dmamem_free(sc, sc->sc_reply_postq);
	mfii_dmamem_free(sc, sc->sc_sense);
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);

	return (0);
}

u_int32_t
mfii_read(struct mfii_softc *sc, bus_size_t r)
{
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, r));
}

void
mfii_write(struct mfii_softc *sc, bus_size_t r, u_int32_t v)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

struct mfii_dmamem *
mfii_dmamem_alloc(struct mfii_softc *sc, size_t size)
{
	struct mfii_dmamem *m;
	int nsegs;

	m = malloc(sizeof(*m), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (m == NULL)
		return (NULL);

	m->mdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &m->mdm_map) != 0)
		goto mdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &m->mdm_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &m->mdm_seg, nsegs, size, &m->mdm_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, m->mdm_map, m->mdm_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	return (m);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, m->mdm_kva, m->mdm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &m->mdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, m->mdm_map);
mdmfree:
	free(m, M_DEVBUF, sizeof *m);

	return (NULL);
}

void
mfii_dmamem_free(struct mfii_softc *sc, struct mfii_dmamem *m)
{
	bus_dmamap_unload(sc->sc_dmat, m->mdm_map);
	bus_dmamem_unmap(sc->sc_dmat, m->mdm_kva, m->mdm_size);
	bus_dmamem_free(sc->sc_dmat, &m->mdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, m->mdm_map);
	free(m, M_DEVBUF, sizeof *m);
}

void
mfii_dcmd_start(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);
	struct mfii_sge *sge = (struct mfii_sge *)(ctx + 1);

	io->function = MFII_FUNCTION_PASSTHRU_IO;
	io->sgl_offset0 = (uint32_t *)sge - (uint32_t *)io;
	io->chain_offset = io->sgl_offset0 / 4;

	htolem64(&sge->sg_addr, ccb->ccb_sense_dva);
	htolem32(&sge->sg_len, sizeof(*ccb->ccb_sense));
	sge->sg_flags = MFII_SGE_CHAIN_ELEMENT | MFII_SGE_ADDR_IOCPLBNTA;

	ccb->ccb_req.flags = MFII_REQ_TYPE_SCSI;
	ccb->ccb_req.smid = letoh16(ccb->ccb_smid);

	mfii_start(sc, ccb);
}

int
mfii_aen_register(struct mfii_softc *sc)
{
	struct mfi_evt_log_info mel;
	struct mfii_ccb *ccb;
	struct mfii_dmamem *mdm;
	int rv;

	ccb = scsi_io_get(&sc->sc_iopool, 0);
	if (ccb == NULL) {
		printf("%s: unable to allocate ccb for aen\n", DEVNAME(sc));
		return (ENOMEM);
	}

	memset(&mel, 0, sizeof(mel));

	rv = mfii_mgmt(sc, ccb, MR_DCMD_CTRL_EVENT_GET_INFO, NULL,
	    &mel, sizeof(mel), SCSI_DATA_IN|SCSI_NOSLEEP);
	if (rv != 0) {
		scsi_io_put(&sc->sc_iopool, ccb);
		printf("%s: unable to get event info\n", DEVNAME(sc));
		return (EIO);
	}

	mdm = mfii_dmamem_alloc(sc, sizeof(struct mfi_evt_detail));
	if (mdm == NULL) {
		scsi_io_put(&sc->sc_iopool, ccb);
		printf("%s: unable to allocate event data\n", DEVNAME(sc));
		return (ENOMEM);
	}

	/* replay all the events from boot */
	mfii_aen_start(sc, ccb, mdm, lemtoh32(&mel.mel_boot_seq_num));

	return (0);
}

void
mfii_aen_start(struct mfii_softc *sc, struct mfii_ccb *ccb,
    struct mfii_dmamem *mdm, uint32_t seq)
{
	struct mfi_dcmd_frame *dcmd = mfii_dcmd_frame(ccb);
	struct mfi_frame_header *hdr = &dcmd->mdf_header;
	union mfi_sgl *sgl = &dcmd->mdf_sgl;
	union mfi_evt_class_locale mec;

	mfii_scrub_ccb(ccb);
	mfii_dcmd_scrub(ccb);
	memset(MFII_DMA_KVA(mdm), 0, MFII_DMA_LEN(mdm));

	ccb->ccb_cookie = mdm;
	ccb->ccb_done = mfii_aen_done;
	sc->sc_aen_ccb = ccb;

	mec.mec_members.class = MFI_EVT_CLASS_DEBUG;
	mec.mec_members.reserved = 0;
	mec.mec_members.locale = htole16(MFI_EVT_LOCALE_ALL);

	hdr->mfh_cmd = MFI_CMD_DCMD;
	hdr->mfh_sg_count = 1;
	hdr->mfh_flags = htole16(MFI_FRAME_DIR_READ | MFI_FRAME_SGL64);
	htolem32(&hdr->mfh_data_len, MFII_DMA_LEN(mdm));
	dcmd->mdf_opcode = htole32(MR_DCMD_CTRL_EVENT_WAIT);
	htolem32(&dcmd->mdf_mbox.w[0], seq);
	htolem32(&dcmd->mdf_mbox.w[1], mec.mec_word);
	htolem64(&sgl->sg64[0].addr, MFII_DMA_DVA(mdm));
	htolem32(&sgl->sg64[0].len, MFII_DMA_LEN(mdm));

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(mdm),
	    0, MFII_DMA_LEN(mdm), BUS_DMASYNC_PREREAD);

	mfii_dcmd_sync(sc, ccb, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	mfii_dcmd_start(sc, ccb);
}

void
mfii_aen_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	KASSERT(sc->sc_aen_ccb == ccb);

	/* defer to a thread with KERNEL_LOCK so we can run autoconf */
	task_add(systq, &sc->sc_aen_task);
}

void
mfii_aen(void *arg)
{
	struct mfii_softc *sc = arg;
	struct mfii_ccb *ccb = sc->sc_aen_ccb;
	struct mfii_dmamem *mdm = ccb->ccb_cookie;
	const struct mfi_evt_detail *med = MFII_DMA_KVA(mdm);

	mfii_dcmd_sync(sc, ccb,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(mdm),
	    0, MFII_DMA_LEN(mdm), BUS_DMASYNC_POSTREAD);

#if 0
	printf("%s: %u %08x %02x %s\n", DEVNAME(sc),
	    lemtoh32(&med->med_seq_num), lemtoh32(&med->med_code),
	    med->med_arg_type, med->med_description);
#endif

	switch (lemtoh32(&med->med_code)) {
	case MFI_EVT_PD_INSERTED_EXT:
		if (med->med_arg_type != MFI_EVT_ARGS_PD_ADDRESS)
			break;
		
		mfii_aen_pd_insert(sc, &med->args.pd_address);
		break;
 	case MFI_EVT_PD_REMOVED_EXT:
		if (med->med_arg_type != MFI_EVT_ARGS_PD_ADDRESS)
			break;
		
		mfii_aen_pd_remove(sc, &med->args.pd_address);
		break;

	case MFI_EVT_PD_STATE_CHANGE:
		if (med->med_arg_type != MFI_EVT_ARGS_PD_STATE)
			break;

		mfii_aen_pd_state_change(sc, &med->args.pd_state);
		break;

	default:
		break;
	}

	mfii_aen_start(sc, ccb, mdm, lemtoh32(&med->med_seq_num) + 1);
}

void
mfii_aen_pd_insert(struct mfii_softc *sc,
    const struct mfi_evtarg_pd_address *pd)
{
#if 0
	printf("%s: pd inserted ext\n", DEVNAME(sc));
	printf("%s:  device_id %04x encl_id: %04x type %x\n", DEVNAME(sc),
	    lemtoh16(&pd->device_id), lemtoh16(&pd->encl_id),
	    pd->scsi_dev_type);
	printf("%s:  connected %02x addrs %016llx %016llx\n", DEVNAME(sc),
	    pd->connected.port_bitmap, lemtoh64(&pd->sas_addr[0]),
	    lemtoh64(&pd->sas_addr[1]));
#endif

	if (mfii_dev_handles_update(sc) != 0) /* refresh map */
		return;

	scsi_probe_target(sc->sc_pd->pd_scsibus, lemtoh16(&pd->device_id));
}

void
mfii_aen_pd_remove(struct mfii_softc *sc,
    const struct mfi_evtarg_pd_address *pd)
{
#if 0
	printf("%s: pd removed ext\n", DEVNAME(sc));
	printf("%s:  device_id %04x encl_id: %04x type %u\n", DEVNAME(sc),
	    lemtoh16(&pd->device_id), lemtoh16(&pd->encl_id),
	    pd->scsi_dev_type);
	printf("%s:  connected %02x addrs %016llx %016llx\n", DEVNAME(sc),
	    pd->connected.port_bitmap, lemtoh64(&pd->sas_addr[0]),
	    lemtoh64(&pd->sas_addr[1]));
#endif
	uint16_t target = lemtoh16(&pd->device_id);

	scsi_activate(sc->sc_pd->pd_scsibus, target, -1, DVACT_DEACTIVATE);

	/* the firmware will abort outstanding commands for us */

	scsi_detach_target(sc->sc_pd->pd_scsibus, target, DETACH_FORCE);
}

void
mfii_aen_pd_state_change(struct mfii_softc *sc,
    const struct mfi_evtarg_pd_state *state)
{
	uint16_t target = lemtoh16(&state->pd.mep_device_id);

	if (state->prev_state == htole32(MFI_PD_SYSTEM) &&
	    state->new_state != htole32(MFI_PD_SYSTEM)) {
		/* it's been pulled or configured for raid */

		scsi_activate(sc->sc_pd->pd_scsibus, target, -1,
		    DVACT_DEACTIVATE);
		/* outstanding commands will simply complete or get aborted */
		scsi_detach_target(sc->sc_pd->pd_scsibus, target,
		    DETACH_FORCE);

	} else if (state->prev_state == htole32(MFI_PD_UNCONFIG_GOOD) &&
	    state->new_state == htole32(MFI_PD_SYSTEM)) {
		/* the firmware is handing the disk over */

		scsi_probe_target(sc->sc_pd->pd_scsibus, target);
	}
}

void
mfii_aen_unregister(struct mfii_softc *sc)
{
	/* XXX */
}

int
mfii_transition_firmware(struct mfii_softc *sc)
{
	int32_t			fw_state, cur_state;
	int			max_wait, i;

	fw_state = mfii_fw_state(sc) & MFI_STATE_MASK;

	while (fw_state != MFI_STATE_READY) {
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_STATE_FAULT:
			printf("%s: firmware fault\n", DEVNAME(sc));
			return (1);
		case MFI_STATE_WAIT_HANDSHAKE:
			mfii_write(sc, MFI_SKINNY_IDB,
			    MFI_INIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_STATE_OPERATIONAL:
			mfii_write(sc, MFI_SKINNY_IDB, MFI_INIT_READY);
			max_wait = 10;
			break;
		case MFI_STATE_UNDEFINED:
		case MFI_STATE_BB_INIT:
			max_wait = 2;
			break;
		case MFI_STATE_FW_INIT:
		case MFI_STATE_DEVICE_SCAN:
		case MFI_STATE_FLUSH_CACHE:
			max_wait = 20;
			break;
		default:
			printf("%s: unknown firmware state %d\n",
			    DEVNAME(sc), fw_state);
			return (1);
		}
		for (i = 0; i < (max_wait * 10); i++) {
			fw_state = mfii_fw_state(sc) & MFI_STATE_MASK;
			if (fw_state == cur_state)
				DELAY(100000);
			else
				break;
		}
		if (fw_state == cur_state) {
			printf("%s: firmware stuck in state %#x\n",
			    DEVNAME(sc), fw_state);
			return (1);
		}
	}

	return (0);
}

int
mfii_get_info(struct mfii_softc *sc)
{
	struct mfii_ccb *ccb;
	int rv;

	ccb = scsi_io_get(&sc->sc_iopool, 0);
	rv = mfii_mgmt(sc, ccb, MR_DCMD_CTRL_GET_INFO, NULL,
	    &sc->sc_info, sizeof(sc->sc_info), SCSI_DATA_IN|SCSI_NOSLEEP);
	scsi_io_put(&sc->sc_iopool, ccb);

	if (rv != 0)
		return (rv);

#ifdef MFI_DEBUG
	for (i = 0; i < sc->sc_info.mci_image_component_count; i++) {
		printf("%s: active FW %s Version %s date %s time %s\n",
		    DEVNAME(sc),
		    sc->sc_info.mci_image_component[i].mic_name,
		    sc->sc_info.mci_image_component[i].mic_version,
		    sc->sc_info.mci_image_component[i].mic_build_date,
		    sc->sc_info.mci_image_component[i].mic_build_time);
	}

	for (i = 0; i < sc->sc_info.mci_pending_image_component_count; i++) {
		printf("%s: pending FW %s Version %s date %s time %s\n",
		    DEVNAME(sc),
		    sc->sc_info.mci_pending_image_component[i].mic_name,
		    sc->sc_info.mci_pending_image_component[i].mic_version,
		    sc->sc_info.mci_pending_image_component[i].mic_build_date,
		    sc->sc_info.mci_pending_image_component[i].mic_build_time);
	}

	printf("%s: max_arms %d max_spans %d max_arrs %d max_lds %d name %s\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_arms,
	    sc->sc_info.mci_max_spans,
	    sc->sc_info.mci_max_arrays,
	    sc->sc_info.mci_max_lds,
	    sc->sc_info.mci_product_name);

	printf("%s: serial %s present %#x fw time %d max_cmds %d max_sg %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_serial_number,
	    sc->sc_info.mci_hw_present,
	    sc->sc_info.mci_current_fw_time,
	    sc->sc_info.mci_max_cmds,
	    sc->sc_info.mci_max_sg_elements);

	printf("%s: max_rq %d lds_pres %d lds_deg %d lds_off %d pd_pres %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_request_size,
	    sc->sc_info.mci_lds_present,
	    sc->sc_info.mci_lds_degraded,
	    sc->sc_info.mci_lds_offline,
	    sc->sc_info.mci_pd_present);

	printf("%s: pd_dsk_prs %d pd_dsk_pred_fail %d pd_dsk_fail %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_pd_disks_present,
	    sc->sc_info.mci_pd_disks_pred_failure,
	    sc->sc_info.mci_pd_disks_failed);

	printf("%s: nvram %d mem %d flash %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_nvram_size,
	    sc->sc_info.mci_memory_size,
	    sc->sc_info.mci_flash_size);

	printf("%s: ram_cor %d ram_uncor %d clus_all %d clus_act %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_ram_correctable_errors,
	    sc->sc_info.mci_ram_uncorrectable_errors,
	    sc->sc_info.mci_cluster_allowed,
	    sc->sc_info.mci_cluster_active);

	printf("%s: max_strps_io %d raid_lvl %#x adapt_ops %#x ld_ops %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_max_strips_per_io,
	    sc->sc_info.mci_raid_levels,
	    sc->sc_info.mci_adapter_ops,
	    sc->sc_info.mci_ld_ops);

	printf("%s: strp_sz_min %d strp_sz_max %d pd_ops %#x pd_mix %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_stripe_sz_ops.min,
	    sc->sc_info.mci_stripe_sz_ops.max,
	    sc->sc_info.mci_pd_ops,
	    sc->sc_info.mci_pd_mix_support);

	printf("%s: ecc_bucket %d pckg_prop %s\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_ecc_bucket_count,
	    sc->sc_info.mci_package_version);

	printf("%s: sq_nm %d prd_fail_poll %d intr_thrtl %d intr_thrtl_to %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_seq_num,
	    sc->sc_info.mci_properties.mcp_pred_fail_poll_interval,
	    sc->sc_info.mci_properties.mcp_intr_throttle_cnt,
	    sc->sc_info.mci_properties.mcp_intr_throttle_timeout);

	printf("%s: rbld_rate %d patr_rd_rate %d bgi_rate %d cc_rate %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_rebuild_rate,
	    sc->sc_info.mci_properties.mcp_patrol_read_rate,
	    sc->sc_info.mci_properties.mcp_bgi_rate,
	    sc->sc_info.mci_properties.mcp_cc_rate);

	printf("%s: rc_rate %d ch_flsh %d spin_cnt %d spin_dly %d clus_en %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_recon_rate,
	    sc->sc_info.mci_properties.mcp_cache_flush_interval,
	    sc->sc_info.mci_properties.mcp_spinup_drv_cnt,
	    sc->sc_info.mci_properties.mcp_spinup_delay,
	    sc->sc_info.mci_properties.mcp_cluster_enable);

	printf("%s: coerc %d alarm %d dis_auto_rbld %d dis_bat_wrn %d ecc %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_coercion_mode,
	    sc->sc_info.mci_properties.mcp_alarm_enable,
	    sc->sc_info.mci_properties.mcp_disable_auto_rebuild,
	    sc->sc_info.mci_properties.mcp_disable_battery_warn,
	    sc->sc_info.mci_properties.mcp_ecc_bucket_size);

	printf("%s: ecc_leak %d rest_hs %d exp_encl_dev %d\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_properties.mcp_ecc_bucket_leak_rate,
	    sc->sc_info.mci_properties.mcp_restore_hotspare_on_insertion,
	    sc->sc_info.mci_properties.mcp_expose_encl_devices);

	printf("%s: vendor %#x device %#x subvendor %#x subdevice %#x\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_pci.mip_vendor,
	    sc->sc_info.mci_pci.mip_device,
	    sc->sc_info.mci_pci.mip_subvendor,
	    sc->sc_info.mci_pci.mip_subdevice);

	printf("%s: type %#x port_count %d port_addr ",
	    DEVNAME(sc),
	    sc->sc_info.mci_host.mih_type,
	    sc->sc_info.mci_host.mih_port_count);

	for (i = 0; i < 8; i++)
		printf("%.0llx ", sc->sc_info.mci_host.mih_port_addr[i]);
	printf("\n");

	printf("%s: type %.x port_count %d port_addr ",
	    DEVNAME(sc),
	    sc->sc_info.mci_device.mid_type,
	    sc->sc_info.mci_device.mid_port_count);

	for (i = 0; i < 8; i++)
		printf("%.0llx ", sc->sc_info.mci_device.mid_port_addr[i]);
	printf("\n");
#endif /* MFI_DEBUG */

	return (0);
}

int
mfii_mfa_poll(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	struct mfi_frame_header	*hdr = ccb->ccb_request;
	u_int64_t r;
	int to = 0, rv = 0;

#ifdef DIAGNOSTIC
	if (ccb->ccb_cookie != NULL || ccb->ccb_done != NULL)
		panic("mfii_mfa_poll called with cookie or done set");
#endif

	hdr->mfh_context = ccb->ccb_smid;
	hdr->mfh_cmd_status = MFI_STAT_INVALID_STATUS;
	hdr->mfh_flags |= htole16(MFI_FRAME_DONT_POST_IN_REPLY_QUEUE);

	r = MFII_REQ_MFA(ccb->ccb_request_dva);
	memcpy(&ccb->ccb_req, &r, sizeof(ccb->ccb_req));

	mfii_start(sc, ccb);

	for (;;) {
		bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_requests),
		    ccb->ccb_request_offset, MFII_REQUEST_SIZE,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (hdr->mfh_cmd_status != MFI_STAT_INVALID_STATUS)
			break;

		if (to++ > 5000) { /* XXX 5 seconds busywait sucks */
			printf("%s: timeout on ccb %d\n", DEVNAME(sc),
			    ccb->ccb_smid);
			ccb->ccb_flags |= MFI_CCB_F_ERR;
			rv = 1;
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_requests),
		    ccb->ccb_request_offset, MFII_REQUEST_SIZE,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		delay(1000);
	}

	if (ccb->ccb_len > 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap,
		    0, ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction == MFII_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	return (rv);
}

int
mfii_poll(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	void (*done)(struct mfii_softc *, struct mfii_ccb *);
	void *cookie;
	int rv = 1;

	done = ccb->ccb_done;
	cookie = ccb->ccb_cookie;

	ccb->ccb_done = mfii_poll_done;
	ccb->ccb_cookie = &rv;

	mfii_start(sc, ccb);

	do {
		delay(10);
		mfii_postq(sc);
	} while (rv == 1);

	ccb->ccb_cookie = cookie;
	done(sc, ccb);

	return (0);
}

void
mfii_poll_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	int *rv = ccb->ccb_cookie;

	*rv = 0;
}

int
mfii_exec(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	struct mutex m = MUTEX_INITIALIZER(IPL_BIO);

#ifdef DIAGNOSTIC
	if (ccb->ccb_cookie != NULL || ccb->ccb_done != NULL)
		panic("mfii_exec called with cookie or done set");
#endif

	ccb->ccb_cookie = &m;
	ccb->ccb_done = mfii_exec_done;

	mfii_start(sc, ccb);

	mtx_enter(&m);
	while (ccb->ccb_cookie != NULL)
		msleep(ccb, &m, PRIBIO, "mfiiexec", 0);
	mtx_leave(&m);

	return (0);
}

void
mfii_exec_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	struct mutex *m = ccb->ccb_cookie;

	mtx_enter(m);
	ccb->ccb_cookie = NULL;
	wakeup_one(ccb);
	mtx_leave(m);
}

int
mfii_mgmt(struct mfii_softc *sc, struct mfii_ccb *ccb,
    u_int32_t opc, const union mfi_mbox *mbox, void *buf, size_t len,
    int flags)
{
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);
	struct mfii_sge *sge = (struct mfii_sge *)(ctx + 1);
	struct mfi_dcmd_frame *dcmd = ccb->ccb_mfi;
	struct mfi_frame_header *hdr = &dcmd->mdf_header;
	u_int8_t *dma_buf;
	int rv = EIO;

	dma_buf = dma_alloc(len, PR_WAITOK);
	if (dma_buf == NULL)
		return (ENOMEM);

	mfii_scrub_ccb(ccb);
	ccb->ccb_data = dma_buf;
	ccb->ccb_len = len;
	switch (flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		ccb->ccb_direction = MFII_DATA_IN;
		hdr->mfh_flags = htole16(MFI_FRAME_DIR_READ);
		break;
	case SCSI_DATA_OUT:
		ccb->ccb_direction = MFII_DATA_OUT;
		hdr->mfh_flags = htole16(MFI_FRAME_DIR_WRITE);
		memcpy(dma_buf, buf, len);
		break;
	}

	if (mfii_load_mfa(sc, ccb, &dcmd->mdf_sgl,
	    ISSET(flags, SCSI_NOSLEEP)) != 0) {
		rv = ENOMEM;
		goto done;
	}

	hdr->mfh_cmd = MFI_CMD_DCMD;
	hdr->mfh_context = ccb->ccb_smid;
	hdr->mfh_data_len = htole32(len);
	hdr->mfh_sg_count = ccb->ccb_dmamap->dm_nsegs;

	dcmd->mdf_opcode = opc;
	/* handle special opcodes */
	if (mbox != NULL)
		memcpy(&dcmd->mdf_mbox, mbox, sizeof(dcmd->mdf_mbox));

	io->function = MFII_FUNCTION_PASSTHRU_IO;
	io->sgl_offset0 = ((u_int8_t *)sge - (u_int8_t *)io) / 4;
	io->chain_offset = ((u_int8_t *)sge - (u_int8_t *)io) / 16;

	htolem64(&sge->sg_addr, ccb->ccb_mfi_dva);
	htolem32(&sge->sg_len, MFI_FRAME_SIZE);
	sge->sg_flags = MFII_SGE_CHAIN_ELEMENT | MFII_SGE_ADDR_IOCPLBNTA;

	ccb->ccb_req.flags = MFII_REQ_TYPE_SCSI;
	ccb->ccb_req.smid = letoh16(ccb->ccb_smid);

	if (ISSET(flags, SCSI_NOSLEEP)) {
		ccb->ccb_done = mfii_empty_done;
		mfii_poll(sc, ccb);
	} else
		mfii_exec(sc, ccb);

	if (hdr->mfh_cmd_status == MFI_STAT_OK) {
		rv = 0;

		if (ccb->ccb_direction == MFII_DATA_IN)
			memcpy(buf, dma_buf, len);
	}

done:
	dma_free(dma_buf, len);

	return (rv);
}

void
mfii_empty_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	return;
}

int
mfii_load_mfa(struct mfii_softc *sc, struct mfii_ccb *ccb,
    void *sglp, int nosleep)
{
	union mfi_sgl *sgl = sglp;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	int error;
	int i;

	if (ccb->ccb_len == 0)
		return (0);

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    ccb->ccb_data, ccb->ccb_len, NULL,
	    nosleep ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: error %d loading dmamap\n", DEVNAME(sc), error);
		return (1);
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		sgl->sg32[i].addr = htole32(dmap->dm_segs[i].ds_addr);
		sgl->sg32[i].len = htole32(dmap->dm_segs[i].ds_len);
	}

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    ccb->ccb_direction == MFII_DATA_OUT ?
	    BUS_DMASYNC_PREWRITE : BUS_DMASYNC_PREREAD);

	return (0);
}

void
mfii_start(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	u_long *r = (u_long *)&ccb->ccb_req;

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_requests),
	    ccb->ccb_request_offset, MFII_REQUEST_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

#if defined(__LP64__)
	bus_space_write_raw_8(sc->sc_iot, sc->sc_ioh, MFI_IQPL, *r);
#else
	mtx_enter(&sc->sc_post_mtx);
	bus_space_write_raw_4(sc->sc_iot, sc->sc_ioh, MFI_IQPL, r[0]);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh,
	    MFI_IQPL, 8, BUS_SPACE_BARRIER_WRITE);

	bus_space_write_raw_4(sc->sc_iot, sc->sc_ioh, MFI_IQPH, r[1]);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh,
	    MFI_IQPH, 8, BUS_SPACE_BARRIER_WRITE);
	mtx_leave(&sc->sc_post_mtx);
#endif
}

void
mfii_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_requests),
	    ccb->ccb_request_offset, MFII_REQUEST_SIZE,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	if (ccb->ccb_sgl_len > 0) {
		bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_sgl),
		    ccb->ccb_sgl_offset, ccb->ccb_sgl_len,
		    BUS_DMASYNC_POSTWRITE);
	}

	if (ccb->ccb_len > 0) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap,
		    0, ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction == MFII_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	ccb->ccb_done(sc, ccb);
}

int
mfii_initialise_firmware(struct mfii_softc *sc)
{
	struct mpii_msg_iocinit_request *iiq;
	struct mfii_dmamem *m;
	struct mfii_ccb *ccb;
	struct mfi_init_frame *init;
	int rv;

	m = mfii_dmamem_alloc(sc, sizeof(*iiq));
	if (m == NULL)
		return (1);

	iiq = MFII_DMA_KVA(m);
	memset(iiq, 0, sizeof(*iiq));

	iiq->function = MPII_FUNCTION_IOC_INIT;
	iiq->whoinit = MPII_WHOINIT_HOST_DRIVER;

	iiq->msg_version_maj = 0x02;
	iiq->msg_version_min = 0x00;
	iiq->hdr_version_unit = 0x10;
	iiq->hdr_version_dev = 0x0;

	iiq->system_request_frame_size = htole16(MFII_REQUEST_SIZE / 4);

	iiq->reply_descriptor_post_queue_depth =
	    htole16(sc->sc_reply_postq_depth);
	iiq->reply_free_queue_depth = htole16(0);

	htolem32(&iiq->sense_buffer_address_high,
	    MFII_DMA_DVA(sc->sc_sense) >> 32);

	htolem32(&iiq->reply_descriptor_post_queue_address_lo,
	    MFII_DMA_DVA(sc->sc_reply_postq));
	htolem32(&iiq->reply_descriptor_post_queue_address_hi,
	    MFII_DMA_DVA(sc->sc_reply_postq) >> 32);

	htolem32(&iiq->system_request_frame_base_address_lo,
	    MFII_DMA_DVA(sc->sc_requests));
	htolem32(&iiq->system_request_frame_base_address_hi,
	    MFII_DMA_DVA(sc->sc_requests) >> 32);

	iiq->timestamp = htole64(time_uptime);

	ccb = scsi_io_get(&sc->sc_iopool, 0);
	mfii_scrub_ccb(ccb);
	init = ccb->ccb_request;

	init->mif_header.mfh_cmd = MFI_CMD_INIT;
	init->mif_header.mfh_data_len = htole32(sizeof(*iiq));
	init->mif_qinfo_new_addr = htole64(MFII_DMA_DVA(m));

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_reply_postq),
	    0, MFII_DMA_LEN(sc->sc_reply_postq),
	    BUS_DMASYNC_PREREAD);

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(m),
	    0, sizeof(*iiq), BUS_DMASYNC_PREREAD);

	rv = mfii_mfa_poll(sc, ccb);

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(m),
	    0, sizeof(*iiq), BUS_DMASYNC_POSTREAD);

	scsi_io_put(&sc->sc_iopool, ccb);
	mfii_dmamem_free(sc, m);

	return (rv);
}

int
mfii_my_intr(struct mfii_softc *sc)
{
	u_int32_t status;

	status = mfii_read(sc, MFI_OSTS);
	if (ISSET(status, 0x1)) {
		mfii_write(sc, MFI_OSTS, status);
		return (1);
	}

	return (ISSET(status, MFII_OSTS_INTR_VALID) ? 1 : 0);
}

int
mfii_intr(void *arg)
{
	struct mfii_softc *sc = arg;

	if (!mfii_my_intr(sc))
		return (0);

	mfii_postq(sc);

	return (1);
}

void
mfii_postq(struct mfii_softc *sc)
{
	struct mfii_ccb_list ccbs = SIMPLEQ_HEAD_INITIALIZER(ccbs);
	struct mpii_reply_descr *postq = MFII_DMA_KVA(sc->sc_reply_postq);
	struct mpii_reply_descr *rdp;
	struct mfii_ccb *ccb;
	int rpi = 0;

	mtx_enter(&sc->sc_reply_postq_mtx);

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_reply_postq),
	    0, MFII_DMA_LEN(sc->sc_reply_postq),
	    BUS_DMASYNC_POSTREAD);

	for (;;) {
		rdp = &postq[sc->sc_reply_postq_index];
		if ((rdp->reply_flags & MPII_REPLY_DESCR_TYPE_MASK) ==
		    MPII_REPLY_DESCR_UNUSED)
			break;
		if (rdp->data == 0xffffffff) {
			/*
			 * ioc is still writing to the reply post queue
			 * race condition - bail!
			 */
			break;
		}

		ccb = &sc->sc_ccb[letoh16(rdp->smid) - 1];
		SIMPLEQ_INSERT_TAIL(&ccbs, ccb, ccb_link);
		memset(rdp, 0xff, sizeof(*rdp));

		sc->sc_reply_postq_index++;
		sc->sc_reply_postq_index %= sc->sc_reply_postq_depth;
		rpi = 1;
	}

	bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_reply_postq),
	    0, MFII_DMA_LEN(sc->sc_reply_postq),
	    BUS_DMASYNC_PREREAD);

	if (rpi)
		mfii_write(sc, MFII_RPI, sc->sc_reply_postq_index);

	mtx_leave(&sc->sc_reply_postq_mtx);

	while ((ccb = SIMPLEQ_FIRST(&ccbs)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&ccbs, ccb_link);
		mfii_done(sc, ccb);
	}
}

void
mfii_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mfii_softc *sc = link->adapter_softc;
	struct mfii_ccb *ccb = xs->io;

	mfii_scrub_ccb(ccb);
	ccb->ccb_cookie = xs;
	ccb->ccb_done = mfii_scsi_cmd_done;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;

	timeout_set(&xs->stimeout, mfii_scsi_cmd_tmo, xs);

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case READ_12:
	case READ_16:
	case WRITE_COMMAND:
	case WRITE_BIG:
	case WRITE_12:
	case WRITE_16:
		if (mfii_scsi_cmd_io(sc, xs) != 0)
			goto stuffup;

		break;

	default:
		if (mfii_scsi_cmd_cdb(sc, xs) != 0)
			goto stuffup;
		break;
	}

	xs->error = XS_NOERROR;
	xs->resid = 0;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (mfii_poll(sc, ccb) != 0)
			goto stuffup;
		return;
	}

	ccb->ccb_refcnt = 2; /* one for the chip, one for the timeout */
	timeout_add_msec(&xs->stimeout, xs->timeout);
	mfii_start(sc, ccb);

	return;

stuffup:
	xs->error = XS_DRIVER_STUFFUP;
	scsi_done(xs);
}

void
mfii_scsi_cmd_done(struct mfii_softc *sc, struct mfii_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ccb_cookie;
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);
	u_int refs = 1;

	if (timeout_del(&xs->stimeout))
		refs = 2;

	switch (ctx->status) {
	case MFI_STAT_OK:
		break;

	case MFI_STAT_SCSI_DONE_WITH_ERROR:
		xs->error = XS_SENSE;
		memset(&xs->sense, 0, sizeof(xs->sense));
		memcpy(&xs->sense, ccb->ccb_sense, sizeof(xs->sense));
		break;

	case MFI_STAT_LD_OFFLINE:
	case MFI_STAT_DEVICE_NOT_FOUND:
		xs->error = XS_SELTIMEOUT;
		break;

	default:
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (atomic_sub_int_nv(&ccb->ccb_refcnt, refs) == 0)
		scsi_done(xs);
}

int
mfii_scsi_cmd_io(struct mfii_softc *sc, struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mfii_ccb *ccb = xs->io;
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);
	int segs;

	io->dev_handle = htole16(link->target);
	io->function = MFII_FUNCTION_LDIO_REQUEST;
	io->sense_buffer_low_address = htole32(ccb->ccb_sense_dva);
	io->sgl_flags = htole16(0x02); /* XXX */
	io->sense_buffer_length = sizeof(xs->sense);
	io->sgl_offset0 = (sizeof(*io) + sizeof(*ctx)) / 4;
	io->data_length = htole32(xs->datalen);
	io->io_flags = htole16(xs->cmdlen);
	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		ccb->ccb_direction = MFII_DATA_IN;
		io->direction = MPII_SCSIIO_DIR_READ;
		break;
	case SCSI_DATA_OUT:
		ccb->ccb_direction = MFII_DATA_OUT;
		io->direction = MPII_SCSIIO_DIR_WRITE;
		break;
	default:
		ccb->ccb_direction = MFII_DATA_NONE;
		io->direction = MPII_SCSIIO_DIR_NONE;
		break;
	}
	memcpy(io->cdb, xs->cmd, xs->cmdlen);

	ctx->type_nseg = sc->sc_iop->ldio_ctx_type_nseg;
	ctx->timeout_value = htole16(0x14); /* XXX */
	ctx->reg_lock_flags = htole16(sc->sc_iop->ldio_ctx_reg_lock_flags);
	ctx->virtual_disk_target_id = htole16(link->target);

	if (mfii_load_ccb(sc, ccb, ctx + 1,
	    ISSET(xs->flags, SCSI_NOSLEEP)) != 0)
		return (1);

	segs = (ccb->ccb_len == 0) ? 0 : ccb->ccb_dmamap->dm_nsegs;
	switch (sc->sc_iop->num_sge_loc) {
	case MFII_IOP_NUM_SGE_LOC_ORIG:
		ctx->num_sge = segs;
		break;
	case MFII_IOP_NUM_SGE_LOC_35:
		/* 12 bit field, but we're only using the lower 8 */
		ctx->span_arm = segs;
		break;
	}

	ccb->ccb_req.flags = sc->sc_iop->ldio_req_type;
	ccb->ccb_req.smid = letoh16(ccb->ccb_smid);

	return (0);
}

int
mfii_scsi_cmd_cdb(struct mfii_softc *sc, struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mfii_ccb *ccb = xs->io;
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);

	io->dev_handle = htole16(link->target);
	io->function = MFII_FUNCTION_LDIO_REQUEST;
	io->sense_buffer_low_address = htole32(ccb->ccb_sense_dva);
	io->sgl_flags = htole16(0x02); /* XXX */
	io->sense_buffer_length = sizeof(xs->sense);
	io->sgl_offset0 = (sizeof(*io) + sizeof(*ctx)) / 4;
	io->data_length = htole32(xs->datalen);
	io->io_flags = htole16(xs->cmdlen);
	io->lun[0] = htobe16(link->lun);
	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		ccb->ccb_direction = MFII_DATA_IN;
		io->direction = MPII_SCSIIO_DIR_READ;
		break;
	case SCSI_DATA_OUT:
		ccb->ccb_direction = MFII_DATA_OUT;
		io->direction = MPII_SCSIIO_DIR_WRITE;
		break;
	default:
		ccb->ccb_direction = MFII_DATA_NONE;
		io->direction = MPII_SCSIIO_DIR_NONE;
		break;
	}
	memcpy(io->cdb, xs->cmd, xs->cmdlen);

	ctx->virtual_disk_target_id = htole16(link->target);

	if (mfii_load_ccb(sc, ccb, ctx + 1,
	    ISSET(xs->flags, SCSI_NOSLEEP)) != 0)
		return (1);

	ctx->num_sge = (ccb->ccb_len == 0) ? 0 : ccb->ccb_dmamap->dm_nsegs;

	ccb->ccb_req.flags = MFII_REQ_TYPE_SCSI;
	ccb->ccb_req.smid = letoh16(ccb->ccb_smid);

	return (0);
}

void
mfii_pd_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mfii_softc *sc = link->adapter_softc;
	struct mfii_ccb *ccb = xs->io;

	mfii_scrub_ccb(ccb);
	ccb->ccb_cookie = xs;
	ccb->ccb_done = mfii_scsi_cmd_done;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;

	timeout_set(&xs->stimeout, mfii_scsi_cmd_tmo, xs);

	xs->error = mfii_pd_scsi_cmd_cdb(sc, xs);
	if (xs->error != XS_NOERROR)
		goto done;

	xs->resid = 0;

	if (ISSET(xs->flags, SCSI_POLL)) {
		if (mfii_poll(sc, ccb) != 0)
			goto stuffup;
		return;
	}

	ccb->ccb_refcnt = 2; /* one for the chip, one for the timeout */
	timeout_add_msec(&xs->stimeout, xs->timeout);
	mfii_start(sc, ccb);

	return;

stuffup:
	xs->error = XS_DRIVER_STUFFUP;
done:
	scsi_done(xs);
}

int
mfii_pd_scsi_probe(struct scsi_link *link)
{
	struct mfii_softc *sc = link->adapter_softc;
	struct mfii_ccb *ccb;
	struct mfi_pd_details mpd;
	union mfi_mbox mbox;
	int rv;

	if (link->lun > 0)
		return (0);

	memset(&mbox, 0, sizeof(mbox));
	mbox.s[0] = htole16(link->target);

	ccb = scsi_io_get(&sc->sc_iopool, 0);
	rv = mfii_mgmt(sc, ccb, MR_DCMD_PD_GET_INFO, &mbox, &mpd, sizeof(mpd),
	    SCSI_DATA_IN|SCSI_NOSLEEP);
	scsi_io_put(&sc->sc_iopool, ccb);
	if (rv != 0)
		return (EIO);

	if (mpd.mpd_fw_state != htole16(MFI_PD_SYSTEM))
		return (ENXIO);

	return (0);
}

int
mfii_pd_scsi_cmd_cdb(struct mfii_softc *sc, struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct mfii_ccb *ccb = xs->io;
	struct mpii_msg_scsi_io *io = ccb->ccb_request;
	struct mfii_raid_context *ctx = (struct mfii_raid_context *)(io + 1);
	uint16_t dev_handle;

	dev_handle = mfii_dev_handle(sc, link->target);
	if (dev_handle == htole16(0xffff))
		return (XS_SELTIMEOUT);

	io->dev_handle = dev_handle;
	io->function = 0;
	io->sense_buffer_low_address = htole32(ccb->ccb_sense_dva);
	io->sgl_flags = htole16(0x02); /* XXX */
	io->sense_buffer_length = sizeof(xs->sense);
	io->sgl_offset0 = (sizeof(*io) + sizeof(*ctx)) / 4;
	io->data_length = htole32(xs->datalen);
	io->io_flags = htole16(xs->cmdlen);
	io->lun[0] = htobe16(link->lun);
	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		ccb->ccb_direction = MFII_DATA_IN;
		io->direction = MPII_SCSIIO_DIR_READ;
		break;
	case SCSI_DATA_OUT:
		ccb->ccb_direction = MFII_DATA_OUT;
		io->direction = MPII_SCSIIO_DIR_WRITE;
		break;
	default:
		ccb->ccb_direction = MFII_DATA_NONE;
		io->direction = MPII_SCSIIO_DIR_NONE;
		break;
	}
	memcpy(io->cdb, xs->cmd, xs->cmdlen);

	ctx->virtual_disk_target_id = htole16(link->target);
	ctx->raid_flags = MFII_RAID_CTX_IO_TYPE_SYSPD;
	ctx->timeout_value = sc->sc_pd->pd_timeout;

	if (mfii_load_ccb(sc, ccb, ctx + 1,
	    ISSET(xs->flags, SCSI_NOSLEEP)) != 0)
		return (XS_DRIVER_STUFFUP);

	ctx->num_sge = (ccb->ccb_len == 0) ? 0 : ccb->ccb_dmamap->dm_nsegs;

	ccb->ccb_req.flags = MFII_REQ_TYPE_HI_PRI;
	ccb->ccb_req.smid = letoh16(ccb->ccb_smid);
	ccb->ccb_req.dev_handle = dev_handle;

	return (XS_NOERROR);
}

int
mfii_load_ccb(struct mfii_softc *sc, struct mfii_ccb *ccb, void *sglp,
    int nosleep)
{
	struct mpii_msg_request *req = ccb->ccb_request;
	struct mfii_sge *sge = NULL, *nsge = sglp;
	struct mfii_sge *ce = NULL;
	bus_dmamap_t dmap = ccb->ccb_dmamap;
	u_int space;
	int i;

	int error;

	if (ccb->ccb_len == 0)
		return (0);

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    ccb->ccb_data, ccb->ccb_len, NULL,
	    nosleep ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: error %d loading dmamap\n", DEVNAME(sc), error);
		return (1);
	}

	space = (MFII_REQUEST_SIZE - ((u_int8_t *)nsge - (u_int8_t *)req)) /
	    sizeof(*nsge);
	if (dmap->dm_nsegs > space) {
		space--;

		ccb->ccb_sgl_len = (dmap->dm_nsegs - space) * sizeof(*nsge);
		memset(ccb->ccb_sgl, 0, ccb->ccb_sgl_len);

		ce = nsge + space;
		ce->sg_addr = htole64(ccb->ccb_sgl_dva);
		ce->sg_len = htole32(ccb->ccb_sgl_len);
		ce->sg_flags = sc->sc_iop->sge_flag_chain;

		req->chain_offset = ((u_int8_t *)ce - (u_int8_t *)req) / 16;
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {
		if (nsge == ce)
			nsge = ccb->ccb_sgl;

		sge = nsge;

		sge->sg_addr = htole64(dmap->dm_segs[i].ds_addr);
		sge->sg_len = htole32(dmap->dm_segs[i].ds_len);
		sge->sg_flags = MFII_SGE_ADDR_SYSTEM;

		nsge = sge + 1;
	}
	sge->sg_flags |= sc->sc_iop->sge_flag_eol;

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    ccb->ccb_direction == MFII_DATA_OUT ?
	    BUS_DMASYNC_PREWRITE : BUS_DMASYNC_PREREAD);

	if (ccb->ccb_sgl_len > 0) {
		bus_dmamap_sync(sc->sc_dmat, MFII_DMA_MAP(sc->sc_sgl),
		    ccb->ccb_sgl_offset, ccb->ccb_sgl_len,
		    BUS_DMASYNC_PREWRITE);
	}

	return (0);
}

void
mfii_scsi_cmd_tmo(void *xsp)
{
	struct scsi_xfer *xs = xsp;
	struct scsi_link *link = xs->sc_link;
	struct mfii_softc *sc = link->adapter_softc;
	struct mfii_ccb *ccb = xs->io;

	mtx_enter(&sc->sc_abort_mtx);
	SIMPLEQ_INSERT_TAIL(&sc->sc_abort_list, ccb, ccb_link);
	mtx_leave(&sc->sc_abort_mtx);

	task_add(systqmp, &sc->sc_abort_task);
}

void
mfii_abort_task(void *scp)
{
	struct mfii_softc *sc = scp;
	struct mfii_ccb *list;

	mtx_enter(&sc->sc_abort_mtx);
	list = SIMPLEQ_FIRST(&sc->sc_abort_list);
	SIMPLEQ_INIT(&sc->sc_abort_list);
	mtx_leave(&sc->sc_abort_mtx);

	while (list != NULL) {
		struct mfii_ccb *ccb = list;
		struct scsi_xfer *xs = ccb->ccb_cookie;
		struct scsi_link *link = xs->sc_link;

		uint16_t dev_handle;
		struct mfii_ccb *accb;

		list = SIMPLEQ_NEXT(ccb, ccb_link);

		dev_handle = mfii_dev_handle(sc, link->target);
		if (dev_handle == htole16(0xffff)) {
			/* device is gone */
			if (atomic_dec_int_nv(&ccb->ccb_refcnt) == 0)
				scsi_done(xs);
			continue;
		}

		accb = scsi_io_get(&sc->sc_iopool, 0);
		mfii_scrub_ccb(accb);
		mfii_abort(sc, accb, dev_handle, ccb->ccb_smid,
		    MPII_SCSI_TASK_ABORT_TASK,
		    htole32(MFII_TASK_MGMT_FLAGS_PD));

		accb->ccb_cookie = ccb;
		accb->ccb_done = mfii_scsi_cmd_abort_done;

		mfii_start(sc, accb);
	}
}

void
mfii_abort(struct mfii_softc *sc, struct mfii_ccb *accb, uint16_t dev_handle,
    uint16_t smid, uint8_t type, uint32_t flags)
{
	struct mfii_task_mgmt *msg;
	struct mpii_msg_scsi_task_request *req;

	msg = accb->ccb_request;
	req = &msg->mpii_request;
	req->dev_handle = dev_handle;
	req->function = MPII_FUNCTION_SCSI_TASK_MGMT;
	req->task_type = type;
	htolem16(&req->task_mid, smid);
	msg->flags = flags;

	accb->ccb_req.flags = MFII_REQ_TYPE_HI_PRI;
	accb->ccb_req.smid = letoh16(accb->ccb_smid);
}

void
mfii_scsi_cmd_abort_done(struct mfii_softc *sc, struct mfii_ccb *accb)
{
	struct mfii_ccb *ccb = accb->ccb_cookie;
	struct scsi_xfer *xs = ccb->ccb_cookie;

	/* XXX check accb completion? */

	scsi_io_put(&sc->sc_iopool, accb);

	if (atomic_dec_int_nv(&ccb->ccb_refcnt) == 0)
		scsi_done(xs);
}

void *
mfii_get_ccb(void *cookie)
{
	struct mfii_softc *sc = cookie;
	struct mfii_ccb *ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = SIMPLEQ_FIRST(&sc->sc_ccb_freeq);
	if (ccb != NULL)
		SIMPLEQ_REMOVE_HEAD(&sc->sc_ccb_freeq, ccb_link);
	mtx_leave(&sc->sc_ccb_mtx);

	return (ccb);
}

void
mfii_scrub_ccb(struct mfii_ccb *ccb)
{
	ccb->ccb_cookie = NULL;
	ccb->ccb_done = NULL;
	ccb->ccb_flags = 0;
	ccb->ccb_data = NULL;
	ccb->ccb_direction = 0;
	ccb->ccb_len = 0;
	ccb->ccb_sgl_len = 0;
	ccb->ccb_refcnt = 1;

	memset(&ccb->ccb_req, 0, sizeof(ccb->ccb_req));
	memset(ccb->ccb_request, 0, MFII_REQUEST_SIZE);
	memset(ccb->ccb_mfi, 0, MFI_FRAME_SIZE);
}

void
mfii_put_ccb(void *cookie, void *io)
{
	struct mfii_softc *sc = cookie;
	struct mfii_ccb *ccb = io;

	mtx_enter(&sc->sc_ccb_mtx);
	SIMPLEQ_INSERT_HEAD(&sc->sc_ccb_freeq, ccb, ccb_link);
	mtx_leave(&sc->sc_ccb_mtx);
}

int
mfii_init_ccb(struct mfii_softc *sc)
{
	struct mfii_ccb *ccb;
	u_int8_t *request = MFII_DMA_KVA(sc->sc_requests);
	u_int8_t *mfi = MFII_DMA_KVA(sc->sc_mfi);
	u_int8_t *sense = MFII_DMA_KVA(sc->sc_sense);
	u_int8_t *sgl = MFII_DMA_KVA(sc->sc_sgl);
	u_int i;
	int error;

	sc->sc_ccb = mallocarray(sc->sc_max_cmds, sizeof(struct mfii_ccb),
	    M_DEVBUF, M_WAITOK|M_ZERO);

	for (i = 0; i < sc->sc_max_cmds; i++) {
		ccb = &sc->sc_ccb[i];

		/* create a dma map for transfer */
		error = bus_dmamap_create(sc->sc_dmat,
		    MAXPHYS, sc->sc_max_sgl, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ccb_dmamap);
		if (error) {
			printf("%s: cannot create ccb dmamap (%d)\n",
			    DEVNAME(sc), error);
			goto destroy;
		}

		/* select i + 1'th request. 0 is reserved for events */
		ccb->ccb_smid = i + 1;
		ccb->ccb_request_offset = MFII_REQUEST_SIZE * (i + 1);
		ccb->ccb_request = request + ccb->ccb_request_offset;
		ccb->ccb_request_dva = MFII_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_request_offset;

		/* select i'th MFI command frame */
		ccb->ccb_mfi_offset = MFI_FRAME_SIZE * i;
		ccb->ccb_mfi = mfi + ccb->ccb_mfi_offset;
		ccb->ccb_mfi_dva = MFII_DMA_DVA(sc->sc_mfi) +
		    ccb->ccb_mfi_offset;

		/* select i'th sense */
		ccb->ccb_sense_offset = MFI_SENSE_SIZE * i;
		ccb->ccb_sense = (struct mfi_sense *)(sense +
		    ccb->ccb_sense_offset);
		ccb->ccb_sense_dva = MFII_DMA_DVA(sc->sc_sense) +
		    ccb->ccb_sense_offset;

		/* select i'th sgl */
		ccb->ccb_sgl_offset = sizeof(struct mfii_sge) *
		    sc->sc_max_sgl * i;
		ccb->ccb_sgl = (struct mfii_sge *)(sgl + ccb->ccb_sgl_offset);
		ccb->ccb_sgl_dva = MFII_DMA_DVA(sc->sc_sgl) +
		    ccb->ccb_sgl_offset;

		/* add ccb to queue */
		mfii_put_ccb(sc, ccb);
	}

	return (0);

destroy:
	/* free dma maps and ccb memory */
	while ((ccb = mfii_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	free(sc->sc_ccb, M_DEVBUF, 0);

	return (1);
}

