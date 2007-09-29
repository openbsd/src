/* $OpenBSD: qlireg.h,v 1.8 2007/09/29 16:03:43 marco Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2007 David Collins <dave@davec.name>
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

#define QLI_MAXFER				(MAXPHYS)
#define QLI_SOFT_RESET_RETRIES			(3)

struct qli_port_regs {
	u_int32_t		qpr_ext_hw_conf;
	u_int32_t		qpr_chip_config;
	u_int32_t		qpr_port_ctrl;
	u_int32_t		qpr_port_status;
	u_int32_t		qpr_host_prim_mac_hi;
	u_int32_t		qpr_host_prim_mac_lo;
	u_int32_t		qpr_sec_host_mac_hi;
	u_int32_t		qpr_sec_host_mac_lo;
	u_int32_t		qpr_ep_prim_mac_hi;
	u_int32_t		qpr_ep_prim_mac_lo;
	u_int32_t		qpr_ep_sec_mac_hi;
	u_int32_t		qpr_ep_sec_mac_lo;
	u_int32_t		qpr_host_prim_ip_hi;
	u_int32_t		qpr_host_prim_ip_mid_hi;
	u_int32_t		qpr_host_prim_ip_mid_lo;
	u_int32_t		qpr_host_prim_ip_lo;
	u_int32_t		qpr_host_sec_ip_hi;
	u_int32_t		qpr_host_sec_ip_mid_hi;
	u_int32_t		qpr_host_sec_ip_mid_lo;
	u_int32_t		qpr_host_sec_ip_lo;
	u_int32_t		qpr_ep_prim_ip_hi;
	u_int32_t		qpr_epprimipmidhi;
	u_int32_t		qpr_ep_prim_ip_mid_lo;
	u_int32_t		qpr_ep_prim_ip_lo;
	u_int32_t		qpr_eps_ec_ip_hi;
	u_int32_t		qpr_ep_sec_ip_mid_hi;
	u_int32_t		qpr_ep_sec_ip_mid_lo;
	u_int32_t		qpr_ep_sec_ip_lo;
	u_int32_t		qpr_ip_reassembly_timeout;
	u_int32_t		qpr_eth_max_frame_payload;
	u_int32_t		qpr_tcp_max_window_size;
	u_int32_t		qpr_tcp_current_timestamp_hi;
	u_int32_t		qpr_tcp_current_timestamp_lo;
	u_int32_t		qpr_local_ram_address;
	u_int32_t		qpr_local_ram_data;
	u_int32_t		qpr_res1;
	u_int32_t		qpr_gp_out;
	u_int32_t		qpr_gp_in;
	u_int32_t		qpr_probe_mux_addr;
	u_int32_t		qpr_probe_mux_data;
	u_int32_t		qpr_stats_index;
	u_int32_t		qpr_stats_read_data_inc;
	u_int32_t		qpr_stats_read_data_noinc;
	u_int32_t		qpr_port_err_status;
} __packed;

struct qli_mem_regs {
	u_int32_t		qmr_net_request_queue_out;
	u_int32_t		qmr_net_request_queue_out_addr_hi;
	u_int32_t		qmr_net_request_queue_out_addr_lo;
	u_int32_t		qmr_net_request_queue_base_addr_hi;
	u_int32_t		qmr_net_request_queue_base_addr_lo;
	u_int32_t		qmr_net_request_queue_length;
	u_int32_t		qmr_net_response_queue_in;
	u_int32_t		qmr_net_response_queue_in_addr_hi;
	u_int32_t		qmr_net_response_queue_in_addr_low;
	u_int32_t		qmr_net_response_queue_base_addr_hi;
	u_int32_t		qmr_net_response_queue_base_addr_lo;
	u_int32_t		qmr_net_response_queue_length;
	u_int32_t		qmr_req_q_out;
	u_int32_t		qmr_request_queue_out_addr_hi;
	u_int32_t		qmr_request_queue_out__addr_lo;
	u_int32_t		qmr_request_queue_base_addr_hi;
	u_int32_t		qmr_request_queue_base_addr_lo;
	u_int32_t		qmr_request_queue_length;
	u_int32_t		qmr_response_queue_in;
	u_int32_t		qmr_response_queue_in_addr_hi;
	u_int32_t		qmr_response_queue_in_addr_lo;
	u_int32_t		qmr_response_queue_base_addr_hi;
	u_int32_t		qmr_response_queue_base_addr_lo;
	u_int32_t		qmr_response_queue_length;
	u_int32_t		qmr_net_rx_large_buffer_queue_out;
	u_int32_t		qmr_net_rx_large_buffer_queue_base_addr_hi;
	u_int32_t		qmr_net_rx_large_buffer_queue_base_addr_lo;
	u_int32_t		qmr_net_rx_large_buffer_queue_length;
	u_int32_t		qmr_net_rx_large_buffer_length;
	u_int32_t		qmr_net_rx_small_buffer_queue_out;
	u_int32_t		qmr_net_rx_small_buffer_queue_base_addr_hi;
	u_int32_t		qmr_net_rx_small_buffer_queue_base_addr_lo;
	u_int32_t		qmr_net_rx_small_buffer_queue_length;
	u_int32_t		qmr_net_rx_small_buffer_length;
	u_int32_t		qmr_res[10];
} __packed;

struct qli_ram_regs {
	u_int32_t		qrr_buflet_size;
	u_int32_t		qrr_buflet_max_count;
	u_int32_t		qrr_buflet_curr_count;
	u_int32_t		qrr_buflet_pause_threshold_count;
	u_int32_t		qrr_buflet_tcp_win_threshold_hi;
	u_int32_t		qrr_buflet_tcp_win_threshold_lo;
	u_int32_t		qrr_ip_hash_table_base_addr;
	u_int32_t		qrr_ip_hash_table_size;
	u_int32_t		qrr_tcp_hash_table_base_addr;
	u_int32_t		qrr_tcp_hash_table_size;
	u_int32_t		qrr_ncb_area_base_addr;
	u_int32_t		qrr_ncb_max_count;
	u_int32_t		qrr_ncb_curr_count;
	u_int32_t		qrr_drb_area_base_addr;
	u_int32_t		qrr_drb_max_count;
	u_int32_t		qrr_drb_curr_count;
	u_int32_t		qrr_res[28];
} __packed;

struct qli_stat_regs{
	u_int32_t		qsr_mac_tx_frame_count;
	u_int32_t		qsr_mac_tx_byte_count;
	u_int32_t		qsr_mac_rx_frame_count;
	u_int32_t		qsr_mac_rx_byte_count;
	u_int32_t		qsr_mac_crc_err_count;
	u_int32_t		qsr_mac_enc_err_count;
	u_int32_t		qsr_mac_rx_length_err_count;
	u_int32_t		qsr_ip_tx_packet_count;
	u_int32_t		qsr_ip_tx_byte_count;
	u_int32_t		qsr_ip_tx_fragment_count;
	u_int32_t		qsr_ip_rx_packet_count;
	u_int32_t		qsr_ip_rx_byte_count;
	u_int32_t		qsr_ip_rx_fragment_count;
	u_int32_t		qsr_ip_datagram_reassembly_count;
	u_int32_t		qsr_ip_v6_rx_packet_count;
	u_int32_t		qsr_ip_err_packet_count;
	u_int32_t		qsr_ip_reassembly_err_count;
	u_int32_t		qsr_tcp_tx_segment_count;
	u_int32_t		qsr_tcp_tx_byte_count;
	u_int32_t		qsr_tcp_rx_segment_count;
	u_int32_t		qsr_tcp_rx_byte_count;
	u_int32_t		qsr_tcp_timer_exp_count;
	u_int32_t		qsr_tcp_rx_ack_count;
	u_int32_t		qsr_tcp_tx_ack_count;
	u_int32_t		qsr_tcp_rx_errOOO_count;
	u_int32_t		qsr_res0;
	u_int32_t		qsr_tcp_rx_window_probe_update_count;
	u_int32_t		qsr_ecc_err_correction_count;
	u_int32_t		qsr_res1[16];
} __packed;

#define QLI_MBOX_SIZE				8
struct qli_reg {
	u_int32_t		qlr_mbox[QLI_MBOX_SIZE];

	u_int32_t		qlr_flash_addr;
	u_int32_t		qlr_flash_data;
	u_int32_t		qlr_ctrl_status;
#define QLI_REG_CTRLSTAT_SCSI_INTR_ENABLE	(0x1<<2) /* 4010 */
#define QLI_REG_CTRLSTAT_SCSI_RESET_INTR	(0x1<<3)
#define QLI_REG_CTRLSTAT_SCSI_COMPL_INTR	(0x1<<4)
#define QLI_REG_CTRLSTAT_SCSI_PROC_INTR		(0x1<<5)
#define QLI_REG_CTRLSTAT_EP_INTR		(0x1<<6)
#define QLI_REG_CTRLSTAT_BOOT_ENABLE		(0x1<<7)
#define QLI_REG_CTRLSTAT_FUNC_MASK		(0x0700) /* 4022 */
#define QLI_REG_CTRLSTAT_NET_INTR_ENABLE	(0x1<<10) /* 4010 */
#define QLI_REG_CTRLSTAT_NET_RESET_INTR		(0x1<<11) /* 4010 */
#define QLI_REG_CTRLSTAT_NET_COMPL_INTR		(0x1<<12) /* 4010 */
#define QLI_REG_CTRLSTAT_FORCE_SOFT_RESET	(0x1<<13) /* 4022 */
#define QLI_REG_CTRLSTAT_FATAL_ERROR		(0x1<<14)
#define QLI_REG_CTRLSTAT_SOFT_RESET		(0x1<<15)

	union {
		struct {
			u_int32_t	q10_nvram;
			u_int32_t 	q10_res[2];
		} __packed isp4010;
		struct {
			u_int32_t	q22_intr_mask;
#define QLI_REG_CTRLSTAT_SCSI_INTR_ENABLE_4022	(0x1<<2) /* 4022 */
			u_int32_t	q22_nvram;
			u_int32_t	q22_sem;
		} __packed isp4022;
	} u1;

	
	u_int32_t		qlr_producer;
	u_int32_t		qlr_consumer;

	u_int32_t		qlr_res[2];
	u_int32_t		qlr_amc;
	u_int32_t		qlr_amd;

	union {
		struct {
			u_int32_t	q10_ext_hw_conf;
			u_int32_t	q10_flow_ctrl;
			u_int32_t	q10_port_ctrl;
			u_int32_t	q10_port_status;
			u_int32_t	q10_res1[8];
			u_int32_t	q10_req_q_out;
			u_int32_t	q10_res2[23];
			u_int32_t	q10_gp_out;
			u_int32_t	q10_gp_in;
			u_int32_t	q10_probe_mux_addr;
			u_int32_t	q10_probe_mux_data;
			u_int32_t	q10_res3[3];
			u_int32_t	q10_port_err_stat;
		} __packed isp4010;
		struct {
			union {
				struct qli_port_regs	q22_pr;
				struct qli_mem_regs	q22_mr;
				struct qli_ram_regs	q22_rr;
				struct qli_stat_regs	q22_sr;
				u_int32_t		q22_union[44];
			};

		} __packed isp4022;
	} u2;
} __packed;

