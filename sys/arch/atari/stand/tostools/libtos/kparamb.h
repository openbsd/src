/*	$NetBSD: kparamb.h,v 1.1.1.1 1996/01/07 21:50:49 leo Exp $	*/

/*
 * Copyright (c) 1995 L. Weppelman
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

/*
 * Structure passed to bsd_startup().
 */
struct kparamb {
	u_char	*kp;		/* 00: Kernel load address		*/
	long	ksize;		/* 04: Size of loaded kernel		*/
	u_long	entry;		/* 08: Kernel entry point		*/
	long	stmem_size;	/* 12: Size of st-ram			*/
	long	ttmem_size;	/* 16: Size of tt-ram			*/
	long	bootflags;	/* 20: Various boot flags		*/
	long	boothowto;	/* 24: How to boot			*/
	long	ttmem_start;	/* 28: Start of tt-ram			*/
	long	esym_loc;	/* 32: End of symbol table		*/
};

#ifndef	STANDALONE
/*
 * Values for 'bootflags'.
 * Note: These should match with the values NetBSD uses!
 */
#define	ATARI_68000	1		/* 68000 CPU			*/
#define	ATARI_68010	(1<<1)		/* 68010 CPU			*/
#define	ATARI_68020	(1<<2)		/* 68020 CPU			*/
#define	ATARI_68030	(1<<3)		/* 68030 CPU			*/
#define	ATARI_68040	(1<<4)		/* 68040 CPU			*/
#define	ATARI_TT	(1L<<11)	/* This is a TT030		*/
#define	ATARI_FALCON	(1L<<12)	/* This is a Falcon		*/

#define	ATARI_CLKBROKEN	(1<<16)		/* GEMDOS has faulty year base	*/

#define	ATARI_ANYCPU	(0x1f)

/*
 * Definitions for boothowto
 * Note: These should match with the values NetBSD uses!
 */
#define	RB_AUTOBOOT	0x00
#define	RB_ASKNAME	0x01
#define	RB_SINGLE	0x02
#define	RB_KDB		0x40

#endif	/* STANDALONE */
