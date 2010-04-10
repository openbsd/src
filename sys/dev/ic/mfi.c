/* $OpenBSD: mfi.c,v 1.102 2010/04/10 17:29:59 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
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
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sensors.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/biovar.h>
#include <dev/ic/mfireg.h>
#include <dev/ic/mfivar.h>

#ifdef MFI_DEBUG
uint32_t	mfi_debug = 0
/*		    | MFI_D_CMD */
/*		    | MFI_D_INTR */
/*		    | MFI_D_MISC */
/*		    | MFI_D_DMA */
/*		    | MFI_D_IOCTL */
/*		    | MFI_D_RW */
/*		    | MFI_D_MEM */
/*		    | MFI_D_CCB */
		;
#endif

struct cfdriver mfi_cd = {
	NULL, "mfi", DV_DULL
};

void	mfi_scsi_cmd(struct scsi_xfer *);
int	mfi_scsi_ioctl(struct scsi_link *, u_long, caddr_t, int, struct proc *);
void	mfiminphys(struct buf *bp, struct scsi_link *sl);

struct scsi_adapter mfi_switch = {
	mfi_scsi_cmd, mfiminphys, 0, 0, mfi_scsi_ioctl
};

struct scsi_device mfi_dev = {
	NULL, NULL, NULL, NULL
};

struct mfi_ccb	*mfi_get_ccb(struct mfi_softc *);
void		mfi_put_ccb(struct mfi_ccb *);
int		mfi_init_ccb(struct mfi_softc *);

struct mfi_mem	*mfi_allocmem(struct mfi_softc *, size_t);
void		mfi_freemem(struct mfi_softc *, struct mfi_mem *);

int		mfi_transition_firmware(struct mfi_softc *);
int		mfi_initialize_firmware(struct mfi_softc *);
int		mfi_get_info(struct mfi_softc *);
uint32_t	mfi_read(struct mfi_softc *, bus_size_t);
void		mfi_write(struct mfi_softc *, bus_size_t, uint32_t);
int		mfi_poll(struct mfi_ccb *);
int		mfi_create_sgl(struct mfi_ccb *, int);

/* commands */
int		mfi_scsi_ld(struct mfi_ccb *, struct scsi_xfer *);
int		mfi_scsi_io(struct mfi_ccb *, struct scsi_xfer *, uint64_t,
		    uint32_t);
void		mfi_scsi_xs_done(struct mfi_ccb *);
int		mfi_mgmt(struct mfi_softc *, uint32_t, uint32_t, uint32_t,
		    void *, uint8_t *);
void		mfi_mgmt_done(struct mfi_ccb *);

#if NBIO > 0
int		mfi_ioctl(struct device *, u_long, caddr_t);
int		mfi_bio_getitall(struct mfi_softc *);
int		mfi_ioctl_inq(struct mfi_softc *, struct bioc_inq *);
int		mfi_ioctl_vol(struct mfi_softc *, struct bioc_vol *);
int		mfi_ioctl_disk(struct mfi_softc *, struct bioc_disk *);
int		mfi_ioctl_alarm(struct mfi_softc *, struct bioc_alarm *);
int		mfi_ioctl_blink(struct mfi_softc *sc, struct bioc_blink *);
int		mfi_ioctl_setstate(struct mfi_softc *, struct bioc_setstate *);
int		mfi_bio_hs(struct mfi_softc *, int, int, void *);
#ifndef SMALL_KERNEL
int		mfi_create_sensors(struct mfi_softc *);
void		mfi_refresh_sensors(void *);
#endif /* SMALL_KERNEL */
#endif /* NBIO > 0 */

void		mfi_start(struct mfi_softc *, struct mfi_ccb *);
void		mfi_done(struct mfi_ccb *);
u_int32_t	mfi_xscale_fw_state(struct mfi_softc *);
void		mfi_xscale_intr_ena(struct mfi_softc *);
int		mfi_xscale_intr(struct mfi_softc *);
void		mfi_xscale_post(struct mfi_softc *, struct mfi_ccb *);

static const struct mfi_iop_ops mfi_iop_xscale = {
	mfi_xscale_fw_state,
	mfi_xscale_intr_ena,
	mfi_xscale_intr,
	mfi_xscale_post
};

u_int32_t	mfi_ppc_fw_state(struct mfi_softc *);
void		mfi_ppc_intr_ena(struct mfi_softc *);
int		mfi_ppc_intr(struct mfi_softc *);
void		mfi_ppc_post(struct mfi_softc *, struct mfi_ccb *);

static const struct mfi_iop_ops mfi_iop_ppc = {
	mfi_ppc_fw_state,
	mfi_ppc_intr_ena,
	mfi_ppc_intr,
	mfi_ppc_post
};

u_int32_t	mfi_gen2_fw_state(struct mfi_softc *);
void		mfi_gen2_intr_ena(struct mfi_softc *);
int		mfi_gen2_intr(struct mfi_softc *);
void		mfi_gen2_post(struct mfi_softc *, struct mfi_ccb *);

static const struct mfi_iop_ops mfi_iop_gen2 = {
	mfi_gen2_fw_state,
	mfi_gen2_intr_ena,
	mfi_gen2_intr,
	mfi_gen2_post
};

#define mfi_fw_state(_s)	((_s)->sc_iop->mio_fw_state(_s))
#define mfi_intr_enable(_s)	((_s)->sc_iop->mio_intr_ena(_s))
#define mfi_my_intr(_s)		((_s)->sc_iop->mio_intr(_s))
#define mfi_post(_s, _c)	((_s)->sc_iop->mio_post((_s), (_c)))

struct mfi_ccb *
mfi_get_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;

	mtx_enter(&sc->sc_ccb_mtx);
	ccb = TAILQ_FIRST(&sc->sc_ccb_freeq);
	if (ccb != NULL) {
		TAILQ_REMOVE(&sc->sc_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = MFI_CCB_READY;
	}
	mtx_leave(&sc->sc_ccb_mtx);

	DNPRINTF(MFI_D_CCB, "%s: mfi_get_ccb: %p\n", DEVNAME(sc), ccb);

	return (ccb);
}

void
mfi_put_ccb(struct mfi_ccb *ccb)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;

	DNPRINTF(MFI_D_CCB, "%s: mfi_put_ccb: %p\n", DEVNAME(sc), ccb);

	hdr->mfh_cmd_status = 0x0;
	hdr->mfh_flags = 0x0;
	ccb->ccb_state = MFI_CCB_FREE;
	ccb->ccb_cookie = NULL;
	ccb->ccb_flags = 0;
	ccb->ccb_done = NULL;
	ccb->ccb_direction = 0;
	ccb->ccb_frame_size = 0;
	ccb->ccb_extra_frames = 0;
	ccb->ccb_sgl = NULL;
	ccb->ccb_data = NULL;
	ccb->ccb_len = 0;

	mtx_enter(&sc->sc_ccb_mtx);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_freeq, ccb, ccb_link);
	mtx_leave(&sc->sc_ccb_mtx);
}

int
mfi_init_ccb(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	uint32_t		i;
	int			error;

	DNPRINTF(MFI_D_CCB, "%s: mfi_init_ccb\n", DEVNAME(sc));

	sc->sc_ccb = malloc(sizeof(struct mfi_ccb) * sc->sc_max_cmds,
	    M_DEVBUF, M_WAITOK|M_ZERO);

	for (i = 0; i < sc->sc_max_cmds; i++) {
		ccb = &sc->sc_ccb[i];

		ccb->ccb_sc = sc;

		/* select i'th frame */
		ccb->ccb_frame = (union mfi_frame *)
		    (MFIMEM_KVA(sc->sc_frames) + sc->sc_frames_size * i);
		ccb->ccb_pframe =
		    MFIMEM_DVA(sc->sc_frames) + sc->sc_frames_size * i;
		ccb->ccb_pframe_offset = sc->sc_frames_size * i;
		ccb->ccb_frame->mfr_header.mfh_context = i;

		/* select i'th sense */
		ccb->ccb_sense = (struct mfi_sense *)
		    (MFIMEM_KVA(sc->sc_sense) + MFI_SENSE_SIZE * i);
		ccb->ccb_psense =
		    (MFIMEM_DVA(sc->sc_sense) + MFI_SENSE_SIZE * i);

		/* create a dma map for transfer */
		error = bus_dmamap_create(sc->sc_dmat,
		    MAXPHYS, sc->sc_max_sgl, MAXPHYS, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &ccb->ccb_dmamap);
		if (error) {
			printf("%s: cannot create ccb dmamap (%d)\n",
			    DEVNAME(sc), error);
			goto destroy;
		}

		DNPRINTF(MFI_D_CCB,
		    "ccb(%d): %p frame: %#x (%#x) sense: %#x (%#x) map: %#x\n",
		    ccb->ccb_frame->mfr_header.mfh_context, ccb,
		    ccb->ccb_frame, ccb->ccb_pframe,
		    ccb->ccb_sense, ccb->ccb_psense,
		    ccb->ccb_dmamap);

		/* add ccb to queue */
		mfi_put_ccb(ccb);
	}

	return (0);
destroy:
	/* free dma maps and ccb memory */
	while (i) {
		ccb = &sc->sc_ccb[i];
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
		i--;
	}

	free(sc->sc_ccb, M_DEVBUF);

	return (1);
}

