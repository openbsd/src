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
 * Copyright (c) 1982, 1986, 1990, 1993, 1995
 *	Regents of the University of California.  All rights reserved.
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
 */

#if __FreeBSD__
#include <sys/queue.h>
#endif /* __FreeBSD__ */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
/*#include <sys/ioctl.h>*/
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>

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

#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
#include <machine/pcpu.h>
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */

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
 * External globals
 */

#if __OpenBSD__
#include <dev/rndvar.h>
#endif /* __OpenBSD__ */

#if __FreeBSD__
/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
static int ipport_lowfirstauto  = IPPORT_RESERVED - 1;	/* 1023 */
static int ipport_lowlastauto = IPPORT_RESERVEDSTART;	/* 600 */
static int ipport_firstauto = IPPORT_RESERVED;		/* 1024 */
static int ipport_lastauto  = IPPORT_USERRESERVED;	/* 5000 */
static int ipport_hifirstauto = IPPORT_HIFIRSTAUTO;	/* 49152 */
static int ipport_hilastauto  = IPPORT_HILASTAUTO;	/* 65535 */
extern void in_rtchange(struct inpcb *, int);
#endif /* __FreeBSD__ */

extern struct in6_ifaddr *in6_ifaddr;
#if __FreeBSD__
int in6_pcbladdr(register struct inpcb *, struct sockaddr *, struct sockaddr **);
extern  TAILQ_HEAD(in_ifaddrhead, in_ifaddr) in_ifaddrhead;
#else /* __FreeBSD__ */
extern struct in_ifaddr *in_ifaddr;
#endif /* __FreeBSD__ */
/*
 * Globals
 */

struct in6_addr zeroin6_addr;

#if __OpenBSD__
extern int ipport_firstauto;
extern int ipport_lastauto;
extern int ipport_hifirstauto;
extern int ipport_hilastauto;
#endif /* __OpenBSD__ */

/*
 * Keep separate inet6ctlerrmap, because I may remap some of these.
 * I also put it here, because, quite frankly, it belongs here, not in
 * ip{v6,}_input().
 */
u_char inet6ctlerrmap[PRC_NCMDS] = {
        0,              0,              0,              0,
        0,              EMSGSIZE,       EHOSTDOWN,      EHOSTUNREACH,
        EHOSTUNREACH,   EHOSTUNREACH,   ECONNREFUSED,   ECONNREFUSED,
        EMSGSIZE,       EHOSTUNREACH,   0,              0,
        0,              0,              0,              0,
        ENOPROTOOPT
};

/*----------------------------------------------------------------------
 * Bind an address (or at least a port) to an PF_INET6 socket.
 ----------------------------------------------------------------------*/
int
in6_pcbbind(inp, nam)
     register struct inpcb *inp;
#if __FreeBSD__
     struct sockaddr *nam;
#else /* __FreeBSD__ */
     struct mbuf *nam;
