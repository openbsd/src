/*	$OpenBSD: lka.c,v 1.99 2010/02/17 08:40:24 gilles Exp $	*/

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
#include <sys/wait.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <pwd.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
void		lka_expand_pickup(struct smtpd *, struct lkasession *);
int		lka_expand_resume(struct smtpd *, struct lkasession *);
int		lka_resolve_node(struct smtpd *, char *tag, struct path *, struct expand_node *);
int		lka_verify_mail(struct smtpd *, struct path *);
struct rule    *ruleset_match(struct smtpd *, char *, struct path *, struct sockaddr_storage *);
int		lka_resolve_path(struct smtpd *, struct lkasession *, struct path *);
struct lkasession *lka_session_init(struct smtpd *, struct submit_status *);
void		lka_request_forwardfile(struct smtpd *, struct lkasession *, char *);
void		lka_clear_expandtree(struct expandtree *);
void		lka_clear_deliverylist(struct deliverylist *);
int		lka_encode_credentials(char *, size_t, char *);
size_t		lka_expand(char *, size_t, struct path *);
void		lka_rcpt_action(struct smtpd *, char *, struct path *);
void		lka_session_destroy(struct smtpd *, struct lkasession *);
void		lka_expansion_done(struct smtpd *, struct lkasession *);
void		lka_session_fail(struct smtpd *, struct lkasession *, struct submit_status *);

void
lka_sig_handler(int sig, short event, void *p)
{
	int status;
	pid_t pid;

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		lka_shutdown();
		break;
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
		} while (pid > 0 || (pid == -1 && errno == EINTR));
		break;
	default:
		fatalx("lka_sig_handler: unexpected signal");
	}
}

