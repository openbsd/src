/*	$OpenBSD: cs4231reg.h,v 1.2 2001/09/30 20:58:16 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for CS4231 SBUS audio.
 */

#define	CS_TIMEOUT	90000

/* CS4231 registers */
#define	CS4231_IAR	0x0000		/* index address */
#define	CS4231_IDR	0x0004		/* index data */
#define	CS4231_STS	0x0008		/* status */
#define	CS4231_DAT	0x000c		/* pio data */

/* APC DMA registers */
#define	APC_CSR		0x0010		/* control/status */
#define	APC_CVA		0x0020		/* capture virtual address */
#define	APC_CC		0x0024		/* capture count */
#define	APC_CNVA	0x0028		/* capture next virtual address */
#define	APC_CNC		0x002c		/* capture next count */
#define	APC_PVA		0x0030		/* playback virtual address */
#define	APC_PC		0x0034		/* playback count */
#define	APC_PNVA	0x0038		/* playback next virtual address */
#define	APC_PNC		0x003c		/* playback next count */

/*
 * The CS4231 has 4 direct access registers: iar, idr, status, pior
 *
 * iar and idr are used to access either 16 or 32 (mode 2 only) "indirect
 * registers".
 */

/*
 * Direct mapped registers
 */

/* cs4231_reg.iar: index address register */
#define	CS_IAR_IR_MASK		0x1f		/* indirect register mask */
#define	CS_IAR_TRD		0x20		/* transfer request disable */
#define	CS_IAR_MCE		0x40		/* mode change enable */
#define	CS_IAR_INIT		0x80		/* initialization */

/* indirect register numbers (mode1/mode2) */
#define	CS_IAR_LADCIN		0x00		/* left adc input control */
#define	CS_IAR_RADCIN		0x01		/* right adc input control */
#define	CS_IAR_LACIN1		0x02		/* left aux #1 input control */
#define	CS_IAR_RACIN1		0x03		/* right aux #1 input control */
#define	CS_IAR_LACIN2		0x04		/* left aux #2 input control */
#define	CS_IAR_RACIN2		0x05		/* right aux #2 input control */
#define	CS_IAR_LDACOUT		0x06		/* left dac output control */
#define	CS_IAR_RDACOUT		0x07		/* right dac output control */
#define	CS_IAR_FSPB		0x08		/* fs and playback format */
#define	CS_IAR_IC		0x09		/* interface configuration */
#define	CS_IAR_PC		0x0a		/* pin control */
#define	CS_IAR_ERRINIT		0x0b		/* error status & init */
#define	CS_IAR_MODEID		0x0c		/* mode and id */
#define	CS_IAR_LOOP		0x0d		/* loopback control */
#define	CS_IAR_PBUB		0x0e		/* playback upper base */
#define	CS_IAR_PBLB		0x0f		/* playback lower base */

/* indirect register numbers (mode2 only) */

#define	CS_IAR_AFE1		0x10		/* alt feature enable I */
#define	CS_IAR_AFE2		0x11		/* alt feature enable II */
#define	CS_IAR_LLI		0x12		/* left line input control */
#define	CS_IAR_RLI		0x13		/* right line input control */
#define	CS_IAR_TLB		0x14		/* timer lower base */
#define	CS_IAR_TUB		0x15		/* timer upper base */
#define	CS_IAR_reserved1	0x16		/* reserved */
#define	CS_IAR_AFE3		0x17		/* alt feature enable III */
#define	CS_IAR_AFS		0x18		/* alt feature status */
#define	CS_IAR_VID		0x19		/* version id */
#define	CS_IAR_MONO		0x1a		/* mono input/output control */
#define	CS_IAR_reserved2	0x1b		/* reserved */
#define	CS_IAR_CDF		0x1c		/* capture data format */
#define	CS_IAR_reserved3	0x1d		/* reserved */
#define	CS_IAR_CUB		0x1e		/* capture upper base */
#define	CS_IAR_CLB		0x1f		/* capture lower base */

/* cs4231_reg.idr: index data register */
/* Contains the data of the indirect register indexed by the iar */

/* cs4231_reg.status: status register */
#define	CS_STATUS_INT		0x01		/* interrupt status(1=active) */
#define	CS_STATUS_PRDY		0x02		/* playback data ready */
#define	CS_STATUS_PL		0x04		/* playback l/r sample */
#define	CS_STATUS_PU		0x08		/* playback up/lw byte needed */
#define	CS_STATUS_SER		0x10		/* sample error */
#define	CS_STATUS_CRDY		0x20		/* capture data ready */
#define	CS_STATUS_CL		0x40		/* capture l/r sample */
#define	CS_STATUS_CU		0x80		/* capture up/lw byte needed */

