/*	$OpenBSD: smtpctl.c,v 1.62 2011/08/16 19:02:03 gilles Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/un.h>
#include <sys/param.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "parser.h"

void usage(void);
static int show_command_output(struct imsg*);
static int show_stats_output(struct imsg *);

int proctype;
struct imsgbuf	*ibuf;

int sendmail = 0;
extern char *__progname;

struct smtpd	*env = NULL;

__dead void
usage(void)
{
	extern char *__progname;

	if (sendmail)
		fprintf(stderr, "usage: %s [-tv] [-f from] [-F name] to ..\n",
		    __progname);
	else
		fprintf(stderr, "usage: %s command [argument ...]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un	sun;
	struct parse_result	*res = NULL;
	struct imsg		imsg;
	int			ctl_sock;
	int			done = 0;
	int			n, verbose = 0;

	/* parse options */
	if (strcmp(__progname, "sendmail") == 0 || strcmp(__progname, "send-mail") == 0)
		sendmail = 1;
	else if (strcmp(__progname, "mailq") == 0) {
		if (geteuid())
			errx(1, "need root privileges");
		show_queue(PATH_QUEUE, 0);
		return 0;
	} else if (strcmp(__progname, "smtpctl") == 0) {
		/* check for root privileges */
		if (geteuid())
			errx(1, "need root privileges");

		if ((res = parse(argc - 1, argv + 1)) == NULL)
			exit(1);

		/* handle "disconnected" commands */
		switch (res->action) {
		case SHOW_QUEUE:
			show_queue(PATH_QUEUE, 0);
			break;
		case SHOW_RUNQUEUE:
			break;
		default:
			goto connected;
		}
		return 0;
	} else
		errx(1, "unsupported mode");

