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

#include <sys/param.h>
#include <sys/systm.h>
/*#include <sys/ioctl.h> */
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>
#include <netinet6/ipv6_addrconf.h>

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
 * External Globals
 */

extern struct in6_ifaddr *in6_ifaddr;
extern u_long v6d_retranstime;
extern int ipv6rsolicit;

void send_nsolicit __P((struct rtentry *, struct ifnet *, struct in6_addr *, int));
struct mbuf *get_discov_cluster __P((void));

static void send_rsolicit __P((struct ifnet *));

/*----------------------------------------------------------------------
 * Initialize addrconf.
 ----------------------------------------------------------------------*/

void
addrconf_init()
{
  timeout(addrconf_timer,NULL,v6d_retranstime * hz);
}

/*----------------------------------------------------------------------
 * Send a router solicitation out a certain interface.
 ----------------------------------------------------------------------*/

static void
send_rsolicit(ifp)
     struct ifnet *ifp;
{
  struct mbuf *solicit = NULL;
  struct ipv6 *header;
  struct ipv6_icmp *icmp;
  struct ipv6_moptions i6mo,*i6mop = NULL;
  struct in6_ifaddr *i6a;

  if ((solicit = get_discov_cluster()) == NULL)
    {
      DPRINTF(IDL_ERROR, ("Can't allocate mbuf in send_gsolicit().\n"));
      return;
    }
  header = mtod(solicit,struct ipv6 *);
  icmp = (struct ipv6_icmp *)(header + 1);/* I want the bytes after the hdr. */

  bzero(&i6mo,sizeof(struct ipv6_moptions));
  i6mo.i6mo_multicast_ifp = ifp;
  i6mo.i6mo_multicast_ttl = 255;
  i6mop = &i6mo;
  /* Find source link-local or use unspec. */
  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
    if (i6a->i6a_ifp == ifp && (i6a->i6a_addrflags & I6AF_LINKLOC) &&
	!(i6a->i6a_addrflags & I6AF_NOTSURE))
      break;

  if (i6a == NULL)
    header->ipv6_src = in6addr_any;
  else
    header->ipv6_src = i6a->i6a_addr.sin6_addr;
  
  header->ipv6_dst.in6a_words[0] = htonl(0xff020000);
  header->ipv6_dst.in6a_words[1] = 0;
  header->ipv6_dst.in6a_words[2] = 0;
  header->ipv6_dst.in6a_words[3] = htonl(2);

  header->ipv6_versfl = htonl(0x6f000000);
  header->ipv6_hoplimit = 255;  /* Guaranteed to be intra-link if arrives with
				   255. */
  header->ipv6_nexthdr = IPPROTO_ICMPV6;
  header->ipv6_length = ICMPV6_RSOLMINLEN;  /* For now. */
  icmp->icmp_type = ICMPV6_ROUTERSOL;
  icmp->icmp_code = 0;
  icmp->icmp_unused = 0;
  icmp->icmp_cksum = 0;

  /*
   * For now, just let ND run its normal course and don't include the link
   * extension.
   */

  solicit->m_len = solicit->m_pkthdr.len = 
                       header->ipv6_length + sizeof(*header);
  icmp->icmp_cksum = in6_cksum(solicit,IPPROTO_ICMPV6,header->ipv6_length,
			       sizeof(struct ipv6));
  /*
   * NOTE: The solicit mbuf chain will have a NULL instead of a valid
   * socket ptr.  When ipv6_output() calls ipsec_output_policy(),
   * this socket ptr will STILL be NULL.  Sooo, the security
   * policy on outbound packets from here will == system security
   * level (set in ipsec_init() of netinet6/ipsec.c).  If your
   * system security level is paranoid, then you won't move packets
   * unless you have _preloaded_ keys for at least the ND addresses. 
   *  - danmcd rja
   */
  ipv6_output(solicit, NULL, IPV6_RAWOUTPUT, i6mop, NULL, NULL);
}

