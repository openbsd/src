/*	$OpenBSD: config.c,v 1.3 2009/01/01 16:15:47 jacekm Exp $	*/

/*
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

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "smtpd.h"

int	is_peer(struct peer *, enum smtp_proc_type, u_int);

int
is_peer(struct peer *p, enum smtp_proc_type peer, u_int peercount)
{
	u_int	i;

	for (i = 0; i < peercount; i++)
		if (p[i].id == peer)
			return (1);
	return (0);
}

void
unconfigure(struct smtpd *env)
{
}

void
configure(struct smtpd *env)
{
}

void
purge_config(struct smtpd *env, u_int8_t what)
{
	struct listener	*l;
	struct map	*m;
	struct rule	*r;
	struct cond	*c;
	struct opt	*o;
	struct ssl	*s;

	if (what & PURGE_LISTENERS) {
		while ((l = TAILQ_FIRST(&env->sc_listeners)) != NULL) {
			TAILQ_REMOVE(&env->sc_listeners, l, entry);
			free(l);
		}
		TAILQ_INIT(&env->sc_listeners);
	}
	if (what & PURGE_MAPS) {
		while ((m = TAILQ_FIRST(env->sc_maps)) != NULL) {
			TAILQ_REMOVE(env->sc_maps, m, m_entry);
			free(m);
		}
		free(env->sc_maps);
		env->sc_maps = NULL;
	}
	if (what & PURGE_RULES) {
		while ((r = TAILQ_FIRST(env->sc_rules)) != NULL) {
			TAILQ_REMOVE(env->sc_rules, r, r_entry);
			while ((c = TAILQ_FIRST(&r->r_conditions)) != NULL) {
				TAILQ_REMOVE(&r->r_conditions, c, c_entry);
				free(c);
			}
			while ((o = TAILQ_FIRST(&r->r_options)) != NULL) {
				TAILQ_REMOVE(&r->r_options, o, o_entry);
				free(o);
			}
			free(r);
		}
		env->sc_rules = NULL;
	}
	if (what & PURGE_SSL) {
		while ((s = SPLAY_ROOT(&env->sc_ssl)) != NULL) {
			SPLAY_REMOVE(ssltree, &env->sc_ssl, s);
			free(s->ssl_cert);
			free(s->ssl_key);
			free(s);
		}
		SPLAY_INIT(&env->sc_ssl);
	}
}

void
init_peers(struct smtpd *env)
{
	int	i;
	int	j;

	for (i = 0; i < PROC_COUNT; i++)
		for (j = 0; j < PROC_COUNT; j++) {
			if (i >= j)
				continue;
			if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
			    env->sc_pipes[i][j]) == -1)
				fatal("socketpair");
			session_socket_blockmode(env->sc_pipes[i][j][0],
			    BM_NONBLOCK);
			session_socket_blockmode(env->sc_pipes[i][j][1],
			    BM_NONBLOCK);
		}
}

void
config_peers(struct smtpd *env, struct peer *p, u_int peercount)
{
	u_int	i;
	u_int	j;
	u_int	src;
	u_int	dst;
	u_int	idx;

	/*
	 * close pipes
	 */
	for (i = 0; i < PROC_COUNT; i++) {
		for (j = 0; j < PROC_COUNT; j++) {
			if (i >= j)
				continue;

			if ((i == smtpd_process && is_peer(p, j, peercount)) ||
			    (j == smtpd_process && is_peer(p, i, peercount))) {
				idx = (i == smtpd_process)?1:0;
				close(env->sc_pipes[i][j][idx]);
			} else {
				close(env->sc_pipes[i][j][0]);
				close(env->sc_pipes[i][j][1]);
			}
		}
	}

	/*
	 * listen on appropriate pipes
	 */
	for (i = 0; i < peercount; i++) {

		if (p[i].id == smtpd_process)
			fatal("config_peers: cannot peer with oneself");

		src = (smtpd_process < p[i].id)?smtpd_process:p[i].id;
		dst = (src == p[i].id)?smtpd_process:p[i].id;

		if ((env->sc_ibufs[p[i].id] =
		     calloc(1, sizeof(struct imsgbuf))) == NULL)
			fatal("config_peers");

		idx = (src == smtpd_process)?0:1;
		imsg_init(env->sc_ibufs[p[i].id],
		    env->sc_pipes[src][dst][idx], p[i].cb);
		env->sc_ibufs[p[i].id]->events = EV_READ;
		env->sc_ibufs[p[i].id]->data = env;
		event_set(&env->sc_ibufs[p[i].id]->ev,
		    env->sc_ibufs[p[i].id]->fd,
		    env->sc_ibufs[p[i].id]->events,
		    env->sc_ibufs[p[i].id]->handler,
		    env->sc_ibufs[p[i].id]->data);
		event_add(&env->sc_ibufs[p[i].id]->ev, NULL);
	}
}
