/*	$NetBSD: mfp.h,v 1.2 1995/03/26 07:24:37 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MACHINE_MFP_H
#define _MACHINE_MFP_H
/*
 * Atari TT hardware: MFP1/MFP2
 * Motorola 68901 Multi-Function Peripheral
 */

#define	MFP	((struct mfp *)AD_MFP)
#define	MFP2	((struct mfp *)AD_MFP2)

struct mfp {
	volatile u_char	mfb[48];	/* use only the odd bytes */
};

#define	mf_gpip		mfb[ 1]	/* general purpose I/O interrupt port	*/
#define	mf_aer		mfb[ 3]	/* active edge register			*/
#define	mf_ddr		mfb[ 5]	/* data direction register		*/
#define	mf_iera		mfb[ 7]	/* interrupt enable register A		*/
#define	mf_ierb		mfb[ 9]	/* interrupt enable register B		*/
#define	mf_ipra		mfb[11]	/* interrupt pending register A		*/
#define	mf_iprb		mfb[13]	/* interrupt pending register B		*/
#define	mf_isra		mfb[15]	/* interrupt in-service register A	*/
#define	mf_isrb		mfb[17]	/* interrupt in-service register B	*/
#define	mf_imra		mfb[19]	/* interrupt mask register A		*/
#define	mf_imrb		mfb[21]	/* interrupt mask register B		*/
#define	mf_vr		mfb[23]	/* vector register			*/
#define	mf_tacr		mfb[25]	/* timer control register A		*/
#define	mf_tbcr		mfb[27]	/* timer control register B		*/
#define	mf_tcdcr	mfb[29]	/* timer control register C+D		*/
#define	mf_tadr		mfb[31]	/* timer data register A		*/
#define	mf_tbdr		mfb[33]	/* timer data register B		*/
#define	mf_tcdr		mfb[35]	/* timer data register C		*/
#define	mf_tddr		mfb[37]	/* timer data register D		*/
#define	mf_scr		mfb[39]	/* synchronous character register	*/
#define	mf_ucr		mfb[41]	/* USART control register		*/
#define	mf_rsr		mfb[43]	/* receiver status register		*/
#define	mf_tsr		mfb[45]	/* transmitter status register		*/
#define	mf_udr		mfb[47]	/* USART data register			*/

/* names of IO port bits: */
#define	IO_PBSY		0x01	/* Parallel Busy			*/
#define	IO_SDCD		0x02	/* Serial Data Carrier Detect		*/
#define	IO_SCTS		0x04	/* Serial Clear To Send			*/
/*		0x08		*//* reserved				*/
#define	IO_AINT		0x10	/* ACIA interrupt (KB or MIDI)		*/
#define	IO_DINT		0x20	/* DMA interrupt (FDC or HDC)		*/
#define	IO_SRI		0x40	/* Serial Ring Indicator		*/
#define	IO_MONO		0x80	/* Monochrome Monitor Detect		*/

/* names of interrupts in register A: MFP1 */
#define	IA_MONO		0x80	/* IO_MONO				*/
#define	IA_SRI		0x40	/* IO_SRI				*/
#define	IA_TIMA		0x20	/* Timer A				*/
#define	IA_RRDY		0x10	/* Serial Receiver Ready(=Full)		*/
#define	IA_RERR		0x08	/* Serial Receiver Error		*/
#define	IA_TRDY		0x04	/* Serial Transmitter Ready(=Empty)	*/
#define	IA_TERR		0x02	/* Serial Transmitter Error		*/
#define	IA_TIMB		0x01	/* Timer B				*/

/* names of interrupts in register A: MFP2 */
#define	IA_SCSI		0x80	/* SCSI-controller			*/
#define	IA_RTC		0x40	/* Real Time Clock			*/
#define IA_TIMA2	0x20	/* Timer A				*/
/*			0x10	*//* reserved				*/
/*			0x08	*//* reserved				*/
/*			0x04	*//* reserved				*/
/*			0x02	*//* reserved				*/
#define	IA_TIMB2	0x01	/* Timer B				*/

