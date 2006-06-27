/*	$OpenBSD: if_zydreg.h,v 1.3 2006/06/27 03:49:44 deraadt Exp $	*/

/*
 * Copyright (c) 2006 by Florian Stoehr <ich@florian-stoehr.de>
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

/*
 * ZyDAS ZD1211 USB WLAN driver
 */

/*
 * The ZD1211 maps threee register spaces into the overall USB
 * address space of 64K words (16-bit values):
 *
 * - Control register space (32-bit registers)
 * - EEPROM register space (16-bit registers)
 * - Firmware register space (16-bit registers)
 *
 * I'll use the masks and type constants below to handle
 * register filtering in code (require different treatment).
 */

enum zyd_masks {
	ZYD_MASK_DESTRANGE	= 0xF0000, /* Mask destination range */
	ZYD_MASK_ADDR_OFFS	= 0x0FFFF  /* Mask address offset */
};

enum zyd_ranges {
	ZYD_RANGE_USB	= 0x10000, /* Range: USB */
	ZYD_RANGE_CTL	= 0x20000, /* Range: Control */
	ZYD_RANGE_E2P	= 0x40000, /* Range: EEPROM */
	ZYD_RANGE_FW	= 0x80000  /* Range: Firmware */
};

/* Nomalize to uint32_t */
#define ZYD_MASK_NORM	0x000FFFFFUL

/* Mask (detect) destination range */
#define ZYD_GET_RANGE(addr) \
	(ZYD_MASK_NORM & ((uint32_t)(addr) & ZYD_MASK_DESTRANGE))

/* Mask register offset (kick the range off) */
#define ZYD_GET_OFFS(addr) \
	(ZYD_MASK_NORM & ((uint32_t)(addr) & ZYD_MASK_ADDR_OFFS))

/* Combine range and offset */
#define ZYD_BUILDCOMBO(range, offs) \
	(ZYD_GET_RANGE(range) | ZYD_GET_OFFS(offs))

/* Quick set macros for ranges */
#define ZYD_REG_USB(offs)	ZYD_BUILDCOMBO(ZYD_RANGE_USB, offs) /* 16-bit addr */
#define ZYD_REG_CTL(offs)	ZYD_BUILDCOMBO(ZYD_RANGE_CTL, offs) /*  8-bit addr */
#define ZYD_REG_E2P(offs)	ZYD_BUILDCOMBO(ZYD_RANGE_E2P, offs) /* 16-bit addr */
#define ZYD_REG_FW(offs)	ZYD_BUILDCOMBO(ZYD_RANGE_FW, offs)  /* 16-bit addr */

#define ZYD_CR_GPI_EN			ZYD_REG_CTL(0x418)
#define ZYD_CR_RADIO_PD			ZYD_REG_CTL(0x42C)
#define ZYD_CR_RF2948_PD		ZYD_REG_CTL(0x42C)
#define ZYD_CR_ENABLE_PS_MANUAL_AGC	ZYD_REG_CTL(0x43C)
#define ZYD_CR_CONFIG_PHILIPS		ZYD_REG_CTL(0x440)
#define ZYD_CR_SA2400_SER_AP		ZYD_REG_CTL(0x444)
#define ZYD_CR_I2C_WRITE		ZYD_REG_CTL(0x444)
#define ZYD_CR_SA2400_SER_RP		ZYD_REG_CTL(0x448)
#define ZYD_CR_RADIO_PE			ZYD_REG_CTL(0x458)
#define ZYD_CR_RST_BUS_MASTER		ZYD_REG_CTL(0x45C)
#define ZYD_CR_RFCFG			ZYD_REG_CTL(0x464)
#define ZYD_CR_HSTSCHG			ZYD_REG_CTL(0x46C)
#define ZYD_CR_PHY_ON			ZYD_REG_CTL(0x474)
#define ZYD_CR_RX_DELAY			ZYD_REG_CTL(0x478)
#define ZYD_CR_RX_PE_DELAY		ZYD_REG_CTL(0x47C)
#define ZYD_CR_GPIO_1			ZYD_REG_CTL(0x490)
#define ZYD_CR_GPIO_2			ZYD_REG_CTL(0x494)
#define ZYD_CR_EnZYD_CRyBufMux		ZYD_REG_CTL(0x4A8)
#define ZYD_CR_PS_CTRL			ZYD_REG_CTL(0x500)
#define ZYD_CR_ADDA_PWR_DWN		ZYD_REG_CTL(0x504)
#define ZYD_CR_ADDA_MBIAS_WARMTIME	ZYD_REG_CTL(0x508)
#define ZYD_CR_INTERRUPT		ZYD_REG_CTL(0x510)
#define ZYD_CR_MAC_PS_STATE		ZYD_REG_CTL(0x50C)

/*
 * Following three values are in time units (1024us)
 * Following condition must be met:
 * atim < tbtt < bcn
 */
#define ZYD_CR_ATIM_WND_PERIOD	ZYD_REG_CTL(0x51C)
#define ZYD_CR_BCN_INTERVAL	ZYD_REG_CTL(0x520)
#define ZYD_CR_PRE_TBTT		ZYD_REG_CTL(0x524)

/*
 * MAC registers
 */
