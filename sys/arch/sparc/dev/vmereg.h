/*	$OpenBSD: vmereg.h,v 1.1 1997/08/08 08:25:32 downsj Exp $	*/
/*	$NetBSD: vmereg.h,v 1.2 1997/06/07 19:10:57 pk Exp $ */

/*
 * Copyright (c) 1997 	Paul Kranenburg
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
 *	This product includes software developed by Paul Kranenburg.
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
 */

struct vmebusreg {
	u_int32_t	vmebus_cr;	/* VMEbus control register */
	u_int32_t	vmebus_afar;	/* VMEbus async fault address */
	u_int32_t	vmebus_afsr;	/* VMEbus async fault status */
};

/* Control Register bits */
#define VMEBUS_CR_C	0x80000000	/* I/O cache enable */
#define VMEBUS_CR_S	0x40000000	/* VME slave enable */
#define VMEBUS_CR_L	0x20000000	/* Loopback enable (diagnostic) */
#define VMEBUS_CR_R	0x10000000	/* VMEbus reset */
#define VMEBUS_CR_RSVD	0x0ffffff0	/* reserved */
#define VMEBUS_CR_IMPL	0x0000000f	/* VMEbus interface implementation */

/* Asynchronous Fault Status bits */
#define VMEBUS_AFSR_SZ	0xe0000000	/* Error transaction size */
#define    VMEBUS_AFSR_SZ4	0	/* 4 byte */
#define    VMEBUS_AFSR_SZ1	1	/* 1 byte */
#define    VMEBUS_AFSR_SZ2	2	/* 2 byte */
#define    VMEBUS_AFSR_SZ32	5	/* 32 byte */
#define VMEBUS_AFSR_TO	0x10000000	/* VME master access time-out */
#define VMEBUS_AFSR_BERR 0x08000000	/* VME master got BERR */
#define VMEBUS_AFSR_WB	0x04000000	/* IOC write-back error (if SZ == 32) */
					/* Non-IOC write error (id SZ != 32) */
#define VMEBUS_AFSR_ERR	0x02000000	/* Error summary bit */
#define VMEBUS_AFSR_S	0x01000000	/* MVME error in supervisor space */
#define VMEBUS_AFSR_ME	0x00800000	/* Multiple error */
#define VMEBUS_AFSR_RSVD 0x007fffff	/* reserved */

struct vmebusvec {
	volatile u_int8_t	vmebusvec[16];
};

/* VME address modifiers */
#define VMEMOD_A16_D_S	0x2d		/* 16-bit address, data, supervisor */
#define VMEMOD_A24_D_S	0x3d		/* 24-bit address, data, supervisor */
#define VMEMOD_A32_D_S	0x0d		/* 32-bit address, data, supervisor */

#define VMEMOD_D32	0x40		/* 32-bit access */