/* names of interrupts in register B: MFP1*/
#define	IB_DINT		0x80	/* IO_DINT: from DMA devices		*/
#define	IB_AINT		0x40	/* IO_AINT: from kbd or midi		*/
#define	IB_TIMC		0x20	/* Timer C				*/
#define	IB_TIMD		0x10	/* Timer D				*/
/*			0x08	*//* reserved				*/
#define	IB_SCTS		0x04	/* IO_SCTS				*/
#define	IB_SDCD		0x02	/* IO_SDCD				*/
#define	IB_PBSY		0x01	/* IO_PBSY				*/

/* names of interrupts in register B: MFP2*/
#define	IB_SCDM		0x80	/* SCSI-dma				*/
#define	IB_DCHG		0x40	/* Diskette change			*/
/*			0x20	*//* reserved				*/
/*			0x10	*//* reserved				*/
#define	IB_RISB		0x80	/* Serial Ring indicator SCC port B	*/
#define	IB_DMSC		0x40	/* SCC-dma				*/
#define IB_J602_3	0x02	/* Pin 3 J602				*/
#define IB_J602_1	0x01	/* Pin 1 J602				*/

/* bits in VR: */
#define	V_S		0x08	/* software end-of-interrupt mode	*/
#define	V_V		0xF0	/* four high bits of vector		*/

/* bits in TCR: */
/*			0x07	*//* divider				*/
#define	T_STOP		0x00	/* don't count				*/
#define	T_Q004		0x01	/* divide by 4				*/
#define	T_Q010		0x02	/* divide by 10				*/
#define	T_Q016		0x03	/* divide by 16				*/
#define	T_Q050		0x04	/* divide by 50				*/
#define	T_Q064		0x05	/* divide by 64				*/
#define	T_Q100		0x06	/* divide by 100			*/
#define	T_Q200		0x07	/* divide by 200			*/
#define	T_EXTI		0x08	/* use extern impulse			*/
#define	T_LOWO		0x10	/* force output low			*/

/* bits in UCR: */
/*			0x01	*//* not used				*/
#define	U_EVEN		0x02	/* even parity				*/
#define	U_PAR		0x04	/* use parity				*/
/*		0x18		*//* sync/async and stop bits		*/
#define	U_SYNC		0x00	/* synchrone				*/
#define	U_ST1		0x08	/* async, 1 stop bit			*/
#define	U_ST1_5		0x10	/* async, 1.5 stop bit			*/
#define	U_ST2		0x18	/* async, 2 stop bits			*/
/*		0x60		*//* number of data bits		*/
#define	U_D8		0x00	/* 8 data bits				*/
#define	U_D7		0x20	/* 7 data bits				*/
#define	U_D6		0x40	/* 6 data bits				*/
#define	U_D5		0x60	/* 5 data bits				*/
#define	U_Q16		0x80	/* divide clock by 16			*/

/* bits in RSR: */
#define	RS_ENA		0x01	/* Receiver Enable			*/
#define	RS_STRIP	0x02	/* Synchronous Strip Enable		*/
#define	RS_CIP		0x04	/* Character in Progress		*/
#define	RS_BREAK	0x08	/* Break Detected			*/
#define	RS_FE		0x10	/* Frame Error				*/
#define	RS_PE		0x20	/* Parity Error				*/
#define	RS_OE		0x40	/* Overrun Error			*/
#define	RS_FULL		0x80	/* Buffer Full				*/

/* bits in TSR: */
#define	TS_ENA		0x01	/* Transmitter Enable					*/
/*			0x06	*//* state of dead transmitter output	*/
#define	TS_TRI		0x00	/* Quiet Output Tristate		*/
#define	TS_LOW		0x02	/* Quiet Output Low			*/
#define	TS_HIGH		0x04	/* Quiet Output High			*/
#define	TS_BACK		0x06	/* Loop Back Mode			*/
#define	TS_BREAK	0x08	/* Break Detected			*/
#define	TS_EOT		0x10	/* End of Transmission			*/
#define	TS_TURN		0x20	/* Auto Turnaround			*/
#define	TS_UE		0x40	/* Underrun Error			*/
#define	TS_EMPTY	0x80	/* Buffer Empty				*/
#endif /* _MACHINE_MFP_H */
