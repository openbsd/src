/*	$OpenBSD: smtpd.c,v 1.92 2009/12/14 19:56:55 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mman.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	 usage(void);
void		 parent_shutdown(struct smtpd *);
void		 parent_send_config(int, short, void *);
void		 parent_send_config_listeners(struct smtpd *);
void		 parent_send_config_client_certs(struct smtpd *);
void		 parent_send_config_ruleset(struct smtpd *, int);
void		 parent_dispatch_lka(int, short, void *);
void		 parent_dispatch_mda(int, short, void *);
void		 parent_dispatch_mfa(int, short, void *);
void		 parent_dispatch_mta(int, short, void *);
void		 parent_dispatch_smtp(int, short, void *);
void		 parent_dispatch_runner(int, short, void *);
void		 parent_dispatch_control(int, short, void *);
void		 parent_sig_handler(int, short, void *);
int		 parent_open_message_file(struct batch *);
int		 parent_mailbox_open(char *, struct passwd *, struct batch *);
int		 parent_filename_open(char *, struct passwd *, struct batch *);
int		 parent_maildir_open(char *, struct passwd *, struct batch *);
int		 parent_maildir_init(struct passwd *, char *);
int		 parent_external_mda(char *, struct passwd *, struct batch *);
int		 parent_enqueue_offline(struct smtpd *, char *);
int		 parent_forward_open(char *);
int		 setup_spool(uid_t, gid_t);
int		 path_starts_with(char *, char *);

void		 fork_peers(struct smtpd *);

struct child	*child_add(struct smtpd *, pid_t, int, int);
void		 child_del(struct smtpd *, pid_t);
struct child	*child_lookup(struct smtpd *, pid_t);

void		 parent_mda_permfail(struct smtpd *, struct batch *);
void		 parent_mda_tempfail(struct smtpd *, struct batch *);
void		 parent_mda_success(struct smtpd *, struct batch *);

extern char	**environ;

int __b64_pton(char const *, unsigned char *, size_t);

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file]\n", __progname);
	exit(1);
}

void
parent_shutdown(struct smtpd *env)
{
	struct child	*child;
	pid_t		 pid;

	SPLAY_FOREACH(child, childtree, &env->children)
		if (child->type == CHILD_DAEMON)
			kill(child->pid, SIGTERM);

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_warnx("parent terminating");
	exit(0);
}

void
parent_send_config(int fd, short event, void *p)
{
	parent_send_config_listeners(p);
	parent_send_config_client_certs(p);
	parent_send_config_ruleset(p, PROC_LKA);
}

void
parent_send_config_listeners(struct smtpd *env)
{
	struct listener		*l;
	struct ssl		*s;
	struct iovec		 iov[3];
	int			 opt;

	log_debug("parent_send_config: configuring smtp");
	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, env->sc_ssl) {
		if (!(s->flags & F_SCERT))
			continue;

		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;

		imsg_composev(&env->sc_ievs[PROC_SMTP]->ibuf,
		    IMSG_CONF_SSL, 0, 0, -1, iov, nitems(iov));
		imsg_event_add(env->sc_ievs[PROC_SMTP]);
	}

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		if ((l->fd = socket(l->ss.ss_family, SOCK_STREAM, 0)) == -1)
			fatal("socket");
		opt = 1;
		if (setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			fatal("setsockopt");
		if (bind(l->fd, (struct sockaddr *)&l->ss, l->ss.ss_len) == -1)
			fatal("bind");
		imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_LISTENER,
		    0, 0, l->fd, l, sizeof(*l));
	}

	imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_send_config_client_certs(struct smtpd *env)
{
	struct ssl		*s;
	struct iovec		 iov[3];

	log_debug("parent_send_config_client_certs: configuring smtp");
	imsg_compose_event(env->sc_ievs[PROC_MTA], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, env->sc_ssl) {
		if (!(s->flags & F_CCERT))
			continue;

		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;

		imsg_composev(&env->sc_ievs[PROC_MTA]->ibuf, IMSG_CONF_SSL,
		    0, 0, -1, iov, nitems(iov));
		imsg_event_add(env->sc_ievs[PROC_MTA]);
	}

	imsg_compose_event(env->sc_ievs[PROC_MTA], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_send_config_ruleset(struct smtpd *env, int proc)
{
	struct rule		*r;
	struct cond		*cond;
	struct map		*m;
	struct mapel		*mapel;
	
	log_debug("parent_send_config_ruleset: reloading rules and maps");
	imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);
	
	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_MAP,
		    0, 0, -1, m, sizeof(*m));
		TAILQ_FOREACH(mapel, &m->m_contents, me_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_MAP_CONTENT,
			    0, 0, -1, mapel, sizeof(*mapel));
		}
	}
	
	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_RULE,
		    0, 0, -1, r, sizeof(*r));
		imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_RULE_SOURCE,
		    0, 0, -1, &r->r_sources->m_name, sizeof(r->r_sources->m_name));
		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {
			imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_CONDITION,
			    0, 0, -1, cond, sizeof(*cond));
		}
	}
	
	imsg_compose_event(env->sc_ievs[proc], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_dispatch_lka(int imsgfd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_LKA];
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
			fatal("parent_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_FORWARD_OPEN: {
			struct forward_req *fwreq = imsg.data;
			int fd;

			IMSG_SIZE_CHECK(fwreq);

			fd = parent_forward_open(fwreq->pw_name);
			fwreq->status = 0;
			if (fd == -2) {
				/* user has no ~/.forward.  it is optional, so
				 * set status to ok. */
				fwreq->status = 1;
				fd = -1;
			} else if (fd != -1)
				fwreq->status = 1;
			imsg_compose_event(iev, IMSG_PARENT_FORWARD_OPEN, 0, 0, fd, fwreq, sizeof(*fwreq));
			break;
		}
		default:
			log_warnx("parent_dispatch_lka: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_mfa(int fd, short event, void *p)
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
			fatal("parent_dispatch_mfa: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("parent_dispatch_mfa: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_mfa: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_mta(int fd, short event, void *p)
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
			fatal("parent_dispatch_mta: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("parent_dispatch_mta: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_mta: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_mda(int imsgfd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MDA];
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
			fatal("parent_dispatch_mda: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_MAILBOX_OPEN: {
			struct batch *batchp = imsg.data;
			struct path *path;
			struct passwd *pw;
			char *pw_name;
			char *file;
			u_int8_t i;
			int fd;
			struct action_handler {
				enum action_type action;
				int (*handler)(char *, struct passwd *, struct batch *);
			} action_hdl_table[] = {
				{ A_MBOX,	parent_mailbox_open },
				{ A_MAILDIR,	parent_maildir_open },
				{ A_EXT,	parent_external_mda },
				{ A_FILENAME,	parent_filename_open }
			};

			IMSG_SIZE_CHECK(batchp);

			batchp->env = env;
			path = &batchp->message.recipient;
			if (batchp->type & T_BOUNCE_BATCH) {
				path = &batchp->message.sender;
			}
			
			for (i = 0; i < nitems(action_hdl_table); ++i)
				if (action_hdl_table[i].action == path->rule.r_action)
					break;
			if (i == nitems(action_hdl_table))
				fatalx("parent_dispatch_mda: unknown action");

			file = path->rule.r_value.path;

			if (path->rule.r_action == A_FILENAME) {
				file = path->u.filename;
				pw_name = SMTPD_USER;
			}
			else if (path->rule.r_user != NULL)
				pw_name = path->rule.r_user;
			else
				pw_name = path->pw_name;

			errno = 0;
			pw = getpwnam(pw_name);
			if (pw == NULL) {
				if (errno) {
					log_warn("%s: getpwnam: %s",
					    batchp->message.message_id,
					    pw_name);
					parent_mda_tempfail(env, batchp);
				} else {
					log_warnx("%s: getpwnam: %s: user does not exist",
					    batchp->message.message_id,
					    pw_name);
					parent_mda_permfail(env, batchp);
				}
				break;
			}

			if (setegid(pw->pw_gid) || seteuid(pw->pw_uid))
				fatal("privdrop failed");

			fd = action_hdl_table[i].handler(file, pw, batchp);

			if (setegid(0) || seteuid(0))
				fatal("privraise failed");

			if (fd == -1) {
				log_warnx("%s: could not init delivery for %s",
				     batchp->message.message_id, pw_name);
				parent_mda_tempfail(env, batchp);
			} else
				imsg_compose_event(iev,
				    IMSG_PARENT_MAILBOX_OPEN, 0, 0, fd, batchp,
				    sizeof(*batchp));
			break;
		}
		case IMSG_PARENT_MESSAGE_OPEN: {
			struct batch *batchp = imsg.data;
			int fd;

			IMSG_SIZE_CHECK(batchp);

			fd = parent_open_message_file(batchp);
			imsg_compose_event(iev, IMSG_PARENT_MESSAGE_OPEN,
			    0, 0, fd, batchp, sizeof(struct batch));

			break;
		}
		case IMSG_PARENT_MAILDIR_FAIL:
		case IMSG_PARENT_MAILDIR_RENAME: {
			char		 tmp[MAXPATHLEN], new[MAXPATHLEN];
			struct batch	*batchp = imsg.data;
			struct path	*path;
			struct passwd	*pw;
			int		 ret;
			char		*pw_name;

			IMSG_SIZE_CHECK(batchp);

			path = &batchp->message.recipient;
			if (batchp->type & T_BOUNCE_BATCH) {
				path = &batchp->message.sender;
			}

			if (path->rule.r_user != NULL)
				pw_name = path->rule.r_user;
			else
				pw_name = path->pw_name;

			errno = 0;
			pw = getpwnam(pw_name);
			if (pw == NULL) {
				if (errno) {
					log_warn("%s: getpwnam: %s",
					    batchp->message.message_id,
					    pw_name);
					parent_mda_tempfail(env, batchp);
				} else {
					log_warnx("%s: getpwnam: %s: user does not exist",
					    batchp->message.message_id,
					    pw_name);
					parent_mda_permfail(env, batchp);
				}
				break;
			}

			if (! bsnprintf(tmp, sizeof(tmp), "%s/tmp/%s",
				path->rule.r_value.path, batchp->message.message_uid))
				fatal("parent_dispatch_mda: snprintf");
			if (! bsnprintf(new, sizeof(new), "%s/new/%s",
				path->rule.r_value.path, batchp->message.message_uid))
				fatal("parent_dispatch_mda: snprintf");

			if (seteuid(pw->pw_uid) == -1)
				fatal("privdrop failed");

			if (imsg.hdr.type == IMSG_PARENT_MAILDIR_FAIL) {
				unlink(tmp);
				ret = 0;
			} else
				ret = rename(tmp, new);

			if (seteuid(0) == -1)
				fatal("privraise failed");

			if (ret < 0) {
				log_warn("%s: %s: cannot rename to the 'new' directory",
				    batchp->message.message_id, tmp);
				parent_mda_tempfail(env, batchp);
				unlink(tmp);
			} else
				parent_mda_success(env, batchp);
			break;
		}
		default:
			log_warnx("parent_dispatch_mfa: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_mda: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_smtp(int fd, short event, void *p)
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
			fatal("parent_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_SEND_CONFIG: {
			parent_send_config_listeners(env);
			break;
		}
		case IMSG_PARENT_AUTHENTICATE: {
			struct auth	*req = imsg.data;

			IMSG_SIZE_CHECK(req);

			req->success = authenticate_user(req->user, req->pass);

			imsg_compose_event(iev, IMSG_PARENT_AUTHENTICATE, 0, 0,
			    -1, req, sizeof(*req));
			break;
		}
		default:
			log_warnx("parent_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_runner(int sig, short event, void *p)
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
			fatal("parent_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_ENQUEUE_OFFLINE:
			if (! parent_enqueue_offline(env, imsg.data))
				imsg_compose_event(iev, IMSG_PARENT_ENQUEUE_OFFLINE,
				    0, 0, -1, NULL, 0);
			break;
		default:
			log_warnx("parent_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_CONTROL];
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
			fatal("parent_dispatch_control: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONF_RELOAD: {
			struct reload *r = imsg.data;
			struct smtpd newenv;
			
			r->ret = 0;
			if (parse_config(&newenv, env->sc_conffile, 0) == 0) {
				
				(void)strlcpy(env->sc_hostname, newenv.sc_hostname,
				    sizeof(env->sc_hostname));
				env->sc_listeners = newenv.sc_listeners;
				env->sc_maps = newenv.sc_maps;
				env->sc_rules = newenv.sc_rules;
				env->sc_rules = newenv.sc_rules;
				env->sc_ssl = newenv.sc_ssl;
				
				parent_send_config_client_certs(env);
				parent_send_config_ruleset(env, PROC_MFA);
				parent_send_config_ruleset(env, PROC_LKA);
				imsg_compose_event(env->sc_ievs[PROC_SMTP],
				    IMSG_CONF_RELOAD, 0, 0, -1, NULL, 0);
				r->ret = 1;
			}
			imsg_compose_event(iev, IMSG_CONF_RELOAD, 0, 0, -1, r, sizeof(*r));
			break;
		}
		default:
			log_warnx("parent_dispatch_control: got imsg %d",
			    imsg.hdr.type);
			fatalx("parent_dispatch_control: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
parent_sig_handler(int sig, short event, void *p)
{
	struct smtpd	*env = p;
	struct child	*child;
	int		 die = 0, status, fail;
	pid_t		 pid;
	char		*cause;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			child = child_lookup(env, pid);
			if (child == NULL)
				fatalx("unexpected SIGCHLD");

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					asprintf(&cause, "exited abnormally");
				} else
					asprintf(&cause, "exited okay");
			} else
				fatalx("unexpected cause of SIGCHLD");

			switch (child->type) {
			case CHILD_DAEMON:
				die = 1;
				if (fail)
					log_warnx("lost child: %s %s",
					    env->sc_title[child->title], cause);
				break;

			case CHILD_MDA:
				if (WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGALRM) {
					free(cause);
					asprintf(&cause, "terminated; timeout");
				}
				if (fail) {
					log_warnx("external mda %s", cause);
					parent_mda_tempfail(env, &child->mda_batch);
				} else {
					log_debug("external mda %s", cause);
					parent_mda_success(env, &child->mda_batch);
				}
				break;

			case CHILD_ENQUEUE_OFFLINE:
				if (fail)
					log_warnx("couldn't enqueue offline "
					    "message; smtpctl %s", cause);
				else
					log_debug("offline message enqueued");
				imsg_compose_event(env->sc_ievs[PROC_RUNNER],
				    IMSG_PARENT_ENQUEUE_OFFLINE, 0, 0, -1,
				    NULL, 0);
				break;

			default:
				fatalx("unexpected child type");
			}

			child_del(env, child->pid);
			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			parent_shutdown(env);
		break;
	default:
		fatalx("unexpected signal");
	}
}

int
main(int argc, char *argv[])
{
	int		 c;
	int		 debug;
	int		 opts;
	const char	*conffile = CONF_FILE;
	struct smtpd	 env;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct timeval	 tv;
	struct peer	 peers[] = {
		{ PROC_CONTROL,	parent_dispatch_control },
		{ PROC_LKA,	parent_dispatch_lka },
		{ PROC_MDA,	parent_dispatch_mda },
		{ PROC_MFA,	parent_dispatch_mfa },
		{ PROC_MTA,	parent_dispatch_mta },
		{ PROC_SMTP,	parent_dispatch_smtp },
		{ PROC_RUNNER,	parent_dispatch_runner }
	};

	opts = 0;
	debug = 0;

	log_init(1);

	while ((c = getopt(argc, argv, "dD:nf:v")) != -1) {
		switch (c) {
		case 'd':
			debug = 2;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			debug = 2;
			opts |= SMTPD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			opts |= SMTPD_OPT_VERBOSE;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	if (parse_config(&env, conffile, opts))
		exit(1);

	if (strlcpy(env.sc_conffile, conffile, MAXPATHLEN) >= MAXPATHLEN)
		errx(1, "config file exceeds MAXPATHLEN");


	if (env.sc_opts & SMTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	if ((env.sc_pw =  getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);

	if (!setup_spool(env.sc_pw->pw_uid, 0))
		errx(1, "invalid directory permissions");

	log_init(debug);

	if (!debug)
		if (daemon(0, 0) == -1)
			err(1, "failed to daemonize");

	log_info("startup%s", (debug > 1)?" [debug mode]":"");

	env.stats = mmap(NULL, sizeof(struct stats), PROT_WRITE|PROT_READ,
	    MAP_ANON|MAP_SHARED, -1, (off_t)0);
	if (env.stats == MAP_FAILED)
		fatal("mmap");
	bzero(env.stats, sizeof(struct stats));

	env.stats->parent.start = time(NULL);

	fork_peers(&env);

	event_init();

	signal_set(&ev_sigint, SIGINT, parent_sig_handler, &env);
	signal_set(&ev_sigterm, SIGTERM, parent_sig_handler, &env);
	signal_set(&ev_sigchld, SIGCHLD, parent_sig_handler, &env);
	signal_set(&ev_sighup, SIGHUP, parent_sig_handler, &env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	config_pipes(&env, peers, nitems(peers));
	config_peers(&env, peers, nitems(peers));

	evtimer_set(&env.sc_ev, parent_send_config, &env);
	bzero(&tv, sizeof(tv));
	evtimer_add(&env.sc_ev, &tv);

	event_dispatch();

	return (0);
}

void
fork_peers(struct smtpd *env)
{
	SPLAY_INIT(&env->children);

	/*
	 * Pick descriptor limit that will guarantee impossibility of fd
	 * starvation condition.  The logic:
	 *
	 * Treat hardlimit as 100%.
	 * Limit smtp to 50% (inbound connections)
	 * Limit mta to 50% (outbound connections)
	 * Limit mda to 50% (local deliveries)
	 * In all three above, compute max session limit by halving the fd
	 * limit (50% -> 25%), because each session costs two fds.
	 * Limit queue to 100% to cover the extreme case when tons of fds are
	 * opened for all four possible purposes (smtp, mta, mda, bounce)
	 */
	fdlimit(0.5);

	env->sc_instances[PROC_CONTROL] = 1;
	env->sc_instances[PROC_LKA] = 1;
	env->sc_instances[PROC_MDA] = 1;
	env->sc_instances[PROC_MFA] = 1;
	env->sc_instances[PROC_MTA] = 1;
	env->sc_instances[PROC_PARENT] = 1;
	env->sc_instances[PROC_QUEUE] = 1;
	env->sc_instances[PROC_RUNNER] = 1;
	env->sc_instances[PROC_SMTP] = 1;

	init_pipes(env);

	env->sc_title[PROC_CONTROL] = "control";
	env->sc_title[PROC_LKA] = "lookup agent";
	env->sc_title[PROC_MDA] = "mail delivery agent";
	env->sc_title[PROC_MFA] = "mail filter agent";
	env->sc_title[PROC_MTA] = "mail transfer agent";
	env->sc_title[PROC_QUEUE] = "queue";
	env->sc_title[PROC_RUNNER] = "runner";
	env->sc_title[PROC_SMTP] = "smtp server";

	child_add(env, control(env), CHILD_DAEMON, PROC_CONTROL);
	child_add(env, lka(env), CHILD_DAEMON, PROC_LKA);
	child_add(env, mda(env), CHILD_DAEMON, PROC_MDA);
	child_add(env, mfa(env), CHILD_DAEMON, PROC_MFA);
	child_add(env, mta(env), CHILD_DAEMON, PROC_MTA);
	child_add(env, queue(env), CHILD_DAEMON, PROC_QUEUE);
	child_add(env, runner(env), CHILD_DAEMON, PROC_RUNNER);
	child_add(env, smtp(env), CHILD_DAEMON, PROC_SMTP);
}

struct child *
child_add(struct smtpd *env, pid_t pid, int type, int title)
{
	struct child	*child;

	if ((child = calloc(1, sizeof(*child))) == NULL)
		fatal(NULL);

	child->pid = pid;
	child->type = type;
	child->title = title;

	if (SPLAY_INSERT(childtree, &env->children, child) != NULL)
		fatalx("child_add: double insert");

	return (child);
}

void
child_del(struct smtpd *env, pid_t pid)
{
	struct child	*p;

	p = child_lookup(env, pid);
	if (p == NULL)
		fatalx("child_del: unknown child");

	if (SPLAY_REMOVE(childtree, &env->children, p) == NULL)
		fatalx("child_del: tree remove failed");
	free(p);
}

struct child *
child_lookup(struct smtpd *env, pid_t pid)
{
	struct child	 key;

	key.pid = pid;
	return SPLAY_FIND(childtree, &env->children, &key);
}

int
setup_spool(uid_t uid, gid_t gid)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_ENQUEUE, PATH_QUEUE,
				     PATH_RUNQUEUE, PATH_RUNQUEUELOW,
				     PATH_RUNQUEUEHIGH, PATH_PURGE,
				     PATH_OFFLINE, PATH_BOUNCE };
	char		 pathname[MAXPATHLEN];
	struct stat	 sb;
	int		 ret;

	if (! bsnprintf(pathname, sizeof(pathname), "%s", PATH_SPOOL))
		fatal("snprintf");

	if (stat(pathname, &sb) == -1) {
		if (errno != ENOENT) {
			warn("stat: %s", pathname);
			return 0;
		}

		if (mkdir(pathname, 0711) == -1) {
			warn("mkdir: %s", pathname);
			return 0;
		}

		if (chown(pathname, 0, 0) == -1) {
			warn("chown: %s", pathname);
			return 0;
		}

		if (stat(pathname, &sb) == -1)
			err(1, "stat: %s", pathname);
	}

	/* check if it's a directory */
	if (!S_ISDIR(sb.st_mode)) {
		warnx("%s is not a directory", pathname);
		return 0;
	}

	/* check that it is owned by uid/gid */
	if (sb.st_uid != 0 || sb.st_gid != 0) {
		warnx("%s must be owned by root:wheel", pathname);
		return 0;
	}

	/* check permission */
	if ((sb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR)) != (S_IRUSR|S_IWUSR|S_IXUSR) ||
	    (sb.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP)) != S_IXGRP ||
	    (sb.st_mode & (S_IROTH|S_IWOTH|S_IXOTH)) != S_IXOTH) {
		warnx("%s must be rwx--x--x (0711)", pathname);
		return 0;
	}

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		mode_t	mode;
		uid_t	owner;
		gid_t	group;

		if (!strcmp(paths[n], PATH_OFFLINE)) {
			mode = 01777;
			owner = 0;
			group = 0;
		} else {
			mode = 0700;
			owner = uid;
			group = gid;
		}

		if (! bsnprintf(pathname, sizeof(pathname), "%s%s", PATH_SPOOL,
			paths[n]))
			fatal("snprintf");

		if (stat(pathname, &sb) == -1) {
			if (errno != ENOENT) {
				warn("stat: %s", pathname);
				ret = 0;
				continue;
			}

			/* chmod is deffered to avoid umask effect */
			if (mkdir(pathname, 0) == -1) {
				ret = 0;
				warn("mkdir: %s", pathname);
			}

			if (chown(pathname, owner, group) == -1) {
				ret = 0;
				warn("chown: %s", pathname);
			}

			if (chmod(pathname, mode) == -1) {
				ret = 0;
				warn("chmod: %s", pathname);
			}

			if (stat(pathname, &sb) == -1)
				err(1, "stat: %s", pathname);
		}

		/* check if it's a directory */
		if (!S_ISDIR(sb.st_mode)) {
			ret = 0;
			warnx("%s is not a directory", pathname);
		}

		/* check that it is owned by owner/group */
		if (sb.st_uid != owner) {
			ret = 0;
			warnx("%s is not owned by uid %d", pathname, owner);
		}
		if (sb.st_gid != group) {
			ret = 0;
			warnx("%s is not owned by gid %d", pathname, group);
		}

		/* check permission */
		if ((sb.st_mode & 07777) != mode) {
			char mode_str[12];

			ret = 0;
			strmode(mode, mode_str);
			mode_str[10] = '\0';
			warnx("%s must be %s (%o)", pathname, mode_str + 1, mode);
		}
	}
	return ret;
}

void
imsg_event_add(struct imsgev *iev)
{
	if (iev->handler == NULL) {
		imsg_flush(&iev->ibuf);
		return;
	}

	iev->events = EV_READ;
	if (iev->ibuf.w.queued)
		iev->events |= EV_WRITE;

	event_del(&iev->ev);
	event_set(&iev->ev, iev->ibuf.fd, iev->events, iev->handler, iev->data);
	event_add(&iev->ev, NULL);
}

int
imsg_compose_event(struct imsgev *iev, u_int16_t type, u_int32_t peerid,
    pid_t pid, int fd, void *data, u_int16_t datalen)
{
	int	ret;

	if ((ret = imsg_compose(&iev->ibuf, type, peerid,
	    pid, fd, data, datalen)) != -1)
		imsg_event_add(iev);
	return (ret);
}

int
parent_open_message_file(struct batch *batchp)
{
	int fd;
	char pathname[MAXPATHLEN];
	u_int16_t hval;
	struct message *messagep;

	messagep = &batchp->message;
	hval = queue_hash(messagep->message_id);

	if (! bsnprintf(pathname, sizeof(pathname), "%s%s/%d/%s/message",
		PATH_SPOOL, PATH_QUEUE, hval, batchp->message_id))
		fatal("parent_open_message_file: snprintf");

	fd = open(pathname, O_RDONLY);
	return fd;
}

int
parent_mailbox_open(char *path, struct passwd *pw, struct batch *batchp)
{
	pid_t pid;
	int pipefd[2];
	struct child *child;
	char sender[MAX_PATH_SIZE];

	/* This can never happen, but better safe than sorry. */
	if (! bsnprintf(sender, MAX_PATH_SIZE, "%s@%s",
		batchp->message.sender.user,
		batchp->message.sender.domain))
		fatal("parent_mailbox_open: bogus email length");

	log_debug("executing mail.local");
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1)
		return -1;

	/* raise privileges because mail.local needs root to
	 * deliver to user mailboxes.
	 */
	if (seteuid(0) == -1)
		fatal("privraise failed");

	pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		close(pipefd[0]);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		dup2(pipefd[1], 0);

		/* avoid hangs by setting a 5m timeout */
		alarm(300);

		execlp(PATH_MAILLOCAL, "mail.local", "-f", sender, pw->pw_name,
		    (void *)NULL);
		_exit(1);
	}

	if (seteuid(pw->pw_uid) == -1)
		fatal("privdrop failed");

	child = child_add(batchp->env, pid, CHILD_MDA, -1);

	/* Each child relates to a batch; record this relationship. */
	child->mda_batch = *batchp;

	close(pipefd[1]);
	return pipefd[0];
}

int
parent_maildir_init(struct passwd *pw, char *root)
{
	u_int8_t i;
	char pathname[MAXPATHLEN];
	char *subdir[] = { "/", "/tmp", "/cur", "/new" };

	for (i = 0; i < nitems(subdir); ++i) {
		if (! bsnprintf(pathname, sizeof(pathname), "%s%s", root,
			subdir[i]))
			return 0;
		if (mkdir(pathname, 0700) == -1)
			if (errno != EEXIST)
				return 0;
	}

	return 1;
}

int
parent_maildir_open(char *path, struct passwd *pw, struct batch *batchp)
{
	char tmp[MAXPATHLEN];
	int mode = O_CREAT|O_RDWR|O_TRUNC|O_SYNC;

	if (! parent_maildir_init(pw, path))
		return -1;

	if (! bsnprintf(tmp, sizeof(tmp), "%s/tmp/%s", path,
		batchp->message.message_uid))
		return -1;

	return open(tmp, mode, 0600);
}

int
parent_external_mda(char *path, struct passwd *pw, struct batch *batchp)
{
	struct child *child;
	pid_t pid;
	int pipefd[2];
	arglist args;
	char *word;
	char *envp[2];

	log_debug("executing filter as user: %s", pw->pw_name);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1) {
		if (errno == ENFILE) {
			log_warn("parent_external_mda: socketpair");
			return -1;
		}
		fatal("parent_external_mda: socketpair");
	}

	pid = fork();
	if (pid == -1) {
		log_warn("parent_external_mda: fork");
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		if (seteuid(0) == -1)
			fatal("privraise failed");
		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			fatal("cannot drop privileges");

		bzero(&args, sizeof(args));
		while ((word = strsep(&path, " \t")) != NULL)
			if (*word != '\0')
				addargs(&args, "%s", word);

		if (setsid() == -1)
			fatal("setsid");

		if (signal(SIGPIPE, SIG_DFL) == SIG_ERR)
			fatal("signal");

		if (dup2(pipefd[0], STDIN_FILENO) == -1)
			fatal("dup2");

		if (chdir(pw->pw_dir) == -1 && chdir("/") == -1)
			fatal("chdir");

		if (closefrom(STDERR_FILENO + 1) == -1)
			fatal("closefrom");

		/* avoid hangs by setting a 5m timeout */
		alarm(300);

		envp[0] = "PATH=" _PATH_DEFPATH;
		envp[1] = (char *)NULL;
		environ = envp;

		execvp(args.list[0], args.list);
		_exit(1);
	}

	child = child_add(batchp->env, pid, CHILD_MDA, -1);

	/* Each child relates to a batch; record this relationship. */
	child->mda_batch = *batchp;

	close(pipefd[0]);
	return pipefd[1];
}

int
parent_enqueue_offline(struct smtpd *env, char *runner_path)
{
	char		 path[MAXPATHLEN];
	struct passwd	*pw;
	struct stat	 sb;
	pid_t		 pid;

	log_debug("parent_enqueue_offline: path %s", runner_path);

	if (! bsnprintf(path, sizeof(path), "%s%s", PATH_SPOOL, runner_path))
		fatalx("parent_enqueue_offline: filename too long");

	if (! path_starts_with(path, PATH_SPOOL PATH_OFFLINE))
		fatalx("parent_enqueue_offline: path outside offline dir");

	if (lstat(path, &sb) == -1) {
		if (errno == ENOENT) {
			log_warn("parent_enqueue_offline: %s", path);
			return (0);
		}
		fatal("parent_enqueue_offline: lstat");
	}

	if (chflags(path, 0) == -1) {
		if (errno == ENOENT) {
			log_warn("parent_enqueue_offline: %s", path);
			return (0);
		}
		fatal("parent_enqueue_offline: chflags");
	}

	errno = 0;
	if ((pw = getpwuid(sb.st_uid)) == NULL) {
		log_warn("parent_enqueue_offline: getpwuid for uid %d failed",
		    sb.st_uid);
		unlink(path);
		return (0);
	}

	if (! S_ISREG(sb.st_mode)) {
		log_warnx("file %s (uid %d) not regular, removing", path, sb.st_uid);
		if (S_ISDIR(sb.st_mode))
			rmdir(path);
		else
			unlink(path);
		return (0);
	}

	if ((pid = fork()) == -1)
		fatal("parent_enqueue_offline: fork");

	if (pid == 0) {
		char	*envp[2], *p, *tmp;
		FILE	*fp;
		size_t	 len;
		arglist	 args;

		bzero(&args, sizeof(args));

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) ||
		    closefrom(STDERR_FILENO + 1) == -1) {
			unlink(path);
			_exit(1);
		}

		if ((fp = fopen(path, "r")) == NULL) {
			unlink(path);
			_exit(1);
		}
		unlink(path);

		if (chdir(pw->pw_dir) == -1 && chdir("/") == -1)
			_exit(1);

		if (setsid() == -1 ||
		    signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
		    dup2(fileno(fp), STDIN_FILENO) == -1)
			_exit(1);

		if ((p = fgetln(fp, &len)) == NULL)
			_exit(1);

		if (p[len - 1] != '\n')
			_exit(1);
		p[len - 1] = '\0';

		addargs(&args, "%s", "sendmail");

		while ((tmp = strsep(&p, "|")) != NULL)
			addargs(&args, "%s", tmp);

		if (lseek(fileno(fp), len, SEEK_SET) == -1)
			_exit(1);

		envp[0] = "PATH=" _PATH_DEFPATH;
		envp[1] = (char *)NULL;
		environ = envp;

		execvp(PATH_SMTPCTL, args.list);
		_exit(1);
	}

	child_add(env, pid, CHILD_ENQUEUE_OFFLINE, -1);

	return (1);
}

