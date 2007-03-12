/*	$OpenBSD: bcwreg.h,v 1.17 2007/03/12 06:51:16 mglocker Exp $ */

/*
 * Copyright (c) 2006 Jon Simola <jsimola@gmail.com>
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
 *
 */

/* Broadcom BCM43xx */

/* PCI registers defined in the PCI 2.2 spec. */
#define BCW_PCI_BAR0			0x10

/* Saved Interrupt mask */
#define BCW_SAVEDINTRMASK		0xb007a864

/* Sonics SB register access */
#define BCW_ADDR_SPACE0			0x80
#define BCW_ADDR_SPACE1			0x84
#define BCW_REG0_WIN			0x80
#define BCW_REG1_WIN			0x84
#define BCW_SPROM_CONTROL		0x88
#define BCW_ADDR1_BURST_CONTROL		0x8C
#define	BCW_PCII			0x90
#define BCW_PCICR			0x94
#define BCW_BI				0x98
#define BCW_PCICI			0xB0
#define BCW_PCICO			0xB4
#define BCW_PCICOE			0xB8
#define BCW_GPIOI			0xB0 /* GPIO IN */
#define BCW_GPIOO			0xB4 /* GPIO OUT */
#define BCW_GPIOE			0xB8 /* GPIO ENABLE */

/* Some bitmasks */
#define BCW_XTALPOWERUP			0x40
#define BCW_PLLPOWERDOWN		0x80

/* Core select address macro */
#define BCW_CORE_SELECT(x)		(0x18000000 + (x * 0x1000))

/* Core information registers */
#define BCW_CIR_BASE			0xf00
#define BCW_CIR_SBTMSTATELOW		(BCW_CIR_BASE + 0x98)

/* Some Core Types */
#define BCW_CORE_COMMON			0x800
#define BCW_CORE_PCI			0x804
#define BCW_CORE_ENET			0x806
#define BCW_CORE_PCMCIA			0x80d
#define BCW_CORE_80211			0x812
#define BCW_CORE_PCIE			0x820
#define BCW_CORE_MIMOPHY		0x821
#define BCW_CORE_NONEXIST		0x8ff

#define BCW_CORE_COMMON_CHIPID		0x0

#define BCW_SONICS_WIN			0x18002000

/* Sonics PCI control */
#define BCW_SPCI_TR2			0x0108	/* Sonics to PCI translation
					         * 2 */
/* bit defines */
#define SBTOPCI_PREF			0x4	/* prefetch enable */
#define SBTOPCI_BURST			0x8	/* burst enable */
#define BCW_SBINTVEC			0x0f94
/* interrupt bits */
#define SBIV_ENET0			0x02	/* enable for enet 0 */
#define SBIV_ENET1			0x40	/* enable for enet 1 */

/* Host Interface Registers */

#define BCW_DEVCTL			0x0000		/* device control */
/* device control bits */
#define BCW_DC_IP			0x00000400	/* internal phy present */
#define BCW_DC_ER			0x00008000	/* ephy reset */
/* Interrupt Control */
#define BCW_INT_STS			0x20
#define BCW_DMA0_INT_MASK		0x24
#define BCW_DMA1_INT_MASK		0x2c
#define BCW_DMA2_INT_MASK		0x34
#define BCW_DMA3_INT_MASK		0x3c
#define BCW_DMA4_INT_MASK		0x44
#define BCW_DMA5_INT_MASK		0x4c

/* bits for both status, and mask */
#define I_TO				0x00000080	/* general timeout */
#define I_PC				0x00000400	/* descriptor error */
#define I_PD				0x00000800	/* data error */
#define I_DE				0x00001000	/* desc. protocol error */
#define I_RU				0x00002000	/* rx desc. underflow */
#define I_RO				0x00004000	/* rx fifo overflow */
#define I_XU				0x00008000	/* tx fifo underflow */
#define I_RI				0x00010000	/* receive interrupt */
#define I_XI				0x01000000	/* transmit interrupt */

