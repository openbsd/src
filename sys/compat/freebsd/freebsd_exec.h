/*	$OpenBSD: freebsd_exec.h,v 1.3 1999/02/10 08:07:19 deraadt Exp $	*/

/*-
 * Copyright (c) 1992, 1993
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
 *	from: @(#)exec.h	8.1 (Berkeley) 6/11/93
 *	from: imgact_aout.h,v 1.2 1994/12/30 08:06:19 bde Exp
 */

#ifndef	_FREEBSD_EXEC_H
#define	_FREEBSD_EXEC_H

#define FREEBSD_N_GETMAGIC(ex) \
	( (ex).a_midmag & 0xffff )
#define FREEBSD_N_GETMID(ex) \
	( ((ex).a_midmag >> 16) & 0x03ff )
#define FREEBSD_N_GETFLAG(ex) \
	( ((ex).a_midmag >> 26) & 0x3f )
#define FREEBSD_N_SETMAGIC(ex,mag,mid,flag) \
	( (ex).a_midmag = (((flag) & 0x3f) <<26) | (((mid) & 0x03ff) << 16) | \
	((mag) & 0xffff) )

#define FREEBSD_N_ALIGN(ex,x) \
	(FREEBSD_N_GETMAGIC(ex) == ZMAGIC || \
	 FREEBSD_N_GETMAGIC(ex) == QMAGIC ? \
	 ((x) + FREEBSD___LDPGSZ - 1) & \
	 ~(unsigned long)(FREEBSD___LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	FREEBSD_N_BADMAG(ex) \
	(FREEBSD_N_GETMAGIC(ex) != OMAGIC && \
	 FREEBSD_N_GETMAGIC(ex) != NMAGIC && \
	 FREEBSD_N_GETMAGIC(ex) != ZMAGIC && \
	 FREEBSD_N_GETMAGIC(ex) != QMAGIC)

/* Address of the bottom of the text segment. */
#define FREEBSD_N_TXTADDR(ex) \
	((FREEBSD_N_GETMAGIC(ex) == OMAGIC || \
	  FREEBSD_N_GETMAGIC(ex) == NMAGIC || \
	  FREEBSD_N_GETMAGIC(ex) == ZMAGIC) ? 0 : __LDPGSZ)

/* Address of the bottom of the data segment. */
#define FREEBSD_N_DATADDR(ex) \
	FREEBSD_N_ALIGN(ex, FREEBSD_N_TXTADDR(ex) + (ex).a_text)

/* Text segment offset. */
#define	FREEBSD_N_TXTOFF(ex) \
	(FREEBSD_N_GETMAGIC(ex) == ZMAGIC ? __LDPGSZ : \
	 FREEBSD_N_GETMAGIC(ex) == QMAGIC ? 0 : sizeof(struct exec)) 

/* Data segment offset. */
#define	FREEBSD_N_DATOFF(ex) \
	FREEBSD_N_ALIGN(ex, FREEBSD_N_TXTOFF(ex) + (ex).a_text)

/* Relocation table offset. */
#define FREEBSD_N_RELOFF(ex) \
	FREEBSD_N_ALIGN(ex, FREEBSD_N_DATOFF(ex) + (ex).a_data)

/* Symbol table offset. */
#define FREEBSD_N_SYMOFF(ex) \
	(FREEBSD_N_RELOFF(ex) + (ex).a_trsize + (ex).a_drsize)

/* String table offset. */
#define	FREEBSD_N_STROFF(ex) 	(FREEBSD_N_SYMOFF(ex) + (ex).a_syms)

#define	FREEBSD_AOUT_HDR_SIZE	sizeof(struct exec)

int exec_freebsd_aout_makecmds __P((struct proc *, struct exec_package *));
int freebsd_elf_probe __P((struct proc *, struct exec_package *, char *,
    u_long *, u_int8_t *));

extern char freebsd_sigcode[], freebsd_esigcode[];

#endif /* !_FREEBSD_EXEC_H */