uint32_t
mfi_read(struct mfi_softc *sc, bus_size_t r)
{
	uint32_t rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MFI_D_RW, "%s: mr 0x%x 0x08%x ", DEVNAME(sc), r, rv);
	return (rv);
}

void
mfi_write(struct mfi_softc *sc, bus_size_t r, uint32_t v)
{
	DNPRINTF(MFI_D_RW, "%s: mw 0x%x 0x%08x", DEVNAME(sc), r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

struct mfi_mem *
mfi_allocmem(struct mfi_softc *sc, size_t size)
{
	struct mfi_mem		*mm;
	int			nsegs;

	DNPRINTF(MFI_D_MEM, "%s: mfi_allocmem: %d\n", DEVNAME(sc),
	    size);

	mm = malloc(sizeof(struct mfi_mem), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (mm == NULL)
		return (NULL);

	mm->am_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mm->am_map) != 0)
		goto amfree; 

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mm->am_seg, 1,
	    &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mm->am_seg, nsegs, size, &mm->am_kva,
	    BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mm->am_map, mm->am_kva, size, NULL,
	    BUS_DMA_NOWAIT) != 0)
		goto unmap;

	DNPRINTF(MFI_D_MEM, "  kva: %p  dva: %p  map: %p\n",
	    mm->am_kva, mm->am_map->dm_segs[0].ds_addr, mm->am_map);

	memset(mm->am_kva, 0, size);
	return (mm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
amfree:
	free(mm, M_DEVBUF);

	return (NULL);
}

void
mfi_freemem(struct mfi_softc *sc, struct mfi_mem *mm)
{
	DNPRINTF(MFI_D_MEM, "%s: mfi_freemem: %p\n", DEVNAME(sc), mm);

	bus_dmamap_unload(sc->sc_dmat, mm->am_map);
	bus_dmamem_unmap(sc->sc_dmat, mm->am_kva, mm->am_size);
	bus_dmamem_free(sc->sc_dmat, &mm->am_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mm->am_map);
	free(mm, M_DEVBUF);
}

int
mfi_transition_firmware(struct mfi_softc *sc)
{
	int32_t			fw_state, cur_state;
	int			max_wait, i;

	fw_state = mfi_fw_state(sc) & MFI_STATE_MASK;

	DNPRINTF(MFI_D_CMD, "%s: mfi_transition_firmware: %#x\n", DEVNAME(sc),
	    fw_state);

	while (fw_state != MFI_STATE_READY) {
		DNPRINTF(MFI_D_MISC,
		    "%s: waiting for firmware to become ready\n",
		    DEVNAME(sc));
		cur_state = fw_state;
		switch (fw_state) {
		case MFI_STATE_FAULT:
			printf("%s: firmware fault\n", DEVNAME(sc));
			return (1);
		case MFI_STATE_WAIT_HANDSHAKE:
			mfi_write(sc, MFI_IDB, MFI_INIT_CLEAR_HANDSHAKE);
			max_wait = 2;
			break;
		case MFI_STATE_OPERATIONAL:
			mfi_write(sc, MFI_IDB, MFI_INIT_READY);
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
			fw_state = mfi_fw_state(sc) & MFI_STATE_MASK;
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
mfi_initialize_firmware(struct mfi_softc *sc)
{
	struct mfi_ccb		*ccb;
	struct mfi_init_frame	*init;
	struct mfi_init_qinfo	*qinfo;

	DNPRINTF(MFI_D_MISC, "%s: mfi_initialize_firmware\n", DEVNAME(sc));

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (1);

	init = &ccb->ccb_frame->mfr_init;
	qinfo = (struct mfi_init_qinfo *)((uint8_t *)init + MFI_FRAME_SIZE);

	memset(qinfo, 0, sizeof *qinfo);
	qinfo->miq_rq_entries = sc->sc_max_cmds + 1;
	qinfo->miq_rq_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_reply_q));
	qinfo->miq_pi_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_producer));
	qinfo->miq_ci_addr_lo = htole32(MFIMEM_DVA(sc->sc_pcq) +
	    offsetof(struct mfi_prod_cons, mpc_consumer));

	init->mif_header.mfh_cmd = MFI_CMD_INIT;
	init->mif_header.mfh_data_len = sizeof *qinfo;
	init->mif_qinfo_new_addr_lo = htole32(ccb->ccb_pframe + MFI_FRAME_SIZE);

	DNPRINTF(MFI_D_MISC, "%s: entries: %#x rq: %#x pi: %#x ci: %#x\n",
	    DEVNAME(sc),
	    qinfo->miq_rq_entries, qinfo->miq_rq_addr_lo,
	    qinfo->miq_pi_addr_lo, qinfo->miq_ci_addr_lo);

	if (mfi_poll(ccb)) {
		printf("%s: mfi_initialize_firmware failed\n", DEVNAME(sc));
		return (1);
	}

	mfi_put_ccb(ccb);

	return (0);
}

