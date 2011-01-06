/*	$OpenBSD: vdsp.c,v 1.8 2011/01/06 23:26:48 kettenis Exp $	*/
/*
 * Copyright (c) 2009, 2011 Mark Kettenis
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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/workq.h>

#include <machine/autoconf.h>
#include <machine/hypervisor.h>
#include <machine/mdesc.h>

#include <uvm/uvm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/sun/disklabel.h>

#include <sparc64/dev/cbusvar.h>
#include <sparc64/dev/ldcvar.h>
#include <sparc64/dev/viovar.h>

#ifdef VDSP_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#define VDSK_TX_ENTRIES			64
#define VDSK_RX_ENTRIES			64

#define VDSK_MAX_DESCRIPTORS		1024
#define VDSK_MAX_DESCRIPTOR_SIZE	512

struct vd_attr_info {
	struct vio_msg_tag	tag;
	uint8_t			xfer_mode;
	uint8_t			vd_type;
	uint8_t			vd_mtype;
	uint8_t			_reserved1;
	uint32_t		vdisk_block_size;
	uint64_t		operations;
	uint64_t		vdisk_size;
	uint64_t		max_xfer_sz;
	uint64_t		_reserved2[2];
};

#define VD_DISK_TYPE_SLICE	0x01
#define VD_DISK_TYPE_DISK	0x02

#define VD_MEDIA_TYPE_FIXED	0x01
#define VD_MEDIA_TYPE_CD	0x02
#define VD_MEDIA_TYPE_DVD	0x03

/* vDisk version 1.0. */
#define VD_OP_BREAD		0x01
#define VD_OP_BWRITE		0x02
#define VD_OP_FLUSH		0x03
#define VD_OP_GET_WCE		0x04
#define VD_OP_SET_WCE		0x05
#define VD_OP_GET_VTOC		0x06
#define VD_OP_SET_VTOC		0x07
#define VD_OP_GET_DISKGEOM	0x08
#define VD_OP_SET_DISKGEOM	0x09
#define VD_OP_GET_DEVID		0x0b
#define VD_OP_GET_EFI		0x0c
#define VD_OP_SET_EFI		0x0d

/* vDisk version 1.1 */
#define VD_OP_SCSICMD		0x0a
#define VD_OP_RESET		0x0e
#define VD_OP_GET_ACCESS	0x0f
#define VD_OP_SET_ACCESS	0x10
#define VD_OP_GET_CAPACITY	0x11

/* Sun standard fields. */
struct sun_vtoc_preamble {
	char	sl_text[128];
	u_int	sl_version;	/* label version */
	char	sl_volume[8];	/* short volume name */
	u_short	sl_nparts;	/* partition count */

	struct sun_partinfo sl_part[8];

	u_int	sl_bootinfo[3];
	u_int	sl_sanity;
};

struct vd_vtoc_part {
	uint16_t	id_tag;
	uint16_t	perm;
	uint32_t	reserved;
	uint64_t	start;
	uint64_t	nblocks;
	
};
struct vd_vtoc {
	uint8_t		volume_name[8];
	uint16_t	sector_size;
	uint16_t	num_partitions;
	uint32_t	reserved;
	uint8_t		ascii_label[128];
	struct vd_vtoc_part partition[8];
};

struct vd_diskgeom {
	uint16_t	ncyl;
	uint16_t	acyl;
	uint16_t	bcyl;
	uint16_t	nhead;
	uint16_t	nsect;
	uint16_t	intrlv;
	uint16_t	apc;
	uint16_t	rpm;
	uint16_t	pcyl;
	uint16_t	write_reinstruct;
	uint16_t	read_reinstruct;
};

struct vd_desc {
	struct vio_dring_hdr	hdr;
	uint64_t		req_id;
	uint8_t			operation;
	uint8_t			slice;
	uint16_t		_reserved1;
	uint32_t		status;
	uint64_t		offset;
	uint64_t		size;
	uint32_t		ncookies;
	uint32_t		_reserved2;
	struct ldc_cookie	cookie[1];
};

#define VD_SLICE_NONE		0xff

struct vdsk_desc_msg {
	struct vio_msg_tag	tag;
	uint64_t		seq_no;
	uint64_t		desc_handle;
	uint64_t		req_id;
	uint8_t			operation;
	uint8_t			slice;
	uint16_t		_reserved1;
	uint32_t		status;
	uint64_t		offset;
	uint64_t		size;
	uint32_t		ncookies;
	uint32_t		_reserved2;
	struct ldc_cookie	cookie[1];
};

/*
 * We support vDisk 1.1.
 */
#define VDSK_MAJOR	1
#define VDSK_MINOR	1

/*
 * But we only support a subset of the defined commands.
 */
#define VD_OP_MASK \
    ((1 << VD_OP_BREAD) | (1 << VD_OP_BWRITE) | (1 << VD_OP_FLUSH) | \
     (1 << VD_OP_GET_VTOC) | (1 << VD_OP_SET_VTOC) | \
     (1 << VD_OP_GET_DISKGEOM))

struct vdsp_softc {
	struct device	sc_dv;
	int		sc_idx;
	bus_space_tag_t	sc_bustag;
	bus_dma_tag_t	sc_dmatag;

	uint64_t	sc_tx_sysino;
	uint64_t	sc_rx_sysino;
	void		*sc_tx_ih;
	void		*sc_rx_ih;

	struct ldc_conn	sc_lc;

	uint16_t	sc_vio_state;
#define VIO_SND_VER_INFO	0x0001
#define VIO_ACK_VER_INFO	0x0002
#define VIO_RCV_VER_INFO	0x0004
#define VIO_SND_ATTR_INFO	0x0008
#define VIO_ACK_ATTR_INFO	0x0010
#define VIO_RCV_ATTR_INFO	0x0020
#define VIO_SND_DRING_REG	0x0040
#define VIO_ACK_DRING_REG	0x0080
#define VIO_RCV_DRING_REG	0x0100
#define VIO_SND_RDX		0x0200
#define VIO_ACK_RDX		0x0400
#define VIO_RCV_RDX		0x0800