#define ZYD_MAC_MACADDRL	ZYD_REG_CTL(0x610) /* MAC address (low) */
#define ZYD_MAC_MACADDRH	ZYD_REG_CTL(0x614) /* MAC address (high) */
#define ZYD_MAC_BSSADRL		ZYD_REG_CTL(0x618) /* BSS address (low) */
#define ZYD_MAC_BSSADRH		ZYD_REG_CTL(0x61C) /* BSS address (high) */
#define ZYD_MAC_BCNCFG		ZYD_REG_CTL(0x620) /* BCN configuration */
#define ZYD_MAC_GHTBL		ZYD_REG_CTL(0x624) /* Group hash table (low) */
#define ZYD_MAC_GHTBH		ZYD_REG_CTL(0x628) /* Group hash table (high) */
#define ZYD_MAC_RX_TIMEOUT	ZYD_REG_CTL(0x62C) /* Rx timeout value */
#define ZYD_MAC_BASICRATE	ZYD_REG_CTL(0x630) /* Basic rate setting */
#define ZYD_MAC_MANDATORYRATE	ZYD_REG_CTL(0x634) /* Mandatory rate setting */
#define ZYD_MAC_RTSCTSRATE	ZYD_REG_CTL(0x638) /* RTS CTS rate */
#define ZYD_MAC_BACKOFF_PROTECT	ZYD_REG_CTL(0x63C) /* Backoff protection */
#define ZYD_MAC_RX_THRESHOLD	ZYD_REG_CTL(0x640) /* Rx threshold */
#define ZYD_MAC_TX_PE_CONTROL	ZYD_REG_CTL(0x644) /* Tx_PE control */
#define ZYD_MAC_AFTER_PNP	ZYD_REG_CTL(0x648) /* After PnP */
#define ZYD_MAC_RX_PE_DELAY	ZYD_REG_CTL(0x64C) /* Rx_pe delay */
#define ZYD_MAC_RX_ADDR2_L	ZYD_REG_CTL(0x650) /* RX address2 (low)	*/
#define ZYD_MAC_RX_ADDR2_H	ZYD_REG_CTL(0x654) /* RX address2 (high) */
#define ZYD_MAC_SIFS_ACK_TIME	ZYD_REG_CTL(0x658) /* Dynamic SIFS ack time */
#define ZYD_MAC_PHY_DELAY	ZYD_REG_CTL(0x660) /* PHY delay */
#define ZYD_MAC_PHY_DELAY2	ZYD_REG_CTL(0x66C) /* PHY delay */
#define ZYD_MAC_BCNFIFO		ZYD_REG_CTL(0x670) /* Beacon FIFO I/O port */
#define ZYD_MAC_SNIFFER		ZYD_REG_CTL(0x674) /* Sniffer on/off */
#define ZYD_MAC_ENCRYPTION_TYPE ZYD_REG_CTL(0x678) /* Encryption type */
#define ZYD_MAC_RETRY		ZYD_REG_CTL(0x67C) /* Retry time */
#define ZYD_MAC_MISC		ZYD_REG_CTL(0x680) /* Misc */
#define ZYD_MAC_STMACHINESTAT	ZYD_REG_CTL(0x684) /* State machine status */
#define ZYD_MAC_TX_UNDERRUN_CNT	ZYD_REG_CTL(0x688) /* TX underrun counter */
#define ZYD_MAC_STOHOSTSETTING	ZYD_REG_CTL(0x68C) /* Send to host settings */
#define ZYD_MAC_ACK_EXT		ZYD_REG_CTL(0x690) /* Acknowledge extension */
#define ZYD_MAC_BCNFIFOST	ZYD_REG_CTL(0x694) /* BCN FIFO set and status */
#define ZYD_MAC_DIFS_EIFS_SIFS	ZYD_REG_CTL(0x698) /* DIFS, EIFS & SIFS settings */
#define ZYD_MAC_RX_TIMEOUT_CNT	ZYD_REG_CTL(0x69C) /* RX timeout count */
#define ZYD_MAC_RX_TOTAL_FRAME	ZYD_REG_CTL(0x6A0) /* RX total frame count */
#define ZYD_MAC_RX_CRC32_CNT	ZYD_REG_CTL(0x6A4) /* RX CRC32 frame count */
#define ZYD_MAC_RX_CRC16_CNT	ZYD_REG_CTL(0x6A8) /* RX CRC16 frame count */
#define ZYD_MAC_RX_UDEC		ZYD_REG_CTL(0x6AC) /* RX unicast decr. error count */
#define ZYD_MAC_RX_OVERRUN_CNT	ZYD_REG_CTL(0x6B0) /* RX FIFO overrun count */
#define ZYD_MAC_RX_MDEC		ZYD_REG_CTL(0x6BC) /* RX multicast decr. err. cnt. */
#define ZYD_MAC_NAV_TCR		ZYD_REG_CTL(0x6C4) /* NAV timer count read */
#define ZYD_MAC_BACKOFF_ST_RD	ZYD_REG_CTL(0x6C8) /* Backoff status read */
#define ZYD_MAC_DM_RETRY_CNT_RD	ZYD_REG_CTL(0x6CC) /* DM retry count read */
#define ZYD_MAC_RX_ACR		ZYD_REG_CTL(0x6D0) /* RX arbitration count read	*/
#define ZYD_MAC_TX_CCR		ZYD_REG_CTL(0x6D4) /* Tx complete count read */
#define ZYD_MAC_TCB_ADDR	ZYD_REG_CTL(0x6E8) /* Current PCI process TCP addr */
#define ZYD_MAC_RCB_ADDR	ZYD_REG_CTL(0x6EC) /* Next RCB address */
#define ZYD_MAC_CONT_WIN_LIMIT	ZYD_REG_CTL(0x6F0) /* Contention window limit */
#define ZYD_MAC_TX_PKT		ZYD_REG_CTL(0x6F4) /* Tx total packet count read */
#define ZYD_MAC_DL_CTRL		ZYD_REG_CTL(0x6F8) /* Download control */

/*
 * EEPROM registers
 */
#define ZYD_E2P_SUBID		ZYD_REG_E2P(0x00)  /* ? */
#define ZYD_E2P_POD		ZYD_REG_E2P(0x02)  /* RF/PA types */
#define ZYD_E2P_MAC_ADDR_P1	ZYD_REG_E2P(0x04)  /* Part 1 of the MAC address	*/
#define ZYD_E2P_MAC_ADDR_P2	ZYD_REG_E2P(0x06)  /* Part 2 of the MAC address	*/
#define ZYD_E2P_PWR_CAL_VALUE1	ZYD_REG_E2P(0x08)  /* Calibration */
#define ZYD_E2P_PWR_CAL_VALUE2	ZYD_REG_E2P(0x0a)  /* Calibration */
#define ZYD_E2P_PWR_CAL_VALUE3	ZYD_REG_E2P(0x0c)  /* Calibration */
#define ZYD_E2P_PWR_CAL_VALUE4	ZYD_REG_E2P(0x0e)  /* Calibration */
#define ZYD_E2P_PWR_INT_VALUE1	ZYD_REG_E2P(0x10)  /* Calibration */
#define ZYD_E2P_PWR_INT_VALUE2	ZYD_REG_E2P(0x12)  /* Calibration */
#define ZYD_E2P_PWR_INT_VALUE3	ZYD_REG_E2P(0x14)  /* Calibration */
#define ZYD_E2P_PWR_INT_VALUE4	ZYD_REG_E2P(0x16)  /* Calibration */
#define ZYD_E2P_ALLOWED_CHANNEL	ZYD_REG_E2P(0x18)  /* Allowed CH mask, 1 bit each */
#define ZYD_E2P_PHY_REG		ZYD_REG_E2P(0x1a)  /* PHY registers */
#define ZYD_E2P_DEVICE_VER	ZYD_REG_E2P(0x20)  /* Device version */
#define ZYD_E2P_36M_CAL_VALUE1	ZYD_REG_E2P(0x28)  /* Calibration */
#define ZYD_E2P_36M_CAL_VALUE2	ZYD_REG_E2P(0x2a)  /* Calibration */
#define ZYD_E2P_36M_CAL_VALUE3	ZYD_REG_E2P(0x2c)  /* Calibration */
#define ZYD_E2P_36M_CAL_VALUE4	ZYD_REG_E2P(0x2e)  /* Calibration */
#define ZYD_E2P_11A_INT_VALUE1	ZYD_REG_E2P(0x30)  /* Calibration */
#define ZYD_E2P_11A_INT_VALUE2	ZYD_REG_E2P(0x32)  /* Calibration */
#define ZYD_E2P_11A_INT_VALUE3	ZYD_REG_E2P(0x34)  /* Calibration */
#define ZYD_E2P_11A_INT_VALUE4	ZYD_REG_E2P(0x36)  /* Calibration */
#define ZYD_E2P_48M_CAL_VALUE1	ZYD_REG_E2P(0x38)  /* Calibration */
#define ZYD_E2P_48M_CAL_VALUE2	ZYD_REG_E2P(0x3a)  /* Calibration */
#define ZYD_E2P_48M_CAL_VALUE3	ZYD_REG_E2P(0x3c)  /* Calibration */
#define ZYD_E2P_48M_CAL_VALUE4	ZYD_REG_E2P(0x3e)  /* Calibration */
#define ZYD_E2P_48M_INT_VALUE1	ZYD_REG_E2P(0x40)  /* Calibration */
#define ZYD_E2P_48M_INT_VALUE2	ZYD_REG_E2P(0x42)  /* Calibration */
#define ZYD_E2P_48e_INT_VALUE3	ZYD_REG_E2P(0x44)  /* Calibration */
#define ZYD_E2P_48M_INT_VALUE4	ZYD_REG_E2P(0x46)  /* Calibration */
#define ZYD_E2P_54M_CAL_VALUE1	ZYD_REG_E2P(0x48)  /* Calibration */
#define ZYD_E2P_54M_CAL_VALUE2	ZYD_REG_E2P(0x4a)  /* Calibration */
#define ZYD_E2P_54M_CAL_VALUE3	ZYD_REG_E2P(0x4c)  /* Calibration */
#define ZYD_E2P_54M_CAL_VALUE4	ZYD_REG_E2P(0x4e)  /* Calibration */
#define ZYD_E2P_54M_INT_VALUE1	ZYD_REG_E2P(0x50)  /* Calibration */
#define ZYD_E2P_54M_INT_VALUE2	ZYD_REG_E2P(0x52)  /* Calibration */
#define ZYD_E2P_54M_INT_VALUE3	ZYD_REG_E2P(0x54)  /* Calibration */
#define ZYD_E2P_54M_INT_VALUE4	ZYD_REG_E2P(0x56)  /* Calibration */