/* Ethernet MAC Control */
#define BCW_MACCTL			0x00A8	/* ethernet mac control */
/* mac control bits */
#define BCW_EMC_CRC32_ENAB		0x00000001	/* crc32 generation */
#define BCW_EMC_PDOWN			0x00000004	/* PHY powerdown */
#define BCW_EMC_EDET			0x00000008	/* PHY energy detect */
#define BCW_EMC_LED			0x000000e0	/* PHY LED control */

/* DMA Interrupt control */
#define BCW_DMAI_CTL			0x0100

/* DMA registers */
#define BCW_DMA_TXCTL			0x0200	/* transmit control */
/* transmit control bits */
#define XC_XE				0x1	/* transmit enable */
#define XC_LE				0x4	/* loopback enable */
#define BCW_DMA_TXADDR			0x0204	/* tx ring base address */
#define BCW_DMA_DPTR			0x0208	/* last tx descriptor */
//#define BCW_DMA_TXSTATUS		0x020C	/* active desc, etc */
#define BCW_DMA_RXCTL			0x0210	/* enable, etc */
#define BCW_DMA_RXADDR			0x0214	/* rx ring base address */
#define BCW_DMA_RXDPTR			0x0218	/* last descriptor */
//#define BCW_DMA_RXSTATUS		0x021C	/* active desc, etc */
/* receive status bits */
#define RS_CD_MASK			0x0fff	/* current descriptor pointer */
#define RS_DMA_IDLE			0x2000	/* DMA is idle */
#define RS_ERROR			0xf0000	/* had an error */

/* Ethernet MAC control registers */
#define BCW_RX_CTL			0x0400		/* receive config */
/* config bits */
#define ERC_DB				0x00000001	/* disable broadcast */
#define ERC_AM				0x00000002	/* rx all multicast */
#define ERC_PE				0x00000008	/* promiscuous enable */

#define BCW_RX_MAX			0x0404		/* max packet length */
#define BCW_TX_MAX			0x0408
#define BCW_MI_CTL			0x0410
#define BCW_MI_COMM			0x0414
#define BCW_MI_STS			0x041C
/* mii status bits */
#define BCW_MIINTR			0x00000001	/* mii mdio interrupt */

#define BCW_FILT_LOW			0x0420		/* mac low 4 bytes */
#define BCW_FILT_HI			0x0424		/* mac hi 2 bytes */
#define BCW_FILT_CTL			0x0428		/* packet filter ctrl */
#define BCW_ENET_CTL			0x042C
/* bits for mac control */
#define EC_EE				0x00000001	/* emac enable */
#define EC_ED				0x00000002	/* disable emac */
#define EC_ES				0x00000004	/* soft reset emac */
#define EC_EP				0x00000008	/* external phy */
#define BCW_TX_CTL			0x0430
/* bits for transmit control */
#define EXC_FD				0x00000001	/* full duplex */
#define BCW_TX_WATER			0x0434		/* tx watermark */

/* statistics counters */
#define BCW_RX_PKTS			0x058C

/* SiliconBackplane registers 0xea8 through 0xffc */
#define BCW_SBTPSFLAG			0x0f18
#define SBTPS_BACKPLANEFLAGMASK		0x3f
#define SBTPS_INT_ALWAYS		0x40		/* Interrupt always sent on backplane */
#define BCW_SBIMSTATE			0x0f90
#define SBIM_REJECT			0x0200000
#define SBIM_INBANDERR			0x20000		/* Inband Error */
#define SBIM_TIMEOUT			0x40000		/* Timeout */
#define BCW_SBTMSTATELOW		0x0f98
#define SBTML_RESET			0x1		/* reset */
#define SBTML_REJ			0x6		/* reject */
#define SBTML_REJ22			0x2		/* v2.2 Reject bit */
#define SBTML_REJ23			0x4		/* v2.3 Reject bit */
#define SBTML_CLK			0x10000		/* clock enable */
#define SBTML_FGC			0x20000		/* force gated clocks on */
#define SBTML_80211FLAG			0x40000		/* core specific flag */
#define SBTML_80211PHY			0x20000000	/* Attach PHY */
#define BCW_SBTMSTATEHI			0x0f9C
#define SBTMH_SERR			0x1		/* S Error */
#define SBTMH_INT			0x2
#define SBTMH_BUSY			0x4

