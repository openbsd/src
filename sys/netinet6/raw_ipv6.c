/*
%%% copyright-nrl-95
This software is Copyright 1995-1998 by Randall Atkinson, Ronald Lee,
Daniel McDonald, Bao Phan, and Chris Winters. All Rights Reserved. All
rights under this copyright have been assigned to the US Naval Research
Laboratory (NRL). The NRL Copyright Notice and License Agreement Version
1.1 (January 17, 1995) applies to this software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/
/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)raw_ip.c	8.7 (Berkeley) 5/15/95
 *	$Id: raw_ipv6.c,v 1.3 1999/03/09 05:31:55 cmetz Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#if __NetBSD__ || __FreeBSD__
#include <sys/proc.h>
#endif /* __NetBSD__ || __FreeBSD__ */
#if __FreeBSD__
#include <vm/vm_zone.h>
#endif /* __FreeBSD__ */

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_mroute.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/icmpv6.h>

#if __OpenBSD__
#undef IPSEC
#ifdef NRL_IPSEC 
#define IPSEC 1
#endif /* NRL_IPSEC */
#endif /* __OpenBSD__ */

#ifdef IPSEC
#include <sys/osdep.h>
#include <net/netproc.h>
#include <net/netproc_var.h>
#endif /* IPSEC */

#ifdef DEBUG_NRL
#include <sys/debug.h>
#else /* DEBUG_NRL */
#if __OpenBSD__
#include <netinet6/debug.h>
#else /* __OpenBSD__ */
#include <sys/debug.h>
#endif /* __OpenBSD__ */
#endif /* DEBUG_NRL */

/*
 * Globals
 */

#if __NetBSD__ || __OpenBSD__
struct inpcbtable rawin6pcbtable;
#else /* __NetBSD__ || __OpenBSD__ */
struct inpcb rawin6pcb;
#endif /* __NetBSD__ || __OpenBSD__ */
struct sockaddr_in6 ripv6src = { sizeof(struct sockaddr_in6), AF_INET6 };

/*
 * Nominal space allocated to a raw ip socket.
 */

#define	RIPV6SNDQ		8192
#define	RIPV6RCVQ		8192
u_long ripv6_sendspace = RIPV6SNDQ;
u_long ripv6_recvspace = RIPV6RCVQ;

/*
 * External globals
 */
#if __FreeBSD__
static struct inpcbhead ri6pcb;
static struct inpcbinfo ri6pcbinfo;
#endif /* __FreeBSD__ */

extern struct ipv6stat ipv6stat;

#define RETURN_ERROR(x) { \
  DPRINTF(EVENT, ("%s: returning %s\n", DEBUG_STATUS, #x)); \
  return x; \
}
#define RETURN_VALUE(x) { \
  DPRINTF(EVENT, ("%s: returning %d\n", DEBUG_STATUS, x)); \
  return x; \
}

/*----------------------------------------------------------------------
 * Raw IPv6 PCB initialization.
 ----------------------------------------------------------------------*/

void
ripv6_init()
{
#if __FreeBSD__
	LIST_INIT(&ri6pcb);
	ri6pcbinfo.listhead = &ri6pcb;
	/*
	 * XXX We don't use the hash list for raw IP, but it's easier
	 * to allocate a one entry hash list than it is to check all
	 * over the place for hashbase == NULL.
	 */
	ri6pcbinfo.hashbase = hashinit(1, M_PCB, &ri6pcbinfo.hashmask);
	ri6pcbinfo.porthashbase = hashinit(1, M_PCB, &ri6pcbinfo.porthashmask);
	ri6pcbinfo.ipi_zone = zinit("ri6pcb", sizeof(struct inpcb),
				   nmbclusters / 4, ZONE_INTERRUPT, 0);
#else /* __FreeBSD__ */
#if __NetBSD__
  in_pcbinit(&rawin6pcbtable, 1, 1);
#else /* __NetBSD__ */
#if __OpenBSD__
  in_pcbinit(&rawin6pcbtable, 1);
#else /* __OpenBSD__ */
  rawin6pcb.inp_next = rawin6pcb.inp_prev = &rawin6pcb;
#endif /* __OpenBSD__ */
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */
}