int
parent_filename_open(char *path, struct passwd *pw, struct batch *batchp)
{
	int fd;
	int mode = O_CREAT|O_APPEND|O_RDWR|O_SYNC|O_NONBLOCK;

	fd = open(path, mode, 0600);
	if (fd == -1) {
		if (errno == EWOULDBLOCK)
			goto lockfail;
		return -1;
	}

	if (flock(fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK)
			goto lockfail;
		fatal("flock");
	}

	return fd;

lockfail:
	if (fd != -1)
		close(fd);

	batchp->message.status |= S_MESSAGE_LOCKFAILURE;
	return -1;
}

int
parent_forward_open(char *username)
{
	struct passwd *pw;
	char pathname[MAXPATHLEN];
	int fd;

	pw = getpwnam(username);
	if (pw == NULL)
		return -1;

	if (! bsnprintf(pathname, sizeof (pathname), "%s/.forward", pw->pw_dir))
		fatal("snprintf");

	fd = open(pathname, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT)
			return -2;
		log_warn("parent_forward_open: %s", pathname);
		return -1;
	}

	if (! secure_file(fd, pathname, pw, 1)) {
		log_warnx("%s: unsecure file", pathname);
		close(fd);
		return -1;
	}

	return fd;
}

int
path_starts_with(char *file, char *prefix)
{
	char	 rprefix[MAXPATHLEN];
	char	 rfile[MAXPATHLEN];

	if (realpath(file, rfile) == NULL || realpath(prefix, rprefix) == NULL)
		return (-1);

	return (strncmp(rfile, rprefix, strlen(rprefix)) == 0);
}

int
child_cmp(struct child *c1, struct child *c2)
{
	if (c1->pid < c2->pid)
		return (-1);

	if (c1->pid > c2->pid)
		return (1);

	return (0);
}

void
parent_mda_permfail(struct smtpd *env, struct batch *b)
{
	b->message.status |= S_MESSAGE_PERMFAILURE;
	imsg_compose_event(env->sc_ievs[PROC_MDA], IMSG_MDA_FINALIZE,
	    0, 0, -1, b, sizeof(*b));
}

void
parent_mda_tempfail(struct smtpd *env, struct batch *b)
{
	imsg_compose_event(env->sc_ievs[PROC_MDA], IMSG_MDA_FINALIZE,
	    0, 0, -1, b, sizeof(*b));
}

void
parent_mda_success(struct smtpd *env, struct batch *b)
{
	b->message.status &= ~S_MESSAGE_TEMPFAILURE;
	imsg_compose_event(env->sc_ievs[PROC_MDA], IMSG_MDA_FINALIZE,
	    0, 0, -1, b, sizeof(*b));
}

SPLAY_GENERATE(childtree, child, entry, child_cmp);
