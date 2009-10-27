/*	$NetBSD: socket.c,v 1.17 2003/05/26 10:05:07 itojun Exp $	*/

 /*
  * This module determines the type of socket (datagram, stream), the client
  * socket address and port, the server socket address and port. In addition,
  * it provides methods to map a transport address to a printable host name
  * or address. Socket address information results are in static memory.
  * 
  * The result from the hostname lookup method is STRING_PARANOID when a host
  * pretends to have someone elses name, or when a host name is available but
  * could not be verified.
  * 
  * When lookup or conversion fails the result is set to STRING_UNKNOWN.
  * 
  * Diagnostics are reported through syslog(3).
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

/* System libraries. */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>

/* Local stuff. */

#include "tcpd.h"

/* Forward declarations. */

#ifdef APPEND_DOT
static const char *append_dot(const char *);
#endif
static void sock_sink(int);

#ifdef APPEND_DOT
 /*
  * Speed up DNS lookups by terminating the host name with a dot. Should be
  * done with care. The speedup can give problems with lookups from sources
  * that lack DNS-style trailing dot magic, such as local files or NIS maps.
  */

static const char *
append_dot(name)
const char *name;
{
    static char hbuf[MAXHOSTNAMELEN + 1];

    /*
     * Don't append dots to unqualified names. Such names are likely to come
     * from local hosts files or from NIS.
     */

    if (strchr(name, '.') == 0 || strlen(name) + 2 > sizeof(hbuf))
	strlcpy(hbuf, name, sizeof(hbuf));
    else
	snprintf(hbuf, sizeof(hbuf), "%s.", name);
    return hbuf;
}
#endif

/* sock_host - look up endpoint addresses and install conversion methods */

void    sock_host(request)
struct request_info *request;
{
    static struct sockaddr_storage client;
    static struct sockaddr_storage server;
    socklen_t   len;
    char        buf[BUFSIZ];
    int         fd = request->fd;

    sock_methods(request);

    /*
     * Look up the client host address. Hal R. Brand <BRAND@addvax.llnl.gov>
     * suggested how to get the client host info in case of UDP connections:
     * peek at the first message without actually looking at its contents. We
     * really should verify that client.sin_family gets the value AF_INET,
     * but this program has already caused too much grief on systems with
     * broken library code.
     *
     * XXX the last sentence is untrue as we support AF_INET6 as well :-)
     */

    len = sizeof(client);
    if (getpeername(fd, (struct sockaddr *) & client, &len) < 0) {
	request->sink = sock_sink;
	len = sizeof(client);
	if (recvfrom(fd, buf, sizeof(buf), MSG_PEEK,
		     (struct sockaddr *) & client, &len) < 0) {
	    tcpd_warn("can't get client address: %m");
	    return;				/* give up */
	}
#ifdef really_paranoid
	memset(buf, 0, sizeof(buf));
#endif
    }
    request->client->sin = (struct sockaddr *)&client;

    /*
     * Determine the server binding. This is used for client username
     * lookups, and for access control rules that trigger on the server
     * address or name.
     */

    len = sizeof(server);
    if (getsockname(fd, (struct sockaddr *) & server, &len) < 0) {
	tcpd_warn("getsockname: %m");
	return;
    }
    request->server->sin = (struct sockaddr *)&server;
}

/* sock_hostaddr - map endpoint address to printable form */

void    sock_hostaddr(host)
struct host_info *host;
{
    struct sockaddr *sa = host->sin;

    if (!sa)
	return;
    host->addr[0] = '\0';
    getnameinfo(sa, sa->sa_len, host->addr, sizeof(host->addr),
	NULL, 0, NI_NUMERICHOST);
}

/* sock_hostname - map endpoint address to host name */

void    sock_hostname(host)
struct host_info *host;
{
    struct sockaddr *sa = host->sin;
    char h1[NI_MAXHOST], h2[NI_MAXHOST];
    struct addrinfo hints, *res, *res0;
#ifdef INET6
    struct sockaddr_in tmp;
#endif

