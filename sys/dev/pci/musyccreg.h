/*	$OpenBSD: musyccreg.h,v 1.3 2005/08/27 12:53:17 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Claudio Jeker <jeker@accoom.net>
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
#ifndef __MUSYCCREG_H__
#define __MUSYCCREG_H__

#define	MUSYCC_PCI_BAR	0x10	/* offset of Base Address Register */

/* Group Base Pointer -- per Group unique */
#define	MUSYCC_GROUPBASE(x)	(0x0800 * (x))
/* Dual Address Cycle Base Pointer */
#define MUSYCC_DACB_PTR		0x0004
/* Service Request Descriptor -- per Group unique */
#define MUSYCC_SERREQ(x)	(0x0008 + 0x0800 * (x))
/* Interrupt Status Descriptor */
#define MUSYCC_INTRSTATUS	0x000c
#define MUSYCC_INTCNT_MASK	0x00007fff
#define MUSYCC_INTFULL		0x00008000
#define MUSYCC_NEXTINT_GET(x)	(((x) >> 16) & 0x7fff)
#define MUSYCC_NEXTINT_SET(x)	(((x) & 0x7fff) << 16)

/* Global Configuration Descriptor */
#define MUSYCC_GLOBALCONF	0x0600
/* Interrupt Queue Descriptor */
#define MUSYCC_INTQPTR		0x0604
#define MUSYCC_INTQLEN		0x0608

/* group structure [page 5-6], this puppy needs to be 2k aligned */
struct musycc_grpdesc {
	u_int32_t		tx_headp[32];	/* transmit head ptr */
	u_int32_t		tx_msgp[32];	/* transmit msg ptr */
	u_int32_t		rx_headp[32];	/* receive head ptr */
	u_int32_t		rx_msgp[32];	/* receive msg ptr */
	u_int8_t		tx_tsmap[128];	/* transmit timeslot map */
	u_int8_t		tx_submap[256];	/* transmit sub channel map */
	u_int32_t		tx_cconf[32];	/* transmit channel config */
	u_int8_t		rx_tsmap[128];	/* receive timeslot map */
	u_int8_t		rx_submap[256];	/* receive sub channel map */
	u_int32_t		rx_cconf[32];	/* receive channel config */
	u_int32_t		global_conf;	/* global config */
	u_int32_t		int_queuep;	/* interrupt queue ptr */
	u_int32_t		int_queuelen;	/* interrupt queue len */
	u_int32_t		group_conf;	/* group config */
	u_int32_t		memprot;	/* memory protection */
	u_int32_t		msglen_conf;	/* message length config */
	u_int32_t		port_conf;	/* serial port config */
};

/* Global Configuration Descriptor [page 5-10] */
#define MUSYCC_CONF_PORTMAP	0x00000003	/* group -> port mapping */
#define MUSYCC_CONF_INTB	0x00000004	/* if set INTB is disabled */
#define MUSYCC_CONF_INTA	0x00000008	/* if set INTA is disabled */
#define MUSYCC_CONF_ELAPSE_GET(x)	\
    (((x) >> 4) & 0x7)				/* get elapse value */
#define MUSYCC_CONF_ELAPSE_SET(x)	\
    ((x & 0x7) << 4)				/* set elapse value */
#define MUSYCC_CONF_ALAPSE_GET(x)	\
    (((x) >> 8) & 0x3)				/* get alapse value */
#define MUSYCC_CONF_ALAPSE_SET(x)	\
    ((x & 0x3) << 8)				/* set alapse value */
#define MUSYCC_CONF_MPUSEL	0x00000400	/* EBUS mode, 1 = intel style */
#define MUSYCC_CONF_ECKEN	0x00000800	/* EBUS clock enable */
#define MUSYCC_CONF_BLAPSE_GET(x)	\
    (((x) >> 12) & 0x7)				/* get blapse value */
#define MUSYCC_CONF_BLAPSE_SET(x)	\
    ((x & 0x7) << 12)				/* set blapse value */