/* At the point where this function gets called, we don't know the nexthdr of
   the current header to be processed, only its offset. So we have to go find
   it the hard way. In the case where there's no chained headers, this is not
   really painful.

   The good news is that all fields have been sanity checked.

   Assumes m has already been pulled up by extra. -cmetz
*/
#if __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__
static __inline__ int ipv6_findnexthdr(struct mbuf *m, size_t extra)
#else /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */
static int ipv6_findnexthdr(struct mbuf *m, size_t extra)
#endif /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ */
{
  caddr_t p = mtod(m, caddr_t);
  int nexthdr = IPPROTO_IPV6;
  unsigned int hl;

  do {
    switch(nexthdr) {
      case IPPROTO_IPV6:
	hl = sizeof(struct ipv6);

	if ((extra -= hl) < 0)
	  return -1;

	nexthdr = ((struct ipv6 *)p)->ipv6_nexthdr;
	break;
      case IPPROTO_HOPOPTS:
      case IPPROTO_DSTOPTS:
	if (extra < sizeof(struct ipv6_opthdr))
	  return -1;

	hl = sizeof(struct ipv6_opthdr) +
	     (((struct ipv6_opthdr *)p)->oh_extlen << 3);

	if ((extra -= hl) < 0)
	  return -1;

	nexthdr = ((struct ipv6_opthdr *)p)->oh_nexthdr;
	break;
      case IPPROTO_ROUTING:
	if (extra < sizeof(struct ipv6_srcroute0))
	  return -1;

	hl = sizeof(struct ipv6_srcroute0) +
	     (((struct ipv6_srcroute0 *)p)->i6sr_len << 3);

	if ((extra -= hl) < 0)
	  return -1;

	nexthdr = ((struct ipv6_srcroute0 *)p)->i6sr_nexthdr;
	break;
#ifdef IPSEC
      case IPPROTO_AH:
	if (extra < sizeof(struct ipv6_srcroute0))
	  return -1;

	hl = sizeof(struct ipv6_srcroute0) +
	     ((struct ipv6_srcroute0 *)p)->i6sr_len << 3;

	if ((extra -= hl) < 0)
	  return -1;

	nexthdr = ((struct ipv6_srcroute0 *)p)->i6sr_nexthdr;
	break;
#endif /* IPSEC */
      default:
	return -1;
    }
    p += hl;
  } while(extra > 0);

  return nexthdr;
}

/*----------------------------------------------------------------------
 * If no HLP's are found for an IPv6 datagram, this routine is called.
 ----------------------------------------------------------------------*/
void
#if __OpenBSD__
ripv6_input(struct mbuf *m, ...)
#else /* __OpenBSD__ */
ripv6_input(m,extra)
     struct mbuf *m;
     int extra;
#endif /* __OpenBSD__ */
{
  register struct ipv6 *ipv6 = mtod(m, struct ipv6 *); /* Will have been
							  pulled up by 
							  ipv6_input(). */
  register struct inpcb *inp;
  int nexthdr, icmpv6type;
  int foundone = 0;
  struct mbuf *m2 = NULL, *opts = NULL;
  struct sockaddr_in6 srcsa;
#ifdef IPSEC
  struct sockaddr_in6 dstsa;
#endif /* IPSEC */
#if __OpenBSD__
  int extra;
  va_list ap;

  va_start(ap, m);
  extra = va_arg(ap, int);
  va_end(ap);
#endif /* __OpenBSD__ */

  DPRINTF(FINISHED, ("ripv6_input(m=%08x, extra=%d)\n", OSDEP_PCAST(m), extra));
  DP(FINISHED, m->m_pkthdr.len, d);
  DDO(FINISHED,printf("In ripv6_input(), header is:\n");dump_mchain(m));
  DPRINTF(EVENT, ("In ripv6_input()\n"));
  DPRINTF(EVENT, ("Header is: "));
  DDO(GROSSEVENT, dump_ipv6(ipv6));

  bzero(&srcsa, sizeof(struct sockaddr_in6));
  srcsa.sin6_family = AF_INET6;
  srcsa.sin6_len = sizeof(struct sockaddr_in6);
  srcsa.sin6_addr = ipv6->ipv6_src;

#if IPSEC
  bzero(&dstsa, sizeof(struct sockaddr_in6));
  dstsa.sin6_family = AF_INET6;
  dstsa.sin6_len = sizeof(struct sockaddr_in6);
  dstsa.sin6_addr = ipv6->ipv6_dst;
#endif /* IPSEC */

#if 0
  /* Will be done already by the previous input functions */
  if (m->m_len < extra)) {
	if (!(m = m_pullup2(m, extra)))
		return;
	ipv6 = mtod(m, struct ipv6 *);
  }
