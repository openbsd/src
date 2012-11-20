/*	$OpenBSD: smtpctl.c,v 1.97 2012/11/20 09:47:46 eric Exp $	*/

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
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "parser.h"
#include "log.h"

#define PATH_CAT	"/bin/cat"
#define PATH_GZCAT	"/bin/gzcat"
#define PATH_QUEUE	"/queue"

void usage(void);
static void setup_env(struct smtpd *);
static int show_command_output(struct imsg *);
static void show_stats_output(void);
static void show_queue(int);
static void show_queue_envelope(struct envelope *, int);
static void getflag(uint *, int, char *, char *, size_t);
static void display(const char *);
static void show_envelope(const char *);
static void show_message(const char *);
static void show_monitor(struct stat_digest *);

static int try_connect(void);
static void flush(void);
static void next_message(struct imsg *);
static int action_schedule_all(void);

static int action_show_queue(void);
static int action_show_queue_message(uint32_t);

int proctype;
struct imsgbuf	*ibuf;

int sendmail = 0;
extern char *__progname;

struct smtpd	*env = NULL;

time_t now;

__dead void
usage(void)
{
	extern char *__progname;

	if (sendmail)
		fprintf(stderr, "usage: %s [-tv] [-f from] [-F name] to ...\n",
		    __progname);
	else
		fprintf(stderr, "usage: %s command [argument ...]\n",
		    __progname);
	exit(1);
}

static void
setup_env(struct smtpd *smtpd)
{
	bzero(smtpd, sizeof (*smtpd));
	env = smtpd;

	if ((env->sc_pw = getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);

	env->sc_queue = queue_backend_lookup("fs");
	if (env->sc_queue == NULL)
		errx(1, "could not find queue backend");

	if (!env->sc_queue->init(0))
		errx(1, "invalid directory permissions");
}

static int
try_connect(void)
{
	struct sockaddr_un	sun;
	int			ctl_sock, saved_errno;

	/* connect to smtpd control socket */
	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SMTPD_SOCKET, sizeof(sun.sun_path));
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		saved_errno = errno;
		close(ctl_sock);
		errno = saved_errno;
		return (0);
	}

	if ((ibuf = calloc(1, sizeof(struct imsgbuf))) == NULL)
		err(1, "calloc");
	imsg_init(ibuf, ctl_sock);

	return (1);
}

static void
flush(void)
{
	if (imsg_flush(ibuf) == -1)
		err(1, "write error");
}

static void
next_message(struct imsg *imsg)
{
	ssize_t	n;

	while (1) {
		if ((n = imsg_get(ibuf, imsg)) == -1)
			errx(1, "imsg_get error");
		if (n)
			return;

		if ((n = imsg_read(ibuf)) == -1)
			errx(1, "imsg_read error");
		if (n == 0)
			errx(1, "pipe closed");
	}
}

