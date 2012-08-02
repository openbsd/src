/*	$OpenBSD: oce.c,v 1.1 2012/08/02 17:35:52 mikeb Exp $	*/

/*-
 * Copyright (C) 2012 Emulex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Emulex Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information:
 * freebsd-drivers@emulex.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/ocereg.h>
#include <dev/pci/ocevar.h>

int oce_post(struct oce_softc *sc);
int oce_fw_clean(struct oce_softc *sc);
int oce_reset_fun(struct oce_softc *sc);
int oce_get_fw_version(struct oce_softc *sc);

int oce_get_fw_config(struct oce_softc *sc);
int oce_if_create(struct oce_softc *sc, uint32_t cap_flags, uint32_t en_flags,
    uint16_t vlan_tag, uint8_t *mac_addr, uint32_t *if_id);
int oce_if_del(struct oce_softc *sc, uint32_t if_id);
int oce_config_vlan(struct oce_softc *sc, uint32_t if_id,
    struct normal_vlan *vtag_arr, uint8_t vtag_cnt, uint32_t untagged,
    uint32_t enable_promisc);
int oce_set_flow_control(struct oce_softc *sc, uint32_t flow_control);
int oce_rss_itbl_init(struct oce_softc *sc, struct mbx_config_nic_rss *fwcmd);
int oce_update_multicast(struct oce_softc *sc, struct oce_dma_mem *pdma_mem);

int oce_set_common_iface_rx_filter(struct oce_softc *sc,
    struct oce_dma_mem *sgl);

int oce_mbox_check_native_mode(struct oce_softc *sc);

int oce_mbox_get_nic_stats_v0(struct oce_softc *sc,
    struct oce_dma_mem *pstats_dma_mem);
int oce_mbox_get_nic_stats(struct oce_softc *sc,
    struct oce_dma_mem *pstats_dma_mem);
int oce_mbox_get_pport_stats(struct oce_softc *sc,
    struct oce_dma_mem *pstats_dma_mem, uint32_t reset_stats);
void copy_stats_to_sc_xe201(struct oce_softc *sc);
void copy_stats_to_sc_be3(struct oce_softc *sc);
void copy_stats_to_sc_be2(struct oce_softc *sc);

/**
 * @brief		Function to post status
 * @param sc		software handle to the device
 */
int
oce_post(struct oce_softc *sc)
{
	mpu_ep_semaphore_t post_status;
	int tmo = 60000;

	/* read semaphore CSR */
	post_status.dw0 = OCE_READ_REG32(sc, csr, MPU_EP_SEMAPHORE(sc));

	/* if host is ready then wait for fw ready else send POST */
	if (post_status.bits.stage <= POST_STAGE_AWAITING_HOST_RDY) {
		post_status.bits.stage = POST_STAGE_CHIP_RESET;
		OCE_WRITE_REG32(sc, csr, MPU_EP_SEMAPHORE(sc), post_status.dw0);
	}

	/* wait for FW ready */
	for (;;) {
		if (--tmo == 0)
			break;

		DELAY(1000);

		post_status.dw0 = OCE_READ_REG32(sc, csr, MPU_EP_SEMAPHORE(sc));
		if (post_status.bits.error) {
			printf("%s: POST failed: %x\n", sc->dev.dv_xname,
			    post_status.dw0);
			return ENXIO;
		}
		if (post_status.bits.stage == POST_STAGE_ARMFW_READY)
			return 0;
	}

	printf("%s: POST timed out: %x\n", sc->dev.dv_xname, post_status.dw0);

	return ENXIO;
}

/**
 * @brief		Function for hardware initialization
 * @param sc		software handle to the device
 */
int
oce_hw_init(struct oce_softc *sc)
{
	int rc = 0;

	rc = oce_post(sc);
	if (rc)
		return rc;

	/* create the bootstrap mailbox */
	rc = oce_dma_alloc(sc, sizeof(struct oce_bmbx), &sc->bsmbx, 0);
	if (rc) {
		printf("%s: Mailbox alloc failed\n", sc->dev.dv_xname);
		return rc;
	}

	rc = oce_reset_fun(sc);
	if (rc)
		goto error;

	rc = oce_mbox_init(sc);
	if (rc)
		goto error;

	rc = oce_get_fw_version(sc);
	if (rc)
		goto error;

	rc = oce_get_fw_config(sc);
	if (rc)
		goto error;

	sc->macaddr.size_of_struct = 6;
	rc = oce_read_mac_addr(sc, 0, 1, MAC_ADDRESS_TYPE_NETWORK,
					&sc->macaddr);
	if (rc)
		goto error;

	if (IS_BE(sc) && (sc->flags & OCE_FLAGS_BE3)) {
		rc = oce_mbox_check_native_mode(sc);
		if (rc)
			goto error;
	} else
		sc->be3_native = 0;

	return rc;

error:
	oce_dma_free(sc, &sc->bsmbx);
	printf("%s: Hardware initialisation failed\n", sc->dev.dv_xname);
	return rc;
}

/**
 * @brief	Allocate PCI resources.
 *
 * @param sc		software handle to the device
 * @returns		0 if successful, or error
 */
int
oce_hw_pci_alloc(struct oce_softc *sc)
{
	struct pci_attach_args *pa = &sc->pa;
	pci_sli_intf_t intf;
	pcireg_t memtype, reg;

	/* setup the device config region */
	if (IS_BE(sc) && (sc->flags & OCE_FLAGS_BE2))
		reg = OCE_DEV_BE2_CFG_BAR;
	else
		reg = OCE_DEV_CFG_BAR;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
	if (pci_mapreg_map(pa, reg, memtype, 0, &sc->cfg_btag,
	    &sc->cfg_bhandle, NULL, &sc->cfg_size,
	    IS_BE(sc) ? 0 : 32768)) {
		printf(": can't find cfg mem space\n");
		return ENXIO;
	}

	/* Read the SLI_INTF register and determine whether we
	 * can use this port and its features
	 */
	intf.dw0 = pci_conf_read(pa->pa_pc, pa->pa_tag, OCE_INTF_REG_OFFSET);

	if (intf.bits.sli_valid != OCE_INTF_VALID_SIG) {
		printf(": invalid signature\n");
		goto fail_1;
	}

	if (intf.bits.sli_rev != OCE_INTF_SLI_REV4) {
		printf(": adapter doesnt support SLI revision %d\n",
		    intf.bits.sli_rev);
		goto fail_1;
	}

	if (intf.bits.sli_if_type == OCE_INTF_IF_TYPE_1)
		sc->flags |= OCE_FLAGS_MBOX_ENDIAN_RQD;

	if (intf.bits.sli_hint1 == OCE_INTF_FUNC_RESET_REQD)
		sc->flags |= OCE_FLAGS_FUNCRESET_RQD;

	if (intf.bits.sli_func_type == OCE_INTF_VIRT_FUNC)
		sc->flags |= OCE_FLAGS_VIRTUAL_PORT;

	/* Lancer has one BAR (CFG) but BE3 has three (CFG, CSR, DB) */
	if (IS_BE(sc)) {
		/* set up CSR region */
		reg = OCE_PCI_CSR_BAR;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
		if (pci_mapreg_map(pa, reg, memtype, 0, &sc->csr_btag,
		    &sc->csr_bhandle, NULL, &sc->csr_size, 0)) {
			printf(": can't find csr mem space\n");
			goto fail_1;
		}

		/* set up DB doorbell region */
		reg = OCE_PCI_DB_BAR;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
		if (pci_mapreg_map(pa, reg, memtype, 0, &sc->db_btag,
		    &sc->db_bhandle, NULL, &sc->db_size, 0)) {
			printf(": can't find csr mem space\n");
			goto fail_2;
		}
	}

	return 0;

fail_2:
	bus_space_unmap(sc->csr_btag, sc->csr_bhandle, sc->csr_size);
fail_1:
	bus_space_unmap(sc->cfg_btag, sc->cfg_bhandle, sc->cfg_size);
	return ENXIO;
}

/**
 * @brief		Function for creating nw interface.
 * @param sc		software handle to the device
 * @returns		0 on success, error otherwise
 */
