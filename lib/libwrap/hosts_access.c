/*	$OpenBSD: hosts_access.c,v 1.10 2002/12/31 02:29:17 itojun Exp $	*/

 /*
  * This module implements a simple access control language that is based on
  * host (or domain) names, NIS (host) netgroup names, IP addresses (or
  * network numbers) and daemon process names. When a match is found the
  * search is terminated, and depending on whether PROCESS_OPTIONS is defined,
  * a list of options is executed or an optional shell command is executed.
  * 
  * Host and user names are looked up on demand, provided that suitable endpoint
  * information is available as sockaddr_in structures or TLI netbufs. As a
  * side effect, the pattern matching process may change the contents of
  * request structure fields.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Compile with -DNETGROUP if your library provides support for netgroups.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) hosts_access.c 1.21 97/02/12 02:13:22";
#else
static char rcsid[] = "$OpenBSD: hosts_access.c,v 1.10 2002/12/31 02:29:17 itojun Exp $";
#endif
#endif

/* System libraries. */

#include <sys/types.h>
#include <sys/param.h>
#ifdef INET6
#include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <string.h>
#ifdef NETGROUP
#include <netgroup.h>
#endif
#include <netdb.h>


/* Local stuff. */

#include "tcpd.h"

/* Error handling. */

extern jmp_buf tcpd_buf;

/* Delimiters for lists of daemons or clients. */

static char sep[] = ", \t\r\n";

/* Constants to be used in assignments only, not in comparisons... */

#define	YES		1
#define	NO		0

 /*
  * These variables are globally visible so that they can be redirected in
  * verification mode.
  */

char   *hosts_allow_table = HOSTS_ALLOW;
char   *hosts_deny_table = HOSTS_DENY;
int     hosts_access_verbose = 0;

 /*
  * In a long-running process, we are not at liberty to just go away.
  */

int     resident = (-1);		/* -1, 0: unknown; +1: yes */

/* Forward declarations. */

static int table_match(char *, struct request_info *);
static int list_match(char *, struct request_info *,
    int (*)(char *, struct request_info *));
static int server_match(char *, struct request_info *);
static int client_match(char *, struct request_info *);
static int host_match(char *, struct host_info *);
static int string_match(char *, char *);
static int masked_match(char *, char *, char *);
static int masked_match4(char *, char *, char *);
#ifdef INET6
static int masked_match6(char *, char *, char *);
#endif

/* Size of logical line buffer. */

#define	BUFLEN 2048

/* hosts_access - host access control facility */

int     hosts_access(request)
struct request_info *request;
{
    int     verdict;

    /*
     * If the (daemon, client) pair is matched by an entry in the file
     * /etc/hosts.allow, access is granted. Otherwise, if the (daemon,
     * client) pair is matched by an entry in the file /etc/hosts.deny,
     * access is denied. Otherwise, access is granted. A non-existent
     * access-control file is treated as an empty file.
     * 
     * After a rule has been matched, the optional language extensions may
     * decide to grant or refuse service anyway. Or, while a rule is being
     * processed, a serious error is found, and it seems better to play safe
     * and deny service. All this is done by jumping back into the
     * hosts_access() routine, bypassing the regular return from the
     * table_match() function calls below.
     */

    if (resident <= 0)
	resident++;
    verdict = setjmp(tcpd_buf);
    if (verdict != 0)
	return (verdict == AC_PERMIT);
    if (table_match(hosts_allow_table, request))
	return (YES);
    if (table_match(hosts_deny_table, request))
	return (NO);
    return (YES);
}

/* table_match - match table entries with (daemon, client) pair */

static int table_match(table, request)
char   *table;
struct request_info *request;
{
    FILE   *fp;
    char    sv_list[BUFLEN];		/* becomes list of daemons */
    char   *cl_list;			/* becomes list of clients */
    char   *sh_cmd;			/* becomes optional shell command */
    int     match = NO;
    struct tcpd_context saved_context;

