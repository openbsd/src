/*	$OpenBSD: linux_sched.h,v 1.2 2008/06/26 05:42:14 ray Exp $	*/
/*	$NetBSD: linux_sched.h,v 1.1 1999/05/12 19:49:09 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

#ifndef _LINUX_SCHED_H
#define	_LINUX_SCHED_H

/*
 * Flags passed to the Linux __clone(2) system call.
 */
#define	LINUX_CLONE_CSIGNAL	0x000000ff	/* signal to be sent at exit */
#define	LINUX_CLONE_VM		0x00000100	/* share address space */
#define	LINUX_CLONE_FS		0x00000200	/* share "file system" info */
#define	LINUX_CLONE_FILES	0x00000400	/* share file descriptors */
#define	LINUX_CLONE_SIGHAND	0x00000800	/* share signal actions */
#define	LINUX_CLONE_PID		0x00001000	/* share process ID */
#define	LINUX_CLONE_PTRACE	0x00002000	/* ptrace(2) continues on
						   child */
#define	LINUX_CLONE_VFORK	0x00004000	/* parent blocks until child
						   exits */

struct linux_sched_param {
	int	sched_priority;
};

#define LINUX_SCHED_OTHER	0
#define LINUX_SCHED_FIFO	1
#define LINUX_SCHED_RR		2

#endif /* _LINUX_SCHED_H */