int
oce_create_nw_interface(struct oce_softc *sc)
{
	int rc;
	uint32_t capab_flags;
	uint32_t capab_en_flags;

	/* interface capabilities to give device when creating interface */
	capab_flags = OCE_CAPAB_FLAGS;

	/* capabilities to enable by default (others set dynamically) */
	capab_en_flags = OCE_CAPAB_ENABLE;

	if (IS_XE201(sc)) {
		/* LANCER A0 workaround */
		capab_en_flags &= ~MBX_RX_IFACE_FLAGS_PASS_L3L4_ERR;
		capab_flags &= ~MBX_RX_IFACE_FLAGS_PASS_L3L4_ERR;
	}

	/* enable capabilities controlled via driver startup parameters */
	if (sc->rss_enable)
		capab_en_flags |= MBX_RX_IFACE_FLAGS_RSS;
	else {
		capab_en_flags &= ~MBX_RX_IFACE_FLAGS_RSS;
		capab_flags &= ~MBX_RX_IFACE_FLAGS_RSS;
	}

	rc = oce_if_create(sc, capab_flags, capab_en_flags, 0,
	    &sc->macaddr.mac_addr[0], &sc->if_id);
	if (rc)
		return rc;

	sc->nifs++;

	sc->if_cap_flags = capab_en_flags;

	/* Enable VLAN Promisc on HW */
	rc = oce_config_vlan(sc, (uint8_t)sc->if_id, NULL, 0, 1, 1);
	if (rc)
		goto error;

	/* set default flow control */
	rc = oce_set_flow_control(sc, sc->flow_control);
	if (rc)
		goto error;

	return rc;

error:
	oce_delete_nw_interface(sc);
	return rc;
}

/**
 * @brief		Function to delete a nw interface.
 * @param sc		software handle to the device
 */
void
oce_delete_nw_interface(struct oce_softc *sc)
{
	/* currently only single interface is implmeneted */
	if (sc->nifs > 0) {
		oce_if_del(sc, sc->if_id);
		sc->nifs--;
	}
}

/**
 * @brief 		Function for hardware enable interupts.
 * @param sc		software handle to the device
 */
void
oce_hw_intr_enable(struct oce_softc *sc)
{
	uint32_t reg;

	reg = OCE_READ_REG32(sc, cfg, PCICFG_INTR_CTRL);
	reg |= HOSTINTR_MASK;
	OCE_WRITE_REG32(sc, cfg, PCICFG_INTR_CTRL, reg);
}

/**
 * @brief 		Function for hardware disable interupts
 * @param sc		software handle to the device
 */
void
oce_hw_intr_disable(struct oce_softc *sc)
{
	uint32_t reg;

	reg = OCE_READ_REG32(sc, cfg, PCICFG_INTR_CTRL);
	reg &= ~HOSTINTR_MASK;
	OCE_WRITE_REG32(sc, cfg, PCICFG_INTR_CTRL, reg);
}

/**
 * @brief		Function for hardware update multicast filter
 * @param sc		software handle to the device
 */
int
oce_hw_update_multicast(struct oce_softc *sc)
{
	struct ether_multi *enm;
	struct ether_multistep step;
	struct mbx_set_common_iface_multicast *req = NULL;
	struct oce_dma_mem dma;
	int rc = 0;

	/* Allocate DMA mem*/
	if (oce_dma_alloc(sc, sizeof(struct mbx_set_common_iface_multicast),
							&dma, 0))
		return ENOMEM;

	req = OCE_DMAPTR(&dma, struct mbx_set_common_iface_multicast);
	bzero(req, sizeof(struct mbx_set_common_iface_multicast));

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		if (req->params.req.num_mac == OCE_MAX_MC_FILTER_SIZE) {
			/*More multicast addresses than our hardware table
			  So Enable multicast promiscus in our hardware to
			  accept all multicat packets
			*/
			req->params.req.promiscuous = 1;
			break;
		}
		bcopy(enm->enm_addrlo,
			&req->params.req.mac[req->params.req.num_mac],
			ETH_ADDR_LEN);
		req->params.req.num_mac = req->params.req.num_mac + 1;
		ETHER_NEXT_MULTI(step, enm);
	}

	req->params.req.if_id = sc->if_id;
	rc = oce_update_multicast(sc, &dma);
	oce_dma_free(sc, &dma);
	return rc;
}

/**
 * @brief Reset (firmware) common function
 * @param sc		software handle to the device
 * @returns		0 on success, ETIMEDOUT on failure
 */
int
oce_reset_fun(struct oce_softc *sc)
{
	struct oce_mbx *mbx;
	struct oce_bmbx *mb;
	struct ioctl_common_function_reset *fwcmd;
	int rc = 0;

	if (sc->flags & OCE_FLAGS_FUNCRESET_RQD) {
		mb = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
		mbx = &mb->mbx;
		bzero(mbx, sizeof(struct oce_mbx));

		fwcmd = (struct ioctl_common_function_reset *)&mbx->payload;
		mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
					MBX_SUBSYSTEM_COMMON,
					OPCODE_COMMON_FUNCTION_RESET,
					10,	/* MBX_TIMEOUT_SEC */
					sizeof(struct
					    ioctl_common_function_reset),
					OCE_MBX_VER_V0);

		mbx->u0.s.embedded = 1;
		mbx->payload_length =
		    sizeof(struct ioctl_common_function_reset);

		rc = oce_mbox_dispatch(sc, 2);
	}

	return rc;
}

/**
 * @brief  		This funtions tells firmware we are
 *			done with commands.
 * @param sc            software handle to the device
 * @returns             0 on success, ETIMEDOUT on failure
 */
int
oce_fw_clean(struct oce_softc *sc)
{
	struct oce_bmbx *mbx;
	uint8_t *ptr;
	int ret = 0;

	mbx = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
	ptr = (uint8_t *)&mbx->mbx;

	/* Endian Signature */
	*ptr++ = 0xff;
	*ptr++ = 0xaa;
	*ptr++ = 0xbb;
	*ptr++ = 0xff;
	*ptr++ = 0xff;
	*ptr++ = 0xcc;
	*ptr++ = 0xdd;
	*ptr = 0xff;

	ret = oce_mbox_dispatch(sc, 2);

	return ret;
}

/**
 * @brief Mailbox wait
 * @param sc		software handle to the device
 * @param tmo_sec	timeout in seconds
 */
int
oce_mbox_wait(struct oce_softc *sc, uint32_t tmo_sec)
{
	tmo_sec *= 10000;
	pd_mpu_mbox_db_t mbox_db;

	for (;;) {
		if (tmo_sec != 0) {
			if (--tmo_sec == 0)
				break;
		}

		mbox_db.dw0 = OCE_READ_REG32(sc, db, PD_MPU_MBOX_DB);

		if (mbox_db.bits.ready)
			return 0;

		DELAY(100);
	}

	printf("%s: Mailbox timed out\n", sc->dev.dv_xname);

	return ETIMEDOUT;
}

/**
 * @brief Mailbox dispatch
 * @param sc		software handle to the device
 * @param tmo_sec	timeout in seconds
 */
int
oce_mbox_dispatch(struct oce_softc *sc, uint32_t tmo_sec)
{
	pd_mpu_mbox_db_t mbox_db;
	uint32_t pa;
	int rc;

	oce_dma_sync(&sc->bsmbx, BUS_DMASYNC_PREWRITE);
	pa = (uint32_t) ((uint64_t) sc->bsmbx.paddr >> 34);
	bzero(&mbox_db, sizeof(pd_mpu_mbox_db_t));
	mbox_db.bits.ready = 0;
	mbox_db.bits.hi = 1;
	mbox_db.bits.address = pa;

	rc = oce_mbox_wait(sc, tmo_sec);
	if (rc == 0) {
		OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

		pa = (uint32_t) ((uint64_t) sc->bsmbx.paddr >> 4) & 0x3fffffff;
		mbox_db.bits.ready = 0;
		mbox_db.bits.hi = 0;
		mbox_db.bits.address = pa;

		rc = oce_mbox_wait(sc, tmo_sec);

		if (rc == 0) {
			OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

			rc = oce_mbox_wait(sc, tmo_sec);

			oce_dma_sync(&sc->bsmbx, BUS_DMASYNC_POSTWRITE);
		}
	}

	return rc;
}

/**
 * @brief 		Mailbox common request header initialization
 * @param hdr		mailbox header
 * @param dom		domain
 * @param port		port
 * @param subsys	subsystem
 * @param opcode	opcode
 * @param timeout	timeout
 * @param payload_len	payload length
 */
void
mbx_common_req_hdr_init(struct mbx_hdr *hdr, uint8_t dom, uint8_t port,
    uint8_t subsys, uint8_t opcode, uint32_t timeout, uint32_t payload_len,
    uint8_t version)
{
	hdr->u0.req.opcode = opcode;
	hdr->u0.req.subsystem = subsys;
	hdr->u0.req.port_number = port;
	hdr->u0.req.domain = dom;

	hdr->u0.req.timeout = timeout;
	hdr->u0.req.request_length = payload_len - sizeof(struct mbx_hdr);
	hdr->u0.req.version = version;
}

/**
 * @brief Function to initialize the hw with host endian information
 * @param sc		software handle to the device
 * @returns		0 on success, ETIMEDOUT on failure
 */
int
oce_mbox_init(struct oce_softc *sc)
{
	struct oce_bmbx *mbx;
	uint8_t *ptr;
	int ret = 0;

	if (sc->flags & OCE_FLAGS_MBOX_ENDIAN_RQD) {
		mbx = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
		ptr = (uint8_t *) &mbx->mbx;

		/* Endian Signature */
		*ptr++ = 0xff;
		*ptr++ = 0x12;
		*ptr++ = 0x34;
		*ptr++ = 0xff;
		*ptr++ = 0xff;
		*ptr++ = 0x56;
		*ptr++ = 0x78;
		*ptr = 0xff;

		ret = oce_mbox_dispatch(sc, 0);
	}

	return ret;
}

