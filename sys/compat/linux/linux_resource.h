/*	$OpenBSD: linux_resource.h,v 1.2 2003/06/03 20:49:28 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Niklas Hallqvist
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

#ifndef _LINUX_RESOURCE_H_
#define _LINUX_RESOURCE_H_

/*
 * Resource limits
 */
#define	LINUX_RLIMIT_CPU	0	/* cpu time in milliseconds */
#define	LINUX_RLIMIT_FSIZE	1	/* maximum file size */
#define	LINUX_RLIMIT_DATA	2	/* data size */
#define	LINUX_RLIMIT_STACK	3	/* stack size */
#define	LINUX_RLIMIT_CORE	4	/* core file size */
#define	LINUX_RLIMIT_RSS	5	/* resident set size */
#define	LINUX_RLIMIT_NPROC	6	/* number of processes */
#define	LINUX_RLIMIT_NOFILE	7	/* number of open files */
#define	LINUX_RLIMIT_MEMLOCK	8	/* locked-in-memory address space */
#define	LINUX_RLIMIT_AS		9	/* address space limit */

#define	LINUX_RLIM_NLIMITS	10	/* number of resource limits */

#define LINUX_RLIM_INFINITY	0xFFFFFFFF
#define LINUX_OLD_RLIM_INFINITY	0x7FFFFFFF

struct linux_rlimit {
	u_long	rlim_cur;
	u_long	rlim_max;
};

#endif /* !_LINUX_RESOURCE_H_ */
