/*	$OpenBSD: if_ixl.c,v 1.8 2018/11/18 08:42:15 jmatthew Exp $ */

/*
 * Copyright (c) 2013-2015, Intel Corporation
 * All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
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
 */

/*
 * Copyright (c) 2016,2017 David Gwynne <dlg@openbsd.org>
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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/timeout.h>
#include <sys/task.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define I40E_MASK(mask, shift)		((mask) << (shift))
#define I40E_PF_RESET_WAIT_COUNT	200
#define I40E_AQ_LARGE_BUF		512

/* bitfields for Tx queue mapping in QTX_CTL */
#define I40E_QTX_CTL_VF_QUEUE		0x0
#define I40E_QTX_CTL_VM_QUEUE		0x1
#define I40E_QTX_CTL_PF_QUEUE		0x2

#define I40E_QUEUE_TYPE_EOL		0x7ff
#define I40E_INTR_NOTX_QUEUE		0

#define I40E_QUEUE_TYPE_RX		0x0
#define I40E_QUEUE_TYPE_TX		0x1
#define I40E_QUEUE_TYPE_PE_CEQ		0x2
#define I40E_QUEUE_TYPE_UNKNOWN		0x3

#define I40E_ITR_INDEX_RX		0x0
#define I40E_ITR_INDEX_TX		0x1
#define I40E_ITR_INDEX_OTHER		0x2
#define I40E_ITR_INDEX_NONE		0x3

#include <dev/pci/if_ixlreg.h>

#define I40E_INTR_NOTX_QUEUE		0
#define I40E_INTR_NOTX_INTR		0
#define I40E_INTR_NOTX_RX_QUEUE		0
#define I40E_INTR_NOTX_TX_QUEUE		1
#define I40E_INTR_NOTX_RX_MASK		I40E_PFINT_ICR0_QUEUE_0_MASK
#define I40E_INTR_NOTX_TX_MASK		I40E_PFINT_ICR0_QUEUE_1_MASK

struct ixl_aq_desc {
	uint16_t	iaq_flags;
#define	IXL_AQ_DD		(1U << 0)
#define	IXL_AQ_CMP		(1U << 1)
#define IXL_AQ_ERR		(1U << 2)
#define IXL_AQ_VFE		(1U << 3)
#define IXL_AQ_LB		(1U << 9)
#define IXL_AQ_RD		(1U << 10)
#define IXL_AQ_VFC		(1U << 11)
#define IXL_AQ_BUF		(1U << 12)
#define IXL_AQ_SI		(1U << 13)
#define IXL_AQ_EI		(1U << 14)
#define IXL_AQ_FE		(1U << 15)

#define IXL_AQ_FLAGS_FMT	"\020" "\020FE" "\017EI" "\016SI" "\015BUF" \
				    "\014VFC" "\013DB" "\012LB" "\004VFE" \
				    "\003ERR" "\002CMP" "\001DD"

	uint16_t	iaq_opcode;

	uint16_t	iaq_datalen;
	uint16_t	iaq_retval;

	uint64_t	iaq_cookie;

	uint32_t	iaq_param[4];
/*	iaq_data_hi	iaq_param[2] */
/*	iaq_data_lo	iaq_param[3] */
} __packed __aligned(8);

/* aq commands */
#define IXL_AQ_OP_GET_VERSION		0x0001
#define IXL_AQ_OP_DRIVER_VERSION	0x0002
#define IXL_AQ_OP_QUEUE_SHUTDOWN	0x0003
#define IXL_AQ_OP_SET_PF_CONTEXT	0x0004
#define IXL_AQ_OP_GET_AQ_ERR_REASON	0x0005
#define IXL_AQ_OP_REQUEST_RESOURCE	0x0008
#define IXL_AQ_OP_RELEASE_RESOURCE	0x0009
#define IXL_AQ_OP_LIST_FUNC_CAP		0x000a
#define IXL_AQ_OP_LIST_DEV_CAP		0x000b
#define IXL_AQ_OP_MAC_ADDRESS_READ	0x0107
#define IXL_AQ_OP_CLEAR_PXE_MODE	0x0110
#define IXL_AQ_OP_SWITCH_GET_CONFIG	0x0200
#define IXL_AQ_OP_ADD_VSI		0x0210
#define IXL_AQ_OP_UPD_VSI_PARAMS	0x0211
#define IXL_AQ_OP_GET_VSI_PARAMS	0x0212
#define IXL_AQ_OP_ADD_VEB		0x0230
#define IXL_AQ_OP_UPD_VEB_PARAMS	0x0231
#define IXL_AQ_OP_GET_VEB_PARAMS	0x0232
#define IXL_AQ_OP_SET_VSI_PROMISC	0x0254
#define IXL_AQ_OP_PHY_GET_ABILITIES	0x0600
#define IXL_AQ_OP_PHY_SET_CONFIG	0x0601
#define IXL_AQ_OP_PHY_SET_MAC_CONFIG	0x0603
#define IXL_AQ_OP_PHY_RESTART_AN	0x0605
#define IXL_AQ_OP_PHY_LINK_STATUS	0x0607
#define IXL_AQ_OP_PHY_SET_EVENT_MASK	0x0613
#define IXL_AQ_OP_LLDP_GET_MIB		0x0a00
#define IXL_AQ_OP_LLDP_MIB_CHG_EV	0x0a01
#define IXL_AQ_OP_LLDP_ADD_TLV		0x0a02
#define IXL_AQ_OP_LLDP_UPD_TLV		0x0a03
#define IXL_AQ_OP_LLDP_DEL_TLV		0x0a04
#define IXL_AQ_OP_LLDP_STOP_AGENT	0x0a05
#define IXL_AQ_OP_LLDP_START_AGENT	0x0a06
#define IXL_AQ_OP_LLDP_GET_CEE_DCBX	0x0a07
#define IXL_AQ_OP_LLDP_SPECIFIC_AGENT	0x0a09

struct ixl_aq_mac_addresses {
	uint8_t		pf_lan[ETHER_ADDR_LEN];
	uint8_t		pf_san[ETHER_ADDR_LEN];
	uint8_t		port[ETHER_ADDR_LEN];
	uint8_t		pf_wol[ETHER_ADDR_LEN];
} __packed;

#define IXL_AQ_MAC_PF_LAN_VALID		(1U << 4)
#define IXL_AQ_MAC_PF_SAN_VALID		(1U << 5)
#define IXL_AQ_MAC_PORT_VALID		(1U << 6)
#define IXL_AQ_MAC_PF_WOL_VALID		(1U << 7)

struct ixl_aq_capability {
	uint16_t	cap_id;
#define IXL_AQ_CAP_SWITCH_MODE		0x0001
#define IXL_AQ_CAP_MNG_MODE		0x0002
#define IXL_AQ_CAP_NPAR_ACTIVE		0x0003
#define IXL_AQ_CAP_OS2BMC_CAP		0x0004
#define IXL_AQ_CAP_FUNCTIONS_VALID	0x0005
#define IXL_AQ_CAP_ALTERNATE_RAM	0x0006
#define IXL_AQ_CAP_WOL_AND_PROXY	0x0008
#define IXL_AQ_CAP_SRIOV		0x0012
#define IXL_AQ_CAP_VF			0x0013
#define IXL_AQ_CAP_VMDQ			0x0014
#define IXL_AQ_CAP_8021QBG		0x0015
#define IXL_AQ_CAP_8021QBR		0x0016
#define IXL_AQ_CAP_VSI			0x0017
#define IXL_AQ_CAP_DCB			0x0018
#define IXL_AQ_CAP_FCOE			0x0021
#define IXL_AQ_CAP_ISCSI		0x0022
#define IXL_AQ_CAP_RSS			0x0040
#define IXL_AQ_CAP_RXQ			0x0041
#define IXL_AQ_CAP_TXQ			0x0042
#define IXL_AQ_CAP_MSIX			0x0043
#define IXL_AQ_CAP_VF_MSIX		0x0044
#define IXL_AQ_CAP_FLOW_DIRECTOR	0x0045
#define IXL_AQ_CAP_1588			0x0046
#define IXL_AQ_CAP_IWARP		0x0051
#define IXL_AQ_CAP_LED			0x0061
#define IXL_AQ_CAP_SDP			0x0062
#define IXL_AQ_CAP_MDIO			0x0063
#define IXL_AQ_CAP_WSR_PROT		0x0064
#define IXL_AQ_CAP_NVM_MGMT		0x0080
#define IXL_AQ_CAP_FLEX10		0x00F1
#define IXL_AQ_CAP_CEM			0x00F2
	uint8_t		major_rev;
	uint8_t		minor_rev;
	uint32_t	number;
	uint32_t	logical_id;
	uint32_t	phys_id;
	uint8_t		_reserved[16];
} __packed __aligned(4);

#define IXL_LLDP_SHUTDOWN		0x1

struct ixl_aq_switch_config {
	uint16_t	num_reported;
	uint16_t	num_total;
	uint8_t		_reserved[12];
} __packed __aligned(4);

struct ixl_aq_switch_config_element {
	uint8_t		type;
#define IXL_AQ_SW_ELEM_TYPE_MAC		1
#define IXL_AQ_SW_ELEM_TYPE_PF		2
#define IXL_AQ_SW_ELEM_TYPE_VF		3
#define IXL_AQ_SW_ELEM_TYPE_EMP		4
#define IXL_AQ_SW_ELEM_TYPE_BMC		5
#define IXL_AQ_SW_ELEM_TYPE_PV		16
#define IXL_AQ_SW_ELEM_TYPE_VEB		17
#define IXL_AQ_SW_ELEM_TYPE_PA		18
#define IXL_AQ_SW_ELEM_TYPE_VSI		19
	uint8_t		revision;
#define IXL_AQ_SW_ELEM_REV_1		1
	uint16_t	seid;

	uint16_t	uplink_seid;
	uint16_t	downlink_seid;

	uint8_t		_reserved[3];
	uint8_t		connection_type;
#define IXL_AQ_CONN_TYPE_REGULAR	0x1
#define IXL_AQ_CONN_TYPE_DEFAULT	0x2
#define IXL_AQ_CONN_TYPE_CASCADED	0x3

	uint16_t	scheduler_id;
	uint16_t	element_info;
} __packed __aligned(4);

#define IXL_PHY_TYPE_SGMII		0x00
#define IXL_PHY_TYPE_1000BASE_KX	0x01
#define IXL_PHY_TYPE_10GBASE_KX4	0x02
#define IXL_PHY_TYPE_10GBASE_KR		0x03
#define IXL_PHY_TYPE_40GBASE_KR4	0x04
#define IXL_PHY_TYPE_XAUI		0x05
#define IXL_PHY_TYPE_XFI		0x06
#define IXL_PHY_TYPE_SFI		0x07
#define IXL_PHY_TYPE_XLAUI		0x08
#define IXL_PHY_TYPE_XLPPI		0x09
#define IXL_PHY_TYPE_40GBASE_CR4_CU	0x0a
#define IXL_PHY_TYPE_10GBASE_CR1_CU	0x0b
#define IXL_PHY_TYPE_10GBASE_AOC	0x0c
#define IXL_PHY_TYPE_40GBASE_AOC	0x0d
#define IXL_PHY_TYPE_100BASE_TX		0x11
#define IXL_PHY_TYPE_1000BASE_T		0x12
#define IXL_PHY_TYPE_10GBASE_T		0x13
#define IXL_PHY_TYPE_10GBASE_SR		0x14
#define IXL_PHY_TYPE_10GBASE_LR		0x15
#define IXL_PHY_TYPE_10GBASE_SFPP_CU	0x16
#define IXL_PHY_TYPE_10GBASE_CR1	0x17
#define IXL_PHY_TYPE_40GBASE_CR4	0x18
#define IXL_PHY_TYPE_40GBASE_SR4	0x19
#define IXL_PHY_TYPE_40GBASE_LR4	0x1a
#define IXL_PHY_TYPE_1000BASE_SX	0x1b
#define IXL_PHY_TYPE_1000BASE_LX	0x1c
#define IXL_PHY_TYPE_1000BASE_T_OPTICAL	0x1d
#define IXL_PHY_TYPE_20GBASE_KR2	0x1e

#define IXL_PHY_TYPE_25GBASE_KR		0x1f
#define IXL_PHY_TYPE_25GBASE_CR		0x20
#define IXL_PHY_TYPE_25GBASE_SR		0x21
#define IXL_PHY_TYPE_25GBASE_LR		0x22
#define IXL_PHY_TYPE_25GBASE_AOC	0x23
#define IXL_PHY_TYPE_25GBASE_ACC	0x24

struct ixl_aq_module_desc {
	uint8_t		oui[3];
	uint8_t		_reserved1;
	uint8_t		part_number[16];
	uint8_t		revision[4];
	uint8_t		_reserved2[8];
} __packed __aligned(4);

struct ixl_aq_phy_abilities {
	uint32_t	phy_type;

	uint8_t		link_speed;
#define IXL_AQ_PHY_LINK_SPEED_100MB	0x1
#define IXL_AQ_PHY_LINK_SPEED_1000MB	0x2
#define IXL_AQ_PHY_LINK_SPEED_10GB	0x3
#define IXL_AQ_PHY_LINK_SPEED_40GB	0x4
#define IXL_AQ_PHY_LINK_SPEED_20GB	0x5
#define IXL_AQ_PHY_LINK_SPEED_25GB	0x6
	uint8_t		abilities;
	uint16_t	eee_capability;

	uint32_t	eeer_val;

	uint8_t		d3_lpan;
	uint8_t		phy_type_ext;
#define IXL_AQ_PHY_TYPE_EXT_25G_KR	0x01
#define IXL_AQ_PHY_TYPE_EXT_25G_CR	0x02
#define IXL_AQ_PHY_TYPE_EXT_25G_SR	0x04
#define IXL_AQ_PHY_TYPE_EXT_25G_LR	0x08
	uint8_t		fec_cfg_curr_mod_ext_info;
#define IXL_AQ_ENABLE_FEC_KR		0x01
#define IXL_AQ_ENABLE_FEC_RS		0x02
#define IXL_AQ_REQUEST_FEC_KR		0x04
#define IXL_AQ_REQUEST_FEC_RS		0x08
#define IXL_AQ_ENABLE_FEC_AUTO		0x10
#define IXL_AQ_MODULE_TYPE_EXT_MASK	0xe0
#define IXL_AQ_MODULE_TYPE_EXT_SHIFT	5
	uint8_t		ext_comp_code;

	uint8_t		phy_id[4];

	uint8_t		module_type[3];
	uint8_t		qualified_module_count;
#define IXL_AQ_PHY_MAX_QMS		16
	struct ixl_aq_module_desc
			qualified_module[IXL_AQ_PHY_MAX_QMS];
} __packed __aligned(4);

struct ixl_aq_link_param {
	uint8_t		notify;
#define IXL_AQ_LINK_NOTIFY	0x03
	uint8_t		_reserved1;
	uint8_t		phy;
	uint8_t		speed;
	uint8_t		status;
	uint8_t		_reserved2[11];
} __packed __aligned(4);

struct ixl_aq_vsi_param {
	uint16_t	uplink_seid;
	uint8_t		connect_type;
#define IXL_AQ_VSI_CONN_TYPE_NORMAL	(0x1)
#define IXL_AQ_VSI_CONN_TYPE_DEFAULT	(0x2)
#define IXL_AQ_VSI_CONN_TYPE_CASCADED	(0x3)
	uint8_t		_reserved1;

	uint8_t		vf_id;
	uint8_t		_reserved2;
	uint16_t	vsi_flags;
#define IXL_AQ_VSI_TYPE_SHIFT		0x0
#define IXL_AQ_VSI_TYPE_MASK		(0x3 << IXL_AQ_VSI_TYPE_SHIFT)
#define IXL_AQ_VSI_TYPE_VF		0x0
#define IXL_AQ_VSI_TYPE_VMDQ2		0x1
#define IXL_AQ_VSI_TYPE_PF		0x2
#define IXL_AQ_VSI_TYPE_EMP_MNG		0x3
#define IXL_AQ_VSI_FLAG_CASCADED_PV	0x4

	uint32_t	addr_hi;
	uint32_t	addr_lo;
} __packed __aligned(16);

struct ixl_aq_vsi_reply {
	uint16_t	seid;
	uint16_t	vsi_number;

	uint16_t	vsis_used;
	uint16_t	vsis_free;

	uint32_t	addr_hi;
	uint32_t	addr_lo;
} __packed __aligned(16);

struct ixl_aq_vsi_data {
	/* first 96 byte are written by SW */
	uint16_t	valid_sections;
#define IXL_AQ_VSI_VALID_SWITCH		(1 << 0)
#define IXL_AQ_VSI_VALID_SECURITY	(1 << 1)
#define IXL_AQ_VSI_VALID_VLAN		(1 << 2)
#define IXL_AQ_VSI_VALID_CAS_PV		(1 << 3)
#define IXL_AQ_VSI_VALID_INGRESS_UP	(1 << 4)
#define IXL_AQ_VSI_VALID_EGRESS_UP	(1 << 5)
#define IXL_AQ_VSI_VALID_QUEUE_MAP	(1 << 6)
#define IXL_AQ_VSI_VALID_QUEUE_OPT	(1 << 7)
#define IXL_AQ_VSI_VALID_OUTER_UP	(1 << 8)
#define IXL_AQ_VSI_VALID_SCHED		(1 << 9)
	/* switch section */
	uint16_t	switch_id;
#define IXL_AQ_VSI_SWITCH_ID_SHIFT	0
#define IXL_AQ_VSI_SWITCH_ID_MASK	(0xfff << IXL_AQ_VSI_SWITCH_ID_SHIFT)
#define IXL_AQ_VSI_SWITCH_NOT_STAG	(1 << 12)
#define IXL_AQ_VSI_SWITCH_LOCAL_LB	(1 << 14)

	uint8_t		_reserved1[2];
	/* security section */
	uint8_t		sec_flags;
#define IXL_AQ_VSI_SEC_ALLOW_DEST_OVRD	(1 << 0)
#define IXL_AQ_VSI_SEC_ENABLE_VLAN_CHK	(1 << 1)
#define IXL_AQ_VSI_SEC_ENABLE_MAC_CHK	(1 << 2)
	uint8_t		_reserved2;

	/* vlan section */
	uint16_t	pvid;
	uint16_t	fcoe_pvid;