#define BCW_CIR_SBID_LO			0xff8
#define BCW_CIR_SBID_LO_INITIATOR	0x80
#define SBREV_MASK			0xf0000000	/* 1 = v2.3, 0 <= v2.2 */
#define SBREV_MASK_SHIFT		28
#define BCW_CIR_SBID_HI			0xffc

/*
 * MMIO Registers by offset, followed by indented bitmasks
 */
#define BCW_SPROM_CONTROL		0x88		/* SPROM Control register */

#define BCW_SBF				0x120		/* MIMO - Status Bit Field */
#define BCW_SBF_MAC_ENABLED		0x00000001	/* Set when mac enabled */
#define BCW_SBF_CORE_READY		0x00000004	/* set after core reset/enabled */
#define BCW_SBF_400_MAGIC		0x00000400	/* MAGIC */
#define BCW_SBF_REGISTER_BYTESWAP	0x00010000	/* xfer regs are byteswapped in hw */
#define  BCW_SBF_ADHOC			0x00020000	/* Operating mode is not adhoc */
#define BCW_SBF_AP			0x00040000	/* Device is in AP mode */
#define BCW_SBF_RADIOREG_LOCK		0x00080000	/* Radio Registers locked for use */
#define BCW_SBF_MONITOR			0x00400000	/* Pass HW handled control frames 
							 * to driver, needs PROMISC also */
#define BCW_SBF_PROMISC			0x01000000	/* Device is in promiscuous mode */
#define BCW_SBF_PS1			0x02000000	/* Power saving bit 1 - unknown */
#define BCW_SBF_PS2			0x04000000	/*  bit 2 - Device is awake */
#define BCW_SBF_SSID_BCAST		0x08000000	/* set = SSID bcast is disabled
							 * unset = SSID bcast enabled */
#define BCW_SBF_TIME_UPDATE		0x10000000	/* Related to TSF updates */

#define BCW_GIR				0x128	/* MIMO - Generic Interrupt Reason */
#define BCW_MMIO_RAM_CONTROL		0x130
#define BCW_MMIO_RAM_DATA		0x134

#define BCW_SHM_CONTROL			0x160	/* Control */
#define BCW_SHM_DATA			0x164	/* Data - 32bit */
#define BCW_SHM_DATALOW			0x164	/* Data Low - 16bit */
#define BCW_SHM_DATAHIGH		0x166	/* Data High - 16 bit */
#define BCW_SHM_CONTROL_SHARED		0x0001	/* Select SHM Routing shared memory */
#define BCW_SHM_CONTROL_80211		0x0002	/* Select 80211 settings */
#define BCW_SHM_CONTROL_PCM		0x0003	/* Select PCM data */
#define BCW_SHM_CONTROL_HWMAC		0x0004	/* Security Hardware MAC Address list */
#define BCW_SHM_CONTROL_MCODE		0x0300	/* Microcode */
#define BCW_SHM_CONTROL_INIMCODE	0x0301	/* Initial Value Microcode? */
/* SHM Addresses */
#define BCW_SHM_MICROCODEFLAGSLOW	0x005e	/* Flags for Microcode ops */
#define BCW_SHM_MICROCODEFLAGSHIGH	0x0060	/* Flags for Microcode ops */
#define BCW_SHM_MICROCODEFLAGSAUTODIV	0x0001
/* http://bcm-specs.sipsolutions.net/MicrocodeFlagsBitfield */

/* 0x200 DMA Register space, there are 6 controllers */
/* 32 bit DMA controllers */
#define BCW_DMA_TXCONTROL(x)		(0x200 + (x * 0x20))
						/* DMA Control Word*/
#define BCW_DMA_TXRINGADDR(x)		(0x204 + (x * 0x20))
						/* Ring Address */
#define BCW_DMA_TXSTOPADDR(x)		(0x208 + (x * 0x20))
						/* Last Descriptor */
#define BCW_DMA_TXSTATUS(x)		(0x20c + (x * 0x20))
						/* Transmit Status Word */