int
mfi_get_info(struct mfi_softc *sc)
{
#ifdef MFI_DEBUG
	int i;
#endif
	DNPRINTF(MFI_D_MISC, "%s: mfi_get_info\n", DEVNAME(sc));

	if (mfi_mgmt(sc, MR_DCMD_CTRL_GET_INFO, MFI_DATA_IN,
	    sizeof(sc->sc_info), &sc->sc_info, NULL))
		return (1);

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

void
mfiminphys(struct buf *bp, struct scsi_link *sl)
{
	DNPRINTF(MFI_D_MISC, "mfiminphys: %d\n", bp->b_bcount);

	/* XXX currently using MFI_MAXFER = MAXPHYS */
	if (bp->b_bcount > MFI_MAXFER)
		bp->b_bcount = MFI_MAXFER;
	minphys(bp);
}

int
mfi_attach(struct mfi_softc *sc, enum mfi_iop iop)
{
	struct scsibus_attach_args saa;
	uint32_t		status, frames;
	int			i;

	switch (iop) {
	case MFI_IOP_XSCALE:
		sc->sc_iop = &mfi_iop_xscale;
		break;
	case MFI_IOP_PPC:
		sc->sc_iop = &mfi_iop_ppc;
		break;
	case MFI_IOP_GEN2:
		sc->sc_iop = &mfi_iop_gen2;
		break;
	default:
		panic("%s: unknown iop %d", DEVNAME(sc), iop);
	}

	DNPRINTF(MFI_D_MISC, "%s: mfi_attach\n", DEVNAME(sc));

	if (mfi_transition_firmware(sc))
		return (1);

	TAILQ_INIT(&sc->sc_ccb_freeq);
	mtx_init(&sc->sc_ccb_mtx, IPL_BIO);

	rw_init(&sc->sc_lock, "mfi_lock");

	status = mfi_fw_state(sc);
	sc->sc_max_cmds = status & MFI_STATE_MAXCMD_MASK;
	sc->sc_max_sgl = (status & MFI_STATE_MAXSGL_MASK) >> 16;
	DNPRINTF(MFI_D_MISC, "%s: max commands: %u, max sgl: %u\n",
	    DEVNAME(sc), sc->sc_max_cmds, sc->sc_max_sgl);

	/* consumer/producer and reply queue memory */
	sc->sc_pcq = mfi_allocmem(sc, (sizeof(uint32_t) * sc->sc_max_cmds) +
	    sizeof(struct mfi_prod_cons));
	if (sc->sc_pcq == NULL) {
		printf("%s: unable to allocate reply queue memory\n",
		    DEVNAME(sc));
		goto nopcq;
	}

	/* frame memory */
	/* we are not doing 64 bit IO so only calculate # of 32 bit frames */
	frames = (sizeof(struct mfi_sg32) * sc->sc_max_sgl +
	    MFI_FRAME_SIZE - 1) / MFI_FRAME_SIZE + 1;
	sc->sc_frames_size = frames * MFI_FRAME_SIZE;
	sc->sc_frames = mfi_allocmem(sc, sc->sc_frames_size * sc->sc_max_cmds);
	if (sc->sc_frames == NULL) {
		printf("%s: unable to allocate frame memory\n", DEVNAME(sc));
		goto noframe;
	}
	/* XXX hack, fix this */
	if (MFIMEM_DVA(sc->sc_frames) & 0x3f) {
		printf("%s: improper frame alignment (%#x) FIXME\n",
		    DEVNAME(sc), MFIMEM_DVA(sc->sc_frames));
		goto noframe;
	}

	/* sense memory */
	sc->sc_sense = mfi_allocmem(sc, sc->sc_max_cmds * MFI_SENSE_SIZE);
	if (sc->sc_sense == NULL) {
		printf("%s: unable to allocate sense memory\n", DEVNAME(sc));
		goto nosense;
	}

	/* now that we have all memory bits go initialize ccbs */
	if (mfi_init_ccb(sc)) {
		printf("%s: could not init ccb list\n", DEVNAME(sc));
		goto noinit;
	}

	/* kickstart firmware with all addresses and pointers */
	if (mfi_initialize_firmware(sc)) {
		printf("%s: could not initialize firmware\n", DEVNAME(sc));
		goto noinit;
	}

	if (mfi_get_info(sc)) {
		printf("%s: could not retrieve controller information\n",
		    DEVNAME(sc));
		goto noinit;
	}

	printf("%s: logical drives %d, version %s, %dMB RAM\n",
	    DEVNAME(sc),
	    sc->sc_info.mci_lds_present,
	    sc->sc_info.mci_package_version,
	    sc->sc_info.mci_memory_size);

	sc->sc_ld_cnt = sc->sc_info.mci_lds_present;
	sc->sc_max_ld = sc->sc_ld_cnt;
	for (i = 0; i < sc->sc_ld_cnt; i++)
		sc->sc_ld[i].ld_present = 1;

	if (sc->sc_ld_cnt)
		sc->sc_link.openings = sc->sc_max_cmds / sc->sc_ld_cnt;
	else
		sc->sc_link.openings = sc->sc_max_cmds;

	sc->sc_link.device = &mfi_dev;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter = &mfi_switch;
	sc->sc_link.adapter_target = MFI_MAX_LD;
	sc->sc_link.adapter_buswidth = sc->sc_max_ld;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;

	config_found(&sc->sc_dev, &saa, scsiprint);

	/* enable interrupts */
	mfi_intr_enable(sc);

#if NBIO > 0
	if (bio_register(&sc->sc_dev, mfi_ioctl) != 0)
		panic("%s: controller registration failed", DEVNAME(sc));
	else
		sc->sc_ioctl = mfi_ioctl;

#ifndef SMALL_KERNEL
	if (mfi_create_sensors(sc) != 0)
		printf("%s: unable to create sensors\n", DEVNAME(sc));
#endif
#endif /* NBIO > 0 */

	return (0);
noinit:
	mfi_freemem(sc, sc->sc_sense);
nosense:
	mfi_freemem(sc, sc->sc_frames);
noframe:
	mfi_freemem(sc, sc->sc_pcq);
nopcq:
	return (1);
}

int
mfi_poll(struct mfi_ccb *ccb)
{
	struct mfi_softc *sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr;
	int			to = 0, rv = 0;

	DNPRINTF(MFI_D_CMD, "%s: mfi_poll\n", DEVNAME(sc));

	hdr = &ccb->ccb_frame->mfr_header;
	hdr->mfh_cmd_status = 0xff;
	hdr->mfh_flags |= MFI_FRAME_DONT_POST_IN_REPLY_QUEUE;

	mfi_start(sc, ccb);

	while (hdr->mfh_cmd_status == 0xff) {
		delay(1000);
		if (to++ > 5000) /* XXX 5 seconds busywait sucks */
			break;
	}
	if (hdr->mfh_cmd_status == 0xff) {
		printf("%s: timeout on ccb %d\n", DEVNAME(sc),
		    hdr->mfh_context);
		ccb->ccb_flags |= MFI_CCB_F_ERR;
		rv = 1;
	}

	if (ccb->ccb_direction != MFI_DATA_NONE) {
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction & MFI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	return (rv);
}

int
mfi_intr(void *arg)
{
	struct mfi_softc	*sc = arg;
	struct mfi_prod_cons	*pcq;
	struct mfi_ccb		*ccb;
	uint32_t		producer, consumer, ctx;
	int			claimed = 0;

	if (!mfi_my_intr(sc))
		return (0);

	pcq = MFIMEM_KVA(sc->sc_pcq);
	producer = pcq->mpc_producer;
	consumer = pcq->mpc_consumer;

	DNPRINTF(MFI_D_INTR, "%s: mfi_intr %#x %#x\n", DEVNAME(sc), sc, pcq);

	while (consumer != producer) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_intr pi %#x ci %#x\n",
		    DEVNAME(sc), producer, consumer);

		ctx = pcq->mpc_reply_q[consumer];
		pcq->mpc_reply_q[consumer] = MFI_INVALID_CTX;
		if (ctx == MFI_INVALID_CTX)
			printf("%s: invalid context, p: %d c: %d\n",
			    DEVNAME(sc), producer, consumer);
		else {
			/* XXX remove from queue and call scsi_done */
			ccb = &sc->sc_ccb[ctx];
			DNPRINTF(MFI_D_INTR, "%s: mfi_intr context %#x\n",
			    DEVNAME(sc), ctx);
			mfi_done(ccb);

			claimed = 1;
		}
		consumer++;
		if (consumer == (sc->sc_max_cmds + 1))
			consumer = 0;
	}

	pcq->mpc_consumer = consumer;

	return (claimed);
}

int
mfi_scsi_io(struct mfi_ccb *ccb, struct scsi_xfer *xs, uint64_t blockno,
    uint32_t blockcnt)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_io_frame	*io;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_io: %d\n",
	    DEVNAME((struct mfi_softc *)link->adapter_softc), link->target);

	if (!xs->data)
		return (1);

	io = &ccb->ccb_frame->mfr_io;
	if (xs->flags & SCSI_DATA_IN) {
		io->mif_header.mfh_cmd = MFI_CMD_LD_READ;
		ccb->ccb_direction = MFI_DATA_IN;
	} else {
		io->mif_header.mfh_cmd = MFI_CMD_LD_WRITE;
		ccb->ccb_direction = MFI_DATA_OUT;
	}
	io->mif_header.mfh_target_id = link->target;
	io->mif_header.mfh_timeout = 0;
	io->mif_header.mfh_flags = 0;
	io->mif_header.mfh_sense_len = MFI_SENSE_SIZE;
	io->mif_header.mfh_data_len= blockcnt;
	io->mif_lba_hi = (uint32_t)(blockno >> 32);
	io->mif_lba_lo = (uint32_t)(blockno & 0xffffffffull);
	io->mif_sense_addr_lo = htole32(ccb->ccb_psense);
	io->mif_sense_addr_hi = 0;

	ccb->ccb_done = mfi_scsi_xs_done;
	ccb->ccb_cookie = xs;
	ccb->ccb_frame_size = MFI_IO_FRAME_SIZE;
	ccb->ccb_sgl = &io->mif_sgl;
	ccb->ccb_data = xs->data;
	ccb->ccb_len = xs->datalen;

	if (mfi_create_sgl(ccb, (xs->flags & SCSI_NOSLEEP) ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK))
		return (1);

	return (0);
}