#endif /* 0 */

  if ((nexthdr = ipv6_findnexthdr(m, extra)) < 0) {
    DPRINTF(ERROR, ("ripv6_input: ipv6_findnexthdr failed\n"));
    goto ret;
  }

  DP(FINISHED, nexthdr, d);

  if (nexthdr == IPPROTO_ICMPV6) {
    if (m->m_len < extra + sizeof(struct icmpv6hdr)) {
      if (!(m = m_pullup2(m, extra + sizeof(struct icmpv6hdr)))) {
        DPRINTF(ERROR, ("ripv6_input: m_pullup2 failed\n"));
        goto ret;
      }
      ipv6 = mtod(m, struct ipv6 *);
    }
    icmpv6type = ((struct icmpv6hdr *)(mtod(m, caddr_t) + extra))->icmpv6_type;
  } else
    icmpv6type = -1;

  /*
   * Locate raw PCB for incoming datagram.
   */
#if __FreeBSD__
  for (inp = ri6pcb.lh_first; inp != NULL; inp = inp->inp_list.le_next) {
#else /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
  for (inp = rawin6pcbtable.inpt_queue.cqh_first;
       inp != (struct inpcb *)&rawin6pcbtable.inpt_queue;
       inp = inp->inp_queue.cqe_next) {
#else /* __NetBSD__ || __OpenBSD__ */
  for (inp = rawin6pcb.inp_next; inp != &rawin6pcb; inp = inp->inp_next) {
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */
    if (inp->inp_ipv6.ipv6_nexthdr && inp->inp_ipv6.ipv6_nexthdr != nexthdr)
      continue;
    if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) && 
	!IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, &ipv6->ipv6_dst))
      continue;
    if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6) && 
	!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, &ipv6->ipv6_src))
      continue;
    if ((icmpv6type >= 0) && 
	ICMPV6_FILTER_WILLBLOCK(icmpv6type, &inp->inp_filter))
      continue;

    DPRINTF(IDL_EVENT, ("Found a raw pcb (>1)\n"));
    foundone = 1;

#ifdef IPSEC
    /* Perform input-side policy check. Drop packet if policy says to drop it.

       Note: For ICMPv6 packets, we also checked policy in ipv6_icmp_input().

       XXX - state arg should NOT be NULL, it should be the netproc state
       carried up the stack - cmetz */
    if (!netproc_inputpolicy(NULL, (struct sockaddr *)&srcsa,
	 (struct sockaddr *)&dstsa, nexthdr, m, NULL, NULL))
#endif /* IPSEC */

    DP(FINISHED, m->m_pkthdr.len, d);
    /* Note the inefficiency here; this is a consequence of the interfaces of
       the functions being used. The raw code is not performance critical
       enough to require an immediate fix. - cmetz */
    if ((m2 = m_copym(m, 0, (int)M_COPYALL, M_DONTWAIT))) {
      m_adj(m2, extra);
      DP(FINISHED, m2->m_pkthdr.len, d);
      if (inp->inp_flags & INP_CONTROLOPTS)
        opts = ipv6_headertocontrol(m, extra, inp->inp_flags);
      else
        opts = NULL;
      if (sbappendaddr(&inp->inp_socket->so_rcv, (struct sockaddr *)&srcsa, m2,
                       opts)) {
        sorwakeup(inp->inp_socket);
      } else {
        m_freem(m2);
      };
    };
  };

  if (!foundone) {
    /*
     * We should send an ICMPv6 protocol unreachable here,
     * though original UCB 4.4-lite BSD's IPv4 does not do so.
     */
    ipv6stat.ips_noproto++;
    ipv6stat.ips_delivered--;
  }

ret:
  if (m)
    m_freem(m);

  DPRINTF(FINISHED, ("ripv6_input\n"));
}

/*----------------------------------------------------------------------
 * Output function for raw IPv6.  Called from ripv6_usrreq(), and
 * ipv6_icmp_usrreq().
 ----------------------------------------------------------------------*/

#if __OpenBSD__
int ripv6_output(struct mbuf *m, ...)
#else /* __OpenBSD__ */
int
ripv6_output(m, so, dst, control)
     struct mbuf *m;
     struct socket *so;
     struct in6_addr *dst;
     struct mbuf *control;
