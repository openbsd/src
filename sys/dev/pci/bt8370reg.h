/*	$OpenBSD: bt8370reg.h,v 1.3 2005/12/19 15:53:15 claudio Exp $ */

/*
 * Copyright (c) 2004,2005  Internet Business Solutions AG, Zurich, Switzerland
 * Written by: Andre Oppermann <oppermann@accoom.net>
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

/* Bt8370 Register definitions */
/* Globals */
#define	Bt8370_DID	0x000		/* Device Identification */
#define	Bt8370_CR0	0x001		/* Primary control register */
#define		CR0_RESET	0x80		/* Reset Framer */
#define		CR0_E1_FAS	0x00		/* E1 FAS only */
#define		CR0_E1_FAS_CRC	0x08		/* E1 FAS+CRC4 */
#define		CR0_T1_SF	0x09		/* T1 SF */
#define		CR0_T1_ESF	0x1B		/* T1 ESF+ForceCRC */
#define	Bt8370_JAT_CR	0x002		/* Jitter attenuator conf */
#define		JAT_CR_JEN	0x80		/* Jitter anntenuator enable */
#define		JAT_CR_JFREE	0x40		/* Free running JCLK and CLADO */
#define		JAT_CR_JDIR_TX	0x00		/* JAT in TX direction */
#define		JAT_CR_JDIR_RX	0x20		/* JAT in RC direction */
#define		JAT_CR_JAUTO	0x10		/* JCLK Acceleration */
#define		JAT_CR_CENTER	0x80		/* Force JAT to center */
#define		JAT_CR_JSIZE8	0x00		/* Elastic store size 8 bits */
#define		JAT_CR_JSIZE16	0x01		/* Elastic store size 16 bits */
#define		JAT_CR_JSIZE32	0x02		/* Elastic store size 32 bits */
#define		JAT_CR_JSIZE64	0x03		/* Elastic store size 64 bits */
#define		JAT_CR_JSIZE128	0x04		/* Elastic store size 128 bits */
#define	Bt8370_IRR	0x003		/* Interrupt request register */
/* Interrupt Status */
#define	Bt8370_ISR7	0x004		/* Alarm 1 Interrupt Status */
#define	Bt8370_ISR6	0x005		/* Alarm 2 Interrupt Status */
#define	Bt8370_ISR5	0x006		/* Error Interrupt Status */
#define	Bt8370_ISR4	0x007		/* Counter Overflow Interrupt Status */
#define	Bt8370_ISR3	0x008		/* Timer Interrupt Status */
#define	Bt8370_ISR2	0x009		/* Data Link 1 Interrupt Status */
#define	Bt8370_ISR1	0x00A		/* Data Link 2 Interrupt Status */
#define	Bt8370_ISR0	0x00B		/* Pattern Interrupt Status */
/* Interrupt Enable */
#define	Bt8370_IER7	0x00C		/* Alarm 1 Interrupt Enable register */
#define	Bt8370_IER6	0x00D		/* Alarm 2 Interrupt Enable register */
#define	Bt8370_IER5	0x00E		/* Error Interrupt Enable register */
#define	Bt8370_IER4	0x00F		/* Count Overflow Interrupt Enable register */
#define	Bt8370_IER3	0x010		/* Timer Interrupt Enable register */
#define	Bt8370_IER2	0x011		/* Data Link 1 Interrupt Enable register */
#define	Bt8370_IER1	0x012		/* Date Link 2 Interrupt Enable register */
#define	Bt8370_IER0	0x013		/* Pattern Interrupt Enable register */
/* Primary */
#define	Bt8370_LOOP	0x014		/* Loopback Configuration register */
#define		LOOP_PLOOP	0x08		/* Remote Payload Loopback */
#define		LOOP_LLOOP	0x04		/* Remote Line Loopback */
#define		LOOP_FLOOP	0x02		/* Local Framer Loopback */
#define		LOOP_ALOOP	0x01		/* Local Analog Loopback */
#define	Bt8370_DL3_TS	0x015		/* External Data Link Channel */
#define	Bt8370_DL3_BIT	0x016		/* External Data Link Bit */
#define	Bt8370_FSTAT	0x017		/* Offline Framer Status */
#define		FSTAT_INVALID	0x10		/* No candidate */
#define		FSTAT_FOUND	0x08		/* Frame Search Successful */
#define		FSTAT_TIMEOUT	0x04		/* Framer Search Timeout */
#define		FSTAT_ACTIVE	0x02		/* Framer Active */
#define		FSTAT_RXTXN	0x01		/* RX/TX Reframe Operation */
#define	Bt8370_PIO	0x018		/* Programmable Input/Output */
#define		PIO_ONESEC_IO	0x80		/*  */
#define		PIO_RDL_IO	0x40		/*  */
#define		PIO_TDL_IO	0x20		/*  */
#define		PIO_INDY_IO	0x10		/*  */
#define		PIO_RFSYNC_IO	0x08		/*  */
#define		PIO_RMSYNC_IO	0x04		/*  */
#define		PIO_TFSYNC_IO	0x02		/*  */
#define		PIO_TMSYNC_IO	0x01		/*  */
#define	Bt8370_POE	0x019		/* Programmable Output Enable */
#define		POE_TDL_OE	0x20		/*  */
#define		POE_RDL_OE	0x10		/*  */
#define		POE_INDY_OE	0x08		/*  */
#define		POE_TCKO_OE	0x04		/*  */
#define		POE_CLADO_OE	0x02		/*  */
#define		POE_RCKO_OE	0x01		/*  */
#define	Bt8370_CMUX	0x01A		/* Clock Input Mux */
#define		CMUX_RSBCKI_RSBCKI	0x00	/*  */
#define		CMUX_RSBCKI_TSBCKI	0x40	/*  */
#define		CMUX_RSBCKI_CLADI	0x80	/*  */
#define		CMUX_RSBCKI_CLADO	0xC0	/*  */
#define		CMUX_TSBCKI_TSBCKI	0x00	/*  */
#define		CMUX_TSBCKI_RSBCKI	0x10	/*  */
#define		CMUX_TSBCKI_CLADI	0x20	/*  */
#define		CMUX_TSBCKI_CLADO	0x30	/*  */
#define		CMUX_CLADI_CLADI	0x00	/*  */
#define		CMUX_CLADI_RCKO		0x04	/*  */
#define		CMUX_CLADI_TSBCKI	0x08	/*  */
#define		CMUX_CLADI_TCKI		0x0C	/*  */
#define		CMUX_TCKI_TCKI		0x00	/*  */
#define		CMUX_TCKI_RCKO		0x01	/*  */
#define		CMUX_TCKI_RSBCKI	0x02	/*  */
#define		CMUX_TCKI_CLADO		0x03	/*  */
#define	Bt8370_TMUX	0x01B		/* Test Mux Configuration */
#define	Bt8370_TEST	0x01C		/* Test Configuration */
/* Receive LIU (RLIU) */
#define	Bt8370_LIU_CR	0x020		/* LIU Configuration */
#define		LIU_CR_RST_LIU	0x80		/* Reset RLIU */
#define		LIU_CR_SQUELCH	0x40		/* Enable Squelch */
#define		LIU_CR_FORCE_VGA 0x20		/* Internal Variable Gain Amp */
#define		LIU_CR_RDIGI	0x10		/* Enable Receive Digital Inputs */
#define		LIU_CR_ATTN0	0x00		/* Bridge Attenuation 0db */
#define		LIU_CR_ATTN10	0x04		/* Bridge Attenuation -10db */
#define		LIU_CR_ATTN20	0x08		/* Bridge Attenuation -20db */
#define		LIU_CR_ATTN30	0x0C		/* Bridge Attenuation -30db */
#define		LIU_CR_MAGIC	0x01		/* This one must be enabled */
#define	Bt8370_RSTAT	0x021		/* Receive LIU Status */
#define		RSTAT_CPDERR	0x80		/* CLAD Phase detector lost lock */
#define		RSTAT_JMPTY	0x40		/* JAT Empty/Full */
#define		RSTAT_ZCSUB	0x20		/* ZCS detected */
#define		RSTAT_EXZ	0x10		/* Excessive Zeros */
#define		RSTAT_BPV	0x08		/* Bipolar Violations */
#define		RSTAT_EYEOPEN	0x02		/* Equalization State */
#define		RSTAT_PRE_EQ	0x01		/* Pre-Equalizer Status */
#define	Bt8370_RLIU_CR	0x022		/* Receive LIU Configuration */
#define		RLIU_CR_FRZ_SHORT 0x80		/* Freeze Equalizer for short lines */
#define		RLIU_CR_HI_CLICE  0x40		/* High Clock Slicer Threshold */
#define		RLIU_CR_AGC32	0x00		/* AGC Observation Window 32bit */
#define		RLIU_CR_AGC128	0x10		/* AGC Observation Window 128bit */
#define		RLIU_CR_AGC512	0x20		/* AGC Observation Window 512bit */
#define		RLIU_CR_AGC2048	0x30		/* AGC Observation Window 2048bit */
#define		RLIU_CR_EQ_FRZ	0x08		/* Freeze EQ Coefficients */
#define		RLIU_CR_OOR_BLOCK 0x04		/* Disable automatic RLBO */
#define		RLIU_CR_RLBO	0x02		/* Receiver Line Build Out */
#define		RLIU_CR_LONG_EYE  0x01		/* Eye Open Timeout 8192bit */
#define	Bt8370_LPF	0x023		/* RPLL Low Pass Filter */
#define	Bt8370_VGA_MAX	0x024		/* Variable Gain Amplifier Maximum */
#define	Bt8370_EQ_DAT	0x025		/* Equalizer Coefficient Data register */
#define	Bt8370_EQ_PTR	0x026		/* Equalizer Coefficient Table Pointer */
#define	Bt8370_DSLICE	0x027		/* Data Slicer Threshold */
#define	Bt8370_EQ_OUT	0x028		/* Equalizer Output Levels */
#define	Bt8370_VGA	0x029		/* Variable Gain Amplifier Status */
#define	Bt8370_PRE_EQ	0x02A		/* Pre-Equalizer */
#define	Bt8370_COEFF	0x030 /* -037 *//* LMS Adjusted Equalizer Coefficient Status */
#define	Bt8370_GAIN	0x038 /* -03C *//* Equalizer Gain Thresholds */
/* Digital Reveiver (RCVR) */
#define	Bt8370_RCR0	0x040		/* Receiver Configuration */
#define		RCR0_HDB3	0x00		/* */
#define		RCR0_B8ZS	0x00		/* */
#define		RCR0_AMI	0x80		/* */
#define		RCR0_RABORT	0x40		/* */
#define		RCR0_RFORCE	0x20		/* */
#define		RCR0_LFA_FAS	0x18		/* 3 consecutive FAS Errors */
#define		RCR0_LFA_FASCRC	0x08		/* 3 consecutive FAS or 915 CRC Errors */
#define		RCR0_LFA_26F	0x08		/* 2 out of 6 F bit Errors */
#define		RCR0_RZCS_BPV	0x00		/* */
#define		RCR0_RZCS_NBPV	0x01		/* */
#define	Bt8370_RPATT	0x041		/* Receive Test Pattern Configuration */
#define	Bt8370_RLB	0x042		/* Receive Loopback Code Detector Configuration */
#define	Bt8370_LBA	0x043		/* Loopback Activate Code Pattern */
#define	Bt8370_LBD	0x044		/* Loopback Deactivate Code Pattern */
#define	Bt8370_RALM	0x045		/* Receive Alarm Signal Configuration */
#define		RALM_FSNFAS	0x20		/* Include FS/NFAS in FERR and FRED */
#define	Bt8370_LATCH	0x046		/* Alarm/Error/Counter Latch Configuration */
#define		LATCH_STOPCNT	0x08		/* Stop Error Counter during RLOF,RLOS,RAIS */
#define	Bt8370_ALM1	0x047		/* Alarm 1 Status */
#define		ALM1_RMYEL	0x80		/* Receive Multifram Yellow Alarm */
#define		ALM1_RYEL	0x40		/* Receive Yellow Alarm */
#define		ALM1_RAIS	0x10		/* Reveive Alarm Indication Signal */
#define		ALM1_RALOS	0x09		/* Receive Analog Loss of Signal */
#define		ALM1_RLOS	0x04		/* Receive Loss of Signal */
#define		ALM1_RLOF	0x02		/* Receive Loss of Frame Alignment */
#define		ALM1_SIGFRZ	0x01		/* Signalling Freeze */
#define	Bt8370_ALM2	0x048		/* Alarm 2 Status */
#define		ALM2_LOOPDN	0x80		/*  */
#define		ALM2_LOOPUP	0x40		/*  */
#define		ALM2_TSHORT	0x10		/* Transmit Short Circuit */
#define		ALM2_TLOC	0x08		/* Transmit Loss of clock */
#define		ALM2_TLOF	0x02		/* Transmit Loss of Frame alignment */
#define	Bt8370_ALM3	0x049		/* Alarm 3 Status */
#define		ALM3_RMAIS	0x40		/* Receive TS16 Alarm Indication */
#define		ALM3_SEF	0x20		/* Severely Errored Frame */
#define		ALM3_SRED	0x10		/* Loss of CAS Alignment */
#define		ALM3_MRED	0x08		/* Loss of MFAS Alignment */
#define		ALM3_FRED	0x04		/* Loss of T1/FAS Alignment */
#define		ALM3_LOF1	0x02		/* Reason for Loss of Frame Alignment */
#define		ALM3_LOF0	0x01		/* Reason for Loss of Frame Alignment */
/* Error/Alarm Counters */
#define	Bt8370_FERR_LSB	0x050		/* Framing Bit Error Counter LSB */
#define	Bt8370_FERR_MSB	0x051		/* ditto MSB */
#define	Bt8370_CERR_LSB	0x052		/* CRC Error Counter LSB */
#define	Bt8370_CERR_MSB	0x053		/* ditto MSB */
#define	Bt8370_LCV_LSB	0x054		/* Line Code Violation Counter LSB*/
#define	Bt8370_LCV_MSB	0x055		/* ditto MSB */
#define	Bt8370_FEBE_LSB	0x056		/* Far End Block Error Counter LSB*/
#define	Bt8370_FEBE_MSB	0x057		/* ditto MSB */
#define	Bt8370_BERR_LSB	0x058		/* PRBS Bit Error Counter LSB */
#define	Bt8370_BERR_MSB	0x059		/* ditto MSB */
/* Receive Sa-Byte */
#define	Bt8370_RSA4	0x05B		/* Receive Sa4 Byte Buffer */
#define	Bt8370_RSA5	0x05C		/* ditto Sa5 */
#define	Bt8370_RSA6	0x05D		/* ditto Sa6 */
#define	Bt8370_RSA7	0x05E		/* ditto Sa7 */
#define	Bt8370_RSA8	0x05F		/* ditto Sa8 */
/* Transmit LIU (TLIU) */
#define	Bt8370_SHAPE	0x060 /* -067 *//* Transmit Pulse Shape Configuration */
#define	Bt8370_TLIU_CR	0x068		/* Transmit LIU Configuration */
#define		TLIU_CR_120	0x4C		/* 120 Ohms, external term */
#define		TLIU_CR_100	0x40		/* 100 Ohms, external term */
/* Digital Transmitter (XMTR) */
#define	Bt8370_TCR0	0x070		/* Transmit Framer Configuration */
#define		TCR0_FAS	0x00		/* FAS Only */
#define		TCR0_MFAS	0x04		/* FAS + MFAS*/
#define		TCR0_SF		0x04		/* SF Only */
#define		TCR0_ESF	0x01		/* ESF Only */
#define		TCR0_ESFCRC	0x0D		/* ESF + Force CRC */
#define	Bt8370_TCR1	0x071		/* Transmitter Configuration */
#define		TCR1_TABORT	0x40		/* Disable TX Offline Framer */
#define		TCR1_TFORCE	0x20		/* Force TX Reframe */
#define		TCR1_HDB3	0x01		/* Line code HDB3 */
#define		TCR1_B8ZS	0x01		/* Line code B8ZS */
#define		TCR1_AMI	0x00		/* Line code AMI */
#define		TCR1_3FAS	0x10		/* 3 consecutive FAS Errors */
#define		TCR1_26F	0x10		/* 2 out of 6 Frame Bit Errors */
#define	Bt8370_TFRM	0x072		/* Transmit Frame Format */
#define		TFRM_MYEL	0x20		/* Insert MultiFrame Yellow Alarm */
#define		TFRM_YEL	0x10		/* Insert Yellow Alarm */
#define		TFRM_MF		0x08		/* Insert MultiFrame Alignment */
#define		TFRM_FE		0x04		/* Insert FEBE */
#define		TFRM_CRC	0x02		/* Insert CRC4 */
#define		TFRM_FBIT	0x01		/* Insert F bit or FAS/NAS alignment */
#define	Bt8370_TERROR	0x073		/* Transmit Error Insert */
#define	Bt8370_TMAN	0x074		/* Transmit Manual Sa-Byte/FEBE Configuration */
#define		TMAN_MALL	0xF8		/* All Sa Bytes Manual */
#define	Bt8370_TALM	0x075		/* Transmit Alarm Signal Configuration */
#define		TALM_AMYEL	0x20		/* Automatic MultiFrame Yellow Alarm transmit */
#define		TALM_AYEL	0x10		/* Automatic Yellow Alarm transmit */
#define		TALM_AAIS	0x08		/* Automatic AIS Alarm transmit */
#define	Bt8370_TPATT	0x076		/* Transmit Test Pattern Configuration */
#define	Bt8370_TLB	0x077		/* Transmit Inband Loopback Code Configuration */
#define	Bt8370_LBP	0x078		/* Transmit Inband Loopback Code Pattern */
/* Transmit Sa-Byte */
#define	Bt8370_TSA4	0x07B		/* Transmit Sa4 Byte Buffer */
#define	Bt8370_TSA5	0x07C		/* ditto Sa5 */
#define	Bt8370_TSA6	0x07D		/* ditto Sa6 */
#define	Bt8370_TSA7	0x07E		/* ditto Sa7 */
#define	Bt8370_TSA8	0x07F		/* ditto Sa8 */
/* Clock Rate Adapter (CLAD) */
#define	Bt8370_CLAD_CR	0x090		/* Clock Rate Adapter Configuration */
#define		CLAD_CR_CEN	0x80		/* Enable CLAD phase detector */
#define		CLAD_CR_XSEL_1X	0x00		/* Line rate multiplier 1X */
#define		CLAD_CR_XSEL_2X	0x10		/* Line rate multiplier 2X */
#define		CLAD_CR_XSEL_4X	0x20		/* Line rate multiplier 4X */
#define		CLAD_CR_XSEL_8X	0x30		/* Line rate multiplier 8X */
#define		CLAD_CR_LFGAIN	0x05		/* Loop filter gain */
#define	Bt8370_CSEL	0x091		/* CLAD Frequency Select */
#define		CSEL_VSEL_1536	0x60		/* 1536kHz */
#define		CSEL_VSEL_1544	0x50		/* 1544kHz */
#define		CSEL_VSEL_2048	0x10		/* 2048kHz */
#define		CSEL_VSEL_4096	0x20		/* 4096kHz */
#define		CSEL_VSEL_8192	0x30		/* 8192kHz */
#define		CSEL_OSEL_1536	0x06		/* 1536kHz */
#define		CSEL_OSEL_1544	0x05		/* 1544kHz */
#define		CSEL_OSEL_2048	0x01		/* 2048kHz */
#define		CSEL_OSEL_4096	0x02		/* 4096kHz */
#define		CSEL_OSEL_8192	0x03		/* 8192kHz */
#define	Bt8370_CPHASE	0x092		/* CLAD Phase Detector Scale Factor */
#define	Bt8370_CTEST	0x093		/* CLAD Test */
/* Bit Oriented Protocol Transceiver (BOP) */
#define	Bt8370_BOP	0x0A0		/* Bit Oriented Protocol Transceiver */
#define	Bt8370_TBOP	0x0A1		/* Transmit BOP Code Word */
#define	Bt8370_RBOP	0x0A2		/* Receive BOP Code Word */
#define	Bt8370_BOP_STAT	0x0A3		/* BOP Status */
/* Data Link #1 */
#define	Bt8370_DL1_TS	0x0A4		/* DL1 Time Slot Enable */
#define	Bt8370_DL1_BIT	0x0A5		/* DL1 Bit Enable */
#define	Bt8370_DL1_CTL	0x0A6		/* DL1 Control */
#define	Bt8370_RDL1_FFC	0x0A7		/* RDL #1 FIFO Fill Control */
#define	Bt8370_RDL1	0x0A8		/* Receive Data Link FIFO #1 */
#define	Bt8370_RDL1_STAT 0x0A9		/* RDL #1 Status */
#define	Bt8370_PRM1	0x0AA		/* Performance Report Message */
#define	Bt8370_TDL1_FEC	0x0AB		/* TDL #1 FIFO Empty Control */
#define	Bt8370_TDL1_EOM	0x0AC		/* TDL #1 End of Message Control */
#define	Bt8370_TDL1	0x0AD		/* Transmit Data Link FIFO #1*/
#define	Bt8370_TDL1_STAT 0x0AE		/* TDL #1 Status */
/* Data Link #2 */
#define	Bt8370_DL2_TS	0x0AF		/* DL2 Time Slot Enable */
#define	Bt8370_DL2_BIT	0x0B0		/* DL2 Bit Enable */
#define	Bt8370_DL2_CTL	0x0B1		/* DL2 Control */
#define	Bt8370_RDL2_FFC	0x0B2		/* RDL #2 FIFO Fill Control */
#define	Bt8370_RDL2	0x0B3		/* Receive Data Link FIFO #2 */
#define	Bt8370_RDL2_STAT 0x0B4		/* RDL #2 Status */
#define	Bt8370_TDL2_FEC	0x0B6		/* TDL #2 FIFO Empty Control */
#define	Bt8370_TDL2_EOM	0x0B7		/* TDL #2 End of Message Control */
#define	Bt8370_TDL2	0x0B8		/* Transmit Data Link FIFO #2*/
#define	Bt8370_TDL2_STAT 0x0B9		/* TDL #2 Status */
/* Test */
#define	Bt8370_TEST1	0x0BA		/* DLINK Test Configuration */
#define	Bt8370_TEST2	0x0BB		/* DLINK Test Status */
#define	Bt8370_TEST3	0x0BC		/* DLINK Test Status */
#define	Bt8370_TEST4	0x0BD		/* DLINK Test Control #1 or Configuration #2 */
#define	Bt8370_TEST5	0x0BE		/* DLINK Test Control #2 or Configuration #2 */
/* System Bus Interface (SBI) */
#define	Bt8370_SBI_CR	0x0D0		/* System Bus Interface Configuration */
#define		SBI_CR_X2CLK	0x80		/* Times 2 clock */
#define		SBI_CR_SBI_OE	0x40		/* Enable SBI */
#define		SBI_CR_1536	0x08		/* 1536, 24TS*/
#define		SBI_CR_1544	0x07		/* 1544, 24TS + F bit */
#define		SBI_CR_2048	0x06		/* 2048, 32TS */
#define		SBI_CR_4096_A	0x04		/* 4096 Group A */
#define		SBI_CR_4096_B	0x05		/* 4096 Group B */
#define		SBI_CR_8192_A	0x00		/* 8192 Group A */
#define		SBI_CR_8192_B	0x01		/* 8192 Group B */
#define		SBI_CR_8192_C	0x02		/* 8192 Group C */
#define		SBI_CR_8192_D	0x03		/* 8192 Group D */
#define	Bt8370_RSB_CR	0x0D1		/* Receive System Bus Configuration */
#define		RSB_CR_BUS_RSB	0x80		/* Multiple devices on bus */
#define		RSB_CR_SIG_OFF	0x40		/* Inhibit RPCMO Signal reinsertion */
#define		RSB_CR_RPCM_NEG	0x20		/* RSB falling edge */
#define		RSB_CR_RSYN_NEG	0x10		/* RFSYNC falling edge */
#define		RSB_CR_BUS_FRZ	0x08		/* Multiple devices on bus */
#define		RSB_CR_RSB_CTR	0x04		/* Force RSLIP Center */
#define		RSB_CR_RSBI_NORMAL	0x00	/* Normal Slip Buffer Mode */
#define		RSB_CR_RSBI_ELASTIC	0x02	/* Receive Slip Buffer Elastic Mode */
#define		RSB_CR_RSBI_BYPASS	0x03	/* Bypass Slip Buffer */
#define	Bt8370_RSYNC_BIT 0x0D2		/* Receive System Bus Sync Bit Offset */
#define	Bt8370_RSYNC_TS	0x0D3		/* Receive System Bus Sync Time Slot Offset */
#define	Bt8370_TSB_CR	0x0D4		/* Transmit System Bus Configuration */
#define		TSB_CR_BUS_TSB	0x80		/* Bused TSB output */
#define		TSB_CR_TPCM_NEG	0x20		/* TINDO falling edge */
#define		TSB_CR_TSYN_NEG	0x10		/* TFSYNC falling edge */
#define		TSB_CR_TSB_CTR	0x04		/* Force TSLIP Center */
#define		TSB_CR_TSB_NORMAL	0x00	/* Normal Slip Buffer Mode */
#define		TSB_CR_TSB_ELASTIC	0x02	/* Send Slip Buffer Elastic Mode */
#define		TSB_CR_TSB_BYPASS	0x03	/* Bypass Slip Buffer */
#define	Bt8370_TSYNC_BIT 0x0D5		/* Transmit System Bus Sync Bit Offset */
#define	Bt8370_TSYNC_TS	0x0D6		/* Transmit System Bus Sync Time Slot Offset */
#define	Bt8370_RSIG_CR	0x0D7		/* Receive Signaling Configuration */
#define		RSIG_CR_FRZ_OFF	0x04		/* Manual Signaling Update FRZ */
#define		RSIG_CR_THRU	0x01		/* Transparent Robbed Bit Signaling */
#define	Bt8370_RSYNC_FRM 0x0D8		/* Signaling Reinsertion Frame Offset */
#define	Bt8370_SSTAT	0x0D9		/* Slip Buffer Status */
#define		SSTAT_TSDIR	0x80		/* Transmit Slip Direction */
#define		SSTAT_TFSLIP	0x40		/* Controlled Slip Event */
#define		SSTAT_TUSLIP	0x20		/* Uncontrolled Slip Event */
#define		SSTAT_RSDIR	0x08		/* Receive Slip Direction */
#define		SSTAT_RFSLIP	0x04		/* Controlled Slip Event */
#define		SSTAT_RUSLIP	0x02		/* Uncontrolled Slip Event */
#define	Bt8370_STACK	0x0DA		/* Receive Signaling Stack */
#define	Bt8370_RPHASE	0x0DB		/* RSLIP Phase Status */
#define	Bt8370_TPHASE	0x0DC		/* TSLIP Phase Status */
#define	Bt8370_PERR	0x0DD		/* RAM Parity Status */
#define	Bt8370_SBCn	0x0E0 /* -0FF *//* System Bus Per-Channel Control */
#define		SBCn_INSERT	0x40		/* Insert RX Signaling on RPCMO */
#define		SBCn_SIG_LP	0x20		/* Local Signaling Loopback */
#define		SBCn_RLOOP	0x10		/* Local Loopback */
#define		SBCn_RINDO	0x08		/* Activate RINDO time slot indicator */
#define		SBCn_TINDO	0x04		/* Activate TINDO time slot indicator */
#define		SBCn_TSIG_AB	0x02		/* AB Signaling */
#define		SBCn_ASSIGN	0x01		/* Enable System Bus Time Slot */
/* Buffer Memory */
#define	Bt8370_TPCn	0x100		/* Transmit Per-Channel Control */
#define		TPCn_CLEAR	0x00		/* Clear Channel Mode */
#define		TPCn_EMFBIT	0x80		/* TB7ZS/EMFBIT */
#define		TPCn_TLOOP	0x40		/* Remote DS0 Channel Loopback */
#define		TPCn_TIDLE	0x20		/* Transmit Idle */
#define		TPCn_TLOCAL	0x10		/* Transmit Local Signaling */
#define		TPCn_TSIGA	0x08		/* ABCD signaling value */
#define		TPCn_TSIGB	0x04		/* ABCD signaling value */
#define		TPCn_TSIGC	0x02		/* ABCD signaling value */
#define		TPCn_TSIGD	0x01		/* ABCD signaling value */
#define		TPCn_TSIGO	TPCn_TSIGA	/* Transmit Signaling Output */
#define		TPCn_RSIGO	TPCn_TSIGB	/* Receive Signaling Output */
#define	Bt8370_TSIGn	0x120		/* Transmit Signaling Buffer */
#define	Bt8370_TSLIP_LOn 0x140		/* Transmit PCM Slip Buffer */
#define	Bt8370_TSLIP_HIn 0x160		/* Transmit PCM Slip Buffer */
#define	Bt8370_RPCn	0x180		/* Receive Per-Channel Control */
#define		RPCn_CLEAR	0x00		/* Clear Channel Mode */
#define		RPCn_RSIG_AB	0x80		/* AB Signaling */
#define		RPCn_RIDLE	0x40		/* Time Slot Idle */
#define		RPCn_SIG_STK	0x20		/* Receive Signal Stack */
#define		RPCn_RLOCAL	0x10		/* Enable Local Signaling Output */
#define		RPCn_RSIGA	0x08		/* Local Receive Signaling */
#define		RPCn_RSIGB	0x04		/* Local Receive Signaling */
#define		RPCn_RSIGC	0x02		/* Local Receive Signaling */
#define		RPCn_RSIGD	0x01		/* Local Receive Signaling */
#define	Bt8370_RSIGn	0x1A0		/* Receive Signaling Buffer */
#define	Bt8370_RSLIP_LOn 0x1C0		/* Receive PCM Slip Buffer */
#define	Bt8370_RSLIP_HIn 0x1E0		/* Receive PCM Slip Buffer */