    saved_context = tcpd_context;		/* stupid compilers */

    /*
     * Between the fopen() and fclose() calls, avoid jumps that may cause
     * file descriptor leaks.
     */

    if ((fp = fopen(table, "r")) != 0) {
	tcpd_context.file = table;
	tcpd_context.line = 0;
	while (match == NO && xgets(sv_list, sizeof(sv_list), fp) != 0) {
	    if (sv_list[strlen(sv_list) - 1] != '\n') {
		tcpd_warn("missing newline or line too long");
		continue;
	    }
	    if (sv_list[0] == '#' || sv_list[strspn(sv_list, " \t\r\n")] == 0)
		continue;
	    if ((cl_list = split_at(sv_list, ':')) == 0) {
		tcpd_warn("missing \":\" separator");
		continue;
	    }
	    sh_cmd = split_at(cl_list, ':');
	    match = list_match(sv_list, request, server_match)
		&& list_match(cl_list, request, client_match);
	}
	(void) fclose(fp);
    } else if (errno != ENOENT) {
	tcpd_warn("cannot open %s: %m", table);
    }
    if (match) {
	if (hosts_access_verbose > 1)
	    syslog(LOG_DEBUG, "matched:  %s line %d",
		   tcpd_context.file, tcpd_context.line);
	if (sh_cmd) {
#ifdef PROCESS_OPTIONS
	    process_options(sh_cmd, request);
#else
	    char    cmd[BUFSIZ];
	    shell_cmd(percent_x(cmd, sizeof(cmd), sh_cmd, request));
#endif
	}
    }
    tcpd_context = saved_context;
    return (match);
}

/* list_match - match a request against a list of patterns with exceptions */

static int list_match(list, request, match_fn)
char   *list;
struct request_info *request;
int   (*match_fn)(char *, struct request_info *);
{
    char   *tok;
    static char *last;
    int l;

    /*
     * Process tokens one at a time. We have exhausted all possible matches
     * when we reach an "EXCEPT" token or the end of the list. If we do find
     * a match, look for an "EXCEPT" list and recurse to determine whether
     * the match is affected by any exceptions.
     */

    for (tok = strtok_r(list, sep, &last); tok != 0;
      tok = strtok_r(NULL, sep, &last)) {
	if (STR_EQ(tok, "EXCEPT"))		/* EXCEPT: give up */
	    return (NO);
	l = strlen(tok);
	if (*tok == '[' && tok[l - 1] == ']') {
	    tok[l - 1] = '\0';
	    tok++;
	}
	if (match_fn(tok, request)) {		/* YES: look for exceptions */
	    while ((tok = strtok_r(NULL, sep, &last)) && STR_NE(tok, "EXCEPT"))
		 /* VOID */ ;
	    return (tok == 0 || list_match(NULL, request, match_fn) == 0);
	}
    }
    return (NO);
}

/* server_match - match server information */

static int server_match(tok, request)
char   *tok;
struct request_info *request;
{
    char   *host;

    if ((host = split_at(tok + 1, '@')) == 0) {	/* plain daemon */
	return (string_match(tok, eval_daemon(request)));
    } else {					/* daemon@host */
	return (string_match(tok, eval_daemon(request))
		&& host_match(host, request->server));
    }
}

/* client_match - match client information */

static int client_match(tok, request)
char   *tok;
struct request_info *request;
{
    char   *host;

    if ((host = split_at(tok + 1, '@')) == 0) {	/* plain host */
	return (host_match(tok, request->client));
    } else {					/* user@host */
	return (host_match(host, request->client)
		&& string_match(tok, eval_user(request)));
    }
}

/* host_match - match host name and/or address against pattern */

static int host_match(tok, host)
char   *tok;
struct host_info *host;
{
    char   *mask;

