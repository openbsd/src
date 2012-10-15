/*	$OpenBSD: oce.c,v 1.13 2012/10/15 19:23:23 mikeb Exp $	*/

/*
 * Copyright (c) 2012 Mike Belopuhov
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

int oce_mbox_wait(struct oce_softc *sc);
int oce_mbox_dispatch(struct oce_softc *sc);

int oce_fw(struct oce_softc *sc, int subsys, int opcode, int version,
    void *payload, int length);

int oce_config_vlan(struct oce_softc *sc, uint32_t if_id,
    struct normal_vlan *vtag_arr, uint8_t vtag_cnt, uint32_t untagged,
    uint32_t enable_promisc);
int oce_set_flow_control(struct oce_softc *sc, uint32_t flow_control);
int oce_rss_itbl_init(struct oce_softc *sc, struct mbx_config_nic_rss *fwcmd);

int oce_set_common_iface_rx_filter(struct oce_softc *sc,
    struct oce_dma_mem *sgl);

int oce_mbox_get_nic_stats_v0(struct oce_softc *sc, void *buf);
int oce_mbox_get_nic_stats(struct oce_softc *sc, void *buf);
int oce_mbox_get_pport_stats(struct oce_softc *sc, void *buf,
    uint32_t reset_stats);

/**
 * @brief Wait for FW to become ready and reset it
 * @param sc		software handle to the device
 */
int
oce_init_fw(struct oce_softc *sc)
{
	struct ioctl_common_function_reset fwcmd;
	mpu_ep_semaphore_t post_status;
	int tmo = 60000;
	int err = 0;

	/* read semaphore CSR */
	post_status.dw0 = OCE_READ_REG32(sc, csr, MPU_EP_SEMAPHORE(sc));

	/* if host is ready then wait for fw ready else send POST */
	if (post_status.bits.stage <= POST_STAGE_AWAITING_HOST_RDY) {
		post_status.bits.stage = POST_STAGE_CHIP_RESET;
		OCE_WRITE_REG32(sc, csr, MPU_EP_SEMAPHORE(sc), post_status.dw0);
	}

	/* wait for FW to become ready */
	for (;;) {
		if (--tmo == 0)
			break;

		DELAY(1000);

		post_status.dw0 = OCE_READ_REG32(sc, csr, MPU_EP_SEMAPHORE(sc));
		if (post_status.bits.error) {
			printf(": POST failed: %x\n", post_status.dw0);
			return ENXIO;
		}
		if (post_status.bits.stage == POST_STAGE_ARMFW_READY) {
			/* reset FW */
			bzero(&fwcmd, sizeof(fwcmd));
			if (sc->flags & OCE_FLAGS_FUNCRESET_RQD)
				err = oce_fw(sc, MBX_SUBSYSTEM_COMMON,
				    OPCODE_COMMON_FUNCTION_RESET, OCE_MBX_VER_V0,
				    &fwcmd, sizeof(fwcmd));
			return (err);
		}
	}

	printf(": POST timed out: %x\n", post_status.dw0);

	return ENXIO;
}

/**
 * @brief	Allocate PCI resources.
 *
 * @param sc		software handle to the device
 * @returns		0 if successful, or error
 */
/**
 * @brief		Function for creating nw interface.
 * @param sc		software handle to the device
 * @returns		0 on success, error otherwise
 */
int
oce_create_iface(struct oce_softc *sc, uint8_t *macaddr)
{
	struct mbx_create_common_iface fwcmd;
	uint32_t capab_flags, capab_en_flags;
	int err = 0;

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

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.version = 0;
	fwcmd.params.req.cap_flags = htole32(capab_flags);
	fwcmd.params.req.enable_flags = htole32(capab_en_flags);
	if (macaddr != NULL) {
		bcopy(macaddr, &fwcmd.params.req.mac_addr[0], ETH_ADDR_LEN);
		fwcmd.params.req.mac_invalid = 0;
	} else
		fwcmd.params.req.mac_invalid = 1;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_CREATE_IFACE,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err)
		return (err);

	sc->if_id = letoh32(fwcmd.params.rsp.if_id);

	if (macaddr != NULL)
		sc->pmac_id = letoh32(fwcmd.params.rsp.pmac_id);

	sc->nifs++;

	sc->if_cap_flags = capab_en_flags;

	/* Enable VLAN Promisc on HW */
	err = oce_config_vlan(sc, (uint8_t)sc->if_id, NULL, 0, 1, 1);
	if (err)
		return (err);

	/* set default flow control */
	err = oce_set_flow_control(sc, sc->flow_control);
	if (err)
		return (err);

	return 0;
}

