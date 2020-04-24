/*	$OpenBSD: if_mcx.c,v 1.44 2020/04/24 07:28:37 mestre Exp $ */

/*
 * Copyright (c) 2017 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2019 Jonathan Matthew <jmatthew@openbsd.org>
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
#include <sys/atomic.h>

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

#define BUS_DMASYNC_PRERW	(BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)
#define BUS_DMASYNC_POSTRW	(BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)

#define MCX_HCA_BAR	PCI_MAPREG_START /* BAR 0 */

#define MCX_FW_VER	 	  0x0000
#define  MCX_FW_VER_MAJOR(_v)		((_v) & 0xffff)
#define  MCX_FW_VER_MINOR(_v)		((_v) >> 16)
#define MCX_CMDIF_FW_SUBVER	  0x0004
#define  MCX_FW_VER_SUBMINOR(_v)	((_v) & 0xffff)
#define  MCX_CMDIF(_v)			((_v) >> 16)

#define MCX_ISSI		 1 /* as per the PRM */
#define MCX_CMD_IF_SUPPORTED	 5

#define MCX_HARDMTU		 9500

#define MCX_MAX_CQS		 2		/* rq, sq */

/* queue sizes */
#define MCX_LOG_EQ_SIZE		 6		/* one page */
#define MCX_LOG_CQ_SIZE		 12
#define MCX_LOG_RQ_SIZE		 10
#define MCX_LOG_SQ_SIZE		 11

/* completion event moderation - about 10khz, or 90% of the cq */
#define MCX_CQ_MOD_PERIOD	50
#define MCX_CQ_MOD_COUNTER	(((1 << (MCX_LOG_CQ_SIZE - 1)) * 9) / 10)

#define MCX_LOG_SQ_ENTRY_SIZE	 6
#define MCX_SQ_ENTRY_MAX_SLOTS	 4
#define MCX_SQ_SEGS_PER_SLOT	 \
	(sizeof(struct mcx_sq_entry) / sizeof(struct mcx_sq_entry_seg))
#define MCX_SQ_MAX_SEGMENTS	 \
	1 + ((MCX_SQ_ENTRY_MAX_SLOTS-1) * MCX_SQ_SEGS_PER_SLOT)

#define MCX_LOG_FLOW_TABLE_SIZE	 5
#define MCX_NUM_STATIC_FLOWS	 4	/* promisc, allmulti, ucast, bcast */
#define MCX_NUM_MCAST_FLOWS 	\
	((1 << MCX_LOG_FLOW_TABLE_SIZE) - MCX_NUM_STATIC_FLOWS)

#define MCX_SQ_INLINE_SIZE	 18

/* doorbell offsets */
#define MCX_CQ_DOORBELL_OFFSET	 0
#define MCX_CQ_DOORBELL_SIZE	 16
#define MCX_RQ_DOORBELL_OFFSET	 64
#define MCX_SQ_DOORBELL_OFFSET	 64

#define MCX_WQ_DOORBELL_MASK	 0xffff

/* uar registers */
#define MCX_UAR_CQ_DOORBELL	 0x20
#define MCX_UAR_EQ_DOORBELL_ARM	 0x40
#define MCX_UAR_EQ_DOORBELL	 0x48
#define MCX_UAR_BF		 0x800

#define MCX_CMDQ_ADDR_HI		 0x0010
#define MCX_CMDQ_ADDR_LO		 0x0014
#define MCX_CMDQ_ADDR_NMASK		0xfff
#define MCX_CMDQ_LOG_SIZE(_v)		((_v) >> 4 & 0xf)
#define MCX_CMDQ_LOG_STRIDE(_v)		((_v) >> 0 & 0xf)
#define MCX_CMDQ_INTERFACE_MASK		(0x3 << 8)
#define MCX_CMDQ_INTERFACE_FULL_DRIVER	(0x0 << 8)
#define MCX_CMDQ_INTERFACE_DISABLED	(0x1 << 8)

#define MCX_CMDQ_DOORBELL		0x0018

#define MCX_STATE		0x01fc
#define MCX_STATE_MASK			(1 << 31)
#define MCX_STATE_INITIALIZING		(1 << 31)
#define MCX_STATE_READY			(0 << 31)
#define MCX_STATE_INTERFACE_MASK	(0x3 << 24)
#define MCX_STATE_INTERFACE_FULL_DRIVER	(0x0 << 24)
#define MCX_STATE_INTERFACE_DISABLED	(0x1 << 24)

#define MCX_INTERNAL_TIMER	0x1000
#define MCX_INTERNAL_TIMER_H	0x1000
#define MCX_INTERNAL_TIMER_L	0x1004

#define MCX_CLEAR_INT		0x100c

#define MCX_REG_OP_WRITE	0
#define MCX_REG_OP_READ		1

#define MCX_REG_PMLP		0x5002
#define MCX_REG_PMTU		0x5003
#define MCX_REG_PTYS		0x5004
#define MCX_REG_PAOS		0x5006
#define MCX_REG_PFCC		0x5007
#define MCX_REG_PPCNT		0x5008
#define MCX_REG_MCIA		0x9014

#define MCX_ETHER_CAP_SGMII	0
#define MCX_ETHER_CAP_1000_KX	1
#define MCX_ETHER_CAP_10G_CX4	2
#define MCX_ETHER_CAP_10G_KX4	3
#define MCX_ETHER_CAP_10G_KR	4
#define MCX_ETHER_CAP_40G_CR4	6
#define MCX_ETHER_CAP_40G_KR4	7
#define MCX_ETHER_CAP_10G_CR	12
#define MCX_ETHER_CAP_10G_SR	13
#define MCX_ETHER_CAP_10G_LR	14
#define MCX_ETHER_CAP_40G_SR4	15
#define MCX_ETHER_CAP_40G_LR4	16
#define MCX_ETHER_CAP_50G_SR2	18
#define MCX_ETHER_CAP_100G_CR4	20
#define MCX_ETHER_CAP_100G_SR4	21
#define MCX_ETHER_CAP_100G_KR4	22
#define MCX_ETHER_CAP_25G_CR	27
#define MCX_ETHER_CAP_25G_KR	28
#define MCX_ETHER_CAP_25G_SR	29
#define MCX_ETHER_CAP_50G_CR2	30
#define MCX_ETHER_CAP_50G_KR2	31

#define MCX_PAGE_SHIFT		12
#define MCX_PAGE_SIZE		(1 << MCX_PAGE_SHIFT)
#define MCX_MAX_CQE		32

#define MCX_CMD_QUERY_HCA_CAP	0x100
#define MCX_CMD_QUERY_ADAPTER	0x101
#define MCX_CMD_INIT_HCA	0x102
#define MCX_CMD_TEARDOWN_HCA	0x103
#define MCX_CMD_ENABLE_HCA	0x104
#define MCX_CMD_DISABLE_HCA	0x105
#define MCX_CMD_QUERY_PAGES	0x107
#define MCX_CMD_MANAGE_PAGES	0x108
#define MCX_CMD_SET_HCA_CAP	0x109
#define MCX_CMD_QUERY_ISSI	0x10a
#define MCX_CMD_SET_ISSI	0x10b
#define MCX_CMD_SET_DRIVER_VERSION \
				0x10d
#define MCX_CMD_QUERY_SPECIAL_CONTEXTS \
				0x203
#define MCX_CMD_CREATE_EQ	0x301
#define MCX_CMD_DESTROY_EQ	0x302
#define MCX_CMD_CREATE_CQ	0x400
#define MCX_CMD_DESTROY_CQ	0x401
#define MCX_CMD_QUERY_NIC_VPORT_CONTEXT \
				0x754
#define MCX_CMD_MODIFY_NIC_VPORT_CONTEXT \
				0x755
#define MCX_CMD_QUERY_VPORT_COUNTERS \
				0x770
#define MCX_CMD_ALLOC_PD	0x800
#define MCX_CMD_ALLOC_UAR	0x802
#define MCX_CMD_ACCESS_REG	0x805
#define MCX_CMD_ALLOC_TRANSPORT_DOMAIN \
				0x816
#define MCX_CMD_CREATE_TIR	0x900
#define MCX_CMD_DESTROY_TIR	0x902
#define MCX_CMD_CREATE_SQ	0x904
#define MCX_CMD_MODIFY_SQ	0x905
#define MCX_CMD_DESTROY_SQ	0x906
#define MCX_CMD_QUERY_SQ	0x907
#define MCX_CMD_CREATE_RQ	0x908
#define MCX_CMD_MODIFY_RQ	0x909
#define MCX_CMD_DESTROY_RQ	0x90a
#define MCX_CMD_QUERY_RQ	0x90b
#define MCX_CMD_CREATE_TIS	0x912
#define MCX_CMD_DESTROY_TIS	0x914
#define MCX_CMD_SET_FLOW_TABLE_ROOT \
				0x92f
#define MCX_CMD_CREATE_FLOW_TABLE \
				0x930
#define MCX_CMD_DESTROY_FLOW_TABLE \
				0x931
#define MCX_CMD_QUERY_FLOW_TABLE \
				0x932
#define MCX_CMD_CREATE_FLOW_GROUP \
				0x933
#define MCX_CMD_DESTROY_FLOW_GROUP \
				0x934
#define MCX_CMD_QUERY_FLOW_GROUP \
				0x935
#define MCX_CMD_SET_FLOW_TABLE_ENTRY \
				0x936
#define MCX_CMD_QUERY_FLOW_TABLE_ENTRY \
				0x937
#define MCX_CMD_DELETE_FLOW_TABLE_ENTRY \
				0x938
#define MCX_CMD_ALLOC_FLOW_COUNTER \
				0x939
#define MCX_CMD_QUERY_FLOW_COUNTER \
				0x93b

#define MCX_QUEUE_STATE_RST	0
#define MCX_QUEUE_STATE_RDY	1
#define MCX_QUEUE_STATE_ERR	3

#define MCX_FLOW_TABLE_TYPE_RX	0
#define MCX_FLOW_TABLE_TYPE_TX	1

#define MCX_CMDQ_INLINE_DATASIZE 16

struct mcx_cmdq_entry {
	uint8_t			cq_type;
#define MCX_CMDQ_TYPE_PCIE		0x7
	uint8_t			cq_reserved0[3];

	uint32_t		cq_input_length;
	uint64_t		cq_input_ptr;
	uint8_t			cq_input_data[MCX_CMDQ_INLINE_DATASIZE];

	uint8_t			cq_output_data[MCX_CMDQ_INLINE_DATASIZE];
	uint64_t		cq_output_ptr;
	uint32_t		cq_output_length;

	uint8_t			cq_token;
	uint8_t			cq_signature;
	uint8_t			cq_reserved1[1];
	uint8_t			cq_status;
#define MCX_CQ_STATUS_SHIFT		1
#define MCX_CQ_STATUS_MASK		(0x7f << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_OK		(0x00 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_INT_ERR		(0x01 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_OPCODE	(0x02 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_PARAM		(0x03 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_SYS_STATE	(0x04 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_RESOURCE	(0x05 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_RESOURCE_BUSY	(0x06 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_EXCEED_LIM	(0x08 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_RES_STATE	(0x09 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_INDEX		(0x0a << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_NO_RESOURCES	(0x0f << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_INPUT_LEN	(0x50 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_OUTPUT_LEN	(0x51 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_RESOURCE_STATE \
					(0x10 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_BAD_SIZE		(0x40 << MCX_CQ_STATUS_SHIFT)
#define MCX_CQ_STATUS_OWN_MASK		0x1
#define MCX_CQ_STATUS_OWN_SW		0x0
#define MCX_CQ_STATUS_OWN_HW		0x1
} __packed __aligned(8);

#define MCX_CMDQ_MAILBOX_DATASIZE	512

struct mcx_cmdq_mailbox {
	uint8_t			mb_data[MCX_CMDQ_MAILBOX_DATASIZE];
	uint8_t			mb_reserved0[48];
	uint64_t		mb_next_ptr;
	uint32_t		mb_block_number;
	uint8_t			mb_reserved1[1];
	uint8_t			mb_token;
	uint8_t			mb_ctrl_signature;
	uint8_t			mb_signature;
} __packed __aligned(8);

#define MCX_CMDQ_MAILBOX_ALIGN	(1 << 10)
#define MCX_CMDQ_MAILBOX_SIZE	roundup(sizeof(struct mcx_cmdq_mailbox), \
				    MCX_CMDQ_MAILBOX_ALIGN)
/*
 * command mailbox structres
 */

struct mcx_cmd_enable_hca_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_function_id;
	uint8_t			cmd_reserved2[4];
} __packed __aligned(4);

struct mcx_cmd_enable_hca_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_init_hca_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_init_hca_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_teardown_hca_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[2];
#define MCX_CMD_TEARDOWN_HCA_GRACEFUL	0x0
#define MCX_CMD_TEARDOWN_HCA_PANIC	0x1
	uint16_t		cmd_profile;
	uint8_t			cmd_reserved2[4];
} __packed __aligned(4);

struct mcx_cmd_teardown_hca_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_access_reg_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_register_id;
	uint32_t		cmd_argument;
} __packed __aligned(4);

struct mcx_cmd_access_reg_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_reg_pmtu {
	uint8_t			rp_reserved1;
	uint8_t			rp_local_port;
	uint8_t			rp_reserved2[2];
	uint16_t		rp_max_mtu;
	uint8_t			rp_reserved3[2];
	uint16_t		rp_admin_mtu;
	uint8_t			rp_reserved4[2];
	uint16_t		rp_oper_mtu;
	uint8_t			rp_reserved5[2];
} __packed __aligned(4);

struct mcx_reg_ptys {
	uint8_t			rp_reserved1;
	uint8_t			rp_local_port;
	uint8_t			rp_reserved2;
	uint8_t			rp_proto_mask;
#define MCX_REG_PTYS_PROTO_MASK_ETH		(1 << 2)
	uint8_t			rp_reserved3[8];
	uint32_t		rp_eth_proto_cap;
	uint8_t			rp_reserved4[8];
	uint32_t		rp_eth_proto_admin;
	uint8_t			rp_reserved5[8];
	uint32_t		rp_eth_proto_oper;
	uint8_t			rp_reserved6[24];
} __packed __aligned(4);

struct mcx_reg_paos {
	uint8_t			rp_reserved1;
	uint8_t			rp_local_port;
	uint8_t			rp_admin_status;
#define MCX_REG_PAOS_ADMIN_STATUS_UP		1
#define MCX_REG_PAOS_ADMIN_STATUS_DOWN		2
#define MCX_REG_PAOS_ADMIN_STATUS_UP_ONCE	3
#define MCX_REG_PAOS_ADMIN_STATUS_DISABLED	4
	uint8_t			rp_oper_status;
#define MCX_REG_PAOS_OPER_STATUS_UP		1
#define MCX_REG_PAOS_OPER_STATUS_DOWN		2
#define MCX_REG_PAOS_OPER_STATUS_FAILED		4
	uint8_t			rp_admin_state_update;
#define MCX_REG_PAOS_ADMIN_STATE_UPDATE_EN	(1 << 7)
	uint8_t			rp_reserved2[11];
} __packed __aligned(4);

struct mcx_reg_pfcc {
	uint8_t			rp_reserved1;
	uint8_t			rp_local_port;
	uint8_t			rp_reserved2[3];
	uint8_t			rp_prio_mask_tx;
	uint8_t			rp_reserved3;
	uint8_t			rp_prio_mask_rx;
	uint8_t			rp_pptx_aptx;
	uint8_t			rp_pfctx;
	uint8_t			rp_fctx_dis;
	uint8_t			rp_reserved4;
	uint8_t			rp_pprx_aprx;
	uint8_t			rp_pfcrx;
	uint8_t			rp_reserved5[2];
	uint16_t		rp_dev_stall_min;
	uint16_t		rp_dev_stall_crit;
	uint8_t			rp_reserved6[12];
} __packed __aligned(4);

#define MCX_PMLP_MODULE_NUM_MASK	0xff
struct mcx_reg_pmlp {
	uint8_t			rp_rxtx;
	uint8_t			rp_local_port;
	uint8_t			rp_reserved0;
	uint8_t			rp_width;
	uint32_t		rp_lane0_mapping;
	uint32_t		rp_lane1_mapping;
	uint32_t		rp_lane2_mapping;
	uint32_t		rp_lane3_mapping;
	uint8_t			rp_reserved1[44];
} __packed __aligned(4);

#define MCX_MCIA_EEPROM_BYTES	32
struct mcx_reg_mcia {
	uint8_t			rm_l;
	uint8_t			rm_module;
	uint8_t			rm_reserved0;
	uint8_t			rm_status;
	uint8_t			rm_i2c_addr;
	uint8_t			rm_page_num;
	uint16_t		rm_dev_addr;
	uint16_t		rm_reserved1;
	uint16_t		rm_size;
	uint32_t		rm_reserved2;
	uint8_t			rm_data[48];
} __packed __aligned(4);

struct mcx_cmd_query_issi_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_issi_il_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_current_issi;
	uint8_t			cmd_reserved2[4];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_query_issi_il_out) == MCX_CMDQ_INLINE_DATASIZE);

struct mcx_cmd_query_issi_mb_out {
	uint8_t			cmd_reserved2[16];
	uint8_t			cmd_supported_issi[80]; /* very big endian */
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_query_issi_mb_out) <= MCX_CMDQ_MAILBOX_DATASIZE);

struct mcx_cmd_set_issi_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_current_issi;
	uint8_t			cmd_reserved2[4];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_set_issi_in) <= MCX_CMDQ_INLINE_DATASIZE);

struct mcx_cmd_set_issi_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_set_issi_out) <= MCX_CMDQ_INLINE_DATASIZE);

struct mcx_cmd_query_pages_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
#define MCX_CMD_QUERY_PAGES_BOOT	0x01
#define MCX_CMD_QUERY_PAGES_INIT	0x02
#define MCX_CMD_QUERY_PAGES_REGULAR	0x03
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_pages_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_func_id;
	uint32_t		cmd_num_pages;
} __packed __aligned(4);

struct mcx_cmd_manage_pages_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
#define MCX_CMD_MANAGE_PAGES_ALLOC_FAIL \
					0x00
#define MCX_CMD_MANAGE_PAGES_ALLOC_SUCCESS \
					0x01
#define MCX_CMD_MANAGE_PAGES_HCA_RETURN_PAGES \
					0x02
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_func_id;
	uint32_t		cmd_input_num_entries;
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_manage_pages_in) == MCX_CMDQ_INLINE_DATASIZE);

struct mcx_cmd_manage_pages_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_output_num_entries;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cmd_manage_pages_out) == MCX_CMDQ_INLINE_DATASIZE);

struct mcx_cmd_query_hca_cap_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
#define MCX_CMD_QUERY_HCA_CAP_MAX	(0x0 << 0)
#define MCX_CMD_QUERY_HCA_CAP_CURRENT	(0x1 << 0)
#define MCX_CMD_QUERY_HCA_CAP_DEVICE	(0x0 << 1)
#define MCX_CMD_QUERY_HCA_CAP_OFFLOAD	(0x1 << 1)
#define MCX_CMD_QUERY_HCA_CAP_FLOW	(0x7 << 1)
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_hca_cap_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

#define MCX_HCA_CAP_LEN			0x1000
#define MCX_HCA_CAP_NMAILBOXES		\
	(MCX_HCA_CAP_LEN / MCX_CMDQ_MAILBOX_DATASIZE)

#if __GNUC_PREREQ__(4, 3)
#define __counter__		__COUNTER__
#else
#define __counter__		__LINE__
#endif

#define __token(_tok, _num)	_tok##_num
#define _token(_tok, _num)	__token(_tok, _num)
#define __reserved__		_token(__reserved, __counter__)

struct mcx_cap_device {
	uint8_t			reserved0[16];

	uint8_t			log_max_srq_sz;
	uint8_t			log_max_qp_sz;
	uint8_t			__reserved__[1];
	uint8_t			log_max_qp; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_QP	0x1f

	uint8_t			__reserved__[1];
	uint8_t			log_max_srq; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_SRQ	0x1f
	uint8_t			__reserved__[2];

	uint8_t			__reserved__[1];
	uint8_t			log_max_cq_sz;
	uint8_t			__reserved__[1];
	uint8_t			log_max_cq; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_CQ	0x1f

	uint8_t			log_max_eq_sz;
	uint8_t			log_max_mkey; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_MKEY	0x3f
	uint8_t			__reserved__[1];
	uint8_t			log_max_eq; /* 4 bits */
#define MCX_CAP_DEVICE_LOG_MAX_EQ	0x0f

	uint8_t			max_indirection;
	uint8_t			log_max_mrw_sz; /* 7 bits */
#define MCX_CAP_DEVICE_LOG_MAX_MRW_SZ	0x7f
	uint8_t			teardown_log_max_msf_list_size;
#define MCX_CAP_DEVICE_FORCE_TEARDOWN	0x80
#define MCX_CAP_DEVICE_LOG_MAX_MSF_LIST_SIZE \
					0x3f
	uint8_t			log_max_klm_list_size; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_KLM_LIST_SIZE \
					0x3f

	uint8_t			__reserved__[1];
	uint8_t			log_max_ra_req_dc; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_REQ_DC	0x3f
	uint8_t			__reserved__[1];
	uint8_t			log_max_ra_res_dc; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RA_RES_DC \
					0x3f

	uint8_t			__reserved__[1];
	uint8_t			log_max_ra_req_qp; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RA_REQ_QP \
					0x3f
	uint8_t			__reserved__[1];
	uint8_t			log_max_ra_res_qp; /* 6 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RA_RES_QP \
					0x3f

	uint8_t			flags1;
#define MCX_CAP_DEVICE_END_PAD		0x80
#define MCX_CAP_DEVICE_CC_QUERY_ALLOWED	0x40
#define MCX_CAP_DEVICE_CC_MODIFY_ALLOWED \
					0x20
#define MCX_CAP_DEVICE_START_PAD	0x10
#define MCX_CAP_DEVICE_128BYTE_CACHELINE \
					0x08
	uint8_t			__reserved__[1];
	uint16_t		gid_table_size;

	uint16_t		flags2;
#define MCX_CAP_DEVICE_OUT_OF_SEQ_CNT	0x8000
#define MCX_CAP_DEVICE_VPORT_COUNTERS	0x4000
#define MCX_CAP_DEVICE_RETRANSMISSION_Q_COUNTERS \
					0x2000
#define MCX_CAP_DEVICE_DEBUG		0x1000
#define MCX_CAP_DEVICE_MODIFY_RQ_COUNTERS_SET_ID \
					0x8000
#define MCX_CAP_DEVICE_RQ_DELAY_DROP	0x4000
#define MCX_CAP_DEVICe_MAX_QP_CNT_MASK	0x03ff
	uint16_t		pkey_table_size;

	uint8_t			flags3;
#define MCX_CAP_DEVICE_VPORT_GROUP_MANAGER \
					0x80
#define MCX_CAP_DEVICE_VHCA_GROUP_MANAGER \
					0x40
#define MCX_CAP_DEVICE_IB_VIRTUAL	0x20
#define MCX_CAP_DEVICE_ETH_VIRTUAL	0x10
#define MCX_CAP_DEVICE_ETS		0x04
#define MCX_CAP_DEVICE_NIC_FLOW_TABLE	0x02
#define MCX_CAP_DEVICE_ESWITCH_FLOW_TABLE \
					0x01
	uint8_t			local_ca_ack_delay; /* 5 bits */
#define MCX_CAP_DEVICE_LOCAL_CA_ACK_DELAY \
					0x1f
	uint8_t			port_type;
#define MCX_CAP_DEVICE_PORT_MODULE_EVENT \
					0x80
#define MCX_CAP_DEVICE_PORT_TYPE	0x03
	uint8_t			num_ports;

