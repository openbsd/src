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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>
#ifdef __FreeBSD__
#include <net/netisr.h>
#endif /* __FreeBSD__ */

#include <netinet/in.h>

#include <netinet6/in6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ipv6.h>
#include <netinet6/ipv6_var.h>
#include <netinet6/ipv6_icmp.h>
#include <netinet6/ipv6_addrconf.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>

#if __OpenBSD__ && defined(NRL_IPSEC)
#define IPSEC 1
#endif /* __OpenBSD__ && defined(NRL_IPSEC) */

#ifdef IPSEC
#include <sys/osdep.h>
#include <net/netproc.h>
#include <net/netproc_var.h>

#include <netsec/ipsec.h>
#endif /* IPSEC */

#if __FreeBSD__
#include <sys/sysctl.h>
#endif /* __FreeBSD__ */

#ifdef DEBUG_NRL_SYS
#include <sys/debug.h>
#endif /* DEBUG_NRL_SYS */
#ifdef DEBUG_NRL_NETINET6
#include <netinet6/debug.h>
#endif /* DEBUG_NRL_NETINET6 */

/*
 * Globals
 */

u_char ipv6_protox[IPPROTO_MAX];       /* For easy demuxing to HLP's. */
struct ipv6stat ipv6stat;              /* Stats. */
struct in6_ifaddr *in6_ifaddr = NULL;  /* List of IPv6 addresses. */
struct in6_ifnet *in6_ifnet = NULL;    /* List of IPv6 interfaces. */
struct ipv6_fragment *ipv6_fragmentq = NULL;   /* Fragment reassembly queue */
struct ifqueue ipv6intrq;
struct route6 ipv6forward_rt;

/*
 * External globals
 */

extern int ipv6forwarding;	  /* See in6_proto.c */
extern int ipv6rsolicit;	  /* See in6_proto.c */
extern int ipv6_defhoplmt;	  /* See in6_proto.c */
extern int ipv6qmaxlen;           /* See in6_proto.c */
extern struct protosw inet6sw[];  /* See in6_proto.c */
extern struct domain inet6domain; /* See in6_proto.c */
extern struct discq dqhead;       /* See ipv6_discovery.c */

/*
 * Funct. prototypes.
 */

void ipv6_forward __P((struct mbuf *));
void ipv6_discovery_init __P((void));
extern int ipv6_clean_nexthop __P((struct radix_node *,void *));
#if !__FreeBSD__ 
int sysctl_int __P((void *, size_t *, void *, size_t, int *));
/* For reasons somewhat unknown, <sys/mbuf.h> doesn't prototype this */
struct mbuf *m_split __P((register struct mbuf *, int, int));
#endif /* !__FreeBSD__ */

void ipv6intr __P((void));
int ipv6_enabled __P((struct ifnet *));

static struct mbuf *ipv6_saveopt(caddr_t p, int size, int type, int level);
#if 0
static struct mbuf *ipv6_savebag(struct mbuf *m, int level);
#endif /* 0 */

/*----------------------------------------------------------------------
 * IPv6 initialization function.
 ----------------------------------------------------------------------*/

void
ipv6_init()
{
  register struct protosw *pr;
  register int i;

  DPRINTF(GROSSEVENT,("IPv6 initializing..."));

  bzero(&ipv6stat,sizeof(struct ipv6stat));

  pr = pffindproto(PF_INET6, IPPROTO_RAW, SOCK_RAW);
  if (pr == 0)
    panic("ipv6_init");  

  /*
   * To call the right IPv6 next header function off next header type, have
   * next header numbers index the protocol switch, like protocols in IP
   * (default the switch for this).  Otherwise, just switch off into normal
   * (TCP,UDP) stuff.
   *
   * Initialize ipv6_protox[].
   */

  for (i = 0; i < IPPROTO_MAX; i++)
    ipv6_protox[i] = pr - inet6sw;
  for (pr = inet6domain.dom_protosw;
       pr < inet6domain.dom_protoswNPROTOSW; pr++)
    if (pr->pr_domain->dom_family == PF_INET6
        && pr->pr_protocol != IPPROTO_RAW )
      ipv6_protox[pr->pr_protocol] = pr - inet6sw;

  /*
   * Initialize discovery stuff.
   */

  ipv6_discovery_init();

  /*
   * Initialize addrconf stuff.
   */

  addrconf_init();

  /*
   * Initialize IPv6 i/f queue.
   */

  ipv6intrq.ifq_maxlen = ipv6qmaxlen;

#if 0 /* defined(INET6) && defined(IPSEC) */
  /*
   * Initialise IPsec
   */
  ipsec_init();
#endif /* defined(INET6) && defined(IPSEC) */

  DPRINTF(GROSSEVENT,("...done\n"));
}

/*----------------------------------------------------------------------
 * IPv6 input queue interrupt handler.
 ----------------------------------------------------------------------*/

void 
ipv6intr()
{
  struct mbuf *m;
  int s;

  while (1)   /* Keep yanking off packets until I hit... */
    {
      s = splimp();
      IF_DEQUEUE(&ipv6intrq, m);
      splx(s);
      
      if (m == NULL)
      {
	return;       /* ...HERE.  THIS is how I exit this endless loop. */
      }

#if 0 /* def IPSEC */
      m->m_flags &= ~(M_AUTHENTIC | M_DECRYPTED);
#endif /* IPSEC */
      
      ipv6_input(m, 0);
    }
}

#if __FreeBSD__ 
NETISR_SET(NETISR_IPV6, ipv6intr);
#endif /* __FreeBSD__ */

/*----------------------------------------------------------------------
 * Actual inbound (up the protocol graph from the device to the user)
 * IPv6 processing.
 ----------------------------------------------------------------------*/

#if __OpenBSD__
void
#if __STDC__
ipv6_input(struct mbuf *incoming, ...)
#else /* __STDC__ */
ipv6_input(incoming, va_alist)
	struct mbuf *m;
	va_dcl
