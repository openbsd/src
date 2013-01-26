/*	$OpenBSD: config.c,v 1.19 2013/01/26 09:37:23 gilles Exp $	*/

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
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

static int pipes[PROC_COUNT][PROC_COUNT];

void
purge_config(uint8_t what)
{
	struct listener	*l;
	struct table	*t;
	struct rule	*r;
	struct ssl	*s;

	if (what & PURGE_LISTENERS) {
		while ((l = TAILQ_FIRST(env->sc_listeners)) != NULL) {
			TAILQ_REMOVE(env->sc_listeners, l, entry);
			free(l);
		}
		free(env->sc_listeners);
		env->sc_listeners = NULL;
	}
	if (what & PURGE_TABLES) {
		while (tree_root(env->sc_tables_tree, NULL, (void **)&t))
			table_destroy(t);
		free(env->sc_tables_dict);
		free(env->sc_tables_tree);
		env->sc_tables_dict = NULL;
		env->sc_tables_tree = NULL;
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
		while (dict_poproot(env->sc_ssl_dict, NULL, (void **)&s)) {
			bzero(s->ssl_cert, s->ssl_cert_len);
			bzero(s->ssl_key, s->ssl_key_len);
			free(s->ssl_cert);
			free(s->ssl_key);
			free(s);
		}
		free(env->sc_ssl_dict);
		env->sc_ssl_dict = NULL;
	}
}

void
init_pipes(void)
{
	int	 i, j, sockpair[2];

	for (i = 0; i < PROC_COUNT; i++)
		for (j = i + 1; j < PROC_COUNT; j++) {
			if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC,
			    sockpair) == -1)
				fatal("socketpair");
			pipes[i][j] = sockpair[0];
			pipes[j][i] = sockpair[1];
			session_socket_blockmode(pipes[i][j], BM_NONBLOCK);
			session_socket_blockmode(pipes[j][i], BM_NONBLOCK);
		}
}

void
config_process(enum smtp_proc_type proc)
{
	smtpd_process = proc;
	setproctitle("%s", proc_title(proc));
}

void
config_peer(enum smtp_proc_type proc)
{
	struct mproc	*p;

	if (proc == smtpd_process)
		fatal("config_peers: cannot peer with oneself");

	p = xcalloc(1, sizeof *p, "config_peer");
	p->proc = proc;
	p->name = xstrdup(proc_name(proc), "config_peer");
	p->handler = imsg_dispatch;

	mproc_init(p, pipes[smtpd_process][proc]);
	mproc_enable(p);
	pipes[smtpd_process][proc] = -1;

	if (proc == PROC_CONTROL)
		p_control = p;
	else if (proc == PROC_LKA)
		p_lka = p;
	else if (proc == PROC_MDA)
		p_mda = p;
	else if (proc == PROC_MFA)
		p_mfa = p;
	else if (proc == PROC_MTA)
		p_mta = p;
	else if (proc == PROC_PARENT)
		p_parent = p;
	else if (proc == PROC_QUEUE)
		p_queue = p;
	else if (proc == PROC_SCHEDULER)
		p_scheduler = p;
	else if (proc == PROC_SMTP)
		p_smtp = p;
	else
		fatalx("bad peer");
}

static void process_stat_event(int, short, void *);

void
config_done(void)
{
	static struct event	ev;
	struct timeval		tv;
	unsigned int		i, j;

	for (i = 0; i < PROC_COUNT; i++) {
		for (j = 0; j < PROC_COUNT; j++) {
			if (i == j || pipes[i][j] == -1)
				continue;
			close(pipes[i][j]);
			pipes[i][j] = -1;
		}
	}

	if (smtpd_process == PROC_CONTROL)
		return;

	evtimer_set(&ev, process_stat_event, &ev);
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	evtimer_add(&ev, &tv);
}

static void
process_stat(struct mproc *p)
{
	char			buf[1024];
	struct stat_value	value;

	if (p == NULL)
		return;

	value.type = STAT_COUNTER;
	snprintf(buf, sizeof buf, "buffer.%s.%s",
	    proc_name(smtpd_process),
	    proc_name(p->proc));
	value.u.counter = p->bytes_queued_max;
	p->bytes_queued_max = p->bytes_queued;
	stat_set(buf, &value);
}

static void
process_stat_event(int fd, short ev, void *arg)
{
	struct event	*e = arg;
	struct timeval	 tv;

	process_stat(p_control);
	process_stat(p_lka);
	process_stat(p_mda);
	process_stat(p_mfa);
	process_stat(p_mda);
	process_stat(p_parent);
	process_stat(p_queue);
	process_stat(p_scheduler);
	process_stat(p_smtp);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(e, &tv);
}