#define QLI_PORT_CTRL_INITIALIZED		(0x1<<15) /* hw init done */
#define QLI_PORT_CTRL(s) (s->sc_ql4010 ? \
	&s->sc_reg->u2.isp4010.q10_port_ctrl : \
	&s->sc_reg->u2.isp4022.q22_pr.qpr_port_ctrl)

#define QLI_PORT_STATUS_RESET_INTR		(0x1<<3) /* reset interrupt */
#define QLI_PORT_STATUS(sc) (sc->sc_ql4010 ? \
	&sc->sc_reg->u2.isp4010.q10_port_status : \
	&sc->sc_reg->u2.isp4022.q22_pr.qpr_port_status)

#define QLI_SET_MASK(v)	((v & 0xffff) | (v << 16))
#define QLI_CLR_MASK(v) (0 | (v << 16))

/* semaphores */
#define QLI_SEM_MAX_RETRIES				(3)
#define QLI_SEM_4010_SCSI				(0x2) /* 4010 */

#define QLI_SEMAPHORE(s) (s->sc_ql4010 ? \
	&s->sc_reg->u1.isp4010.q10_nvram : \
	&s->sc_reg->u1.isp4022.q22_sem)

/* nvram sempahore */
#define QLI_SEM_4010_NVRAM_SHIFT			(12)
#define QLI_SEM_4022_NVRAM_SHIFT			(10)
#define QLI_SEM_4010_NVRAM_MASK				(0x30000000)
#define QLI_SEM_4022_NVRAM_MASK				(0x1c000000)
#define QLI_SEM_NVRAM(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_NVRAM_SHIFT: \
	QLI_SEM_4022_NVRAM_SHIFT)
