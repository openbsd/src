/*	$OpenBSD: if_san_xilinx.h,v 1.4 2004/07/16 15:11:45 alex Exp $	*/

/*-
 * Copyright (c) 2001-2004 Sangoma Technologies (SAN)
 * All rights reserved.  www.sangoma.com
 *
 * This code is written by Nenad Corbic <ncorbic@sangoma.com> for SAN.
 * The code is derived from permitted modifications to software created
 * by Alex Feldman (al.feldman@sangoma.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Sangoma Technologies nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SANGOMA TECHNOLOGIES AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __IF_SAN_XILINX_H
#define __IF_SAN_XILINX_H

#define XILINX_CHIP_CFG_REG		0x40

#define XILINX_MCPU_INTERFACE		0x44
#define XILINX_MCPU_INTERFACE_ADDR	0x46

#define XILINX_GLOBAL_INTER_MASK	0x4C


#define XILINX_HDLC_TX_INTR_PENDING_REG	0x50
#define XILINX_HDLC_RX_INTR_PENDING_REG	0x54

enum {
	WP_FIFO_ERROR_BIT,
	WP_CRC_ERROR_BIT,
	WP_ABORT_ERROR_BIT,
};

#define WP_MAX_FIFO_FRAMES	7

#define XILINX_DMA_TX_INTR_PENDING_REG	0x58
#define XILINX_DMA_RX_INTR_PENDING_REG	0x5C

#define XILINX_TIMESLOT_HDLC_CHAN_REG	0x60

#define AFT_T3_RXTX_ADDR_SELECT_REG	0x60

#define XILINX_CURRENT_TIMESLOT_MASK	0x00001F00
#define XILINX_CURRENT_TIMESLOT_SHIFT   8

#define XILINX_HDLC_CONTROL_REG		0x64
#define XILINX_HDLC_ADDR_REG		0x68

#define XILINX_CONTROL_RAM_ACCESS_BUF	0x6C



#define XILINX_DMA_CONTROL_REG		0x70
#define XILINX_DMA_TX_STATUS_REG	0x74
#define AFT_TE3_TX_WDT_CTRL_REG		0x74
#define XILINX_DMA_RX_STATUS_REG	0x78
#define AFT_TE3_RX_WDT_CTRL_REG		0x78
#define XILINX_DMA_DATA_REG		0x7C

#define AFT_TE3_CRNT_DMA_DESC_ADDR_REG	0x80

#define XILINX_TxDMA_DESCRIPTOR_LO	0x100
#define XILINX_TxDMA_DESCRIPTOR_HI	0x104
#define XILINX_RxDMA_DESCRIPTOR_LO	0x108
#define XILINX_RxDMA_DESCRIPTOR_HI	0x10C


#define INTERFACE_TYPE_T1_E1_BIT	0
#define INTERFACE_TYPE_T3_E3_BIT	0

#define XILINX_RED_LED			1
#define AFT_T3_HDLC_TRANS_MODE		1
#define FRONT_END_FRAME_FLAG_ENABLE_BIT	2
#define AFT_T3_CLOCK_MODE		2
#define SIGNALLING_ENABLE_BIT		3
#define FRONT_END_RESET_BIT		4
#define CHIP_RESET_BIT			5
#define HDLC_CORE_RESET_BIT		6
#define HDLC_CORE_READY_FLAG_BIT	7
#define GLOBAL_INTR_ENABLE_BIT		8
#define ERROR_INTR_ENABLE_BIT		9
#define FRONT_END_INTR_ENABLE_BIT	10

#define CHIP_ERROR_MASK			0x00FF0000

#define AFT_TE3_TX_WDT_INTR_PND	26
#define AFT_TE3_RX_WDT_INTR_PND	27

#define FRONT_END_INTR_FLAG		28
#define SECURITY_STATUS_FLAG		29
#define ERROR_INTR_FLAG			30
#define DMA_INTR_FLAG			31

#define XILINX_GLOBAL_INTER_STATUS	0xD0000000

#define	TIMESLOT_BIT_SHIFT	16
#define TIMESLOT_BIT_MASK	0x001F0000
#define HDLC_LOGIC_CH_BIT_MASK	0x0000001F

#define HDLC_LCH_TIMESLOT_MASK  0x001F001F


#define	HDLC_RX_CHAN_ENABLE_BIT		0
#define	HDLC_RX_FRAME_DATA_BIT		1
#define	HDLC_RC_CHAN_ACTIVE_BIT		2
#define HDLC_RX_FRAME_ERROR_BIT		3
#define HDLC_RX_FRAME_ABORT_BIT		4
#define HDLC_RX_PROT_DISABLE_BIT	16
#define HDLC_RX_ADDR_RECOGN_DIS_BIT	17
#define HDLC_RX_ADDR_FIELD_DISC_BIT	18
#define HDLC_RX_ADDR_SIZE_BIT		19
#define HDLC_RX_BRD_ADDR_MATCH_BIT	20
#define HDLC_RX_FCS_SIZE_BIT		21
#define HDLC_CORE_RX_IDLE_LINE_BIT	22
#define HDLC_CODE_RX_ABORT_LINE_BIT	23
#define HDLC_TX_CHAN_ENABLE_BIT		24
#define HDLC_TX_PROT_DISABLE_BIT	25
#define HDLC_TX_ADDR_INSERTION_BIT	26
#define HDLC_TX_ADDR_SIZE_BIT		27
#define HDLC_TX_FCS_SIZE_BIT		28
#define HDLC_TX_FRAME_ABORT_BIT		29
#define HDLC_TX_STOP_TX_ON_ABORT_BIT	30
#define	HDLC_TX_CHANNEL_ACTIVE_BIT	31


#define CONTROL_RAM_DATA_MASK		0x0000001F


#define HDLC_FIFO_BASE_ADDR_SHIFT	16
#define HDLC_FIFO_BASE_ADDR_MASK	0x1F

#define HDLC_FIFO_SIZE_SHIFT		8
#define HDLC_FIFO_SIZE_MASK		0x1F

#define HDLC_FREE_LOGIC_CH		 31
#define TRANSPARENT_MODE_BIT		 31


#define DMA_SIZE_BIT_SHIFT		0
#define DMA_FIFO_HI_MARK_BIT_SHIFT	4
#define DMA_FIFO_LO_MARK_BIT_SHIFT	8
#define DMA_FIFO_T3_MARK_BIT_SHIFT	8

#define DMA_ACTIVE_CHANNEL_BIT_SHIFT	16
#define DMA_ACTIVE_CHANNEL_BIT_MASK	0xFFE0FFFF

#define DMA_ENGINE_ENABLE_BIT		31

#define DMA_CHAIN_TE3_MASK		0x0000000F

#define TxDMA_LO_PC_ADDR_PTR_BIT_MASK	0xFFFFFFFC
#define TxDMA_LO_ALIGNMENT_BIT_MASK	0x00000003
#define TxDMA_HI_DMA_DATA_LENGTH_MASK	0x000007FF

#define TxDMA_HI_DMA_PCI_ERROR_MASK		0x00007800
#define TxDMA_HI_DMA_PCI_ERROR_M_ABRT		0x00000800
#define TxDMA_HI_DMA_PCI_ERROR_T_ABRT		0x00001000
#define TxDMA_HI_DMA_PCI_ERROR_DS_TOUT		0x00002000
#define TxDMA_HI_DMA_PCI_ERROR_RETRY_TOUT	0x00004000


#define INIT_DMA_FIFO_CMD_BIT		28
#define TxDMA_HI_DMA_FRAME_START_BIT	30
#define TxDMA_HI_DMA_FRAME_END_BIT	29
#define TxDMA_HI_DMA_GO_READY_BIT	31
#define DMA_FIFO_BASE_ADDR_SHIFT	20
#define DMA_FIFO_BASE_ADDR_MASK		0x1F
#define DMA_FIFO_SIZE_SHIFT		15
#define DMA_FIFO_SIZE_MASK		0x1F

#define DMA_FIFO_PARAM_CLEAR_MASK	0xFE007FFF

#define FIFO_32B			0x00
#define FIFO_64B			0x01
#define FIFO_128B			0x03
#define FIFO_256B			0x07
#define FIFO_512B			0x0F
#define FIFO_1024B			0x1F


#define RxDMA_LO_PC_ADDR_PTR_BIT_MASK	0xFFFFFFFC
#define RxDMA_LO_ALIGNMENT_BIT_MASK	0x00000003
#define RxDMA_HI_DMA_DATA_LENGTH_MASK	0x000007FF

#define RxDMA_HI_DMA_PCI_ERROR_MASK		0x00007800
#define RxDMA_HI_DMA_PCI_ERROR_M_ABRT		0x00000800
#define RxDMA_HI_DMA_PCI_ERROR_T_ABRT		0x00001000
#define RxDMA_HI_DMA_PCI_ERROR_DS_TOUT		0x00002000
#define RxDMA_HI_DMA_PCI_ERROR_RETRY_TOUT	0x00004000


#define RxDMA_HI_DMA_COMMAND_BIT_SHIFT	28
#define RxDMA_HI_DMA_FRAME_START_BIT	30
#define RxDMA_HI_DMA_CRC_ERROR_BIT	25
#define RxDMA_HI_DMA_FRAME_ABORT_BIT	26
#define RxDMA_HI_DMA_FRAME_END_BIT	29
#define RxDMA_HI_DMA_GO_READY_BIT	31

#define DMA_HI_TE3_INTR_DISABLE_BIT	27
#define DMA_HI_TE3_NOT_LAST_FRAME_BIT	24

#define AFT_TE3_CRNT_TX_DMA_MASK	0x0000000F
#define AFT_TE3_CRNT_RX_DMA_MASK	0x000000F0
#define AFT_TE3_CRNT_RX_DMA_SHIFT	4

typedef struct xilinx_config
{
	unsigned long xilinx_chip_cfg_reg;
	unsigned long xilinx_dma_control_reg;
} xilinx_config_t;


#define XILINX_DMA_SIZE		10
#define XILINX_DMA_FIFO_UP	8
#define XILINX_DMA_FIFO_LO	8
#define AFT_T3_DMA_FIFO_MARK	8
#define XILINX_DEFLT_ACTIVE_CH  0

#define MAX_XILINX_TX_DMA_SIZE  0xFFFF

#define MIN_WP_PRI_MTU		128
#define DEFAULT_WP_PRI_MTU	1500
#define MAX_WP_PRI_MTU		8188


#define MAX_DATA_SIZE 2000
struct sdla_hdlc_api {
	unsigned int  cmd;
	unsigned short len;
	unsigned char  bar;
	unsigned short offset;
	unsigned char data[MAX_DATA_SIZE];
};

#pragma pack(1)
typedef struct {
	unsigned char	error_flag;
	unsigned short	time_stamp;
	unsigned char	reserved[13];
} api_rx_hdr_t;

typedef struct {
	api_rx_hdr_t	api_rx_hdr;
	unsigned char	data[1];
} api_rx_element_t;

typedef struct {
	unsigned char	attr;
	unsigned char   misc_Tx_bits;
	unsigned char	reserved[14];
} api_tx_hdr_t;

typedef struct {
	api_tx_hdr_t	api_tx_hdr;
	unsigned char	data[1];
} api_tx_element_t;
#pragma pack()

#undef  wan_udphdr_data
#define wan_udphdr_data	wan_udphdr_u.aft.data



#define PMC_CONTROL_REG		0x00


#define PMC_RESET_BIT		0
#define PMC_CLOCK_SELECT	1

#define LED_CONTROL_REG		0x01

#define JP8_VALUE		0x02
#define JP7_VALUE		0x01
#define SW0_VALUE		0x04
#define SW1_VALUE		0x08


#define SECURITY_CPLD_REG	0x09

#define SECURITY_CPLD_MASK	0x03
#define SECURITY_CPLD_SHIFT	0x02

#define SECURITY_1LINE_UNCH	0x00
#define SECURITY_1LINE_CH	0x01
#define SECURITY_2LINE_UNCH	0x02
#define SECURITY_2LINE_CH	0x03



#define WRITE_DEF_SECTOR_DSBL   0x01
#define FRONT_END_TYPE_MASK     0x38

#define BIT_DEV_ADDR_CLEAR	0x600
#define BIT_DEV_ADDR_CPLD	0x200

#define MEMORY_TYPE_SRAM	0x00
#define MEMORY_TYPE_FLASH	0x01
#define MASK_MEMORY_TYPE_SRAM   0x10
#define MASK_MEMORY_TYPE_FLASH  0x20

#define BIT_A18_SECTOR_SA4_SA7  0x20
#define USER_SECTOR_START_ADDR  0x40000

#define MAX_TRACE_QUEUE		100

#define TX_DMA_BUF_INIT		0

#define MAX_TRACE_BUFFER	(MAX_LGTH_UDP_MGNT_PKT -	\
				 sizeof(iphdr_t) -		\
				 sizeof(udphdr_t) -		\
				 sizeof(wan_mgmt_t) -		\
				 sizeof(wan_trace_info_t) -	\
				 sizeof(wan_cmd_t))

enum {
	ROUTER_UP_TIME = 0x50,
	ENABLE_TRACING,
	DISABLE_TRACING,
	GET_TRACE_INFO,
	READ_CODE_VERSION,
	FLUSH_OPERATIONAL_STATS,
	OPERATIONAL_STATS,
	READ_OPERATIONAL_STATS,
	READ_CONFIGURATION,
	COMMS_ERROR_STATS_STRUCT,
	AFT_LINK_STATUS
};

#define UDPMGMT_SIGNATURE		"AFTPIPEA"


typedef struct {
	unsigned char flag;
	unsigned short length;
	unsigned char rsrv0[2];
	unsigned char attr;
	unsigned short tmstamp;
	unsigned char rsrv1[4];
	unsigned long offset;
} aft_trc_el_t;


typedef struct wp_rx_element
{
	unsigned long dma_addr;
	unsigned int reg;
	unsigned int align;
	unsigned char pkt_error;
}wp_rx_element_t;


#if defined(_KERNEL)

static __inline unsigned short xilinx_valid_mtu(unsigned short mtu)
{
	if (mtu <= 128) {
		return 128;
	} else if (mtu <= 256) {
		return 256;
	} else if (mtu <= 512) {
		return 512;
	} else if (mtu <= 1024) {
		return 1024;
	} else if (mtu <= 2048) {
		return 2048;
	} else if (mtu <= 4096) {
		return 4096;
	} else if (mtu <= 8188) {
		return 8188;
	} else {
		return 0;
	}
}

static __inline unsigned short xilinx_dma_buf_bits(unsigned short dma_bufs)
{
	if (dma_bufs < 2) {
		return 0;
	} else if (dma_bufs < 3) {
		return 1;
	} else if (dma_bufs < 5) {
		return 2;
	} else if (dma_bufs < 9) {
		return 3;
	} else if (dma_bufs < 17) {
		return 4;
	} else {
		return 0;
	}
}

#define AFT_TX_TIMEOUT 25
#define AFT_RX_TIMEOUT 10
#define AFT_MAX_WTD_TIMEOUT 250

static __inline void aft_reset_rx_watchdog(sdla_t *card)
{
	sdla_bus_write_4(card->hw,AFT_TE3_RX_WDT_CTRL_REG,0);
}

static __inline void aft_enable_rx_watchdog(sdla_t *card, unsigned char timeout)
{
	aft_reset_rx_watchdog(card);
	sdla_bus_write_4(card->hw,AFT_TE3_RX_WDT_CTRL_REG,timeout);
}

static __inline void aft_reset_tx_watchdog(sdla_t *card)
{
	sdla_bus_write_4(card->hw,AFT_TE3_TX_WDT_CTRL_REG,0);
}

static __inline void aft_enable_tx_watchdog(sdla_t *card, unsigned char timeout)
{
	aft_reset_tx_watchdog(card);
	sdla_bus_write_4(card->hw,AFT_TE3_TX_WDT_CTRL_REG,timeout);
}

#endif

#endif
