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
#ifndef _NETINET6_IPV6_VAR_H
#define _NETINET6_IPV6_VAR_H 1

#include <netinet6/in6.h>

/*
 * IPv6 multicast "options".  Session state for multicast, including
 * weird per-session multicast things.
 */

struct ipv6_moptions
{
  struct ifnet *i6mo_multicast_ifp;     /* ifp for outgoing multicasts */
  u_char i6mo_multicast_ttl;            /* TTL for outgoing multicasts.
					   Does this matter in IPv6? */
  u_char i6mo_multicast_loop;           /* 1 => hear sends if a member */
  u_short i6mo_num_memberships;         /* no. memberships this socket */
  struct in6_multi *i6mo_membership[IN6_MAX_MEMBERSHIPS];
};

/*
 * IPv6 stats.
 */

#if defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802
#define _IPV6STAT_TYPE u_quad_t
#else /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */
#define _IPV6STAT_TYPE u_long
#endif /* defined(_BSDI_VERSION) && _BSDI_VERSION >= 199802 */

struct	ipv6stat {
	_IPV6STAT_TYPE	ips_total;		/* total packets received */
	_IPV6STAT_TYPE	ips_tooshort;		/* packet too short */
	_IPV6STAT_TYPE	ips_toosmall;		/* not enough data */
	_IPV6STAT_TYPE	ips_fragments;		/* fragments received */
	_IPV6STAT_TYPE	ips_fragdropped;	/* frags dropped (dups, out of space) */
	_IPV6STAT_TYPE	ips_fragtimeout;	/* fragments timed out */
	_IPV6STAT_TYPE	ips_forward;		/* packets forwarded */
	_IPV6STAT_TYPE	ips_cantforward;	/* packets rcvd for unreachable dest */
	_IPV6STAT_TYPE	ips_redirectsent;	/* packets forwarded on same net */
	_IPV6STAT_TYPE	ips_noproto;		/* unknown or unsupported protocol */
	_IPV6STAT_TYPE	ips_delivered;		/* datagrams delivered to upper level*/
	_IPV6STAT_TYPE	ips_localout;		/* total ip packets generated here */
	_IPV6STAT_TYPE	ips_odropped;		/* lost packets due to nobufs, etc. */
	_IPV6STAT_TYPE	ips_reassembled;	/* total packets reassembled ok */
	_IPV6STAT_TYPE	ips_fragmented;		/* datagrams sucessfully fragmented */
	_IPV6STAT_TYPE	ips_ofragments;		/* output fragments created */
	_IPV6STAT_TYPE	ips_cantfrag;		/* don't fragment flag was set, etc. */
	_IPV6STAT_TYPE	ips_badoptions;		/* error in option processing */
	_IPV6STAT_TYPE	ips_noroute;		/* packets discarded due to no route */
	_IPV6STAT_TYPE	ips_badvers;            /* IPv6 version != 6 */
	_IPV6STAT_TYPE	ips_rawout;		/* total raw ip packets generated */
};

#ifdef KERNEL

/*
 * The IPv6 fragment queue entry structure.
 * Notes:
 *   Nodes are stored in ttl order.
 *   prefix comes from whichever packet gets here first.
 *   data contains a chain of chains of mbufs (m_next down a chain, m_nextpkt
 *     chaining chains together) where the chains are ordered by assembly
 *     position. When two chains are contiguous for reassembly, they are
 *     combined and the frag header disappears.
 *   The structure is deliberately sized so MALLOC will round up on the order
 *     of much less than the total size instead of doubling the size.
 */

struct ipv6_fragment
{
  struct ipv6_fragment *next;		/* Next fragment chain */
  struct mbuf *prefix;			/* Headers before frag header(s) */
  struct mbuf *data;			/* Frag headers + whatever data */
  u_char ttl;				/* Fragment chain TTL. */
  u_char flags;				/* Bit 0 indicates got end of chain */
};

/*
 * Structures and definitions for discovery mechanisms in IPv6.
 */