/*
 * Firmware registers (all 16-bit)
 */
#define ZYD_FW_FIRMWARE_VER	ZYD_REG_FW(0) /* Firmware version */
#define ZYD_FW_USB_SPEED	ZYD_REG_FW(1) /* USB speed (!=0 if highspeed) */
#define ZYD_FW_FIX_TX_RATE	ZYD_REG_FW(2) /* Fixed TX rate */
#define ZYD_FW_LINK_STATUS	ZYD_REG_FW(3) /* LED control? */
#define ZYD_FW_SOFT_RESET	ZYD_REG_FW(4) /* LED control? */
#define ZYD_FW_FLASH_CHK	ZYD_REG_FW(5) /* LED control? */

/*
 * Some addresses
 */
#define ZYD_CTRL_START_ADDR	0x9000 /* Control registers start */
#define ZYD_FIRMWARE_START_ADDR	0xEE00 /* !! NEWER devices only! */
#define ZYD_FIRMWARE_OLD_ADDR	0xEC00 /* Devices prior to 43.30 */
#define ZYD_FIRMWARE_BASE_ADDR	0xEE1D /* Firmware base addr offset */
#define ZYD_E2P_START_HEAD	0xF800 /* EEPROM start (head) */
#define ZYD_E2P_START_ADDR	0xF817 /* EEPROM registers start */

/*
 * Firmware uploader - requests and ids
 */
#define ZYD_FIRMDOWN_REQ	0x40 /* Firmware dl request type */
#define ZYD_FIRMDOWN_ID		0x30 /* Firmware dl ID */
#define ZYD_FIRMSTAT_REQ	0xC0 /* Firmware stat query type */
#define ZYD_FIRMSTAT_ID		0x31 /* Firmware stat query ID */

/*
 * Firmware uploader - status
 */
#define ZYD_FIRMWAREUP_OK	0x01 /* Firmware upload OK */
#define ZYD_FIRMWAREUP_FAILURE	0x80 /* Firmware upload failure */

/*
 * USB stuff
 */
#define ZYD_CONFIG_NO		1 /* USB configuration */
#define ZYD_IFACE_IDX		0 /* Interface index */

/*
 * RFs
 */
#define ZYD_RF_UW2451		0x2
#define ZYD_RF_UCHIP		0x3
#define ZYD_RF_AL2230		0x4
#define ZYD_RF_AL7230B		0x5 /* a,b,g */
#define ZYD_RF_THETA		0x6
#define ZYD_RF_AL2210		0x7
#define ZYD_RF_MAXIM_NEW	0x8
#define ZYD_RF_GCT		0x9
#define ZYD_RF_PV2000		0xa
#define ZYD_RF_RALINK		0xb
#define ZYD_RF_INTERSIL		0xc
#define ZYD_RF_RFMD		0xd
#define ZYD_RF_MAXIM_NEW2	0xe
#define ZYD_RF_PHILIPS		0xf

/*
 * Channal stuff, including domains
 */
#define ZYD_RF_CHANNEL(ch)	[(ch)-1]

#define ZYD_REGDOMAIN_FCC	0x10
#define ZYD_REGDOMAIN_IC	0x20
#define ZYD_REGDOMAIN_ETSI	0x30
#define ZYD_REGDOMAIN_SPAIN	0x31
#define ZYD_REGDOMAIN_FRANCE	0x32
#define ZYD_REGDOMAIN_JAPAN_ADD	0x40
#define ZYD_REGDOMAIN_JAPAN	0x41

/*
 * Register access responses
 */
#define ZYD_CRS_IORDRSP		0x0190 /* Resonse for IORDREQ */
#define ZYD_CRS_MACINTERRUPT	0x0190 /* Interrupt notification */
#define ZYD_CRS_RETRYSTATUS	0x01A0 /* Report retry fail status */

/*
 * Register access commands
 */
#define ZYD_CMD_IOWRREQ		0x0021 /* Request to write register */
#define ZYD_CMD_IORDREQ		0x0022 /* Request to read register */
#define ZYD_CMD_RFCFGREQ	0x0023 /* Write config to RF regs */

/*
 * RF access
 */
#define ZYD_RF_IF_LE		2
#define ZYD_RF_CLK		4
#define ZYD_RF_DATA		8

#define ZYD_RF_REG_BITS		6
#define ZYD_RF_VALUE_BITS	18
#define ZYD_RF_RV_BITS		ZYD_RF_REG_BITS + ZYD_RF_VALUE_BITS

/*
 * Various
 */
#define ZYD_MAX_SSID_LEN	32 /* SSID max length */
#define ZYD_ALLOWED_DEV_VERSION	0x4330 /* We only allow 43.30 devices */
#define ZYD_AP_RX_FILTER	0x0400FEFF
#define ZYD_STA_RX_FILTER	0x0000FFFF

#define ZYD_HWINT_ENABLED	0x004f0000
#define ZYD_HWINT_DISABLED	0

#define ZYD_E2P_PWR_INT_GUARD	8
#define ZYD_E2P_CHANNEL_COUNT	14


/* 1,2,5.5,11,6,12,24 */
#define ZYD_DEFAULT_MAND_RATES	0x150f

/*
 * 0x80 set = PHY controlled by MAC, 00 = controlled by host
 * Affected register is 0x680 (ZYD_MAC_MISC)
 */
#define ZYD_UNLOCK_PHY_REGS	0x0080 /* Disallow PHY write access */

/* 8-bit hardware registers */
#define ZYD_CR0		ZYD_REG_CTL(0x0000)
#define ZYD_CR1		ZYD_REG_CTL(0x0004)
#define ZYD_CR2		ZYD_REG_CTL(0x0008)
#define ZYD_CR3		ZYD_REG_CTL(0x000C)

#define ZYD_CR5		ZYD_REG_CTL(0x0010)
/*	bit 5: if set short preamble used
 *	bit 6: filter band - Japan channel 14 on, else off
 */
#define ZYD_CR6		ZYD_REG_CTL(0x0014)
#define ZYD_CR7		ZYD_REG_CTL(0x0018)
#define ZYD_CR8		ZYD_REG_CTL(0x001C)