#endif /* __STDC__ */
#else /* __OpenBSD__ */
void
ipv6_input(incoming, extra)
     struct mbuf *incoming;
     int extra;
#endif /* __OpenBSD__ */
{
  struct ipv6 *header;
  struct in6_ifaddr *i6a = NULL;
  struct in6_multi *in6m = NULL;
  int jumbogram = 0;
  uint8_t nexthdr;
  int payload_len;
#if __OpenBSD__
  va_list ap;
  int extra;
#endif /* __OpenBSD__ */

  DPRINTF(GROSSEVENT,("ipv6_input(struct mbuf *incoming=%08lx, int extra=%x)\n", (unsigned long)incoming, extra));
  DPRINTF(IDL_FINISHED,("incoming->m_data = %08lx, & 3 = %lx\n", (unsigned long)incoming->m_data, (unsigned long)incoming->m_data & 3));

#if __OpenBSD__
  va_start(ap, incoming);
  extra = va_arg(ap, int);
  va_end(ap);
#endif /* __OpenBSD__ */

  /*
   * Can't do anything until at least an interface is marked as
   * ready for IPv6.
   */

  if (in6_ifaddr == NULL)
    {
      m_freem(incoming);
      return;
    }

  ipv6stat.ips_total++;

  /*
   * IPv6 inside something else.  Discard encapsulating header(s) and
   * (maybe try to) make it look like virgin IPv6 packet.
   *
   * As with other stupid mbuf tricks, there probably is a better way.
   */

  if (extra) {
      struct mbuf *newpacket;

      DP(FINISHED, extra, d);

#if IPSEC
	/* Perform input-side policy check. Drop packet if policy says to drop
	   it.

	   Note that the gist of this check is that every decapsulation
	   requires a trip to input policy. For packets that end up locally,
	   this is probably bad. For packets that go off-host, this is probably
	   good.

	   Right now, we err on the side of security and always check. -cmetz*/
	{
	  struct sockaddr_in6 srcsa, dstsa;

	  bzero(&srcsa, sizeof(struct sockaddr_in6));
	  srcsa.sin6_family = AF_INET6;
	  srcsa.sin6_len = sizeof(struct sockaddr_in6);
	  srcsa.sin6_addr = mtod(incoming, struct ipv6 *)->ipv6_src;

	  bzero(&dstsa, sizeof(struct sockaddr_in6));
	  dstsa.sin6_family = AF_INET6;
	  dstsa.sin6_len = sizeof(struct sockaddr_in6);
	  dstsa.sin6_addr = mtod(incoming, struct ipv6 *)->ipv6_dst;

	  /* XXX - state arg should NOT be NULL, it should be the netproc state
	     carried up the stack - cmetz */
	  if (netproc_inputpolicy(NULL, (struct sockaddr *)&srcsa,
				  (struct sockaddr *)&dstsa, IPPROTO_IPV6,
				  incoming, NULL, NULL)) {
	    m_freem(incoming);
	    return;
	  }
	}
#endif /* IPSEC */

      /*
       * Split packet into what I need, and what was encapsulating what I
       * need.  Discard the encapsulating portion.
       */

      if (!(newpacket = m_split(incoming, extra, M_DONTWAIT))) {
	  printf("ipv6_input() couldn't trim extra off.\n");
	  m_freem(incoming);
	  return;
      }
      newpacket->m_flags |= incoming->m_flags;
      m_freem(incoming);
      incoming = newpacket;
      extra = 0;
  }

  /*
   * Even before preparsing (see vitriolic comments later), 
   * I need to have the whole IPv6 header at my disposal.
   */


  if (incoming->m_len < sizeof (struct ipv6) &&
      (incoming = m_pullup(incoming, sizeof(struct ipv6))) == NULL)
      {
	ipv6stat.ips_toosmall++;
	return;
      }

  /*
   * Check version bits immediately.
   */

  if ((incoming->m_data[0] >> 4) != IPV6VERSION)
    {
      ipv6stat.ips_badvers++;
      m_freem(incoming);
      return;
    }

  header = mtod(incoming, struct ipv6 *);

  DDO(IDL_FINISHED,dump_ipv6(header));

  /*
   * Save off payload_len because of munging later...
   */
  payload_len = ntohs(header->ipv6_length);

  /*
   * Check that the amount of data in the buffers
   * is as at least much as the IPv6 header would have us expect.
   * Trim mbufs if longer than we expect.
   * Drop packet if shorter than we expect.
   */

  if (incoming->m_pkthdr.len < payload_len + sizeof(struct ipv6))
    {
      ipv6stat.ips_tooshort++;
      m_freem(incoming);
      return;
    }

  if (incoming->m_pkthdr.len > payload_len + sizeof(struct ipv6)) {
    if (!payload_len) {
      jumbogram = 1;  /* We might have a jumbogram here! */
    } else {
      if (incoming->m_len == incoming->m_pkthdr.len) {
	incoming->m_len = payload_len + sizeof(struct ipv6);
	incoming->m_pkthdr.len = payload_len + sizeof(struct ipv6);
      } else {
         m_adj(incoming,
	       (payload_len + sizeof(struct ipv6)) - incoming->m_pkthdr.len );
      }
    }
  }

  /*
   * See if it's for me by checking list of i6a's.  I may want to convert
   * this into a routing lookup and see if rt->rt_ifp is loopback.
   * (Might be quicker. :)
   */

  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
    if (IN6_ARE_ADDR_EQUAL(&I6A_SIN(i6a)->sin6_addr, &header->ipv6_dst))
      break;
  /*
   * Check for inbound multicast datagram is for us.
   */

  if (i6a == NULL && IN6_IS_ADDR_MULTICAST(&header->ipv6_dst))
    {
#ifdef MROUTING
      /*
       * Do IPv6 mcast routing stuff here.  This means even if I'm in the
       * m-cast group, I may have to forward the packet too.  Hence, I put
       * the m-cast routing stuff HERE.
       */
#endif
      DPRINTF(IDL_EVENT,("In multicast case.  Looking up..."));
      IN6_LOOKUP_MULTI(&header->ipv6_dst,incoming->m_pkthdr.rcvif,in6m);
      if (!in6m) {
	DPRINTF(IDL_EVENT,("mcast lookup failed.\n"));
	DDO(IDL_EVENT, dump_ipv6(header));
	ipv6stat.ips_cantforward++;
	m_freem(incoming);
	return;
      }
      DPRINTF(IDL_EVENT,("succeeded.\n"));
    }

  /* If this datagram is for me, I'll either have one of my addresses in
     i6a, or one of my multicast groups in in6m.   From the code given
     above, the condition (in6m && i6a) is always false. */

  if (i6a || in6m)
    {
      if (i6a) {     
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
        /* Don't increment stats for encapsulated pkt */
	if (!extra) {
          i6a->i6a_ifa.ifa_ipackets++;
          i6a->i6a_ifa.ifa_ibytes += incoming->m_pkthdr.len;
	};
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */

        if (i6a->i6a_addrflags & I6AF_NOTSURE) {
	  /*
	   * Don't want to unicast receive packets for me unless I know this 
	   * is a verified unique address.
	   *
	   * Multicast packets will still come in and be handled.
	   */
	  m_freem(incoming);
	  return;
	}
      };

      nexthdr = (mtod(incoming, struct ipv6 *))->ipv6_nexthdr;

      DPRINTF(IDL_EVENT, ("nexthdr = %d.\n", nexthdr));

      DPRINTF(IDL_EVENT, ("Calling protoswitch for %d\n", nexthdr));

      ipv6stat.ips_delivered++;
      (*inet6sw[ipv6_protox[nexthdr]].pr_input)
	(incoming, extra + sizeof(struct ipv6));

      DPRINTF(IDL_EVENT, ("Back from protoswitch for %d\n", nexthdr));
      return;
    }

  DDO(IDL_EVENT,printf("Might forward for dest: ");\
      dump_in6_addr(&header->ipv6_dst););

  if (ipv6forwarding == 0)
    {
      DPRINTF(IDL_ERROR,("ipv6_input: Would call ipv6_forward if on.\n"));
      ipv6stat.ips_cantforward++;
      m_freem(incoming);
      return;
    }
  else
    {
      /*
       * Perform hop-by-hop options, if present.  Dest opt. bags, and source
       * routes SHOULD be handled by previous protosw call.
       */
      DPRINTF(IDL_FINISHED,("Calling ipv6_forward.\n"));
      ipv6_forward(incoming);
    }
}