	uint16_t	sc_major;
	uint16_t	sc_minor;

	uint8_t		sc_xfer_mode;

	uint32_t	sc_local_sid;
	uint64_t	sc_seq_no;

	uint64_t	sc_dring_ident;
	uint32_t	sc_num_descriptors;
	uint32_t	sc_descriptor_size;
	struct ldc_cookie sc_dring_cookie;

	caddr_t		sc_vd;

	int		sc_tx_cnt;
	int		sc_tx_prod;
	int		sc_tx_cons;

	uint32_t	sc_vdisk_block_size;
	uint64_t	sc_vdisk_size;

	struct vnode	*sc_vp;

	struct sun_disklabel *sc_label;
	uint16_t	sc_ncyl;
	uint16_t	sc_acyl;
	uint16_t	sc_nhead;
	uint16_t	sc_nsect;
};

int	vdsp_match(struct device *, void *, void *);
void	vdsp_attach(struct device *, struct device *, void *);

struct cfattach vdsp_ca = {
	sizeof(struct vdsp_softc), vdsp_match, vdsp_attach
};

struct cfdriver vdsp_cd = {
	NULL, "vdsp", DV_DULL
};

int	vdsp_tx_intr(void *);
int	vdsp_rx_intr(void *);

void	vdsp_rx_data(struct ldc_conn *, struct ldc_pkt *);
void	vdsp_rx_vio_ctrl(struct vdsp_softc *, struct vio_msg *);
void	vdsp_rx_vio_ver_info(struct vdsp_softc *, struct vio_msg_tag *);
void	vdsp_rx_vio_attr_info(struct vdsp_softc *, struct vio_msg_tag *);
void	vdsp_rx_vio_dring_reg(struct vdsp_softc *, struct vio_msg_tag *);
void	vdsp_rx_vio_rdx(struct vdsp_softc *sc, struct vio_msg_tag *);
void	vdsp_rx_vio_data(struct vdsp_softc *sc, struct vio_msg *);
void	vdsp_rx_vio_dring_data(struct vdsp_softc *sc,
	    struct vio_msg_tag *);
void	vdsp_rx_vio_desc_data(struct vdsp_softc *sc, struct vio_msg_tag *);

void	vdsp_ldc_reset(struct ldc_conn *);
void	vdsp_ldc_start(struct ldc_conn *);

void	vdsp_sendmsg(struct vdsp_softc *, void *, size_t);

void	vdsp_mountroot(void *);
void	vdsp_open(void *, void *);
void	vdsp_alloc(void *, void *);
void	vdsp_readlabel(struct vdsp_softc *);
int	vdsp_writelabel(struct vdsp_softc *);
void	vdsp_read(void *, void *);
void	vdsp_read_dring(void *, void *);
void	vdsp_write_dring(void *, void *);
void	vdsp_flush_dring(void *, void *);
void	vdsp_get_vtoc(void *, void *);
void	vdsp_set_vtoc(void *, void *);
void	vdsp_get_diskgeom(void *, void *);
void	vdsp_unimp(void *, void *);

void	vdsp_ack_desc(struct vdsp_softc *, struct vd_desc *);

int
vdsp_match(struct device *parent, void *match, void *aux)
{
	struct cbus_attach_args *ca = aux;

	if (strcmp(ca->ca_name, "vds-port") == 0)
		return (1);

	return (0);
}

void
vdsp_attach(struct device *parent, struct device *self, void *aux)
{
	struct vdsp_softc *sc = (struct vdsp_softc *)self;
	struct cbus_attach_args *ca = aux;
	struct ldc_conn *lc;

	sc->sc_idx = ca->ca_idx;
	sc->sc_bustag = ca->ca_bustag;
	sc->sc_dmatag = ca->ca_dmatag;

	if (cbus_intr_map(ca->ca_node, ca->ca_tx_ino, &sc->sc_tx_sysino) ||
	    cbus_intr_map(ca->ca_node, ca->ca_rx_ino, &sc->sc_rx_sysino)) {
		printf(": can't map interrupt\n");
		return;
	}
	printf(": ivec 0x%lx, 0x%lx", sc->sc_tx_sysino, sc->sc_tx_sysino);

	/*
	 * Un-configure queues before registering interrupt handlers,
	 * such that we dont get any stale LDC packets or events.
	 */
	hv_ldc_tx_qconf(ca->ca_id, 0, 0);
	hv_ldc_rx_qconf(ca->ca_id, 0, 0);

	sc->sc_tx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_tx_sysino,
	    IPL_BIO, 0, vdsp_tx_intr, sc, sc->sc_dv.dv_xname);
	sc->sc_rx_ih = bus_intr_establish(ca->ca_bustag, sc->sc_rx_sysino,
	    IPL_BIO, 0, vdsp_rx_intr, sc, sc->sc_dv.dv_xname);
	if (sc->sc_tx_ih == NULL || sc->sc_rx_ih == NULL) {
		printf(", can't establish interrupt\n");
		return;
	}

	/*
	 * Disable interrupts while we have no queues allocated.
	 * Otherwise we may end up with an interrupt storm as soon as
	 * our peer places a packet in their transmit queue.
	 */
	cbus_intr_setenabled(sc->sc_tx_sysino, INTR_DISABLED);
	cbus_intr_setenabled(sc->sc_rx_sysino, INTR_DISABLED);

	lc = &sc->sc_lc;
	lc->lc_id = ca->ca_id;
	lc->lc_sc = sc;
	lc->lc_reset = vdsp_ldc_reset;
	lc->lc_start = vdsp_ldc_start;
	lc->lc_rx_data = vdsp_rx_data;

	lc->lc_txq = ldc_queue_alloc(sc->sc_dmatag, VDSK_TX_ENTRIES);
	if (lc->lc_txq == NULL) {
		printf(", can't allocate tx queue\n");
		return;
	}

	lc->lc_rxq = ldc_queue_alloc(sc->sc_dmatag, VDSK_RX_ENTRIES);
	if (lc->lc_rxq == NULL) {
		printf(", can't allocate rx queue\n");
		goto free_txqueue;
	}

	printf("\n");

	mountroothook_establish(vdsp_mountroot, sc);
	return;