void
mfi_scsi_xs_done(struct mfi_ccb *ccb)
{
	struct scsi_xfer	*xs = ccb->ccb_cookie;
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;

	DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done %#x %#x\n",
	    DEVNAME(sc), ccb, ccb->ccb_frame);

	if (xs->data != NULL) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done sync\n",
		    DEVNAME(sc));
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	if (hdr->mfh_cmd_status != MFI_STAT_OK) {
		xs->error = XS_DRIVER_STUFFUP;
		DNPRINTF(MFI_D_INTR, "%s: mfi_scsi_xs_done stuffup %#x\n",
		    DEVNAME(sc), hdr->mfh_cmd_status);

		if (hdr->mfh_scsi_status != 0) {
			DNPRINTF(MFI_D_INTR,
			    "%s: mfi_scsi_xs_done sense %#x %x %x\n",
			    DEVNAME(sc), hdr->mfh_scsi_status,
			    &xs->sense, ccb->ccb_sense);
			memset(&xs->sense, 0, sizeof(xs->sense));
			memcpy(&xs->sense, ccb->ccb_sense,
			    sizeof(struct scsi_sense_data));
			xs->error = XS_SENSE;
		}
	}

	xs->resid = 0;

	mfi_put_ccb(ccb);
	scsi_done(xs);
}

int
mfi_scsi_ld(struct mfi_ccb *ccb, struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_pass_frame	*pf;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_ld: %d\n",
	    DEVNAME((struct mfi_softc *)link->adapter_softc), link->target);

	pf = &ccb->ccb_frame->mfr_pass;
	pf->mpf_header.mfh_cmd = MFI_CMD_LD_SCSI_IO;
	pf->mpf_header.mfh_target_id = link->target;
	pf->mpf_header.mfh_lun_id = 0;
	pf->mpf_header.mfh_cdb_len = xs->cmdlen;
	pf->mpf_header.mfh_timeout = 0;
	pf->mpf_header.mfh_data_len= xs->datalen; /* XXX */
	pf->mpf_header.mfh_sense_len = MFI_SENSE_SIZE;

	pf->mpf_sense_addr_hi = 0;
	pf->mpf_sense_addr_lo = htole32(ccb->ccb_psense);

	memset(pf->mpf_cdb, 0, 16);
	memcpy(pf->mpf_cdb, &xs->cmdstore, xs->cmdlen);

	ccb->ccb_done = mfi_scsi_xs_done;
	ccb->ccb_cookie = xs;
	ccb->ccb_frame_size = MFI_PASS_FRAME_SIZE;
	ccb->ccb_sgl = &pf->mpf_sgl;

	if (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT))
		ccb->ccb_direction = xs->flags & SCSI_DATA_IN ?
		    MFI_DATA_IN : MFI_DATA_OUT;
	else
		ccb->ccb_direction = MFI_DATA_NONE;

	if (xs->data) {
		ccb->ccb_data = xs->data;
		ccb->ccb_len = xs->datalen;

		if (mfi_create_sgl(ccb, (xs->flags & SCSI_NOSLEEP) ?
		    BUS_DMA_NOWAIT : BUS_DMA_WAITOK))
			return (1);
	}

	return (0);
}

void
mfi_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link	*link = xs->sc_link;
	struct mfi_softc	*sc = link->adapter_softc;
	struct device		*dev = link->device_softc;
	struct mfi_ccb		*ccb;
	struct scsi_rw		*rw;
	struct scsi_rw_big	*rwb;
	struct scsi_rw_16	*rw16;
	uint64_t		blockno;
	uint32_t		blockcnt;
	uint8_t			target = link->target;
	uint8_t			mbox[MFI_MBOX_SIZE];
	int			s;

	DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_cmd opcode: %#x\n",
	    DEVNAME(sc), xs->cmd->opcode);

	if (target >= MFI_MAX_LD || !sc->sc_ld[target].ld_present ||
	    link->lun != 0) {
		DNPRINTF(MFI_D_CMD, "%s: invalid target %d\n",
		    DEVNAME(sc), target);
		goto stuffup;
	}

	if ((ccb = mfi_get_ccb(sc)) == NULL) {
		DNPRINTF(MFI_D_CMD, "%s: mfi_scsi_cmd no ccb\n", DEVNAME(sc));
		xs->error = XS_NO_CCB;
		s = splbio();
		scsi_done(xs);
		splx(s);
		return;
	}

	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	/* IO path */
	case READ_BIG:
	case WRITE_BIG:
		rwb = (struct scsi_rw_big *)xs->cmd;
		blockno = (uint64_t)_4btol(rwb->addr);
		blockcnt = _2btol(rwb->length);
		if (mfi_scsi_io(ccb, xs, blockno, blockcnt)) {
			mfi_put_ccb(ccb);
			goto stuffup;
		}
		break;

	case READ_COMMAND:
	case WRITE_COMMAND:
		rw = (struct scsi_rw *)xs->cmd;
		blockno =
		    (uint64_t)(_3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff));
		blockcnt = rw->length ? rw->length : 0x100;
		if (mfi_scsi_io(ccb, xs, blockno, blockcnt)) {
			mfi_put_ccb(ccb);
			goto stuffup;
		}
		break;

	case READ_16:
	case WRITE_16:
		rw16 = (struct scsi_rw_16 *)xs->cmd;
		blockno = _8btol(rw16->addr);
		blockcnt = _4btol(rw16->length);
		if (mfi_scsi_io(ccb, xs, blockno, blockcnt)) {
			mfi_put_ccb(ccb);
			goto stuffup;
		}
		break;

	case SYNCHRONIZE_CACHE:
		mfi_put_ccb(ccb); /* we don't need this */

		mbox[0] = MR_FLUSH_CTRL_CACHE | MR_FLUSH_DISK_CACHE;
		if (mfi_mgmt(sc, MR_DCMD_CTRL_CACHE_FLUSH, MFI_DATA_NONE,
		    0, NULL, mbox))
			goto stuffup;

		goto complete;
		/* NOTREACHED */

	/* hand it of to the firmware and let it deal with it */
	case TEST_UNIT_READY:
		/* save off sd? after autoconf */
		if (!cold)	/* XXX bogus */
			strlcpy(sc->sc_ld[target].ld_dev, dev->dv_xname,
			    sizeof(sc->sc_ld[target].ld_dev));
		/* FALLTHROUGH */

	default:
		if (mfi_scsi_ld(ccb, xs)) {
			mfi_put_ccb(ccb);
			goto stuffup;
		}
		break;
	}

	DNPRINTF(MFI_D_CMD, "%s: start io %d\n", DEVNAME(sc), target);

	if (xs->flags & SCSI_POLL) {
		if (mfi_poll(ccb)) {
			/* XXX check for sense in ccb->ccb_sense? */
			printf("%s: mfi_scsi_cmd poll failed\n",
			    DEVNAME(sc));
			bzero(&xs->sense, sizeof(xs->sense));
			xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
			xs->sense.flags = SKEY_ILLEGAL_REQUEST;
			xs->sense.add_sense_code = 0x20; /* invalid opcode */
			xs->error = XS_SENSE;
		}

		mfi_put_ccb(ccb);
		s = splbio();
		scsi_done(xs);
		splx(s);
		return;
	}

	mfi_start(sc, ccb);

	DNPRINTF(MFI_D_DMA, "%s: mfi_scsi_cmd queued %d\n", DEVNAME(sc),
	    ccb->ccb_dmamap->dm_nsegs);

	return;

stuffup:
	xs->error = XS_DRIVER_STUFFUP;
complete:
	s = splbio();
	scsi_done(xs);
	splx(s);
}