void
lka_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_PARENT];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_parent: imsg_get error");
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
			/* switch and destroy old ruleset */
			if (env->sc_rules)
				purge_config(env, PURGE_RULES);
			if (env->sc_maps)
				purge_config(env, PURGE_MAPS);
			env->sc_rules = env->sc_rules_reload;
			env->sc_maps = env->sc_maps_reload;
			break;
		}
		case IMSG_PARENT_FORWARD_OPEN: {
			int fd;
			struct forward_req	*fwreq = imsg.data;
			struct lkasession	key;
			struct lkasession	*lkasession;
			struct path *path;

			IMSG_SIZE_CHECK(fwreq);

			key.id = fwreq->id;
			lkasession = SPLAY_FIND(lkatree, &env->lka_sessions, &key);
			if (lkasession == NULL)
				fatal("lka_dispatch_parent: lka session is gone");
			fd = imsg.fd;
			--lkasession->pending;

			strlcpy(lkasession->path.pw_name, fwreq->pw_name,
			    sizeof(lkasession->path.pw_name));
			lkasession->path.flags |= F_PATH_FORWARDED;

			/* received a descriptor, we have a forward file ... */
			if (fd != -1) {
				if (! forwards_get(fd, &lkasession->expandtree)) {
					lkasession->ss.code = 530;
					lkasession->flags |= F_ERROR;					
				}
				close(fd);
				lkasession->path.flags |= F_PATH_FORWARDED;
				lka_expand_pickup(env, lkasession);
				break;
			}

			/* did not receive a descriptor but expected one ... */
			if (! fwreq->status) {
				lkasession->ss.code = 530;
				lkasession->flags |= F_ERROR;
				lka_expand_pickup(env, lkasession);
				break;
			}

			/* no forward file, convert pw_name to a struct path ... */
			path = path_dup(&lkasession->path);
			strlcpy(path->pw_name, fwreq->pw_name, sizeof(path->pw_name));
			TAILQ_INSERT_TAIL(&lkasession->deliverylist, path, entry);			
			lka_expand_pickup(env, lkasession);
			break;
		}
		case IMSG_CTL_VERBOSE: {
			int verbose;

			IMSG_SIZE_CHECK(&verbose);

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		}
		default:
			log_warnx("lka_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("lka_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lka_dispatch_mfa(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MFA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_mfa: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_MAIL: {
			struct submit_status	 *ss = imsg.data;

			IMSG_SIZE_CHECK(ss);

			ss->code = 530;

			if (ss->u.path.user[0] == '\0' && ss->u.path.domain[0] == '\0')
				ss->code = 250;
			else
				if (lka_verify_mail(env, &ss->u.path))
					ss->code = 250;

			imsg_compose_event(iev, IMSG_LKA_MAIL, 0, 0, -1,
				ss, sizeof(*ss));

			break;
		}
		case IMSG_LKA_RULEMATCH: {
			struct submit_status	*ss = imsg.data;
			struct rule *r;

			IMSG_SIZE_CHECK(ss);

			ss->code = 530;

			r = ruleset_match(env, ss->msg.tag, &ss->u.path, &ss->ss);
			if (r != NULL) {
				ss->code = 250;
				ss->u.path.rule = *r;
			}

			imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RULEMATCH, 0, 0, -1,
			    ss, sizeof(*ss));

			break;
		}
		case IMSG_LKA_RCPT: {
			struct submit_status	*ss = imsg.data;
			struct lkasession	*lkasession;
			struct path		*path;

			IMSG_SIZE_CHECK(ss);

			ss->code = 250;
			path = &ss->u.path;

			lkasession = lka_session_init(env, ss);

			if (! lka_resolve_path(env, lkasession, path))
				lka_session_fail(env, lkasession, ss);
			else
				lka_expand_pickup(env, lkasession);

			break;
		}
		default:
			log_warnx("lka_dispatch_mfa: got imsg %d",
			    imsg.hdr.type);
			fatalx("lka_dispatch_mfa: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lka_dispatch_mta(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MTA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_mta: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_LKA_SECRET: {
			struct secret	*query = imsg.data;
			char		*secret = NULL;
			char		*map = "secrets";

			IMSG_SIZE_CHECK(query);

			secret = map_dblookupbyname(env, map, query->host);

			log_debug("secret for %s %s", query->host,
			    secret ? "found" : "not found");
			
			query->secret[0] = '\0';

			if (secret == NULL) {
				log_warnx("failed to lookup %s in the %s map",
				    query->host, map);
			} else if (! lka_encode_credentials(query->secret,
			    sizeof(query->secret), secret)) {
				log_warnx("parse error for %s in the %s map",
				    query->host, map);
			}

			imsg_compose_event(iev, IMSG_LKA_SECRET, 0, 0, -1, query,
			    sizeof(*query));
			free(secret);
			break;
		}

		case IMSG_DNS_MX:
		case IMSG_DNS_PTR: {
			struct dns	*query = imsg.data;

			IMSG_SIZE_CHECK(query);
			dns_async(env, iev, imsg.hdr.type, query);
			break;
		}

		default:
			log_warnx("lka_dispatch_mta: got imsg %d",
			    imsg.hdr.type);
			fatalx("lka_dispatch_mta: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lka_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_SMTP];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DNS_PTR: {
			struct dns	*query = imsg.data;

			IMSG_SIZE_CHECK(query);
			dns_async(env, iev, IMSG_DNS_PTR, query);
			break;
		}
		default:
			log_warnx("lka_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("lka_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lka_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_QUEUE];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("lka_dispatch_queue: got imsg %d",
			   imsg.hdr.type);
			fatalx("lka_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
lka_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_RUNNER];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("lka_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("lka_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("lka_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
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
	struct event	 ev_sigchld;

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

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	smtpd_process = PROC_LKA;
	setproctitle("%s", env->sc_title[smtpd_process]);

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
	signal_set(&ev_sigchld, SIGCHLD, lka_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/*
	 * lka opens all kinds of files and sockets, so bump the limit to max.
	 * XXX: need to analyse the exact hard limit.
	 */
	fdlimit(1.0);

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	lka_setup_events(env);
	event_dispatch();
	lka_shutdown();

	return (0);
}

int
lka_verify_mail(struct smtpd *env, struct path *path)
{
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
				pw = getpwnam(path->pw_name);
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
lka_resolve_node(struct smtpd *env, char *tag, struct path *path, struct expand_node *expnode)
{
	struct path psave = *path;

	bzero(path, sizeof(struct path));

	switch (expnode->type) {
	case EXPAND_USERNAME:
		log_debug("lka_resolve_node: node is local username: %s",
		    expnode->u.username);
		if (strlcpy(path->pw_name, expnode->u.username,
			sizeof(path->pw_name)) >= sizeof(path->pw_name))
			return 0;

		if (strlcpy(path->user, expnode->u.username,
			sizeof(path->user)) >= sizeof(path->user))
			return 0;

		if (psave.domain[0] == '\0') {
			if (strlcpy(path->domain, env->sc_hostname,
				sizeof(path->domain)) >= sizeof(path->domain))
				return 0;
		}
		else {
			strlcpy(path->domain, psave.domain,
			    sizeof(psave.domain));
		}

		log_debug("lka_resolve_node: resolved to address: %s@%s",
		    path->user, path->domain);
		lka_rcpt_action(env, tag, path);
		break;

	case EXPAND_FILENAME:
		log_debug("lka_resolve_node: node is filename: %s",
		    expnode->u.filename);
		path->rule.r_action = A_FILENAME;
		strlcpy(path->u.filename, expnode->u.filename,
		    sizeof(path->u.filename));
		break;

	case EXPAND_FILTER:
		log_debug("lka_resolve_node: node is filter: %s",
		    expnode->u.filter);
		path->rule.r_action = A_EXT;
		strlcpy(path->rule.r_value.command, expnode->u.filter + 2,
		    sizeof(path->rule.r_value.command));
		path->rule.r_value.command[strlen(path->rule.r_value.command) - 1] = '\0';
		break;

	case EXPAND_ADDRESS:
		log_debug("lka_resolve_node: node is address: %s@%s",
		    expnode->u.mailaddr.user, expnode->u.mailaddr.domain);

		if (strlcpy(path->user, expnode->u.mailaddr.user,
			sizeof(path->user)) >= sizeof(path->user))
			return 0;

		if (strlcpy(path->domain, expnode->u.mailaddr.domain,
			sizeof(path->domain)) >= sizeof(path->domain))
			return 0;

		lka_rcpt_action(env, tag, path);
		break;
	case EXPAND_INVALID:
	case EXPAND_INCLUDE:
		fatalx("lka_resolve_node: unexpected type");
		break;
	}

	return 1;
}

void
lka_expand_pickup(struct smtpd *env, struct lkasession *lkasession)
{
	int	ret;

	/* we want to do five iterations of lka_expand_resume() but
	 * we need to be interruptible in case lka_expand_resume()
	 * has sent an imsg and expects an answer.
	 */
	ret = 0;
	while (! (lkasession->flags & F_ERROR) &&
	    ! lkasession->pending && lkasession->iterations < 5) {
		++lkasession->iterations;
		ret = lka_expand_resume(env, lkasession);
		if (ret == -1) {
			lkasession->ss.code = 530;
			lkasession->flags |= F_ERROR;
		}

		if (lkasession->pending || ret <= 0)
			break;
	}

	if (lkasession->pending)
		return;

	lka_expansion_done(env, lkasession);
}

int
lka_expand_resume(struct smtpd *env, struct lkasession *lkasession)
{
	u_int8_t done = 1;
	struct expand_node *expnode = NULL;
	struct path *lkasessionpath = NULL;
	struct path path, *pathp = NULL;

	lkasessionpath = &lkasession->path;
	RB_FOREACH(expnode, expandtree, &lkasession->expandtree) {

		/* this node has already been expanded, skip*/
		if (expnode->flags & F_EXPAND_DONE)
			continue;
		done = 0;

		/* convert node to path, then inherit flags from lkasession */
		if (! lka_resolve_node(env, lkasession->message.tag, &path, expnode))
			return -1;
		path.flags = lkasessionpath->flags;

		/* resolve path, eventually populating expandtree.
		 * we need to dup because path may be added to the deliverylist.
		 */
		pathp = path_dup(&path);
		if (! lka_resolve_path(env, lkasession, pathp))
			return -1;

		/* decrement refcount on this node and flag it as processed */
		expandtree_decrement_node(&lkasession->expandtree, expnode);
		expnode->flags |= F_EXPAND_DONE;
	}

	/* still not done after 5 iterations ? loop detected ... reject */
	if (!done && lkasession->iterations == 5) {
		return -1;
	}

	/* we're done expanding, no need for another iteration */
	if (RB_ROOT(&lkasession->expandtree) == NULL || done)
		return 0;

	return 1;
}

void
lka_expansion_done(struct smtpd *env, struct lkasession *lkasession)
{
	struct message message;
	struct path *path;

	/* delivery list is empty OR expansion led to an error, reject */
	if (TAILQ_FIRST(&lkasession->deliverylist) == NULL ||
	    lkasession->flags & F_ERROR) {
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0,
		    -1, &lkasession->ss, sizeof(struct submit_status));
		goto done;
	}

	/* process the delivery list and submit envelopes to queue */
	message = lkasession->message;
	while ((path = TAILQ_FIRST(&lkasession->deliverylist)) != NULL) {
		lka_expand(path->rule.r_value.path,
			sizeof(path->rule.r_value.path), path);
		message.recipient = *path;
		queue_submit_envelope(env, &message);
		
		TAILQ_REMOVE(&lkasession->deliverylist, path, entry);
		free(path);
	}
	queue_commit_envelopes(env, &message);

done:
	lka_clear_expandtree(&lkasession->expandtree);
	lka_clear_deliverylist(&lkasession->deliverylist);
	lka_session_destroy(env, lkasession);
}

int
lka_resolve_path(struct smtpd *env, struct lkasession *lkasession, struct path *path)
{
	if (IS_RELAY(*path) || path->cond == NULL) {
		path = path_dup(path);
		path->flags |= F_PATH_RELAY;
		TAILQ_INSERT_TAIL(&lkasession->deliverylist, path, entry);
		return 1;
	}

	switch (path->cond->c_type) {
	case C_ALL:
	case C_NET:
	case C_DOM: {
		char username[MAX_LOCALPART_SIZE];
		char *sep;
		struct passwd *pw;

		lowercase(username, path->user, sizeof(username));

		sep = strchr(username, '+');
		if (sep != NULL)
			*sep = '\0';

		if (aliases_exist(env, path->rule.r_amap, username)) {
			path->flags |= F_PATH_ALIAS;
			if (! aliases_get(env, path->rule.r_amap,
				&lkasession->expandtree, path->user))
				return 0;
			return 1;
		}

		if (strlen(username) >= MAX_LOCALPART_SIZE)
			return 0;

		path->flags |= F_PATH_ACCOUNT;
		pw = getpwnam(username);
		if (pw == NULL)
			return 0;

		(void)strlcpy(path->pw_name, pw->pw_name,
		    sizeof(path->pw_name));

		if (path->flags & F_PATH_FORWARDED)
			TAILQ_INSERT_TAIL(&lkasession->deliverylist, path, entry);
		else
			lka_request_forwardfile(env, lkasession, path->pw_name);

		return 1;
	}
	case C_VDOM: {
		if (aliases_virtual_exist(env, path->cond->c_map, path)) {
			path->flags |= F_PATH_VIRTUAL;
			if (! aliases_virtual_get(env, path->cond->c_map,
				&lkasession->expandtree, path))
				return 0;
			return 1;
		}
		break;
	}
	default:
		fatalx("lka_resolve_path: unexpected type");
	}

	return 0;
}

void
lka_rcpt_action(struct smtpd *env, char *tag, struct path *path)
{
	struct rule *r;

	if (path->domain[0] == '\0')
		(void)strlcpy(path->domain, env->sc_hostname,
		    sizeof (path->domain));

	r = ruleset_match(env, tag, path, NULL);
	if (r == NULL) {
		path->rule.r_action = A_RELAY;
		return;
	}

	path->rule = *r;
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
lka_clear_expandtree(struct expandtree *expandtree)
{
	struct expand_node *expnode;

	while ((expnode = RB_ROOT(expandtree)) != NULL) {
		expandtree_remove_node(expandtree, expnode);
		free(expnode);
	}
}

void
lka_clear_deliverylist(struct deliverylist *deliverylist)
{
	struct path *path;

	while ((path = TAILQ_FIRST(deliverylist)) != NULL) {
		TAILQ_REMOVE(deliverylist, path, entry);
		free(path);
	}
}

int
lka_encode_credentials(char *dst, size_t size, char *user)
{
	char	*pass, *buf;
	int	 buflen;

	if ((pass = strchr(user, ':')) == NULL)
		return 0;
	*pass++ = '\0';

	if ((buflen = asprintf(&buf, "%c%s%c%s", '\0', user, '\0', pass)) == -1)
		fatal(NULL);

	if (__b64_ntop((unsigned char *)buf, buflen, dst, size) == -1) {
		free(buf);
		return 0;
	}

	free(buf);
	return 1;
}

struct lkasession *
lka_session_init(struct smtpd *env, struct submit_status *ss)
{
	struct lkasession *lkasession;

	lkasession = calloc(1, sizeof(struct lkasession));
	if (lkasession == NULL)
		fatal("lka_session_init: calloc");

	lkasession->id = generate_uid();
	lkasession->path = ss->u.path;
	lkasession->message = ss->msg;
	lkasession->ss = *ss;
	
	RB_INIT(&lkasession->expandtree);
	TAILQ_INIT(&lkasession->deliverylist);
	SPLAY_INSERT(lkatree, &env->lka_sessions, lkasession);

	return lkasession;
}

void
lka_session_fail(struct smtpd *env, struct lkasession *lkasession, struct submit_status *ss)
{
	ss->code = 530;
	imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT, 0, 0, -1,
	    ss, sizeof(*ss));
	lka_session_destroy(env, lkasession);
}

void
lka_session_destroy(struct smtpd *env, struct lkasession *lkasession)
{
	SPLAY_REMOVE(lkatree, &env->lka_sessions, lkasession);
	free(lkasession);
}

void
lka_request_forwardfile(struct smtpd *env, struct lkasession *lkasession, char *username)
{
	struct forward_req	 fwreq;

	fwreq.id = lkasession->id;
	(void)strlcpy(fwreq.pw_name, username, sizeof(fwreq.pw_name));
	imsg_compose_event(env->sc_ievs[PROC_PARENT], IMSG_PARENT_FORWARD_OPEN, 0, 0, -1,
	    &fwreq, sizeof(fwreq));
	++lkasession->pending;
}

SPLAY_GENERATE(lkatree, lkasession, nodes, lkasession_cmp);