	uint8_t			snapshot_log_max_msg;
#define MCX_CAP_DEVICE_SNAPSHOT		0x80
#define MCX_CAP_DEVICE_LOG_MAX_MSG	0x1f
	uint8_t			max_tc; /* 4 bits */
#define MCX_CAP_DEVICE_MAX_TC		0x0f
	uint8_t			flags4;
#define MCX_CAP_DEVICE_TEMP_WARN_EVENT	0x80
#define MCX_CAP_DEVICE_DCBX		0x40
#define MCX_CAP_DEVICE_ROL_S		0x02
#define MCX_CAP_DEVICE_ROL_G		0x01
	uint8_t			wol;
#define MCX_CAP_DEVICE_WOL_S		0x40
#define MCX_CAP_DEVICE_WOL_G		0x20
#define MCX_CAP_DEVICE_WOL_A		0x10
#define MCX_CAP_DEVICE_WOL_B		0x08
#define MCX_CAP_DEVICE_WOL_M		0x04
#define MCX_CAP_DEVICE_WOL_U		0x02
#define MCX_CAP_DEVICE_WOL_P		0x01

	uint16_t		stat_rate_support;
	uint8_t			__reserved__[1];
	uint8_t			cqe_version; /* 4 bits */
#define MCX_CAP_DEVICE_CQE_VERSION	0x0f

	uint32_t		flags5;
#define MCX_CAP_DEVICE_COMPACT_ADDRESS_VECTOR \
					0x80000000
#define MCX_CAP_DEVICE_STRIDING_RQ	0x40000000
#define MCX_CAP_DEVICE_IPOIP_ENHANCED_OFFLOADS \
					0x10000000
#define MCX_CAP_DEVICE_IPOIP_IPOIP_OFFLOADS \
					0x08000000
#define MCX_CAP_DEVICE_DC_CONNECT_CP	0x00040000
#define MCX_CAP_DEVICE_DC_CNAK_DRACE	0x00020000
#define MCX_CAP_DEVICE_DRAIN_SIGERR	0x00010000
#define MCX_CAP_DEVICE_DRAIN_SIGERR	0x00010000
#define MCX_CAP_DEVICE_CMDIF_CHECKSUM	0x0000c000
#define MCX_CAP_DEVICE_SIGERR_QCE	0x00002000
#define MCX_CAP_DEVICE_WQ_SIGNATURE	0x00000800
#define MCX_CAP_DEVICE_SCTR_DATA_CQE	0x00000400
#define MCX_CAP_DEVICE_SHO		0x00000100
#define MCX_CAP_DEVICE_TPH		0x00000080
#define MCX_CAP_DEVICE_RF		0x00000040
#define MCX_CAP_DEVICE_DCT		0x00000020
#define MCX_CAP_DEVICE_QOS		0x00000010
#define MCX_CAP_DEVICe_ETH_NET_OFFLOADS	0x00000008
#define MCX_CAP_DEVICE_ROCE		0x00000004
#define MCX_CAP_DEVICE_ATOMIC		0x00000002

	uint32_t		flags6;
#define MCX_CAP_DEVICE_CQ_OI		0x80000000
#define MCX_CAP_DEVICE_CQ_RESIZE	0x40000000
#define MCX_CAP_DEVICE_CQ_MODERATION	0x20000000
#define MCX_CAP_DEVICE_CQ_PERIOD_MODE_MODIFY \
					0x10000000
#define MCX_CAP_DEVICE_CQ_INVALIDATE	0x08000000
#define MCX_CAP_DEVICE_RESERVED_AT_255	0x04000000
#define MCX_CAP_DEVICE_CQ_EQ_REMAP	0x02000000
#define MCX_CAP_DEVICE_PG		0x01000000
#define MCX_CAP_DEVICE_BLOCK_LB_MC	0x00800000
#define MCX_CAP_DEVICE_EXPONENTIAL_BACKOFF \
					0x00400000
#define MCX_CAP_DEVICE_SCQE_BREAK_MODERATION \
					0x00200000
#define MCX_CAP_DEVICE_CQ_PERIOD_START_FROM_CQE \
					0x00100000
#define MCX_CAP_DEVICE_CD		0x00080000
#define MCX_CAP_DEVICE_ATM		0x00040000
#define MCX_CAP_DEVICE_APM		0x00020000
#define MCX_CAP_DEVICE_IMAICL		0x00010000
#define MCX_CAP_DEVICE_QKV		0x00000200
#define MCX_CAP_DEVICE_PKV		0x00000100
#define MCX_CAP_DEVICE_SET_DETH_SQPN	0x00000080
#define MCX_CAP_DEVICE_XRC		0x00000008
#define MCX_CAP_DEVICE_UD		0x00000004
#define MCX_CAP_DEVICE_UC		0x00000002
#define MCX_CAP_DEVICE_RC		0x00000001

	uint8_t			uar_flags;
#define MCX_CAP_DEVICE_UAR_4K		0x80
	uint8_t			uar_sz;	/* 6 bits */
#define MCX_CAP_DEVICE_UAR_SZ		0x3f
	uint8_t			__reserved__[1];
	uint8_t			log_pg_sz;

	uint8_t			flags7;
#define MCX_CAP_DEVICE_BF		0x80
#define MCX_CAP_DEVICE_DRIVER_VERSION	0x40
#define MCX_CAP_DEVICE_PAD_TX_ETH_PACKET \
					0x20
	uint8_t			log_bf_reg_size; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_BF_REG_SIZE	0x1f
	uint8_t			__reserved__[2];

	uint16_t		num_of_diagnostic_counters;
	uint16_t		max_wqe_sz_sq;

	uint8_t			__reserved__[2];
	uint16_t		max_wqe_sz_rq;

	uint8_t			__reserved__[2];
	uint16_t		max_wqe_sz_sq_dc;

	uint32_t		max_qp_mcg; /* 25 bits */
#define MCX_CAP_DEVICE_MAX_QP_MCG	0x1ffffff

	uint8_t			__reserved__[3];
	uint8_t			log_max_mcq;

	uint8_t			log_max_transport_domain; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_TRANSORT_DOMAIN \
					0x1f
	uint8_t			log_max_pd; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_PD	0x1f
	uint8_t			__reserved__[1];
	uint8_t			log_max_xrcd; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_XRCD	0x1f

	uint8_t			__reserved__[2];
	uint16_t		max_flow_counter;

	uint8_t			log_max_rq; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RQ	0x1f
	uint8_t			log_max_sq; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_SQ	0x1f
	uint8_t			log_max_tir; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_TIR	0x1f
	uint8_t			log_max_tis; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_TIS	0x1f

	uint8_t 		flags8;
#define MCX_CAP_DEVICE_BASIC_CYCLIC_RCV_WQE \
					0x80
#define MCX_CAP_DEVICE_LOG_MAX_RMP	0x1f
	uint8_t			log_max_rqt; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RQT	0x1f
	uint8_t			log_max_rqt_size; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_RQT_SIZE	0x1f
	uint8_t			log_max_tis_per_sq; /* 5 bits */
#define MCX_CAP_DEVICE_LOG_MAX_TIS_PER_SQ \
					0x1f
} __packed __aligned(8);

CTASSERT(offsetof(struct mcx_cap_device, max_indirection) == 0x20);
CTASSERT(offsetof(struct mcx_cap_device, flags1) == 0x2c);
CTASSERT(offsetof(struct mcx_cap_device, flags2) == 0x30);
CTASSERT(offsetof(struct mcx_cap_device, snapshot_log_max_msg) == 0x38);
CTASSERT(offsetof(struct mcx_cap_device, flags5) == 0x40);
CTASSERT(offsetof(struct mcx_cap_device, flags7) == 0x4c);
CTASSERT(sizeof(struct mcx_cap_device) <= MCX_CMDQ_MAILBOX_DATASIZE);

struct mcx_cmd_set_driver_version_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_driver_version_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_driver_version {
	uint8_t			cmd_driver_version[64];
} __packed __aligned(8);

struct mcx_cmd_modify_nic_vport_context_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[4];
	uint32_t		cmd_field_select;
#define MCX_CMD_MODIFY_NIC_VPORT_CONTEXT_FIELD_ADDR	0x04
#define MCX_CMD_MODIFY_NIC_VPORT_CONTEXT_FIELD_PROMISC	0x10
#define MCX_CMD_MODIFY_NIC_VPORT_CONTEXT_FIELD_MTU	0x40
} __packed __aligned(4);

struct mcx_cmd_modify_nic_vport_context_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_nic_vport_context_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[4];
	uint8_t			cmd_allowed_list_type;
	uint8_t			cmd_reserved2[3];
} __packed __aligned(4);

struct mcx_cmd_query_nic_vport_context_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_nic_vport_ctx {
	uint32_t		vp_min_wqe_inline_mode;
	uint8_t			vp_reserved0[32];
	uint32_t		vp_mtu;
	uint8_t			vp_reserved1[200];
	uint16_t		vp_flags;
#define MCX_NIC_VPORT_CTX_LIST_UC_MAC			(0)
#define MCX_NIC_VPORT_CTX_LIST_MC_MAC			(1 << 24)
#define MCX_NIC_VPORT_CTX_LIST_VLAN			(2 << 24)
#define MCX_NIC_VPORT_CTX_PROMISC_ALL			(1 << 13)
#define MCX_NIC_VPORT_CTX_PROMISC_MCAST			(1 << 14)
#define MCX_NIC_VPORT_CTX_PROMISC_UCAST			(1 << 15)
	uint16_t		vp_allowed_list_size;
	uint64_t		vp_perm_addr;
	uint8_t			vp_reserved2[4];
	/* allowed list follows */
} __packed __aligned(4);

struct mcx_counter {
	uint64_t		packets;
	uint64_t		octets;
} __packed __aligned(4);

struct mcx_nic_vport_counters {
	struct mcx_counter	rx_err;
	struct mcx_counter	tx_err;
	uint8_t			reserved0[64]; /* 0x30 */
	struct mcx_counter	rx_bcast;
	struct mcx_counter	tx_bcast;
	struct mcx_counter	rx_ucast;
	struct mcx_counter	tx_ucast;
	struct mcx_counter	rx_mcast;
	struct mcx_counter	tx_mcast;
	uint8_t			reserved1[0x210 - 0xd0];
} __packed __aligned(4);

struct mcx_cmd_query_vport_counters_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_vport_counters_mb_in {
	uint8_t			cmd_reserved0[8];
	uint8_t			cmd_clear;
	uint8_t			cmd_reserved1[7];
} __packed __aligned(4);

struct mcx_cmd_query_vport_counters_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_counter_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_counter_mb_in {
	uint8_t			cmd_reserved0[8];
	uint8_t			cmd_clear;
	uint8_t			cmd_reserved1[5];
	uint16_t		cmd_flow_counter_id;
} __packed __aligned(4);

struct mcx_cmd_query_flow_counter_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_alloc_uar_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_alloc_uar_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_uar;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_query_special_ctx_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_special_ctx_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[4];
	uint32_t		cmd_resd_lkey;
} __packed __aligned(4);

struct mcx_eq_ctx {
	uint32_t		eq_status;
#define MCX_EQ_CTX_ST_SHIFT		8
#define MCX_EQ_CTX_ST_MASK		(0xf << MCX_EQ_CTX_ST_SHIFT)
#define MCX_EQ_CTX_ST_ARMED		(0x9 << MCX_EQ_CTX_ST_SHIFT)
#define MCX_EQ_CTX_ST_FIRED		(0xa << MCX_EQ_CTX_ST_SHIFT)
#define MCX_EQ_CTX_OI_SHIFT		17
#define MCX_EQ_CTX_OI			(1 << MCX_EQ_CTX_OI_SHIFT)
#define MCX_EQ_CTX_EC_SHIFT		18
#define MCX_EQ_CTX_EC			(1 << MCX_EQ_CTX_EC_SHIFT)
#define MCX_EQ_CTX_STATUS_SHIFT		28
#define MCX_EQ_CTX_STATUS_MASK		(0xf << MCX_EQ_CTX_STATUS_SHIFT)
#define MCX_EQ_CTX_STATUS_OK		(0x0 << MCX_EQ_CTX_STATUS_SHIFT)
#define MCX_EQ_CTX_STATUS_EQ_WRITE_FAILURE \
					(0xa << MCX_EQ_CTX_STATUS_SHIFT)
	uint32_t		eq_reserved1;
	uint32_t		eq_page_offset;
#define MCX_EQ_CTX_PAGE_OFFSET_SHIFT	5
	uint32_t		eq_uar_size;
#define MCX_EQ_CTX_UAR_PAGE_MASK	0xffffff
#define MCX_EQ_CTX_LOG_EQ_SIZE_SHIFT	24
	uint32_t		eq_reserved2;
	uint8_t			eq_reserved3[3];
	uint8_t			eq_intr;
	uint32_t		eq_log_page_size;
#define MCX_EQ_CTX_LOG_PAGE_SIZE_SHIFT	24
	uint32_t		eq_reserved4[3];
	uint32_t		eq_consumer_counter;
	uint32_t		eq_producer_counter;
#define MCX_EQ_CTX_COUNTER_MASK		0xffffff
	uint32_t		eq_reserved5[4];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_eq_ctx) == 64);

struct mcx_cmd_create_eq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_eq_mb_in {
	struct mcx_eq_ctx	cmd_eq_ctx;
	uint8_t			cmd_reserved0[8];
	uint64_t		cmd_event_bitmask;
#define MCX_EVENT_TYPE_COMPLETION	0x00
#define MCX_EVENT_TYPE_CQ_ERROR		0x04
#define MCX_EVENT_TYPE_INTERNAL_ERROR	0x08
#define MCX_EVENT_TYPE_PORT_CHANGE	0x09
#define MCX_EVENT_TYPE_CMD_COMPLETION	0x0a
#define MCX_EVENT_TYPE_PAGE_REQUEST	0x0b
#define MCX_EVENT_TYPE_LAST_WQE		0x13
	uint8_t			cmd_reserved1[176];
} __packed __aligned(4);

struct mcx_cmd_create_eq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_eqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_eq_entry {
	uint8_t			eq_reserved1;
	uint8_t			eq_event_type;
	uint8_t			eq_reserved2;
	uint8_t			eq_event_sub_type;

	uint8_t			eq_reserved3[28];
	uint32_t		eq_event_data[7];
	uint8_t			eq_reserved4[2];
	uint8_t			eq_signature;
	uint8_t			eq_owner;
#define MCX_EQ_ENTRY_OWNER_INIT			1
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_eq_entry) == 64);

struct mcx_cmd_alloc_pd_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_alloc_pd_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_pd;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_alloc_td_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_alloc_td_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_tdomain;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_create_tir_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_tir_mb_in {
	uint8_t			cmd_reserved0[20];
	uint32_t		cmd_disp_type;
#define MCX_TIR_CTX_DISP_TYPE_SHIFT	28
	uint8_t			cmd_reserved1[8];
	uint32_t		cmd_lro;
	uint8_t			cmd_reserved2[8];
	uint32_t		cmd_inline_rqn;
	uint32_t		cmd_indir_table;
	uint32_t		cmd_tdomain;
	uint8_t			cmd_rx_hash_key[40];
	uint32_t		cmd_rx_hash_sel_outer;
	uint32_t		cmd_rx_hash_sel_inner;
	uint8_t			cmd_reserved3[152];
} __packed __aligned(4);

struct mcx_cmd_create_tir_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_tirn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_tir_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_tirn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_tir_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_tis_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_tis_mb_in {
	uint8_t			cmd_reserved[16];
	uint32_t		cmd_prio;
	uint8_t			cmd_reserved1[32];
	uint32_t		cmd_tdomain;
	uint8_t			cmd_reserved2[120];
} __packed __aligned(4);

struct mcx_cmd_create_tis_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_tisn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_tis_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_tisn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_tis_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cq_ctx {
	uint32_t		cq_status;
	uint32_t		cq_reserved1;
	uint32_t		cq_page_offset;
	uint32_t		cq_uar_size;
#define MCX_CQ_CTX_UAR_PAGE_MASK	0xffffff
#define MCX_CQ_CTX_LOG_CQ_SIZE_SHIFT	24
	uint32_t		cq_period_max_count;
#define MCX_CQ_CTX_PERIOD_SHIFT		16
	uint32_t		cq_eqn;
	uint32_t		cq_log_page_size;
#define MCX_CQ_CTX_LOG_PAGE_SIZE_SHIFT	24
	uint32_t		cq_reserved2;
	uint32_t		cq_last_notified;
	uint32_t		cq_last_solicit;
	uint32_t		cq_consumer_counter;
	uint32_t		cq_producer_counter;
	uint8_t			cq_reserved3[8];
	uint64_t		cq_doorbell;
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cq_ctx) == 64);

struct mcx_cmd_create_cq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_cq_mb_in {
	struct mcx_cq_ctx	cmd_cq_ctx;
	uint8_t			cmd_reserved1[192];
} __packed __aligned(4);

struct mcx_cmd_create_cq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_cqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_cq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_cqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_cq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cq_entry {
	uint32_t		__reserved__;
	uint32_t		cq_lro;
	uint32_t		cq_lro_ack_seq_num;
	uint32_t		cq_rx_hash;
	uint8_t			cq_rx_hash_type;
	uint8_t			cq_ml_path;
	uint16_t		__reserved__;
	uint32_t		cq_checksum;
	uint32_t		__reserved__;
	uint32_t		cq_flags;
	uint32_t		cq_lro_srqn;
	uint32_t		__reserved__[2];
	uint32_t		cq_byte_cnt;
	uint64_t		cq_timestamp;
	uint8_t			cq_rx_drops;
	uint8_t			cq_flow_tag[3];
	uint16_t		cq_wqe_count;
	uint8_t			cq_signature;
	uint8_t			cq_opcode_owner;
#define MCX_CQ_ENTRY_FLAG_OWNER			(1 << 0)
#define MCX_CQ_ENTRY_FLAG_SE			(1 << 1)
#define MCX_CQ_ENTRY_FORMAT_SHIFT		2
#define MCX_CQ_ENTRY_OPCODE_SHIFT		4

#define MCX_CQ_ENTRY_FORMAT_NO_INLINE		0
#define MCX_CQ_ENTRY_FORMAT_INLINE_32		1
#define MCX_CQ_ENTRY_FORMAT_INLINE_64		2
#define MCX_CQ_ENTRY_FORMAT_COMPRESSED		3

#define MCX_CQ_ENTRY_OPCODE_REQ			0
#define MCX_CQ_ENTRY_OPCODE_SEND		2
#define MCX_CQ_ENTRY_OPCODE_REQ_ERR		13
#define MCX_CQ_ENTRY_OPCODE_SEND_ERR		14
#define MCX_CQ_ENTRY_OPCODE_INVALID		15

} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_cq_entry) == 64);

struct mcx_cq_doorbell {
	uint32_t		 db_update_ci;
	uint32_t		 db_arm_ci;
#define MCX_CQ_DOORBELL_ARM_CMD_SN_SHIFT	28
#define MCX_CQ_DOORBELL_ARM_CMD			(1 << 24)
#define MCX_CQ_DOORBELL_ARM_CI_MASK		(0xffffff)
} __packed __aligned(8);

struct mcx_wq_ctx {
	uint8_t			 wq_type;
#define MCX_WQ_CTX_TYPE_CYCLIC			(1 << 4)
#define MCX_WQ_CTX_TYPE_SIGNATURE		(1 << 3)
	uint8_t			 wq_reserved0[5];
	uint16_t		 wq_lwm;
	uint32_t		 wq_pd;
	uint32_t		 wq_uar_page;
	uint64_t		 wq_doorbell;
	uint32_t		 wq_hw_counter;
	uint32_t		 wq_sw_counter;
	uint16_t		 wq_log_stride;
	uint8_t			 wq_log_page_sz;
	uint8_t			 wq_log_size;
	uint8_t			 wq_reserved1[156];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_wq_ctx) == 0xC0);

struct mcx_sq_ctx {
	uint32_t		sq_flags;
#define MCX_SQ_CTX_RLKEY			(1 << 31)
#define MCX_SQ_CTX_FRE_SHIFT			(1 << 29)
#define MCX_SQ_CTX_FLUSH_IN_ERROR		(1 << 28)
#define MCX_SQ_CTX_MIN_WQE_INLINE_SHIFT		24
#define MCX_SQ_CTX_STATE_SHIFT			20
	uint32_t		sq_user_index;
	uint32_t		sq_cqn;
	uint32_t		sq_reserved1[5];
	uint32_t		sq_tis_lst_sz;
#define MCX_SQ_CTX_TIS_LST_SZ_SHIFT		16
	uint32_t		sq_reserved2[2];
	uint32_t		sq_tis_num;
	struct mcx_wq_ctx	sq_wq;
} __packed __aligned(4);

struct mcx_sq_entry_seg {
	uint32_t		sqs_byte_count;
	uint32_t		sqs_lkey;
	uint64_t		sqs_addr;
} __packed __aligned(4);

struct mcx_sq_entry {
	/* control segment */
	uint32_t		sqe_opcode_index;
#define MCX_SQE_WQE_INDEX_SHIFT			8
#define MCX_SQE_WQE_OPCODE_NOP			0x00
#define MCX_SQE_WQE_OPCODE_SEND			0x0a
	uint32_t		sqe_ds_sq_num;
#define MCX_SQE_SQ_NUM_SHIFT			8
	uint32_t		sqe_signature;
#define MCX_SQE_SIGNATURE_SHIFT			24
#define MCX_SQE_SOLICITED_EVENT			0x02
#define MCX_SQE_CE_CQE_ON_ERR			0x00
#define MCX_SQE_CE_CQE_FIRST_ERR		0x04
#define MCX_SQE_CE_CQE_ALWAYS			0x08
#define MCX_SQE_CE_CQE_SOLICIT			0x0C
#define MCX_SQE_FM_NO_FENCE			0x00
#define MCX_SQE_FM_SMALL_FENCE			0x40
	uint32_t		sqe_mkey;

	/* ethernet segment */
	uint32_t		sqe_reserved1;
	uint32_t		sqe_mss_csum;
#define MCX_SQE_L4_CSUM				(1 << 31)
#define MCX_SQE_L3_CSUM				(1 << 30)
	uint32_t		sqe_reserved2;
	uint16_t		sqe_inline_header_size;
	uint16_t		sqe_inline_headers[9];

	/* data segment */
	struct mcx_sq_entry_seg sqe_segs[1];
} __packed __aligned(64);

CTASSERT(sizeof(struct mcx_sq_entry) == 64);

