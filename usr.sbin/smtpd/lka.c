/*	$OpenBSD: lka.c,v 1.128 2011/10/09 18:39:53 eric Exp $	*/

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

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <pwd.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

struct rule *ruleset_match(struct envelope *);
static void lka_imsg(struct imsgev *, struct imsg *);
static void lka_shutdown(void);
static void lka_sig_handler(int, short, void *);
static int lka_verify_mail(struct mailaddr *);
static int lka_encode_credentials(char *, size_t, struct map_secret *);

void lka_session(struct submit_status *);
void lka_session_forward_reply(struct forward_req *, int);

static void
lka_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct submit_status	*ss;
	struct secret		*secret;
	struct mapel		*mapel;
	struct rule		*rule;
	struct map		*map;
	void			*tmp;

	log_imsg(PROC_LKA, iev->proc, imsg);

	if (imsg->hdr.type == IMSG_DNS_HOST || imsg->hdr.type == IMSG_DNS_MX ||
	    imsg->hdr.type == IMSG_DNS_PTR) {
		dns_async(iev, imsg->hdr.type, imsg->data);
		return;
	}

	if (iev->proc == PROC_MFA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_MAIL:
			ss = imsg->data;
			ss->code = 530;
			if (ss->u.maddr.user[0] == '\0' &&
			    ss->u.maddr.domain[0] == '\0')
				ss->code = 250;
			else
				if (lka_verify_mail(&ss->u.maddr))
					ss->code = 250;
			imsg_compose_event(iev, IMSG_LKA_MAIL, 0, 0, -1, ss,
			    sizeof *ss);
			return;

		case IMSG_LKA_RULEMATCH:
			ss = imsg->data;
			ss->code = 530;
			rule = ruleset_match(&ss->envelope);
			if (rule) {
				ss->code = 250;
				ss->envelope.rule = *rule;
				if (IS_RELAY(*rule))
					ss->envelope.delivery.type = D_MTA;
				else
					ss->envelope.delivery.type = D_MDA;
			}
			imsg_compose_event(iev, IMSG_LKA_RULEMATCH, 0, 0, -1,
			    ss, sizeof *ss);
			return;

		case IMSG_LKA_RCPT:
			lka_session(imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_MTA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_SECRET: {
			struct map_secret *map_secret;

			secret = imsg->data;
			map = map_find(secret->secmapid);
			if (map == NULL)
				fatalx("lka: secrets map not found");
			map_secret = map_lookup(map->m_id, secret->host, K_SECRET);
			log_debug("lka: %s secret lookup (%d)", secret->host,
			    map_secret != NULL);
			secret->secret[0] = '\0';
			if (map_secret == NULL)
				log_warnx("%s secret not found", secret->host);
			else if (lka_encode_credentials(secret->secret,
				     sizeof secret->secret, map_secret) == 0)
				log_warnx("%s secret parse fail", secret->host);
			imsg_compose_event(iev, IMSG_LKA_SECRET, 0, 0, -1, secret,
			    sizeof *secret);
			free(map_secret);
			return;
		}
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
			rule->r_sources = map_findbyname(imsg->data);
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
				purge_config(PURGE_RULES);
			if (env->sc_maps)
				purge_config(PURGE_MAPS);
			env->sc_rules = env->sc_rules_reload;
			env->sc_maps = env->sc_maps_reload;
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;

		case IMSG_PARENT_FORWARD_OPEN:
			lka_session_forward_reply(imsg->data, imsg->fd);
			return;

		}
	}

	fatalx("lka_imsg: unexpected imsg");
}

static void
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

pid_t
lka(void)
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

	purge_config(PURGE_EVERYTHING);

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

	signal_set(&ev_sigint, SIGINT, lka_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, lka_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, lka_sig_handler, NULL);
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

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));

	if (event_dispatch() < 0)
		fatal("event_dispatch");
	lka_shutdown();

	return (0);
}

int
lka_verify_mail(struct mailaddr *maddr)
{
	return 1;
}

static int
lka_encode_credentials(char *dst, size_t size, struct map_secret *map_secret)
{
	char	*buf;
	int	 buflen;

	if ((buflen = asprintf(&buf, "%c%s%c%s", '\0', map_secret->username,
		    '\0', map_secret->password)) == -1)
		fatal(NULL);

	if (__b64_ntop((unsigned char *)buf, buflen, dst, size) == -1) {
		free(buf);
		return 0;
	}

	free(buf);
	return 1;
}
