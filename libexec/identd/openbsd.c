/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 *
 * $Id: openbsd.c,v 1.12 1998/06/23 11:38:41 deraadt Exp $ 
 * This version elminates the kmem search in favour of a kernel sysctl to
 * get the user id associated with a connection - Bob Beck <beck@obtuse.com>
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>

#include "identd.h"
#include "error.h"


/*
 * Return the user number for the connection owner
 */
int
k_getuid(faddr, fport, laddr, lport, uid)
	struct in_addr *faddr;
	int     fport;
	struct in_addr *laddr;
	int     lport;
	int    *uid;
{
	struct tcp_ident_mapping tir;
	struct sockaddr_in *fin, *lin;
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_IDENT };
	int error = 0;
	size_t i;

	memset(&tir, 0, sizeof (tir));
	tir.faddr.sa_len = sizeof (struct sockaddr);
	tir.laddr.sa_len = sizeof (struct sockaddr);
	tir.faddr.sa_family = AF_INET;
	tir.laddr.sa_family = AF_INET;
	fin = (struct sockaddr_in *) &tir.faddr;
	lin = (struct sockaddr_in *) &tir.laddr;
	
	memcpy(&fin->sin_addr, faddr, sizeof (struct in_addr));
	memcpy(&lin->sin_addr, laddr, sizeof (struct in_addr));
	fin->sin_port = fport;
	lin->sin_port = lport;
	i = sizeof (tir);
	error = sysctl(mib, sizeof (mib) / sizeof (int), &tir, &i, NULL, 0);
	if (!error && tir.ruid != -1) {
		*uid = tir.ruid;
		return (0);
	}
	if (error == -1)
		syslog(LOG_DEBUG, "sysctl failed (%m)");

	return (-1);
}
