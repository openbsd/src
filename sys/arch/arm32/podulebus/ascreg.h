/* $NetBSD: ascreg.h,v 1.4 1996/03/07 23:54:29 mark Exp $ */

/*
 * Copyright (c) 1996 Mark Brinicombe
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from:ahscreg.h,v 1.2 1994/10/26 02:02:46
 */

#ifndef _ASCREG_H_
#define _ASCREG_H_

/*
 * Hardware layout of the A3000 SDMAC. This also contains the
 * registers for the sbic chip, but in favor of separating DMA and
 * scsi, the scsi-driver doesn't make use of this dependency
 */

#define v_char		volatile char
#define	v_int		volatile int
#define vu_char		volatile u_char
#define vu_short	volatile u_short
#define vu_int		volatile u_int

struct sdmac {
	short		pad0;
	vu_short DAWR;		/* DACK Width Register WO */
	vu_int   WTC;		/* Word Transfer Count Register RW */
	short		pad1;
	vu_short CNTR;		/* Control Register RW */
	vu_int   ACR;		/* Address Count Register RW */
	short		pad2;
	vu_short ST_DMA;	/* Start DMA Transfers RW-Strobe */
	short		pad3;
	vu_short FLUSH;		/* Flush FIFO RW-Strobe */
	short		pad4;
	vu_short CINT;		/* Clear Interrupts RW-Strobe */
	short		pad5;
	vu_short ISTR;		/* Interrupt Status Register RO */
	int		pad6[7];
	short		pad7;
	vu_short SP_DMA;	/* Stop DMA Transfers RW-Strobe */
	char		pad8;
	vu_char  SASR;		/* sbic asr */
	char		pad9;
	vu_char  SCMD;		/* sbic data */
};

/*
 * value to go into DAWR
 */
#define DAWR_AHSC	3	/* according to A3000T service-manual */

/*
 * bits defined for CNTR
 */
#define CNTR_TCEN	(1<<5)	/* Terminal Count Enable */
#define CNTR_PREST	(1<<4)	/* Perp Reset (not implemented :-((( ) */
#define CNTR_PDMD	(1<<3)  /* Perp Device Mode Select (1=SCSI,0=XT/AT) */
#define CNTR_INTEN	(1<<2)	/* Interrupt Enable */
#define CNTR_DDIR	(1<<1)	/* Device Direction. 1==rd host, wr perp */
#define CNTR_IO_DX	(1<<0)	/* IORDY & CSX1 Polarity Select */

/*
 * bits defined for ISTR
 */
#define ISTR_INTX	(1<<8)	/* XT/AT Interrupt pending */
#define ISTR_INT_F	(1<<7)	/* Interrupt Follow */
#define ISTR_INTS	(1<<6)	/* SCSI Peripheral Interrupt */
#define ISTR_E_INT	(1<<5)	/* End-Of-Process Interrupt */
#define ISTR_INT_P	(1<<4)	/* Interrupt Pending */
#define ISTR_UE_INT	(1<<3)	/* Under-Run FIFO Error Interrupt */
#define ISTR_OE_INT	(1<<2)	/* Over-Run FIFO Error Interrupt */
#define ISTR_FF_FLG	(1<<1)	/* FIFO-Full Flag */
#define ISTR_FE_FLG	(1<<0)	/* FIFO-Empty Flag */

#define DMAGO_READ 0x01


/* Addresses relative to podule base */

#define ASC_INTSTATUS	0x2000
#define ASC_CLRINT	0x2000
#define ASC_PAGEREG	0x3000

/* Addresses relative to module base */

#define ASC_DMAC		0x3000
#define ASC_SBIC		0x2000
#define ASC_SRAM		0x0000

#define ASC_SRAM_BLKSIZE	0x1000

#define IS_IRQREQ		0x01
#define IS_DMAC_IRQ		0x02
#define IS_SBIC_IRQ		0x08

#if 0
/* SBIC Commands */

#define SBIC_CMD_Reset		0x00	/* Reset the SBIC */
#define SBIC_Abort		0x01	/* Abort command */
#define SBIC_Sel_tx_wATN	0x08	/* Select and Transfer with ATN */
#define SBIC_Sel_tx_woATN	0x09	/* Select and Transfer without ATN */

/* SBIC status codes */

#define SBIC_ResetOk	0x00
#define SBIC_ResetAFOk	0x01