#endif /* __OpenBSD__ */
{
  register struct ipv6 *ipv6;
  register struct inpcb *inp;
  int flags;
  struct ifnet *forceif = NULL;
#if __OpenBSD__
  va_list ap;
  struct socket *so;
  struct in6_addr *dst;
  struct mbuf *control;

  va_start(ap, m);
  so = va_arg(ap, struct socket *);
  dst = va_arg(ap, struct in6_addr *);
  control = va_arg(ap, struct mbuf *);
  va_end(ap);
#endif /* __OpenBSD__ */

  inp = sotoinpcb(so);
  flags = (so->so_options & SO_DONTROUTE);

  if (inp->inp_flags & INP_HDRINCL)
    {
      flags |= IPV6_RAWOUTPUT;
      ipv6stat.ips_rawout++;
      /* Maybe m_pullup() ipv6 header here for ipv6_output(), which
	 expects it to be contiguous. */
    }
  else
    {
      M_PREPEND(m, sizeof(struct ipv6), M_WAIT);
      ipv6 = mtod(m, struct ipv6 *);
      ipv6->ipv6_nexthdr = inp->inp_ipv6.ipv6_nexthdr;
      ipv6->ipv6_hoplimit = MAXHOPLIMIT;
      ipv6->ipv6_src = inp->inp_laddr6;
      ipv6->ipv6_dst = *dst;
      ipv6->ipv6_versfl = 0;  /* Or possibly user flow label, in host order. */
      /*
       * Question:  How do I handle options?
       *
       * Answer:  I put them in here, but how?
       */
    }

  {
    int payload = sizeof(struct ipv6);
    int nexthdr = mtod(m, struct ipv6 *)->ipv6_nexthdr;
    int error;

    if (control)
      if ((error = ipv6_controltoheader(&m, control, &forceif, &payload))) {
        m_freem(m);
        return error;
      }

    if (inp->inp_csumoffset >= 0) {
      uint16_t *csum;

      if (!(m = m_pullup2(m, payload + inp->inp_csumoffset))) {
	DPRINTF(IDL_ERROR, ("ripv6_output: m_pullup2(m, %d) failed\n", payload + inp->inp_csumoffset));
	m_freem(m);
	return ENOBUFS;
      };

      csum = (uint16_t *)(mtod(m, uint8_t *) + payload + inp->inp_csumoffset);

      *csum = 0;
      *csum = in6_cksum(m, nexthdr, m->m_pkthdr.len - payload, payload);
    };
  };

  return ipv6_output(m,&inp->inp_route6,flags,inp->inp_moptions6, forceif, so);
}

/*----------------------------------------------------------------------
 * Handles [gs]etsockopt() calls.
 ----------------------------------------------------------------------*/

#if __FreeBSD__
int ripv6_ctloutput(struct socket *so, struct sockopt *sopt)
{
  register struct inpcb *inp = sotoinpcb(so);
  int op;
  int level;
  int optname;
  int optval;

  DPRINTF(FINISHED, ("ripv6_ctloutput(so=%08x, sopt=%08x)\n",
		       OSDEP_PCAST(so), OSDEP_PCAST(sopt)));

  switch(sopt->sopt_dir) {
    case SOPT_GET:
      op = PRCO_GETOPT;
      break;
    case SOPT_SET:
      op = PRCO_SETOPT;
      break;
    default:
      RETURN_ERROR(EINVAL);
  };

  level = sopt->sopt_level;
  optname = sopt->sopt_name;
#else /* __FreeBSD__ */
int
ripv6_ctloutput (op, so, level, optname, m)
     int op;
     struct socket *so;
     int level, optname;
     struct mbuf **m;
{
  register struct inpcb *inp = sotoinpcb(so);

  DPRINTF(FINISHED, ("ripv6_ctloutput(op=%x,so,level=%x,optname=%x,m)\n", op, level, optname));
#endif /* __FreeBSD__ */

  if ((level != IPPROTO_IP) && (level != IPPROTO_IPV6) && (level != IPPROTO_ICMPV6)) {
#if !__FreeBSD__
      if (op == PRCO_SETOPT && *m)
	(void)m_free(*m);
#endif /* !__FreeBSD__ */
      RETURN_ERROR(EINVAL);
  }

  switch (optname) {
    case IPV6_CHECKSUM:
      if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
#if __FreeBSD__
        if (!sopt->sopt_val || (sopt->sopt_valsize != sizeof(int)))
	  RETURN_ERROR(EINVAL);
        if (op == PRCO_SETOPT) {
          int error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int));
          if (error)
	    RETURN_VALUE(error);
          inp->inp_csumoffset = optval;

	  return 0;
        } else
          return sooptcopyout(sopt, &inp->inp_csumoffset, sizeof(int));
#else /* __FreeBSD__ */
        if (!m || !*m || (*m)->m_len != sizeof(int))
	  RETURN_ERROR(EINVAL);
        if (op == PRCO_SETOPT) {
          inp->inp_csumoffset = *(mtod(*m, int *));
          m_freem(*m);
        } else {
          (*m)->m_len = sizeof(int);
          *(mtod(*m, int *)) = inp->inp_csumoffset;
        };
#endif /* __FreeBSD__ */
        return 0;
      };
      break;
    case ICMPV6_FILTER:
      if (op == PRCO_SETOPT || op == PRCO_GETOPT) {
#if __FreeBSD__
        if (!sopt->sopt_val || (sopt->sopt_valsize !=
            sizeof(struct icmpv6_filter)))
	  RETURN_ERROR(EINVAL);
        if (op == PRCO_SETOPT) {
          struct icmpv6_filter icmpv6_filter;
          int error = sooptcopyin(sopt, &icmpv6_filter,
            sizeof(struct icmpv6_filter), sizeof(struct icmpv6_filter));
          if (error)
            return error;

          inp->inp_filter = icmpv6_filter;

          return 0;
        } else
          return sooptcopyout(sopt, &inp->inp_filter,
            sizeof(struct icmpv6_filter));
#else /* __FreeBSD__ */
        if (!m || !*m || (*m)->m_len != sizeof(struct icmpv6_filter))
	  RETURN_ERROR(EINVAL);
        if (op == PRCO_SETOPT) {
          inp->inp_filter = *mtod(*m, struct icmpv6_filter *);
          m_freem(*m);
        } else {
          (*m)->m_len = sizeof(struct icmpv6_filter);
          *mtod(*m, struct icmpv6_filter *) = inp->inp_filter;
        };
        return 0;
#endif /* __FreeBSD__ */
      };
      break;

