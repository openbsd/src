/*	$OpenBSD: smtpctl.c,v 1.70 2011/10/26 20:47:31 gilles Exp $	*/

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
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "parser.h"

void usage(void);
static void show_sizes(void);
static int show_command_output(struct imsg *);
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
		case SHOW_SIZES:
			show_sizes();
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
		if (ulval == 0)
			errx(1, "invalid msgid/evpid");

		if (res->action == SCHEDULE)
			imsg_compose(ibuf, IMSG_RUNNER_SCHEDULE, 0, 0, -1, &ulval,
			    sizeof(ulval));
		if (res->action == REMOVE)
			imsg_compose(ibuf, IMSG_RUNNER_REMOVE, 0, 0, -1, &ulval,
			    sizeof(ulval));
		break;
	}

	case SCHEDULE_ALL: {
		u_int64_t ulval = 0;

		imsg_compose(ibuf, IMSG_RUNNER_SCHEDULE, 0, 0, -1, &ulval,
		    sizeof(ulval));
		break;
	}

	case SHUTDOWN:
		imsg_compose(ibuf, IMSG_CTL_SHUTDOWN, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_MDA:
		imsg_compose(ibuf, IMSG_QUEUE_PAUSE_MDA, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_MTA:
		imsg_compose(ibuf, IMSG_QUEUE_PAUSE_MTA, 0, 0, -1, NULL, 0);
		break;
	case PAUSE_SMTP:
		imsg_compose(ibuf, IMSG_SMTP_PAUSE, 0, 0, -1, NULL, 0);
		break;
	case RESUME_MDA:
		imsg_compose(ibuf, IMSG_QUEUE_RESUME_MDA, 0, 0, -1, NULL, 0);
		break;
	case RESUME_MTA:
		imsg_compose(ibuf, IMSG_QUEUE_RESUME_MTA, 0, 0, -1, NULL, 0);
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
			case SCHEDULE_ALL:
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

void
show_sizes(void)
{
	/*
	 * size _does_ matter.
	 *
	 * small changes to ramqueue and diskqueue structures may cause
	 * large changes to memory and disk usage on busy/large hosts.
	 *
	 * this will help developers optimize memory/disk use, and help
	 * admins understand how the ramqueue.size / ramqueue.size.max
	 * stats are computed (smtpctl show stats).
	 *
	 * -- gilles@
	 *
	 */
	printf("struct ramqueue: %zu\n", sizeof (struct ramqueue));
	printf("struct ramqueue_host: %zu\n", sizeof (struct ramqueue_host));
	printf("struct ramqueue_message: %zu\n", sizeof (struct ramqueue_message));
	printf("struct ramqueue_envelope: %zu\n", sizeof (struct ramqueue_envelope));

	printf("struct envelope: %zu\n", sizeof (struct envelope));
}

static void
stat_print(int stat, int what)
{
	static const char *names[STATS_MAX] = {
		"smtp.sessions",
		"smtp.sessions.inet4",
		"smtp.sessions.inet6",
		"smtp.sessions.smtps",
		"smtp.sessions.starttls",

		"mta.sessions",

		"mda.sessions",

		"control.sessions",

		"lka.sessions",
		"lka.sessions.mx",
		"lka.sessions.host",
		"lka.sessions.cname",
		"lka.sessions.failure",

		"runner",
		"runner.bounces",

		"queue.inserts.local",
		"queue.inserts.remote",

		"ramqueue.envelopes",
		"ramqueue.messages",
		"ramqueue.batches",
		"ramqueue.hosts",
	};
	const char *sfx;

	if (what == STAT_ACTIVE)
		sfx = ".active";
	else if (what == STAT_MAXACTIVE)
		sfx = ".maxactive";
	else
		sfx = "";

	printf("%s%s=%zd\n", names[stat], sfx, stat_get(stat, what));
}

static int
show_stats_output(struct imsg *imsg)
{
	struct stats	*stats;
	struct stat_counter	*s;

	if (imsg->hdr.type != IMSG_STATS)
		errx(1, "show_stats_output: bad hdr type (%d)", imsg->hdr.type);
	
	if (IMSG_DATA_SIZE(imsg) != sizeof(*stats))
		errx(1, "show_stats_output: bad data size");

	stats = imsg->data;
	stat_init(stats->counters, STATS_MAX);
	s = stats->counters;

	stat_print(STATS_CONTROL_SESSION, STAT_COUNT);
	stat_print(STATS_CONTROL_SESSION, STAT_ACTIVE);
	stat_print(STATS_CONTROL_SESSION, STAT_MAXACTIVE);

	stat_print(STATS_MDA_SESSION, STAT_COUNT);
	stat_print(STATS_MDA_SESSION, STAT_ACTIVE);
	stat_print(STATS_MDA_SESSION, STAT_MAXACTIVE);

	stat_print(STATS_MTA_SESSION, STAT_COUNT);
	stat_print(STATS_MTA_SESSION, STAT_ACTIVE);
	stat_print(STATS_MTA_SESSION, STAT_MAXACTIVE);

	stat_print(STATS_LKA_SESSION, STAT_COUNT);
	stat_print(STATS_LKA_SESSION, STAT_ACTIVE);
	stat_print(STATS_LKA_SESSION, STAT_MAXACTIVE);
	stat_print(STATS_LKA_SESSION_MX, STAT_COUNT);
	stat_print(STATS_LKA_SESSION_HOST, STAT_COUNT);
	stat_print(STATS_LKA_SESSION_CNAME, STAT_COUNT);
	stat_print(STATS_LKA_FAILURE, STAT_COUNT);

	printf("parent.uptime=%lld\n",
	    (long long int) (time(NULL) - stats->parent.start));

	stat_print(STATS_QUEUE_LOCAL, STAT_COUNT);
	stat_print(STATS_QUEUE_REMOTE, STAT_COUNT);

	stat_print(STATS_RUNNER, STAT_COUNT);
	stat_print(STATS_RUNNER, STAT_ACTIVE);
	stat_print(STATS_RUNNER, STAT_MAXACTIVE);

	stat_print(STATS_RUNNER_BOUNCES, STAT_COUNT);
	stat_print(STATS_RUNNER_BOUNCES, STAT_ACTIVE);
	stat_print(STATS_RUNNER_BOUNCES, STAT_MAXACTIVE);

	stat_print(STATS_RAMQUEUE_HOST, STAT_ACTIVE);
	stat_print(STATS_RAMQUEUE_BATCH, STAT_ACTIVE);
	stat_print(STATS_RAMQUEUE_MESSAGE, STAT_ACTIVE);
	stat_print(STATS_RAMQUEUE_ENVELOPE, STAT_ACTIVE);

	stat_print(STATS_RAMQUEUE_HOST, STAT_MAXACTIVE);
	stat_print(STATS_RAMQUEUE_BATCH, STAT_MAXACTIVE);
	stat_print(STATS_RAMQUEUE_MESSAGE, STAT_MAXACTIVE);
	stat_print(STATS_RAMQUEUE_ENVELOPE, STAT_MAXACTIVE);

	printf("ramqueue.size=%zd\n",
	    s[STATS_RAMQUEUE_HOST].active * sizeof(struct ramqueue_host) +
	    s[STATS_RAMQUEUE_BATCH].active * sizeof(struct ramqueue_batch) +
	    s[STATS_RAMQUEUE_MESSAGE].active * sizeof(struct ramqueue_message) +
	    s[STATS_RAMQUEUE_ENVELOPE].active * sizeof(struct ramqueue_envelope));
	printf("ramqueue.size.max=%zd\n",
	    s[STATS_RAMQUEUE_HOST].maxactive * sizeof(struct ramqueue_host) +
	    s[STATS_RAMQUEUE_BATCH].maxactive * sizeof(struct ramqueue_batch) +
	    s[STATS_RAMQUEUE_MESSAGE].maxactive * sizeof(struct ramqueue_message) +
	    s[STATS_RAMQUEUE_ENVELOPE].maxactive * sizeof(struct ramqueue_envelope));

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

	stat_print(STATS_SMTP_SESSION, STAT_COUNT);
	stat_print(STATS_SMTP_SESSION_INET4, STAT_COUNT);
	stat_print(STATS_SMTP_SESSION_INET6, STAT_COUNT);
	printf("smtp.sessions.aborted=%zd\n", stats->smtp.read_eof +
	    stats->smtp.read_error + stats->smtp.write_eof +
	    stats->smtp.write_error);

	stat_print(STATS_SMTP_SESSION, STAT_ACTIVE);
	stat_print(STATS_SMTP_SESSION, STAT_MAXACTIVE);

	printf("smtp.sessions.timeout=%zd\n", stats->smtp.read_timeout +
	    stats->smtp.write_timeout);

	stat_print(STATS_SMTP_SMTPS, STAT_COUNT);
	stat_print(STATS_SMTP_SMTPS, STAT_ACTIVE);
	stat_print(STATS_SMTP_SMTPS, STAT_MAXACTIVE);

	stat_print(STATS_SMTP_STARTTLS, STAT_COUNT);
	stat_print(STATS_SMTP_STARTTLS, STAT_ACTIVE);
	stat_print(STATS_SMTP_STARTTLS, STAT_MAXACTIVE);

	return (1);
}