struct mcx_cmd_create_sq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_sq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_sqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_modify_sq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_sq_state;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_modify_sq_mb_in {
	uint32_t		cmd_modify_hi;
	uint32_t		cmd_modify_lo;
	uint8_t			cmd_reserved0[8];
	struct mcx_sq_ctx	cmd_sq_ctx;
} __packed __aligned(4);

struct mcx_cmd_modify_sq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_destroy_sq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_sqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_sq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);


struct mcx_rq_ctx {
	uint32_t		rq_flags;
#define MCX_RQ_CTX_RLKEY			(1 << 31)
#define MCX_RQ_CTX_VLAN_STRIP_DIS		(1 << 28)
#define MCX_RQ_CTX_MEM_RQ_TYPE_SHIFT		24
#define MCX_RQ_CTX_STATE_SHIFT			20
#define MCX_RQ_CTX_FLUSH_IN_ERROR		(1 << 18)
	uint32_t		rq_user_index;
	uint32_t		rq_cqn;
	uint32_t		rq_reserved1;
	uint32_t		rq_rmpn;
	uint32_t		rq_reserved2[7];
	struct mcx_wq_ctx	rq_wq;
} __packed __aligned(4);

struct mcx_rq_entry {
	uint32_t		rqe_byte_count;
	uint32_t		rqe_lkey;
	uint64_t		rqe_addr;
} __packed __aligned(16);

struct mcx_cmd_create_rq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_rq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_rqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_modify_rq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_rq_state;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_modify_rq_mb_in {
	uint32_t		cmd_modify_hi;
	uint32_t		cmd_modify_lo;
	uint8_t			cmd_reserved0[8];
	struct mcx_rq_ctx	cmd_rq_ctx;
} __packed __aligned(4);

struct mcx_cmd_modify_rq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_destroy_rq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_rqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_rq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_flow_table_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_flow_table_ctx {
	uint8_t			ft_miss_action;
	uint8_t			ft_level;
	uint8_t			ft_reserved0;
	uint8_t			ft_log_size;
	uint32_t		ft_table_miss_id;
	uint8_t			ft_reserved1[28];
} __packed __aligned(4);

struct mcx_cmd_create_flow_table_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[7];
	struct mcx_flow_table_ctx cmd_ctx;
} __packed __aligned(4);

struct mcx_cmd_create_flow_table_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_destroy_flow_table_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_destroy_flow_table_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[40];
} __packed __aligned(4);

struct mcx_cmd_destroy_flow_table_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_root_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_root_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[56];
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_root_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_flow_match {
	/* outer headers */
	uint8_t			mc_src_mac[6];
	uint16_t		mc_ethertype;
	uint8_t			mc_dest_mac[6];
	uint16_t		mc_first_vlan;
	uint8_t			mc_ip_proto;
	uint8_t			mc_ip_dscp_ecn;
	uint8_t			mc_vlan_flags;
	uint8_t			mc_tcp_flags;
	uint16_t		mc_tcp_sport;
	uint16_t		mc_tcp_dport;
	uint32_t		mc_reserved0;
	uint16_t		mc_udp_sport;
	uint16_t		mc_udp_dport;
	uint8_t			mc_src_ip[16];
	uint8_t			mc_dest_ip[16];

	/* misc parameters */
	uint8_t			mc_reserved1[8];
	uint16_t		mc_second_vlan;
	uint8_t			mc_reserved2[2];
	uint8_t			mc_second_vlan_flags;
	uint8_t			mc_reserved3[15];
	uint32_t		mc_outer_ipv6_flow_label;
	uint8_t			mc_reserved4[32];

	uint8_t			mc_reserved[384];
} __packed __aligned(4);

CTASSERT(sizeof(struct mcx_flow_match) == 512);

struct mcx_cmd_create_flow_group_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_create_flow_group_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[4];
	uint32_t		cmd_start_flow_index;
	uint8_t			cmd_reserved2[4];
	uint32_t		cmd_end_flow_index;
	uint8_t			cmd_reserved3[23];
	uint8_t			cmd_match_criteria_enable;
#define MCX_CREATE_FLOW_GROUP_CRIT_OUTER	(1 << 0)
#define MCX_CREATE_FLOW_GROUP_CRIT_MISC		(1 << 1)
#define MCX_CREATE_FLOW_GROUP_CRIT_INNER	(1 << 2)
	struct mcx_flow_match	cmd_match_criteria;
	uint8_t			cmd_reserved4[448];
} __packed __aligned(4);

struct mcx_cmd_create_flow_group_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint32_t		cmd_group_id;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_flow_ctx {
	uint8_t			fc_reserved0[4];
	uint32_t		fc_group_id;
	uint32_t		fc_flow_tag;
	uint32_t		fc_action;
#define MCX_FLOW_CONTEXT_ACTION_ALLOW		(1 << 0)
#define MCX_FLOW_CONTEXT_ACTION_DROP		(1 << 1)
#define MCX_FLOW_CONTEXT_ACTION_FORWARD		(1 << 2)
#define MCX_FLOW_CONTEXT_ACTION_COUNT		(1 << 3)
	uint32_t		fc_dest_list_size;
	uint32_t		fc_counter_list_size;
	uint8_t			fc_reserved1[40];
	struct mcx_flow_match	fc_match_value;
	uint8_t			fc_reserved2[192];
} __packed __aligned(4);

#define MCX_FLOW_CONTEXT_DEST_TYPE_TABLE	(1 << 24)
#define MCX_FLOW_CONTEXT_DEST_TYPE_TIR		(2 << 24)

struct mcx_cmd_destroy_flow_group_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_destroy_flow_group_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint32_t		cmd_group_id;
	uint8_t			cmd_reserved1[36];
} __packed __aligned(4);

struct mcx_cmd_destroy_flow_group_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_entry_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_entry_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint32_t		cmd_modify_enable_mask;
	uint8_t			cmd_reserved1[4];
	uint32_t		cmd_flow_index;
	uint8_t			cmd_reserved2[28];
	struct mcx_flow_ctx	cmd_flow_ctx;
} __packed __aligned(4);

struct mcx_cmd_set_flow_table_entry_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_entry_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_entry_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[8];
	uint32_t		cmd_flow_index;
	uint8_t			cmd_reserved2[28];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_entry_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_entry_mb_out {
	uint8_t			cmd_reserved0[48];
	struct mcx_flow_ctx	cmd_flow_ctx;
} __packed __aligned(4);

struct mcx_cmd_delete_flow_table_entry_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_delete_flow_table_entry_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[8];
	uint32_t		cmd_flow_index;
	uint8_t			cmd_reserved2[28];
} __packed __aligned(4);

struct mcx_cmd_delete_flow_table_entry_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_group_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_group_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint32_t		cmd_group_id;
	uint8_t			cmd_reserved1[36];
} __packed __aligned(4);

struct mcx_cmd_query_flow_group_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_group_mb_out {
	uint8_t			cmd_reserved0[12];
	uint32_t		cmd_start_flow_index;
	uint8_t			cmd_reserved1[4];
	uint32_t		cmd_end_flow_index;
	uint8_t			cmd_reserved2[20];
	uint32_t		cmd_match_criteria_enable;
	uint8_t			cmd_match_criteria[512];
	uint8_t			cmd_reserved4[448];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_mb_in {
	uint8_t			cmd_table_type;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_table_id;
	uint8_t			cmd_reserved1[40];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_flow_table_mb_out {
	uint8_t			cmd_reserved0[4];
	struct mcx_flow_table_ctx cmd_ctx;
} __packed __aligned(4);

struct mcx_cmd_alloc_flow_counter_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_rq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_rqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_query_rq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_rq_mb_out {
	uint8_t			cmd_reserved0[16];
	struct mcx_rq_ctx	cmd_ctx;
};

struct mcx_cmd_query_sq_in {
	uint16_t		cmd_opcode;
	uint8_t			cmd_reserved0[4];
	uint16_t		cmd_op_mod;
	uint32_t		cmd_sqn;
	uint8_t			cmd_reserved1[4];
} __packed __aligned(4);

struct mcx_cmd_query_sq_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[8];
} __packed __aligned(4);

struct mcx_cmd_query_sq_mb_out {
	uint8_t			cmd_reserved0[16];
	struct mcx_sq_ctx	cmd_ctx;
};

struct mcx_cmd_alloc_flow_counter_out {
	uint8_t			cmd_status;
	uint8_t			cmd_reserved0[3];
	uint32_t		cmd_syndrome;
	uint8_t			cmd_reserved1[2];
	uint16_t		cmd_flow_counter_id;
	uint8_t			cmd_reserved2[4];
} __packed __aligned(4);

struct mcx_wq_doorbell {
	uint32_t		 db_recv_counter;
	uint32_t		 db_send_counter;
} __packed __aligned(8);

struct mcx_dmamem {
	bus_dmamap_t		 mxm_map;
	bus_dma_segment_t	 mxm_seg;
	int			 mxm_nsegs;
	size_t			 mxm_size;
	caddr_t			 mxm_kva;
};
#define MCX_DMA_MAP(_mxm)	((_mxm)->mxm_map)
#define MCX_DMA_DVA(_mxm)	((_mxm)->mxm_map->dm_segs[0].ds_addr)
#define MCX_DMA_KVA(_mxm)	((void *)(_mxm)->mxm_kva)
#define MCX_DMA_LEN(_mxm)	((_mxm)->mxm_size)

struct mcx_hwmem {
	bus_dmamap_t		 mhm_map;
	bus_dma_segment_t	*mhm_segs;
	unsigned int		 mhm_seg_count;
	unsigned int		 mhm_npages;
};

struct mcx_slot {
	bus_dmamap_t		 ms_map;
	struct mbuf		*ms_m;
};

struct mcx_cq {
	int			 cq_n;
	struct mcx_dmamem	 cq_mem;
	uint32_t		*cq_doorbell;
	uint32_t		 cq_cons;
	uint32_t		 cq_count;
};

struct mcx_calibration {
	uint64_t		 c_timestamp;	/* previous mcx chip time */
	uint64_t		 c_uptime;	/* previous kernel nanouptime */
	uint64_t		 c_tbase;	/* mcx chip time */
	uint64_t		 c_ubase;	/* kernel nanouptime */
	uint64_t		 c_tdiff;
	uint64_t		 c_udiff;
};

#define MCX_CALIBRATE_FIRST    2
#define MCX_CALIBRATE_NORMAL   30

struct mcx_softc {
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

	struct mcx_dmamem	 sc_cmdq_mem;
	unsigned int		 sc_cmdq_mask;
	unsigned int		 sc_cmdq_size;

	unsigned int		 sc_cmdq_token;

	struct mcx_hwmem	 sc_boot_pages;
	struct mcx_hwmem	 sc_init_pages;
	struct mcx_hwmem	 sc_regular_pages;

	int			 sc_uar;
	int			 sc_pd;
	int			 sc_tdomain;
	uint32_t		 sc_lkey;

	struct mcx_dmamem	 sc_doorbell_mem;

	int			 sc_eqn;
	uint32_t		 sc_eq_cons;
	struct mcx_dmamem	 sc_eq_mem;
	int			 sc_hardmtu;

	struct task		 sc_port_change;

	int			 sc_flow_table_id;
#define MCX_FLOW_GROUP_PROMISC	 0
#define MCX_FLOW_GROUP_ALLMULTI	 1
#define MCX_FLOW_GROUP_MAC	 2
#define MCX_NUM_FLOW_GROUPS	 3
	int			 sc_flow_group_id[MCX_NUM_FLOW_GROUPS];
	int			 sc_flow_group_size[MCX_NUM_FLOW_GROUPS];
	int			 sc_flow_group_start[MCX_NUM_FLOW_GROUPS];
	int			 sc_promisc_flow_enabled;
	int			 sc_allmulti_flow_enabled;
	int			 sc_mcast_flow_base;
	int			 sc_extra_mcast;
	uint8_t			 sc_mcast_flows[MCX_NUM_MCAST_FLOWS][ETHER_ADDR_LEN];

	struct mcx_calibration	 sc_calibration[2];
	unsigned int		 sc_calibration_gen;
	struct timeout		 sc_calibrate;

	struct mcx_cq		 sc_cq[MCX_MAX_CQS];
	int			 sc_num_cq;

	/* rx */
	int			 sc_tirn;
	int			 sc_rqn;
	struct mcx_dmamem	 sc_rq_mem;
	struct mcx_slot		*sc_rx_slots;
	uint32_t		*sc_rx_doorbell;

	uint32_t		 sc_rx_prod;
	struct timeout		 sc_rx_refill;
	struct if_rxring	 sc_rxr;

	/* tx */
	int			 sc_tisn;
	int			 sc_sqn;
	struct mcx_dmamem	 sc_sq_mem;
	struct mcx_slot		*sc_tx_slots;
	uint32_t		*sc_tx_doorbell;
	int			 sc_bf_size;
	int			 sc_bf_offset;

	uint32_t		 sc_tx_cons;
	uint32_t		 sc_tx_prod;

	uint64_t		 sc_last_cq_db;
	uint64_t		 sc_last_srq_db;
};
#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)

static int	mcx_match(struct device *, void *, void *);
static void	mcx_attach(struct device *, struct device *, void *);

static int	mcx_version(struct mcx_softc *);
static int	mcx_init_wait(struct mcx_softc *);
static int	mcx_enable_hca(struct mcx_softc *);
static int	mcx_teardown_hca(struct mcx_softc *, uint16_t);
static int	mcx_access_hca_reg(struct mcx_softc *, uint16_t, int, void *,
		    int);
static int	mcx_issi(struct mcx_softc *);
static int	mcx_pages(struct mcx_softc *, struct mcx_hwmem *, uint16_t);
static int	mcx_hca_max_caps(struct mcx_softc *);
static int	mcx_hca_set_caps(struct mcx_softc *);
static int	mcx_init_hca(struct mcx_softc *);
static int	mcx_set_driver_version(struct mcx_softc *);
static int	mcx_iff(struct mcx_softc *);
static int	mcx_alloc_uar(struct mcx_softc *);
static int	mcx_alloc_pd(struct mcx_softc *);
static int	mcx_alloc_tdomain(struct mcx_softc *);
static int	mcx_create_eq(struct mcx_softc *);
static int	mcx_query_nic_vport_context(struct mcx_softc *);
static int	mcx_query_special_contexts(struct mcx_softc *);
static int	mcx_set_port_mtu(struct mcx_softc *, int);
static int	mcx_create_cq(struct mcx_softc *, int);
static int	mcx_destroy_cq(struct mcx_softc *, int);
static int	mcx_create_sq(struct mcx_softc *, int);
static int	mcx_destroy_sq(struct mcx_softc *);
static int	mcx_ready_sq(struct mcx_softc *);
static int	mcx_create_rq(struct mcx_softc *, int);
static int	mcx_destroy_rq(struct mcx_softc *);
static int	mcx_ready_rq(struct mcx_softc *);
static int	mcx_create_tir(struct mcx_softc *);
static int	mcx_destroy_tir(struct mcx_softc *);
static int	mcx_create_tis(struct mcx_softc *);
static int	mcx_destroy_tis(struct mcx_softc *);
static int	mcx_create_flow_table(struct mcx_softc *, int);
static int	mcx_set_flow_table_root(struct mcx_softc *);
static int	mcx_destroy_flow_table(struct mcx_softc *);
static int	mcx_create_flow_group(struct mcx_softc *, int, int,
		    int, int, struct mcx_flow_match *);
static int	mcx_destroy_flow_group(struct mcx_softc *, int);
static int	mcx_set_flow_table_entry(struct mcx_softc *, int, int,
		    uint8_t *);
static int	mcx_delete_flow_table_entry(struct mcx_softc *, int, int);

#if 0
static int	mcx_dump_flow_table(struct mcx_softc *);
static int	mcx_dump_flow_table_entry(struct mcx_softc *, int);
static int	mcx_dump_flow_group(struct mcx_softc *);
static int	mcx_dump_rq(struct mcx_softc *);
static int	mcx_dump_sq(struct mcx_softc *);
#endif


/*
static void	mcx_cmdq_dump(const struct mcx_cmdq_entry *);
static void	mcx_cmdq_mbox_dump(struct mcx_dmamem *, int);
*/
static void	mcx_refill(void *);
static int	mcx_process_rx(struct mcx_softc *, struct mcx_cq_entry *,
		    struct mbuf_list *, const struct mcx_calibration *);
static void	mcx_process_txeof(struct mcx_softc *, struct mcx_cq_entry *,
		    int *);
static void	mcx_process_cq(struct mcx_softc *, struct mcx_cq *);

static void	mcx_arm_cq(struct mcx_softc *, struct mcx_cq *);
static void	mcx_arm_eq(struct mcx_softc *);
static int	mcx_intr(void *);

static int	mcx_up(struct mcx_softc *);
static void	mcx_down(struct mcx_softc *);
static int	mcx_ioctl(struct ifnet *, u_long, caddr_t);
static int	mcx_rxrinfo(struct mcx_softc *, struct if_rxrinfo *);
static void	mcx_start(struct ifqueue *);
static void	mcx_watchdog(struct ifnet *);
static void	mcx_media_add_types(struct mcx_softc *);
static void	mcx_media_status(struct ifnet *, struct ifmediareq *);
static int	mcx_media_change(struct ifnet *);
static int	mcx_get_sffpage(struct ifnet *, struct if_sffpage *);
static void	mcx_port_change(void *);

static void	mcx_calibrate_first(struct mcx_softc *);
static void	mcx_calibrate(void *);

static inline uint32_t
		mcx_rd(struct mcx_softc *, bus_size_t);
static inline void
		mcx_wr(struct mcx_softc *, bus_size_t, uint32_t);
static inline void
		mcx_bar(struct mcx_softc *, bus_size_t, bus_size_t, int);

static uint64_t	mcx_timer(struct mcx_softc *);

static int	mcx_dmamem_alloc(struct mcx_softc *, struct mcx_dmamem *,
		    bus_size_t, u_int align);
static void	mcx_dmamem_zero(struct mcx_dmamem *);
static void	mcx_dmamem_free(struct mcx_softc *, struct mcx_dmamem *);

static int	mcx_hwmem_alloc(struct mcx_softc *, struct mcx_hwmem *,
		    unsigned int);
static void	mcx_hwmem_free(struct mcx_softc *, struct mcx_hwmem *);

struct cfdriver mcx_cd = {
	NULL,
	"mcx",
	DV_IFNET,
};

struct cfattach mcx_ca = {
	sizeof(struct mcx_softc),
	mcx_match,
	mcx_attach,
};

static const struct pci_matchid mcx_devices[] = {
	{ PCI_VENDOR_MELLANOX,	PCI_PRODUCT_MELLANOX_MT27700 },
	{ PCI_VENDOR_MELLANOX,	PCI_PRODUCT_MELLANOX_MT27710 },
	{ PCI_VENDOR_MELLANOX,	PCI_PRODUCT_MELLANOX_MT27800 },
	{ PCI_VENDOR_MELLANOX,	PCI_PRODUCT_MELLANOX_MT28800 },
};

struct mcx_eth_proto_capability {
	uint64_t	cap_media;
	uint64_t	cap_baudrate;
};

static const struct mcx_eth_proto_capability mcx_eth_cap_map[] = {
	[MCX_ETHER_CAP_SGMII]		= { IFM_1000_SGMII,	IF_Gbps(1) },
	[MCX_ETHER_CAP_1000_KX]		= { IFM_1000_KX,	IF_Gbps(1) },
	[MCX_ETHER_CAP_10G_CX4]		= { IFM_10G_CX4,	IF_Gbps(10) },
	[MCX_ETHER_CAP_10G_KX4]		= { IFM_10G_KX4,	IF_Gbps(10) },
	[MCX_ETHER_CAP_10G_KR]		= { IFM_10G_KR,		IF_Gbps(10) },
	[MCX_ETHER_CAP_40G_CR4]		= { IFM_40G_CR4,	IF_Gbps(40) },
	[MCX_ETHER_CAP_40G_KR4]		= { IFM_40G_KR4,	IF_Gbps(40) },
	[MCX_ETHER_CAP_10G_CR]		= { IFM_10G_SFP_CU,	IF_Gbps(10) },
	[MCX_ETHER_CAP_10G_SR]		= { IFM_10G_SR,		IF_Gbps(10) },
	[MCX_ETHER_CAP_10G_LR]		= { IFM_10G_LR,		IF_Gbps(10) },
	[MCX_ETHER_CAP_40G_SR4]		= { IFM_40G_SR4,	IF_Gbps(40) },
	[MCX_ETHER_CAP_40G_LR4]		= { IFM_40G_LR4,	IF_Gbps(40) },
	[MCX_ETHER_CAP_50G_SR2]		= { 0 /*IFM_50G_SR2*/,	IF_Gbps(50) },
	[MCX_ETHER_CAP_100G_CR4]	= { IFM_100G_CR4,	IF_Gbps(100) },
	[MCX_ETHER_CAP_100G_SR4]	= { IFM_100G_SR4,	IF_Gbps(100) },
	[MCX_ETHER_CAP_100G_KR4]	= { IFM_100G_KR4,	IF_Gbps(100) },
	[MCX_ETHER_CAP_25G_CR]		= { IFM_25G_CR,		IF_Gbps(25) },
	[MCX_ETHER_CAP_25G_KR]		= { IFM_25G_KR,		IF_Gbps(25) },
	[MCX_ETHER_CAP_25G_SR]		= { IFM_25G_SR,		IF_Gbps(25) },
	[MCX_ETHER_CAP_50G_CR2]		= { IFM_50G_CR2,	IF_Gbps(50) },
	[MCX_ETHER_CAP_50G_KR2]		= { IFM_50G_KR2,	IF_Gbps(50) },
};

static int
mcx_get_id(uint32_t val)
{
	return betoh32(val) & 0x00ffffff;
}

static int
mcx_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, mcx_devices, nitems(mcx_devices)));
}