	uint8_t		port_vlan_flags;
#define IXL_AQ_VSI_PVLAN_MODE_SHIFT	0
#define IXL_AQ_VSI_PVLAN_MODE_MASK	(0x3 << IXL_AQ_VSI_PVLAN_MODE_SHIFT)
#define IXL_AQ_VSI_PVLAN_MODE_TAGGED	(0x1 << IXL_AQ_VSI_PVLAN_MODE_SHIFT)
#define IXL_AQ_VSI_PVLAN_MODE_UNTAGGED 	(0x2 << IXL_AQ_VSI_PVLAN_MODE_SHIFT)
#define IXL_AQ_VSI_PVLAN_MODE_ALL	(0x3 << IXL_AQ_VSI_PVLAN_MODE_SHIFT)
#define IXL_AQ_VSI_PVLAN_INSERT_PVID	(0x4 << IXL_AQ_VSI_PVLAN_MODE_SHIFT)
#define IXL_AQ_VSI_PVLAN_EMOD_SHIFT	0x3
#define IXL_AQ_VSI_PVLAN_EMOD_MASK	(0x3 << IXL_AQ_VSI_PVLAN_EMOD_SHIFT)
#define IXL_AQ_VSI_PVLAN_EMOD_STR_BOTH	(0x0 << IXL_AQ_VSI_PVLAN_EMOD_SHIFT)
#define IXL_AQ_VSI_PVLAN_EMOD_STR_UP	(0x1 << IXL_AQ_VSI_PVLAN_EMOD_SHIFT)
#define IXL_AQ_VSI_PVLAN_EMOD_STR	(0x2 << IXL_AQ_VSI_PVLAN_EMOD_SHIFT)
#define IXL_AQ_VSI_PVLAN_EMOD_NOTHING	(0x3 << IXL_AQ_VSI_PVLAN_EMOD_SHIFT)
	uint8_t		_reserved3[3];

	/* ingress egress up section */
	uint32_t	ingress_table;
#define IXL_AQ_VSI_UP_SHIFT(_up)	((_up) * 3)
#define IXL_AQ_VSI_UP_MASK(_up)		(0x7 << (IXL_AQ_VSI_UP_SHIFT(_up))
	uint32_t	egress_table;

	/* cascaded pv section */
	uint16_t	cas_pv_tag;
	uint8_t		cas_pv_flags;
#define IXL_AQ_VSI_CAS_PV_TAGX_SHIFT	0
#define IXL_AQ_VSI_CAS_PV_TAGX_MASK	(0x3 << IXL_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define IXL_AQ_VSI_CAS_PV_TAGX_LEAVE	(0x0 << IXL_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define IXL_AQ_VSI_CAS_PV_TAGX_REMOVE	(0x1 << IXL_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define IXL_AQ_VSI_CAS_PV_TAGX_COPY	(0x2 << IXL_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define IXL_AQ_VSI_CAS_PV_INSERT_TAG	(1 << 4)
#define IXL_AQ_VSI_CAS_PV_ETAG_PRUNE	(1 << 5)
#define IXL_AQ_VSI_CAS_PV_ACCEPT_HOST_TAG \
					(1 << 6)
	uint8_t		_reserved4;

	/* queue mapping section */
	uint16_t	mapping_flags;
#define IXL_AQ_VSI_QUE_MAP_MASK		0x1
#define IXL_AQ_VSI_QUE_MAP_CONTIG	0x0
#define IXL_AQ_VSI_QUE_MAP_NONCONTIG	0x1
	uint16_t	queue_mapping[16];
#define IXL_AQ_VSI_QUEUE_SHIFT		0x0
#define IXL_AQ_VSI_QUEUE_MASK		(0x7ff << IXL_AQ_VSI_QUEUE_SHIFT)
	uint16_t	tc_mapping[8];
#define IXL_AQ_VSI_TC_Q_OFFSET_SHIFT	0
#define IXL_AQ_VSI_TC_Q_OFFSET_MASK	(0x1ff << IXL_AQ_VSI_TC_Q_OFFSET_SHIFT)
#define IXL_AQ_VSI_TC_Q_NUMBER_SHIFT	9
#define IXL_AQ_VSI_TC_Q_NUMBER_MASK	(0x7 << IXL_AQ_VSI_TC_Q_NUMBER_SHIFT)

	/* queueing option section */
	uint8_t		queueing_opt_flags;
#define IXL_AQ_VSI_QUE_OPT_MCAST_UDP_EN	(1 << 2)
#define IXL_AQ_VSI_QUE_OPT_UCAST_UDP_EN	(1 << 3)
#define IXL_AQ_VSI_QUE_OPT_TCP_EN	(1 << 4)
#define IXL_AQ_VSI_QUE_OPT_FCOE_EN	(1 << 5)
#define IXL_AQ_VSI_QUE_OPT_RSS_LUT_PF	0
#define IXL_AQ_VSI_QUE_OPT_RSS_LUT_VSI	(1 << 6)
	uint8_t		_reserved5[3];

	/* scheduler section */
	uint8_t		up_enable_bits;
	uint8_t		_reserved6;

	/* outer up section */
	uint32_t	outer_up_table; /* same as ingress/egress tables */
	uint8_t		_reserved7[8];

	/* last 32 bytes are written by FW */
	uint16_t	qs_handle[8];
#define IXL_AQ_VSI_QS_HANDLE_INVALID	0xffff
	uint16_t	stat_counter_idx;
	uint16_t	sched_id;

	uint8_t		_reserved8[12];
} __packed __aligned(8);

CTASSERT(sizeof(struct ixl_aq_vsi_data) == 128);

struct ixl_aq_vsi_promisc_param {
	uint16_t	flags;
	uint16_t	valid_flags;
#define IXL_AQ_VSI_PROMISC_FLAG_UCAST	(1 << 0)
#define IXL_AQ_VSI_PROMISC_FLAG_MCAST	(1 << 1)
#define IXL_AQ_VSI_PROMISC_FLAG_BCAST	(1 << 2)
#define IXL_AQ_VSI_PROMISC_FLAG_DFLT	(1 << 3)
#define IXL_AQ_VSI_PROMISC_FLAG_VLAN	(1 << 4)
#define IXL_AQ_VSI_PROMISC_FLAG_RXONLY	(1 << 15)

	uint16_t	seid;
#define IXL_AQ_VSI_PROMISC_SEID_VALID	(1 << 15)
	uint16_t	vlan;
#define IXL_AQ_VSI_PROMISC_VLAN_VALID	(1 << 15)
	uint32_t	reserved[2];
} __packed __aligned(8);

struct ixl_aq_veb_param {
	uint16_t	uplink_seid;
	uint16_t	downlink_seid;
	uint16_t	veb_flags;
#define IXL_AQ_ADD_VEB_FLOATING		(1 << 0)
#define IXL_AQ_ADD_VEB_PORT_TYPE_SHIFT	1
#define IXL_AQ_ADD_VEB_PORT_TYPE_MASK	(0x3 << IXL_AQ_ADD_VEB_PORT_TYPE_SHIFT)
#define IXL_AQ_ADD_VEB_PORT_TYPE_DEFAULT \
					(0x2 << IXL_AQ_ADD_VEB_PORT_TYPE_SHIFT)
#define IXL_AQ_ADD_VEB_PORT_TYPE_DATA	(0x4 << IXL_AQ_ADD_VEB_PORT_TYPE_SHIFT)
#define IXL_AQ_ADD_VEB_ENABLE_L2_FILTER	(1 << 3) /* deprecated */
#define IXL_AQ_ADD_VEB_DISABLE_STATS	(1 << 4)
	uint8_t		enable_tcs;
	uint8_t		_reserved[9];
} __packed __aligned(16);

struct ixl_aq_veb_reply {
	uint16_t	_reserved1;
	uint16_t	_reserved2;
	uint16_t	_reserved3;
	uint16_t	switch_seid;
	uint16_t	veb_seid;
#define IXL_AQ_VEB_ERR_FLAG_NO_VEB	(1 << 0)
#define IXL_AQ_VEB_ERR_FLAG_NO_SCHED	(1 << 1)
#define IXL_AQ_VEB_ERR_FLAG_NO_COUNTER	(1 << 2)
#define IXL_AQ_VEB_ERR_FLAG_NO_ENTRY	(1 << 3);
	uint16_t	statistic_index;
	uint16_t	vebs_used;
	uint16_t	vebs_free;
} __packed __aligned(16);

/* GET PHY ABILITIES param[0] */
#define IXL_AQ_PHY_REPORT_QUAL		(1 << 0)
#define IXL_AQ_PHY_REPORT_INIT		(1 << 1)

/* RESTART_AN param[0] */
#define IXL_AQ_PHY_RESTART_AN		(1 << 1)
#define IXL_AQ_PHY_LINK_ENABLE		(1 << 2)

struct ixl_aq_link_status { /* this occupies the iaq_param space */
	uint16_t	command_flags; /* only field set on command */
#define IXL_AQ_LSE_MASK			0x3
#define IXL_AQ_LSE_NOP			0x0
#define IXL_AQ_LSE_DISABLE		0x2
#define IXL_AQ_LSE_ENABLE		0x3
#define IXL_AQ_LSE_IS_ENABLED		0x1 /* only set in response */
	uint8_t		phy_type;
	uint8_t		link_speed;
	uint8_t		link_info;
#define IXL_AQ_LINK_UP_FUNCTION		0x01
#define IXL_AQ_LINK_FAULT		0x02
#define IXL_AQ_LINK_FAULT_TX		0x04
#define IXL_AQ_LINK_FAULT_RX		0x08
#define IXL_AQ_LINK_FAULT_REMOTE	0x10
#define IXL_AQ_LINK_UP_PORT		0x20
#define IXL_AQ_MEDIA_AVAILABLE		0x40
#define IXL_AQ_SIGNAL_DETECT		0x80
	uint8_t		an_info;
#define IXL_AQ_AN_COMPLETED		0x01
#define IXL_AQ_LP_AN_ABILITY		0x02
#define IXL_AQ_PD_FAULT			0x04
#define IXL_AQ_FEC_EN			0x08
#define IXL_AQ_PHY_LOW_POWER		0x10
#define IXL_AQ_LINK_PAUSE_TX		0x20
#define IXL_AQ_LINK_PAUSE_RX		0x40
#define IXL_AQ_QUALIFIED_MODULE		0x80

	uint8_t		ext_info;
#define IXL_AQ_LINK_PHY_TEMP_ALARM	0x01
#define IXL_AQ_LINK_XCESSIVE_ERRORS	0x02
#define IXL_AQ_LINK_TX_SHIFT		0x02
#define IXL_AQ_LINK_TX_MASK		(0x03 << IXL_AQ_LINK_TX_SHIFT)
#define IXL_AQ_LINK_TX_ACTIVE		0x00
#define IXL_AQ_LINK_TX_DRAINED		0x01
#define IXL_AQ_LINK_TX_FLUSHED		0x03
#define IXL_AQ_LINK_FORCED_40G		0x10
/* 25G Error Codes */
#define IXL_AQ_25G_NO_ERR		0X00
#define IXL_AQ_25G_NOT_PRESENT		0X01
#define IXL_AQ_25G_NVM_CRC_ERR		0X02
#define IXL_AQ_25G_SBUS_UCODE_ERR	0X03
#define IXL_AQ_25G_SERDES_UCODE_ERR	0X04
#define IXL_AQ_25G_NIMB_UCODE_ERR	0X05
	uint8_t		loopback;
	uint16_t	max_frame_size;

	uint8_t		config;
#define IXL_AQ_CONFIG_FEC_KR_ENA	0x01
#define IXL_AQ_CONFIG_FEC_RS_ENA	0x02
#define IXL_AQ_CONFIG_CRC_ENA	0x04
#define IXL_AQ_CONFIG_PACING_MASK	0x78
	uint8_t		power_desc;
#define IXL_AQ_LINK_POWER_CLASS_1	0x00
#define IXL_AQ_LINK_POWER_CLASS_2	0x01
#define IXL_AQ_LINK_POWER_CLASS_3	0x02
#define IXL_AQ_LINK_POWER_CLASS_4	0x03
#define IXL_AQ_PWR_CLASS_MASK		0x03

	uint8_t		reserved[4];
} __packed __aligned(4);
/* event mask command flags for param[2] */
#define IXL_AQ_PHY_EV_MASK		0x3ff
#define IXL_AQ_PHY_EV_LINK_UPDOWN	(1 << 1)
#define IXL_AQ_PHY_EV_MEDIA_NA		(1 << 2)
#define IXL_AQ_PHY_EV_LINK_FAULT	(1 << 3)
#define IXL_AQ_PHY_EV_PHY_TEMP_ALARM	(1 << 4)
#define IXL_AQ_PHY_EV_EXCESS_ERRORS	(1 << 5)
#define IXL_AQ_PHY_EV_SIGNAL_DETECT	(1 << 6)
#define IXL_AQ_PHY_EV_AN_COMPLETED	(1 << 7)
#define IXL_AQ_PHY_EV_MODULE_QUAL_FAIL	(1 << 8)
#define IXL_AQ_PHY_EV_PORT_TX_SUSPENDED	(1 << 9)

/* aq response codes */
#define IXL_AQ_RC_OK			0  /* success */
#define IXL_AQ_RC_EPERM			1  /* Operation not permitted */
#define IXL_AQ_RC_ENOENT		2  /* No such element */
#define IXL_AQ_RC_ESRCH			3  /* Bad opcode */
#define IXL_AQ_RC_EINTR			4  /* operation interrupted */
#define IXL_AQ_RC_EIO			5  /* I/O error */
#define IXL_AQ_RC_ENXIO			6  /* No such resource */
#define IXL_AQ_RC_E2BIG			7  /* Arg too long */
#define IXL_AQ_RC_EAGAIN		8  /* Try again */
#define IXL_AQ_RC_ENOMEM		9  /* Out of memory */
#define IXL_AQ_RC_EACCES		10 /* Permission denied */
#define IXL_AQ_RC_EFAULT		11 /* Bad address */
#define IXL_AQ_RC_EBUSY			12 /* Device or resource busy */
#define IXL_AQ_RC_EEXIST		13 /* object already exists */
#define IXL_AQ_RC_EINVAL		14 /* invalid argument */
#define IXL_AQ_RC_ENOTTY		15 /* not a typewriter */
#define IXL_AQ_RC_ENOSPC		16 /* No space or alloc failure */
#define IXL_AQ_RC_ENOSYS		17 /* function not implemented */
#define IXL_AQ_RC_ERANGE		18 /* parameter out of range */
#define IXL_AQ_RC_EFLUSHED		19 /* cmd flushed due to prev error */
#define IXL_AQ_RC_BAD_ADDR		20 /* contains a bad pointer */
#define IXL_AQ_RC_EMODE			21 /* not allowed in current mode */
#define IXL_AQ_RC_EFBIG			22 /* file too large */

struct ixl_tx_desc {
	uint64_t		addr;
	uint64_t		cmd;
#define IXL_TX_DESC_DTYPE_SHIFT		0
#define IXL_TX_DESC_DTYPE_MASK		(0xfULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_DATA		(0x0ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_NOP		(0x1ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_CONTEXT	(0x1ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_FCOE_CTX	(0x2ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_FD		(0x8ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_DDP_CTX	(0x9ULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_FLEX_DATA	(0xbULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_FLEX_CTX_1	(0xcULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_FLEX_CTX_2	(0xdULL << IXL_TX_DESC_DTYPE_SHIFT)
#define IXL_TX_DESC_DTYPE_DONE		(0xfULL << IXL_TX_DESC_DTYPE_SHIFT)

#define IXL_TX_DESC_CMD_SHIFT		4
#define IXL_TX_DESC_CMD_MASK		(0x3ffULL << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_EOP		(0x001 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_RS		(0x002 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_ICRC		(0x004 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IL2TAG1		(0x008 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_DUMMY		(0x010 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IIPT_MASK	(0x060 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IIPT_NONIP	(0x000 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IIPT_IPV6	(0x020 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IIPT_IPV4	(0x040 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_IIPT_IPV4_CSUM	(0x060 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_FCOET		(0x080 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_L4T_EOFT_MASK	(0x300 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_L4T_EOFT_UNK	(0x000 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_L4T_EOFT_TCP	(0x100 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_L4T_EOFT_SCTP	(0x200 << IXL_TX_DESC_CMD_SHIFT)
#define IXL_TX_DESC_CMD_L4T_EOFT_UDP	(0x300 << IXL_TX_DESC_CMD_SHIFT)

#define IXL_TX_DESC_MACLEN_SHIFT	16
#define IXL_TX_DESC_MACLEN_MASK		(0x7fULL << IXL_TX_DESC_MACLEN_SHIFT)
#define IXL_TX_DESC_IPLEN_SHIFT		23
#define IXL_TX_DESC_IPLEN_MASK		(0x7fULL << IXL_TX_DESC_IPLEN_SHIFT)
#define IXL_TX_DESC_L4LEN_SHIFT		30
#define IXL_TX_DESC_L4LEN_MASK		(0xfULL << IXL_TX_DESC_L4LEN_SHIFT)
#define IXL_TX_DESC_FCLEN_SHIFT		30
#define IXL_TX_DESC_FCLEN_MASK		(0xfULL << IXL_TX_DESC_FCLEN_SHIFT)

#define IXL_TX_DESC_BSIZE_SHIFT		34
#define IXL_TX_DESC_BSIZE_MAX		0x3fffULL
#define IXL_TX_DESC_BSIZE_MASK		\
	(IXL_TX_DESC_BSIZE_MAX << IXL_TX_DESC_BSIZE_SHIFT)
} __packed __aligned(16);

struct ixl_rx_rd_desc_16 {
	uint64_t		paddr; /* packet addr */
	uint64_t		haddr; /* header addr */
} __packed __aligned(16);

struct ixl_rx_rd_desc_32 {
	uint64_t		paddr; /* packet addr */
	uint64_t		haddr; /* header addr */
	uint64_t		_reserved1;
	uint64_t		_reserved2;
} __packed __aligned(16);

