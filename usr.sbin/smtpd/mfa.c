/*	$OpenBSD: mfa.c,v 1.31 2009/05/24 14:38:56 jacekm Exp $	*/

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

void		mfa_test_mail(struct smtpd *, struct message *);
void		mfa_test_rcpt(struct smtpd *, struct message *);
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
			fatalx("mfa_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONF_START:
			if ((env->sc_rules_reload = calloc(1, sizeof(*env->sc_rules))) == NULL)
				fatal("mfa_dispatch_parent: calloc");
			if ((env->sc_maps_reload = calloc(1, sizeof(*env->sc_maps))) == NULL)
				fatal("mfa_dispatch_parent: calloc");
			TAILQ_INIT(env->sc_rules_reload);
			TAILQ_INIT(env->sc_maps_reload);
			break;
		case IMSG_CONF_RULE: {
			struct rule *rule = imsg.data;

			IMSG_SIZE_CHECK(rule);

			rule = calloc(1, sizeof(*rule));
			if (rule == NULL)
				fatal("mfa_dispatch_parent: calloc");
			*rule = *(struct rule *)imsg.data;

			TAILQ_INIT(&rule->r_conditions);
			TAILQ_INSERT_TAIL(env->sc_rules_reload, rule, r_entry);
			break;
		}
		case IMSG_CONF_CONDITION: {
			struct rule *r = TAILQ_LAST(env->sc_rules_reload, rulelist);
			struct cond *cond = imsg.data;

			IMSG_SIZE_CHECK(cond);

			cond = calloc(1, sizeof(*cond));
			if (cond == NULL)
				fatal("mfa_dispatch_parent: calloc");
			*cond = *(struct cond *)imsg.data;

			TAILQ_INSERT_TAIL(&r->r_conditions, cond, c_entry);
			break;
		}
		case IMSG_CONF_MAP: {
			struct map *m = imsg.data;

			IMSG_SIZE_CHECK(m);

			m = calloc(1, sizeof(*m));
			if (m == NULL)
				fatal("mfa_dispatch_parent: calloc");
			*m = *(struct map *)imsg.data;

			TAILQ_INIT(&m->m_contents);
			TAILQ_INSERT_TAIL(env->sc_maps_reload, m, m_entry);
			break;
		}
		case IMSG_CONF_RULE_SOURCE: {
			struct rule *rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			char *sourcemap = imsg.data;
			void *temp = env->sc_maps;

			/* map lookup must be done in the reloaded conf */
			env->sc_maps = env->sc_maps_reload;
			rule->r_sources = map_findbyname(env, sourcemap);
			if (rule->r_sources == NULL)
				fatalx("maps inconsistency");
			env->sc_maps = temp;
			break;
		}
		case IMSG_CONF_MAP_CONTENT: {
			struct map *m = TAILQ_LAST(env->sc_maps_reload, maplist);
			struct mapel *mapel = imsg.data;
			
			IMSG_SIZE_CHECK(mapel);
			
			mapel = calloc(1, sizeof(*mapel));
			if (mapel == NULL)
				fatal("mfa_dispatch_parent: calloc");
			*mapel = *(struct mapel *)imsg.data;

			TAILQ_INSERT_TAIL(&m->m_contents, mapel, me_entry);
			break;
		}
		case IMSG_CONF_END: {
			void *temp;
			struct rule *r;
			struct cond *cond;
			struct map *m;
			struct mapel *mapel;
			
			/* switch and destroy old ruleset */
			temp = env->sc_rules;
			env->sc_rules = env->sc_rules_reload;
			env->sc_rules_reload = temp;

			temp = env->sc_maps;
			env->sc_maps = env->sc_maps_reload;
			env->sc_maps_reload = temp;
			
			if (env->sc_rules_reload) {
				while ((r = TAILQ_FIRST(env->sc_rules_reload))) {
					TAILQ_REMOVE(env->sc_rules_reload, r, r_entry);
					while ((cond = TAILQ_FIRST(&r->r_conditions))) {
						TAILQ_REMOVE(&r->r_conditions, cond, c_entry);
						free(cond);
					}
					free(r);
				}
				free(env->sc_rules_reload);
				env->sc_rules_reload = NULL;
			}

			if (env->sc_maps_reload) {
				while ((m = TAILQ_FIRST(env->sc_maps_reload))) {
					TAILQ_REMOVE(env->sc_maps_reload, m, m_entry);
					while ((mapel = TAILQ_FIRST(&m->m_contents))) {
						TAILQ_REMOVE(&m->m_contents, mapel, me_entry);
						free(mapel);
					}
					free(m);
				}
				free(env->sc_maps_reload);
				env->sc_maps_reload = NULL;
			}

			break;
		}
		default:
			log_warnx("mfa_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_parent: unexpected imsg");
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
			fatalx("mfa_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_MAIL: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			mfa_test_mail(env, m);
			break;
		}

		case IMSG_MFA_RCPT: {
			struct message	*m = imsg.data;

			IMSG_SIZE_CHECK(m);

			mfa_test_rcpt(env, m);
			break;
		}

		default:
			log_warnx("mfa_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_smtp: unexpected imsg");
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
			fatalx("mfa_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_MAIL,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_RCPT,
			    0, 0, -1, ss, sizeof(*ss));
			break;
		}
		default:
			log_warnx("mfa_dispatch_lka: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_lka: unexpected imsg");
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
			fatalx("mfa_dispatch_control: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("mfa_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("mfa_dispatch_control: unexpected imsg");
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
}

void
mfa_disable_events(struct smtpd *env)
{
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

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mfa: chroot");
	if (chdir("/") == -1)
		fatal("mfa: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_MFA;
	setproctitle("%s", env->sc_title[smtpd_process]);

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

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

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
mfa_test_mail(struct smtpd *env, struct message *m)
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
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_MAIL, 0, 0, -1, &ss,
	    sizeof(ss));
	return;

accept:
	ss.code = 250;
	imsg_compose(env->sc_ibufs[PROC_LKA], IMSG_LKA_MAIL, 0,
	    0, -1, &ss, sizeof(ss));
}

void
mfa_test_rcpt(struct smtpd *env, struct message *m)
{
	struct submit_status	 ss;

	if (! valid_message_id(m->message_id))
		fatalx("mfa_test_rcpt: received corrupted message_id");

	ss.id = m->session_id;
	ss.code = 530;
	ss.u.path = m->session_rcpt;
	ss.ss = m->session_ss;
	ss.msg = *m;
	ss.flags = m->flags;

	strip_source_route(ss.u.path.user, sizeof(ss.u.path.user));

	if (! valid_localpart(ss.u.path.user) ||
	    ! valid_domainpart(ss.u.path.domain))
		goto refuse;

	if (ss.flags & F_MESSAGE_AUTHENTICATED)
		ss.u.path.flags |= F_PATH_AUTHENTICATED;

	if (mfa_ruletest_rcpt(env, &ss.u.path, &ss.ss))
		goto accept;
		
refuse:
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_MFA_RCPT, 0, 0, -1, &ss,
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
		if (!(path->flags & F_PATH_AUTHENTICATED) &&
		    ! mfa_check_source(r->r_sources, ss))
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

		if (ss->ss_len != me->me_key.med_addr.ss.ss_len)
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