#define BCW_DMA_RXCONTROL(x)		(0x200 + (x * 0x20))
						/* DMA Control Word*/
#define BCW_DMA_RXRINGADDR(x)		(0x204 + (x * 0x20))
						/* Ring Address */
#define BCW_DMA_RXSTOPADDR(x)		(0x208 + (x * 0x20))
						/* Last descriptor */
#define BCW_DMA_RXSTATUS(x)		(0x20c + (x * 0x20))
						/* Receive Status Word */
/* 64 bit DMA controllers */
#define BCW_DMA64_TXCONTROL(x)		(0x200 + (x * 0x40))
						/* DMA control Word */
#define BCW_DMA64_TXSTOPADDR(x)		(0x204 + (x * 0x40))
						/* Last Descriptor */
#define BCW_DMA64_TXRINGADDRLOW(x)	(0x208 + (x * 0x40))
						/* Low 32 bits Ring Address */
#define BCW_DMA64_TXRINGADDRHIGH(x)	(0x20c + (x * 0x40))
						/* High 32 bits Ring Address */
#define BCW_DMA64_TXSTATUS(x)		(0x210 + (x * 0x40))
						/* Transmit Status State */
#define BCW_DMA64_TXERROR(x)		(0x214 + (x * 0x40))
						/* Transmit Status Error */
#define BCW_DMA64_RXCONTROL(x)		(0x220 + (x * 0x40))
						/* DMA control Word */
#define BCW_DMA64_RXSTOPADDR(x)		(0x224 + (x * 0x40))
						/* Last Descriptor */
#define BCW_DMA64_RXRINGADDRLOW(x)	(0x228 + (x * 0x40))
						/* Low 32 bits Ring Address */
#define BCW_DMA64_RXRINGADDRHIGH(x)	(0x22c + (x * 0x40))
						/* High 32 bits Ring Address */
#define BCW_DMA64_RXSTATUS(x)		(0x230 + (x * 0x40))
						/* Transmit Status State */
#define BCW_DMA64_RXERROR(x)		(0x234 + (x * 0x40))
						/* Transmit Status Error */

#define BCW_DMA_CONTROL_ENABLE		0x1	/* Enable */
#define BCW_DMA_CONTROL_SUSPEND		0x2	/* Suspend Request */
#define BCW_DMA_CONTROL_LOOPBACK	0x4	/* Loopback Enable */
#define BCW_DMA_CONTROL_FLUSH		0x10	/* Flush Request */
#define BCW_DMA_CONTROL_ADDREXT		0x30000	/* Address Extension */

/* 0x300 PIO Register space */
#define BCW_RADIO_BASEBAND		0x3e6	/* Baseband Attenuation */
#define BCW_RADIO_CONTROL		0x3f6	/* Control - 16bit */
#define BCW_RADIO_DATA			0x3fa	/* Data - 16bit */
#define BCW_RADIO_DATALOW		0x3fa	/* Data Low - 16bit */
#define BCW_RADIO_DATAHIGH		0x3f8	/* Data High - 16 bit */
#define BCW_RADIO_ID			0x01	/* Radio ID offset */

#define BCW_PHY_CONTROL			0x3fc	/* Control - 16bit */
#define BCW_PHY_DATA			0x3fe	/* Data - 16bit */

/* SPROM registers are 16 bit and based at MMIO offset 0x1000 */
#define BCW_MMIO_BASE			0x1000
#define BCW_SPROM_SUBPRODID		0x1004	/* Subsystem Product ID */
#define BCW_SPROM_SUBVENID		0x1006	/* Subsystem Vendor ID */
#define BCW_SPROM_PRODID		0x1008	/* Product ID */
#define BCW_SPROM_IL0MACADDR		0x1048	/* 802.11b/g MAC */
#define BCW_SPROM_ET0MACADDR		0x104e	/* ethernet MAC */
#define BCW_SPROM_ET1MACADDR		0x1054	/* 802.11a MAC */