void
mcx_attach(struct device *parent, struct device *self, void *aux)
{
	struct mcx_softc *sc = (struct mcx_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct pci_attach_args *pa = aux;
	pcireg_t memtype;
	uint32_t r;
	unsigned int cq_stride;
	unsigned int cq_size;
	const char *intrstr;
	int i;

	sc->sc_pc = pa->pa_pc;
	sc->sc_tag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	/* Map the PCI memory space */
	memtype = pci_mapreg_type(sc->sc_pc, sc->sc_tag, MCX_HCA_BAR);
	if (pci_mapreg_map(pa, MCX_HCA_BAR, memtype,
	    BUS_SPACE_MAP_PREFETCHABLE, &sc->sc_memt, &sc->sc_memh,
	    NULL, &sc->sc_mems, 0)) {
		printf(": unable to map register memory\n");
		return;
	}

	if (mcx_version(sc) != 0) {
		/* error printed by mcx_version */
		goto unmap;
	}

	r = mcx_rd(sc, MCX_CMDQ_ADDR_LO);
	cq_stride = 1 << MCX_CMDQ_LOG_STRIDE(r); /* size of the entries */
	cq_size = 1 << MCX_CMDQ_LOG_SIZE(r); /* number of entries */
	if (cq_size > MCX_MAX_CQE) {
		printf(", command queue size overflow %u\n", cq_size);
		goto unmap;
	}
	if (cq_stride < sizeof(struct mcx_cmdq_entry)) {
		printf(", command queue entry size underflow %u\n", cq_stride);
		goto unmap;
	}
	if (cq_stride * cq_size > MCX_PAGE_SIZE) {
		printf(", command queue page overflow\n");
		goto unmap;
	}

	if (mcx_dmamem_alloc(sc, &sc->sc_doorbell_mem, MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf(", unable to allocate doorbell memory\n");
		goto unmap;
	}

	if (mcx_dmamem_alloc(sc, &sc->sc_cmdq_mem, MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf(", unable to allocate command queue\n");
		goto dbfree;
	}

	mcx_wr(sc, MCX_CMDQ_ADDR_HI, MCX_DMA_DVA(&sc->sc_cmdq_mem) >> 32);
	mcx_bar(sc, MCX_CMDQ_ADDR_HI, sizeof(uint32_t), BUS_SPACE_BARRIER_WRITE);
	mcx_wr(sc, MCX_CMDQ_ADDR_LO, MCX_DMA_DVA(&sc->sc_cmdq_mem));
	mcx_bar(sc, MCX_CMDQ_ADDR_LO, sizeof(uint32_t), BUS_SPACE_BARRIER_WRITE);

	if (mcx_init_wait(sc) != 0) {
		printf(", timeout waiting for init\n");
		goto cqfree;
	}

	sc->sc_cmdq_mask = cq_size - 1;
	sc->sc_cmdq_size = cq_stride;

	if (mcx_enable_hca(sc) != 0) {
		/* error printed by mcx_enable_hca */
		goto cqfree;
	}

	if (mcx_issi(sc) != 0) {
		/* error printed by mcx_issi */
		goto teardown;
	}

	if (mcx_pages(sc, &sc->sc_boot_pages,
	    htobe16(MCX_CMD_QUERY_PAGES_BOOT)) != 0) {
		/* error printed by mcx_pages */
		goto teardown;
	}

	if (mcx_hca_max_caps(sc) != 0) {
		/* error printed by mcx_hca_max_caps */
		goto teardown;
	}

	if (mcx_hca_set_caps(sc) != 0) {
		/* error printed by mcx_hca_set_caps */
		goto teardown;
	}

	if (mcx_pages(sc, &sc->sc_init_pages,
	    htobe16(MCX_CMD_QUERY_PAGES_INIT)) != 0) {
		/* error printed by mcx_pages */
		goto teardown;
	}

	if (mcx_init_hca(sc) != 0) {
		/* error printed by mcx_init_hca */
		goto teardown;
	}

	if (mcx_pages(sc, &sc->sc_regular_pages,
	    htobe16(MCX_CMD_QUERY_PAGES_REGULAR)) != 0) {
		/* error printed by mcx_pages */
		goto teardown;
	}

	/* apparently not necessary? */
	if (mcx_set_driver_version(sc) != 0) {
		/* error printed by mcx_set_driver_version */
		goto teardown;
	}

	if (mcx_iff(sc) != 0) {	/* modify nic vport context */
		/* error printed by mcx_iff? */
		goto teardown;
	}

	if (mcx_alloc_uar(sc) != 0) {
		/* error printed by mcx_alloc_uar */
		goto teardown;
	}

	if (mcx_alloc_pd(sc) != 0) {
		/* error printed by mcx_alloc_pd */
		goto teardown;
	}

	if (mcx_alloc_tdomain(sc) != 0) {
		/* error printed by mcx_alloc_tdomain */
		goto teardown;
	}

	/*
	 * PRM makes no mention of msi interrupts, just legacy and msi-x.
	 * mellanox support tells me legacy interrupts are not supported,
	 * so we're stuck with just msi-x.
	 */
	if (pci_intr_map_msix(pa, 0, &sc->sc_ih) != 0) {
		printf(": unable to map interrupt\n");
		goto teardown;
	}
	intrstr = pci_intr_string(sc->sc_pc, sc->sc_ih);
	sc->sc_ihc = pci_intr_establish(sc->sc_pc, sc->sc_ih,
	    IPL_NET | IPL_MPSAFE, mcx_intr, sc, DEVNAME(sc));
	if (sc->sc_ihc == NULL) {
		printf(": unable to establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto teardown;
	}

	if (mcx_create_eq(sc) != 0) {
		/* error printed by mcx_create_eq */
		goto teardown;
	}

	if (mcx_query_nic_vport_context(sc) != 0) {
		/* error printed by mcx_query_nic_vport_context */
		goto teardown;
	}

	if (mcx_query_special_contexts(sc) != 0) {
		/* error printed by mcx_query_special_contexts */
		goto teardown;
	}

	if (mcx_set_port_mtu(sc, MCX_HARDMTU) != 0) {
		/* error printed by mcx_set_port_mtu */
		goto teardown;
	}

	printf(", %s, address %s\n", intrstr,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	strlcpy(ifp->if_xname, DEVNAME(sc), IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = mcx_ioctl;
	ifp->if_qstart = mcx_start;
	ifp->if_watchdog = mcx_watchdog;
	ifp->if_hardmtu = sc->sc_hardmtu;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, 1024);

	ifmedia_init(&sc->sc_media, IFM_IMASK, mcx_media_change,
	    mcx_media_status);
	mcx_media_add_types(sc);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	timeout_set(&sc->sc_rx_refill, mcx_refill, sc);
	timeout_set(&sc->sc_calibrate, mcx_calibrate, sc);

	task_set(&sc->sc_port_change, mcx_port_change, sc);
	mcx_port_change(sc);

	sc->sc_flow_table_id = -1;
	for (i = 0; i < MCX_NUM_FLOW_GROUPS; i++) {
		sc->sc_flow_group_id[i] = -1;
		sc->sc_flow_group_size[i] = 0;
		sc->sc_flow_group_start[i] = 0;
	}
	sc->sc_extra_mcast = 0;
	memset(sc->sc_mcast_flows, 0, sizeof(sc->sc_mcast_flows));
	return;

teardown:
	mcx_teardown_hca(sc, htobe16(MCX_CMD_TEARDOWN_HCA_GRACEFUL));
	/* error printed by mcx_teardown_hca, and we're already unwinding */
cqfree:
	mcx_wr(sc, MCX_CMDQ_ADDR_HI, MCX_DMA_DVA(&sc->sc_cmdq_mem) >> 32);
	mcx_bar(sc, MCX_CMDQ_ADDR_HI, sizeof(uint64_t), BUS_SPACE_BARRIER_WRITE);
	mcx_wr(sc, MCX_CMDQ_ADDR_LO, MCX_DMA_DVA(&sc->sc_cmdq_mem) |
	    MCX_CMDQ_INTERFACE_DISABLED);
	mcx_bar(sc, MCX_CMDQ_ADDR_LO, sizeof(uint64_t), BUS_SPACE_BARRIER_WRITE);

	mcx_wr(sc, MCX_CMDQ_ADDR_HI, 0);
	mcx_bar(sc, MCX_CMDQ_ADDR_HI, sizeof(uint64_t), BUS_SPACE_BARRIER_WRITE);
	mcx_wr(sc, MCX_CMDQ_ADDR_LO, MCX_CMDQ_INTERFACE_DISABLED);

	mcx_dmamem_free(sc, &sc->sc_cmdq_mem);
dbfree:
	mcx_dmamem_free(sc, &sc->sc_doorbell_mem);
unmap:
	bus_space_unmap(sc->sc_memt, sc->sc_memh, sc->sc_mems);
	sc->sc_mems = 0;
}

static int
mcx_version(struct mcx_softc *sc)
{
	uint32_t fw0, fw1;
	uint16_t cmdif;

	fw0 = mcx_rd(sc, MCX_FW_VER);
	fw1 = mcx_rd(sc, MCX_CMDIF_FW_SUBVER);

	printf(": FW %u.%u.%04u", MCX_FW_VER_MAJOR(fw0),
	    MCX_FW_VER_MINOR(fw0), MCX_FW_VER_SUBMINOR(fw1));

	cmdif = MCX_CMDIF(fw1);
	if (cmdif != MCX_CMD_IF_SUPPORTED) {
		printf(", unsupported command interface %u\n", cmdif);
		return (-1);
	}

	return (0);
}

static int
mcx_init_wait(struct mcx_softc *sc)
{
	unsigned int i;
	uint32_t r;

	for (i = 0; i < 2000; i++) {
		r = mcx_rd(sc, MCX_STATE);
		if ((r & MCX_STATE_MASK) == MCX_STATE_READY)
			return (0);

		delay(1000);
		mcx_bar(sc, MCX_STATE, sizeof(uint32_t),
		    BUS_SPACE_BARRIER_READ);
	}

	return (-1);
}

static uint8_t
mcx_cmdq_poll(struct mcx_softc *sc, struct mcx_cmdq_entry *cqe,
    unsigned int msec)
{
	unsigned int i;

	for (i = 0; i < msec; i++) {
		bus_dmamap_sync(sc->sc_dmat, MCX_DMA_MAP(&sc->sc_cmdq_mem),
		    0, MCX_DMA_LEN(&sc->sc_cmdq_mem), BUS_DMASYNC_POSTRW);

		if ((cqe->cq_status & MCX_CQ_STATUS_OWN_MASK) ==
		    MCX_CQ_STATUS_OWN_SW)
			return (0);

		delay(1000);
	}

	return (ETIMEDOUT);
}

static uint32_t
mcx_mix_u64(uint32_t xor, uint64_t u64)
{
	xor ^= u64 >> 32;
	xor ^= u64;

	return (xor);
}

static uint32_t
mcx_mix_u32(uint32_t xor, uint32_t u32)
{
	xor ^= u32;

	return (xor);
}

static uint32_t
mcx_mix_u8(uint32_t xor, uint8_t u8)
{
	xor ^= u8;

	return (xor);
}

static uint8_t
mcx_mix_done(uint32_t xor)
{
	xor ^= xor >> 16;
	xor ^= xor >> 8;

	return (xor);
}

static uint8_t
mcx_xor(const void *buf, size_t len)
{
	const uint32_t *dwords = buf;
	uint32_t xor = 0xff;
	size_t i;

	len /= sizeof(*dwords);

	for (i = 0; i < len; i++)
		xor ^= dwords[i];

	return (mcx_mix_done(xor));
}

static uint8_t
mcx_cmdq_token(struct mcx_softc *sc)
{
	uint8_t token;

	do {
		token = ++sc->sc_cmdq_token;
	} while (token == 0);

	return (token);
}

static void
mcx_cmdq_init(struct mcx_softc *sc, struct mcx_cmdq_entry *cqe,
    uint32_t ilen, uint32_t olen, uint8_t token)
{
	memset(cqe, 0, sc->sc_cmdq_size);

	cqe->cq_type = MCX_CMDQ_TYPE_PCIE;
	htobem32(&cqe->cq_input_length, ilen);
	htobem32(&cqe->cq_output_length, olen);
	cqe->cq_token = token;
	cqe->cq_status = MCX_CQ_STATUS_OWN_HW;
}

static void
mcx_cmdq_sign(struct mcx_cmdq_entry *cqe)
{
	cqe->cq_signature = ~mcx_xor(cqe, sizeof(*cqe));
}

static int
mcx_cmdq_verify(const struct mcx_cmdq_entry *cqe)
{
	/* return (mcx_xor(cqe, sizeof(*cqe)) ? -1 :  0); */
	return (0);
}

static void *
mcx_cmdq_in(struct mcx_cmdq_entry *cqe)
{
	return (&cqe->cq_input_data);
}

static void *
mcx_cmdq_out(struct mcx_cmdq_entry *cqe)
{
	return (&cqe->cq_output_data);
}

static void
mcx_cmdq_post(struct mcx_softc *sc, struct mcx_cmdq_entry *cqe,
    unsigned int slot)
{
	mcx_cmdq_sign(cqe);

	bus_dmamap_sync(sc->sc_dmat, MCX_DMA_MAP(&sc->sc_cmdq_mem),
	    0, MCX_DMA_LEN(&sc->sc_cmdq_mem), BUS_DMASYNC_PRERW);

	mcx_wr(sc, MCX_CMDQ_DOORBELL, 1U << slot);
}

static int
mcx_enable_hca(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_enable_hca_in *in;
	struct mcx_cmd_enable_hca_out *out;
	int error;
	uint8_t status;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ENABLE_HCA);
	in->cmd_op_mod = htobe16(0);
	in->cmd_function_id = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", hca enable timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", hca enable command corrupt\n");
		return (-1);
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", hca enable failed (%x)\n", status);
		return (-1);
	}

	return (0);
}

static int
mcx_teardown_hca(struct mcx_softc *sc, uint16_t profile)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_teardown_hca_in *in;
	struct mcx_cmd_teardown_hca_out *out;
	int error;
	uint8_t status;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_TEARDOWN_HCA);
	in->cmd_op_mod = htobe16(0);
	in->cmd_profile = profile;

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", hca teardown timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", hca teardown command corrupt\n");
		return (-1);
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", hca teardown failed (%x)\n", status);
		return (-1);
	}

	return (0);
}

static int
mcx_cmdq_mboxes_alloc(struct mcx_softc *sc, struct mcx_dmamem *mxm,
    unsigned int nmb, uint64_t *ptr, uint8_t token)
{
	caddr_t kva;
	uint64_t dva;
	int i;
	int error;

	error = mcx_dmamem_alloc(sc, mxm,
	    nmb * MCX_CMDQ_MAILBOX_SIZE, MCX_CMDQ_MAILBOX_ALIGN);
	if (error != 0)
		return (error);

	mcx_dmamem_zero(mxm);

	dva = MCX_DMA_DVA(mxm);
	kva = MCX_DMA_KVA(mxm);
	for (i = 0; i < nmb; i++) {
		struct mcx_cmdq_mailbox *mbox = (struct mcx_cmdq_mailbox *)kva;

		/* patch the cqe or mbox pointing at this one */
		htobem64(ptr, dva);

		/* fill in this mbox */
		htobem32(&mbox->mb_block_number, i);
		mbox->mb_token = token;

		/* move to the next one */
		ptr = &mbox->mb_next_ptr;

		dva += MCX_CMDQ_MAILBOX_SIZE;
		kva += MCX_CMDQ_MAILBOX_SIZE;
	}

	return (0);
}

static uint32_t
mcx_cmdq_mbox_ctrl_sig(const struct mcx_cmdq_mailbox *mb)
{
	uint32_t xor = 0xff;

	/* only 3 fields get set, so mix them directly */
	xor = mcx_mix_u64(xor, mb->mb_next_ptr);
	xor = mcx_mix_u32(xor, mb->mb_block_number);
	xor = mcx_mix_u8(xor, mb->mb_token);

	return (mcx_mix_done(xor));
}

static void
mcx_cmdq_mboxes_sign(struct mcx_dmamem *mxm, unsigned int nmb)
{
	caddr_t kva;
	int i;

	kva = MCX_DMA_KVA(mxm);

	for (i = 0; i < nmb; i++) {
		struct mcx_cmdq_mailbox *mb = (struct mcx_cmdq_mailbox *)kva;
		uint8_t sig = mcx_cmdq_mbox_ctrl_sig(mb);
		mb->mb_ctrl_signature = sig;
		mb->mb_signature = sig ^
		    mcx_xor(mb->mb_data, sizeof(mb->mb_data));

		kva += MCX_CMDQ_MAILBOX_SIZE;
	}
}

static void
mcx_cmdq_mboxes_sync(struct mcx_softc *sc, struct mcx_dmamem *mxm, int ops)
{
	bus_dmamap_sync(sc->sc_dmat, MCX_DMA_MAP(mxm),
	    0, MCX_DMA_LEN(mxm), ops);
}

static struct mcx_cmdq_mailbox *
mcx_cq_mbox(struct mcx_dmamem *mxm, unsigned int i)
{
	caddr_t kva;

	kva = MCX_DMA_KVA(mxm);
	kva += i * MCX_CMDQ_MAILBOX_SIZE;

	return ((struct mcx_cmdq_mailbox *)kva);
}

static inline void *
mcx_cq_mbox_data(struct mcx_cmdq_mailbox *mb)
{
	return (&mb->mb_data);
}

static void
mcx_cmdq_mboxes_copyin(struct mcx_dmamem *mxm, unsigned int nmb,
    void *b, size_t len)
{
	caddr_t buf = b;
	struct mcx_cmdq_mailbox *mb;
	int i;

	mb = (struct mcx_cmdq_mailbox *)MCX_DMA_KVA(mxm);
	for (i = 0; i < nmb; i++) {

		memcpy(mb->mb_data, buf, min(sizeof(mb->mb_data), len));

		if (sizeof(mb->mb_data) >= len)
			break;

		buf += sizeof(mb->mb_data);
		len -= sizeof(mb->mb_data);
		mb++;
	}
}

static void
mcx_cmdq_mboxes_pas(struct mcx_dmamem *mxm, int offset, int npages,
    struct mcx_dmamem *buf)
{
	uint64_t *pas;
	int mbox, mbox_pages, i;

	mbox = offset / MCX_CMDQ_MAILBOX_DATASIZE;
	offset %= MCX_CMDQ_MAILBOX_DATASIZE;

	pas = mcx_cq_mbox_data(mcx_cq_mbox(mxm, mbox));
	pas += (offset / sizeof(*pas));
	mbox_pages = (MCX_CMDQ_MAILBOX_DATASIZE - offset) / sizeof(*pas);
	for (i = 0; i < npages; i++) {
		if (i == mbox_pages) {
			mbox++;
			pas = mcx_cq_mbox_data(mcx_cq_mbox(mxm, mbox));
			mbox_pages += MCX_CMDQ_MAILBOX_DATASIZE / sizeof(*pas);
		}
		*pas = htobe64(MCX_DMA_DVA(buf) + (i * MCX_PAGE_SIZE));
		pas++;
	}
}

static void
mcx_cmdq_mboxes_copyout(struct mcx_dmamem *mxm, int nmb, void *b, size_t len)
{
	caddr_t buf = b;
	struct mcx_cmdq_mailbox *mb;
	int i;

	mb = (struct mcx_cmdq_mailbox *)MCX_DMA_KVA(mxm);
	for (i = 0; i < nmb; i++) {
		memcpy(buf, mb->mb_data, min(sizeof(mb->mb_data), len));

		if (sizeof(mb->mb_data) >= len)
			break;

		buf += sizeof(mb->mb_data);
		len -= sizeof(mb->mb_data);
		mb++;
	}
}

static void
mcx_cq_mboxes_free(struct mcx_softc *sc, struct mcx_dmamem *mxm)
{
	mcx_dmamem_free(sc, mxm);
}

#if 0
static void
mcx_cmdq_dump(const struct mcx_cmdq_entry *cqe)
{
	unsigned int i;

	printf(" type %02x, ilen %u, iptr %016llx", cqe->cq_type,
	    bemtoh32(&cqe->cq_input_length), bemtoh64(&cqe->cq_input_ptr));

	printf(", idata ");
	for (i = 0; i < sizeof(cqe->cq_input_data); i++)
		printf("%02x", cqe->cq_input_data[i]);

	printf(", odata ");
	for (i = 0; i < sizeof(cqe->cq_output_data); i++)
		printf("%02x", cqe->cq_output_data[i]);

	printf(", optr %016llx, olen %u, token %02x, sig %02x, status %02x",
	    bemtoh64(&cqe->cq_output_ptr), bemtoh32(&cqe->cq_output_length),
	    cqe->cq_token, cqe->cq_signature, cqe->cq_status);
}

static void
mcx_cmdq_mbox_dump(struct mcx_dmamem *mboxes, int num)
{
	int i, j;
	uint8_t *d;

	for (i = 0; i < num; i++) {
		struct mcx_cmdq_mailbox *mbox;
		mbox = mcx_cq_mbox(mboxes, i);

		d = mcx_cq_mbox_data(mbox);
		for (j = 0; j < MCX_CMDQ_MAILBOX_DATASIZE; j++) {
			if (j != 0 && (j % 16 == 0))
				printf("\n");
			printf("%.2x ", d[j]);
		}
	}
}
#endif

static int
mcx_access_hca_reg(struct mcx_softc *sc, uint16_t reg, int op, void *data,
    int len)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_access_reg_in *in;
	struct mcx_cmd_access_reg_out *out;
	uint8_t token = mcx_cmdq_token(sc);
	int error, nmb;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + len, sizeof(*out) + len,
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ACCESS_REG);
	in->cmd_op_mod = htobe16(op);
	in->cmd_register_id = htobe16(reg);

	nmb = howmany(len, MCX_CMDQ_MAILBOX_DATASIZE);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, nmb, &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate access reg mailboxen\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;
	mcx_cmdq_mboxes_copyin(&mxm, nmb, data, len);
	mcx_cmdq_mboxes_sign(&mxm, nmb);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_PRERW);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_POSTRW);

	if (error != 0) {
		printf("%s: access reg (%s %x) timeout\n", DEVNAME(sc),
		    (op == MCX_REG_OP_WRITE ? "write" : "read"), reg);
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: access reg (%s %x) reply corrupt\n",
		    (op == MCX_REG_OP_WRITE ? "write" : "read"), DEVNAME(sc),
		    reg);
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: access reg (%s %x) failed (%x, %.6x)\n",
		    DEVNAME(sc), (op == MCX_REG_OP_WRITE ? "write" : "read"),
		    reg, out->cmd_status, out->cmd_syndrome);
		error = -1;
		goto free;
	}

	mcx_cmdq_mboxes_copyout(&mxm, nmb, data, len);
free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_set_issi(struct mcx_softc *sc, struct mcx_cmdq_entry *cqe, unsigned int slot)
{
	struct mcx_cmd_set_issi_in *in;
	struct mcx_cmd_set_issi_out *out;
	uint8_t status;

	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_SET_ISSI);
	in->cmd_op_mod = htobe16(0);
	in->cmd_current_issi = htobe16(MCX_ISSI);

	mcx_cmdq_post(sc, cqe, slot);
	if (mcx_cmdq_poll(sc, cqe, 1000) != 0)
		return (-1);
	if (mcx_cmdq_verify(cqe) != 0)
		return (-1);

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK)
		return (-1);

	return (0);
}

static int
mcx_issi(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_issi_in *in;
	struct mcx_cmd_query_issi_il_out *out;
	struct mcx_cmd_query_issi_mb_out *mb;
	uint8_t token = mcx_cmdq_token(sc);
	uint8_t status;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + sizeof(*mb), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_ISSI);
	in->cmd_op_mod = htobe16(0);

	CTASSERT(sizeof(*mb) <= MCX_CMDQ_MAILBOX_DATASIZE);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query issi mailbox\n");
		return (-1);
	}
	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", query issi timeout\n");
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf(", query issi reply corrupt\n");
		goto free;
	}

	status = cqe->cq_output_data[0];
	switch (status) {
	case MCX_CQ_STATUS_OK:
		break;
	case MCX_CQ_STATUS_BAD_OPCODE:
		/* use ISSI 0 */
		goto free;
	default:
		printf(", query issi failed (%x)\n", status);
		error = -1;
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_current_issi == htobe16(MCX_ISSI)) {
		/* use ISSI 1 */
		goto free;
	}

	/* don't need to read cqe anymore, can be used for SET ISSI */

	mb = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	CTASSERT(MCX_ISSI < NBBY);
	 /* XXX math is hard */
	if (!ISSET(mb->cmd_supported_issi[79], 1 << MCX_ISSI)) {
		/* use ISSI 0 */
		goto free;
	}

	if (mcx_set_issi(sc, cqe, 0) != 0) {
		/* ignore the error, just use ISSI 0 */
	} else {
		/* use ISSI 1 */
	}

