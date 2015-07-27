/*	$OpenBSD: tame.h,v 1.2 2015/07/27 17:48:06 deraadt Exp $	*/

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 * Copyright (c) 2015 Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _SYS_TAME_H_
#define _SYS_TAME_H_

#include <sys/cdefs.h>

#define _TM_SELF	0x00000001	/* operate on own pid */
#define _TM_RW		0x00000002	/* basic io operations */
#define _TM_MALLOC	0x00000004	/* enough for malloc */
#define _TM_DNSPATH	0x00000008	/* access to DNS pathnames */
#define _TM_RPATH	0x00000010	/* allow open for read */
#define _TM_WPATH	0x00000020	/* allow open for write */
#define _TM_TMPPATH	0x00000040	/* for mk*temp() */
#define _TM_INET	0x00000080	/* AF_INET/AF_INET6 sockets */
#define _TM_UNIX	0x00000100	/* AF_UNIX sockets */
#define _TM_CMSG	0x00000200	/* AF_UNIX CMSG fd passing */
#define _TM_IOCTL	0x00000400	/* scary */
#define _TM_GETPW	0x00000800	/* enough to enable YP */
#define _TM_PROC	0x00001000	/* fork, waitpid, etc */
#define _TM_CPATH	0x00002000	/* allow create, mkdir, or inode mods */

#define _TM_ABORT	0x08000000	/* SIGABRT instead of SIGKILL */

/* Following flags are set by kernel, as it learns things.
 * Not user settable. Should be moved to a seperate variable */
#define _TM_USERSET	0x0fffffff
#define _TM_YP_ACTIVE	0x10000000	/* YP use detected and allowed */
#define _TM_DNS_ACTIVE	0x20000000	/* DNS use detected and allowed */

#define TAME_MALLOC	(_TM_SELF | _TM_MALLOC)
#define TAME_RW		(_TM_SELF | _TM_RW)
#define TAME_STDIO	(_TM_SELF | _TM_MALLOC | _TM_RW)
#define TAME_RPATH	(_TM_SELF | _TM_RW | _TM_RPATH)
#define TAME_WPATH	(_TM_SELF | _TM_RW | _TM_WPATH)
#define TAME_TMPPATH	(_TM_SELF | _TM_RW | _TM_TMPPATH)
#define TAME_INET	(_TM_SELF | _TM_RW | _TM_INET)
#define TAME_UNIX	(_TM_SELF | _TM_RW | _TM_UNIX)
#define TAME_CMSG	(TAME_UNIX | _TM_CMSG)
#define TAME_DNS	(TAME_MALLOC | _TM_DNSPATH)
#define TAME_IOCTL	(_TM_IOCTL)
#define TAME_GETPW	(TAME_STDIO | _TM_GETPW)
#define TAME_PROC	(_TM_PROC)
#define TAME_CPATH	(_TM_CPATH)
#define TAME_ABORT	(_TM_ABORT)

#ifdef _KERNEL

int	tame_check(struct proc *, int);
int	tame_fail(struct proc *, int, int);
int	tame_namei(struct proc *, char *);
void	tame_aftersyscall(struct proc *, int, int);

int	tame_cmsg_send(struct proc *p, void *v, int controllen);
int	tame_cmsg_recv(struct proc *p, void *v, int controllen);
int	tame_sysctl_check(struct proc *p, int namelen, int *name, void *new);
int	tame_adjtime_check(struct proc *p, const void *v);
int	tame_recvfrom_check(struct proc *p, void *from);
int	tame_sendto_check(struct proc *p, const void *to);
int	tame_bind_check(struct proc *p, const void *v);
int	tame_connect_check(struct proc *p);
int	tame_socket_check(struct proc *p, int domain);
int	tame_setsockopt_check(struct proc *p, int level, int optname);
int	tame_dns_check(struct proc *p, in_port_t port);
int	tame_ioctl_check(struct proc *p, long com, void *);

#else /* _KERNEL */

int	tame(int);

#endif /* _KERNEL */

#endif /* _SYS_TAME_H_ */
