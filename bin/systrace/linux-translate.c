/*
 * Copyright 2002 Marius Aamodt Eriksen <marius@umich.edu>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pwd.h>
#include <err.h>
#include <netdb.h>

#include <compat/linux/linux_socket.h>
#include <compat/linux/linux_types.h>
#include <compat/linux/linux_fcntl.h>
#include "linux_socketcall.h"

#include "intercept.h"
#include "systrace.h"

extern struct intercept_system intercept;

/* XXX register_t */
#define ARGSIZE(n) ((n) * sizeof(unsigned long))
static unsigned char socketcall_argsize[18] = {
	ARGSIZE(0),		/* none */
	ARGSIZE(3),		/* LINUX_SYS_socket */
	ARGSIZE(3),		/* LINUX_SYS_bind */
	ARGSIZE(3),		/* LINUX_SYS_connect */
	ARGSIZE(2),		/* LINUX_SYS_listen */
	ARGSIZE(3),		/* LINUX_SYS_accept */
	ARGSIZE(3),		/* LINUX_SYS_getsockname */
	ARGSIZE(3),		/* LINUX_SYS_getpeername */
	ARGSIZE(4),		/* LINUX_SYS_socketpair */
	ARGSIZE(4),		/* LINUX_SYS_send */
	ARGSIZE(4),		/* LINUX_SYS_recv */
	ARGSIZE(6),		/* LINUX_SYS_sendto */
	ARGSIZE(6),		/* LINUX_SYS_recvfrom */
	ARGSIZE(2),		/* LINUX_SYS_shutdown */
	ARGSIZE(5),		/* LINUX_SYS_setsockopt */
	ARGSIZE(5),		/* LINUX_SYS_getsockopt */
	ARGSIZE(3),		/* LINUX_SYS_sendmsg */
	ARGSIZE(3)		/* LINUX_SYS_recvmsg */
};

/* ARGSUSED */
static int
get_socketcall(struct intercept_translate *trans, int fd, pid_t pid, void *addr)
{
	int call = (intptr_t)addr;

	systrace_switch_alias("linux", "socketcall", "linux",
	    linux_socketcall_names[call]);

	/* We don't want to print the argument .. */
	trans->trans_valid = 0;
	return (0);
}

/* ARGSUSED */
static int
print_socketcall(char *buf, size_t buflen, struct intercept_translate *tl)
{
	return (0);
}

static int
get_socketcall_args(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	int call = (intptr_t)trans->trans_addr2;
	unsigned long argsize;

	if (call != (intptr_t)trans->user) {
		trans->trans_valid = 0;
		return (0);
	}

	argsize = socketcall_argsize[call];

	if ((trans->trans_data = malloc(argsize)) == NULL)
		return (-1);

	if (intercept.io(fd, pid, INTERCEPT_READ, addr,
		trans->trans_data, argsize) == -1) {
		free(trans->trans_data);
		return (-1);
	}

	return (0);
}

static int
print_socktype(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char *what = NULL;
	unsigned long *args = tl->trans_data;
	int type = args[1];

	switch (type) {
	case SOCK_STREAM:
		what = "SOCK_STREAM";
		break;
	case SOCK_DGRAM:
		what = "SOCK_DGRAM";
		break;
	case SOCK_RAW:
		what = "SOCK_RAW";
		break;
	case SOCK_SEQPACKET:
		what = "SOCK_SEQPACKET";
		break;
	case SOCK_RDM:
		what = "SOCK_RDM";
		break;
	default:
		snprintf(buf, buflen, "SOCK_UNKNOWN(%d)", type);
		break;
	}

	if (what != NULL)
		strlcpy(buf, what, buflen);

	return (0);
}

static int
print_sockdom(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char *what = NULL;
	unsigned long *args = tl->trans_data;
	int domain = args[0];

	switch (domain) {
	case LINUX_AF_UNIX:
		what = "AF_UNIX";
		break;
	case LINUX_AF_INET:
		what = "AF_INET";
		break;
	case LINUX_AF_INET6:
		what = "AF_INET6";
		break;
	default:
		snprintf(buf, buflen, "AF_UNKNOWN(%d)", domain);
		break;
	}

	if (what != NULL)
		strlcpy(buf, what, buflen);

	return (0);
}

