/* $OpenBSD: npppdctl.c,v 1.6 2010/09/22 00:32:48 jsg Exp $ */

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: npppdctl.c,v 1.6 2010/09/22 00:32:48 jsg Exp $ */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <libgen.h>
#include <netdb.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debugutil.h"
#include "npppd_local.h"
#include "npppd_ctl.h"

#ifndef	MIN
#define	MIN(m,n)	((m) < (n))? (m) : (n)
#endif
#define	DEFAULT_TIMEOUT		5

/** Filename template for listening soccket of UNIX domain datagram */
char dgramsock[] = "/tmp/npppdctl.XXXXXX";

/** Daemon side socket address */
struct sockaddr_un peersock;

/** Socket descriptor */
int sock = -1;

/** Show 'since' field as unix time. */
int uflag = 0;

/** Don't convert addresses/ports to names */
int nflag = 0;

/** Receive buffer size */
int rcvbuf_sz = DEFAULT_NPPPD_CTL_MAX_MSGSZ;

/** Use long line to display information */
int lflag = 0;

static void        usage (void);
static void        on_exit (void);
static void        npppd_who (int);
static void        npppd_disconnect (const char *);
static const char  *eat_null (const char *);
static void        npppd_ctl_common(int);
static void        print_who(struct npppd_who *);
static void        print_stat(struct npppd_who *);

static const char *progname = NULL;

/** show usage */
static void
usage(void)
{

	fprintf(stderr,
	"usage: %s [-slnuh] [-d ppp_user] [-r rcvbuf_sz] [-p npppd_ctl_path]\n"
	"usage: %s -R\n"
	    "\t-R: Reset the routing table.\n"
	    "\t-d: Disconnect specified user.\n"
	    "\t-h: Show this usage.\n"
	    "\t-l: Use long line to display information.\n"
	    "\t-n: Don't convert addresses/ports to names.\n"
	    "\t-p: Specify the path to the npppd's control socket.\n"
	    "\t-r: Receive buffer size (default %d).\n"
	    "\t-s: Show statistics informations instead of who.\n"
	    "\t-u: Show 'since' field as unix time.\n",
	    progname, progname, rcvbuf_sz);
}

static void
on_signal(int notused)
{
	exit(1);
}

/** entry point of 'npppdctl' command */
int
main(int argc, char *argv[])
{
	int ch, sflag, fdgramsock, rtflag;
	const char *path = DEFAULT_NPPPD_CTL_SOCK_PATH;
	const char *disconn;
	struct sockaddr_un sun;
	struct timeval tv;
	extern int optind;

	progname = basename(argv[0]);
	disconn = NULL;
	sflag = rtflag = 0;
	while ((ch = getopt(argc, argv, "ld:sunhp:r:R")) != -1) {
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		case 'd':
			disconn = optarg;
			break;
		case 's':
			sflag = 1;
			break;
		case 'u':
			uflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'r':
			if (sscanf(optarg, "%d", &rcvbuf_sz) != 1) {
				fprintf(stderr, "Invalid argument: %s\n",
				    optarg);
				exit(1);
			}
			break;
		case 'p':
			path = optarg;
			break;
		case 'R':
			rtflag = 1;
			break;
		case 'h':
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	/* create a listening socket for unix domain datagram. */
	if ((fdgramsock = mkstemp(dgramsock)) < 0)
		err(1, "mkstemp");

	/* set the hook that deletes the listening socket on exiting */
	if (atexit(on_exit) != 0)
		err(1, "atexit");
	signal(SIGINT, on_signal);
	signal(SIGHUP, on_signal);
	signal(SIGTERM, on_signal);

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");
	tv.tv_sec = DEFAULT_TIMEOUT;
	tv.tv_usec = 0;

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf_sz,
	    sizeof(rcvbuf_sz)) < 0)
		err(1, "setsockopt(SO_RCVBUF)");
	if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		err(1, "setsockopt(SO_RCVTIMEO)");

	/* Prepare a listening socket */
	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, dgramsock, sizeof(sun.sun_path));
	/* delete the file that is created by mkstemp */
	close(fdgramsock); unlink(dgramsock);

	if (bind(sock, (struct sockaddr *)&sun, sizeof(sun)) != 0)
		err(1, "bind()");
	if (chmod(dgramsock, 0600) != 0)
		err(1, "chmod(%s,%d)", dgramsock, 0600);

	/* Prepare a socket for sending to the daemon */
	memset(&peersock, 0, sizeof(peersock));
	peersock.sun_family = AF_UNIX;
	peersock.sun_len = sizeof(peersock);
	strlcpy(peersock.sun_path, path, sizeof(peersock.sun_path));

	if (disconn != NULL)
		npppd_disconnect(disconn);
	else if (sflag > 0)
		npppd_who(1);
	else if (rtflag)
		npppd_ctl_common(NPPPD_CTL_CMD_RESET_ROUTING_TABLE);
	else
		npppd_who(0);

	close(sock);
	sock = -1;

	return 0;
}

/** exiting hook.  delete the listening socket */
static void
on_exit(void)
{
	if (sock >= 0)
		close(sock);
	unlink(dgramsock);
}

/**
 * This function displays connected ppp link one by one.
<pre>
name             assigned         since         proto  from
username         10.0.0.116       Aug 02 21:10  L2TP   192.168.159.103:1701
</pre></p>
 */