#endif /* __FreeBSD__ */
{
  register struct socket *so = inp->inp_socket;

#if __NetBSD__ || __OpenBSD__
  register struct inpcbtable *head = inp->inp_table;
#else /* __OpenBSD__ */
#ifndef __FreeBSD__
  register struct inpcb *head = inp->inp_head;
#else /* __FreeBSD__ */
    struct inpcbinfo *pcbinfo = inp->inp_pcbinfo;
#endif /* __FreeBSD__ */
#endif /* __NetBSD__ || __OpenBSD__ */
  register struct sockaddr_in6 *sin6;
#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
  struct proc *p = curproc;		/* XXX */
#else /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
  struct proc *p = PCPU(curproc);		/* XXX */
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
  u_short lport = 0;
  int wild = INPLOOKUP_IPV6, reuseport = (so->so_options & SO_REUSEPORT);
  int error;

  /*
   * REMINDER:  Once up to speed, flow label processing should go here,
   *            too.  (Same with in6_pcbconnect.)  
   */

#if __FreeBSD__
  if (in6_ifaddr == 0 || in_ifaddrhead.tqh_first == 0)
    return EADDRNOTAVAIL;
#else /* __FreeBSD__ */
  if (in6_ifaddr == 0 || in_ifaddr == 0)
    return EADDRNOTAVAIL;
#endif/* __FreeBSD__ */
  if (inp->inp_lport != 0 || !IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
    return EINVAL;   /* If already bound, EINVAL! */

  if ((so->so_options & (SO_REUSEADDR | SO_REUSEPORT)) == 0 &&
      ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
       (so->so_options & SO_ACCEPTCONN) == 0))
    wild |= INPLOOKUP_WILDCARD;

  /*
   * If I did get a sockaddr passed in...
   */
  if (nam)
    {
#if __FreeBSD__
      sin6 = (struct sockaddr_in6 *)nam;
      if (nam->sa_len != sizeof (*sin6))
	return EINVAL;
#else /* __FreeBSD__ */
      sin6 = mtod(nam, struct sockaddr_in6 *);
      if (nam->m_len != sizeof (*sin6))
	return EINVAL;
#endif /* __FreeBSD__ */

      /*
       * Unlike v4, I have no qualms about EAFNOSUPPORT if the
       * wretched family is not filled in!
       */
      if (sin6->sin6_family != AF_INET6)
	return EAFNOSUPPORT;
      lport = sin6->sin6_port;

      if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
	{
	  /*
	   * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
	   * allow complete duplication of binding if
	   * SO_REUSEPORT is set, or if SO_REUSEADDR is set
	   * and a multicast address is bound on both
	   * new and duplicated sockets.
	   */
	  if (so->so_options & SO_REUSEADDR)
	    reuseport = SO_REUSEADDR | SO_REUSEPORT;
	}
      else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
	{
	  struct sockaddr_in sin;

	  sin.sin_port = 0;
	  sin.sin_len = sizeof(sin);
	  sin.sin_family = AF_INET;
	  sin.sin_addr.s_addr = sin6->sin6_addr.in6a_words[3];
	  bzero(&sin.sin_zero,8);

	  sin6->sin6_port = 0;  /* Yechhhh, because of upcoming call to
				   ifa_ifwithaddr(), which does bcmp's
				   over the PORTS as well.  (What about flow?)
				   */
	  sin6->sin6_flowinfo = 0;
	  if (ifa_ifwithaddr((struct sockaddr *)sin6) == 0)
	    if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) ||
		ifa_ifwithaddr((struct sockaddr *)&sin) == 0)
	      return EADDRNOTAVAIL;
	}
      if (lport)
	{
	  struct inpcb *t;
	  struct in_addr fa,la;

	  /* Question:  Do we wish to continue the Berkeley tradition of
	     ports < IPPORT_RESERVED be only for root? 

	     Answer:  For now yes, but IMHO, it should be REMOVED! 

	     OUCH:  One other thing, is there no better way of finding
	     a process for a socket instead of using curproc?  (Marked
	     with BSD's {in,}famous XXX ? */
	  if (ntohs(lport) < IPPORT_RESERVED &&
	      (error = suser(p->p_ucred, &p->p_acflag)))
	    return error;

	  if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	    {
	      fa.s_addr = 0;
	      la.s_addr = sin6->sin6_addr.in6a_words[3];
	      wild &= ~INPLOOKUP_IPV6;

#if __FreeBSD__
	      t = in_pcblookup_local(inp->inp_pcbinfo, 
			       (struct in_addr *)&la, lport, wild);
#else /* __FreeBSD__ */
#if __NetBSD__
	      t = in_pcblookup_port(inp->inp_table, (struct in_addr *)&la,
				    lport, wild);
#else /* __NetBSD__ */
	      t = in_pcblookup(head, (struct in_addr *)&fa, 0,
			       (struct in_addr *)&la, lport, wild);
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */

	    }
	  else
	    {
#if __FreeBSD__
	      t = in_pcblookup_local(inp->inp_pcbinfo, 
			(struct in_addr *)&sin6->sin6_addr, lport, wild);
#else /* __FreeBSD__ */
#if __NetBSD__
	      t = in_pcblookup_port(inp->inp_table,
			          (struct in_addr *)&in6addr_any, lport, wild);
#else /* __NetBSD__ */
	      t = in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&sin6->sin6_addr, lport, wild);
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */
	    }

	  if (t && (reuseport & t->inp_socket->so_options) == 0)
	    return EADDRINUSE;
	}
      inp->inp_laddr6 = sin6->sin6_addr;

      if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	inp->inp_ipv6.ipv6_versfl = htonl(0x60000000) | 
	  (sin6->sin6_flowinfo & htonl(0x0fffffff));

      /*
       * Unroll first 2 compares of {UNSPEC,V4MAPPED}.
       * Mark PF_INET6 socket as undecided (bound to port-only) or
       * mapped (INET6 socket talking IPv4) here.  I may need to move
       * this code out of this if (nam) clause, and put it just before
       * function return.
       *
       * Then again, the only time this function is called with NULL nam
       * might be during a *_pcbconnect(), which then sets the local address
       * ANYWAY.
       */
      if (inp->inp_laddr6.in6a_words[0] == 0 && 
	  inp->inp_laddr6.in6a_words[1] == 0)
	{
	  if (inp->inp_laddr6.in6a_words[2] == ntohl(0xffff))
	    inp->inp_flags |= INP_IPV6_MAPPED;
	  if (inp->inp_laddr6.in6a_words[2] == 0 &&
	      inp->inp_laddr6.in6a_words[3] == 0)
	    inp->inp_flags |= INP_IPV6_UNDEC;
	}
    }

  if (lport == 0) {
    /* This code block was derived from OpenBSD */
    uint16_t first, last, old = 0;
    int count;
    int loopcount = 0;
    struct in_addr fa, la;
    u_int16_t *lastport;
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
    u_int16_t last_port = head->inp_lport;

    lastport = &last_port;
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
#if __NetBSD__ || __OpenBSD__
    lastport = &inp->inp_table->inpt_lastport;
#endif /* __NetBSD__ || __OpenBSD__ */

    if (inp->inp_flags & INP_IPV6_MAPPED) {
      la.s_addr = inp->inp_laddr6.in6a_words[3];
      fa.s_addr = 0;
      wild &= ~INPLOOKUP_IPV6;
    };

#if __OpenBSD__ || __FreeBSD__
#if __FreeBSD__
    inp->inp_flags |= INP_ANONPORT;
#endif /* __FreeBSD__ */

    if (inp->inp_flags & INP_HIGHPORT) {
      first = ipport_hifirstauto;	/* sysctl */
      last = ipport_hilastauto;
#if __FreeBSD__
      lastport = &pcbinfo->lasthi;
#endif /* __FreeBSD__ */
    } else if (inp->inp_flags & INP_LOWPORT) {
      if ((error = suser(p->p_ucred, &p->p_acflag)))
	return (EACCES);
#if __FreeBSD__
      first = ipport_lowfirstauto;
      last = ipport_lowlastauto;
      lastport = &pcbinfo->lastlow;
#else /* __FreeBSD__ */
      first = IPPORT_RESERVED-1; /* 1023 */
      last = 600;		   /* not IPPORT_RESERVED/2 */
#endif /* __FreeBSD__ */
    } else {
      first = ipport_firstauto;	/* sysctl */
      last  = ipport_lastauto;
#if __FreeBSD__
      lastport = &pcbinfo->lastport;
#endif /* __FreeBSD__ */
    }
#else /* __OpenBSD__ */
#if __bsdi__
    first = IPPORT_DYNAMIC;
    last = IPPORT_DYNAMIC_LAST;
#else /* __bsdi__ */
    first = IPPORT_RESERVED;
    last = IPPORT_USERRESERVED;
#endif /* __bsdi__ */
#endif /* __OpenBSD__ */

    /*
     * Simple check to ensure all ports are not used up causing
     * a deadlock here.
     *
     * We split the two cases (up and down) so that the direction
     * is not being tested on each round of the loop.
     */

portloop:
    if (first > last) {
      /*
       * counting down
       */
      if (loopcount == 0) {	/* only do this once. */
	old = first;
#if __OpenBSD__
	first -= (arc4random() % (first - last));
#else /* __OpenBSD__ */
	first -= (random() % (first - last));
#endif /* __OpenBSD__ */
      }
      count = first - last;
      *lastport = first;		/* restart each time */
      
      do {
	if (count-- <= 0) {	/* completely used? */
	  if (loopcount == 0) {
	    last = old;
	    loopcount++;
	    goto portloop;
	  }
	  return (EADDRNOTAVAIL);
	}
	--*lastport;
	if (*lastport > first || *lastport < last)
	  *lastport = first;
	lport = htons(*lastport);
#if __OpenBSD__
      } while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
#else /* __OpenBSD__ */
      } while (
#endif /* __OpenBSD__ */
	       ((wild & INPLOOKUP_IPV6) ?
#ifdef __FreeBSD__ 
		in_pcblookup_local(pcbinfo,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup_local(pcbinfo,
			(struct in_addr *)&la, lport, wild)));
#else /* __FreeBSD__ */
#if __NetBSD__
		in_pcblookup_port(head, (struct in_addr *)&inp->inp_laddr6,
				  lport, wild) :
		in_pcblookup_port(head, (struct in_addr *)&la, lport, wild)));