int
mfi_create_sgl(struct mfi_ccb *ccb, int flags)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr;
	bus_dma_segment_t	*sgd;
	union mfi_sgl		*sgl;
	int			error, i;

	DNPRINTF(MFI_D_DMA, "%s: mfi_create_sgl %#x\n", DEVNAME(sc),
	    ccb->ccb_data);

	if (!ccb->ccb_data)
		return (1);

	error = bus_dmamap_load(sc->sc_dmat, ccb->ccb_dmamap,
	    ccb->ccb_data, ccb->ccb_len, NULL, flags);
	if (error) {
		if (error == EFBIG)
			printf("more than %d dma segs\n",
			    sc->sc_max_sgl);
		else
			printf("error %d loading dma map\n", error);
		return (1);
	}

	hdr = &ccb->ccb_frame->mfr_header;
	sgl = ccb->ccb_sgl;
	sgd = ccb->ccb_dmamap->dm_segs;
	for (i = 0; i < ccb->ccb_dmamap->dm_nsegs; i++) {
		sgl->sg32[i].addr = htole32(sgd[i].ds_addr);
		sgl->sg32[i].len = htole32(sgd[i].ds_len);
		DNPRINTF(MFI_D_DMA, "%s: addr: %#x  len: %#x\n",
		    DEVNAME(sc), sgl->sg32[i].addr, sgl->sg32[i].len);
	}

	if (ccb->ccb_direction == MFI_DATA_IN) {
		hdr->mfh_flags |= MFI_FRAME_DIR_READ;
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREREAD);
	} else {
		hdr->mfh_flags |= MFI_FRAME_DIR_WRITE;
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}

	hdr->mfh_sg_count = ccb->ccb_dmamap->dm_nsegs;
	/* for 64 bit io make the sizeof a variable to hold whatever sg size */
	ccb->ccb_frame_size += sizeof(struct mfi_sg32) *
	    ccb->ccb_dmamap->dm_nsegs;
	ccb->ccb_extra_frames = (ccb->ccb_frame_size - 1) / MFI_FRAME_SIZE;

	DNPRINTF(MFI_D_DMA, "%s: sg_count: %d  frame_size: %d  frames_size: %d"
	    "  dm_nsegs: %d  extra_frames: %d\n",
	    DEVNAME(sc),
	    hdr->mfh_sg_count,
	    ccb->ccb_frame_size,
	    sc->sc_frames_size,
	    ccb->ccb_dmamap->dm_nsegs,
	    ccb->ccb_extra_frames);

	return (0);
}

int
mfi_mgmt(struct mfi_softc *sc, uint32_t opc, uint32_t dir, uint32_t len,
    void *buf, uint8_t *mbox)
{
	struct mfi_ccb		*ccb;
	struct mfi_dcmd_frame	*dcmd;
	int			rv = 1;
	int			s;

	DNPRINTF(MFI_D_MISC, "%s: mfi_mgmt %#x\n", DEVNAME(sc), opc);

	if ((ccb = mfi_get_ccb(sc)) == NULL)
		return (rv);

	dcmd = &ccb->ccb_frame->mfr_dcmd;
	memset(dcmd->mdf_mbox, 0, MFI_MBOX_SIZE);
	dcmd->mdf_header.mfh_cmd = MFI_CMD_DCMD;
	dcmd->mdf_header.mfh_timeout = 0;

	dcmd->mdf_opcode = opc;
	dcmd->mdf_header.mfh_data_len = 0;
	ccb->ccb_direction = dir;
	ccb->ccb_done = mfi_mgmt_done;

	ccb->ccb_frame_size = MFI_DCMD_FRAME_SIZE;

	/* handle special opcodes */
	if (mbox)
		memcpy(dcmd->mdf_mbox, mbox, MFI_MBOX_SIZE);

	if (dir != MFI_DATA_NONE) {
		dcmd->mdf_header.mfh_data_len = len;
		ccb->ccb_data = buf;
		ccb->ccb_len = len;
		ccb->ccb_sgl = &dcmd->mdf_sgl;

		if (mfi_create_sgl(ccb, BUS_DMA_WAITOK))
			goto done;
	}

	if (cold) {
		if (mfi_poll(ccb))
			goto done;
	} else {
		s = splbio();
		mfi_start(sc, ccb);

		DNPRINTF(MFI_D_MISC, "%s: mfi_mgmt sleeping\n", DEVNAME(sc));
		while (ccb->ccb_state != MFI_CCB_DONE)
			tsleep(ccb, PRIBIO, "mfi_mgmt", 0);
		splx(s);

		if (ccb->ccb_flags & MFI_CCB_F_ERR)
			goto done;
	}

	rv = 0;

done:
	mfi_put_ccb(ccb);
	return (rv);
}

void
mfi_mgmt_done(struct mfi_ccb *ccb)
{
	struct mfi_softc	*sc = ccb->ccb_sc;
	struct mfi_frame_header	*hdr = &ccb->ccb_frame->mfr_header;

	DNPRINTF(MFI_D_INTR, "%s: mfi_mgmt_done %#x %#x\n",
	    DEVNAME(sc), ccb, ccb->ccb_frame);

	if (ccb->ccb_data != NULL) {
		DNPRINTF(MFI_D_INTR, "%s: mfi_mgmt_done sync\n",
		    DEVNAME(sc));
		bus_dmamap_sync(sc->sc_dmat, ccb->ccb_dmamap, 0,
		    ccb->ccb_dmamap->dm_mapsize,
		    (ccb->ccb_direction & MFI_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, ccb->ccb_dmamap);
	}

	if (hdr->mfh_cmd_status != MFI_STAT_OK)
		ccb->ccb_flags |= MFI_CCB_F_ERR;

	ccb->ccb_state = MFI_CCB_DONE;

	wakeup(ccb);
}

int
mfi_scsi_ioctl(struct scsi_link *link, u_long cmd, caddr_t addr, int flag,
    struct proc *p)
{
	struct mfi_softc	*sc = (struct mfi_softc *)link->adapter_softc;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_scsi_ioctl\n", DEVNAME(sc));

	if (sc->sc_ioctl)
		return (sc->sc_ioctl(link->adapter_softc, cmd, addr));
	else
		return (ENOTTY);
}

#if NBIO > 0
int
mfi_ioctl(struct device *dev, u_long cmd, caddr_t addr)
{
	struct mfi_softc	*sc = (struct mfi_softc *)dev;
	int error = 0;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl ", DEVNAME(sc));

	rw_enter_write(&sc->sc_lock);

	switch (cmd) {
	case BIOCINQ:
		DNPRINTF(MFI_D_IOCTL, "inq\n");
		error = mfi_ioctl_inq(sc, (struct bioc_inq *)addr);
		break;

	case BIOCVOL:
		DNPRINTF(MFI_D_IOCTL, "vol\n");
		error = mfi_ioctl_vol(sc, (struct bioc_vol *)addr);
		break;

	case BIOCDISK:
		DNPRINTF(MFI_D_IOCTL, "disk\n");
		error = mfi_ioctl_disk(sc, (struct bioc_disk *)addr);
		break;

	case BIOCALARM:
		DNPRINTF(MFI_D_IOCTL, "alarm\n");
		error = mfi_ioctl_alarm(sc, (struct bioc_alarm *)addr);
		break;

	case BIOCBLINK:
		DNPRINTF(MFI_D_IOCTL, "blink\n");
		error = mfi_ioctl_blink(sc, (struct bioc_blink *)addr);
		break;

	case BIOCSETSTATE:
		DNPRINTF(MFI_D_IOCTL, "setstate\n");
		error = mfi_ioctl_setstate(sc, (struct bioc_setstate *)addr);
		break;

	default:
		DNPRINTF(MFI_D_IOCTL, " invalid ioctl\n");
		error = EINVAL;
	}

	rw_exit_write(&sc->sc_lock);

	return (error);
}

int
mfi_bio_getitall(struct mfi_softc *sc)
{
	int			i, d, size, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];
	struct mfi_conf		*cfg = NULL;
	struct mfi_ld_details	*ld_det = NULL;

	/* get info */
	if (mfi_get_info(sc)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_get_info failed\n",
		    DEVNAME(sc));
		goto done;
	}

	/* send single element command to retrieve size for full structure */
	cfg = malloc(sizeof *cfg, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cfg == NULL)
		goto done;
	if (mfi_mgmt(sc, MD_DCMD_CONF_GET, MFI_DATA_IN, sizeof *cfg, cfg, NULL))
		goto done;

	size = cfg->mfc_size;
	free(cfg, M_DEVBUF);

	/* memory for read config */
	cfg = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cfg == NULL)
		goto done;
	if (mfi_mgmt(sc, MD_DCMD_CONF_GET, MFI_DATA_IN, size, cfg, NULL))
		goto done;

	/* replace current pointer with enw one */
	if (sc->sc_cfg)
		free(sc->sc_cfg, M_DEVBUF);
	sc->sc_cfg = cfg;

	/* get all ld info */
	if (mfi_mgmt(sc, MR_DCMD_LD_GET_LIST, MFI_DATA_IN,
	    sizeof(sc->sc_ld_list), &sc->sc_ld_list, NULL))
		goto done;

	/* get memory for all ld structures */
	size = cfg->mfc_no_ld * sizeof(struct mfi_ld_details);
	if (sc->sc_ld_sz != size) {
		if (sc->sc_ld_details)
			free(sc->sc_ld_details, M_DEVBUF);

		ld_det = malloc( size, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ld_det == NULL)
			goto done;
		sc->sc_ld_sz = size;
		sc->sc_ld_details = ld_det;
	}

	/* find used physical disks */
	size = sizeof(struct mfi_ld_details);
	for (i = 0, d = 0; i < cfg->mfc_no_ld; i++) {
		mbox[0] = sc->sc_ld_list.mll_list[i].mll_ld.mld_target;
		if (mfi_mgmt(sc, MR_DCMD_LD_GET_INFO, MFI_DATA_IN, size,
		    &sc->sc_ld_details[i], mbox))
			goto done;

		d += sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_no_drv_per_span *
		    sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_span_depth;
	}
	sc->sc_no_pd = d;

	rv = 0;
