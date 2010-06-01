/*	$OpenBSD: lka.c,v 1.110 2010/06/01 02:08:56 jacekm Exp $	*/

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

void		lka_imsg(struct smtpd *, struct imsgev *, struct imsg *);
__dead void	lka_shutdown(void);
void		lka_sig_handler(int, short, void *);
void		lka_setup_events(struct smtpd *);
void		lka_disable_events(struct smtpd *);
void		lka_expand_pickup(struct smtpd *, struct lkasession *);
int		lka_expand_resume(struct smtpd *, struct lkasession *);
int		lka_resolve_node(struct smtpd *, char *tag, struct path *, struct expandnode *);
int		lka_verify_mail(struct smtpd *, struct path *);
struct rule    *ruleset_match(struct smtpd *, char *, struct path *, struct sockaddr_storage *);
int		lka_resolve_path(struct smtpd *, struct lkasession *, struct path *);
struct lkasession *lka_session_init(struct smtpd *, struct message *);
void		lka_request_forwardfile(struct smtpd *, struct lkasession *, struct path *);
void		lka_clear_expandtree(struct expandtree *);
void		lka_clear_deliverylist(struct deliverylist *);
char           *lka_encode_secret(struct map_secret *);
size_t		lka_expand(char *, size_t, struct path *);
void		lka_rcpt_action(struct smtpd *, char *, struct path *);
void		lka_session_destroy(struct smtpd *, struct lkasession *);
void		lka_expansion_done(struct smtpd *, struct lkasession *);
void		lka_session_fail(struct smtpd *, struct lkasession *);
void		lka_queue_append(struct smtpd *, struct lkasession *, int);

u_int32_t lka_id;

