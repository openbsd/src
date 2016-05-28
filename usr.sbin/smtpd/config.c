/*	$OpenBSD: config.c,v 1.35 2016/05/28 21:21:20 eric Exp $	*/

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
#include <sys/socket.h>
#include <sys/resource.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

extern int profiling;

void
purge_config(uint8_t what)
{
	struct listener	*l;
	struct table	*t;
	struct rule	*r;
	struct pki	*p;
	const char	*k;
	void		*iter_dict;

	if (what & PURGE_LISTENERS) {
		while ((l = TAILQ_FIRST(env->sc_listeners)) != NULL) {
			TAILQ_REMOVE(env->sc_listeners, l, entry);
			free(l);
		}
		free(env->sc_listeners);
		env->sc_listeners = NULL;
	}
	if (what & PURGE_TABLES) {
		while (dict_root(env->sc_tables_dict, NULL, (void **)&t))
			table_destroy(t);
		free(env->sc_tables_dict);
		env->sc_tables_dict = NULL;
	}
	if (what & PURGE_RULES) {
		while ((r = TAILQ_FIRST(env->sc_rules)) != NULL) {
			TAILQ_REMOVE(env->sc_rules, r, r_entry);
			free(r);
		}
		free(env->sc_rules);
		env->sc_rules = NULL;
	}
	if (what & PURGE_PKI) {
		while (dict_poproot(env->sc_pki_dict, (void **)&p)) {
			explicit_bzero(p->pki_cert, p->pki_cert_len);
			free(p->pki_cert);
			if (p->pki_key) {
				explicit_bzero(p->pki_key, p->pki_key_len);
				free(p->pki_key);
			}
			if (p->pki_pkey)
				EVP_PKEY_free(p->pki_pkey);
			free(p);
		}
		free(env->sc_pki_dict);
		env->sc_pki_dict = NULL;
	} else if (what & PURGE_PKI_KEYS) {
		iter_dict = NULL;
		while (dict_iter(env->sc_pki_dict, &iter_dict, &k,
		    (void **)&p)) {
			explicit_bzero(p->pki_cert, p->pki_cert_len);
			free(p->pki_cert);
			p->pki_cert = NULL;
			if (p->pki_key) {
				explicit_bzero(p->pki_key, p->pki_key_len);
				free(p->pki_key);
				p->pki_key = NULL;
			}
			if (p->pki_pkey)
				EVP_PKEY_free(p->pki_pkey);
			p->pki_pkey = NULL;
		}
	}
}

void
config_process(enum smtp_proc_type proc)
{
	struct rlimit rl;

	smtpd_process = proc;
	setproctitle("%s", proc_title(proc));

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("fdlimit: getrlimit");
	rl.rlim_cur = rl.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("fdlimit: setrlimit");
}

void
config_peer(enum smtp_proc_type proc)
{
	struct mproc	*p;

	if (proc == smtpd_process)
		fatal("config_peers: cannot peer with oneself");

	if (proc == PROC_CONTROL)
		p = p_control;
	else if (proc == PROC_LKA)
		p = p_lka;
	else if (proc == PROC_PARENT)
		p = p_parent;
	else if (proc == PROC_QUEUE)
		p = p_queue;
	else if (proc == PROC_SCHEDULER)
		p = p_scheduler;
	else if (proc == PROC_PONY)
		p = p_pony;
	else if (proc == PROC_CA)
		p = p_ca;
	else
		fatalx("bad peer");

	mproc_enable(p);
}

static void process_stat_event(int, short, void *);

void
config_done(void)
{
	static struct event	ev;
	struct timeval		tv;

	if (smtpd_process == PROC_CONTROL)
		return;

	if (!(profiling & PROFILE_BUFFERS))
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
	(void)snprintf(buf, sizeof buf, "buffer.%s.%s",
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
	process_stat(p_parent);
	process_stat(p_queue);
	process_stat(p_scheduler);
	process_stat(p_pony);
	process_stat(p_ca);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(e, &tv);
}
