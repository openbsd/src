/*	$OpenBSD: tropicreg.h,v 1.1 1999/12/27 21:51:35 fgsch Exp $	*/
/*	$NetBSD: tropicreg.h,v 1.3 1999/10/17 23:53:45 cgd Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 IBM Corporation 
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation,
 * and that the name IBM not be used in advertising or publicity 
 * pertaining to distribution of the software without specific, written
 * prior permission.
 * 
 * CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/* $ACIS:if_lanreg.h 12.0$ */

#define TR_SWITCH 0
#define TR_RESET 1
#define TR_RELEASE 2
#define TR_CLEARINT 3

/* macros to deal with accessing the MMIO region */
#define MM_OUTB(sc, off, val)	\
	bus_space_write_1((sc)->sc_memt, (sc)->sc_mmioh, (off), (val))
#define MM_OUTW(sc, off, val)	\
	bus_space_write_2((sc)->sc_memt, (sc)->sc_mmioh, (off), htons((val)))
#define MM_INB(sc, off)		\
	bus_space_read_1((sc)->sc_memt, (sc)->sc_mmioh, (off))
#define MM_INW(sc, off)		\
	ntohs(bus_space_read_2((sc)->sc_memt, (sc)->sc_mmioh, (off)))

/* macros to deal with accesses to the shared ram */
#define SR_INB(sc, off)	\
	bus_space_read_1(sc->sc_memt, sc->sc_sramh, (off))
#define SR_INW(sc, off)	\
	htons(bus_space_read_2(sc->sc_memt, sc->sc_sramh, (off)))
#define SR_OUTB(sc, off, val)	\
	bus_space_write_1((sc)->sc_memt, (sc)->sc_sramh, (off), (val))
#define SR_OUTW(sc, off, val)	\
	bus_space_write_2((sc)->sc_memt, (sc)->sc_sramh, (off), htons((val)))

/* macros to deal with accesses to the recv buffers */
#define RB_INB(sc, rb, reg)	SR_INB(sc, (rb)+(reg))
#define RB_INW(sc, rb, reg)	SR_INW(sc, (rb)+(reg))

/* macros to deal with the ACA */
#define ACA_RDB(sc, reg)	MM_INB(sc, ((sc->sc_aca+(reg))|ACA_RW))
#define ACA_RDW(sc, reg)	MM_INW(sc, ((sc->sc_aca+(reg))|ACA_RW))
#define ACA_OUTB(sc, reg, val)	MM_OUTB(sc, ((sc->sc_aca+(reg))|ACA_RW), (val))
#define ACA_SETB(sc, reg, val)	MM_OUTB(sc, ((sc->sc_aca+(reg))|ACA_SET), (val))
#define ACA_RSTB(sc, reg, val)	MM_OUTB(sc, ((sc->sc_aca+(reg))|ACA_RST), (val))

/* macros to deal with the SSB */
#define SSB_INB(sc, ssb, reg)	SR_INB(sc, (ssb)+(reg))

/* macros to deal with the ARB */
#define ARB_INB(sc, arb, reg)	SR_INB(sc, (arb)+(reg))
#define ARB_INW(sc, arb, reg)	SR_INW(sc, (arb)+(reg))

/* macros to deal with the SRB */
#define SRB_INB(sc, srb, reg)	SR_INB(sc, (srb)+(reg))
#define SRB_INW(sc, srb, reg)	SR_INW(sc, (srb)+(reg))
#define SRB_OUTB(sc, srb, reg, val)	SR_OUTB(sc, (srb)+(reg), (val))
#define SRB_OUTW(sc, srb, reg, val)	SR_OUTW(sc, (srb)+(reg), (val))

/* macros to deal with the ASB */
#define ASB_INB(sc, asb, reg)	SR_INB(sc, (asb)+(reg))
#define ASB_INW(sc, asb, reg)	SR_INW(sc, (asb)+(reg))
#define ASB_OUTB(sc, asb, reg, val)	SR_OUTB(sc, (asb)+(reg), (val))
#define ASB_OUTW(sc, asb, reg, val)	SR_OUTW(sc, (asb)+(reg), (val))

/* macros to deal with the TXCA */
#define TXCA_INW(sc, reg)	SR_INW(sc, sc->sc_txca+(reg))
#define TXCA_OUTW(sc, reg, val)	SR_OUTW(sc, sc->sc_txca+(reg), (val))