/**
 * @brief 		Function to get the firmware version
 * @param sc		software handle to the device
 * @returns		0 on success, EIO on failure
 */
int
oce_get_fw_version(struct oce_softc *sc)
{
	struct oce_mbx mbx;
	struct mbx_get_common_fw_version *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_get_common_fw_version *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_GET_FW_VERSION,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_common_fw_version),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_get_common_fw_version);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	bcopy(fwcmd->params.rsp.fw_ver_str, sc->fw_version, 32);

	return 0;
}

/**
 * @brief	Firmware will send gracious notifications during
 *		attach only after sending first mcc commnad. We
 *		use MCC queue only for getting async and mailbox
 *		for sending cmds. So to get gracious notifications
 *		atleast send one dummy command on mcc.
 */
int
oce_first_mcc_cmd(struct oce_softc *sc)
{
	struct oce_mbx *mbx;
	struct oce_mq *mq = sc->mq;
	struct mbx_get_common_fw_version *fwcmd;
	uint32_t reg_value;

	mbx = RING_GET_PRODUCER_ITEM_VA(mq->ring, struct oce_mbx);
	bzero(mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_get_common_fw_version *)&mbx->payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_GET_FW_VERSION,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_common_fw_version),
				OCE_MBX_VER_V0);
	mbx->u0.s.embedded = 1;
	mbx->payload_length = sizeof(struct mbx_get_common_fw_version);
	oce_dma_sync(&mq->ring->dma, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);
	RING_PUT(mq->ring, 1);
	reg_value = (1 << 16) | mq->mq_id;
	OCE_WRITE_REG32(sc, db, PD_MQ_DB, reg_value);

	return 0;
}

/**
 * @brief		Function to post a MBX to the mbox
 * @param sc		software handle to the device
 * @param mbx 		pointer to the MBX to send
 * @param mbxctx	pointer to the mbx context structure
 * @returns		0 on success, error on failure
 */
int
oce_mbox_post(struct oce_softc *sc, struct oce_mbx *mbx, struct oce_mbx_ctx *mbxctx)
{
	struct oce_mbx *mb_mbx = NULL;
	struct oce_mq_cqe *mb_cqe = NULL;
	struct oce_bmbx *mb = NULL;
	int rc = 0;
	uint32_t tmo = 0;
	uint32_t cstatus = 0;
	uint32_t xstatus = 0;

	mb = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
	mb_mbx = &mb->mbx;

	/* get the tmo */
	tmo = mbx->tag[0];
	mbx->tag[0] = 0;

	/* copy mbx into mbox */
	bcopy(mbx, mb_mbx, sizeof(struct oce_mbx));

	/* now dispatch */
	rc = oce_mbox_dispatch(sc, tmo);
	if (rc == 0) {
		/*
		 * the command completed successfully. Now get the
		 * completion queue entry
		 */
		mb_cqe = &mb->cqe;
		DW_SWAP(u32ptr(&mb_cqe->u0.dw[0]), sizeof(struct oce_mq_cqe));

		/* copy mbox mbx back */
		bcopy(mb_mbx, mbx, sizeof(struct oce_mbx));

		/* pick up the mailbox status */
		cstatus = mb_cqe->u0.s.completion_status;
		xstatus = mb_cqe->u0.s.extended_status;

		/*
		 * store the mbx context in the cqe tag section so that
		 * the upper layer handling the cqe can associate the mbx
		 * with the response
		 */
		if (cstatus == 0 && mbxctx) {
			/* save context */
			mbxctx->mbx = mb_mbx;
			bcopy(&mbxctx, mb_cqe->u0.s.mq_tag,
				sizeof(struct oce_mbx_ctx *));
		}
	}

	return rc;
}

/**
 * @brief Function to read the mac address associated with an interface
 * @param sc		software handle to the device
 * @param if_id 	interface id to read the address from
 * @param perm 		set to 1 if reading the factory mac address.
 *			In this case if_id is ignored
 * @param type 		type of the mac address, whether network or storage
 * @param[out] mac 	[OUTPUT] pointer to a buffer containing the
 *			mac address when the command succeeds.
 * @returns		0 on success, EIO on failure
 */
int
oce_read_mac_addr(struct oce_softc *sc, uint32_t if_id, uint8_t perm,
    uint8_t type, struct mac_address_format *mac)
{
	struct oce_mbx mbx;
	struct mbx_query_common_iface_mac *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_query_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_query_common_iface_mac),
				OCE_MBX_VER_V0);

	fwcmd->params.req.permanent = perm;
	if (!perm)
		fwcmd->params.req.if_id = (uint16_t) if_id;
	else
		fwcmd->params.req.if_id = 0;

	fwcmd->params.req.type = type;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_query_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	/* copy the mac addres in the output parameter */
	mac->size_of_struct = fwcmd->params.rsp.mac.size_of_struct;
	bcopy(&fwcmd->params.rsp.mac.mac_addr[0], &mac->mac_addr[0],
		mac->size_of_struct);

	return 0;
}

/**
 * @brief Function to query the fw attributes from the hw
 * @param sc		software handle to the device
 * @returns		0 on success, EIO on failure
 */
int
oce_get_fw_config(struct oce_softc *sc)
{
	struct oce_mbx mbx;
	struct mbx_common_query_fw_config *fwcmd;
	int ret = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_query_fw_config *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_FIRMWARE_CONFIG,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_query_fw_config),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_query_fw_config);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	ret = oce_mbox_post(sc, &mbx, NULL);
	if (ret)
		return ret;

	DW_SWAP(u32ptr(fwcmd), sizeof(struct mbx_common_query_fw_config));

	sc->config_number = fwcmd->params.rsp.config_number;
	sc->asic_revision = fwcmd->params.rsp.asic_revision;
	sc->port_id	  = fwcmd->params.rsp.port_id;
	sc->function_mode = fwcmd->params.rsp.function_mode;
	sc->function_caps = fwcmd->params.rsp.function_caps;

	if (fwcmd->params.rsp.ulp[0].ulp_mode & ULP_NIC_MODE) {
		sc->max_tx_rings = fwcmd->params.rsp.ulp[0].nic_wq_tot;
		sc->max_rx_rings = fwcmd->params.rsp.ulp[0].lro_rqid_tot;
	} else {
		sc->max_tx_rings = fwcmd->params.rsp.ulp[1].nic_wq_tot;
		sc->max_rx_rings = fwcmd->params.rsp.ulp[1].lro_rqid_tot;
	}

	return 0;

}

/**
 *
 * @brief function to create a device interface
 * @param sc		software handle to the device
 * @param cap_flags	capability flags
 * @param en_flags	enable capability flags
 * @param vlan_tag	optional vlan tag to associate with the if
 * @param mac_addr	pointer to a buffer containing the mac address
 * @param[out] if_id	[OUTPUT] pointer to an integer to hold the ID of the
 interface created
 * @returns		0 on success, EIO on failure
 */
int
oce_if_create(struct oce_softc *sc, uint32_t cap_flags, uint32_t en_flags,
    uint16_t vlan_tag, uint8_t *mac_addr, uint32_t *if_id)
{
	struct oce_mbx mbx;
	struct mbx_create_common_iface *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_iface *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_IFACE,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_iface),
				OCE_MBX_VER_V0);
	DW_SWAP(u32ptr(&fwcmd->hdr), sizeof(struct mbx_hdr));

	fwcmd->params.req.version = 0;
	fwcmd->params.req.cap_flags = htole32(cap_flags);
	fwcmd->params.req.enable_flags = htole32(en_flags);
	if (mac_addr != NULL) {
		bcopy(mac_addr, &fwcmd->params.req.mac_addr[0], 6);
		fwcmd->params.req.vlan_tag.u0.normal.vtag = htole16(vlan_tag);
		fwcmd->params.req.mac_invalid = 0;
	} else {
		fwcmd->params.req.mac_invalid = 1;
		printf(": invalid mac");
	}

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_iface);
	DW_SWAP(u32ptr(&mbx), OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		return rc;

	*if_id = letoh32(fwcmd->params.rsp.if_id);

	if (mac_addr != NULL)
		sc->pmac_id = letoh32(fwcmd->params.rsp.pmac_id);

	return 0;
}

/**
 * @brief		Function to delete an interface
 * @param sc 		software handle to the device
 * @param if_id		ID of the interface to delete
 * @returns		0 on success, EIO on failure
 */
int
oce_if_del(struct oce_softc *sc, uint32_t if_id)
{
	struct oce_mbx mbx;
	struct mbx_destroy_common_iface *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_destroy_common_iface *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_DESTROY_IFACE,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_destroy_common_iface),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = if_id;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_destroy_common_iface);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}

