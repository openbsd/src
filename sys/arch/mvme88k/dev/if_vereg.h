/*	$OpenBSD: if_vereg.h,v 1.1 1999/05/29 04:41:43 smurph Exp $ */

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
 * @(#)if_lereg.h	8.2 (Berkeley) 10/30/93
 */

#define LEMEMSIZE 0x40000

/*
 * LANCE registers.
 */
struct vereg1 {
   volatile u_int16_t      ver1_csr;       /* board control/status register */
   volatile u_int16_t      ver1_vec;       /* interupt vector register */
   volatile u_int16_t      ver1_rdp;       /* data port */
   volatile u_int16_t      ver1_rap;       /* register select port */
   volatile u_int16_t      ver1_ear;       /* ethernet address register */
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
#define WRITE_CSR_OR(x)    reg1->ver1_csr=sc->csr|=x
#define WRITE_CSR_AND(x)   reg1->ver1_csr=sc->csr&=x
#define ENABLE_NVRAM       WRITE_CSR_AND(~NVRAM_EN)
#define DISABLE_NVRAM      WRITE_CSR_OR(NVRAM_EN)
#define ENABLE_INTR        WRITE_CSR_AND(~INTR_EN)
#define DISABLE_INTR       WRITE_CSR_OR(INTR_EN)
#define RESET_HW           WRITE_CSR_AND(~0xFF00);WRITE_CSR_AND(~HW_RS);CDELAY
#define SET_IPL(x)         WRITE_CSR_AND(~x)
#define SET_VEC(x)         reg1->ver1_vec=0;reg1->ver1_vec |=x;
#define PARITY_CL          WRITE_CSR_AND(~PARITYB)
#define SYSFAIL_CL         WRITE_CSR_AND(~SYSFAILB)
#define NVRAM_CMD(c,a)     for(i=0;i<8;i++){ \
                              reg1->ver1_ear=((c|(a<<1))>>i); \
                              CDELAY; \
                           } \
                           CDELAY;

#define LEBLEN    1536  /* ETHERMTU + header + CRC */
#define LEMINSIZE 60    /* should be 64 if mode DTCR is set */

/*
 * Receive message descriptor
 */
struct vermd {
	u_int16_t rmd0;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t  rmd1_bits;
	u_int8_t  rmd1_hadr;
#else
	u_int8_t  rmd1_hadr;
	u_int8_t  rmd1_bits;
#endif
	int16_t	  rmd2;
	u_int16_t rmd3;
};

/*
 * Transmit message descriptor
 */
struct vetmd {
	u_int16_t tmd0;
#if BYTE_ORDER == BIG_ENDIAN
	u_int8_t  tmd1_bits;
	u_int8_t  tmd1_hadr;
#else
	u_int8_t  tmd1_hadr;
	u_int8_t  tmd1_bits;
#endif
	int16_t	  tmd2;
	u_int16_t tmd3;
};

/*
 * Initialization block
 */
struct veinit {
	u_int16_t init_mode;		/* +0x0000 */
	u_int16_t init_padr[3];		/* +0x0002 */
	u_int16_t init_ladrf[4];	/* +0x0008 */
	u_int16_t init_rdra;		/* +0x0010 */
	u_int16_t init_rlen;		/* +0x0012 */
	u_int16_t init_tdra;		/* +0x0014 */
	u_int16_t init_tlen;		/* +0x0016 */
	int16_t	  pad0[4];		/* Pad to 16 shorts */
};

#define	LE_INITADDR(sc)		(sc->sc_initaddr)
#define	LE_RMDADDR(sc, bix)	(sc->sc_rmdaddr + sizeof(struct vermd) * (bix))
#define	LE_TMDADDR(sc, bix)	(sc->sc_tmdaddr + sizeof(struct vetmd) * (bix))
#define	LE_RBUFADDR(sc, bix)	(sc->sc_rbufaddr + LEBLEN * (bix))
#define	LE_TBUFADDR(sc, bix)	(sc->sc_tbufaddr + LEBLEN * (bix))

/* register addresses */
#define	LE_CSR0		0x0000		/* Control and status register */
#define	LE_CSR1		0x0001		/* low address of init block */
#define	LE_CSR2		0x0002		/* high address of init block */
#define	LE_CSR3		0x0003		/* Bus master and control */

/* Control and status register 0 (csr0) */
#define	LE_C0_ERR	0x8000		/* error summary */
#define	LE_C0_BABL	0x4000		/* transmitter timeout error */
#define	LE_C0_CERR	0x2000		/* collision */
#define	LE_C0_MISS	0x1000		/* missed a packet */
#define	LE_C0_MERR	0x0800		/* memory error */
#define	LE_C0_RINT	0x0400		/* receiver interrupt */
#define	LE_C0_TINT	0x0200		/* transmitter interrupt */
#define	LE_C0_IDON	0x0100		/* initalization done */
#define	LE_C0_INTR	0x0080		/* interrupt condition */
#define	LE_C0_INEA	0x0040		/* interrupt enable */
#define	LE_C0_RXON	0x0020		/* receiver on */
#define	LE_C0_TXON	0x0010		/* transmitter on */
#define	LE_C0_TDMD	0x0008		/* transmit demand */
#define	LE_C0_STOP	0x0004		/* disable all external activity */
#define	LE_C0_STRT	0x0002		/* enable external activity */
#define	LE_C0_INIT	0x0001		/* begin initalization */

#define	LE_C0_BITS \
    "\20\20ERR\17BABL\16CERR\15MISS\14MERR\13RINT\
\12TINT\11IDON\10INTR\07INEA\06RXON\05TXON\04TDMD\03STOP\02STRT\01INIT"

/* Control and status register 3 (csr3) */
#define	LE_C3_BSWP	0x0004		/* byte swap */
#define	LE_C3_ACON	0x0002		/* ALE control, eh? */
#define	LE_C3_BCON	0x0001		/* byte control */

/* Initialzation block (mode) */
#define	LE_MODE_PROM	0x8000		/* promiscuous mode */
/*			0x7f80		   reserved, must be zero */
#define	LE_MODE_INTL	0x0040		/* internal loopback */
#define	LE_MODE_DRTY	0x0020		/* disable retry */
#define	LE_MODE_COLL	0x0010		/* force a collision */
#define	LE_MODE_DTCR	0x0008		/* disable transmit CRC */
#define	LE_MODE_LOOP	0x0004		/* loopback mode */
#define	LE_MODE_DTX	0x0002		/* disable transmitter */
#define	LE_MODE_DRX	0x0001		/* disable receiver */
#define	LE_MODE_NORMAL	0		/* none of the above */

/* Receive message descriptor 1 (rmd1_bits) */ 
#define	LE_R1_OWN	0x80		/* LANCE owns the packet */
#define	LE_R1_ERR	0x40		/* error summary */
#define	LE_R1_FRAM	0x20		/* framing error */
#define	LE_R1_OFLO	0x10		/* overflow error */
#define	LE_R1_CRC	0x08		/* CRC error */
#define	LE_R1_BUFF	0x04		/* buffer error */
#define	LE_R1_STP	0x02		/* start of packet */
#define	LE_R1_ENP	0x01		/* end of packet */

#define	LE_R1_BITS \
    "\20\10OWN\7ERR\6FRAM\5OFLO\4CRC\3BUFF\2STP\1ENP"

/* Transmit message descriptor 1 (tmd1_bits) */ 
#define	LE_T1_OWN	0x80		/* LANCE owns the packet */
#define	LE_T1_ERR	0x40		/* error summary */
#define	LE_T1_MORE	0x10		/* multiple collisions */
#define	LE_T1_ONE	0x08		/* single collision */
#define	LE_T1_DEF	0x04		/* defferred transmit */
#define	LE_T1_STP	0x02		/* start of packet */
#define	LE_T1_ENP	0x01		/* end of packet */

#define	LE_T1_BITS \
    "\20\10OWN\7ERR\6RES\5MORE\4ONE\3DEF\2STP\1ENP"

/* Transmit message descriptor 3 (tmd3) */ 
#define	LE_T3_BUFF	0x8000		/* buffer error */
#define	LE_T3_UFLO	0x4000		/* underflow error */
#define	LE_T3_LCOL	0x1000		/* late collision */
#define	LE_T3_LCAR	0x0800		/* loss of carrier */
#define	LE_T3_RTRY	0x0400		/* retry error */
#define	LE_T3_TDR_MASK	0x03ff		/* time domain reflectometry counter */

#define	LE_XMD2_ONES	0xf000

#define	LE_T3_BITS \
    "\20\20BUFF\17UFLO\16RES\15LCOL\14LCAR\13RTRY"