int
main(int argc, char *argv[])
{
	struct parse_result	*res = NULL;
	struct imsg		imsg;
	struct smtpd		smtpd;
	uint64_t		ulval;
	char			name[MAX_LINE_SIZE];
	int			done = 0;
	int			verbose = 0;
	int			action = -1;

	/* parse options */
	if (strcmp(__progname, "sendmail") == 0 ||
	    strcmp(__progname, "send-mail") == 0) {
		sendmail = 1;
		if (try_connect())
			return (enqueue(argc, argv));
		return (enqueue_offline(argc, argv));
	}

	if (geteuid())
		errx(1, "need root privileges");

	if (strcmp(__progname, "mailq") == 0)
		action = SHOW_QUEUE;
	else if (strcmp(__progname, "smtpctl") == 0) {
		if ((res = parse(argc - 1, argv + 1)) == NULL)
			exit(1);
		action = res->action;
	} else
		errx(1, "unsupported mode");

	if (action == SHOW_ENVELOPE ||
	    action == SHOW_MESSAGE ||
	    !try_connect()) {
		setup_env(&smtpd);
		switch (action) {
		case SHOW_QUEUE:
			show_queue(0);
			break;
		case SHOW_ENVELOPE:
			show_envelope(res->data);
			break;
		case SHOW_MESSAGE:
			show_message(res->data);
			break;
		default:
			errx(1, "smtpd doesn't seem to be running");
		}
		return (0);
	}

	/* process user request */
	switch (action) {
	case NONE:
		usage();
		/* not reached */

	case SCHEDULE:
		ulval = strtoevpid(res->data);
		imsg_compose(ibuf, IMSG_SCHEDULER_SCHEDULE, 0, 0, -1, &ulval,
		    sizeof(ulval));
		break;
	case REMOVE:
		ulval = strtoevpid(res->data);
		imsg_compose(ibuf, IMSG_SCHEDULER_REMOVE, 0, 0, -1, &ulval,
		    sizeof(ulval));
		break;
	case SHOW_QUEUE:
		return action_show_queue();
	case SCHEDULE_ALL:
		return action_schedule_all();
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
	case UPDATE_MAP:
		if (strlcpy(name, res->data, sizeof name) >= sizeof name)
			errx(1, "map name too long.");
		imsg_compose(ibuf, IMSG_LKA_UPDATE_MAP, 0, 0, -1,
		    name, strlen(name) + 1);
		done = 1;
		break;
	case MONITOR:
		done = 1;
		while (1) {
			imsg_compose(ibuf, IMSG_DIGEST, 0, 0, -1, NULL, 0);
			flush();
			next_message(&imsg);
			show_monitor(imsg.data);
			imsg_free(&imsg);
			sleep(1);
		}
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
		errx(1, "unknown request (%d)", action);
	}

	do {
		flush();
		next_message(&imsg);

		switch (action) {
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
			show_stats_output();
			done = 1;
			break;
		case NONE:
			break;
		case UPDATE_MAP:
			break;
		case MONITOR:

		default:
			err(1, "unexpected reply (%d)", action);
		}

		imsg_free(&imsg);
	} while (!done);
	free(ibuf);

	return (0);
}


static int
action_show_queue_message(uint32_t msgid)
{
	struct imsg	 imsg;
	struct envelope	*evp;
	uint64_t	 evpid;
	size_t		 found;

	evpid = msgid_to_evpid(msgid);

    nextbatch:

	found = 0;
	imsg_compose(ibuf, IMSG_SCHEDULER_ENVELOPES, 0, 0, -1,
	    &evpid, sizeof evpid);
	flush();

	while (1) {
		next_message(&imsg);
		if (imsg.hdr.type != IMSG_SCHEDULER_ENVELOPES)
			errx(1, "unexpected message %i", imsg.hdr.type);

		if (imsg.hdr.len == sizeof imsg.hdr) {
			imsg_free(&imsg);
			if (!found || evpid_to_msgid(++evpid) != msgid)
				return (0);
			goto nextbatch;
		}
		found++;
		evp = imsg.data;
		evpid = evp->id;
		show_queue_envelope(evp, 1);
		imsg_free(&imsg);
	}

}

static int
action_show_queue(void)
{
	struct imsg	 imsg;
	uint32_t	*msgids, msgid;
	size_t		 i, n;

	msgid = 0;
	now = time(NULL);

	do {
		imsg_compose(ibuf, IMSG_SCHEDULER_MESSAGES, 0, 0, -1,
		    &msgid, sizeof msgid);
		flush();
		next_message(&imsg);
		if (imsg.hdr.type != IMSG_SCHEDULER_MESSAGES)
			errx(1, "unexpected message type %i", imsg.hdr.type);
		msgids = imsg.data;
		n = (imsg.hdr.len - sizeof imsg.hdr) / sizeof (*msgids);
		if (n == 0) {
			imsg_free(&imsg);
			break;
		}
		for (i = 0; i < n; i++) {
			msgid = msgids[i];
			action_show_queue_message(msgid);
		}
		imsg_free(&imsg);

	} while (++msgid);

	return (0);
}