/**
 * @brief Mailbox wait
 * @param sc		software handle to the device
 */
int
oce_mbox_wait(struct oce_softc *sc)
{
	pd_mpu_mbox_db_t mbox_db;
	int i;

	for (i = 0; i < 20000; i++) {
		mbox_db.dw0 = OCE_READ_REG32(sc, db, PD_MPU_MBOX_DB);
		if (mbox_db.bits.ready)
			return (0);
		DELAY(100);
	}

	printf("%s: Mailbox timed out\n", sc->dev.dv_xname);

	return (ETIMEDOUT);
}

/**
 * @brief Mailbox dispatch
 * @param sc		software handle to the device
 */
int
oce_mbox_dispatch(struct oce_softc *sc)
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

	rc = oce_mbox_wait(sc);
	if (rc == 0) {
		OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

		pa = (uint32_t) ((uint64_t) sc->bsmbx.paddr >> 4) & 0x3fffffff;
		mbox_db.bits.ready = 0;
		mbox_db.bits.hi = 0;
		mbox_db.bits.address = pa;

		rc = oce_mbox_wait(sc);

		if (rc == 0) {
			OCE_WRITE_REG32(sc, db, PD_MPU_MBOX_DB, mbox_db.dw0);

			rc = oce_mbox_wait(sc);

			oce_dma_sync(&sc->bsmbx, BUS_DMASYNC_POSTWRITE);
		}
	}

	return rc;
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

		ret = oce_mbox_dispatch(sc);
	}

	return ret;
}

/**
 * @brief Function to initialize the hw with host endian information
 * @param sc		software handle to the device
 * @returns		0 on success, !0 on failure
 */
int
oce_fw(struct oce_softc *sc, int subsys, int opcode, int version,
    void *payload, int length)
{
	struct oce_bmbx *bmbx = OCE_DMAPTR(&sc->bsmbx, struct oce_bmbx);
	struct oce_mbx *mbx = &bmbx->mbx;
	struct oce_dma_mem sgl;
	struct mbx_hdr *hdr;
	caddr_t epayload = NULL;
	int err, embed = 1;

	if (length > OCE_MBX_PAYLOAD) {
		embed = 0;
		if (oce_dma_alloc(sc, length, &sgl))
			return (-1);
		epayload = OCE_DMAPTR(&sgl, char);
	}

	bzero(mbx, sizeof(struct oce_mbx));

	mbx->payload_length = length;
	mbx->tag[0] = 0;	/* ??? */

	if (embed) {
		mbx->u0.s.embedded = 1;
		bcopy(payload, &mbx->payload, length);
		hdr = (struct mbx_hdr *)&mbx->payload;
	} else {
		mbx->u0.s.sge_count = 1;
		oce_dma_sync(&sgl, BUS_DMASYNC_PREWRITE);
		bcopy(payload, epayload, length);
		mbx->payload.u0.u1.sgl[0].pa_lo = ADDR_LO(sgl.paddr);
		mbx->payload.u0.u1.sgl[0].pa_lo = ADDR_HI(sgl.paddr);
		mbx->payload.u0.u1.sgl[0].length = length;
		hdr = OCE_DMAPTR(&sgl, struct mbx_hdr);
	}

	hdr->u0.req.opcode = opcode;
	hdr->u0.req.subsystem = subsys;
	hdr->u0.req.request_length = length - sizeof(*hdr);
	hdr->u0.req.version = version;
	if (opcode == OPCODE_COMMON_FUNCTION_RESET)
		hdr->u0.req.timeout = 2 * MBX_TIMEOUT_SEC;
	else
		hdr->u0.req.timeout = MBX_TIMEOUT_SEC;

	err = oce_mbox_dispatch(sc);
	if (err == 0) {
		if (!embed) {
			oce_dma_sync(&sgl, BUS_DMASYNC_POSTWRITE);
			bcopy(epayload, payload, length);
		} else
			bcopy(&mbx->payload, payload, length);
	} else
		printf("%s: %s: error %d\n", sc->dev.dv_xname, __func__, err);
	if (!embed)
		oce_dma_free(sc, &sgl);
	return (err);
}