free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}

static int
mcx_query_pages(struct mcx_softc *sc, uint16_t type,
    uint32_t *npages, uint16_t *func_id)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_pages_in *in;
	struct mcx_cmd_query_pages_out *out;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_PAGES);
	in->cmd_op_mod = type;

	mcx_cmdq_post(sc, cqe, 0);
	if (mcx_cmdq_poll(sc, cqe, 1000) != 0) {
		printf(", query pages timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", query pages reply corrupt\n");
		return (-1);
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", query pages failed (%x)\n", out->cmd_status);
		return (-1);
	}

	*func_id = out->cmd_func_id;
	*npages = bemtoh32(&out->cmd_num_pages);

	return (0);
}

struct bus_dma_iter {
	bus_dmamap_t		i_map;
	bus_size_t		i_offset;
	unsigned int		i_index;
};

static void
bus_dma_iter_init(struct bus_dma_iter *i, bus_dmamap_t map)
{
	i->i_map = map;
	i->i_offset = 0;
	i->i_index = 0;
}

static bus_addr_t
bus_dma_iter_addr(struct bus_dma_iter *i)
{
	return (i->i_map->dm_segs[i->i_index].ds_addr + i->i_offset);
}

static void
bus_dma_iter_add(struct bus_dma_iter *i, bus_size_t size)
{
	bus_dma_segment_t *seg = i->i_map->dm_segs + i->i_index;
	bus_size_t diff;

	do {
		diff = seg->ds_len - i->i_offset;
		if (size < diff)
			break;

		size -= diff;

		seg++;

		i->i_offset = 0;
		i->i_index++;
	} while (size > 0);

	i->i_offset += size;
}

static int
mcx_add_pages(struct mcx_softc *sc, struct mcx_hwmem *mhm, uint16_t func_id)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_manage_pages_in *in;
	struct mcx_cmd_manage_pages_out *out;
	unsigned int paslen, nmb, i, j, npages;
	struct bus_dma_iter iter;
	uint64_t *pas;
	uint8_t status;
	uint8_t token = mcx_cmdq_token(sc);
	int error;

	npages = mhm->mhm_npages;

	paslen = sizeof(*pas) * npages;
	nmb = howmany(paslen, MCX_CMDQ_MAILBOX_DATASIZE);

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + paslen, sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_MANAGE_PAGES);
	in->cmd_op_mod = htobe16(MCX_CMD_MANAGE_PAGES_ALLOC_SUCCESS);
	in->cmd_func_id = func_id;
	htobem32(&in->cmd_input_num_entries, npages);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, nmb,
	    &cqe->cq_input_ptr, token) != 0) {
		printf(", unable to allocate manage pages mailboxen\n");
		return (-1);
	}

	bus_dma_iter_init(&iter, mhm->mhm_map);
	for (i = 0; i < nmb; i++) {
		unsigned int lim;

		pas = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, i));
		lim = min(MCX_CMDQ_MAILBOX_DATASIZE / sizeof(*pas), npages);

		for (j = 0; j < lim; j++) {
			htobem64(&pas[j], bus_dma_iter_addr(&iter));
			bus_dma_iter_add(&iter, MCX_PAGE_SIZE);
		}

		npages -= lim;
	}

	mcx_cmdq_mboxes_sign(&mxm, nmb);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", manage pages timeout\n");
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf(", manage pages reply corrupt\n");
		goto free;
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", manage pages failed (%x)\n", status);
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_pages(struct mcx_softc *sc, struct mcx_hwmem *mhm, uint16_t type)
{
	uint32_t npages;
	uint16_t func_id;

	if (mcx_query_pages(sc, type, &npages, &func_id) != 0) {
		/* error printed by mcx_query_pages */
		return (-1);
	}

	if (npages == 0)
		return (0);

	if (mcx_hwmem_alloc(sc, mhm, npages) != 0) {
		printf(", unable to allocate hwmem\n");
		return (-1);
	}

	if (mcx_add_pages(sc, mhm, func_id) != 0) {
		printf(", unable to add hwmem\n");
		goto free;
	}

	return (0);

free:
	mcx_hwmem_free(sc, mhm);

	return (-1);
}

static int
mcx_hca_max_caps(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_hca_cap_in *in;
	struct mcx_cmd_query_hca_cap_out *out;
	struct mcx_cmdq_mailbox *mb;
	struct mcx_cap_device *hca;
	uint8_t status;
	uint8_t token = mcx_cmdq_token(sc);
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + MCX_HCA_CAP_LEN,
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_HCA_CAP);
	in->cmd_op_mod = htobe16(MCX_CMD_QUERY_HCA_CAP_MAX |
	    MCX_CMD_QUERY_HCA_CAP_DEVICE);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, MCX_HCA_CAP_NMAILBOXES,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query hca caps mailboxen\n");
		return (-1);
	}
	mcx_cmdq_mboxes_sign(&mxm, MCX_HCA_CAP_NMAILBOXES);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_PRERW);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_POSTRW);

	if (error != 0) {
		printf(", query hca caps timeout\n");
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf(", query hca caps reply corrupt\n");
		goto free;
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", query hca caps failed (%x)\n", status);
		error = -1;
		goto free;
	}

	mb = mcx_cq_mbox(&mxm, 0);
	hca = mcx_cq_mbox_data(mb);

	if (hca->log_pg_sz > PAGE_SHIFT) {
		printf(", minimum system page shift %u is too large\n",
		    hca->log_pg_sz);
		error = -1;
		goto free;
	}
	/*
	 * blueflame register is split into two buffers, and we must alternate
	 * between the two of them.
	 */
	sc->sc_bf_size = (1 << hca->log_bf_reg_size) / 2;

free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_hca_set_caps(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_hca_cap_in *in;
	struct mcx_cmd_query_hca_cap_out *out;
	struct mcx_cmdq_mailbox *mb;
	struct mcx_cap_device *hca;
	uint8_t status;
	uint8_t token = mcx_cmdq_token(sc);
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + MCX_HCA_CAP_LEN,
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_HCA_CAP);
	in->cmd_op_mod = htobe16(MCX_CMD_QUERY_HCA_CAP_CURRENT |
	    MCX_CMD_QUERY_HCA_CAP_DEVICE);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, MCX_HCA_CAP_NMAILBOXES,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate manage pages mailboxen\n");
		return (-1);
	}
	mcx_cmdq_mboxes_sign(&mxm, MCX_HCA_CAP_NMAILBOXES);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_PRERW);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	mcx_cmdq_mboxes_sync(sc, &mxm, BUS_DMASYNC_POSTRW);

	if (error != 0) {
		printf(", query hca caps timeout\n");
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf(", query hca caps reply corrupt\n");
		goto free;
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", query hca caps failed (%x)\n", status);
		error = -1;
		goto free;
	}

	mb = mcx_cq_mbox(&mxm, 0);
	hca = mcx_cq_mbox_data(mb);

	hca->log_pg_sz = PAGE_SHIFT;

free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}


static int
mcx_init_hca(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_init_hca_in *in;
	struct mcx_cmd_init_hca_out *out;
	int error;
	uint8_t status;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_INIT_HCA);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", hca init timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", hca init command corrupt\n");
		return (-1);
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", hca init failed (%x)\n", status);
		return (-1);
	}

	return (0);
}

static int
mcx_set_driver_version(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_set_driver_version_in *in;
	struct mcx_cmd_set_driver_version_out *out;
	int error;
	int token;
	uint8_t status;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) +
	    sizeof(struct mcx_cmd_set_driver_version), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_SET_DRIVER_VERSION);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1,
	    &cqe->cq_input_ptr, token) != 0) {
		printf(", unable to allocate set driver version mailboxen\n");
		return (-1);
	}
	strlcpy(mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)),
	    "OpenBSD,mcx,1.000.000000", MCX_CMDQ_MAILBOX_DATASIZE);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", set driver version timeout\n");
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", set driver version command corrupt\n");
		goto free;
	}

	status = cqe->cq_output_data[0];
	if (status != MCX_CQ_STATUS_OK) {
		printf(", set driver version failed (%x)\n", status);
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_iff(struct mcx_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_modify_nic_vport_context_in *in;
	struct mcx_cmd_modify_nic_vport_context_out *out;
	struct mcx_nic_vport_ctx *ctx;
	int error;
	int token;
	int insize;

	/* enable or disable the promisc flow */
	if (ISSET(ifp->if_flags, IFF_PROMISC)) {
		if (sc->sc_promisc_flow_enabled == 0) {
			mcx_set_flow_table_entry(sc, MCX_FLOW_GROUP_PROMISC,
			    0, NULL);
			sc->sc_promisc_flow_enabled = 1;
		}
	} else if (sc->sc_promisc_flow_enabled != 0) {
		mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_PROMISC, 0);
		sc->sc_promisc_flow_enabled = 0;
	}

	/* enable or disable the all-multicast flow */
	if (ISSET(ifp->if_flags, IFF_ALLMULTI)) {
		if (sc->sc_allmulti_flow_enabled == 0) {
			uint8_t mcast[ETHER_ADDR_LEN];

			memset(mcast, 0, sizeof(mcast));
			mcast[0] = 0x01;
			mcx_set_flow_table_entry(sc, MCX_FLOW_GROUP_ALLMULTI,
			    0, mcast);
			sc->sc_allmulti_flow_enabled = 1;
		}
	} else if (sc->sc_allmulti_flow_enabled != 0) {
		mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_ALLMULTI, 0);
		sc->sc_allmulti_flow_enabled = 0;
	}

	insize = sizeof(struct mcx_nic_vport_ctx) + 240;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + insize, sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_MODIFY_NIC_VPORT_CONTEXT);
	in->cmd_op_mod = htobe16(0);
	in->cmd_field_select = htobe32(
	    MCX_CMD_MODIFY_NIC_VPORT_CONTEXT_FIELD_PROMISC |
	    MCX_CMD_MODIFY_NIC_VPORT_CONTEXT_FIELD_MTU);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf(", unable to allocate modify nic vport context mailboxen\n");
		return (-1);
	}
	ctx = (struct mcx_nic_vport_ctx *)
	    (((char *)mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0))) + 240);
	ctx->vp_mtu = htobe32(sc->sc_hardmtu);
	/*
	 * always leave promisc-all enabled on the vport since we can't give it
	 * a vlan list, and we're already doing multicast filtering in the flow
	 * table.
	 */
	ctx->vp_flags = htobe16(MCX_NIC_VPORT_CTX_PROMISC_ALL);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", modify nic vport context timeout\n");
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", modify nic vport context command corrupt\n");
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", modify nic vport context failed (%x, %x)\n",
		    out->cmd_status, out->cmd_syndrome);
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_alloc_uar(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_alloc_uar_in *in;
	struct mcx_cmd_alloc_uar_out *out;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ALLOC_UAR);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", alloc uar timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", alloc uar command corrupt\n");
		return (-1);
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", alloc uar failed (%x)\n", out->cmd_status);
		return (-1);
	}

	sc->sc_uar = mcx_get_id(out->cmd_uar);

	return (0);
}

static int
mcx_create_eq(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_eq_in *in;
	struct mcx_cmd_create_eq_mb_in *mbin;
	struct mcx_cmd_create_eq_out *out;
	struct mcx_eq_entry *eqe;
	int error;
	uint64_t *pas;
	int insize, npages, paslen, i, token;

	sc->sc_eq_cons = 0;

	npages = howmany((1 << MCX_LOG_EQ_SIZE) * sizeof(struct mcx_eq_entry),
	    MCX_PAGE_SIZE);
	paslen = npages * sizeof(*pas);
	insize = sizeof(struct mcx_cmd_create_eq_mb_in) + paslen;

	if (mcx_dmamem_alloc(sc, &sc->sc_eq_mem, npages * MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf(", unable to allocate event queue memory\n");
		return (-1);
	}

	eqe = (struct mcx_eq_entry *)MCX_DMA_KVA(&sc->sc_eq_mem);
	for (i = 0; i < (1 << MCX_LOG_EQ_SIZE); i++) {
		eqe[i].eq_owner = MCX_EQ_ENTRY_OWNER_INIT;
	}

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + insize, sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_EQ);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, howmany(insize, MCX_CMDQ_MAILBOX_DATASIZE),
	    &cqe->cq_input_ptr, token) != 0) {
		printf(", unable to allocate create eq mailboxen\n");
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_eq_ctx.eq_uar_size = htobe32(
	    (MCX_LOG_EQ_SIZE << MCX_EQ_CTX_LOG_EQ_SIZE_SHIFT) | sc->sc_uar);
	mbin->cmd_event_bitmask = htobe64(
	    (1ull << MCX_EVENT_TYPE_INTERNAL_ERROR) |
	    (1ull << MCX_EVENT_TYPE_PORT_CHANGE) |
	    (1ull << MCX_EVENT_TYPE_CMD_COMPLETION) |
	    (1ull << MCX_EVENT_TYPE_PAGE_REQUEST));

	/* physical addresses follow the mailbox in data */
	mcx_cmdq_mboxes_pas(&mxm, sizeof(*mbin), npages, &sc->sc_eq_mem);
	mcx_cmdq_mboxes_sign(&mxm, howmany(insize, MCX_CMDQ_MAILBOX_DATASIZE));
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", create eq timeout\n");
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", create eq command corrupt\n");
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", create eq failed (%x, %x)\n", out->cmd_status,
		    betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_eqn = mcx_get_id(out->cmd_eqn);
	mcx_arm_eq(sc);
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_alloc_pd(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_alloc_pd_in *in;
	struct mcx_cmd_alloc_pd_out *out;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ALLOC_PD);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", alloc pd timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", alloc pd command corrupt\n");
		return (-1);
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", alloc pd failed (%x)\n", out->cmd_status);
		return (-1);
	}

	sc->sc_pd = mcx_get_id(out->cmd_pd);
	return (0);
}

static int
mcx_alloc_tdomain(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_alloc_td_in *in;
	struct mcx_cmd_alloc_td_out *out;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ALLOC_TRANSPORT_DOMAIN);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", alloc transport domain timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", alloc transport domain command corrupt\n");
		return (-1);
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", alloc transport domain failed (%x)\n",
		    out->cmd_status);
		return (-1);
	}

	sc->sc_tdomain = mcx_get_id(out->cmd_tdomain);
	return (0);
}

static int
mcx_query_nic_vport_context(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_nic_vport_context_in *in;
	struct mcx_cmd_query_nic_vport_context_out *out;
	struct mcx_nic_vport_ctx *ctx;
	uint8_t *addr;
	int error, token, i;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + sizeof(*ctx), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_NIC_VPORT_CONTEXT);
	in->cmd_op_mod = htobe16(0);
	in->cmd_allowed_list_type = 0;

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query nic vport context mailboxen\n");
		return (-1);
	}
	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", query nic vport context timeout\n");
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", query nic vport context command corrupt\n");
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", query nic vport context failed (%x, %x)\n",
		    out->cmd_status, out->cmd_syndrome);
		error = -1;
		goto free;
	}

	ctx = (struct mcx_nic_vport_ctx *)(mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	addr = (uint8_t *)&ctx->vp_perm_addr;
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sc->sc_ac.ac_enaddr[i] = addr[i + 2];
	}
free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_query_special_contexts(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_special_ctx_in *in;
	struct mcx_cmd_query_special_ctx_out *out;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_SPECIAL_CONTEXTS);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf(", query special contexts timeout\n");
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf(", query special contexts command corrupt\n");
		return (-1);
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf(", query special contexts failed (%x)\n",
		    out->cmd_status);
		return (-1);
	}

	sc->sc_lkey = betoh32(out->cmd_resd_lkey);
	return (0);
}

static int
mcx_set_port_mtu(struct mcx_softc *sc, int mtu)
{
	struct mcx_reg_pmtu pmtu;
	int error;

	/* read max mtu */
	memset(&pmtu, 0, sizeof(pmtu));
	pmtu.rp_local_port = 1;
	error = mcx_access_hca_reg(sc, MCX_REG_PMTU, MCX_REG_OP_READ, &pmtu,
	    sizeof(pmtu));
	if (error != 0) {
		printf(", unable to get port MTU\n");
		return error;
	}

	mtu = min(mtu, betoh16(pmtu.rp_max_mtu));
	pmtu.rp_admin_mtu = htobe16(mtu);
	error = mcx_access_hca_reg(sc, MCX_REG_PMTU, MCX_REG_OP_WRITE, &pmtu,
	    sizeof(pmtu));
	if (error != 0) {
		printf(", unable to set port MTU\n");
		return error;
	}

	sc->sc_hardmtu = mtu;
	return 0;
}

static int
mcx_create_cq(struct mcx_softc *sc, int eqn)
{
	struct mcx_cmdq_entry *cmde;
	struct mcx_cq_entry *cqe;
	struct mcx_cq *cq;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_cq_in *in;
	struct mcx_cmd_create_cq_mb_in *mbin;
	struct mcx_cmd_create_cq_out *out;
	int error;
	uint64_t *pas;
	int insize, npages, paslen, i, token;

	if (sc->sc_num_cq >= MCX_MAX_CQS) {
		printf("%s: tried to create too many cqs\n", DEVNAME(sc));
		return (-1);
	}
	cq = &sc->sc_cq[sc->sc_num_cq];

	npages = howmany((1 << MCX_LOG_CQ_SIZE) * sizeof(struct mcx_cq_entry),
	    MCX_PAGE_SIZE);
	paslen = npages * sizeof(*pas);
	insize = sizeof(struct mcx_cmd_create_cq_mb_in) + paslen;

	if (mcx_dmamem_alloc(sc, &cq->cq_mem, npages * MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf("%s: unable to allocate completion queue memory\n",
		    DEVNAME(sc));
		return (-1);
	}
	cqe = MCX_DMA_KVA(&cq->cq_mem);
	for (i = 0; i < (1 << MCX_LOG_CQ_SIZE); i++) {
		cqe[i].cq_opcode_owner = MCX_CQ_ENTRY_FLAG_OWNER;
	}

	cmde = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cmde, sizeof(*in) + insize, sizeof(*out), token);

	in = mcx_cmdq_in(cmde);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_CQ);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, howmany(insize, MCX_CMDQ_MAILBOX_DATASIZE),
	    &cmde->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create cq mailboxen\n", DEVNAME(sc));
		error = -1;
		goto free;
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_cq_ctx.cq_uar_size = htobe32(
	    (MCX_LOG_CQ_SIZE << MCX_CQ_CTX_LOG_CQ_SIZE_SHIFT) | sc->sc_uar);
	mbin->cmd_cq_ctx.cq_eqn = htobe32(eqn);
	mbin->cmd_cq_ctx.cq_period_max_count = htobe32(
	    (MCX_CQ_MOD_PERIOD << MCX_CQ_CTX_PERIOD_SHIFT) |
	    MCX_CQ_MOD_COUNTER);
	mbin->cmd_cq_ctx.cq_doorbell = htobe64(
	    MCX_DMA_DVA(&sc->sc_doorbell_mem) +
	    MCX_CQ_DOORBELL_OFFSET + (MCX_CQ_DOORBELL_SIZE * sc->sc_num_cq));

	/* physical addresses follow the mailbox in data */
	mcx_cmdq_mboxes_pas(&mxm, sizeof(*mbin), npages, &cq->cq_mem);
	mcx_cmdq_post(sc, cmde, 0);

	error = mcx_cmdq_poll(sc, cmde, 1000);
	if (error != 0) {
		printf("%s: create cq timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cmde) != 0) {
		printf("%s: create cq command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cmde);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create cq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	cq->cq_n = mcx_get_id(out->cmd_cqn);
	cq->cq_cons = 0;
	cq->cq_count = 0;
	cq->cq_doorbell = MCX_DMA_KVA(&sc->sc_doorbell_mem) +
	    MCX_CQ_DOORBELL_OFFSET + (MCX_CQ_DOORBELL_SIZE * sc->sc_num_cq);
	mcx_arm_cq(sc, cq);
	sc->sc_num_cq++;

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_cq(struct mcx_softc *sc, int index)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_destroy_cq_in *in;
	struct mcx_cmd_destroy_cq_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_CQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_cqn = htobe32(sc->sc_cq[index].cq_n);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy cq timeout\n", DEVNAME(sc));
		return error;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy cq command corrupt\n", DEVNAME(sc));
		return error;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy cq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		return -1;
	}

	sc->sc_cq[index].cq_n = 0;
	mcx_dmamem_free(sc, &sc->sc_cq[index].cq_mem);
	sc->sc_cq[index].cq_cons = 0;
	sc->sc_cq[index].cq_count = 0;
	return 0;
}

static int
mcx_create_rq(struct mcx_softc *sc, int cqn)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_rq_in *in;
	struct mcx_cmd_create_rq_out *out;
	struct mcx_rq_ctx *mbin;
	int error;
	uint64_t *pas;
	uint8_t *doorbell;
	int insize, npages, paslen, token;

	npages = howmany((1 << MCX_LOG_RQ_SIZE) * sizeof(struct mcx_rq_entry),
	    MCX_PAGE_SIZE);
	paslen = npages * sizeof(*pas);
	insize = 0x10 + sizeof(struct mcx_rq_ctx) + paslen;

	if (mcx_dmamem_alloc(sc, &sc->sc_rq_mem, npages * MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf("%s: unable to allocate receive queue memory\n",
		    DEVNAME(sc));
		return (-1);
	}

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + insize, sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_RQ);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, howmany(insize, MCX_CMDQ_MAILBOX_DATASIZE),
	    &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create rq mailboxen\n",
		    DEVNAME(sc));
		error = -1;
		goto free;
	}
	mbin = (struct mcx_rq_ctx *)(((char *)mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0))) + 0x10);
	mbin->rq_flags = htobe32(MCX_RQ_CTX_RLKEY | MCX_RQ_CTX_VLAN_STRIP_DIS);
	mbin->rq_cqn = htobe32(cqn);
	mbin->rq_wq.wq_type = MCX_WQ_CTX_TYPE_CYCLIC;
	mbin->rq_wq.wq_pd = htobe32(sc->sc_pd);
	mbin->rq_wq.wq_doorbell = htobe64(MCX_DMA_DVA(&sc->sc_doorbell_mem) +
	    MCX_RQ_DOORBELL_OFFSET);
	mbin->rq_wq.wq_log_stride = htobe16(4);
	mbin->rq_wq.wq_log_size = MCX_LOG_RQ_SIZE;

	/* physical addresses follow the mailbox in data */
	mcx_cmdq_mboxes_pas(&mxm, sizeof(*mbin) + 0x10, npages, &sc->sc_rq_mem);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create rq timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create rq command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create rq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_rqn = mcx_get_id(out->cmd_rqn);

	doorbell = MCX_DMA_KVA(&sc->sc_doorbell_mem);
	sc->sc_rx_doorbell = (uint32_t *)(doorbell + MCX_RQ_DOORBELL_OFFSET);

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_ready_rq(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_modify_rq_in *in;
	struct mcx_cmd_modify_rq_mb_in *mbin;
	struct mcx_cmd_modify_rq_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_MODIFY_RQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_rq_state = htobe32((MCX_QUEUE_STATE_RST << 28) | sc->sc_rqn);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate modify rq mailbox\n", DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_rq_ctx.rq_flags = htobe32(
	    MCX_QUEUE_STATE_RDY << MCX_RQ_CTX_STATE_SHIFT);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: modify rq timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: modify rq command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: modify rq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_rq(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_destroy_rq_in *in;
	struct mcx_cmd_destroy_rq_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_RQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_rqn = htobe32(sc->sc_rqn);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy rq timeout\n", DEVNAME(sc));
		return error;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy rq command corrupt\n", DEVNAME(sc));
		return error;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy rq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		return -1;
	}

	sc->sc_rqn = 0;
	return 0;
}