static int
get_sockaddr(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	struct sockaddr_storage sa;
	socklen_t len;
	void *sockaddr_addr;
	unsigned long *args;
	int call = (intptr_t)trans->trans_addr2;

	if (get_socketcall_args(trans, fd, pid, addr) == -1)
		return (-1);

	if (trans->trans_valid == 0)
		return (0);

	args = trans->trans_data;

	len = call == LINUX_SYS_sendto ? args[5] : args[2];
	sockaddr_addr = (void *)(call == LINUX_SYS_sendto ? args[4] : args[1]);

	if (len == 0 || len > sizeof(struct sockaddr_storage))
		return (-1);

	if (intercept.io(fd, pid, INTERCEPT_READ, sockaddr_addr,
	    (void *)&sa, len) == -1)
		return (-1);

	free(trans->trans_data);
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);
	trans->trans_size = len;
	memcpy(trans->trans_data, &sa, len);

	return (0);
}

#ifndef offsetof
#define offsetof(s, e)  ((size_t)&((s *)0)->e)
#endif

static int
print_sockaddr(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
	struct linux_sockaddr *linux_sa = tl->trans_data;
	struct sockaddr sa;
	socklen_t len = (socklen_t)tl->trans_size;

	/* XXX - Niels */
	tl->trans_size = 0;

	buf[0] = '\0';

	switch (linux_sa->sa_family) {
	case LINUX_AF_UNIX:
		if (len <= offsetof(struct linux_sockaddr, sa_data))
			return (-1);
		len -= offsetof(struct linux_sockaddr, sa_data);
		if (buflen < len + 1)
			len = buflen - 1;
		memcpy(buf, linux_sa->sa_data, len);
		buf[len] = '\0';
		return (0);
	case LINUX_AF_INET:
	case LINUX_AF_INET6:
		break;
	default:
		snprintf(buf, buflen, "family(%d)", linux_sa->sa_family);
		return (0);
	}

	memcpy(&sa.sa_family, &linux_sa->sa_family, sizeof(sa.sa_family));
	memcpy(&sa.sa_data, &linux_sa->sa_data, sizeof(sa.sa_data));
#ifdef HAVE_SOCKADDR_SA_LEN
	sa.sa_len = len;
#endif /* HAVE_SOCKADDR_SA_LEN */
	if (getnameinfo(&sa, len,
		host, sizeof(host), serv, sizeof(serv),
		NI_NUMERICHOST | NI_NUMERICSERV)) {
		warn("getnameinfo");
		return (-1);
	}

	snprintf(buf, buflen, "inet-[%s]:%s", host, serv);

	return (0);
}

static int
get_msghdr(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
 	struct msghdr msg;
	int len = sizeof(struct msghdr);
	unsigned long *args;

	if (get_socketcall_args(trans, fd, pid, addr) == -1)
		return (-1);

	if (trans->trans_valid == 0)
		return (0);

	args = trans->trans_data;
	if (intercept.io(fd, pid, INTERCEPT_READ, (void *)args[1],
		(void *)&msg, len) == -1)
		return (-1);

	if (msg.msg_name == NULL) {
		trans->trans_data = NULL;
		trans->trans_size = 0;
		return (0);
	}

	trans->trans_size = msg.msg_namelen;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);
	if (intercept.io(fd, pid, INTERCEPT_READ, msg.msg_name,
		(void *)trans->trans_data, trans->trans_size) == -1)
		return (-1);
	
	return (0);
}

static int
print_msghdr(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int res = 0;
	if (tl->trans_size == 0) {
		snprintf(buf, buflen, "<unknown>");
	} else {
		res = print_sockaddr(buf, buflen, tl);
		/*
		 * disable replacement of this argument because it's two levels
		 * deep and we cant replace that far.
		 */
		tl->trans_size = 0;
		
		/* TODO: make this less of a hack */
	}

	return (res);
}

struct intercept_translate ic_linux_socket_sockdom = {
	"sockdom",
	get_socketcall_args, print_sockdom,
	-1,
	.user = (void *)LINUX_SYS_socket
};

struct intercept_translate ic_linux_socket_socktype = {
	"socktype",
	get_socketcall_args, print_socktype,
	-1,
	.user = (void *)LINUX_SYS_socket
};

struct intercept_translate ic_linux_connect_sockaddr = {
	"sockaddr",
	get_sockaddr, print_sockaddr,
	-1,
	.user = (void *)LINUX_SYS_connect
};

struct intercept_translate ic_linux_bind_sockaddr = {
	"sockaddr",
	get_sockaddr, print_sockaddr,
	-1,
	.user = (void *)LINUX_SYS_bind
};

struct intercept_translate ic_linux_sendto_sockaddr = {
	"sockaddr",
	get_sockaddr, print_sockaddr,
	-1,
	.user = (void *)LINUX_SYS_sendto
};

struct intercept_translate ic_linux_sendmsg_sockaddr = {
	"sockaddr",
	get_msghdr, print_msghdr,
	-1,
	.user = (void *)LINUX_SYS_sendmsg
};

struct intercept_translate ic_linux_socketcall_catchall = {
	"call",
	get_socketcall, print_socketcall,
};