/**
 * @brief Function to query the fw attributes from the hw
 * @param sc		software handle to the device
 * @returns		0 on success, EIO on failure
 */
int
oce_get_fw_config(struct oce_softc *sc)
{
	struct mbx_common_query_fw_config fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON,
	    OPCODE_COMMON_QUERY_FIRMWARE_CONFIG, OCE_MBX_VER_V0, &fwcmd,
	    sizeof(fwcmd));
	if (err)
		return (err);

	sc->port_id	  = fwcmd.params.rsp.port_id;
	sc->function_mode = fwcmd.params.rsp.function_mode;
	sc->function_caps = fwcmd.params.rsp.function_caps;

	if (fwcmd.params.rsp.ulp[0].ulp_mode & ULP_NIC_MODE) {
		sc->max_tx_rings = fwcmd.params.rsp.ulp[0].nic_wq_tot;
		sc->max_rx_rings = fwcmd.params.rsp.ulp[0].lro_rqid_tot;
	} else {
		sc->max_tx_rings = fwcmd.params.rsp.ulp[1].nic_wq_tot;
		sc->max_rx_rings = fwcmd.params.rsp.ulp[1].lro_rqid_tot;
	}

	return (0);
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
	struct mbx_hdr *hdr;
	struct mbx_get_common_fw_version *fwcmd;
	uint32_t reg_value;

	mbx = RING_GET_PRODUCER_ITEM_VA(mq->ring, struct oce_mbx);
	bzero(mbx, sizeof(struct oce_mbx));

	fwcmd = (struct mbx_get_common_fw_version *)&mbx->payload;

	hdr = &fwcmd->hdr;
	hdr->u0.req.subsystem = MBX_SUBSYSTEM_COMMON;
	hdr->u0.req.opcode = OPCODE_COMMON_GET_FW_VERSION;
	hdr->u0.req.version = OCE_MBX_VER_V0;
	hdr->u0.req.timeout = MBX_TIMEOUT_SEC;
	hdr->u0.req.request_length = sizeof(*fwcmd) - sizeof(*hdr);

	mbx->u0.s.embedded = 1;
	mbx->payload_length = sizeof(*fwcmd);
	oce_dma_sync(&mq->ring->dma, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);
	RING_PUT(mq->ring, 1);
	reg_value = (1 << 16) | mq->id;
	OCE_WRITE_REG32(sc, db, PD_MQ_DB, reg_value);

	return 0;
}

int
oce_read_macaddr(struct oce_softc *sc, uint8_t *macaddr)
{
	struct mbx_query_common_iface_mac fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.type = MAC_ADDRESS_TYPE_NETWORK;
	fwcmd.params.req.permanent = 1;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_QUERY_IFACE_MAC,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err == 0)
		bcopy(&fwcmd.params.rsp.mac.mac_addr[0], macaddr, ETH_ADDR_LEN);
	return (err);
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
	struct mbx_common_config_vlan fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.if_id = (uint8_t) if_id;
	fwcmd.params.req.promisc = (uint8_t) enable_promisc;
	fwcmd.params.req.untagged = (uint8_t) untagged;
	fwcmd.params.req.num_vlans = vtag_cnt;

	if (!enable_promisc)
		bcopy(vtag_arr, fwcmd.params.req.tags.normal_vlans,
			vtag_cnt * sizeof(struct normal_vlan));

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_CONFIG_IFACE_VLAN,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	return (err);
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
	struct mbx_common_get_set_flow_control fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	if (flow_control & OCE_FC_TX)
		fwcmd.tx_flow_control = 1;

	if (flow_control & OCE_FC_RX)
		fwcmd.rx_flow_control = 1;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_SET_FLOW_CONTROL,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	return (err);
}

