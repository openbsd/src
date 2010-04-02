/*-
 * Copyright (c) 2009, 2010 Christopher Zimmermann <madroach@zakweb.de>
 * Copyright (c) 2007, 2008 Alexander Pohoyda <alexander.pohoyda@gmx.net>
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL AUTHORS OR
 * THE VOICES IN THEIR HEADS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

struct se_desc {
	volatile u_int32_t	se_sts_size;
	volatile u_int32_t	se_cmdsts;
	volatile u_int32_t	se_ptr;
	volatile u_int32_t	se_flags;
};

#define SE_RX_RING_CNT		1000 /* [8, 1024] */
#define SE_TX_RING_CNT		1000 /* [8, 8192] */

#define SE_RX_RING_SZ		(SE_RX_RING_CNT * sizeof(struct se_desc))
#define SE_TX_RING_SZ		(SE_TX_RING_CNT * sizeof(struct se_desc))

struct se_list_data {
	struct se_desc	*se_rx_ring;
	struct se_desc	*se_tx_ring;
	bus_dmamap_t		se_rx_dmamap;
	bus_dmamap_t		se_tx_dmamap;
};

struct se_chain_data {
	struct mbuf		*se_rx_mbuf[SE_RX_RING_CNT];
	struct mbuf		*se_tx_mbuf[SE_TX_RING_CNT];
	bus_dmamap_t		se_rx_map[SE_RX_RING_CNT];
	bus_dmamap_t		se_tx_map[SE_TX_RING_CNT];
	int			se_rx_prod;
	int			se_tx_prod;
	int			se_tx_cons;
	int			se_tx_cnt;
};

struct se_softc {
    	struct device		sc_dev;
	void			*sc_ih;
	struct ifnet		*se_ifp;	/* interface info */
	struct resource		*se_res[2];
	void			*se_intrhand;
	mii_data_t		sc_mii;
	struct arpcom		arpcom;

	u_int8_t		se_link;

	bus_space_handle_t	se_bhandle;
	bus_space_tag_t		se_btag;
	bus_dma_tag_t		se_tag;

	struct se_list_data	se_ldata;
	struct se_chain_data	se_cdata;

	struct timeout		se_timeout;
	int			in_tick;
	int			se_watchdog_timer;
	int			se_stopped;
};


#define SE_PCI_LOMEM		0x10

enum sis19x_registers {
	TxControl		= 0x00,
	TxDescStartAddr		= 0x04,
	Reserved0		= 0x08, // unused
	TxNextDescAddr		= 0x0c,	// unused

	RxControl		= 0x10,
	RxDescStartAddr		= 0x14,
	Reserved1		= 0x18, // unused
	RxNextDescAddr		= 0x1c,	// unused

	IntrStatus		= 0x20,
	IntrMask		= 0x24,
	IntrControl		= 0x28,
	IntrTimer		= 0x2c,	// unused

	PMControl		= 0x30,	// unused
	Reserved2		= 0x34, // unused
	ROMControl		= 0x38,
	ROMInterface		= 0x3c,
	StationControl		= 0x40,
	GMIIControl		= 0x44,
	GMacIOCR		= 0x48,
	GMacIOCTL		= 0x4c,
	TxMacControl		= 0x50,
	TxMacTimeLimit		= 0x54,
	RGMIIDelay		= 0x58,
	Reserved3		= 0x5c,
	RxMacControl		= 0x60, // 1  WORD
	RxMacAddr		= 0x62, // 6x BYTE
	RxHashTable		= 0x68, // 1 LONG
	RxHashTable2		= 0x6c, // 1 LONG
	RxWakeOnLan		= 0x70,
	RxWakeOnLanData		= 0x74,
	RxMPSControl		= 0x78,
	Reserved4		= 0x7c,
};

enum sis19x_register_content {
	/* IntrStatus */
	SoftInt			= 0x40000000,	// unused
	Timeup			= 0x20000000,	// unused
	PauseFrame		= 0x00080000,	// unused
	MagicPacket		= 0x00040000,	// unused
	WakeupFrame		= 0x00020000,	// unused
	LinkChange		= 0x00010000,
	RxQEmpty		= 0x00000080,			//! RXIDLE
	RxQInt			= 0x00000040,			//! RXDONE
	TxQ1Empty		= 0x00000020,	// unused
	TxQ1Int			= 0x00000010,	// unused
	TxQEmpty		= 0x00000008,			//! TXIDLE
	TxQInt			= 0x00000004,			//! TXDONE
	RxHalt			= 0x00000002,			//! RXHALT
	TxHalt			= 0x00000001,			//! TXHALT

	/* RxStatusDesc */
	RxRES			= 0x00200000,	// unused
	RxCRC			= 0x00080000,
	RxRUNT			= 0x00100000,	// unused
	RxRWT			= 0x00400000,	// unused

	/* {Rx/Tx}CmdBits */
	CmdReset		= 0x10,
	CmdRxEnb		= 0x01, /* Linux does not use it, but 0x8 */
	CmdTxEnb		= 0x01,