/*
 * Neighbor cache:
 *
 *     Number of unanswered probes is in discq.
 *     "Time of next event" will be in rt->rt_rmx.rmx_expire
 *          (rmx_expire will actually be quite overloaded, actually.)
 *     Status REACHABLE will be dq_unanswered < 0
 *     Status PROBE will be dq_unanswered >= 0
 *     Status INCOMPLETE will be link addr length of 0 if held,
 *     or deleted if not held.
 *
 *     If held, but INCOMPLETE fails set RTF_REJECT and make sure
 *     IPv6 and HLP's know how to deal with RTF_REJECT being set.
 */

struct discq   /* Similar to v4's llinfo_arp, discovery's "neighbor entry". */
{
  struct discq *dq_next,*dq_prev;        /* For {ins,rem}que(). */
  struct rtentry *dq_rt;                 /* Back pointer to routing entry for
					    an address that may be dead. */
  struct mbuf *dq_queue;                 /* Queue of outgoing messages. */
  int dq_unanswered;                     /* Number of unanswered probes. */
};

#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
/* Routing flag redefinitions */
#define RTF_ISAROUTER RTF_PROTO2         /* Neighbor is a router. */
#define RTF_DEFAULT RTF_PROTO1           /* Default route. */
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */

/*
 * These should be configurable parameters, see ipv6_discovery.c.
 * All units are in comments besides constants.
 */

#define MAX_INITIAL_RTR_ADVERT_INTERVAL		16  /* seconds */
#define MAX_INITIAL_RTR_ADVERTISEMENTS          3   /* transmissions */
#define MAX_RTR_RESPONSE_DELAY                  2   /* seconds */

#define MAX_RTR_SOLICITATION_DELAY              1   /* second */
#define RTR_SOLICITATION_INTERVAL               3   /* seconds */
#define MAX_RTR_SOLICITATIONS                   3   /* transmissions */

#define MAX_MULTICAST_SOLICIT                   3   /* transmissions */
#define MAX_UNICAST_SOLICIT                     3   /* transmissions */
#define MAX_ANYCAST_DELAY_TIME                  1   /* seconds */
#define MAX_NEIGHBOR_ADVERTISEMENTS             3   /* transmissions */
#define MIN_NEIGHBOR_ADVERT_INTERVAL            16  /* seconds */
#define REACHABLE_TIME                          30  /* seconds */
#define RETRANS_TIMER				3   /* seconds */
#define DELAY_FIRST_PROBE_TIME                  3   /* seconds */
/* Need to somehow define random factors. */

#define NEXTHOP_CLEAN_INTERVAL                  600 /* seconds */
#define REJECT_TIMER                            20 /* seconds */

/* 
 * Child of a router or tunnel.  Is a "meta-entry" for garbage collection.
 */

struct v6child
{
  struct v6child *v6c_next,*v6c_prev;   /* For {ins,rem}que() */
  struct v6router *v6c_parent;         /* Parent router.  I'm null if
					  I'm the router, or a tunnel
					  child. */
  struct rtentry *v6c_route;           /* Next-hop cache entry.  I won't
					  be holding it, but I'm attached
					  to it, like discq is to neighbor
					  cache entries. */
};

/*
 * Default router list entry.  Should be inserted
 * in priority order.  Will also have entries for non-
 * default routers, because I may be a router myself.
 */

struct v6router
{
  struct v6router *v6r_next,*v6r_prev;  /* For {ins,rem}que() */
  struct rtentry *v6r_rt;       /* Route for this.  Could be neighbor,
				   could be tunnel. */
  struct v6child v6r_children;  /* Children of this router. */
  
  /* Metric information? */
  uint32_t v6r_expire;            /* Expiration time. */
};
#define V6R_SIN6(v6r)  ((struct sockaddr_in6 *)rt_key((v6r)->v6r_rt))

/*
 * Flags for "flags" argument in ipv6_output().
 */

#define IPV6_FORWARDING 0x1          /* Most of IPv6 header exists? */
#define IPV6_RAWOUTPUT 0x2           /* Raw IPv6 packet! */
#define IPV6_ROUTETOIF SO_DONTROUTE  /* Include sys/socket.h... */

