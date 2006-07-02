/*	$OpenBSD: linux_socketcall.h,v 1.1 2006/07/02 12:34:15 sturm Exp $	*/
/*	$NetBSD: linux_socketcall.h,v 1.1 1995/02/28 23:26:05 fvdl Exp $	*/

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

#ifndef _LINUX_SOCKETCALL_H
#define _LINUX_SOCKETCALL_H

/*
 * Values passed to the Linux socketcall() syscall, determining the actual
 * action to take.
 */
char *linux_socketcall_names[] = {
	NULL,
#define LINUX_SYS_socket	1
	"socket",
#define LINUX_SYS_bind		2
	"bind",
#define LINUX_SYS_connect	3
	"connect",
#define LINUX_SYS_listen	4
	"listen",
#define LINUX_SYS_accept	5
	"accept",
#define LINUX_SYS_getsockname	6
	"getsockname",
#define LINUX_SYS_getpeername	7
	"getpeername",
#define LINUX_SYS_socketpair	8
	"socketpair",
#define LINUX_SYS_send		9
	"send",
#define LINUX_SYS_recv		10
	"recv",
#define LINUX_SYS_sendto	11
	"sendto",
#define LINUX_SYS_recvfrom	12
	"recvfrom",
#define LINUX_SYS_shutdown	13
	"shutdown",
#define LINUX_SYS_setsockopt	14
	"setsockopt",
#define LINUX_SYS_getsockopt	15
	"getsockopt",
#define LINUX_SYS_sendmsg	16
	"sendmsg",
#define LINUX_SYS_recvmsg	17
	"recvmsg" };

/*
 * Structures for the arguments of the different system calls. This looks
 * a little better than copyin() of all values one by one.
 */
struct linux_socket_args {
	int domain;
	int type;
	int protocol;
};

struct linux_bind_args {
	int s;
	struct sockaddr *name;
	int namelen;
};

struct linux_connect_args {
	int s;
	struct sockaddr *name;
	int namelen;
};

struct linux_listen_args {
	int s;
	int backlog;
};

struct linux_accept_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

struct linux_getsockname_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

struct linux_getpeername_args {
	int s;
	struct sockaddr *addr;
	int *namelen;
};

struct linux_socketpair_args {
	int domain;
	int type;	
	int protocol;
	int *rsv;
};

struct linux_send_args {
	int s;
	void *msg;
	int len;
	int flags;
};

struct linux_recv_args {
	int s;
	void *msg;
	int len;
	int flags;
};

struct linux_sendto_args {
	int s;
	void *msg;
	int len;
	int flags;
	struct sockaddr *to;
	int tolen;
};

struct linux_recvfrom_args {
	int s;
	void *buf;
	int len;
	int flags;
	struct sockaddr *from;	
	int *fromlen;
};

struct linux_shutdown_args {
	int s;
	int how;
};

struct linux_getsockopt_args {
	int s;
	int level;
	int optname;
	void *optval;
	int *optlen;
};

struct linux_setsockopt_args {
	int s;
	int level;
	int optname;
	void *optval;
	int optlen;
};

struct linux_sendmsg_args {
	int s;
	struct msghdr *msg;
	int flags;
};

struct linux_recvmsg_args {
	int s;
	struct msghdr *msg;
	int flags;
};

#endif /* _LINUX_SOCKETCALL_H */
