/*	$OpenBSD: if_lereg.h,v 1.6 2007/05/25 21:27:15 krw Exp $ */

/*-
 * Copyright (c) 1982, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 * @(#)if_lereg.h	8.2 (Berkeley) 10/30/93
 */

#define	VLEMEMSIZE	0x00040000
#define	VLEMEMBASE	0xfd6c0000

/*
 * LANCE registers for MVME376
 */
struct vlereg1 {
	volatile u_int16_t	ler1_csr;	/* board control/status register */
	volatile u_int16_t	ler1_vec;	/* interrupt vector register */
	volatile u_int16_t	ler1_rdp;	/* data port */
	volatile u_int16_t	ler1_rap;	/* register select port */
	volatile u_int16_t	ler1_ear;	/* ethernet address register */
};

#define	NVRAM_EN	0x0008	/* NVRAM enable bit (active low) */
#define	INTR_EN		0x0010	/* interrupt enable bit (active low) */
#define	PARITYB		0x0020	/* parity error clear bit */
#define	HW_RS		0x0040	/* hardware reset bit (active low) */
#define	SYSFAILB	0x0080	/* SYSFAIL bit */

#define	NVRAM_RWEL	0xe0	/* Reset write enable latch      */
#define	NVRAM_STO	0x60	/* Store ram to eeprom           */
#define	NVRAM_SLP	0xa0	/* Novram into low power mode    */
#define	NVRAM_WRITE	0x20	/* Writes word from location x   */
#define	NVRAM_SWEL	0xc0	/* Set write enable latch        */
#define	NVRAM_RCL	0x40	/* Recall eeprom data into ram   */
#define	NVRAM_READ	0x00	/* Reads word from location x    */

#define	CDELAY		delay(10000)
#define	WRITE_CSR_OR(x) \
	do { \
		((struct le_softc *)sc)->sc_csr |= (x); \
		reg1->ler1_csr = ((struct le_softc *)sc)->sc_csr; \
	} while (0)
#define	WRITE_CSR_AND(x) \
	do { \
		((struct le_softc *)sc)->sc_csr &= ~(x); \
		reg1->ler1_csr = ((struct le_softc *)sc)->sc_csr; \
	} while (0)
#define	ENABLE_NVRAM	WRITE_CSR_AND(NVRAM_EN)
#define	DISABLE_NVRAM	WRITE_CSR_OR(NVRAM_EN)
#define	ENABLE_INTR	WRITE_CSR_AND(INTR_EN)
#define	DISABLE_INTR	WRITE_CSR_OR(INTR_EN)
#define	RESET_HW \
	do { \
		WRITE_CSR_AND(HW_RS); \
		CDELAY; \
	} while (0)
#define	SET_VEC(x) \
	reg1->ler1_vec = (x)
#define	SYSFAIL_CL	WRITE_CSR_AND(SYSFAILB)