/* macros to deal with the txbuffers */
#define TXB_INB(sc, buf, reg)	SR_INB(sc, (buf)+(reg))
#define TXB_INW(sc, buf, reg)	SR_INW(sc, (buf)+(reg))
#define TXB_OUTB(sc, buf, reg, val)	SR_OUTB(sc, (buf)+(reg), (val))
#define TXB_OUTW(sc, buf, reg, val)	SR_OUTW(sc, (buf)+(reg), (val))

/* ACA registers */
#define ACA_RRR		0
#define ACA_RRR_e	ACA_RRR
#define ACA_RRR_o	(ACA_RRR+1)
#define ACA_WRBR	2
#define ACA_WRBR_e	ACA_WRBR
#define ACA_WRBR_o	(ACA_WRBR+1)
#define ACA_WWOR	4
#define ACA_WWOR_e	ACA_WWOR
#define ACA_WWOR_o	(ACA_WWOR+1)
#define ACA_WWCR	6
#define ACA_WWCR_e	ACA_WWCR
#define ACA_WWCR_o	(ACA_WWCR+1)
#define ACA_ISRP	8
#define ACA_ISRP_e	ACA_ISRP
#define ACA_ISRP_o	(ACA_ISRP+1)
#define ACA_ISRA	10
#define ACA_ISRA_e	ACA_ISRA
#define ACA_ISRA_o	(ACA_ISRA+1)
#define ACA_TCR		12
#define ACA_TCR_e	ACA_TCR
#define ACA_TCR_o	(ACA_TCR+1)
#define ACA_TVR		14
#define ACA_TVR_e	ACA_TVR
#define ACA_TVR_o	(ACA_TVR+1)

/* access flags; to be or-ed into offset */
#define ACA_RW		0
#define ACA_RST		0x20
#define ACA_SET		0x40

/* offsets valid for all command blocks */
#define CMD_CMD		0
#define CMD_RETCODE	2

/*
 * Structure of SSB (System Status Block)
 */
#define SSB_SIZE	20		/* size of SSB */
#define SSB_CMD		0
#define SSB_CMDCORR	1
#define SSB_RETCODE	2
#define SSB_STATIONID	4
#define SSB_XMITERR	6

/*
 * Structure of ARB (Adapter Request Block)
 */
#define ARB_SIZE	28		/* size of ARB */
#define ARB_CMD		0
#define ARB_STATIONID	4		/* ID of receiving station */

/* receive data command block */
#define ARB_RXD_BUFADDR	6		/* RAM offset of 1st rec buf */
#define ARB_RXD_LANHDRLEN	8	/* Length of LAN header */
#define ARB_RXD_DLCHDRLEN	9	/* Length of DLC header */
#define ARB_RXD_FRAMELEN	10	/* Length of entire frame */
#define ARB_RXD_MSGTYPE		12	/* Category of message */

/* transmit data command block */
#define ARB_XMT_CMDCORR		1
#define ARB_XMT_DHBADDR		6

/* ring status change information block */
#define ARB_RINGSTATUS		6

/* DLC status change response block */
#define ARB_DLCSTAT_STATUS	6	/* status info field */ 
#define ARB_DLCSTAT_FRMRDATA	8	/* 5 bytes */
#define ARB_DLCSTAT_ACCPRIO	13
#define ARB_DLCSTAT_REMADDR	14	/* remote address */
#define ARB_DLCSTAT_REMSAP	20	/* remote sap */

/*
 * Structure of SRB (System Request Block)
 */
#define SRB_SIZE	28		/* size of SRB */
#define SRB_CMD	0
#define SRB_RETCODE	2

/* contents of SRB after adapter reset */
#define		INIT_COMPL		0x80	/* in SRB_CMD */
#define SRB_INIT_STATUS	1
#define		RSP_DETECT		0x40
#define		FAST_PATH_TRANSMIT	0x20
#define		RING_MEASUREMENT	0x08
#define		RPL			0x02
#define		RSP_16			0x01
#define SRB_INIT_STATUS2	2
#define		PNP			0x80
#define		SET_DEF_RSP		0x40
#define		AUTO_DEF_RSP_UPDATE	0x20
#define SRB_INIT_BUC	6		/* bring up code */
#define SRB_INIT_ENCADDR	8	/* offset of adapter's */
					/* permanent encoded address */
