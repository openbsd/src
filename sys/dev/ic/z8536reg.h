/*	$OpenBSD: z8536reg.h,v 1.3 2009/03/08 16:03:06 miod Exp $ */

/*
 * Copyright (c) 1997 Per Fogelstrom
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/*
 * Zilog CIO registers.
 */
#define	ZCIO_MIC	0	/* Master Interrupt Control */
#define	ZCIO_MCC	1	/* Master Configuration Control */
#define	ZCIO_PAIV	2	/* Port A, Interrupt Vector */
#define	ZCIO_PBIV	3	/* Port B, Interrupt Vector */
#define	ZCIO_CTIV	4	/* Counter/Timer, Interrupt Vector */
#define	ZCIO_PCPOL	5	/* Port C, Data Path Polarity */
#define	ZCIO_PCDIR	6	/* Port C, Data Direction */
#define	ZCIO_PCSIOC	7	/* Port C, Special I/O Control */

#define	ZCIO_PACS	8	/* Port A, Command and Status */
#define	ZCIO_PBCS	9	/* Port B, Command and Status */
#define	ZCIO_CT1CS	10	/* Counter 1, Command and Status */
#define	ZCIO_CT2CS	11	/* Counter 2, Command and Status */
#define	ZCIO_CT3CS	12	/* Counter 2, Command and Status */
#define	ZCIO_PADATA	13	/* Port A Data (can be accessed directly) */
#define	ZCIO_PBDATA	14	/* Port B Data (can be accessed directly) */
#define	ZCIO_PCDATA	15	/* Port C Data (can be accessed directly) */

#define	ZCIO_CT1CCM	16	/* Counter/Timer 1 Current Count MSB */
#define	ZCIO_CT1CCL	17	/* Counter/Timer 1 Current Count LSB */
#define	ZCIO_CT2CCM	18	/* Counter/Timer 2 Current Count MSB */
#define	ZCIO_CT2CCL	19	/* Counter/Timer 2 Current Count LSB */
#define	ZCIO_CT3CCM	20	/* Counter/Timer 3 Current Count MSB */
#define	ZCIO_CT3CCL	21	/* Counter/Timer 3 Current Count LSB */
#define	ZCIO_CT1TCM	22	/* Counter/Timer 1 Time Constant MSB */
#define	ZCIO_CT1TCL	23	/* Counter/Timer 1 Time Constant LSB */
#define	ZCIO_CT2TCM	24	/* Counter/Timer 2 Time Constant MSB */
#define	ZCIO_CT2TCL	25	/* Counter/Timer 2 Time Constant LSB */
#define	ZCIO_CT3TCM	26	/* Counter/Timer 3 Time Constant MSB */
#define	ZCIO_CT3TCL	27	/* Counter/Timer 3 Time Constant LSB */
#define	ZCIO_CT1MD	28	/* Counter/Timer 1 Mode Specification */
#define	ZCIO_CT2MD	29	/* Counter/Timer 2 Mode Specification */
#define	ZCIO_CT3MD	30	/* Counter/Timer 3 Mode Specification */

#define	ZCIO_CVEC	31	/* Current Vector */

#define	ZCIO_PAMD	32	/* Port A Mode Specification */
#define	ZCIO_PAHS	33	/* Port A Handshake Specification */
#define	ZCIO_PAPOL	34	/* Port A Polarity Specification */
#define	ZCIO_PADIR	35	/* Port A Direction Specification */
#define	ZCIO_PASIOC	36	/* Port A Special I/O Specification */
#define	ZCIO_PASPP	37	/* Port A Pattern Polarity Specification */
#define	ZCIO_PASPT	38	/* Port A Pattern Transition Specification */
#define	ZCIO_PASPM	39	/* Port A Pattern MASK Specification */

#define	ZCIO_PBMD	40	/* Port B Mode Specification */
#define	ZCIO_PBHS	41	/* Port B Handshake Specification */
#define	ZCIO_PBPOL	42	/* Port B Polarity Specification */
#define	ZCIO_PBDIR	43	/* Port B Direction Specification */
#define	ZCIO_PBSIOC	44	/* Port B Special I/O Specification */
#define	ZCIO_PBSPP	45	/* Port B Pattern Polarity Specification */
#define	ZCIO_PBSPT	46	/* Port B Pattern Transition Specification */
#define	ZCIO_PBSPM	47	/* Port B Pattern MASK Specification */

#define ZCIO_MIC_MIE		0x80	/* Master Interrupt Enable */
#define ZCIO_MIC_DLC		0x40	/* Disable Lower Chain */
#define ZCIO_MIC_NV		0x20	/* No Vector */
#define ZCIO_MIC_PAVIS		0x10	/* Port A Vector Includes Status */
#define ZCIO_MIC_PBVIS		0x08	/* Port B Vector Includes Status */
#define ZCIO_MIC_CTVIS		0x04	/* C/T Vector Includes Status */
#define ZCIO_MIC_RJA		0x02	/* Right Justified Addresses */
#define ZCIO_MIC_RESET		0x01	/* Reset */

#define ZCIO_MCC_PBE		0x80	/* Port B Enable */
#define ZCIO_MCC_CT1E		0x40	/* Counter/Timer 1 Enable */
#define ZCIO_MCC_CT2E		0x20	/* Counter/Timer 2 Enable */
#define ZCIO_MCC_CT3E		0x10	/* Counter/Timer 3 Enable */
#define ZCIO_MCC_PLC		0x08	/* Port Link Control */
#define ZCIO_MCC_PAE		0x04	/* Port A Enable */

#define ZCIO_CTMD_CSC		0x80	/* Continuous Single Cycle */
#define ZCIO_CTMD_EOE		0x40	/* External Output Enable  */
#define ZCIO_CTMD_ECE		0x20	/* External Count Enable   */
#define ZCIO_CTMD_ETE		0x10	/* External Trigger Enable */
#define ZCIO_CTMD_EGE		0x08	/* External Gate Enable    */
#define ZCIO_CTMD_REB		0x04	/* Retrigger Enable Bit    */
#define ZCIO_CTMD_PO		0x00	/* Pulse Output            */
#define ZCIO_CTMD_OSO		0x01	/* One Shot Output         */
#define ZCIO_CTMD_SWO		0x02	/* Square Wave Output      */

/* CTCS read values */
#define	ZCIO_CTCS_IUS		0x80	/* Interrupt Under Service */
#define	ZCIO_CTCS_IE		0x40	/* Interrupt Enable */
#define	ZCIO_CTCS_IP		0x20	/* Interrupt Pending */
#define	ZCIO_CTCS_IERR		0x10	/* Interrupt Error */
#define	ZCIO_CTCS_CIP		0x01	/* Count In Progress */

/* CTCS write values */
#define	ZCIO_CTCS_C_IE		0xe0	/* Clear IE */
#define	ZCIO_CTCS_S_IE		0xc0	/* Set IE */
#define	ZCIO_CTCS_C_IP		0xa0	/* Clear IP */
#define	ZCIO_CTCS_S_IP		0x80	/* Set IP */
#define	ZCIO_CTCS_C_IUS		0x60	/* Clear IUS */
#define	ZCIO_CTCS_S_IUS		0x40	/* Set IUS */
#define	ZCIO_CTCS_C_IUS_IP	0x20	/* Clear IUS and IP */
#define	ZCIO_CTCS_RCC		0x08	/* Read Counter Control */
#define ZCIO_CTCS_GCB		0x04	/* Gate Command Bit */
#define ZCIO_CTCS_TCB		0x02	/* Trigger Command Bit */
