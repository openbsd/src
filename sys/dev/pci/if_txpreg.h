/*	$OpenBSD: if_txpreg.h,v 1.1 2001/04/08 02:16:52 jason Exp $ */

/*
 * Copyright (c) 2001 Aaron Campbell <aaron@monkey.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Aaron Campbell.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define TXP_INTR	TXP_INT_STATUS_REGISTER

/*
 * Typhoon registers.
 */
#define TXP_SOFT_RESET_REGISTER		0x00
#define TXP_INT_STATUS_REGISTER		0x04
#define TXP_INT_ENABLE_REGISTER		0x08
#define TXP_INT_MASK_REGISTER		0x0C
#define TXP_SELF_INT_REGISTER		0x10
#define TXP_HOST2ARM_COMM_7_REGISTER	0x14
#define TXP_HOST2ARM_COMM_6_REGISTER	0x18
#define TXP_HOST2ARM_COMM_5_REGISTER	0x1C
#define TXP_HOST2ARM_COMM_4_REGISTER	0x20
#define TXP_HOST2ARM_COMM_3_REGISTER	0x24
#define TXP_HOST2ARM_COMM_2_REGISTER	0x28
#define TXP_HOST2ARM_COMM_1_REGISTER	0x2C
#define TXP_HOST2ARM_COMM_0_REGISTER	0x30
#define TXP_ARM2HOST_COMM_3_REGISTER	0x34
#define TXP_ARM2HOST_COMM_2_REGISTER	0x38
#define TXP_ARM2HOST_COMM_1_REGISTER	0x3C
#define TXP_ARM2HOST_COMM_0_REGISTER	0x40

/*
 * Typhoon commands.
 */
#define TXP_CMD_GLOBAL_RESET			0x00
#define TXP_CMD_TX_ENABLE			0x01
#define TXP_CMD_TX_DISABLE			0x02
#define TXP_CMD_RX_ENABLE			0x03
#define TXP_CMD_RX_DISABLE			0x04
#define TXP_CMD_RX_FILTER_WRITE			0x05
#define TXP_CMD_RX_FILTER_READ			0x06
#define TXP_CMD_READ_STATISTICS			0x07
#define TXP_CMD_CYCLE_STATISTICS		0x08
#define TXP_CMD_ERROR_READ			0x09
#define TXP_CMD_MEMORY_READ			0x0a
#define TXP_CMD_MEMORY_WRITE_SINGLE		0x0b
#define TXP_CMD_VARIABLE_SECTION_READ		0x0c
#define TXP_CMD_VARIABLE_SECTION_WRITE		0x0d
#define TXP_CMD_STATIC_SECTION_READ		0x0e
#define TXP_CMD_STATIC_SECTION_WRITE		0x0f
#define TXP_CMD_IMAGE_SECTION_PROGRAM		0x10
#define TXP_CMD_NVRAM_PAGE_READ			0x11
#define TXP_CMD_NVRAM_PAGE_WRITE		0x12
#define TXP_CMD_XCVR_SELECT			0x13
#define TXP_CMD_TEST_MUX			0x14
#define TXP_CMD_PHYLOOPBACK_ENABLE		0x15
#define TXP_CMD_PHYLOOPBACK_DISABLE		0x16
#define TXP_CMD_MAC_CONTROL_READ		0x17
#define TXP_CMD_MAC_CONTROL_WRITE		0x18
#define TXP_CMD_MAX_PKT_SIZE_READ		0x19
#define TXP_CMD_MAX_PKT_SIZE_WRITE		0x1a
#define TXP_CMD_MEDIA_STATUS_READ		0x1b
#define TXP_CMD_MEDIA_STATUS_WRITE		0x1c
#define TXP_CMD_NETWORK_DIAGS_READ		0x1d
#define TXP_CMD_NETWORK_DIAGS_WRITE		0x1e
#define TXP_CMD_POWER_MGMT_EVENT_READ		0x1f
#define TXP_CMD_POWER_MGMT_EVENT_WRITE		0x20
#define TXP_CMD_VARIABLE_PARAMETER_READ		0x21
#define TXP_CMD_VARIABLE_PARAMETER_WRITE	0x22
#define TXP_CMD_GOTO_SLEEP			0x23
#define TXP_CMD_FIREWALL_CONTROL		0x24
#define TXP_CMD_MCAST_HASH_MASK_WRITE		0x25
#define TXP_CMD_STATION_ADDRESS_WRITE		0x26
#define TXP_CMD_STATION_ADDRESS_READ		0x27
#define TXP_CMD_STATION_MASK_WRITE		0x28
#define TXP_CMD_STATION_MASK_READ		0x29
#define TXP_CMD_VLAN_ETHER_TYPE_READ		0x2a
#define TXP_CMD_VLAN_ETHER_TYPE_WRITE		0x2b
#define TXP_CMD_VLAN_MASK_READ			0x2c
#define TXP_CMD_VLAN_MASK_WRITE			0x2d
#define TXP_CMD_BCAST_THROTTLE_WRITE		0x2e
#define TXP_CMD_BCAST_THROTTLE_READ		0x2f
#define TXP_CMD_DHCP_PREVENT_WRITE		0x30
#define TXP_CMD_DHCP_PREVENT_READ		0x31
#define TXP_CMD_RECV_BUFFER_CONTROL		0x32
#define TXP_CMD_SOFTWARE_RESET			0x33
#define TXP_CMD_CREATE_SA			0x34
#define TXP_CMD_DELETE_SA			0x35
#define TXP_CMD_ENABLE_RX_IP_OPTION		0x36
#define TXP_CMD_RANDOM_NUMBER_CONTROL		0x37
#define TXP_CMD_RANDOM_NUMBER_READ		0x38
#define TXP_CMD_MATRIX_TABLE_MODE_WRITE		0x39
#define TXP_CMD_MATRIX_DETAIL_READ		0x3a
#define TXP_CMD_FILTER_ARRAY_READ		0x3b
#define TXP_CMD_FILTER_DETAIL_READ		0x3c
#define TXP_CMD_FILTER_TABLE_MODE_WRITE		0x3d
#define TXP_CMD_FILTER_TCL_WRITE		0x3e
#define TXP_CMD_FILTER_TBL_READ			0x3f
#define TXP_CMD_FILTER_DEFINE			0x45
#define TXP_CMD_ADD_WAKEUP_PKT			0x46
#define TXP_CMD_ADD_SLEEP_PKT			0x47
#define TXP_CMD_ENABLE_SLEEP_EVENTS		0x48
#define TXP_CMD_ENABLE_WAKEUP_EVENTS		0x49
#define TXP_CMD_GET_IP_ADDRESS			0x4a
#define TXP_CMD_READ_PCI_REG			0x4c
#define TXP_CMD_WRITE_PCI_REG			0x4d
#define TXP_CMD_OFFLOAD_WRITE			0x4f
#define TXP_CMD_HELLO_RESPONSE			0x57
#define TXP_CMD_ENABLE_RX_FILTER		0x58
#define TXP_CMD_RX_FILTER_CAPABILITY		0x59
#define TXP_CMD_HALT				0x5d
#define TXP_CMD_INVALID				0xffff