#if 0
free_rxqueue:
	ldc_queue_free(sc->sc_dmatag, lc->lc_rxq);
#endif
free_txqueue:
	ldc_queue_free(sc->sc_dmatag, lc->lc_txq);
}

int
vdsp_tx_intr(void *arg)
{
	struct vdsp_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t tx_head, tx_tail, tx_state;

	hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (tx_state != lc->lc_tx_state) {
		switch (tx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Tx link down\n"));
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Tx link up\n"));
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Tx link reset\n"));
			break;
		}
		lc->lc_tx_state = tx_state;
	}

	return (1);
}

int
vdsp_rx_intr(void *arg)
{
	struct vdsp_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	uint64_t rx_head, rx_tail, rx_state;
	struct ldc_pkt *lp;
	int err;

	err = hv_ldc_rx_get_state(lc->lc_id, &rx_head, &rx_tail, &rx_state);
	if (err == H_EINVAL)
		return (0);
	if (err != H_EOK) {
		printf("hv_ldc_rx_get_state %d\n", err);
		return (0);
	}

	if (rx_state != lc->lc_rx_state) {
		switch (rx_state) {
		case LDC_CHANNEL_DOWN:
			DPRINTF(("Rx link down\n"));
			lc->lc_tx_seqid = 0;
			lc->lc_state = 0;
			lc->lc_reset(lc);
			break;
		case LDC_CHANNEL_UP:
			DPRINTF(("Rx link up\n"));
			break;
		case LDC_CHANNEL_RESET:
			DPRINTF(("Rx link reset\n"));
			lc->lc_tx_seqid = 0;
			lc->lc_state = 0;
			lc->lc_reset(lc);
			break;
		}
		lc->lc_rx_state = rx_state;
		return (1);
	}

	if (lc->lc_rx_state == LDC_CHANNEL_DOWN)
		return (1);

	lp = (struct ldc_pkt *)(lc->lc_rxq->lq_va + rx_head);
	switch (lp->type) {
	case LDC_CTRL:
		ldc_rx_ctrl(lc, lp);
		break;

	case LDC_DATA:
		ldc_rx_data(lc, lp);
		break;

	default:
		DPRINTF(("0x%02x/0x%02x/0x%02x\n", lp->type, lp->stype,
		    lp->ctrl));
		ldc_reset(lc);
		break;
	}

	rx_head += sizeof(*lp);
	rx_head &= ((lc->lc_rxq->lq_nentries * sizeof(*lp)) - 1);
	err = hv_ldc_rx_set_qhead(lc->lc_id, rx_head);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_set_qhead %d\n", __func__, err);

	return (1);
}

void
vdsp_rx_data(struct ldc_conn *lc, struct ldc_pkt *lp)
{
	struct vio_msg *vm = (struct vio_msg *)lp;

	switch (vm->type) {
	case VIO_TYPE_CTRL:
		if ((lp->env & LDC_FRAG_START) == 0 &&
		    (lp->env & LDC_FRAG_STOP) == 0)
			return;
		vdsp_rx_vio_ctrl(lc->lc_sc, vm);
		break;

	case VIO_TYPE_DATA:
		if((lp->env & LDC_FRAG_START) == 0)
			return;
		vdsp_rx_vio_data(lc->lc_sc, vm);
		break;

	default:
		DPRINTF(("Unhandled packet type 0x%02x\n", vm->type));
		ldc_reset(lc);
		break;
	}
}

void
vdsp_rx_vio_ctrl(struct vdsp_softc *sc, struct vio_msg *vm)
{
	struct vio_msg_tag *tag = (struct vio_msg_tag *)&vm->type;

	switch (tag->stype_env) {
	case VIO_VER_INFO:
		vdsp_rx_vio_ver_info(sc, tag);
		break;
	case VIO_ATTR_INFO:
		vdsp_rx_vio_attr_info(sc, tag);
		break;
	case VIO_DRING_REG:
		vdsp_rx_vio_dring_reg(sc, tag);
		break;
	case VIO_RDX:
		vdsp_rx_vio_rdx(sc, tag);
		break;
	default:
		DPRINTF(("CTRL/0x%02x/0x%04x\n", tag->stype, tag->stype_env));
		break;
	}
}