/**
 * @brief Function to send the mbx command to configure vlan
 * @param sc 		software handle to the device
 * @param if_id 	interface identifier index
 * @param vtag_arr	array of vlan tags
 * @param vtag_cnt	number of elements in array
 * @param untagged	boolean TRUE/FLASE
 * @param enable_promisc flag to enable/disable VLAN promiscuous mode
 * @returns		0 on success, EIO on failure
 */
int
oce_config_vlan(struct oce_softc *sc, uint32_t if_id,
    struct normal_vlan *vtag_arr, uint8_t vtag_cnt, uint32_t untagged,
    uint32_t enable_promisc)
{
	struct oce_mbx mbx;
	struct mbx_common_config_vlan *fwcmd;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = (struct mbx_common_config_vlan *)&mbx.payload;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CONFIG_IFACE_VLAN,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_config_vlan),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = (uint8_t) if_id;
	fwcmd->params.req.promisc = (uint8_t) enable_promisc;
	fwcmd->params.req.untagged = (uint8_t) untagged;
	fwcmd->params.req.num_vlans = vtag_cnt;

	if (!enable_promisc) {
		bcopy(vtag_arr, fwcmd->params.req.tags.normal_vlans,
			vtag_cnt * sizeof(struct normal_vlan));
	}
	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_config_vlan);
	DW_SWAP(u32ptr(&mbx), (OCE_BMBX_RHDR_SZ + mbx.payload_length));

	rc = oce_mbox_post(sc, &mbx, NULL);

	return rc;
}

/**
 * @brief Function to set flow control capability in the hardware
 * @param sc 		software handle to the device
 * @param flow_control	flow control flags to set
 * @returns		0 on success, EIO on failure
 */
int
oce_set_flow_control(struct oce_softc *sc, uint32_t flow_control)
{
	struct oce_mbx mbx;
	struct mbx_common_get_set_flow_control *fwcmd =
		(struct mbx_common_get_set_flow_control *)&mbx.payload;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_FLOW_CONTROL,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_get_set_flow_control),
				OCE_MBX_VER_V0);

	if (flow_control & OCE_FC_TX)
		fwcmd->tx_flow_control = 1;

	if (flow_control & OCE_FC_RX)
		fwcmd->rx_flow_control = 1;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_get_set_flow_control);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	return rc;
}

/**
 * @brief Initialize the RSS CPU indirection table
 *
 * The table is used to choose the queue to place the incomming packets.
 * Incomming packets are hashed.  The lowest bits in the hash result
 * are used as the index into the CPU indirection table.
 * Each entry in the table contains the RSS CPU-ID returned by the NIC
 * create.  Based on the CPU ID, the receive completion is routed to
 * the corresponding RSS CQs.  (Non-RSS packets are always completed
 * on the default (0) CQ).
 *
 * @param sc 		software handle to the device
 * @param *fwcmd	pointer to the rss mbox command
 * @returns		none
 */
int
oce_rss_itbl_init(struct oce_softc *sc, struct mbx_config_nic_rss *fwcmd)
{
	int i = 0, j = 0, rc = 0;
	uint8_t *tbl = fwcmd->params.req.cputable;


	for (j = 0; j < sc->nrqs; j++) {
		if (sc->rq[j]->cfg.is_rss_queue) {
			tbl[i] = sc->rq[j]->rss_cpuid;
			i = i + 1;
		}
	}
	if (i == 0) {
		printf("%s: error: Invalid number of RSS RQ's\n",
		    sc->dev.dv_xname);
		rc = ENXIO;

	}

	/* fill log2 value indicating the size of the CPU table */
	if (rc == 0)
		fwcmd->params.req.cpu_tbl_sz_log2 = htole16(OCE_LOG2(i));

	return rc;
}

/**
 * @brief Function to set flow control capability in the hardware
 * @param sc 		software handle to the device
 * @param if_id 	interface id to read the address from
 * @param enable_rss	0=disable, RSS_ENABLE_xxx flags otherwise
 * @returns		0 on success, EIO on failure
 */
int
oce_config_nic_rss(struct oce_softc *sc, uint32_t if_id, uint16_t enable_rss)
{
	int rc;
	struct oce_mbx mbx;
	struct mbx_config_nic_rss *fwcmd =
				(struct mbx_config_nic_rss *)&mbx.payload;

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_CONFIG_RSS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_config_nic_rss),
				OCE_MBX_VER_V0);
	if (enable_rss)
		fwcmd->params.req.enable_rss = (RSS_ENABLE_IPV4 |
						RSS_ENABLE_TCP_IPV4 |
						RSS_ENABLE_IPV6 |
						RSS_ENABLE_TCP_IPV6);
	fwcmd->params.req.flush = OCE_FLUSH;
	fwcmd->params.req.if_id = htole32(if_id);

	arc4random_buf(fwcmd->params.req.hash, sizeof(fwcmd->params.req.hash));

	rc = oce_rss_itbl_init(sc, fwcmd);
	if (rc == 0) {
		mbx.u0.s.embedded = 1;
		mbx.payload_length = sizeof(struct mbx_config_nic_rss);
		DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

		rc = oce_mbox_post(sc, &mbx, NULL);

	}

	return rc;
}

/**
 * @brief 		RXF function to enable/disable device promiscuous mode
 * @param sc		software handle to the device
 * @param enable	enable/disable flag
 * @returns		0 on success, EIO on failure
 * @note
 *	The OPCODE_NIC_CONFIG_PROMISCUOUS command deprecated for Lancer.
 *	This function uses the COMMON_SET_IFACE_RX_FILTER command instead.
 */
int
oce_rxf_set_promiscuous(struct oce_softc *sc, uint32_t enable)
{
	struct mbx_set_common_iface_rx_filter *fwcmd;
	int sz = sizeof(struct mbx_set_common_iface_rx_filter);
	iface_rx_filter_ctx_t *req;
	struct oce_dma_mem sgl;
	int rc;

	/* allocate mbx payload's dma scatter/gather memory */
	rc = oce_dma_alloc(sc, sz, &sgl, 0);
	if (rc)
		return rc;

	fwcmd = OCE_DMAPTR(&sgl, struct mbx_set_common_iface_rx_filter);

	req =  &fwcmd->params.req;
	req->iface_flags_mask = MBX_RX_IFACE_FLAGS_PROMISCUOUS |
				MBX_RX_IFACE_FLAGS_VLAN_PROMISCUOUS;
	if (enable) {
		req->iface_flags = MBX_RX_IFACE_FLAGS_PROMISCUOUS |
				   MBX_RX_IFACE_FLAGS_VLAN_PROMISCUOUS;
	}
	req->if_id = sc->if_id;

	rc = oce_set_common_iface_rx_filter(sc, &sgl);
	oce_dma_free(sc, &sgl);

	return rc;
}

/**
 * @brief 			Function modify and select rx filter options
 * @param sc			software handle to the device
 * @param sgl			scatter/gather request/response
 * @returns			0 on success, error code on failure
 */
int
oce_set_common_iface_rx_filter(struct oce_softc *sc, struct oce_dma_mem *sgl)
{
	struct oce_mbx mbx;
	int mbx_sz = sizeof(struct mbx_set_common_iface_rx_filter);
	struct mbx_set_common_iface_rx_filter *fwcmd;
	int rc;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(sgl, struct mbx_set_common_iface_rx_filter);

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_IFACE_RX_FILTER,
				MBX_TIMEOUT_SEC,
				mbx_sz,
				OCE_MBX_VER_V0);

	oce_dma_sync(sgl, BUS_DMASYNC_PREWRITE);
	mbx.u0.s.embedded = 0;
	mbx.u0.s.sge_count = 1;
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(sgl->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(sgl->paddr);
	mbx.payload.u0.u1.sgl[0].length = mbx_sz;
	mbx.payload_length = mbx_sz;
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}

/**
 * @brief Function to query the link status from the hardware
 * @param sc 		software handle to the device
 * @param[out] link	pointer to the structure returning link attributes
 * @returns		0 on success, EIO on failure
 */
int
oce_get_link_status(struct oce_softc *sc)
{
	struct link_status link;
	struct oce_mbx mbx;
	struct mbx_query_common_link_config *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_query_common_link_config *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_QUERY_LINK_CONFIG,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_query_common_link_config),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_query_common_link_config);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	if (rc) {
		printf("%s: Could not get link speed: %d\n",
		    sc->dev.dv_xname, rc);
		return rc;
	} else {
		/* interpret response */
		bcopy(&fwcmd->params.rsp, &link, sizeof(struct link_status));
		link.logical_link_status = letoh32(link.logical_link_status);
		link.qos_link_speed = letoh16(link.qos_link_speed);
	}

	if (link.logical_link_status == NTWK_LOGICAL_LINK_UP)
		sc->link_status = NTWK_LOGICAL_LINK_UP;
	else
		sc->link_status = NTWK_LOGICAL_LINK_DOWN;

	if (link.mac_speed > 0 && link.mac_speed < 5)
		sc->link_speed = link.mac_speed;
	else
		sc->link_speed = 0;

	sc->duplex = link.mac_duplex;

	sc->qos_link_speed = (uint32_t )link.qos_link_speed * 10;

	return rc;
}