done:
	return (rv);
}

int
mfi_ioctl_inq(struct mfi_softc *sc, struct bioc_inq *bi)
{
	int			rv = EINVAL;
	struct mfi_conf		*cfg = NULL;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_inq\n", DEVNAME(sc));

	if (mfi_bio_getitall(sc)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_bio_getitall failed\n",
		    DEVNAME(sc));
		goto done;
	}

	/* count unused disks as volumes */
	if (sc->sc_cfg == NULL)
		goto done;
	cfg = sc->sc_cfg;

	bi->bi_nodisk = sc->sc_info.mci_pd_disks_present;
	bi->bi_novol = cfg->mfc_no_ld + cfg->mfc_no_hs;
#if notyet
	bi->bi_novol = cfg->mfc_no_ld + cfg->mfc_no_hs +
	    (bi->bi_nodisk - sc->sc_no_pd);
#endif
	/* tell bio who we are */
	strlcpy(bi->bi_dev, DEVNAME(sc), sizeof(bi->bi_dev));

	rv = 0;
done:
	return (rv);
}

int
mfi_ioctl_vol(struct mfi_softc *sc, struct bioc_vol *bv)
{
	int			i, per, rv = EINVAL;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_vol %#x\n",
	    DEVNAME(sc), bv->bv_volid);

	/* we really could skip and expect that inq took care of it */
	if (mfi_bio_getitall(sc)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_bio_getitall failed\n",
		    DEVNAME(sc));
		goto done;
	}

	if (bv->bv_volid >= sc->sc_ld_list.mll_no_ld) {
		/* go do hotspares & unused disks */
		rv = mfi_bio_hs(sc, bv->bv_volid, MFI_MGMT_VD, bv);
		goto done;
	}

	i = bv->bv_volid;
	strlcpy(bv->bv_dev, sc->sc_ld[i].ld_dev, sizeof(bv->bv_dev));

	switch(sc->sc_ld_list.mll_list[i].mll_state) {
	case MFI_LD_OFFLINE:
		bv->bv_status = BIOC_SVOFFLINE;
		break;

	case MFI_LD_PART_DEGRADED:
	case MFI_LD_DEGRADED:
		bv->bv_status = BIOC_SVDEGRADED;
		break;

	case MFI_LD_ONLINE:
		bv->bv_status = BIOC_SVONLINE;
		break;

	default:
		bv->bv_status = BIOC_SVINVALID;
		DNPRINTF(MFI_D_IOCTL, "%s: invalid logical disk state %#x\n",
		    DEVNAME(sc),
		    sc->sc_ld_list.mll_list[i].mll_state);
	}

	/* additional status can modify MFI status */
	switch (sc->sc_ld_details[i].mld_progress.mlp_in_prog) {
	case MFI_LD_PROG_CC:
	case MFI_LD_PROG_BGI:
		bv->bv_status = BIOC_SVSCRUB;
		per = (int)sc->sc_ld_details[i].mld_progress.mlp_cc.mp_progress;
		bv->bv_percent = (per * 100) / 0xffff;
		bv->bv_seconds =
		    sc->sc_ld_details[i].mld_progress.mlp_cc.mp_elapsed_seconds;
		break;

	case MFI_LD_PROG_FGI:
	case MFI_LD_PROG_RECONSTRUCT:
		/* nothing yet */
		break;
	}

	/*
	 * The RAID levels are determined per the SNIA DDF spec, this is only
	 * a subset that is valid for the MFI controller.
	 */
	bv->bv_level = sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_pri_raid;
	if (sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_sec_raid ==
	    MFI_DDF_SRL_SPANNED)
		bv->bv_level *= 10;

	bv->bv_nodisk = sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_no_drv_per_span *
	    sc->sc_ld_details[i].mld_cfg.mlc_parm.mpa_span_depth;

	bv->bv_size = sc->sc_ld_details[i].mld_size * 512; /* bytes per block */

	rv = 0;
done:
	return (rv);
}

int
mfi_ioctl_disk(struct mfi_softc *sc, struct bioc_disk *bd)
{
	struct mfi_conf		*cfg;
	struct mfi_array	*ar;
	struct mfi_ld_cfg	*ld;
	struct mfi_pd_details	*pd;
	struct scsi_inquiry_data *inqbuf;
	char			vend[8+16+4+1];
	int			rv = EINVAL;
	int			arr, vol, disk, span;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_disk %#x\n",
	    DEVNAME(sc), bd->bd_diskid);

	/* we really could skip and expect that inq took care of it */
	if (mfi_bio_getitall(sc)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_bio_getitall failed\n",
		    DEVNAME(sc));
		return (rv);
	}
	cfg = sc->sc_cfg;

	pd = malloc(sizeof *pd, M_DEVBUF, M_WAITOK);

	ar = cfg->mfc_array;
	vol = bd->bd_volid;
	if (vol >= cfg->mfc_no_ld) {
		/* do hotspares */
		rv = mfi_bio_hs(sc, bd->bd_volid, MFI_MGMT_SD, bd);
		goto freeme;
	}

	/* calculate offset to ld structure */
	ld = (struct mfi_ld_cfg *)(
	    ((uint8_t *)cfg) + offsetof(struct mfi_conf, mfc_array) +
	    cfg->mfc_array_size * cfg->mfc_no_array);

	/* use span 0 only when raid group is not spanned */
	if (ld[vol].mlc_parm.mpa_span_depth > 1)
		span = bd->bd_diskid / ld[vol].mlc_parm.mpa_no_drv_per_span;
	else
		span = 0;
	arr = ld[vol].mlc_span[span].mls_index;

	/* offset disk into pd list */
	disk = bd->bd_diskid % ld[vol].mlc_parm.mpa_no_drv_per_span;
	bd->bd_target = ar[arr].pd[disk].mar_enc_slot;

	/* get status */
	switch (ar[arr].pd[disk].mar_pd_state){
	case MFI_PD_UNCONFIG_GOOD:
	case MFI_PD_FAILED:
		bd->bd_status = BIOC_SDFAILED;
		break;

	case MFI_PD_HOTSPARE: /* XXX dedicated hotspare part of array? */
		bd->bd_status = BIOC_SDHOTSPARE;
		break;

	case MFI_PD_OFFLINE:
		bd->bd_status = BIOC_SDOFFLINE;
		break;

	case MFI_PD_REBUILD:
		bd->bd_status = BIOC_SDREBUILD;
		break;

	case MFI_PD_ONLINE:
		bd->bd_status = BIOC_SDONLINE;
		break;

	case MFI_PD_UNCONFIG_BAD: /* XXX define new state in bio */
	default:
		bd->bd_status = BIOC_SDINVALID;
		break;
	}

	/* get the remaining fields */
	*((uint16_t *)&mbox) = ar[arr].pd[disk].mar_pd.mfp_id;
	if (mfi_mgmt(sc, MR_DCMD_PD_GET_INFO, MFI_DATA_IN,
	    sizeof *pd, pd, mbox)) {
		/* disk is missing but succeed command */
		rv = 0;
		goto freeme;
	}

	bd->bd_size = pd->mpd_size * 512; /* bytes per block */

	/* if pd->mpd_enc_idx is 0 then it is not in an enclosure */
	bd->bd_channel = pd->mpd_enc_idx;

	inqbuf = (struct scsi_inquiry_data *)&pd->mpd_inq_data;
	memcpy(vend, inqbuf->vendor, sizeof vend - 1);
	vend[sizeof vend - 1] = '\0';
	strlcpy(bd->bd_vendor, vend, sizeof(bd->bd_vendor));

	/* XXX find a way to retrieve serial nr from drive */
	/* XXX find a way to get bd_procdev */

	rv = 0;