static int
mcx_create_tir(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_tir_in *in;
	struct mcx_cmd_create_tir_mb_in *mbin;
	struct mcx_cmd_create_tir_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_TIR);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create tir mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	/* leave disp_type = 0, so packets get sent to the inline rqn */
	mbin->cmd_inline_rqn = htobe32(sc->sc_rqn);
	mbin->cmd_tdomain = htobe32(sc->sc_tdomain);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create tir timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create tir command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create tir failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_tirn = mcx_get_id(out->cmd_tirn);
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_tir(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_destroy_tir_in *in;
	struct mcx_cmd_destroy_tir_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_TIR);
	in->cmd_op_mod = htobe16(0);
	in->cmd_tirn = htobe32(sc->sc_tirn);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy tir timeout\n", DEVNAME(sc));
		return error;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy tir command corrupt\n", DEVNAME(sc));
		return error;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy tir failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		return -1;
	}

	sc->sc_tirn = 0;
	return 0;
}

static int
mcx_create_sq(struct mcx_softc *sc, int cqn)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_sq_in *in;
	struct mcx_sq_ctx *mbin;
	struct mcx_cmd_create_sq_out *out;
	int error;
	uint64_t *pas;
	uint8_t *doorbell;
	int insize, npages, paslen, token;

	npages = howmany((1 << MCX_LOG_SQ_SIZE) * sizeof(struct mcx_sq_entry),
	    MCX_PAGE_SIZE);
	paslen = npages * sizeof(*pas);
	insize = sizeof(struct mcx_sq_ctx) + paslen;

	if (mcx_dmamem_alloc(sc, &sc->sc_sq_mem, npages * MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE) != 0) {
		printf("%s: unable to allocate send queue memory\n", DEVNAME(sc));
		return (-1);
	}

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + insize + paslen, sizeof(*out),
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_SQ);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, howmany(insize, MCX_CMDQ_MAILBOX_DATASIZE),
	    &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create sq mailboxen\n", DEVNAME(sc));
		error = -1;
		goto free;
	}
	mbin = (struct mcx_sq_ctx *)(((char *)mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0))) + 0x10);
	mbin->sq_flags = htobe32(MCX_SQ_CTX_RLKEY |
	    (1 << MCX_SQ_CTX_MIN_WQE_INLINE_SHIFT));
	mbin->sq_cqn = htobe32(cqn);
	mbin->sq_tis_lst_sz = htobe32(1 << MCX_SQ_CTX_TIS_LST_SZ_SHIFT);
	mbin->sq_tis_num = htobe32(sc->sc_tisn);
	mbin->sq_wq.wq_type = MCX_WQ_CTX_TYPE_CYCLIC;
	mbin->sq_wq.wq_pd = htobe32(sc->sc_pd);
	mbin->sq_wq.wq_uar_page = htobe32(sc->sc_uar);
	mbin->sq_wq.wq_doorbell = htobe64(MCX_DMA_DVA(&sc->sc_doorbell_mem) +
	    MCX_SQ_DOORBELL_OFFSET);
	mbin->sq_wq.wq_log_stride = htobe16(MCX_LOG_SQ_ENTRY_SIZE);
	mbin->sq_wq.wq_log_size = MCX_LOG_SQ_SIZE;

	/* physical addresses follow the mailbox in data */
	mcx_cmdq_mboxes_pas(&mxm, sizeof(*mbin) + 0x10, npages, &sc->sc_sq_mem);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create sq timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create sq command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create sq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_sqn = mcx_get_id(out->cmd_sqn);

	doorbell = MCX_DMA_KVA(&sc->sc_doorbell_mem);
	sc->sc_tx_doorbell = (uint32_t *)(doorbell + MCX_SQ_DOORBELL_OFFSET + 4);
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_sq(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_destroy_sq_in *in;
	struct mcx_cmd_destroy_sq_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_SQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_sqn = htobe32(sc->sc_sqn);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy sq timeout\n", DEVNAME(sc));
		return error;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy sq command corrupt\n", DEVNAME(sc));
		return error;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy sq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		return -1;
	}

	sc->sc_sqn = 0;
	return 0;
}

static int
mcx_ready_sq(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_modify_sq_in *in;
	struct mcx_cmd_modify_sq_mb_in *mbin;
	struct mcx_cmd_modify_sq_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_MODIFY_SQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_sq_state = htobe32((MCX_QUEUE_STATE_RST << 28) | sc->sc_sqn);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate modify sq mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_sq_ctx.sq_flags = htobe32(
	    MCX_QUEUE_STATE_RDY << MCX_SQ_CTX_STATE_SHIFT);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: modify sq timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: modify sq command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: modify sq failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_create_tis(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_tis_in *in;
	struct mcx_cmd_create_tis_mb_in *mbin;
	struct mcx_cmd_create_tis_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_TIS);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create tis mailbox\n", DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_tdomain = htobe32(sc->sc_tdomain);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create tis timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create tis command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create tis failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_tisn = mcx_get_id(out->cmd_tisn);
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_tis(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_destroy_tis_in *in;
	struct mcx_cmd_destroy_tis_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_TIS);
	in->cmd_op_mod = htobe16(0);
	in->cmd_tisn = htobe32(sc->sc_tisn);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy tis timeout\n", DEVNAME(sc));
		return error;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy tis command corrupt\n", DEVNAME(sc));
		return error;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy tis failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		return -1;
	}

	sc->sc_tirn = 0;
	return 0;
}

#if 0
static int
mcx_alloc_flow_counter(struct mcx_softc *sc, int i)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_alloc_flow_counter_in *in;
	struct mcx_cmd_alloc_flow_counter_out *out;
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out), mcx_cmdq_token(sc));

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_ALLOC_FLOW_COUNTER);
	in->cmd_op_mod = htobe16(0);

	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: alloc flow counter timeout\n", DEVNAME(sc));
		return (-1);
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: alloc flow counter command corrupt\n", DEVNAME(sc));
		return (-1);
	}

	out = (struct mcx_cmd_alloc_flow_counter_out *)cqe->cq_output_data;
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: alloc flow counter failed (%x)\n", DEVNAME(sc),
		    out->cmd_status);
		return (-1);
	}

	sc->sc_flow_counter_id[i]  = betoh16(out->cmd_flow_counter_id);
	printf("flow counter id %d = %d\n", i, sc->sc_flow_counter_id[i]);

	return (0);
}
#endif

static int
mcx_create_flow_table(struct mcx_softc *sc, int log_size)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_flow_table_in *in;
	struct mcx_cmd_create_flow_table_mb_in *mbin;
	struct mcx_cmd_create_flow_table_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_FLOW_TABLE);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate create flow table mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mbin->cmd_ctx.ft_log_size = log_size;

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create flow table timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create flow table command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create flow table failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_flow_table_id = mcx_get_id(out->cmd_table_id);
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_set_flow_table_root(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_set_flow_table_root_in *in;
	struct mcx_cmd_set_flow_table_root_mb_in *mbin;
	struct mcx_cmd_set_flow_table_root_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_SET_FLOW_TABLE_ROOT);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate set flow table root mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: set flow table root timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: set flow table root command corrupt\n",
		    DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: set flow table root failed (%x, %x)\n",
		    DEVNAME(sc), out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_flow_table(struct mcx_softc *sc)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_destroy_flow_table_in *in;
	struct mcx_cmd_destroy_flow_table_mb_in *mb;
	struct mcx_cmd_destroy_flow_table_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mb), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_FLOW_TABLE);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate destroy flow table mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mb = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mb->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mb->cmd_table_id = htobe32(sc->sc_flow_table_id);

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy flow table timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy flow table command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy flow table failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_flow_table_id = -1;
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}


static int
mcx_create_flow_group(struct mcx_softc *sc, int group, int start, int size,
    int match_enable, struct mcx_flow_match *match)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_create_flow_group_in *in;
	struct mcx_cmd_create_flow_group_mb_in *mbin;
	struct mcx_cmd_create_flow_group_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out),
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_CREATE_FLOW_GROUP);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2, &cqe->cq_input_ptr, token)
	    != 0) {
		printf("%s: unable to allocate create flow group mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mbin->cmd_start_flow_index = htobe32(start);
	mbin->cmd_end_flow_index = htobe32(start + (size - 1));

	mbin->cmd_match_criteria_enable = match_enable;
	memcpy(&mbin->cmd_match_criteria, match, sizeof(*match));

	mcx_cmdq_mboxes_sign(&mxm, 2);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: create flow group timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: create flow group command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: create flow group failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_flow_group_id[group] = mcx_get_id(out->cmd_group_id);
	sc->sc_flow_group_size[group] = size;
	sc->sc_flow_group_start[group] = start;

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_destroy_flow_group(struct mcx_softc *sc, int group)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_destroy_flow_group_in *in;
	struct mcx_cmd_destroy_flow_group_mb_in *mb;
	struct mcx_cmd_destroy_flow_group_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mb), sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DESTROY_FLOW_GROUP);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate destroy flow group mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mb = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mb->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mb->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mb->cmd_group_id = htobe32(sc->sc_flow_group_id[group]);

	mcx_cmdq_mboxes_sign(&mxm, 2);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: destroy flow group timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: destroy flow group command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: destroy flow group failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

	sc->sc_flow_group_id[group] = -1;
	sc->sc_flow_group_size[group] = 0;
free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_set_flow_table_entry(struct mcx_softc *sc, int group, int index,
    uint8_t *macaddr)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_set_flow_table_entry_in *in;
	struct mcx_cmd_set_flow_table_entry_mb_in *mbin;
	struct mcx_cmd_set_flow_table_entry_out *out;
	uint32_t *dest;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin) + sizeof(*dest),
	    sizeof(*out), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_SET_FLOW_TABLE_ENTRY);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2, &cqe->cq_input_ptr, token)
	    != 0) {
		printf("%s: unable to allocate set flow table entry mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mbin->cmd_flow_index = htobe32(sc->sc_flow_group_start[group] + index);
	mbin->cmd_flow_ctx.fc_group_id = htobe32(sc->sc_flow_group_id[group]);

	/* flow context ends at offset 0x330, 0x130 into the second mbox */
	dest = (uint32_t *)
	    (((char *)mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 1))) + 0x130);
	mbin->cmd_flow_ctx.fc_action = htobe32(MCX_FLOW_CONTEXT_ACTION_FORWARD);
	mbin->cmd_flow_ctx.fc_dest_list_size = htobe32(1);
	*dest = htobe32(sc->sc_tirn | MCX_FLOW_CONTEXT_DEST_TYPE_TIR);

	/* the only thing we match on at the moment is the dest mac address */
	if (macaddr != NULL) {
		memcpy(mbin->cmd_flow_ctx.fc_match_value.mc_dest_mac, macaddr,
		    ETHER_ADDR_LEN);
	}

	mcx_cmdq_mboxes_sign(&mxm, 2);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: set flow table entry timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: set flow table entry command corrupt\n",
		    DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: set flow table entry failed (%x, %x)\n",
		    DEVNAME(sc), out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

static int
mcx_delete_flow_table_entry(struct mcx_softc *sc, int group, int index)
{
	struct mcx_cmdq_entry *cqe;
	struct mcx_dmamem mxm;
	struct mcx_cmd_delete_flow_table_entry_in *in;
	struct mcx_cmd_delete_flow_table_entry_mb_in *mbin;
	struct mcx_cmd_delete_flow_table_entry_out *out;
	int error;
	int token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out),
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_DELETE_FLOW_TABLE_ENTRY);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2, &cqe->cq_input_ptr, token) != 0) {
		printf("%s: unable to allocate delete flow table entry mailbox\n",
		    DEVNAME(sc));
		return (-1);
	}
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = MCX_FLOW_TABLE_TYPE_RX;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mbin->cmd_flow_index = htobe32(sc->sc_flow_group_start[group] + index);

	mcx_cmdq_mboxes_sign(&mxm, 2);
	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: delete flow table entry timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: delete flow table entry command corrupt\n",
		    DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: delete flow table entry %d:%d failed (%x, %x)\n",
		    DEVNAME(sc), group, index, out->cmd_status,
		    betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

free:
	mcx_dmamem_free(sc, &mxm);
	return (error);
}

#if 0
int
mcx_dump_flow_table(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_flow_table_in *in;
	struct mcx_cmd_query_flow_table_mb_in *mbin;
	struct mcx_cmd_query_flow_table_out *out;
	struct mcx_cmd_query_flow_table_mb_out *mbout;
	uint8_t token = mcx_cmdq_token(sc);
	int error;
	int i;
	uint8_t *dump;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin),
	    sizeof(*out) + sizeof(*mbout) + 16, token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_FLOW_TABLE);
	in->cmd_op_mod = htobe16(0);

	CTASSERT(sizeof(*mbin) <= MCX_CMDQ_MAILBOX_DATASIZE);
	CTASSERT(sizeof(*mbout) <= MCX_CMDQ_MAILBOX_DATASIZE);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query flow table mailboxes\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;

	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = 0;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);

	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query flow table timeout\n", DEVNAME(sc));
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: query flow table reply corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	switch (out->cmd_status) {
	case MCX_CQ_STATUS_OK:
		break;
	default:
		printf("%s: query flow table failed (%x/%x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

        mbout = (struct mcx_cmd_query_flow_table_mb_out *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	dump = (uint8_t *)mbout + 8;
	for (i = 0; i < sizeof(struct mcx_flow_table_ctx); i++) {
		printf("%.2x ", dump[i]);
		if (i % 16 == 15)
			printf("\n");
	}
free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}
int
mcx_dump_flow_table_entry(struct mcx_softc *sc, int index)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_flow_table_entry_in *in;
	struct mcx_cmd_query_flow_table_entry_mb_in *mbin;
	struct mcx_cmd_query_flow_table_entry_out *out;
	struct mcx_cmd_query_flow_table_entry_mb_out *mbout;
	uint8_t token = mcx_cmdq_token(sc);
	int error;
	int i;
	uint8_t *dump;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin),
	    sizeof(*out) + sizeof(*mbout) + 16, token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_FLOW_TABLE_ENTRY);
	in->cmd_op_mod = htobe16(0);

	CTASSERT(sizeof(*mbin) <= MCX_CMDQ_MAILBOX_DATASIZE);
	CTASSERT(sizeof(*mbout) <= MCX_CMDQ_MAILBOX_DATASIZE*2);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query flow table entry mailboxes\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;

	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = 0;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mbin->cmd_flow_index = htobe32(index);

	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query flow table entry timeout\n", DEVNAME(sc));
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: query flow table entry reply corrupt\n",
		    DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	switch (out->cmd_status) {
	case MCX_CQ_STATUS_OK:
		break;
	default:
		printf("%s: query flow table entry failed (%x/%x)\n",
		    DEVNAME(sc), out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

        mbout = (struct mcx_cmd_query_flow_table_entry_mb_out *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	dump = (uint8_t *)mbout;
	for (i = 0; i < MCX_CMDQ_MAILBOX_DATASIZE; i++) {
		printf("%.2x ", dump[i]);
		if (i % 16 == 15)
			printf("\n");
	}

free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}

int
mcx_dump_flow_group(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_flow_group_in *in;
	struct mcx_cmd_query_flow_group_mb_in *mbin;
	struct mcx_cmd_query_flow_group_out *out;
	struct mcx_cmd_query_flow_group_mb_out *mbout;
	uint8_t token = mcx_cmdq_token(sc);
	int error;
	int i;
	uint8_t *dump;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin),
	    sizeof(*out) + sizeof(*mbout) + 16, token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_FLOW_GROUP);
	in->cmd_op_mod = htobe16(0);

	CTASSERT(sizeof(*mbin) <= MCX_CMDQ_MAILBOX_DATASIZE);
	CTASSERT(sizeof(*mbout) <= MCX_CMDQ_MAILBOX_DATASIZE*2);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query flow group mailboxes\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;

	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_table_type = 0;
	mbin->cmd_table_id = htobe32(sc->sc_flow_table_id);
	mbin->cmd_group_id = htobe32(sc->sc_flow_group_id);

	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query flow group timeout\n", DEVNAME(sc));
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: query flow group reply corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	switch (out->cmd_status) {
	case MCX_CQ_STATUS_OK:
		break;
	default:
		printf("%s: query flow group failed (%x/%x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

        mbout = (struct mcx_cmd_query_flow_group_mb_out *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	dump = (uint8_t *)mbout;
	for (i = 0; i < MCX_CMDQ_MAILBOX_DATASIZE; i++) {
		printf("%.2x ", dump[i]);
		if (i % 16 == 15)
			printf("\n");
	}
	dump = (uint8_t *)(mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 1)));
	for (i = 0; i < MCX_CMDQ_MAILBOX_DATASIZE; i++) {
		printf("%.2x ", dump[i]);
		if (i % 16 == 15)
			printf("\n");
	}

free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}

int
mcx_dump_rq(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_rq_in *in;
	struct mcx_cmd_query_rq_out *out;
	struct mcx_cmd_query_rq_mb_out *mbout;
	uint8_t token = mcx_cmdq_token(sc);
	int error;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + sizeof(*mbout) + 16,
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_RQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_rqn = htobe32(sc->sc_rqn);

	CTASSERT(sizeof(*mbout) <= MCX_CMDQ_MAILBOX_DATASIZE*2);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query flow group mailboxes\n");
		return (-1);
	}

	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query rq timeout\n", DEVNAME(sc));
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: query rq reply corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	switch (out->cmd_status) {
	case MCX_CQ_STATUS_OK:
		break;
	default:
		printf("%s: query rq failed (%x/%x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

        mbout = (struct mcx_cmd_query_rq_mb_out *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	printf("%s: rq: state %d, ui %d, cqn %d, s/s %d/%d/%d, hw %d, sw %d\n",
	    DEVNAME(sc),
	    (betoh32(mbout->cmd_ctx.rq_flags) >> MCX_RQ_CTX_STATE_SHIFT) & 0x0f,
	    betoh32(mbout->cmd_ctx.rq_user_index),
	    betoh32(mbout->cmd_ctx.rq_cqn),
	    betoh16(mbout->cmd_ctx.rq_wq.wq_log_stride),
	    mbout->cmd_ctx.rq_wq.wq_log_page_sz,
	    mbout->cmd_ctx.rq_wq.wq_log_size,
	    betoh32(mbout->cmd_ctx.rq_wq.wq_hw_counter),
	    betoh32(mbout->cmd_ctx.rq_wq.wq_sw_counter));

free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}

int
mcx_dump_sq(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_sq_in *in;
	struct mcx_cmd_query_sq_out *out;
	struct mcx_cmd_query_sq_mb_out *mbout;
	uint8_t token = mcx_cmdq_token(sc);
	int error;
	int i;
	uint8_t *dump;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	mcx_cmdq_init(sc, cqe, sizeof(*in), sizeof(*out) + sizeof(*mbout) + 16,
	    token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_SQ);
	in->cmd_op_mod = htobe16(0);
	in->cmd_sqn = htobe32(sc->sc_sqn);

	CTASSERT(sizeof(*mbout) <= MCX_CMDQ_MAILBOX_DATASIZE*2);
	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 2,
	    &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query sq mailboxes\n");
		return (-1);
	}

	mcx_cmdq_mboxes_sign(&mxm, 1);

	mcx_cmdq_post(sc, cqe, 0);
	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query sq timeout\n", DEVNAME(sc));
		goto free;
	}
	error = mcx_cmdq_verify(cqe);
	if (error != 0) {
		printf("%s: query sq reply corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	switch (out->cmd_status) {
	case MCX_CQ_STATUS_OK:
		break;
	default:
		printf("%s: query sq failed (%x/%x)\n", DEVNAME(sc),
		    out->cmd_status, betoh32(out->cmd_syndrome));
		error = -1;
		goto free;
	}

        mbout = (struct mcx_cmd_query_sq_mb_out *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
/*
	printf("%s: rq: state %d, ui %d, cqn %d, s/s %d/%d/%d, hw %d, sw %d\n",
	    DEVNAME(sc),
	    (betoh32(mbout->cmd_ctx.rq_flags) >> MCX_RQ_CTX_STATE_SHIFT) & 0x0f,
	    betoh32(mbout->cmd_ctx.rq_user_index),
	    betoh32(mbout->cmd_ctx.rq_cqn),
	    betoh16(mbout->cmd_ctx.rq_wq.wq_log_stride),
	    mbout->cmd_ctx.rq_wq.wq_log_page_sz,
	    mbout->cmd_ctx.rq_wq.wq_log_size,
	    betoh32(mbout->cmd_ctx.rq_wq.wq_hw_counter),
	    betoh32(mbout->cmd_ctx.rq_wq.wq_sw_counter));
*/
	dump = (uint8_t *)mbout;
	for (i = 0; i < MCX_CMDQ_MAILBOX_DATASIZE; i++) {
		printf("%.2x ", dump[i]);
		if (i % 16 == 15)
			printf("\n");
	}

free:
	mcx_cq_mboxes_free(sc, &mxm);
	return (error);
}

static int
mcx_dump_counters(struct mcx_softc *sc)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_vport_counters_in *in;
	struct mcx_cmd_query_vport_counters_mb_in *mbin;
	struct mcx_cmd_query_vport_counters_out *out;
	struct mcx_nic_vport_counters *counters;
	int error, token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin),
	    sizeof(*out) + sizeof(*counters), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_VPORT_COUNTERS);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query nic vport counters mailboxen\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;

	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_clear = 0x80;

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query nic vport counters timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: query nic vport counters command corrupt\n",
		    DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: query nic vport counters failed (%x, %x)\n",
		    DEVNAME(sc), out->cmd_status, out->cmd_syndrome);
		error = -1;
		goto free;
	}

	counters = (struct mcx_nic_vport_counters *)
	    (mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	if (counters->rx_bcast.packets + counters->tx_bcast.packets +
	    counters->rx_ucast.packets + counters->tx_ucast.packets +
	    counters->rx_err.packets + counters->tx_err.packets)
		printf("%s: err %llx/%llx uc %llx/%llx bc %llx/%llx\n",
		    DEVNAME(sc),
		    betoh64(counters->tx_err.packets),
		    betoh64(counters->rx_err.packets),
		    betoh64(counters->tx_ucast.packets),
		    betoh64(counters->rx_ucast.packets),
		    betoh64(counters->tx_bcast.packets),
		    betoh64(counters->rx_bcast.packets));
free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

static int
mcx_dump_flow_counter(struct mcx_softc *sc, int index, const char *what)
{
	struct mcx_dmamem mxm;
	struct mcx_cmdq_entry *cqe;
	struct mcx_cmd_query_flow_counter_in *in;
	struct mcx_cmd_query_flow_counter_mb_in *mbin;
	struct mcx_cmd_query_flow_counter_out *out;
	struct mcx_counter *counters;
	int error, token;

	cqe = MCX_DMA_KVA(&sc->sc_cmdq_mem);
	token = mcx_cmdq_token(sc);
	mcx_cmdq_init(sc, cqe, sizeof(*in) + sizeof(*mbin), sizeof(*out) +
	    sizeof(*counters), token);

	in = mcx_cmdq_in(cqe);
	in->cmd_opcode = htobe16(MCX_CMD_QUERY_FLOW_COUNTER);
	in->cmd_op_mod = htobe16(0);

	if (mcx_cmdq_mboxes_alloc(sc, &mxm, 1, &cqe->cq_output_ptr, token) != 0) {
		printf(", unable to allocate query flow counter mailboxen\n");
		return (-1);
	}
	cqe->cq_input_ptr = cqe->cq_output_ptr;
	mbin = mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0));
	mbin->cmd_flow_counter_id = htobe16(sc->sc_flow_counter_id[index]);
	mbin->cmd_clear = 0x80;

	mcx_cmdq_mboxes_sign(&mxm, 1);
	mcx_cmdq_post(sc, cqe, 0);

	error = mcx_cmdq_poll(sc, cqe, 1000);
	if (error != 0) {
		printf("%s: query flow counter timeout\n", DEVNAME(sc));
		goto free;
	}
	if (mcx_cmdq_verify(cqe) != 0) {
		printf("%s: query flow counter command corrupt\n", DEVNAME(sc));
		goto free;
	}

	out = mcx_cmdq_out(cqe);
	if (out->cmd_status != MCX_CQ_STATUS_OK) {
		printf("%s: query flow counter failed (%x, %x)\n", DEVNAME(sc),
		    out->cmd_status, out->cmd_syndrome);
		error = -1;
		goto free;
	}

	counters = (struct mcx_counter *)(mcx_cq_mbox_data(mcx_cq_mbox(&mxm, 0)));
	if (counters->packets)
		printf("%s: %s inflow %llx\n", DEVNAME(sc), what,
		    betoh64(counters->packets));