static int
action_schedule_all(void)
{
	struct imsg	 imsg;
	uint32_t	*msgids, from;
	uint64_t	 evpid;
	size_t		 i, n;

	from = 0;
	while (1) {
		imsg_compose(ibuf, IMSG_SCHEDULER_MESSAGES, 0, 0, -1,
		    &from, sizeof from);
		flush();
		next_message(&imsg);
		if (imsg.hdr.type != IMSG_SCHEDULER_MESSAGES)
			errx(1, "unexpected message type %i", imsg.hdr.type);
		msgids = imsg.data;
		n = (imsg.hdr.len - sizeof imsg.hdr) / sizeof (*msgids);
		if (n == 0)
			break;

		for (i = 0; i < n; i++) {
			evpid = msgids[i];
			imsg_compose(ibuf, IMSG_SCHEDULER_SCHEDULE, 0,
			    0, -1, &evpid, sizeof(evpid));
		}
		from = msgids[n - 1] + 1;

		imsg_free(&imsg);
		flush();

		for (i = 0; i < n; i++) {
			next_message(&imsg);
			if (imsg.hdr.type != IMSG_CTL_OK)
				errx(1, "unexpected message type %i",
				    imsg.hdr.type);
		}

		if (from == 0)
			break;
	}

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

static void
show_stats_output(void)
{
	struct stat_kv	kv, *kvp;
	struct imsg	imsg;
	time_t		duration;

	bzero(&kv, sizeof kv);

	while (1) {
		imsg_compose(ibuf, IMSG_STATS_GET, 0, 0, -1, &kv, sizeof kv);
		flush();
		next_message(&imsg);
		if (imsg.hdr.type != IMSG_STATS_GET)
			errx(1, "invalid imsg type");

		kvp = imsg.data;
		if (kvp->iter == NULL) {
			imsg_free(&imsg);
			return;
		}

		if (strcmp(kvp->key, "uptime") == 0) {
			duration = time(NULL) - kvp->val.u.counter;
			printf("uptime=%zd\n", (size_t)duration);
			printf("uptime.human=%s\n",
			    duration_to_text(duration));
		}
		else {
			switch (kvp->val.type) {
			case STAT_COUNTER:
				printf("%s=%zd\n",
				    kvp->key, kvp->val.u.counter);
				break;
			case STAT_TIMESTAMP:
				printf("%s=%" PRId64 "\n",
				    kvp->key, (int64_t)kvp->val.u.timestamp);
				break;
			case STAT_TIMEVAL:
				printf("%s=%zd.%zd\n",
				    kvp->key, kvp->val.u.tv.tv_sec,
				    kvp->val.u.tv.tv_usec);
				break;
			case STAT_TIMESPEC:
				printf("%s=%li.%06li\n",
				    kvp->key,
				    kvp->val.u.ts.tv_sec * 1000000 +
				    kvp->val.u.ts.tv_nsec / 1000000,
				    kvp->val.u.ts.tv_nsec % 1000000);
				break;
			}
		}

		kv = *kvp;
		imsg_free(&imsg);
	}
}

static void
show_queue(int flags)
{
	struct qwalk	*q;
	struct envelope	 envelope;
	uint64_t	 evpid;

	log_init(1);

	if (chroot(PATH_SPOOL) == -1 || chdir(".") == -1)
		err(1, "%s", PATH_SPOOL);

	q = qwalk_new(0);

	while (qwalk(q, &evpid)) {
		if (! queue_envelope_load(evpid, &envelope))
			continue;
		show_queue_envelope(&envelope, flags);
	}

	qwalk_close(q);
}


static void
show_queue_envelope(struct envelope *e, int online)
{
	const char	*src = "?", *agent = "?";
	char		 status[128], runstate[128];

	status[0] = '\0';

	getflag(&e->flags, DF_BOUNCE, "bounce",
	    status, sizeof(status));
	getflag(&e->flags, DF_AUTHENTICATED, "auth",
	    status, sizeof(status));
	getflag(&e->flags, DF_INTERNAL, "internal",
	    status, sizeof(status));

	if (online) {
		if (e->flags & DF_PENDING)
			snprintf(runstate, sizeof runstate, "pending|%zi",
			    (ssize_t)(e->nexttry - now));
		else if (e->flags & DF_INFLIGHT)
			snprintf(runstate, sizeof runstate, "inflight|%zi",
			    (ssize_t)(now - e->lasttry));
		else
			snprintf(runstate, sizeof runstate, "invalid|");
		e->flags &= ~(DF_PENDING|DF_INFLIGHT);
	}
	else
		strlcpy(runstate, "offline|", sizeof runstate);

	if (e->flags)
		errx(1, "%016" PRIx64 ": unexpected flags 0x%04x", e->id,
		    e->flags);

	if (status[0])
		status[strlen(status) - 1] = '\0';

	if (e->type == D_MDA)
		agent = "mda";
	else if (e->type == D_MTA)
		agent = "mta";
	else if (e->type == D_BOUNCE)
		agent = "bounce";

	if (e->ss.ss_family == AF_LOCAL)
		src = "local";
	else if (e->ss.ss_family == AF_INET)
		src = "inet4";
	else if (e->ss.ss_family == AF_INET6)
		src = "inet6";

	printf("%016"PRIx64
	    "|%s|%s|%s|%s@%s|%s@%s|%s@%s"
	    "|%zu|%zu|%zu|%zu|%s|%s\n",

	    e->id,

	    src,
	    agent,
	    status,
	    e->sender.user, e->sender.domain,
	    e->rcpt.user, e->rcpt.domain,
	    e->dest.user, e->dest.domain,

	    (size_t) e->creation,
	    (size_t) (e->creation + e->expire),
	    (size_t) e->lasttry,
	    (size_t) e->retry,
	    runstate,
	    e->errorline);
}

static void
getflag(uint *bitmap, int bit, char *bitstr, char *buf, size_t len)
{
	if (*bitmap & bit) {
		*bitmap &= ~bit;
		strlcat(buf, bitstr, len);
		strlcat(buf, ",", len);
	}
}

static void
display(const char *s)
{
	arglist args;
	char	*cmd;

	if (env->sc_queue_flags & QUEUE_COMPRESS)
		cmd = PATH_GZCAT;
	else
		cmd = PATH_CAT;

	bzero(&args, sizeof(args));
	addargs(&args, "%s", cmd);
	addargs(&args, "%s", s);
	execvp(cmd, args.list);
	errx(1, "execvp");
}

static void
show_envelope(const char *s)
{
	char	 buf[MAXPATHLEN];
	uint64_t evpid;

	evpid = strtoevpid(s);
	if (! bsnprintf(buf, sizeof(buf), "%s%s/%02x/%08x%s/%016" PRIx64,
	    PATH_SPOOL,
	    PATH_QUEUE,
	    evpid_to_msgid(evpid) & 0xff,
	    evpid_to_msgid(evpid),
	    PATH_ENVELOPES, evpid))
		errx(1, "unable to retrieve envelope");

	display(buf);
}

static void
show_message(const char *s)
{
	char	 buf[MAXPATHLEN];
	uint32_t msgid;

	msgid = evpid_to_msgid(strtoevpid(s));
	if (! bsnprintf(buf, sizeof(buf), "%s%s/%02x/%08x/message",
	    PATH_SPOOL,
	    PATH_QUEUE,
	    msgid & 0xff,
	    msgid))
		errx(1, "unable to retrieve message");

	display(buf);
}

static void
show_monitor(struct stat_digest *d)
{
	static int init = 0;
	static size_t count = 0;
	static struct stat_digest last;

	if (init == 0) {
		init = 1;
		bzero(&last, sizeof last);
	}

	if (count % 25 == 0) {
		if (count != 0)
			printf("\n");
		printf("--- client ---  "
		    "-- envelope --   "
		    "---- relay/delivery --- "
		    "------- misc -------\n"
		    "curr conn disc  "
		    "curr  enq  deq   "
		    "ok tmpfail prmfail loop "
		    "expire remove bounce\n");
	}
	printf("%4zu %4zu %4zu  "
	    "%4zu %4zu %4zu "
	    "%4zu    %4zu    %4zu %4zu   "
	    "%4zu   %4zu   %4zu\n",
	    d->clt_connect - d->clt_disconnect,
	    d->clt_connect - last.clt_connect,
	    d->clt_disconnect - last.clt_disconnect,

	    d->evp_enqueued - d->evp_dequeued,
	    d->evp_enqueued - last.evp_enqueued,
	    d->evp_dequeued - last.evp_dequeued,

	    d->dlv_ok - last.dlv_ok,
	    d->dlv_tempfail - last.dlv_tempfail,
	    d->dlv_permfail - last.dlv_permfail,
	    d->dlv_loop - last.dlv_loop,

	    d->evp_expired - last.evp_expired,
	    d->evp_removed - last.evp_removed,
	    d->evp_bounce - last.evp_bounce);

	last = *d;
	count++;
}