/* Interrupt Descriptor [page 5-41] */
#define	MUSYCC_INTD_BLEN	0x00001fff	/* size of data on EOB & EOM */
#define	MUSYCC_INTD_ILOST	0x00008000	/* Interrupt Lost */
#define	MUSYCC_INTD_DIR		0x80000000	/* transmit specific int */
#define	MUSYCC_INTD_GRP(x)	\
    ((((x) >> 29) & 0x3) | (((x) >> 12) & 0x4))	/* Group Number [0-7] */
#define	MUSYCC_INTD_CHAN(x)	\
    (((x) >> 24) & 0x1f)			/* Channel Number [0-31] */
#define	MUSYCC_INTD_EVENT(x)	\
    (((x) >> 20) & 0xf)				/* Event that caused the int */
#define	MUSYCC_INTD_ERROR(x)	\
    (((x) >> 16) & 0xf)				/* Error that caused the int */

/* possible Interrupt Events */
#define	MUSYCC_INTEV_NONE	0		/* No Event to report */
#define	MUSYCC_INTEV_SACK	1		/* Service Request Ack */
#define MUSYCC_INTEV_EOB	2		/* End of Buffer */
#define MUSYCC_INTEV_EOM	3		/* End of Message */
#define MUSYCC_INTEV_EOP	4		/* End of Padfill */
#define MUSYCC_INTEV_CHABT	5		/* Change to Abort Code */
#define MUSYCC_INTEV_CHIC	6		/* Change to Idle Code */
#define MUSYCC_INTEV_FREC	7		/* Frame Recovery */
#define MUSYCC_INTEV_SINC	8		/* SS7 SUERM Octet Count inc */
#define MUSYCC_INTEV_SDEC	9		/* SS7 SUERM Octet Count dec */
#define MUSYCC_INTEV_SFILT	10		/* SS7 Filtered Message */

/* possible Interrupt Errors */
#define	MUSYCC_INTERR_NONE	0		/* No Error to report */
#define	MUSYCC_INTERR_BUFF	1		/* Buffer Error */
#define	MUSYCC_INTERR_COFA	2		/* Change of Frame Alignment */
#define	MUSYCC_INTERR_ONR	3		/* Owner-Bit Error */
#define	MUSYCC_INTERR_PROT	4		/* Mem Protection Violation */
#define	MUSYCC_INTERR_OOF	8		/* Out of Frame */
#define	MUSYCC_INTERR_FCS	9		/* Frame Check Sequence Error */
#define	MUSYCC_INTERR_ALIGN	10		/* Octet Alignment Error */
#define	MUSYCC_INTERR_ABT	11		/* Abort Termination */
#define	MUSYCC_INTERR_LNG	12		/* Long Message */
#define	MUSYCC_INTERR_SHT	13		/* Short Message */
#define	MUSYCC_INTERR_SUERR	14		/* SS7 Signal Unit Error */
#define	MUSYCC_INTERR_PERR	15		/* PCI Bus Parity Error */

/* Service Request Descriptor [page 5-14] */
#define MUSYCC_SREQ_MASK	0x001f		/* Generic SREQ/Channel Mask */
#define MUSYCC_SREQ_CHSET(x)		\
    ((x) & MUSYCC_SREQ_MASK)			/* shortcut */
#define MUSYCC_SREQ_TXDIR	0x0020		/* Transmit Direction */
#define MUSYCC_SREQ_SET(x)		\
    (((x) & MUSYCC_SREQ_MASK) << 8)		/* Service Request */

#define MUSYCC_SREQ_RX		0x1		/* Receive Request */
#define MUSYCC_SREQ_TX		0x2		/* Transmit Request */
#define MUSYCC_SREQ_BOTH	0x3		/* both directions */
#define MUSYCC_SREQ_NOWAIT	0x8
#define MUSYCC_SREQ_NONE	0xffffffff