/*----------------------------------------------------------------------
 * IPv6 reassembly code.  Returns an mbuf chain if the fragment completes
 * a message, otherwise it returns NULL.
 *
 * Assumptions: 
 * * The IPv6 header is at the beginning of the incoming structure 
 *   and has already been pulled up. I don't expect this to be
 *   a problem.
 * * Fragments don't overlap (or, if they do, we can discard them).
 *   They can be duplicates (which we drop), but they cannot overlap.
 *   If we have IPv4->IPv6 header translation, this assumption could
 *   be incorrect because IPv4 has intermediate fragmentation (which
 *   you have to have in order for fragments to overlap). This is yet
 *   another reason why header translation is a Bad Thing.
 * * The slowtimeo()-based routine that frees fragments will not
 *   get called during the middle of this routine. This assumption
 *   seems to be made in the IPv4 reassembly code. How this is so
 *   actually so I have yet to discover. Requiring splnet() or some
 *   sort of resource arbitration to handle this assumption being
 *   incorrect could be a major hassle.
 *
 * Other comments:
 *
 * * m_split(), a valuable call that gets no press, does much magic here.
 * * Security against denial-of-service attacks on reassembly cannot be
 *   provided.
 * * frag_id is more or less used as a 32-bit cookie. We don't need to
 *   htonl() it, because it's either equal or it's not, no matter what
 *   byte order you use.
 ----------------------------------------------------------------------*/