#ifdef OCE_RSS
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
		fwcmd->params.req.cpu_tbl_sz_log2 = htole16(ilog2(i));

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
	struct mbx_config_nic_rss fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	if (enable_rss)
		fwcmd.params.req.enable_rss = RSS_ENABLE_IPV4 |
		    RSS_ENABLE_TCP_IPV4 | RSS_ENABLE_IPV6 |
		    RSS_ENABLE_TCP_IPV6);
	fwcmd.params.req.flush = OCE_FLUSH;
	fwcmd.params.req.if_id = htole32(if_id);

	arc4random_buf(fwcmd.params.req.hash, sizeof(fwcmd.params.req.hash));

	err = oce_rss_itbl_init(sc, &fwcmd);
	if (err)
		return (err);

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_CONFIG_RSS,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	return (err);
}
#endif	/* OCE_RSS */

/**
 * @brief		Function for hardware update multicast filter
 * @param sc		software handle to the device
 * @param multi		table of multicast addresses
 * @param naddr		number of multicast addresses in the table
 */
int
oce_update_mcast(struct oce_softc *sc,
    uint8_t multi[][ETH_ADDR_LEN], int naddr)
{
	struct mbx_set_common_iface_multicast fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	bcopy(&multi[0], &fwcmd.params.req.mac[0], naddr * ETH_ADDR_LEN);
	fwcmd.params.req.num_mac = htole16(naddr);
	fwcmd.params.req.if_id = sc->if_id;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON,
	    OPCODE_COMMON_SET_IFACE_MULTICAST, OCE_MBX_VER_V0,
	    &fwcmd, sizeof(fwcmd));
	return (err);
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
oce_set_promisc(struct oce_softc *sc, uint32_t enable)
{
	struct mbx_set_common_iface_rx_filter fwcmd;
	struct iface_rx_filter_ctx *req;
	int rc;

	req = &fwcmd.params.req;
	req->if_id = sc->if_id;
	req->iface_flags_mask = MBX_RX_IFACE_FLAGS_PROMISCUOUS |
				MBX_RX_IFACE_FLAGS_VLAN_PROMISCUOUS;
	if (enable)
		req->iface_flags = req->iface_flags_mask;

	rc = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_SET_IFACE_RX_FILTER,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));

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
	struct mbx_query_common_link_config fwcmd;
	struct link_status link;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_QUERY_LINK_CONFIG,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err)
		return (err);

	bcopy(&fwcmd.params.rsp, &link, sizeof(struct link_status));
	link.logical_link_status = letoh32(link.logical_link_status);
	link.qos_link_speed = letoh16(link.qos_link_speed);

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

	return (0);
}

int
oce_macaddr_add(struct oce_softc *sc, uint8_t *enaddr, uint32_t if_id,
    uint32_t *pmac_id)
{
	struct mbx_add_common_iface_mac fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.if_id = htole16(if_id);
	bcopy(enaddr, fwcmd.params.req.mac_address, ETH_ADDR_LEN);

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_ADD_IFACE_MAC,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err == 0)
		*pmac_id = letoh32(fwcmd.params.rsp.pmac_id);
	return (err);
}

int
oce_macaddr_del(struct oce_softc *sc, uint32_t if_id, uint32_t pmac_id)
{
	struct mbx_del_common_iface_mac fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.if_id = htole16(if_id);
	fwcmd.params.req.pmac_id = htole32(pmac_id);

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_DEL_IFACE_MAC,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	return (err);
}