#define ZYD_CR4		ZYD_REG_CTL(0x0020)

#define ZYD_CR9		ZYD_REG_CTL(0x0024)
/*	bit 2: antenna switch (together with ZYD_CR10) */
#define ZYD_CR10	ZYD_REG_CTL(0x0028)
/*	bit 1: antenna switch (together with ZYD_CR9)
 *	RF2959 controls with ZYD_CR11 radion on and off
 */
#define ZYD_CR11	ZYD_REG_CTL(0x002C)
/*	bit 6:  TX power control for OFDM
 *	RF2959 controls with ZYD_CR10 radio on and off
 */
#define ZYD_CR12	ZYD_REG_CTL(0x0030)
#define ZYD_CR13	ZYD_REG_CTL(0x0034)
#define ZYD_CR14	ZYD_REG_CTL(0x0038)
#define ZYD_CR15	ZYD_REG_CTL(0x003C)
#define ZYD_CR16	ZYD_REG_CTL(0x0040)
#define ZYD_CR17	ZYD_REG_CTL(0x0044)
#define ZYD_CR18	ZYD_REG_CTL(0x0048)
#define ZYD_CR19	ZYD_REG_CTL(0x004C)
#define ZYD_CR20	ZYD_REG_CTL(0x0050)
#define ZYD_CR21	ZYD_REG_CTL(0x0054)
#define ZYD_CR22	ZYD_REG_CTL(0x0058)
#define ZYD_CR23	ZYD_REG_CTL(0x005C)
#define ZYD_CR24	ZYD_REG_CTL(0x0060) /* CCA threshold */
#define ZYD_CR25	ZYD_REG_CTL(0x0064)
#define ZYD_CR26	ZYD_REG_CTL(0x0068)
#define ZYD_CR27	ZYD_REG_CTL(0x006C)
#define ZYD_CR28	ZYD_REG_CTL(0x0070)
#define ZYD_CR29	ZYD_REG_CTL(0x0074)
#define ZYD_CR30	ZYD_REG_CTL(0x0078)
#define ZYD_CR31	ZYD_REG_CTL(0x007C) /* TX power control for RF in CCK mode */
#define ZYD_CR32	ZYD_REG_CTL(0x0080)
#define ZYD_CR33	ZYD_REG_CTL(0x0084)
#define ZYD_CR34	ZYD_REG_CTL(0x0088)
#define ZYD_CR35	ZYD_REG_CTL(0x008C)
#define ZYD_CR36	ZYD_REG_CTL(0x0090)
#define ZYD_CR37	ZYD_REG_CTL(0x0094)
#define ZYD_CR38	ZYD_REG_CTL(0x0098)
#define ZYD_CR39	ZYD_REG_CTL(0x009C)
#define ZYD_CR40	ZYD_REG_CTL(0x00A0)
#define ZYD_CR41	ZYD_REG_CTL(0x00A4)
#define ZYD_CR42	ZYD_REG_CTL(0x00A8)
#define ZYD_CR43	ZYD_REG_CTL(0x00AC)
#define ZYD_CR44	ZYD_REG_CTL(0x00B0)
#define ZYD_CR45	ZYD_REG_CTL(0x00B4)
#define ZYD_CR46	ZYD_REG_CTL(0x00B8)
#define ZYD_CR47	ZYD_REG_CTL(0x00BC) /* CCK baseband gain (patch value might be in EEPROM) */
#define ZYD_CR48	ZYD_REG_CTL(0x00C0)
#define ZYD_CR49	ZYD_REG_CTL(0x00C4)
#define ZYD_CR50	ZYD_REG_CTL(0x00C8)
#define ZYD_CR51	ZYD_REG_CTL(0x00CC) /* TX power control for RF in 6-36M modes */
#define ZYD_CR52	ZYD_REG_CTL(0x00D0) /* TX power control for RF in 48M mode */
#define ZYD_CR53	ZYD_REG_CTL(0x00D4) /* TX power control for RF in 54M mode */
#define ZYD_CR54	ZYD_REG_CTL(0x00D8)
#define ZYD_CR55	ZYD_REG_CTL(0x00DC)
#define ZYD_CR56	ZYD_REG_CTL(0x00E0)
#define ZYD_CR57	ZYD_REG_CTL(0x00E4)
#define ZYD_CR58	ZYD_REG_CTL(0x00E8)
#define ZYD_CR59	ZYD_REG_CTL(0x00EC)
#define ZYD_CR60	ZYD_REG_CTL(0x00F0)
#define ZYD_CR61	ZYD_REG_CTL(0x00F4)
#define ZYD_CR62	ZYD_REG_CTL(0x00F8)
#define ZYD_CR63	ZYD_REG_CTL(0x00FC)
#define ZYD_CR64	ZYD_REG_CTL(0x0100)
#define ZYD_CR65	ZYD_REG_CTL(0x0104) /* OFDM 54M calibration */
#define ZYD_CR66	ZYD_REG_CTL(0x0108) /* OFDM 48M calibration */
#define ZYD_CR67	ZYD_REG_CTL(0x010C) /* OFDM 36M calibration */
#define ZYD_CR68	ZYD_REG_CTL(0x0110) /* CCK calibration */
#define ZYD_CR69	ZYD_REG_CTL(0x0114)
#define ZYD_CR70	ZYD_REG_CTL(0x0118)
#define ZYD_CR71	ZYD_REG_CTL(0x011C)
#define ZYD_CR72	ZYD_REG_CTL(0x0120)
#define ZYD_CR73	ZYD_REG_CTL(0x0124)
#define ZYD_CR74	ZYD_REG_CTL(0x0128)
#define ZYD_CR75	ZYD_REG_CTL(0x012C)
#define ZYD_CR76	ZYD_REG_CTL(0x0130)
#define ZYD_CR77	ZYD_REG_CTL(0x0134)
#define ZYD_CR78	ZYD_REG_CTL(0x0138)
#define ZYD_CR79	ZYD_REG_CTL(0x013C)
#define ZYD_CR80	ZYD_REG_CTL(0x0140)
#define ZYD_CR81	ZYD_REG_CTL(0x0144)
#define ZYD_CR82	ZYD_REG_CTL(0x0148)
#define ZYD_CR83	ZYD_REG_CTL(0x014C)
#define ZYD_CR84	ZYD_REG_CTL(0x0150)
#define ZYD_CR85	ZYD_REG_CTL(0x0154)
#define ZYD_CR86	ZYD_REG_CTL(0x0158)
#define ZYD_CR87	ZYD_REG_CTL(0x015C)
#define ZYD_CR88	ZYD_REG_CTL(0x0160)
#define ZYD_CR89	ZYD_REG_CTL(0x0164)
#define ZYD_CR90	ZYD_REG_CTL(0x0168)
#define ZYD_CR91	ZYD_REG_CTL(0x016C)
#define ZYD_CR92	ZYD_REG_CTL(0x0170)
#define ZYD_CR93	ZYD_REG_CTL(0x0174)
#define ZYD_CR94	ZYD_REG_CTL(0x0178)
#define ZYD_CR95	ZYD_REG_CTL(0x017C)
#define ZYD_CR96	ZYD_REG_CTL(0x0180)
#define ZYD_CR97	ZYD_REG_CTL(0x0184)
#define ZYD_CR98	ZYD_REG_CTL(0x0188)
#define ZYD_CR99	ZYD_REG_CTL(0x018C)
#define ZYD_CR100	ZYD_REG_CTL(0x0190)
#define ZYD_CR101	ZYD_REG_CTL(0x0194)
#define ZYD_CR102	ZYD_REG_CTL(0x0198)
#define ZYD_CR103	ZYD_REG_CTL(0x019C)
#define ZYD_CR104	ZYD_REG_CTL(0x01A0)
#define ZYD_CR105	ZYD_REG_CTL(0x01A4)
#define ZYD_CR106	ZYD_REG_CTL(0x01A8)
#define ZYD_CR107	ZYD_REG_CTL(0x01AC)
#define ZYD_CR108	ZYD_REG_CTL(0x01B0)
#define ZYD_CR109	ZYD_REG_CTL(0x01B4)
#define ZYD_CR110	ZYD_REG_CTL(0x01B8)
#define ZYD_CR111	ZYD_REG_CTL(0x01BC)
#define ZYD_CR112	ZYD_REG_CTL(0x01C0)
#define ZYD_CR113	ZYD_REG_CTL(0x01C4)
#define ZYD_CR114	ZYD_REG_CTL(0x01C8)
#define ZYD_CR115	ZYD_REG_CTL(0x01CC)
#define ZYD_CR116	ZYD_REG_CTL(0x01D0)
#define ZYD_CR117	ZYD_REG_CTL(0x01D4)
#define ZYD_CR118	ZYD_REG_CTL(0x01D8)
#define ZYD_CR119	ZYD_REG_CTL(0x01DC)
#define ZYD_CR120	ZYD_REG_CTL(0x01E0)
#define ZYD_CR121	ZYD_REG_CTL(0x01E4)
#define ZYD_CR122	ZYD_REG_CTL(0x01E8)
#define ZYD_CR123	ZYD_REG_CTL(0x01EC)
#define ZYD_CR124	ZYD_REG_CTL(0x01F0)
#define ZYD_CR125	ZYD_REG_CTL(0x01F4)
#define ZYD_CR126	ZYD_REG_CTL(0x01F8)
#define ZYD_CR127	ZYD_REG_CTL(0x01FC)
#define ZYD_CR128	ZYD_REG_CTL(0x0200)
#define ZYD_CR129	ZYD_REG_CTL(0x0204)
#define ZYD_CR130	ZYD_REG_CTL(0x0208)
#define ZYD_CR131	ZYD_REG_CTL(0x020C)
#define ZYD_CR132	ZYD_REG_CTL(0x0210)
#define ZYD_CR133	ZYD_REG_CTL(0x0214)
#define ZYD_CR134	ZYD_REG_CTL(0x0218)
#define ZYD_CR135	ZYD_REG_CTL(0x021C)
#define ZYD_CR136	ZYD_REG_CTL(0x0220)
#define ZYD_CR137	ZYD_REG_CTL(0x0224)
#define ZYD_CR138	ZYD_REG_CTL(0x0228)
#define ZYD_CR139	ZYD_REG_CTL(0x022C)
#define ZYD_CR140	ZYD_REG_CTL(0x0230)
#define ZYD_CR141	ZYD_REG_CTL(0x0234)
#define ZYD_CR142	ZYD_REG_CTL(0x0238)
#define ZYD_CR143	ZYD_REG_CTL(0x023C)
#define ZYD_CR144	ZYD_REG_CTL(0x0240)
#define ZYD_CR145	ZYD_REG_CTL(0x0244)
#define ZYD_CR146	ZYD_REG_CTL(0x0248)
#define ZYD_CR147	ZYD_REG_CTL(0x024C)
#define ZYD_CR148	ZYD_REG_CTL(0x0250)
#define ZYD_CR149	ZYD_REG_CTL(0x0254)
#define ZYD_CR150	ZYD_REG_CTL(0x0258)
#define ZYD_CR151	ZYD_REG_CTL(0x025C)
#define ZYD_CR152	ZYD_REG_CTL(0x0260)
#define ZYD_CR153	ZYD_REG_CTL(0x0264)
#define ZYD_CR154	ZYD_REG_CTL(0x0268)
#define ZYD_CR155	ZYD_REG_CTL(0x026C)
#define ZYD_CR156	ZYD_REG_CTL(0x0270)
#define ZYD_CR157	ZYD_REG_CTL(0x0274)
#define ZYD_CR158	ZYD_REG_CTL(0x0278)
#define ZYD_CR159	ZYD_REG_CTL(0x027C)
#define ZYD_CR160	ZYD_REG_CTL(0x0280)
#define ZYD_CR161	ZYD_REG_CTL(0x0284)
#define ZYD_CR162	ZYD_REG_CTL(0x0288)
#define ZYD_CR163	ZYD_REG_CTL(0x028C)
#define ZYD_CR164	ZYD_REG_CTL(0x0290)
#define ZYD_CR165	ZYD_REG_CTL(0x0294)
#define ZYD_CR166	ZYD_REG_CTL(0x0298)
#define ZYD_CR167	ZYD_REG_CTL(0x029C)
#define ZYD_CR168	ZYD_REG_CTL(0x02A0)
#define ZYD_CR169	ZYD_REG_CTL(0x02A4)
#define ZYD_CR170	ZYD_REG_CTL(0x02A8)
#define ZYD_CR171	ZYD_REG_CTL(0x02AC)
#define ZYD_CR172	ZYD_REG_CTL(0x02B0)
#define ZYD_CR173	ZYD_REG_CTL(0x02B4)
#define ZYD_CR174	ZYD_REG_CTL(0x02B8)
#define ZYD_CR175	ZYD_REG_CTL(0x02BC)
#define ZYD_CR176	ZYD_REG_CTL(0x02C0)
#define ZYD_CR177	ZYD_REG_CTL(0x02C4)
#define ZYD_CR178	ZYD_REG_CTL(0x02C8)
#define ZYD_CR179	ZYD_REG_CTL(0x02CC)
#define ZYD_CR180	ZYD_REG_CTL(0x02D0)
#define ZYD_CR181	ZYD_REG_CTL(0x02D4)
#define ZYD_CR182	ZYD_REG_CTL(0x02D8)
#define ZYD_CR183	ZYD_REG_CTL(0x02DC)
#define ZYD_CR184	ZYD_REG_CTL(0x02E0)
#define ZYD_CR185	ZYD_REG_CTL(0x02E4)
#define ZYD_CR186	ZYD_REG_CTL(0x02E8)
#define ZYD_CR187	ZYD_REG_CTL(0x02EC)
#define ZYD_CR188	ZYD_REG_CTL(0x02F0)
#define ZYD_CR189	ZYD_REG_CTL(0x02F4)
#define ZYD_CR190	ZYD_REG_CTL(0x02F8)
#define ZYD_CR191	ZYD_REG_CTL(0x02FC)
#define ZYD_CR192	ZYD_REG_CTL(0x0300)
#define ZYD_CR193	ZYD_REG_CTL(0x0304)
#define ZYD_CR194	ZYD_REG_CTL(0x0308)
#define ZYD_CR195	ZYD_REG_CTL(0x030C)
#define ZYD_CR196	ZYD_REG_CTL(0x0310)
#define ZYD_CR197	ZYD_REG_CTL(0x0314)
#define ZYD_CR198	ZYD_REG_CTL(0x0318)
#define ZYD_CR199	ZYD_REG_CTL(0x031C)
#define ZYD_CR200	ZYD_REG_CTL(0x0320)
#define ZYD_CR201	ZYD_REG_CTL(0x0324)
#define ZYD_CR202	ZYD_REG_CTL(0x0328)
#define ZYD_CR203	ZYD_REG_CTL(0x032C)	/* I2C bus template value & flash control */
#define ZYD_CR204	ZYD_REG_CTL(0x0330)
#define ZYD_CR205	ZYD_REG_CTL(0x0334)
#define ZYD_CR206	ZYD_REG_CTL(0x0338)
#define ZYD_CR207	ZYD_REG_CTL(0x033C)
#define ZYD_CR208	ZYD_REG_CTL(0x0340)
#define ZYD_CR209	ZYD_REG_CTL(0x0344)
#define ZYD_CR210	ZYD_REG_CTL(0x0348)
#define ZYD_CR211	ZYD_REG_CTL(0x034C)
#define ZYD_CR212	ZYD_REG_CTL(0x0350)
#define ZYD_CR213	ZYD_REG_CTL(0x0354)
#define ZYD_CR214	ZYD_REG_CTL(0x0358)
#define ZYD_CR215	ZYD_REG_CTL(0x035C)
#define ZYD_CR216	ZYD_REG_CTL(0x0360)
#define ZYD_CR217	ZYD_REG_CTL(0x0364)
#define ZYD_CR218	ZYD_REG_CTL(0x0368)
#define ZYD_CR219	ZYD_REG_CTL(0x036C)
#define ZYD_CR220	ZYD_REG_CTL(0x0370)
#define ZYD_CR221	ZYD_REG_CTL(0x0374)
#define ZYD_CR222	ZYD_REG_CTL(0x0378)
#define ZYD_CR223	ZYD_REG_CTL(0x037C)
#define ZYD_CR224	ZYD_REG_CTL(0x0380)
#define ZYD_CR225	ZYD_REG_CTL(0x0384)
#define ZYD_CR226	ZYD_REG_CTL(0x0388)
#define ZYD_CR227	ZYD_REG_CTL(0x038C)
#define ZYD_CR228	ZYD_REG_CTL(0x0390)
#define ZYD_CR229	ZYD_REG_CTL(0x0394)
#define ZYD_CR230	ZYD_REG_CTL(0x0398)
#define ZYD_CR231	ZYD_REG_CTL(0x039C)
#define ZYD_CR232	ZYD_REG_CTL(0x03A0)
#define ZYD_CR233	ZYD_REG_CTL(0x03A4)
#define ZYD_CR234	ZYD_REG_CTL(0x03A8)
#define ZYD_CR235	ZYD_REG_CTL(0x03AC)
#define ZYD_CR236	ZYD_REG_CTL(0x03B0)