#define QLI_SEM_NVRAM_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_NVRAM_MASK: \
	QLI_SEM_4022_NVRAM_MASK)

/* flash memory sempahore */
#define QLI_SEM_4010_FLASH_SHIFT			(14)
#define QLI_SEM_4022_FLASH_SHIFT			(13)
#define QLI_SEM_4010_FLASH_MASK				(0xc0000000)
#define QLI_SEM_4022_FLASH_MASK				(0xe0000000)
#define QLI_SEM_FLASH(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_FLASH_SHIFT: \
	QLI_SEM_4022_FLASH_SHIFT)
#define QLI_SEM_FLASH_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_FLASH_MASK: \
	QLI_SEM_4022_FLASH_MASK)

/* memory semaphore */
#define QLI_SEM_4010_MEM_SHIFT				(8)
#define QLI_SEM_4022_MEM_SHIFT				(4)
#define QLI_SEM_4010_MEM_MASK				(0x03000000)
#define QLI_SEM_4022_MEM_MASK				(0x00700000)
#define QLI_SEM_MEM(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_MEM_SHIFT: \
	QLI_SEM_4022_MEM_SHIFT)
#define QLI_SEM_MEM_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_MEM_MASK: \
	QLI_SEM_4022_MEM_MASK)

/* gpio & phy semaphore are the same on the 4022 */
#define QLI_SEM_4010_PHY_SHIFT				(10)
#define QLI_SEM_4022_PHY_SHIFT				(7)
#define QLI_SEM_4010_PHY_MASK				(0x0c000000)
#define QLI_SEM_4022_PHY_MASK				(0x03800000)
#define QLI_SEM_PHY(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_PHY_SHIFT: \
	QLI_SEM_4022_PHY_SHIFT)
#define QLI_SEM_PHY_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_PHY_MASK: \
	QLI_SEM_4022_PHY_MASK)