struct ixl_rx_wb_desc_16 {
	uint64_t		qword0;
	uint64_t		qword1;
#define IXL_RX_DESC_DD			(1 << 0)
#define IXL_RX_DESC_EOP			(1 << 1)
#define IXL_RX_DESC_L2TAG1P		(1 << 2)
#define IXL_RX_DESC_L3L4P		(1 << 3)
#define IXL_RX_DESC_CRCP		(1 << 4)
#define IXL_RX_DESC_TSYNINDX_SHIFT	5	/* TSYNINDX */
#define IXL_RX_DESC_TSYNINDX_MASK	(7 << IXL_RX_DESC_TSYNINDX_SHIFT)
#define IXL_RX_DESC_UMB_SHIFT		9
#define IXL_RX_DESC_UMB_MASK		(0x3 << IXL_RX_DESC_UMB_SHIFT)
#define IXL_RX_DESC_UMB_UCAST		(0x0 << IXL_RX_DESC_UMB_SHIFT)
#define IXL_RX_DESC_UMB_MCAST		(0x1 << IXL_RX_DESC_UMB_SHIFT)
#define IXL_RX_DESC_UMB_BCAST		(0x2 << IXL_RX_DESC_UMB_SHIFT)
#define IXL_RX_DESC_UMB_MIRROR		(0x3 << IXL_RX_DESC_UMB_SHIFT)
#define IXL_RX_DESC_FLM			(1 << 11)
#define IXL_RX_DESC_FLTSTAT_SHIFT 	12
#define IXL_RX_DESC_FLTSTAT_MASK 	(0x3 << IXL_RX_DESC_FLTSTAT_SHIFT)
#define IXL_RX_DESC_FLTSTAT_NODATA 	(0x0 << IXL_RX_DESC_FLTSTAT_SHIFT)
#define IXL_RX_DESC_FLTSTAT_FDFILTID 	(0x1 << IXL_RX_DESC_FLTSTAT_SHIFT)
#define IXL_RX_DESC_FLTSTAT_RSS 	(0x3 << IXL_RX_DESC_FLTSTAT_SHIFT)
#define IXL_RX_DESC_LPBK		(1 << 14)
#define IXL_RX_DESC_IPV6EXTADD		(1 << 15)
#define IXL_RX_DESC_INT_UDP_0		(1 << 18)

#define IXL_RX_DESC_RXE			(1 << 19)
#define IXL_RX_DESC_HBO			(1 << 21)
#define IXL_RX_DESC_IPE			(1 << 22)
#define IXL_RX_DESC_L4E			(1 << 23)
#define IXL_RX_DESC_EIPE		(1 << 24)
#define IXL_RX_DESC_OVERSIZE		(1 << 25)

#define IXL_RX_DESC_PTYPE_SHIFT		30
#define IXL_RX_DESC_PTYPE_MASK		(0xffULL << IXL_RX_DESC_PTYPE_SHIFT)

#define IXL_RX_DESC_PLEN_SHIFT		38
#define IXL_RX_DESC_PLEN_MASK		(0x3fffULL << IXL_RX_DESC_PLEN_SHIFT)
#define IXL_RX_DESC_HLEN_SHIFT		42
#define IXL_RX_DESC_HLEN_MASK		(0x7ffULL << IXL_RX_DESC_HLEN_SHIFT)
} __packed __aligned(16);

struct ixl_rx_wb_desc_32 {
	uint64_t		qword0;
	uint64_t		qword1;
	uint64_t		qword2;
	uint64_t		qword3;
} __packed __aligned(16);

#define IXL_TX_PKT_DESCS		8
#define IXL_TX_QUEUE_ALIGN		128
#define IXL_RX_QUEUE_ALIGN		128

#define IXL_HARDMTU			9706 /* - ETHER_HEADER_LEN? */

#define IXL_PCIREG			PCI_MAPREG_START

#define IXL_ITR0			0x0
#define IXL_ITR1			0x1
#define IXL_ITR2			0x2
#define IXL_NOITR			0x2

#define IXL_AQ_NUM			256
#define IXL_AQ_MASK			(IXL_AQ_NUM - 1)
#define IXL_AQ_ALIGN			64 /* lol */
#define IXL_AQ_BUFLEN			4096

#define IXL_HMC_ROUNDUP			512
#define IXL_HMC_PGSIZE			4096
#define IXL_HMC_DVASZ			sizeof(uint64_t)
#define IXL_HMC_PGS			(IXL_HMC_PGSIZE / IXL_HMC_DVASZ)
#define IXL_HMC_L2SZ			(IXL_HMC_PGSIZE * IXL_HMC_PGS)
#define IXL_HMC_PDVALID			1ULL

struct ixl_aq_regs {
	bus_size_t		atq_tail;
	bus_size_t		atq_head;
	bus_size_t		atq_len;
	bus_size_t		atq_bal;
	bus_size_t		atq_bah;

	bus_size_t		arq_tail;
	bus_size_t		arq_head;
	bus_size_t		arq_len;
	bus_size_t		arq_bal;
	bus_size_t		arq_bah;

	uint32_t		atq_len_enable;
	uint32_t		atq_tail_mask;
	uint32_t		atq_head_mask;

	uint32_t		arq_len_enable;
	uint32_t		arq_tail_mask;
	uint32_t		arq_head_mask;
};

struct ixl_phy_type {
	uint64_t	phy_type;
	uint64_t	ifm_type;
};

struct ixl_speed_type {
	uint8_t		dev_speed;
	uint64_t	net_speed;
};

struct ixl_aq_buf {
	SIMPLEQ_ENTRY(ixl_aq_buf)
				 aqb_entry;
	void			*aqb_data;
	bus_dmamap_t		 aqb_map;
};
SIMPLEQ_HEAD(ixl_aq_bufs, ixl_aq_buf);

struct ixl_dmamem {
	bus_dmamap_t		ixm_map;
	bus_dma_segment_t	ixm_seg;
	int			ixm_nsegs;
	size_t			ixm_size;
	caddr_t			ixm_kva;
};
#define IXL_DMA_MAP(_ixm)	((_ixm)->ixm_map)
#define IXL_DMA_DVA(_ixm)	((_ixm)->ixm_map->dm_segs[0].ds_addr)
#define IXL_DMA_KVA(_ixm)	((void *)(_ixm)->ixm_kva)
#define IXL_DMA_LEN(_ixm)	((_ixm)->ixm_size)

struct ixl_hmc_entry {
	uint64_t		 hmc_base;
	uint32_t		 hmc_count;
	uint32_t		 hmc_size;
};

#define IXL_HMC_LAN_TX		 0
#define IXL_HMC_LAN_RX		 1
#define IXL_HMC_FCOE_CTX	 2
#define IXL_HMC_FCOE_FILTER	 3
#define IXL_HMC_COUNT		 4

struct ixl_hmc_pack {
	uint16_t		offset;
	uint16_t		width;
	uint16_t		lsb;
};

/*
 * these hmc objects have weird sizes and alignments, so these are abstract
 * representations of them that are nice for c to populate.
 *
 * the packing code relies on little-endian values being stored in the fields,
 * no high bits in the fields being set, and the fields must be packed in the
 * same order as they are in the ctx structure.
 */

struct ixl_hmc_rxq {
	uint16_t		 head;
	uint8_t			 cpuid;
	uint64_t		 base;
#define IXL_HMC_RXQ_BASE_UNIT		128
	uint16_t		 qlen;
	uint16_t		 dbuff;
#define IXL_HMC_RXQ_DBUFF_UNIT		128
	uint8_t			 hbuff;
#define IXL_HMC_RXQ_HBUFF_UNIT		64
	uint8_t			 dtype;
#define IXL_HMC_RXQ_DTYPE_NOSPLIT	0x0
#define IXL_HMC_RXQ_DTYPE_HSPLIT	0x1
#define IXL_HMC_RXQ_DTYPE_SPLIT_ALWAYS	0x2
	uint8_t			 dsize;
#define IXL_HMC_RXQ_DSIZE_16		0
#define IXL_HMC_RXQ_DSIZE_32		1
	uint8_t			 crcstrip;
	uint8_t			 fc_ena;
	uint8_t			 l2sel;
	uint8_t			 hsplit_0;
	uint8_t			 hsplit_1;
	uint8_t			 showiv;
	uint16_t		 rxmax;
	uint8_t			 tphrdesc_ena;
	uint8_t			 tphwdesc_ena;
	uint8_t			 tphdata_ena;
	uint8_t			 tphhead_ena;
	uint8_t			 lrxqthresh;
	uint8_t			 prefena;
};

static const struct ixl_hmc_pack ixl_hmc_pack_rxq[] = {
	{ offsetof(struct ixl_hmc_rxq, head),		13,	0 },
	{ offsetof(struct ixl_hmc_rxq, cpuid),		8,	13 },
	{ offsetof(struct ixl_hmc_rxq, base),		57,	32 },
	{ offsetof(struct ixl_hmc_rxq, qlen),		13,	89 },
	{ offsetof(struct ixl_hmc_rxq, dbuff),		7,	102 },
	{ offsetof(struct ixl_hmc_rxq, hbuff),		5,	109 },
	{ offsetof(struct ixl_hmc_rxq, dtype),		2,	114 },
	{ offsetof(struct ixl_hmc_rxq, dsize),		1,	116 },
	{ offsetof(struct ixl_hmc_rxq, crcstrip),	1,	117 },
	{ offsetof(struct ixl_hmc_rxq, fc_ena),		1,	118 },
	{ offsetof(struct ixl_hmc_rxq, l2sel),		1,	119 },
	{ offsetof(struct ixl_hmc_rxq, hsplit_0),	4,	120 },
	{ offsetof(struct ixl_hmc_rxq, hsplit_1),	2,	124 },
	{ offsetof(struct ixl_hmc_rxq, showiv),		1,	127 },
	{ offsetof(struct ixl_hmc_rxq, rxmax),		14,	174 },
	{ offsetof(struct ixl_hmc_rxq, tphrdesc_ena),	1,	193 },
	{ offsetof(struct ixl_hmc_rxq, tphwdesc_ena),	1,	194 },
	{ offsetof(struct ixl_hmc_rxq, tphdata_ena),	1,	195 },
	{ offsetof(struct ixl_hmc_rxq, tphhead_ena),	1,	196 },
	{ offsetof(struct ixl_hmc_rxq, lrxqthresh),	3,	198 },
	{ offsetof(struct ixl_hmc_rxq, prefena),	1,	201 },
};

#define IXL_HMC_RXQ_MINSIZE (201 + 1)

struct ixl_hmc_txq {
	uint16_t		head;
	uint8_t			new_context;
	uint64_t		base;
#define IXL_HMC_TXQ_BASE_UNIT		128
	uint8_t			fc_ena;
	uint8_t			timesync_ena;
	uint8_t			fd_ena;
	uint8_t			alt_vlan_ena;
	uint16_t		thead_wb;
	uint8_t			cpuid;
	uint8_t			head_wb_ena;
#define IXL_HMC_TXQ_DESC_WB		0
#define IXL_HMC_TXQ_HEAD_WB		1
	uint16_t		qlen;
	uint8_t			tphrdesc_ena;
	uint8_t			tphrpacket_ena;
	uint8_t			tphwdesc_ena;
	uint64_t		head_wb_addr;
	uint32_t		crc;
	uint16_t		rdylist;
	uint8_t			rdylist_act;
};

static const struct ixl_hmc_pack ixl_hmc_pack_txq[] = {
	{ offsetof(struct ixl_hmc_txq, head),		13,	0 },
	{ offsetof(struct ixl_hmc_txq, new_context),	1,	30 },
	{ offsetof(struct ixl_hmc_txq, base),		57,	32 },
	{ offsetof(struct ixl_hmc_txq, fc_ena),		1,	89 },
	{ offsetof(struct ixl_hmc_txq, timesync_ena),	1,	90 },
	{ offsetof(struct ixl_hmc_txq, fd_ena),		1,	91 },
	{ offsetof(struct ixl_hmc_txq, alt_vlan_ena),	1,	92 },
	{ offsetof(struct ixl_hmc_txq, cpuid),		8,	96 },
/* line 1 */
	{ offsetof(struct ixl_hmc_txq, thead_wb),	13,	0 + 128 },
	{ offsetof(struct ixl_hmc_txq, head_wb_ena),	1,	32 + 128 },
	{ offsetof(struct ixl_hmc_txq, qlen),		13,	33 + 128 },
	{ offsetof(struct ixl_hmc_txq, tphrdesc_ena),	1,	46 + 128 },
	{ offsetof(struct ixl_hmc_txq, tphrpacket_ena),	1,	47 + 128 },
	{ offsetof(struct ixl_hmc_txq, tphwdesc_ena),	1,	48 + 128 },
	{ offsetof(struct ixl_hmc_txq, head_wb_addr),	64,	64 + 128 },
/* line 7 */
	{ offsetof(struct ixl_hmc_txq, crc),		32,	0 + (7*128) },
	{ offsetof(struct ixl_hmc_txq, rdylist),	10,	84 + (7*128) },
	{ offsetof(struct ixl_hmc_txq, rdylist_act),	1,	94 + (7*128) },
};

#define IXL_HMC_TXQ_MINSIZE (94 + (7*128) + 1)

struct ixl_tx_map {
	struct mbuf		*txm_m;
	bus_dmamap_t		 txm_map;
	unsigned int		 txm_eop;
};

struct ixl_tx_ring {
	unsigned int		 txr_prod;
	unsigned int		 txr_cons;

	struct ixl_tx_map	*txr_maps;
	struct ixl_dmamem	 txr_mem;

	bus_size_t		 txr_tail;
	unsigned int		 txr_qid;
};

struct ixl_rx_map {
	struct mbuf		*rxm_m;
	bus_dmamap_t		 rxm_map;
};

struct ixl_rx_ring {
	struct ixl_softc	*rxr_sc;

	struct if_rxring	 rxr_acct;
	struct timeout		 rxr_refill;

	unsigned int		 rxr_prod;
	unsigned int		 rxr_cons;

	struct ixl_rx_map	*rxr_maps;
	struct ixl_dmamem	 rxr_mem;

	struct mbuf		*rxr_m_head;
	struct mbuf		**rxr_m_tail;

	bus_size_t		 rxr_tail;
	unsigned int		 rxr_qid;
};

struct ixl_softc {
	struct device		 sc_dev;
	struct arpcom		 sc_ac;
	struct ifmedia		 sc_media;
	uint64_t		 sc_media_status;
	uint64_t		 sc_media_active;

	pci_chipset_tag_t	 sc_pc;
	pci_intr_handle_t	 sc_ih;
	void			*sc_ihc;
	pcitag_t		 sc_tag;

	bus_dma_tag_t		 sc_dmat;
	bus_space_tag_t		 sc_memt;
	bus_space_handle_t	 sc_memh;
	bus_size_t		 sc_mems;

	uint8_t			 sc_pf_id;
	uint16_t		 sc_uplink_seid;	/* le */
	uint16_t		 sc_downlink_seid;	/* le */
	uint16_t		 sc_veb_seid;		/* le */
	uint16_t		 sc_vsi_number;		/* le */
	uint16_t		 sc_seid;
	unsigned int		 sc_base_queue;

	struct ixl_dmamem	 sc_vsi;

	const struct ixl_aq_regs *
				 sc_aq_regs;

	struct mutex		 sc_atq_mtx;
	struct ixl_dmamem	 sc_atq;
	unsigned int		 sc_atq_prod;
	unsigned int		 sc_atq_cons;

	struct ixl_dmamem	 sc_arq;
	struct task		 sc_arq_task;
	struct ixl_aq_bufs	 sc_arq_idle;
	struct ixl_aq_bufs	 sc_arq_live;
	struct if_rxring	 sc_arq_ring;
	unsigned int		 sc_arq_prod;
	unsigned int		 sc_arq_cons;

	struct ixl_dmamem	 sc_hmc_sd;
	struct ixl_dmamem	 sc_hmc_pd;
	struct ixl_hmc_entry	 sc_hmc_entries[IXL_HMC_COUNT];

	unsigned int		 sc_nrings;

	unsigned int		 sc_tx_ring_ndescs;
	unsigned int		 sc_rx_ring_ndescs;
	unsigned int		 sc_nqueues;	/* 1 << sc_nqueues */
};
#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

struct ixl_atq {
	SIMPLEQ_ENTRY(ixl_atq)	  iatq_entry;
	struct ixl_aq_desc	  iatq_desc;
	void			 *iatq_arg;
	void			(*iatq_fn)(struct ixl_softc *, void *);
};
SIMPLEQ_HEAD(ixl_atq_list, ixl_atq);

#define delaymsec(_ms)	delay(1000 * (_ms))

static void	ixl_clear_hw(struct ixl_softc *);
static int	ixl_pf_reset(struct ixl_softc *);

static int	ixl_dmamem_alloc(struct ixl_softc *, struct ixl_dmamem *,
		    bus_size_t, u_int);
static void	ixl_dmamem_free(struct ixl_softc *, struct ixl_dmamem *);

static int	ixl_arq_fill(struct ixl_softc *);
static void	ixl_arq_unfill(struct ixl_softc *);

static int	ixl_atq_poll(struct ixl_softc *, struct ixl_aq_desc *,
		    unsigned int);
static void	ixl_atq_set(struct ixl_atq *,
		    void (*)(struct ixl_softc *, void *), void *);
static void	ixl_atq_post(struct ixl_softc *, struct ixl_atq *);
static void	ixl_atq_done(struct ixl_softc *);
static void	ixl_atq_exec(struct ixl_softc *, struct ixl_atq *,
		    const char *);
static int	ixl_get_version(struct ixl_softc *);
static int	ixl_pxe_clear(struct ixl_softc *);
static int	ixl_lldp_shut(struct ixl_softc *);
static int	ixl_get_mac(struct ixl_softc *);
static int	ixl_get_switch_config(struct ixl_softc *);
static int	ixl_phy_mask_ints(struct ixl_softc *);
static int	ixl_get_phy_abilities(struct ixl_softc *, uint64_t *);
static int	ixl_restart_an(struct ixl_softc *);
static int	ixl_hmc(struct ixl_softc *);
static void	ixl_hmc_free(struct ixl_softc *);
static int	ixl_get_vsi(struct ixl_softc *);
static int	ixl_set_vsi(struct ixl_softc *);
static int	ixl_get_link_status(struct ixl_softc *);
static int	ixl_set_link_status(struct ixl_softc *,
		    const struct ixl_aq_desc *);
static void	ixl_arq(void *);
static void	ixl_hmc_pack(void *, const void *,
		    const struct ixl_hmc_pack *, unsigned int);

static int	ixl_match(struct device *, void *, void *);
static void	ixl_attach(struct device *, struct device *, void *);

static void	ixl_media_add(struct ixl_softc *, uint64_t);
static int	ixl_media_change(struct ifnet *);
static void	ixl_media_status(struct ifnet *, struct ifmediareq *);
static void	ixl_watchdog(struct ifnet *);
static int	ixl_ioctl(struct ifnet *, u_long, caddr_t);
static void	ixl_start(struct ifqueue *);
static int	ixl_intr(void *);
static int	ixl_up(struct ixl_softc *);
static int	ixl_down(struct ixl_softc *);
static int	ixl_iff(struct ixl_softc *);

static struct ixl_tx_ring *
		ixl_txr_alloc(struct ixl_softc *, unsigned int);