#define SRB_INIT_LVLADDR	10	/* offset of adapter's */
					/* microcode level */
#define SRB_INIT_ADAPADDR	12	/* offset of adapter addresses */
#define SRB_INIT_PARMSADDR	14	/* offset of adapter parameters */
#define SRB_INIT_MACADDR	16	/* offset of adapter MAC buffer */
#define SRB_INIT_UTILADDR	18	/* offset of ring utilization measurement */

/* config fast path ram command block */
#define SRB_CFP_CMDSIZE		12	/* length of command block */
#define SRB_CFP_RAMSIZE		8
#define SRB_CFP_BUFSIZE		10

/* config fast path transmit response block */
#define SRB_CFPRESP_FPXMIT	8	/* offset to transmit control area */
#define SRB_CFPRESP_SRBADDR	10	/* offset for the next SRB address */

/* open command block */
#define SRB_OPEN_CMDSIZE	60
#define SRB_OPEN_OPTIONS	8	/* open options */
#define SRB_OPEN_NODEADDR	10	/* adapter's ring address */
#define SRB_OPEN_GROUPADDR	16	/* adapter's group address */
#define SRB_OPEN_FUNCTADDR	20	/* adapter's functional address */
#define SRB_OPEN_NUMRCVBUF	24	/* number of receive buffers */
#define SRB_OPEN_RCVBUFLEN	26	/* length of receive buffers */
#define SRB_OPEN_DHBLEN		28	/* length of DHBs */
#define SRB_OPEN_NUMDHB		30	/* number of DHBs */
#define SRB_OPEN_DLCMAXSAP	32	/* max. number of SAPs */
#define SRB_OPEN_DLCMAXSTA	33	/* max. number of link stations */
#define SRB_OPEN_DLCMAXGSAP	34	/* max. number of group SAPs */
#define SRB_OPEN_DLCMAXGMEM	35	/* max. members per group SAP */
#define SRB_OPEN_DLCT1TICK1	36	/* timer T1 intvl. group one */
#define SRB_OPEN_DLCT2TICK1	37	/* timer T7 intvl. group one */
#define SRB_OPEN_DLCTITICK1	38	/* timer Ti intvl. group one */
#define SRB_OPEN_DLCT1TICK2	39	/* timer T1 intvl. group two */
#define SRB_OPEN_DLCT2TICK2	40	/* timer T7 intvl. group two */
#define SRB_OPEN_DLCTITICK2	41	/* timer Ti intvl. group two */
#define SRB_OPEN_PODUCTID	42	/* product id (18 bytes) */

/* open command response block */
#define SRB_OPENRESP_ERRCODE	6
#define SRB_OPENRESP_ASBADDR	8	/* offset of ASB */
#define SRB_OPENRESP_SRBADDR	10	/* offset of SRB */
#define SRB_OPENRESP_ARBADDR	12	/* offset of ARB */
#define SRB_OPENRESP_SSBADDR	14	/* offset of SSB */

/* open sap command and response block */
#define SRB_OPNSAP_STATIONID	4	/* ID of SAP after open */
#define SRB_OPNSAP_TIMERT1	6	/* response timer */
#define SRB_OPNSAP_TIMERT2	7	/* acknowledge timer */
#define SRB_OPNSAP_TIMERTI	8	/* inactivity timer */
#define SRB_OPNSAP_MAXOUT	9	/* max. xmits without ack */
#define SRB_OPNSAP_MAXIN	10	/* max. recvs without ack */
#define SRB_OPNSAP_MAXOUTINCR	11	/* window increment value */
#define SRB_OPNSAP_MAXRETRY	12	/* N2 value */
#define SRB_OPNSAP_GSAPMAXMEMB	13	/* max. SAPs for a group SAP */
#define SRB_OPNSAP_MAXIFIELD	14	/* max recv info field length */
#define SRB_OPNSAP_SAPVALUE	16	/* SAP to be opened */
#define SRB_OPNSAP_SAPOPTIONS	17	/* options to be set */
#define SRB_OPNSAP_STATIONCNT	18	/* num of link stations to reserve */
#define SRB_OPNSAP_SAPGSAPMEMB	19	/* number of GSAP members */
#define SRB_OPNSAP_GSAP1	20	/* first gsap request */