int
oce_mbox_get_nic_stats_v0(struct oce_softc *sc, struct oce_dma_mem *pstats_dma_mem)
{
	struct oce_mbx mbx;
	struct mbx_get_nic_stats_v0 *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_nic_stats_v0);
	bzero(fwcmd, sizeof(struct mbx_get_nic_stats_v0));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_GET_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_nic_stats_v0),
				OCE_MBX_VER_V0);

	mbx.u0.s.embedded = 0;
	mbx.u0.s.sge_count = 1;

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);

	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_nic_stats_v0);

	mbx.payload_length = sizeof(struct mbx_get_nic_stats_v0);

	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);

	if (rc) {
		printf("%s: Could not get nic statistics: %d\n",
		    sc->dev.dv_xname, rc);
	}

	return rc;
}

/**
 * @brief Function to get NIC statistics
 * @param sc 		software handle to the device
 * @param *stats	pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 * @note		command depricated in Lancer
 */
int
oce_mbox_get_nic_stats(struct oce_softc *sc, struct oce_dma_mem *pstats_dma_mem)
{
	struct oce_mbx mbx;
	struct mbx_get_nic_stats *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_nic_stats);
	bzero(fwcmd, sizeof(struct mbx_get_nic_stats));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_GET_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_nic_stats),
				OCE_MBX_VER_V1);


	mbx.u0.s.embedded = 0;  /* stats too large for embedded mbx rsp */
	mbx.u0.s.sge_count = 1; /* using scatter gather instead */

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_nic_stats);

	mbx.payload_length = sizeof(struct mbx_get_nic_stats);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);
	if (rc) {
		printf("%s: Could not get nic statistics: %d\n",
		    sc->dev.dv_xname, rc);
	}
	return rc;
}

/**
 * @brief Function to get pport (physical port) statistics
 * @param sc 		software handle to the device
 * @param *stats	pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 */
int
oce_mbox_get_pport_stats(struct oce_softc *sc,
    struct oce_dma_mem *pstats_dma_mem, uint32_t reset_stats)
{
	struct oce_mbx mbx;
	struct mbx_get_pport_stats *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));
	fwcmd = OCE_DMAPTR(pstats_dma_mem, struct mbx_get_pport_stats);
	bzero(fwcmd, sizeof(struct mbx_get_pport_stats));

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_GET_PPORT_STATS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_get_pport_stats),
				OCE_MBX_VER_V0);

	fwcmd->params.req.reset_stats = reset_stats;
	fwcmd->params.req.port_number = sc->if_id;

	mbx.u0.s.embedded = 0;	/* stats too large for embedded mbx rsp */
	mbx.u0.s.sge_count = 1; /* using scatter gather instead */

	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_PREWRITE);
	mbx.payload.u0.u1.sgl[0].pa_lo = ADDR_LO(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].pa_hi = ADDR_HI(pstats_dma_mem->paddr);
	mbx.payload.u0.u1.sgl[0].length = sizeof(struct mbx_get_pport_stats);

	mbx.payload_length = sizeof(struct mbx_get_pport_stats);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	oce_dma_sync(pstats_dma_mem, BUS_DMASYNC_POSTWRITE);

	if (rc != 0) {
		printf("%s: Could not get physical port statistics: %d\n",
		    sc->dev.dv_xname, rc);
	}

	return rc;
}

/**
 * @brief               Function to update the muticast filter with
 *                      values in dma_mem
 * @param sc            software handle to the device
 * @param dma_mem       pointer to dma memory region
 * @returns             0 on success, EIO on failure
 */
int
oce_update_multicast(struct oce_softc *sc, struct oce_dma_mem *pdma_mem)
{
	struct oce_mbx mbx;
	struct oce_mq_sge *sgl;
	struct mbx_set_common_iface_multicast *req = NULL;
	int rc = 0;

	req = OCE_DMAPTR(pdma_mem, struct mbx_set_common_iface_multicast);
	mbx_common_req_hdr_init(&req->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_IFACE_MULTICAST,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_set_common_iface_multicast),
				OCE_MBX_VER_V0);

	bzero(&mbx, sizeof(struct oce_mbx));

	mbx.u0.s.embedded = 0; /*Non embeded*/
	mbx.payload_length = sizeof(struct mbx_set_common_iface_multicast);
	mbx.u0.s.sge_count = 1;
	sgl = &mbx.payload.u0.u1.sgl[0];
	sgl->pa_hi = htole32(upper_32_bits(pdma_mem->paddr));
	sgl->pa_lo = htole32((pdma_mem->paddr) & 0xFFFFFFFF);
	sgl->length = htole32(mbx.payload_length);

	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);

	return rc;
}

int
oce_mbox_macaddr_add(struct oce_softc *sc, uint8_t *mac_addr, uint32_t if_id,
    uint32_t *pmac_id)
{
	struct oce_mbx mbx;
	struct mbx_add_common_iface_mac *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_add_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_ADD_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_add_common_iface_mac),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = (uint16_t) if_id;
	bcopy(mac_addr, fwcmd->params.req.mac_address, 6);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_add_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);
	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		return rc;

	*pmac_id = fwcmd->params.rsp.pmac_id;

	return rc;
}

int
oce_mbox_macaddr_del(struct oce_softc *sc, uint32_t if_id, uint32_t pmac_id)
{
	struct oce_mbx mbx;
	struct mbx_del_common_iface_mac *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_del_common_iface_mac *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_DEL_IFACE_MAC,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_del_common_iface_mac),
				OCE_MBX_VER_V0);

	fwcmd->params.req.if_id = (uint16_t)if_id;
	fwcmd->params.req.pmac_id = pmac_id;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct  mbx_del_common_iface_mac);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	return rc;
}

int
oce_mbox_check_native_mode(struct oce_softc *sc)
{
	struct oce_mbx mbx;
	struct mbx_common_set_function_cap *fwcmd;
	int rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_common_set_function_cap *)&mbx.payload;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_SET_FUNCTIONAL_CAPS,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_common_set_function_cap),
				OCE_MBX_VER_V0);

	fwcmd->params.req.valid_capability_flags = CAP_SW_TIMESTAMPS |
							CAP_BE3_NATIVE_ERX_API;

	fwcmd->params.req.capability_flags = CAP_BE3_NATIVE_ERX_API;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_common_set_function_cap);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc != 0)
		printf(" mbox failure!");
	//if (rc != 0)		This can fail in legacy mode. So skip
	//	FN_LEAVE(rc);

	sc->be3_native = fwcmd->params.rsp.capability_flags
			& CAP_BE3_NATIVE_ERX_API;

	return 0;
}

int
oce_mbox_create_rq(struct oce_rq *rq)
{
	struct oce_mbx mbx;
	struct mbx_create_nic_rq *fwcmd;
	struct oce_softc *sc = rq->parent;
	int num_pages, version, rc = 0;

	if (rq->qstate == QCREATED)
		return 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_nic_rq *)&mbx.payload;
	if (IS_XE201(sc))
		version = OCE_MBX_VER_V1;
	else
		version = OCE_MBX_VER_V0;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_CREATE_RQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_nic_rq),
				version);

	num_pages = oce_page_list(sc, rq->ring, &fwcmd->params.req.pages[0],
	    nitems(fwcmd->params.req.pages));
	if (!num_pages) {
		printf("%s: failed to load the rq ring\n", __func__);
		goto out;
	}

	if (version == OCE_MBX_VER_V1) {
		fwcmd->params.req.frag_size = rq->cfg.frag_size / 2048;
		fwcmd->params.req.page_size = 1;
	} else
		fwcmd->params.req.frag_size = OCE_LOG2(rq->cfg.frag_size);
	fwcmd->params.req.num_pages = num_pages;
	fwcmd->params.req.cq_id = rq->cq->cq_id;
	fwcmd->params.req.if_id = sc->if_id;
	fwcmd->params.req.max_frame_size = rq->cfg.mtu;
	fwcmd->params.req.is_rss_queue = rq->cfg.is_rss_queue;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_nic_rq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto out;

	rq->rq_id = letoh16(fwcmd->params.rsp.rq_id);
	rq->rss_cpuid = fwcmd->params.rsp.rss_cpuid;

out:
	return rc;
}

