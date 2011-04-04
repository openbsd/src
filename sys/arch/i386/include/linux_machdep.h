/*	$OpenBSD: linux_machdep.h,v 1.10 2011/04/04 21:50:41 pirofti Exp $	*/
/*	$NetBSD: linux_machdep.h,v 1.5 1996/05/03 19:26:28 christos Exp $	*/

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
 */

#ifndef _LINUX_MACHDEP_H
#define _LINUX_MACHDEP_H

/*
 * The Linux sigcontext, pretty much a standard 386 trapframe.
 */

struct linux_sigcontext {
	int	sc_gs;
	int	sc_fs;
	int     sc_es;
	int     sc_ds;
	int     sc_edi;
	int     sc_esi;
	int     sc_ebp;
	int	sc_esp;
	int     sc_ebx;
	int     sc_edx;
	int     sc_ecx;
	int     sc_eax;
	int     sc_trapno;
	int     sc_err;
	int     sc_eip;
	int     sc_cs;
	int     sc_eflags;
	int     sc_esp_at_signal;
	int     sc_ss;
	int	sc_387;
	int	sc_mask;
	int	sc_cr2;
};

/*
 * We make the stack look like Linux expects it when calling a signal
 * handler, but use the BSD way of calling the handler and sigreturn().
 * This means that we need to pass the pointer to the handler too.
 * It is appended to the frame to not interfere with the rest of it.
 */

struct linux_sigframe {
	int	sf_sig;
	struct	linux_sigcontext sf_sc;
	sig_t	sf_handler;
};

#ifdef _KERNEL
void linux_sendsig(sig_t, int, int, u_long, int, union sigval);
dev_t linux_fakedev(dev_t);
#endif

/*
 * Major device numbers of VT device on both Linux and NetBSD. Used in
 * ugly patch to fake device numbers.
 */
#define LINUX_CONS_MAJOR   4
#define NATIVE_CONS_MAJOR 12

/*
 * Linux ioctl calls for the keyboard.
 */
#define LINUX_KDGKBMODE   0x4b44
#define LINUX_KDSKBMODE   0x4b45
#define LINUX_KDMKTONE    0x4b30
#define LINUX_KDSETMODE   0x4b3a
#define LINUX_KDGETMODE   0x4b3b
#define LINUX_KDENABIO    0x4b36
#define LINUX_KDDISABIO   0x4b37
#define LINUX_KDGETLED    0x4b31
#define LINUX_KDSETLED    0x4b32
#define	LINUX_KDGKBTYPE   0x4b33
#define	LINUX_KDGKBENT    0x4b46
#define	LINUX_KIOCSOUND   0x4b2f

/*
 * Mode for KDSKBMODE which we don't have (we just use plain mode for this)
 */
#define LINUX_K_MEDIUMRAW 2

/*
 * VT ioctl calls in Linux (the ones that pcvt can handle)
 */
#define LINUX_VT_OPENQRY    0x5600
#define LINUX_VT_GETMODE    0x5601
#define LINUX_VT_SETMODE    0x5602
#define LINUX_VT_GETSTATE   0x5603
#define LINUX_VT_RELDISP    0x5605
#define LINUX_VT_ACTIVATE   0x5606
#define LINUX_VT_WAITACTIVE 0x5607
#define LINUX_VT_DISALLOCATE 0x5608

struct l_segment_descriptor {
	unsigned int	entry_number;
	unsigned int	base_addr;
	unsigned int	limit;
	unsigned int	seg_32bit:1;
	unsigned int	contents:2;
	unsigned int	read_exec_only:1;
	unsigned int	limit_in_pages:1;
	unsigned int	seg_not_present:1;
	unsigned int	useable:1;
};

#endif /* _LINUX_MACHDEP_H */