freeme:
	free(pd, M_DEVBUF);

	return (rv);
}

int
mfi_ioctl_alarm(struct mfi_softc *sc, struct bioc_alarm *ba)
{
	uint32_t		opc, dir = MFI_DATA_NONE;
	int			rv = 0;
	int8_t			ret;

	switch(ba->ba_opcode) {
	case BIOC_SADISABLE:
		opc = MR_DCMD_SPEAKER_DISABLE;
		break;

	case BIOC_SAENABLE:
		opc = MR_DCMD_SPEAKER_ENABLE;
		break;

	case BIOC_SASILENCE:
		opc = MR_DCMD_SPEAKER_SILENCE;
		break;

	case BIOC_GASTATUS:
		opc = MR_DCMD_SPEAKER_GET;
		dir = MFI_DATA_IN;
		break;

	case BIOC_SATEST:
		opc = MR_DCMD_SPEAKER_TEST;
		break;

	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_alarm biocalarm invalid "
		    "opcode %x\n", DEVNAME(sc), ba->ba_opcode);
		return (EINVAL);
	}

	if (mfi_mgmt(sc, opc, dir, sizeof(ret), &ret, NULL))
		rv = EINVAL;
	else
		if (ba->ba_opcode == BIOC_GASTATUS)
			ba->ba_status = ret;
		else
			ba->ba_status = 0;

	return (rv);
}

int
mfi_ioctl_blink(struct mfi_softc *sc, struct bioc_blink *bb)
{
	int			i, found, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];
	uint32_t		cmd;
	struct mfi_pd_list	*pd;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_blink %x\n", DEVNAME(sc),
	    bb->bb_status);

	/* channel 0 means not in an enclosure so can't be blinked */
	if (bb->bb_channel == 0)
		return (EINVAL);

	pd = malloc(MFI_PD_LIST_SIZE, M_DEVBUF, M_WAITOK);

	if (mfi_mgmt(sc, MR_DCMD_PD_GET_LIST, MFI_DATA_IN,
	    MFI_PD_LIST_SIZE, pd, NULL))
		goto done;

	for (i = 0, found = 0; i < pd->mpl_no_pd; i++)
		if (bb->bb_channel == pd->mpl_address[i].mpa_enc_index &&
		    bb->bb_target == pd->mpl_address[i].mpa_enc_slot) {
		    	found = 1;
			break;
		}

	if (!found)
		goto done;

	memset(mbox, 0, sizeof mbox);

	*((uint16_t *)&mbox) = pd->mpl_address[i].mpa_pd_id;

	switch (bb->bb_status) {
	case BIOC_SBUNBLINK:
		cmd = MR_DCMD_PD_UNBLINK;
		break;

	case BIOC_SBBLINK:
		cmd = MR_DCMD_PD_BLINK;
		break;

	case BIOC_SBALARM:
	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_blink biocblink invalid "
		    "opcode %x\n", DEVNAME(sc), bb->bb_status);
		goto done;
	}


	if (mfi_mgmt(sc, cmd, MFI_DATA_NONE, 0, NULL, mbox))
		goto done;

	rv = 0;
done:
	free(pd, M_DEVBUF);
	return (rv);
}

int
mfi_ioctl_setstate(struct mfi_softc *sc, struct bioc_setstate *bs)
{
	struct mfi_pd_list	*pd;
	int			i, found, rv = EINVAL;
	uint8_t			mbox[MFI_MBOX_SIZE];
	uint32_t		cmd;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_setstate %x\n", DEVNAME(sc),
	    bs->bs_status);

	pd = malloc(MFI_PD_LIST_SIZE, M_DEVBUF, M_WAITOK);

	if (mfi_mgmt(sc, MR_DCMD_PD_GET_LIST, MFI_DATA_IN,
	    MFI_PD_LIST_SIZE, pd, NULL))
		goto done;

	for (i = 0, found = 0; i < pd->mpl_no_pd; i++)
		if (bs->bs_channel == pd->mpl_address[i].mpa_enc_index &&
		    bs->bs_target == pd->mpl_address[i].mpa_enc_slot) {
		    	found = 1;
			break;
		}

	if (!found)
		goto done;

	memset(mbox, 0, sizeof mbox);

	*((uint16_t *)&mbox) = pd->mpl_address[i].mpa_pd_id;

	switch (bs->bs_status) {
	case BIOC_SSONLINE:
		mbox[2] = MFI_PD_ONLINE;
		cmd = MD_DCMD_PD_SET_STATE;
		break;

	case BIOC_SSOFFLINE:
		mbox[2] = MFI_PD_OFFLINE;
		cmd = MD_DCMD_PD_SET_STATE;
		break;

	case BIOC_SSHOTSPARE:
		mbox[2] = MFI_PD_HOTSPARE;
		cmd = MD_DCMD_PD_SET_STATE;
		break;
/*
	case BIOC_SSREBUILD:
		cmd = MD_DCMD_PD_REBUILD;
		break;
*/
	default:
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_ioctl_setstate invalid "
		    "opcode %x\n", DEVNAME(sc), bs->bs_status);
		goto done;
	}


	if (mfi_mgmt(sc, MD_DCMD_PD_SET_STATE, MFI_DATA_NONE, 0, NULL, mbox))
		goto done;

	rv = 0;
done:
	free(pd, M_DEVBUF);
	return (rv);
}