#define ZYD_CR240	ZYD_REG_CTL(0x03C0)
/*	bit 7:  host-controlled RF register writes
 * ZYD_CR241-ZYD_CR245: for hardware controlled writing of RF bits, not needed for
 *			  USB
 */
#define ZYD_CR241	ZYD_REG_CTL(0x03C4)
#define ZYD_CR242	ZYD_REG_CTL(0x03C8)
#define ZYD_CR243	ZYD_REG_CTL(0x03CC)
#define ZYD_CR244	ZYD_REG_CTL(0x03D0)
#define ZYD_CR245	ZYD_REG_CTL(0x03D4)

#define ZYD_CR251	ZYD_REG_CTL(0x03EC)	/* only used for activation and deactivation of
				 * Airoha RFs AL2230 and AL7230B
				 */
#define ZYD_CR252	ZYD_REG_CTL(0x03F0)
#define ZYD_CR253	ZYD_REG_CTL(0x03F4)
#define ZYD_CR254	ZYD_REG_CTL(0x03F8)
#define ZYD_CR255	ZYD_REG_CTL(0x03FC)

/*
 * Device configurations and types
 */

struct zyd_type {
	uint16_t vid;
	uint16_t pid;
};

struct zyd_type zyd_devs[] = {
	{ 0x0586, 0x3401 }, /* Zyxel ZyAIR G-220 */
	{ 0x0675, 0x0550 }, /* DrayTek Vigor 550 */
	{ 0x079b, 0x004a }, /* Sagem XG 760A */
	{ 0x07b8, 0x6001 }, /* AOpen 802.11g WL54 */
	{ 0x0ace, 0x1211 }, /* Fiberline Networks WL-43OU, Airlink+ AWLL3025, X-Micro XWL-11GUZX, Edimax EW-7317UG, Planet WL-U356, Acer WLAN-G-US1, Trendnet TEW-424UB */
	{ 0x0b05, 0x170c }, /* Asus WL-159g */
	{ 0x0b3b, 0x5630 }, /* Tekram/Siemens USB adapter */
	{ 0x0df6, 0x9071 }, /* Sitecom WL-113 */
	{ 0x0b3b, 0x1630 }, /* Yakumo QuickWLAN USB */
	{ 0x126f, 0xa006 }, /* TwinMOS G240 */
	{ 0x129b, 0x1666 }, /* Telegent TG54USB */
	{ 0x1435, 0x0711 }, /* iNexQ UR055g */
	{ 0x14ea, 0xab13 }, /* Planex GW-US54Mini */
	{ 0x157e, 0x300b }, /* Trendnet TEW-429UB */
	{ 0x2019, 0xc007 }, /* Planex GW-US54GZL */
	{ 0x5173, 0x1809 }, /* Sweex wireless USB 54 Mbps */
	{ 0x6891, 0xa727 }, /* 3COM 3CRUSB10075 */
	{ }
};