/* Should this be obsoleted? */
    case IP_HDRINCL:
      if (op == PRCO_SETOPT || op == PRCO_GETOPT)
	{
#if __FreeBSD__
        if (!sopt->sopt_val || (sopt->sopt_valsize != sizeof(int)))
	  RETURN_ERROR(EINVAL);
        if (op == PRCO_SETOPT) {
          int error = sooptcopyin(sopt, &optval, sizeof(int), sizeof(int));
          if (error)
            return error;

          if (optval)
            inp->inp_flags |= INP_HDRINCL;
          else
            inp->inp_flags &= ~INP_HDRINCL;

          return 0;
        } else {
          optval = (inp->inp_flags & INP_HDRINCL) ? 1 : 0;
          return sooptcopyout(sopt, &optval, sizeof(int));
        };
#else /* __FreeBSD__ */
	  if (m == 0 || *m == 0 || (*m)->m_len != sizeof(int))
	    RETURN_ERROR(EINVAL);
	  if (op == PRCO_SETOPT)
	    {
	      if (*mtod(*m, int *))
		inp->inp_flags |= INP_HDRINCL;
	      else inp->inp_flags &= ~INP_HDRINCL;
	      m_free(*m);
	    }
	  else
	    {
	      (*m)->m_len = sizeof(int);
	      *(mtod(*m, int *)) = (inp->inp_flags & INP_HDRINCL) ? 1 : 0;
	    }
	  return 0;
#endif /* __FreeBSD__ */
	}
      break;

#ifdef MRT_INIT
    default:
      if (optname >= MRT_INIT) {
#else /* MRT_INIT */
    case DVMRP_INIT:
    case DVMRP_DONE:
    case DVMRP_ADD_VIF:
    case DVMRP_DEL_VIF:
    case DVMRP_ADD_LGRP:
    case DVMRP_DEL_LGRP:
    case DVMRP_ADD_MRT:
    case DVMRP_DEL_MRT:
      {
#endif /* MRT_INIT */
#ifdef MROUTING
/* Be careful here! */
/*      if (op == PRCO_SETOPT)
	{
	  error = ipv6_mrouter_cmd(optname, so, *m);
	  if (*m)
	    (void)m_free(*m);
	}
      else error = EINVAL;
      return (error);*/
	RETURN_ERROR(EOPNOTSUPP);
#else /* MROUTING */
#if !__FreeBSD__
      if (op == PRCO_SETOPT && *m)
	(void)m_free(*m);
#endif /* !__FreeBSD__ */
      RETURN_ERROR(EOPNOTSUPP);
#endif /* MROUTING */
      };
  }
