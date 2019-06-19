/*	$OpenBSD: bgplg.h,v 1.16 2018/10/24 13:18:52 denis Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _BGPLG_H
#define _BGPLG_H

#define NAME		"bgplg"

#define BGPLG_TIMEOUT	60	/* 60 seconds */

struct cmd {
	const char *name;
	int minargs;
	int maxargs;
	const char *args;
	char *earg[255];
	int (*func)(struct cmd *, char **);
};

#define CMDS	{							\
	{ "show ip bgp", 1, 1, "&lt;prefix&gt;",			\
	    { BGPCTL, "show", "ip", "bgp", NULL } },			\
	{ "show ip bgp as", 1, 1, "&lt;asnum&gt;",			\
	    { BGPCTL, "show", "ip", "bgp", "as", NULL } },		\
	{ "show ip bgp source-as", 1, 1, "&lt;asnum&gt;",		\
	    { BGPCTL, "show", "ip", "bgp", "source-as", NULL } },	\
	{ "show ip bgp transit-as", 1, 1, "&lt;asnum&gt;",		\
	    { BGPCTL, "show", "ip", "bgp", "transit-as", NULL } },	\
	{ "show ip bgp peer-as", 1, 1, "&lt;asnum&gt;",			\
	    { BGPCTL, "show", "ip", "bgp", "peer-as", NULL } },		\
	{ "show ip bgp empty-as", 0, 0, NULL,				\
	    { BGPCTL, "show", "ip", "bgp", "empty-as", NULL } },	\
	{ "show ip bgp summary", 0, 0, NULL,				\
	    { BGPCTL, "show", "ip", "bgp", "summary", NULL } },		\
	{ "show ip bgp community", 1, 1, "&lt;community&gt;",		\
	    { BGPCTL, "show","ip", "bgp", "community", NULL } },	\
	{ "show ip bgp detail community", 1, 1, "&lt;community&gt;",	\
	    { BGPCTL, "show","ip", "bgp", "detail", "community", NULL } },\
	{ "show ip bgp ext-community", 2, 2, "&lt;ext-community&gt;",	\
	    { BGPCTL, "show","ip", "bgp", "ext-community", NULL } },	\
	{ "show ip bgp detail ext-community", 2, 2, "&lt;ext-community&gt;",\
	    { BGPCTL, "show","ip", "bgp", "detail", "ext-community", NULL } },\
	{ "show ip bgp large-community", 1, 1, "&lt;large-community&gt;",\
	    { BGPCTL, "show","ip", "bgp", "large-community", NULL } },	\
	{ "show ip bgp detail large-community", 1, 1, "&lt;large-community&gt;",\
	    { BGPCTL, "show","ip", "bgp", "detail", "large-community", NULL } },\
	{ "show ip bgp detail", 1, 1, "&lt;prefix&gt;",			\
	    { BGPCTL, "show","ip", "bgp", "detail", NULL } },		\
	{ "show ip bgp detail as", 1, 1, "&lt;asnum&gt;",		\
	    { BGPCTL, "show","ip", "bgp", "detail", "as", NULL } },	\
	{ "show ip bgp in", 1, 1, "&lt;neighbor&gt;",			\
	    { BGPCTL, "show","ip", "bgp", "in", "neighbor", NULL } },	\
	{ "show ip bgp out", 1, 1, "&lt;neighbor&gt;",			\
	    { BGPCTL, "show","ip", "bgp", "out", "neighbor", NULL } },	\
	{ "show ip bgp ovs", 1, 1, "&lt;state&gt;",			\
	    { BGPCTL, "show","ip", "bgp", "ovs", NULL } },		\
	{ "show ip bgp memory", 0, 0, NULL,				\
	    { BGPCTL, "show", "ip", "bgp", "memory", NULL } },		\
	{ "show neighbor", 0, 1, NULL,					\
	    { BGPCTL, "show", "neighbor", NULL } },			\
	{ "show nexthop", 0, 0, NULL,					\
	    { BGPCTL, "show", "nexthop", NULL } },			\
	{ "traceroute", 1, 1, "&lt;address&gt;",			\
	    { TRACEROUTE, "-ASl", NULL } },				\
	{ "ping", 1, 1, "&lt;address&gt;",				\
	    { PING, "-c4", "-w2", NULL } },				\
	{ "traceroute6", 1, 1, "&lt;address&gt;",			\
	    { TRACEROUTE6, "-Al", NULL } },				\
	{ "ping6", 1, 1, "&lt;address&gt;",				\
	    { PING6, "-c4", "-i2", NULL } },				\
	{ "help", 0, 0, NULL, { NULL }, lg_help },			\
	{ NULL }							\
}

int	 lg_help(struct cmd *, char **);
int	 lg_exec(const char *, char **);
int	 lg_checkperm(struct cmd *);
void	 lg_sig_alarm(int);
ssize_t	 lg_strip(char *);

#endif /* _BGPLG_H */
