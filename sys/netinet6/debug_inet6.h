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
#ifndef _NETINET6_DEBUG_INET6_H
#define _NETINET6_DEBUG_INET6_H 1

struct in6_addr;   
void dump_in6_addr(struct in6_addr *);
struct sockaddr_in6;
void dump_sockaddr_in6(struct sockaddr_in6 *);
struct ipv6;
void dump_ipv6(struct ipv6 *ipv6);
struct ipv6_icmp;
void dump_ipv6_icmp(struct ipv6_icmp *icp);
struct discq;
void dump_discq(struct discq *dq);
#endif /* _NETINET6_DEBUG_INET6_H */