void
ipv6_reasm(incoming, extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6_fraghdr *fraghdr;
  struct ipv6_fragment *fragment, **pfragment;

  if (incoming->m_len < extra + sizeof(struct ipv6_fraghdr))
    if (!(incoming = m_pullup(incoming, extra + sizeof(struct ipv6_fraghdr))))
      return;

  DP(FINISHED, OSDEP_PCAST(incoming), 08x);

  fraghdr = (struct ipv6_fraghdr *)(mtod(incoming, caddr_t) + extra);
  fraghdr->frag_bitsoffset = htons(fraghdr->frag_bitsoffset);

  /*
   * Locate reassembly queue for incoming fragment.
   */
  {
    struct ipv6 *ipv6i, *ipv6f;

    ipv6i = mtod(incoming, struct ipv6 *);

    for (pfragment = &ipv6_fragmentq; *pfragment;
	 pfragment = &((*pfragment)->next)) {

      ipv6f = mtod((*pfragment)->prefix, struct ipv6 *);

      if (IN6_ARE_ADDR_EQUAL(&ipv6i->ipv6_src, &ipv6f->ipv6_src)) {
	if (IN6_ARE_ADDR_EQUAL(&ipv6i->ipv6_dst, &ipv6f->ipv6_dst)) {
	  register struct ipv6_fraghdr *fhf = mtod((*pfragment)->data,
						   struct ipv6_fraghdr *);

	  if ((fraghdr->frag_id == fhf->frag_id) &&
	      (fraghdr->frag_nexthdr == fhf->frag_nexthdr)) {
	    break;
	  }
	}
      }
    }
  }

  fragment = *pfragment;

  if (!fragment) {
    /* 
     * Create a new fragment queue entry - this one looks new.
     *
     * Notice that the order of insertion is such that the newest queues
     * are at the head of the one-way list.  The entry aging code takes
     * advantage of this.
     */
    MALLOC(fragment, struct ipv6_fragment *, sizeof(*fragment), M_FRAGQ,
	   M_NOWAIT);
    if (!fragment) {
      DPRINTF(IDL_ERROR, ("MALLOC fragment queue entry failed.\n"));
      goto reasm_cleanup;
    };
    fragment->ttl = 60; /* 30 seconds */
    if (!(fragment->data = m_split(incoming, extra, M_DONTWAIT))) {
      free(fragment, M_FRAGQ); 
      goto reasm_cleanup;
    }

    fragment->prefix = incoming;

    DP(FINISHED, OSDEP_PCAST(fragment->prefix), 08x);
    DP(FINISHED, OSDEP_PCAST(fragment->data), 08x);

    fragment->flags = (~fraghdr->frag_bitsoffset) & 1;

    fragment->next = ipv6_fragmentq;
    ipv6_fragmentq = fragment;
    return;
  }

  /*
   * If two packets have claimed to be the beginning or the end, we don't
   * know which is right. The easiest solution is to drop this packet.
   */

  if (!(fraghdr->frag_bitsoffset & 1)) {
    if (fragment->flags & 1)  /* i.e. we already have the end... */
      {
	/*
	 * Duplicate (ending) fragment.
	 */
	DPRINTF(IDL_FINISHED, ("Got a dupe/overlap fragment"));
	goto reasm_cleanup;
      }
    else
      fragment->flags |= 1;
  }

  if (!(fraghdr->frag_bitsoffset & 0xFFF8)) {
    /*
     * We want to end up with the part before the frag header for the packet
     * at offset zero. (RFC1123?)
     */
    if (!(mtod(fragment->data, struct ipv6_fraghdr *)->frag_bitsoffset & 0xFFF8)) {
      /*
       * Duplicate (initial) packet.
       */
      DPRINTF(IDL_FINISHED, ("Got a dupe/overlap fragment"));
      goto reasm_cleanup;
    } else {
      struct mbuf *mbuf;

      DP(FINISHED, OSDEP_PCAST(fragment->prefix), 08x);
      DP(FINISHED, OSDEP_PCAST(fragment->data), 08x);

      m_freem(fragment->prefix);

      /* Save everything before the frag header */
      if (!(mbuf = m_split(incoming, extra, M_DONTWAIT))) {
	/* should probably toss whole fragment queue */
	m_freem(incoming);
	return;
      }

      fragment->prefix = incoming;
      incoming = mbuf;

      DP(FINISHED, OSDEP_PCAST(fragment->prefix), 08x);
      DP(FINISHED, OSDEP_PCAST(fragment->data), 08x);
    }
  } else
    m_adj(incoming, extra); /* Discard everything before the frag header */

  {
    struct mbuf *hm[3];
    int i;

    {
      struct mbuf *m, *pm;

      /*
       * Find where this fragment fits in.
       */
      for (pm = NULL, m = fragment->data; m; pm = m, m = m->m_nextpkt) {
	if (((mtod(m, struct ipv6_fraghdr *))->frag_bitsoffset & 0xFFF8) > 
	    (fraghdr->frag_bitsoffset & 0xFFF8)) {
	  break;
	}

	if ((mtod(m, struct ipv6_fraghdr *))->frag_bitsoffset ==
	    fraghdr->frag_bitsoffset) {
	  /*
	   * Duplicate fragment.
	   */
	  DPRINTF(IDL_FINISHED, ("Got a dupe/overlap fragment"));
	  goto reasm_cleanup;
	}
      }

      /*
       * Right here, pm will contain the preceeding fragment to the incoming
       * one, and m will contain the succeeding fragment to the incoming one.
       *
       * This is somewhat non-obvious. hm[] is a vector of pointers to the
       * mbufs containing ipv6_fraghdrs and dm[] is a vector of pointers to
       * the mbufs at the head of each of their associated data lists.  [0]
       * is the mbuf in the main chain to the immediate left of where our
       * new data should go, [1] is our new data, and [2] is the mbuf in
       * the main chain to the immediate right of where our new data should
       * go. One of [0] or [2] may be NULL.
       *
       * Each dm[n]->m_nextpkt will have that fragment's length stored.
       *
       * The reason why we do this is so we can bubble together the [0] and
       * [1] elements if possible, make the [1] = [0] if we do, then we can
       * bubble the [1] and the [2] together and it'll do the right thing.
       * We could theoretically do this for the rest of the list except that
       * it is made deliberately unecessary (we bubble on insertion until we
       * have a known-done big bubble so we don't have to do a O(N/2)
       * rescan of the list every time just to figure out whether or not
       * we're done.
       *
       * This seems really ugly, but it does the job and it may even be
       * somewhat efficient. 
       */
    
      hm[0] = pm;
      hm[1] = incoming;
      hm[2] = m;

      if (!pm) {
	incoming->m_nextpkt = fragment->data;
	fragment->data = incoming;
      } else {
	pm->m_nextpkt = incoming;
	incoming->m_nextpkt = m;
      }
    }

    for (i = 0; i < 2; i++) {
      if (!hm[i] || !hm[i+1]) {
	DP(FINISHED, i, d);
	continue;
      }

      if ((((mtod(hm[i], struct ipv6_fraghdr *))->frag_bitsoffset 
	    & 0xFFF8) + hm[i]->m_pkthdr.len - sizeof(struct ipv6_fraghdr)) >
	  ((mtod(hm[i+1], struct ipv6_fraghdr *))->frag_bitsoffset 
	   & 0xFFF8)) {
	/*
	 * Overlapping fragment.
	 */
	DPRINTF(IDL_FINISHED, ("Got a dupe/overlap fragment"));
	goto reasm_cleanup;
      }
      
      if ((((mtod(hm[i], struct ipv6_fraghdr *))->frag_bitsoffset
	    & 0xFFF8) + hm[i]->m_pkthdr.len - sizeof(struct ipv6_fraghdr)) ==
	  ((mtod(hm[i+1], struct ipv6_fraghdr *))->frag_bitsoffset 
	   & 0xFFF8)) {
	/*
	 * If the fragments are contiguous, bubble them together:
	 * Combine the data chains and increase the appropriate
	 * chain data length. The second fragment header is now
	 * unnecessary.
	 */
	if (!((mtod(hm[i+1], struct ipv6_fraghdr *))->frag_bitsoffset & 1)){
	  (mtod(hm[i], struct ipv6_fraghdr *))->frag_bitsoffset &= (~1);
	}

	DDO(FINISHED, dump_mchain(hm[i]));
	DDO(FINISHED, dump_mchain(hm[i+1]));

	/* Trim the second fragment header */
	m_adj(hm[i+1], sizeof(struct ipv6_fraghdr));
	/* Append the second fragment's data */
	m_cat(hm[i], hm[i+1]);
	/* And update the first fragment's length */
        hm[i]->m_pkthdr.len += hm[i+1]->m_pkthdr.len;

	hm[i]->m_nextpkt = hm[i+1]->m_nextpkt;

	DDO(FINISHED, dump_mchain(hm[i]));

	/* Hack to make the bubble happen with the m found above if we
	   just bubbled with the pm found above */
	if (!i)
	  hm[i+1] = hm[i];
      }
    }
  }

  /*
   * Now, the moment of truth. Do we have a complete packet? 
   * To be done, we have to have only one packet left in the queue now that
   * we've bubbled together (i.e., one complete packet), have it at offset
   * zero (i.e., there's nothing before it), and not have its more bit set
   * (i.e., there's nothing after it). If we meet these conditions, we are
   * DONE! 
   *
   * Remember when we htons()ed frag_bitsoffset? If we're done, it contains
   * a zero. I don't know of any architecture in which a zero in network
   * byte order isn't a zero in host byte order, do you? 
   */

  if (!fragment->data->m_nextpkt &&
      !(mtod(fragment->data, struct ipv6_fraghdr *))->frag_bitsoffset) {
    uint8_t nexthdr = mtod(fragment->data, struct ipv6_fraghdr *)->frag_nexthdr;
    
    if (*pfragment)
      *pfragment = fragment->next;

    /*
     * Pain time.
     * 
     * The fragmentation header must be removed. This requires me to rescan
     * prefix, going through each header until I figure out where the last
     * header before the fragmentation header is. Then I set that header's
     * next header to the fragmentation header's next header.
     *
     * N.B. that this means people adding new random header processing code
     * to this IPv6 implementation need to make the appropriate mods below.
     * Failure to do so will really hose you if your header appears before
     * a fragment header. It is a Good Thing to mod the code below even if
     * you don't *think* it will ever appear before a fragment header, just
     * because it *could*.
     *
     * This is really annoying and somewhat expensive. On the other hand,
     * it might prove itself to be yet another reason for higher level
     * protocols to work at avoiding fragmentation where possible.
     */

    /*
      Since we start out by doing a m_pullup() of extra + sizeof(struct
      ipv6_fraghdr, we can treat this as a straight linear buffer.
      
      This could easily be implemented as a fast and slow path, but
      reassembly is an inherently slow path anyway. -cmetz
    */

    DDO(FINISHED, dump_mchain(fragment->prefix));
    DDO(FINISHED, dump_mchain(fragment->data));

    {
      caddr_t data;
      uint8_t *type;

      DDO(FINISHED, dump_ipv6(mtod(fragment->prefix, struct ipv6 *)));

      data = mtod(fragment->prefix, caddr_t) + sizeof(struct ipv6);
      type = &(mtod(fragment->prefix, struct ipv6 *)->ipv6_nexthdr);

      while(*type != IPPROTO_FRAGMENT) {
	switch(*type) {
	  case IPPROTO_HOPOPTS:
	  case IPPROTO_DSTOPTS:
	    {
	      struct ipv6_opthdr *ipv6_opthdr = (struct ipv6_opthdr *)data;
	      
	      type = &(ipv6_opthdr->oh_nexthdr);
	      data += sizeof(struct ipv6_opthdr) +
		ipv6_opthdr->oh_extlen * sizeof(uint64_t);
	    };
	    break;
	  case IPPROTO_ROUTING:
	    {
	      struct ipv6_srcroute0 *ipv6_srcroute0 =
		(struct ipv6_srcroute0 *)data;
	      
	      type = &(ipv6_srcroute0->i6sr_nexthdr);
	      data += sizeof(struct ipv6_srcroute0) +
		ipv6_srcroute0->i6sr_len * sizeof(uint64_t);
	    };
	    break;
	  default:
	    DPRINTF(ERROR, ("ipv6_reasm: Received a header (%d) that isn't allowed before a fragment header", *type));

	    DP(FINISHED, OSDEP_PCAST(fragment->data), 08x);
	    DP(FINISHED, OSDEP_PCAST(fragment->prefix), 08x);

	    m_freem(fragment->data);
	    m_freem(fragment->prefix);
	    FREE(fragment, M_FRAGQ);

	    return;
	}
      }
      *type = nexthdr;
    }

    extra = fragment->prefix->m_pkthdr.len;

    incoming = fragment->prefix;
    m_adj(fragment->data, sizeof(struct ipv6_fraghdr));
    m_cat(incoming, fragment->data);
    incoming->m_pkthdr.len += fragment->data->m_pkthdr.len;

    FREE(fragment, M_FRAGQ);

    /* Can't reassemble into a jumbogram */
    if (incoming->m_pkthdr.len > 0xffff) {
      m_freem(incoming);
      return; /* no other cleanup needed */
    }

    /* Dummy up length */
    (mtod(incoming, struct ipv6 *))->ipv6_length = 
      htons(incoming->m_pkthdr.len - sizeof(struct ipv6));

    DDO(FINISHED, dump_mchain(incoming));

    (*inet6sw[ipv6_protox[nexthdr]].pr_input)(incoming, extra);
  }
  return;

reasm_cleanup:
  if (incoming)
    m_freem(incoming);

  return;
};