#define TXP_FRAGMENT		0x0000
#define TXP_TXFRAME		0x0001
#define TXP_COMMAND		0x0002
#define TXP_OPTION		0x0003
#define TXP_RECEIVE		0x0004
#define TXP_RESPONSE		0x0005

#define TXP_TYPE_IPSEC		0x0000
#define TXP_TYPE_TCPSEGMENT	0x0001

#define TXP_PFLAG_NOCRC		0x0000
#define TXP_PFLAG_IPCKSUM	0x0001
#define TXP_PFLAG_TCPCKSUM	0x0002
#define TXP_PFLAG_TCPSEGMENT	0x0004
#define TXP_PFLAG_INSERTVLAN	0x0008
#define TXP_PFLAG_IPSEC		0x0010
#define TXP_PFLAG_PRIORITY	0x0020
#define TXP_PFLAG_UDPCKSUM	0x0040
#define TXP_PFLAG_PADFRAME	0x0080

#define TXP_MISC_FIRSTDESC	0x0000
#define TXP_MISC_LASTDESC	0x0001

#define TXP_ERR_INTERNAL	0x0000
#define TXP_ERR_FIFOUNDERRUN	0x0001
#define TXP_ERR_BADSSD		0x0002
#define TXP_ERR_RUNT		0x0003
#define TXP_ERR_CRC		0x0004
#define TXP_ERR_OVERSIZE	0x0005
#define TXP_ERR_ALIGNMENT	0x0006
#define TXP_ERR_DRIBBLEBIT	0x0007

#define TXP_PROTO_UNKNOWN	0x0000
#define TXP_PROTO_IP		0x0001
#define TXP_PROTO_IPX		0x0002
#define TXP_PROTO_RESERVED	0x0003