int
oce_check_native_mode(struct oce_softc *sc)
{
	struct mbx_common_set_function_cap fwcmd;
	int err;

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.valid_capability_flags = CAP_SW_TIMESTAMPS |
	    CAP_BE3_NATIVE_ERX_API;
	fwcmd.params.req.capability_flags = CAP_BE3_NATIVE_ERX_API;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON,
	    OPCODE_COMMON_SET_FUNCTIONAL_CAPS, OCE_MBX_VER_V0, &fwcmd,
	    sizeof(fwcmd));
	if (err)
		return (err);

	sc->be3_native = fwcmd.params.rsp.capability_flags &
	    CAP_BE3_NATIVE_ERX_API;

	return (0);
}

int
oce_new_rq(struct oce_softc *sc, struct oce_rq *rq)
{
	struct mbx_create_nic_rq fwcmd;
	int err, npages;

	bzero(&fwcmd, sizeof(fwcmd));

	npages = oce_load_ring(sc, rq->ring, &fwcmd.params.req.pages[0],
	    nitems(fwcmd.params.req.pages));
	if (!npages) {
		printf("%s: failed to load the rq ring\n", __func__);
		return (1);
	}

	if (IS_XE201(sc)) {
		fwcmd.params.req.frag_size = rq->cfg.frag_size / 2048;
		fwcmd.params.req.page_size = 1;
	} else
		fwcmd.params.req.frag_size = ilog2(rq->cfg.frag_size);
	fwcmd.params.req.num_pages = npages;
	fwcmd.params.req.cq_id = rq->cq->id;
	fwcmd.params.req.if_id = htole32(sc->if_id);
	fwcmd.params.req.max_frame_size = htole16(rq->cfg.mtu);
	fwcmd.params.req.is_rss_queue = htole32(rq->cfg.is_rss_queue);

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_CREATE_RQ,
	    IS_XE201(sc) ? OCE_MBX_VER_V1 : OCE_MBX_VER_V0, &fwcmd,
	    sizeof(fwcmd));
	if (err)
		return (err);

	rq->id = letoh16(fwcmd.params.rsp.rq_id);
	rq->rss_cpuid = fwcmd.params.rsp.rss_cpuid;

	return (0);
}

int
oce_new_wq(struct oce_softc *sc, struct oce_wq *wq)
{
	struct mbx_create_nic_wq fwcmd;
	int err, npages;

	bzero(&fwcmd, sizeof(fwcmd));

	npages = oce_load_ring(sc, wq->ring, &fwcmd.params.req.pages[0],
	    nitems(fwcmd.params.req.pages));
	if (!npages) {
		printf("%s: failed to load the wq ring\n", __func__);
		return (1);
	}

	if (IS_XE201(sc))
		fwcmd.params.req.if_id = sc->if_id;
	fwcmd.params.req.nic_wq_type = wq->cfg.wq_type;
	fwcmd.params.req.num_pages = npages;
	fwcmd.params.req.wq_size = ilog2(wq->cfg.q_len) + 1;
	fwcmd.params.req.cq_id = htole16(wq->cq->id);
	fwcmd.params.req.ulp_num = 1;

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_CREATE_WQ,
	    IS_XE201(sc) ? OCE_MBX_VER_V1 : OCE_MBX_VER_V0, &fwcmd,
	    sizeof(fwcmd));
	if (err)
		return (err);

	wq->id = letoh16(fwcmd.params.rsp.wq_id);

	return (0);
}

int
oce_new_mq(struct oce_softc *sc, struct oce_mq *mq)
{
	struct mbx_create_common_mq_ex fwcmd;
	union oce_mq_ext_ctx *ctx;
	int err, npages;

	bzero(&fwcmd, sizeof(fwcmd));

	npages = oce_load_ring(sc, mq->ring, &fwcmd.params.req.pages[0],
	    nitems(fwcmd.params.req.pages));
	if (!npages) {
		printf("%s: failed to load the mq ring\n", __func__);
		return (-1);
	}

	ctx = &fwcmd.params.req.context;
	ctx->v0.num_pages = npages;
	ctx->v0.cq_id = mq->cq->id;
	ctx->v0.ring_size = ilog2(mq->cfg.q_len) + 1;
	ctx->v0.valid = 1;
	/* Subscribe to Link State and Group 5 Events(bits 1 and 5 set) */
	ctx->v0.async_evt_bitmap = 0xffffffff;

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_CREATE_MQ_EXT,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err)
		return (err);

	mq->id = letoh16(fwcmd.params.rsp.mq_id);

	return (0);
}