static const char *zyd_rfs[] = {
	"unknown",
	"unknown",
	"UW2451",
	"UCHIP",
	"AL2230",
	"AL7230B",
	"THETA",
	"AL2210",
	"MAXIM_NEW",
	"GCT",
	"PV2000",
	"RALINK",
	"INTERSIL",
	"RFMD",
	"MAXIM_NEW2",
	"PHILIPS"
};

struct zyd_channel_range {
	int regdomain;
	uint8_t start;
	uint8_t end; /* exclusive (channel must be less than end) */
};

/*
 * Blows size up (132 bytes, but not that smaller than extra
 * mapping code, so ...
 */
static const struct zyd_channel_range zyd_channel_ranges[] = {
	{ 0, 0, 0 },
	{ ZYD_REGDOMAIN_FCC, 1, 12 },
	{ ZYD_REGDOMAIN_IC, 1, 12 },
	{ ZYD_REGDOMAIN_ETSI, 1, 14 },
	{ ZYD_REGDOMAIN_JAPAN, 1, 14 },
	{ ZYD_REGDOMAIN_SPAIN, 10, 12 },
	{ ZYD_REGDOMAIN_FRANCE, 10, 14 },
	{ ZYD_REGDOMAIN_JAPAN_ADD, 14, 15 },
	{ -1, 0, 0 }
};

/*
 * Supported rates for 802.11b/g modes (in 500Kbps unit).
 */
static const struct ieee80211_rateset zyd_rateset_11b = {
	4, { 2, 4, 11, 22 }
};