#else /* __NetBSD__ */
		in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup(head, (struct in_addr *)&fa, 0,
			(struct in_addr *)&la, lport, wild)));
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */
    } else {
      /*
       * counting up
       */
      if (loopcount == 0) {	/* only do this once. */
	old = first;
#if __OpenBSD__
	first += (arc4random() % (last - first));
#else /* __OpenBSD__ */
	first += (random() % (last - first));
#endif /* __OpenBSD__ */
      }
      count = last - first;
      *lastport = first;		/* restart each time */
      
      do {
	if (count-- <= 0) {	/* completely used? */
	  if (loopcount == 0) {
	    first = old;
	    loopcount++;
	    goto portloop;
	  }
	  return (EADDRNOTAVAIL);
	}
	++*lastport;
	if (*lastport < first || *lastport > last)
	  *lastport = first;
	lport = htons(*lastport);
#if __OpenBSD__
      } while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
#else /* __OpenBSD__ */
      } while (
#endif /* __OpenBSD__ */
	       ((wild & INPLOOKUP_IPV6) ?
#ifdef __FreeBSD__ 
		in_pcblookup_local(pcbinfo,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup_local(pcbinfo,
			(struct in_addr *)&la, lport, wild)));
#else /* __FreeBSD__ */
#if __NetBSD__
		in_pcblookup_port(head, (struct in_addr *)&inp->inp_laddr6,
		  lport, wild) :
		in_pcblookup_port(head, (struct in_addr *)&la, lport, wild)));
#else /* __NetBSD__ */
		in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup(head, (struct in_addr *)&fa, 0,
			(struct in_addr *)&la, lport, wild)));
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */
    }
  }

  inp->inp_lport = lport;

#if __FreeBSD__ 
#if 0
	 inp->inp_laddr.s_addr = inp->inp_laddr6.in6a_words[3]
	                       ^ inp->inp_laddr6.in6a_words[1];
  	/* 
	 * If this is FreeBSD then it requires the inpcb structure to be 
	 * inserted into various hash-tables.  Right now this is copies 
	 * directly from FreeBSD's source, but this will have to change for
	 * v6 addresses soon I think.  We're still not sure if word2 XOR word4
	 * is a good subject for the hash function.
	 */
#endif /* 0 */
	if (in_pcbinshash(inp) != 0) {
		bzero(&inp->inp_laddru, sizeof(inp->inp_laddru));
		inp->inp_lport = 0;
		return (EAGAIN);
	}
#endif /* __FreeBSD__ */
#if __NetBSD__
  in_pcbstate(inp, INP_BOUND);
#endif /* __NetBSD__ */

  /* XXX hash */
  return 0;
}

/*----------------------------------------------------------------------
 * This is maximum suckage point value.  This function is the first 
 * half or so of in6_pcbconnect.  It exists because FreeBSD's code 
 * supports this function for T/TCP.  If we want TCP to work with our 
 * code it must be supported. 
 ----------------------------------------------------------------------*/
#if __FreeBSD__ 
int
in6_pcbladdr(inp, nam, plocal_sin)
	 register struct inpcb *inp;
	 struct sockaddr *nam;
	 struct sockaddr **plocal_sin;
{

  struct in6_ifaddr *i6a;
  struct sockaddr_in  *ifaddr = NULL;

  register struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
  if (nam->sa_len != sizeof(struct sockaddr_in6))
    return (EINVAL);
    
  if (sin6->sin6_family != AF_INET6)
    return (EAFNOSUPPORT);
  if (sin6->sin6_port == 0)
    return (EADDRNOTAVAIL);


