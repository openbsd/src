/*	$OpenBSD: lka.c,v 1.36 2009/03/20 09:34:34 gilles Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <netdb.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <keynote.h>

#include "smtpd.h"

__dead void	lka_shutdown(void);
void		lka_sig_handler(int, short, void *);
void		lka_dispatch_parent(int, short, void *);
void		lka_dispatch_mfa(int, short, void *);
void		lka_dispatch_smtp(int, short, void *);
void		lka_dispatch_queue(int, short, void *);
void		lka_dispatch_runner(int, short, void *);
void		lka_dispatch_mta(int, short, void *);
void		lka_setup_events(struct smtpd *);
void		lka_disable_events(struct smtpd *);
int		lka_verify_mail(struct smtpd *, struct path *);
int		lka_resolve_mail(struct smtpd *, struct rule *, struct path *);
int		lka_forward_file(struct passwd *);
size_t		lka_expand(char *, size_t, struct path *);
int		aliases_exist(struct smtpd *, char *);
int		aliases_get(struct smtpd *, struct aliaseslist *, char *);
int		lka_resolve_alias(struct path *, struct alias *);
int		lka_parse_include(char *);
int		lka_check_source(struct smtpd *, struct map *, struct sockaddr_storage *);
int		lka_match_mask(struct sockaddr_storage *, struct netaddr *);
int		aliases_virtual_get(struct smtpd *, struct aliaseslist *, struct path *);
int		aliases_virtual_exist(struct smtpd *, struct path *);
int		lka_resolve_path(struct smtpd *, struct path *);
void		lka_expand_rcpt(struct smtpd *, struct aliaseslist *, struct lkasession *);
int		lka_expand_rcpt_iteration(struct smtpd *, struct aliaseslist *, struct lkasession *);
void		lka_rcpt_action(struct smtpd *, struct path *);
void		lka_clear_aliaseslist(struct aliaseslist *);
int		lka_encode_credentials(char *, char *);

void
lka_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		lka_shutdown();
		break;
	default:
		fatalx("lka_sig_handler: unexpected signal");
	}
}

void
lka_dispatch_parent(int sig, short event, void *p)
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
			fatal("parent_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_FORWARD_OPEN: {
			int fd;
			struct forward_req	*fwreq;
			struct lkasession	key;
			struct lkasession	*lkasession;

			fwreq = imsg.data;

			key.id = fwreq->id;
			lkasession = SPLAY_FIND(lkatree, &env->lka_sessions, &key);
			if (lkasession == NULL)
				fatal("lka_dispatch_parent: lka session is gone");
			fd = imsg_get_fd(ibuf, &imsg);
			--lkasession->pending;

			if (fd == -1) {
				if (fwreq->pw_name[0] != '\0') {
					lkasession->ss.code = 530;
					lkasession->flags |= F_ERROR;
				}
			}
			else {
				if (! forwards_get(fd, &lkasession->aliaseslist)) {
					lkasession->ss.code = 530;
					lkasession->flags |= F_ERROR;
				}
				close(fd);
			}
			lka_expand_rcpt(env, &lkasession->aliaseslist, lkasession);
			break;
		}
		default:
			log_debug("parent_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_dispatch_mfa(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MFA];
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
			fatal("lka_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss;

			ss = imsg.data;
			ss->code = 530;

			if (ss->u.path.user[0] == '\0' && ss->u.path.domain[0] == '\0')
				ss->code = 250;
			else
				if (lka_verify_mail(env, &ss->u.path))
					ss->code = 250;

			imsg_compose(ibuf, IMSG_LKA_MAIL, 0, 0, -1,
				ss, sizeof(*ss));

			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	*ss;
			struct message		message;
			struct lkasession	*lkasession;
			struct forward_req	 fwreq;
			int ret;

			ss = imsg.data;
			ss->code = 530;
			
			if (IS_RELAY(ss->u.path.rule.r_action)) {
				ss->code = 250;
				message = ss->msg;
				message.recipient = ss->u.path;
				imsg_compose(env->sc_ibufs[PROC_QUEUE],
				    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
				    &message, sizeof(struct message));
				imsg_compose(env->sc_ibufs[PROC_QUEUE],
				    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1,
				    &message, sizeof(struct message));
				break;
			}

			if (! lka_resolve_path(env, &ss->u.path)) {
				imsg_compose(ibuf, IMSG_LKA_RCPT, 0, 0, -1,
				    ss, sizeof(*ss));
				break;
			}

			ss->code = 250;

			lkasession = calloc(1, sizeof(struct lkasession));
			if (lkasession == NULL)
				fatal("lka_dispatch_mfa: calloc");
			lkasession->id = queue_generate_id();
			lkasession->path = ss->u.path;
			lkasession->message = ss->msg;
			lkasession->ss = *ss;

			TAILQ_INIT(&lkasession->aliaseslist);

			SPLAY_INSERT(lkatree, &env->lka_sessions, lkasession);

			ret = 0;
			if (lkasession->path.flags & F_ACCOUNT) {
				fwreq.id = lkasession->id;
				(void)strlcpy(fwreq.pw_name, ss->u.path.pw_name, sizeof(fwreq.pw_name));
				imsg_compose(env->sc_ibufs[PROC_PARENT], IMSG_PARENT_FORWARD_OPEN, 0, 0, -1,
				    &fwreq, sizeof(fwreq));
				++lkasession->pending;
				break;
			}
			else if (lkasession->path.flags & F_ALIAS) {
				ret = aliases_get(env, &lkasession->aliaseslist, lkasession->path.user);
			}
			else if (lkasession->path.flags & F_VIRTUAL) {
				ret = aliases_virtual_get(env, &lkasession->aliaseslist, &lkasession->path);
			}
			else
				fatal("lka_dispatch_mfa: path with illegal flag");

			if (ret == 0) {
				/* No aliases ... */
				log_debug("expansion resulted in empty list");
				ss->code = 530;
				imsg_compose(ibuf, IMSG_LKA_RCPT, 0, 0,
				    -1, ss, sizeof(*ss));
				break;
			}

			lka_expand_rcpt(env, &lkasession->aliaseslist, lkasession);

			break;
		}
		default:
			log_debug("lka_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_dispatch_mta(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MTA];
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
			fatal("lka_dispatch_mta: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MX: {
			struct mxreq *mxreq;
			struct mxrep mxrep;
			struct addrinfo hints, *res, *resp;
			char **mx = NULL;
			char *lmx[1];
			int len, i;
			int mxcnt;
			int error;
			struct mxhost mxhost;
			char *secret;

			mxreq = imsg.data;
			mxrep.id = mxreq->id;
			mxrep.getaddrinfo_error = 0;

			if (mxreq->rule.r_action == A_RELAY) {
				log_debug("attempting to resolve %s", mxreq->hostname);
				len = getmxbyname(mxreq->hostname, &mx);
				if (len < 0) {
					mxrep.getaddrinfo_error = len;
					imsg_compose(ibuf, IMSG_LKA_MX_END, 0, 0, -1,
					    &mxrep, sizeof(struct mxrep));
					break;
				}
				if (len == 0) {
					lmx[0] = mxreq->hostname;
					mx = lmx;
					len = 1;
				}
			}
			else if (mxreq->rule.r_action == A_RELAYVIA) {
				lmx[0] = mxreq->rule.r_value.relayhost.hostname;
				log_debug("attempting to resolve %s:%d (forced)", lmx[0],
				    ntohs(mxreq->rule.r_value.relayhost.port));
				mx = lmx;
				len = 1;

			}
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_protocol = IPPROTO_TCP;

			for (i = 0; i < len; ++i) {

				error = getaddrinfo(mx[i], NULL, &hints, &res);
				if (error) {
					mxrep.getaddrinfo_error = error;
					imsg_compose(ibuf, IMSG_LKA_MX_END, 0, 0, -1,
					    &mxrep, sizeof(struct mxrep));
				}

				if (mxreq->rule.r_action == A_RELAYVIA)
					mxhost.flags = mxreq->rule.r_value.relayhost.flags;

				if (mxhost.flags & F_AUTH) {
					secret = map_dblookup(env, "secrets", mx[i]);
					if (secret == NULL) {
						log_warn("no credentials for relay through \"%s\"", mx[i]);
						freeaddrinfo(res);
						continue;
					}
				}
				if (secret)
					lka_encode_credentials(mxhost.credentials, secret);

				for (resp = res; resp != NULL && mxcnt < MAX_MX_COUNT; resp = resp->ai_next) {

					if (resp->ai_family == PF_INET) {
						struct sockaddr_in *ssin;
						
						mxhost.ss = *(struct sockaddr_storage *)resp->ai_addr;
						
						ssin = (struct sockaddr_in *)&mxhost.ss;
						if (mxreq->rule.r_value.relayhost.port != 0) {
							ssin->sin_port = mxreq->rule.r_value.relayhost.port;
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							continue;
						}

						switch (mxhost.flags & F_SSL) {
						case F_SSMTP:
							ssin->sin_port = htons(465);
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							break;
						case F_SSL:
							ssin->sin_port = htons(465);
							mxrep.mxhost = mxhost;
							mxrep.mxhost.flags &= ~F_STARTTLS;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
						case F_STARTTLS:
							ssin->sin_port = htons(25);
							mxrep.mxhost = mxhost;
							mxrep.mxhost.flags &= ~F_SSMTP;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							break;
						default:
							ssin->sin_port = htons(25);
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
						}
					}
					
					if (resp->ai_family == PF_INET6) {
						struct sockaddr_in6 *ssin6;
						
						mxhost.ss = *(struct sockaddr_storage *)resp->ai_addr;
						ssin6 = (struct sockaddr_in6 *)&mxhost.ss;
						if (mxreq->rule.r_value.relayhost.port != 0) {
							ssin6->sin6_port = mxreq->rule.r_value.relayhost.port;
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							continue;
						}
						
						switch (mxhost.flags & F_SSL) {
						case F_SSMTP:
							ssin6->sin6_port = htons(465);
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							break;
						case F_SSL:
							ssin6->sin6_port = htons(465);
							mxrep.mxhost = mxhost;
							mxrep.mxhost.flags &= ~F_STARTTLS;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
						case F_STARTTLS:
							ssin6->sin6_port = htons(25);
							mxrep.mxhost = mxhost;
							mxrep.mxhost.flags &= ~F_SSMTP;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
							break;
						default:
							ssin6->sin6_port = htons(25);
							mxrep.mxhost = mxhost;
							imsg_compose(ibuf, IMSG_LKA_MX, 0, 0, -1, &mxrep,
							    sizeof(struct mxrep));
						}
					}
				}
				freeaddrinfo(res);
				free(secret);
				bzero(&mxhost.credentials, MAX_LINE_SIZE);
			}

			mxrep.getaddrinfo_error = error;

			imsg_compose(ibuf, IMSG_LKA_MX_END, 0, 0, -1,
			    &mxrep, sizeof(struct mxrep));

			if (mx != lmx)
				free(mx);

			break;
		}

		default:
			log_debug("lka_dispatch_mta: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_dispatch_smtp(int sig, short event, void *p)
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
			fatal("lka_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_HOST: {
			struct sockaddr *sa;
			char addr[NI_MAXHOST];
			struct addrinfo hints, *res;
			struct session *s;

			s = imsg.data;
			sa = (struct sockaddr *)&s->s_ss;
			if (getnameinfo(sa, sa->sa_len, addr, sizeof(addr),
			    NULL, 0, NI_NAMEREQD))
				break;

			memset(&hints, 0, sizeof(hints));
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_flags = AI_NUMERICHOST;
			if (getaddrinfo(addr, NULL, &hints, &res) == 0) {
				/* Malicious PTR record. */
				freeaddrinfo(res);
				break;
			}

			strlcpy(s->s_hostname, addr, sizeof(s->s_hostname));
			imsg_compose(ibuf, IMSG_LKA_HOST, 0, 0, -1, s,
			    sizeof(struct session));
			break;
		}
		default:
			log_debug("lka_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_QUEUE];
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
			fatal("lka_dispatch_queue: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("lka_dispatch_queue: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_RUNNER];
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
			fatal("lka_dispatch_runner: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("lka_dispatch_runner: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
lka_shutdown(void)
{
	log_info("lookup agent exiting");
	_exit(0);
}

void
lka_setup_events(struct smtpd *env)
{
}

void
lka_disable_events(struct smtpd *env)
{
}

pid_t
lka(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	lka_dispatch_parent },
		{ PROC_MFA,	lka_dispatch_mfa },
		{ PROC_QUEUE,	lka_dispatch_queue },
		{ PROC_SMTP,	lka_dispatch_smtp },
		{ PROC_RUNNER,	lka_dispatch_runner },
		{ PROC_MTA,	lka_dispatch_mta }
	};

	switch (pid = fork()) {
	case -1:
		fatal("lka: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

//	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	setproctitle("lookup agent");
	smtpd_process = PROC_LKA;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("lka: cannot drop privileges");
#endif

	event_init();
	SPLAY_INIT(&env->lka_sessions);

	signal_set(&ev_sigint, SIGINT, lka_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, lka_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(env, peers, 6);
	config_peers(env, peers, 6);

	lka_setup_events(env);
	event_dispatch();
	lka_shutdown();

	return (0);
}

int
lka_verify_mail(struct smtpd *env, struct path *path)
{
	struct rule *r;
	struct cond *cond;
	struct map *map;
	struct mapel *me;

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {
			if (cond->c_type == C_ALL) {
				path->rule = *r;
				if (r->r_action == A_MBOX ||
				    r->r_action == A_MAILDIR) {
					return lka_resolve_mail(env, r, path);
				}
				return 1;
			}

			if (cond->c_type == C_DOM) {
				cond->c_match = map_find(env, cond->c_map);
				if (cond->c_match == NULL)
					fatal("lka failed to lookup map.");

				map = cond->c_match;
				TAILQ_FOREACH(me, &map->m_contents, me_entry) {
					if (hostname_match(path->domain, me->me_key.med_string)) {
						path->rule = *r;
						if (r->r_action == A_MBOX ||
						    r->r_action == A_MAILDIR ||
						    r->r_action == A_EXT) {
							return lka_resolve_mail(env, r, path);
						}
						return 1;
					}
				}
			}
		}
	}
	path->rule.r_action = A_RELAY;
	return 1;
}

int
lka_resolve_mail(struct smtpd *env, struct rule *rule, struct path *path)
{
	char username[MAXLOGNAME];
	struct passwd *pw;
	char *p;

	(void)strlcpy(username, path->user, sizeof(username));

	for (p = &username[0]; *p != '\0' && *p != '+'; ++p)
		*p = tolower((int)*p);
	*p = '\0';

	if (aliases_virtual_exist(env, path))
		path->flags |= F_VIRTUAL;
	else if (aliases_exist(env, username))
		path->flags |= F_ALIAS;
	else {
		pw = safe_getpwnam(username);
		if (pw == NULL)
			return 0;
		(void)strlcpy(path->pw_name, pw->pw_name,
		    sizeof(path->pw_name));
		if (lka_expand(path->rule.r_value.path,
		    sizeof(path->rule.r_value.path), path) >=
		    sizeof(path->rule.r_value.path))
			return 0;
	}

	return 1;
}

size_t
lka_expand(char *buf, size_t len, struct path *path)
{
	char *p, *pbuf;
	struct rule r;
	size_t ret;
	struct passwd *pw;

	bzero(r.r_value.path, MAXPATHLEN);
	pbuf = r.r_value.path;

	ret = 0;
	for (p = path->rule.r_value.path; *p != '\0'; ++p) {
		if (p == path->rule.r_value.path && *p == '~') {
			if (*(p + 1) == '/' || *(p + 1) == '\0') {
				pw = safe_getpwnam(path->pw_name);
				if (pw == NULL)
					continue;

				ret += strlcat(pbuf, pw->pw_dir, len);
				if (ret >= len)
					return ret;
				pbuf += strlen(pw->pw_dir);
				continue;
			}

			if (*(p + 1) != '/') {
				char username[MAXLOGNAME];
				char *delim;

				ret = strlcpy(username, p + 1,
				    sizeof(username));
				delim = strchr(username, '/');
				if (delim == NULL && ret >= sizeof(username)) {
					continue;
				}

				if (delim != NULL) {
					*delim = '\0';
				}

				pw = safe_getpwnam(username);
				if (pw == NULL)
					continue;

				ret += strlcat(pbuf, pw->pw_dir, len);
				if (ret >= len)
					return ret;
				pbuf += strlen(pw->pw_dir);
				p += strlen(username);
				continue;
			}
		}
		if (strncmp(p, "%a", 2) == 0) {
			ret += strlcat(pbuf, path->user, len);
			if (ret >= len)
				return ret;
			pbuf += strlen(path->user);
			++p;
			continue;
		}
		if (strncmp(p, "%u", 2) == 0) {
			ret += strlcat(pbuf, path->pw_name, len);
			if (ret >= len)
				return ret;
			pbuf += strlen(path->pw_name);
			++p;
			continue;
		}
		if (strncmp(p, "%d", 2) == 0) {
			ret += strlcat(pbuf, path->domain, len);
			if (ret >= len)
				return ret;
			pbuf += strlen(path->domain);
			++p;
			continue;
		}
		if (*p == '%' && isdigit((int)*(p+1)) && *(p+2) == 'a') {
			size_t idx;

			idx = *(p+1) - '0';
			if (idx < strlen(path->user))
				*pbuf++ = path->user[idx];
			p+=2;
			++ret;
			continue;
		}
		if (*p == '%' && isdigit((int)*(p+1)) && *(p+2) == 'u') {
			size_t idx;

			idx = *(p+1) - '0';
			if (idx < strlen(path->pw_name))
				*pbuf++ = path->pw_name[idx];
			p+=2;
			++ret;
			continue;
		}
		if (*p == '%' && isdigit((int)*(p+1)) && *(p+2) == 'd') {
			size_t idx;

			idx = *(p+1) - '0';
			if (idx < strlen(path->domain))
				*pbuf++ = path->domain[idx];
			p+=2;
			++ret;
			continue;
		}

		*pbuf++ = *p;
		++ret;
	}

	memcpy(path->rule.r_value.path, r.r_value.path, ret);

	return ret;
}

int
lka_resolve_alias(struct path *path, struct alias *alias)
{
	switch (alias->type) {
	case ALIAS_USERNAME:
		log_debug("USERNAME: %s", alias->u.username);
		if (strlcpy(path->pw_name, alias->u.username,
			sizeof(path->pw_name)) >= sizeof(path->pw_name))
			return 0;
		break;

	case ALIAS_FILENAME:
		log_debug("FILENAME: %s", alias->u.filename);
		path->rule.r_action = A_FILENAME;
		strlcpy(path->u.filename, alias->u.filename,
		    sizeof(path->u.filename));
		break;

	case ALIAS_FILTER:
		log_debug("FILTER: %s", alias->u.filter);
		path->rule.r_action = A_EXT;
		strlcpy(path->rule.r_value.command, alias->u.filter + 2,
		    sizeof(path->rule.r_value.command));
		path->rule.r_value.command[strlen(path->rule.r_value.command) - 1] = '\0';
		break;

	case ALIAS_ADDRESS:
		log_debug("ADDRESS: %s@%s", alias->u.path.user, alias->u.path.domain);
		*path = alias->u.path;
		break;
	case ALIAS_INCLUDE:
		fatalx("lka_resolve_alias: unexpected type");
		break;
	}
	return 1;
}

void
lka_expand_rcpt(struct smtpd *env, struct aliaseslist *aliases, struct lkasession *lkasession)
{
	int	ret;
	struct alias	*alias;
	struct message	message;

	ret = 0;
	while (! (lkasession->flags & F_ERROR) &&
	    ! lkasession->pending && lkasession->iterations < 5) {
		++lkasession->iterations;
		ret = lka_expand_rcpt_iteration(env, &lkasession->aliaseslist, lkasession);
		if (ret == -1) {
			lkasession->ss.code = 530;
			lkasession->flags |= F_ERROR;
		}
		
		if (lkasession->pending || ret <= 0)
			break;
	}
	
	if (lkasession->pending)
		return;
	
	if (lkasession->flags & F_ERROR) {
		lka_clear_aliaseslist(&lkasession->aliaseslist);
		imsg_compose(env->sc_ibufs[PROC_MFA], IMSG_LKA_RCPT, 0, 0,
		    -1, &lkasession->ss, sizeof(struct submit_status));
	}
	else if (ret == 0) {
		log_debug("expansion resulted in empty list");
		message = lkasession->message;
		message.recipient = lkasession->path;
		imsg_compose(env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
		    &message, sizeof(struct message));
		imsg_compose(env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1,
		    &message, sizeof(struct message));
	}
	else {
		log_debug("a list of aliases is available");
		message = lkasession->message;
		while ((alias = TAILQ_FIRST(&lkasession->aliaseslist)) != NULL) {
			bzero(&message.recipient, sizeof(struct path));
			
			lka_resolve_alias(&message.recipient, alias);
			lka_rcpt_action(env, &message.recipient);
			
			imsg_compose(env->sc_ibufs[PROC_QUEUE],
			    IMSG_QUEUE_SUBMIT_ENVELOPE, 0, 0, -1,
			    &message, sizeof(struct message));
			
			TAILQ_REMOVE(&lkasession->aliaseslist, alias, entry);
			free(alias);
		}
		imsg_compose(env->sc_ibufs[PROC_QUEUE],
		    IMSG_QUEUE_COMMIT_ENVELOPES, 0, 0, -1, &message,
		    sizeof(struct message));
	}
	SPLAY_REMOVE(lkatree, &env->lka_sessions, lkasession);
	free(lkasession);
}

int
lka_expand_rcpt_iteration(struct smtpd *env, struct aliaseslist *aliases, struct lkasession *lkasession)
{
	u_int8_t done = 1;
	struct alias *rmalias = NULL;
	struct alias *alias;
	struct forward_req fwreq;

	rmalias = NULL;
	TAILQ_FOREACH(alias, aliases, entry) {
		if (rmalias) {
			TAILQ_REMOVE(aliases, rmalias, entry);
			free(rmalias);
			rmalias = NULL;
		}
		
		if (alias->type == ALIAS_ADDRESS) {
			if (aliases_virtual_get(env, aliases, &alias->u.path)) {
				rmalias = alias;
				done = 0;
			}
		}
		
		else if (alias->type == ALIAS_USERNAME) {
			if (aliases_get(env, aliases, alias->u.username)) {
				done = 0;
				rmalias = alias;
			}
			else {
				done = 0;
				fwreq.id = lkasession->id;
				(void)strlcpy(fwreq.pw_name, alias->u.username, sizeof(fwreq.pw_name));
				imsg_compose(env->sc_ibufs[PROC_PARENT], IMSG_PARENT_FORWARD_OPEN, 0, 0, -1,
				    &fwreq, sizeof(fwreq));
				++lkasession->pending;
				rmalias = alias;
			}
		}
	}
	if (rmalias) {
		TAILQ_REMOVE(aliases, rmalias, entry);
		free(rmalias);
		rmalias = NULL;
	}

	if (!done && lkasession->iterations == 5) {
		log_debug("loop detected");
		return -1;
	}

	if (TAILQ_FIRST(aliases) == NULL)
		return 0;

	return 1;
}

int
lka_resolve_path(struct smtpd *env, struct path *path)
{
	char username[MAXLOGNAME];
	struct passwd *pw;
	char *p;

	(void)strlcpy(username, path->user, sizeof(username));

	for (p = &username[0]; *p != '\0' && *p != '+'; ++p)
		*p = tolower((int)*p);
	*p = '\0';

	if (aliases_virtual_exist(env, path))
		path->flags |= F_VIRTUAL;
	else if (aliases_exist(env, username))
		path->flags |= F_ALIAS;
	else {
		path->flags |= F_ACCOUNT;
		pw = safe_getpwnam(username);
		if (pw == NULL)
			return 0;
		(void)strlcpy(path->pw_name, pw->pw_name,
		    sizeof(path->pw_name));
		if (lka_expand(path->rule.r_value.path,
		    sizeof(path->rule.r_value.path), path) >=
		    sizeof(path->rule.r_value.path))
			return 0;
	}

	return 1;
}

void
lka_rcpt_action(struct smtpd *env, struct path *path)
{
	struct rule *r;
	struct cond *cond;
	struct map *map;
	struct mapel *me;

	if (path->domain[0] == '\0')
		(void)strlcpy(path->domain, "localhost", sizeof (path->domain));

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {

		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {
			if (cond->c_type == C_ALL) {
				path->rule = *r;
				return;
			}

			if (cond->c_type == C_DOM) {
				cond->c_match = map_find(env, cond->c_map);
				if (cond->c_match == NULL)
					fatal("mfa failed to lookup map.");

				map = cond->c_match;
				TAILQ_FOREACH(me, &map->m_contents, me_entry) {
					log_debug("trying to match [%s] with [%s]",
					    path->domain, me->me_key.med_string);
					if (hostname_match(path->domain, me->me_key.med_string)) {
						path->rule = *r;
						return;
					}
				}
			}
		}
	}
	path->rule.r_action = A_RELAY;
	return;
}

int
lkasession_cmp(struct lkasession *s1, struct lkasession *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return (-1);

	if (s1->id > s2->id)
		return (1);

	return (0);
}

void
lka_clear_aliaseslist(struct aliaseslist *aliaseslist)
{
	struct alias *alias;

	while ((alias = TAILQ_FIRST(aliaseslist)) != NULL) {
		TAILQ_REMOVE(aliaseslist, alias, entry);
		free(alias);
	}
}

int
lka_encode_credentials(char *dest, char *src)
{
	size_t len;
	char buffer[MAX_LINE_SIZE];
	size_t i;

	len = strlen(src) + 1;
	if (len < 1)
		return 0;

	bzero(buffer, sizeof (buffer));
	if (strlcpy(buffer + 1, src, sizeof(buffer) - 1) >=
	    sizeof (buffer) - 1)
		return 0;

	for (i = 0; i < len; ++i) {
		if (buffer[i] == ':') {
			buffer[i] = '\0';
			break;
		}
	}
	if (kn_encode_base64(buffer, len, dest, MAX_LINE_SIZE - 1) == -1)
		return 0;
	return 1;
}

SPLAY_GENERATE(lkatree, lkasession, nodes, lkasession_cmp);