static const struct ieee80211_rateset zyd_rateset_11g =	{
	8, { 12, 18, 24, 36, 48, 72, 96, 108 }
};

/*
 * Various structure content enums
 */

/*
 * Driver internal
 */

enum zyd_operationmode {
	OM_NONEYET		= 0,
	OM_ADHOC		= 1,
	OM_INFRASTRUCTURE	= 2
};

enum zyd_cmd {
	ZC_NONE,
	ZC_SCAN,
	ZC_JOIN
};

enum zyd_encrtypes {
	ENC_NOWEP	= 0,
	ENC_WEP64	= 1,
	ENC_WEP128	= 5,
	ENC_WEP256	= 6,
	ENC_SNIFFER	= 8
};

enum zyd_macflags {
	ZMF_FIXED_CHANNEL = 1
};

enum zyd_stohostflags {
	STH_ASS_REQ	= 1,
	STH_ASS_RSP	= 2,
	STH_REASS_REQ	= 4,
	STH_REASS_RSP	= 8,
	STH_PRB_REQ	= 16,
	STH_PRB_RSP	= 32,
	STH_BCN		= 256,
	STH_ATIM	= 512,
	STH_DEASS	= 1024,
	STH_AUTH	= 2048,
	STH_DEAUTH	= 4096,
	STH_PS_POLL	= 67108864UL,
	STH_RTS		= 134217728UL,
	STH_CTS		= 268435456UL,
	STH_ACK		= 536870912UL,
	STH_CFE		= 1073741824UL,
	STH_CFE_A	= 2147483648UL
};

/*
 * Control set format
 */
enum zyd_controlsetformatrate {
	/*  MBPS rates for Direct Sequence Spread Spectrum */
	CSF_RT_CCK_1	= 0x00,
	CSF_RT_CCK_2	= 0x01,
	CSF_RT_CCK_5_5	= 0x02,
	CSF_RT_CCK_11	= 0x03,

	/* MBPS rates for Orthogonal Frequency Division Multiplexing */
	CSF_RT_OFDM_6	= 0x0b,
	CSF_RT_OFDM_9	= 0x0f,
	CSF_RT_OFDM_12	= 0x0a,
	CSF_RT_OFDM_18	= 0x0e,
	CSF_RT_OFDM_24	= 0x09,
	CSF_RT_OFDM_36	= 0x0d,
	CSF_RT_OFDM_48	= 0x08,
	CSF_RT_OFDM_54	= 0x0c
};

enum zyd_controlsetformatmodtype {
	CSF_MT_CCK	= 0,
	CSF_MT_OFDM	= 1
};

enum zyd_controlsetformatpreamblecck {
	CSF_PM_CCK_LONG		= 0,
	CSF_PM_CCK_SHORT	= 1
};

enum zyd_controlsetformatpreambleofdm {
	CSF_PM_OFDM_11G	= 0,
	CSF_PM_OFDM_11A	= 1
};

enum zyd_controlsetformatbackoff {
	CSF_BO_SIFS	= 0, /* SIFS (fragmentation) */
	CSF_BO_RAND	= 1  /* Need random backoff  */
};

enum zyd_controlsetformatmulticast {
	CSF_MC_UNICAST		= 0,
	CSF_MC_MULTICAST	= 1
};

enum zyd_controlsetformatframetype {
	CSF_FT_DATAFRAME	= 0, /* Date frame (header size = 24)		  */
	CSF_FT_POLLFRAME	= 1, /* Poll frame (needn't modify duration ID */
	CSF_FT_MGMTFRAME	= 2, /* Management frame (header size = 24)	*/
	CSF_FT_NOSEQCONTROL	= 3  /* No sequence control (header size = 16) */
};

enum zyd_controlsetformatwakedst {
	CSF_WD_DONTWAKEUP	= 0,
	CSF_WD_WAKEUP		= 1
};

enum zyd_controlsetformatrts {
	CSF_RTS_NORTSFRAME	= 0,
	CSF_RTS_NEEDRTSFRAME	= 1
};

enum zyd_controlsetformatencryption {
	CSF_ENC_NOENCRYPTION	= 0,
	CSF_ENC_NEEDENCRYPTION	= 1
};

enum zyd_controlsetformatselfcts {
	CSF_SC_NOTSCFRAME	= 0,
	CSF_SC_SCFRAME		= 1
};

/*
 * Rx status report
 */

enum zyd_rxstatusreportdecrtype {
	RSR_DT_NOWEP		= 0,
	RSR_DT_WEP64		= 1,
	RSR_DT_TKIP		= 2,
	/* 3 is reserved */
	RSR_DT_AES		= 4,
	RSR_DT_WEP128		= 5,
	RSR_DT_WEP256		= 6
};

enum zyd_rxstatusreportframestatmodtype {
	RSR_FS_MT_CCK		= 0,
	RSR_FS_MT_OFDM		= 1
};

/* These are flags */
enum zyd_rxstatusreportframestaterrreason {
	RSR_FS_ER_TIMEOUT		= 0x02,
	RSR_FS_ER_FIFO_OVERRUN		= 0x04,
	RSR_FS_ER_DECRYPTION_ERROR	= 0x08,
	RSR_FS_ER_CRC32_ERROR		= 0x10,
	RSR_FS_ER_ADDR1_NOT_MATCH	= 0x20,
	RSR_FS_ER_CRC16_ERROR		= 0x40
};

enum zyd_rxstatusreportframestaterrind {
	RSR_FS_EI_ERROR_INDICATION	= 0x80
};

/*
 * Structures
 */

struct zyd_controlsetformat {
	uint8_t rate		:4;
	uint8_t modulationtype	:1;
	uint8_t preamble	:1;
	uint8_t reserved	:2;

	uint16_t txlen;

	uint8_t needbackoff	:1;
	uint8_t multicast	:1;
	uint8_t frametype	:2;
	uint8_t wakedst		:1;
	uint8_t rts		:1;
	uint8_t encryption	:1;
	uint8_t selfcts		:1;

	uint16_t packetlength;
	uint16_t currentlength;
	uint8_t service;
	uint16_t nextframelen;
} UPACKED;

struct zyd_rxstatusreport {
	uint8_t signalstrength;
	uint8_t signalqualitycck;
	uint8_t signalqualityofdm;
	uint8_t decryptiontype;

	uint8_t modulationtype	:1;
	uint8_t rxerrorreason	:6;
	uint8_t errorindication	:1;
} UPACKED;

/* Appended length info for multiframe transmission */
struct zyd_rxleninfoapp {
	uWord len[3];
	uWord marker; /* 0x697E */
#define ZYD_MULTIFRAME_MARKER 0x697E
} UPACKED;

struct zyd_aw_pt_bi {
	uint32_t atim_wnd_period;
	uint32_t pre_tbtt;
	uint32_t beacon_interval;
};

/* RF control. Kinda driver-in-driver via function pointers. */
struct zyd_softc; /* FORWARD */
struct zyd_rf {
	uint8_t flags;
	uint8_t type;
	usbd_status (*init_hw)(struct zyd_softc *, struct zyd_rf *);
	usbd_status (*switch_radio)(struct zyd_softc *, uint8_t onoff);
	usbd_status (*set_channel)(struct zyd_softc *, struct zyd_rf *, uint8_t);
};