#if __FreeBSD__
  return ipv6_ctloutput(so, sopt);
#else /* __FreeBSD__ */
  return ipv6_ctloutput(op, so, level, optname, m);
#endif /* __FreeBSD__ */
}

#if __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__
#define MAYBESTATIC static
#define MAYBEINLINE __inline__
#else /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__ */
#define MAYBESTATIC
#define MAYBEINLINE
#endif /* __GNUC__ && __GNUC__ >= 2 && __OPTIMIZE__ && !__FreeBSD__ */

#if __NetBSD__ || __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_attach(struct socket *so, int proto,
                                                struct proc *p)
#else /* __NetBSD__ || __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_attach(struct socket *so, int proto)
#endif /* __NetBSD__ || __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  register int error = 0;

  if (inp)
     panic("ripv6_attach - Already got PCB");

#if __NetBSD__ || __FreeBSD__
  if (p == 0 || (error = suser(p->p_ucred, &p->p_acflag)))
#else /* __NetBSD__ || __FreeBSD__ */
  if ((so->so_state & SS_PRIV) == 0)
#endif /* __NetBSD__ || __FreeBSD__ */
  {
    error = EACCES;
    return error;
  }
  if ((error = soreserve(so, ripv6_sendspace, ripv6_recvspace)) ||

#if __FreeBSD__
    (error = in_pcballoc(so, &ri6pcbinfo, p)))
#else /* __FreeBSD__ */
#if __NetBSD__ ||  __OpenBSD__
    (error = in_pcballoc(so, &rawin6pcbtable)))
#else /* __NetBSD__ || __OpenBSD__ */
    (error = in_pcballoc(so, &rawin6pcb)))
#endif /* __NetBSD__ || __OpenBSD__ */
#endif /* __FreeBSD__ */

     return error;
     
  inp = sotoinpcb(so);
#ifdef	__alpha__
  inp->inp_ipv6.ipv6_nexthdr = (u_long)proto;     /*nam;  Nam contains protocol
						 type, apparently. */
#else
  inp->inp_ipv6.ipv6_nexthdr = (int)proto;     /*nam;   Nam contains protocol
						 type, apparently. */
#endif
  if (inp->inp_ipv6.ipv6_nexthdr == IPPROTO_ICMPV6)
     inp->inp_csumoffset = 2;
  return error;
}

MAYBESTATIC MAYBEINLINE int ripv6_usrreq_detach(struct socket *so)
{
  register struct inpcb *inp = sotoinpcb(so);

  if (inp == 0)
     panic("ripv6_detach");
#ifdef MROUTING
      /* More MROUTING stuff. */
#endif
  in_pcbdetach(inp);
  return 0;
}

MAYBESTATIC MAYBEINLINE int ripv6_usrreq_abort(struct socket *so)
{
   soisdisconnected(so);
   return ripv6_usrreq_detach(so);
}

static MAYBEINLINE int ripv6_usrreq_disconnect(struct socket *so)
{ 
   if ((so->so_state & SS_ISCONNECTED) == 0)
      return ENOTCONN;
   return ripv6_usrreq_abort(so);
}

#if __NetBSD__ || __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_bind(struct socket *so,
				         struct sockaddr *nam, struct proc *p)
#else /* __NetBSD__ || __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_bind(struct socket *so,
					      struct sockaddr *nam)
#endif /* __NetBSD__ || __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  register struct sockaddr_in6 *addr = (struct sockaddr_in6 *)nam;

   /* 'ifnet' is declared in one of the net/ header files. */
#if __NetBSD__ || __OpenBSD__ || __FreeBSD__
   if ((ifnet.tqh_first == 0) ||
#else /* __NetBSD__ || __OpenBSD__ || __FreeBSD__ */
   if ((ifnet == 0) ||
#endif /* __NetBSD__ || __OpenBSD__ || __FreeBSD__ */
       (addr->sin6_family != AF_INET6) ||    /* I only allow AF_INET6 */
       (!IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr) &&
        ifa_ifwithaddr((struct sockaddr *)addr) == 0 ) )

          return EADDRNOTAVAIL;

   inp->inp_laddr6 = addr->sin6_addr;
   return 0; 
}

#if __NetBSD__ || __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_connect(struct socket *so,
					 struct sockaddr *nam, struct proc *p)
#else /* __NetBSD__ || __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_connect(struct socket *so,
						 struct sockaddr *nam)
