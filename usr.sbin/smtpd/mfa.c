/*	$OpenBSD: mfa.c,v 1.17 2009/03/08 19:11:22 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	mfa_shutdown(void);
void		mfa_sig_handler(int, short, void *);
void		mfa_dispatch_parent(int, short, void *);
void		mfa_dispatch_smtp(int, short, void *);
void		mfa_dispatch_lka(int, short, void *);
void		mfa_dispatch_control(int, short, void *);
void		mfa_setup_events(struct smtpd *);
void		mfa_disable_events(struct smtpd *);
void		mfa_timeout(int, short, void *);

void		mfa_test_mail(struct smtpd *, struct message *, int);
void		mfa_test_rcpt(struct smtpd *, struct message_recipient *, int);
int		mfa_ruletest_rcpt(struct smtpd *, struct path *, struct sockaddr_storage *);
int		mfa_check_source(struct map *, struct sockaddr_storage *);
int		mfa_match_mask(struct sockaddr_storage *, struct netaddr *);

int		strip_source_route(char *, size_t);

void
mfa_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mfa_shutdown();
		break;
	default:
		fatalx("mfa_sig_handler: unexpected signal");
	}
}

void
mfa_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_PARENT];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mfa_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_MAIL:
			mfa_test_mail(env, imsg.data, PROC_SMTP);
			break;
		case IMSG_MFA_RCPT:
			mfa_test_rcpt(env, imsg.data, PROC_SMTP);
			break;
		default:
			log_debug("mfa_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mfa_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss;

			ss = imsg.data;
			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_MAIL,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	 *ss;

			ss = imsg.data;
			if (ss->msg.flags & F_MESSAGE_ENQUEUED)
				imsg_compose(env->sc_ibufs[PROC_CONTROL], IMSG_MFA_RCPT,
				    0, 0, -1, ss, sizeof(*ss));
			else
				imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_RCPT,
				    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		default:
			log_debug("mfa_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_CONTROL];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("mfa_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_RCPT:
			mfa_test_rcpt(env, imsg.data, PROC_CONTROL);
			break;
		default:
			log_debug("mfa_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mfa_shutdown(void)
{
	log_info("mail filter exiting");
	_exit(0);
}

void
mfa_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, mfa_timeout, env);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
mfa_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

void
mfa_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
mfa(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	mfa_dispatch_parent },
		{ PROC_SMTP,	mfa_dispatch_smtp },
		{ PROC_LKA,	mfa_dispatch_lka },
		{ PROC_CONTROL,	mfa_dispatch_control},
	};

	switch (pid = fork()) {
	case -1:
		fatal("mfa: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

//	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mfa: chroot");
	if (chdir("/") == -1)
		fatal("mfa: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("mail filter agent");
	smtpd_process = PROC_MFA;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mfa: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, mfa_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mfa_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(env, peers, 4);
	config_peers(env, peers, 4);

	mfa_setup_events(env);
	event_dispatch();
	mfa_shutdown();

	return (0);
}

int
msg_cmp(struct message *m1, struct message *m2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (m1->id - m2->id > 0)
		return (1);
	else if (m1->id - m2->id < 0)
		return (-1);
	else
		return (0);
}

void
mfa_test_mail(struct smtpd *env, struct message *m, int sender)
{
	struct submit_status	 ss;

	ss.id = m->id;
	ss.code = 530;
	ss.u.path = m->sender;

	if (strip_source_route(ss.u.path.user, sizeof(ss.u.path.user)))
		goto refuse;

	if (! valid_localpart(ss.u.path.user) ||
	    ! valid_domainpart(ss.u.path.domain)) {
		/*
		 * "MAIL FROM:<>" is the exception we allow.
		 */
		if (!(ss.u.path.user[0] == '\0' && ss.u.path.domain[0] == '\0'))
			goto refuse;
	}

	/* Current policy is to allow all well-formed addresses. */
	goto accept;

refuse:
	imsg_compose(env->sc_ibufs[sender], IMSG_MFA_MAIL, 0, 0, -1, &ss,
	    sizeof(ss));
	return;

accept:
	ss.code = 250;
	imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_MAIL, 0,
	    0, -1, &ss, sizeof(ss));
}

