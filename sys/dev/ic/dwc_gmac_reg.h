/*	$OpenBSD: dwc_gmac_reg.h,v 1.1 2016/08/13 20:35:57 kettenis Exp $	*/
/* $NetBSD: dwc_gmac_reg.h,v 1.15 2015/11/21 16:04:11 martin Exp $ */

/*-
 * Copyright (c) 2013, 2014 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of 3am Software Foundry and Martin Husemann.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	AWIN_GMAC_MAC_CONF		0x0000
#define	AWIN_GMAC_MAC_FFILT		0x0004
#define	AWIN_GMAC_MAC_HTHIGH		0x0008
#define	AWIN_GMAC_MAC_HTLOW		0x000c
#define	AWIN_GMAC_MAC_MIIADDR		0x0010
#define	AWIN_GMAC_MAC_MIIDATA		0x0014
#define	AWIN_GMAC_MAC_FLOWCTRL		0x0018
#define	AWIN_GMAC_MAC_VLANTAG		0x001c
#define	AWIN_GMAC_MAC_VERSION		0x0020	/* not always implemented? */
#define	AWIN_GMAC_MAC_INTR		0x0038
#define	AWIN_GMAC_MAC_INTMASK		0x003c
#define	AWIN_GMAC_MAC_ADDR0HI		0x0040
#define	AWIN_GMAC_MAC_ADDR0LO		0x0044
#define	AWIN_GMAC_MII_STATUS		0x00D8

#define	AWIN_GMAC_MAC_CONF_DISABLEJABBER (1 << 22) /* jabber disable */
#define	AWIN_GMAC_MAC_CONF_FRAMEBURST	(1 << 21) /* allow TX frameburst when
						     in half duplex mode */
#define	AWIN_GMAC_MAC_CONF_MIISEL	(1 << 15) /* select MII phy */
#define	AWIN_GMAC_MAC_CONF_FES100	(1 << 14) /* 100 mbit mode */
#define	AWIN_GMAC_MAC_CONF_DISABLERXOWN	(1 << 13) /* do not receive our own
						     TX frames in half duplex
						     mode */
#define	AWIN_GMAC_MAC_CONF_FULLDPLX	(1 << 11) /* select full duplex */
#define	AWIN_GMAC_MAC_CONF_ACS		(1 << 7)  /* auto pad/CRC stripping */
#define	AWIN_GMAC_MAC_CONF_TXENABLE	(1 << 3)  /* enable TX dma engine */
#define	AWIN_GMAC_MAC_CONF_RXENABLE	(1 << 2)  /* enable RX dma engine */

#define	AWIN_GMAC_MAC_FFILT_RA		(1U << 31) /* receive all mode */
#define	AWIN_GMAC_MAC_FFILT_HPF		(1 << 10) /* hash or perfect filter */
#define	AWIN_GMAC_MAC_FFILT_SAF		(1 << 9)  /* source address filter */
#define	AWIN_GMAC_MAC_FFILT_SAIF	(1 << 8)  /* inverse filtering */
#define	AWIN_GMAC_MAC_FFILT_DBF		(1 << 5)  /* disable broadcast frames */
#define	AWIN_GMAC_MAC_FFILT_PM		(1 << 4)  /* promiscious multicast */
#define	AWIN_GMAC_MAC_FFILT_DAIF	(1 << 3)  /* DA inverse filtering */
#define	AWIN_GMAC_MAC_FFILT_HMC		(1 << 2)  /* multicast hash compare */
#define	AWIN_GMAC_MAC_FFILT_HUC		(1 << 1)  /* unicast hash compare */
#define	AWIN_GMAC_MAC_FFILT_PR		(1 << 0)  /* promiscious mode */

#define	AWIN_GMAC_MAC_INT_LPI		(1 << 10)
#define	AWIN_GMAC_MAC_INT_TSI		(1 << 9)
#define	AWIN_GMAC_MAC_INT_ANEG		(1 << 2)
#define	AWIN_GMAC_MAC_INT_LINKCHG	(1 << 1)
#define	AWIN_GMAC_MAC_INT_RGSMII	(1 << 0)

#define	AWIN_GMAC_MAC_FLOWCTRL_PAUSE_SHIFT	16
#define	AWIN_GMAC_MAC_FLOWCTRL_PAUSE_MASK	0xffff
#define	AWIN_GMAC_MAC_FLOWCTRL_RFE	(1 << 2)
#define	AWIN_GMAC_MAC_FLOWCTRL_TFE	(1 << 1)
#define	AWIN_GMAC_MAC_FLOWCTRL_BUSY	(1 << 0)

