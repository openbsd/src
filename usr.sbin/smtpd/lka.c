/*	$OpenBSD: lka.c,v 1.2 2008/11/05 12:14:45 sobrado Exp $	*/

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
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"

__dead void	lka_shutdown(void);
void		lka_sig_handler(int, short, void *);
void		lka_dispatch_parent(int, short, void *);
void		lka_dispatch_mfa(int, short, void *);
void		lka_dispatch_smtp(int, short, void *);
void		lka_dispatch_queue(int, short, void *);
void		lka_setup_events(struct smtpd *);
void		lka_disable_events(struct smtpd *);
int		lka_verify_mail(struct smtpd *, struct path *);
int		lka_verify_rcpt(struct smtpd *, struct path *, struct sockaddr_storage *);
int		lka_resolve_mail(struct smtpd *, struct rule *, struct path *);
int		lka_resolve_rcpt(struct smtpd *, struct rule *, struct path *);
int		lka_forward_file(struct passwd *);
size_t		getmxbyname(char *, char ***);
int		lka_expand(char *, size_t, struct path *);
int		aliases_exist(struct smtpd *, char *);
int		aliases_get(struct smtpd *, struct aliaseslist *, char *);
int		lka_resolve_alias(struct smtpd *, struct imsgbuf *, struct message *, struct alias *);
int		lka_parse_include(char *);
int		forwards_get(struct aliaseslist *, char *);
int		lka_check_source(struct smtpd *, struct map *, struct sockaddr_storage *);
int		lka_match_mask(struct sockaddr_storage *, struct netaddr *);
int		aliases_virtual_get(struct smtpd *, struct aliaseslist *, struct path *);
int		aliases_virtual_exist(struct smtpd *, struct path *);

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
		case IMSG_LKA_LOOKUP_MAIL: {
			struct submit_status	 *ss;

			ss = imsg.data;
			ss->code = 530;

			if (ss->u.path.user[0] == '\0' && ss->u.path.domain[0] == '\0')
				ss->code = 250;
			else
				if (lka_verify_mail(env, &ss->u.path))
					ss->code = 250;

			imsg_compose(ibuf, IMSG_MFA_LOOKUP_MAIL, 0, 0, -1,
				ss, sizeof(*ss));

			break;
		}
		case IMSG_LKA_LOOKUP_RCPT: {
			struct submit_status	 *ss;

			ss = imsg.data;
			ss->code = 530;

			if (lka_verify_rcpt(env, &ss->u.path, &ss->ss))
				ss->code = 250;

			imsg_compose(ibuf, IMSG_MFA_LOOKUP_RCPT, 0, 0, -1,
			    ss, sizeof(*ss));

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
		case IMSG_LKA_HOSTNAME_LOOKUP: {

			struct sockaddr *sa = NULL;
			socklen_t salen;
			char addr[NI_MAXHOST];
			struct addrinfo hints, *res;
			int error;
			struct session *s = imsg.data;

			if (s->s_ss.ss_family == PF_INET) {
				struct sockaddr_in *ssin = (struct sockaddr_in *)&s->s_ss;
				sa = (struct sockaddr *)ssin;
			}
			if (s->s_ss.ss_family == PF_INET6) {
				struct sockaddr_in6 *ssin6 = (struct sockaddr_in6 *)&s->s_ss;
				sa = (struct sockaddr *)ssin6;
			}

			error = getnameinfo(sa, sa->sa_len, addr, sizeof(addr),
			    NULL, 0, NI_NAMEREQD);
			if (error == 0) {
				memset(&hints, 0, sizeof(hints));
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_flags = AI_NUMERICHOST;
				if (getaddrinfo(addr, "0", &hints, &res) == 0) {
					freeaddrinfo(res);
					strlcpy(s->s_hostname, "<bogus>", MAXHOSTNAMELEN);
					imsg_compose(ibuf, IMSG_SMTP_HOSTNAME_ANSWER, 0, 0, -1,
					    s, sizeof(struct session));
					break;
				}
			} else {
				error = getnameinfo(sa, salen, addr, sizeof(addr),
				    NULL, 0, NI_NUMERICHOST);
			}
			strlcpy(s->s_hostname, addr, MAXHOSTNAMELEN);
			imsg_compose(ibuf, IMSG_SMTP_HOSTNAME_ANSWER, 0, 0, -1,
			    s, sizeof(struct session));
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

		case IMSG_LKA_ALIAS_LOOKUP: {
			struct message *messagep;
			struct alias *alias;
			struct alias *remalias;
			struct path *path;
			struct aliaseslist aliases;
			u_int8_t done = 0;
			size_t nbiterations = 5;
			int ret;

			messagep = imsg.data;
			path = &messagep->recipient;

			if (path->flags & F_EXPANDED)
				break;

			TAILQ_INIT(&aliases);

			if (path->flags & F_ALIAS) {
				ret = aliases_get(env, &aliases, path->user);
			}

			if (path->flags & F_VIRTUAL) {
				ret = aliases_virtual_get(env, &aliases, path);
			}

			if (! ret) {
				/*
				 * Aliases could not be retrieved, this happens
				 * if the aliases database is regenerated while
				 * the message is being processed. It is not an
				 * error necessarily so just ignore this and it
				 * will be handled by the queue process.
				 */
				imsg_compose(ibuf, IMSG_QUEUE_REMOVE_SUBMISSION, 0, 0, -1, messagep,
				    sizeof(struct message));
				break;
			}

			/* First pass, figure out if some of the usernames that
			 * are in the list are actually aliases and expand them
			 * if they are. The resolution will be tried five times
			 * at most with an early exit if list did not change in
			 * a pass.
			 */
			while (!done && nbiterations--) {
				done = 1;
				remalias = NULL;
				TAILQ_FOREACH(alias, &aliases, entry) {
					if (remalias) {
						TAILQ_REMOVE(&aliases, remalias, entry);
						free(remalias);
						remalias = NULL;
					}

					if (alias->type == ALIAS_ADDRESS) {
						if (aliases_virtual_get(env, &aliases, &alias->u.path)) {
							done = 0;
							remalias = alias;
						}
					}

					else if (alias->type == ALIAS_USERNAME) {
						if (aliases_get(env, &aliases, alias->u.username)) {
							done = 0;
							remalias = alias;
						}
					}
				}
				if (remalias) {
					TAILQ_REMOVE(&aliases, remalias, entry);
					free(remalias);
					remalias = NULL;
				}
			}

			/* Second pass, the list no longer contains aliases and
			 * the message can be sent back to queue process with a
			 * modified path.
			 */
			TAILQ_FOREACH(alias, &aliases, entry) {
				struct message message = *messagep;
				lka_resolve_alias(env, ibuf, &message, alias);
				imsg_compose(ibuf, IMSG_LKA_ALIAS_RESULT, 0, 0, -1,
				    &message, sizeof(struct message));
			}

			imsg_compose(ibuf, IMSG_QUEUE_REMOVE_SUBMISSION, 0, 0, -1,
			    messagep, sizeof(struct message));

			while ((alias = TAILQ_FIRST(&aliases))) {
				TAILQ_REMOVE(&aliases, alias, entry);
				free(alias);
			}
			break;
		}

		case IMSG_LKA_FORWARD_LOOKUP: {
			struct message *messagep;
			struct aliaseslist aliases;
			struct alias *alias;

			messagep = imsg.data;

			/* this is the tenth time the message has been forwarded
			 * internally, break out of the loop.
			 */
			if (messagep->recipient.forwardcnt == 10) {
				imsg_compose(ibuf, IMSG_QUEUE_REMOVE_SUBMISSION, 0, 0, -1, messagep,
				    sizeof(struct message));
				break;
			}
			messagep->recipient.forwardcnt++;

			TAILQ_INIT(&aliases);
			if (! forwards_get(&aliases, messagep->recipient.pw_name)) {
				messagep->recipient.flags |= F_NOFORWARD;
				imsg_compose(ibuf, IMSG_LKA_FORWARD_LOOKUP, 0, 0, -1, messagep, sizeof(struct message));
				imsg_compose(ibuf, IMSG_QUEUE_REMOVE_SUBMISSION, 0, 0, -1, messagep,
				    sizeof(struct message));
				break;
			}

			TAILQ_FOREACH(alias, &aliases, entry) {
				struct message message = *messagep;
				lka_resolve_alias(env, ibuf, &message, alias);
				if (strcmp(messagep->recipient.pw_name, alias->u.username) == 0) {

					message.recipient.flags |= F_FORWARDED;
				}
				imsg_compose(ibuf, IMSG_LKA_FORWARD_LOOKUP, 0, 0, -1, &message, sizeof(struct message));
			}

			imsg_compose(ibuf, IMSG_QUEUE_REMOVE_SUBMISSION, 0, 0, -1, messagep, sizeof(struct message));

			while ((alias = TAILQ_FIRST(&aliases))) {
				TAILQ_REMOVE(&aliases, alias, entry);
				free(alias);
			}

			break;
		}

		case IMSG_LKA_MX_LOOKUP: {
			struct batch *batchp;
			struct addrinfo hints, *res, *resp;
			char **mx = NULL;
			char *lmx[1];
			size_t len, i, j;
			int error;
			u_int16_t port = 25;

			batchp = imsg.data;

			if (! IS_RELAY(batchp->rule.r_action))
				err(1, "lka_dispatch_queue: inconsistent internal state");

			if (batchp->rule.r_action == A_RELAY) {
				log_debug("attempting to resolve %s", batchp->hostname);
				len = getmxbyname(batchp->hostname, &mx);
				if (len == 0) {
					lmx[0] = batchp->hostname;
					mx = lmx;
					len = 1;
				}
			}
			else if (batchp->rule.r_action == A_RELAYVIA) {
				lmx[0] = batchp->rule.r_value.host.hostname;
				port = batchp->rule.r_value.host.port;
				log_debug("attempting to resolve %s:%d (forced)", lmx[0], port);
				mx = lmx;
				len = 1;
			}

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = PF_UNSPEC;
			hints.ai_protocol = IPPROTO_TCP;
			for (i = j = 0; i < len; ++i) {
				error = getaddrinfo(mx[i], NULL, &hints, &res);
				if (error)
					continue;

				log_debug("resolving MX: %s", mx[i]);

				for (resp = res; resp != NULL; resp = resp->ai_next) {
					if (resp->ai_family == PF_INET) {
						struct sockaddr_in *ssin;

						batchp->ss[j] = *(struct sockaddr_storage *)resp->ai_addr;
						ssin = (struct sockaddr_in *)&batchp->ss[j];
						ssin->sin_port = htons(port);
						++j;
					}
					if (resp->ai_family == PF_INET6) {
						struct sockaddr_in6 *ssin6;
						batchp->ss[j] = *(struct sockaddr_storage *)resp->ai_addr;
						ssin6 = (struct sockaddr_in6 *)&batchp->ss[j];
						ssin6->sin6_port = htons(port);
						++j;
					}
				}

				freeaddrinfo(res);
			}

			batchp->ss_cnt = j;
			batchp->h_errno = 0;
			if (j == 0)
				batchp->h_errno = error;
			imsg_compose(ibuf, IMSG_LKA_MX_LOOKUP, 0, 0, -1, batchp, sizeof(*batchp));

			if (mx != lmx)
				free(mx);

			break;
		}
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

	signal_set(&ev_sigint, SIGINT, lka_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, lka_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 4);

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
					if (strcasecmp(me->me_key.med_string,
						path->domain) == 0) {
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
lka_verify_rcpt(struct smtpd *env, struct path *path, struct sockaddr_storage *ss)
{
	struct rule *r;
	struct cond *cond;
	struct map *map;
	struct mapel *me;

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {

		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {

			if (cond->c_type == C_ALL) {
				path->rule = *r;

				if (! lka_check_source(env, r->r_sources, ss))
					return 0;

				if (r->r_action == A_MBOX ||
				    r->r_action == A_MAILDIR) {
					return lka_resolve_rcpt(env, r, path);
				}
				return 1;
			}

			if (cond->c_type == C_DOM) {

				cond->c_match = map_find(env, cond->c_map);
				if (cond->c_match == NULL)
					fatal("lka failed to lookup map.");

				map = cond->c_match;
				TAILQ_FOREACH(me, &map->m_contents, me_entry) {
					if (strcasecmp(me->me_key.med_string,
						path->domain) == 0) {
						path->rule = *r;
						if (! lka_check_source(env, r->r_sources, ss))
							return 0;

						if (IS_MAILBOX(r->r_action) ||
						    IS_EXT(r->r_action)) {
							return lka_resolve_rcpt(env, r, path);
						}
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

int
lka_resolve_mail(struct smtpd *env, struct rule *rule, struct path *path)
{
	char username[MAXLOGNAME];
	struct passwd *pw;
	char *p;

	(void)strlcpy(username, path->user, MAXLOGNAME);

	for (p = &username[0]; *p != '\0' && *p != '+'; ++p)
		*p = tolower((int)*p);
	*p = '\0';

	if (aliases_virtual_exist(env, path))
		path->flags |= F_VIRTUAL;
	else if (aliases_exist(env, username))
		path->flags |= F_ALIAS;
	else {
		pw = getpwnam(username);
		if (pw == NULL)
			return 0;
		(void)strlcpy(path->pw_name, pw->pw_name, MAXLOGNAME);
		if (lka_expand(path->rule.r_value.path, MAXPATHLEN, path) >=
		    MAXPATHLEN)
			return 0;
	}

	return 1;
}

int
lka_resolve_rcpt(struct smtpd *env, struct rule *rule, struct path *path)
{
	char username[MAXLOGNAME];
	struct passwd *pw;
	char *p;

	(void)strlcpy(username, path->user, MAXLOGNAME);

	for (p = &username[0]; *p != '\0' && *p != '+'; ++p)
		*p = tolower((int)*p);
	*p = '\0';

	if ((path->flags & F_EXPANDED) == 0 && aliases_virtual_exist(env, path))
		path->flags |= F_VIRTUAL;
	else if ((path->flags & F_EXPANDED) == 0 && aliases_exist(env, username))
		path->flags |= F_ALIAS;
	else {
		pw = getpwnam(path->pw_name);
		if (pw == NULL)
			pw = getpwnam(username);
		if (pw == NULL)
			return 0;
		(void)strlcpy(path->pw_name, pw->pw_name, MAXLOGNAME);
		if (lka_expand(path->rule.r_value.path, MAXPATHLEN, path) >=
		    MAXPATHLEN) {
			return 0;
		}
	}

	return 1;
}

int
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
				pw = getpwnam(path->pw_name);
				if (pw == NULL)
					continue;

				ret += strlcat(pbuf, pw->pw_dir, len);
				if (ret >= len)
					return ret;
				pbuf += strlen(pw->pw_dir);
				++p;
				continue;
			}

			if (*(p + 1) != '/') {
				char username[MAXLOGNAME];
				char *delim;

				ret = strlcpy(username, p + 1, MAXLOGNAME);
				delim = strchr(username, '/');
				if (delim == NULL && ret >= MAXLOGNAME) {
					continue;
				}

				if (delim != NULL) {
					*delim = '\0';
				}

				pw = getpwnam(username);
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
lka_resolve_alias(struct smtpd *env, struct imsgbuf *ibuf, struct message *messagep, struct alias *alias)
{
	struct path *rpath = &messagep->recipient;

	rpath->flags &= ~F_ALIAS;
	rpath->flags |= F_EXPANDED;

	switch (alias->type) {
	case ALIAS_USERNAME:
		if (strlcpy(rpath->pw_name, alias->u.username,
			sizeof(rpath->pw_name)) >= sizeof(rpath->pw_name))
			return 0;
		lka_verify_rcpt(env, rpath, NULL);
		break;

	case ALIAS_FILENAME:
		rpath->rule.r_action = A_FILENAME;
		strlcpy(rpath->u.filename, alias->u.filename, MAXPATHLEN);
		break;

	case ALIAS_FILTER:
		rpath->rule.r_action = A_EXT;
		strlcpy(rpath->rule.r_value.command, alias->u.filter + 2, MAXPATHLEN);
		rpath->rule.r_value.command[strlen(rpath->rule.r_value.command) - 1] = '\0';
		break;

	case ALIAS_ADDRESS:
		*rpath = alias->u.path;
		lka_verify_rcpt(env, rpath, NULL);
		if (IS_MAILBOX(rpath->rule.r_action) ||
		    IS_EXT(rpath->rule.r_action))
			messagep->type = T_MDA_MESSAGE;
		else
			messagep->type = T_MTA_MESSAGE;

		break;
	default:
		/* ALIAS_INCLUDE cannot happen here, make gcc shut up */
		break;
	}
	return 1;
}

int
lka_check_source(struct smtpd *env, struct map *map, struct sockaddr_storage *ss)
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

		if (lka_match_mask(ss, &me->me_key.med_addr))
			return 1;

	}
	return 0;
}

int
lka_match_mask(struct sockaddr_storage *ss, struct netaddr *ssmask)
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
		for (i = 0; i < (128 - ssmask->masked) / 8; i++)
			mask.s6_addr[i] = 0xff;
		i = ssmask->masked % 8;
		if (i)
			mask.s6_addr[ssmask->masked / 8] = 0xff00 >> i;

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
