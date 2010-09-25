/*	$OpenBSD: iscsictl.c,v 1.2 2010/09/25 16:23:01 sobrado Exp $ */

/*
 * Copyright (c) 2010 Claudio Jeker <claudio@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <event.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "iscsictl.h"

__dead void	 usage(void);
void		 run_command(int, struct pdu *);
struct pdu	*ctl_getpdu(char *, size_t);
int		 ctl_sendpdu(int, struct pdu *);

char		cbuf[CONTROL_READ_SIZE];

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,"usage: %s [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

int
main (int argc, char* argv[])
{
	struct sockaddr_un sun;
	struct parse_result *res;
	char *confname = ISCSID_CONFIG;
	char *sockname = ISCSID_CONTROL;
	struct pdu *pdu;
	struct ctrlmsghdr *cmh;
	struct session_config *sc;
	struct session_ctlcfg *s;
	struct iscsi_config *cf;
	char *tname, *iname;
	int ch, csock;

	/* check flags */
	while ((ch = getopt(argc, argv, "f:s:")) != -1) {
		switch (ch) {
		case 'f':
			confname = optarg;
			break;
		case 's':
			sockname = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* parse options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	/* connect to ospfd control socket */
	if ((csock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path));

	if (connect(csock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	switch (res->action) {
	case NONE:
	case LOG_VERBOSE:
	case LOG_BRIEF:
	case SHOW:
	case SHOW_SUM:
		usage();
		/* NOTREACHED */
	case RELOAD:
		if ((cf = parse_config(confname)) == NULL)
			errx(1, "errors while loading configuration file.");
		SIMPLEQ_FOREACH(s, &cf->sessions, entry) {
			if ((pdu = pdu_new()) == NULL)
				err(1, "pdu_new");
			if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
				err(1, "pdu_alloc");
			bzero(cmh, sizeof(*cmh));
			cmh->type = CTRL_SESSION_CONFIG;
			cmh->len[0] = sizeof(*sc);
			if ((sc = pdu_dup(&s->session, sizeof(s->session))) ==
			    NULL)
				err(1, "pdu_dup");
			if (s->session.TargetName) {
				if ((tname = pdu_dup(s->session.TargetName,
				    strlen(s->session.TargetName) + 1)) ==
				    NULL)
					err(1, "pdu_dup");
				cmh->len[1] = strlen(s->session.TargetName) + 1;
			} else
				tname = NULL;
			if (s->session.InitiatorName) {
				if ((iname = pdu_dup(s->session.InitiatorName,
				    strlen(s->session.InitiatorName) + 1)) ==
				    NULL)
					err(1, "pdu_dup");
				cmh->len[2] = strlen(s->session.InitiatorName)
				    + 1;
			} else
				iname = NULL;
			pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
			pdu_addbuf(pdu, sc, sizeof(*sc), 1);
			if (tname)
				pdu_addbuf(pdu, tname, strlen(tname) + 1, 2);
			if (iname)
				pdu_addbuf(pdu, iname, strlen(iname) + 1, 3);

			run_command(csock, pdu);
		}
		break;
	case DISCOVERY:
		printf("discover %s\n", log_sockaddr(&res->addr));
		if ((pdu = pdu_new()) == NULL)
			err(1, "pdu_new");
		if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
			err(1, "pdu_alloc");
		if ((sc = pdu_alloc(sizeof(*sc))) == NULL)
			err(1, "pdu_alloc");
		bzero(cmh, sizeof(*cmh));
		bzero(sc, sizeof(*sc));
		snprintf(sc->SessionName, sizeof(sc->SessionName),
		    "discovery.%d", (int)getpid());
		bcopy(&res->addr, &sc->connection.TargetAddr, res->addr.ss_len);
		sc->SessionType = SESSION_TYPE_DISCOVERY;
		cmh->type = CTRL_SESSION_CONFIG;
		cmh->len[0] = sizeof(*sc);
		pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
		pdu_addbuf(pdu, sc, sizeof(*sc), 1);

		run_command(csock, pdu);
	}
printf("sent pdu to daemon\n");

	close(csock);

	return (0);
}

void
run_command(int csock, struct pdu *pdu)
{
	struct ctrlmsghdr *cmh;
	int done = 0;
	ssize_t n;

	if (ctl_sendpdu(csock, pdu) == -1)
		err(1, "send");
	while (!done) {
		if ((n = recv(csock, cbuf, sizeof(cbuf), 0)) == -1 &&
		    !(errno == EAGAIN || errno == EINTR))
			err(1, "recv");

		if (n == 0)
			errx(1, "connection to iscsid closed");

		pdu = ctl_getpdu(cbuf, n);
		cmh = pdu_getbuf(pdu, NULL, 0);
			if (cmh == NULL)
				break;
		switch (cmh->type) {
		case CTRL_SUCCESS:
			printf("command successful\n");
			done = 1;
			break;
		case CTRL_FAILURE:
			printf("command failed\n");
			done = 1;
			break;
		}
	}
}

struct pdu *
ctl_getpdu(char *buf, size_t len)
{
	struct pdu *p;
	struct ctrlmsghdr *cmh;
	void *data;
	size_t n;
	int i;

	if (len < sizeof(*cmh))
		return NULL;

	if (!(p = pdu_new()))
		return NULL;

	n = sizeof(*cmh);
	cmh = pdu_alloc(n);
	bcopy(buf, cmh, n);
	buf += n;
	len -= n;

	if (pdu_addbuf(p, cmh, n, 0)) {
		free(cmh);
fail:
		pdu_free(p);
		return NULL;
	}

	for (i = 0; i < 3; i++) {
		n = cmh->len[i];
		if (n == 0)
			continue;
		if (PDU_LEN(n) > len)
			goto fail;
		if (!(data = pdu_alloc(n)))
			goto fail;
		bcopy(buf, data, n);
		if (pdu_addbuf(p, data, n, i + 1)) {
			free(data);
			goto fail;
		}
		buf += PDU_LEN(n);
		len -= PDU_LEN(n);
	}

	return p;
}

int
ctl_sendpdu(int fd, struct pdu *pdu)
{
	struct iovec iov[PDU_MAXIOV];
	struct msghdr msg;
	unsigned int niov = 0;

	for (niov = 0; niov < PDU_MAXIOV; niov++) {
		iov[niov].iov_base = pdu->iov[niov].iov_base;
		iov[niov].iov_len = pdu->iov[niov].iov_len;
	}
	bzero(&msg, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = niov;
	if (sendmsg(fd, &msg, 0) == -1)
		return -1;
	return 0;
}