int
mfi_bio_hs(struct mfi_softc *sc, int volid, int type, void *bio_hs)
{
	struct mfi_conf		*cfg;
	struct mfi_hotspare	*hs;
	struct mfi_pd_details	*pd;
	struct bioc_disk	*sdhs;
	struct bioc_vol		*vdhs;
	struct scsi_inquiry_data *inqbuf;
	char			vend[8+16+4+1];
	int			i, rv = EINVAL;
	uint32_t		size;
	uint8_t			mbox[MFI_MBOX_SIZE];

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs %d\n", DEVNAME(sc), volid);

	if (!bio_hs)
		return (EINVAL);

	pd = malloc(sizeof *pd, M_DEVBUF, M_WAITOK);

	/* send single element command to retrieve size for full structure */
	cfg = malloc(sizeof *cfg, M_DEVBUF, M_WAITOK);
	if (mfi_mgmt(sc, MD_DCMD_CONF_GET, MFI_DATA_IN, sizeof *cfg, cfg, NULL))
		goto freeme;

	size = cfg->mfc_size;
	free(cfg, M_DEVBUF);

	/* memory for read config */
	cfg = malloc(size, M_DEVBUF, M_WAITOK|M_ZERO);
	if (mfi_mgmt(sc, MD_DCMD_CONF_GET, MFI_DATA_IN, size, cfg, NULL))
		goto freeme;

	/* calculate offset to hs structure */
	hs = (struct mfi_hotspare *)(
	    ((uint8_t *)cfg) + offsetof(struct mfi_conf, mfc_array) +
	    cfg->mfc_array_size * cfg->mfc_no_array +
	    cfg->mfc_ld_size * cfg->mfc_no_ld);

	if (volid < cfg->mfc_no_ld)
		goto freeme; /* not a hotspare */

	if (volid > (cfg->mfc_no_ld + cfg->mfc_no_hs))
		goto freeme; /* not a hotspare */

	/* offset into hotspare structure */
	i = volid - cfg->mfc_no_ld;

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs i %d volid %d no_ld %d no_hs %d "
	    "hs %p cfg %p id %02x\n", DEVNAME(sc), i, volid, cfg->mfc_no_ld,
	    cfg->mfc_no_hs, hs, cfg, hs[i].mhs_pd.mfp_id);

	/* get pd fields */
	memset(mbox, 0, sizeof mbox);
	*((uint16_t *)&mbox) = hs[i].mhs_pd.mfp_id;
	if (mfi_mgmt(sc, MR_DCMD_PD_GET_INFO, MFI_DATA_IN,
	    sizeof *pd, pd, mbox)) {
		DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs illegal PD\n",
		    DEVNAME(sc));
		goto freeme;
	}

	switch (type) {
	case MFI_MGMT_VD:
		vdhs = bio_hs;
		vdhs->bv_status = BIOC_SVONLINE;
		vdhs->bv_size = pd->mpd_size / 2 * 1024; /* XXX why? */
		vdhs->bv_level = -1; /* hotspare */
		vdhs->bv_nodisk = 1;
		break;

	case MFI_MGMT_SD:
		sdhs = bio_hs;
		sdhs->bd_status = BIOC_SDHOTSPARE;
		sdhs->bd_size = pd->mpd_size / 2 * 1024; /* XXX why? */
		sdhs->bd_channel = pd->mpd_enc_idx;
		sdhs->bd_target = pd->mpd_enc_slot;
		inqbuf = (struct scsi_inquiry_data *)&pd->mpd_inq_data;
		memcpy(vend, inqbuf->vendor, sizeof vend - 1);
		vend[sizeof vend - 1] = '\0';
		strlcpy(sdhs->bd_vendor, vend, sizeof(sdhs->bd_vendor));
		break;

	default:
		goto freeme;
	}

	DNPRINTF(MFI_D_IOCTL, "%s: mfi_vol_hs 6\n", DEVNAME(sc));
	rv = 0;
freeme:
	free(pd, M_DEVBUF);
	free(cfg, M_DEVBUF);

	return (rv);
}

#ifndef SMALL_KERNEL
int
mfi_create_sensors(struct mfi_softc *sc)
{
	struct device		*dev;
	struct scsibus_softc	*ssc = NULL;
	int			i;

	TAILQ_FOREACH(dev, &alldevs, dv_list) {
		if (dev->dv_parent != &sc->sc_dev)
			continue;

		/* check if this is the scsibus for the logical disks */
		ssc = (struct scsibus_softc *)dev;
		if (ssc->adapter_link == &sc->sc_link)
			break;
	}

	if (ssc == NULL)
		return (1);

	sc->sc_sensors = malloc(sizeof(struct ksensor) * sc->sc_ld_cnt,
	    M_DEVBUF, M_WAITOK | M_CANFAIL | M_ZERO);
	if (sc->sc_sensors == NULL)
		return (1);

	strlcpy(sc->sc_sensordev.xname, DEVNAME(sc),
	    sizeof(sc->sc_sensordev.xname));

	for (i = 0; i < sc->sc_ld_cnt; i++) {
		if (ssc->sc_link[i][0] == NULL)
			goto bad;

		dev = ssc->sc_link[i][0]->device_softc;

		sc->sc_sensors[i].type = SENSOR_DRIVE;
		sc->sc_sensors[i].status = SENSOR_S_UNKNOWN;

		strlcpy(sc->sc_sensors[i].desc, dev->dv_xname,
		    sizeof(sc->sc_sensors[i].desc));

		sensor_attach(&sc->sc_sensordev, &sc->sc_sensors[i]);
	}

	if (sensor_task_register(sc, mfi_refresh_sensors, 10) == NULL)
		goto bad;

	sensordev_install(&sc->sc_sensordev);

	return (0);

bad:
	free(sc->sc_sensors, M_DEVBUF);

	return (1);
}

void
mfi_refresh_sensors(void *arg)
{
	struct mfi_softc	*sc = arg;
	int			i;
	struct bioc_vol		bv;


	for (i = 0; i < sc->sc_ld_cnt; i++) {
		bzero(&bv, sizeof(bv));
		bv.bv_volid = i;
		if (mfi_ioctl_vol(sc, &bv))
			return;

		switch(bv.bv_status) {
		case BIOC_SVOFFLINE:
			sc->sc_sensors[i].value = SENSOR_DRIVE_FAIL;
			sc->sc_sensors[i].status = SENSOR_S_CRIT;
			break;

		case BIOC_SVDEGRADED:
			sc->sc_sensors[i].value = SENSOR_DRIVE_PFAIL;
			sc->sc_sensors[i].status = SENSOR_S_WARN;
			break;

		case BIOC_SVSCRUB:
		case BIOC_SVONLINE:
			sc->sc_sensors[i].value = SENSOR_DRIVE_ONLINE;
			sc->sc_sensors[i].status = SENSOR_S_OK;
			break;

		case BIOC_SVINVALID:
			/* FALLTRHOUGH */
		default:
			sc->sc_sensors[i].value = 0; /* unknown */
			sc->sc_sensors[i].status = SENSOR_S_UNKNOWN;
		}

	}
}
#endif /* SMALL_KERNEL */
#endif /* NBIO > 0 */

void
mfi_start(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
	    ccb->ccb_pframe_offset, sc->sc_frames_size,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	mfi_post(sc, ccb);
}

void
mfi_done(struct mfi_ccb *ccb)
{
	struct mfi_softc	*sc = ccb->ccb_sc;

	bus_dmamap_sync(sc->sc_dmat, MFIMEM_MAP(sc->sc_frames),
	    ccb->ccb_pframe_offset, sc->sc_frames_size, BUS_DMASYNC_PREREAD);

	ccb->ccb_done(ccb);
}

u_int32_t
mfi_xscale_fw_state(struct mfi_softc *sc)
{
	return (mfi_read(sc, MFI_OMSG0));
}

void
mfi_xscale_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_OMSK, MFI_ENABLE_INTR);
}

int
mfi_xscale_intr(struct mfi_softc *sc)
{
	u_int32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_INTR_VALID))
		return (0);

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_OSTS, status);

	return (1);
}

void
mfi_xscale_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQP, (ccb->ccb_pframe >> 3) |
	    ccb->ccb_extra_frames);
}

u_int32_t
mfi_ppc_fw_state(struct mfi_softc *sc)
{
	return (mfi_read(sc, MFI_OSP));
}

void
mfi_ppc_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_ODC, 0xffffffff);
	mfi_write(sc, MFI_OMSK, ~0x80000004);
}

int
mfi_ppc_intr(struct mfi_softc *sc)
{
	u_int32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_PPC_INTR_VALID))
		return (0);

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_ODC, status);

	return (1);
}

void
mfi_ppc_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQP, 0x1 | ccb->ccb_pframe |
	    (ccb->ccb_extra_frames << 1));
}

u_int32_t
mfi_gen2_fw_state(struct mfi_softc *sc)
{
	return (mfi_read(sc, MFI_OSP));
}

void
mfi_gen2_intr_ena(struct mfi_softc *sc)
{
	mfi_write(sc, MFI_ODC, 0xffffffff);
	mfi_write(sc, MFI_OMSK, ~MFI_OSTS_GEN2_INTR_VALID);
}

int
mfi_gen2_intr(struct mfi_softc *sc)
{
	u_int32_t status;

	status = mfi_read(sc, MFI_OSTS);
	if (!ISSET(status, MFI_OSTS_GEN2_INTR_VALID))
		return (0);

	/* write status back to acknowledge interrupt */
	mfi_write(sc, MFI_ODC, status);

	return (1);
}

void
mfi_gen2_post(struct mfi_softc *sc, struct mfi_ccb *ccb)
{
	mfi_write(sc, MFI_IQP, 0x1 | ccb->ccb_pframe |
	    (ccb->ccb_extra_frames << 1));
}
