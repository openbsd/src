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

#ifndef _NETINET6_IN6_H
#define _NETINET6_IN6_H 1

#if !defined(_NETINET_IN_H) && !defined(_NETINET_IN_H_)
#error in6.h should no longer be included directly; include <netinet/in.h>
#endif /* !defined(_NETINET_IN_H) && !defined(_NETINET_IN_H_) */
#if __bsdi__ && !defined(_BSDI_VERSION)
#include <sys/param.h>
#endif /* __bsdi__ && !defined(_BSDI_VERSION) */

/* IPPROTO type macros. */

#define IS_PREFRAG(x)   ( (x)==IPPROTO_HOPOPTS || (x)==IPPROTO_ROUTING || \
			 (x) == IPPROTO_DSTOPTS)
#define IS_IPV6OPT(x)   ( (x)==IPPROTO_FRAGMENT || (x) == IPPROTO_AH || \
			 IS_PRFRAG(x) )

#define CREATE_IPV6_MAPPED(v6, v4) { \
	v6.in6a_words[0] = 0; \
	v6.in6a_words[1] = 0; \
	v6.in6a_words[2] = htonl(0xffff); \
	v6.in6a_words[3] = v4; }

#if BYTE_ORDER ==  BIG_ENDIAN

#define SET_IN6_ALLNODES(a)  {(a).in6a_words[0]=0xff000000;(a).in6a_words[3]=1;\
                              (a).in6a_words[1]=0;(a).in6a_words[2]=0;}
#define SET_IN6_ALLROUTERS(a)  {(a).in6a_words[0]=0xff000000;(a).in6a_words[3]=2;\
                              (a).in6a_words[1]=0;(a).in6a_words[2]=0;}

#define SET_IN6_MCASTSCOPE(a,bits) {(a).in6a_words[0]&=0xfff0ffff;\
                                    (a).in6a_words[0]|=(bits<<16);}
#define GET_IN6_MCASTSCOPE(a) ( ((a).in6a_words[0] & 0x000f0000) >> 16  )

#else   /* BYTE_ORDER == LITTLE_ENDIAN */

#define SET_IN6_ALLNODES(a)  {(a).in6a_words[0]=0xff;(a).in6a_words[3]=0x01000000;\
                              (a).in6a_words[1] = 0; (a).in6a_words[2] = 0;}
#define SET_IN6_ALLROUTERS(a)  {(a).in6a_words[0]=0xff;(a).in6a_words[3]=0x02000000;\
                              (a).in6a_words[1] = 0; (a).in6a_words[2] = 0;}

#define SET_IN6_MCASTSCOPE(a,bits) {(a).in6a_words[0]&=0xfffff0ff;\
                                    (a).in6a_words[0]|=(bits<<8);}
#define GET_IN6_MCASTSCOPE(a)  ( ((a).in6a_words[0] & 0x00000f00) >>8)

#endif  /* BYTE_ORDER == {BIG,LITTLE}_ENDIAN */

/*
 * IP options for IPv6.  Note I use the IPV6_* semantics for IPv6-
 * specific options.  Another reason for the inclusion of <netinet/in.h> is
 * for the options that are common between IPv6 and IPv4.
 */

#define IN6_MAX_MEMBERSHIPS 20  /* Maximum number of multicast memberships. */
#define IPV6_DEFAULT_MCAST_HOPS 1
#define IPV6_DEFAULT_MCAST_LOOP 1

/*
 * Definitions for inet6 sysctl operations.
 *
 * Third level is protocol number.
 * Fourth level is desired variable within that protocol.
 */

#define	IPV6PROTO_MAXID	(IPPROTO_ICMPV6 + 1)	/* don't list to IPPROTO_MAX. */

#define	CTL_IPV6PROTO_NAMES { \
	{ "ipv6", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "ipv4", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ "tcp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "udp", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "esp", CTLTYPE_NODE }, \
	{ "ah", CTLTYPE_NODE }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ 0, 0 }, \
	{ "icmpv6", CTLTYPE_NODE }, \
}
 
/*
 * Names for IPv6 sysctl objects
 */

#define	IPV6CTL_FORWARDING	1	/* act as router */
#define	IPV6CTL_SENDREDIRECTS	2	/* may send redirects when forwarding */
#define	IPV6CTL_DEFTTL		3	/* default TTL */
#ifdef notyet
#define	IPV6CTL_DEFMTU		4	/* default MTU */
#endif
#define	IPV6CTL_STATS		5
#define	IPV6CTL_ROUTERSOLICIT	6
#define	IPV6CTL_MAXID		7

#define	IPV6CTL_NAMES { \
	{ 0, 0 }, \
	{ "forwarding", CTLTYPE_INT }, \
	{ "redirect", CTLTYPE_INT }, \
	{ "ttl", CTLTYPE_INT }, \
	{ "mtu", CTLTYPE_INT }, \
	{ "stats", CTLTYPE_STRUCT }, \
	{ "routersolicit", CTLTYPE_INT }, \
	{ 0, 0 }, \
}

#define IPV6CTL_VARS { \
	0, \
	&ipv6forwarding, \
	0, \
	&ipv6_defhoplmt, \
	0, \
	&ipv6rsolicit \
}

/* Cheesy hack for if net/route.h included... */
#ifdef RTM_VERSION 
/*
 * sizeof(struct sockaddr_in6) > sizeof(struct sockaddr), therefore, I
 * need to define... 
 */ 
struct route6
{
  struct  rtentry *ro_rt;
  struct  sockaddr_in6 ro_dst;
};
#endif RTM_VERSION 

#if defined(_KERNEL) || defined(KERNEL)
/* Function prototypes go here. */
int in6_cksum __P((struct mbuf *,int, u_int, u_int));
#endif /* defined(_KERNEL) || defined(KERNEL) */
#endif /* _NETINET6_IN6_H */
