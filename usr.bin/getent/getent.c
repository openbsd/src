/*	$OpenBSD: getent.c,v 1.5 2008/06/26 05:42:21 ray Exp $	*/
/*	$NetBSD: getent.c,v 1.7 2005/08/24 14:31:02 ginsbach Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <netinet/in.h>		/* for INET6_ADDRSTRLEN */
#include <netinet/if_ether.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <rpc/rpc.h>

static int	usage(void);
static int	ethers(int, char *[]);
static int	group(int, char *[]);
static int	hosts(int, char *[]);
static int	networks(int, char *[]);
static int	passwd(int, char *[]);
static int	protocols(int, char *[]);
static int	rpc(int, char *[]);
static int	services(int, char *[]);
static int	shells(int, char *[]);
extern char *__progname;

enum {
	RV_OK		= 0,
	RV_USAGE	= 1,
	RV_NOTFOUND	= 2,
	RV_NOENUM	= 3
};

static struct getentdb {
	const char	*name;
	int		(*fn)(int, char *[]);
} databases[] = {
	{	"ethers",	ethers,		},
	{	"group",	group,		},
	{	"hosts",	hosts,		},
	{	"networks",	networks,	},
	{	"passwd",	passwd,		},
	{	"protocols",	protocols,	},
	{	"rpc",		rpc,		},
	{	"services",	services,	},
	{	"shells",	shells,		},

	{	NULL,		NULL,		},
};

int
main(int argc, char *argv[])
{
	struct getentdb	*curdb;

	if (argc < 2)
		usage();
	for (curdb = databases; curdb->name != NULL; curdb++) {
		if (strcmp(curdb->name, argv[1]) == 0) {
			exit(curdb->fn(argc, argv));
			break;
		}
	}
	fprintf(stderr, "%s: unknown database: %s\n", __progname, argv[1]);
	return RV_USAGE;
}

static int
usage(void)
{
	fprintf(stderr, "usage: %s database [key ...]\n", __progname);
	exit(RV_USAGE);
	/* NOTREACHED */
}

/*
 * printfmtstrings --
 *	vprintf(format, ...),
 *	then the aliases (beginning with prefix, separated by sep),
 *	then a newline
 */
static void
printfmtstrings(char *strings[], const char *prefix, const char *sep,
	const char *fmt, ...)
{
	va_list		ap;
	const char	*curpref;
	int		i;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);

	curpref = prefix;
	for (i = 0; strings[i] != NULL; i++) {
		printf("%s%s", curpref, strings[i]);
		curpref = sep;
	}
	printf("\n");
}

#define ETHERSPRINT	printf("%-17s  %s\n", ether_ntoa(eap), hp)

static int
ethers(int argc, char *argv[])
{
	char		hostname[MAXHOSTNAMELEN], *hp;
	int		i, rv = RV_OK;
	struct ether_addr ea, *eap;

	if (argc == 2) {
		fprintf(stderr, "%s: Enumeration not supported on ethers\n",
		    __progname);
		rv = RV_NOENUM;
	} else {
		for (i = 2; i < argc; i++) {
			if ((eap = ether_aton(argv[i])) == NULL) {
				eap = &ea;
				hp = argv[i];
				if (ether_hostton(hp, eap) != 0) {
					rv = RV_NOTFOUND;
					break;
				}
			} else {
				hp = hostname;
				if (ether_ntohost(hp, eap) != 0) {
					rv = RV_NOTFOUND;
					break;
				}
			}
			ETHERSPRINT;
		}
	}
	return rv;
}

#define GROUPPRINT	\
	printfmtstrings(gr->gr_mem, ":", ",", "%s:%s:%u", \
	    gr->gr_name, gr->gr_passwd, gr->gr_gid)

