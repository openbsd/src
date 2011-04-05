/*	$OpenBSD: linux_exec.h,v 1.7 2011/04/05 22:54:30 pirofti Exp $	*/
/*	$NetBSD: linux_exec.h,v 1.5 1995/10/07 06:27:01 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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
 *
 */

#ifndef	_LINUX_EXEC_H_
#define	_LINUX_EXEC_H_

#define LINUX_M_I386	100
/* Sparc? Alpha? */

/* XXX linux_machdep.h ? */
#ifdef __i386__
#define LINUX_MID_MACHINE LINUX_M_I386
#endif

#define LINUX_AOUT_HDR_SIZE (sizeof (struct exec))

#define LINUX_N_MAGIC(ep)    ((ep)->a_midmag & 0xffff)
#define LINUX_N_MACHTYPE(ep) (((ep)->a_midmag >> 16) & 0xff)

#define LINUX_N_TXTOFF(x,m) \
 ((m) == ZMAGIC ? 1024 : ((m) == QMAGIC ? 0 : sizeof (struct exec)))

#define LINUX_N_DATOFF(x,m) (LINUX_N_TXTOFF(x,m) + (x).a_text)

#define LINUX_N_TXTADDR(x,m) ((m) == QMAGIC ? PAGE_SIZE : 0)

#define LINUX__N_SEGMENT_ROUND(x) (((x) + NBPG - 1) & ~(NBPG - 1))

#define LINUX__N_TXTENDADDR(x,m) (LINUX_N_TXTADDR(x,m)+(x).a_text)

#define LINUX_N_DATADDR(x,m) \
    ((m)==OMAGIC? (LINUX__N_TXTENDADDR(x,m)) \
     : (LINUX__N_SEGMENT_ROUND (LINUX__N_TXTENDADDR(x,m))))

#define LINUX_N_BSSADDR(x,m) (LINUX_N_DATADDR(x,m) + (x).a_data)

int exec_linux_aout_makecmds(struct proc *, struct exec_package *);
int exec_linux_elf32_makecmds(struct proc *, struct exec_package *);

int linux_elf_probe(struct proc *, struct exec_package *, char *,
    u_long *, u_int8_t *);

#endif /* !_LINUX_EXEC_H_ */