    /*
     * This code looks a little hairy because we want to avoid unnecessary
     * hostname lookups.
     * 
     * The KNOWN pattern requires that both address AND name be known; some
     * patterns are specific to host names or to host addresses; all other
     * patterns are satisfied when either the address OR the name match.
     */

    if (tok[0] == '@') {			/* netgroup: look it up */
#ifdef  NETGROUP
	static char mydomain[MAXHOSTNAMELEN];
	if (mydomain[0] == '\0')
	    getdomainname(mydomain, sizeof(mydomain));
	return (innetgr(tok + 1, eval_hostname(host), NULL, mydomain));
#else
	tcpd_warn("netgroup support is disabled");	/* not tcpd_jump() */
	return (NO);
#endif
    } else if (STR_EQ(tok, "KNOWN")) {		/* check address and name */
	char   *name = eval_hostname(host);
	return (STR_NE(eval_hostaddr(host), unknown) && HOSTNAME_KNOWN(name));
    } else if (STR_EQ(tok, "LOCAL")) {		/* local: no dots in name */
	char   *name = eval_hostname(host);
	return (strchr(name, '.') == 0 && HOSTNAME_KNOWN(name));
    } else if ((mask = split_at(tok, '/')) != 0) {	/* net/mask */
	return (masked_match(tok, mask, eval_hostaddr(host)));
    } else {					/* anything else */
	return (string_match(tok, eval_hostaddr(host))
	    || (NOT_INADDR(tok) && string_match(tok, eval_hostname(host))));
    }
}

/* string_match - match string against pattern */

static int string_match(tok, string)
char   *tok;
char   *string;
{
    int     n;

    if (tok[0] == '.') {			/* suffix */
	n = strlen(string) - strlen(tok);
	return (n > 0 && STR_EQ(tok, string + n));
    } else if (STR_EQ(tok, "ALL")) {		/* all: match any */
	return (YES);
    } else if (STR_EQ(tok, "KNOWN")) {		/* not unknown */
	return (STR_NE(string, unknown));
    } else if (tok[(n = strlen(tok)) - 1] == '.') {	/* prefix */
	return (STRN_EQ(tok, string, n));
    } else {					/* exact match */
	return (STR_EQ(tok, string));
    }
}

/* masked_match - match address against netnumber/netmask */

static int masked_match(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
#ifndef INET6
    return masked_match4(net_tok, mask_tok, string);
#else
    /*
     * masked_match4() is kept just for supporting shortened IPv4 address form.
     * If we could get rid of shortened IPv4 form, we could just always use
     * masked_match6().
     */
    if (dot_quad_addr_new(net_tok, NULL) &&
        dot_quad_addr_new(mask_tok, NULL) &&
        dot_quad_addr_new(string, NULL)) {
	return masked_match4(net_tok, mask_tok, string);
    } else
	return masked_match6(net_tok, mask_tok, string);
#endif
}

static int masked_match4(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
    in_addr_t net;
    in_addr_t mask;
    in_addr_t addr;

    /*
     * Disallow forms other than dotted quad: the treatment that inet_addr()
     * gives to forms with less than four components is inconsistent with the
     * access control language. John P. Rouillard <rouilj@cs.umb.edu>.
     */

    if (!dot_quad_addr_new(string, &addr))
	return (NO);
    if (!dot_quad_addr_new(net_tok, &net) ||
        !dot_quad_addr_new(mask_tok, &mask)) {
	tcpd_warn("bad net/mask expression: %s/%s", net_tok, mask_tok);
	return (NO);				/* not tcpd_jump() */
    }

    if ((net & ~mask) != 0)
	tcpd_warn("host bits not all zero in %s/%s", net_tok, mask_tok);

    return ((addr & mask) == net);
}

