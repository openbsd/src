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

/*
 * Next header types (called Protocols in netinet/in.h).
 */

#define IPPROTO_HOPOPTS		0	/* Hop-by-hop option header. */
#define IPPROTO_IPV4		4	/* IPv4 in IPv6 (?!?) */
/* BAD PLACE #define IPPROTO_IPV6		41	 IPv6 in IPv6 */
#define IPPROTO_ROUTING		43	/* Routing header. */
#define IPPROTO_FRAGMENT	44	/* Fragmentation/reassembly header. */
#define IPPROTO_ESP		50	/* Encapsulating security payload. */
#define IPPROTO_AH		51	/* Authentication header. */
#define IPPROTO_ICMPV6		58      /* ICMP for IPv6 */
#define IPPROTO_NONE		59	/* No next header */
#define IPPROTO_DSTOPTS		60	/* Destination options header. */

/*
 * Following are TBD, and subject to change rapidly
 */
#define IPPROTO_RAW		255	/* Payload of unknown type? */
#define IPPROTO_MAX		256	/* Upper bound for next header type. */

/* IPPROTO type macros. */

#define IS_PREFRAG(x)   ( (x)==IPPROTO_HOPOPTS || (x)==IPPROTO_ROUTING || \
			 (x) == IPPROTO_DSTOPTS)
#define IS_IPV6OPT(x)   ( (x)==IPPROTO_FRAGMENT || (x) == IPPROTO_AH || \
			 IS_PRFRAG(x) )

#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
struct in6_addr
{
  union 
    {
      uint8_t bytes[16];
      uint32_t words[4];
    } in6a_u;
#define in6a_words in6a_u.words
#define s6_addr in6a_u.bytes
};
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */

#if !defined(SIN6_LEN)
struct sockaddr_in6 {
#if __linux__
  uint16_t sin6_family;
#else /* __linux__ */
#define SIN6_LEN
  uint8_t sin6_len;
  uint8_t sin6_family;
#endif /* __linux__ */
  uint16_t sin6_port;
  uint32_t sin6_flowinfo;
  struct in6_addr sin6_addr;
};
#endif /* !defined(SIN6_LEN) */

#define CREATE_IPV6_MAPPED(v6, v4) { \
	v6.in6a_words[0] = 0; \
	v6.in6a_words[1] = 0; \
	v6.in6a_words[2] = htonl(0xffff); \
	v6.in6a_words[3] = v4; }

#if 0 /* defined(__GNUC__) && (__GNUC__ >= 2) && defined(__OPTIMIZE__) */
#define IN6_ARE_ADDR_EQUAL(x, y) memcmp((x), (y), sizeof(struct in6_addr))
#else /* defined(__GNUC__) && (__GNUC__ >= 2) && defined(__OPTIMIZE__) */
#define IN6_ARE_ADDR_EQUAL(x, y) ( \
        (x)->in6a_words[0] == (y)->in6a_words[0] && \
        (x)->in6a_words[1] == (y)->in6a_words[1] && \
        (x)->in6a_words[2] == (y)->in6a_words[2] && \
        (x)->in6a_words[3] == (y)->in6a_words[3])
#endif /* defined(__GNUC__) && (__GNUC__ >= 2) && defined(__OPTIMIZE__) */

#ifndef IN6_IS_ADDR_UNSPECIFIED
#define IN6_IS_ADDR_UNSPECIFIED(a) ( \
        ((a)->in6a_words[0] == 0) && \
        ((a)->in6a_words[1] == 0) && \
        ((a)->in6a_words[2] == 0) && \
        ((a)->in6a_words[3] == 0))

#define IN6_IS_ADDR_LOOPBACK(a) ( \
        ((a)->in6a_words[0] == 0) && \
        ((a)->in6a_words[1] == 0) && \
        ((a)->in6a_words[2] == 0) && \
        ((a)->in6a_words[3] == htonl(1)))

#define IN6_IS_ADDR_MULTICAST(a) ((a)->s6_addr[0] == 0xff)

#define IN6_IS_ADDR_LINKLOCAL(a) \
        (((a)->in6a_words[0] & htonl(0xffc00000)) == htonl(0xfe800000))

#define IN6_IS_ADDR_SITELOCAL(a) \
        (((a)->in6a_words[0] & htonl(0xffc00000)) == htonl(0xfec00000))

#define IN6_IS_ADDR_V4MAPPED(a) ( \
        ((a)->in6a_words[0] == 0) && \
        ((a)->in6a_words[1] == 0) && \
        ((a)->in6a_words[2] == htonl(0xffff)))

#define IN6_IS_ADDR_V4COMPAT(a) ( \
        ((a)->in6a_words[0] == 0) && \
        ((a)->in6a_words[1] == 0) && \
        ((a)->in6a_words[2] == 0) && \
        ((a)->in6a_words[3] & htonl(0xfffffffe)))

