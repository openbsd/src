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

#if __linux__
#ifdef _KERNEL
#define KERNEL 1
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/random.h>
#include <asm/uaccess.h>
#include <linux/malloc.h>

#define printf printk

/* XXX */
#define OSDEP_CRITICALDCL unsigned long flags;
#define OSDEP_CRITICALSTART save_flags(flags); cli()
#define OSDEP_CRITICALEND restore_flags(flags)

#define OSDEP_TIMESECONDS (xtime.tv_sec)
#define OSDEP_CURRENTPID (current->pid)
#define OSDEP_PCAST(x) ((unsigned int)(x) & 0xffffffff)
#define OSDEP_SOCKET struct sock
#define OSDEP_PACKET struct sk_buff
struct sk_buff;

#define OSDEP_REAL_MALLOC(n) kmalloc(n, GFP_ATOMIC)
#define OSDEP_REAL_FREE(p) kfree(p)

#define OSDEP_SOCKETFAMILY(socket) (socket->family)
#define OSDEP_SOCKETPRIVELEGED(socket) (suser())

static inline uint32_t __osdep_pseudorandom(void)
{
  static uint32_t seed=152;
  seed=seed*69069+1;
  return seed^jiffies;
};
#define OSDEP_PSEUDORANDOM __osdep_pseudorandom()

static inline int __osdep_datatopacket(void *data, int len, OSDEP_PACKET **packet)
{
  if (!(*packet = alloc_skb(len, GFP_ATOMIC)))
    return -ENOMEM;

  memcpy((*packet)->h.raw = skb_put(*packet, len), data, len);

  return 0;
};
#define OSDEP_DATATOPACKET(data, len, packet) __osdep_datatopacket(data, len, packet)
#define OSDEP_ZEROPACKET(packet) memset(packet->head, 0, packet->end - packet->head)
#define OSDEP_FREEPACKET(packet) kfree_skb(packet)

#define OSDEP_COPYFROMUSER(dst, src, len) \
        (copy_from_user(dst, src, len) ? -EFAULT : 0)

#define OSDEP_COPYTOUSER(dst, src, len) \
        (copy_to_user(dst, src, len) ? -EFAULT : 0)

#define __P(x) x
#else /* _KERNEL */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif /* _KERNEL */

#define OSDEP_SALEN 0
#define OSDEP_ERROR(x) (-(x))

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) \
        ((((uint32_t *)(a))[0] == 0) && (((uint32_t *)(a))[1] == 0) && \
         (((uint32_t *)(a))[2] == 0) && (((uint32_t *)(a))[3] == 0))
#endif /* IN6_IS_ADDR_UNSPECIFIED */

/* Stupid C trick: We can define the structures that are members of union
   sockaddr_union as empty and later redefine them as non-empty. We CAN'T,
   however, define them as non-empty and later redefine them as empty. So
   the empty declarations must be wrapped to ensure that we don't do that.

   WARNING: gcc < 2.8 generates incorrect debugging information for this;
   the symptom is that gdb thinks that all struct sockaddr_*'s are empty
   structures. gcc >= 2.8 correctly figures out what's going on. - cmetz
*/

#ifdef _KERNEL
#ifndef _NETINET_IN_H_
/* struct sockaddr_in {}; */
#endif /* _NETINET_IN_H_ */
#ifndef _NETINET6_IN6_H
/* struct sockaddr_in6 {}; */
#endif /* _NETINET6_IN6_H */
#ifndef _SYS_UN_H_
struct sockaddr_un {};
#endif /* _SYS_UN_H_ */

union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	struct sockaddr_un	sun;
	char __maxsize[128];		/* should probably be MHLEN on BSD */
};

static inline uint8_t __osdep_sa_len(struct sockaddr *sockaddr)
{
  switch(sockaddr->sa_family) {
    case AF_INET:
      return 16; /* sizeof(struct sockaddr_in); */
    case AF_INET6:
      return 24; /* sizeof(struct sockaddr_in6); */
    default:
      return 0;
  };
};
#define SA_LEN(sockaddr) __osdep_sa_len(sockaddr)
#endif /* KERNEL */
#endif /* __linux__ */

#if __NetBSD__ || __bsdi__ || __OpenBSD__ || __FreeBSD__
#define OSDEP_BSD 1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#ifdef _KERNEL
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

#ifdef _KERNEL
/* XXX */
#define OSDEP_CRITICALDCL int __s;
#define OSDEP_CRITICALSTART __s = splnet()
#define OSDEP_CRITICALEND splx(__s)

#ifdef __FreeBSD__
#define OSDEP_TIMESECONDS (time_second)
#else /* __FreeBSD__ */
#define OSDEP_TIMESECONDS (time.tv_sec)
#endif /* __FreeBSD__ */

#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
#define OSDEP_CURRENTPID (curproc->p_pid)
#else /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
#include <machine/pcpu.h>
#define OSDEP_CURRENTPID (PCPU(curproc)->p_pid)
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */

#ifdef SS_PRIV
#define OSDEP_SOCKETPRIVELEGED(socket) (socket->so_state & SS_PRIV)
#else /* SS_PRIV */
/* XXX? */
#define OSDEP_SOCKETPRIVELEGED(socket) (!curproc || !curproc->p_ucred || !curproc->p_ucred->cr_uid)
#endif /* SS_PRIV */
#define OSDEP_PCAST(x) ((unsigned int)(x) & 0xffffffff)
#define OSDEP_SOCKET struct socket
#define OSDEP_PACKET struct mbuf

#define OSDEP_REAL_MALLOC(n) malloc((unsigned long)(n), M_TEMP, M_DONTWAIT)
#define OSDEP_REAL_FREE(p) free((void *)p, M_TEMP)
#define OSDEP_FAMILY(socket) (socket->so_proto->pr_domain->dom_family)
#define OSDEP_PSEUDORANDOM (uint32_t)random()

#if !__FreeBSD__
struct ifnet;
struct mbuf *m_devget(char *, int, int, struct ifnet *, void (*)(const void *, void *, size_t));
#endif /* !__FreeBSD__ */

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
#define memset(p, zero, len) bzero(p, len) /* XXX */
#define memcmp(p1, p2, len) bcmp(p1, p2, len)

#define OSDEP_COPYFROMUSER(dst, src, len) copyin(src, dst, len)
#define OSDEP_COPYTOUSER(dst, src, len) copyout(src, dst, len)

#if __FreeBSD__
#define M_SOCKET M_TEMP
#define MT_SOOPTS MT_DATA
#endif /* __FreeBSD__ */
#endif /* KERNEL */
#define OSDEP_SALEN 1
#define OSDEP_ERROR(x) (x)
#endif /* __NetBSD__ || __bsdi__ || __OpenBSD__ || __FreeBSD__ */

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