  if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
    {
      struct domain hackdomain, *save;
      struct sockaddr_in *sin;
      int rc;

      /*
       * FreeBSD doesn't want an mbuf to pcbconnect.
       */
      struct sockaddr_in hacksin;

      /*
       * Hmmm, if this is a v4-mapped v6 address, re-call in_pcbconnect
       * with a fake domain for now.  Then re-adjust the socket, and
       * return out of here.  Since this is called at splnet(), I don't
       * think temporarily altering the socket will matter.  XXX
       */

      /*
       * Can't have v6 talking to v4, now can we!
       */
      if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) && 
	  !IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6))
	return EINVAL;  

      bzero(&hackdomain,sizeof(hackdomain));
      save = inp->inp_socket->so_proto->pr_domain;
      inp->inp_socket->so_proto->pr_domain = &hackdomain;
      hackdomain.dom_family = PF_INET;

      sin = &hacksin;
      sin->sin_len = sizeof(*sin);
      sin->sin_family = AF_INET;
      sin->sin_port = sin6->sin6_port;
      sin->sin_addr.s_addr = sin6->sin6_addr.in6a_words[3];

      /* CHANGE */
      rc = in_pcbladdr(inp, (struct sockaddr *) sin, &ifaddr);

      inp->inp_socket->so_proto->pr_domain = save;

      if (rc == 0)
	{
	  inp->inp_laddr6.in6a_words[2] = htonl(0xffff);
	  inp->inp_faddr6.in6a_words[2] = htonl(0xffff);
	  inp->inp_flags |= INP_IPV6_MAPPED;
	  inp->inp_flags &= ~INP_IPV6_UNDEC;
	}
      *plocal_sin = (struct sockaddr *)ifaddr;

      return rc;
    }

  if (in6_ifaddr && IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
    {
      /*
       * If the destination address is all 0's,
       * use the first non-loopback (and if possibly, non-link-local, else
       * use the LAST link-local, non-loopback) address as the destination.
       */
#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define sin6tosa(sin6)	((struct sockaddr *)(sin6))
#define ifatoi6a(ifa)	((struct in6_ifaddr *)(ifa))
      struct in6_ifaddr *ti6a = NULL;

      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
        {
          /* Find first (non-link-local if possible) address for
             source usage.  If multiple link-locals, use last one found. */
	  if (IN6_IS_ADDR_LINKLOCAL(&I6A_SIN(i6a)->sin6_addr))
	    ti6a=i6a;
	  else if (!IN6_IS_ADDR_LOOPBACK(&I6A_SIN(i6a)->sin6_addr))
	    break;
        }
      if (i6a == NULL && ti6a != NULL)
	i6a = ti6a;
    }

  if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
    {
      register struct route6 *ro;

      i6a = NULL;
      /* 
       * If route is known or can be allocated now,
       * our src addr is taken from the rt_ifa, else punt.
       */
      ro = &inp->inp_route6;
      if (ro->ro_rt &&
	  (ro->ro_dst.sin6_family != sin6->sin6_family ||	
	   rt_key(ro->ro_rt)->sa_family != sin6->sin6_family ||
	   !IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &sin6->sin6_addr) ||
	   inp->inp_socket->so_options & SO_DONTROUTE))
	{
	  RTFREE(ro->ro_rt);
	  ro->ro_rt = NULL;
	}
      if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
	  (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL))
	{
	  /* No route yet, try and acquire one. */
	  ro->ro_dst.sin6_family = AF_INET6;  /* Is ro blanked out? */
	  ro->ro_dst.sin6_len = sizeof(struct sockaddr_in6);
	  ro->ro_dst.sin6_addr = sin6->sin6_addr;
	  /* 
	   *  Need to wipe out the flowlabel for ifa_ifwith*
	   *  but don't need to for rtalloc.
	   */
	  rtalloc((struct route *)ro);
	}

      if (ro->ro_rt == NULL)
	{
	  /*
	   * No route of any kind, so spray neighbor solicits out all
	   * interfaces, unless it's a multicast address.
	   */
	  if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
	    return ENETUNREACH;
	  ipv6_onlink_query((struct sockaddr_in6 *)&ro->ro_dst);
	  rtalloc((struct route *)ro);
	}
      if (ro->ro_rt == NULL)
	{
	  /*
	   * ipv6_onlink_query() should've added a route.  It probably
	   * failed.
	   */
	  DPRINTF(IDL_GROSS_EVENT, ("v6_output: onlink_query didn't add route!\n"));
	  return ENETUNREACH;
	}

      if (ro->ro_rt->rt_ifa == NULL)
	{
	  /*
	   * We have a route where we don't quite know which interface 
	   * the neighbor belongs to yet.  If I get here, I know that this
	   * route is not pre-allocated (such as done by in6_pcbconnect()),
	   * because those pre-allocators will do the same ipv6_onlink_query()
	   * and ipv6_verify_onlink() in advance.
	   *
	   * I can therefore free the route, and get it again.
	   */
	  int error;

	  RTFREE(ro->ro_rt);
	  ro->ro_rt = NULL;
	  switch (error = ipv6_verify_onlink(&ro->ro_dst))
	    {
	    case -1:
	      return ENETUNREACH;
	    case 0:
	      break;
	    default:
	      return error;
	    }
	  rtalloc((struct route *)ro);
	  if (ro->ro_rt == NULL || ro->ro_rt->rt_ifa == NULL)
	    panic("Oops2, I'm forgetting something after verify_onlink().");
	}


      /*
       * If we found a route, use the address
       * corresponding to the outgoing interface
       * unless it is the loopback (in case a route
       * to our address on another net goes to loopback).
       *
       * This code may be simplified if the above gyrations work.
       */
      if (ro->ro_rt && ro->ro_rt->rt_ifp &&
		!(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
	i6a = ifatoi6a(ro->ro_rt->rt_ifa);
      if (i6a == NULL)
	{
	  u_short fport = sin6->sin6_port;

	  /*
	   * Source address selection when there is no route.
	   *
	   * If ND out all if's is to be used, this is the point to do it.
	   * (Similar to when the route lookup fails in ipv6_output.c, and
	   * the 'on-link' assumption kicks in.)
	   *
	   * For multicast, use i6a of mcastdef.
	   */

	  sin6->sin6_port = 0;
	  i6a = ifatoi6a(ifa_ifwithdstaddr(sin6tosa(sin6)));
	  if (i6a == NULL)
	    i6a = ifatoi6a(ifa_ifwithnet(sin6tosa(sin6)));
	  sin6->sin6_port = fport;
	  if (i6a == NULL)
	    {
	      struct in6_ifaddr *ti6a = NULL;
	      
	      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
		{
		  /* Find first (non-local if possible) address for
		     source usage.  If multiple locals, use last one found. */
		  if (IN6_IS_ADDR_LINKLOCAL(&I6A_SIN(i6a)->sin6_addr))
		    ti6a=i6a;
		  else if (!IN6_IS_ADDR_LOOPBACK(&I6A_SIN(i6a)->sin6_addr))
		    break;
		}
	      if (i6a == NULL && ti6a != NULL)
		i6a = ti6a;
	    }
	  if (i6a == NULL)
	    return EADDRNOTAVAIL;
	}

      /*
       * If I'm "connecting" to a multicast address, gyrate properly to
       * get a source address based upon the user-requested mcast interface.
       */
      if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) && inp->inp_moptions6 != NULL &&
	  (inp->inp_flags & INP_IPV6_MCAST))
	{
	  struct ipv6_moptions *i6mo;
	  struct ifnet *ifp;
	  
	  i6mo = inp->inp_moptions6;
	  if (i6mo->i6mo_multicast_ifp != NULL)
	    {
	      ifp = i6mo->i6mo_multicast_ifp;
	      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
		if (i6a->i6a_ifp == ifp)  /* Linkloc vs. global? */
		  break;
	      if (i6a == NULL)
		return EADDRNOTAVAIL;
	    }
	}

      *plocal_sin = (struct sockaddr *)&i6a->i6a_addr;
    }

    return (0);
}
#endif /* __FreeBSD__ */

