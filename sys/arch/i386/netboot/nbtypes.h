/*	$NetBSD: nbtypes.h,v 1.4 1994/10/27 04:21:19 cgd Exp $	*/

/*
 * source in this file came from
 * various 386BSD system header files.
 * The intent is to render these sources compilable from environments
 * other than native 386bsd.
 *
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
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
 */

#ifndef __nbtypes_h_
#define __nbtypes_h_

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

#define _JBLEN	10
typedef int jmp_buf[_JBLEN];

struct exec {
	 long	a_midmag;	/* magic number */
unsigned long	a_text;		/* text segment size */
unsigned long	a_data;		/* initialized data size */
unsigned long	a_bss;		/* uninitialized data size */
unsigned long	a_syms;		/* symbol table size */
unsigned long	a_entry;	/* entry point */
unsigned long	a_trsize;	/* text relocation size */
unsigned long	a_drsize;	/* data relocation size */
};

#define N_GETMAGIC(ex) \
            ( (((ex).a_midmag)&0xffff0000) ? (ntohl(((ex).a_midmag))&0xffff) : ((ex).a_midmag))

#define ZMAGIC          0413    /* demand load format */

#if 0
typedef char *va_list;
#define	__va_promote(type) \
	(((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))
#define	va_start(ap, last) \
	(ap = ((char *)&(last) + __va_promote(last)))
#define	va_arg(ap, type) \
	((type *)(ap += sizeof(type) < sizeof(int) ? \
		(abort(), 0) : sizeof(type)))[-1]
#define	va_end(ap)
#else
typedef char *va_list;
#define va_start(ap, parmN) ((ap) = (char *)(&parmN + 1))
#define va_arg(ap, type) ((ap) += sizeof(type), ((type *)(ap))[-1])
#define va_end(ap)
#endif

#define	RB_ASKNAME	0x01	/* ask for file name to reboot from */
#define	RB_SINGLE	0x02	/* reboot to single user only */
#define	RB_NOSYNC	0x04	/* dont sync before reboot */
#define	RB_HALT		0x08	/* don't reboot, just halt */
#define	RB_INITNAME	0x10	/* name given for /etc/init (unused) */
#define	RB_DFLTROOT	0x20	/* use compiled-in rootdev */
#define	RB_KDB		0x40	/* give control to kernel debugger */
#define	RB_RDONLY	0x80	/* mount root fs read-only */
#define	RB_DUMP		0x100	/* dump kernel memory before reboot */

#endif /* __types_h_ */
