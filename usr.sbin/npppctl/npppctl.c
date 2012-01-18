/*	$OpenBSD: npppctl.c,v 1.1 2012/01/18 03:13:04 yasuoka Exp $	*/

/*
 * Copyright (c) 2012 Internet Initiative Japan Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <err.h>

#include "parser.h"
#include "npppd_ctl.h"

#ifndef nitems
#define nitems(_x)	(sizeof(_x) / sizeof(_x[0]))
#endif

#define	NMAX_DISCONNECT		2048

static void         usage (void);
static void         on_exit (void);
static void         show_clear_session (struct parse_result *, FILE *);
static void         clear_session (u_int[], int, int, FILE *);
static void         fprint_who_brief (int, struct npppd_who *, FILE *);
static void         fprint_who_packets (int, struct npppd_who *, FILE *);
static void         fprint_who_all (int, struct npppd_who *, FILE *);
static const char  *peerstr (struct sockaddr *, char *, int);
static const char  *humanize_duration (uint32_t, char *, int);
static const char  *humanize_bytes (double, char *, int);
static bool         filter_match(struct parse_result *, struct npppd_who *);

static int	ctlsock = -1;
static char	ctlsockpath[] = "/tmp/npppctl.XXXXXX";
static int	nflag = 0;

static void
usage(void)
{
	extern char		*__progname;

	fprintf(stderr,
	    "usage: %s [-n] [-r rcvbuf_size] [-s socket] [-t timeout_sec] "
	    "command [arg ...]\n", __progname);
}

int
main(int argc, char *argv[])
{
	int			 ch, timeout_sec, rcvbuf;
	struct parse_result	*result;
	struct sockaddr_un	 sun;
	const char		*npppd_ctlpath = NPPPD_CTL_SOCK_PATH;
	struct timeval		 tv;
	extern int		 optind;
	extern char		*optarg;

	rcvbuf = NPPPD_CTL_MSGSZ * 64;
	timeout_sec = 2;
	while ((ch = getopt(argc, argv, "nr:s:t:")) != -1)
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		case 'r':
			if (sscanf(optarg, "%d", &rcvbuf) != 1 || rcvbuf <= 0)
				errx(EXIT_FAILURE, "invalid rcvbuf_size value");
			if (rcvbuf < NPPPD_CTL_MSGSZ)
				errx(EXIT_FAILURE, 
				    "rcvbuf_size can not be less than %d",
				    NPPPD_CTL_MSGSZ);
			break;
		case 's':
			npppd_ctlpath = optarg;
			break;
		case 't':
			if (sscanf(optarg, "%d", &timeout_sec) != 1 ||
			    timeout_sec <= 0)
				errx(EXIT_FAILURE, "invalid timeout value");
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}

	argc -= optind;
	argv += optind;

	if ((result = parse(argc, argv)) == NULL)
		exit(EXIT_FAILURE);

	if ((ctlsock = mkstemp(ctlsockpath)) < 0)
		err(1, "mkstemp");

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	sun.sun_len = sizeof(sun);
	strlcpy(sun.sun_path, ctlsockpath, sizeof(sun.sun_path));

	close(ctlsock);
	unlink(ctlsockpath);

	if ((ctlsock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
		err(EXIT_FAILURE, "socket");
	if (bind(ctlsock, (struct sockaddr *)&sun, sizeof(sun)) != 0)
		err(EXIT_FAILURE, "bind");

	if (setsockopt(ctlsock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf))
	    != 0)
		err(EXIT_FAILURE, "setsockopt(,,SO_RCVBUF)");
	strlcpy(sun.sun_path, npppd_ctlpath, sizeof(sun.sun_path));
	if (connect(ctlsock, (struct sockaddr *)&sun, sizeof(sun)) != 0)
		err(EXIT_FAILURE, "connect");
	/* setup cleaner */
	atexit(on_exit);

	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0L;
	if (setsockopt(ctlsock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)))
		warn("setsockopt(,,SO_SNDTIMEO)");
	tv.tv_sec = timeout_sec;
	tv.tv_usec = 0L;
	if (setsockopt(ctlsock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
		warn("setsockopt(,,SO_RCVTIMEO)");

	switch (result->action) {
	case SESSION_BRIEF:
	case SESSION_PKTS:
	case SESSION_ALL:
		show_clear_session(result, stdout);
		break;
	case CLEAR_SESSION:
		if (!result->has_ppp_id)
			show_clear_session(result, stdout);
		else {
			u_int ids[1];
			ids[0] = result->ppp_id;
			clear_session(ids, 1, 1, stdout);
		}
		break;
	case NONE:
		break;
	}

	exit(EXIT_SUCCESS);
}

static void
on_exit(void)
{
	if (ctlsock >= 0)
		close(ctlsock);
	unlink(ctlsockpath);
}

static void
show_clear_session(struct parse_result *result, FILE *out)
{
	int				 i, n, sz, ppp_id_idx;
	struct npppd_ctl_who_request	 req = { .cmd = NPPPD_CTL_CMD_WHO };
	struct npppd_ctl_who_response	*res;
	u_char				 buf[NPPPD_CTL_MSGSZ + BUFSIZ];
	u_int				 ppp_id[NMAX_DISCONNECT];

	res = (struct npppd_ctl_who_response *)buf;
	if (send(ctlsock, &req, sizeof(req), 0) < 0)
		err(1, "send");
	res->count = 0;
	n = 0;
	ppp_id_idx = 0;
	do {
		if ((sz = recv(ctlsock, &buf, sizeof(buf), MSG_WAITALL)) < 0) {
			if (errno == EAGAIN) {
				warn("recv");
				break;
			}
			err(EXIT_FAILURE, "recv");
		}
		for (i = 0; n < res->count &&
		    offsetof(struct npppd_ctl_who_response, entry[i + 1]) <= sz;
		    i++, n++)
			switch (result->action) {
			case SESSION_BRIEF:
				fprint_who_brief(n, &res->entry[i], out);
				break;
			case SESSION_PKTS:
				fprint_who_packets(n, &res->entry[i], out);
				break;
			case SESSION_ALL:
				if (filter_match(result, &res->entry[i]))
					fprint_who_all(n, &res->entry[i], out);
				break;
			case CLEAR_SESSION:
				if (filter_match(result, &res->entry[i])) {
					if (ppp_id_idx < nitems(ppp_id))
						ppp_id[ppp_id_idx] =
						    res->entry[i].ppp_id;
					ppp_id_idx++;
				}
				break;
			default:
				warnx("must not reached here");
				abort();
			}
	} while (n < res->count);
	if (n > 0 && n < res->count)
		warnx("There are %d sessions in total, but we received only %d "
		    "sessions.  Receive buffer size may not be enough, use -r "
		    "option to increase the size.", res->count, n);
	if (result->action == CLEAR_SESSION)
		clear_session(ppp_id, MIN(ppp_id_idx,  nitems(ppp_id)),
		    ppp_id_idx, out);
}

static void
fprint_who_brief(int i, struct npppd_who *w, FILE *out)
{
	char buf[BUFSIZ];

	if (i == 0)
		fputs(
"Ppp Id     Assigned IPv4   Username             Proto Tunnel From\n"
"---------- --------------- -------------------- ----- ------------------------"
"-\n",
		    out);
	fprintf(out, "%10u %-15s %-20s %-5s %s\n", w->ppp_id,
	    inet_ntoa(w->framed_ip_address), w->username, w->tunnel_proto,
	    peerstr((struct sockaddr *)&w->tunnel_peer, buf, sizeof(buf)));
}

static void
fprint_who_packets(int i, struct npppd_who *w, FILE *out)
{
	if (i == 0)
		fputs(
"Ppd Id     Username             In(Kbytes/pkts/errs)    Out(Kbytes/pkts/errs)"
"\n"
"---------- -------------------- ----------------------- ----------------------"
"-\n",
		    out);
	fprintf(out, "%10u %-20s %9.1f %7u %5u %9.1f %7u %5u\n", w->ppp_id,
	    w->username,
	    (double)w->ibytes/1024, w->ipackets, w->ierrors,
	    (double)w->obytes/1024, w->opackets, w->oerrors);
}

static void
fprint_who_all(int i, struct npppd_who *w, FILE *out)
{
	struct tm		tm;
	char			ibytes_buf[48], obytes_buf[48], peer_buf[48],
				time_buf[48], dur_buf[48];

	localtime_r(&w->time, &tm);
	strftime(time_buf, sizeof(time_buf), "%Y/%m/%d %T", &tm);
	if (i != 0)
		fputs("\n", out);

	fprintf(out,
	    "Ppp Id = %u\n"
	    "          Ppp Id                  : %u\n"
	    "          Username                : %s\n"
	    "          Realm Name              : %s\n"
	    "          Concentrated Interface  : %s\n"
	    "          Assigned IPv4 Address   : %s\n"
	    "          Tunnel Protocol         : %s\n"
	    "          Tunnel From             : %s\n"
	    "          Start Time              : %s\n"
	    "          Elapsed Time            : %lu sec %s\n"
	    "          Input Bytes             : %llu%s\n"
	    "          Input Packets           : %lu\n"
	    "          Input Errors            : %lu (%.1f%%)\n"
	    "          Output Bytes            : %llu%s\n"
	    "          Output Packets          : %lu\n"
	    "          Output Errors           : %lu (%.1f%%)\n",
	    w->ppp_id, w->ppp_id, w->username, w->rlmname, w->ifname,
	    inet_ntoa(w->framed_ip_address), w->tunnel_proto,
	    peerstr((struct sockaddr *)&w->tunnel_peer, peer_buf,
		sizeof(peer_buf)), time_buf,
	    (unsigned long)w->duration_sec,
	    humanize_duration(w->duration_sec, dur_buf, sizeof(dur_buf)),
	    (unsigned long long)w->ibytes,
	    humanize_bytes((double)w->ibytes, ibytes_buf, sizeof(ibytes_buf)),
	    (unsigned long)w->ipackets,
	    (unsigned long)w->ierrors,
	    ((w->ipackets + w->ierrors) <= 0)
		? 0.0 : (100.0 * w->ierrors) / (w->ierrors + w->ipackets),
	    (unsigned long long)w->obytes,
	    humanize_bytes((double)w->obytes, obytes_buf, sizeof(obytes_buf)),
	    (unsigned long)w->opackets,
	    (unsigned long)w->oerrors,
	    ((w->opackets + w->oerrors) <= 0)
		? 0.0 : (100.0 * w->oerrors) / (w->oerrors + w->opackets));
}

/***********************************************************************
 * clear session
 ***********************************************************************/
static void
clear_session(u_int ppp_id[], int ppp_id_count, int total, FILE *out)
{
	int					 succ, fail, i, n, nmax;
	struct npppd_ctl_disconnect_request	 req;
	struct npppd_ctl_disconnect_response	 res;
	struct iovec				 iov[2];
	struct msghdr				 msg;

	succ = fail = 0;
	if (ppp_id_count > 0) {
		nmax = (NPPPD_CTL_MSGSZ -
		    offsetof(struct npppd_ctl_disconnect_request, ppp_id[0])) /
		    sizeof(u_int);
		for (i = 0; i < ppp_id_count; i += n) {
			n = MIN(nmax, ppp_id_count - i);
			req.cmd = NPPPD_CTL_CMD_DISCONNECT;
			req.count = n;
			memset(&msg, 0, sizeof(msg));
			msg.msg_iov = iov;
			msg.msg_iovlen = nitems(iov);
			iov[0].iov_base = &req;
			iov[0].iov_len = offsetof(
			    struct npppd_ctl_disconnect_request, ppp_id[0]);
			iov[1].iov_base = &ppp_id[i];
			iov[1].iov_len = sizeof(u_int) * n;
			if (sendmsg(ctlsock, &msg, 0) < 0)
				err(EXIT_FAILURE, "sendmsg");
			if (recv(ctlsock, &res, sizeof(res), 0) < 0)
				err(EXIT_FAILURE, "recv");
			succ += res.count;
		}
		fail = total - succ;
	}
	if (succ > 0)
		fprintf(out, "Successfully disconnected %d session%s.\n", 
		    succ, (succ > 1)? "s" : "");
	if (fail > 0)
		fprintf(out, "Failed to disconnect %d session%s.\n", 
		    fail, (fail > 1)? "s" : "");
	if (succ == 0 && fail == 0)
		fprintf(out, "No session to disconnect.\n");
}

/***********************************************************************
 * common functions
 ***********************************************************************/
static bool
filter_match(struct parse_result *result, struct npppd_who *who)
{
	if (result->has_ppp_id && result->ppp_id != who->ppp_id)
		return false;

	switch (result->address.ss_family) {
	case AF_INET:
		if (((struct sockaddr_in *)&result->address)->sin_addr.
		    s_addr != who->framed_ip_address.s_addr)
			return false;
		break;
	case AF_INET6:
		/* npppd doesn't support IPv6 yet */
		return false;
	}

	if (result->interface != NULL &&
	    strcmp(result->interface, who->ifname) != 0)
		return false;

	if (result->protocol != PROTO_UNSPEC &&
	    result->protocol != parse_protocol(who->tunnel_proto) )
		return false;

	if (result->realm != NULL && strcmp(result->realm, who->rlmname) != 0)
		return false;

	if (result->username != NULL &&
	    strcmp(result->username, who->username) != 0)
		return false;

	return true;
}

static const char *
peerstr(struct sockaddr *sa, char *buf, int lbuf)
{
	int		 niflags, hasserv;
	char		 hoststr[NI_MAXHOST], servstr[NI_MAXSERV];

	niflags = hasserv = 0;
	if (nflag)
		niflags |= NI_NUMERICHOST;
	if (sa->sa_family == AF_INET || sa->sa_family ==AF_INET6) {
		hasserv = 1;
		niflags |= NI_NUMERICSERV;
	}

	if (sa->sa_family == AF_LINK)
		snprintf(hoststr, sizeof(hoststr),
		    "%02x:%02x:%02x:%02x:%02x:%02x",
		    LLADDR((struct sockaddr_dl *)sa)[0] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[1] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[2] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[3] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[4] & 0xff,
		    LLADDR((struct sockaddr_dl *)sa)[5] & 0xff);
	else
		getnameinfo(sa, sa->sa_len, hoststr, sizeof(hoststr), servstr,
		    sizeof(servstr), niflags);
	
	strlcpy(buf, hoststr, lbuf);
	if (hasserv) {
		strlcat(buf, ":", lbuf);
		strlcat(buf, servstr, lbuf);
	}

	return buf;
}

static const char *
humanize_duration(uint32_t sec, char *buf, int lbuf)
{
	char		fbuf[128];
	int		hour, min;

	hour = sec / (60 * 60);
	min = sec / 60;
	min %= 60;

	if (lbuf <= 0)
		return buf;
	buf[0] = '\0';
	if (hour || min) {
		strlcat(buf, "(", lbuf);
		if (hour) {
			snprintf(fbuf, sizeof(fbuf),
			    "%d hour%s", hour, (hour > 1)? "s" : "");
			strlcat(buf, fbuf, lbuf);
		}
		if (hour && min)
			strlcat(buf, " and ", lbuf);
		if (min) {
			snprintf(fbuf, sizeof(fbuf),
			    "%d minute%s", min, (min > 1)? "s" : "");
			strlcat(buf, fbuf, lbuf);
		}
		strlcat(buf, ")", lbuf);
	}

	return buf;
}

static const char *
humanize_bytes(double val, char *buf, int lbuf)
{
	if (lbuf <= 0)
		return buf;

	if (val >= 1000 * 1024 * 1024)
		snprintf(buf, lbuf, " (%.1f GB)",
		    (double)val / (1024 * 1024 * 1024));
	else if (val >= 1000 * 1024)
		snprintf(buf, lbuf, " (%.1f MB)", (double)val / (1024 * 1024));
	else if (val >= 1000)
		snprintf(buf, lbuf, " (%.1f KB)", (double)val / 1024);
	else
		buf[0] = '\0';
			
	return buf;
}