/*----------------------------------------------------------------------
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin6.
 * Eventually, flow labels will have to be dealt with here, as well.
 *
 * If don't have a local address for this socket yet,
 * then pick one.
 *
 * I believe this has to be called at splnet().
 ----------------------------------------------------------------------*/

int
in6_pcbconnect(inp, nam)
	register struct inpcb *inp;
#if __FreeBSD__
	struct sockaddr *nam;
#else /* __FreeBSD__ */
	struct mbuf *nam;
#endif /* __FreeBSD__ */
{
  struct in6_ifaddr *i6a;
  struct sockaddr_in6 *ifaddr = NULL;

#if __FreeBSD__
  register struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)nam;
#else /* __FreeBSD__ */
  register struct sockaddr_in6 *sin6 = mtod(nam, struct sockaddr_in6 *);
  if (nam->m_len != sizeof(struct sockaddr_in6))
    return (EINVAL);
#endif /* __FreeBSD__ */

  if (sin6->sin6_len != sizeof(struct sockaddr_in6))
    return (EINVAL);
  if (sin6->sin6_family != AF_INET6)
    return (EAFNOSUPPORT);
  if (sin6->sin6_port == 0)
    return (EADDRNOTAVAIL);

  if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
    {
      struct domain hackdomain, *save;
      struct mbuf hackmbuf;
      struct sockaddr_in *sin;
      int rc;

#if __FreeBSD__ 
      /*
       * FreeBSD doesn't want an mbuf to pcbconnect.
       */
      struct sockaddr_in hacksin;
#endif /* __FreeBSD__ */

      /*
       * Hmmm, if this is a v4-mapped v6 address, re-call in_pcbconnect
       * with a fake domain for now.  Then re-adjust the socket, and
       * return out of here.  Since this is called at splnet(), I don't
       * think temporarily altering the socket will matter.  XXX
       */

      /*
       * Can't have v6 talking to v4, now can we!
       */
      if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) && 
	  !IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6))
	return EINVAL;  

      bzero(&hackdomain,sizeof(hackdomain));
      bzero(&hackmbuf,sizeof(hackmbuf));
      save = inp->inp_socket->so_proto->pr_domain;
      inp->inp_socket->so_proto->pr_domain = &hackdomain;
      hackdomain.dom_family = PF_INET;
#ifndef __FreeBSD__ 
      hackmbuf.m_hdr = nam->m_hdr;
#endif /* __FreeBSD__ */
      hackmbuf.m_len = sizeof(*sin);
      hackmbuf.m_data = hackmbuf.m_dat;

#if __FreeBSD__ 
      sin = &hacksin;
#else /* __FreeBSD__ */
      sin = mtod(&hackmbuf,struct sockaddr_in *);
#endif /* __FreeBSD__ */
      sin->sin_len = sizeof(*sin);
      sin->sin_family = AF_INET;
      sin->sin_port = sin6->sin6_port;
      sin->sin_addr.s_addr = sin6->sin6_addr.in6a_words[3];

#if __FreeBSD__
  {
#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
  struct proc *p = curproc;		/* XXX */
#else /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
  struct proc *p = PCPU(curproc);		/* XXX */
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */
      rc = in_pcbconnect(inp, (struct sockaddr *) sin, p);
  }
#else /* __FreeBSD__ */
      rc = in_pcbconnect(inp,&hackmbuf);
