/*	$OpenBSD: queue.c,v 1.166 2015/01/20 17:37:54 deraadt Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <err.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

static void queue_imsg(struct mproc *, struct imsg *);
static void queue_timeout(int, short, void *);
static void queue_bounce(struct envelope *, struct delivery_bounce *);
static void queue_shutdown(void);
static void queue_sig_handler(int, short, void *);
static void queue_log(const struct envelope *, const char *, const char *);

static size_t	flow_agent_hiwat = 10 * 1024 * 1024;
static size_t	flow_agent_lowat =   1 * 1024 * 1024;
static size_t	flow_scheduler_hiwat = 10 * 1024 * 1024;
static size_t	flow_scheduler_lowat = 1 * 1024 * 1024;

#define LIMIT_AGENT	0x01
#define LIMIT_SCHEDULER	0x02

static int limit = 0;

static void
queue_imsg(struct mproc *p, struct imsg *imsg)
{
	struct delivery_bounce	 bounce;
	struct bounce_req_msg	*req_bounce;
	struct envelope		 evp;
	struct msg		 m;
	const char		*reason;
	uint64_t		 reqid, evpid, holdq;
	uint32_t		 msgid;
	time_t			 nexttry;
	int			 fd, mta_ext, ret, v, flags, code;

	memset(&bounce, 0, sizeof(struct delivery_bounce));
	if (p->proc == PROC_PONY) {

		switch (imsg->hdr.type) {
		case IMSG_SMTP_MESSAGE_CREATE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);

			ret = queue_message_create(&msgid);

			m_create(p, IMSG_SMTP_MESSAGE_CREATE, 0, 0, -1);
			m_add_id(p, reqid);
			if (ret == 0)
				m_add_int(p, 0);
			else {
				m_add_int(p, 1);
				m_add_msgid(p, msgid);
			}
			m_close(p);
			return;

		case IMSG_SMTP_MESSAGE_ROLLBACK:
			m_msg(&m, imsg);
			m_get_msgid(&m, &msgid);
			m_end(&m);

			queue_message_delete(msgid);

			m_create(p_scheduler, IMSG_QUEUE_MESSAGE_ROLLBACK,
			    0, 0, -1);
			m_add_msgid(p_scheduler, msgid);
			m_close(p_scheduler);
			return;

		case IMSG_SMTP_MESSAGE_COMMIT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_msgid(&m, &msgid);
			m_end(&m);

			ret = queue_message_commit(msgid);

			m_create(p, IMSG_SMTP_MESSAGE_COMMIT, 0, 0, -1);
			m_add_id(p, reqid);
			m_add_int(p, (ret == 0) ? 0 : 1);
			m_close(p);

			if (ret) {
				m_create(p_scheduler, IMSG_QUEUE_MESSAGE_COMMIT,
				    0, 0, -1);
				m_add_msgid(p_scheduler, msgid);
				m_close(p_scheduler);
			}
			return;

		case IMSG_SMTP_MESSAGE_OPEN:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_msgid(&m, &msgid);
			m_end(&m);

			fd = queue_message_fd_rw(msgid);

			m_create(p, IMSG_SMTP_MESSAGE_OPEN, 0, 0, fd);
			m_add_id(p, reqid);
			m_add_int(p, (fd == -1) ? 0 : 1);
			m_close(p);
			return;

		case IMSG_QUEUE_SMTP_SESSION:
			bounce_fd(imsg->fd);
			return;
		}
	}

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_ENVELOPE_SUBMIT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_envelope(&m, &evp);
			m_end(&m);
		    
			if (evp.id == 0)
				log_warnx("warn: imsg_queue_submit_envelope: evpid=0");
			if (evpid_to_msgid(evp.id) == 0)
				log_warnx("warn: imsg_queue_submit_envelope: msgid=0, "
				    "evpid=%016"PRIx64, evp.id);
			ret = queue_envelope_create(&evp);
			m_create(p_pony, IMSG_QUEUE_ENVELOPE_SUBMIT, 0, 0, -1);
			m_add_id(p_pony, reqid);
			if (ret == 0)
				m_add_int(p_pony, 0);
			else {
				m_add_int(p_pony, 1);
				m_add_evpid(p_pony, evp.id);
			}
			m_close(p_pony);
			if (ret) {
				m_create(p_scheduler,
				    IMSG_QUEUE_ENVELOPE_SUBMIT, 0, 0, -1);
				m_add_envelope(p_scheduler, &evp);
				m_close(p_scheduler);

			}
			return;

		case IMSG_LKA_ENVELOPE_COMMIT:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_end(&m);
			m_create(p_pony, IMSG_QUEUE_ENVELOPE_COMMIT, 0, 0, -1);
			m_add_id(p_pony, reqid);
			m_add_int(p_pony, 1);
			m_close(p_pony);
			return;
		}
	}

	if (p->proc == PROC_SCHEDULER) {
		switch (imsg->hdr.type) {
		case IMSG_SCHED_ENVELOPE_REMOVE:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);

			m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_ACK, 0, 0, -1);
			m_add_evpid(p_scheduler, evpid);
			m_close(p_scheduler);

			/* already removed by scheduler */
			if (queue_envelope_load(evpid, &evp) == 0)
				return;

			queue_log(&evp, "Remove", "Removed by administrator");
			queue_envelope_delete(evpid);
			return;

		case IMSG_SCHED_ENVELOPE_EXPIRE:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);

			m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_ACK, 0, 0, -1);
			m_add_evpid(p_scheduler, evpid);
			m_close(p_scheduler);

			/* already removed by scheduler*/
			if (queue_envelope_load(evpid, &evp) == 0)
				return;

			bounce.type = B_ERROR;
			envelope_set_errormsg(&evp, "Envelope expired");
			envelope_set_esc_class(&evp, ESC_STATUS_TEMPFAIL);
			envelope_set_esc_code(&evp, ESC_DELIVERY_TIME_EXPIRED);
			queue_bounce(&evp, &bounce);
			queue_log(&evp, "Expire", "Envelope expired");
			queue_envelope_delete(evpid);
			return;

		case IMSG_SCHED_ENVELOPE_BOUNCE:
			req_bounce = imsg->data;
			evpid = req_bounce->evpid;

			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: bounce: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 0); /* not in-flight */
				m_close(p_scheduler);
				return;
			}
			queue_bounce(&evp, &req_bounce->bounce);
			evp.lastbounce = req_bounce->timestamp;
			if (!queue_envelope_update(&evp))
				log_warnx("warn: could not update envelope %016"PRIx64, evpid);
			return;

		case IMSG_SCHED_ENVELOPE_DELIVER:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: deliver: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 1); /* in-flight */
				m_close(p_scheduler);
				return;
			}
			evp.lasttry = time(NULL);
			m_create(p_pony, IMSG_QUEUE_DELIVER, 0, 0, -1);
			m_add_envelope(p_pony, &evp);
			m_close(p_pony);
			return;

		case IMSG_SCHED_ENVELOPE_INJECT:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);
			bounce_add(evpid);
			return;

		case IMSG_SCHED_ENVELOPE_TRANSFER:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 1); /* in-flight */
				m_close(p_scheduler);
				return;
			}
			evp.lasttry = time(NULL);
			m_create(p_pony, IMSG_QUEUE_TRANSFER, 0, 0, -1);
			m_add_envelope(p_pony, &evp);
			m_close(p_pony);
			return;

		case IMSG_CTL_LIST_ENVELOPES:
			if (imsg->hdr.len == sizeof imsg->hdr) {
				m_forward(p_control, imsg);
				return;
			}

			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_get_int(&m, &flags);
			m_get_time(&m, &nexttry);
			m_end(&m);

			if (queue_envelope_load(evpid, &evp) == 0)
				return; /* Envelope is gone, drop it */

			/*
			 * XXX consistency: The envelope might already be on
			 * its way back to the scheduler.  We need to detect
			 * this properly and report that state.
			 */
			evp.flags |= flags;
			/* In the past if running or runnable */
			evp.nexttry = nexttry;
			if (flags & EF_INFLIGHT) {
				/*
				 * Not exactly correct but pretty close: The
				 * value is not recorded on the envelope unless
				 * a tempfail occurs.
				 */
				evp.lasttry = nexttry;
			}
			m_compose(p_control, IMSG_CTL_LIST_ENVELOPES,
			    imsg->hdr.peerid, 0, -1, &evp, sizeof evp);
			return;
		}
	}

	if (p->proc == PROC_PONY) {
		switch (imsg->hdr.type) {
		case IMSG_MDA_OPEN_MESSAGE:
		case IMSG_MTA_OPEN_MESSAGE:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_msgid(&m, &msgid);
			m_end(&m);
			fd = queue_message_fd_r(msgid);
			m_create(p, imsg->hdr.type, 0, 0, fd);
			m_add_id(p, reqid);
			m_close(p);
			return;

		case IMSG_MDA_DELIVERY_OK:
		case IMSG_MTA_DELIVERY_OK:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			if (imsg->hdr.type == IMSG_MTA_DELIVERY_OK)
				m_get_int(&m, &mta_ext);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warn("queue: dsn: failed to load envelope");
				return;
			}
			if (evp.dsn_notify & DSN_SUCCESS) {
				bounce.type = B_DSN;
				bounce.dsn_ret = evp.dsn_ret;

				if (imsg->hdr.type == IMSG_MDA_DELIVERY_OK)
					queue_bounce(&evp, &bounce);
				else if (imsg->hdr.type == IMSG_MTA_DELIVERY_OK &&
				    (mta_ext & MTA_EXT_DSN) == 0) {
					bounce.mta_without_dsn = 1;
					queue_bounce(&evp, &bounce);
				}
			}
			queue_envelope_delete(evpid);
			m_create(p_scheduler, IMSG_QUEUE_DELIVERY_OK, 0, 0, -1);
			m_add_evpid(p_scheduler, evpid);
			m_close(p_scheduler);
			return;

		case IMSG_MDA_DELIVERY_TEMPFAIL:
		case IMSG_MTA_DELIVERY_TEMPFAIL:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_get_string(&m, &reason);
			m_get_int(&m, &code);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: tempfail: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 1); /* in-flight */
				m_close(p_scheduler);
				return;
			}
			envelope_set_errormsg(&evp, "%s", reason);
			envelope_set_esc_class(&evp, ESC_STATUS_TEMPFAIL);
			envelope_set_esc_code(&evp, code);
			evp.retry++;
			if (!queue_envelope_update(&evp))
				log_warnx("warn: could not update envelope %016"PRIx64, evpid);
			m_create(p_scheduler, IMSG_QUEUE_DELIVERY_TEMPFAIL, 0, 0, -1);
			m_add_envelope(p_scheduler, &evp);
			m_close(p_scheduler);
			return;

		case IMSG_MDA_DELIVERY_PERMFAIL:
		case IMSG_MTA_DELIVERY_PERMFAIL:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_get_string(&m, &reason);
			m_get_int(&m, &code);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: permfail: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 1); /* in-flight */
				m_close(p_scheduler);
				return;
			}
			bounce.type = B_ERROR;
			envelope_set_errormsg(&evp, "%s", reason);
			envelope_set_esc_class(&evp, ESC_STATUS_PERMFAIL);
			envelope_set_esc_code(&evp, code);
			queue_bounce(&evp, &bounce);
			queue_envelope_delete(evpid);
			m_create(p_scheduler, IMSG_QUEUE_DELIVERY_PERMFAIL, 0, 0, -1);
			m_add_evpid(p_scheduler, evpid);
			m_close(p_scheduler);
			return;

		case IMSG_MDA_DELIVERY_LOOP:
		case IMSG_MTA_DELIVERY_LOOP:
			m_msg(&m, imsg);
			m_get_evpid(&m, &evpid);
			m_end(&m);
			if (queue_envelope_load(evpid, &evp) == 0) {
				log_warnx("queue: loop: failed to load envelope");
				m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_REMOVE, 0, 0, -1);
				m_add_evpid(p_scheduler, evpid);
				m_add_u32(p_scheduler, 1); /* in-flight */
				m_close(p_scheduler);
				return;
			}
			envelope_set_errormsg(&evp, "%s", "Loop detected");
			envelope_set_esc_class(&evp, ESC_STATUS_TEMPFAIL);
			envelope_set_esc_code(&evp, ESC_ROUTING_LOOP_DETECTED);
			bounce.type = B_ERROR;
			queue_bounce(&evp, &bounce);
			queue_envelope_delete(evp.id);
			m_create(p_scheduler, IMSG_QUEUE_DELIVERY_LOOP, 0, 0, -1);
			m_add_evpid(p_scheduler, evp.id);
			m_close(p_scheduler);
			return;

		case IMSG_MTA_DELIVERY_HOLD:
		case IMSG_MDA_DELIVERY_HOLD:
			imsg->hdr.type = IMSG_QUEUE_HOLDQ_HOLD;
			m_forward(p_scheduler, imsg);
			return;

		case IMSG_MTA_SCHEDULE:
			imsg->hdr.type = IMSG_QUEUE_ENVELOPE_SCHEDULE;
			m_forward(p_scheduler, imsg);
			return;

		case IMSG_MTA_HOLDQ_RELEASE:
		case IMSG_MDA_HOLDQ_RELEASE:
			m_msg(&m, imsg);
			m_get_id(&m, &holdq);
			m_get_int(&m, &v);
			m_end(&m);
			m_create(p_scheduler, IMSG_QUEUE_HOLDQ_RELEASE, 0, 0, -1);
			if (imsg->hdr.type == IMSG_MTA_HOLDQ_RELEASE)
				m_add_int(p_scheduler, D_MTA);
			else
				m_add_int(p_scheduler, D_MDA);
			m_add_id(p_scheduler, holdq);
			m_add_int(p_scheduler, v);
			m_close(p_scheduler);
			return;
		}
	}

	if (p->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_PAUSE_MDA:
		case IMSG_CTL_PAUSE_MTA:
		case IMSG_CTL_RESUME_MDA:
		case IMSG_CTL_RESUME_MTA:
			m_forward(p_scheduler, imsg);
			return;

		case IMSG_CTL_VERBOSE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			log_verbose(v);
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling = v;
			return;
		}
	}

	errx(1, "queue_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
queue_bounce(struct envelope *e, struct delivery_bounce *d)
{
	struct envelope	b;

	b = *e;
	b.type = D_BOUNCE;
	b.agent.bounce = *d;
	b.retry = 0;
	b.lasttry = 0;
	b.creation = time(NULL);
	b.expire = 3600 * 24 * 7;

	if (e->dsn_notify & DSN_NEVER)
		return;

	if (b.id == 0)
		log_warnx("warn: queue_bounce: evpid=0");
	if (evpid_to_msgid(b.id) == 0)
		log_warnx("warn: queue_bounce: msgid=0, evpid=%016"PRIx64,
			b.id);
	if (e->type == D_BOUNCE) {
		log_warnx("warn: queue: double bounce!");
	} else if (e->sender.user[0] == '\0') {
		log_warnx("warn: queue: no return path!");
	} else if (!queue_envelope_create(&b)) {
		log_warnx("warn: queue: cannot bounce!");
	} else {
		log_debug("debug: queue: bouncing evp:%016" PRIx64
		    " as evp:%016" PRIx64, e->id, b.id);

		m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_SUBMIT, 0, 0, -1);
		m_add_envelope(p_scheduler, &b);
		m_close(p_scheduler);

		m_create(p_scheduler, IMSG_QUEUE_MESSAGE_COMMIT, 0, 0, -1);
		m_add_msgid(p_scheduler, evpid_to_msgid(b.id));
		m_close(p_scheduler);

		stat_increment("queue.bounce", 1);
	}
}

