/*	$OpenBSD: linux_ipc.h,v 1.3 2000/07/23 22:35:38 jasoni Exp $	*/
/*	$NetBSD: linux_ipc.h,v 1.1 1995/02/28 23:25:47 fvdl Exp $	*/

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

#ifndef _LINUX_IPC_H
#define _LINUX_IPC_H

/*
 * Structs and values to handle the SYSV ipc/shm/msg calls implemented
 * in Linux. Most values match the OpenBSD values (as they are both derived
 * from SysV values). Values that are the same may not be defined here.
 */

typedef int linux_key_t;

/*
 * The only thing different about the Linux ipc_perm structure is the
 * order of the fields.
 */
struct linux_ipc_perm {
	linux_key_t	l_key;
	ushort		l_uid;
	ushort		l_gid;
	ushort		l_cuid;
	ushort		l_cgid;
	ushort		l_mode;
	ushort		l_seq;
};

#define LINUX_IPC_RMID	0
#define LINUX_IPC_SET	1
#define LINUX_IPC_STAT	2
#define LINUX_IPC_INFO	3

#endif /* _LINUX_IPC_H */