/* SBIC registers		      bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 */

#define SBIC_OWNID	0x00	/* RW  FS1  FS0    0  EHP  EAF  ID2  ID1  ID0 */
#define SBIC_CONTROL	0x01	/* RW  DM2  DM1  DM0  HHP  EDI  IDI   HA  HSP */
#define SBIC_TIMEREG	0x02	/* RW timeout period  value = Tper*Ficlk/80d  */
#define SBIC_CDB1TSECT	0x03	/* RW CDB byte 1 & Total sectors per track    */
#define SBIC_CDB2THEAD	0x04	/* RW CDB byte 2 & Total number of heads      */
#define SBIC_CDB3TCYL1	0x05	/* RW CDB byte 3 & Total no. of cylinders MSB */
#define SBIC_CDB4TCYL2	0x06	/* RW CDB byte 4 & Total no. of cylinders LSB */
#define SBIC_CDB5LADR1	0x07	/* RW CDB byte 5 & Logical addr to translate  */
#define SBIC_CBD6LADR2	0x08	/* RW CDB byte 6 & Logical addr to translate  */
#define SBIC_CDB7LADR3	0x09	/* RW CDB byte 7 & Logical addr to translate  */
#define SBIC_CDB8LADR4	0x0A	/* RW CDB byte 8 & Logical addr to translate  */
#define SBIC_CDB9SECT	0x0B	/* RW CDB byte 9 & Translation sector result  */
#define SBIC_CDB10HEAD	0x0C	/* RW CDB byte 10 & Translation head result   */
#define SBIC_CDB11CYL1	0x0D	/* RW CDB byte 11 & Translation cyl result MSB*/
#define SBIC_CDB12CYL2	0x0E	/* RW CDB byte 12 & Translation cyl result LSB*/
#define SBIC_TARGETLUN	0x0F	/* RW  TLV  DOK    0    0    0  TL2  TL1  TL0 */
#define SBIC_COMPHASE	0x10	/* RW Command Phase Register for multi-phase  */
#define SBIC_SYNCTX	0x11	/* RW    0  TP2  TP1  TP0  OF3  OF2  OF1  OF0 */
#define SBIC_TXCOUNT1	0x12	/* RW Transfer count MSB                      */
#define SBIC_TXCOUNT2	0x13	/* RW Transfer count                          */
#define SBIC_TXCOUNT3	0x14	/* RW Transfer count LSB                      */
#define SBIC_DESTID	0x15	/* RW  SCC  DPD    0    0    0  DI2  DI1  DI0 */
#define SBIC_SOURCEID	0x16	/* RW   ER   ES  DSP    0  SIV  SI2  SI1  SI0 */
#define SBIC_SCSISTAT	0x17	/* RO **Interrupt type***  **Int. qualifier** */
#define SBIC_COMMAND	0x18	/* RW  SBT *********Command code************* */
#define SBIC_DATA	0x19	/* RW Access to data i/o FIFO for polled use  */

#define SBIC_ADDRREG	0x00
#define SBIC_DATAREG	0x04
#define SBIC_AUX_STATUS	0x00

/*
 * My ID register, and/or CDB Size
 */
  
#define SBIC_ID_FS_8_10		0x00	/* Input clock is  8-10 Mhz */
					/* 11 Mhz is invalid */
#define SBIC_ID_FS_12_15	0x40	/* Input clock is 12-15 Mhz */
#define SBIC_ID_FS_16_20	0x80	/* Input clock is 16-20 Mhz */
#define SBIC_ID_EHP		0x10	/* Enable host parity */
#define SBIC_ID_EAF		0x08	/* Enable Advanced Features */
#define SBIC_ID_MASK		0x07
#define SBIC_ID_CBDSIZE_MASK	0x0f	/* if unk SCSI cmd group */

/*
 * Control register
*/
  
#define SBIC_CTL_DMA		0x80	/* Single byte dma */
#define SBIC_CTL_DBA_DMA	0x40	/* direct buffer acces (bus master)*/
#define SBIC_CTL_BURST_DMA	0x20	/* continuous mode (8237) */
#define SBIC_CTL_NO_DMA		0x00	/* Programmed I/O */
#define SBIC_CTL_HHP		0x10	/* Halt on host parity error */
#define SBIC_CTL_EDI		0x08	/* Ending disconnect interrupt */
#define SBIC_CTL_IDI		0x04	/* Intermediate disconnect interrupt*/
#define SBIC_CTL_HA		0x02	/* Halt on ATN */
#define SBIC_CTL_HSP		0x01	/* Halt on SCSI parity error */