#define QLI_SEM_4010_GPIO_SHIFT				(6)
#define QLI_SEM_4022_GPIO_SHIFT				QLI_SEM_4022_PHY_SHIFT
#define QLI_SEM_4010_GPIO_MASK				(0x00c00000)
#define QLI_SEM_4022_GPIO_MASK				QLI_SEM_4022_PHY_MASK
#define QLI_SEM_GPIO(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_GPIO_SHIFT: \
	QLI_SEM_4022_GPIO_SHIFT)
#define QLI_SEM_GPIO_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_GPIO_MASK: \
	QLI_SEM_4022_GPIO_MASK)

/* global driver semaphore */
#define QLI_SEM_4010_DRIVER_SHIFT			(4)
#define QLI_SEM_4022_DRIVER_SHIFT			(1)
#define QLI_SEM_4010_DRIVER_MASK			(0x00300000)
#define QLI_SEM_4022_DRIVER_MASK			(0x000e0000)
#define QLI_SEM_DRIVER(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_DRIVER_SHIFT: \
	QLI_SEM_4022_DRIVER_SHIFT)
#define QLI_SEM_DRIVER_MASK(s) (s->sc_ql4010 ? \
	QLI_SEM_4010_DRIVER_MASK: \
	QLI_SEM_4022_DRIVER_MASK)

/* mailbox commands */
#define QLI_MBOX_OPC_ABOUT_FIRMWARE			(0x09)
#define QLI_MBOX_OPC_GET_FW_STATUS			(0x1f)
#define QLI_MBOX_OPC_GET_INITIAL_FW_CB			(0x61)
#define QLI_MBOX_OPC_GET_FW_STATE			(0x69)
		/* mbox 1 firmware state */
#define		QLI_MBOX_STATE_READY			(0x0<<0)
#define		QLI_MBOX_STATE_CONFIG_WAIT		(0x1<<0)
#define		QLI_MBOX_STATE_WAIT_AUTOCONNECT		(0x1<<1)
#define		QLI_MBOX_STATE_ERROR			(0x1<<2)
#define		QLI_MBOX_STATE_CONFIGURING_IP		(0x1<<3)
#define		QLI_MBOX_STATE_WAIT_ACTIVATE_PRI_ACB	(0x1<<4)
#define		QLI_MBOX_STATE_WAIT_ACTIVATE_SEC_ACB	(0x1<<5)
		/* mbox 2 chip version */
		/* mbox 3 additional state flags */
#define		QLI_MBOX_ASTATE_COPPER_MEDIA		(0x0<<0)
#define		QLI_MBOX_ASTATE_OPTICAL_MEDIA		(0x1<<0)
#define		QLI_MBOX_ASTATE_DHCPv4_ENABLED		(0x1<<1)
#define		QLI_MBOX_ASTATE_DHCPv4_LEASE_ACQUIRED	(0x1<<2)
#define		QLI_MBOX_ASTATE_DHCPv4_LEASE_EXPIRED	(0x1<<3)
#define		QLI_MBOX_ASTATE_LINK_UP			(0x1<<4)
#define		QLI_MBOX_ASTATE_ISNSv4_SVC_ENABLED	(0x1<<5)
#define		QLI_MBOX_ASTATE_LINK_SPEED_10MBPS	(0x1<<8)
#define		QLI_MBOX_ASTATE_LINK_SPEED_100MBPS	(0x1<<9)
#define		QLI_MBOX_ASTATE_LINK_SPEED_1000MBPS	(0x1<<10)
#define		QLI_MBOX_ASTATE_HALF_DUPLEX		(0x1<<12)
#define		QLI_MBOX_ASTATE_FULL_DUPLEX		(0x1<<13)
#define		QLI_MBOX_ASTATE_FLOW_CTRL_ENABLED	(0x1<<14)
#define		QLI_MBOX_ASTATE_AUTONEG_ENABLED		(0x1<<15)
#define		QLI_MBOX_ASTATE_FW_CTRLS_PORT_LINK	(0x1<<16)
#define		QLI_MBOX_ASTATE_PAUSE_TX_ENABLED	(0x1<<17)
#define		QLI_MBOX_ASTATE_PAUSE_RX_ENABLED	(0x1<<18)
#define		QLI_MBOX_ASTATE_IPV4_PRI_ENABLED	(0x1<<19)
#define		QLI_MBOX_ASTATE_IPV4_SEC_ENABLED	(0x1<<20)
#define		QLI_MBOX_ASTATE_IPV6_PRI_ENABLED	(0x1<<21)
#define		QLI_MBOX_ASTATE_IPV6_SEC_ENABLED	(0x1<<22)
#define		QLI_MBOX_ASTATE_DHCPV6_ENABLED		(0x1<<23)
#define		QLI_MBOX_ASTATE_IPV6_AUTOCONFIG_ENABLED	(0x1<<24)
#define		QLI_MBOX_ASTATE_IPV6_ADDR0_STATE	(0x1<<25)
#define		QLI_MBOX_ASTATE_IPV6_ADDR0_EXPIRED	(0x1<<26)
#define		QLI_MBOX_ASTATE_IPV6_ADDR1_STATE	(0x1<<27)
#define		QLI_MBOX_ASTATE_IPV6_ADDR1_EXPIRED	(0x1<<28)
#define QLI_MBOX_OPC_NOOP				(0xFF)