static int
group(int argc, char *argv[])
{
	int		i, rv = RV_OK;
	struct group	*gr;

	setgroupent(1);
	if (argc == 2) {
		while ((gr = getgrent()) != NULL)
			GROUPPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			const char	*err;
			long long id = strtonum(argv[i], 0, UINT_MAX, &err);

			if (!err)
				gr = getgrgid((gid_t)id);
			else
				gr = getgrnam(argv[i]);
			if (gr != NULL)
				GROUPPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endgrent();
	return rv;
}

static void
hostsprint(const struct hostent *he)
{
	char	buf[INET6_ADDRSTRLEN];

	if (inet_ntop(he->h_addrtype, he->h_addr, buf, sizeof(buf)) == NULL)
		strlcpy(buf, "# unknown", sizeof(buf));
	printfmtstrings(he->h_aliases, "  ", " ", "%-16s  %s", buf, he->h_name);
}

static int
hosts(int argc, char *argv[])
{
	char		addr[IN6ADDRSZ];
	int		i, rv = RV_OK;
	struct hostent	*he;

	sethostent(1);
	if (argc == 2) {
		while ((he = gethostent()) != NULL)
			hostsprint(he);
	} else {
		for (i = 2; i < argc; i++) {
			if (inet_pton(AF_INET6, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, IN6ADDRSZ, AF_INET6);
			else if (inet_pton(AF_INET, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, INADDRSZ, AF_INET);
			else
				he = gethostbyname(argv[i]);
			if (he != NULL)
				hostsprint(he);
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endhostent();
	return rv;
}

static void
networksprint(const struct netent *ne)
{
	char		buf[INET6_ADDRSTRLEN];
	struct in_addr	ianet;

	ianet = inet_makeaddr(ne->n_net, 0);
	if (inet_ntop(ne->n_addrtype, &ianet, buf, sizeof(buf)) == NULL)
		strlcpy(buf, "# unknown", sizeof(buf));
	printfmtstrings(ne->n_aliases, "  ", " ", "%-16s  %s", ne->n_name, buf);
}

static int
networks(int argc, char *argv[])
{
	int		i, rv = RV_OK;
	struct netent	*ne;
	in_addr_t	net;

	setnetent(1);
	if (argc == 2) {
		while ((ne = getnetent()) != NULL)
			networksprint(ne);
	} else {
		for (i = 2; i < argc; i++) {
			net = inet_network(argv[i]);
			if (net != INADDR_NONE)
				ne = getnetbyaddr(net, AF_INET);
			else
				ne = getnetbyname(argv[i]);
			if (ne != NULL)
				networksprint(ne);
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endnetent();
	return rv;
}

#define PASSWDPRINT	\
	printf("%s:%s:%u:%u:%s:%s:%s\n", \
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, \
	    pw->pw_gid, pw->pw_gecos, pw->pw_dir, pw->pw_shell)

static int
passwd(int argc, char *argv[])
{
	int		i, rv = RV_OK;
	struct passwd	*pw;

	setpassent(1);
	if (argc == 2) {
		while ((pw = getpwent()) != NULL)
			PASSWDPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			const char	*err;
			long long id = strtonum(argv[i], 0, UINT_MAX, &err);

			if (!err)
				pw = getpwuid((uid_t)id);
			else
				pw = getpwnam(argv[i]);
			if (pw != NULL)
				PASSWDPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endpwent();
	return rv;
}

#define PROTOCOLSPRINT	\
	printfmtstrings(pe->p_aliases, "  ", " ", \
	    "%-16s  %5d", pe->p_name, pe->p_proto)

static int
protocols(int argc, char *argv[])
{
	struct protoent	*pe;
	int		i, rv = RV_OK;

	setprotoent(1);
	if (argc == 2) {
		while ((pe = getprotoent()) != NULL)
			PROTOCOLSPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			const char	*err;
			long long id = strtonum(argv[i], 0, UINT_MAX, &err);

			if (!err)
				pe = getprotobynumber((int)id);
			else
				pe = getprotobyname(argv[i]);
			if (pe != NULL)
				PROTOCOLSPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endprotoent();
	return rv;
}

#define RPCPRINT	\
	printfmtstrings(re->r_aliases, "  ", " ", \
	    "%-16s  %6d", re->r_name, re->r_number)

static int
rpc(int argc, char *argv[])
{
	struct rpcent	*re;
	int		i, rv = RV_OK;

	setrpcent(1);
	if (argc == 2) {
		while ((re = getrpcent()) != NULL)
			RPCPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			const char	*err;
			long long id = strtonum(argv[i], 0, UINT_MAX, &err);

			if (!err)
				re = getrpcbynumber((int)id);
			else
				re = getrpcbyname(argv[i]);
			if (re != NULL)
				RPCPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endrpcent();
	return rv;
}

#define SERVICESPRINT	\
	printfmtstrings(se->s_aliases, "  ", " ", \
	    "%-16s  %5d/%s", se->s_name, ntohs(se->s_port), se->s_proto)

static int
services(int argc, char *argv[])
{
	struct servent	*se;
	int		i, rv = RV_OK;

	setservent(1);
	if (argc == 2) {
		while ((se = getservent()) != NULL)
			SERVICESPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			const char	*err;
			long long	id;
			char *proto = strchr(argv[i], '/');

			if (proto != NULL)
				*proto++ = '\0';
			id = strtonum(argv[i], 0, UINT_MAX, &err);
			if (!err)
				se = getservbyport(htons((u_short)id), proto);
			else
				se = getservbyname(argv[i], proto);
			if (se != NULL)
				SERVICESPRINT;
			else {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endservent();
	return rv;
}

#define SHELLSPRINT	printf("%s\n", sh)

static int
shells(int argc, char *argv[])
{
	const char	*sh;
	int		i, rv = RV_OK;

	setusershell();
	if (argc == 2) {
		while ((sh = getusershell()) != NULL)
			SHELLSPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			setusershell();
			while ((sh = getusershell()) != NULL) {
				if (strcmp(sh, argv[i]) == 0) {
					SHELLSPRINT;
					break;
				}
			}
			if (sh == NULL) {
				rv = RV_NOTFOUND;
				break;
			}
		}
	}
	endusershell();
	return rv;
}
