/*	$OpenBSD: getent.c,v 1.21 2018/11/02 10:21:29 kn Exp $	*/
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
#include <err.h>
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

static void	usage(void);
static int	ethers(int, char *[]);
static int	group(int, char *[]);
static int	hosts(int, char *[]);
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
	const char	*pledge;
	const char	*unveil;
} databases[] = {
	{	"ethers",	ethers,		"stdio rpath",	"/etc/ethers"	},
	{	"group",	group,		"stdio getpw",	NULL	},
	{	"hosts",	hosts,		"stdio dns",	NULL	},
	{	"passwd",	passwd,		"stdio getpw",	NULL	},
	{	"protocols",	protocols,	"stdio rpath",	"/etc/protocols"	},
	{	"rpc",		rpc,		"stdio rpath",	"/etc/rpc"	},
	{	"services",	services,	"stdio rpath",	"/etc/services"	},
	{	"shells",	shells,		"stdio rpath",	"/etc/shells"	},

	{	NULL,		NULL,				},
};

int
main(int argc, char *argv[])
{
	struct getentdb	*curdb;

	if (argc < 2)
		usage();
	for (curdb = databases; curdb->name != NULL; curdb++) {
		if (strcmp(curdb->name, argv[1]) == 0) {
			if (curdb->unveil != NULL) {
				if (unveil(curdb->unveil, "r") == -1)
					err(1, "unveil");
			}
			if (pledge(curdb->pledge, NULL) == -1)
				err(1, "pledge");

			exit(curdb->fn(argc, argv));
			break;
		}
	}
	fprintf(stderr, "%s: unknown database: %s\n", __progname, argv[1]);
	return RV_USAGE;
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s database [key ...]\n", __progname);
	exit(RV_USAGE);
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
	char		hostname[HOST_NAME_MAX+1], *hp;
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
	struct group	*gr;
	const char	*err;
	gid_t		gid;
	int		i, rv = RV_OK;

	setgroupent(1);
	if (argc == 2) {
		while ((gr = getgrent()) != NULL)
			GROUPPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if ((gr = getgrnam(argv[i])) == NULL) {
				gid = strtonum(argv[i], 0, GID_MAX, &err);
				if (err == NULL)
					gr = getgrgid(gid);
			}
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
	printfmtstrings(he->h_aliases, "  ", " ", "%-39s %s", buf, he->h_name);
}
static int
hostsaddrinfo(const char *name)
{
	struct addrinfo	 hints, *res, *res0;
	char		 buf[INET6_ADDRSTRLEN];
	int		 rv;

	rv = RV_NOTFOUND;
	memset(buf, 0, sizeof(buf));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(name, NULL, &hints, &res0) != 0)
		return (rv);
	for (res = res0; res; res = res->ai_next) {
		if ((res->ai_family != AF_INET6 && res->ai_family != AF_INET) ||
		    getnameinfo(res->ai_addr, res->ai_addrlen, buf, sizeof(buf),
		    NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(buf, "# unknown", sizeof(buf));
		else
			rv = RV_OK;
		printf("%-39s %s\n", buf, name);
	}
	freeaddrinfo(res0);

	return (rv);
}

static int
hosts(int argc, char *argv[])
{
	char		addr[IN6ADDRSZ];
	int		i, rv = RV_OK;
	struct hostent	*he;

	if (argc == 2) {
		fprintf(stderr, "%s: Enumeration not supported on hosts\n",
		    __progname);
		rv = RV_NOENUM;
	} else {
		for (i = 2; i < argc; i++) {
			he = NULL;
			if (inet_pton(AF_INET6, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, IN6ADDRSZ, AF_INET6);
			else if (inet_pton(AF_INET, argv[i], (void *)addr) > 0)
				he = gethostbyaddr(addr, INADDRSZ, AF_INET);
			if (he != NULL)
				hostsprint(he);
			else if ((rv = hostsaddrinfo(argv[i])) == RV_NOTFOUND)
				break;
		}
	}
	return rv;
}

#define PASSWDPRINT	\
	printf("%s:%s:%u:%u:%s:%s:%s\n", \
	    pw->pw_name, pw->pw_passwd, pw->pw_uid, \
	    pw->pw_gid, pw->pw_gecos, pw->pw_dir, pw->pw_shell)

static int
passwd(int argc, char *argv[])
{
	struct passwd	*pw;
	const char	*err;
	uid_t		uid;
	int		i, rv = RV_OK;

	setpassent(1);
	if (argc == 2) {
		while ((pw = getpwent()) != NULL)
			PASSWDPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if ((pw = getpwnam(argv[i])) == NULL) {
				uid = strtonum(argv[i], 0, UID_MAX, &err);
				if (err == NULL)
					pw = getpwuid(uid);
			}
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
	const char	*err;
	int		proto;
	int		i, rv = RV_OK;

	setprotoent(1);
	if (argc == 2) {
		while ((pe = getprotoent()) != NULL)
			PROTOCOLSPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			proto = strtonum(argv[i], 0, INT_MAX, &err);
			if (!err)
				pe = getprotobynumber(proto);
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
	const char	*err;
	int		rpc;
	int		i, rv = RV_OK;

	setrpcent(1);
	if (argc == 2) {
		while ((re = getrpcent()) != NULL)
			RPCPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			rpc = strtonum(argv[i], 0, INT_MAX, &err);
			if (!err)
				re = getrpcbynumber(rpc);
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
	const char	*err;
	char		*proto;
	in_port_t	port;
	int		i, rv = RV_OK;

	setservent(1);
	if (argc == 2) {
		while ((se = getservent()) != NULL)
			SERVICESPRINT;
	} else {
		for (i = 2; i < argc; i++) {
			if ((proto = strchr(argv[i], '/')) != NULL)
				*proto++ = '\0';
			port = strtonum(argv[i], 0, IPPORT_HILASTAUTO, &err);
			if (!err)
				se = getservbyport(htons(port), proto);
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