connected:
	/* connect to smtpd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SMTPD_SOCKET, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		if (sendmail)
			return enqueue_offline(argc, argv);
		err(1, "connect: %s", SMTPD_SOCKET);
	}

	if ((ibuf = calloc(1, sizeof(struct imsgbuf))) == NULL)
		err(1, NULL);
	imsg_init(ibuf, ctl_sock);

	if (sendmail)
		return enqueue(argc, argv);

	/* process user request */
	switch (res->action) {
	case NONE:
		usage();
		/* not reached */

	case SCHEDULE:
	case REMOVE: {
		u_int64_t ulval;
		char *ep;

		errno = 0;
		ulval = strtoul(res->data, &ep, 16);
		if (res->data[0] == '\0' || *ep != '\0')
			errx(1, "invalid msgid/evpid");
		if (errno == ERANGE && ulval == ULLONG_MAX)
			errx(1, "invalid msgid/evpid");

		if (res->action == SCHEDULE)
			imsg_compose(ibuf, IMSG_RUNNER_SCHEDULE, 0, 0, -1, &ulval,
			    sizeof(ulval));
		if (res->action == REMOVE)
			imsg_compose(ibuf, IMSG_RUNNER_REMOVE, 0, 0, -1, &ulval,
			    sizeof(ulval));
		break;
	}

	case SHUTDOWN:
		imsg_compose(ibuf, IMSG_CTL_SHUTDOWN, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_MDA:
		imsg_compose(ibuf, IMSG_QUEUE_PAUSE_LOCAL, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_MTA:
		imsg_compose(ibuf, IMSG_QUEUE_PAUSE_OUTGOING, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_SMTP:
		imsg_compose(ibuf, IMSG_SMTP_PAUSE, 0, 0, -1, NULL, 0);
		break;
	case RESUME_MDA:
		imsg_compose(ibuf, IMSG_QUEUE_RESUME_LOCAL, 0, 0, -1, NULL, 0);
		break;
	case RESUME_MTA:
		imsg_compose(ibuf, IMSG_QUEUE_RESUME_OUTGOING, 0, 0, -1, NULL, 0);
		break;
	case RESUME_SMTP:
		imsg_compose(ibuf, IMSG_SMTP_RESUME, 0, 0, -1, NULL, 0);
		break;
	case SHOW_STATS:
		imsg_compose(ibuf, IMSG_STATS, 0, 0, -1, NULL, 0);
		break;
	case MONITOR:
		/* XXX */
		break;
	case LOG_VERBOSE:
		verbose = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		imsg_compose(ibuf, IMSG_CTL_VERBOSE, 0, 0, -1, &verbose,
		    sizeof(verbose));
		printf("logging request sent.\n");
		done = 1;
		break;
	default:
		err(1, "unknown request (%d)", res->action);
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
			switch(res->action) {
			/* case RELOAD: */
			case REMOVE:
			case SCHEDULE:
			case SHUTDOWN:
			case PAUSE_MDA:
			case PAUSE_MTA:
			case PAUSE_SMTP:
			case RESUME_MDA:
			case RESUME_MTA:
			case RESUME_SMTP:
			case LOG_VERBOSE:
			case LOG_BRIEF:
				done = show_command_output(&imsg);
				break;
			case SHOW_STATS:
				done = show_stats_output(&imsg);
				break;
			case NONE:
				break;
			case MONITOR:
				break;
			default:
				err(1, "unexpected reply (%d)", res->action);
			}
			/* insert imsg replies switch here */

			imsg_free(&imsg);
		}
	}
	close(ctl_sock);
	free(ibuf);

	return (0);
}

static int
show_command_output(struct imsg *imsg)
{
	switch (imsg->hdr.type) {
	case IMSG_CTL_OK:
		printf("command succeeded\n");
		break;
	case IMSG_CTL_FAIL:
		printf("command failed\n");
		break;
	default:
		errx(1, "wrong message in summary: %u", imsg->hdr.type);
	}
	return (1);
}

static int
show_stats_output(struct imsg *imsg)
{
	struct stats	*stats;

	if (imsg->hdr.type != IMSG_STATS)
		errx(1, "show_stats_output: bad hdr type (%d)", imsg->hdr.type);
	
	if (IMSG_DATA_SIZE(imsg) != sizeof(*stats))
		errx(1, "show_stats_output: bad data size");

	stats = imsg->data;

	printf("control.sessions=%zd\n", stats->control.sessions);
	printf("control.sessions_active=%zd\n",
	    stats->control.sessions_active);
	printf("control.sessions_maxactive=%zd\n",
	    stats->control.sessions_maxactive);

	printf("mda.sessions=%zd\n", stats->mda.sessions);
	printf("mda.sessions.active=%zd\n", stats->mda.sessions_active);
	printf("mda.sessions.maxactive=%zd\n", stats->mda.sessions_maxactive);

	printf("mta.sessions=%zd\n", stats->mta.sessions);
	printf("mta.sessions.active=%zd\n", stats->mta.sessions_active);
	printf("mta.sessions.maxactive=%zd\n", stats->mta.sessions_maxactive);

	printf("lka.queries=%zd\n", stats->lka.queries);
	printf("lka.queries.active=%zd\n", stats->lka.queries_active);
	printf("lka.queries.maxactive=%zd\n", stats->lka.queries_maxactive);
	printf("lka.queries.mx=%zd\n", stats->lka.queries_mx);
	printf("lka.queries.host=%zd\n", stats->lka.queries_host);
	printf("lka.queries.cname=%zd\n", stats->lka.queries_cname);
	printf("lka.queries.failure=%zd\n", stats->lka.queries_failure);

	printf("parent.uptime=%d\n", time(NULL) - stats->parent.start);

	printf("queue.inserts.local=%zd\n", stats->queue.inserts_local);
	printf("queue.inserts.remote=%zd\n", stats->queue.inserts_remote);

	printf("runner.active=%zd\n", stats->runner.active);
	printf("runner.maxactive=%zd\n", stats->runner.maxactive);
	printf("runner.bounces=%zd\n", stats->runner.bounces);
	printf("runner.bounces.active=%zd\n", stats->runner.bounces_active);
	printf("runner.bounces.maxactive=%zd\n",
	    stats->runner.bounces_maxactive);

	printf("ramqueue.hosts=%zd\n", stats->ramqueue.hosts);
	printf("ramqueue.batches=%zd\n", stats->ramqueue.batches);
	printf("ramqueue.messages=%zd\n", stats->ramqueue.messages);
	printf("ramqueue.envelopes=%zd\n", stats->ramqueue.envelopes);
	printf("ramqueue.hosts.max=%zd\n", stats->ramqueue.hosts_max);
	printf("ramqueue.batches.max=%zd\n", stats->ramqueue.batches_max);
	printf("ramqueue.messages.max=%zd\n", stats->ramqueue.messages_max);
	printf("ramqueue.envelopes.max=%zd\n", stats->ramqueue.envelopes_max);
	printf("ramqueue.size=%zd\n",
	    stats->ramqueue.hosts * sizeof(struct ramqueue_host) +
	    stats->ramqueue.batches * sizeof(struct ramqueue_batch) +
	    stats->ramqueue.messages * sizeof(struct ramqueue_message) +
	    stats->ramqueue.envelopes * sizeof(struct ramqueue_envelope));
	printf("ramqueue.size.max=%zd\n",
	    stats->ramqueue.hosts_max * sizeof(struct ramqueue_host) +
	    stats->ramqueue.batches_max * sizeof(struct ramqueue_batch) +
	    stats->ramqueue.messages_max * sizeof(struct ramqueue_message) +
	    stats->ramqueue.envelopes_max * sizeof(struct ramqueue_envelope));

	printf("smtp.errors.delays=%zd\n", stats->smtp.delays);
	printf("smtp.errors.linetoolong=%zd\n", stats->smtp.linetoolong);
	printf("smtp.errors.read_eof=%zd\n", stats->smtp.read_eof);
	printf("smtp.errors.read_system=%zd\n", stats->smtp.read_error);
	printf("smtp.errors.read_timeout=%zd\n", stats->smtp.read_timeout);
	printf("smtp.errors.tempfail=%zd\n", stats->smtp.tempfail);
	printf("smtp.errors.toofast=%zd\n", stats->smtp.toofast);
	printf("smtp.errors.write_eof=%zd\n", stats->smtp.write_eof);
	printf("smtp.errors.write_system=%zd\n", stats->smtp.write_error);
	printf("smtp.errors.write_timeout=%zd\n", stats->smtp.write_timeout);

	printf("smtp.sessions.inet4=%zd\n", stats->smtp.sessions_inet4);
	printf("smtp.sessions.inet6=%zd\n", stats->smtp.sessions_inet6);

	printf("smtp.sessions=%zd\n", stats->smtp.sessions);
	printf("smtp.sessions.aborted=%zd\n", stats->smtp.read_eof +
	    stats->smtp.read_error + stats->smtp.write_eof +
	    stats->smtp.write_error);
	printf("smtp.sessions.active=%zd\n", stats->smtp.sessions_active);
	printf("smtp.sessions.maxactive=%zd\n",
	    stats->smtp.sessions_maxactive);
	printf("smtp.sessions.timeout=%zd\n", stats->smtp.read_timeout +
	    stats->smtp.write_timeout);
	printf("smtp.sessions.smtps=%zd\n", stats->smtp.smtps);
	printf("smtp.sessions.smtps.active=%zd\n", stats->smtp.smtps_active);
	printf("smtp.sessions.smtps.maxactive=%zd\n",
	    stats->smtp.smtps_maxactive);
	printf("smtp.sessions.starttls=%zd\n", stats->smtp.starttls);
	printf("smtp.sessions.starttls.active=%zd\n",
	    stats->smtp.starttls_active);
	printf("smtp.sessions.starttls.maxactive=%zd\n",
	    stats->smtp.starttls_maxactive);

	return (1);
}