/*
 * Destination ID register
 */

#define SBIC_DID_DPD		0x40	/* Data Phase Direction */

/*
 * Auxiliary Status Register
 */
  
#define SBIC_ASR_INT		0x80	/* Interrupt pending */
#define SBIC_ASR_LCI		0x40	/* Last command ignored */
#define SBIC_ASR_BSY		0x20	/* Busy, only cmd/data/asr readable */
#define SBIC_ASR_CIP		0x10	/* Busy, cmd unavail also */
#define SBIC_ASR_xxx		0x0c
#define SBIC_ASR_PE		0x02	/* Parity error (even) */
#define SBIC_ASR_DBR		0x01	/* Data Buffer Ready */
   
/* DMAC constants */

#define DMAC_Bits		0x01
#define DMAC_Ctrl1		0x60
#define DMAC_Ctrl2		0x01
#define DMAC_CLEAR_MASK		0x0E
#define DMAC_SET_MASK		0x0F
#define DMAC_DMA_RD_MODE	0x04
#define DMAC_DMA_WR_MODE	0x08

/* DMAC registers */

#define DMAC_INITIALISE	0x0000	/* WO ---- ---- ---- ---- ---- ----  16B  RES */
#define DMAC_CHANNEL	0x0200	/* R  ---- ---- ---- BASE SEL3 SEL2 SEL1 SEL0 */
				/* W  ---- ---- ---- ---- ---- BASE *SELECT** */
#define DMAC_TXCNTLO	0x0004	/* RW   C7   C6   C5   C4   C3   C2   C1   C0 */
#define DMAC_TXCNTHI	0x0204	/* RW  C15  C14  C13  C12  C11  C10   C9   C8 */
#define DMAC_TXADRLO	0x0008	/* RW   A7   A6   A5   A4   A3   A2   A1   A0 */
#define DMAC_TXADRMD	0x0208	/* RW  A15  A14  A13  A12  A11  A10   A9   A8 */
#define DMAC_TXADRHI	0x000C	/* RW  A23  A22  A21  A20  A19  A18  A17  A16 */
#define DMAC_DEVCON1	0x0010	/* RW  AKL  RQL  EXW  ROT  CMP DDMA AHLD  MTM */
#define DMAC_DEVCON2	0x0210	/* RW ---- ---- ---- ---- ---- ----  WEV BHLD */
#define DMAC_MODECON	0x0014	/* RW **TMODE** ADIR AUTI **TDIR*** ---- WORD */
#define DMAC_STATUS	0x0214	/* RO  RQ3  RQ2  RQ1  RQ0  TC3  TC2  TC1  TC0 */
#if 0
templo  = dmac + 0x0018;/*    RO   T7   T6   T5   T4   T3   T2   T1   T0 */
temphi  = dmac + 0x0218;/*    RO  T15  T14  T13  T12  T11  T10   T9   T8 */
#endif
#define DMAC_REQREG	0x001C	/* RW ---- ---- ---- ---- SRQ3 SRQ2 SRQ1 SRQ0 */
#define DMAC_MASKREG	0x021C	/* RW ---- ---- ---- ----   M3   M2   M1   M0 */

#ifndef _LOCORE
#define WriteSBIC(a, d) \
	WriteByte(sbic_base + SBIC_ADDRREG, a); \
	WriteByte(sbic_base + SBIC_DATAREG, d);

/*
#define ReadSBIC(a) \
	(WriteByte(sbic_base, a), ReadWord(sbic_base + 4) & 0xff)
*/
#define ReadSBIC(a) \
	ReadSBIC1(sbic_base, a)


static inline int
ReadSBIC1(sbic_base, a)
	u_int sbic_base;
	int a;
{
	WriteByte(sbic_base + SBIC_ADDRREG, a);
	return(ReadByte(sbic_base + SBIC_DATAREG));
}


#define WriteDMAC(a, d) WriteByte(dmac_base + a, d)
#define ReadDMAC(a) ReadByte(dmac_base + a)
#endif


#endif
#endif /* _ASCREG_H_ */