/* read log command and response block */
#define SRB_RLOG_LOGDATA	14	/* 14 bytes of log data */
#define SRB_LOG_LINEERRS	(SRB_RLOG_LOGDATA+0)	/* line errors */
#define SRB_LOG_INTERRS		(SRB_RLOG_LOGDATA+1)	/* internal errors */
#define SRB_LOG_BRSTERRS	(SRB_RLOG_LOGDATA+2)	/* burst errors */
#define SRB_LOG_ACERRS		(SRB_RLOG_LOGDATA+3)	/* AC errors */
#define SRB_LOG_ABRTERRS	(SRB_RLOG_LOGDATA+4)	/* abort errors */
#define SRB_LOG_LOSTFRMS	(SRB_RLOG_LOGDATA+6)	/* lost frames */
#define SRB_LOG_RCVCONG		(SRB_RLOG_LOGDATA+7)	/* receive congestion count */
#define SRB_LOG_FCPYERRS	(SRB_RLOG_LOGDATA+8)	/* frame copied errors */
#define SRB_LOG_FREQERRS	(SRB_RLOG_LOGDATA+9)	/* frequency erros  */
#define SRB_LOG_TOKENERRS	(SRB_RLOG_LOGDATA+10)	/* token errors */

/* set default ring speed command */
#define SRB_SET_DEFRSP		6

/*
 * Structure of ASB (Adapter Status Block)
 */
#define ASB_SIZE	12		/* size of ASB */
#define RECV_CMD	0
#define RECV_RETCODE	2
#define RECV_STATIONID	4

#define RECV_RESP_RECBUFADDR	6

/* host response to xmit-req-data command */
#define XMIT_CMD	0
#define XMIT_CMDCORR	1		/* command correlator */
#define XMIT_RETCODE	2		/* return code */
#define XMIT_STATIONID	4		/* id of sending station */
#define XMIT_FRAMELEN	6		/* length of entire frame */
#define XMIT_HDRLEN	8		/* length of LAN header */
#define XMIT_REMSAP	9		/* remote SAP */
#define XMIT_DATA	10		/* offset of first data byte */
/* fast path specific data */
#define XMIT_LASTBUF	12
#define XMIT_FRAMEPTR	14
#define XMIT_NEXTBUF	16
#define XMIT_STATUS	18
#define XMIT_STRIPFS	19
#define XMIT_BUFLEN	20
#define XMIT_FP_DATA	22		/* offset of first data byte */

#if 0	/* XXXchb unused? */
/*
 *	Adapter addresses
 */
struct adapt_addr {
	unsigned char   node_addr[6];	/* Adapter node address */
	unsigned char   grp_addr[4];	/* Adapter group address */
	unsigned char   func_addr[4];	/* Adapter functional address */
};

/*
 *	Adapter parameters
 */
struct param_addr {
	unsigned char   phys_addr[4];	/* Adapter physical address */
	unsigned char   up_node_addr[6];	/* Next active upstream node
					 * addr */
	unsigned char   up_phys_addr[4];	/* Next active upstream phys
					 * addr */
	unsigned char   poll_addr[6];	/* Last poll address */
	unsigned char   res0[2];/* Reserved */
	unsigned char   acc_priority[2];	/* Transmit access priority */
	unsigned char   src_class[2];	/* Source class authorization */
	unsigned char   att_code[2];	/* Last attention code */
	unsigned char   src_addr[6];	/* Last source address */
	unsigned char   bcon_type[2];	/* Last beacon type */
	unsigned char   major_vector[2];	/* Last major vector */
	unsigned char   ring_stat[2];	/* ring status */
	unsigned char   soft_error[2];	/* soft error timer value */
	unsigned char   fe_error[2];	/* front end error counter */
	unsigned char   next_state[2];	/* next state indicator */
	unsigned char   mon_error[2];	/* Monitor error code */
	unsigned char   bcon_xmit[2];	/* Beacon transmit type */
	unsigned char   bcon_receive[2];	/* Beacon receive type */
	unsigned char   frame_correl[2];	/* Frame correlator save */
	unsigned char   bcon_naun[6];	/* beacon station NAUN */
	unsigned char   res1[4];/* Reserved */
	unsigned char   bcon_phys[4];	/* Beacon station physical addr */
};
#endif