#define IN6_IS_ADDR_MC_NODELOCAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_INTRA_NODE)

#define IN6_IS_ADDR_MC_LINKLOCAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_INTRA_LINK)

#define IN6_IS_ADDR_MC_SITELOCAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_INTRA_SITE)

#define IN6_IS_ADDR_MC_ORGLOCAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_INTRA_ORG)

#define IN6_IS_ADDR_MC_COMMLOCAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_INTRA_COMM)

#define IN6_IS_ADDR_MC_GLOBAL(a) \
        (GET_IN6_MCASTSCOPE(*a) == IN6_GLOBAL)
#endif /* IN6_IS_ADDR_UNSPECIFIED */

/* NOTE:  IS_IN6_ALL* macros only check the 0x1 and 0x2 scoping levels.
          The IN6 ROAD document doesn't say those are good for higher
          scoping levels.
*/

#define IN6_INTRA_NODE 1  /*  intra-node scope */
#define IN6_INTRA_LINK 2  /*  intra-link scope */
/*            3  (unassigned)
            4  (unassigned)
*/
#define IN6_INTRA_SITE 5  /* intra-site scope */
/*            6  (unassigned)
            7  (unassigned)
*/
#define IN6_INTRA_ORG  8  /* intra-organization scope */
/*            9  (unassigned)
            A  (unassigned)
*/
#define IN6_INTRA_COMM 0xB/*  intra-community scope */
/*            C  (unassigned)
            D  (unassigned)
*/

#define IN6_GLOBAL   0xE  /* global scope*/
/*            F  reserved
*/

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
 * Additonal type information.
 */

#if !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802)
struct ipv6_mreq
{
  struct in6_addr ipv6mr_multiaddr;     /* Group addr. to join/leave. */
  unsigned int ipv6mr_interface;     /* Interface on which to do it. */
};
#endif /* !defined(_BSDI_VERSION) || (_BSDI_VERSION < 199802) */

/*
 * IP options for IPv6.  Note I use the IPV6_* semantics for IPv6-
 * specific options.  Another reason for the inclusion of <netinet/in.h> is
 * for the options that are common between IPv6 and IPv4.
 */

#ifdef __linux__
#define IPV6_ADDRFORM           1
#define IPV6_PKTINFO            2
#define IPV6_HOPOPTS            3
#define IPV6_DSTOPTS            4
#define IPV6_RTHDR              5
#define IPV6_PKTOPTIONS         6
#define IPV6_CHECKSUM           7
#define IPV6_HOPLIMIT           8

#define IPV6_UNICAST_HOPS       16
#define IPV6_MULTICAST_IF       17
#define IPV6_MULTICAST_HOPS     18
#define IPV6_MULTICAST_LOOP     19
#define IPV6_ADD_MEMBERSHIP     20
#define IPV6_DROP_MEMBERSHIP    21
#else /* __linux__ */
#define	IPV6_OPTIONS		1    /* buf/ipv6_opts; set/get IP options */
#define	IPV6_HDRINCL		2    /* int; header is included with data */
#define	IPV6_TOS		3    /* int; IP type of service and preced. */
#define	IPV6_UNICAST_HOPS	4    /* int; IPv6 unicast hop limit */
#define IPV6_PKTINFO            5    /* struct in6_pktinfo: if. and addr */
#define IPV6_HOPLIMIT		6    /* int; hop limit */
#define IPV6_CHECKSUM		7    /* int: checksum offset */
#define ICMPV6_FILTER		8    /* struct icmpv6_filter: type filter */
#define	IPV6_MULTICAST_IF	9    /* u_int; set/get multicast interface */
#define	IPV6_MULTICAST_HOPS	10   /* int; set/get multicast hop limit */
#define	IPV6_MULTICAST_LOOP	11   /* u_int; set/get multicast loopback */
#define	IPV6_ADD_MEMBERSHIP	12   /* ipv6_mreq; add group membership */
#define	IPV6_DROP_MEMBERSHIP	13   /* ipv6_mreq; drop group membership */

#define IPV6_ADDRFORM         0x16   /* int; get/set form of returned addrs */
#define IPV6_HOPOPTS          0x19   /* int; receive hop-by-hop options */
#define IPV6_DSTOPTS          0x1a   /* int; receive destination options */
#define IPV6_RTHDR            0x1b   /* int; receive routing header */

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

#ifdef KERNEL
/* Function prototypes go here. */
int in6_cksum __P((struct mbuf *,int, u_int, u_int));
#endif /* KERNEL */
#endif /* __linux__ */

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

#define IN6ADDR_ANY_INIT {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }}}
#define IN6ADDR_LOOPBACK_INIT {{{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 }}}

struct in6_pktinfo {
  struct in6_addr ipi6_addr;
  unsigned int ipi6_ifindex;
};

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

#endif /* _NETINET6_IN6_H */