/*----------------------------------------------------------------------
 * IPv6 forwarding engine.
 *
 * Assumes IPv6 header is already pulled up and ready-to-read.
 ----------------------------------------------------------------------*/

void
ipv6_forward(outbound)
     struct mbuf *outbound;
{
  struct ipv6 *ipv6 = mtod(outbound, struct ipv6 *);
  struct sockaddr_in6 *sin6;
  struct rtentry *rt;
  struct mbuf *ocopy;
  int type = 0, code = 0, pptr = 0, error;

  /*
   * Check hop limit.
   */

  if (ipv6->ipv6_hoplimit <= 1)
    {
      ipv6_icmp_error(outbound,ICMPV6_TIMXCEED,ICMPV6_TIMXCEED_INTRANS,0);
      return;
    }

  /*
   * Check link-local nature of source and dest.  (Thanks to rja!)
   */
  if (IN6_IS_ADDR_LINKLOCAL(&ipv6->ipv6_src) || IN6_IS_ADDR_LINKLOCAL(&ipv6->ipv6_dst))
    {
      printf("Can't forward packet with link-locals in it!\n");
      m_freem(outbound);
      return;
    }
  ipv6->ipv6_hoplimit--;
  sin6 = &ipv6forward_rt.ro_dst;
  if ((rt = ipv6forward_rt.ro_rt) == NULL ||
      !IN6_ARE_ADDR_EQUAL(&ipv6->ipv6_dst, &sin6->sin6_addr))
    {
      if (ipv6forward_rt.ro_rt != NULL)
	{
	  RTFREE(ipv6forward_rt.ro_rt);
	  ipv6forward_rt.ro_rt = NULL;
	}
      sin6->sin6_family = AF_INET6;
      sin6->sin6_len = sizeof(*sin6);
      sin6->sin6_addr = ipv6->ipv6_dst;

      rtalloc_noclone((struct route *)&ipv6forward_rt,ONNET_CLONING);
      if (ipv6forward_rt.ro_rt == NULL)
	{
	  ipv6_icmp_error(outbound,ICMPV6_UNREACH, ICMPV6_UNREACH_NOROUTE, 0);
	  return;
	}
      rt = ipv6forward_rt.ro_rt;
    }