#define TXCA_BUFFER_COUNT	0
#define TXCA_FREE_QUEUE_HEAD	2
#define TXCA_FREE_QUEUE_TAIL	4
#define TXCA_ADAPTER_QUEUE_HEAD	6
#define TXCA_BUFFER_SIZE	8
#define TXCA_COMPLETION_QUEUE_TAIL	10

/* Adapter receive buffer structure */
#define RB_NEXTBUF	2	/* offset of next buf plus 2 in sram */
#define RB_FS		5	/* FS/addr match (last buf only) */
#define RB_BUFLEN	6	/* length of data in this buffer */
#define RB_DATA		8	/* RCV_BUF_DLEN bytes frame data */

/* Misc. structure sizes. */
#define SAPCB_SIZE	64	/* size of SAP control block */
#define LSCB_SIZE	144	/* size of DLC link station control block */

/* memory in shared ram area that is reserved by the adapter */
#define PRIVRAM_SIZE	1416	/* adapter private ram area */
#define RESVDMEM_SIZE	(PRIVRAM_SIZE+ARB_SIZE+SSB_SIZE+SRB_SIZE+ASB_SIZE)

/* Memory offsets of adapter control areas */

#define TR_SRAM_DEFAULT	0xd8000

/* Offset of MMIO region */
#define TR_MMIO_OFFSET	0x80000 
#define TR_MMIO_MINADDR	0xc0000
#define TR_MMIO_MAXADDR 0xde000
#define TR_MMIO_SIZE	8192

#define	TR_ACA_OFFSET	0x1e00	/* Offset of ACA in MMIO region */
/*
 * XXX Create AIP structure 
 */
#define TR_MAC_OFFSET	0x1f00	/* Offset of MAC address in MMIO region */
#define TR_ID_OFFSET	0x1f30	/* Offset of ID in MMIO region */
#define TR_TYP_OFFSET	0x1fa0	/* Offset of TYP in MMIO region */
#define TR_RATES_OFFSET	0x1fa2	/* Offset of supported speeds in MMIO region */
#define TR_RAM_OFFSET	0x1fa6	/* Offset of available shared RAM */
#define TR_PAGE_OFFSET	0x1fa8	/* Offset of shared-RAM paging support */
#define TR_DHB4_OFFSET	0x1faa	/* Offset of available DHB size at 4Mbit */
#define TR_DHB16_OFFSET	0x1fac	/* Offset of available DHB size at 16Mbit */
#define TR_MEDIAS_OFFSET 0x1fb6	/* Offset of supported media types in MMIO */
#define TR_MEDIA_OFFSET	0x1fb8  /* Offset of selected media type in MMIO */
#define TR_IRQ_OFFSET	0x1fba	/* Offset of IRQs supported in MMIO region */

/* Bring-Up Test results */

#define	BUT_OK			0x0000	/* Initialization completed OK */
#define	BUT_PROCESSOR_FAIL	0x0020	/* Failed processor initialization */
#define	BUT_ROM_FAIL		0x0022	/* Failed ROM test diagnostic */
#define	BUT_RAM_FAIL		0x0024	/* Failed RAM test diagnostic */
#define	BUT_INST_FAIL		0x0026	/* Failed instruction test diag. */
#define	BUT_INTER_FAIL		0x0028	/* Failed interrupt test diagnostic */
#define	BUT_MEM_FAIL		0x002a	/* Failed memory interface diag. */
#define	BUT_PROTOCOL_FAIL	0x002c	/* Failed protocol handler diag. */


/* Direct PC-to-adapter commands */

#define	DIR_INTERRUPT		0x00	/* Cause adapter to interrupt the PC */
#define	DIR_MOD_OPEN_PARAMS	0x01	/* Modify open options */
#define DIR_RESTORE_OPEN_PARMS	0x02	/* Restore open options */
#define	DIR_OPEN_ADAPTER	0x03	/* Open the adapter card */
#define	DIR_CLOSE		0x04	/* Close adapter card */
#define	DIR_SET_GRP_ADDR	0x06	/* Set adapter group address */
#define	DIR_SET_FUNC_ADDR	0x07	/* Set adapter functional addr */
#define	DIR_READ_LOG		0x08	/* Read and reset error counters */
#define DIR_SET_BRIDGE_PARMS	0x09
#define DIR_CONFIG_BRIDGE_RAM	0x0c
#define DIR_CONFIG_FAST_PATH_RAM	0x12
#define DIR_SINGLE_ROUTE_BROADCAST	0x1f
#define DIR_SET_DEFAULT_RING_SPEED	0x21

