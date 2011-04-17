/*	$OpenBSD: config.c,v 1.14 2011/04/17 13:36:07 gilles Exp $	*/

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
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

static int is_peer(struct peer *, enum smtp_proc_type, u_int);

static int
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
	struct ssl	*s;
	struct mapel	*me;

	if (what & PURGE_LISTENERS) {
		while ((l = TAILQ_FIRST(env->sc_listeners)) != NULL) {
			TAILQ_REMOVE(env->sc_listeners, l, entry);
			free(l);
		}
		free(env->sc_listeners);
		env->sc_listeners = NULL;
	}
	if (what & PURGE_MAPS) {
		while ((m = TAILQ_FIRST(env->sc_maps)) != NULL) {
			TAILQ_REMOVE(env->sc_maps, m, m_entry);
			while ((me = TAILQ_FIRST(&m->m_contents))) {
				TAILQ_REMOVE(&m->m_contents, me, me_entry);
				free(me);
			}
			free(m);
		}
		free(env->sc_maps);
		env->sc_maps = NULL;
	}
	if (what & PURGE_RULES) {
		while ((r = TAILQ_FIRST(env->sc_rules)) != NULL) {
			TAILQ_REMOVE(env->sc_rules, r, r_entry);
			free(r);
		}
		free(env->sc_rules);
		env->sc_rules = NULL;
	}
	if (what & PURGE_SSL) {
		while ((s = SPLAY_ROOT(env->sc_ssl)) != NULL) {
			SPLAY_REMOVE(ssltree, env->sc_ssl, s);
			free(s->ssl_cert);
			free(s->ssl_key);
			free(s);
		}
		free(env->sc_ssl);
		env->sc_ssl = NULL;
	}
}

void
init_pipes(struct smtpd *env)
{
	int	 i;
	int	 j;
	int	 count;
	int	 sockpair[2];

	for (i = 0; i < PROC_COUNT; i++)
		for (j = 0; j < PROC_COUNT; j++) {
			/*
			 * find out how many instances of this peer there are.
			 */
			if (i >= j || env->sc_instances[i] == 0||
			   env->sc_instances[j] == 0)
				continue;

			if (env->sc_instances[i] > 1 &&
			    env->sc_instances[j] > 1)
				fatalx("N:N peering not supported");

			count = env->sc_instances[i] * env->sc_instances[j];

			if ((env->sc_pipes[i][j] =
			    calloc(count, sizeof(int))) == NULL ||
			    (env->sc_pipes[j][i] =
			    calloc(count, sizeof(int))) == NULL)
				fatal(NULL);

			while (--count >= 0) {
				if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
				    sockpair) == -1)
					fatal("socketpair");
				env->sc_pipes[i][j][count] = sockpair[0];
				env->sc_pipes[j][i][count] = sockpair[1];
				session_socket_blockmode(
				    env->sc_pipes[i][j][count],
				    BM_NONBLOCK);
				session_socket_blockmode(
				    env->sc_pipes[j][i][count],
				    BM_NONBLOCK);
			}
		}
}

void
config_pipes(struct smtpd *env, struct peer *p, u_int peercount)
{
	u_int	i;
	u_int	j;
	int	count;

	/*
	 * close pipes
	 */
	for (i = 0; i < PROC_COUNT; i++) {
		for (j = 0; j < PROC_COUNT; j++) {
			if (i == j ||
			    env->sc_instances[i] == 0 ||
			    env->sc_instances[j] == 0)
				continue;

			for (count = 0;
			    count < env->sc_instances[i]*env->sc_instances[j];
			    count++) {
				if (i == smtpd_process &&
				    is_peer(p, j, peercount) &&
				    count == env->sc_instance)
					continue;
				if (i == smtpd_process &&
				    is_peer(p, j, peercount) &&
				    env->sc_instances[i] == 1)
					continue;
				close(env->sc_pipes[i][j][count]);
				env->sc_pipes[i][j][count] = -1;
			}
		}
	}
}

void
config_peers(struct smtpd *env, struct peer *p, u_int peercount)
{
	int	count;
	u_int	src;
	u_int	dst;
	u_int	i;
	/*
	 * listen on appropriate pipes
	 */
	for (i = 0; i < peercount; i++) {

		src = smtpd_process;
		dst = p[i].id;

		if (dst == smtpd_process)
			fatal("config_peers: cannot peer with oneself");
		
		if ((env->sc_ievs[dst] = calloc(env->sc_instances[dst],
		    sizeof(struct imsgev))) == NULL)
			fatal("config_peers");

		for (count = 0; count < env->sc_instances[dst]; count++) {
			imsg_init(&(env->sc_ievs[dst][count].ibuf),
			    env->sc_pipes[src][dst][count]);
			env->sc_ievs[dst][count].handler =  p[i].cb;
			env->sc_ievs[dst][count].events = EV_READ;
			env->sc_ievs[dst][count].proc = dst;
			env->sc_ievs[dst][count].data = &env->sc_ievs[dst][count];
			env->sc_ievs[dst][count].env = env;

			event_set(&(env->sc_ievs[dst][count].ev),
			    env->sc_ievs[dst][count].ibuf.fd,
			    env->sc_ievs[dst][count].events,
			    env->sc_ievs[dst][count].handler,
			    env->sc_ievs[dst][count].data);
			event_add(&(env->sc_ievs[dst][count].ev), NULL);
		}
	}
}