  /*
   * Save 576 bytes of the packet in case we need to generate and ICMPv6
   * message to the sender.
   */

  ocopy = m_copy(outbound,0, min(ntohs(ipv6->ipv6_length),IPV6_MINMTU));

  /*
   * ip_forward() keeps some statistics here if GATEWAY is defined.  We
   * skip that for now.
   */

  if (rt->rt_ifp == outbound->m_pkthdr.rcvif && rt_mask(rt)
      && rt_mask(rt)->sa_len 
      && (rt->rt_flags & (RTF_TUNNEL|RTF_DYNAMIC|RTF_MODIFIED)) == 0 )
    {
      DPRINTF(IDL_EVENT, ("WARNING:  May be cause for a redirect in ipv6_forward().\n"));
    }

  /*
   * Perform hop-by-hop options since we're ready to go!
   */

  if (!ipv6->ipv6_nexthdr)
    {
      /*
       * If bad hop-by-hop, return.
       */
      printf("Hop-by-hop options present in packet to be forwarded!\n");
    }

  error = ipv6_output(outbound, &ipv6forward_rt, IPV6_FORWARDING, NULL, NULL, NULL);

  if (error)
    ipv6stat.ips_cantforward++;
  else
    {
      /*
       * Check for redirect flags that should've been set in the redirect
       * code, otherwise...
       */
      m_freem(ocopy);
      return;
    }

  switch (error)
    {
    case 0:     /* type and code should've been set by redirect code. */
      break;

    case ENETUNREACH:    /* These two should've been checked above. */
    case EHOSTUNREACH:
      type = ICMPV6_UNREACH;
      code = ICMPV6_UNREACH_NOROUTE;
      break;

    case EHOSTDOWN:
      type = ICMPV6_UNREACH;
      code = ICMPV6_UNREACH_ADDRESS;
      break;

    case EMSGSIZE:
      type = ICMPV6_TOOBIG;
      code = 0;
      ipv6stat.ips_cantfrag++;
      pptr = rt->rt_rmx.rmx_mtu;
      break;
    }

  ipv6_icmp_error(ocopy, type, code, pptr);
}

/*----------------------------------------------------------------------
 * IPv6 hop-by-hop option handler.
 ----------------------------------------------------------------------*/

void
ipv6_hop(incoming, extra)
     struct mbuf *incoming;
     int extra;
{
  struct ipv6 *header;
  struct ipv6_opthdr *hopopt;
  struct ipv6_option *option;
  uint8_t *tmp;

  if (incoming->m_len < extra + sizeof(struct ipv6_opthdr))
    if (!(incoming = m_pullup2(incoming, extra + sizeof(struct ipv6_opthdr))))
      return;

  hopopt = (struct ipv6_opthdr *)(mtod(incoming, caddr_t) + extra);

  if (incoming->m_len < extra + sizeof(struct ipv6_opthdr) +
      (hopopt->oh_extlen * sizeof(uint64_t))) {
    if (!(incoming = m_pullup2(incoming, extra + sizeof(struct ipv6_opthdr) +
			       (hopopt->oh_extlen * sizeof(uint64_t)))))
      return;

    hopopt = (struct ipv6_opthdr *)(mtod(incoming, caddr_t) + extra);
  }

  header = mtod(incoming, struct ipv6 *);

  tmp = hopopt->oh_data;
  /*
   * Slide the char pointer tmp along, parsing each option in the "bag" of
   * hop-by-hop options.
   */
  while (tmp < hopopt->oh_data + (hopopt->oh_extlen << 2))
    {
      option = (struct ipv6_option *)tmp;
      switch (option->opt_type)
	{
	case OPT_PAD1:
	  tmp++;
	  break;
	case OPT_PADN:
	  tmp += option->opt_datalen + 2;
	  break;
	case OPT_JUMBO:
	  tmp += 2;
	  /*
	   * Confirm that the packet header field matches the jumbogram size.
	   */
	  if (incoming->m_pkthdr.len != ntohl(*(uint32_t *)tmp) + sizeof(*header))
	    {
	      /*
	       * For now, bail.  Add instrumenting code here, too.
	       */
	      m_freem(incoming);
	      return;
	    }
	  break;
	default:
	  /*
	   * Handle unknown option by taking appropriate action based on
	   * high bit values.  With this code, this first non-skipping
	   * unknown option will cause the packet to be dropped.
	   */
	  switch (option->opt_type & 0xC0)
	    {
	    case 0:    /* Skip over */
	      tmp += option->opt_datalen + 2;
	      break;
	    case 0xC0: /* Only if not multicast... */
	      if (IN6_IS_ADDR_MULTICAST(&header->ipv6_dst))
		/* FALLTHROUGH */
	    case 0x80: /* Send ICMP Unrecognized type. */
		{
		  /* Issue ICMP parameter problem. */
		  ipv6_icmp_error(incoming,ICMPV6_PARAMPROB,
		   ICMPV6_PARAMPROB_BADOPT,
#ifdef	__alpha__
		   (u_long)option - (u_long)hopopt->oh_data + 
#else
		   (uint32_t)option - (uint32_t)hopopt->oh_data + 
#endif
				       sizeof(struct ipv6));
		}
	      return;  /* incoming has already been freed. */

	    case 0x40: /* Discard packet */
	      m_freem(incoming);
	      return;
	    }
	}
    }

  DPRINTF(GROSSEVENT, ("ipv6_hop calling protoswitch for %d\n", \
		       hopopt->oh_nexthdr));

  (*inet6sw[ipv6_protox[hopopt->oh_nexthdr]].pr_input)
        (incoming, extra + sizeof(struct ipv6_opthdr) + 
	(hopopt->oh_extlen * sizeof(uint64_t)));

  DPRINTF(GROSSEVENT, ("Leaving ipv6_hop\n"));
}