#define XMIT_DIR_FRAME		0x0a	/* Direct station transmit */
#define XMIT_I_FRAME		0x0b
#define XMIT_UI_FRM 		0x0d	/* transmit unnumbered info frame */
#define XMIT_XID_CMD		0x0e	/* transmit XID command */
#define XMIT_XID_RESP_FINAL	0x0f
#define XMIT_XID_RESP_NOT_FINAL	0x10
#define XMIT_TEST_CMD		0x11	/* transmit TEST command */


/* Adapter-Card-to-PC commands */

#define	REC_DATA		0x81	/* Data received from ring station */
#define	XMIT_DATA_REQ		0x82	/* Adapter needs data to xmit */
#define	DLC_STATUS   		0x83    /* DLC status has changed */
#define	RING_STAT_CHANGE	0x84	/* Adapter has new ring-status info */
#define REC_BRIDGE_DATA		0x85
#define REXMIT_DATA_REQ		0x86

/* Open options */

#define	OPEN_WRAP		0x8000	/* Wrap xmit data to receive data */
#define	OPEN_NO_HARD_ERR	0x4000	/* Ring hard error and xmit beacon */
					/* conditions do not cause interrupt */
#define	OPEN_NO_SOFT_ERR	0x2000	/* Ring soft errors do not cause */
					/* interrupt */
#define	OPEN_PASS_MAC		0x1000	/* Pass all adapter-class MAC frames */
					/* received but not supported by the */
					/* adapter */
#define	OPEN_PASS_ATTN_MAC	0x0800	/* Pass all attention-class MAC */
					/* frames != the previously received */
					/* attention MAC frame */
#define	OPEN_PASS_BCON_MAC	0x0100	/* Pass the first beacon MAC frame */
					/* and all subsequent beacon MAC */
					/* frames that have a change in */
					/* source address or beacon type */
#define	OPEN_CONT		0x0080	/* Adapter will participate in */
					/* monitor contention */

#define	NUM_RCV_BUF		4	/* Number of receive buffers in */
					/* shared RAM needed for adapter to */
					/* open */
#define	RCV_BUF_LEN		520	/* Length of each receive buffer */
#define	RCV_BUF_DLEN		RCV_BUF_LEN - 8	/* Length of data in rec buf */

#define	DHB_LENGTH		512	/* Length of each transmit buffer */
#define FP_BUF_LEN		536	/* length of each FP transmit buffer */

/*
 * Integrity cannot be guaranteed if number of dhbs > 2
 */
#define	NUM_DHB			1	/* Number of transmit buffers */

#define	DLC_MAX_SAP		0	/* MAX number of SAPs */
#define DLC_MAX_STA		0	/* MAX number of link stations */
#define	DLC_MAX_GSAP		0	/* MAX number of group SAPs */
#define	DLC_MAX_GMEM		0	/* MAX number of SAPs that can be */
					/* assigned to any given group */
#define	DLC_TICK		0	/* Zero selects default of 40ms */


/* Open return codes */

#define	OPEN_OK			0x00	/* Open completed successfully */
#define	OPEN_BAD_COMMAND	0x01	/* Invalid command code */
#define	OPEN_ALREADY		0x03	/* Adapter is ALREADY open */
#define	OPEN_MISSING_PARAMS	0x05	/* Required paramaters missing */
#define	OPEN_UNRECOV_FAIL	0x07	/* Unrecoverable failure occurred */
#define	OPEN_INAD_REC_BUFS	0x30	/* Inadequate receive buffers */
#define	OPEN_BAD_NODE_ADDR	0x32	/* Invalid NODE address */
#define	OPEN_BAD_REC_BUF_LEN	0x33	/* Invalid receive buffer length */
#define	OPEN_BAD_XMIT_BUF_LEN	0x43	/* Invalid transmit buffer length */