#define TXP_STAT_PROTO		0x0001
#define TXP_STAT_VLAN		0x0002
#define TXP_STAT_IPFRAGMENT	0x0004
#define TXP_STAT_IPSEC		0x0008
#define TXP_STAT_IPCKSUMBAD	0x0010
#define TXP_STAT_TCPCKSUMBAD	0x0020
#define TXP_STAT_UDPCKSUMBAD	0x0040
#define TXP_STAT_IPCKSUMGOOD	0x0080
#define TXP_STAT_TCPCKSUMGOOD	0x0100
#define TXP_STAT_UDPCKSUMGOOD	0x0200

struct txp_tx_desc {
	u_int8_t		tx_desctype:3,
				tx_rsvd:5;

	u_int8_t		tx_num;
	u_int16_t		tx_rsvd1;
	u_int32_t		tx_addrlo;
	u_int32_t		tx_addrhi;

	u_int32_t		tx_proc_flags:9,
				tx_proc_rsvd1:3,
				tx_proc_vlanpri:16,
				tx_proc_rsvd2:4;
};

struct txp_rx_desc {
	u_int8_t		rx_desctype:3,
				rx_rcvtype:2,
				rx_rsvdA:1,
				rx_error:1,
				rx_rsvdB:1;

	u_int8_t		rx_num;
	u_int16_t		rx_len;
	u_int32_t		rx_addrlo;
	u_int32_t		rx_addrhi;
	u_int32_t		rx_stat;
	u_int16_t		rx_filter;
	u_int16_t		rx_ipsechash;
	u_int32_t		rx_vlan;
};

struct txp_cmd_desc {
	u_int8_t		cmd_desctype:3,
				cmd_rsvd:3,
				cmd_respond:1,
				cmd_rsvd1:1;

	u_int8_t		cmd_num;
	u_int16_t		cmd_id;
	u_int16_t		cmd_seq;
	u_int16_t		cmd_par1;
	u_int32_t		cmd_par2;
	u_int32_t		cmd_par3;
};

struct txp_frag_desc {
	u_int8_t		frag_desctype:3,
				frag_rsvd:5;

	u_int8_t		frag_rsvd1;
	u_int16_t		frag_num;
	u_int32_t		frag_addrlo;
	u_int32_t		frag_addrhi;
	u_int32_t		frag_rsvd2;
};

struct txp_opt_desc {
	u_int8_t		opt_desctype:3,
				opt_rsvd:1,
				opt_type:4;

	u_int8_t		opt_num;
	u_int16_t		opt_dep1;
	u_int32_t		opt_dep2;
	u_int32_t		opt_dep3;
	u_int32_t		opt_dep4;
};

struct txp_ipsec_desc {
	u_int8_t		ipsec_desctpe:3,
				ipsec_rsvd:1,
				ipsec_type:4;

	u_int8_t		ipsec_num;
	u_int16_t		ipsec_flags;
	u_int16_t		ipsec_ah1;
	u_int16_t		ipsec_esp1;
	u_int16_t		ipsec_ah2;
	u_int16_t		ipsec_esp2;
	u_int32_t		ipsec_rsvd1;
};

struct txp_tcpseg_desc {
	u_int8_t		tcpseg_desctype:3,
				tcpseg_rsvd:1,
				tcpseg_type:4;

	u_int8_t		tcpseg_num;

	u_int16_t		tcpseg_mss:12,
				tcpseg_misc:4;

	u_int32_t		tcpseg_respaddr;
	u_int32_t		tcpseg_txbytes;
	u_int32_t		tcpseg_lss;
};

struct txp_resp_desc {
	u_int8_t		resp_desctype:3,
				resp_rsvd1:3,
				resp_error:1,
				resp_resv2:1;

	u_int8_t		resp_num;
	u_int16_t		resp_cmd;
	u_int16_t		resp_seq;
	u_int16_t		resp_par1;
	u_int32_t		resp_par2;
	u_int32_t		resp_par3;
};


/*
 * TYPHOON status register state
 */
#define	TYPHOON_WAITING_FOR_BOOT		0x00000007
#define	TYPHOON_RUNNING				0x00000009
#define	TYPHOON_WAITING_FOR_HOST_REQUEST	0x0000000D
#define	TYPHOON_WAITING_FOR_SEGMENT		0x00000010
#define	TYPHOON_SLEEPING			0x00000011
#define	TYPHOON_HALTED				0x00000014


/* Random stuff... */
#define TYPHOON_INT_ARM2HOST_COMM_0     0x00000002
#define TYPHOON_BOOTCOMMAND_RUNTIME_IMAGE              0xFD
#define TYPHOON_BOOTCOMMAND_DOWNLOAD_COMPLETE          0xFB
#define TYPHOON_BOOTCOMMAND_SEGMENT_AVAILABLE          0xFC