/*----------------------------------------------------------------------
 * If timers expires on reassembly queues, discard them.
 * Also update any discovery queues.
 ----------------------------------------------------------------------*/

void
ipv6_slowtimo()
{
  struct ipv6_fragment *fragment, *fragmentprev;
  int s = splnet();

  /*
   * Age reasssembly fragments.
   *
   * (Since fragments are inserted at the beginning of the queue, once we've
   * found the first timed-out fragment, we know that everything beyond is
   * also timed-out since it must be older.)
   *
   */
  for (fragmentprev = NULL, fragment = ipv6_fragmentq; fragment; 
       fragmentprev = fragment, fragment = fragment->next)
    if (fragment->ttl <= 1)
      break;
    else
      fragment->ttl--;

  if (fragment) {
      struct mbuf *m, *m2;

      if (fragmentprev)
	fragmentprev->next = NULL;
      else
	ipv6_fragmentq = NULL;
      
      /*
       * This loop does most of the work and doesn't require splnet()...?
       */
      splx(s);

      while(fragment) {
	/*
	 * We "should" (says the spec) send an ICMP time exceeded here.
	 * However, among other headaches, we may not actually have a copy
	 * of the real packet sent to us (if we bubbled, we now have a frag
	 * header that never really came from the sender). The solution taken
	 * for now is to continue the BSD tradition of not bothering to send
	 * these messages.
	 */
	m = fragment->data;
	ipv6stat.ips_fragtimeout++;
	while(m) {
	  m2 = m;
	  m = m->m_nextpkt;
	  m_freem(m2);
	}
	m_freem(fragment->prefix);
	fragmentprev = fragment;
	fragment = fragment->next;
	FREE(fragmentprev, M_FRAGQ);
      }
    } else
      splx(s);
}

/*----------------------------------------------------------------------
 * Drain all fragments & possibly discovery structures.
 ----------------------------------------------------------------------*/

void
ipv6_drain()
{
  struct ipv6_fragment *totrash;
  struct mbuf *m,*m2;
  struct radix_node_head *rnh = rt_tables[AF_INET6];

  while (ipv6_fragmentq != NULL)
    {
      ipv6stat.ips_fragdropped++;
      totrash = ipv6_fragmentq;
      ipv6_fragmentq = totrash->next;
      m = totrash->data;
      while (m)
	{
	  m2 = m;
	  m = m->m_nextpkt;
	  m_freem(m2);
	}
      if (totrash->prefix)
	m_freem(totrash->prefix);
      FREE(totrash, M_FRAGQ);
    }

  /*
   * Might want to delete all off-net host routes, 
   *     and maybe even on-net host routes.  
   *
   * For now, do only the off-net host routes.
   */
  (void) rnh->rnh_walktree(rnh, ipv6_clean_nexthop, (void *)1);
}

/*----------------------------------------------------------------------
 * sysctl(2) handler for IPv6. Not yet implemented.
 ----------------------------------------------------------------------*/
int *ipv6_sysvars[] = IPV6CTL_VARS;

#if __FreeBSD__ 
SYSCTL_STRUCT(_net_inet_ipv6, IPV6CTL_STATS, ipv6stats, CTLFLAG_RD, &ipv6stat, ipv6stat, "");
#else /* __FreeBSD__ */
int
ipv6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	uint namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
  if (namelen != 1)
    return ENOTDIR;

  switch (name[0])
    {
#if defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802)
    case IPV6CTL_STATS:
      return sysctl_rdtrunc(oldp, oldlenp, newp, &ipv6stat, sizeof(ipv6stat));
    default:
      return (sysctl_int_arr(ipv6_sysvars, name, namelen, oldp, oldlenp, newp, newlen));
#else /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
    default:
      return EOPNOTSUPP;
#endif /* defined(_BSDI_VERSION) && (_BSDI_VERSION >= 199802) */
    }
}
#endif __FreeBSD__ 

/*----------------------------------------------------------------------
 * Should be a macro, this function determines if IPv6 is running on a
 * given interface.
 ----------------------------------------------------------------------*/

int
ipv6_enabled(ifp)
     struct ifnet *ifp;
{
  struct in6_ifaddr *i6a;

  for (i6a = in6_ifaddr; i6a; i6a = i6a->i6a_next)
    if (i6a->i6a_ifp == ifp)
      return 1;