/* mailbox status */
#define QLI_MBOX_TYPE_SHIFT				(12)
#define QLI_MBOX_COMPLETION_STATUS			(4)
#define QLI_MBOX_STATUS_BUSY				(0x0007)
#define QLI_MBOX_STATUS_INTERMEDIATE_COMPLETION		(0x1000)
#define QLI_MBOX_STATUS_COMMAND_COMPLETE		(0x4000)
#define QLI_MBOX_STATUS_INVALID_COMMAND			(0x4001)
#define QLI_MBOX_STATUS_HOST_INTERFACE_ERROR		(0x4002)
#define QLI_MBOX_STATUS_TEST_FAILED			(0x4003)
#define QLI_MBOX_STATUS_COMMAND_ERROR			(0x4005)
#define QLI_MBOX_STATUS_COMMAND_PARAMETER_ERROR		(0x4006)
#define QLI_MBOX_STATUS_TARGET_MODE_INIT_FAIL		(0x4007)
#define QLI_MBOX_STATUS_INITIATOR_MODE_INIT_FAIL	(0x4008)

/* async events */
#define QLI_MBOX_ASYNC_EVENT_STATUS			(8)
#define QLI_MBOX_AES_SYSTEM_ERROR			(0x8002)
#define QLI_MBOX_AES_REQUEST_TRANSFER_ERROR		(0x8003)
#define QLI_MBOX_AES_RESPONSE_TRANSFER_ERROR		(0x8004)
#define QLI_MBOX_AES_PROTOCOL_STATISTIC_ALARM		(0x8005)
#define QLI_MBOX_AES_SCSI_COMMAND_PDU_REJECTED		(0x8006)
#define QLI_MBOX_AES_LINK_UP				(0x8010)
#define QLI_MBOX_AES_LINK_DOWN				(0x8011)
#define QLI_MBOX_AES_DATABASE_CHANGED			(0x8014)

/* external hardware config */
#define QLI_EXT_HW_CFG(s) (s->sc_ql4010 ? \
	&s->sc_reg->u2.isp4010.q10_ext_hw_conf : \
	&s->sc_reg->u2.isp4022.q22_pr.qpr_ext_hw_conf)

#define QLI_EXT_HW_CFG_DEFAULT_QL4010			(0x1912)
#define QLI_EXT_HW_CFG_DEFAULT_QL4022			(0x0023)

#define		QLI_EXT_HW_CFG_IGNORE_SHRINK_TCP_WINDOW	(0x1<<0)
#define		QLI_EXT_HW_CFG_SDRAM_PROTECTION_NONE	(0x00)
#define		QLI_EXT_HW_CFG_SDRAM_PROTECTION_BYTE	(0x02)
#define		QLI_EXT_HW_CFG_SDRAM_PROTECTION_ECC	(0x04)
#define		QLI_EXT_HW_CFG_SDRAM_PROTECTION_ECC2	(0x06)
#define		QLI_EXT_HW_CFG_BANKS			(0x1<<3)
#define		QLI_EXT_HW_CFG_CHIP_WIDTH		(0x1<<4)
#define		QLI_EXT_HW_CFG_CHIP_SIZE_64M		(0x00)
#define		QLI_EXT_HW_CFG_CHIP_SIZE_256M		(0x20)
#define		QLI_EXT_HW_CFG_CHIP_SIZE_512M		(0x40)
#define		QLI_EXT_HW_CFG_CHIP_SIZE_1G		(0x60)
#define		QLI_EXT_HW_CFG_PARITY_DISABLE		(0x1<<7)
#define		QLI_EXT_HW_CFG_EXTERNAL_MEM_TYPE	(0x1<<8)
#define		QLI_EXT_HW_CFG_FLASH_BIOS_WRT_ENABLE	(0x1<<9)
#define		QLI_EXT_HW_CFG_FLASH_UPPER_BANK_SELECT	(0x1<<10)
#define		QLI_EXT_HW_CFG_WRITE_BURST_9MA		(0x0000)
#define		QLI_EXT_HW_CFG_WRITE_BURST_15MA		(0x0800)
#define		QLI_EXT_HW_CFG_WRITE_BURST_18MA		(0x1000)
#define		QLI_EXT_HW_CFG_WRITE_BURST_24MA		(0x1800)
#define		QLI_EXT_HW_CFG_DDR_DRIVE_STRENGTH_9MA	(0x0000)
#define		QLI_EXT_HW_CFG_DDR_DRIVE_STRENGTH_15MA	(0x2000)
#define		QLI_EXT_HW_CFG_DDR_DRIVE_STRENGTH_18MA	(0x4000)
#define		QLI_EXT_HW_CFG_DDR_DRIVE_STRENGTH_24MA	(0x6000)

/* nvram */
#define QLI_NVRAM_MASK					(0xf<<16)
#define QLI_NVRAM(s) (s->sc_ql4010 ? \
	&s->sc_reg->u1.isp4010.q10_nvram : \
	&s->sc_reg->u1.isp4022.q22_nvram)