void ipv6_init __P((void));
void ipv6_drain __P((void));
void ipv6_slowtimo __P((void));
int ipv6_sysctl __P((int *, uint, void *, size_t *, void *, size_t));
struct route6;

#if __FreeBSD__
int ipv6_ctloutput __P((struct socket *, struct sockopt *));
int ripv6_ctloutput __P((struct socket *, struct sockopt *));
#else /* __FreeBSD__ */
int ipv6_ctloutput __P((int, struct socket *,int,int, struct mbuf **));
int ripv6_ctloutput __P((int, struct socket *, int, int, struct mbuf **));
#endif /* __FreeBSD__ */
void	 ripv6_init __P((void));
#if __OpenBSD__
void	 ripv6_input __P((struct mbuf *, ...));
int	 ripv6_output __P((struct mbuf *, ...));
#else /* __OpenBSD__ */
void	 ripv6_input __P((struct mbuf *, int));
int	 ripv6_output __P((struct mbuf *, struct socket *, struct in6_addr *, struct mbuf *));
#endif /* __OpenBSD__ */

#if __NetBSD__ || __FreeBSD__
int ripv6_usrreq_send(struct socket *, int, struct mbuf *, struct sockaddr *,
		      struct mbuf *, struct proc *);
#else /* __NetBSD__ || __FreeBSD__ */
int ripv6_usrreq_send(struct socket *, int, struct mbuf *, struct sockaddr *,
		      struct mbuf *);
#endif /* __NetBSD__ || __FreeBSD__ */

#if __FreeBSD__
int ripv6_usrreq_abort(struct socket *);
int ripv6_usrreq_attach(struct socket *, int , struct proc *);
int ripv6_usrreq_bind(struct socket *, struct sockaddr *, struct proc *);
int ripv6_usrreq_connect(struct socket *, struct sockaddr *, struct proc *);
int ripv6_usrreq_control(struct socket *, u_long, caddr_t, struct ifnet *,
			 struct proc *);
int ripv6_usrreq_detach(struct socket *);
int ripv6_usrreq_peeraddr(struct socket *, struct sockaddr **);
int ripv6_usrreq_sense(struct socket *, struct stat *);
int ripv6_usrreq_shutdown(struct socket *);
int ripv6_usrreq_sockaddr(struct socket *, struct sockaddr **);
#else /* __FreeBSD__ */
#if __NetBSD__
int ripv6_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
		      struct mbuf *, struct proc *));
#else /* __NetBSD__ */
int ripv6_usrreq __P((struct socket *, int, struct mbuf *, struct mbuf *,
		      struct mbuf *));
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */

#if 0 /* __OpenBSD__ */
void ipv6_input __P((struct mbuf *, ...));
int ipv6_output __P((struct mbuf *, ...));
#else /* __OpenBSD__ */
void ipv6_input __P((struct mbuf *, int));
int ipv6_output __P((struct mbuf *, struct route6 *, int, struct ipv6_moptions *, struct ifnet *, struct socket *));
#endif /* __OpenBSD__ */
void ipv6_reasm __P((struct mbuf *, int));
void ipv6_hop __P((struct mbuf *, int));

#if __FreeBSD__
int in6_control __P((struct socket *,int, caddr_t, struct ifnet *,int, struct proc *));
#else /* __FreeBSD__ */
#if __NetBSD__
int in6_control __P((struct socket *,u_long, caddr_t, struct ifnet *,int, struct proc *));
#else /* __NetBSD__ */
int in6_control __P((struct socket *,int, caddr_t, struct ifnet *,int));
#endif /* __NetBSD__ */
#endif /* __FreeBSD__ */
void ipv6_stripoptions __P((struct mbuf *, int));
struct in6_multi *in6_addmulti __P((struct in6_addr *,struct ifnet *));
void in6_delmulti __P((struct in6_multi *));

#if __FreeBSD__
/* ripv6_usrreq  and  ipv6_icmp_usrreq functions */
extern struct pr_usrreqs ripv6_usrreqs;
extern struct pr_usrreqs ipv6_icmp_usrreqs;