int
oce_mbox_create_wq(struct oce_wq *wq)
{
	struct oce_mbx mbx;
	struct mbx_create_nic_wq *fwcmd;
	struct oce_softc *sc = wq->parent;
	int num_pages, version, rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_nic_wq *)&mbx.payload;
	if (IS_XE201(sc)) {
		version = OCE_MBX_VER_V1;
		fwcmd->params.req.if_id = sc->if_id;
	} else
		version = OCE_MBX_VER_V0;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_NIC,
				OPCODE_NIC_CREATE_WQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_nic_wq),
				version);

	num_pages = oce_page_list(sc, wq->ring, &fwcmd->params.req.pages[0],
	    nitems(fwcmd->params.req.pages));
	if (!num_pages) {
		printf("%s: failed to load the wq ring\n", __func__);
		goto out;
	}

	fwcmd->params.req.nic_wq_type = wq->cfg.wq_type;
	fwcmd->params.req.num_pages = num_pages;
	fwcmd->params.req.wq_size = OCE_LOG2(wq->cfg.q_len) + 1;
	fwcmd->params.req.cq_id = htole16(wq->cq->cq_id);
	fwcmd->params.req.ulp_num = 1;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_nic_wq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto out;

	wq->wq_id = letoh16(fwcmd->params.rsp.wq_id);

out:
	return 0;
}

int
oce_mbox_create_mq(struct oce_mq *mq)
{
	struct oce_mbx mbx;
	struct mbx_create_common_mq_ex *fwcmd = NULL;
	struct oce_softc *sc = mq->parent;
	oce_mq_ext_ctx_t *ctx;
	int num_pages, version, rc = 0;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_mq_ex *)&mbx.payload;
	version = OCE_MBX_VER_V0;
	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_MQ_EXT,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_mq_ex),
				version);

	num_pages = oce_page_list(sc, mq->ring, &fwcmd->params.req.pages[0],
	    nitems(fwcmd->params.req.pages));
	if (!num_pages) {
		printf("%s: failed to load the mq ring\n", __func__);
		goto out;
	}

	ctx = &fwcmd->params.req.context;
	ctx->v0.num_pages = num_pages;
	ctx->v0.cq_id = mq->cq->cq_id;
	ctx->v0.ring_size = OCE_LOG2(mq->cfg.q_len) + 1;
	ctx->v0.valid = 1;
	/* Subscribe to Link State and Group 5 Events(bits 1 and 5 set) */
	ctx->v0.async_evt_bitmap = 0xffffffff;

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_mq_ex);
	DW_SWAP(u32ptr(&mbx), mbx.payload_length + OCE_BMBX_RHDR_SZ);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto out;

	mq->mq_id = letoh16(fwcmd->params.rsp.mq_id);

out:
	return rc;
}

int
oce_mbox_create_eq(struct oce_eq *eq)
{
	struct oce_mbx mbx;
	struct mbx_create_common_eq *fwcmd;
	struct oce_softc *sc = eq->parent;
	int rc = 0;
	uint32_t num_pages;

	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_eq *)&mbx.payload;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_EQ, MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_eq),
				OCE_MBX_VER_V0);

	num_pages = oce_page_list(sc, eq->ring, &fwcmd->params.req.pages[0],
	    nitems(fwcmd->params.req.pages));
	if (!num_pages) {
		printf("%s: failed to load the eq ring\n", __func__);
		goto out;
	}

	fwcmd->params.req.ctx.num_pages = htole16(num_pages);
	fwcmd->params.req.ctx.valid = 1;
	fwcmd->params.req.ctx.size = (eq->eq_cfg.item_size == 4) ? 0 : 1;
	fwcmd->params.req.ctx.count = OCE_LOG2(eq->eq_cfg.q_len / 256);
	fwcmd->params.req.ctx.armed = 0;
	fwcmd->params.req.ctx.delay_mult = htole32(eq->eq_cfg.cur_eqd);

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_eq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto out;

	eq->eq_id = letoh16(fwcmd->params.rsp.eq_id);

out:
	return rc;
}

int
oce_mbox_create_cq(struct oce_cq *cq, uint32_t ncoalesce,
    uint32_t is_eventable)
{
	struct oce_mbx mbx;
	struct mbx_create_common_cq *fwcmd;
	struct oce_softc *sc = cq->parent;
	uint8_t version;
	oce_cq_ctx_t *ctx;
	uint32_t num_pages, page_size;
	int rc = 0;


	bzero(&mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_create_common_cq *)&mbx.payload;

	if (IS_XE201(sc))
		version = OCE_MBX_VER_V2;
	else
		version = OCE_MBX_VER_V0;

	mbx_common_req_hdr_init(&fwcmd->hdr, 0, 0,
				MBX_SUBSYSTEM_COMMON,
				OPCODE_COMMON_CREATE_CQ,
				MBX_TIMEOUT_SEC,
				sizeof(struct mbx_create_common_cq),
				version);

	num_pages = oce_page_list(sc, cq->ring, &fwcmd->params.req.pages[0],
	    nitems(fwcmd->params.req.pages));
	if (!num_pages) {
		printf("%s: failed to load the cq ring\n", __func__);
		goto out;
	}

	page_size = 1;  /* 1 for 4K */

	ctx = &fwcmd->params.req.cq_ctx;

	if (version == OCE_MBX_VER_V2) {
		ctx->v2.num_pages = htole16(num_pages);
		ctx->v2.page_size = page_size;
		ctx->v2.eventable = is_eventable;
		ctx->v2.valid = 1;
		ctx->v2.count = OCE_LOG2(cq->cq_cfg.q_len / 256);
		ctx->v2.nodelay = cq->cq_cfg.nodelay;
		ctx->v2.coalesce_wm = ncoalesce;
		ctx->v2.armed = 0;
		ctx->v2.eq_id = cq->eq->eq_id;
		if (ctx->v2.count == 3) {
			if (cq->cq_cfg.q_len > (4*1024)-1)
				ctx->v2.cqe_count = (4*1024)-1;
			else
				ctx->v2.cqe_count = cq->cq_cfg.q_len;
		}
	} else {
		ctx->v0.num_pages = htole16(num_pages);
		ctx->v0.eventable = is_eventable;
		ctx->v0.valid = 1;
		ctx->v0.count = OCE_LOG2(cq->cq_cfg.q_len / 256);
		ctx->v0.nodelay = cq->cq_cfg.nodelay;
		ctx->v0.coalesce_wm = ncoalesce;
		ctx->v0.armed = 0;
		ctx->v0.eq_id = cq->eq->eq_id;
	}

	mbx.u0.s.embedded = 1;
	mbx.payload_length = sizeof(struct mbx_create_common_cq);

	rc = oce_mbox_post(sc, &mbx, NULL);
	if (rc)
		goto out;

	cq->cq_id = letoh16(fwcmd->params.rsp.cq_id);

out:
	return rc;
}

void
oce_refresh_queue_stats(struct oce_softc *sc)
{
	struct oce_drv_stats *adapter_stats;
	int i;

	adapter_stats = &sc->oce_stats_info;

	/* Caluculate total TX and TXstats from all queues */

	for (i = 0; i < sc->nrqs; i++) {
		adapter_stats->rx.t_rx_pkts += sc->rq[i]->rx_stats.rx_pkts;
		adapter_stats->rx.t_rx_bytes += sc->rq[i]->rx_stats.rx_bytes;
		adapter_stats->rx.t_rx_frags += sc->rq[i]->rx_stats.rx_frags;
		adapter_stats->rx.t_rx_mcast_pkts +=
					sc->rq[i]->rx_stats.rx_mcast_pkts;
		adapter_stats->rx.t_rx_ucast_pkts +=
					sc->rq[i]->rx_stats.rx_ucast_pkts;
		adapter_stats->rx.t_rxcp_errs += sc-> rq[i]->rx_stats.rxcp_err;
	}

	for (i = 0; i < sc->nwqs; i++) {
		adapter_stats->tx.t_tx_reqs += sc->wq[i]->tx_stats.tx_reqs;
		adapter_stats->tx.t_tx_stops += sc->wq[i]->tx_stats.tx_stops;
		adapter_stats->tx.t_tx_wrbs += sc->wq[i]->tx_stats.tx_wrbs;
		adapter_stats->tx.t_tx_compl += sc->wq[i]->tx_stats.tx_compl;
		adapter_stats->tx.t_tx_bytes += sc->wq[i]->tx_stats.tx_bytes;
		adapter_stats->tx.t_tx_pkts += sc->wq[i]->tx_stats.tx_pkts;
		adapter_stats->tx.t_ipv6_ext_hdr_tx_drop +=
				sc->wq[i]->tx_stats.ipv6_ext_hdr_tx_drop;
	}
}