void
vdsp_rx_vio_ver_info(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_ver_info *vi = (struct vio_ver_info *)tag;

	switch (vi->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/VER_INFO\n"));

		/* Make sure we're talking to a virtual disk. */
		if (vi->dev_class != VDEV_DISK) {
			/* Huh, we're not talking to a disk device? */
			printf("%s: peer is not a disk device\n",
			    sc->sc_dv.dv_xname);
			vi->tag.stype = VIO_SUBTYPE_NACK;
			vi->major = 0;
			vdsp_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		if (vi->major != VDSK_MAJOR) {
			vi->tag.stype = VIO_SUBTYPE_NACK;
			vi->major = VDSK_MAJOR;
			vi->minor = VDSK_MINOR;
			vdsp_sendmsg(sc, vi, sizeof(*vi));
			return;
		}

		sc->sc_major = vi->major;
		sc->sc_minor = vi->minor;
		sc->sc_local_sid = vi->tag.sid;

		vi->tag.stype = VIO_SUBTYPE_ACK;
		if (vi->minor > VDSK_MINOR)
			vi->minor = VDSK_MINOR;
		vi->dev_class = VDEV_DISK_SERVER;
		vdsp_sendmsg(sc, vi, sizeof(*vi));
		sc->sc_vio_state |= VIO_RCV_VER_INFO;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/VER_INFO\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/VER_INFO\n", vi->tag.stype));
		break;
	}
}

void
vdsp_rx_vio_attr_info(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	struct vd_attr_info *ai = (struct vd_attr_info *)tag;

	switch (ai->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/ATTR_INFO\n"));

		if (ai->xfer_mode != VIO_DESC_MODE &&
		    ai->xfer_mode != VIO_DRING_MODE) {
			printf("%s: peer uses unsupported xfer mode 0x%02x\n",
			    sc->sc_dv.dv_xname, ai->xfer_mode);
			ai->tag.stype = VIO_SUBTYPE_NACK;
			vdsp_sendmsg(sc, ai, sizeof(*ai));
			return;
		}
		sc->sc_xfer_mode = ai->xfer_mode;
		sc->sc_vio_state |= VIO_RCV_ATTR_INFO;

		workq_add_task(NULL, 0, vdsp_open, sc, NULL);
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/ATTR_INFO\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/ATTR_INFO\n", ai->tag.stype));
		break;
	}
}

void
vdsp_rx_vio_dring_reg(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_dring_reg *dr = (struct vio_dring_reg *)tag;

	switch (dr->tag.stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/DRING_REG\n"));

		if (dr->num_descriptors > VDSK_MAX_DESCRIPTORS ||
		    dr->descriptor_size > VDSK_MAX_DESCRIPTOR_SIZE ||
		    dr->ncookies > 1) {
			dr->tag.stype = VIO_SUBTYPE_NACK;
			vdsp_sendmsg(sc, dr, sizeof(*dr));
			return;
		}
		sc->sc_num_descriptors = dr->num_descriptors;
		sc->sc_descriptor_size = dr->descriptor_size;
		sc->sc_dring_cookie = dr->cookie[0];
		sc->sc_vio_state |= VIO_RCV_DRING_REG;

		workq_add_task(NULL, 0, vdsp_alloc, sc, NULL);
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/DRING_REG\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/DRING_REG\n", dr->tag.stype));
		break;
	}
}

void
vdsp_rx_vio_rdx(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("CTRL/INFO/RDX\n"));

		tag->stype = VIO_SUBTYPE_ACK;
		tag->sid = sc->sc_local_sid;
		vdsp_sendmsg(sc, tag, sizeof(*tag));
		sc->sc_vio_state |= VIO_RCV_RDX;
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("CTRL/ACK/RDX\n"));
		break;

	default:
		DPRINTF(("CTRL/0x%02x/RDX (VIO)\n", tag->stype));
		break;
	}
}

void
vdsp_rx_vio_data(struct vdsp_softc *sc, struct vio_msg *vm)
{
	struct vio_msg_tag *tag = (struct vio_msg_tag *)&vm->type;

	if (!ISSET(sc->sc_vio_state, VIO_RCV_RDX)) {
		DPRINTF(("Spurious DATA/0x%02x/0x%04x\n", tag->stype,
		    tag->stype_env));
		return;
	}

	switch(tag->stype_env) {
	case VIO_DESC_DATA:
		vdsp_rx_vio_desc_data(sc, tag);
		break;

	case VIO_DRING_DATA:
		vdsp_rx_vio_dring_data(sc, tag);
		break;

	default:
		DPRINTF(("DATA/0x%02x/0x%04x\n", tag->stype, tag->stype_env));
		break;
	}
}

void
vdsp_rx_vio_dring_data(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	struct vio_dring_msg *dm = (struct vio_dring_msg *)tag;
	struct vd_desc *vd;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err;

	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("DATA/INFO/DRING_DATA\n"));

		if (dm->dring_ident != sc->sc_dring_ident ||
		    dm->start_idx >= sc->sc_num_descriptors) {
			dm->tag.stype = VIO_SUBTYPE_NACK;
			vdsp_sendmsg(sc, dm, sizeof(*dm));
			return;
		}

		off = dm->start_idx * sc->sc_descriptor_size;
		vd = (struct vd_desc *)(sc->sc_vd + off);
		va = (vaddr_t)vd;
		size = sc->sc_descriptor_size;
		while (size > 0) {
			pmap_extract(pmap_kernel(), va, &pa);
			nbytes = min(size, PAGE_SIZE - (off & PAGE_MASK));
			err = hv_ldc_copy(sc->sc_lc.lc_id, LDC_COPY_IN,
			    sc->sc_dring_cookie.addr | off, pa,
			    nbytes, &nbytes);
			va += nbytes;
			size -= nbytes;
			off += nbytes;
		}
		if (err != H_EOK) {
			printf("%s: hv_ldc_copy %d\n", __func__, err);
			return;
		}

		DPRINTF(("%s: start_idx %d, end_idx %d, operation %x\n",
		    sc->sc_dv.dv_xname, dm->start_idx, dm->end_idx,
		    vd->operation));
		switch (vd->operation) {
		case VD_OP_BREAD:
			workq_add_task(NULL, 0, vdsp_read_dring, sc, vd);
			break;
		case VD_OP_BWRITE:
			workq_add_task(NULL, 0, vdsp_write_dring, sc, vd);
			break;
		case VD_OP_FLUSH:
			workq_add_task(NULL, 0, vdsp_flush_dring, sc, vd);
			break;
		case VD_OP_GET_VTOC:
			workq_add_task(NULL, 0, vdsp_get_vtoc, sc, vd);
			break;
		case VD_OP_SET_VTOC:
			workq_add_task(NULL, 0, vdsp_set_vtoc, sc, vd);
			break;
		case VD_OP_GET_DISKGEOM:
			workq_add_task(NULL, 0, vdsp_get_diskgeom, sc, vd);
			break;
		default:
			printf("%s: unsupported operation 0x%02x\n",
			    sc->sc_dv.dv_xname, vd->operation);
			workq_add_task(NULL, 0, vdsp_unimp, sc, vd);
			break;
		}
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("DATA/ACK/DRING_DATA\n"));
		break;

	case VIO_SUBTYPE_NACK:
		DPRINTF(("DATA/NACK/DRING_DATA\n"));
		break;

	default:
		DPRINTF(("DATA/0x%02x/DRING_DATA\n", tag->stype));
		break;
	}
}