/* cs4231_reg.pior: programmed i/o register */
/* On write, this is the playback data byte */
/* On read, it is the capture data byte */

/*
 * Indirect Mapped Registers
 */

/* Left ADC Input Control: I0 */
#define	CS_LADCIN_GAIN_MASK	0x0f		/* left adc gain */
#define	CS_LADCIN_reserved	0x10		/* reserved */
#define	CS_LADCIN_LMGE		0x20		/* left mic gain enable */
#define	CS_LADCIN_SRC_MASK	0xc0		/* left adc source select */

#define	CS_LADCIN_SRC_LINE	0x00		/* left input src: line */
#define	CS_LADCIN_SRC_AUX	0x40		/* left input src: aux */
#define	CS_LADCIN_SRC_MIC	0x80		/* left input src: mic */
#define	CS_LADCIN_SRC_LOOP	0xc0		/* left input src: loopback */

/* Right ADC Input Control: I1 */
#define	CS_RADCIN_GAIN_MASK	0x0f		/* right adc gain */
#define	CS_RADCIN_reserved	0x10		/* reserved */
#define	CS_RADCIN_LMGE		0x20		/* right mic gain enable */
#define	CS_RADCIN_SRC_MASK	0xc0		/* right adc source select */

#define	CS_RADCIN_SRC_LINE	0x00		/* right input src: line */
#define	CS_RADCIN_SRC_AUX	0x40		/* right input src: aux */
#define	CS_RADCIN_SRC_MIC	0x80		/* right input src: mic */
#define	CS_RADCIN_SRC_LOOP	0xc0		/* right input src: loopback */

/* Left Auxiliary #1 Input Control: I2 */
#define	CS_LACIN1_GAIN_MASK	0x1f		/* left aux #1 mix gain */
#define	CS_LACIN1_reserved1	0x20		/* reserved */
#define	CS_LACIN1_reserved2	0x40		/* reserved */
#define	CS_LACIN1_LX1M		0x80		/* left aux #1 mute */

/* Right Auxiliary #1 Input Control: I3 */
#define	CS_RACIN1_GAIN_MASK	0x1f		/* right aux #1 mix gain */
#define	CS_RACIN1_reserved1	0x20		/* reserved */
#define	CS_RACIN1_reserved2	0x40		/* reserved */
#define	CS_RACIN1_RX1M		0x80		/* right aux #1 mute */

/* Left Auxiliary #2 Input Control: I4 */
#define	CS_LACIN2_GAIN_MASK	0x1f		/* left aux #2 mix gain */
#define	CS_LACIN2_reserved1	0x20		/* reserved */
#define	CS_LACIN2_reserved2	0x40		/* reserved */
#define	CS_LACIN2_LX2M		0x80		/* left aux #2 mute */

/* Right Auxiliary #2 Input Control: I5 */
#define	CS_RACIN2_GAIN_MASK	0x1f		/* right aux #2 mix gain */
#define	CS_RACIN2_reserved1	0x20		/* reserved */
#define	CS_RACIN2_reserved2	0x40		/* reserved */
#define	CS_RACIN2_RX2M		0x80		/* right aux #2 mute */

/* Left DAC Output Control: I6 */
#define	CS_LDACOUT_LDA_MASK	0x3f		/* left dac attenuator */
#define	CS_LDACOUT_reserved	0x40		/* reserved */
#define	CS_LDACOUT_LDM		0x80		/* left dac mute */

/* Right DAC Output Control: I7 */
#define	CS_RDACOUT_RDA_MASK	0x3f		/* right dac attenuator */
#define	CS_RDACOUT_reserved	0x40		/* reserved */
#define	CS_RDACOUT_RDM		0x80		/* right dac mute */

/* Fs and Playback Data Format: I8 */
#define	CS_FSPB_C2SL		0x01		/* clock 2 source select */
#define	CS_FSPB_CFS_MASK	0x0e		/* clock frequency div select */
#define	CS_FSPB_SM		0x10		/* stereo/mono select */
#define	CS_FSPB_FMT_MASK	0xe0		/* playback format select */