void
copy_stats_to_sc_xe201(struct oce_softc *sc)
{
	struct oce_xe201_stats *adapter_stats;
	struct mbx_get_pport_stats *nic_mbx;
	struct pport_stats *port_stats;

	nic_mbx = OCE_DMAPTR(&sc->stats_mem, struct mbx_get_pport_stats);
	port_stats = &nic_mbx->params.rsp.pps;
	adapter_stats = &sc->oce_stats_info.u0.xe201;

	adapter_stats->tx_pkts = port_stats->tx_pkts;
	adapter_stats->tx_unicast_pkts = port_stats->tx_unicast_pkts;
	adapter_stats->tx_multicast_pkts = port_stats->tx_multicast_pkts;
	adapter_stats->tx_broadcast_pkts = port_stats->tx_broadcast_pkts;
	adapter_stats->tx_bytes = port_stats->tx_bytes;
	adapter_stats->tx_unicast_bytes = port_stats->tx_unicast_bytes;
	adapter_stats->tx_multicast_bytes = port_stats->tx_multicast_bytes;
	adapter_stats->tx_broadcast_bytes = port_stats->tx_broadcast_bytes;
	adapter_stats->tx_discards = port_stats->tx_discards;
	adapter_stats->tx_errors = port_stats->tx_errors;
	adapter_stats->tx_pause_frames = port_stats->tx_pause_frames;
	adapter_stats->tx_pause_on_frames = port_stats->tx_pause_on_frames;
	adapter_stats->tx_pause_off_frames = port_stats->tx_pause_off_frames;
	adapter_stats->tx_internal_mac_errors =
		port_stats->tx_internal_mac_errors;
	adapter_stats->tx_control_frames = port_stats->tx_control_frames;
	adapter_stats->tx_pkts_64_bytes = port_stats->tx_pkts_64_bytes;
	adapter_stats->tx_pkts_65_to_127_bytes =
		port_stats->tx_pkts_65_to_127_bytes;
	adapter_stats->tx_pkts_128_to_255_bytes =
		port_stats->tx_pkts_128_to_255_bytes;
	adapter_stats->tx_pkts_256_to_511_bytes =
		port_stats->tx_pkts_256_to_511_bytes;
	adapter_stats->tx_pkts_512_to_1023_bytes =
		port_stats->tx_pkts_512_to_1023_bytes;
	adapter_stats->tx_pkts_1024_to_1518_bytes =
		port_stats->tx_pkts_1024_to_1518_bytes;
	adapter_stats->tx_pkts_1519_to_2047_bytes =
		port_stats->tx_pkts_1519_to_2047_bytes;
	adapter_stats->tx_pkts_2048_to_4095_bytes =
		port_stats->tx_pkts_2048_to_4095_bytes;
	adapter_stats->tx_pkts_4096_to_8191_bytes =
		port_stats->tx_pkts_4096_to_8191_bytes;
	adapter_stats->tx_pkts_8192_to_9216_bytes =
		port_stats->tx_pkts_8192_to_9216_bytes;
	adapter_stats->tx_lso_pkts = port_stats->tx_lso_pkts;
	adapter_stats->rx_pkts = port_stats->rx_pkts;
	adapter_stats->rx_unicast_pkts = port_stats->rx_unicast_pkts;
	adapter_stats->rx_multicast_pkts = port_stats->rx_multicast_pkts;
	adapter_stats->rx_broadcast_pkts = port_stats->rx_broadcast_pkts;
	adapter_stats->rx_bytes = port_stats->rx_bytes;
	adapter_stats->rx_unicast_bytes = port_stats->rx_unicast_bytes;
	adapter_stats->rx_multicast_bytes = port_stats->rx_multicast_bytes;
	adapter_stats->rx_broadcast_bytes = port_stats->rx_broadcast_bytes;
	adapter_stats->rx_unknown_protos = port_stats->rx_unknown_protos;
	adapter_stats->rx_discards = port_stats->rx_discards;
	adapter_stats->rx_errors = port_stats->rx_errors;
	adapter_stats->rx_crc_errors = port_stats->rx_crc_errors;
	adapter_stats->rx_alignment_errors = port_stats->rx_alignment_errors;
	adapter_stats->rx_symbol_errors = port_stats->rx_symbol_errors;
	adapter_stats->rx_pause_frames = port_stats->rx_pause_frames;
	adapter_stats->rx_pause_on_frames = port_stats->rx_pause_on_frames;
	adapter_stats->rx_pause_off_frames = port_stats->rx_pause_off_frames;
	adapter_stats->rx_frames_too_long = port_stats->rx_frames_too_long;
	adapter_stats->rx_internal_mac_errors =
		port_stats->rx_internal_mac_errors;
	adapter_stats->rx_undersize_pkts = port_stats->rx_undersize_pkts;
	adapter_stats->rx_oversize_pkts = port_stats->rx_oversize_pkts;
	adapter_stats->rx_fragment_pkts = port_stats->rx_fragment_pkts;
	adapter_stats->rx_jabbers = port_stats->rx_jabbers;
	adapter_stats->rx_control_frames = port_stats->rx_control_frames;
	adapter_stats->rx_control_frames_unknown_opcode =
		port_stats->rx_control_frames_unknown_opcode;
	adapter_stats->rx_in_range_errors = port_stats->rx_in_range_errors;
	adapter_stats->rx_out_of_range_errors =
		port_stats->rx_out_of_range_errors;
	adapter_stats->rx_address_match_errors =
		port_stats->rx_address_match_errors;
	adapter_stats->rx_vlan_mismatch_errors =
		port_stats->rx_vlan_mismatch_errors;
	adapter_stats->rx_dropped_too_small = port_stats->rx_dropped_too_small;
	adapter_stats->rx_dropped_too_short = port_stats->rx_dropped_too_short;
	adapter_stats->rx_dropped_header_too_small =
		port_stats->rx_dropped_header_too_small;
	adapter_stats->rx_dropped_invalid_tcp_length =
		port_stats->rx_dropped_invalid_tcp_length;
	adapter_stats->rx_dropped_runt = port_stats->rx_dropped_runt;
	adapter_stats->rx_ip_checksum_errors =
		port_stats->rx_ip_checksum_errors;
	adapter_stats->rx_tcp_checksum_errors =
		port_stats->rx_tcp_checksum_errors;
	adapter_stats->rx_udp_checksum_errors =
		port_stats->rx_udp_checksum_errors;
	adapter_stats->rx_non_rss_pkts = port_stats->rx_non_rss_pkts;
	adapter_stats->rx_ipv4_pkts = port_stats->rx_ipv4_pkts;
	adapter_stats->rx_ipv6_pkts = port_stats->rx_ipv6_pkts;
	adapter_stats->rx_ipv4_bytes = port_stats->rx_ipv4_bytes;
	adapter_stats->rx_ipv6_bytes = port_stats->rx_ipv6_bytes;
	adapter_stats->rx_nic_pkts = port_stats->rx_nic_pkts;
	adapter_stats->rx_tcp_pkts = port_stats->rx_tcp_pkts;
	adapter_stats->rx_iscsi_pkts = port_stats->rx_iscsi_pkts;
	adapter_stats->rx_management_pkts = port_stats->rx_management_pkts;
	adapter_stats->rx_switched_unicast_pkts =
		port_stats->rx_switched_unicast_pkts;
	adapter_stats->rx_switched_multicast_pkts =
		port_stats->rx_switched_multicast_pkts;
	adapter_stats->rx_switched_broadcast_pkts =
		port_stats->rx_switched_broadcast_pkts;
	adapter_stats->num_forwards = port_stats->num_forwards;
	adapter_stats->rx_fifo_overflow = port_stats->rx_fifo_overflow;
	adapter_stats->rx_input_fifo_overflow =
		port_stats->rx_input_fifo_overflow;
	adapter_stats->rx_drops_too_many_frags =
		port_stats->rx_drops_too_many_frags;
	adapter_stats->rx_drops_invalid_queue =
		port_stats->rx_drops_invalid_queue;
	adapter_stats->rx_drops_mtu = port_stats->rx_drops_mtu;
	adapter_stats->rx_pkts_64_bytes = port_stats->rx_pkts_64_bytes;
	adapter_stats->rx_pkts_65_to_127_bytes =
		port_stats->rx_pkts_65_to_127_bytes;
	adapter_stats->rx_pkts_128_to_255_bytes =
		port_stats->rx_pkts_128_to_255_bytes;
	adapter_stats->rx_pkts_256_to_511_bytes =
		port_stats->rx_pkts_256_to_511_bytes;
	adapter_stats->rx_pkts_512_to_1023_bytes =
		port_stats->rx_pkts_512_to_1023_bytes;
	adapter_stats->rx_pkts_1024_to_1518_bytes =
		port_stats->rx_pkts_1024_to_1518_bytes;
	adapter_stats->rx_pkts_1519_to_2047_bytes =
		port_stats->rx_pkts_1519_to_2047_bytes;
	adapter_stats->rx_pkts_2048_to_4095_bytes =
		port_stats->rx_pkts_2048_to_4095_bytes;
	adapter_stats->rx_pkts_4096_to_8191_bytes =
		port_stats->rx_pkts_4096_to_8191_bytes;
	adapter_stats->rx_pkts_8192_to_9216_bytes =
		port_stats->rx_pkts_8192_to_9216_bytes;
}

