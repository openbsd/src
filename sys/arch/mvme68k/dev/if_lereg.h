/*	$OpenBSD: if_lereg.h,v 1.10 2009/02/17 22:28:40 miod Exp $ */

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

#define LEMEMSIZE 0x4000

/*
 * LANCE registers.
 */
struct lereg1 {
        volatile u_int16_t      ler1_rdp;       /* data port */
        volatile u_int16_t      ler1_rap;       /* register select port */
};

#define	VLEREGSIZE	0x200
#define	VLEMEMSIZE	0x00040000 
#define	VLEMEMBASE	0xfd6c0000

/*
 * LANCE registers for MVME376
 */
struct vlereg1 {
   volatile u_int16_t      ler1_csr;       /* board control/status register */
   volatile u_int16_t      ler1_vec;       /* interrupt vector register */
   volatile u_int16_t      ler1_rdp;       /* data port */
   volatile u_int16_t      ler1_rap;       /* register select port */
   volatile u_int16_t      ler1_ear;       /* ethernet address register */
};

#define NVRAM_EN    0x0008 /* NVRAM enable bit              */
#define INTR_EN     0x0010 /* Interrupt enable bit          */
#define PARITYB     0x0020 /* Parity clear bit              */
#define HW_RS       0x0040 /* Hardware reset bit            */
#define SYSFAILB    0x0080 /* SYSFAIL bit                   */
#define NVRAM_RWEL   0xE0  /* Reset write enable latch      */
#define NVRAM_STO    0x60  /* Store ram to eeprom           */
#define NVRAM_SLP    0xA0  /* Novram into low power mode    */
#define NVRAM_WRITE  0x20  /* Writes word from location x   */
#define NVRAM_SWEL   0xC0  /* Set write enable latch        */
#define NVRAM_RCL    0x40  /* Recall eeprom data into ram   */
#define NVRAM_READ   0x00  /* Reads word from location x    */

#define CDELAY  delay(10000)
#define WRITE_CSR_OR(x)    reg1->ler1_csr=((struct le_softc *)sc)->csr|=x
#define WRITE_CSR_AND(x)   reg1->ler1_csr=((struct le_softc *)sc)->csr&=x
#define ENABLE_NVRAM       WRITE_CSR_AND(~NVRAM_EN)
#define DISABLE_NVRAM      WRITE_CSR_OR(NVRAM_EN)
#define ENABLE_INTR        WRITE_CSR_AND(~INTR_EN)
#define DISABLE_INTR       WRITE_CSR_OR(INTR_EN)
#define RESET_HW           WRITE_CSR_AND(~0xFF00);WRITE_CSR_AND(~HW_RS);CDELAY
#define SET_IPL(x)         WRITE_CSR_AND(~x)
#define SET_VEC(x)         reg1->ler1_vec=0;reg1->ler1_vec |=x;
#define PARITY_CL          WRITE_CSR_AND(~PARITYB)
#define SYSFAIL_CL         WRITE_CSR_AND(~SYSFAILB)
#define NVRAM_CMD(c,a)     for(i=0;i<8;i++){ \
                              reg1->ler1_ear=((c|(a<<1))>>i); \
                              CDELAY; \
                           } \
                           CDELAY;