static void
queue_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		queue_shutdown();
		break;
	default:
		fatalx("queue_sig_handler: unexpected signal");
	}
}

static void
queue_shutdown(void)
{
	log_info("info: queue handler exiting");
	queue_close();
	_exit(0);
}

pid_t
queue(void)
{
	pid_t		 pid;
	struct passwd	*pw;
	struct timeval	 tv;
	struct event	 ev_qload;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		post_fork(PROC_QUEUE);
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

	if ((pw = getpwnam(SMTPD_QUEUE_USER)) == NULL)
		if ((pw = getpwnam(SMTPD_USER)) == NULL)
			fatalx("unknown user " SMTPD_USER);

	env->sc_queue_flags |= QUEUE_EVPCACHE;
	env->sc_queue_evpcache_size = 1024;

	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");

	config_process(PROC_QUEUE);

	if (env->sc_queue_flags & QUEUE_COMPRESSION)
		log_info("queue: queue compression enabled");

	if (env->sc_queue_key) {
		if (! crypto_setup(env->sc_queue_key, strlen(env->sc_queue_key)))
			fatalx("crypto_setup: invalid key for queue encryption");
		log_info("queue: queue encryption enabled");
	}

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");

	imsg_callback = queue_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peer(PROC_PARENT);
	config_peer(PROC_CONTROL);
	config_peer(PROC_LKA);
	config_peer(PROC_SCHEDULER);
	config_peer(PROC_PONY);
	config_done();

	/* setup queue loading task */
	evtimer_set(&ev_qload, queue_timeout, &ev_qload);
	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(&ev_qload, &tv);

	if (event_dispatch() <  0)
		fatal("event_dispatch");
	queue_shutdown();

	return (0);
}