void
vdsp_rx_vio_desc_data(struct vdsp_softc *sc, struct vio_msg_tag *tag)
{
	struct vdsk_desc_msg *dm = (struct vdsk_desc_msg *)tag;

	switch(tag->stype) {
	case VIO_SUBTYPE_INFO:
		DPRINTF(("DATA/INFO/DESC_DATA\n"));

		switch (dm->operation) {
		case VD_OP_BREAD:
			workq_add_task(NULL, 0, vdsp_read, sc, dm);
			break;
		default:
			printf("%s: unsupported operation 0x%02x\n",
			    sc->sc_dv.dv_xname, dm->operation);
			break;
		}
		break;

	case VIO_SUBTYPE_ACK:
		DPRINTF(("DATA/ACK/DESC_DATA\n"));
		break;

	case VIO_SUBTYPE_NACK:
		DPRINTF(("DATA/NACK/DESC_DATA\n"));
		break;

	default:
		DPRINTF(("DATA/0x%02x/DESC_DATA\n", tag->stype));
		break;
	}
}

void
vdsp_ldc_reset(struct ldc_conn *lc)
{
	struct vdsp_softc *sc = lc->lc_sc;

	sc->sc_vio_state = 0;
	sc->sc_seq_no = 0;
	if (sc->sc_vd) {
		free(sc->sc_vd, M_DEVBUF);
		sc->sc_vd = NULL;
	}
	if (sc->sc_label) {
		free(sc->sc_label, M_DEVBUF);
		sc->sc_label = NULL;
	}
}

void
vdsp_ldc_start(struct ldc_conn *lc)
{
	/* The vDisk client is supposed to initiate the handshake. */
}

void
vdsp_sendmsg(struct vdsp_softc *sc, void *msg, size_t len)
{
	struct ldc_conn *lc = &sc->sc_lc;
	struct ldc_pkt *lp;
	uint64_t tx_head, tx_tail, tx_state;
	uint8_t *p = msg;
	int err;

	err = hv_ldc_tx_get_state(lc->lc_id, &tx_head, &tx_tail, &tx_state);
	if (err != H_EOK)
		return;

	while (len > 0) {
		lp = (struct ldc_pkt *)(lc->lc_txq->lq_va + tx_tail);
		bzero(lp, sizeof(struct ldc_pkt));
		lp->type = LDC_DATA;
		lp->stype = LDC_INFO;
		lp->env = min(len, LDC_PKT_PAYLOAD);
		if (p == msg)
			lp->env |= LDC_FRAG_START;
		if (len <= LDC_PKT_PAYLOAD)
			lp->env |= LDC_FRAG_STOP;
		lp->seqid = lc->lc_tx_seqid++;
		bcopy(p, &lp->major, min(len, LDC_PKT_PAYLOAD));

		tx_tail += sizeof(*lp);
		tx_tail &= ((lc->lc_txq->lq_nentries * sizeof(*lp)) - 1);
		err = hv_ldc_tx_set_qtail(lc->lc_id, tx_tail);
		if (err != H_EOK)
			printf("%s: hv_ldc_tx_set_qtail: %d\n", __func__, err);
		p += min(len, LDC_PKT_PAYLOAD);
		len -= min(len, LDC_PKT_PAYLOAD);
	}
}

void
vdsp_mountroot(void *arg)
{
	struct vdsp_softc *sc = arg;
	struct ldc_conn *lc = &sc->sc_lc;
	int err;

	err = hv_ldc_tx_qconf(lc->lc_id,
	    lc->lc_txq->lq_map->dm_segs[0].ds_addr, lc->lc_txq->lq_nentries);
	if (err != H_EOK)
		printf("%s: hv_ldc_tx_qconf %d\n", __func__, err);

	err = hv_ldc_rx_qconf(lc->lc_id,
	    lc->lc_rxq->lq_map->dm_segs[0].ds_addr, lc->lc_rxq->lq_nentries);
	if (err != H_EOK)
		printf("%s: hv_ldc_rx_qconf %d\n", err, __func__);

	cbus_intr_setenabled(sc->sc_tx_sysino, INTR_ENABLED);
	cbus_intr_setenabled(sc->sc_rx_sysino, INTR_ENABLED);
}

void
vdsp_open(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct proc *p = curproc;
	struct vd_attr_info ai;

	if (sc->sc_vp == NULL) {
		struct nameidata nd;
		struct vattr va;
		const char *name;
		int error;

		name = mdesc_get_prop_str(sc->sc_idx, "vds-block-device");
		if (name == NULL)
			return;

		NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, name, p);
		error = vn_open(&nd, FREAD, 0);
		if (error) {
			printf("VOP_OPEN: %s, %d\n", name, error);
			return;
		}

		error = VOP_GETATTR(nd.ni_vp, &va, p->p_ucred, p);
		if (error)
			printf("VOP_GETATTR: %s, %d\n", name, error);
		sc->sc_vdisk_block_size = DEV_BSIZE;
		sc->sc_vdisk_size = va.va_size / DEV_BSIZE;

		VOP_UNLOCK(nd.ni_vp, 0, p);
		sc->sc_vp = nd.ni_vp;

		vdsp_readlabel(sc);
	}

	bzero(&ai, sizeof(ai));
	ai.tag.type = VIO_TYPE_CTRL;
	ai.tag.stype = VIO_SUBTYPE_ACK;
	ai.tag.stype_env = VIO_ATTR_INFO;
	ai.tag.sid = sc->sc_local_sid;
	ai.xfer_mode = sc->sc_xfer_mode;
	ai.vd_type = VD_DISK_TYPE_DISK;
	if (sc->sc_major > 1 || sc->sc_minor >= 1)
		ai.vd_mtype = VD_MEDIA_TYPE_FIXED;
	ai.vdisk_block_size = sc->sc_vdisk_block_size;
	ai.operations = VD_OP_MASK;
	ai.vdisk_size = sc->sc_vdisk_size;
	ai.max_xfer_sz = MAXPHYS / sc->sc_vdisk_block_size;
	vdsp_sendmsg(sc, &ai, sizeof(ai));
}