#define QLI_NVRAM_CLOCK					(0x1<<0)
#define QLI_NVRAM_SELECT				(0x1<<1)
#define QLI_NVRAM_DATA_OUT				(0x1<<2)
#define QLI_NVRAM_DATA_IN				(0x1<<3)

#define QLI_NVRAM_SIZE_4010				(0x100)
#define QLI_NVRAM_SIZE_4022				(0x400)
#define QLI_NVRAM_SIZE(s) (s->sc_ql4010 ? \
	QLI_NVRAM_SIZE_4010 : QLI_NVRAM_SIZE_4022)

#define QLI_NVRAM_NUM_CMD_BITS				(0x2)
#define QLI_NVRAM_CMD_READ				(0x2)

#define QLI_NVRAM_NUM_ADDR_BITS_4010			(0x8)
#define QLI_NVRAM_NUM_ADDR_BITS_4022			(0xa)
#define QLI_NVRAM_NUM_ADDR_BITS(s) (s->sc_ql4010 ? \
	QLI_NVRAM_NUM_ADDR_BITS_4010 : \
	QLI_NVRAM_NUM_ADDR_BITS_4022)

#define QLI_NVRAM_NUM_DATA_BITS				(0x10)

#define QLI_NVRAM_EXT_HW_CFG_4010			(0xc)
#define QLI_NVRAM_EXT_HW_CFG_4022			(0x14)
#define QLI_NVRAM_EXT_HW_CFG(s) (s->sc_ql4010 ? \
	QLI_NVRAM_EXT_HW_CFG_4010 : \
	QLI_NVRAM_EXT_HW_CFG_4022)