static void	ixl_txr_qdis(struct ixl_softc *, struct ixl_tx_ring *, int);
static void	ixl_txr_config(struct ixl_softc *, struct ixl_tx_ring *);
static int	ixl_txr_enabled(struct ixl_softc *, struct ixl_tx_ring *);
static int	ixl_txr_disabled(struct ixl_softc *, struct ixl_tx_ring *);
static void	ixl_txr_unconfig(struct ixl_softc *, struct ixl_tx_ring *);
static void	ixl_txr_clean(struct ixl_softc *, struct ixl_tx_ring *);
static void	ixl_txr_free(struct ixl_softc *, struct ixl_tx_ring *);
static int	ixl_txeof(struct ixl_softc *, struct ifqueue *);

static struct ixl_rx_ring *
		ixl_rxr_alloc(struct ixl_softc *, unsigned int);
static void	ixl_rxr_config(struct ixl_softc *, struct ixl_rx_ring *);
static int	ixl_rxr_enabled(struct ixl_softc *, struct ixl_rx_ring *);
static int	ixl_rxr_disabled(struct ixl_softc *, struct ixl_rx_ring *);
static void	ixl_rxr_unconfig(struct ixl_softc *, struct ixl_rx_ring *);
static void	ixl_rxr_clean(struct ixl_softc *, struct ixl_rx_ring *);
static void	ixl_rxr_free(struct ixl_softc *, struct ixl_rx_ring *);
static int	ixl_rxeof(struct ixl_softc *, struct ifiqueue *);
static void	ixl_rxfill(struct ixl_softc *, struct ixl_rx_ring *);
static void	ixl_rxrefill(void *);

struct cfdriver ixl_cd = {
	NULL,
	"ixl",
	DV_IFNET,
};

struct cfattach ixl_ca = {
	sizeof(struct ixl_softc),
	ixl_match,
	ixl_attach,
};

static const struct ixl_phy_type ixl_phy_type_map[] = {
	{ 1ULL << IXL_PHY_TYPE_SGMII,		IFM_1000_SGMII },
	{ 1ULL << IXL_PHY_TYPE_1000BASE_KX,	IFM_1000_KX },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_KX4,	IFM_10G_KX4 },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_KR,	IFM_10G_KR },
	{ 1ULL << IXL_PHY_TYPE_40GBASE_KR4,	IFM_40G_KR4 },
	{ 1ULL << IXL_PHY_TYPE_XAUI |
	  1ULL << IXL_PHY_TYPE_XFI,		IFM_10G_CX4 },
	{ 1ULL << IXL_PHY_TYPE_SFI,		IFM_10G_SFI },
	{ 1ULL << IXL_PHY_TYPE_XLAUI |
	  1ULL << IXL_PHY_TYPE_XLPPI,		IFM_40G_XLPPI },
	{ 1ULL << IXL_PHY_TYPE_40GBASE_CR4_CU |
	  1ULL << IXL_PHY_TYPE_40GBASE_CR4,	IFM_40G_CR4 },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_CR1_CU |
	  1ULL << IXL_PHY_TYPE_10GBASE_CR1,	IFM_10G_CR1 },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_AOC,	IFM_10G_AOC },
	{ 1ULL << IXL_PHY_TYPE_40GBASE_AOC,	IFM_40G_AOC },
	{ 1ULL << IXL_PHY_TYPE_100BASE_TX,	IFM_100_TX },
	{ 1ULL << IXL_PHY_TYPE_1000BASE_T_OPTICAL |
	  1ULL << IXL_PHY_TYPE_1000BASE_T,	IFM_1000_T },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_T,	IFM_10G_T },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_SR,	IFM_10G_SR },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_LR,	IFM_10G_LR },
	{ 1ULL << IXL_PHY_TYPE_10GBASE_SFPP_CU,	IFM_10G_SFP_CU },
	{ 1ULL << IXL_PHY_TYPE_40GBASE_SR4,	IFM_40G_SR4 },
	{ 1ULL << IXL_PHY_TYPE_40GBASE_LR4,	IFM_40G_LR4 },
	{ 1ULL << IXL_PHY_TYPE_1000BASE_SX,	IFM_1000_SX },
	{ 1ULL << IXL_PHY_TYPE_1000BASE_LX,	IFM_1000_LX },
	{ 1ULL << IXL_PHY_TYPE_20GBASE_KR2,	IFM_20G_KR2 },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_KR,	IFM_25G_KR },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_CR,	IFM_25G_CR },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_SR,	IFM_25G_SR },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_LR,	IFM_25G_LR },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_AOC,	IFM_25G_AOC },
	{ 1ULL << IXL_PHY_TYPE_25GBASE_ACC,	IFM_25G_CR },
};

static const struct ixl_speed_type ixl_speed_type_map[] = {
	{ IXL_AQ_PHY_LINK_SPEED_40GB,		IF_Gbps(40) },
	{ IXL_AQ_PHY_LINK_SPEED_25GB,		IF_Gbps(25) },
	{ IXL_AQ_PHY_LINK_SPEED_20GB,		IF_Gbps(20) },
	{ IXL_AQ_PHY_LINK_SPEED_10GB,		IF_Gbps(10) },
	{ IXL_AQ_PHY_LINK_SPEED_1000MB,		IF_Mbps(1000) },
	{ IXL_AQ_PHY_LINK_SPEED_100MB,		IF_Mbps(100) },
};

static const struct ixl_aq_regs ixl_pf_aq_regs = {
	.atq_tail	= I40E_PF_ATQT,
	.atq_tail_mask	= I40E_PF_ATQT_ATQT_MASK,
	.atq_head	= I40E_PF_ATQH,
	.atq_head_mask	= I40E_PF_ATQH_ATQH_MASK,
	.atq_len	= I40E_PF_ATQLEN,
	.atq_bal	= I40E_PF_ATQBAL,
	.atq_bah	= I40E_PF_ATQBAH,
	.atq_len_enable	= I40E_PF_ATQLEN_ATQENABLE_MASK,

	.arq_tail	= I40E_PF_ARQT,
	.arq_tail_mask	= I40E_PF_ARQT_ARQT_MASK,
	.arq_head	= I40E_PF_ARQH,
	.arq_head_mask	= I40E_PF_ARQH_ARQH_MASK,
	.arq_len	= I40E_PF_ARQLEN,
	.arq_bal	= I40E_PF_ARQBAL,
	.arq_bah	= I40E_PF_ARQBAH,
	.arq_len_enable	= I40E_PF_ARQLEN_ARQENABLE_MASK,
};

#ifdef notyet
static const struct ixl_aq_regs ixl_vf_aq_regs = {
	.atq_tail	= I40E_VF_ATQT1,
	.atq_tail_mask	= I40E_VF_ATQT1_ATQT_MASK;
	.atq_head	= I40E_VF_ATQH1,
	.atq_head_mask	= I40E_VF_ARQH1_ARQH_MASK;
	.atq_len	= I40E_VF_ATQLEN1,
	.atq_bal	= I40E_VF_ATQBAL1,
	.atq_bah	= I40E_VF_ATQBAH1,
	.atq_len_enable	= I40E_VF_ATQLEN1_ATQENABLE_MASK,

	.arq_tail	= I40E_VF_ARQT1,
	.arq_tail_mask	= I40E_VF_ARQT1_ARQT_MASK;
	.arq_head	= I40E_VF_ARQH1,
	.arq_head_mask	= I40E_VF_ARQH1_ARQH_MASK;
	.arq_len	= I40E_VF_ARQLEN1,
	.arq_bal	= I40E_VF_ARQBAL1,
	.arq_bah	= I40E_VF_ARQBAH1,
	.arq_len_enable	= I40E_VF_ARQLEN1_ARQENABLE_MASK,
};
#endif

#define ixl_rd(_s, _r) \
	bus_space_read_4((_s)->sc_memt, (_s)->sc_memh, (_r))
#define ixl_wr(_s, _r, _v) \
	bus_space_write_4((_s)->sc_memt, (_s)->sc_memh, (_r), (_v))
#define ixl_barrier(_s, _r, _l, _o) \
	bus_space_barrier((_s)->sc_memt, (_s)->sc_memh, (_r), (_l), (_o))
#define ixl_intr_enable(_s) \
	ixl_wr((_s), I40E_PFINT_DYN_CTL0, I40E_PFINT_DYN_CTL0_INTENA_MASK | \
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK | \
	    (IXL_NOITR << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT))

#define ixl_nqueues(_sc)	(1 << (_sc)->sc_nqueues)

#ifdef __LP64__
#define ixl_dmamem_hi(_ixm)	(uint32_t)(IXL_DMA_DVA(_ixm) >> 32)
#else
#define ixl_dmamem_hi(_ixm)	0
#endif

#define ixl_dmamem_lo(_ixm) 	(uint32_t)IXL_DMA_DVA(_ixm)

static inline void
ixl_aq_dva(struct ixl_aq_desc *iaq, bus_addr_t addr)
{
#ifdef __LP64__
	htolem32(&iaq->iaq_param[2], addr >> 32);
#else
	iaq->iaq_param[2] = htole32(0);
#endif
	htolem32(&iaq->iaq_param[3], addr);
}

#if _BYTE_ORDER == _BIG_ENDIAN
#define HTOLE16(_x)	(uint16_t)(((_x) & 0xff) << 8 | ((_x) & 0xff00) >> 8)
#else
#define HTOLE16(_x)	(_x)
#endif

static const struct pci_matchid ixl_devices[] = {
#ifdef notyet
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_VF },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_VF_HV },
#endif
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X710_10G_SFP },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_40G_BP },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X710_10G_BP },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_QSFP_1 },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_QSFP_2 },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X710_10G_QSFP },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X710_10G_BASET },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_20G_BP_1 },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XL710_20G_BP_2 },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_X710_T4_10G },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XXV710_25G_BP },
	{ PCI_VENDOR_INTEL,	PCI_PRODUCT_INTEL_XXV710_25G_SFP28 },
};

static int
ixl_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, ixl_devices, nitems(ixl_devices)));
}

void
ixl_attach(struct device *parent, struct device *self, void *aux)
{
	struct ixl_softc *sc = (struct ixl_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct pci_attach_args *pa = aux;
	pcireg_t memtype;
	uint32_t port, ari, func;
	uint64_t phy_types = 0;
	int tries;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;
	sc->sc_aq_regs = &ixl_pf_aq_regs; /* VF? */

	sc->sc_nqueues = 0; /* 1 << 0 is 1 queue */
	sc->sc_tx_ring_ndescs = 1024;
	sc->sc_rx_ring_ndescs = 1024;

	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, IXL_PCIREG);
	if (pci_mapreg_map(pa, IXL_PCIREG, memtype, BUS_SPACE_MAP_PREFETCHABLE,
	    &sc->sc_memt, &sc->sc_memh, NULL, &sc->sc_mems, 0)) {
		printf(": unable to map registers\n");
		return;
	}

	sc->sc_base_queue = (ixl_rd(sc, I40E_PFLAN_QALLOC) &
	    I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
	    I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	printf(" %u", sc->sc_base_queue);

	ixl_clear_hw(sc);

	if (ixl_pf_reset(sc) == -1) {
		/* error printed by ixl_pf_reset */
		goto unmap;
	}

	port = ixl_rd(sc, I40E_PFGEN_PORTNUM);
	port &= I40E_PFGEN_PORTNUM_PORT_NUM_MASK;
	port >>= I40E_PFGEN_PORTNUM_PORT_NUM_SHIFT;
	printf(": port %u", port);

	ari = ixl_rd(sc, I40E_GLPCI_CAPSUP);
	ari &= I40E_GLPCI_CAPSUP_ARI_EN_MASK;
	ari >>= I40E_GLPCI_CAPSUP_ARI_EN_SHIFT;

	func = ixl_rd(sc, I40E_PF_FUNC_RID);
	func &= I40E_GLPCI_CAPSUP_ARI_EN_MASK;
	func >>= I40E_GLPCI_CAPSUP_ARI_EN_SHIFT;

	sc->sc_pf_id = func & (ari ? 0xff : 0x7);

	/* initialise the adminq */

	mtx_init(&sc->sc_atq_mtx, IPL_NET);

	if (ixl_dmamem_alloc(sc, &sc->sc_atq,
	    sizeof(struct ixl_aq_desc) * IXL_AQ_NUM, IXL_AQ_ALIGN) != 0) {
		printf("\n" "%s: unable to allocate atq\n", DEVNAME(sc));
		goto unmap;
	}

	SIMPLEQ_INIT(&sc->sc_arq_idle);
	SIMPLEQ_INIT(&sc->sc_arq_live);
	if_rxr_init(&sc->sc_arq_ring, 2, IXL_AQ_NUM - 1);
	task_set(&sc->sc_arq_task, ixl_arq, sc);
	sc->sc_arq_cons = 0;
	sc->sc_arq_prod = 0;

	if (ixl_dmamem_alloc(sc, &sc->sc_arq,
	    sizeof(struct ixl_aq_desc) * IXL_AQ_NUM, IXL_AQ_ALIGN) != 0) {
		printf("\n" "%s: unable to allocate arq\n", DEVNAME(sc));
		goto free_atq;
	}

	if (!ixl_arq_fill(sc)) {
		printf("\n" "%s: unable to fill arq descriptors\n",
		    DEVNAME(sc));
		goto free_arq;
	}

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_arq),
	    0, IXL_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

 	for (tries = 0; tries < 10; tries++) { 
		int rv;

		sc->sc_atq_cons = 0;
		sc->sc_atq_prod = 0;

		ixl_wr(sc, sc->sc_aq_regs->atq_head, 0);
		ixl_wr(sc, sc->sc_aq_regs->arq_head, 0);
		ixl_wr(sc, sc->sc_aq_regs->atq_tail, 0);
		ixl_wr(sc, sc->sc_aq_regs->arq_tail, 0);

		ixl_barrier(sc, 0, sc->sc_mems, BUS_SPACE_BARRIER_WRITE);

		ixl_wr(sc, sc->sc_aq_regs->atq_bal,
		    ixl_dmamem_lo(&sc->sc_atq));
		ixl_wr(sc, sc->sc_aq_regs->atq_bah,
		    ixl_dmamem_hi(&sc->sc_atq));
		ixl_wr(sc, sc->sc_aq_regs->atq_len,
		    sc->sc_aq_regs->atq_len_enable | IXL_AQ_NUM);

		ixl_wr(sc, sc->sc_aq_regs->arq_bal,
		    ixl_dmamem_lo(&sc->sc_arq));
		ixl_wr(sc, sc->sc_aq_regs->arq_bah,
		    ixl_dmamem_hi(&sc->sc_arq));
		ixl_wr(sc, sc->sc_aq_regs->arq_len,
		    sc->sc_aq_regs->arq_len_enable | IXL_AQ_NUM);

		rv = ixl_get_version(sc);
		if (rv == 0)
			break;
		if (rv != ETIMEDOUT) {
			printf(", unable to get firmware version\n");
			goto shutdown;
		}

		delaymsec(100);
	}

	ixl_wr(sc, sc->sc_aq_regs->arq_tail, sc->sc_arq_prod);

	if (ixl_pxe_clear(sc) != 0) {
		/* error printed by ixl_pxe_clear */
		goto shutdown;
	}

	if (ixl_get_mac(sc) != 0) {
		/* error printed by ixl_get_mac */
		goto shutdown;
	}

	if (pci_intr_map_msi(pa, &sc->sc_ih) != 0 &&
	    pci_intr_map(pa, &sc->sc_ih) != 0) {
		printf(", unable to map interrupt\n");
		goto shutdown;
	}

	printf(", %s, address %s\n", pci_intr_string(sc->sc_pc, sc->sc_ih),
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	if (ixl_hmc(sc) != 0) {
		/* error printed by ixl_hmc */
		goto shutdown;
	}

	if (ixl_lldp_shut(sc) != 0) {
		/* error printed by ixl_lldp_shut */
		goto free_hmc;
	}

	if (ixl_phy_mask_ints(sc) != 0) {
		/* error printed by ixl_phy_mask_ints */
		goto free_hmc;
	}

	if (ixl_restart_an(sc) != 0) {
		/* error printed by ixl_restart_an */
		goto free_hmc;
	}

	if (ixl_get_switch_config(sc) != 0) {
		/* error printed by ixl_get_switch_config */
		goto free_hmc;
	}

	if (ixl_get_phy_abilities(sc, &phy_types) != 0) {
		/* error printed by ixl_get_phy_abilities */
		goto free_hmc;
	}

	if (ixl_get_link_status(sc) != 0) {
		/* error printed by ixl_get_link_status */
		goto free_hmc;
	}

	if (ixl_dmamem_alloc(sc, &sc->sc_vsi,
	    sizeof(struct ixl_aq_vsi_data), 8) != 0) {
		printf("%s: unable to allocate VSI data\n", DEVNAME(sc));
		goto free_hmc;
	}

	if (ixl_get_vsi(sc) != 0) {
		/* error printed by ixl_get_vsi */
		goto free_vsi;
	}

	if (ixl_set_vsi(sc) != 0) {
		/* error printed by ixl_set_vsi */
		goto free_vsi;
	}

	sc->sc_ihc = pci_intr_establish(sc->sc_pc, sc->sc_ih,
	    IPL_NET | IPL_MPSAFE, ixl_intr, sc, DEVNAME(sc));
	if (sc->sc_ihc == NULL) {
		printf("%s: unable to establish interrupt handler\n",
		    DEVNAME(sc));
		goto free_vsi;
	}

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = ixl_ioctl;
	ifp->if_qstart = ixl_start;
	ifp->if_watchdog = ixl_watchdog;
	ifp->if_hardmtu = IXL_HARDMTU;
	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);

	ifp->if_capabilities = IFCAP_VLAN_MTU;
#if 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#endif

	ifmedia_init(&sc->sc_media, 0, ixl_media_change, ixl_media_status);

	ixl_media_add(sc, phy_types);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	if_attach_queues(ifp, ixl_nqueues(sc));
	if_attach_iqueues(ifp, ixl_nqueues(sc));

	ixl_wr(sc, I40E_PFINT_ICR0_ENA,
	    I40E_PFINT_ICR0_ENA_LINK_STAT_CHANGE_MASK |
	    I40E_PFINT_ICR0_ENA_ADMINQ_MASK);
	ixl_wr(sc, I40E_PFINT_STAT_CTL0,
	    IXL_NOITR << I40E_PFINT_STAT_CTL0_OTHER_ITR_INDX_SHIFT);

	ixl_intr_enable(sc);

	return;

free_vsi:
	ixl_dmamem_free(sc, &sc->sc_vsi);
free_hmc:
	ixl_hmc_free(sc);
shutdown:
	ixl_wr(sc, sc->sc_aq_regs->atq_head, 0);
	ixl_wr(sc, sc->sc_aq_regs->arq_head, 0);
	ixl_wr(sc, sc->sc_aq_regs->atq_tail, 0);
	ixl_wr(sc, sc->sc_aq_regs->arq_tail, 0);

	ixl_wr(sc, sc->sc_aq_regs->atq_bal, 0);
	ixl_wr(sc, sc->sc_aq_regs->atq_bah, 0);
	ixl_wr(sc, sc->sc_aq_regs->atq_len, 0);

	ixl_wr(sc, sc->sc_aq_regs->arq_bal, 0);
	ixl_wr(sc, sc->sc_aq_regs->arq_bah, 0);
	ixl_wr(sc, sc->sc_aq_regs->arq_len, 0);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_arq),
	    0, IXL_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	ixl_arq_unfill(sc);