    if (!sa)
	return;
#ifdef INET6
    /* special case on reverse lookup: mapped addr.  I hate it */
    if (sa->sa_family == AF_INET6 &&
        IN6_IS_ADDR_V4MAPPED(&((struct sockaddr_in6 *)sa)->sin6_addr)) {
	memset(&tmp, 0, sizeof(tmp));
	tmp.sin_family = AF_INET;
	tmp.sin_len = sizeof(struct sockaddr_in);
	memcpy(&tmp.sin_addr,
	    &((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr[12], 4);
	sa = (struct sockaddr *)&tmp;
    }
#endif
    if (getnameinfo(sa, sa->sa_len, h1, sizeof(h1), NULL, 0,
        NI_NUMERICHOST) != 0) {
	return;
    }
    if (getnameinfo(sa, sa->sa_len, host->name, sizeof(host->name), NULL, 0,
        NI_NAMEREQD) == 0) {
	/*
	 * if reverse lookup result looks like a numeric hostname,
	 * someone is trying to trick us by PTR record like following:
	 *	1.1.1.10.in-addr.arpa.  IN PTR  2.3.4.5
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
#ifdef APPEND_DOT
	if (getaddrinfo(append_dot(host->name), "0", &hints, &res0) == 0)
#else
	if (getaddrinfo(host->name, "0", &hints, &res0) == 0)
#endif
	{
	    tcpd_warn("Nasty PTR record is configured");
	    freeaddrinfo(res0);
	    /* name is bad, clobber it */
	    (void)strlcpy(host->name, paranoid, sizeof(host->name));
	    return;
	}
	 
	/*
	 * Verify that the address is a member of the address list returned
	 * by getaddrinfo(hostname).
	 * 
	 * Verify also that getnameinfo() and getaddrinfo() return the same
	 * hostname, or rshd and rlogind may still end up being spoofed.
	 * 
	 * On some sites, getaddrinfo("localhost") returns "localhost.domain".
	 * This is a DNS artefact. We treat it as a special case. When we
	 * can't believe the address list from getaddrinfo("localhost")
	 * we're in big trouble anyway.
	 */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = sa->sa_family;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_CANONNAME;
#ifdef APPEND_DOT
	if (getaddrinfo(append_dot(host->name), "0", &hints, &res0) != 0)
#else
	if (getaddrinfo(host->name, "0", &hints, &res0) != 0)
#endif
	{
	    /*
	     * Unable to verify that the host name matches the address. This
	     * may be a transient problem or a botched name server setup.
	     */

	    tcpd_warn("can't verify hostname: getaddrinfo(%s, %d) failed",
	        host->name, hints.ai_family);
	} else if (res0->ai_canonname &&
	    STR_NE(host->name, res0->ai_canonname) &&
	    STR_NE(host->name, "localhost")) {
	    /*
	     * The getnameinfo() and getaddrinfo() calls did not return
	     * the same hostname. This could be a nameserver configuration
	     * problem. It could also be that someone is trying to spoof us.
	     */

	    tcpd_warn("host name/name mismatch: %s != %s",
		host->name, res0->ai_canonname);
	    freeaddrinfo(res0);
	} else {
	    /*
	     * The address should be a member of the address list returned by
	     * getaddrinfo().
	     */

	    for (res = res0; res; res = res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, h2, sizeof(h2),
		    NULL, 0, NI_NUMERICHOST) != 0) {
		    continue;
		}
		if (STR_EQ(h1, h2)) {
		    freeaddrinfo(res0);
		    return;
		}
	    }

	    /*
	     * The host name does not map to the initial address. Perhaps
	     * someone has messed up. Perhaps someone compromised a name
	     * server.
	     */

	    tcpd_warn("host name/address mismatch: %s != %s", h1,
		res0->ai_canonname ? res0->ai_canonname : "?");

	    freeaddrinfo(res0);
	}
	/* name is bad, clobber it */
	strlcpy(host->name, paranoid, sizeof(host->name));
    }
}

/* sock_sink - absorb unreceived IP datagram */

static void sock_sink(fd)
int     fd;
{
    char    	buf[BUFSIZ];
    struct sockaddr_storage ss;
    socklen_t	size = sizeof(ss);

    /*
     * Eat up the not-yet received datagram. Some systems insist on a
     * non-zero source address argument in the recvfrom() call below.
     */

    (void) recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) & ss, &size);
}