/* firmware control block */
#define QLI_FW_CTRL_BLK_SIZE				(0x400) /* 1k */
struct qli_cb {
	u_int8_t		qcb_version;
#define QLI_QCB_VER_NO_ADDTIIONAL_INFO			(0x00)
#define QLI_QCB_VER_MIN					(0x01)
#define QLI_QCB_VER_MAX					(0x02)
	u_int8_t		qcb_ctrl;
#define QLI_QCB_CTRL_NEW_CONN_DISABLE			(0x02)
#define QLI_QCB_CTRL_SECONDARY_ACB			(0x01)
	u_int16_t		qcb_fw_options;
#define QLI_QCB_FWOPT_HEARTBEAT_ENABLE			(0x1000)
#define QLI_QCB_FWOPT_MARKER_DISABLE			(0x0400)
#define QLI_QCB_FWOPT_PROT_STAT_ALARM_DISABLE		(0x0200)
#define QLI_QCB_FWOPT_TARGET_ACCEPT_AEN_ENABLE		(0x0100)
#define QLI_QCB_FWOPT_ACCESS_CTRL_ENABLE		(0x0080)
#define QLI_QCB_FWOPT_SESSION_MODE			(0x0040)
#define QLI_QCB_FWOPT_INITIATOR_MODE			(0x0020)
#define QLI_QCB_FWOPT_TARGET_MODE			(0x0010)
#define QLI_QCB_FWOPT_FAST_POSTING			(0x0008)
#define QLI_QCB_FWOPT_AUTO_TARGET_INFO_DISABLE		(0x0004)
#define QLI_QCB_FWOPT_SENSE_BUFFER_DATA_ENABLE		(0x0002)
	u_int16_t		qcb_exec_throttle;
	u_int8_t		qcb_zio_count;
	u_int8_t		qcb_res0;
	u_int16_t		qcb_max_eth_payload;
	u_int16_t		qcb_add_fw_options;
#define QLI_QCB_ADDFWOPT_AUTOCONNECT_DISABLE		(0x0002)
#define QLI_QCB_ADDFWOPT_SUSPEND_ON_FW_ERROR		(0x0001)
	u_int8_t		qcb_heartbeat_intr;
	u_int8_t		qcb_instance_nr;
	u_int16_t		qcb_res1;
	u_int16_t		qcb_req_q_cons_idx;  /* 4010 */
	u_int16_t		qcb_comp_q_prod_idx; /* 4010 */
	u_int16_t		qcb_req_q_len;
	u_int16_t		qcb_comp_q_len;
	u_int32_t		qcb_req_q_addr_lo;
	u_int32_t		qcb_req_q_addr_hi;
	u_int32_t		qcb_comp_q_addr_lo;
	u_int32_t		qcb_comp_q_addr_hi;
	u_int32_t		qcb_shadow_reg_addr_lo;
	u_int32_t		qcb_shadow_reg_addr_hi;
	u_int16_t		qcb_iscsi_options;
#define QLI_QCB_ISCSIOPTS_RECV_MARKER_ENABLE		(0x8000)
#define QLI_QCB_ISCSIOPTS_SEND_MARKER_ENABLE		(0x4000)
#define QLI_QCB_ISCSIOPTS_HDR_DIGEST_ENABLE		(0x2000)
#define QLI_QCB_ISCSIOPTS_DATA_DIGEST_ENABLE		(0x1000)
#define QLI_QCB_ISCSIOPTS_IMMEDIATE_DATA_ENABLE		(0x0800)
#define QLI_QCB_ISCSIOPTS_INITIAL_R2T_ENABLE		(0x0400)
#define QLI_QCB_ISCSIOPTS_DATA_SEQ_IN_ORDER		(0x0200)
#define QLI_QCB_ISCSIOPTS_DATA_PDU_IN_ORDER		(0x0100)
#define QLI_QCB_ISCSIOPTS_CHAP_AUTH_ENABLE		(0x0080)
#define QLI_QCB_ISCSIOPTS_SNACK_REQ_ENABLE		(0x0040)
#define QLI_QCB_ISCSIOPTS_DISCOVERY_LOGOUT_ENABLE	(0x0020)
#define QLI_QCB_ISCSIOPTS_BIDIR_CHAP_ENABLE		(0x0010)
	u_int16_t		qcb_tcp_options;
#define QLI_QCB_TCPOPTS_ISNS_ENABLE			(0x4000)
#define QLI_QCB_TCPOPTS_SLP_USE_DA_ENABLE		(0x2000)
#define QLI_QCB_TCPOPTS_AUTO_DISCOVERY_ENABLE		(0x1000)
#define QLI_QCB_TCPOPTS_SLP_UA_ENABLE			(0x0800)
#define QLI_QCB_TCPOPTS_SLP_SA_ENABLE			(0x0400)
#define QLI_QCB_TCPOPTS_DHCP_ENABLE			(0x0200)
#define QLI_QCB_TCPOPTS_GET_DNS_VIA_DHCP_ENABLE		(0x0100)
#define QLI_QCB_TCPOPTS_GET_SLP_VIA_DHCP_ENABLE		(0x0080)
#define QLI_QCB_TCPOPTS_LEARN_ISNS_IPADDR_ENABLE	(0x0040)
#define QLI_QCB_TCPOPTS_NAGLE_DISABLE			(0x0020)
#define QLI_QCB_TCPOPTS_TIMER_SCALE_MASK		(0x000e)
#define QLI_QCB_TCPOPTS_TIME_STAMP_ENABLE		(0x0001)
	u_int16_t		qcb_ip_options;
#define QLI_QCB_IPOPTS_IPV4_ENABLE			(0x8000)
#define QLI_QCB_IPOPTS_IPV4_TOS_ENABLE			(0x4000)
#define QLI_QCB_IPOPTS_VLAN_TAGGING_ENABLE		(0x2000)
#define QLI_QCB_IPOPTS_GRAT_ARP_ENABLE			(0x1000)
#define QLI_QCB_IPOPTS_DHCP_USE_ALT_CLIENT_ID		(0x0800)
#define QLI_QCB_IPOPTS_DHCP_REQUIRE_VENDOR_ID		(0x0400)
#define QLI_QCB_IPOPTS_DHCP_USE_VENDOR_ID		(0x0200)
#define QLI_QCB_IPOPTS_LEARN_IQN			(0x0100)
#define QLI_QCB_IPOPTS_FRAG_DISABLE			(0x0010)
#define QLI_QCB_IPOPTS_INCOMMING_FORWARDING_ENABLE	(0x0008)
#define QLI_QCB_IPOPTS_ARP_REDIRECT_ENABLE		(0x0004)
#define QLI_QCB_IPOPTS_PAUSE_FRAME_ENABLE		(0x0002)
#define QLI_QCB_IPOPTS_IPADDR_VALID			(0x0001)
	u_int16_t		qcb_max_pdu_size;
	u_int8_t		qcb_tos;
	u_int8_t		qcb_ttl;
	u_int8_t		qcb_acb_version;
#define QLI_QCB_ACBVER_NOT_SUPPORTED			(0x00)
#define QLI_QCB_ACBVER_SUPPORTED			(0x02)
	u_int8_t		qcb_res2;
	u_int16_t		qcb_def_timeout;
	u_int16_t		qcb_first_burst_size;
	u_int16_t		qcb_def_time_to_wait;
	u_int16_t		qcb_def_time_to_retain;
	u_int16_t		qcb_max_out_r2t;
	u_int16_t		qcb_keep_alive_timeout;
	u_int16_t		qcb_port;
	u_int16_t		max_burst_size;
	u_int32_t		qcb_res3;
	u_int32_t		qcb_ip_addr;
	u_int16_t		qcb_vlan_tag_ctrl;
	u_int8_t		qcb_ip_addr_state;
	u_int8_t		qcb_ip_cache_id;
	u_int8_t		qcb_res4[8];
	u_int32_t		qcb_subnet_mask;
	u_int8_t		qcb_res5[12];
	u_int32_t		qcb_gateway_addr;
	u_int8_t		qcb_res6[12];
	u_int32_t		qcb_pri_dns_addr;
	u_int32_t		qcb_sec_dns_addr;
	u_int16_t		qcb_min_eport;
	u_int16_t		qcb_max_eport;
	u_int8_t		qcb_res7[4];
	u_int8_t		qcb_iscsi_alias[32];
	u_int8_t		qcb_res8[24];
	u_int8_t		qcb_abort_timer;
	u_int8_t		qcb_tcp_win_scale_factor;
	u_int8_t		qcb_res9[10];
	u_int8_t		qcb_dhcp_vendor_id_len;
	u_int8_t		qcb_dhcp_vendor_id[11];
	u_int32_t		qcb_isns_addr;
	u_int16_t		qcb_isns_port;
	u_int8_t		qcb_res10[14];
	u_int8_t		qcb_dhcp_client_id_len;
	u_int8_t		qcb_dhcp_client_id[11];
	u_int8_t		qcb_iscsi_name[224];
	u_int8_t		qcb_res11[32];
	u_int32_t		qcb_cookie;
#define QLI_QCB_COOKIE				(0x11bead5a)
	/* ip v6 section */
	u_int16_t		qcb_ipv6_port;
	u_int16_t		qcb_ipv6_options;
#define QLI_QCB_IPV6OPTS_IPV6_ENABLE		(0x8000)
#define QLI_QCB_IPV6OPTS_VLAN_TAGGING_ENABLE	(0x2000)
#define QLI_QCB_IPV6OPTS_GRAT_NEIGHBOR_ENABLE	(0x1000)
#define QLI_QCB_IPV6OPTS_INBOUND_FORW_ENABLE	(0x0008)
	u_int16_t		qcb_ipv6_add_options;
#define QLI_QCB_IPV6AOPTS_NEIGHB_DISC_ENABLE	(0x0002)
#define QLI_QCB_IPV6AOPTS_AUTOCFG_LINK_ENABLE	(0x0001)
	u_int16_t		qcb_ipv6_tcp_options;
#define QLI_QCB_IPV6TCPOPTS_DELAYED_ACK_DISABLE	(0x8000)
#define QLI_QCB_IPV6TCPOPTS_ISNS_ENABLE		(0x4000)
#define QLI_QCB_IPV6TCPOPTS_TCP_WINDOW_SCALE	(0x0400)
#define QLI_QCB_IPV6TCPOPTS_NAGLE_DISABLE	(0x0020)
#define QLI_QCB_IPV6TCPOPTS_TCP_WIN_SCALE_DISA	(0x0010)
#define QLI_QCB_IPV6TCPOPTS_TIMER_SCALE_MASK	(0x000e)
#define QLI_QCB_IPV6TCPOPTS_TIME_STAMP_ENABLE	(0x0001)
	u_int8_t		qcb_ipv6_tcp_recv_scale;
	u_int8_t		qcb_ipv6_flow_label[3];
	u_int8_t		qcb_ipv6_def_router_addr[16];
	u_int8_t		qcb_ipv6_vlan_tci[2];
	u_int8_t		qcb_ipv6_link_local_addr_state;;
#define QLI_QCB_IPV6_LLAS_UNCONFIGURED		(0x00)
#define QLI_QCB_IPV6_LLAS_INVALID		(0x01)
#define QLI_QCB_IPV6_LLAS_ACQUIRING		(0x02)
#define QLI_QCB_IPV6_LLAS_TENTATIVE		(0x03)
#define QLI_QCB_IPV6_LLAS_DEPRECATED		(0x04)
#define QLI_QCB_IPV6_LLAS_PREFERRED		(0x05)
#define QLI_QCB_IPV6_LLAS_DISABLING		(0x06)
	u_int8_t		qcb_ipv6_addr0_state;
	u_int8_t		qcb_ipv6_addr1_state;
	u_int8_t		qcb_ipv6_def_router_state;
	u_int8_t		qcb_ipv6_traf_class;
	u_int8_t		qcb_ipv6_hop_limit;
	u_int8_t		qcb_ipv6_iface_id[8];
	u_int8_t		qcb_ipv6_addr0[16];
	u_int8_t		qcb_ipv6_addr1[16];
	u_int32_t		qcb_ipv6_nd_reachable_time;
	u_int32_t		qcb_ipv6_nd_retransmit_timer;
	u_int32_t		qcb_ipv6_nd_stale_timeout;
	u_int8_t		qcb_ipv6_dup_addr_count;
	u_int8_t		qcb_ipv6_cache_id;
	u_int8_t		qcb_ipv6_res0[2];
	u_int8_t		qcb_ipv6_isns_addr1[16];
	u_int8_t		qcb_ipv6_router_ad_lnk_mtu[4];
	u_int8_t		qcb_ipv6_res1[140];
} __packed;

/* queues */
#define QLI_REQUESTQ_DEPTH			(1024)
#define QLI_QUEUE_SIZE				(64)
#define QLI_RESPONSEQ_DEPTH			(64)

struct qli_queue_entry {
	u_int8_t		qqe_data[60];
	u_int32_t		qqe_signature;
} __packed;

/* shadow regs */
struct qli_shadow_regs {
	u_int32_t		qsr_req_q;
	u_int32_t		qsr_resp_q;
} __packed;