#define BCW_SPROM_PA0B0			0x105e
#define BCW_SPROM_PA0B1			0x1060
#define BCW_SPROM_PA0B2			0x1062
#define BCW_SPROM_PAMAXPOWER		0x1066	/* 7-0 for A, 15-8 for B/G */
#define BCW_SPROM_PA1B0			0x106a
#define BCW_SPROM_PA1B1			0x106c
#define BCW_SPROM_PA1B2			0x106e
#define BCW_SPROM_IDLETSSI		0x1070  /* As below */
#define BCW_SPROM_BOARDFLAGS		0x1072	/* lower 16 bits */
#define BCW_BF_BTCOEXIST		0x0001	/* Bluetooth Co-existance */
#define BCW_BF_PACTRL			0x0002	/* GPIO 9 controls PA */
#define BCW_BF_AIRLINEMODE		0x0004	/* GPIO13 -> radio off LED */
#define BCW_BF_RSSI			0x0008	/* "something RSSI related */
#define BCW_BF_ENETSPI			0x0010	/* EPHY Roboswitch SPI */
#define BCW_BF_XTAL			0x0020	/* XTAL doesn't slow clock */
#define BCW_BF_CCKHIPWR			0x0040	/* HiPower CCK xmission */
#define BCW_BF_ENETADM			0x0080	/* has ADMtek Switch */
#define BCW_BF_ENETVLAN			0x0100	/* VLAN capable - huh? */
#define BCW_BF_AFTERBURNER		0x0200	/* Afterburner capable */
#define BCW_BF_NOPCI			0x0400	/* Board leaves PCI floating */
#define BCW_BF_FEM			0x0800	/* Front End Module support */
#define BCW_BF_EXTLNA			0x1000	/* has external LNA */
#define BCW_BF_HGPA			0x2000	/* has High Gain PA */
#define BCW_BF_BTCMOD			0x4000	/* BTCOEXIST in alt GPIOs */
#define BCW_BF_ALTIQ			0x8000	/* Alternate I/Q settings */

#define BCW_SPROM_ANTGAIN		0x1074	/* bits 7-0 for an A PHY
						   bits 15-8 for B/G PHYs */

#define BCW_PHY_TYPEA			0x0	/* 802.11a PHY */
#define BCW_PHY_TYPEB			0x1	/* 802.11b PHY */
#define BCW_PHY_TYPEG			0x2	/* 802.11g PHY */
#define BCW_PHY_TYPEN			0x4	/* 802.11n PHY */

/* generic interrupt reasons */
#define BCW_INTR_READY			(1 << 0)

/* firmware revision informations */
#define BCW_UCODE_REVISION		0x0000
#define BCW_UCODE_PATCHLEVEL		0x0002
#define BCW_UCODE_DATE			0x0004
#define BCW_UCODE_TIME			0x0006
#define BCW_UCODE_STATUS		0x0040

#define BCW_PHY_RADIO_BITFIELD		0x0401

/* GPIO */
#define BCW_GPIO_MASK			0x49e
#define BCW_GPIO_CTRL			0x6c

#define BCW_MMIO_PHY_RADIO		0x3e2
#define BCW_MMIO_CHANNEL		0x3f0
#define BCW_MMIO_CHANNEL_EXT		0x3f4

#define BCW_MMIO_GPIO_CONTROL		0x49c

/* LEDS */
#define BCW_NR_LEDS			4

/* PHY registers */
#define BCW_PHY_G_LO_CONTROL		0x0810
#define BCW_PHY_ILT_A_CTRL		0x0072
#define BCW_PHY_ILT_A_DATA1		0x0073
#define BCW_PHY_ILT_G_CTRL		0x0472
#define BCW_PHY_ILT_G_DATA1		0x0473
#define BCW_PHY_G_CRS			0x0429
#define BCW_PHY_A_CRS			0x0029
#define BCW_PHY_NRSSILT_CTRL		0x0803
#define BCW_PHY_NRSSILT_DATA		0x0804
#define BCW_PHY_A_PCTL			0x007b
#define BCW_PHY_G_PCTL			0x0029

#define BCW_READ16(sc, reg) \
	bus_space_read_2((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define BCW_READ(sc, reg) \
	bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (reg))
#define BCW_WRITE16(sc, reg, val) \
	bus_space_write_2((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
#define BCW_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (reg), (val))
