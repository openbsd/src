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

#ifndef _NETINET6_ICMPV6_H
#define _NETINET6_ICMPV6_H 1

#include <netinet6/ipv6.h>

struct icmpv6hdr {
  uint8_t     icmpv6_type;   /* type field */
  uint8_t     icmpv6_code;   /* code field */
  uint16_t    icmpv6_cksum;  /* checksum field */
  union {
    uint32_t  un_data32[1]; /* type-specific field */
    uint16_t  un_data16[2]; /* type-specific field */
    uint8_t   un_data8[4];  /* type-specific field */
  } icmpv6_dataun;
};

#define icmpv6_data32    icmpv6_dataun.un_data32
#define icmpv6_data16    icmpv6_dataun.un_data16
#define icmpv6_data8     icmpv6_dataun.un_data8
#define icmpv6_pptr      icmpv6_data32[0]  /* parameter prob */
#define icmpv6_mtu       icmpv6_data32[0]  /* packet too big */
#define icmpv6_id        icmpv6_data16[0]  /* echo request/reply */
#define icmpv6_seq       icmpv6_data16[1]  /* echo request/reply */
#define icmpv6_maxdelay  icmpv6_data16[0]  /* mcast group membership */

#define ICMPV6_DST_UNREACH           1
#define ICMPV6_PACKET_TOOBIG         2
#define ICMPV6_TIME_EXCEEDED         3 
#define ICMPV6_PARAMETER_PROBLEM     4

#define ICMPV6_INFOMSG_MASK          128 /* all informational messages */
#define ICMPV6_ECHO_REQUEST          128
#define ICMPV6_ECHO_REPLY            129
#define ICMPV6_MEMBERSHIP_QUERY      130
#define ICMPV6_MEMBERSHIP_REPORT     131
#define ICMPV6_MEMBERSHIP_REDUCTION  132

#define ICMPV6_UNREACH_NOROUTE       0
#define ICMPV6_UNREACH_ADMIN         1 /* administratively prohibited */
#define ICMPV6_UNREACH_NOTNEIGHBOR   2 /* not a neighbor (and must be) */
#define ICMPV6_UNREACH_ADDRESS       3
#define ICMPV6_UNREACH_PORT          4

#define ICMPV6_EXCEEDED_HOPS         0 /* Hop Limit == 0 in transit */
#define ICMPV6_EXCEEDED_REASSEMBLY   1 /* Reassembly time out */

#define ICMPV6_PARAMPROB_HDR         0 /* erroneous header field */
#define ICMPV6_PARAMPROB_NEXTHDR     1 /* unrecognized Next Header */
#define ICMPV6_PARAMPROB_OPTION      2 /* unrecognized option */

struct icmpv6_filter {
  uint32_t data[8];  /* 8*32 = 256 bits */
};

#define ICMPV6_FILTER_WILLPASS(type, filterp) \
  ((((filterp)->data[(type) >> 5]) & (1 << ((type) & 31))) == 0)
#define ICMPV6_FILTER_WILLBLOCK(type, filterp) \
  ((((filterp)->data[(type) >> 5]) & (1 << ((type) & 31))) != 0)
#define ICMPV6_FILTER_SETPASS(type, filterp) \
  ((((filterp)->data[(type) >> 5]) &= ~(1 << ((type) & 31))))
#define ICMPV6_FILTER_SETBLOCK(type, filterp) \
  ((((filterp)->data[(type) >> 5]) |=  (1 << ((type) & 31))))
#define ICMPV6_FILTER_SETPASSALL(filterp) \
  memset((filterp), 0, sizeof(struct icmpv6_filter))
#define ICMPV6_FILTER_SETBLOCKALL(filterp) \
  memset((filterp), 0xff, sizeof(struct icmpv6_filter))

#endif /* _NETINET6_ICMPV6_H */