free_arq:
	ixl_dmamem_free(sc, &sc->sc_arq);
free_atq:
	ixl_dmamem_free(sc, &sc->sc_atq);
unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

static void
ixl_media_add(struct ixl_softc *sc, uint64_t phy_types)
{
	struct ifmedia *ifm = &sc->sc_media;
	const struct ixl_phy_type *itype;
	unsigned int i;

	for (i = 0; i < nitems(ixl_phy_type_map); i++) {
		itype = &ixl_phy_type_map[i];

		if (ISSET(phy_types, itype->phy_type))
			ifmedia_add(ifm, IFM_ETHER | itype->ifm_type, 0, NULL);
	}
}

static int
ixl_media_change(struct ifnet *ifp)
{
	/* ignore? */
	return (EOPNOTSUPP);
}

static void
ixl_media_status(struct ifnet *ifp, struct ifmediareq *ifm)
{
	struct ixl_softc *sc = ifp->if_softc;

	NET_ASSERT_LOCKED();

	ifm->ifm_status = sc->sc_media_status;
	ifm->ifm_active = sc->sc_media_active;
}

static void
ixl_watchdog(struct ifnet *ifp)
{

}

int
ixl_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ixl_softc *sc = (struct ixl_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				error = ixl_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ixl_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

#if 0
	case SIOCGIFRXR:
		error = ixl_rxrinfo(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;
#endif

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	if (error == ENETRESET)
		error = ixl_iff(sc);

	return (error);
}

static inline void *
ixl_hmc_kva(struct ixl_softc *sc, unsigned int type, unsigned int i)
{
	uint8_t *kva = IXL_DMA_KVA(&sc->sc_hmc_pd);
	struct ixl_hmc_entry *e = &sc->sc_hmc_entries[type];

	if (i >= e->hmc_count)
		return (NULL);

	kva += e->hmc_base;
	kva += i * e->hmc_size;

	return (kva);
}

static inline size_t
ixl_hmc_len(struct ixl_softc *sc, unsigned int type)
{
	struct ixl_hmc_entry *e = &sc->sc_hmc_entries[type];

	return (e->hmc_size);
}

static int
ixl_up(struct ixl_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ixl_rx_ring *rxr;
	struct ixl_tx_ring *txr;
	unsigned int nqueues, i;
	uint32_t reg;
	int rv = ENOMEM;

	nqueues = ixl_nqueues(sc);
	KASSERT(nqueues == 1); /* XXX */

	/* allocation is the only thing that can fail, so do it up front */
	for (i = 0; i < nqueues; i++) {
		rxr = ixl_rxr_alloc(sc, i);
		if (rxr == NULL)
			goto free;

		txr = ixl_txr_alloc(sc, i);
		if (txr == NULL) {
			ixl_rxr_free(sc, rxr);
			goto free;
		}

		ifp->if_iqs[i]->ifiq_softc = rxr;
		ifp->if_ifqs[i]->ifq_softc = txr;
	}

	/* XXX wait 50ms from completion of last RX queue disable */

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		ixl_txr_qdis(sc, txr, 1);

		ixl_rxr_config(sc, rxr);
		ixl_txr_config(sc, txr);

		ixl_wr(sc, I40E_QTX_CTL(i), I40E_QTX_CTL_PF_QUEUE |
		    (sc->sc_pf_id << I40E_QTX_CTL_PF_INDX_SHIFT));

		ixl_wr(sc, rxr->rxr_tail, 0);
		ixl_rxfill(sc, rxr);

		reg = ixl_rd(sc, I40E_QRX_ENA(i));
		SET(reg, I40E_QRX_ENA_QENA_REQ_MASK);
		ixl_wr(sc, I40E_QRX_ENA(i), reg);

		reg = ixl_rd(sc, I40E_QTX_ENA(i));
		SET(reg, I40E_QTX_ENA_QENA_REQ_MASK);
		ixl_wr(sc, I40E_QTX_ENA(i), reg);
	}

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		if (ixl_rxr_enabled(sc, rxr) != 0)
			goto down;

		if (ixl_txr_enabled(sc, txr) != 0)
			goto down;
	}

	SET(ifp->if_flags, IFF_RUNNING);

#if 0
	reg = ixl_rd(sc, I40E_QINT_RQCTL(I40E_INTR_NOTX_QUEUE));
	SET(reg, I40E_QINT_RQCTL_CAUSE_ENA_MASK);
	ixl_wr(sc, I40E_QINT_RQCTL(I40E_INTR_NOTX_QUEUE), reg);

	reg = ixl_rd(sc, I40E_QINT_TQCTL(I40E_INTR_NOTX_QUEUE));
	SET(reg, I40E_QINT_TQCTL_CAUSE_ENA_MASK);
	ixl_wr(sc, I40E_QINT_TQCTL(I40E_INTR_NOTX_QUEUE), reg);
#endif

	ixl_wr(sc, I40E_PFINT_LNKLST0,
	    (I40E_INTR_NOTX_QUEUE << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT) |
	    (I40E_QUEUE_TYPE_RX << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT));

	ixl_wr(sc, I40E_QINT_RQCTL(I40E_INTR_NOTX_QUEUE),
	    (I40E_INTR_NOTX_INTR << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
	    (I40E_ITR_INDEX_RX << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
	    (I40E_INTR_NOTX_RX_QUEUE << I40E_QINT_RQCTL_MSIX0_INDX_SHIFT) |
	    (I40E_INTR_NOTX_QUEUE << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
	    (I40E_QUEUE_TYPE_TX << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT));

	ixl_wr(sc, I40E_QINT_TQCTL(I40E_INTR_NOTX_QUEUE),
	    (I40E_INTR_NOTX_INTR << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
	    (I40E_ITR_INDEX_TX << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
	    (I40E_INTR_NOTX_TX_QUEUE << I40E_QINT_TQCTL_MSIX0_INDX_SHIFT) |
	    (I40E_QUEUE_TYPE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
	    (I40E_QUEUE_TYPE_RX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT));

	ixl_wr(sc, I40E_PFINT_ITR0(0), 0x7a);
	ixl_wr(sc, I40E_PFINT_ITR0(1), 0x7a);
	ixl_wr(sc, I40E_PFINT_ITR0(2), 0);

	printf("%s: info %08x data %08x\n", DEVNAME(sc),
	    ixl_rd(sc, I40E_PFHMC_ERRORINFO),
	    ixl_rd(sc, I40E_PFHMC_ERRORDATA));

	return (ENETRESET);

free:
	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		if (rxr == NULL) {
			/*
			 * tx and rx get set at the same time, so if one
			 * is NULL, the other is too.
			 */
			continue;
		}

		ixl_txr_free(sc, txr);
		ixl_rxr_free(sc, rxr);
	}
	return (rv);
down:
	ixl_down(sc);
	return (ETIMEDOUT);
}

static int
ixl_iff(struct ixl_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ixl_atq iatq;
	struct ixl_aq_desc *iaq;
	struct ixl_aq_vsi_promisc_param *param;

#if 0
	if (!ISSET(ifp->if_flags, IFF_ALLMULTI))
		return (0);
#endif

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return (0);

	memset(&iatq, 0, sizeof(iatq));

	iaq = &iatq.iatq_desc;
	iaq->iaq_opcode = htole16(IXL_AQ_OP_SET_VSI_PROMISC);

	param = (struct ixl_aq_vsi_promisc_param *)&iaq->iaq_param;
	param->flags = htole16(IXL_AQ_VSI_PROMISC_FLAG_BCAST);
//	if (ISSET(ifp->if_flags, IFF_PROMISC)) {
		param->flags |= htole16(IXL_AQ_VSI_PROMISC_FLAG_UCAST |
		    IXL_AQ_VSI_PROMISC_FLAG_MCAST);
//	}
	param->valid_flags = htole16(IXL_AQ_VSI_PROMISC_FLAG_UCAST |
	    IXL_AQ_VSI_PROMISC_FLAG_MCAST | IXL_AQ_VSI_PROMISC_FLAG_BCAST);
	param->seid = sc->sc_seid;

	ixl_atq_exec(sc, &iatq, "ixliff");

	if (iaq->iaq_retval != htole16(IXL_AQ_RC_OK))
		return (EIO);

	return (0);
}

static int
ixl_down(struct ixl_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ixl_rx_ring *rxr;
	struct ixl_tx_ring *txr;
	unsigned int nqueues, i;
	uint32_t reg;
	int error = 0;

	nqueues = ixl_nqueues(sc);

	CLR(ifp->if_flags, IFF_RUNNING);

	/* mask interrupts */
	reg = ixl_rd(sc, I40E_QINT_RQCTL(I40E_INTR_NOTX_QUEUE));
	CLR(reg, I40E_QINT_RQCTL_CAUSE_ENA_MASK);
	ixl_wr(sc, I40E_QINT_RQCTL(I40E_INTR_NOTX_QUEUE), reg);

	reg = ixl_rd(sc, I40E_QINT_TQCTL(I40E_INTR_NOTX_QUEUE));
	CLR(reg, I40E_QINT_TQCTL_CAUSE_ENA_MASK);
	ixl_wr(sc, I40E_QINT_TQCTL(I40E_INTR_NOTX_QUEUE), reg);

	ixl_wr(sc, I40E_PFINT_LNKLST0, I40E_QUEUE_TYPE_EOL);

	/* make sure the no hw generated work is still in flight */
	intr_barrier(sc->sc_ihc);
	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		ixl_txr_qdis(sc, txr, 0);

		ifiq_barrier(ifp->if_iqs[i]);
		ifq_barrier(ifp->if_ifqs[i]);

		if (!timeout_del(&rxr->rxr_refill))
			timeout_barrier(&rxr->rxr_refill);
	}

	/* XXX wait at least 400 usec for all tx queues in one go */
	delay(500);

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		reg = ixl_rd(sc, I40E_QTX_ENA(i));
		CLR(reg, I40E_QTX_ENA_QENA_REQ_MASK);
		ixl_wr(sc, I40E_QTX_ENA(i), reg);

		reg = ixl_rd(sc, I40E_QRX_ENA(i));
		CLR(reg, I40E_QRX_ENA_QENA_REQ_MASK);
		ixl_wr(sc, I40E_QRX_ENA(i), reg);
	}

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		if (ixl_txr_disabled(sc, txr) != 0)
			error = ETIMEDOUT;

		if (ixl_rxr_disabled(sc, rxr) != 0)
			error = ETIMEDOUT;
	}

	if (error) {
	printf("%s: info %08x data %08x\n", DEVNAME(sc),
	    ixl_rd(sc, I40E_PFHMC_ERRORINFO),
	    ixl_rd(sc, I40E_PFHMC_ERRORDATA));

		printf("%s: failed to shut down rings\n", DEVNAME(sc));
		return (error);
	}

	for (i = 0; i < nqueues; i++) {
		rxr = ifp->if_iqs[i]->ifiq_softc;
		txr = ifp->if_ifqs[i]->ifq_softc;

		ixl_txr_unconfig(sc, txr);
		ixl_rxr_unconfig(sc, rxr);

		ixl_txr_clean(sc, txr);
		ixl_rxr_clean(sc, rxr);

		ixl_txr_free(sc, txr);
		ixl_rxr_free(sc, rxr);

		ifp->if_iqs[i]->ifiq_softc = NULL;
		ifp->if_ifqs[i]->ifq_softc =  NULL;
	}

	return (0);
}

static struct ixl_tx_ring *
ixl_txr_alloc(struct ixl_softc *sc, unsigned int qid)
{
	struct ixl_tx_ring *txr;
	struct ixl_tx_map *maps, *txm;
	unsigned int i;

	txr = malloc(sizeof(*txr), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (txr == NULL)
		return (NULL);

	maps = mallocarray(sizeof(*maps),
	    sc->sc_tx_ring_ndescs, M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (maps == NULL)
		goto free;

	if (ixl_dmamem_alloc(sc, &txr->txr_mem,
	    sizeof(struct ixl_tx_desc) * sc->sc_tx_ring_ndescs,
	    IXL_TX_QUEUE_ALIGN) != 0)
		goto freemap;

	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (bus_dmamap_create(sc->sc_dmat,
		    IXL_HARDMTU, IXL_TX_PKT_DESCS, IXL_HARDMTU, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &txm->txm_map) != 0)
			goto uncreate;

		txm->txm_eop = -1;
		txm->txm_m = NULL;
	}

	txr->txr_cons = txr->txr_prod = 0;
	txr->txr_maps = maps;

	txr->txr_tail = I40E_QTX_TAIL(qid);
	txr->txr_qid = qid;

	return (txr);

uncreate:
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (txm->txm_map == NULL)
			continue;

		bus_dmamap_destroy(sc->sc_dmat, txm->txm_map);
	}

	ixl_dmamem_free(sc, &txr->txr_mem);
freemap:
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_tx_ring_ndescs);
free:
	free(txr, M_DEVBUF, sizeof(*txr));
	return (NULL);
}

static void
ixl_txr_qdis(struct ixl_softc *sc, struct ixl_tx_ring *txr, int enable)
{
	unsigned int qid;
	bus_size_t reg;
	uint32_t r;

	qid = txr->txr_qid + sc->sc_base_queue;
	reg = I40E_GLLAN_TXPRE_QDIS(qid / 128);
	qid %= 128;

	r = ixl_rd(sc, reg);
	CLR(r, I40E_GLLAN_TXPRE_QDIS_QINDX_MASK);
	SET(r, qid << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
	SET(r, enable ? I40E_GLLAN_TXPRE_QDIS_CLEAR_QDIS_MASK :
	    I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK);
	ixl_wr(sc, reg, r);
}

static void
ixl_txr_config(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	struct ixl_hmc_txq txq;
	struct ixl_aq_vsi_data *data = IXL_DMA_KVA(&sc->sc_vsi);
	void *hmc;

	memset(&txq, 0, sizeof(txq));
	txq.head = htole16(0);
	txq.new_context = 1;
	htolem64(&txq.base,
	    IXL_DMA_DVA(&txr->txr_mem) / IXL_HMC_TXQ_BASE_UNIT);
	txq.head_wb_ena = IXL_HMC_TXQ_DESC_WB;
	htolem16(&txq.qlen, sc->sc_tx_ring_ndescs);
	txq.tphrdesc_ena = 0;
	txq.tphrpacket_ena = 0;
	txq.tphwdesc_ena = 0;
	txq.rdylist = data->qs_handle[0];

	hmc = ixl_hmc_kva(sc, IXL_HMC_LAN_TX, txr->txr_qid);
	memset(hmc, 0, ixl_hmc_len(sc, IXL_HMC_LAN_TX));
	ixl_hmc_pack(hmc, &txq, ixl_hmc_pack_txq, nitems(ixl_hmc_pack_txq));
}

static void
ixl_txr_unconfig(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	void *hmc;

	hmc = ixl_hmc_kva(sc, IXL_HMC_LAN_TX, txr->txr_qid);
	memset(hmc, 0, ixl_hmc_len(sc, IXL_HMC_LAN_TX));
}

static void
ixl_txr_clean(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	struct ixl_tx_map *maps, *txm;
	bus_dmamap_t map;
	unsigned int i;

	maps = txr->txr_maps;
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		if (txm->txm_m == NULL)
			continue;

		map = txm->txm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);

		m_freem(txm->txm_m);
		txm->txm_m = NULL;
	}
}

static int
ixl_txr_enabled(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	bus_size_t ena = I40E_QTX_ENA(txr->txr_qid);
	uint32_t reg;
	int i;

	for (i = 0; i < 10; i++) {
		reg = ixl_rd(sc, ena);
		if (ISSET(reg, I40E_QTX_ENA_QENA_STAT_MASK))
			return (0);

		delaymsec(10);
	}

	return (ETIMEDOUT);
}

static int
ixl_txr_disabled(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	bus_size_t ena = I40E_QTX_ENA(txr->txr_qid);
	uint32_t reg;
	int i;

	for (i = 0; i < 20; i++) {
		reg = ixl_rd(sc, ena);
		if (ISSET(reg, I40E_QTX_ENA_QENA_STAT_MASK) == 0)
			return (0);

		delaymsec(10);
	}

	return (ETIMEDOUT);
}

static void
ixl_txr_free(struct ixl_softc *sc, struct ixl_tx_ring *txr)
{
	struct ixl_tx_map *maps, *txm;
	unsigned int i;

	maps = txr->txr_maps;
	for (i = 0; i < sc->sc_tx_ring_ndescs; i++) {
		txm = &maps[i];

		bus_dmamap_destroy(sc->sc_dmat, txm->txm_map);
	}

	ixl_dmamem_free(sc, &txr->txr_mem);
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_tx_ring_ndescs);
	free(txr, M_DEVBUF, sizeof(*txr));
}

static inline int
ixl_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m)
{
	int error;

	error = bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT);
	if (error != EFBIG || m_defrag(m, M_DONTWAIT) != 0)
		return (error);

	return (bus_dmamap_load_mbuf(dmat, map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT));
}

