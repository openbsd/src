/* $OpenBSD: osdep.h,v 1.3 1999/12/10 08:53:18 angelos Exp $ */
/*
%%% copyright-nrl-97
This software is Copyright 1997-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

%%% copyright-cmetz-97
This software is Copyright 1997-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

*/
#ifndef __OSDEP_H
#define __OSDEP_H 1

#define OSDEP_BSD 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifdef KERNEL
#ifndef ATSH_ADD
#include <sys/systm.h>
#endif /* ATSH_ADD */
#ifndef MLEN
#include <sys/mbuf.h>
#endif /* MLEN */
#include <sys/kernel.h>
#include <sys/malloc.h>
#ifndef SB_MAX
#include <sys/socketvar.h>
#endif /* SB_MAX */
#include <sys/proc.h>
#ifndef RTM_RTTUNIT
#include <net/route.h>
#endif /* RTM_RTTUNIT */
#endif /* KERNEL */
struct ifnet;
struct mbuf;
#include <netinet/in.h>
struct route6;

#ifdef KERNEL
/* XXX */
#define OSDEP_CRITICALDCL int __s;
#define OSDEP_CRITICALSTART __s = splnet()
#define OSDEP_CRITICALEND splx(__s)
#define OSDEP_TIMESECONDS (time.tv_sec)
#define OSDEP_PROCESS struct proc
#define OSDEP_PROCESSCURRENT (curproc)
#define OSDEP_PROCESSPARENT(x) ((x)->p_pptr)
#define OSDEP_PROCESSPID(x) ((x)->p_pid)

#ifdef SS_PRIV
#define OSDEP_SOCKETPRIVELEGED(socket) (socket->so_state & SS_PRIV)
#else /* SS_PRIV */
/* XXX? */
#define OSDEP_SOCKETPRIVELEGED(socket) (!curproc || !curproc->p_ucred || !curproc->p_ucred->cr_uid)
#endif /* SS_PRIV */
#define OSDEP_PCAST(x) ((unsigned int)(x) & 0xffffffff)
#define OSDEP_SOCKET struct socket
#define OSDEP_PACKET struct mbuf
struct mbuf;

#define OSDEP_REAL_MALLOC(n) malloc((unsigned long)(n), M_TEMP, M_DONTWAIT)
#define OSDEP_REAL_FREE(p) free((void *)p, M_TEMP)
#define OSDEP_FAMILY(socket) (socket->so_proto->pr_domain->dom_family)
#define OSDEP_PSEUDORANDOM (uint32_t)random()

struct ifnet;
struct mbuf *m_devget(char *, int, int, struct ifnet *, void (*)(const void *, void *, size_t));

static __inline__ int __osdep_datatopacket(void *data, int len, OSDEP_PACKET **packet)
{
  if (!(*packet = m_devget(data, len, 0, NULL, NULL)))
    return -ENOMEM;

  return 0;
};

#define OSDEP_DATATOPACKET(data, len, packet) __osdep_datatopacket(data, len, packet)

#define OSDEP_ZEROPACKET(packet) m_zero(packet)
#define OSDEP_FREEPACKET(packet) m_freem(packet)

#define memcpy(dst, src, len) bcopy(src, dst, len)
#define memmove(dst, src, len) bcopy(src, dst, len)
#define memset(p, zero, len) bzero(p, len) /* XXX */
#define memcmp(p1, p2, len) bcmp(p1, p2, len)

#define OSDEP_COPYFROMUSER(dst, src, len) copyin(src, dst, len)
#define OSDEP_COPYTOUSER(dst, src, len) copyout(src, dst, len)

#endif /* KERNEL */
#define OSDEP_SALEN 1
#define OSDEP_ERROR(x) (x)

#define ENETSECURITYPOLICY -ECOMM

#ifdef DEBUG_MALLOC
#undef OSDEP_MALLOC
#undef OSDEP_FREE
#define OSDEP_MALLOC(n) debug_malloc_malloc(n, DEBUG_STATUS)
#define OSDEP_FREE(p) debug_malloc_free(p)
#else /* DEBUG_MALLOC */
#undef OSDEP_MALLOC
#define OSDEP_MALLOC(n) OSDEP_REAL_MALLOC(n)
#undef OSDEP_FREE
#define OSDEP_FREE(p) OSDEP_REAL_FREE(p)
#endif /* DEBUG_MALLOC */

#endif /* __OSDEP_H */
