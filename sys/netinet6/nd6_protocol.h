/*
%%% portions-copyright-nrl-97
Portions of this software are Copyright 1997-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.

*/

#ifndef _NETINET6_ND6_PROTOCOL_H
#define _NETINET6_ND6_PROTOCOL_H 1

#include <netinet6/icmpv6.h>

#define ND6_ROUTER_SOLICITATION         133
#define ND6_ROUTER_ADVERTISEMENT        134
#define ND6_NEIGHBOR_SOLICITATION       135
#define ND6_NEIGHBOR_ADVERTISEMENT      136
#define ND6_REDIRECT                    137

enum nd6_option {
  ND6_OPT_SOURCE_LINKADDR=1,
  ND6_OPT_TARGET_LINKADDR=2,
  ND6_OPT_PREFIX_INFORMATION=3,
  ND6_OPT_REDIRECTED_HEADER=4,
  ND6_OPT_MTU=5,
  ND6_OPT_ENDOFLIST=256
};

struct nd_router_solicit {     /* router solicitation */
  struct icmpv6hdr rsol_hdr;
};

#define rsol_type               rsol_hdr.icmpv6_type
#define rsol_code               rsol_hdr.icmpv6_code
#define rsol_cksum              rsol_hdr.icmpv6_cksum
#define rsol_reserved           rsol_hdr.icmpv6_data32[0]

struct nd_router_advert {       /* router advertisement */
  struct icmpv6hdr radv_hdr;
  uint32_t   radv_reachable;   /* reachable time */
  uint32_t   radv_retransmit;  /* reachable retransmit time */
};

#define radv_type               radv_hdr.icmpv6_type
#define radv_code               radv_hdr.icmpv6_code
#define radv_cksum              radv_hdr.icmpv6_cksum
#define radv_maxhoplimit        radv_hdr.icmpv6_data8[0]
#define radv_m_o_res            radv_hdr.icmpv6_data8[1]
#define ND6_RADV_M_BIT          0x80
#define ND6_RADV_O_BIT          0x40
#define radv_router_lifetime    radv_hdr.icmpv6_data16[1]

struct nd6_nsolicitation {      /* neighbor solicitation */
  struct icmpv6hdr  nsol6_hdr;
  struct in6_addr   nsol6_target;
};

struct nd6_nadvertisement {     /* neighbor advertisement */
  struct icmpv6hdr  nadv6_hdr;
  struct in6_addr   nadv6_target;
};

#define nadv6_flags nadv6_hdr.icmpv6_data32[0]
#define ND6_NADVERFLAG_ISROUTER      0x80
#define ND6_NADVERFLAG_SOLICITED     0x40
#define ND6_NADVERFLAG_OVERRIDE      0x20

struct nd6_redirect {           /* redirect */
  struct icmpv6hdr  redirect_hdr;
  struct in6_addr   redirect_target;
  struct in6_addr   redirect_destination;
};

struct nd6_opt_prefix_info {    /* prefix information */
  uint8_t    opt_type;
  uint8_t    opt_length;
  uint8_t    opt_prefix_length;
  uint8_t    opt_l_a_res;
  uint32_t   opt_valid_life;
  uint32_t   opt_preferred_life;
  uint32_t   opt_reserved2;
  struct in6_addr  opt_prefix;
};

#define ND6_OPT_PI_L_BIT        0x80
#define ND6_OPT_PI_A_BIT        0x40

struct nd6_opt_mtu {            /* MTU option */
  uint8_t   opt_type;
  uint8_t   opt_length;
  uint16_t  opt_reserved;
  uint32_t  opt_mtu;
};

#endif /* _NETINET6_ND6_PROTOCOL_H */