static void
queue_timeout(int fd, short event, void *p)
{
	static uint32_t	 msgid = 0;
	struct envelope	 evp;
	struct event	*ev = p;
	struct timeval	 tv;
	int		 r;

	r = queue_envelope_walk(&evp);
	if (r == -1) {
		if (msgid) {
			m_create(p_scheduler, IMSG_QUEUE_MESSAGE_COMMIT,
			    0, 0, -1);
			m_add_msgid(p_scheduler, msgid);
			m_close(p_scheduler);
		}
		log_debug("debug: queue: done loading queue into scheduler");
		return;
	}

	if (r) {
		if (msgid && evpid_to_msgid(evp.id) != msgid) {
			m_create(p_scheduler, IMSG_QUEUE_MESSAGE_COMMIT,
			    0, 0, -1);
			m_add_msgid(p_scheduler, msgid);
			m_close(p_scheduler);
		}
		msgid = evpid_to_msgid(evp.id);
		m_create(p_scheduler, IMSG_QUEUE_ENVELOPE_SUBMIT, 0, 0, -1);
		m_add_envelope(p_scheduler, &evp);
		m_close(p_scheduler);
	}

	tv.tv_sec = 0;
	tv.tv_usec = 10;
	evtimer_add(ev, &tv);
}

static void
queue_log(const struct envelope *e, const char *prefix, const char *status)
{
	char rcpt[LINE_MAX];
	
	(void)strlcpy(rcpt, "-", sizeof rcpt);
	if (strcmp(e->rcpt.user, e->dest.user) ||
	    strcmp(e->rcpt.domain, e->dest.domain))
		(void)snprintf(rcpt, sizeof rcpt, "%s@%s",
		    e->rcpt.user, e->rcpt.domain);
	
	log_info("%s: %s for %016" PRIx64 ": from=<%s@%s>, to=<%s@%s>, "
	    "rcpt=<%s>, delay=%s, stat=%s",
	    e->type == D_MDA ? "delivery" : "relay",
	    prefix,
	    e->id, e->sender.user, e->sender.domain,
	    e->dest.user, e->dest.domain,
	    rcpt,
	    duration_to_text(time(NULL) - e->creation),
	    status);
}