void
mfa_test_rcpt(struct smtpd *env, struct message_recipient *mr, int sender)
{
	struct submit_status	 ss;

	ss.id = mr->id;
	ss.code = 530;
	ss.u.path = mr->path;
	ss.ss = mr->ss;
	ss.msg = mr->msg;

	ss.flags = mr->flags;

	strip_source_route(ss.u.path.user, sizeof(ss.u.path.user));

	if (! valid_localpart(ss.u.path.user) ||
	    ! valid_domainpart(ss.u.path.domain))
		goto refuse;

	if (sender == PROC_SMTP && (ss.flags & F_MESSAGE_AUTHENTICATED))
		goto accept;

	if (mfa_ruletest_rcpt(env, &ss.u.path, &ss.ss))
		goto accept;
		
refuse:
	imsg_compose(env->sc_ibufs[sender], IMSG_MFA_RCPT, 0, 0, -1, &ss,
	    sizeof(ss));
	return;

accept:
	ss.code = 250;
	imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_RCPT, 0, 0, -1,
	    &ss, sizeof(ss));
}

int
mfa_ruletest_rcpt(struct smtpd *env, struct path *path, struct sockaddr_storage *ss)
{
	struct rule *r;
	struct cond *cond;
	struct map *map;
	struct mapel *me;

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		if (! mfa_check_source(r->r_sources, ss))
			continue;

		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {
			if (cond->c_type == C_ALL) {
				path->rule = *r;
				return 1;
			}

			if (cond->c_type == C_DOM) {
				cond->c_match = map_find(env, cond->c_map);
				if (cond->c_match == NULL)
					fatal("mfa failed to lookup map.");

				map = cond->c_match;
				TAILQ_FOREACH(me, &map->m_contents, me_entry) {
					log_debug("matching: %s to %s",
					    path->domain, me->me_key.med_string);
					if (hostname_match(path->domain, me->me_key.med_string)) {
						path->rule = *r;
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

int
mfa_check_source(struct map *map, struct sockaddr_storage *ss)
{
	struct mapel *me;

	if (ss == NULL) {
		/* This happens when caller is part of an internal
		 * lookup (ie: alias resolved to a remote address)
		 */
		return 1;
	}

	TAILQ_FOREACH(me, &map->m_contents, me_entry) {

		if (ss->ss_family != me->me_key.med_addr.ss.ss_family)
			continue;

		if (ss->ss_len == me->me_key.med_addr.ss.ss_len)
			continue;

		if (mfa_match_mask(ss, &me->me_key.med_addr))
			return 1;
	}

	return 0;
}

int
mfa_match_mask(struct sockaddr_storage *ss, struct netaddr *ssmask)
{
	if (ss->ss_family == AF_INET) {
		struct sockaddr_in *ssin = (struct sockaddr_in *)ss;
		struct sockaddr_in *ssinmask = (struct sockaddr_in *)&ssmask->ss;

		if ((ssin->sin_addr.s_addr & ssinmask->sin_addr.s_addr) ==
		    ssinmask->sin_addr.s_addr)
			return (1);
		return (0);
	}

	if (ss->ss_family == AF_INET6) {
		struct in6_addr	*in;
		struct in6_addr	*inmask;
		struct in6_addr	 mask;
		int		 i;

		bzero(&mask, sizeof(mask));
		for (i = 0; i < (128 - ssmask->bits) / 8; i++)
			mask.s6_addr[i] = 0xff;
		i = ssmask->bits % 8;
		if (i)
			mask.s6_addr[ssmask->bits / 8] = 0xff00 >> i;

		in = &((struct sockaddr_in6 *)ss)->sin6_addr;
		inmask = &((struct sockaddr_in6 *)&ssmask->ss)->sin6_addr;

		for (i = 0; i < 16; i++) {
			if ((in->s6_addr[i] & mask.s6_addr[i]) !=
			    inmask->s6_addr[i])
				return (0);
		}
		return (1);
	}

	return (0);
}

int
strip_source_route(char *buf, size_t len)
{
	char *p;

	p = strchr(buf, ':');
	if (p != NULL) {
		p++;
		memmove(buf, p, strlen(p) + 1);
		return 1;
	}

	return 0;
}
