/*	$NetBSD: loader.h,v 1.6 1996/01/07 22:06:18 leo Exp $	*/

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
 * NetBSD loader for the Atari-TT.
 *
 * Assume compiling under TOS or MINT. The page-size will always
 * be incorrect then (if it is defined anyway).
 */
#ifdef __LDPGSZ
#undef __LDPGSZ
#endif

#define __LDPGSZ	(8*1024)	/* Page size for NetBSD		*/

#ifndef N_MAGIC
#define	N_MAGIC(hdr)	(hdr.a_magic & 0xffff)
#endif

#define	TTRAM_BASE	0x1000000	/* Fastram always starts here	*/

/*
 * System var's used in low-memory
 */
#define	ADDR_RAMTOP	((long*)0x5a4)	/* End of TT-ram (unofficial)	*/
#define	ADDR_CHKRAMTOP	((long*)0x5a8)	/*   above is valid (unofficial)*/
#define	ADDR_PHYSTOP	((long*)0x42e)	/* End of ST-ram		*/
#define	ADDR_P_COOKIE	((long**)0x5a0)	/* Pointer to cookie jar	*/
#define	ADDR_OSHEAD	((OSH**)0x4f2)	/* Pointer Os-header		*/

#define	RAM_TOP_MAGIC	(0x1357bd13)	/* Magic nr. for ADDR_CHKRAMTOP	*/

/*
 * Sufficient but incomplete definition os Os-header
 */
typedef struct osh {
	unsigned short	os_entry;
	unsigned short	os_version;
	void		*reseth;
	struct osh	*os_beg;
} OSH;
