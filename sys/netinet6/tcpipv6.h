/*	$OpenBSD: tcpipv6.h,v 1.6 2000/02/28 11:55:23 itojun Exp $	*/

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
#ifndef _NETINET6_TCPIPV6_H
#define _NETINET6_TCPIPV6_H 1

#include <netinet/ip6.h>
#include <netinet/tcp.h>

struct tcpipv6hdr {
	struct ip6_hdr ti6_i;
	struct tcphdr ti6_t;
};

#define ti6_src		ti6_i.ipv6_src
#define ti6_dst		ti6_i.ipv6_dst
#define	ti6_sport	ti6_t.th_sport
#define	ti6_dport	ti6_t.th_dport
#define	ti6_seq		ti6_t.th_seq
#define	ti6_ack		ti6_t.th_ack
#define	ti6_x2		ti6_t.th_x2
#define	ti6_off		ti6_t.th_off
#define	ti6_flags	ti6_t.th_flags
#define	ti6_win		ti6_t.th_win
#define	ti6_sum		ti6_t.th_sum
#define	ti6_urp		ti6_t.th_urp

#endif /* _NETINET6_TCPIPV6_H */
