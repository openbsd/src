/*	$OpenBSD: madreg.h,v 1.1 1998/04/26 21:02:37 provos Exp $	*/
/*	$NetBSD: madreg.h,v 1.4 1998/01/19 22:18:27 augustss Exp $	*/
/*
 * Copyright (c) 1996 Lennart Augustsson
 * Copyright (c) 1995 Hannu Savolainen
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/*
 * Variations of the suppored chips.
 */

#define MAD_NONE	0
#define MAD_82C928	1		/* OPTi 82C928     MAD16 */
#define MAD_OTI601D	2		/* OAK OTI-601D    Mozart */
#define MAD_82C929	3		/* OPTi 82C929     MAD16 Pro */
#define MAD_82C931	4		/* OPTi 82C831     */

/*
 *    Registers
 *
 *      The MAD16 occupies I/O ports 0xf8d to 0xf93 (fixed locations).
 *      All ports are inactive by default. They can be activated by
 *      writing 0xE2 or 0xE3 to the password register. The password is valid
 *      only until the next I/O read or write.
 */

#define MAD_BASE	0xf8d
#define MAD_NPORT	7

#define MC1_PORT	0	/* SB address, CDROM interface type, joystick */
#define MC2_PORT	1	/* CDROM address, IRQ, DMA, plus OPL4 bit */
#define MC3_PORT	2
#define MC_PASSWD_REG	MC3_PORT
#define MC4_PORT	3
#define MC5_PORT	4
#define MC6_PORT	5
#define MC7_PORT	6

#define MC1_NOCD	0x00
#define MC1_JOYDISABLE	0x01
#define MC1_SONY	0x02
#define MC1_MITSUMI	0x04
#define MC1_PANASONIC	0x06
#define MC1_SECIDE	0x08
#define MC1_PRIMIDE	0x0a

#define MC2_CDDISABLE	0x03
#define MC2_OPL4	0x20

/* Possible WSS emulation ports */
#define M_WSS_PORT0 0x530
#define M_WSS_PORT1 0xe80
#define M_WSS_PORT2 0xf40
#define M_WSS_PORT3 0x604
#define M_WSS_NPORTS 4

/* Port 1 */
#define M_WSS_PORT_SELECT(i) (0x80 | ((i) << 4))

#define M_PASSWD_928	0xe2
#define M_PASSWD_929	0xe3
#define M_PASSWD_931	0xe4

/* Regions of I/O space that the MAD occupies besides
   WSS emulation and MAD_BASE.  Talk about waste. */
#define MAD_REG1 0x220
#define MAD_LEN1 16
#define MAD_REG2 0x380
#define MAD_LEN2 2
#define MAD_REG3 0x388
#define MAD_LEN3 4