#define	CS_FSPB_C2SL_XTAL1	0x00		/* use 24.576 Mhz crystal */
#define	CS_FSPB_C2SL_XTAL2	0x01		/* use 16.9344 Mhz crystal */
#define	CS_FSPB_SM_MONO		0x00		/* use mono output */
#define	CS_FSPB_SM_STEREO	0x10		/* use stereo output */
#define	CS_FSPB_FMT_ULINEAR	0x00		/* fmt: linear 8bit unsigned */
#define	CS_FSPB_FMT_ULAW	0x20		/* fmt: ulaw, 8bit companded */
#define	CS_FSPB_FMT_LINEAR_LE	0x40		/* fmt: linear 16bit little */
#define	CS_FSPB_FMT_ALAW	0x60		/* fmt: alaw, 8bit companded */
#define	CS_FSPB_FMT_reserved1	0x80		/* fmt: reserved */
#define	CS_FSPB_FMT_ADPCM	0xa0		/* fmt: adpcm 4bit */
#define	CS_FSPB_FMT_LINEAR_BE	0xc0		/* fmt: linear 16bit big */
#define	CS_FSPB_FMT_reserved2	0xe0		/* fmt: reserved */

/* Interface Configuration: I9 */
#define	CS_IC_PEN		0x01		/* playback enable */
#define	CS_IC_CEN		0x02		/* capture enable */
#define	CS_IC_SDC		0x04		/* single dma channel */
#define	CS_IC_ACAL		0x08		/* auto calibration */
#define	CS_IC_CAL_MASK		0x18		/* calibration type mask */
#define	CS_IC_reserved		0x20		/* reserved */
#define	CS_IC_PPIO		0x40		/* playback pio enable */
#define	CS_IC_CPIO		0x80		/* capture pio enable */

/* Pin Control: I10 */
#define	CS_PC_reserved1		0x01		/* reserved */
#define	CS_PC_IEN		0x02		/* interrupt enable */
#define	CS_PC_reserved2		0x04		/* reserved */
#define	CS_PC_DEN		0x08		/* dither enable */
#define	CS_PC_reserved3		0x10		/* reserved */
#define	CS_PC_reserved4		0x20		/* reserved */
#define	CS_PC_XCTL_MASK		0xc0		/* xctl control */
#define	CS_PC_LINEMUTE		0x40		/* mute line */
#define	CS_PC_HDPHMUTE		0x80		/* mute headphone */
#define	CS_PC_XCTL0		0x40		/* set xtcl0 to 1 */
#define	CS_PC_XCTL1		0x80		/* set xctl1 to 1 */

/* Error Status and Initialization: I11 */
#define	CS_ERRINIT_ORL_MASK	0x03		/* overrange left detect */
#define	CS_ERRINIT_ORR_MASK	0x0c		/* overrange right detect */
#define	CS_ERRINIT_DRQ		0x10		/* drq status */
#define	CS_ERRINIT_ACI		0x20		/* auto-calibrate in progress */
#define	CS_ERRINIT_PUR		0x40		/* playback underrun */
#define	CS_ERRINIT_COR		0x80		/* capture overrun */

#define	CS_ERRINIT_ORL_VLOW	0x00		/* < -1.5 dB from full scale */
#define	CS_ERRINIT_ORL_LOW	0x01		/* -1.5dB < x < 0dB */
#define	CS_ERRINIT_ORL_HIGH	0x02		/* 0dB < x < 1.5dB */
#define	CS_ERRINIT_ORL_VHIGH	0x03		/* > 1.5dB overrange */
#define	CS_ERRINIT_ORR_VLOW	0x00		/* < -1.5 dB from full scale */
#define	CS_ERRINIT_ORR_LOW	0x04		/* -1.5dB < x < 0dB */
#define	CS_ERRINIT_ORR_HIGH	0x08		/* 0dB < x < 1.5dB */
#define	CS_ERRINIT_ORR_VHIGH	0x0c		/* > 1.5dB overrange */

/* Mode and ID: I12 */
#define	CS_MODEID_ID_MASK	0x0f		/* Codec ID */
#define	CS_MODEID_reserved1	0x10		/* reserved */
#define	CS_MODEID_reserved2	0x20		/* reserved */
#define	CS_MODEID_MODE2		0x40		/* enable mode2 operation */

#define	CS_MODEID_CS4231	0x0a		/* 1010 == cs4231 */

/* Loopback Control: I13 */
#define	CS_LOOP_LBE		0x01		/* loopback enable */
#define	CS_LOOP_reserved	0x02		/* reserved */
#define	CS_LOOP_LBA_MASK	0xfc		/* loopback attenuation */