void
lka_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	struct lkasession	 skey;
	struct forward_req	*fwreq;
	struct lkasession	*s;
	struct message		*m;
	struct mapel		*mapel;
	struct rule		*rule;
	struct path		*path;
	struct map		*map;
	struct map_secret	*map_secret;
	char			*secret;
	void			*tmp;
	int			 status;

	if (imsg->hdr.type == IMSG_DNS_A || imsg->hdr.type == IMSG_DNS_MX ||
	    imsg->hdr.type == IMSG_DNS_PTR) {
		dns_async(env, iev, imsg->hdr.type, imsg->data);
		return;
	}

	if (iev->proc == PROC_MFA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_MAIL:
			m = imsg->data;
			status = 0;
			if (m->sender.user[0] || m->sender.domain[0])
				if (! lka_verify_mail(env, &m->sender))
					status = S_MESSAGE_PERMFAILURE;
			imsg_compose_event(iev, IMSG_LKA_MAIL,
			    m->id, 0, -1, &status, sizeof status);
			return;

		case IMSG_LKA_RCPT:
			m = imsg->data;
			rule = ruleset_match(env, m->tag, &m->recipient, &m->session_ss);
			if (rule == NULL) {
				log_debug("lka: rule not found");
				status = S_MESSAGE_PERMFAILURE;
				imsg_compose_event(iev, IMSG_LKA_RULEMATCH, m->id, 0, -1,
				    &status, sizeof status);
				return;
			}
			m->recipient.rule = *rule;
			s = lka_session_init(env, m);
			if (! lka_resolve_path(env, s, &m->recipient))
				lka_session_fail(env, s);
			else
				lka_expand_pickup(env, s);
			return;
		}
	}

	if (iev->proc == PROC_QUEUE) {
		switch (imsg->hdr.type) {
		case IMSG_QUEUE_APPEND:
			skey.id = imsg->hdr.peerid;
			s = SPLAY_FIND(lkatree, &env->lka_sessions, &skey);
			if (s == NULL)
				fatalx("lka: session missing");
			memcpy(&status, imsg->data, sizeof status);
			lka_queue_append(env, s, status);
			return;
		}
	}

	if (iev->proc == PROC_MTA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET:
			map = map_findbyname(env, "secrets");
			if (map == NULL)
				fatalx("lka: secrets map not found");
			map_secret = map_lookup(env, map->m_id, imsg->data, K_SECRET);
			if (map_secret)
				secret = lka_encode_secret(map_secret);
			else
				secret = "";
			if (*secret == '\0')
				log_warnx("%s secret not found", (char *)imsg->data);
			imsg_compose_event(iev, IMSG_LKA_SECRET,
			    imsg->hdr.peerid, 0, -1, secret,
			    strlen(secret) + 1);
			free(map_secret);
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_START:
			env->sc_rules_reload = calloc(1, sizeof *env->sc_rules);
			if (env->sc_rules_reload == NULL)
				fatal(NULL);
			env->sc_maps_reload = calloc(1, sizeof *env->sc_maps);
			if (env->sc_maps_reload == NULL)
				fatal(NULL);
			TAILQ_INIT(env->sc_rules_reload);
			TAILQ_INIT(env->sc_maps_reload);
			return;

		case IMSG_CONF_RULE:
			rule = calloc(1, sizeof *rule);
			if (rule == NULL)
				fatal(NULL);
			*rule = *(struct rule *)imsg->data;
			TAILQ_INSERT_TAIL(env->sc_rules_reload, rule, r_entry);
			return;

		case IMSG_CONF_MAP:
			map = calloc(1, sizeof *map);
			if (map == NULL)
				fatal(NULL);
			*map = *(struct map *)imsg->data;
			TAILQ_INIT(&map->m_contents);
			TAILQ_INSERT_TAIL(env->sc_maps_reload, map, m_entry);
			return;

		case IMSG_CONF_RULE_SOURCE:
			rule = TAILQ_LAST(env->sc_rules_reload, rulelist);
			tmp = env->sc_maps;
			env->sc_maps = env->sc_maps_reload;
			rule->r_sources = map_findbyname(env, imsg->data);
			if (rule->r_sources == NULL)
				fatalx("lka: maps inconsistency");
			env->sc_maps = tmp;
			return;

		case IMSG_CONF_MAP_CONTENT:
			map = TAILQ_LAST(env->sc_maps_reload, maplist);
			mapel = calloc(1, sizeof *mapel);
			if (mapel == NULL)
				fatal(NULL);
			*mapel = *(struct mapel *)imsg->data;
			TAILQ_INSERT_TAIL(&map->m_contents, mapel, me_entry);
			return;

		case IMSG_CONF_END:
			if (env->sc_rules)
				purge_config(env, PURGE_RULES);
			if (env->sc_maps)
				purge_config(env, PURGE_MAPS);
			env->sc_rules = env->sc_rules_reload;
			env->sc_maps = env->sc_maps_reload;
			return;

		case IMSG_PARENT_FORWARD_OPEN:
			fwreq = imsg->data;
			skey.id = fwreq->id;
			s = SPLAY_FIND(lkatree, &env->lka_sessions, &skey);
			if (s == NULL)
				fatalx("lka: session missing");
			s->pending--;
			strlcpy(s->path.pw_name, fwreq->pw_name,
			    sizeof s->path.pw_name);
			s->path.flags |= F_PATH_FORWARDED;

			if (imsg->fd != -1) {
				/* opened .forward okay */
				if (! forwards_get(imsg->fd, &s->expandtree)) {
					s->ss.code = 530;
					s->flags |= F_ERROR;					
				}
				close(imsg->fd);
				s->path.flags |= F_PATH_FORWARDED;
				lka_expand_pickup(env, s);
			} else {
				if (fwreq->status) {
					/* .forward not present */
					path = path_dup(&s->path);
					strlcpy(path->pw_name, fwreq->pw_name,
					    sizeof path->pw_name);
					TAILQ_INSERT_TAIL(&s->deliverylist, path, entry);
					lka_expand_pickup(env, s);
				} else {
					/* opening .forward failed */
					s->ss.code = 530;
					s->flags |= F_ERROR;
					lka_expand_pickup(env, s);
				}
			}
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	fatalx("lka_imsg: unexpected imsg");
}

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
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_MFA,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_SMTP,	imsg_dispatch },
		{ PROC_MTA,	imsg_dispatch }
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

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("lka: cannot drop privileges");

	imsg_callback = lka_imsg;
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
lka_resolve_node(struct smtpd *env, char *tag, struct path *path, struct expandnode *expnode)
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
lka_expand_pickup(struct smtpd *env, struct lkasession *s)
{
	int	ret;

	if (s->pending)
		return;

	if (s->flags & F_ERROR) {
		lka_expansion_done(env, s);
		return;
	}

	/* we want to do five iterations of lka_expand_resume() but
	 * we need to be interruptible in case lka_expand_resume()
	 * has sent an imsg and expects an answer.
	 */
	while (s->iterations < 5) {
		s->iterations++;
		ret = lka_expand_resume(env, s);
		if (ret == -1)
			s->flags |= F_ERROR;
		if (s->pending)
			return;
		if (ret == 0)
			break;
	}

	lka_expansion_done(env, s);
}

int
lka_expand_resume(struct smtpd *env, struct lkasession *lkasession)
{
	u_int8_t done = 1;
	struct expandnode *expnode = NULL;
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
lka_expansion_done(struct smtpd *env, struct lkasession *s)
{
	int status;

	/* delivery list is empty OR expansion led to an error, reject */
	if (TAILQ_EMPTY(&s->deliverylist) || s->flags & F_ERROR) {
		if (TAILQ_EMPTY(&s->deliverylist))
			log_debug("lka_expansion_done: list empty");
		else
			log_debug("lka_expansion_done: session error");
		status = S_MESSAGE_PERMFAILURE;
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT,
		    s->message.id, 0, -1, &status, sizeof status);
		lka_clear_expandtree(&s->expandtree);
		lka_clear_deliverylist(&s->deliverylist);
		lka_session_destroy(env, s);
	} else
		lka_queue_append(env, s, 0);
}

