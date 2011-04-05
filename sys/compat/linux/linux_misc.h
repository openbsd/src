/*	$OpenBSD: linux_misc.h,v 1.5 2011/04/05 22:54:30 pirofti Exp $	*/
/*	$NetBSD: linux_misc.h,v 1.3 1999/05/13 00:31:57 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Eric Haszlakiewicz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_MISC_H_
#define _LINUX_MISC_H_

/* This looks very unportable to me, but this is how Linux defines it. */
struct linux_sysinfo {
	long uptime;
	unsigned long loads[3];
#define LINUX_SYSINFO_LOADS_SCALE 65536
	unsigned long totalram;
	unsigned long freeram;
	unsigned long sharedram;
	unsigned long bufferram;
	unsigned long totalswap;
	unsigned long freeswap;
	unsigned short procs;
	unsigned long totalbig;
	unsigned long freebig;
	unsigned int mem_unit;
	char _f[20-2*sizeof(long)-sizeof(int)];
};

/*
 * Options passed to the Linux wait4() system call.
 */
#define	LINUX_WAIT4_WNOHANG	0x00000001
#define	LINUX_WAIT4_WUNTRACED	0x00000002
#define	LINUX_WAIT4_WCLONE	0x80000000

#ifdef _KERNEL
__BEGIN_DECLS
void bsd_to_linux_wstat(int *);
int linux_select1(struct proc *, register_t *, int, fd_set *, fd_set *,
    fd_set *, struct timeval *);
__END_DECLS
#endif /* !_KERNEL */

#endif /* !_LINUX_MISC_H_ */