static void
ixl_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct ixl_softc *sc = ifp->if_softc;
	struct ixl_tx_ring *txr = ifq->ifq_softc;
	struct ixl_tx_desc *ring, *txd;
	struct ixl_tx_map *txm;
	bus_dmamap_t map;
	struct mbuf *m;
	uint64_t cmd;
	unsigned int prod, free, last, i;
	unsigned int mask;
	int post = 0;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	if (!LINK_STATE_IS_UP(ifp->if_link_state)) {
		ifq_purge(ifq);
		return;
	}

	prod = txr->txr_prod;
	free = txr->txr_cons;
	if (free <= prod)
		free += sc->sc_tx_ring_ndescs;
	free -= prod;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&txr->txr_mem),
	    0, IXL_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_POSTWRITE);

	ring = IXL_DMA_KVA(&txr->txr_mem);
	mask = sc->sc_tx_ring_ndescs - 1;

	for (;;) {
		if (free <= IXL_TX_PKT_DESCS) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL)
			break;

		txm = &txr->txr_maps[prod];
		map = txm->txm_map;

		if (ixl_load_mbuf(sc->sc_dmat, map, m) != 0) {
			m_freem(m);
			continue;
		}

		bus_dmamap_sync(sc->sc_dmat, map, 0,
		    map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		for (i = 0; i < map->dm_nsegs; i++) {
			txd = &ring[prod];

			cmd = (uint64_t)map->dm_segs[i].ds_len <<
			    IXL_TX_DESC_BSIZE_SHIFT;
			cmd |= IXL_TX_DESC_DTYPE_DATA | IXL_TX_DESC_CMD_ICRC;

			htolem64(&txd->addr, map->dm_segs[i].ds_addr);
			htolem64(&txd->cmd, cmd);

			last = prod;

			prod++;
			prod &= mask;
		}
		cmd |= IXL_TX_DESC_CMD_EOP | IXL_TX_DESC_CMD_RS;
		htolem64(&txd->cmd, cmd);

		txm->txm_m = m;
		txm->txm_eop = last;

#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf)
			bpf_mtap_ether(if_bpf, m, BPF_DIRECTION_OUT);
#endif

		free -= i;
		post = 1;
	}

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&txr->txr_mem),
	    0, IXL_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_PREWRITE);

	if (post) {
		txr->txr_prod = prod;
		ixl_wr(sc, txr->txr_tail, prod);
	}
}

static int
ixl_txeof(struct ixl_softc *sc, struct ifqueue *ifq)
{
	struct ixl_tx_ring *txr = ifq->ifq_softc;
	struct ixl_tx_desc *ring, *txd;
	struct ixl_tx_map *txm;
	bus_dmamap_t map;
	unsigned int cons, prod, last;
	unsigned int mask;
	uint64_t dtype;
	int done = 0;

	prod = txr->txr_prod;
	cons = txr->txr_cons;

	if (cons == prod)
		return (0);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&txr->txr_mem),
	    0, IXL_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_POSTREAD);

	ring = IXL_DMA_KVA(&txr->txr_mem);
	mask = sc->sc_tx_ring_ndescs - 1;

	do {
		txm = &txr->txr_maps[cons];
		last = txm->txm_eop;
		txd = &ring[last];

		dtype = txd->cmd & htole64(IXL_TX_DESC_DTYPE_MASK);
		if (dtype != htole64(IXL_TX_DESC_DTYPE_DONE))
			break;

		map = txm->txm_map;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);
		m_freem(txm->txm_m);

		txm->txm_m = NULL;
		txm->txm_eop = -1;

		cons = last + 1;
		cons &= mask;

		done = 1;
	} while (cons != prod);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&txr->txr_mem),
	    0, IXL_DMA_LEN(&txr->txr_mem), BUS_DMASYNC_PREREAD);

	txr->txr_cons = cons;

	//ixl_enable(sc, txr->txr_msix);

	if (ifq_is_oactive(ifq))
		ifq_restart(ifq);

	return (done);
}

static struct ixl_rx_ring *
ixl_rxr_alloc(struct ixl_softc *sc, unsigned int qid)
{
	struct ixl_rx_ring *rxr;
	struct ixl_rx_map *maps, *rxm;
	unsigned int i;

	rxr = malloc(sizeof(*rxr), M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (rxr == NULL)
		return (NULL);

	maps = mallocarray(sizeof(*maps),
	    sc->sc_rx_ring_ndescs, M_DEVBUF, M_WAITOK|M_CANFAIL|M_ZERO);
	if (maps == NULL)
		goto free;

	if (ixl_dmamem_alloc(sc, &rxr->rxr_mem,
	    sizeof(struct ixl_rx_rd_desc_16) * sc->sc_rx_ring_ndescs,
	    IXL_RX_QUEUE_ALIGN) != 0)
		goto freemap;

	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (bus_dmamap_create(sc->sc_dmat,
		    IXL_HARDMTU, 1, IXL_HARDMTU, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &rxm->rxm_map) != 0)
			goto uncreate;

		rxm->rxm_m = NULL;
	}

	rxr->rxr_sc = sc;
	if_rxr_init(&rxr->rxr_acct, 17, sc->sc_rx_ring_ndescs - 1);
	timeout_set(&rxr->rxr_refill, ixl_rxrefill, rxr);
	rxr->rxr_cons = rxr->rxr_prod = 0;
	rxr->rxr_m_head = NULL;
	rxr->rxr_m_tail = &rxr->rxr_m_head;
	rxr->rxr_maps = maps;

	rxr->rxr_tail = I40E_QRX_TAIL(qid);
	rxr->rxr_qid = qid;

	return (rxr);

uncreate:
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (rxm->rxm_map == NULL)
			continue;

		bus_dmamap_destroy(sc->sc_dmat, rxm->rxm_map);
	}

	ixl_dmamem_free(sc, &rxr->rxr_mem);
freemap:
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_rx_ring_ndescs);
free:
	free(rxr, M_DEVBUF, sizeof(*rxr));
	return (NULL);
}

static void
ixl_rxr_clean(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	struct ixl_rx_map *maps, *rxm;
	bus_dmamap_t map;
	unsigned int i;

	if (!timeout_del(&rxr->rxr_refill))
		timeout_barrier(&rxr->rxr_refill);

	maps = rxr->rxr_maps;
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		if (rxm->rxm_m == NULL)
			continue;

		map = rxm->rxm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);

		m_freem(rxm->rxm_m);
		rxm->rxm_m = NULL;
	}

	m_freem(rxr->rxr_m_head);
	rxr->rxr_m_head = NULL;
	rxr->rxr_m_tail = &rxr->rxr_m_head;

	rxr->rxr_prod = rxr->rxr_cons = 0;
}

static int
ixl_rxr_enabled(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	bus_size_t ena = I40E_QRX_ENA(rxr->rxr_qid);
	uint32_t reg;
	int i;

	for (i = 0; i < 10; i++) {
		reg = ixl_rd(sc, ena);
		if (ISSET(reg, I40E_QRX_ENA_QENA_STAT_MASK))
			return (0);

		delaymsec(10);
	}

	return (ETIMEDOUT);
}

static int
ixl_rxr_disabled(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	bus_size_t ena = I40E_QRX_ENA(rxr->rxr_qid);
	uint32_t reg;
	int i;

	for (i = 0; i < 20; i++) {
		reg = ixl_rd(sc, ena);
		if (ISSET(reg, I40E_QRX_ENA_QENA_STAT_MASK) == 0)
			return (0);

		delaymsec(10);
	}

	return (ETIMEDOUT);
}

static void
ixl_rxr_config(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	struct ixl_hmc_rxq rxq;
	void *hmc;

	memset(&rxq, 0, sizeof(rxq));

	rxq.head = htole16(0);
	htolem64(&rxq.base,
	    IXL_DMA_DVA(&rxr->rxr_mem) / IXL_HMC_RXQ_BASE_UNIT);
	htolem16(&rxq.qlen, sc->sc_rx_ring_ndescs);
	rxq.dbuff = htole16(MCLBYTES / IXL_HMC_RXQ_DBUFF_UNIT);
	rxq.hbuff = 0;
	rxq.dtype = IXL_HMC_RXQ_DTYPE_NOSPLIT;
	rxq.dsize = IXL_HMC_RXQ_DSIZE_16;
	rxq.crcstrip = 1;
	rxq.l2sel = 0;
	rxq.showiv = 0;
	rxq.rxmax = htole16(MCLBYTES); /* XXX */
	rxq.tphrdesc_ena = 0;
	rxq.tphwdesc_ena = 0;
	rxq.tphdata_ena = 0;
	rxq.tphhead_ena = 0;
	rxq.lrxqthresh = 0;
	rxq.prefena = 1;

	hmc = ixl_hmc_kva(sc, IXL_HMC_LAN_RX, rxr->rxr_qid);
	memset(hmc, 0, ixl_hmc_len(sc, IXL_HMC_LAN_RX));
	ixl_hmc_pack(hmc, &rxq, ixl_hmc_pack_rxq, nitems(ixl_hmc_pack_rxq));
}

static void
ixl_rxr_unconfig(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	void *hmc;

	hmc = ixl_hmc_kva(sc, IXL_HMC_LAN_RX, rxr->rxr_qid);
	memset(hmc, 0, ixl_hmc_len(sc, IXL_HMC_LAN_RX));
}

static void
ixl_rxr_free(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	struct ixl_rx_map *maps, *rxm;
	unsigned int i;

	maps = rxr->rxr_maps;
	for (i = 0; i < sc->sc_rx_ring_ndescs; i++) {
		rxm = &maps[i];

		bus_dmamap_destroy(sc->sc_dmat, rxm->rxm_map);
	}

	ixl_dmamem_free(sc, &rxr->rxr_mem);
	free(maps, M_DEVBUF, sizeof(*maps) * sc->sc_rx_ring_ndescs);
	free(rxr, M_DEVBUF, sizeof(*rxr));
}

static int
ixl_rxeof(struct ixl_softc *sc, struct ifiqueue *ifiq)
{
	struct ixl_rx_ring *rxr = ifiq->ifiq_softc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct ixl_rx_wb_desc_16 *ring, *rxd;
	struct ixl_rx_map *rxm;
	bus_dmamap_t map;
	unsigned int cons, prod;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *m;
	uint64_t word;
	unsigned int len;
	unsigned int mask;
	int done = 0;

	prod = rxr->rxr_prod;
	cons = rxr->rxr_cons;

	if (cons == prod)
		return (0);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&rxr->rxr_mem),
	    0, IXL_DMA_LEN(&rxr->rxr_mem),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	ring = IXL_DMA_KVA(&rxr->rxr_mem);
	mask = sc->sc_rx_ring_ndescs - 1;

	do {
		rxd = &ring[cons];

		word = lemtoh64(&rxd->qword1);
		if (!ISSET(word, IXL_RX_DESC_DD))
			break;

		if_rxr_put(&rxr->rxr_acct, 1);

		rxm = &rxr->rxr_maps[cons];

		map = rxm->rxm_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, map);
		
		m = rxm->rxm_m;
		rxm->rxm_m = NULL;

		len = (word & IXL_RX_DESC_PLEN_MASK) >> IXL_RX_DESC_PLEN_SHIFT;
		m->m_len = len;
		m->m_pkthdr.len = 0;

		m->m_next = NULL;
		*rxr->rxr_m_tail = m;
		rxr->rxr_m_tail = &m->m_next;

		m = rxr->rxr_m_head;
		m->m_pkthdr.len += len;

		if (ISSET(word, IXL_RX_DESC_EOP)) {
			if (!ISSET(word,
			    IXL_RX_DESC_RXE | IXL_RX_DESC_OVERSIZE)) {
				ml_enqueue(&ml, m);
			} else {
				ifp->if_ierrors++; /* XXX */
				m_freem(m);
			}

			rxr->rxr_m_head = NULL;
			rxr->rxr_m_tail = &rxr->rxr_m_head;
		}

		cons++;
		cons &= mask;

		done = 1;
	} while (cons != prod);

	if (done) {
		rxr->rxr_cons = cons;
		ixl_rxfill(sc, rxr);
		if_input(ifp, &ml);
	}

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&rxr->rxr_mem),
	    0, IXL_DMA_LEN(&rxr->rxr_mem),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return (done);
}

static void
ixl_rxfill(struct ixl_softc *sc, struct ixl_rx_ring *rxr)
{
	struct ixl_rx_rd_desc_16 *ring, *rxd;
	struct ixl_rx_map *rxm;
	bus_dmamap_t map;
	struct mbuf *m;
	unsigned int prod;
	unsigned int slots;
	unsigned int mask;
	int post = 0;

	slots = if_rxr_get(&rxr->rxr_acct, sc->sc_rx_ring_ndescs);
	if (slots == 0)
		return;

	prod = rxr->rxr_prod;

	ring = IXL_DMA_KVA(&rxr->rxr_mem);
	mask = sc->sc_rx_ring_ndescs - 1;

	do {
		rxm = &rxr->rxr_maps[prod];

		m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES + ETHER_ALIGN);
		if (m == NULL)
			break;
		m->m_len = m->m_pkthdr.len = MCLBYTES + ETHER_ALIGN;
		m_adj(m, ETHER_ALIGN);

		map = rxm->rxm_map;

		if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			break;
		}

		rxm->rxm_m = m;

		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		rxd = &ring[prod];

		htolem64(&rxd->paddr, map->dm_segs[0].ds_addr);
		rxd->haddr = htole64(0);

		prod++;
		prod &= mask;

		post = 1;
	} while (--slots);

	if_rxr_put(&rxr->rxr_acct, slots);

	if (if_rxr_inuse(&rxr->rxr_acct) == 0)
		timeout_add(&rxr->rxr_refill, 1);
	else if (post) {
		rxr->rxr_prod = prod;
		ixl_wr(sc, rxr->rxr_tail, prod);
	}
}

void
ixl_rxrefill(void *arg)
{
	struct ixl_rx_ring *rxr = arg;
	struct ixl_softc *sc = rxr->rxr_sc;

	ixl_rxfill(sc, rxr);
}

static int
ixl_intr(void *xsc)
{
	struct ixl_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	uint32_t icr;
	int rv = 0;

	icr = ixl_rd(sc, I40E_PFINT_ICR0);

	if (ISSET(icr, I40E_PFINT_ICR0_ADMINQ_MASK)) {
		ixl_atq_done(sc);
		task_add(systq, &sc->sc_arq_task);
		rv = 1;
	}

	if (ISSET(icr, I40E_INTR_NOTX_RX_MASK))
		rv |= ixl_rxeof(sc, ifp->if_iqs[0]);
	if (ISSET(icr, I40E_INTR_NOTX_TX_MASK))
		rv |= ixl_txeof(sc, ifp->if_ifqs[0]);

	return (rv);
}

static void
ixl_arq_link_status(struct ixl_softc *sc, const struct ixl_aq_desc *iaq)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int link_state;

	NET_LOCK();
	link_state = ixl_set_link_status(sc, iaq);
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
	NET_UNLOCK();
}

#if 0
static void
ixl_aq_dump(const struct ixl_softc *sc, const struct ixl_aq_desc *iaq)
{
	printf("%s: flags %b opcode %04x\n", DEVNAME(sc),
	    lemtoh16(&iaq->iaq_flags), IXL_AQ_FLAGS_FMT,
	    lemtoh16(&iaq->iaq_opcode));
	printf("%s: datalen %u retval %u\n", DEVNAME(sc),
	    lemtoh16(&iaq->iaq_datalen), lemtoh16(&iaq->iaq_retval));
	printf("%s: cookie %016llx\n", DEVNAME(sc), iaq->iaq_cookie);
	printf("%s: %08x %08x %08x %08x\n", DEVNAME(sc),
	    lemtoh32(&iaq->iaq_param[0]), lemtoh32(&iaq->iaq_param[1]),
	    lemtoh32(&iaq->iaq_param[2]), lemtoh32(&iaq->iaq_param[3]));
}
#endif

static void
ixl_arq(void *xsc)
{
	struct ixl_softc *sc = xsc;
	struct ixl_aq_desc *arq, *iaq;
	struct ixl_aq_buf *aqb;
	unsigned int cons = sc->sc_arq_cons;
	unsigned int prod;
	int done = 0;

	prod = ixl_rd(sc, sc->sc_aq_regs->arq_head) &
	    sc->sc_aq_regs->arq_head_mask;

	if (cons == prod)
		goto done;

	arq = IXL_DMA_KVA(&sc->sc_arq);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_arq),
	    0, IXL_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	do {
		iaq = &arq[cons];

		aqb = SIMPLEQ_FIRST(&sc->sc_arq_live);
		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IXL_AQ_BUFLEN,
		    BUS_DMASYNC_POSTREAD);

		switch (iaq->iaq_opcode) {
		case HTOLE16(IXL_AQ_OP_PHY_LINK_STATUS):
			ixl_arq_link_status(sc, iaq);
			break;
		}

		memset(iaq, 0, sizeof(*iaq));
		SIMPLEQ_INSERT_TAIL(&sc->sc_arq_idle, aqb, aqb_entry);
		if_rxr_put(&sc->sc_arq_ring, 1);

		cons++;
		cons &= IXL_AQ_MASK;

		done = 1;
	} while (cons != prod);

	if (done && ixl_arq_fill(sc))
		ixl_wr(sc, sc->sc_aq_regs->arq_tail, sc->sc_arq_prod);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_arq),
	    0, IXL_DMA_LEN(&sc->sc_arq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->sc_arq_cons = cons;

done:
	ixl_intr_enable(sc);
}

static void
ixl_atq_set(struct ixl_atq *iatq,
    void (*fn)(struct ixl_softc *, void *), void *arg)
{
	iatq->iatq_fn = fn;
	iatq->iatq_arg = arg;
}

static void
ixl_atq_post(struct ixl_softc *sc, struct ixl_atq *iatq)
{
	struct ixl_aq_desc *atq, *slot;
	unsigned int prod;

	/* assert locked */

	atq = IXL_DMA_KVA(&sc->sc_atq);
	prod = sc->sc_atq_prod;
	slot = atq + prod;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_POSTWRITE);

	*slot = iatq->iatq_desc;
	slot->iaq_cookie = (uint64_t)iatq;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_PREWRITE);

	prod++;
	prod &= IXL_AQ_MASK;
	sc->sc_atq_prod = prod;
	ixl_wr(sc, sc->sc_aq_regs->atq_tail, prod);
}

static void
ixl_atq_done(struct ixl_softc *sc)
{
	struct ixl_atq_list cmds = SIMPLEQ_HEAD_INITIALIZER(cmds);
	struct ixl_aq_desc *atq, *slot;
	struct ixl_atq *iatq;
	unsigned int cons;
	unsigned int prod;

	prod = sc->sc_atq_prod;
	cons = sc->sc_atq_cons;

	if (prod == cons)
		return;

	atq = IXL_DMA_KVA(&sc->sc_atq);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	do {
		slot = &atq[cons];

		iatq = (struct ixl_atq *)slot->iaq_cookie;
		iatq->iatq_desc = *slot;
		SIMPLEQ_INSERT_TAIL(&cmds, iatq, iatq_entry);

		memset(slot, 0, sizeof(*slot));

		cons++;
		cons &= IXL_AQ_MASK;
	} while (cons != prod);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc->sc_atq_cons = cons;

	while ((iatq = SIMPLEQ_FIRST(&cmds)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&cmds, iatq_entry);

		(*iatq->iatq_fn)(sc, iatq->iatq_arg);
	}
}