int
oce_new_eq(struct oce_softc *sc, struct oce_eq *eq)
{
	struct mbx_create_common_eq fwcmd;
	int err, npages;

	bzero(&fwcmd, sizeof(fwcmd));

	npages = oce_load_ring(sc, eq->ring, &fwcmd.params.req.pages[0],
	    nitems(fwcmd.params.req.pages));
	if (!npages) {
		printf("%s: failed to load the eq ring\n", __func__);
		return (-1);
	}

	fwcmd.params.req.ctx.num_pages = htole16(npages);
	fwcmd.params.req.ctx.valid = 1;
	fwcmd.params.req.ctx.size = (eq->cfg.item_size == 4) ? 0 : 1;
	fwcmd.params.req.ctx.count = ilog2(eq->cfg.q_len / 256);
	fwcmd.params.req.ctx.armed = 0;
	fwcmd.params.req.ctx.delay_mult = htole32(eq->cfg.cur_eqd);

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_CREATE_EQ,
	    OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	if (err)
		return (err);

	eq->id = letoh16(fwcmd.params.rsp.eq_id);

	return (0);
}

int
oce_new_cq(struct oce_softc *sc, struct oce_cq *cq)
{
	struct mbx_create_common_cq fwcmd;
	union oce_cq_ctx *ctx;
	int err, npages;

	bzero(&fwcmd, sizeof(fwcmd));

	npages = oce_load_ring(sc, cq->ring, &fwcmd.params.req.pages[0],
	    nitems(fwcmd.params.req.pages));
	if (!npages) {
		printf("%s: failed to load the cq ring\n", __func__);
		return (-1);
	}

	ctx = &fwcmd.params.req.cq_ctx;

	if (IS_XE201(sc)) {
		ctx->v2.num_pages = htole16(npages);
		ctx->v2.page_size = 1; /* for 4K */
		ctx->v2.eventable = cq->cfg.eventable;
		ctx->v2.valid = 1;
		ctx->v2.count = ilog2(cq->cfg.q_len / 256);
		ctx->v2.nodelay = cq->cfg.nodelay;
		ctx->v2.coalesce_wm = cq->cfg.ncoalesce;
		ctx->v2.armed = 0;
		ctx->v2.eq_id = cq->eq->id;
		if (ctx->v2.count == 3) {
			if (cq->cfg.q_len > (4*1024)-1)
				ctx->v2.cqe_count = (4*1024)-1;
			else
				ctx->v2.cqe_count = cq->cfg.q_len;
		}
	} else {
		ctx->v0.num_pages = htole16(npages);
		ctx->v0.eventable = cq->cfg.eventable;
		ctx->v0.valid = 1;
		ctx->v0.count = ilog2(cq->cfg.q_len / 256);
		ctx->v0.nodelay = cq->cfg.nodelay;
		ctx->v0.coalesce_wm = cq->cfg.ncoalesce;
		ctx->v0.armed = 0;
		ctx->v0.eq_id = cq->eq->id;
	}

	err = oce_fw(sc, MBX_SUBSYSTEM_COMMON, OPCODE_COMMON_CREATE_CQ,
	    IS_XE201(sc) ? OCE_MBX_VER_V2 : OCE_MBX_VER_V0, &fwcmd,
	    sizeof(fwcmd));
	if (err)
		return (err);

	cq->id = letoh16(fwcmd.params.rsp.cq_id);

	return (0);
}

