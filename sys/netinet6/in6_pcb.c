/* $OpenBSD: in6_pcb.c,v 1.9 2000/02/07 06:09:10 itojun Exp $ */

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

/*
 * External globals
 */

#include <dev/rndvar.h>

extern struct in6_ifaddr *in6_ifaddr;
extern struct in_ifaddr *in_ifaddr;

/*
 * Globals
 */

struct in6_addr zeroin6_addr;

extern int ipport_firstauto;
extern int ipport_lastauto;
extern int ipport_hifirstauto;
extern int ipport_hilastauto;

/*
 * Keep separate inet6ctlerrmap, because I may remap some of these.
 * I also put it here, because, quite frankly, it belongs here, not in
 * ip{v6,}_input().
 */
#if 0
u_char inet6ctlerrmap[PRC_NCMDS] = {
        0,              0,              0,              0,
        0,              EMSGSIZE,       EHOSTDOWN,      EHOSTUNREACH,
        EHOSTUNREACH,   EHOSTUNREACH,   ECONNREFUSED,   ECONNREFUSED,
        EMSGSIZE,       EHOSTUNREACH,   0,              0,
        0,              0,              0,              0,
        ENOPROTOOPT
};
#endif

/*----------------------------------------------------------------------
 * Bind an address (or at least a port) to an PF_INET6 socket.
 ----------------------------------------------------------------------*/
int
in6_pcbbind(inp, nam)
     register struct inpcb *inp;
     struct mbuf *nam;
{
  register struct socket *so = inp->inp_socket;

  register struct inpcbtable *head = inp->inp_table;
  register struct sockaddr_in6 *sin6;
  struct proc *p = curproc;		/* XXX */
  u_short lport = 0;
  int wild = INPLOOKUP_IPV6, reuseport = (so->so_options & SO_REUSEPORT);
  int error;

  /*
   * REMINDER:  Once up to speed, flow label processing should go here,
   *            too.  (Same with in6_pcbconnect.)  
   */

  if (in6_ifaddr == 0 || in_ifaddr == 0)
    return EADDRNOTAVAIL;

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
      sin6 = mtod(nam, struct sockaddr_in6 *);
      if (nam->m_len != sizeof (*sin6))
	return EINVAL;

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
	  sin.sin_addr.s_addr = sin6->sin6_addr.s6_addr32[3];
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
	      la.s_addr = sin6->sin6_addr.s6_addr32[3];
	      wild &= ~INPLOOKUP_IPV6;

	      t = in_pcblookup(head, (struct in_addr *)&fa, 0,
			       (struct in_addr *)&la, lport, wild);
	    }
	  else
	    {
	      t = in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&sin6->sin6_addr, lport, wild);
	    }

	  if (t && (reuseport & t->inp_socket->so_options) == 0)
	    return EADDRINUSE;
	}
      inp->inp_laddr6 = sin6->sin6_addr;

      if (!IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
	inp->inp_ipv6.ip6_flow = htonl(0x60000000) | 
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
      if (inp->inp_laddr6.s6_addr32[0] == 0 && 
	  inp->inp_laddr6.s6_addr32[1] == 0)
	{
	  if (inp->inp_laddr6.s6_addr32[2] == ntohl(0xffff))
	    inp->inp_flags |= INP_IPV6_MAPPED;
	  if (inp->inp_laddr6.s6_addr32[2] == 0 &&
	      inp->inp_laddr6.s6_addr32[3] == 0)
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

    lastport = &inp->inp_table->inpt_lastport;

    if (inp->inp_flags & INP_IPV6_MAPPED) {
      la.s_addr = inp->inp_laddr6.s6_addr32[3];
      fa.s_addr = 0;
      wild &= ~INPLOOKUP_IPV6;
    };

    if (inp->inp_flags & INP_HIGHPORT) {
      first = ipport_hifirstauto;	/* sysctl */
      last = ipport_hilastauto;
    } else if (inp->inp_flags & INP_LOWPORT) {
      if ((error = suser(p->p_ucred, &p->p_acflag)))
	return (EACCES);

      first = IPPORT_RESERVED-1; /* 1023 */
      last = 600;		   /* not IPPORT_RESERVED/2 */
    } else {
      first = ipport_firstauto;	/* sysctl */
      last  = ipport_lastauto;
    }

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
	first -= (arc4random() % (first - last));
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
      } while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
	       ((wild & INPLOOKUP_IPV6) ?
		in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup(head, (struct in_addr *)&fa, 0,
			(struct in_addr *)&la, lport, wild)));
    } else {
      /*
       * counting up
       */
      if (loopcount == 0) {	/* only do this once. */
	old = first;
	first += (arc4random() % (last - first));
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
      } while (in_baddynamic(*lastport, so->so_proto->pr_protocol) ||
	       ((wild & INPLOOKUP_IPV6) ?
		in_pcblookup(head, (struct in_addr *)&zeroin6_addr, 0,
			(struct in_addr *)&inp->inp_laddr6, lport, wild) :
		in_pcblookup(head, (struct in_addr *)&fa, 0,
			(struct in_addr *)&la, lport, wild)));
    }
  }

  inp->inp_lport = lport;

  /* XXX hash */
  return 0;
}

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
	struct mbuf *nam;
{
	struct in6_addr *in6a = NULL;
	struct sockaddr_in6 *sin6 = mtod(nam, struct sockaddr_in6 *);
	struct in6_pktinfo *pi;
	struct ifnet *ifp = NULL;	/* outgoing interface */
	int error = 0;
	struct in6_addr mapped;

