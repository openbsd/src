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
#ifndef _NETINET6_IPV6_ICMP_H
#define _NETINET6_IPV6_ICMP_H 1

/*
 * ICMPv6 header.
 */

struct ipv6_icmp
{
  uint8_t icmp_type;
  uint8_t icmp_code;
  uint16_t icmp_cksum;
  union
    {
      uint32_t ih_reserved;
      struct
	{
	  uint16_t ihs_id;
	  uint16_t ihs_seq;
	} ih_idseq;
      struct
	{
	  uint8_t ihr_hoplimit;
	  uint8_t ihr_bits;
	  uint16_t ihr_lifetime;
	} ih_radv;
    } icmp_hun;
#define icmp_unused icmp_hun.ih_reserved
#define icmp_nexthopmtu icmp_hun.ih_reserved
#define icmp_paramptr icmp_hun.ih_reserved
#define icmp_echoid icmp_hun.ih_idseq.ihs_id
#define icmp_echoseq icmp_hun.ih_idseq.ihs_seq
#define icmp_grpdelay icmp_hun.ih_idseq.ihs_id
#define icmp_grpunused icmp_hun.ih_idseq.ihs_seq
#define icmp_nadvbits icmp_hun.ih_reserved
#define icmp_radvhop icmp_hun.ih_radv.ihr_hoplimit
#define icmp_radvbits icmp_hun.ih_radv.ihr_bits
#define icmp_radvlifetime icmp_hun.ih_radv.ihr_lifetime
  union
    {
      struct
	{
	  struct ipv6 ido_ipv6;
	  uint8_t ido_remaining[1];
	} id_offending;
      uint8_t id_data[1];
      struct
	{
	  struct in6_addr idn_addr;
	  uint8_t idn_ext[1];
	} id_neighbor;
      struct
	{
	  struct in6_addr idr_addr1;
	  struct in6_addr idr_addr2;
	  uint8_t idr_ext[1];
	} id_redirect;
      struct
	{
	  uint32_t ida_reachable;
	  uint32_t ida_retrans;
	  uint8_t ida_opt[1];
	} id_radv;
    } icmp_dun;
#define icmp_offending icmp_dun.id_offending
#define icmp_ipv6 icmp_dun.id_offending.ido_ipv6

#define icmp_echodata icmp_dun.id_data

#define icmp_grpaddr icmp_dun.id_neighbor.idn_addr

#define icmp_radvreach icmp_dun.id_radv.ida_reachable
#define icmp_radvretrans icmp_dun.id_radv.ida_retrans
#define icmp_radvext icmp_dun.id_radv.ida_opt

#define icmp_nsoltarg icmp_dun.id_neighbor.idn_addr
#define icmp_nsolext icmp_dun.id_neighbor.idn_ext
#define icmp_nadvaddr icmp_dun.id_neighbor.idn_addr
#define icmp_nadvext icmp_dun.id_neighbor.idn_ext

#define icmp_redirtarg icmp_dun.id_redirect.idr_addr1
#define icmp_redirdest icmp_dun.id_redirect.idr_addr2
#define icmp_redirext icmp_dun.id_redirect.idr_ext
};

/*
 * ICMPv6 extension constants.
 */

#define EXT_SOURCELINK 1
#define EXT_TARGETLINK 2
#define EXT_PREFIX 3
#define EXT_REDIR 4
#define EXT_MTU 5

/*
 * Extension structures for IPv6 discovery messages.
 */

struct icmp_exthdr    /* Generic extension */
{
  uint8_t ext_id;
  uint8_t ext_length;    /* Length is 8 * this field, 0 is invalid. */
  uint8_t ext_data[6];   /* Padded to 8 bytes. */
};

struct ext_prefinfo    /* Prefix information */
{
  uint8_t pre_extid;
  uint8_t pre_length;

  uint8_t pre_prefixsize;
  uint8_t pre_bits;

  uint32_t pre_valid;
  uint32_t pre_preferred;
  uint32_t pre_reserved;

  struct in6_addr pre_prefix;
};

/*
 * Values for pre_bits
 */
#define ICMPV6_PREFIX_ONLINK 0x80
#define ICMPV6_PREFIX_AUTO 0x40

struct ext_redir    /* Redirected header */
{
  uint8_t rd_extid;
  uint8_t rd_length;
  uint8_t rd_reserved[6];
  struct ipv6 rd_header;
};

struct ext_mtu      /* Recommended link MTU. */
{
  uint8_t mtu_extid;
  uint8_t mtu_length;
  uint16_t mtu_reserved;
  uint32_t mtu_mtu;
};

/*
 * Constants
 */

/*
 * Lower bounds on packet lengths for various types.
 * For the error advice packets must first insure that the
 * packet is large enought to contain the returned ip header.
 * Only then can we do the check to see if enough bits of packet
 * data have been returned, since we need to check the returned
 * ipv6 header length.
 */