/* Playback Upper Base: I14 */

/* Playback Lower Base: I15 */

/* Alternate Feature Enable I: I16 */
#define	CS_AFE1_DACZ		0x01		/* dac zero */
#define	CS_AFE1_SPE		0x02		/* serial port enable */
#define	CS_AFE1_SF_MASK		0x0c		/* serial format mask */
#define	CS_AFE1_PMCE		0x10		/* playback mode change enbl */
#define	CS_AFE1_CMCE		0x20		/* capture mode change enable */
#define	CS_AFE1_TE		0x40		/* timer enable */
#define	CS_AFE1_OLB		0x80		/* output level bit */

#define	CS_AFE1_SF_64E		0x00		/* 64 bit enhanced */
#define	CS_AFE1_SF_64		0x04		/* 64 bit */
#define	CS_AFE1_SF_32		0x08		/* 32 bit */
#define	CS_AFE1_SF_reserved	0x0c		/* reserved */
#define	CS_AFE1_OLB_2		0x00		/* full scale 2Vpp (-3dB) */
#define	CS_AFE1_OLB_28		0x80		/* full scale 2.8Vpp (0dB) */

/* Alternate Feature Enable II: I17 */
#define	CS_AFE2_HPF		0x01		/* high pass filter enable */
#define	CS_AFE2_XTALE		0x02		/* crystal enable */
#define	CS_AFE2_APAR		0x04		/* ADPCM pb accumulator reset */
#define	CS_AFE2_reserved	0x08		/* reserved */
#define	CS_AFE2_TEST_MASK	0xf0		/* factory test bits */

/* Left Line Input Control: I18 */
#define	CS_LLI_GAIN_MASK	0x1f		/* left line mix gain mask */
#define	CS_LLI_reserved1	0x20		/* reserved */
#define	CS_LLI_reserved2	0x40		/* reserved */
#define	CS_LLI_MUTE		0x80		/* left line mute */

/* Right Line Input Control: I19 */
#define	CS_RLI_GAIN_MASK	0x1f		/* right line mix gain mask */
#define	CS_RLI_reserved1	0x20		/* reserved */
#define	CS_RLI_reserved2	0x40		/* reserved */
#define	CS_RLI_MUTE		0x80		/* right line mute */

/* Timer Lower Base: I20 */

/* Timer Upper Base: I21 */

/* Reserved: I22 */

/* Alternate Feature Enable III: I23 */
#define	CS_AFE3_ACF		0x01		/* ADPCM capture freeze */
#define	CS_AFE3_reserved	0xfe		/* reserved bits */

/* Alternate Feature Status: I24 */
#define	CS_AFS_PU		0x01		/* playback underrun */
#define	CS_AFS_PO		0x02		/* playback overrun */
#define	CS_AFS_CO		0x04		/* capture underrun */
#define	CS_AFS_CU		0x08		/* capture overrun */
#define	CS_AFS_PI		0x10		/* playback interrupt */
#define	CS_AFS_CI		0x20		/* capture interrupt */
#define	CS_AFS_TI		0x40		/* timer interrupt */
#define	CS_AFS_reserved		0x80		/* reserved */

/* Version ID: I25 */
#define	CS_VID_CHIP_MASK	0x07		/* chip id mask */
#define	CS_VID_VER_MASK		0xe0		/* version number mask */

#define	CS_VID_CHIP_CS4231	0x00		/* CS4231 and CS4231A */
#define	CS_VID_VER_CS4231	0x80		/* CS4231 */
#define	CS_VID_VER_CS4232	0x82		/* CS4232 */
#define	CS_VID_VER_CS4231A	0xa0		/* CS4231A */

/* Mono Input & Output Control: I26 */
#define	CS_MONO_MIA_MASK	0x0f		/* mono attenuation mask */
#define	CS_MONO_reserved	0x10		/* reserved */
#define	CS_MONO_MBY		0x20		/* mono bypass */
#define	CS_MONO_MOM		0x40		/* mono output mute */
#define	CS_MONO_MIM		0x80		/* mono input mute */

/* Reserved: I27 */

/* Capture Data Format: I28 */
#define	CS_CDF_reserved		0x0f		/* reserved bits */
#define	CS_CDF_SM		0x10		/* Stereo/mono select */
#define	CS_CDF_FMT_MASK		0xe0		/* capture format mask */