int
oce_destroy_queue(struct oce_softc *sc, enum qtype qtype, uint32_t qid)
{
	struct mbx_destroy_common_mq fwcmd;
	int opcode, subsys, err;

	switch (qtype) {
	case QTYPE_CQ:
		opcode = OPCODE_COMMON_DESTROY_CQ;
		subsys = MBX_SUBSYSTEM_COMMON;
		break;
	case QTYPE_EQ:
		opcode = OPCODE_COMMON_DESTROY_EQ;
		subsys = MBX_SUBSYSTEM_COMMON;
		break;
	case QTYPE_MQ:
		opcode = OPCODE_COMMON_DESTROY_MQ;
		subsys = MBX_SUBSYSTEM_COMMON;
		break;
	case QTYPE_RQ:
		opcode = OPCODE_NIC_DELETE_RQ;
		subsys = MBX_SUBSYSTEM_NIC;
		break;
	case QTYPE_WQ:
		opcode = OPCODE_NIC_DELETE_WQ;
		subsys = MBX_SUBSYSTEM_NIC;
		break;
	default:
		return (EINVAL);
	}

	bzero(&fwcmd, sizeof(fwcmd));

	fwcmd.params.req.id = htole16(qid);

	err = oce_fw(sc, subsys, opcode, OCE_MBX_VER_V0, &fwcmd, sizeof(fwcmd));
	return (err);
}

/**
 * @brief Function to get NIC statistics for BE2 devices
 * @param sc 		software handle to the device
 * @param *buf		pointer to where to store statistics
 * @returns		0 on success, EIO on failure
 * @note		command depricated in Lancer
 */
int
oce_mbox_get_nic_stats_v0(struct oce_softc *sc, void *buf)
{
	struct mbx_get_nic_stats_v0 *fwcmd = buf;
	int err;

	bzero(fwcmd, sizeof(*fwcmd));

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_GET_STATS,
	    OCE_MBX_VER_V0, fwcmd, sizeof(*fwcmd));
	return (err);
}

/**
 * @brief Function to get NIC statistics for BE3 devices
 * @param sc 		software handle to the device
 * @param *buf		pointer to where to store statistics
 * @returns		0 on success, EIO on failure
 * @note		command depricated in Lancer
 */
int
oce_mbox_get_nic_stats(struct oce_softc *sc, void *buf)
{
	struct mbx_get_nic_stats *fwcmd = buf;
	int err;

	bzero(fwcmd, sizeof(*fwcmd));

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_GET_STATS,
	    OCE_MBX_VER_V1, fwcmd, sizeof(*fwcmd));
	return (err);
}

/**
 * @brief Function to get pport (physical port) statistics
 * @param sc 		software handle to the device
 * @param *buf		pointer to where to store statistics
 * @param reset_stats	resets statistics of set
 * @returns		0 on success, EIO on failure
 */
int
oce_mbox_get_pport_stats(struct oce_softc *sc, void *buf, uint32_t reset_stats)
{
	struct mbx_get_pport_stats *fwcmd = buf;
	int err;

	bzero(fwcmd, sizeof(*fwcmd));

	fwcmd->params.req.reset_stats = reset_stats;
	fwcmd->params.req.port_number = sc->if_id;

	err = oce_fw(sc, MBX_SUBSYSTEM_NIC, OPCODE_NIC_GET_PPORT_STATS,
	    OCE_MBX_VER_V0, fwcmd, sizeof(*fwcmd));
	return (err);
}

static inline void
update_stats_xe(struct oce_softc *sc, u_int64_t *rxe, u_int64_t *txe)
{
	struct mbx_get_pport_stats *mbx;
	struct oce_pport_stats *pps;

	mbx = &sc->stats.xe;
	mbx = (struct mbx_get_pport_stats *)&sc->stats;
	pps = &mbx->params.rsp.pps;

	*rxe = pps->rx_discards + pps->rx_errors + pps->rx_crc_errors +
	    pps->rx_alignment_errors + pps->rx_symbol_errors +
	    pps->rx_frames_too_long + pps->rx_internal_mac_errors +
	    pps->rx_undersize_pkts + pps->rx_oversize_pkts + pps->rx_jabbers +
	    pps->rx_control_frames_unknown_opcode + pps->rx_in_range_errors +
	    pps->rx_out_of_range_errors + pps->rx_ip_checksum_errors +
	    pps->rx_tcp_checksum_errors + pps->rx_udp_checksum_errors +
	    pps->rx_fifo_overflow + pps->rx_input_fifo_overflow +
	    pps->rx_drops_too_many_frags + pps->rx_drops_mtu;

	*txe = pps->tx_discards + pps->tx_errors + pps->tx_internal_mac_errors;
}