/* Group Configuration Descriptor [page 5-16] */
#define MUSYCC_GRCFG_RXENBL	0x0001		/* Receiver Enabled */
#define MUSYCC_GRCFG_TXENBL	0x0002		/* Transmitter Enabled */
#define MUSYCC_GRCFG_SUBDSBL	0x0004		/* Subchanneling Disabled */
#define MUSYCC_GRCFG_OOFABT	0x0008		/* OOF Message Processing */
#define MUSYCC_GRCFG_MSKOOF	0x0010		/* OOF Interrupt Disabled */
#define MUSYCC_GRCFG_MSKCOFA	0x0020		/* COFA Interrupt Disabled */
#define MUSYCC_GRCFG_MCENBL	0x0040		/* Msg Config Bits Copy */
#define MUSYCC_GRCFG_INHRBSD	0x0100		/* Inihibit RX Buf Stat Desc */
#define MUSYCC_GRCFG_INHTBSD	0x0200		/* Inihibit TX Buf Stat Desc */
#define MUSYCC_GRCFG_POLL16	0x0400		/* Poll at all 16 frame sync */
#define MUSYCC_GRCFG_POLL32	0x0800		/* Poll at all 32 frame sync */
#define MUSYCC_GRCFG_POLL64	0x0C00		/* Poll at all 64 frame sync */
#define MUSYCC_GRCFG_SFALIGN	0x8000		/* Super Frame Alignment */
#define MUSYCC_GRCFG_SUETMASK	0x3f0000	/* SS7 SUERR Threshold */

/* Port Configuration Descriptor [page 5-19] */
#define MUSYCC_PORT_MODEMASK	0x007		/* Port Mode Mask */
#define MUSYCC_PORT_MODE_T1	0		/* T1 - 24 time slots */
#define MUSYCC_PORT_MODE_E1	1		/* E1 - 32 time slots */
#define MUSYCC_PORT_MODE_2E1	2		/* 2*E1 - 64 time slots */
#define MUSYCC_PORT_MODE_4E1	3		/* 4*E1 - 128 time slots */
#define MUSYCC_PORT_MODE_N64	4		/* N*64 mode */
#define MUSYCC_PORT_TDAT_EDGE	0x010		/* TX Data on rising Edge */
#define MUSYCC_PORT_TSYNC_EDGE	0x020		/* TX Frame Sync on rising E */
#define MUSYCC_PORT_RDAT_EDGE	0x040		/* RX Data on rising Edge */
#define MUSYCC_PORT_RSYNC_EDGE	0x080		/* RX Frame Sync on rising E */
#define MUSYCC_PORT_ROOF_EDGE	0x100		/* RX OOF on rising Edge */
#define MUSYCC_PORT_TRITX	0x200		/* TX Three-state disabled */

/* Message Length Descriptor [page 5-20] */
#define MUSYCC_MAXFRM_MAX	4094		/* maximum message length */
#define MUSYCC_MAXFRM_MASK	0x0fff
#define MUSYCC_MAXFRM2_SHIFT	16

/* Time Slot Descriptor [page 5-23] */
#define MUSYCC_TSLOT_ENABLED	0x80		/* timeslot enabled */
#define MUSYCC_TSLOT_56K	0x20		/* 56kbps timeslots */
#define MUSYCC_TSLOT_SUB	0x40		/* subchannel timeslots */
#define MUSYCC_TSLOT_MASK	0x1f		/* channel number mask */
#define MUSYCC_TSLOT_CHAN(x)		\
    ((x) & MUSYCC_TSLOT_MASK)			/* masked channel number */