static void
npppd_who(int show_stat)
{
	int i, n, sz, command;
	struct npppd_who_list *l;

	if ((l = malloc(rcvbuf_sz)) == NULL)
		err(1, "malloc(%d)", rcvbuf_sz);

	command = NPPPD_CTL_CMD_WHO;
	if (sendto(sock, &command, sizeof(command), 0,
	    (struct sockaddr *)&peersock, sizeof(peersock)) < 0)
		err(1 ,"sendto() failed");

	if (!show_stat)
		printf("name             assigned         since         proto  "
			"from\n");
	else
		printf("id       name                  in(Kbytes/pkts/errs)"
		    "     out(Kbytes/pkts/errs)\n");

	n = 0;
	l->count = -1;
	do {
		if ((sz = recv(sock, l, rcvbuf_sz, 0)) <= 0)
			break;
		for (i = 0; n < l->count &&
		    offsetof(struct npppd_who_list, entry[i + 1]) <= sz;
		    i++, n++) {
			if (show_stat)
				print_stat(&l->entry[i]);
			else
				print_who(&l->entry[i]);
		}
	} while (i < l->count);

	if (l->count >= 0) {
		if (n < l->count)
			warn("Warning: received message is truncated.  "
			    "Received %d user informations, but %d users are "
			    "active.", n, l->count);
	} else {
		warn("recv");
	}

	free(l);
}

static void
print_who(struct npppd_who *w)
{
	int niflags, hasserv;
	char assign_ip4buf[16];
	char timestr[64], hoststr[NI_MAXHOST], servstr[NI_MAXSERV];
	struct sockaddr *sa;

	if(nflag)
		niflags = NI_NUMERICHOST;
	else
		niflags = 0;

	strlcpy(assign_ip4buf, inet_ntoa(w->assign_ip4), sizeof(assign_ip4buf));
	if(!uflag)
		strftime(timestr, sizeof(timestr), "%b %d %H:%M",
		    localtime(&w->time));
	else
		snprintf(timestr, sizeof(timestr), "%ld", (long)w->time);

	sa = (struct sockaddr *)&w->phy_info;

	hasserv = (sa->sa_family == AF_INET || sa->sa_family ==AF_INET6)? 1 : 0;
	if (sa->sa_family == AF_LINK) {
		snprintf(hoststr, sizeof(hoststr),
		    "%02x:%02x:%02x:%02x:%02x:%02x",
		    LLADDR((struct sockaddr_dl *)sa)[0] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[1] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[2] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[3] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[4] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[5] & 0xff);
	} else if (sa->sa_family < AF_MAX) {
		getnameinfo((const struct sockaddr *)&w->phy_info,
		    sa->sa_len, hoststr, sizeof(hoststr), servstr,
		    sizeof(servstr),
		    niflags | ((hasserv)? NI_NUMERICSERV :0));
	} else if (sa->sa_family == NPPPD_AF_PHONE_NUMBER) {
		strlcpy(hoststr, ((npppd_phone_number *)sa)->pn_number,
		    sizeof(hoststr));
	} else {
		strlcpy(hoststr, "error", sizeof(hoststr));
	}

	if (hasserv)
		printf((lflag)
		    ? "%-15s  %-15s  %-12s  %-6s %s:%s\n"
		    : "%-15.15s  %-15s  %-12s  %-6s %s:%s\n",
		    eat_null(w->name), assign_ip4buf, timestr, w->phy_label,
		    hoststr, servstr);
	else
		printf((lflag)
		    ? "%-15s  %-15s  %-12s  %-6s %s\n"
		    : "%-15.15s  %-15s  %-12s  %-6s %s\n",
		    eat_null(w->name), assign_ip4buf, timestr, w->phy_label,
		    hoststr);
}

static void
print_stat(struct npppd_who *w)
{
	printf((lflag)
	    ? "%7d  %-20s  %9.1f %7u %5u  %9.1f %7u %5u\n"
	    : "%7d  %-20.20s  %9.1f %7u %5u  %9.1f %7u %5u\n", w->id,
	    eat_null(w->name), (double)w->ibytes/1024, w->ipackets, w->ierrors,
	    (double)w->obytes/1024, w->opackets, w->oerrors);
}

/** disconnect by username */
static void
npppd_disconnect(const char *username)
{
	int sz, len;
	u_char buf[BUFSIZ];
	struct npppd_disconnect_user_req *req;

	req = (struct npppd_disconnect_user_req *)buf;
	req->command = NPPPD_CTL_CMD_DISCONNECT_USER;
	len = sizeof(struct npppd_disconnect_user_req);
	len += strlcpy(req->username, username, sizeof(req->username));

	if (sendto(sock, buf, sizeof(struct npppd_disconnect_user_req), 0,
	    (struct sockaddr *)&peersock, sizeof(peersock)) < 0)
		err(1, "sendto");
	if ((sz = recv(sock, buf, sizeof(buf), 0)) <= 0)
		err(1, "recv");

	buf[sz] = '\0';
	printf("%s\n", buf);
}

/**
 * make sure "str" is not NULL or not zero length string.
 * <p>
 * When NULL pointer or zero length string is specified as "str", this function
 * returns "<none>".  Otherwise it returns the "str" without modification.</p>
 */
static const char *
eat_null(const char *str)
{
	if (str == NULL || *str == '\0')
		return "<none>";
	return str;
}

static void
npppd_ctl_common(int command)
{
	int sz;
	u_char buf[BUFSIZ];

	if (sendto(sock, &command, sizeof(command), 0,
	    (struct sockaddr *)&peersock, sizeof(peersock)) < 0) {
		err(1 ,"sendto() failed");
	}

	if ((sz = recv(sock, buf, sizeof(buf), 0)) <= 0)
		err(1, "recv");
	buf[sz] = '\0';

	printf("%s\n", buf);
}