#define	AWIN_GMAC_DMA_BUSMODE		0x1000
#define	AWIN_GMAC_DMA_TXPOLL		0x1004
#define	AWIN_GMAC_DMA_RXPOLL		0x1008
#define	AWIN_GMAC_DMA_RX_ADDR		0x100c
#define	AWIN_GMAC_DMA_TX_ADDR		0x1010
#define	AWIN_GMAC_DMA_STATUS		0x1014
#define	AWIN_GMAC_DMA_OPMODE		0x1018
#define	AWIN_GMAC_DMA_INTENABLE		0x101c
#define	AWIN_GMAC_DMA_CUR_TX_DESC	0x1048
#define	AWIN_GMAC_DMA_CUR_RX_DESC	0x104c
#define	AWIN_GMAC_DMA_CUR_TX_BUFADDR	0x1050
#define	AWIN_GMAC_DMA_CUR_RX_BUFADDR	0x1054
#define	AWIN_GMAC_DMA_HWFEATURES	0x1058	/* not always implemented? */

#define	GMAC_MII_PHY_SHIFT		11
#define	GMAC_MII_PHY_MASK		0x1f
#define	GMAC_MII_REG_SHIFT		6
#define	GMAC_MII_REG_MASK		0x1f

#define	GMAC_MII_BUSY			(1 << 0)
#define	GMAC_MII_WRITE			(1 << 1)
#define	GMAC_MII_CLK_60_100M_DIV42	0x0
#define	GMAC_MII_CLK_100_150M_DIV62	0x1
#define	GMAC_MII_CLK_25_35M_DIV16	0x2
#define	GMAC_MII_CLK_35_60M_DIV26	0x3
#define	GMAC_MII_CLK_150_250M_DIV102	0x4
#define	GMAC_MII_CLK_250_300M_DIV124	0x5
#define	GMAC_MII_CLK_DIV4		0x8
#define	GMAC_MII_CLK_DIV6		0x9
#define	GMAC_MII_CLK_DIV8		0xa
#define	GMAC_MII_CLK_DIV10		0xb
#define	GMAC_MII_CLK_DIV12		0xc
#define	GMAC_MII_CLK_DIV14		0xd
#define	GMAC_MII_CLK_DIV16		0xe
#define	GMAC_MII_CLK_DIV18		0xf
#define	GMAC_MII_CLKMASK_SHIFT		2
#define	GMAC_MII_CLKMASK_MASK		0xf

#define	GMAC_BUSMODE_4PBL		(1 << 24)
#define	GMAC_BUSMODE_RPBL_SHIFT		17
#define	GMAC_BUSMODE_RPBL_MASK		0x3f
#define	GMAC_BUSMODE_FIXEDBURST		(1 << 16)
#define	GMAC_BUSMODE_PRIORXTX_SHIFT	14
#define	GMAC_BUSMODE_PRIORXTX_MASK	0x3
#define	GMAC_BUSMODE_PRIORXTX_41	3
#define	GMAC_BUSMODE_PRIORXTX_31	2
#define	GMAC_BUSMODE_PRIORXTX_21	1
#define	GMAC_BUSMODE_PRIORXTX_11	0
#define	GMAC_BUSMODE_PBL_SHIFT		8
#define	GMAC_BUSMODE_PBL_MASK		0x3f /* possible DMA
						burst len */
#define	GMAC_BUSMODE_RESET		(1 << 0)

#define	AWIN_GMAC_MII_IRQ		(1 << 0)


#define	GMAC_DMA_OP_DISABLECSDROP	(1 << 26) /* disable dropping of
						     frames with TCP/IP
						     checksum errors */
#define	GMAC_DMA_OP_RXSTOREFORWARD	(1 << 25) /* start RX when a
						    full frame is available */
#define	GMAC_DMA_OP_DISABLERXFLUSH	(1 << 24) /* Do not drop frames
						     when out of RX descr. */
#define	GMAC_DMA_OP_TXSTOREFORWARD	(1 << 21) /* start TX when a
 						    full frame is available */
#define	GMAC_DMA_OP_FLUSHTX		(1 << 20) /* flush TX fifo */
#define	GMAC_DMA_OP_TXSTART		(1 << 13) /* start TX DMA engine */
#define	GMAC_DMA_OP_RXSTART		(1 << 1)  /* start RX DMA engine */