#ifdef INET6
static int masked_match6(net_tok, mask_tok, string)
char   *net_tok;
char   *mask_tok;
char   *string;
{
    union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
    } net, mask, addr;
    struct addrinfo hints, *res;
    unsigned long masklen;
    char *ep;
    int i;
    char *np, *mp, *ap;
    int alen;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo(net_tok, "0", &hints, &res) == 0) {
	if (res->ai_addrlen > sizeof(net) || res->ai_next) {
	    freeaddrinfo(res);
	    return NO;
	}
	memcpy(&net, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
    } else
	return NO;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = net.sa.sa_family;
    hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
    hints.ai_flags = AI_NUMERICHOST;
    ep = NULL;
    if (getaddrinfo(mask_tok, "0", &hints, &res) == 0) {
	if (res->ai_family == AF_INET6 &&
	    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id) {
	    freeaddrinfo(res);
	    return NO;
	}
	if (res->ai_addrlen > sizeof(mask) || res->ai_next) {
	    freeaddrinfo(res);
	    return NO;
	}
	memcpy(&mask, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
    } else {
	ep = NULL;
	masklen = strtoul(mask_tok, &ep, 10);
	if (ep && !*ep) {
	    memset(&mask, 0, sizeof(mask));
	    mask.sa.sa_family = net.sa.sa_family;
	    mask.sa.sa_len = net.sa.sa_len;
	    switch (mask.sa.sa_family) {
	    case AF_INET:
		mp = (char *)&mask.sin.sin_addr;
		alen = sizeof(mask.sin.sin_addr);
		break;
	    case AF_INET6:
		mp = (char *)&mask.sin6.sin6_addr;
		alen = sizeof(mask.sin6.sin6_addr);
		break;
	    default:
		return NO;
	    }
	    if (masklen / 8 > alen)
		return NO;
	    memset(mp, 0xff, masklen / 8);
	    if (masklen % 8)
		mp[masklen / 8] = 0xff00 >> (masklen % 8);
	} else
	    return NO;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo(string, "0", &hints, &res) == 0) {
	if (res->ai_addrlen > sizeof(addr) || res->ai_next) {
	    freeaddrinfo(res);
	    return NO;
	}
	/* special case - IPv4 mapped address */
	if (net.sa.sa_family == AF_INET && res->ai_family == AF_INET6 && 
	    IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)res->ai_addr)->sin6_addr)) {
	    memset(&addr, 0, sizeof(addr));
	    addr.sa.sa_family = net.sa.sa_family;
	    addr.sa.sa_len = net.sa.sa_len;
	    memcpy(&addr.sin.sin_addr,
	        &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr.s6_addr[12],
		sizeof(addr.sin.sin_addr));
	} else
	    memcpy(&addr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
    } else
	return NO;

    if (net.sa.sa_family != mask.sa.sa_family ||
        net.sa.sa_family != addr.sa.sa_family) {
	return NO;
    }
     
    switch (net.sa.sa_family) {
    case AF_INET:
	np = (char *)&net.sin.sin_addr;
	mp = (char *)&mask.sin.sin_addr;
	ap = (char *)&addr.sin.sin_addr;
	alen = sizeof(net.sin.sin_addr);
	break;
    case AF_INET6:
	np = (char *)&net.sin6.sin6_addr;
	mp = (char *)&mask.sin6.sin6_addr;
	ap = (char *)&addr.sin6.sin6_addr;
	alen = sizeof(net.sin6.sin6_addr);
	break;
    default:
	return NO;
    }

    for (i = 0; i < alen; i++)
	if (np[i] & ~mp[i]) {
	    tcpd_warn("host bits not all zero in %s/%s", net_tok, mask_tok);
	    break;
	}

    for (i = 0; i < alen; i++)
	ap[i] &= mp[i];

    if (addr.sa.sa_family == AF_INET6 && addr.sin6.sin6_scope_id &&
        addr.sin6.sin6_scope_id != net.sin6.sin6_scope_id)
	return NO;
    return (memcmp(ap, np, alen) == 0);
}
#endif