#define	ICMPV6_MINLEN	8				/* abs minimum */
#define	ICMPV6_TSLEN	(8 + 3 * sizeof (n_time))	/* timestamp */
#define	ICMPV6_NADVMINLEN 24	/* min neighbor advertisement */
#define ICMPV6_NSOLMINLEN 24    /* min neighbor solicit */
#define ICMPV6_RADVMINLEN 16    /* min router advertisement */
#define ICMPV6_RSOLMINLEN 8     /* min router solicit */
#define ICMPV6_REDIRMINLEN 40   /* min redirect */
#define ICMPV6_HLPMINLEN (8 + sizeof(struct ipv6) + 8)  /* HLP demux len. */
#define ICMPV6_MAXLEN     576   /* This should be whatever IPV6_MINMTU
				   will be.  I take this to be the WHOLE
				   packet, including IPv6 header, and any
				   IPv6 options before the ICMP message. */

/*
 * Definition of type and code field values.
 * ICMPv6 fixes things so that info messages are >= 128.
 */

/* Error messages and codes. */

#define	ICMPV6_UNREACH		1		/* dest unreachable, codes: */
#define         ICMPV6_UNREACH_NOROUTE 0                  /* No route to dest. */
#define         ICMPV6_UNREACH_ADMIN   1                  /* Admin. prohibited */
#define         ICMPV6_UNREACH_NOTNEIGHBOR 2              /* For strict source
							   routing. */
#define         ICMPV6_UNREACH_ADDRESS 3                  /* Address unreach. */
#define         ICMPV6_UNREACH_PORT  4                    /* Port unreachable */
#define ICMPV6_TOOBIG             2               /* Packet too big. */
#define	ICMPV6_TIMXCEED		3	        /* time exceeded, code: */
#define		ICMPV6_TIMXCEED_INTRANS	0		/* ttl==0 in transit */
#define		ICMPV6_TIMXCEED_REASS	1		/* Reassembly t.o. */
#define	ICMPV6_PARAMPROB		4		/* ip header bad */
#define		ICMPV6_PARAMPROB_PROB    0                /* Actual incorrect
							   parameter. */
#define         ICMPV6_PARAMPROB_NEXTHDR 1	        /* Bad next hdr. */
#define         ICMPV6_PARAMPROB_BADOPT  2                /* Unrec. option */

/* Info messages. */

#define	ICMPV6_ECHO		128		/* echo service */
#define	ICMPV6_ECHOREPLY	129		/* echo reply */
#define ICMPV6_GRPQUERY		130		/* Query group membership. */
#define ICMPV6_GRPREPORT	131		/* Join mcast group. */
#define ICMPV6_GRPTERM		132		/* Leave mcast group. */

#define ICMPV6_ROUTERSOL        133             /* Router solicit. */
#define ICMPV6_ROUTERADV        134             /* Router advertisement. */
#define ICMPV6_NEIGHBORSOL      135             /* Neighbor solicit. */
#define ICMPV6_NEIGHBORADV      136             /* Neighbor advertisement. */

#define ICMPV6_REDIRECT         137             /* ICMPv6 redirect. */

/* Defined this way to save some HTONL cycles on little-endian boxes. */
#if BYTE_ORDER == BIG_ENDIAN
#define           ICMPV6_NEIGHBORADV_RTR   0x80000000  /* Router flag. */
#define           ICMPV6_NEIGHBORADV_SOL   0x40000000  /* Solicited flag. */
#define           ICMPV6_NEIGHBORADV_OVERRIDE 0x20000000 /* Override flag. */
#else  /* BYTE_ORDER == LITTLE_ENDIAN */
#define           ICMPV6_NEIGHBORADV_RTR   0x80  /* Router flag. */
#define           ICMPV6_NEIGHBORADV_SOL   0x40  /* Solicited flag. */
#define           ICMPV6_NEIGHBORADV_OVERRIDE 0x20 /* Override flag. */
#endif

#define	ICMPV6_MAXTYPE		137

#define	ICMPV6_INFOTYPE(type) ((type) >= 128)

#ifdef KERNEL
#include <netinet6/ipv6_var.h>

/* Function prototypes */
void ipv6_icmp_error(struct mbuf *, int, int, uint32_t);
void ipv6_icmp_input(struct mbuf *, int);
void ipv6_gsolicit(struct ifnet *, struct mbuf *, struct rtentry *);
void ipv6_rtrequest(int, struct rtentry *, struct sockaddr *);
int ipv6_icmp_output(struct mbuf *,struct socket *, struct in6_addr *);
int ipv6_icmp_sysctl(int *, u_int, void *, size_t *, void *, size_t);
#if __NetBSD__ || __FreeBSD__
int ipv6_icmp_usrreq(struct socket *,int, struct mbuf *,struct mbuf *, struct mbuf *, struct proc *);
#else /* __NetBSD__ || __FreeBSD__ */
int ipv6_icmp_usrreq(struct socket *,int, struct mbuf *,struct mbuf *, struct mbuf *);
#endif /* __NetBSD__ || __FreeBSD__ */

void ipv6_routersol_input(struct mbuf *, int);
void ipv6_routeradv_input(struct mbuf *, int);
void ipv6_neighborsol_input(struct mbuf *, int);
void ipv6_neighboradv_input(struct mbuf *, int);
void ipv6_redirect_input(struct mbuf *, int);
#endif

#endif /* _NETINET6_IPV6_ICMP_H */
