/*	$OpenBSD: scaffold.c,v 1.5 2002/05/07 23:01:07 deraadt Exp $	*/

 /*
  * Routines for testing only. Not really industrial strength.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccs_id[] = "@(#) scaffold.c 1.5 95/01/03 09:13:48";
#else
static char rcsid[] = "$OpenBSD: scaffold.c,v 1.5 2002/05/07 23:01:07 deraadt Exp $";
#endif
#endif

/* System libraries. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>

#include <tcpd.h>

#ifndef INADDR_NONE
#define	INADDR_NONE	(-1)		/* XXX should be 0xffffffff */
#endif

/* Application-specific. */

#include "scaffold.h"

 /*
  * These are referenced by the options module and by rfc931.c.
  */
int     allow_severity = SEVERITY;
int     deny_severity = LOG_WARNING;
int     rfc931_timeout = RFC931_TIMEOUT;

/* dup_hostent - create hostent in one memory block */

static struct hostent *dup_hostent(hp)
struct hostent *hp;
{
    struct hostent_block {
	struct hostent host;
	char   *addr_list[1];
    };
    struct hostent_block *hb;
    int     count;
    char   *data;
    char   *addr;

    for (count = 0; hp->h_addr_list[count] != 0; count++)
	 /* void */ ;

    if ((hb = (struct hostent_block *) malloc(sizeof(struct hostent_block)
			 + (hp->h_length + sizeof(char *)) * count)) == 0) {
	fprintf(stderr, "Sorry, out of memory\n");
	exit(1);
    }
    memset((char *) &hb->host, 0, sizeof(hb->host));
    hb->host.h_addrtype = hp->h_addrtype;
    hb->host.h_length = hp->h_length;
    hb->host.h_addr_list = hb->addr_list;
    hb->host.h_addr_list[count] = 0;
    data = (char *) (hb->host.h_addr_list + count + 1);

    for (count = 0; (addr = hp->h_addr_list[count]) != 0; count++) {
	hb->host.h_addr_list[count] = data + hp->h_length * count;
	memcpy(hb->host.h_addr_list[count], addr, hp->h_length);
    }
    return (&hb->host);
}

/* find_inet_addr - find all addresses for this host, result to free() */

struct hostent *find_inet_addr(host)
char   *host;
{
    struct in_addr addr;
    struct hostent *hp;
    static struct hostent h;
    static char *addr_list[2];
#ifdef INET6
    static struct in6_addr in6;
#endif

    /*
     * Host address: translate it to internal form.
     */
    if (dot_quad_addr_new(host, &addr.s_addr)) {
	h.h_addr_list = addr_list;
	h.h_addr_list[0] = (char *) &addr;
	h.h_length = sizeof(addr);
	h.h_addrtype = AF_INET;
	return (dup_hostent(&h));
    }
#ifdef INET6
    if (inet_pton(AF_INET6, host, &in6) == 1) {
	h.h_addr_list = addr_list;
	h.h_addr_list[0] = (char *) &in6;
	h.h_length = sizeof(in6);
	h.h_addrtype = AF_INET6;
	return (dup_hostent(&h));
    }
#endif

    /*
     * Map host name to a series of addresses. Watch out for non-internet
     * forms or aliases. The NOT_INADDR() is here in case gethostbyname() has
     * been "enhanced" to accept numeric addresses. Make a copy of the
     * address list so that later gethostbyXXX() calls will not clobber it.
     */
#ifdef INET6
    if (NOT_INADDR(host) == 0 && inet_pton(AF_INET6, host, &in6) == 1)
#else
    if (NOT_INADDR(host) == 0)
#endif
    {
	tcpd_warn("%s: not an internet address", host);
	return (0);
    }
#ifdef INET6
    /*
     * XXX this behavior may, or may not be desirable.
     * - we may better use getipnodebyname() to addresses of get both AFs,
     *   however, getipnodebyname() is not widely implemented.
     * - it may be better to have a way to specify the AF to use.
     */
    if ((hp = gethostbyname2(host, AF_INET)) == 0
     && (hp = gethostbyname2(host, AF_INET6)) == 0) {
	tcpd_warn("%s: host not found", host);
	return (0);
    }
#else
    if ((hp = gethostbyname(host)) == 0) {
	tcpd_warn("%s: host not found", host);
	return (0);
    }
    if (hp->h_addrtype != AF_INET) {
	tcpd_warn("%d: not an internet host", hp->h_addrtype);
	return (0);
    }
#endif
    if (STR_NE(host, hp->h_name)) {
	tcpd_warn("%s: hostname alias", host);
	tcpd_warn("(official name: %s)", hp->h_name);
    }
    return (dup_hostent(hp));
}

/* check_dns - give each address thorough workout, return address count */

int     check_dns(host)
char   *host;
{
    struct request_info request;
    struct sockaddr_storage sin;
    struct hostent *hp;
    int     count;
    char   *addr;
    char *ap;
    int alen;

    if ((hp = find_inet_addr(host)) == 0)
	return (0);
    request_init(&request, RQ_CLIENT_SIN, &sin, 0);
    sock_methods(&request);
    memset((char *) &sin, 0, sizeof(sin));
    sin.ss_family = hp->h_addrtype;
    switch (hp->h_addrtype) {
    case AF_INET:
	ap = (char *)&((struct sockaddr_in *)&sin)->sin_addr;
	alen = sizeof(struct in_addr);
	break;
#ifdef INET6
    case AF_INET6:
	ap = (char *)&((struct sockaddr_in6 *)&sin)->sin6_addr;
	alen = sizeof(struct in6_addr);
	break;
#endif
    default:
	return (0);
    }

    for (count = 0; (addr = hp->h_addr_list[count]) != 0; count++) {
	memcpy(ap, addr, alen);

	/*
	 * Force host name and address conversions. Use the request structure
	 * as a cache. Detect hostname lookup problems. Any name/name or
	 * name/address conflicts will be reported while eval_hostname() does
	 * its job.
	 */
	request_set(&request, RQ_CLIENT_ADDR, "", RQ_CLIENT_NAME, "", 0);
	if (STR_EQ(eval_hostname(request.client), unknown))
	    tcpd_warn("host address %s->name lookup failed",
		      eval_hostaddr(request.client));
    }
    free((char *) hp);
    return (count);
}

/* dummy function to intercept the real shell_cmd() */

/* ARGSUSED */

void    shell_cmd(command)
char   *command;
{
    if (hosts_access_verbose)
	printf("command: %s", command);
}

/* dummy function  to intercept the real clean_exit() */

/* ARGSUSED */

void    clean_exit(request)
struct request_info *request;
{
    exit(0);
}

/* dummy function  to intercept the real rfc931() */

/* ARGSUSED */
void    rfc931(a1, a2, d1)
struct sockaddr *a1, *a2;
char *d1;
{
}

/* check_path - examine accessibility */

int     check_path(path, st)
char   *path;
struct stat *st;
{
    struct stat stbuf;
    char    buf[BUFSIZ];

    if (stat(path, st) < 0)
	return (-1);
#ifdef notdef
    if (st->st_uid != 0)
	tcpd_warn("%s: not owned by root", path);
    if (st->st_mode & 020)
	tcpd_warn("%s: group writable", path);
#endif
    if (st->st_mode & 002)
	tcpd_warn("%s: world writable", path);
    if (path[0] == '/' && path[1] != 0) {
	strrchr(strcpy(buf, path), '/')[0] = 0;
	(void) check_path(buf[0] ? buf : "/", &stbuf);
    }
    return (0);
}