/*----------------------------------------------------------------------
 * Scan list if in6_ifaddrs and see if any are expired (or can go to
 * being unique).
 ----------------------------------------------------------------------*/
/* XXX - This function doesn't appear to ever actually remove addresses... */
void
addrconf_timer(whocares)
     void *whocares;
{
  struct in6_ifaddr *i6a = in6_ifaddr;
  int s = splnet();

  while (i6a != NULL)
    {
      /*
       * Scan address list for all sorts of neat stuff.  Also, in6_ifaddr
       * may be a "prefix list" as well.  This will be difficult when
       * an router advert. advertises an on-link prefix, but I don't have
       * (for whatever reason) an address on that link.
       */
#ifdef __FreeBSD__
      if (i6a->i6a_preferred && i6a->i6a_preferred <= time_second)
#else /* __FreeBSD__ */
      if (i6a->i6a_preferred && i6a->i6a_preferred <= time.tv_sec)
#endif /* __FreeBSD__ */
	{
	  i6a->i6a_addrflags |= I6AF_DEPRECATED;
	  DPRINTF(IDL_EVENT,("Address has been deprecated.\n"));
	}
#ifdef __FreeBSD__
      if (i6a->i6a_expire && i6a->i6a_expire <= time_second) {
#else /* __FreeBSD__ */
      if (i6a->i6a_expire && i6a->i6a_expire <= time.tv_sec) {
#endif /* __FreeBSD__ */
	if (i6a->i6a_addrflags & I6AF_NOTSURE) {
	  DPRINTF(IDL_FINISHED, ("Address appears to be unique.\n"));
	  i6a->i6a_addrflags &= ~I6AF_NOTSURE;
	  /*
	   * From what I can tell, addrs that survive DAD are
	   * permanent.  I won't mark as permanent, but I will zero
	   * expiration for now.
	   *
	   * If this is a link-local address, it may be a good idea to
	   * send a router solicit.  (But only if I'm a host.)
	   */
	  if (ipv6rsolicit && IN6_IS_ADDR_LINKLOCAL(&i6a->i6a_addr.sin6_addr))
	    send_rsolicit(i6a->i6a_ifp);

	  i6a->i6a_expire = 0;
	  i6a->i6a_preferred = 0;
	} else {
	  /*
	   * Do address deletion, and nuke any routes, pcb's, etc.
	   * that use this address.
	   *
	   * As an implementation note, it's probably more likely than
	   * not that addresses that get deprecated (see above) will be
	   * moved off the master list, as that keeps them away from
	   * some things.  This is something we couldn't implement in time,
	   * however.
	   */
	}
      }

      i6a = i6a->i6a_next;
    }
  timeout(addrconf_timer,NULL,v6d_retranstime * hz);
  splx(s);
}

/*----------------------------------------------------------------------
 * Send multicast solicit for this address from all 0's.  Set timer such
 * that if address is still in in6_ifaddr list, it's good.
 ----------------------------------------------------------------------*/

void
addrconf_dad(i6a)
     struct in6_ifaddr *i6a;
{
  int s;
  struct rtentry dummy;

  DPRINTF(IDL_GROSS_EVENT,("Sending DAD solicit!\n"));
  s = splnet();
  rt_key(&dummy) = i6a->i6a_ifa.ifa_addr;
  dummy.rt_gateway = NULL;
  /*
   * Set i6a flags and expirations such that it is NOT SURE about uniqueness.
   *
   * What about random delay?
   */
  i6a->i6a_addrflags |= I6AF_NOTSURE;  /* Might be done already. */
  i6a->i6a_preferred = 0;
#ifdef __FreeBSD__
  i6a->i6a_expire = time_second + v6d_retranstime;
#else /* __FreeBSD__ */
  i6a->i6a_expire = time.tv_sec + v6d_retranstime;
#endif /* __FreeBSD__ */
  splx(s);
  /*
   * It would be nice if I delayed a random amount of time here.
   */
  send_nsolicit(&dummy, i6a->i6a_ifp, (struct in6_addr *)&in6addr_any, 1);
}