extern int ripv6_usr_attach(struct socket *, int , struct proc *);
extern int ripv6_usr_disconnect(struct socket *);
extern int ripv6_usr_abort(struct socket *);
extern int ripv6_usr_detach(struct socket *);
extern int ripv6_usr_bind(struct socket *, struct sockaddr *, struct proc *);
extern int ripv6_usr_connect(struct socket *, struct sockaddr *, struct proc *);
extern int ripv6_usr_shutdown(struct socket *);
extern int ripv6_usr_send(struct socket *, int, struct mbuf *, 
			  struct sockaddr *, struct mbuf *, struct proc *);
extern int ripv6_usr_control(struct socket *, int, caddr_t,   
			     struct ifnet *, struct proc *);
extern int ripv6_usr_sense(struct socket *, struct stat *);
extern int ripv6_usr_sockaddr(struct socket *, struct sockaddr **);
extern int ripv6_usr_peeraddr(struct socket *, struct sockaddr **);
#endif /* __FreeBSD__ */

extern int ipv6_icmp_send(struct socket *, int, struct mbuf *, 
			  struct sockaddr *, struct mbuf *, struct proc *);

#if __OpenBSD__
void *ipv6_trans_ctlinput __P((int, struct sockaddr *, void *, struct mbuf *));
#else /* __OpenBSD__ */
struct ip;
void ipv6_trans_ctlinput __P((int, struct sockaddr *, struct ip *, struct mbuf *));
#endif /* __OpenBSD__ */

/* These might belong in in_pcb.h */
struct inpcb;
#if __FreeBSD__
/*
 * FreeBSD, having done away with the *_usrreq() functions no longer needs to 
 * pass mbufs to these functions.  Thus they pass in sockaddrs instead.  
 */
int in6_pcbbind(struct inpcb *, struct sockaddr *);
int in6_pcbconnect(struct inpcb *, struct sockaddr *);
int in6_setsockaddr(struct inpcb *, struct sockaddr **);
int in6_setpeeraddr(struct inpcb *, struct sockaddr **);
#else /* __FreeBSD__ */
int in6_pcbbind(struct inpcb *, struct mbuf *);
int in6_pcbconnect(struct inpcb *, struct mbuf *);
int in6_setsockaddr(struct inpcb *, struct mbuf *);
int in6_setpeeraddr(struct inpcb *, struct mbuf *);
#endif /* __FreeBSD__ */
void ipv6_onlink_query(struct sockaddr_in6 *);
int ipv6_verify_onlink(struct sockaddr_in6 *);

#if __FreeBSD__ 
struct inpcbhead; 		/* XXX?  Forward declaration needed. */
#define __IN6_PCBNOTIFY_FIRSTARG struct inpcbhead *
#endif /* __FreeBSD__ */
#if __NetBSD__ || __OpenBSD__
struct inpcbtable;
#define __IN6_PCBNOTIFY_FIRSTARG struct inpcbtable *
#endif /* __NetBSD__ || __OpenBSD__ */
#if __bsdi__
struct inpcb;
#define __IN6_PCBNOTIFY_FIRSTARG struct inpcb *
#endif /* __bsdi__ */

#if (!__OpenBSD__ && defined(IPSEC)) || (__OpenBSD__ && defined(NRL_IPSEC))
int in6_pcbnotify __P((__IN6_PCBNOTIFY_FIRSTARG, struct sockaddr *, uint,
                       struct in6_addr *, uint, int, void (*)(struct inpcb *,
                       int), struct mbuf *, int));
#else /* (!__OpenBSD__ && defined(IPSEC)) || (__OpenBSD__ && defined(NRL_IPSEC)) */
int in6_pcbnotify __P((__IN6_PCBNOTIFY_FIRSTARG, struct sockaddr *, uint,
                       struct in6_addr *, uint, int, void (*)(struct inpcb *,
                       int)));
#endif /* (!__OpenBSD__ && defined(IPSEC)) || (__OpenBSD__ && defined(NRL_IPSEC)) */

#undef __IN6_PCBNOTIFY_FIRSTARG

void ipv6_freemoptions __P((struct ipv6_moptions *));
#endif /* KERNEL */

#endif /* _NETINET6_IPV6_VAR_H */