struct ixl_wakeup {
	struct mutex mtx;
	int notdone;
};

static void
ixl_wakeup(struct ixl_softc *sc, void *arg)
{
	struct ixl_wakeup *wake = arg;

	mtx_enter(&wake->mtx);
	wake->notdone = 0;
	mtx_leave(&wake->mtx);

	wakeup(wake);
}

static void
ixl_atq_exec(struct ixl_softc *sc, struct ixl_atq *iatq, const char *wmesg)
{
	struct ixl_wakeup wake = { MUTEX_INITIALIZER(IPL_NET), 1 };

	KASSERT(iatq->iatq_desc.iaq_cookie == 0);

	ixl_atq_set(iatq, ixl_wakeup, &wake);
	ixl_atq_post(sc, iatq);

	mtx_enter(&wake.mtx);
	while (wake.notdone) {
		mtx_leave(&wake.mtx);
		ixl_atq_done(sc);
		mtx_enter(&wake.mtx);
		msleep(&wake, &wake.mtx, 0, wmesg, 1);
	}
	mtx_leave(&wake.mtx);
}

static int
ixl_atq_poll(struct ixl_softc *sc, struct ixl_aq_desc *iaq, unsigned int tm)
{
	struct ixl_aq_desc *atq, *slot;
	unsigned int prod;
	unsigned int t = 0;

	atq = IXL_DMA_KVA(&sc->sc_atq);
	prod = sc->sc_atq_prod;
	slot = atq + prod;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_POSTWRITE);

	*slot = *iaq;
	slot->iaq_flags |= htole16(IXL_AQ_SI);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_PREWRITE);

	prod++;
	prod &= IXL_AQ_MASK;
	sc->sc_atq_prod = prod;
	ixl_wr(sc, sc->sc_aq_regs->atq_tail, prod);

	while (ixl_rd(sc, sc->sc_aq_regs->atq_head) != prod) {
		delaymsec(1);

		if (t++ > tm)
			return (ETIMEDOUT);
	}

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_POSTREAD);
	*iaq = *slot;
	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_atq),
	    0, IXL_DMA_LEN(&sc->sc_atq), BUS_DMASYNC_PREREAD);

	sc->sc_atq_cons = prod;

	return (0);
}

static int
ixl_get_version(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;
	uint32_t fwbuild, fwver, apiver;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_GET_VERSION);

	if (ixl_atq_poll(sc, &iaq, 2000) != 0)
		return (ETIMEDOUT);
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK))
		return (EIO);

	fwbuild = lemtoh32(&iaq.iaq_param[1]);
	fwver = lemtoh32(&iaq.iaq_param[2]);
	apiver = lemtoh32(&iaq.iaq_param[3]);

	printf(", FW %hu.%hu.%05u API %hu.%hu", (uint16_t)fwver,
	    (uint16_t)(fwver >> 16), fwbuild, (uint16_t)apiver,
	    (uint16_t)(apiver >> 16));

	return (0);
}

static int
ixl_pxe_clear(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_CLEAR_PXE_MODE);
	iaq.iaq_param[0] = htole32(0x2);

	if (ixl_atq_poll(sc, &iaq, 250) != 0) {
		printf(", CLEAR PXE MODE timeout\n");
		return (-1);
	}

	switch (iaq.iaq_retval) {
	case HTOLE16(IXL_AQ_RC_OK):
	case HTOLE16(IXL_AQ_RC_EEXIST):
		break;
	default:
		printf(", CLEAR PXE MODE error\n");
		return (-1);
	}

	return (0);
}

static int
ixl_lldp_shut(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_LLDP_STOP_AGENT);
	iaq.iaq_param[0] = htole32(IXL_LLDP_SHUTDOWN);

	if (ixl_atq_poll(sc, &iaq, 250) != 0) {
		printf(", STOP LLDP AGENT timeout\n");
		return (-1);
	}

	switch (iaq.iaq_retval) {
	case HTOLE16(IXL_AQ_RC_EMODE):
	case HTOLE16(IXL_AQ_RC_EPERM):
		/* ignore silently */
	default:
		break;
	}

	return (0);
}

static int
ixl_get_mac(struct ixl_softc *sc)
{
	struct ixl_dmamem idm;
	struct ixl_aq_desc iaq;
	struct ixl_aq_mac_addresses *addrs;
	int rv;

	if (ixl_dmamem_alloc(sc, &idm, sizeof(*addrs), 0) != 0) {
		printf(", unable to allocate mac addresses\n");
		return (-1);
	}

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IXL_AQ_BUF);
	iaq.iaq_opcode = htole16(IXL_AQ_OP_MAC_ADDRESS_READ);
	iaq.iaq_datalen = htole16(sizeof(*addrs));
	ixl_aq_dva(&iaq, IXL_DMA_DVA(&idm));

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_PREREAD);

	rv = ixl_atq_poll(sc, &iaq, 250);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_POSTREAD);

	if (rv != 0) {
		printf(", MAC ADDRESS READ timeout\n");
		rv = -1;
		goto done;
	}
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf(", MAC ADDRESS READ error\n");
		rv = -1;
		goto done;
	}

	addrs = IXL_DMA_KVA(&idm);
	if (!ISSET(iaq.iaq_param[0], htole32(IXL_AQ_MAC_PORT_VALID))) {
		printf(", port address is not valid\n");
		goto done;
	}

	memcpy(sc->sc_ac.ac_enaddr, addrs->port, ETHER_ADDR_LEN);
	rv = 0;

done:
	ixl_dmamem_free(sc, &idm);
	return (rv);
}

static int
ixl_get_switch_config(struct ixl_softc *sc)
{
	struct ixl_dmamem idm;
	struct ixl_aq_desc iaq;
	struct ixl_aq_switch_config *hdr;
	struct ixl_aq_switch_config_element *elms, *elm;
	unsigned int nelm;
	int rv;

	if (ixl_dmamem_alloc(sc, &idm, IXL_AQ_BUFLEN, 0) != 0) {
		printf("%s: unable to allocate switch config buffer\n",
		    DEVNAME(sc));
		return (-1);
	}

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IXL_AQ_BUF |
	    (IXL_AQ_BUFLEN > I40E_AQ_LARGE_BUF ? IXL_AQ_LB : 0));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_SWITCH_GET_CONFIG);
	iaq.iaq_datalen = htole16(IXL_AQ_BUFLEN);
	ixl_aq_dva(&iaq, IXL_DMA_DVA(&idm));

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_PREREAD);

	rv = ixl_atq_poll(sc, &iaq, 250);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_POSTREAD);

	if (rv != 0) {
		printf("%s: GET SWITCH CONFIG timeout\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: GET SWITCH CONFIG error\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}

	hdr = IXL_DMA_KVA(&idm);
	elms = (struct ixl_aq_switch_config_element *)(hdr + 1);

	nelm = lemtoh16(&hdr->num_reported);
	if (nelm < 1) {
		printf("%s: no switch config available\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}

#if 0
	for (i = 0; i < nelm; i++) {
		elm = &elms[i];

		printf("%s: type %x revision %u seid %04x\n", DEVNAME(sc),
		    elm->type, elm->revision, lemtoh16(&elm->seid));
		printf("%s: uplink %04x downlink %04x\n", DEVNAME(sc),
		    lemtoh16(&elm->uplink_seid),
		    lemtoh16(&elm->downlink_seid));
		printf("%s: conntype %x scheduler %04x extra %04x\n",
		    DEVNAME(sc), elm->connection_type,
		    lemtoh16(&elm->scheduler_id),
		    lemtoh16(&elm->element_info));
	}
#endif

	elm = &elms[0];

	sc->sc_uplink_seid = elm->uplink_seid;
	sc->sc_downlink_seid = elm->downlink_seid;
	sc->sc_seid = elm->seid;

	if ((sc->sc_uplink_seid == htole16(0)) !=
	    (sc->sc_downlink_seid == htole16(0))) {
		printf("%s: SEIDs are misconfigured\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}

done:
	ixl_dmamem_free(sc, &idm);
	return (rv);
}

static int
ixl_phy_mask_ints(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_PHY_SET_EVENT_MASK);
	iaq.iaq_param[2] = htole32(IXL_AQ_PHY_EV_MASK &
	    ~(IXL_AQ_PHY_EV_LINK_UPDOWN | IXL_AQ_PHY_EV_MODULE_QUAL_FAIL |
	      IXL_AQ_PHY_EV_MEDIA_NA));

	if (ixl_atq_poll(sc, &iaq, 250) != 0) {
		printf("%s: SET PHY EVENT MASK timeout\n", DEVNAME(sc));
		return (-1);
	}
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: SET PHY EVENT MASK error\n", DEVNAME(sc));
		return (-1);
	}

	return (0);
}

static int
ixl_get_phy_abilities(struct ixl_softc *sc, uint64_t *phy_types_ptr)
{
	struct ixl_dmamem idm;
	struct ixl_aq_desc iaq;
	struct ixl_aq_phy_abilities *phy;
	uint64_t phy_types;
	int rv;

	if (ixl_dmamem_alloc(sc, &idm, IXL_AQ_BUFLEN, 0) != 0) {
		printf("%s: unable to allocate switch config buffer\n",
		    DEVNAME(sc));
		return (-1);
	}

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_flags = htole16(IXL_AQ_BUF |
	    (IXL_AQ_BUFLEN > I40E_AQ_LARGE_BUF ? IXL_AQ_LB : 0));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_PHY_GET_ABILITIES);
	iaq.iaq_datalen = htole16(IXL_AQ_BUFLEN);
	iaq.iaq_param[0] = htole32(IXL_AQ_PHY_REPORT_INIT);
	ixl_aq_dva(&iaq, IXL_DMA_DVA(&idm));

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_PREREAD);

	rv = ixl_atq_poll(sc, &iaq, 250);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&idm), 0, IXL_DMA_LEN(&idm),
	    BUS_DMASYNC_POSTREAD);

	if (rv != 0) {
		printf("%s: GET PHY ABILITIES timeout\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}
	switch (iaq.iaq_retval) {
	case HTOLE16(IXL_AQ_RC_OK):
		break;
	case HTOLE16(IXL_AQ_RC_EIO):
		printf("%s: unable to query phy types\n", DEVNAME(sc));
		rv = 0;
		goto done;
	default:
		printf("%s: GET PHY ABILITIIES error\n", DEVNAME(sc));
		rv = -1;
		goto done;
	}

	phy = IXL_DMA_KVA(&idm);

	phy_types = lemtoh32(&phy->phy_type);
	phy_types |= (uint64_t)phy->phy_type_ext << 32;

	*phy_types_ptr = phy_types;

	rv = 0;

done:
	ixl_dmamem_free(sc, &idm);
	return (rv);
}

static int
ixl_get_link_status(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;
	struct ixl_aq_link_param *param;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_PHY_LINK_STATUS);
	param = (struct ixl_aq_link_param *)iaq.iaq_param;
	param->notify = IXL_AQ_LINK_NOTIFY;

	if (ixl_atq_poll(sc, &iaq, 250) != 0) {
		printf("%s: GET LINK STATUS timeout\n", DEVNAME(sc));
		return (-1);
	}
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: GET LINK STATUS error\n", DEVNAME(sc));
		return (0);
	}

	sc->sc_ac.ac_if.if_link_state = ixl_set_link_status(sc, &iaq);

	return (0);
}

static int
ixl_get_vsi(struct ixl_softc *sc)
{
	struct ixl_dmamem *vsi = &sc->sc_vsi;
	struct ixl_aq_desc iaq;
	struct ixl_aq_vsi_param *param;
	struct ixl_aq_vsi_reply *reply;
	int rv;

	/* grumble, vsi info isn't "known" at compile time */

	memset(&iaq, 0, sizeof(iaq));
	htolem16(&iaq.iaq_flags, IXL_AQ_BUF |
	    (IXL_DMA_LEN(vsi) > I40E_AQ_LARGE_BUF ? IXL_AQ_LB : 0));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_GET_VSI_PARAMS);
	htolem16(&iaq.iaq_datalen, IXL_DMA_LEN(vsi));
	ixl_aq_dva(&iaq, IXL_DMA_DVA(vsi));

	param = (struct ixl_aq_vsi_param *)iaq.iaq_param;
	param->uplink_seid = sc->sc_seid;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(vsi), 0, IXL_DMA_LEN(vsi),
	    BUS_DMASYNC_PREREAD);

	rv = ixl_atq_poll(sc, &iaq, 250);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(vsi), 0, IXL_DMA_LEN(vsi),
	    BUS_DMASYNC_POSTREAD);

	if (rv != 0) { 
		printf("%s: GET VSI timeout\n", DEVNAME(sc));
		return (-1);
	}

	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: GET VSI error %u\n", DEVNAME(sc),
		    lemtoh16(&iaq.iaq_retval));
		return (-1);
	}

	reply = (struct ixl_aq_vsi_reply *)iaq.iaq_param;
	sc->sc_vsi_number = reply->vsi_number;

	return (0);
}

static int
ixl_set_vsi(struct ixl_softc *sc)
{
	struct ixl_dmamem *vsi = &sc->sc_vsi;
	struct ixl_aq_desc iaq;
	struct ixl_aq_vsi_param *param;
	struct ixl_aq_vsi_data *data = IXL_DMA_KVA(vsi);
	int rv;

	data->valid_sections = htole16(IXL_AQ_VSI_VALID_QUEUE_MAP |
	    IXL_AQ_VSI_VALID_VLAN);

	CLR(data->mapping_flags, htole16(IXL_AQ_VSI_QUE_MAP_MASK));
	SET(data->mapping_flags, htole16(IXL_AQ_VSI_QUE_MAP_CONTIG));
	data->queue_mapping[0] = htole16(0);
	data->tc_mapping[0] = htole16((0 << IXL_AQ_VSI_TC_Q_OFFSET_SHIFT) |
	    (sc->sc_nqueues << IXL_AQ_VSI_TC_Q_NUMBER_SHIFT));

	CLR(data->port_vlan_flags,
	    htole16(IXL_AQ_VSI_PVLAN_MODE_MASK | IXL_AQ_VSI_PVLAN_EMOD_MASK));
	SET(data->port_vlan_flags,
	    htole16(IXL_AQ_VSI_PVLAN_MODE_ALL | IXL_AQ_VSI_PVLAN_EMOD_NOTHING));

	/* grumble, vsi info isn't "known" at compile time */

	memset(&iaq, 0, sizeof(iaq));
	htolem16(&iaq.iaq_flags, IXL_AQ_BUF | IXL_AQ_RD |
	    (IXL_DMA_LEN(vsi) > I40E_AQ_LARGE_BUF ? IXL_AQ_LB : 0));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_UPD_VSI_PARAMS);
	htolem16(&iaq.iaq_datalen, IXL_DMA_LEN(vsi));
	ixl_aq_dva(&iaq, IXL_DMA_DVA(vsi));

	param = (struct ixl_aq_vsi_param *)iaq.iaq_param;
	param->uplink_seid = sc->sc_seid;

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(vsi), 0, IXL_DMA_LEN(vsi),
	    BUS_DMASYNC_PREWRITE);

	rv = ixl_atq_poll(sc, &iaq, 250);

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(vsi), 0, IXL_DMA_LEN(vsi),
	    BUS_DMASYNC_POSTWRITE);

	if (rv != 0) { 
		printf("%s: UPDATE VSI timeout\n", DEVNAME(sc));
		return (-1);
	}

	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: UPDATE VSI error %u\n", DEVNAME(sc),
		    lemtoh16(&iaq.iaq_retval));
		return (-1);
	}

	return (0);
}

static const struct ixl_phy_type *
ixl_search_phy_type(uint8_t phy_type)
{
	const struct ixl_phy_type *itype;
	uint64_t mask;
	unsigned int i;

	if (phy_type >= 64)
		return (NULL);

	mask = 1ULL << phy_type;

	for (i = 0; i < nitems(ixl_phy_type_map); i++) {
		itype = &ixl_phy_type_map[i];

		if (ISSET(itype->phy_type, mask))
			return (itype);
	}

	return (NULL);
}

static uint64_t
ixl_search_link_speed(uint8_t link_speed)
{
	const struct ixl_speed_type *type;
	unsigned int i;

	for (i = 0; i < nitems(ixl_phy_type_map); i++) {
		type = &ixl_speed_type_map[i];

		if (ISSET(type->dev_speed, link_speed))
			return (type->net_speed);
	}

	return (0);
}

static int
ixl_set_link_status(struct ixl_softc *sc, const struct ixl_aq_desc *iaq)
{
	const struct ixl_aq_link_status *status;
	const struct ixl_phy_type *itype;

	uint64_t ifm_active = IFM_ETHER;
	uint64_t ifm_status = IFM_AVALID;
	int link_state = LINK_STATE_DOWN;
	uint64_t baudrate = 0;

	status = (const struct ixl_aq_link_status *)iaq->iaq_param;
	if (!ISSET(status->link_info, IXL_AQ_LINK_UP_FUNCTION))
		goto done;

	ifm_active |= IFM_FDX;
	ifm_status |= IFM_ACTIVE;
	link_state = LINK_STATE_FULL_DUPLEX;

	itype = ixl_search_phy_type(status->phy_type);
	if (itype != NULL)
		ifm_active |= itype->ifm_type;

	if (ISSET(status->an_info, IXL_AQ_LINK_PAUSE_TX))
		ifm_active |= IFM_ETH_TXPAUSE;
	if (ISSET(status->an_info, IXL_AQ_LINK_PAUSE_RX))
		ifm_active |= IFM_ETH_RXPAUSE;

	baudrate = ixl_search_link_speed(status->link_speed);

done:
	/* NET_ASSERT_LOCKED() except during attach */
	sc->sc_media_active = ifm_active;
	sc->sc_media_status = ifm_status;
	sc->sc_ac.ac_if.if_baudrate = baudrate;

	return (link_state);
}

static int
ixl_restart_an(struct ixl_softc *sc)
{
	struct ixl_aq_desc iaq;

	memset(&iaq, 0, sizeof(iaq));
	iaq.iaq_opcode = htole16(IXL_AQ_OP_PHY_RESTART_AN);
	iaq.iaq_param[0] =
	    htole32(IXL_AQ_PHY_RESTART_AN | IXL_AQ_PHY_LINK_ENABLE);

	if (ixl_atq_poll(sc, &iaq, 250) != 0) {
		printf("%s: RESTART AN timeout\n", DEVNAME(sc));
		return (-1);
	}
	if (iaq.iaq_retval != htole16(IXL_AQ_RC_OK)) {
		printf("%s: RESTART AN error\n", DEVNAME(sc));
		return (-1);
	}

	return (0);
}