	/* RxMacControl */
	AcceptBroadcast		= 0x0800,
	AcceptMulticast		= 0x0400,
	AcceptMyPhys		= 0x0200,
	AcceptAllPhys		= 0x0100,
	AcceptErr		= 0x0020,	// unused
	AcceptRunt		= 0x0010,	// unused
};

/*
 * register space access macros
 */
#define CSR_WRITE_4(sc, reg, val)	\
	bus_space_write_4(sc->se_btag, sc->se_bhandle, reg, val)
#define CSR_WRITE_2(sc, reg, val)	\
	bus_space_write_2(sc->se_btag, sc->se_bhandle, reg, val)

#define CSR_READ_4(sc, reg)	\
	bus_space_read_4(sc->se_btag, sc->se_bhandle, reg)
#define CSR_READ_2(sc, reg)	\
	bus_space_read_2(sc->se_btag, sc->se_bhandle, reg)

#define SE_PCI_COMMIT()	CSR_READ_4(sc, IntrControl)

#define SE_SETBIT(_sc, _reg, x)				\
	CSR_WRITE_4(_sc, _reg, CSR_READ_4(_sc, _reg) | (x))

#define SE_CLRBIT(_sc, _reg, x)				\
	CSR_WRITE_4(_sc, _reg, CSR_READ_4(_sc, _reg) & ~(x))

#define DISABLE_INTERRUPTS(sc)	CSR_WRITE_4(sc, IntrMask, 0x0)
#define ENABLE_INTERRUPTS(sc)	CSR_WRITE_4(sc, IntrMask, RxQEmpty | RxQInt | \
					    TxQInt | RxHalt | TxHalt)


/*
 * Gigabit Media Independent Interface CTL register
 */
#define	GMI_DATA	0xffff0000
#define	GMI_DATA_SHIFT	16
#define	GMI_REG		0x0000f800
#define	GMI_REG_SHIFT	11
#define	GMI_PHY		0x000007c0
#define	GMI_PHY_SHIFT	6
#define	GMI_OP		0x00000020
#define	GMI_OP_SHIFT		5
#define	GMI_OP_WR	(1 << GMI_OP_SHIFT)
#define	GMI_OP_RD	(0 << GMI_OP_SHIFT)
#define	GMI_REQ		0x00000010
#define	GMI_MDIO	0x00000008 	/* not used */
#define	GMI_MDDIR	0x00000004	/* not used */
#define	GMI_MDC		0x00000002	/* not used */
#define	GMI_MDEN	0x00000001	/* not used */

enum CommandStatus {
	OWNbit		= 0x80000000,
	INTbit		= 0x40000000,
	IPbit		= 0x20000000,
	TCPbit		= 0x10000000,
	UDPbit		= 0x08000000,
	DEFbit		= 0x00200000,
	CRCbit		= 0x00020000,
	PADbit		= 0x00010000,
};


/*
 * RX descriptor status bits
 */
#define	RDS_TAGON	0x80000000
#define	RDS_DESCS	0x3f000000
#define	RDS_ABORT	0x00800000
#define	RDS_SHORT	0x00400000
#define	RDS_LIMIT	0x00200000
#define	RDS_MIIER	0x00100000
#define	RDS_OVRUN	0x00080000
#define	RDS_NIBON	0x00040000
#define	RDS_COLON	0x00020000
#define	RDS_CRCOK	0x00010000
#define	RX_ERR_BITS \
	(RDS_COLON | RDS_NIBON | RDS_OVRUN | RDS_MIIER | \
	 RDS_LIMIT | RDS_SHORT | RDS_ABORT)

#define RING_END		0x80000000

#define SE_RXSIZE(x)		letoh32((x)->se_sts_size & 0x0000ffff)
#define SE_RXSTATUS(x)		letoh32((x)->se_sts_size & 0xffff0000)

#undef SE_OWNDESC
#define SE_OWNDESC(x)		((x)->se_cmdsts & OWNbit)

#define SE_INC(x, y)		(x) = (((x) == ((y)-1)) ? 0 : (x)+1)


/*
 * TX descriptor status bits
 */
#define	TDS_OWC		0x00080000
#define	TDS_ABT		0x00040000
#define	TDS_FIFO	0x00020000
#define	TDS_CRS		0x00010000
#define	TDS_COLLS	0x0000ffff
#define TX_ERR_BITS 	(TDS_OWC | TDS_ABT | TDS_FIFO | TDS_CRS)

/* Taken from Solaris driver */
#define	EI_DATA		0xffff0000 
#define	EI_DATA_SHIFT	16
#define	EI_OFFSET	0x0000fc00
#define	EI_OFFSET_SHIFT	10
#define	EI_OP		0x00000300 
#define	EI_OP_SHIFT	8 
#define	EI_OP_RD	(2 << EI_OP_SHIFT)
#define	EI_OP_WR	(1 << EI_OP_SHIFT)
#define	EI_REQ		0x00000080
#define	EI_DO		0x00000008	/* not used */
#define	EI_DI		0x00000004	/* not used */
#define	EI_CLK		0x00000002	/* not used */
#define	EI_CS		0x00000001

/*
 * EEPROM Addresses
 */
#define	EEPROMSignature	0x00
#define	EEPROMCLK	0x01
#define	EEPROMInfo	0x02
#define	EEPROMMACAddr	0x03
