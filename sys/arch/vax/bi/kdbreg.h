/*	$OpenBSD: kdbreg.h,v 1.5 2003/06/02 23:27:57 millert Exp $ */
/*	$NetBSD: kdbreg.h,v 1.3 1999/11/03 21:57:40 ragge Exp $	*/
/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
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
 *	@(#)kdbreg.h	7.3 (Berkeley) 6/28/90
 */

/*
 * The KDB50 registers are embedded inside the bi interface
 * general-purpose registers.
 */
#ifdef notdef
struct	kdb_regs {
	struct	biiregs kdb_bi;
	short	kdb_xxx;	/* first half of GPR 0 unused */
	short	kdb_ip;		/* initialisation and polling */
	short	kdb_sa;		/* status & address (r/o half) */
	short	kdb_sw;		/* status & address (w/o half) */
};
#endif

#define	KDB_IP	0xf2
#define	KDB_SA	0xf4
#define	KDB_SW	0xf6

#define KDBSR_BITS \
"\20\20ERR\17STEP4\16STEP3\15STEP2\14STEP1\13oldNV\12oldQB\11DI\10IE\1GO"

/*
 * Asserting KDB_MAP in values placed in mscp_seq.seq_buffer tells
 * the KDB to use mscp_seq.seq_mapbase as a set of PTEs and seq_buffer
 * as an offset value.  Hence we need no mappings; the KDB50 reads
 * the hardware page tables directly.  (Without KDB_MAP, seq_bufer
 * represents the physical memory address instead, and seq_mapbase is
 * unused.)
 */
#define	KDB_MAP		0x80000000
#define	KDB_PHYS	0		/* pseudo flag */
