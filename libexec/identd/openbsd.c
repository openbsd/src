/* $OpenBSD: openbsd.c,v 1.19 2004/09/16 08:25:05 deraadt Exp $ */

/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 *
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
#include <syslog.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/tcp.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>

#include <arpa/inet.h>

#include "identd.h"

/*
 * Return the user number for the connection owner
 */
int
k_getuid(struct in_addr *faddr, int fport, struct in_addr *laddr,
    int lport, uid_t *uid)
{
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_IDENT };
	struct sockaddr_in *fin, *lin;
	struct tcp_ident_mapping tir;
	int error = 0;
	size_t i;

	memset(&tir, 0, sizeof (tir));
	tir.faddr.ss_len = (sizeof (struct sockaddr_storage) & 0xff);
	tir.laddr.ss_len = (sizeof (struct sockaddr_storage) &0xff);
	tir.faddr.ss_family = AF_INET;
	tir.laddr.ss_family = AF_INET;
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

/*
 * Return the user number for the connection owner
 * New minty IPv6 version.
 */
int
k_getuid6(struct sockaddr_in6 *faddr, int fport, struct sockaddr_in6 *laddr,
    int lport, uid_t *uid)
{
	int mib[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_IDENT };
	struct sockaddr_in6 *fin, *lin;
	struct tcp_ident_mapping tir;
	int error = 0;
	size_t i;

	memset(&tir, 0, sizeof (tir));
	fin = (struct sockaddr_in6 *) &tir.faddr;
	lin = (struct sockaddr_in6 *) &tir.laddr;

	if (faddr->sin6_len > sizeof(tir.faddr))
		return -1;
	memcpy(fin, faddr, faddr->sin6_len);
	if (laddr->sin6_len > sizeof(tir.laddr))
		return -1;
	memcpy(lin, laddr, laddr->sin6_len);
	fin->sin6_port = fport;
	lin->sin6_port = lport;
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
