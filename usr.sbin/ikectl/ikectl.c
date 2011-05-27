/*	$OpenBSD: ikectl.c,v 1.12 2011/05/27 12:01:02 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@vantronix.net>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/un.h>
#include <sys/tree.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <event.h>

#include "iked.h"
#include "parser.h"

__dead void	 usage(void);

struct imsgname {
	int type;
	char *name;
	void (*func)(struct imsg *);
};

struct imsgname *monitor_lookup(u_int8_t);
void		 monitor_id(struct imsg *);
int		 monitor(struct imsg *);

int		 ca_opt(struct parse_result *);

struct imsgname imsgs[] = {
	{ IMSG_CTL_OK,			"ok",			NULL },
	{ IMSG_CTL_FAIL,		"fail",			NULL },
	{ IMSG_CTL_VERBOSE,		"verbose",		NULL },
	{ IMSG_CTL_RELOAD,		"reload",		NULL },
	{ IMSG_CTL_RESET,		"reset",		NULL },
	{ 0,				NULL,			NULL }

};
struct imsgname imsgunknown = {
	-1,				"<unknown>",		NULL
};

struct imsgbuf	*ibuf;
struct snmpd	*env;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-q] [-s socket] command [arg ...]\n",
	    __progname);
	exit(1);
}

int
ca_opt(struct parse_result *res)
{
	struct ca	*ca;
	size_t		 len;
	char		*p;

	ca = ca_setup(res->caname, (res->action == CA_CREATE),
	    res->quiet, res->pass);
	if (ca == NULL)
		errx(1, "ca_setup failed");

	/* assume paths are relative to /etc if not absolute */
	if (res->path && (res->path[0] != '.') && (res->path[0] != '/')) {
		len = 5 + strlen(res->path) + 1;
		if ((p = malloc(len)) == NULL)
			err(1, "malloc");
		snprintf(p, len, "/etc/%s", res->path);
		free(res->path);
		res->path = p;
	}

	switch (res->action) {
	case CA_CREATE:
		ca_create(ca);
		break;
	case CA_DELETE:
		ca_delete(ca);
		break;
	case CA_INSTALL:
		ca_install(ca, res->path);
		break;
	case CA_EXPORT:
		ca_export(ca, NULL, res->peer, res->pass);
		break;
	case CA_CERT_CREATE:
	case CA_SERVER:
	case CA_CLIENT:
		ca_certificate(ca, res->host, res->htype, res->action);
		break;
	case CA_CERT_DELETE:
		ca_delkey(ca, res->host);
		break;
	case CA_CERT_INSTALL:
		ca_cert_install(ca, res->host, res->path);
		break;
	case CA_CERT_EXPORT:
		ca_export(ca, res->host, res->peer, res->pass);
		break;
	case CA_CERT_REVOKE:
		ca_revoke(ca, res->host);
		break;
	case SHOW_CA_CERTIFICATES:
		ca_show_certs(ca, res->host);
		break;
	case CA_KEY_CREATE:
		ca_key_create(ca, res->host);
		break;
	case CA_KEY_DELETE:
		ca_key_delete(ca, res->host);
		break;
	case CA_KEY_INSTALL:
		ca_key_install(ca, res->host, res->path);
		break;
	case CA_KEY_IMPORT:
		ca_key_import(ca, res->host, res->path);
		break;
	default:
		break;
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	 sun;
	struct parse_result	*res;
	struct imsg		 imsg;
	int			 ctl_sock;
	int			 done = 1;
	int			 n;
	int			 ch;
	int			 v = 0;
	int			 quiet = 0;
	const char		*sock = IKED_SOCKET;

	if ((env = calloc(1, sizeof(struct snmpd *))) == NULL)
		err(1, "calloc");

	while ((ch = getopt(argc, argv, "qs:")) != -1) {
		switch (ch) {
		case 'q':
			quiet = 1;
			break;
		case 's':
			sock = optarg;
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

	res->quiet = quiet;

	switch (res->action) {
	case CA_CREATE:
	case CA_DELETE:
	case CA_INSTALL:
	case CA_EXPORT:
	case CA_CERT_CREATE:
	case CA_CLIENT:
	case CA_SERVER:
	case CA_CERT_DELETE:
	case CA_CERT_INSTALL:
	case CA_CERT_EXPORT:
	case CA_CERT_REVOKE:
	case SHOW_CA:
	case SHOW_CA_CERTIFICATES:
	case CA_KEY_CREATE:
	case CA_KEY_DELETE:
	case CA_KEY_INSTALL:
	case CA_KEY_IMPORT:
		ca_opt(res);
		break;
	case NONE:
		usage();
		break;
	default:
		goto connect;
	}

	free(env);
	return (0);

 connect:
	/* connect to snmpd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sock, sizeof(sun.sun_path));
 reconnect:
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		/* Keep retrying if running in monitor mode */
		if (res->action == MONITOR &&
		    (errno == ENOENT || errno == ECONNREFUSED)) {
			usleep(100);
			goto reconnect;
		}
		err(1, "connect: %s", sock);
	}

	if (res->ibuf != NULL)
		ibuf = res->ibuf;
	else
		if ((ibuf = malloc(sizeof(struct imsgbuf))) == NULL)
			err(1, "malloc");
	imsg_init(ibuf, ctl_sock);

	/* process user request */
	switch (res->action) {
	case RESETALL:
		v = RESET_ALL;
		break;
	case RESETCA:
		v = RESET_CA;
		break;
	case RESETPOLICY:
		v = RESET_POLICY;
		break;
	case RESETSA:
		v = RESET_SA;
		break;
	case RESETUSER:
		v = RESET_USER;
		break;
	case LOG_VERBOSE:
		v = 2;
		break;
	case LOG_BRIEF:
	default:
		v = 0;
		break;
	}

	switch (res->action) {
	case NONE:
		usage();
		/* NOTREACHED */
		break;
	case RESETALL:
	case RESETCA:
	case RESETPOLICY:
	case RESETSA:
	case RESETUSER:
		imsg_compose(ibuf, IMSG_CTL_RESET, 0, 0, -1, &v, sizeof(v));
		printf("reset request sent.\n");
		break;
	case LOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1,
		    res->path, strlen(res->path));
		break;
	case RELOAD:
		imsg_compose(ibuf, IMSG_CTL_RELOAD, 0, 0, -1, NULL, 0);
		break;
	case MONITOR:
		imsg_compose(ibuf, IMSG_CTL_NOTIFY, 0, 0, -1, NULL, 0);
		done = 0;
		break;
	case COUPLE:
		imsg_compose(ibuf, IMSG_CTL_COUPLE, 0, 0, -1, NULL, 0);
		break;
	case DECOUPLE:
		imsg_compose(ibuf, IMSG_CTL_DECOUPLE, 0, 0, -1, NULL, 0);
		break;
	case ACTIVE:
		imsg_compose(ibuf, IMSG_CTL_ACTIVE, 0, 0, -1, NULL, 0);
		break;
	case PASSIVE:
		imsg_compose(ibuf, IMSG_CTL_PASSIVE, 0, 0, -1, NULL, 0);
		break;
	case LOG_VERBOSE:
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_VERBOSE, 0, 0, -1, &v, sizeof(v));
		printf("logging request sent.\n");
		break;
	default:
		break;
	}

	while (ibuf->w.queued)
		if (msgbuf_write(&ibuf->w) < 0)
			err(1, "write error");

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");

		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				errx(1, "imsg_get error");
			if (n == 0)
				break;
			switch (res->action) {
			case MONITOR:
				done = monitor(&imsg);
				break;
			default:
				break;
			}
			imsg_free(&imsg);
		}
	}
	close(ctl_sock);
	free(ibuf);

	return (0);
}

struct imsgname *
monitor_lookup(u_int8_t type)
{
	int i;

	for (i = 0; imsgs[i].name != NULL; i++)
		if (imsgs[i].type == type)
			return (&imsgs[i]);
	return (&imsgunknown);
}

int
monitor(struct imsg *imsg)
{
	time_t			 now;
	int			 done = 0;
	struct imsgname		*imn;

	now = time(NULL);

	imn = monitor_lookup(imsg->hdr.type);
	printf("%s: imsg type %u len %u peerid %u pid %d\n", imn->name,
	    imsg->hdr.type, imsg->hdr.len, imsg->hdr.peerid, imsg->hdr.pid);
	printf("\ttimestamp: %u, %s", now, ctime(&now));
	if (imn->type == -1)
		done = 1;
	if (imn->func != NULL)
		(*imn->func)(imsg);

	return (done);
}