  return 0;
}

/*----------------------------------------------------------------------
 * Strips IPv6 options for TCP or UDP.
 *
 * This function assumes that the input chain (incoming) has been
 * munged by ipv6_preparse() and starts with an IPv6 header.
 * The header index is invalid after this call. 
 * The IPv6 header is not updated EXCEPT for the length, which must be
 * in HOST order.
 * Note that the API for this function is NOT THE SAME as its IPv4
 * counterpart.
 *
 * Often called like:
 *
 * ipv6_stripoptions(incoming, extra, nexthdr);
 *
 * Where ihi and ihioff are the header index arrays, passed up after
 * ipv6_preparse().  If preparse is eliminated or altered, this code will
 * be too.
 ----------------------------------------------------------------------*/

void ipv6_stripoptions(incoming, extra)
register struct mbuf *incoming;
int extra;
{
  struct mbuf *optm, *m;
  int optlen;

  if (extra == sizeof(struct ipv6)) {
    /* i.e. If there are no options... */
    optm = NULL;
    optlen = 0;
    return;
  }

  if (!(optm = m_split(incoming, sizeof(struct ipv6), M_DONTWAIT)))
    return;

  if (!(m = m_split(optm, extra - sizeof(struct ipv6), M_DONTWAIT))) {
    m_cat(incoming, optm);
    return;
  }

  m_cat(incoming, m);
  incoming->m_pkthdr.len -= (extra - sizeof(struct ipv6));

  (mtod(incoming, struct ipv6 *))->ipv6_length = 
    (incoming->m_pkthdr.len - sizeof(struct ipv6) > 0xffff) ? 0 :
    htons(incoming->m_pkthdr.len - sizeof(struct ipv6));

  /*
   * XXX - We should be saving these stripped options somewhere...
   */
  m_freem(optm);
}

static struct mbuf *ipv6_saveopt(caddr_t p, int size, int type, int level)
{
  register struct cmsghdr *cp;
  struct mbuf *m;
  
  if ((m = m_get(M_DONTWAIT, MT_CONTROL)) == NULL)
    return ((struct mbuf *) NULL);
  cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
  bcopy(p, CMSG_DATA(cp), size);
  size += sizeof(*cp);
  m->m_len = size;
  cp->cmsg_len = size;
  cp->cmsg_level = level;
  cp->cmsg_type = type;
  return m;
}

#if 0
static struct mbuf *ipv6_savebag(struct mbuf *m, int level)
{
  uint8_t *p = mtod(m, uint8_t *) + 1;
  int len = (*(p++) << 3) + 6;
  struct mbuf *opts = NULL, **mp = &opts;

  while(len > 0) {
    if (!*p) {
      p++; len--;
      continue;
    }

    if (len <= 1)
      return opts;

    if (*p != 1)
      if ((*mp = ipv6_saveopt((caddr_t)(p + 2), *(p + 1), *p, level)))
	mp = &(*mp)->m_next;

    len -= *(p + 1) + 2;
    p += *(p + 1) + 2;
  };

  return opts;
};
#endif /* 0 */

struct mbuf *ipv6_headertocontrol(struct mbuf *m, int extra, int inp_flags)
{
  struct mbuf *opts = NULL, **mp = &opts;

  if (inp_flags & INP_RXINFO) {
    struct in6_pktinfo pktinfo;

    if (m->m_pkthdr.rcvif) {
      pktinfo.ipi6_ifindex = m->m_pkthdr.rcvif->if_index;
    } else {
      DPRINTF(IDL_ERROR, ("ipv6_controldata: m->m_pkthdr.rcvif = NULL!\n"));
      pktinfo.ipi6_ifindex = 0;
    }

    bcopy(&(mtod(m, struct ipv6 *)->ipv6_dst), &pktinfo.ipi6_addr, sizeof(struct in6_addr));
    if ((*mp = ipv6_saveopt((caddr_t)&pktinfo, sizeof(struct in6_pktinfo), IPV6_PKTINFO, IPPROTO_IPV6)))
      mp = &(*mp)->m_next;
  };

  if (inp_flags & INP_HOPLIMIT) {
    int hoplimit = mtod(m, struct ipv6 *)->ipv6_hoplimit;
    if ((*mp = ipv6_saveopt((caddr_t)&hoplimit, sizeof(int), IPV6_HOPLIMIT, IPPROTO_IPV6)))
      mp = &(*mp)->m_next;
  };

#if 0
  /* Since there's not any immediate need for these options anyway, we'll
     worry about reimplementing them later. - cmetz */

  if (inp_flags & INP_RXHOPOPTS) {
    int i;
    for (i = 1; i < ihioff; i++)
      if (ihi[i].ihi_nexthdr == IPPROTO_HOPOPTS) {
	if ((*mp = ipv6_savebag(ihi[i].ihi_mbuf, IPPROTO_HOPOPTS)))
	  mp = &(*mp)->m_next;
	break;
      };
  };

  if (inp_flags & INP_RXDSTOPTS) {
    int i;
    for (i = 1; i < ihioff; i++)
      if (ihi[i].ihi_nexthdr == IPPROTO_DSTOPTS) {
	if ((*mp = ipv6_savebag(ihi[i].ihi_mbuf, IPPROTO_DSTOPTS)))
	  mp = &(*mp)->m_next;
	break;
      };
  };

  if (inp_flags & INP_RXSRCRT) {
    int i;
    for (i = 1; i < ihioff; i++)
      if (ihi[i].ihi_nexthdr == IPPROTO_ROUTING) {
	if ((*mp = ipv6_saveopt((caddr_t)(mtod(ihi[i].ihi_mbuf, uint8_t *) + 3), (*mtod(ihi[i].ihi_mbuf, uint8_t *) << 3) + 5, *(mtod(ihi[i].ihi_mbuf, uint8_t *) + 2), IPPROTO_ROUTING)))
	  mp = &(*mp)->m_next;
	break;
      };
  };
#endif /* 0 */

  return opts;
};
