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
#ifndef _NETINET6_IPV6_TRANS_H
#define _NETINET6_IPV6_TRANS_H 1

/*  I don't include any #includes, as I'm using this for our (NRL)
 *  modified netinet/ip_output() function; thus, this #include should be
 *  used/stuck-in after all the other necessary includes.
 *
 *  And yes, I only put one declaration here.  There's no real need
 *  to stick the other prototypes in here and have ip_output() fluffed
 *  during preprocessing time.
 */

int ipv4_tunnel_output __P((struct mbuf *, struct sockaddr_in *, struct rtentry *));

#endif /* _NETINET6_IPV6_TRANS_H */