void
vdsp_readlabel(struct vdsp_softc *sc)
{
	struct proc *p = curproc;
	struct iovec iov;
	struct uio uio;
	int err;

	if (sc->sc_vp == NULL)
		return;

	sc->sc_label = malloc(sizeof(*sc->sc_label), M_DEVBUF, M_WAITOK);

	iov.iov_base = sc->sc_label;
	iov.iov_len = sizeof(*sc->sc_label);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = sizeof(*sc->sc_label);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	err = VOP_READ(sc->sc_vp, &uio, 0, p->p_ucred);
	VOP_UNLOCK(sc->sc_vp, 0, p);
	if (err) {
		free(sc->sc_label, M_DEVBUF);
		sc->sc_label = NULL;
	}
}

int
vdsp_writelabel(struct vdsp_softc *sc)
{
	struct proc *p = curproc;
	struct iovec iov;
	struct uio uio;
	int err;

	if (sc->sc_vp == NULL || sc->sc_label == NULL)
		return (EINVAL);

	iov.iov_base = sc->sc_label;
	iov.iov_len = sizeof(*sc->sc_label);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = sizeof(*sc->sc_label);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	err = VOP_WRITE(sc->sc_vp, &uio, 0, p->p_ucred);
	VOP_UNLOCK(sc->sc_vp, 0, p);

	return (err);
}

void
vdsp_alloc(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct vio_dring_reg dr;

	KASSERT(sc->sc_num_descriptors <= VDSK_MAX_DESCRIPTORS);
	KASSERT(sc->sc_descriptor_size <= VDSK_MAX_DESCRIPTOR_SIZE);
	sc->sc_vd = malloc(sc->sc_num_descriptors * sc->sc_descriptor_size,
	    M_WAITOK, M_DEVBUF);

	bzero(&dr, sizeof(dr));
	dr.tag.type = VIO_TYPE_CTRL;
	dr.tag.stype = VIO_SUBTYPE_ACK;
	dr.tag.stype_env = VIO_DRING_REG;
	dr.tag.sid = sc->sc_local_sid;
	dr.dring_ident = ++sc->sc_dring_ident;
	vdsp_sendmsg(sc, &dr, sizeof(dr));
}

void
vdsp_read(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vdsk_desc_msg *dm = arg2;
	struct proc *p = curproc;
	struct iovec iov;
	struct uio uio;
	caddr_t buf;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	if (sc->sc_vp == NULL)
		return;

	buf = malloc(dm->size, M_DEVBUF, M_WAITOK);

	iov.iov_base = buf;
	iov.iov_len = dm->size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = dm->offset * DEV_BSIZE;
	uio.uio_resid = dm->size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	dm->status = VOP_READ(sc->sc_vp, &uio, 0, p->p_ucred);
	VOP_UNLOCK(sc->sc_vp, 0, p);

	if (dm->status == 0) {
		i = 0;
		va = (vaddr_t)buf;
		size = dm->size;
		off = 0;
		while (size > 0 && i < dm->ncookies) {
			pmap_extract(pmap_kernel(), va, &pa);
			nbytes = min(size, dm->cookie[i].size - off);
			nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT,
			    dm->cookie[i].addr + off, pa, nbytes, &nbytes);
			if (err != H_EOK)
				printf("%s: hv_ldc_copy: %d\n", __func__, err);
			va += nbytes;
			size -= nbytes;
			off += nbytes;
			if (off >= dm->cookie[i].size) {
				off = 0;
				i++;
			}
		}
	}

	free(buf, M_DEVBUF);

	/* ACK the descriptor. */
	dm->tag.stype = VIO_SUBTYPE_ACK;
	dm->tag.sid = sc->sc_local_sid;
	vdsp_sendmsg(sc, dm, sizeof(*dm) +
	    (dm->ncookies - 1) * sizeof(struct ldc_cookie));
}