free:
	mcx_dmamem_free(sc, &mxm);

	return (error);
}

#endif

int
mcx_rx_fill_slots(struct mcx_softc *sc, void *ring, struct mcx_slot *slots,
    uint *prod, int bufsize, uint nslots)
{
	struct mcx_rq_entry *rqe;
	struct mcx_slot *ms;
	struct mbuf *m;
	uint slot, p, fills;

	p = *prod;
	slot = (p % (1 << MCX_LOG_RQ_SIZE));
	rqe = ring;
	for (fills = 0; fills < nslots; fills++) {
		ms = &slots[slot];
		m = MCLGETI(NULL, M_DONTWAIT, NULL, bufsize + ETHER_ALIGN);
		if (m == NULL)
			break;

		m->m_data += ETHER_ALIGN;
		m->m_len = m->m_pkthdr.len = bufsize;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, ms->ms_map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			break;
		}
		ms->ms_m = m;

		rqe[slot].rqe_byte_count = htobe32(bufsize);
		rqe[slot].rqe_addr = htobe64(ms->ms_map->dm_segs[0].ds_addr);
		rqe[slot].rqe_lkey = htobe32(sc->sc_lkey);

		p++;
		slot++;
		if (slot == (1 << MCX_LOG_RQ_SIZE))
			slot = 0;
	}

	if (fills != 0) {
		*sc->sc_rx_doorbell = htobe32(p & MCX_WQ_DOORBELL_MASK);
		/* barrier? */
	}

	*prod = p;

	return (nslots - fills);
}

int
mcx_rx_fill(struct mcx_softc *sc)
{
	u_int slots;

	slots = if_rxr_get(&sc->sc_rxr, (1 << MCX_LOG_RQ_SIZE));
	if (slots == 0)
		return (1);

	slots = mcx_rx_fill_slots(sc, MCX_DMA_KVA(&sc->sc_rq_mem),
	    sc->sc_rx_slots, &sc->sc_rx_prod, sc->sc_hardmtu, slots);
	if_rxr_put(&sc->sc_rxr, slots);
	return (0);
}

void
mcx_refill(void *xsc)
{
	struct mcx_softc *sc = xsc;

	mcx_rx_fill(sc);

	if (if_rxr_inuse(&sc->sc_rxr) == 0)
		timeout_add(&sc->sc_rx_refill, 1);
}

void
mcx_process_txeof(struct mcx_softc *sc, struct mcx_cq_entry *cqe, int *txfree)
{
	struct mcx_slot *ms;
	bus_dmamap_t map;
	int slot, slots;

	slot = betoh16(cqe->cq_wqe_count) % (1 << MCX_LOG_SQ_SIZE);

	ms = &sc->sc_tx_slots[slot];
	map = ms->ms_map;
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);

	slots = 1;
	if (map->dm_nsegs > 1)
		slots += (map->dm_nsegs+2) / MCX_SQ_SEGS_PER_SLOT;

	(*txfree) += slots;
	bus_dmamap_unload(sc->sc_dmat, map);
	m_freem(ms->ms_m);
	ms->ms_m = NULL;
}

static uint64_t
mcx_uptime(void)
{
	struct timespec ts;

	nanouptime(&ts);

	return ((uint64_t)ts.tv_sec * 1000000000 + (uint64_t)ts.tv_nsec);
}

static void
mcx_calibrate_first(struct mcx_softc *sc)
{
	struct mcx_calibration *c = &sc->sc_calibration[0];

	sc->sc_calibration_gen = 0;

	c->c_ubase = mcx_uptime();
	c->c_tbase = mcx_timer(sc);
	c->c_tdiff = 0;

	timeout_add_sec(&sc->sc_calibrate, MCX_CALIBRATE_FIRST);
}

#define MCX_TIMESTAMP_SHIFT 10

static void
mcx_calibrate(void *arg)
{
	struct mcx_softc *sc = arg;
	struct mcx_calibration *nc, *pc;
	unsigned int gen;

	if (!ISSET(sc->sc_ac.ac_if.if_flags, IFF_RUNNING))
		return;

	timeout_add_sec(&sc->sc_calibrate, MCX_CALIBRATE_NORMAL);

	gen = sc->sc_calibration_gen;
	pc = &sc->sc_calibration[gen % nitems(sc->sc_calibration)];
	gen++;
	nc = &sc->sc_calibration[gen % nitems(sc->sc_calibration)];

	nc->c_uptime = pc->c_ubase;
	nc->c_timestamp = pc->c_tbase;

	nc->c_ubase = mcx_uptime();
	nc->c_tbase = mcx_timer(sc);

	nc->c_udiff = (nc->c_ubase - nc->c_uptime) >> MCX_TIMESTAMP_SHIFT;
	nc->c_tdiff = (nc->c_tbase - nc->c_timestamp) >> MCX_TIMESTAMP_SHIFT;

	membar_producer();
	sc->sc_calibration_gen = gen;
}

static int
mcx_process_rx(struct mcx_softc *sc, struct mcx_cq_entry *cqe,
    struct mbuf_list *ml, const struct mcx_calibration *c)
{
	struct mcx_slot *ms;
	struct mbuf *m;
	int slot;

	slot = betoh16(cqe->cq_wqe_count) % (1 << MCX_LOG_RQ_SIZE);

	ms = &sc->sc_rx_slots[slot];
	bus_dmamap_sync(sc->sc_dmat, ms->ms_map, 0, ms->ms_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, ms->ms_map);

	m = ms->ms_m;
	ms->ms_m = NULL;

	m->m_pkthdr.len = m->m_len = bemtoh32(&cqe->cq_byte_cnt);

	if (cqe->cq_rx_hash_type) {
		m->m_pkthdr.ph_flowid = M_FLOWID_VALID |
		    betoh32(cqe->cq_rx_hash);
	}

	if (c->c_tdiff) {
		uint64_t t = bemtoh64(&cqe->cq_timestamp) - c->c_timestamp;
		t *= c->c_udiff;
		t /= c->c_tdiff;

		m->m_pkthdr.ph_timestamp = c->c_uptime + t;
		SET(m->m_pkthdr.csum_flags, M_TIMESTAMP);
	}

	ml_enqueue(ml, m);

	return (1);
}

static struct mcx_cq_entry *
mcx_next_cq_entry(struct mcx_softc *sc, struct mcx_cq *cq)
{
	struct mcx_cq_entry *cqe;
	int next;

	cqe = (struct mcx_cq_entry *)MCX_DMA_KVA(&cq->cq_mem);
	next = cq->cq_cons % (1 << MCX_LOG_CQ_SIZE);

	if ((cqe[next].cq_opcode_owner & MCX_CQ_ENTRY_FLAG_OWNER) ==
	    ((cq->cq_cons >> MCX_LOG_CQ_SIZE) & 1)) {
		return (&cqe[next]);
	}

	return (NULL);
}

static void
mcx_arm_cq(struct mcx_softc *sc, struct mcx_cq *cq)
{
	bus_size_t offset;
	uint32_t val;
	uint64_t uval;

	/* different uar per cq? */
	offset = (MCX_PAGE_SIZE * sc->sc_uar);
	val = ((cq->cq_count) & 3) << MCX_CQ_DOORBELL_ARM_CMD_SN_SHIFT;
	val |= (cq->cq_cons & MCX_CQ_DOORBELL_ARM_CI_MASK);

	cq->cq_doorbell[0] = htobe32(cq->cq_cons & MCX_CQ_DOORBELL_ARM_CI_MASK);
	cq->cq_doorbell[1] = htobe32(val);

	uval = val;
	uval <<= 32;
	uval |= cq->cq_n;
	bus_space_write_raw_8(sc->sc_memt, sc->sc_memh,
	    offset + MCX_UAR_CQ_DOORBELL, htobe64(uval));
	mcx_bar(sc, offset + MCX_UAR_CQ_DOORBELL, sizeof(uint64_t),
	    BUS_SPACE_BARRIER_WRITE);
}

void
mcx_process_cq(struct mcx_softc *sc, struct mcx_cq *cq)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	const struct mcx_calibration *c;
	unsigned int gen;
	struct mcx_cq_entry *cqe;
	uint8_t *cqp;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	int rxfree, txfree;

	gen = sc->sc_calibration_gen;
	membar_consumer();
	c = &sc->sc_calibration[gen % nitems(sc->sc_calibration)];

	rxfree = 0;
	txfree = 0;
	while ((cqe = mcx_next_cq_entry(sc, cq))) {
		uint8_t opcode;
		opcode = (cqe->cq_opcode_owner >> MCX_CQ_ENTRY_OPCODE_SHIFT);
		switch (opcode) {
		case MCX_CQ_ENTRY_OPCODE_REQ:
			mcx_process_txeof(sc, cqe, &txfree);
			break;
		case MCX_CQ_ENTRY_OPCODE_SEND:
			rxfree += mcx_process_rx(sc, cqe, &ml, c);
			break;
		case MCX_CQ_ENTRY_OPCODE_REQ_ERR:
		case MCX_CQ_ENTRY_OPCODE_SEND_ERR:
			cqp = (uint8_t *)cqe;
			/* printf("%s: cq completion error: %x\n", DEVNAME(sc), cqp[0x37]); */
			break;

		default:
			/* printf("%s: cq completion opcode %x??\n", DEVNAME(sc), opcode); */
			break;
		}

		cq->cq_cons++;
	}

	cq->cq_count++;
	mcx_arm_cq(sc, cq);

	if (rxfree > 0) {
		if_rxr_put(&sc->sc_rxr, rxfree);
		if (ifiq_input(&sc->sc_ac.ac_if.if_rcv, &ml))
			if_rxr_livelocked(&sc->sc_rxr);

		mcx_rx_fill(sc);
		if (if_rxr_inuse(&sc->sc_rxr) == 0)
			timeout_add(&sc->sc_rx_refill, 1);
	}
	if (txfree > 0) {
		sc->sc_tx_cons += txfree;
		if (ifq_is_oactive(&ifp->if_snd))
			ifq_restart(&ifp->if_snd);
	}
}

static void
mcx_arm_eq(struct mcx_softc *sc)
{
	bus_size_t offset;
	uint32_t val;

	offset = (MCX_PAGE_SIZE * sc->sc_uar) + MCX_UAR_EQ_DOORBELL_ARM;
	val = (sc->sc_eqn << 24) | (sc->sc_eq_cons & 0xffffff);

	mcx_wr(sc, offset, val);
	/* barrier? */
}

static struct mcx_eq_entry *
mcx_next_eq_entry(struct mcx_softc *sc)
{
	struct mcx_eq_entry *eqe;
	int next;

	eqe = (struct mcx_eq_entry *)MCX_DMA_KVA(&sc->sc_eq_mem);
	next = sc->sc_eq_cons % (1 << MCX_LOG_EQ_SIZE);
	if ((eqe[next].eq_owner & 1) == ((sc->sc_eq_cons >> MCX_LOG_EQ_SIZE) & 1)) {
		sc->sc_eq_cons++;
		return (&eqe[next]);
	}
	return (NULL);
}

int
mcx_intr(void *xsc)
{
	struct mcx_softc *sc = (struct mcx_softc *)xsc;
	struct mcx_eq_entry *eqe;
	int i, cq;

	while ((eqe = mcx_next_eq_entry(sc))) {
		switch (eqe->eq_event_type) {
		case MCX_EVENT_TYPE_COMPLETION:
			cq = betoh32(eqe->eq_event_data[6]);
			for (i = 0; i < sc->sc_num_cq; i++) {
				if (sc->sc_cq[i].cq_n == cq) {
					mcx_process_cq(sc, &sc->sc_cq[i]);
					break;
				}
			}
			break;

		case MCX_EVENT_TYPE_LAST_WQE:
			/* printf("%s: last wqe reached?\n", DEVNAME(sc)); */
			break;

		case MCX_EVENT_TYPE_CQ_ERROR:
			/* printf("%s: cq error\n", DEVNAME(sc)); */
			break;

		case MCX_EVENT_TYPE_CMD_COMPLETION:
			/* wakeup probably */
			break;

		case MCX_EVENT_TYPE_PORT_CHANGE:
			task_add(systq, &sc->sc_port_change);
			break;

		default:
			/* printf("%s: something happened\n", DEVNAME(sc)); */
			break;
		}
	}
	mcx_arm_eq(sc);
	return (1);
}

static void
mcx_free_slots(struct mcx_softc *sc, struct mcx_slot *slots, int allocated,
    int total)
{
	struct mcx_slot *ms;

	int i = allocated;
	while (i-- > 0) {
		ms = &slots[i];
		bus_dmamap_destroy(sc->sc_dmat, ms->ms_map);
		if (ms->ms_m != NULL)
			m_freem(ms->ms_m);
	}
	free(slots, M_DEVBUF, total * sizeof(*ms));
}

static int
mcx_up(struct mcx_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mcx_slot *ms;
	int i, start;
	struct mcx_flow_match match_crit;

	sc->sc_rx_slots = mallocarray(sizeof(*ms), (1 << MCX_LOG_RQ_SIZE),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc->sc_rx_slots == NULL) {
		printf("%s: failed to allocate rx slots\n", DEVNAME(sc));
		return ENOMEM;
	}

	for (i = 0; i < (1 << MCX_LOG_RQ_SIZE); i++) {
		ms = &sc->sc_rx_slots[i];
		if (bus_dmamap_create(sc->sc_dmat, sc->sc_hardmtu, 1,
		    sc->sc_hardmtu, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &ms->ms_map) != 0) {
			printf("%s: failed to allocate rx dma maps\n",
			    DEVNAME(sc));
			goto destroy_rx_slots;
		}
	}

	sc->sc_tx_slots = mallocarray(sizeof(*ms), (1 << MCX_LOG_SQ_SIZE),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc->sc_tx_slots == NULL) {
		printf("%s: failed to allocate tx slots\n", DEVNAME(sc));
		goto destroy_rx_slots;
	}

	for (i = 0; i < (1 << MCX_LOG_SQ_SIZE); i++) {
		ms = &sc->sc_tx_slots[i];
		if (bus_dmamap_create(sc->sc_dmat, sc->sc_hardmtu,
		    MCX_SQ_MAX_SEGMENTS, sc->sc_hardmtu, 0,
		    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
		    &ms->ms_map) != 0) {
			printf("%s: failed to allocate tx dma maps\n",
			    DEVNAME(sc));
			goto destroy_tx_slots;
		}
	}

	if (mcx_create_cq(sc, sc->sc_eqn) != 0)
		goto down;

	/* send queue */
	if (mcx_create_tis(sc) != 0)
		goto down;

	if (mcx_create_sq(sc, sc->sc_cq[0].cq_n) != 0)
		goto down;

	/* receive queue */
	if (mcx_create_rq(sc, sc->sc_cq[0].cq_n) != 0)
		goto down;

	if (mcx_create_tir(sc) != 0)
		goto down;

	if (mcx_create_flow_table(sc, MCX_LOG_FLOW_TABLE_SIZE) != 0)
		goto down;

	/* promisc flow group */
	start = 0;
	memset(&match_crit, 0, sizeof(match_crit));
	if (mcx_create_flow_group(sc, MCX_FLOW_GROUP_PROMISC, start, 1,
	    0, &match_crit) != 0)
		goto down;
	sc->sc_promisc_flow_enabled = 0;
	start++;

	/* all multicast flow group */
	match_crit.mc_dest_mac[0] = 0x01;
	if (mcx_create_flow_group(sc, MCX_FLOW_GROUP_ALLMULTI, start, 1,
	    MCX_CREATE_FLOW_GROUP_CRIT_OUTER, &match_crit) != 0)
		goto down;
	sc->sc_allmulti_flow_enabled = 0;
	start++;

	/* mac address matching flow group */
	memset(&match_crit.mc_dest_mac, 0xff, sizeof(match_crit.mc_dest_mac));
	if (mcx_create_flow_group(sc, MCX_FLOW_GROUP_MAC, start,
	    (1 << MCX_LOG_FLOW_TABLE_SIZE) - start,
	    MCX_CREATE_FLOW_GROUP_CRIT_OUTER, &match_crit) != 0)
		goto down;

	/* flow table entries for unicast and broadcast */
	start = 0;
	if (mcx_set_flow_table_entry(sc, MCX_FLOW_GROUP_MAC, start,
	    sc->sc_ac.ac_enaddr) != 0)
		goto down;
	start++;

	if (mcx_set_flow_table_entry(sc, MCX_FLOW_GROUP_MAC, start,
	    etherbroadcastaddr) != 0)
		goto down;
	start++;

	/* multicast entries go after that */
	sc->sc_mcast_flow_base = start;

	/* re-add any existing multicast flows */
	for (i = 0; i < MCX_NUM_MCAST_FLOWS; i++) {
		if (sc->sc_mcast_flows[i][0] != 0) {
			mcx_set_flow_table_entry(sc, MCX_FLOW_GROUP_MAC,
			    sc->sc_mcast_flow_base + i,
			    sc->sc_mcast_flows[i]);
		}
	}

	if (mcx_set_flow_table_root(sc) != 0)
		goto down;

	/* start the queues */
	if (mcx_ready_sq(sc) != 0)
		goto down;

	if (mcx_ready_rq(sc) != 0)
		goto down;

	if_rxr_init(&sc->sc_rxr, 1, (1 << MCX_LOG_RQ_SIZE));
	sc->sc_rx_prod = 0;
	mcx_rx_fill(sc);

	mcx_calibrate_first(sc);

	SET(ifp->if_flags, IFF_RUNNING);

	sc->sc_tx_cons = 0;
	sc->sc_tx_prod = 0;
	ifq_clr_oactive(&ifp->if_snd);
	ifq_restart(&ifp->if_snd);

	return ENETRESET;
destroy_tx_slots:
	mcx_free_slots(sc, sc->sc_tx_slots, i, (1 << MCX_LOG_SQ_SIZE));
	sc->sc_tx_slots = NULL;

	i = (1 << MCX_LOG_RQ_SIZE);
destroy_rx_slots:
	mcx_free_slots(sc, sc->sc_rx_slots, i, (1 << MCX_LOG_RQ_SIZE));
	sc->sc_rx_slots = NULL;
down:
	mcx_down(sc);
	return ENOMEM;
}

static void
mcx_down(struct mcx_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	int group, i;

	CLR(ifp->if_flags, IFF_RUNNING);

	/*
	 * delete flow table entries first, so no packets can arrive
	 * after the barriers
	 */
	if (sc->sc_promisc_flow_enabled)
		mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_PROMISC, 0);
	if (sc->sc_allmulti_flow_enabled)
		mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_ALLMULTI, 0);
	mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_MAC, 0);
	mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_MAC, 1);
	for (i = 0; i < MCX_NUM_MCAST_FLOWS; i++) {
		if (sc->sc_mcast_flows[i][0] != 0) {
			mcx_delete_flow_table_entry(sc, MCX_FLOW_GROUP_MAC,
			    sc->sc_mcast_flow_base + i);
		}
	}

	intr_barrier(sc->sc_ihc);
	ifq_barrier(&ifp->if_snd);

	timeout_del_barrier(&sc->sc_calibrate);

	for (group = 0; group < MCX_NUM_FLOW_GROUPS; group++) {
		if (sc->sc_flow_group_id[group] != -1)
			mcx_destroy_flow_group(sc,
			    sc->sc_flow_group_id[group]);
	}

	if (sc->sc_flow_table_id != -1)
		mcx_destroy_flow_table(sc);

	if (sc->sc_tirn != 0)
		mcx_destroy_tir(sc);
	if (sc->sc_rqn != 0)
		mcx_destroy_rq(sc);

	if (sc->sc_sqn != 0)
		mcx_destroy_sq(sc);
	if (sc->sc_tisn != 0)
		mcx_destroy_tis(sc);

	for (i = 0; i < sc->sc_num_cq; i++)
		mcx_destroy_cq(sc, i);
	sc->sc_num_cq = 0;

	if (sc->sc_tx_slots != NULL) {
		mcx_free_slots(sc, sc->sc_tx_slots, (1 << MCX_LOG_SQ_SIZE),
		    (1 << MCX_LOG_SQ_SIZE));
		sc->sc_tx_slots = NULL;
	}
	if (sc->sc_rx_slots != NULL) {
		mcx_free_slots(sc, sc->sc_rx_slots, (1 << MCX_LOG_RQ_SIZE),
		    (1 << MCX_LOG_RQ_SIZE));
		sc->sc_rx_slots = NULL;
	}
}