#endif /* __FreeBSD__ */

      inp->inp_socket->so_proto->pr_domain = save;

      if (rc == 0)
	{
	  inp->inp_laddr6.in6a_words[2] = htonl(0xffff);
	  inp->inp_faddr6.in6a_words[2] = htonl(0xffff);
	  inp->inp_flags |= INP_IPV6_MAPPED;
	  inp->inp_flags &= ~INP_IPV6_UNDEC;
	}

      return rc;
    }

  if (in6_ifaddr && IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr))
    {
      /*
       * If the destination address is all 0's,
       * use the first non-loopback (and if possibly, non-link-local, else
       * use the LAST link-local, non-loopback) address as the destination.
       */
#define	satosin6(sa)	((struct sockaddr_in6 *)(sa))
#define sin6tosa(sin6)	((struct sockaddr *)(sin6))
#define ifatoi6a(ifa)	((struct in6_ifaddr *)(ifa))
      struct in6_ifaddr *ti6a = NULL;

      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
        {
          /* Find first (non-link-local if possible) address for
             source usage.  If multiple link-locals, use last one found. */
	  if (IN6_IS_ADDR_LINKLOCAL(&I6A_SIN(i6a)->sin6_addr))
	    ti6a=i6a;
	  else if (!IN6_IS_ADDR_LOOPBACK(&I6A_SIN(i6a)->sin6_addr))
	    break;
        }
      if (i6a == NULL && ti6a != NULL)
	i6a = ti6a;
    }

  if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
    {
      register struct route6 *ro;


      i6a = NULL;
      /* 
       * If route is known or can be allocated now,
       * our src addr is taken from the rt_ifa, else punt.
       */
      ro = &inp->inp_route6;
      if (ro->ro_rt &&
	  (ro->ro_dst.sin6_family != sin6->sin6_family ||	
	   rt_key(ro->ro_rt)->sa_family != sin6->sin6_family ||
	   !IN6_ARE_ADDR_EQUAL(&ro->ro_dst.sin6_addr, &sin6->sin6_addr) ||
	   inp->inp_socket->so_options & SO_DONTROUTE))
	{
	  RTFREE(ro->ro_rt);
	  ro->ro_rt = NULL;
	}
      if ((inp->inp_socket->so_options & SO_DONTROUTE) == 0 && /*XXX*/
	  (ro->ro_rt == NULL || ro->ro_rt->rt_ifp == NULL))
	{
	  /* No route yet, try and acquire one. */
	  ro->ro_dst.sin6_family = AF_INET6;  /* Is ro blanked out? */
	  ro->ro_dst.sin6_len = sizeof(struct sockaddr_in6);
	  ro->ro_dst.sin6_addr = sin6->sin6_addr;
	  /* 
	   *  Need to wipe out the flowlabel for ifa_ifwith*
	   *  but don't need to for rtalloc.
	   */
	  rtalloc((struct route *)ro);
	}

      if (ro->ro_rt == NULL)
	{
	  /*
	   * No route of any kind, so spray neighbor solicits out all
	   * interfaces, unless it's a multicast address.
	   */
	  if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr))
	    return ENETUNREACH;
	  ipv6_onlink_query((struct sockaddr_in6 *)&ro->ro_dst);
	  rtalloc((struct route *)ro);
	}
      if (ro->ro_rt == NULL)
	{
	  /*
	   * ipv6_onlink_query() should've added a route.  It probably
	   * failed.
	   */
	  DPRINTF(GROSSEVENT, ("v6_output: onlink_query didn't add route!\n"));
	  return ENETUNREACH;
	}

      if (ro->ro_rt->rt_ifa == NULL)
	{
	  /*
	   * We have a route where we don't quite know which interface 
	   * the neighbor belongs to yet.  If I get here, I know that this
	   * route is not pre-allocated (such as done by in6_pcbconnect()),
	   * because those pre-allocators will do the same ipv6_onlink_query()
	   * and ipv6_verify_onlink() in advance.
	   *
	   * I can therefore free the route, and get it again.
	   */
	  int error;

	  RTFREE(ro->ro_rt);
	  ro->ro_rt = NULL;
	  switch (error = ipv6_verify_onlink(&ro->ro_dst))
	    {
	    case -1:
	      return ENETUNREACH;
	    case 0:
	      break;
	    default:
	      return error;
	    }
	  rtalloc((struct route *)ro);
	  if (ro->ro_rt == NULL || ro->ro_rt->rt_ifa == NULL)
	    panic("Oops2, I'm forgetting something after verify_onlink().");
	}


      /*
       * If we found a route, use the address
       * corresponding to the outgoing interface
       * unless it is the loopback (in case a route
       * to our address on another net goes to loopback).
       *
       * This code may be simplified if the above gyrations work.
       */
      if (ro->ro_rt && ro->ro_rt->rt_ifp &&
		!(ro->ro_rt->rt_ifp->if_flags & IFF_LOOPBACK))
	i6a = ifatoi6a(ro->ro_rt->rt_ifa);
      if (i6a == NULL)
	{
	  u_short fport = sin6->sin6_port;

	  /*
	   * Source address selection when there is no route.
	   *
	   * If ND out all if's is to be used, this is the point to do it.
	   * (Similar to when the route lookup fails in ipv6_output.c, and
	   * the 'on-link' assumption kicks in.)
	   *
	   * For multicast, use i6a of mcastdef.
	   */

	  sin6->sin6_port = 0;
	  i6a = ifatoi6a(ifa_ifwithdstaddr(sin6tosa(sin6)));
	  if (i6a == NULL)
	    i6a = ifatoi6a(ifa_ifwithnet(sin6tosa(sin6)));
	  sin6->sin6_port = fport;
	  if (i6a == NULL)
	    {
	      struct in6_ifaddr *ti6a = NULL;
	      
	      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
		{
		  /* Find first (non-local if possible) address for
		     source usage.  If multiple locals, use last one found. */
		  if (IN6_IS_ADDR_LINKLOCAL(&I6A_SIN(i6a)->sin6_addr))
		    ti6a=i6a;
		  else if (!IN6_IS_ADDR_LOOPBACK(&I6A_SIN(i6a)->sin6_addr))
		    break;
		}
	      if (i6a == NULL && ti6a != NULL)
		i6a = ti6a;
	    }
	  if (i6a == NULL)
	    return EADDRNOTAVAIL;
	}

      /*
       * If I'm "connecting" to a multicast address, gyrate properly to
       * get a source address based upon the user-requested mcast interface.
       */
      if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) && inp->inp_moptions6 != NULL &&
	  (inp->inp_flags & INP_IPV6_MCAST))
	{
	  struct ipv6_moptions *i6mo;
	  struct ifnet *ifp;
	  
	  i6mo = inp->inp_moptions6;
	  if (i6mo->i6mo_multicast_ifp != NULL)
	    {
	      ifp = i6mo->i6mo_multicast_ifp;
	      for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
		if (i6a->i6a_ifp == ifp)  /* Linkloc vs. global? */
		  break;
	      if (i6a == NULL)
		return EADDRNOTAVAIL;
	    }
	}

      ifaddr = (struct sockaddr_in6 *)&i6a->i6a_addr;
    }