static inline void
update_stats_be2(struct oce_softc *sc, u_int64_t *rxe, u_int64_t *txe)
{
	struct mbx_get_nic_stats_v0 *mbx;
	struct oce_pmem_stats *ms;
	struct oce_rxf_stats_v0 *rs;
	struct oce_port_rxf_stats_v0 *ps;

	mbx = &sc->stats.be2;
	ms = &mbx->params.rsp.stats.pmem;
	rs = &mbx->params.rsp.stats.rxf;
	ps = &rs->port[sc->port_id];

	*rxe = ps->rx_crc_errors + ps->rx_in_range_errors +
	    ps->rx_frame_too_long + ps->rx_dropped_runt +
	    ps->rx_ip_checksum_errs + ps->rx_tcp_checksum_errs +
	    ps->rx_udp_checksum_errs + ps->rxpp_fifo_overflow_drop +
	    ps->rx_dropped_tcp_length + ps->rx_dropped_too_small +
	    ps->rx_dropped_too_short + ps->rx_out_range_errors +
	    ps->rx_dropped_header_too_small + ps->rx_input_fifo_overflow_drop +
	    ps->rx_alignment_symbol_errors;
	if (sc->if_id)
		*rxe += rs->port1_jabber_events;
	else
		*rxe += rs->port0_jabber_events;
	*rxe += ms->eth_red_drops;

	*txe = 0; /* hardware doesn't provide any extra tx error statistics */
}

static inline void
update_stats_be3(struct oce_softc *sc, u_int64_t *rxe, u_int64_t *txe)
{
	struct mbx_get_nic_stats *mbx;
	struct oce_pmem_stats *ms;
	struct oce_rxf_stats_v1 *rs;
	struct oce_port_rxf_stats_v1 *ps;

	mbx = &sc->stats.be3;
	ms = &mbx->params.rsp.stats.pmem;
	rs = &mbx->params.rsp.stats.rxf;
	ps = &rs->port[sc->port_id];

	*rxe = ps->rx_crc_errors + ps->rx_in_range_errors +
	    ps->rx_frame_too_long + ps->rx_dropped_runt +
	    ps->rx_ip_checksum_errs + ps->rx_tcp_checksum_errs +
	    ps->rx_udp_checksum_errs + ps->rxpp_fifo_overflow_drop +
	    ps->rx_dropped_tcp_length + ps->rx_dropped_too_small +
	    ps->rx_dropped_too_short + ps->rx_out_range_errors +
	    ps->rx_dropped_header_too_small + ps->rx_input_fifo_overflow_drop +
	    ps->rx_alignment_symbol_errors + ps->jabber_events;
	*rxe += ms->eth_red_drops;

	*txe = 0; /* hardware doesn't provide any extra tx error statistics */
}

int
oce_update_stats(struct oce_softc *sc, u_int64_t *rxe, u_int64_t *txe)
{
	int rc = 0;

	if (IS_BE(sc)) {
		if (sc->flags & OCE_FLAGS_BE2) {
			rc = oce_mbox_get_nic_stats_v0(sc, &sc->stats);
			if (!rc)
				update_stats_be2(sc, rxe, txe);
		} else {
			rc = oce_mbox_get_nic_stats(sc, &sc->stats);
			if (!rc)
				update_stats_be3(sc, rxe, txe);
		}

	} else {
		rc = oce_mbox_get_pport_stats(sc, &sc->stats, 0);
		if (!rc)
			update_stats_xe(sc, rxe, txe);
	}

	return rc;
}