static int
mcx_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct mcx_softc *sc = (struct mcx_softc *)ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	uint8_t addrhi[ETHER_ADDR_LEN], addrlo[ETHER_ADDR_LEN];
	int s, i, error = 0;

	s = splnet();
	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = ENETRESET;
			else
				error = mcx_up(sc);
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				mcx_down(sc);
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	case SIOCGIFSFFPAGE:
		error = mcx_get_sffpage(ifp, (struct if_sffpage *)data);
		break;

	case SIOCGIFRXR:
		error = mcx_rxrinfo(sc, (struct if_rxrinfo *)ifr->ifr_data);
		break;

	case SIOCADDMULTI:
		if (ether_addmulti(ifr, &sc->sc_ac) == ENETRESET) {
			error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
			if (error != 0)
				return (error);

			for (i = 0; i < MCX_NUM_MCAST_FLOWS; i++) {
				if (sc->sc_mcast_flows[i][0] == 0) {
					memcpy(sc->sc_mcast_flows[i], addrlo,
					    ETHER_ADDR_LEN);
					if (ISSET(ifp->if_flags, IFF_RUNNING)) {
						mcx_set_flow_table_entry(sc,
						    MCX_FLOW_GROUP_MAC,
						    sc->sc_mcast_flow_base + i,
						    sc->sc_mcast_flows[i]);
					}
					break;
				}
			}

			if (!ISSET(ifp->if_flags, IFF_ALLMULTI)) {
				if (i == MCX_NUM_MCAST_FLOWS) {
					SET(ifp->if_flags, IFF_ALLMULTI);
					sc->sc_extra_mcast++;
					error = ENETRESET;
				}

				if (sc->sc_ac.ac_multirangecnt > 0) {
					SET(ifp->if_flags, IFF_ALLMULTI);
					error = ENETRESET;
				}
			}
		}
		break;

	case SIOCDELMULTI:
		if (ether_delmulti(ifr, &sc->sc_ac) == ENETRESET) {
			error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
			if (error != 0)
				return (error);

			for (i = 0; i < MCX_NUM_MCAST_FLOWS; i++) {
				if (memcmp(sc->sc_mcast_flows[i], addrlo,
				    ETHER_ADDR_LEN) == 0) {
					if (ISSET(ifp->if_flags, IFF_RUNNING)) {
						mcx_delete_flow_table_entry(sc,
						    MCX_FLOW_GROUP_MAC,
						    sc->sc_mcast_flow_base + i);
					}
					sc->sc_mcast_flows[i][0] = 0;
					break;
				}
			}

			if (i == MCX_NUM_MCAST_FLOWS)
				sc->sc_extra_mcast--;

			if (ISSET(ifp->if_flags, IFF_ALLMULTI) &&
			    (sc->sc_extra_mcast == 0) &&
			    (sc->sc_ac.ac_multirangecnt == 0)) {
				CLR(ifp->if_flags, IFF_ALLMULTI);
				error = ENETRESET;
			}
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			mcx_iff(sc);
		error = 0;
	}
	splx(s);

	return (error);
}

static int
mcx_get_sffpage(struct ifnet *ifp, struct if_sffpage *sff)
{
	struct mcx_softc *sc = (struct mcx_softc *)ifp->if_softc;
	struct mcx_reg_mcia mcia;
	struct mcx_reg_pmlp pmlp;
	int offset, error;

	/* get module number */
	memset(&pmlp, 0, sizeof(pmlp));
	pmlp.rp_local_port = 1;
	error = mcx_access_hca_reg(sc, MCX_REG_PMLP, MCX_REG_OP_READ, &pmlp,
	    sizeof(pmlp));
	if (error != 0) {
		printf("%s: unable to get eeprom module number\n",
		    DEVNAME(sc));
		return error;
	}

	for (offset = 0; offset < 256; offset += MCX_MCIA_EEPROM_BYTES) {
		memset(&mcia, 0, sizeof(mcia));
		mcia.rm_l = 0;
		mcia.rm_module = betoh32(pmlp.rp_lane0_mapping) &
		    MCX_PMLP_MODULE_NUM_MASK;
		mcia.rm_i2c_addr = sff->sff_addr / 2;	/* apparently */
		mcia.rm_page_num = sff->sff_page;
		mcia.rm_dev_addr = htobe16(offset);
		mcia.rm_size = htobe16(MCX_MCIA_EEPROM_BYTES);

		error = mcx_access_hca_reg(sc, MCX_REG_MCIA, MCX_REG_OP_READ,
		    &mcia, sizeof(mcia));
		if (error != 0) {
			printf("%s: unable to read eeprom at %x\n",
			    DEVNAME(sc), offset);
			return error;
		}

		memcpy(sff->sff_data + offset, mcia.rm_data,
		    MCX_MCIA_EEPROM_BYTES);
	}

	return 0;
}

static int
mcx_rxrinfo(struct mcx_softc *sc, struct if_rxrinfo *ifri)
{
	struct if_rxring_info ifr;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_size = sc->sc_hardmtu;
	ifr.ifr_info = sc->sc_rxr;

	return (if_rxr_info_ioctl(ifri, 1, &ifr));
}

int
mcx_load_mbuf(struct mcx_softc *sc, struct mcx_slot *ms, struct mbuf *m)
{
	switch (bus_dmamap_load_mbuf(sc->sc_dmat, ms->ms_map, m,
	    BUS_DMA_STREAMING | BUS_DMA_NOWAIT)) {
	case 0:
		break;

	case EFBIG:
		if (m_defrag(m, M_DONTWAIT) == 0 &&
		    bus_dmamap_load_mbuf(sc->sc_dmat, ms->ms_map, m,
		    BUS_DMA_STREAMING | BUS_DMA_NOWAIT) == 0)
			break;

	default:
		return (1);
	}

	ms->ms_m = m;
	return (0);
}

static void
mcx_start(struct ifqueue *ifq)
{
	struct ifnet *ifp = ifq->ifq_if;
	struct mcx_softc *sc = ifp->if_softc;
	struct mcx_sq_entry *sq, *sqe;
	struct mcx_sq_entry_seg *sqs;
	struct mcx_slot *ms;
	bus_dmamap_t map;
	struct mbuf *m;
	u_int idx, free, used;
	uint64_t *bf;
	size_t bf_base;
	int i, seg, nseg;

	bf_base = (sc->sc_uar * MCX_PAGE_SIZE) + MCX_UAR_BF;

	idx = sc->sc_tx_prod % (1 << MCX_LOG_SQ_SIZE);
	free = (sc->sc_tx_cons + (1 << MCX_LOG_SQ_SIZE)) - sc->sc_tx_prod;

	used = 0;
	bf = NULL;
	sq = (struct mcx_sq_entry *)MCX_DMA_KVA(&sc->sc_sq_mem);

	for (;;) {
		if (used + MCX_SQ_ENTRY_MAX_SLOTS >= free) {
			ifq_set_oactive(ifq);
			break;
		}

		m = ifq_dequeue(ifq);
		if (m == NULL) {
			break;
		}

		sqe = sq + idx;
		ms = &sc->sc_tx_slots[idx];
		memset(sqe, 0, sizeof(*sqe));

		/* ctrl segment */
		sqe->sqe_opcode_index = htobe32(MCX_SQE_WQE_OPCODE_SEND |
		    ((sc->sc_tx_prod & 0xffff) << MCX_SQE_WQE_INDEX_SHIFT));
		/* always generate a completion event */
		sqe->sqe_signature = htobe32(MCX_SQE_CE_CQE_ALWAYS);

		/* eth segment */
		sqe->sqe_inline_header_size = htobe16(MCX_SQ_INLINE_SIZE);
		m_copydata(m, 0, MCX_SQ_INLINE_SIZE,
		    (caddr_t)sqe->sqe_inline_headers);
		m_adj(m, MCX_SQ_INLINE_SIZE);

		if (mcx_load_mbuf(sc, ms, m) != 0) {
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}
		bf = (uint64_t *)sqe;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_hdr(ifp->if_bpf,
			    (caddr_t)sqe->sqe_inline_headers,
			    MCX_SQ_INLINE_SIZE, m, BPF_DIRECTION_OUT);
#endif
		map = ms->ms_map;
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		sqe->sqe_ds_sq_num =
		    htobe32((sc->sc_sqn << MCX_SQE_SQ_NUM_SHIFT) |
		    (map->dm_nsegs + 3));

		/* data segment - first wqe has one segment */
		sqs = sqe->sqe_segs;
		seg = 0;
		nseg = 1;
		for (i = 0; i < map->dm_nsegs; i++) {
			if (seg == nseg) {
				/* next slot */
				idx++;
				if (idx == (1 << MCX_LOG_SQ_SIZE))
					idx = 0;
				sc->sc_tx_prod++;
				used++;

				sqs = (struct mcx_sq_entry_seg *)(sq + idx);
				seg = 0;
				nseg = MCX_SQ_SEGS_PER_SLOT;
			}
			sqs[seg].sqs_byte_count =
			    htobe32(map->dm_segs[i].ds_len);
			sqs[seg].sqs_lkey = htobe32(sc->sc_lkey);
			sqs[seg].sqs_addr = htobe64(map->dm_segs[i].ds_addr);
			seg++;
		}

		idx++;
		if (idx == (1 << MCX_LOG_SQ_SIZE))
			idx = 0;
		sc->sc_tx_prod++;
		used++;
	}

	if (used) {
		*sc->sc_tx_doorbell = htobe32(sc->sc_tx_prod & MCX_WQ_DOORBELL_MASK);

		membar_sync();

		/*
		 * write the first 64 bits of the last sqe we produced
		 * to the blue flame buffer
		 */
		bus_space_write_raw_8(sc->sc_memt, sc->sc_memh,
		    bf_base + sc->sc_bf_offset, *bf);
		/* next write goes to the other buffer */
		sc->sc_bf_offset ^= sc->sc_bf_size;

		membar_sync();
	}
}

static void
mcx_watchdog(struct ifnet *ifp)
{
}

static void
mcx_media_add_types(struct mcx_softc *sc)
{
	struct mcx_reg_ptys ptys;
	int i;
	uint32_t proto_cap;

	memset(&ptys, 0, sizeof(ptys));
	ptys.rp_local_port = 1;
	ptys.rp_proto_mask = MCX_REG_PTYS_PROTO_MASK_ETH;
	if (mcx_access_hca_reg(sc, MCX_REG_PTYS, MCX_REG_OP_READ, &ptys,
	    sizeof(ptys)) != 0) {
		printf("%s: unable to read port type/speed\n", DEVNAME(sc));
		return;
	}

	proto_cap = betoh32(ptys.rp_eth_proto_cap);
	for (i = 0; i < nitems(mcx_eth_cap_map); i++) {
		const struct mcx_eth_proto_capability *cap;
		if (!ISSET(proto_cap, 1 << i))
			continue;

		cap = &mcx_eth_cap_map[i];
		if (cap->cap_media == 0)
			continue;

		ifmedia_add(&sc->sc_media, IFM_ETHER | cap->cap_media, 0, NULL);
	}
}

static void
mcx_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct mcx_softc *sc = (struct mcx_softc *)ifp->if_softc;
	struct mcx_reg_ptys ptys;
	int i;
	uint32_t proto_oper;
	uint64_t media_oper;

	memset(&ptys, 0, sizeof(ptys));
	ptys.rp_local_port = 1;
	ptys.rp_proto_mask = MCX_REG_PTYS_PROTO_MASK_ETH;

	if (mcx_access_hca_reg(sc, MCX_REG_PTYS, MCX_REG_OP_READ, &ptys,
	    sizeof(ptys)) != 0) {
		printf("%s: unable to read port type/speed\n", DEVNAME(sc));
		return;
	}

	proto_oper = betoh32(ptys.rp_eth_proto_oper);

	media_oper = 0;

	for (i = 0; i < nitems(mcx_eth_cap_map); i++) {
		const struct mcx_eth_proto_capability *cap;
		if (!ISSET(proto_oper, 1 << i))
			continue;

		cap = &mcx_eth_cap_map[i];

		if (cap->cap_media != 0)
			media_oper = cap->cap_media;
	}

	ifmr->ifm_status = IFM_AVALID;
	/* not sure if this is the right thing to check, maybe paos? */
	if (proto_oper != 0) {
		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active = IFM_ETHER | IFM_AUTO | media_oper;
		/* txpause, rxpause, duplex? */
	}
}

static int
mcx_media_change(struct ifnet *ifp)
{
	struct mcx_softc *sc = (struct mcx_softc *)ifp->if_softc;
	struct mcx_reg_ptys ptys;
	struct mcx_reg_paos paos;
	uint32_t media;
	int i, error;

	if (IFM_TYPE(sc->sc_media.ifm_media) != IFM_ETHER)
		return EINVAL;

	error = 0;

	if (IFM_SUBTYPE(sc->sc_media.ifm_media) == IFM_AUTO) {
		/* read ptys to get supported media */
		memset(&ptys, 0, sizeof(ptys));
		ptys.rp_local_port = 1;
		ptys.rp_proto_mask = MCX_REG_PTYS_PROTO_MASK_ETH;
		if (mcx_access_hca_reg(sc, MCX_REG_PTYS, MCX_REG_OP_READ,
		    &ptys, sizeof(ptys)) != 0) {
			printf("%s: unable to read port type/speed\n",
			    DEVNAME(sc));
			return EIO;
		}

		media = betoh32(ptys.rp_eth_proto_cap);
	} else {
		/* map media type */
		media = 0;
		for (i = 0; i < nitems(mcx_eth_cap_map); i++) {
			const struct  mcx_eth_proto_capability *cap;

			cap = &mcx_eth_cap_map[i];
			if (cap->cap_media ==
			    IFM_SUBTYPE(sc->sc_media.ifm_media)) {
				media = (1 << i);
				break;
			}
		}
	}

	/* disable the port */
	memset(&paos, 0, sizeof(paos));
	paos.rp_local_port = 1;
	paos.rp_admin_status = MCX_REG_PAOS_ADMIN_STATUS_DOWN;
	paos.rp_admin_state_update = MCX_REG_PAOS_ADMIN_STATE_UPDATE_EN;
	if (mcx_access_hca_reg(sc, MCX_REG_PAOS, MCX_REG_OP_WRITE, &paos,
	    sizeof(paos)) != 0) {
		printf("%s: unable to set port state to down\n", DEVNAME(sc));
		return EIO;
	}

	memset(&ptys, 0, sizeof(ptys));
	ptys.rp_local_port = 1;
	ptys.rp_proto_mask = MCX_REG_PTYS_PROTO_MASK_ETH;
	ptys.rp_eth_proto_admin = htobe32(media);
	if (mcx_access_hca_reg(sc, MCX_REG_PTYS, MCX_REG_OP_WRITE, &ptys,
	    sizeof(ptys)) != 0) {
		printf("%s: unable to set port media type/speed\n",
		    DEVNAME(sc));
		error = EIO;
	}

	/* re-enable the port to start negotiation */
	memset(&paos, 0, sizeof(paos));
	paos.rp_local_port = 1;
	paos.rp_admin_status = MCX_REG_PAOS_ADMIN_STATUS_UP;
	paos.rp_admin_state_update = MCX_REG_PAOS_ADMIN_STATE_UPDATE_EN;
	if (mcx_access_hca_reg(sc, MCX_REG_PAOS, MCX_REG_OP_WRITE, &paos,
	    sizeof(paos)) != 0) {
		printf("%s: unable to set port state to up\n", DEVNAME(sc));
		error = EIO;
	}

	return error;
}

static void
mcx_port_change(void *xsc)
{
	struct mcx_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mcx_reg_paos paos = {
		.rp_local_port = 1,
	};
	struct mcx_reg_ptys ptys = {
		.rp_local_port = 1,
		.rp_proto_mask = MCX_REG_PTYS_PROTO_MASK_ETH,
	};
	int link_state = LINK_STATE_DOWN;

	if (mcx_access_hca_reg(sc, MCX_REG_PAOS, MCX_REG_OP_READ, &paos,
	    sizeof(paos)) == 0) {
		if (paos.rp_oper_status == MCX_REG_PAOS_OPER_STATUS_UP)
			link_state = LINK_STATE_FULL_DUPLEX;
	}

	if (mcx_access_hca_reg(sc, MCX_REG_PTYS, MCX_REG_OP_READ, &ptys,
	    sizeof(ptys)) == 0) {
		uint32_t proto_oper = betoh32(ptys.rp_eth_proto_oper);
		uint64_t baudrate = 0;
		unsigned int i;

		for (i = 0; i < nitems(mcx_eth_cap_map); i++) {
			const struct mcx_eth_proto_capability *cap;
			if (!ISSET(proto_oper, 1 << i))
				continue;

			cap = &mcx_eth_cap_map[i];
			if (cap->cap_baudrate == 0)
				continue;

			baudrate = cap->cap_baudrate;
			break;
		}

		ifp->if_baudrate = baudrate;
	}

	if (link_state != ifp->if_link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

static inline uint32_t
mcx_rd(struct mcx_softc *sc, bus_size_t r)
{
	uint32_t word;

	word = bus_space_read_raw_4(sc->sc_memt, sc->sc_memh, r);

	return (betoh32(word));
}

static inline void
mcx_wr(struct mcx_softc *sc, bus_size_t r, uint32_t v)
{
	bus_space_write_raw_4(sc->sc_memt, sc->sc_memh, r, htobe32(v));
}

static inline void
mcx_bar(struct mcx_softc *sc, bus_size_t r, bus_size_t l, int f)
{
	bus_space_barrier(sc->sc_memt, sc->sc_memh, r, l, f);
}

static uint64_t
mcx_timer(struct mcx_softc *sc)
{
	uint32_t hi, lo, ni;

	hi = mcx_rd(sc, MCX_INTERNAL_TIMER_H);
	for (;;) {
		lo = mcx_rd(sc, MCX_INTERNAL_TIMER_L);
		mcx_bar(sc, MCX_INTERNAL_TIMER_L, 8, BUS_SPACE_BARRIER_READ);
		ni = mcx_rd(sc, MCX_INTERNAL_TIMER_H);

		if (ni == hi)
			break;

		hi = ni;
	}

	return (((uint64_t)hi << 32) | (uint64_t)lo);
}

static int
mcx_dmamem_alloc(struct mcx_softc *sc, struct mcx_dmamem *mxm,
    bus_size_t size, u_int align)
{
	mxm->mxm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, mxm->mxm_size, 1,
	    mxm->mxm_size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW | BUS_DMA_64BIT,
	    &mxm->mxm_map) != 0)
		return (1);
	if (bus_dmamem_alloc(sc->sc_dmat, mxm->mxm_size,
	    align, 0, &mxm->mxm_seg, 1, &mxm->mxm_nsegs,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;
	if (bus_dmamem_map(sc->sc_dmat, &mxm->mxm_seg, mxm->mxm_nsegs,
	    mxm->mxm_size, &mxm->mxm_kva, BUS_DMA_WAITOK) != 0)
		goto free;
	if (bus_dmamap_load(sc->sc_dmat, mxm->mxm_map, mxm->mxm_kva,
	    mxm->mxm_size, NULL, BUS_DMA_WAITOK) != 0)
		goto unmap;

	return (0);
unmap:
	bus_dmamem_unmap(sc->sc_dmat, mxm->mxm_kva, mxm->mxm_size);
free:
	bus_dmamem_free(sc->sc_dmat, &mxm->mxm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mxm->mxm_map);
	return (1);
}

static void
mcx_dmamem_zero(struct mcx_dmamem *mxm)
{
	memset(MCX_DMA_KVA(mxm), 0, MCX_DMA_LEN(mxm));
}

static void
mcx_dmamem_free(struct mcx_softc *sc, struct mcx_dmamem *mxm)
{
	bus_dmamap_unload(sc->sc_dmat, mxm->mxm_map);
	bus_dmamem_unmap(sc->sc_dmat, mxm->mxm_kva, mxm->mxm_size);
	bus_dmamem_free(sc->sc_dmat, &mxm->mxm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mxm->mxm_map);
}

static int
mcx_hwmem_alloc(struct mcx_softc *sc, struct mcx_hwmem *mhm, unsigned int pages)
{
	bus_dma_segment_t *segs;
	bus_size_t len = pages * MCX_PAGE_SIZE;
	size_t seglen;

	segs = mallocarray(sizeof(*segs), pages, M_DEVBUF, M_WAITOK|M_CANFAIL);
	if (segs == NULL)
		return (-1);

	seglen = sizeof(*segs) * pages;

	if (bus_dmamem_alloc(sc->sc_dmat, len, MCX_PAGE_SIZE, 0,
	    segs, pages, &mhm->mhm_seg_count, BUS_DMA_NOWAIT) != 0)
		goto free_segs;

	if (mhm->mhm_seg_count < pages) {
		size_t nseglen;

		mhm->mhm_segs = mallocarray(sizeof(*mhm->mhm_segs),
		    mhm->mhm_seg_count, M_DEVBUF, M_WAITOK|M_CANFAIL);
		if (mhm->mhm_segs == NULL)
			goto free_dmamem;

		nseglen = sizeof(*mhm->mhm_segs) * mhm->mhm_seg_count;

		memcpy(mhm->mhm_segs, segs, nseglen);

		free(segs, M_DEVBUF, seglen);

		segs = mhm->mhm_segs;
		seglen = nseglen;
	} else
		mhm->mhm_segs = segs;

	if (bus_dmamap_create(sc->sc_dmat, len, pages, MCX_PAGE_SIZE,
	    MCX_PAGE_SIZE, BUS_DMA_NOWAIT|BUS_DMA_ALLOCNOW /*|BUS_DMA_64BIT*/,
	    &mhm->mhm_map) != 0)
		goto free_dmamem;

	if (bus_dmamap_load_raw(sc->sc_dmat, mhm->mhm_map,
	    mhm->mhm_segs, mhm->mhm_seg_count, len, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	bus_dmamap_sync(sc->sc_dmat, mhm->mhm_map,
	    0, mhm->mhm_map->dm_mapsize, BUS_DMASYNC_PRERW);

	mhm->mhm_npages = pages;

	return (0);

destroy:
	bus_dmamap_destroy(sc->sc_dmat, mhm->mhm_map);
free_dmamem:
	bus_dmamem_free(sc->sc_dmat, mhm->mhm_segs, mhm->mhm_seg_count);
free_segs:
	free(segs, M_DEVBUF, seglen);
	mhm->mhm_segs = NULL;

	return (-1);
}

static void
mcx_hwmem_free(struct mcx_softc *sc, struct mcx_hwmem *mhm)
{
	if (mhm->mhm_npages == 0)
		return;

	bus_dmamap_sync(sc->sc_dmat, mhm->mhm_map,
	    0, mhm->mhm_map->dm_mapsize, BUS_DMASYNC_POSTRW);

	bus_dmamap_unload(sc->sc_dmat, mhm->mhm_map);
	bus_dmamap_destroy(sc->sc_dmat, mhm->mhm_map);
	bus_dmamem_free(sc->sc_dmat, mhm->mhm_segs, mhm->mhm_seg_count);
	free(mhm->mhm_segs, M_DEVBUF,
	    sizeof(*mhm->mhm_segs) * mhm->mhm_seg_count);

	mhm->mhm_npages = 0;
}