#if __FreeBSD__
  if (in_pcblookup_hash(inp->inp_pcbinfo,
    ((IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) ?
    	(struct in_addr *)&ifaddr->sin6_addr :
	(struct in_addr *)&inp->inp_laddr6),
    	sin6->sin6_port, (struct in_addr *)&sin6->sin6_addr,
	inp->inp_lport, INPLOOKUP_IPV6)) return EADDRINUSE;
#else /* __FreeBSD__ */
#if __NetBSD__
  if (in6_pcblookup_connect(inp->inp_table,
    ((IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) ? &ifaddr->sin6_addr :
    &inp->inp_laddr6), sin6->sin6_port, &sin6->sin6_addr, inp->inp_lport))
     return EADDRINUSE;
#else /* __NetBSD__ */
#ifdef __OpenBSD__
  if (in_pcblookup(inp->inp_table, ((IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) ?
#else /* __OpenBSD__ */
  if (in_pcblookup(inp->inp_head, ((IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) ?
#endif /* __OpenBSD__ */
    	(struct in_addr *)&ifaddr->sin6_addr :
	(struct in_addr *)&inp->inp_laddr6),
    	sin6->sin6_port, (struct in_addr *)&sin6->sin6_addr,
	inp->inp_lport, INPLOOKUP_IPV6)) return EADDRINUSE;
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */

  if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
    {
      if (inp->inp_lport == 0)
	(void)in6_pcbbind(inp, NULL);  /* To find free port & bind to it. */
      inp->inp_laddr6 = ifaddr->sin6_addr;
    }
  inp->inp_faddr6 = sin6->sin6_addr;
  inp->inp_fport = sin6->sin6_port;

  /*
   * Assumes user specify flowinfo in network order.
   */
  inp->inp_ipv6.ipv6_versfl = htonl(0x60000000) | 
    (sin6->sin6_flowinfo & htonl(0x0fffffff));

#if __NetBSD__
  in_pcbstate(inp, INP_CONNECTED);
#endif /* __NetBSD__ */
  
#if __FreeBSD__ 
  /*
   * See pcbbind near the end for reasoning on setting this for a value to 
   * hash on. 
   */
#if 0
  inp->inp_laddr.s_addr = inp->inp_laddr6.in6a_words[3]
                        ^ inp->inp_laddr6.in6a_words[1];
#endif /* 0 */
  in_pcbrehash(inp);
#endif /* __FreeBSD__ */
  return 0;
}

/*----------------------------------------------------------------------
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Also perform input-side security policy check 
 *    once PCB to be notified has been located.
 *
 * Must be called at splnet.
 ----------------------------------------------------------------------*/

int
#ifdef IPSEC
in6_pcbnotify(head, dst, fport_arg, la, lport_arg, cmd, notify, m, nexthdr)
#else /* IPSEC */
in6_pcbnotify(head, dst, fport_arg, la, lport_arg, cmd, notify)
#endif /* IPSEC */
#if __NetBSD__ || __OpenBSD__
     struct inpcbtable *head;
#else /* __NetBSD__ || __OpenBSD__ */
#if __FreeBSD__ 
     struct inpcbhead *head;
#else /* __FreeBSD__ */
     struct inpcb *head;
#endif /* __FreeBSD__ */
#endif /* __NetBSD__ || __OpenBSD__ */
     struct sockaddr *dst;
     uint fport_arg;
     struct in6_addr *la;
     uint lport_arg;
     int cmd;
     void (*notify) __P((struct inpcb *, int));
#ifdef IPSEC
     struct mbuf *m;
     int nexthdr;
#endif /* IPSEC */
{
  register struct inpcb *inp, *oinp;
  struct in6_addr *faddr,laddr = *la;
  u_short fport = fport_arg, lport = lport_arg;
  int errno;
#ifdef IPSEC
  struct sockaddr_in6 srcsa, dstsa;
#endif /* IPSEC */

  DPRINTF(IDL_EVENT,("Entering in6_pcbnotify.  head = 0x%lx, dst is\n", (unsigned long)head));
  DDO(IDL_EVENT,dump_smart_sockaddr(dst));
  DPRINTF(IDL_EVENT,("fport_arg = %d, lport_arg = %d, cmd = %d\n",\
			   fport_arg, lport_arg, cmd));
  DDO(IDL_EVENT,printf("la is ");dump_in6_addr(la));

  if ((unsigned)cmd > PRC_NCMDS || dst->sa_family != AF_INET6)
    return 1;
  faddr = &(((struct sockaddr_in6 *)dst)->sin6_addr);
  if (IN6_IS_ADDR_UNSPECIFIED(faddr))
    return 1;
  if (IN6_IS_ADDR_V4MAPPED(faddr))
    {
      printf("Huh?  Thought in6_pcbnotify() never got called with mapped!\n");
    }
  
  /*
   * Redirects go to all references to the destination,
   * and use in_rtchange to invalidate the route cache.
   * Dead host indications: notify all references to the destination.
   * Otherwise, if we have knowledge of the local port and address,
   * deliver only to that socket.
   */

  if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD)
    {
      fport = 0;
      lport = 0;
      laddr = in6addr_any;
      if (cmd != PRC_HOSTDEAD)
	notify = in_rtchange;
    }
  errno = inet6ctlerrmap[cmd];

#ifdef IPSEC
  /* Build these again because args aren't necessarily correctly formed
     sockaddrs, and the policy code will eventually need that. -cmetz */

  memset(&srcsa, 0, sizeof(struct sockaddr_in6));
  srcsa.sin6_len = sizeof(struct sockaddr_in6);
  srcsa.sin6_family = AF_INET6;
  /* defer port */
  srcsa.sin6_addr = *faddr;

  memset(&dstsa, 0, sizeof(struct sockaddr_in6));
  dstsa.sin6_len = sizeof(struct sockaddr_in6);
  dstsa.sin6_family = AF_INET6;
  /* defer port */
  dstsa.sin6_addr = laddr;
#endif /* IPSEC */

#if __NetBSD__ || __OpenBSD__
  for (inp = head->inpt_queue.cqh_first;
       inp != (struct inpcb *)&head->inpt_queue;)
#else /* __NetBSD__ || __OpenBSD__ */
#if __FreeBSD__
  for (inp = head->lh_first; inp != NULL;)
#else /* __FreeBSD__ */
  for (inp = head->inp_next; inp != head;)
#endif /* __FreeBSD__ */
#endif /* __NetBSD__ || __OpenBSD__ */
    {
      if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, faddr) ||
	  !inp->inp_socket ||
	  (lport && inp->inp_lport != lport) ||
	  (!IN6_IS_ADDR_UNSPECIFIED(&laddr) && !IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, &laddr)) ||
	  (fport && inp->inp_fport != fport))
	{
#if __NetBSD__ || __OpenBSD__
	  inp = inp->inp_queue.cqe_next;
#else /* __NetBSD__ || __OpenBSD__ */
#if __FreeBSD__
	  inp = inp->inp_list.le_next;
#else /* __FreeBSD__ */
	  inp = inp->inp_next;
#endif /* __FreeBSD__ */
#endif /* __NetBSD__ || __OpenBSD__ */
	  continue;
	}
      oinp = inp;

