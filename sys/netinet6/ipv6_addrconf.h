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
#ifndef _NETINET6_IPV6_ADDRCONF_H
#define _NETINET6_IPV6_ADDRCONF_H 1

/*
 * Function definitions.
 */
void addrconf_init __P((void));
void addrconf_timer __P((void *));
void addrconf_dad __P((struct in6_ifaddr *));

#endif /* _NETINET6_IPV6_ADDRCONF_H */