static int
ixl_hmc(struct ixl_softc *sc)
{
	struct {
		uint32_t   count;
		uint32_t   minsize;
		bus_size_t maxcnt;
		bus_size_t setoff;
		bus_size_t setcnt;
	} regs[] = {
		{
			0,
			IXL_HMC_TXQ_MINSIZE,
			I40E_GLHMC_LANTXOBJSZ,
			I40E_GLHMC_LANTXBASE(sc->sc_pf_id),
			I40E_GLHMC_LANTXCNT(sc->sc_pf_id),
		},
		{
			0,
			IXL_HMC_RXQ_MINSIZE,
			I40E_GLHMC_LANRXOBJSZ,
			I40E_GLHMC_LANRXBASE(sc->sc_pf_id),
			I40E_GLHMC_LANRXCNT(sc->sc_pf_id),
		},
		{
			0,
			0,
			I40E_GLHMC_FCOEMAX,
			I40E_GLHMC_FCOEDDPBASE(sc->sc_pf_id),
			I40E_GLHMC_FCOEDDPCNT(sc->sc_pf_id),
		},
		{
			0,
			0,
			I40E_GLHMC_FCOEFMAX,
			I40E_GLHMC_FCOEFBASE(sc->sc_pf_id),
			I40E_GLHMC_FCOEFCNT(sc->sc_pf_id),
		},
	};
	struct ixl_hmc_entry *e;
	uint64_t size, dva;
	uint8_t *kva;
	uint64_t *sdpage;
	unsigned int i;
	int npages, tables;

	CTASSERT(nitems(regs) <= nitems(sc->sc_hmc_entries));

	regs[IXL_HMC_LAN_TX].count = regs[IXL_HMC_LAN_RX].count =
	    ixl_rd(sc, I40E_GLHMC_LANQMAX);

	size = 0;
	for (i = 0; i < nitems(regs); i++) {
		e = &sc->sc_hmc_entries[i];

		e->hmc_count = regs[i].count;
		e->hmc_size = 1U << ixl_rd(sc, regs[i].maxcnt);
		e->hmc_base = size;

		if ((e->hmc_size * 8) < regs[i].minsize) {
			printf("%s: kernel hmc entry is too big\n",
			    DEVNAME(sc));
			return (-1);
		}

		size += roundup(e->hmc_size * e->hmc_count, IXL_HMC_ROUNDUP);
	}
	size = roundup(size, IXL_HMC_PGSIZE);
	npages = size / IXL_HMC_PGSIZE;

	tables = roundup(size, IXL_HMC_L2SZ) / IXL_HMC_L2SZ;

	if (ixl_dmamem_alloc(sc, &sc->sc_hmc_pd, size, IXL_HMC_PGSIZE) != 0) {
		printf("%s: unable to allocate hmc pd memory\n", DEVNAME(sc));
		return (-1);
	}

	if (ixl_dmamem_alloc(sc, &sc->sc_hmc_sd, tables * IXL_HMC_PGSIZE,
	    IXL_HMC_PGSIZE) != 0) {
		printf("%s: unable to allocate hmc sd memory\n", DEVNAME(sc));
		ixl_dmamem_free(sc, &sc->sc_hmc_pd);
		return (-1);
	}

	kva = IXL_DMA_KVA(&sc->sc_hmc_pd);
	memset(kva, 0, IXL_DMA_LEN(&sc->sc_hmc_pd));

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_hmc_pd),
	    0, IXL_DMA_LEN(&sc->sc_hmc_pd),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	dva = IXL_DMA_DVA(&sc->sc_hmc_pd);
	sdpage = IXL_DMA_KVA(&sc->sc_hmc_sd);
	for (i = 0; i < npages; i++) {
		htolem64(sdpage++, dva | IXL_HMC_PDVALID);

		dva += IXL_HMC_PGSIZE;
	}

	bus_dmamap_sync(sc->sc_dmat, IXL_DMA_MAP(&sc->sc_hmc_sd),
	    0, IXL_DMA_LEN(&sc->sc_hmc_sd),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	dva = IXL_DMA_DVA(&sc->sc_hmc_sd);
	for (i = 0; i < tables; i++) {
		uint32_t count;

		KASSERT(npages >= 0);

		count = (npages > IXL_HMC_PGS) ? IXL_HMC_PGS : npages;

		ixl_wr(sc, I40E_PFHMC_SDDATAHIGH, dva >> 32);
		ixl_wr(sc, I40E_PFHMC_SDDATALOW, dva |
		    (count << I40E_PFHMC_SDDATALOW_PMSDBPCOUNT_SHIFT) |
		    (1U << I40E_PFHMC_SDDATALOW_PMSDVALID_SHIFT));
		ixl_barrier(sc, 0, sc->sc_mems, BUS_SPACE_BARRIER_WRITE);
		ixl_wr(sc, I40E_PFHMC_SDCMD,
		    (1U << I40E_PFHMC_SDCMD_PMSDWR_SHIFT) | i);

		npages -= IXL_HMC_PGS;
		dva += IXL_HMC_PGSIZE;
	}

	for (i = 0; i < nitems(regs); i++) {
		e = &sc->sc_hmc_entries[i];

		ixl_wr(sc, regs[i].setoff, e->hmc_base / IXL_HMC_ROUNDUP);
		ixl_wr(sc, regs[i].setcnt, e->hmc_count);
	}

	return (0);
}

static void
ixl_hmc_free(struct ixl_softc *sc)
{
	ixl_dmamem_free(sc, &sc->sc_hmc_sd);
	ixl_dmamem_free(sc, &sc->sc_hmc_pd);
}

static void
ixl_hmc_pack(void *d, const void *s, const struct ixl_hmc_pack *packing,
    unsigned int npacking)
{
	uint8_t *dst = d;
	const uint8_t *src = s;
	unsigned int i;

	for (i = 0; i < npacking; i++) {
		const struct ixl_hmc_pack *pack = &packing[i];
		unsigned int offset = pack->lsb / 8;
		unsigned int align = pack->lsb % 8;
		const uint8_t *in = src + pack->offset;
		uint8_t *out = dst + offset;
		int width = pack->width;
		unsigned int inbits = 0;

		if (align) {
			inbits = *in++;

			*out++ |= inbits << align;

			width -= 8 - align;
		}

		while (width >= 8) {
			inbits <<= 8;
			inbits |= *in++;

			*out++ = inbits << align;

			width -= 8;
		}

		if (width)
			*out = inbits >> (8 - align);
	}
}

static struct ixl_aq_buf *
ixl_aqb_alloc(struct ixl_softc *sc)
{
	struct ixl_aq_buf *aqb;

	aqb = malloc(sizeof(*aqb), M_DEVBUF, M_WAITOK);
	if (aqb == NULL)
		return (NULL);

	aqb->aqb_data = dma_alloc(IXL_AQ_BUFLEN, PR_WAITOK);
	if (aqb->aqb_data == NULL)
		goto free;

	if (bus_dmamap_create(sc->sc_dmat, IXL_AQ_BUFLEN, 1,
	    IXL_AQ_BUFLEN, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &aqb->aqb_map) != 0)
		goto dma_free;

	if (bus_dmamap_load(sc->sc_dmat, aqb->aqb_map, aqb->aqb_data,
	    IXL_AQ_BUFLEN, NULL, BUS_DMA_WAITOK) != 0)
		goto destroy;

	return (aqb);

destroy:
	bus_dmamap_destroy(sc->sc_dmat, aqb->aqb_map);
dma_free:
	dma_free(aqb->aqb_data, IXL_AQ_BUFLEN);
free:
	free(aqb, M_DEVBUF, sizeof(*aqb));

	return (NULL);
}

static void
ixl_aqb_free(struct ixl_softc *sc, struct ixl_aq_buf *aqb)
{
	bus_dmamap_unload(sc->sc_dmat, aqb->aqb_map);
	bus_dmamap_destroy(sc->sc_dmat, aqb->aqb_map);
	dma_free(aqb->aqb_data, IXL_AQ_BUFLEN);
	free(aqb, M_DEVBUF, sizeof(*aqb));
}

static int
ixl_arq_fill(struct ixl_softc *sc)
{
	struct ixl_aq_buf *aqb;
	struct ixl_aq_desc *arq, *iaq;
	unsigned int prod = sc->sc_arq_prod;
	unsigned int n;
	int post = 0;

	n = if_rxr_get(&sc->sc_arq_ring, IXL_AQ_NUM);
 	arq = IXL_DMA_KVA(&sc->sc_arq);

	while (n > 0) {
		aqb = SIMPLEQ_FIRST(&sc->sc_arq_idle);
		if (aqb != NULL)
			SIMPLEQ_REMOVE_HEAD(&sc->sc_arq_idle, aqb_entry);
		else if ((aqb = ixl_aqb_alloc(sc)) == NULL)
			break;

		memset(aqb->aqb_data, 0, IXL_AQ_BUFLEN);

		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IXL_AQ_BUFLEN,
		    BUS_DMASYNC_PREREAD);

		iaq = &arq[prod];
		iaq->iaq_flags = htole16(IXL_AQ_BUF |
		    (IXL_AQ_BUFLEN > I40E_AQ_LARGE_BUF ? IXL_AQ_LB : 0));
		iaq->iaq_opcode = 0;
		iaq->iaq_datalen = htole16(IXL_AQ_BUFLEN);
		iaq->iaq_retval = 0;
		iaq->iaq_cookie = 0;
		iaq->iaq_param[0] = 0;
		iaq->iaq_param[1] = 0;
		ixl_aq_dva(iaq, aqb->aqb_map->dm_segs[0].ds_addr);

		SIMPLEQ_INSERT_TAIL(&sc->sc_arq_live, aqb, aqb_entry);

		prod++;
		prod &= IXL_AQ_MASK;

		post = 1;

		n--;
	}

	if_rxr_put(&sc->sc_arq_ring, n);
	sc->sc_arq_prod = prod;

	return (post);
}

static void
ixl_arq_unfill(struct ixl_softc *sc)
{
	struct ixl_aq_buf *aqb;

	while ((aqb = SIMPLEQ_FIRST(&sc->sc_arq_live)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_arq_live, aqb_entry);

		bus_dmamap_sync(sc->sc_dmat, aqb->aqb_map, 0, IXL_AQ_BUFLEN,
		    BUS_DMASYNC_POSTREAD);
		ixl_aqb_free(sc, aqb);
	}
}

static void
ixl_clear_hw(struct ixl_softc *sc)
{
	uint32_t num_queues, base_queue;
	uint32_t num_pf_int;
	uint32_t num_vf_int;
	uint32_t num_vfs;
	uint32_t i, j;
	uint32_t val;
	uint32_t eol = 0x7ff;

	/* get number of interrupts, queues, and vfs */
	val = ixl_rd(sc, I40E_GLPCI_CNF2);
	num_pf_int = (val & I40E_GLPCI_CNF2_MSI_X_PF_N_MASK) >>
	    I40E_GLPCI_CNF2_MSI_X_PF_N_SHIFT;
	num_vf_int = (val & I40E_GLPCI_CNF2_MSI_X_VF_N_MASK) >>
	    I40E_GLPCI_CNF2_MSI_X_VF_N_SHIFT;

	val = ixl_rd(sc, I40E_PFLAN_QALLOC);
	base_queue = (val & I40E_PFLAN_QALLOC_FIRSTQ_MASK) >>
	    I40E_PFLAN_QALLOC_FIRSTQ_SHIFT;
	j = (val & I40E_PFLAN_QALLOC_LASTQ_MASK) >>
	    I40E_PFLAN_QALLOC_LASTQ_SHIFT;
	if (val & I40E_PFLAN_QALLOC_VALID_MASK)
		num_queues = (j - base_queue) + 1;
	else
		num_queues = 0;

	val = ixl_rd(sc, I40E_PF_VT_PFALLOC);
	i = (val & I40E_PF_VT_PFALLOC_FIRSTVF_MASK) >>
	    I40E_PF_VT_PFALLOC_FIRSTVF_SHIFT;
	j = (val & I40E_PF_VT_PFALLOC_LASTVF_MASK) >>
	    I40E_PF_VT_PFALLOC_LASTVF_SHIFT;
	if (val & I40E_PF_VT_PFALLOC_VALID_MASK)
		num_vfs = (j - i) + 1;
	else
		num_vfs = 0;

	/* stop all the interrupts */
	ixl_wr(sc, I40E_PFINT_ICR0_ENA, 0);
	val = 0x3 << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	for (i = 0; i < num_pf_int - 2; i++)
		ixl_wr(sc, I40E_PFINT_DYN_CTLN(i), val);

	/* Set the FIRSTQ_INDX field to 0x7FF in PFINT_LNKLSTx */
	val = eol << I40E_PFINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	ixl_wr(sc, I40E_PFINT_LNKLST0, val);
	for (i = 0; i < num_pf_int - 2; i++)
		ixl_wr(sc, I40E_PFINT_LNKLSTN(i), val);
	val = eol << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT;
	for (i = 0; i < num_vfs; i++)
		ixl_wr(sc, I40E_VPINT_LNKLST0(i), val);
	for (i = 0; i < num_vf_int - 2; i++)
		ixl_wr(sc, I40E_VPINT_LNKLSTN(i), val);

	/* warn the HW of the coming Tx disables */
	for (i = 0; i < num_queues; i++) {
		uint32_t abs_queue_idx = base_queue + i;
		uint32_t reg_block = 0;

		if (abs_queue_idx >= 128) {
			reg_block = abs_queue_idx / 128;
			abs_queue_idx %= 128;
		}

		val = ixl_rd(sc, I40E_GLLAN_TXPRE_QDIS(reg_block));
		val &= ~I40E_GLLAN_TXPRE_QDIS_QINDX_MASK;
		val |= (abs_queue_idx << I40E_GLLAN_TXPRE_QDIS_QINDX_SHIFT);
		val |= I40E_GLLAN_TXPRE_QDIS_SET_QDIS_MASK;

		ixl_wr(sc, I40E_GLLAN_TXPRE_QDIS(reg_block), val);
	}
	delaymsec(400);

	/* stop all the queues */
	for (i = 0; i < num_queues; i++) {
		ixl_wr(sc, I40E_QINT_TQCTL(i), 0);
		ixl_wr(sc, I40E_QTX_ENA(i), 0);
		ixl_wr(sc, I40E_QINT_RQCTL(i), 0);
		ixl_wr(sc, I40E_QRX_ENA(i), 0);
	}

	/* short wait for all queue disables to settle */
	delaymsec(50);
}

static int
ixl_pf_reset(struct ixl_softc *sc)
{
	uint32_t cnt = 0;
	uint32_t cnt1 = 0;
	uint32_t reg = 0;
	uint32_t grst_del;

	/*
	 * Poll for Global Reset steady state in case of recent GRST.
	 * The grst delay value is in 100ms units, and we'll wait a
	 * couple counts longer to be sure we don't just miss the end.
	 */
	grst_del = ixl_rd(sc, I40E_GLGEN_RSTCTL);
	grst_del &= I40E_GLGEN_RSTCTL_GRSTDEL_MASK;
	grst_del >>= I40E_GLGEN_RSTCTL_GRSTDEL_SHIFT;
	grst_del += 10;

	for (cnt = 0; cnt < grst_del; cnt++) {
		reg = ixl_rd(sc, I40E_GLGEN_RSTAT);
		if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
			break;
		delaymsec(100);
	}
	if (reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK) {
		printf(", Global reset polling failed to complete\n");
		return (-1);
	}

	/* Now Wait for the FW to be ready */
	for (cnt1 = 0; cnt1 < I40E_PF_RESET_WAIT_COUNT; cnt1++) {
		reg = ixl_rd(sc, I40E_GLNVM_ULD);
		reg &= (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
		    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK);
		if (reg == (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
		    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))
			break;

		delaymsec(10);
	}
	if (!(reg & (I40E_GLNVM_ULD_CONF_CORE_DONE_MASK |
	    I40E_GLNVM_ULD_CONF_GLOBAL_DONE_MASK))) {
		printf(", wait for FW Reset complete timed out "
		    "(I40E_GLNVM_ULD = 0x%x)\n", reg);
		return (-1);
	}

	/*
	 * If there was a Global Reset in progress when we got here,
	 * we don't need to do the PF Reset
	 */
	if (cnt == 0) {
		reg = ixl_rd(sc, I40E_PFGEN_CTRL);
		ixl_wr(sc, I40E_PFGEN_CTRL, reg | I40E_PFGEN_CTRL_PFSWR_MASK);
		for (cnt = 0; cnt < I40E_PF_RESET_WAIT_COUNT; cnt++) {
			reg = ixl_rd(sc, I40E_PFGEN_CTRL);
			if (!(reg & I40E_PFGEN_CTRL_PFSWR_MASK))
				break;
			delaymsec(1);
		}
		if (reg & I40E_PFGEN_CTRL_PFSWR_MASK) {
			printf(", PF reset polling failed to complete"
			    "(I40E_PFGEN_CTRL= 0x%x)\n", reg);
			return (-1);
		}
	}

	return (0);
}

static int
ixl_dmamem_alloc(struct ixl_softc *sc, struct ixl_dmamem *ixm,
    bus_size_t size, u_int align)
{
	ixm->ixm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, ixm->ixm_size, 1,
	    ixm->ixm_size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &ixm->ixm_map) != 0)
		return (1);
	if (bus_dmamem_alloc(sc->sc_dmat, ixm->ixm_size,
	    align, 0, &ixm->ixm_seg, 1, &ixm->ixm_nsegs,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;
	if (bus_dmamem_map(sc->sc_dmat, &ixm->ixm_seg, ixm->ixm_nsegs,
	    ixm->ixm_size, &ixm->ixm_kva, BUS_DMA_WAITOK) != 0)
		goto free;
	if (bus_dmamap_load(sc->sc_dmat, ixm->ixm_map, ixm->ixm_kva,
	    ixm->ixm_size, NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return (0);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, ixm->ixm_kva, ixm->ixm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &ixm->ixm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, ixm->ixm_map);
	return (1);
}

static void
ixl_dmamem_free(struct ixl_softc *sc, struct ixl_dmamem *ixm)
{
	bus_dmamap_unload(sc->sc_dmat, ixm->ixm_map);
	bus_dmamem_unmap(sc->sc_dmat, ixm->ixm_kva, ixm->ixm_size);
	bus_dmamem_free(sc->sc_dmat, &ixm->ixm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, ixm->ixm_map);
}