#if __NetBSD__ || __OpenBSD__
      inp = inp->inp_queue.cqe_next;
#else /* __NetBSD__ || __OpenBSD__ */
#if __FreeBSD__
      inp = inp->inp_list.le_next;
#else /* __FreeBSD__ */
      inp = inp->inp_next;
#endif /* __FreeBSD__ */
#endif /* __NetBSD__ || __OpenBSD__ */

      if (notify)
#ifdef IPSEC
	{
	  /* Pretend the packet came in for this source/destination port pair,
	     since that's what we care about for policy. If the passed in
	     fport and/or lport were nonzero, the comparison will make sure
	     they match that of the PCB and the right thing will happen.
	     -cmetz */
	  srcsa.sin6_port = oinp->inp_fport;
	  dstsa.sin6_port = oinp->inp_lport;

	  /* XXX - state arg should not be NULL */
	  if (!netproc_inputpolicy(oinp->inp_socket, (struct sockaddr *)&srcsa,
			          (struct sockaddr *)&dstsa, nexthdr, m, NULL,
				   NULL))
#endif /* IPSEC */
	  (*notify)(oinp, errno);
#ifdef IPSEC
	}
#endif /* IPSEC */
    }
   return 0;
}

/*----------------------------------------------------------------------
 * Get the local address/port, and put it in a sockaddr_in6.
 * This services the getsockname(2) call.
 ----------------------------------------------------------------------*/

int
in6_setsockaddr(inp, nam)
     register struct inpcb *inp;
#if __FreeBSD__
     struct sockaddr **nam; 
#else /* __FreeBSD__ */
     struct mbuf *nam;
#endif /* __FreeBSD__ */
{
  register struct sockaddr_in6 *sin6;


#if __FreeBSD__ 
  /*
   * In FreeBSD we have to allocate the sockaddr_in6 structure since we aren't
   * given an mbuf, but a sockaddr ** as the second argument. (i.e. a location
   * to reference the space this routine allocates.  
   */
  if (!(sin6 = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6),M_SONAME, M_DONTWAIT)))
  	return ENOBUFS; /* XXX */

#else /* __FreeBSD__ */

  nam->m_len = sizeof(struct sockaddr_in6);
  sin6 = mtod(nam,struct sockaddr_in6 *);

#endif /* __FreeBSD__ */

  bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
  sin6->sin6_family = AF_INET6;
  sin6->sin6_len = sizeof(struct sockaddr_in6);
  sin6->sin6_port = inp->inp_lport;
  sin6->sin6_addr = inp->inp_laddr6;

#if __FreeBSD__
  *nam = (struct sockaddr *) sin6;
#endif /* __FreeBSD__ */

  return 0;
}

/*----------------------------------------------------------------------
 * Get the foreign address/port, and put it in a sockaddr_in6.
 * This services the getpeername(2) call.
 ----------------------------------------------------------------------*/

int
in6_setpeeraddr(inp, nam)
     register struct inpcb *inp;
#if __FreeBSD__
     struct sockaddr **nam; 
#else /* __FreeBSD__ */
     struct mbuf *nam;
#endif /* __FreeBSD__ */
{
  register struct sockaddr_in6 *sin6;

#if __FreeBSD__ 
  /*
   * In FreeBSD we have to allocate the sockaddr_in6 structure since we aren't
   * given an mbuf, but a sockaddr ** as the second argument. (i.e. a location
   * to reference the space this routine allocates.  
   */
  if (!(sin6 = (struct sockaddr_in6 *)malloc(sizeof(struct sockaddr_in6),M_SONAME, M_DONTWAIT)))
  	return ENOBUFS; /* XXX */

#else /* __FreeBSD__ */

  nam->m_len = sizeof(struct sockaddr_in6);
  sin6 = mtod(nam,struct sockaddr_in6 *);

#endif /* __FreeBSD__ */

  bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
  sin6->sin6_family = AF_INET6;
  sin6->sin6_len = sizeof(struct sockaddr_in6);
  sin6->sin6_port = inp->inp_fport;
  sin6->sin6_addr = inp->inp_faddr6;
  sin6->sin6_flowinfo = inp->inp_fflowinfo;

#if __FreeBSD__
  *nam = (struct sockaddr *) sin6;
#endif /* __FreeBSD__ */

  return 0;
}