/* Channel Configuration Descriptor [page 5-27] */
#define MUSYCC_CHAN_MSKBUFF	0x00000002	/* BUFF & ONR Intr disabled */
#define MUSYCC_CHAN_MSKEOM	0x00000004	/* EOM Interrupt disabled */
#define MUSYCC_CHAN_MSKMSG	0x00000008	/* LNG, FCS, ALIGN, ABT mask */
#define MUSYCC_CHAN_MSKIDLE	0x00000010	/* CHABT, CHIC, SHT Intr mask */
#define MUSYCC_CHAN_MSKSFILT	0x00000020	/* SS7 SFILT Interrupt mask */
#define MUSYCC_CHAN_MSKSDEC	0x00000040	/* SS7 SDEC Interrupt mask */
#define MUSYCC_CHAN_MSKSINC	0x00000080	/* SS7 SINC Interrupt mask */
#define MUSYCC_CHAN_MSKSUERR	0x00000100	/* SS7 SUERR Interrupt mask */
#define MUSYCC_CHAN_FCS		0x00000200	/* FCS checksum disable */
#define MUSYCC_CHAN_MAXLEN1	0x00000400	/* Msg Len Max via MAXFRM1 */
#define MUSYCC_CHAN_MAXLEN2	0x00000800	/* Msg Len Max via MAXFRM1 */
#define MUSYCC_CHAN_EOPI	0x00008000	/* End of Padfill Int enable */
#define MUSYCC_CHAN_INV		0x00800000	/* Data Inversion */
#define MUSYCC_CHAN_PADJ	0x80000000	/* Pad Count Adjust enabled */

#define MUSYCC_CHAN_PROTO_GET(x)	\
    (((x) >> 12) & 0x7)				/* get line protocol */
#define MUSYCC_CHAN_PROTO_SET(x)	\
    ((x & 0x7) << 12)				/* set line protocol */
#define MUSYCC_PROTO_TRANSPARENT	0	/* raw stream */
#define MUSYCC_PROTO_SS7HDLC		1	/* SS7 HDLC messages */
#define MUSYCC_PROTO_HDLC16		2	/* basic HDLC with 16 bit FCS */
#define MUSYCC_PROTO_HDLC32		3	/* basic HDLC with 32 bit FCS */

#define MUSYCC_CHAN_BUFLEN_GET(x)	\
    (((x) >> 16) & 0x3f)			/* get FIFO Buffer Length */
#define MUSYCC_CHAN_BUFLEN_SET(x)	\
    (((x) & 0x3F) << 16)			/* set FIFO Buffer Length */
#define MUSYCC_CHAN_BUFIDX_GET(x)	\
    (((x) >> 24) & 0x3f)			/* get FIFO Buffer Index */
#define MUSYCC_CHAN_BUFIDX_SET(x)	\
    (((x) & 0x3F) << 24)			/* set FIFO Buffer Index */


/* Tx / Rx Buffer Descriptor [page 5-33] */
#define MUSYCC_STATUS_LEN(x)		\
    ((x) & 0x3fff)				/* length of dma buffer */
#define MUSYCC_STATUS_REPEAT	0x00008000	/* repeat buffer */
#define MUSYCC_STATUS_ERROR	0x000f0000
#define MUSYCC_STATUS_EOBI	0x10000000	/* end of buffer interrupt */
#define MUSYCC_STATUS_EOM	0x20000000	/* end of message */
#define MUSYCC_STATUS_NOPOLL	0x40000000	/* don't poll for new descr */
#define MUSYCC_STATUS_OWNER	0x80000000


/*
 * ROM data structures
 */

struct musycc_rom {
	u_int16_t	magic;
#define MUSYCC_ROM_MAGIC	(htons(0xacc0))
	u_int8_t	rev;			/* rev. of the card */
	u_int8_t	vers;			/* version of the rom */
	char		product[64];
	u_int8_t	portmap;		/* portmap config */
	u_int8_t	portmode;		/* port mode e.g. 2*E1 */
	u_int8_t	numframer;		/* # of sub-configs */
	u_int8_t	ledmask;		/* mask for led register */
	u_int32_t	ledbase;		/* base of the led register */
	u_int32_t	rfu[2];			/* RFU */
};

struct musycc_rom_framer {
	u_int32_t	type;
	u_int32_t	base;
	u_int32_t	size;
	u_int8_t	gnum;
	u_int8_t	port;
	char		slot;
	u_int8_t	flags;
	u_int32_t	rfu[2];			/* RFU */
};
#endif