#endif /* __NetBSD__ || __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  register struct sockaddr_in6 *addr = (struct sockaddr_in6 *) nam;

   if (addr->sin6_family != AF_INET6)
       return EAFNOSUPPORT;

#if __NetBSD__ || __FreeBSD__ || __OpenBSD__
     if (ifnet.tqh_first == 0) {
#else /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */
     if (ifnet == 0) {
#endif /* __NetBSD__ || __FreeBSD__ || __OpenBSD__ */
       return EADDRNOTAVAIL;  /* This is a weird way to say there
			          are no interfaces, no? */
     }

   inp->inp_faddr6 = addr->sin6_addr;  /* Will structure assignment
				          work with this compiler? */
   soisconnected(so);
   return 0;
}

MAYBESTATIC MAYBEINLINE int ripv6_usrreq_shutdown(struct socket *so)
{ 
  socantsendmore(so);
  return 0;
}

#if __NetBSD__ || __FreeBSD__
/*
 * Note that flags and p are not used, but required by protosw in 
 * FreeBSD.
 */
int ripv6_usrreq_send(struct socket *so, int flags, struct mbuf *m,
                      struct sockaddr *addr, struct mbuf *control,
		      struct proc *p)
#else /* __NetBSD__ || __FreeBSD__ */
int ripv6_usrreq_send(struct socket *so, int flags, struct mbuf *m,
                      struct sockaddr *addr, struct mbuf *control)
#endif /* __NetBSD__ || __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  register int error = 0;
  struct in6_addr *dst;

   if (inp == 0) {
      m_freem(m);
      return EINVAL;
   }

   /*
    * Check "connected" status, and if there is a supplied destination
    * address.
    */
   if (so->so_state & SS_ISCONNECTED)
     {
       if (addr)
	   return EISCONN;

       dst = &(inp->inp_faddr6); 
     }
   else
     {
       if (addr == NULL)
	   return ENOTCONN;

       dst = &((struct sockaddr_in6 *)addr)->sin6_addr;
     }

   error = ripv6_output(m,so,dst,control);
   /* m = NULL; */
   return error;
}

#if __NetBSD__ || __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_control(struct socket *so, u_long cmd,
                               caddr_t data, struct ifnet *ifp, struct proc *p)
#else /* __NetBSD__ || __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_control(struct socket *so, int cmd,
                                               caddr_t data, struct ifnet *ifp)
#endif /* __NetBSD__ || __FreeBSD__ */
{ 
/* Notice that IPv4 raw sockets don't pass PRU_CONTROL.  I wonder
   if they panic as well? */
#if __NetBSD__ || __FreeBSD__
      return in6_control(so, cmd, data, ifp, 0, p);
#else /* __NetBSD__ || __FreeBSD__ */
      return in6_control(so, cmd, data, ifp, 0);
#endif /* __NetBSD__ || __FreeBSD__ */
}

MAYBESTATIC MAYBEINLINE int ripv6_usrreq_sense(struct socket *so,
                                               struct stat *sb)
{ 
  /* services stat(2) call. */
  return 0;
}

#if __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_sockaddr(struct socket *so,
                                                  struct sockaddr **nam)
#else /* __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_sockaddr(struct socket *so,
                                                  struct mbuf *nam)
#endif /* __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  return in6_setsockaddr(inp, nam);
}

#if __FreeBSD__
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_peeraddr(struct socket *so,
                                                  struct sockaddr **nam)
#else /* __FreeBSD__ */
MAYBESTATIC MAYBEINLINE int ripv6_usrreq_peeraddr(struct socket *so,
                                                  struct mbuf *nam)
#endif /* __FreeBSD__ */
{
  register struct inpcb *inp = sotoinpcb(so);
  return in6_setpeeraddr(inp, nam);
}

#if __FreeBSD__
struct pr_usrreqs ripv6_usrreqs = {
  ripv6_usrreq_abort, pru_accept_notsupp, ripv6_usrreq_attach,
  ripv6_usrreq_bind, ripv6_usrreq_connect, pru_connect2_notsupp,
  ripv6_usrreq_control, ripv6_usrreq_detach, ripv6_usrreq_detach,
  pru_listen_notsupp, ripv6_usrreq_peeraddr, pru_rcvd_notsupp,
  pru_rcvoob_notsupp, ripv6_usrreq_send, ripv6_usrreq_sense,
  ripv6_usrreq_shutdown, ripv6_usrreq_sockaddr, sosend, soreceive, sopoll
};
#endif /* __FreeBSD__ */