void
vdsp_read_dring(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vd_desc *vd = arg2;
	struct proc *p = curproc;
	struct iovec iov;
	struct uio uio;
	caddr_t buf;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	if (sc->sc_vp == NULL)
		return;

	buf = malloc(vd->size, M_DEVBUF, M_WAITOK);

	iov.iov_base = buf;
	iov.iov_len = vd->size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = vd->offset * DEV_BSIZE;
	uio.uio_resid = vd->size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	vd->status = VOP_READ(sc->sc_vp, &uio, 0, p->p_ucred);
	VOP_UNLOCK(sc->sc_vp, 0, p);

	if (vd->status == 0) {
		i = 0;
		va = (vaddr_t)buf;
		size = vd->size;
		off = 0;
		while (size > 0 && i < vd->ncookies) {
			pmap_extract(pmap_kernel(), va, &pa);
			nbytes = min(size, vd->cookie[i].size - off);
			nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
			err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT,
			    vd->cookie[i].addr + off, pa, nbytes, &nbytes);
			if (err != H_EOK)
				printf("%s: hv_ldc_copy: %d\n", __func__, err);
			va += nbytes;
			size -= nbytes;
			off += nbytes;
			if (off >= vd->cookie[i].size) {
				off = 0;
				i++;
			}
		}
	}

	free(buf, M_DEVBUF);

	/* ACK the descriptor. */
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_write_dring(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vd_desc *vd = arg2;
	struct proc *p = curproc;
	struct iovec iov;
	struct uio uio;
	caddr_t buf;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	if (sc->sc_vp == NULL)
		return;

	buf = malloc(vd->size, M_DEVBUF, M_WAITOK);

	i = 0;
	va = (vaddr_t)buf;
	size = vd->size;
	off = 0;
	while (size > 0 && i < vd->ncookies) {
		pmap_extract(pmap_kernel(), va, &pa);
		nbytes = min(size, vd->cookie[i].size - off);
		nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
		err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN,
		    vd->cookie[i].addr + off, pa, nbytes, &nbytes);
		if (err != H_EOK)
			printf("%s: hv_ldc_copy: %d\n", __func__, err);
		va += nbytes;
		size -= nbytes;
		off += nbytes;
		if (off >= vd->cookie[i].size) {
			off = 0;
			i++;
		}
	}

	iov.iov_base = buf;
	iov.iov_len = vd->size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = vd->offset * DEV_BSIZE;
	uio.uio_resid = vd->size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = p;

	vn_lock(sc->sc_vp, LK_EXCLUSIVE | LK_RETRY, p);
	vd->status = VOP_WRITE(sc->sc_vp, &uio, 0, p->p_ucred);
	VOP_UNLOCK(sc->sc_vp, 0, p);

	free(buf, M_DEVBUF);

	/* ACK the descriptor. */
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_flush_dring(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct vd_desc *vd = arg2;

	if (sc->sc_vp == NULL)
		return;

	/* ACK the descriptor. */
	vd->status = 0;
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_get_vtoc(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vd_desc *vd = arg2;
	struct sun_vtoc_preamble *sl;
	struct vd_vtoc *vt;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	vt = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO);

	if (sc->sc_label == NULL)
		vdsp_readlabel(sc);

	if (sc->sc_label && sc->sc_label->sl_magic == SUN_DKMAGIC) {
		sl = (struct sun_vtoc_preamble *)sc->sc_label;

		memcpy(vt->ascii_label, sl->sl_text, sizeof(sl->sl_text));
		memcpy(vt->volume_name, sl->sl_volume, sizeof(sl->sl_volume));
		vt->sector_size = DEV_BSIZE;
		vt->num_partitions = sl->sl_nparts;
		for (i = 0; i < vt->num_partitions; i++) {
			vt->partition[i].id_tag = sl->sl_part[i].spi_tag;
			vt->partition[i].perm = sl->sl_part[i].spi_flag;
			vt->partition[i].start =
			    sc->sc_label->sl_part[i].sdkp_cyloffset *
				sc->sc_label->sl_ntracks *
				sc->sc_label->sl_nsectors;
			vt->partition[i].nblocks =
			    sc->sc_label->sl_part[i].sdkp_nsectors;
		}
	} else {
		uint64_t disk_size;
		int unit;

		/* Human-readable disk size. */
		disk_size = sc->sc_vdisk_size * sc->sc_vdisk_block_size;
		disk_size >>= 10;
		unit = 'K';
		if (disk_size > (2 << 10)) {
			disk_size >>= 10;
			unit = 'M';
		}
		if (disk_size > (2 << 10)) {
			disk_size >>= 10;
			unit = 'G';
		}

		snprintf(vt->ascii_label, sizeof(vt->ascii_label),
		    "OpenBSD-DiskImage-%lld%cB cyl %d alt %d hd %d sec %d",
		    disk_size, unit, sc->sc_ncyl, sc->sc_acyl,
		    sc->sc_nhead, sc->sc_nsect);
		vt->sector_size = sc->sc_vdisk_block_size;
		vt->num_partitions = 8;
		vt->partition[2].id_tag = SPTAG_WHOLE_DISK;
		vt->partition[2].nblocks =
		    sc->sc_ncyl * sc->sc_nhead * sc->sc_nsect;
	}

	i = 0;
	va = (vaddr_t)vt;
	size = roundup(sizeof(*vt), 64);
	off = 0;
	while (size > 0 && i < vd->ncookies) {
		pmap_extract(pmap_kernel(), va, &pa);
		nbytes = min(size, vd->cookie[i].size - off);
		nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
		err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT,
		    vd->cookie[i].addr + off, pa, nbytes, &nbytes);
		if (err != H_EOK)
			printf("%s: hv_ldc_copy: %d\n", __func__, err);
		va += nbytes;
		size -= nbytes;
		off += nbytes;
		if (off >= vd->cookie[i].size) {
			off = 0;
			i++;
		}
	}

	free(vt, M_DEVBUF);

	/* ACK the descriptor. */
	vd->status = 0;
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_set_vtoc(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vd_desc *vd = arg2;
	struct sun_vtoc_preamble *sl;
	struct vd_vtoc *vt;
	u_short cksum = 0, *sp1, *sp2;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	vt = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO);

	i = 0;
	va = (vaddr_t)vt;
	size = sizeof(*vt);
	off = 0;
	while (size > 0 && i < vd->ncookies) {
		pmap_extract(pmap_kernel(), va, &pa);
		nbytes = min(size, vd->cookie[i].size - off);
		nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
		err = hv_ldc_copy(lc->lc_id, LDC_COPY_IN,
		    vd->cookie[i].addr + off, pa, nbytes, &nbytes);
		if (err != H_EOK)
			printf("%s: hv_ldc_copy: %d\n", __func__, err);
		va += nbytes;
		size -= nbytes;
		off += nbytes;
		if (off >= vd->cookie[i].size) {
			off = 0;
			i++;
		}
	}

	if (vt->num_partitions > nitems(sc->sc_label->sl_part)) {
		vd->status = EINVAL;
		goto fail;
	}

	if (sc->sc_label == NULL || sc->sc_label->sl_magic != SUN_DKMAGIC) {
		sc->sc_label = malloc(sizeof(*sc->sc_label),
		    M_DEVBUF, M_WAITOK | M_ZERO);

		sc->sc_label->sl_ntracks = sc->sc_nhead;
		sc->sc_label->sl_nsectors = sc->sc_nsect;
		sc->sc_label->sl_ncylinders = sc->sc_ncyl;
		sc->sc_label->sl_acylinders = sc->sc_acyl;
		sc->sc_label->sl_pcylinders = sc->sc_ncyl + sc->sc_acyl;
		sc->sc_label->sl_rpm = 3600;

		sc->sc_label->sl_magic = SUN_DKMAGIC;
	}

	sl = (struct sun_vtoc_preamble *)sc->sc_label;
	memcpy(sl->sl_text, vt->ascii_label, sizeof(sl->sl_text));
	sl->sl_version = 0x01;
	memcpy(sl->sl_volume, sl->sl_volume, sizeof(sl->sl_volume));
	sl->sl_nparts = vt->num_partitions;
	for (i = 0; i < vt->num_partitions; i++) {
		sl->sl_part[i].spi_tag = vt->partition[i].id_tag;
		sl->sl_part[i].spi_flag = vt->partition[i].perm;
		sc->sc_label->sl_part[i].sdkp_cyloffset =
		    vt->partition[i].start / (sc->sc_nhead * sc->sc_nsect);
		sc->sc_label->sl_part[i].sdkp_nsectors =
		    vt->partition[i].nblocks;
	}
	sl->sl_sanity = 0x600ddeee;

	/* Compute the checksum. */
	sp1 = (u_short *)sc->sc_label;
	sp2 = (u_short *)(sc->sc_label + 1);
	while (sp1 < sp2)
		cksum ^= *sp1++;
	sc->sc_label->sl_cksum = cksum;

	vd->status = vdsp_writelabel(sc);