void
copy_stats_to_sc_be2(struct oce_softc *sc)
{
	struct oce_be_stats *adapter_stats;
	struct oce_pmem_stats *pmem;
	struct oce_rxf_stats_v0 *rxf_stats;
	struct oce_port_rxf_stats_v0 *port_stats;
	struct mbx_get_nic_stats_v0 *nic_mbx;
	uint32_t port = sc->port_id;

	nic_mbx = OCE_DMAPTR(&sc->stats_mem, struct mbx_get_nic_stats_v0);
	pmem = &nic_mbx->params.rsp.stats.pmem;
	rxf_stats = &nic_mbx->params.rsp.stats.rxf;
	port_stats = &nic_mbx->params.rsp.stats.rxf.port[port];

	adapter_stats = &sc->oce_stats_info.u0.be;

	/* Update stats */
	adapter_stats->rx_pause_frames = port_stats->rx_pause_frames;
	adapter_stats->rx_crc_errors = port_stats->rx_crc_errors;
	adapter_stats->rx_control_frames = port_stats->rx_control_frames;
	adapter_stats->rx_in_range_errors = port_stats->rx_in_range_errors;
	adapter_stats->rx_frame_too_long = port_stats->rx_frame_too_long;
	adapter_stats->rx_dropped_runt = port_stats->rx_dropped_runt;
	adapter_stats->rx_ip_checksum_errs = port_stats->rx_ip_checksum_errs;
	adapter_stats->rx_tcp_checksum_errs = port_stats->rx_tcp_checksum_errs;
	adapter_stats->rx_udp_checksum_errs = port_stats->rx_udp_checksum_errs;
	adapter_stats->rxpp_fifo_overflow_drop =
					port_stats->rxpp_fifo_overflow_drop;
	adapter_stats->rx_dropped_tcp_length =
		port_stats->rx_dropped_tcp_length;
	adapter_stats->rx_dropped_too_small = port_stats->rx_dropped_too_small;
	adapter_stats->rx_dropped_too_short = port_stats->rx_dropped_too_short;
	adapter_stats->rx_out_range_errors = port_stats->rx_out_range_errors;
	adapter_stats->rx_dropped_header_too_small =
		port_stats->rx_dropped_header_too_small;
	adapter_stats->rx_input_fifo_overflow_drop =
		port_stats->rx_input_fifo_overflow_drop;
	adapter_stats->rx_address_match_errors =
		port_stats->rx_address_match_errors;
	adapter_stats->rx_alignment_symbol_errors =
		port_stats->rx_alignment_symbol_errors;
	adapter_stats->tx_pauseframes = port_stats->tx_pauseframes;
	adapter_stats->tx_controlframes = port_stats->tx_controlframes;

	if (sc->if_id)
		adapter_stats->jabber_events = rxf_stats->port1_jabber_events;
	else
		adapter_stats->jabber_events = rxf_stats->port0_jabber_events;

	adapter_stats->rx_drops_no_pbuf = rxf_stats->rx_drops_no_pbuf;
	adapter_stats->rx_drops_no_txpb = rxf_stats->rx_drops_no_txpb;
	adapter_stats->rx_drops_no_erx_descr = rxf_stats->rx_drops_no_erx_descr;
	adapter_stats->rx_drops_invalid_ring = rxf_stats->rx_drops_invalid_ring;
	adapter_stats->forwarded_packets = rxf_stats->forwarded_packets;
	adapter_stats->rx_drops_mtu = rxf_stats->rx_drops_mtu;
	adapter_stats->rx_drops_no_tpre_descr =
		rxf_stats->rx_drops_no_tpre_descr;
	adapter_stats->rx_drops_too_many_frags =
		rxf_stats->rx_drops_too_many_frags;
	adapter_stats->eth_red_drops = pmem->eth_red_drops;
}

void
copy_stats_to_sc_be3(struct oce_softc *sc)
{
	struct oce_be_stats *adapter_stats;
	struct oce_pmem_stats *pmem;
	struct oce_rxf_stats_v1 *rxf_stats;
	struct oce_port_rxf_stats_v1 *port_stats;
	struct mbx_get_nic_stats *nic_mbx;
	uint32_t port = sc->port_id;

	nic_mbx = OCE_DMAPTR(&sc->stats_mem, struct mbx_get_nic_stats);
	pmem = &nic_mbx->params.rsp.stats.pmem;
	rxf_stats = &nic_mbx->params.rsp.stats.rxf;
	port_stats = &nic_mbx->params.rsp.stats.rxf.port[port];

	adapter_stats = &sc->oce_stats_info.u0.be;

	/* Update stats */
	adapter_stats->pmem_fifo_overflow_drop =
		port_stats->pmem_fifo_overflow_drop;
	adapter_stats->rx_priority_pause_frames =
		port_stats->rx_priority_pause_frames;
	adapter_stats->rx_pause_frames = port_stats->rx_pause_frames;
	adapter_stats->rx_crc_errors = port_stats->rx_crc_errors;
	adapter_stats->rx_control_frames = port_stats->rx_control_frames;
	adapter_stats->rx_in_range_errors = port_stats->rx_in_range_errors;
	adapter_stats->rx_frame_too_long = port_stats->rx_frame_too_long;
	adapter_stats->rx_dropped_runt = port_stats->rx_dropped_runt;
	adapter_stats->rx_ip_checksum_errs = port_stats->rx_ip_checksum_errs;
	adapter_stats->rx_tcp_checksum_errs = port_stats->rx_tcp_checksum_errs;
	adapter_stats->rx_udp_checksum_errs = port_stats->rx_udp_checksum_errs;
	adapter_stats->rx_dropped_tcp_length =
		port_stats->rx_dropped_tcp_length;
	adapter_stats->rx_dropped_too_small = port_stats->rx_dropped_too_small;
	adapter_stats->rx_dropped_too_short = port_stats->rx_dropped_too_short;
	adapter_stats->rx_out_range_errors = port_stats->rx_out_range_errors;
	adapter_stats->rx_dropped_header_too_small =
		port_stats->rx_dropped_header_too_small;
	adapter_stats->rx_input_fifo_overflow_drop =
		port_stats->rx_input_fifo_overflow_drop;
	adapter_stats->rx_address_match_errors =
		port_stats->rx_address_match_errors;
	adapter_stats->rx_alignment_symbol_errors =
		port_stats->rx_alignment_symbol_errors;
	adapter_stats->rxpp_fifo_overflow_drop =
		port_stats->rxpp_fifo_overflow_drop;
	adapter_stats->tx_pauseframes = port_stats->tx_pauseframes;
	adapter_stats->tx_controlframes = port_stats->tx_controlframes;
	adapter_stats->jabber_events = port_stats->jabber_events;

	adapter_stats->rx_drops_no_pbuf = rxf_stats->rx_drops_no_pbuf;
	adapter_stats->rx_drops_no_txpb = rxf_stats->rx_drops_no_txpb;
	adapter_stats->rx_drops_no_erx_descr = rxf_stats->rx_drops_no_erx_descr;
	adapter_stats->rx_drops_invalid_ring = rxf_stats->rx_drops_invalid_ring;
	adapter_stats->forwarded_packets = rxf_stats->forwarded_packets;
	adapter_stats->rx_drops_mtu = rxf_stats->rx_drops_mtu;
	adapter_stats->rx_drops_no_tpre_descr =
		rxf_stats->rx_drops_no_tpre_descr;
	adapter_stats->rx_drops_too_many_frags =
		rxf_stats->rx_drops_too_many_frags;

	adapter_stats->eth_red_drops = pmem->eth_red_drops;
}

int
oce_stats_init(struct oce_softc *sc)
{
	int rc = 0, sz;

	if (IS_BE(sc)) {
		if (sc->flags & OCE_FLAGS_BE2)
			sz = sizeof(struct mbx_get_nic_stats_v0);
		else
			sz = sizeof(struct mbx_get_nic_stats);
	} else
		sz = sizeof(struct mbx_get_pport_stats);

	rc = oce_dma_alloc(sc, sz, &sc->stats_mem, 0);

	return rc;
}

void
oce_stats_free(struct oce_softc *sc)
{
	oce_dma_free(sc, &sc->stats_mem);
}

int
oce_refresh_nic_stats(struct oce_softc *sc)
{
	int rc = 0, reset = 0;

	if (IS_BE(sc)) {
		if (sc->flags & OCE_FLAGS_BE2) {
			rc = oce_mbox_get_nic_stats_v0(sc, &sc->stats_mem);
			if (!rc)
				copy_stats_to_sc_be2(sc);
		} else {
			rc = oce_mbox_get_nic_stats(sc, &sc->stats_mem);
			if (!rc)
				copy_stats_to_sc_be3(sc);
		}

	} else {
		rc = oce_mbox_get_pport_stats(sc, &sc->stats_mem, reset);
		if (!rc)
			copy_stats_to_sc_xe201(sc);
	}

	return rc;
}