/*----------------------------------------------------------------------
 * Handles PRU_* for raw IPv6 sockets.
 ----------------------------------------------------------------------*/
#if !__FreeBSD__
int
#if __NetBSD__
ripv6_usrreq(so, req, m, nam, control, p)
#else /* __NetBSD__ */
ripv6_usrreq(so, req, m, nam, control)
#endif /* __NetBSD__ */
     struct socket *so;
     int req;
     struct mbuf *m, *nam, *control;
#if __NetBSD__
     struct proc *p;
#endif /* __NetBSD__ */
{
  register int error = 0;

  DPRINTF(IDL_EVENT, ("ripv6_usrreq(so, req, m, nam, control)\n"));

#ifdef MROUTING
  /*
   * Ummm, like, multicast routing stuff goes here, huh huh huh.
   *
   * Seriously, this would be for user-level multicast routing daemons.  With
   * multicast being a requirement for IPv6, code like what might go here
   * may go away.
   */
#endif

  switch (req)
    {
    case PRU_ATTACH:
#if __NetBSD__
      error = ripv6_usrreq_attach(so, 0, p);  /* XXX */
#else /* __NetBSD__ */
      error = ripv6_usrreq_attach(so, 0);  /* XXX */
#endif /* __NetBSD__ */
      break;
    case PRU_DISCONNECT:
      error = ripv6_usrreq_disconnect(so);
      break;
      /* NOT */
      /* FALLTHROUGH */
    case PRU_ABORT:
      error = ripv6_usrreq_abort(so);
      break;
      /* NOT */
      /* FALLTHROUGH */
    case PRU_DETACH:
      error = ripv6_usrreq_detach(so);
      break;
    case PRU_BIND:
      if (nam->m_len != sizeof(struct sockaddr_in6))
         return EINVAL;
      /*
       * Be strict regarding sockaddr_in6 fields.
       */
#if __NetBSD__
      error = ripv6_usrreq_bind(so, mtod(nam, struct sockaddr *), p);
#else /* __NetBSD__ */
      error = ripv6_usrreq_bind(so, mtod(nam, struct sockaddr *));
#endif /* __NetBSD__ */
      break;
    case PRU_CONNECT:
      /*
       * Be strict regarding sockaddr_in6 fields.
       */
      if (nam->m_len != sizeof(struct sockaddr_in6))
          return EINVAL;
#if __NetBSD__
      error = ripv6_usrreq_connect(so, mtod(nam, struct sockaddr *), p);
#else /* __NetBSD__ */
      error = ripv6_usrreq_connect(so, mtod(nam, struct sockaddr *));
#endif /* __NetBSD__ */
      break;
    case PRU_SHUTDOWN:
      error = ripv6_usrreq_shutdown(so);
      break;
    case PRU_SEND:
      /*
       * Be strict regarding sockaddr_in6 fields.
       */
      if (nam->m_len != sizeof(struct sockaddr_in6))
          return EINVAL;
#if __NetBSD__
      error = ripv6_usrreq_send(so, 0, m, mtod(nam, struct sockaddr *), control, p);
#else /* __NetBSD__ */
      error = ripv6_usrreq_send(so, 0, m, mtod(nam, struct sockaddr *), control);
#endif /* __NetBSD__ */
      m = NULL;
      break;
    case PRU_CONTROL:
#if __NetBSD__
      return ripv6_usrreq_control(so, (u_long)m, (caddr_t) nam, 
                               (struct ifnet *) control, p);
#else /* __NetBSD__ */
      return ripv6_usrreq_control(so, (int)m, (caddr_t) nam, 
                               (struct ifnet *) control);
#endif /* __NetBSD__ */
    case PRU_SENSE:
      return ripv6_usrreq_sense(so, NULL); /* XXX */
    case PRU_CONNECT2:
    case PRU_RCVOOB:
    case PRU_LISTEN:
    case PRU_SENDOOB:
    case PRU_RCVD:
    case PRU_ACCEPT:
      error = EOPNOTSUPP;
      break;
    case PRU_SOCKADDR:
      error = ripv6_usrreq_sockaddr(so, nam);
      break;
    case PRU_PEERADDR:
      error = ripv6_usrreq_peeraddr(so, nam);
      break;
    default:
      panic ("ripv6_usrreq - unknown req\n");
    }
  if (m != NULL)
    m_freem(m);
  return error;
}
#endif /* !__FreeBSD__ */