void
queue_flow_control(void)
{
	size_t	bufsz;
	int	oldlimit = limit;
	int	set, unset;

	bufsz = p_pony->bytes_queued;
	if (bufsz <= flow_agent_lowat)
		limit &= ~LIMIT_AGENT;
	else if (bufsz > flow_agent_hiwat)
		limit |= LIMIT_AGENT;

	if (p_scheduler->bytes_queued <= flow_scheduler_lowat)
		limit &= ~LIMIT_SCHEDULER;
	else if (p_scheduler->bytes_queued > flow_scheduler_hiwat)
		limit |= LIMIT_SCHEDULER;

	set = limit & (limit ^ oldlimit);
	unset = oldlimit & (limit ^ oldlimit);

	if (set & LIMIT_SCHEDULER) {
		log_warnx("warn: queue: Hiwat reached on scheduler buffer: "
		    "suspending transfer, delivery and lookup input");
		mproc_disable(p_pony);
		mproc_disable(p_lka);
	}
	else if (unset & LIMIT_SCHEDULER) {
		log_warnx("warn: queue: Down to lowat on scheduler buffer: "
		    "resuming transfer, delivery and lookup input");
		mproc_enable(p_pony);
		mproc_enable(p_lka);
	}

	if (set & LIMIT_AGENT) {
		log_warnx("warn: queue: Hiwat reached on transfer and delivery "
		    "buffers: suspending scheduler input");
		mproc_disable(p_scheduler);
	}
	else if (unset & LIMIT_AGENT) {
		log_warnx("warn: queue: Down to lowat on transfer and delivery "
		    "buffers: resuming scheduler input");
		mproc_enable(p_scheduler);
	}
}