void
lka_queue_append(struct smtpd *env, struct lkasession *s, int status)
{
	struct path *path;
	struct message message;
	struct passwd *pw;
	const char *errstr;
	char *sep;
	uid_t uid;

	path = TAILQ_FIRST(&s->deliverylist);

	if (path == NULL || status) {
		imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT,
		    s->message.id, 0, -1, &status, sizeof status);
		lka_clear_expandtree(&s->expandtree);
		lka_clear_deliverylist(&s->deliverylist);
		lka_session_destroy(env, s);
		return;
	}

	/* send next item to queue */
	message = s->message;
	lka_expand(path->rule.r_value.path, sizeof(path->rule.r_value.path), path);
	message.recipient = *path;
	sep = strchr(message.session_hostname, '@');
	if (sep) {
		*sep = '\0';
		uid = strtonum(message.session_hostname, 0, UID_MAX, &errstr);
		if (errstr)
			fatalx("lka: invalid uid");
		pw = getpwuid(uid);
		if (pw == NULL)
			fatalx("lka: non-existent uid"); /* XXX */
		strlcpy(message.sender.pw_name, pw->pw_name, sizeof message.sender.pw_name);
	}
	imsg_compose_event(env->sc_ievs[PROC_QUEUE], IMSG_QUEUE_APPEND,
	    s->id, 0, -1, &message, sizeof message);
	TAILQ_REMOVE(&s->deliverylist, path, entry);
	free(path);
}

int
lka_resolve_path(struct smtpd *env, struct lkasession *s, struct path *path)
{
	if (IS_RELAY(*path)) {
		path = path_dup(path);
		path->flags |= F_PATH_RELAY;
		TAILQ_INSERT_TAIL(&s->deliverylist, path, entry);
		return 1;
	}

	switch (path->rule.r_condition.c_type) {
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
			return aliases_get(env, path->rule.r_amap,
			    &s->expandtree, username);
		}

		path->flags |= F_PATH_ACCOUNT;
		pw = getpwnam(username);
		if (pw == NULL)
			return 0;

		strlcpy(path->pw_name, pw->pw_name, sizeof path->pw_name);

		if (path->flags & F_PATH_FORWARDED)
			TAILQ_INSERT_TAIL(&s->deliverylist, path, entry);
		else
			lka_request_forwardfile(env, s, path);
		return 1;
	}
	case C_VDOM: {
		if (aliases_virtual_exist(env, path->rule.r_condition.c_map, path)) {
			path->flags |= F_PATH_VIRTUAL;
                        return aliases_virtual_get(env, path->rule.r_condition.c_map, &s->expandtree, path);
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
	struct expandnode *expnode;

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

char *
lka_encode_secret(struct map_secret *map_secret)
{
	static char	 dst[1024];
	char		*src;
	int		 src_sz;

	src_sz = asprintf(&src, "%c%s%c%s", '\0', map_secret->username, '\0',
	    map_secret->password);
	if (src_sz == -1)
		fatal(NULL);
	if (__b64_ntop(src, src_sz, dst, sizeof dst) == -1) {
		free(src);
		return NULL;
	}
	dst[sizeof(dst) - 1] = '\0';

	return dst;
}

struct lkasession *
lka_session_init(struct smtpd *env, struct message *m)
{
	struct lkasession *s;

	s = calloc(1, sizeof *s);
	if (s == NULL)
		fatal(NULL);

	s->id = lka_id++;
	s->path = m->recipient;
	s->message = *m;

	RB_INIT(&s->expandtree);
	TAILQ_INIT(&s->deliverylist);
	SPLAY_INSERT(lkatree, &env->lka_sessions, s);

	return s;
}

void
lka_session_fail(struct smtpd *env, struct lkasession *s)
{
	int status;

	log_debug("lka: initina lka_resolve_path failed");
	status = S_MESSAGE_PERMFAILURE;
	imsg_compose_event(env->sc_ievs[PROC_MFA], IMSG_LKA_RCPT,
	    s->message.id, 0, -1, &status, sizeof status);
	lka_session_destroy(env, s);
}

void
lka_session_destroy(struct smtpd *env, struct lkasession *s)
{
	SPLAY_REMOVE(lkatree, &env->lka_sessions, s);
	free(s);
}

void
lka_request_forwardfile(struct smtpd *env, struct lkasession *s, struct path *path)
{
	struct forward_req	 fwreq;

	fwreq.id = s->id;
	strlcpy(fwreq.pw_name, path->pw_name, sizeof fwreq.pw_name);
	imsg_compose_event(env->sc_ievs[PROC_PARENT], IMSG_PARENT_FORWARD_OPEN, 0, 0, -1,
	    &fwreq, sizeof fwreq);
	s->pending++;
}

SPLAY_GENERATE(lkatree, lkasession, nodes, lkasession_cmp);