struct zyd_req_rfwrite {
	uWord id;
	uWord value;
	/* 1: 3683a */
	/* 2: other (default) */
	uWord bits;
	/* RF2595: 24 */
	uWord bit_values[0];
	/* (CR203 & ~(RF_IF_LE | RF_CLK | RF_DATA)) | (bit ? RF_DATA : 0) */
} UPACKED;

struct zyd_macaddr {
	uint8_t addr[6];
} UPACKED;

/* Control interface @ EP 0 */
struct zyd_control
{
	uint8_t type;
	uint8_t id;
	uint16_t value;
	uint16_t index;
	uint16_t length;
	void* data;
} UPACKED;

/* Int IN interface @ EP 3 (single register access) */
struct zyd_intinsingle
{
	uWord id;
	uWord sts1;
	uWord sts2;
	uWord sts3;
	uWord sts4;
	uWord sts5;
} UPACKED;

/* Int/bulk OUT interface @ EP 4 (single register access) */
struct zyd_intoutsingle
{
	uWord id;
	uWord par1;
	uWord par2;
	uWord length;
	uWord data1;
	uWord data2;
} UPACKED;

/* Register addr/data combo */
struct zyd_regadcombo {
	uWord addr;
	uWord data;
} UPACKED;

/* Int IN interface @ EP 3 (multi register access) output */
struct zyd_intinmultioutput {
	uWord id;
	struct zyd_regadcombo registers[15]; /* pairs of addr-data-addr-data-... */
} UPACKED;

/* Int/bulk OUT interface @ EP 4 (multi register access) read */
struct zyd_intoutmultiread {
	uWord id;
	uWord addr[15];
} UPACKED;

/* Int/bulk OUT interface @ EP 4 (multi register access) write */
struct zyd_intoutmultiwrite {
	uWord id;
	struct zyd_regadcombo registers[15]; /* pairs of addr-data-addr-data-... */
} UPACKED;

/* Pairs of address and 16-bit data. For batch write. */
struct zyd_adpairs16 {
	uint32_t addr;
	uint16_t data;
};

/* Pairs of address and 32-bit data. For batch write. */
struct zyd_adpairs32 {
	uint32_t addr;
	uint32_t data;
};

/*
 * RX/TX
 */
struct zyd_tx_data {
	struct zyd_softc *sc;
	usbd_xfer_handle xfer;
	uint8_t	*buf;
	struct mbuf	*m;
	struct ieee80211_node *ni;
};

struct zyd_rx_data {
	struct zyd_softc *sc;
	usbd_xfer_handle xfer;
	uint8_t	*buf;
	struct mbuf *m;
};

/*
 * OS driver interface
 */

/* The number of simultaneously requested RX transfers */
#define ZYD_RX_LIST_CNT	1

/* The number of simultaneously started TX transfers */
#define ZYD_TX_LIST_CNT	1

/* sizers */
#define ZYD_RX_DESC_SIZE	(sizeof(struct zyd_rxstatusreport))
#define ZYD_TX_DESC_SIZE	(sizeof(struct zyd_controlsetformat))

/*
 * According to the 802.11 spec (7.1.2) the frame body can be up to 2312 bytes
 */
#define ZYD_RX_BUFSZ	(ZYD_RX_HDRLEN + \
			    sizeof(struct ieee80211_frame_addr4) + 2312 + 4)
/* BE CAREFULL! should add ZYD_TX_PADDING */
#define ZYD_TX_BUFSZ	(ZYD_TX_HDRLEN + \
			    sizeof(struct ieee80211_frame_addr4) + 2312)

#define ZYD_MIN_FRAMELEN	60

/*
 * Sending packets of more than 1500 bytes confuses some access points, so the
 * default MTU is set to 1500 but can be increased up to 2310 bytes using
 * ifconfig
 */
#define ZYD_DEFAULT_MTU		1500
#define ZYD_MAX_MTU		(2312 - 2)

/*
 * Endpoints: 0 = control (as always)
 * 1 = bulk out, 2 = bulk in
 * 3 = int in, 4 = int out (is bulk if not high-speed USB)
 */
#define ZYD_ENDPT_CTRL	0
#define ZYD_ENDPT_BOUT	1
#define ZYD_ENDPT_BIN	2
#define ZYD_ENDPT_IIN	3
#define ZYD_ENDPT_IOUT	4
#define ZYD_ENDPT_CNT	5

#define ZYD_TX_TIMEOUT		10000
#define ZYD_JOIN_TIMEOUT	2000

#define ZYD_DEFAULT_SSID	""
#define ZYD_DEFAULT_CHANNEL	10

/* quickly determine if a given rate is CCK or OFDM */
#define ZYD_RATE_IS_OFDM(rate)	((rate) >= 12 && (rate) != 22)

#define ZYD_ACK_SIZE	14 /* 10 + 4(FCS) */
#define ZYD_CTS_SIZE	14 /* 10 + 4(FCS) */
#define ZYD_SIFS	10

/* PLCP header at beginning of any frame */
#define ZYD_PLCP_HDR_SIZE	5

struct zyd_softc;

struct zyd_softc {
	/* Driver handling */
	USBBASEDEVICE zyd_dev;
	struct ieee80211com sc_ic;
	int (*sc_newstate)(struct ieee80211com *, enum ieee80211_state, int);

	/* Interrupt related */
	int pending;
	struct clist q_reply; /* queue for register read replies */
	uint8_t *ibuf;
	struct selinfo rsel;

	/* Device state */
	enum ieee80211_state sc_state;

	/* Command to device (for usb task) */
	enum zyd_cmd sc_cmd;

	/* Device hardware constellation */
	uint16_t firmware_base;
	struct zyd_rf rf;
	uint8_t default_regdomain;
	uint8_t regdomain;
	uint8_t channel;
	uint8_t mac_flags;

	/* Calibration tables */
	uint8_t pwr_cal_values[ZYD_E2P_CHANNEL_COUNT];
	uint8_t pwr_int_values[ZYD_E2P_CHANNEL_COUNT];
	uint8_t ofdm_cal_values[3][ZYD_E2P_CHANNEL_COUNT];

	/* USB stuff */
	struct usb_task sc_task;
	usbd_device_handle zyd_udev;
	usbd_interface_handle zyd_iface;
	int zyd_ed[ZYD_ENDPT_CNT];
	usbd_pipe_handle zyd_ep[ZYD_ENDPT_CNT];

	/* Ethernet stuff */
/*	struct ethercom zyd_ec;*/
	struct ifmedia zyd_media;

	/* TX/RX */
	struct zyd_rx_data rx_data[ZYD_RX_LIST_CNT];
	struct zyd_tx_data tx_data[ZYD_TX_LIST_CNT];
	int tx_queued;

	int zyd_unit;
	int zyd_if_flags;
	int zyd_attached;

	struct timeval zyd_rx_notice;

	struct timeout scan_ch;

	int tx_timer;

	/* WLAN specific */
	enum zyd_operationmode zyd_operation;
	uint8_t zyd_bssid[ETHER_ADDR_LEN];
	uint8_t zyd_ssid[ZYD_MAX_SSID_LEN];
	uint8_t zyd_ssidlen;
	uint16_t zyd_desired_channel;
	uint8_t zyd_radio_on;
	enum zyd_encrtypes zyd_encrypt;

	/* WEP configuration */
	int zyd_wepkey;
	int zyd_wepkeylen;
	uint8_t zyd_wepkeys[4][13];
};