/* Bit definitions of ISRA High Byte, (Adapter Status)  */
#define PARITY_ERROR	0x80	/* Parity error on the adapter's internal bus */
#define	TIMER_STAT	0x40	/* A Timer Control Reg. has an interrupt */
#define	ACCESS_STAT	0x20	/* Shared RAM or MMIO access violation */
#define	DEADMAN_TIMER	0x10	/* The deadman timer has expired */
#define PROCESSOR_CK	0x08	/* Adapter Processor Check */
#define	H_INT_MASK	0x02	/* When on, no adapter hardware interrupts */
#define	S_INT_MASK	0x01	/* When on, no adapter software interrupts */

/* Bit definitions of ISRA Low Byte, (Used by PC to interrupt adapter) */
#define XMIT_REQ	0x40	/* Transmit frame in fast path transmit buf */
#define	CMD_IN_SRB	0x20	/* Inform adapter of command in SRB */
#define	RESP_IN_ASB	0x10	/* Inform adapter of response in ASB */
#define	SRB_FREE	0x08	/* Inform PC when SRB is FREE */
#define	ASB_FREE	0x04	/* Inform PC when ASB is FREE */
#define	ARB_FREE	0x02	/* Inform adapter ARB is FREE */
#define	SSB_FREE	0x01	/* Inform adapter SSB is FREE */

/* Bit definitions of ISRP High Byte, (PC interrupts and interrupt control) */
#define	NMI_INT_CTL	0x80	/* 1 = all interrupts to PC interrupt level */
				/* 0 = error and timer interrupts to PC NMI */
#define	INT_ENABLE	0x40	/* Allow adapter to interrupt the PC */
#define	TCR_INT		0x10	/* Timer Control Reg. has interrupt for PC */
#define	ERR_INT		0x08	/* Adap machine check, deadman timer, overrun */
#define	ACCESS_INT	0x04	/* Shared RAM or MMIO access violation */
#define	SHARED_INT_BLK	0x02	/* Shared interrupt blocked */
#define	PRIM_ALT_ADDR	0x01	/* 0 = primary adapter address */
				/* 1 = alternate adapter address */

/* Bit definitions of ISRP Low Byte, (PC interrupts) */
#define	ADAP_CHK_INT	0x40	/* The adapter has an unrecoverable error */
#define	SRB_RESP_INT	0x20	/* Adapter has placed a response in the SRB */
#define	ASB_FREE_INT	0x10	/* Adapter has read response in ARB */
#define	ARB_CMD_INT	0x08	/* ARB has command for PC to act on */
#define	SSB_RESP_INT	0x04	/* SSB has response to previous SRB command */
#define XMIT_COMPLETE	0x02	/* Fast path transmit frame complete */


/* Constants for Token-Ring physical header */
#define	DLC_HDR_LEN	0x3	/* Length of DLC header */
#define	SNAP_LENGTH     0x05	/* SNAP field length */              
				/* protocol id = 3 bytes */
				/* ethertype = 2 bytes */
#define HDR_LNGTH_NOROUTE 14    /* length of header with no route info */
#define SKIP_DSAP_SSAP    0x02  /* length of dsap and ssap in llc frame */
#define TR_MAX_LINK_HDR	46	/* max length of link header with route info */

/* SAP DLC SRB commands (page 6-50 Token Ring Tech. Ref.) */
#define DLC_RESET		0x14
#define DLC_OPEN_SAP		0x15    /* activate service access point */
#define DLC_CLOSE_SAP		0x16	/* de-activate SAP */
#define DLC_REALLOCATE		0x17
#define DLC_OPEN_STATION	0x19
#define DLC_CLOSE_STATION	0x1a
#define DLC_CONNECT_STATION	0x1b
#define DLC_MODIFY		0x1c
#define DLC_FLOW_CONTROL	0x1d
#define DLC_STATISTICS		0x1e


/* ARB RING STATUS CHANGE */
#define SIGNAL_LOSS	0x8000	/* signal loss */
#define HARD_ERR	0x4000	/* beacon frames sent */
#define SOFT_ERR   	0x2000  /* soft error */
#define LOBE_FAULT 	0x0800  /* lobe wire fault */
#define LOG_OFLOW	0x0080	/* adapter error log counter overflow */
#define SINGLE_STATION	0x0040	/* single station on ring */
