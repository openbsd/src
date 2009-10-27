/*	$OpenBSD: fix_options.c,v 1.8 2009/10/27 23:59:30 deraadt Exp $	*/

 /*
  * Routine to disable IP-level socket options. This code was taken from 4.4BSD
  * rlogind and kernel source, but all mistakes in it are my fault.
  *
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>

#ifndef IPOPT_OPTVAL
#define IPOPT_OPTVAL	0
#define IPOPT_OLEN	1
#endif

#include "tcpd.h"

#define BUFFER_SIZE	512		/* Was: BUFSIZ */

/* fix_options - get rid of IP-level socket options */

void
fix_options(request)
struct request_info *request;
{
#ifdef IP_OPTIONS
    struct ipoption optbuf;
    char    lbuf[BUFFER_SIZE], *lp, *cp;
    int     ipproto;
    socklen_t optsize = sizeof(optbuf);
    size_t  lps;
    struct protoent *ip;
    int     fd = request->fd;
    int     i;
    struct sockaddr_storage ss;
    socklen_t sslen;

    /*
     * check if this is AF_INET socket
     * XXX IPv6 support?
     */
    sslen = sizeof(ss);
    if (getsockname(fd, (struct sockaddr *)&ss, &sslen) < 0) {
	syslog(LOG_ERR, "getsockname: %m");
	clean_exit(request);
    }
    if (ss.ss_family != AF_INET)
	return;
  
    if ((ip = getprotobyname("ip")) != 0)
	ipproto = ip->p_proto;
    else
	ipproto = IPPROTO_IP;

    if (getsockopt(fd, ipproto, IP_OPTIONS, (char *) &optbuf, &optsize) == 0
	&& optsize != 0) {

	/*
	 * Properly deal with source routing entries.  The original code
	 * here was wrong.
	 */
	for (i = 0; (void *)&optbuf.ipopt_list[i] - (void *)&optbuf <
	    optsize; ) {
		u_char c = (u_char)optbuf.ipopt_list[i];
		if (c == IPOPT_LSRR || c == IPOPT_SSRR) {
			syslog(LOG_WARNING,
			   "refused connect from %s with IP source routing options",
			       eval_client(request));
			clean_exit(request);
		}
		if (c == IPOPT_EOL)
			break;
		i += (c == IPOPT_NOP) ? 1 : (u_char)optbuf.ipopt_list[i+1];
	}

	lp = lbuf;
	lps = sizeof(lbuf);
	for (cp = (char *)&optbuf; optsize > 0 && lps > 0;
	     cp++, optsize--, lp += 3, lps -= 3)
		snprintf(lp, lps, " %2.2x", *cp);
	syslog(LOG_NOTICE,
	       "connect from %s with IP options (ignored):%s",
	       eval_client(request), lbuf);
	if (setsockopt(fd, ipproto, IP_OPTIONS, (char *) 0, optsize) != 0) {
	    syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
	    clean_exit(request);
	}
    }
#endif
}