fail:
	free(vt, M_DEVBUF);

	/* ACK the descriptor. */
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_get_diskgeom(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct ldc_conn *lc = &sc->sc_lc;
	struct vd_desc *vd = arg2;
	struct vd_diskgeom *vg;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err, i;

	vg = malloc(PAGE_SIZE, M_DEVBUF, M_WAITOK | M_ZERO);

	if (sc->sc_label == NULL)
		vdsp_readlabel(sc);

	if (sc->sc_label && sc->sc_label->sl_magic == SUN_DKMAGIC) {
		vg->ncyl = sc->sc_label->sl_ncylinders;
		vg->acyl = sc->sc_label->sl_acylinders;
		vg->nhead = sc->sc_label->sl_ntracks;
		vg->nsect = sc->sc_label->sl_nsectors;
		vg->intrlv = sc->sc_label->sl_interleave;
		vg->apc = sc->sc_label->sl_sparespercyl;
		vg->rpm = sc->sc_label->sl_rpm;
		vg->pcyl = sc->sc_label->sl_pcylinders;
	} else {
		uint64_t disk_size, block_size;

		disk_size = sc->sc_vdisk_size * sc->sc_vdisk_block_size;
		block_size = sc->sc_vdisk_block_size;

		if (disk_size >= 8L * 1024 * 1024 * 1024) {
			vg->nhead = 96;
			vg->nsect = 768;
		} else if (disk_size >= 2 *1024 * 1024) {
			vg->nhead = 1;
			vg->nsect = 600;
		} else {
			vg->nhead = 1;
			vg->nsect = 200;
		}

		vg->pcyl = disk_size / (block_size * vg->nhead * vg->nsect);
		if (vg->pcyl == 0)
			vg->pcyl = 1;
		if (vg->pcyl > 2)
			vg->acyl = 2;
		vg->ncyl = vg->pcyl - vg->acyl;

		vg->rpm = 3600;
	}

	sc->sc_ncyl = vg->ncyl;
	sc->sc_acyl = vg->acyl;
	sc->sc_nhead = vg->nhead;
	sc->sc_nsect = vg->nsect;

	i = 0;
	va = (vaddr_t)vg;
	size = roundup(sizeof(*vg), 64);
	off = 0;
	while (size > 0 && i < vd->ncookies) {
		pmap_extract(pmap_kernel(), va, &pa);
		nbytes = min(size, vd->cookie[i].size - off);
		nbytes = min(nbytes, PAGE_SIZE - (off & PAGE_MASK));
		err = hv_ldc_copy(lc->lc_id, LDC_COPY_OUT,
		    vd->cookie[i].addr + off, pa, nbytes, &nbytes);
		if (err != H_EOK)
			printf("%s: hv_ldc_copy: %d\n", __func__, err);
		va += nbytes;
		size -= nbytes;
		off += nbytes;
		if (off >= vd->cookie[i].size) {
			off = 0;
			i++;
		}
	}

	/* ACK the descriptor. */
	vd->status = 0;
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_unimp(void *arg1, void *arg2)
{
	struct vdsp_softc *sc = arg1;
	struct vd_desc *vd = arg2;

	/* ACK the descriptor. */
	vd->status = ENOTSUP;
	vd->hdr.dstate = VIO_DESC_DONE;
	vdsp_ack_desc(sc, vd);
}

void
vdsp_ack_desc(struct vdsp_softc *sc, struct vd_desc *vd)
{
	struct vio_dring_msg dm;
	vaddr_t va;
	paddr_t pa;
	uint64_t size, off;
	psize_t nbytes;
	int err;

	va = (vaddr_t)vd;
	off = (caddr_t)vd - sc->sc_vd;
	size = sc->sc_descriptor_size;
	while (size > 0) {
		pmap_extract(pmap_kernel(), va, &pa);
		nbytes = min(size, PAGE_SIZE - (off & PAGE_MASK));
		err = hv_ldc_copy(sc->sc_lc.lc_id, LDC_COPY_OUT,
		    sc->sc_dring_cookie.addr | off, pa, nbytes, &nbytes);
		va += nbytes;
		size -= nbytes;
		off += nbytes;
	}
	if (err != H_EOK) {
		printf("%s: hv_ldc_copy %d\n", __func__, err);
		return;
	}

	/* ACK the descriptor. */
	bzero(&dm, sizeof(dm));
	dm.tag.type = VIO_TYPE_DATA;
	dm.tag.stype = VIO_SUBTYPE_ACK;
	dm.tag.stype_env = VIO_DRING_DATA;
	dm.tag.sid = sc->sc_local_sid;
	dm.seq_no = ++sc->sc_seq_no;
	dm.dring_ident = sc->sc_dring_ident;
	off = (caddr_t)vd - sc->sc_vd;
	dm.start_idx = off / sc->sc_descriptor_size;
	dm.end_idx = off / sc->sc_descriptor_size;
	vdsp_sendmsg(sc, &dm, sizeof(dm));
}