#define	GMAC_DMA_INT_NIE		(1 << 16) /* Normal/Summary */
#define	GMAC_DMA_INT_AIE		(1 << 15) /* Abnormal/Summary */
#define	GMAC_DMA_INT_ERE		(1 << 14) /* Early receive */
#define	GMAC_DMA_INT_FBE		(1 << 13) /* Fatal bus error */
#define	GMAC_DMA_INT_ETE		(1 << 10) /* Early transmit */
#define	GMAC_DMA_INT_RWE		(1 << 9)  /* Receive watchdog */
#define	GMAC_DMA_INT_RSE		(1 << 8)  /* Receive stopped */
#define	GMAC_DMA_INT_RUE		(1 << 7)  /* Receive buffer unavail. */
#define	GMAC_DMA_INT_RIE		(1 << 6)  /* Receive interrupt */
#define	GMAC_DMA_INT_UNE		(1 << 5)  /* Tx underflow */
#define	GMAC_DMA_INT_OVE		(1 << 4)  /* Receive overflow */
#define	GMAC_DMA_INT_TJE		(1 << 3)  /* Transmit jabber */
#define	GMAC_DMA_INT_TUE		(1 << 2)  /* Transmit buffer unavail. */
#define	GMAC_DMA_INT_TSE		(1 << 1)  /* Transmit stopped */
#define	GMAC_DMA_INT_TIE		(1 << 0)  /* Transmit interrupt */

#define	GMAC_DMA_INT_MASK		0x1ffff   /* all possible intr bits */

struct dwc_gmac_dev_dmadesc {
	uint32_t ddesc_status;
/* both: */
#define	DDESC_STATUS_OWNEDBYDEV		(1U << 31)

/* for RX descriptors */
#define	DDESC_STATUS_DAFILTERFAIL	(1 << 30)
#define	DDESC_STATUS_FRMLENMSK		0x3fff
#define	DDESC_STATUS_FRMLENSHIFT	16
#define	DDESC_STATUS_RXERROR		(1 << 15)
#define	DDESC_STATUS_RXTRUNCATED	(1 << 14)
#define	DDESC_STATUS_SAFILTERFAIL	(1 << 13)
#define	DDESC_STATUS_RXIPC_GIANTFRAME	(1 << 12)
#define	DDESC_STATUS_RXDAMAGED		(1 << 11)
#define	DDESC_STATUS_RXVLANTAG		(1 << 10)
#define	DDESC_STATUS_RXFIRST		(1 << 9)
#define	DDESC_STATUS_RXLAST		(1 << 8)
#define	DDESC_STATUS_RXIPC_GIANT	(1 << 7)
#define	DDESC_STATUS_RXCOLLISION	(1 << 6)
#define	DDESC_STATUS_RXFRAMEETHER	(1 << 5)
#define	DDESC_STATUS_RXWATCHDOG		(1 << 4)
#define	DDESC_STATUS_RXMIIERROR		(1 << 3)
#define	DDESC_STATUS_RXDRIBBLING	(1 << 2)
#define	DDESC_STATUS_RXCRC		(1 << 1)

	uint32_t ddesc_cntl;

/* for TX descriptors */
#define	DDESC_CNTL_TXINT		(1U << 31)
#define	DDESC_CNTL_TXLAST		(1 << 30)
#define	DDESC_CNTL_TXFIRST		(1 << 29)
#define	DDESC_CNTL_TXCHECKINSCTRL	__BITS(27,28)

#define	    DDESC_TXCHECK_DISABLED	0
#define	    DDESC_TXCHECK_IP		1
#define	    DDESC_TXCHECK_IP_NO_PSE	2
#define	    DDESC_TXCHECK_FULL		3

#define	DDESC_CNTL_TXCRCDIS		(1 << 26)
#define	DDESC_CNTL_TXRINGEND		(1 << 25)
#define	DDESC_CNTL_TXCHAIN		(1 << 24)
#define	DDESC_CNTL_TXDISPAD		(1 << 23)

/* for RX descriptors */
#define	DDESC_CNTL_RXINTDIS		(1U << 31)
#define	DDESC_CNTL_RXRINGEND		(1 << 25)
#define	DDESC_CNTL_RXCHAIN		(1 << 24)

/* both */
#define	DDESC_CNTL_SIZE1MASK		0x7ff
#define	DDESC_CNTL_SIZE1SHIFT		0
#define	DDESC_CNTL_SIZE2MASK		0x7ff
#define	DDESC_CNTL_SIZE2SHIFT		11

	uint32_t ddesc_data;	/* pointer to buffer data */
	uint32_t ddesc_next;	/* link to next descriptor */
};
