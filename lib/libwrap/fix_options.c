/*	$OpenBSD: fix_options.c,v 1.1 1997/02/26 03:06:51 downsj Exp $	*/

 /*
  * Routine to disable IP-level socket options. This code was taken from 4.4BSD
  * rlogind and kernel source, but all mistakes in it are my fault.
  *
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) fix_options.c 1.4 97/02/12 02:13:22";
#else
static char rcsid[] = "$OpenBSD: fix_options.c,v 1.1 1997/02/26 03:06:51 downsj Exp $";
#endif
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
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
    unsigned char optbuf[BUFFER_SIZE / 3], *cp;
    char    lbuf[BUFFER_SIZE], *lp;
    int     optsize = sizeof(optbuf), ipproto;
    struct protoent *ip;
    int     fd = request->fd;
    unsigned int opt;
    int     optlen;
    unsigned char *first_option = optbuf;

    if ((ip = getprotobyname("ip")) != 0)
	ipproto = ip->p_proto;
    else
	ipproto = IPPROTO_IP;

    if (getsockopt(fd, ipproto, IP_OPTIONS, (char *) optbuf, &optsize) == 0
	&& optsize != 0) {

	/*
	 * Horror! 4.[34] BSD getsockopt() prepends the first-hop destination
	 * address to the result IP options list when source routing options
	 * are present (see <netinet/ip_var.h>), but produces no output for
	 * other IP options. Solaris 2.x getsockopt() does produce output for
	 * non-routing IP options, and uses the same format as BSD even when
	 * the space for the destination address is unused. However, we must
	 * be prepared to deal with systems that return the options only. The
	 * code below does the right thing with 4.[34]BSD derivatives and
	 * Solaris 2, but may occasionally miss source routing options on
	 * incompatible systems such as Linux. Their choice.
	 */
#define ADDR_LEN sizeof(struct in_addr)

	for (cp = optbuf + ADDR_LEN; cp < optbuf + optsize; cp++) {
	    opt = cp[IPOPT_OPTVAL];
	    if (opt != IPOPT_NOP) {
		if (opt == IPOPT_RR
		    || opt == IPOPT_TS
		    || opt == IPOPT_SECURITY
		    || opt == IPOPT_LSRR
		    || opt == IPOPT_SATID
		    || opt == IPOPT_SSRR)
		    first_option = cp;
		break;
	    }
	}

	/*
	 * Look for source routing options. Drop the connection when one is
	 * found. Just wiping the IP options is insufficient: we would still
	 * help the attacker by providing a real TCP sequence number, and the
	 * attacker would still be able to send packets (blind spoofing). I
	 * discussed this attack with Niels Provos, half a year before the
	 * attack was described in open mailing lists.
	 * 
	 * It would be cleaner to just return a yes/no reply and let the caller
	 * decide how to deal with it. Resident servers should not terminate.
	 * However I am not prepared to make changes to internal interfaces
	 * on short notice.
	 */
	for (cp = first_option; cp < optbuf + optsize; cp += optlen) {
	    opt = cp[IPOPT_OPTVAL];
	    if (opt == IPOPT_LSRR || opt == IPOPT_SSRR) {
		syslog(LOG_WARNING,
		   "refused connect from %s with IP source routing options",
		       eval_client(request));
		clean_exit(request);
	    }
	    if (opt == IPOPT_EOL)
		break;
	    if (opt == IPOPT_NOP) {
		optlen = 1;
	    } else {
		optlen = cp[IPOPT_OLEN];
		if (optlen <= 0)		/* Do not loop! */
		    break;
	    }
	}
	lp = lbuf;
	for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
	    sprintf(lp, " %2.2x", *cp);
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