	(void)&in6a;				/* XXX fool gcc */

	if (nam->m_len != sizeof(*sin6))
		return(EINVAL);
	if (sin6->sin6_family != AF_INET6)
		return(EAFNOSUPPORT);
	if (sin6->sin6_port == 0)
		return(EADDRNOTAVAIL);

	/* sanity check for mapped address case */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
			inp->inp_laddr6.s6_addr16[5] = htons(0xffff);
		if (!IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6))
			return EINVAL;
	} else {
		if (IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6))
			return EINVAL;
	}

	/*
	 * If the scope of the destination is link-local, embed the interface
	 * index in the address.
	 */
	if (IN6_IS_SCOPE_LINKLOCAL(&sin6->sin6_addr)) {
		/* XXX boundary check is assumed to be already done. */
		/* XXX sin6_scope_id is weaker than advanced-api. */
		if (inp->inp_outputopts6 &&
		    (pi = inp->inp_outputopts6->ip6po_pktinfo) &&
		    pi->ipi6_ifindex) {
			sin6->sin6_addr.s6_addr16[1] = htons(pi->ipi6_ifindex);
			ifp = ifindex2ifnet[pi->ipi6_ifindex];
		}
		else if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr) &&
			 inp->inp_moptions6 &&
			 inp->inp_moptions6->im6o_multicast_ifp) {
			sin6->sin6_addr.s6_addr16[1] =
				htons(inp->inp_moptions6->im6o_multicast_ifp->if_index);
			ifp = ifindex2ifnet[inp->inp_moptions6->im6o_multicast_ifp->if_index];
		} else if (sin6->sin6_scope_id) {
			/* boundary check */
			if (sin6->sin6_scope_id < 0 
			 || if_index < sin6->sin6_scope_id) {
				return ENXIO;  /* XXX EINVAL? */
			}
			sin6->sin6_addr.s6_addr16[1]
				= htons(sin6->sin6_scope_id & 0xffff);/*XXX*/
			ifp = ifindex2ifnet[sin6->sin6_scope_id];
		}
	}

	/* Source address selection. */
	if (IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6)
	 && inp->inp_laddr6.s6_addr32[3] == 0) {
		struct sockaddr_in sin, *sinp;

		bzero(&sin, sizeof(sin));
		sin.sin_len = sizeof(sin);
		sin.sin_family = AF_INET;
		bcopy(&sin6->sin6_addr.s6_addr32[3], &sin.sin_addr,
			sizeof(sin.sin_addr));
		sinp = in_selectsrc(&sin, (struct route *)&inp->inp_route6,
			inp->inp_socket->so_options, NULL, &error);
		if (sinp == 0) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return(error);
		}
		bzero(&mapped, sizeof(mapped));
		mapped.s6_addr16[5] = htons(0xffff);
		bcopy(&sinp->sin_addr, &mapped.s6_addr32[3], sizeof(sinp->sin_addr));
		in6a = &mapped;
	} else {
		/*
		 * XXX: in6_selectsrc might replace the bound local address
		 * with the address specified by setsockopt(IPV6_PKTINFO).
		 * Is it the intended behavior?
		 */
		in6a = in6_selectsrc(sin6, inp->inp_outputopts6,
				     inp->inp_moptions6,
				     &inp->inp_route6,
				     &inp->inp_laddr6, &error);
		if (in6a == 0) {
			if (error == 0)
				error = EADDRNOTAVAIL;
			return(error);
		}
	}
	if (inp->inp_route6.ro_rt)
		ifp = inp->inp_route6.ro_rt->rt_ifp;

	inp->inp_ipv6.ip6_hlim = (u_int8_t)in6_selecthlim(inp, ifp);

	if (in_pcblookup(inp->inp_table,
			 &sin6->sin6_addr,
			 sin6->sin6_port,
			 IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) ?
			  in6a : &inp->inp_laddr6,
			 inp->inp_lport,
			 INPLOOKUP_IPV6))
		return(EADDRINUSE);
	if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)
	 || (IN6_IS_ADDR_V4MAPPED(&inp->inp_laddr6)
	  && inp->inp_laddr6.s6_addr32[3] == 0)) {
		if (inp->inp_lport == 0)
			(void)in6_pcbbind(inp, (struct mbuf *)0);
		inp->inp_laddr6 = *in6a;
	}
	inp->inp_faddr6 = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	/*
	 * xxx kazu flowlabel is necessary for connect?
	 * but if this line is missing, the garbage value remains.
	 */
	inp->inp_ipv6.ip6_flow = sin6->sin6_flowinfo;
	/* configure NRL flags properly */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
		inp->inp_flags |= INP_IPV6_MAPPED;
		inp->inp_flags &= ~INP_IPV6_UNDEC;
	}
	in_pcbrehash(inp);
	return(0);
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
in6_pcbnotify(head, dst, fport_arg, la, lport_arg, cmd, notify)
     struct inpcbtable *head;
     struct sockaddr *dst;
     uint fport_arg;
     struct in6_addr *la;
     uint lport_arg;
     int cmd;
     void (*notify) __P((struct inpcb *, int));
{
  register struct inpcb *inp, *oinp;
  struct in6_addr *faddr,laddr = *la;
  u_short fport = fport_arg, lport = lport_arg;
  int errno;

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

  for (inp = head->inpt_queue.cqh_first;
       inp != (struct inpcb *)&head->inpt_queue;)
    {
      if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, faddr) ||
	  !inp->inp_socket ||
	  (lport && inp->inp_lport != lport) ||
	  (!IN6_IS_ADDR_UNSPECIFIED(&laddr) && !IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, &laddr)) ||
	  (fport && inp->inp_fport != fport))
	{
	  inp = inp->inp_queue.cqe_next;
	  continue;
	}
      oinp = inp;

      inp = inp->inp_queue.cqe_next;
      if (notify)
	  (*notify)(oinp, errno);
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
     struct mbuf *nam;
{
  register struct sockaddr_in6 *sin6;

  nam->m_len = sizeof(struct sockaddr_in6);
  sin6 = mtod(nam,struct sockaddr_in6 *);

  bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
  sin6->sin6_family = AF_INET6;
  sin6->sin6_len = sizeof(struct sockaddr_in6);
  sin6->sin6_port = inp->inp_lport;
  sin6->sin6_addr = inp->inp_laddr6;
  return 0;
}

/*----------------------------------------------------------------------
 * Get the foreign address/port, and put it in a sockaddr_in6.
 * This services the getpeername(2) call.
 ----------------------------------------------------------------------*/

int
in6_setpeeraddr(inp, nam)
     register struct inpcb *inp;
     struct mbuf *nam;
{
  register struct sockaddr_in6 *sin6;

  nam->m_len = sizeof(struct sockaddr_in6);
  sin6 = mtod(nam,struct sockaddr_in6 *);

  bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
  sin6->sin6_family = AF_INET6;
  sin6->sin6_len = sizeof(struct sockaddr_in6);
  sin6->sin6_port = inp->inp_fport;
  sin6->sin6_addr = inp->inp_faddr6;
  sin6->sin6_flowinfo = inp->inp_fflowinfo;
  return 0;
}