#define	CS_CDF_SM_MONO		0x00		/* select mono capture */
#define	CS_CDF_SM_STEREO	0x10		/* select stereo capture */
#define	CS_CDF_FMT_ULINEAR	0x00		/* fmt: linear 8bit unsigned */
#define	CS_CDF_FMT_ULAW		0x20		/* fmt: ulaw, 8bit companded */
#define	CS_CDF_FMT_LINEAR_LE	0x40		/* fmt: linear 16bit little */
#define	CS_CDF_FMT_ALAW		0x60		/* fmt: alaw, 8bit companded */
#define	CS_CDF_FMT_reserved1	0x80		/* fmt: reserved */
#define	CS_CDF_FMT_ADPCM	0xa0		/* fmt: adpcm 4bit */
#define	CS_CDF_FMT_LINEAR_BE	0xc0		/* fmt: linear 16bit big */
#define	CS_CDF_FMT_reserved2	0xe0		/* fmt: reserved */

/* Reserved: I29 */

/* Capture Upper Base: I30 */

/* Capture Lower Base: I31 */

/*
 * APC DMA Register definitions
 */
#define	APC_CSR_RESET		0x00000001	/* reset */
#define	APC_CSR_CDMA_GO		0x00000004	/* capture dma go */
#define	APC_CSR_PDMA_GO		0x00000008	/* playback dma go */
#define	APC_CSR_CODEC_RESET	0x00000020	/* codec reset */
#define	APC_CSR_CPAUSE		0x00000040	/* capture dma pause */
#define	APC_CSR_PPAUSE		0x00000080	/* playback dma pause */
#define	APC_CSR_CMIE		0x00000100	/* capture pipe empty enb */
#define	APC_CSR_CMI		0x00000200	/* capture pipe empty intr */
#define	APC_CSR_CD		0x00000400	/* capture nva dirty */
#define	APC_CSR_CM		0x00000800	/* capture data lost */
#define	APC_CSR_PMIE		0x00001000	/* pb pipe empty intr enable */
#define	APC_CSR_PD		0x00002000	/* pb nva dirty */
#define	APC_CSR_PM		0x00004000	/* pb pipe empty */
#define	APC_CSR_PMI		0x00008000	/* pb pipe empty interrupt */
#define	APC_CSR_EIE		0x00010000	/* error interrupt enable */
#define	APC_CSR_CIE		0x00020000	/* capture intr enable */
#define	APC_CSR_PIE		0x00040000	/* playback intr enable */
#define	APC_CSR_GIE		0x00080000	/* general intr enable */
#define	APC_CSR_EI		0x00100000	/* error interrupt */
#define	APC_CSR_CI		0x00200000	/* capture interrupt */
#define	APC_CSR_PI		0x00400000	/* playback interrupt */
#define	APC_CSR_GI		0x00800000	/* general interrupt */

#define	APC_CSR_PLAY			( \
		APC_CSR_EI		| \
	 	APC_CSR_GIE		| \
		APC_CSR_PIE		| \
		APC_CSR_EIE		| \
		APC_CSR_PDMA_GO		| \
		APC_CSR_PMIE		)

#define	APC_CSR_CAPTURE			( \
		APC_CSR_EI		| \
	 	APC_CSR_GIE		| \
		APC_CSR_CIE		| \
		APC_CSR_EIE		| \
		APC_CSR_CDMA_GO	)

#define	APC_CSR_PLAY_PAUSE		(~( \
		APC_CSR_PPAUSE		| \
		APC_CSR_GI		| \
		APC_CSR_PI		| \
		APC_CSR_CI		| \
		APC_CSR_EI		| \
		APC_CSR_PMI		| \
		APC_CSR_PMIE		| \
		APC_CSR_CMI		| \
		APC_CSR_CMIE		) )

#define	APC_CSR_CAPTURE_PAUSE		(~( \
		APC_CSR_PPAUSE		| \
		APC_CSR_GI		| \
		APC_CSR_PI		| \
		APC_CSR_CI		| \
		APC_CSR_EI		| \
		APC_CSR_PMI		| \
		APC_CSR_PMIE		| \
		APC_CSR_CMI		| \
		APC_CSR_CMIE		) )

#define	APC_CSR_INTR_MASK		( \
		APC_CSR_GI		| \
		APC_CSR_PI		| \
		APC_CSR_CI		| \
		APC_CSR_EI		| \
		APC_CSR_PMI		| \
		APC_CSR_CMI		)
