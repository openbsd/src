/*	$OpenBSD: smtpd.c,v 1.186 2013/01/31 18:34:43 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
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

#include <bsd_auth.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <login_cap.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/ssl.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

static void parent_imsg(struct mproc *, struct imsg *);
static void usage(void);
static void parent_shutdown(void);
static void parent_send_config(int, short, void *);
static void parent_send_config_lka(void);
static void parent_send_config_mfa(void);
static void parent_send_config_smtp(void);
static void parent_sig_handler(int, short, void *);
static void forkmda(struct mproc *, uint64_t, struct deliver *);
static int parent_forward_open(char *, char *, uid_t, gid_t);
static void parent_broadcast_verbose(uint32_t);
static void parent_broadcast_profile(uint32_t);
static void fork_peers(void);
static struct child *child_add(pid_t, int, const char *);

static void	offline_scan(int, short, void *);
static int	offline_add(char *);
static void	offline_done(void);
static int	offline_enqueue(char *);

static void	purge_task(int, short, void *);
static void	log_imsg(int, int, struct imsg *);
static int	parent_auth_user(const char *, const char *);

enum child_type {
	CHILD_DAEMON,
	CHILD_MDA,
	CHILD_ENQUEUE_OFFLINE,
};

struct child {
	pid_t			 pid;
	enum child_type		 type;
	const char		*title;
	int			 mda_out;
	uint64_t		 mda_id;
	char			*path;
	char			*cause;
};

struct offline {
	TAILQ_ENTRY(offline)	 entry;
	char			*path;
};

#define OFFLINE_READMAX		20
#define OFFLINE_QUEUEMAX	5
static size_t			offline_running = 0;
TAILQ_HEAD(, offline)		offline_q;

static struct event		offline_ev;
static struct timeval		offline_timeout;

static pid_t			purge_pid;
static struct timeval		purge_timeout;
static struct event		purge_ev;

extern char	**environ;
void		(*imsg_callback)(struct mproc *, struct imsg *);

enum smtp_proc_type	smtpd_process;

struct smtpd	*env = NULL;

struct mproc	*p_control = NULL;
struct mproc	*p_lka = NULL;
struct mproc	*p_mda = NULL;
struct mproc	*p_mfa = NULL;
struct mproc	*p_mta = NULL;
struct mproc	*p_parent = NULL;
struct mproc	*p_queue = NULL;
struct mproc	*p_scheduler = NULL;
struct mproc	*p_smtp = NULL;

const char	*backend_queue = "fs";
const char	*backend_scheduler = "ramqueue";
const char	*backend_stat = "ram";

int	profiling = 0;
int	verbose = 0;
int	debug = 0;

struct tree	 children;

static void
parent_imsg(struct mproc *p, struct imsg *imsg)
{
	struct forward_req	*fwreq;
	struct deliver		 deliver;
	struct child		*c;
	struct msg		 m;
	const void		*data;
	const char		*username, *password, *cause;
	uint64_t		 reqid;
	size_t			 sz;
	void			*i;
	int			 fd, n, v, ret;

	if (p->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_SEND_CONFIG:
			parent_send_config_smtp();
			return;
		}
	}

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORWARD_OPEN:
			fwreq = imsg->data;
			fd = parent_forward_open(fwreq->user, fwreq->directory,
			    fwreq->uid, fwreq->gid);
			fwreq->status = 0;
			if (fd == -1 && errno != ENOENT) {
				if (errno == EAGAIN)
					fwreq->status = -1;
			}
			else
				fwreq->status = 1;
			m_compose(p, IMSG_PARENT_FORWARD_OPEN, 0, 0, fd,
			    fwreq, sizeof *fwreq);
			return;

		case IMSG_LKA_AUTHENTICATE:
			/*
			 * If we reached here, it means we want root to lookup
			 * system user.
			 */
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &username);
			m_get_string(&m, &password);
			m_end(&m);

			ret = parent_auth_user(username, password);

			m_create(p, IMSG_LKA_AUTHENTICATE, 0, 0, -1, 128);
			m_add_id(p, reqid);
			m_add_int(p, ret);
			m_close(p);
			return;
		}
	}

	if (p->proc == PROC_MDA) {
		switch (imsg->hdr.type) {
		case IMSG_PARENT_FORK_MDA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_data(&m, &data, &sz);
			m_end(&m);
			if (sz != sizeof(deliver))
				fatalx("expected deliver");
			memmove(&deliver, data, sz);
			forkmda(p, reqid, &deliver);
			return;

		case IMSG_PARENT_KILL_MDA:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_string(&m, &cause);
			m_end(&m);

			i = NULL;
			while ((n = tree_iter(&children, &i, NULL, (void**)&c)))
				if (c->type == CHILD_MDA &&
				    c->mda_id == reqid &&
				    c->cause == NULL)
					break;
			if (!n) {
				log_debug("debug: smptd: "
				    "kill request: proc not found");
				return;
			}

			c->cause = xstrdup(cause, "parent_imsg");
			log_debug("debug: smptd: kill requested for %u: %s",
			    c->pid, c->cause);
			kill(c->pid, SIGTERM);
			return;
		}
	}

	if (p->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_CTL_VERBOSE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			log_verbose(v);
			m_forward(p_lka, imsg);
			m_forward(p_mda, imsg);
			m_forward(p_mfa, imsg);
			m_forward(p_mta, imsg);
			m_forward(p_queue, imsg);
			m_forward(p_smtp, imsg);
			return;

		case IMSG_CTL_TRACE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			verbose |= v;
			log_verbose(verbose);
			parent_broadcast_verbose(verbose);
			return;

		case IMSG_CTL_UNTRACE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			verbose &= ~v;
			log_verbose(verbose);
			parent_broadcast_verbose(verbose);
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling |= v;
			parent_broadcast_profile(profiling);
			return;

		case IMSG_CTL_UNPROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling &= ~v;
			parent_broadcast_profile(profiling);
			return;

		case IMSG_CTL_SHUTDOWN:
			parent_shutdown();
			return;
		}
	}

	errx(1, "parent_imsg: unexpected %s imsg", imsg_to_str(imsg->hdr.type));
}

static void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] "
	    "[-f file] [-P system]\n", __progname);
	exit(1);
}

static void
parent_shutdown(void)
{
	void		*iter;
	struct child	*child;
	pid_t		 pid;

	iter = NULL;
	while (tree_iter(&children, &iter, NULL, (void**)&child))
		if (child->type == CHILD_DAEMON)
			kill(child->pid, SIGTERM);

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_warnx("warn: parent terminating");
	exit(0);
}

static void
parent_send_config(int fd, short event, void *p)
{
	parent_send_config_lka();
	parent_send_config_mfa();
	parent_send_config_smtp();
	purge_config(PURGE_SSL);
}

static void
parent_send_config_smtp(void)
{
	struct listener		*l;
	struct ssl		*s;
	void			*iter = NULL;
	struct iovec		 iov[5];
	int			 opt;

	log_debug("debug: parent_send_config: configuring smtp");
	m_compose(p_smtp, IMSG_CONF_START, 0, 0, -1, NULL, 0);

	while (dict_iter(env->sc_ssl_dict, &iter, NULL, (void **)&s)) {
		if (!(s->flags & F_SCERT))
			continue;
		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;
		iov[3].iov_base = s->ssl_dhparams;
		iov[3].iov_len = s->ssl_dhparams_len;
		iov[4].iov_base = s->ssl_ca;
		iov[4].iov_len = s->ssl_ca_len;
		m_composev(p_smtp, IMSG_CONF_SSL, 0, 0, -1, iov, nitems(iov));
	}

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		if ((l->fd = socket(l->ss.ss_family, SOCK_STREAM, 0)) == -1)
			fatal("smtpd: socket");
		opt = 1;
		if (setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &opt,
			sizeof(opt)) < 0)
			fatal("smtpd: setsockopt");
		if (bind(l->fd, (struct sockaddr *)&l->ss, l->ss.ss_len) == -1)
			fatal("smtpd: bind");
		m_compose(p_smtp, IMSG_CONF_LISTENER, 0, 0, l->fd,
		    l, sizeof(*l));
	}

	m_compose(p_smtp, IMSG_CONF_END, 0, 0, -1, NULL, 0);
}

void
parent_send_config_mfa()
{
	struct filter	       *f;
	void		       *iter_dict = NULL;

	log_debug("debug: parent_send_config_mfa: reloading");
	m_compose(p_mfa, IMSG_CONF_START, 0, 0, -1, NULL, 0);

	while (dict_iter(&env->sc_filters, &iter_dict, NULL, (void **)&f))
		m_compose(p_mfa, IMSG_CONF_FILTER, 0, 0, -1, f, sizeof(*f));

	m_compose(p_mfa, IMSG_CONF_END, 0, 0, -1, NULL, 0);
}

void
parent_send_config_lka()
{
	struct rule	       *r;
	struct table	       *t;
	void		       *iter_tree;
	void		       *iter_dict;
	const char	       *k;
	char		       *v;
	char		       *buffer;
	size_t			buflen;
	struct ssl	       *s;
	struct iovec		iov[5];

	log_debug("debug: parent_send_config_ruleset: reloading");
	m_compose(p_lka, IMSG_CONF_START, 0, 0, -1, NULL, 0);

	iter_dict = NULL;
	while (dict_iter(env->sc_ssl_dict, &iter_dict, NULL, (void **)&s)) {
		if (!(s->flags & F_SCERT))
			continue;
		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;
		iov[3].iov_base = s->ssl_dhparams;
		iov[3].iov_len = s->ssl_dhparams_len;
		iov[4].iov_base = s->ssl_ca;
		iov[4].iov_len = s->ssl_ca_len;
		m_composev(p_lka, IMSG_CONF_SSL, 0, 0, -1, iov, nitems(iov));
	}

	iter_tree = NULL;
	while (tree_iter(env->sc_tables_tree, &iter_tree, NULL,
		(void **)&t)) {
		m_compose(p_lka, IMSG_CONF_TABLE, 0, 0, -1, t, sizeof(*t));

		iter_dict = NULL;
		while (dict_iter(&t->t_dict, &iter_dict, &k,
			(void **)&v)) {
			buflen = strlen(k) + 1;
			if (v)
				buflen += strlen(v) + 1;
			buffer = xcalloc(1, buflen,
			    "parent_send_config_ruleset");
			memcpy(buffer, k, strlen(k) + 1);
			if (v)
				memcpy(buffer + strlen(k) + 1, v,
				    strlen(v) + 1);
			m_compose(p_lka, IMSG_CONF_TABLE_CONTENT, 0, 0, -1,
			    buffer, buflen);
			free(buffer);
		}
	}

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		m_compose(p_lka, IMSG_CONF_RULE, 0, 0, -1, r, sizeof(*r));
		m_compose(p_lka, IMSG_CONF_RULE_SOURCE, 0, 0, -1,
		    &r->r_sources->t_name,
		    sizeof(r->r_sources->t_name));
		if (r->r_senders) {
			m_compose(p_lka, IMSG_CONF_RULE_SENDER,
			    0, 0, -1,
			    &r->r_senders->t_name,
			    sizeof(r->r_senders->t_name));
		}
		if (r->r_destination) {
			m_compose(p_lka, IMSG_CONF_RULE_DESTINATION,
			    0, 0, -1,
			    &r->r_destination->t_name,
			    sizeof(r->r_destination->t_name));
		}
		if (r->r_mapping) {
			m_compose(p_lka, IMSG_CONF_RULE_MAPPING, 0, 0, -1,
			    &r->r_mapping->t_name,
			    sizeof(r->r_mapping->t_name));
		}
		if (r->r_users) {
			m_compose(p_lka, IMSG_CONF_RULE_USERS, 0, 0, -1,
			    &r->r_users->t_name,
			    sizeof(r->r_users->t_name));
		}
	}

	m_compose(p_lka, IMSG_CONF_END, 0, 0, -1, NULL, 0);
}

static void
parent_sig_handler(int sig, short event, void *p)
{
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
				fatalx("smtpd: unexpected cause of SIGCHLD");

			if (pid == purge_pid)
				purge_pid = -1;

			child = tree_pop(&children, pid);
			if (child == NULL)
				goto skip;

			switch (child->type) {
			case CHILD_DAEMON:
				die = 1;
				if (fail)
					log_warnx("warn: lost child: %s %s",
					    child->title, cause);
				break;

			case CHILD_MDA:
				if (WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGALRM) {
					free(cause);
					asprintf(&cause, "terminated; timeout");
				}
				else if (child->cause &&
				    WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGTERM) {
					free(cause);
					cause = child->cause;
					child->cause = NULL;
				}
				if (child->cause)
					free(child->cause);
				log_debug("debug: smtpd: mda process done "
				    "for session %016"PRIx64 ": %s",
				    child->mda_id, cause);
				m_create(p_mda, IMSG_MDA_DONE, 0, 0,
				    child->mda_out, 32 + strlen(cause));
				m_add_id(p_mda, child->mda_id);
				m_add_string(p_mda, cause);
				m_close(p_mda);
				/* free(cause); */
				break;

			case CHILD_ENQUEUE_OFFLINE:
				if (fail)
					log_warnx("warn: smtpd: "
					    "couldn't enqueue offline "
					    "message %s; smtpctl %s",
					    child->path, cause);
				else
					unlink(child->path);
				free(child->path);
				offline_done();
				break;

			default:
				fatalx("smtpd: unexpected child type");
			}
			free(child);
    skip:
			free(cause);
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		if (die)
			parent_shutdown();
		break;
	default:
		fatalx("smtpd: unexpected signal");
	}
}

int
main(int argc, char *argv[])
{
	int		 c, i;
	int		 opts, flags;
	const char	*conffile = CONF_FILE;
	struct smtpd	 smtpd;
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct timeval	 tv;
	struct passwd	*pwq;
	struct listener	*l;
	struct rule	*r;
	struct ssl	*ssl;

	env = &smtpd;

	flags = 0;
	opts = 0;
	debug = 0;
	verbose = 0;

	log_init(1);

	TAILQ_INIT(&offline_q);

	while ((c = getopt(argc, argv, "B:dD:nP:f:T:v")) != -1) {
		switch (c) {
		case 'B':
			if (strstr(optarg, "queue=") == optarg)
				backend_queue = strchr(optarg, '=') + 1;
			else if (strstr(optarg, "scheduler=") == optarg)
				backend_scheduler = strchr(optarg, '=') + 1;
			else if (strstr(optarg, "stat=") == optarg)
				backend_stat = strchr(optarg, '=') + 1;
			else
				log_warnx("warn: "
				    "invalid backend specifier %s",
				    optarg);
			break;
		case 'd':
			debug = 2;
			verbose |= TRACE_VERBOSE;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("warn: "
				    "could not parse macro definition %s",
				    optarg);
			break;
		case 'n':
			debug = 2;
			opts |= SMTPD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'T':
			if (!strcmp(optarg, "imsg"))
				verbose |= TRACE_IMSG;
			else if (!strcmp(optarg, "io"))
				verbose |= TRACE_IO;
			else if (!strcmp(optarg, "smtp"))
				verbose |= TRACE_SMTP;
			else if (!strcmp(optarg, "mfa"))
				verbose |= TRACE_MFA;
			else if (!strcmp(optarg, "mta"))
				verbose |= TRACE_MTA;
			else if (!strcmp(optarg, "bounce"))
				verbose |= TRACE_BOUNCE;
			else if (!strcmp(optarg, "scheduler"))
				verbose |= TRACE_SCHEDULER;
			else if (!strcmp(optarg, "lookup"))
				verbose |= TRACE_LOOKUP;
			else if (!strcmp(optarg, "stat"))
				verbose |= TRACE_STAT;
			else if (!strcmp(optarg, "rules"))
				verbose |= TRACE_RULES;
			else if (!strcmp(optarg, "imsg-size"))
				verbose |= TRACE_IMSGSIZE;
			else if (!strcmp(optarg, "all"))
				verbose |= ~TRACE_VERBOSE;
			else if (!strcmp(optarg, "profstat"))
				profiling |= PROFILE_TOSTAT;
			else if (!strcmp(optarg, "profile-imsg"))
				profiling |= PROFILE_IMSG;
			else if (!strcmp(optarg, "profile-queue"))
				profiling |= PROFILE_QUEUE;
			else
				log_warnx("warn: unknown trace flag \"%s\"",
				    optarg);
			break;
		case 'P':
			if (!strcmp(optarg, "smtp"))
				flags |= SMTPD_SMTP_PAUSED;
			else if (!strcmp(optarg, "mta"))
				flags |= SMTPD_MTA_PAUSED;
			else if (!strcmp(optarg, "mda"))
				flags |= SMTPD_MDA_PAUSED;
			break;
		case 'v':
			verbose |=  TRACE_VERBOSE;
			break;
		default:
			usage();
		}
	}

	if (!(verbose & TRACE_VERBOSE))
		verbose = 0;

	argv += optind;
	argc -= optind;

	if (argc || *argv)
		usage();

	ssl_init();

	if (parse_config(&smtpd, conffile, opts))
		exit(1);

	if (strlcpy(env->sc_conffile, conffile, MAXPATHLEN) >= MAXPATHLEN)
		errx(1, "config file exceeds MAXPATHLEN");

	if (env->sc_opts & SMTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	env->sc_flags |= flags;

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	if ((env->sc_pw = getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);
	if ((env->sc_pw = pw_dup(env->sc_pw)) == NULL)
		err(1, NULL);

	env->sc_pwqueue = getpwnam(SMTPD_QUEUE_USER);
	if (env->sc_pwqueue)
		pwq = env->sc_pwqueue = pw_dup(env->sc_pwqueue);
	else
		pwq = env->sc_pwqueue = pw_dup(env->sc_pw);
	if (env->sc_pwqueue == NULL)
		err(1, NULL);

	if (ckdir(PATH_SPOOL, 0711, 0, 0, 1) == 0)
		errx(1, "error in spool directory setup");
	if (ckdir(PATH_SPOOL PATH_OFFLINE, 01777, 0, 0, 1) == 0)
		errx(1, "error in offline directory setup");
	if (ckdir(PATH_SPOOL PATH_PURGE, 0700, pwq->pw_uid, 0, 1) == 0)
		errx(1, "error in purge directory setup");
	if (ckdir(PATH_SPOOL PATH_TEMPORARY, 0700, pwq->pw_uid, 0, 1) == 0)
		errx(1, "error in purge directory setup");

	mvpurge(PATH_SPOOL PATH_INCOMING, PATH_SPOOL PATH_PURGE);

	if (ckdir(PATH_SPOOL PATH_INCOMING, 0700, pwq->pw_uid, 0, 1) == 0)
		errx(1, "error in incoming directory setup");

	if (!queue_init(backend_queue, 1))
		errx(1, "could not initialize queue backend");

	env->sc_stat = stat_backend_lookup(backend_stat);
	if (env->sc_stat == NULL)
		errx(1, "could not find stat backend \"%s\"", backend_stat);

	log_init(debug);
	log_verbose(verbose);

	if (!debug)
		if (daemon(0, 0) == -1)
			err(1, "failed to daemonize");

	for (i = 0; i < MAX_BOUNCE_WARN; i++) {
		if (env->sc_bounce_warn[i] == 0)
			break;
		log_debug("debug: bounce warning after %s",
		    duration_to_text(env->sc_bounce_warn[i]));
	}

	log_debug("debug: using \"%s\" queue backend", backend_queue);
	log_debug("debug: using \"%s\" scheduler backend", backend_scheduler);
	log_debug("debug: using \"%s\" stat backend", backend_stat);
	log_info("info: startup%s", (debug > 1)?" [debug mode]":"");

	if (env->sc_hostname[0] == '\0')
		errx(1, "machine does not have a hostname set");
	env->sc_uptime = time(NULL);

	log_debug("debug: init server-ssl tree");
	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		if (!(l->flags & F_SSL))
			continue;
		ssl = NULL;
		if (! ssl_load_certfile(&ssl, "/etc/mail/certs",
			l->ssl_cert_name, F_SCERT))
			errx(1, "cannot load certificate: %s", l->ssl_cert_name);
		dict_set(env->sc_ssl_dict, ssl->ssl_name, ssl);
	}

	log_debug("debug: init client-ssl tree");
	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		if (r->r_action != A_RELAY && r->r_action != A_RELAYVIA)
			continue;
		if (! r->r_value.relayhost.cert[0])
			continue;
		ssl = NULL;
		if (! ssl_load_certfile(&ssl, "/etc/mail/certs",
			r->r_value.relayhost.cert, F_CCERT))
			errx(1, "cannot load certificate: %s", r->r_value.relayhost.cert);
		dict_set(env->sc_ssl_dict, ssl->ssl_name, ssl);
	}

	fork_peers();

	config_process(PROC_PARENT);

	imsg_callback = parent_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, parent_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, parent_sig_handler, NULL);
	signal_set(&ev_sigchld, SIGCHLD, parent_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, parent_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sigchld, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	config_peer(PROC_CONTROL);
	config_peer(PROC_LKA);
	config_peer(PROC_MDA);
	config_peer(PROC_MFA);
	config_peer(PROC_MTA);
	config_peer(PROC_SMTP);
	config_peer(PROC_QUEUE);
	config_done();

	evtimer_set(&env->sc_ev, parent_send_config, NULL);
	bzero(&tv, sizeof(tv));
	evtimer_add(&env->sc_ev, &tv);

	/* defer offline scanning for a second */
	evtimer_set(&offline_ev, offline_scan, NULL);
	offline_timeout.tv_sec = 1;
	offline_timeout.tv_usec = 0;
	evtimer_add(&offline_ev, &offline_timeout);

	purge_pid = -1;
	evtimer_set(&purge_ev, purge_task, NULL);
	purge_timeout.tv_sec = 10;
	purge_timeout.tv_usec = 0;
	evtimer_add(&purge_ev, &purge_timeout);

	if (event_dispatch() < 0)
		fatal("smtpd: event_dispatch");

	return (0);
}

static void
fork_peers(void)
{
	tree_init(&children);

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

	init_pipes();

	child_add(control(), CHILD_DAEMON, proc_title(PROC_CONTROL));
	child_add(lka(), CHILD_DAEMON, proc_title(PROC_LKA));
	child_add(mda(), CHILD_DAEMON, proc_title(PROC_MDA));
	child_add(mfa(), CHILD_DAEMON, proc_title(PROC_MFA));
	child_add(mta(), CHILD_DAEMON, proc_title(PROC_MTA));
	child_add(queue(), CHILD_DAEMON, proc_title(PROC_QUEUE));
	child_add(scheduler(), CHILD_DAEMON, proc_title(PROC_SCHEDULER));
	child_add(smtp(), CHILD_DAEMON, proc_title(PROC_SMTP));
}

struct child *
child_add(pid_t pid, int type, const char *title)
{
	struct child	*child;

	if ((child = calloc(1, sizeof(*child))) == NULL)
		fatal("smtpd: child_add: calloc");

	child->pid = pid;
	child->type = type;
	child->title = title;

	tree_xset(&children, pid, child);

	return (child);
}

static void
purge_task(int fd, short ev, void *arg)
{
	DIR		*d;
	int		 n;
	uid_t		 uid;
	gid_t		 gid;

	if (purge_pid == -1) {

		n = 0;
		if ((d = opendir(PATH_SPOOL PATH_PURGE))) {
			while (readdir(d) != NULL)
				n++;
			closedir(d);
		} else
			log_warn("warn: purge_task: opendir");

		if (n > 2) {
			switch (purge_pid = fork()) {
			case -1:
				log_warn("warn: purge_task: fork");
				break;
			case 0:
				if (chroot(PATH_SPOOL PATH_PURGE) == -1)
					fatal("smtpd: chroot");
				if (chdir("/") == -1)
					fatal("smtpd: chdir");
				uid = env->sc_pw->pw_uid;
				gid = env->sc_pw->pw_gid;
				if (setgroups(1, &gid) ||
				    setresgid(gid, gid, gid) ||
				    setresuid(uid, uid, uid))
					fatal("smtpd: cannot drop privileges");
				rmtree("/", 1);
				_exit(0);
				break;
			default:
				break;
			}
		}
	}

	evtimer_add(&purge_ev, &purge_timeout);
}

static void
forkmda(struct mproc *p, uint64_t id, struct deliver *deliver)
{
	char		 ebuf[128], sfn[32];
	struct delivery_backend	*db;
	struct child	*child;
	pid_t		 pid;
	int		 n, allout, pipefd[2];
	mode_t		 omode;

	log_debug("debug: smtpd: forking mda for session %016"PRIx64
	    ": \"%s\" as %s", id, deliver->to, deliver->user);

	db = delivery_backend_lookup(deliver->mode);
	if (db == NULL)
		return;

	if (deliver->userinfo.uid == 0 && ! db->allow_root) {
		snprintf(ebuf, sizeof ebuf, "not allowed to deliver to: %s",
		    deliver->user);
		m_create(p_mda, IMSG_MDA_DONE, 0, 0, -1, 128);
		m_add_id(p_mda,	id);
		m_add_string(p_mda, ebuf);
		m_close(p_mda);
		return;
	}

	/* lower privs early to allow fork fail due to ulimit */
	if (seteuid(deliver->userinfo.uid) < 0)
		fatal("smtpd: forkmda: cannot lower privileges");

	if (pipe(pipefd) < 0) {
		n = snprintf(ebuf, sizeof ebuf, "pipe: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		m_create(p_mda, IMSG_MDA_DONE, 0, 0, -1, 128);
		m_add_id(p_mda,	id);
		m_add_string(p_mda, ebuf);
		m_close(p_mda);
		return;
	}

	/* prepare file which captures stdout and stderr */
	strlcpy(sfn, "/tmp/smtpd.out.XXXXXXXXXXX", sizeof(sfn));
	omode = umask(7077);
	allout = mkstemp(sfn);
	umask(omode);
	if (allout < 0) {
		n = snprintf(ebuf, sizeof ebuf, "mkstemp: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		m_create(p_mda, IMSG_MDA_DONE, 0, 0, -1, 128);
		m_add_id(p_mda,	id);
		m_add_string(p_mda, ebuf);
		m_close(p_mda);
		close(pipefd[0]);
		close(pipefd[1]);
		return;
	}
	unlink(sfn);

	pid = fork();
	if (pid < 0) {
		n = snprintf(ebuf, sizeof ebuf, "fork: %s", strerror(errno));
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		m_create(p_mda, IMSG_MDA_DONE, 0, 0, -1, 128);
		m_add_id(p_mda,	id);
		m_add_string(p_mda, ebuf);
		m_close(p_mda);
		close(pipefd[0]);
		close(pipefd[1]);
		close(allout);
		return;
	}

	/* parent passes the child fd over to mda */
	if (pid > 0) {
		if (seteuid(0) < 0)
			fatal("smtpd: forkmda: cannot restore privileges");
		child = child_add(pid, CHILD_MDA, NULL);
		child->mda_out = allout;
		child->mda_id = id;
		close(pipefd[0]);
		m_create(p, IMSG_PARENT_FORK_MDA, 0, 0, pipefd[1], 9);
		m_add_id(p, id);
		m_close(p);
		return;
	}

#define error(m) { perror(m); _exit(1); }
	if (seteuid(0) < 0)
		error("forkmda: cannot restore privileges");
	if (chdir(deliver->userinfo.directory) < 0 && chdir("/") < 0)
		error("chdir");
	if (dup2(pipefd[0], STDIN_FILENO) < 0 ||
	    dup2(allout, STDOUT_FILENO) < 0 ||
	    dup2(allout, STDERR_FILENO) < 0)
		error("forkmda: dup2");
	if (closefrom(STDERR_FILENO + 1) < 0)
		error("closefrom");
	if (setgroups(1, &deliver->userinfo.gid) ||
	    setresgid(deliver->userinfo.gid, deliver->userinfo.gid, deliver->userinfo.gid) ||
	    setresuid(deliver->userinfo.uid, deliver->userinfo.uid, deliver->userinfo.uid))
		error("forkmda: cannot drop privileges");
	if (setsid() < 0)
		error("setsid");
	if (signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
	    signal(SIGINT, SIG_DFL) == SIG_ERR ||
	    signal(SIGTERM, SIG_DFL) == SIG_ERR ||
	    signal(SIGCHLD, SIG_DFL) == SIG_ERR ||
	    signal(SIGHUP, SIG_DFL) == SIG_ERR)
		error("signal");

	/* avoid hangs by setting 5m timeout */
	alarm(300);

	db->open(deliver);

	error("forkmda: unknown mode");
}
#undef error

static void
offline_scan(int fd, short ev, void *arg)
{
	DIR		*dir = arg;
	struct dirent	*d;
	int		 n = 0;

	if (dir == NULL) {
		log_debug("debug: smtpd: scanning offline queue...");
		if ((dir = opendir(PATH_SPOOL PATH_OFFLINE)) == NULL)
			errx(1, "smtpd: opendir");
	}

	while ((d = readdir(dir)) != NULL) {
		if (d->d_type != DT_REG)
			continue;

		if (offline_add(d->d_name)) {
			log_warnx("warn: smtpd: "
			    "could not add offline message %s", d->d_name);
			continue;
		}

		if ((n++) == OFFLINE_READMAX) {
			evtimer_set(&offline_ev, offline_scan, dir);
			offline_timeout.tv_sec = 0;
			offline_timeout.tv_usec = 100000;
			evtimer_add(&offline_ev, &offline_timeout);
			return;
		}
	}

	log_debug("debug: smtpd: offline scanning done");
	closedir(dir);
}

static int
offline_enqueue(char *name)
{
	char		 t[MAXPATHLEN], *path;
	struct stat	 sb;
	pid_t		 pid;
	struct child	*child;
	struct passwd	*pw;

	if (!bsnprintf(t, sizeof t, "%s/%s", PATH_SPOOL PATH_OFFLINE, name)) {
		log_warnx("warn: smtpd: path name too long");
		return (-1);
	}

	if ((path = strdup(t)) == NULL) {
		log_warn("warn: smtpd: strdup");
		return (-1);
	}

	log_debug("debug: smtpd: enqueueing offline message %s", path);

	if ((pid = fork()) == -1) {
		log_warn("warn: smtpd: fork");
		free(path);
		return (-1);
	}

	if (pid == 0) {
		char	*envp[2], *p, *tmp;
		FILE	*fp;
		size_t	 len;
		arglist	 args;

		bzero(&args, sizeof(args));

		if (lstat(path, &sb) == -1) {
			log_warn("warn: smtpd: lstat: %s", path);
			_exit(1);
		}

		if (chflags(path, 0) == -1) {
			log_warn("warn: smtpd: chflags: %s", path);
			_exit(1);
		}

		pw = getpwuid(sb.st_uid);
		if (pw == NULL) {
			log_warnx("warn: smtpd: getpwuid for uid %d failed",
			    sb.st_uid);
			_exit(1);
		}

		if (! S_ISREG(sb.st_mode)) {
			log_warnx("warn: smtpd: file %s (uid %d) not regular",
			    path, sb.st_uid);
			_exit(1);
		}

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) ||
		    closefrom(STDERR_FILENO + 1) == -1)
			_exit(1);

		if ((fp = fopen(path, "r")) == NULL)
			_exit(1);

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

	offline_running++;
	child = child_add(pid, CHILD_ENQUEUE_OFFLINE, NULL);
	child->path = path;

	return (0);
}

static int
offline_add(char *path)
{
	struct offline	*q;

	if (offline_running < OFFLINE_QUEUEMAX)
		/* skip queue */
		return offline_enqueue(path);

	q = malloc(sizeof(*q) + strlen(path) + 1);
	if (q == NULL)
		return (-1);
	q->path = (char *)q + sizeof(*q);
	memmove(q->path, path, strlen(path) + 1);
	TAILQ_INSERT_TAIL(&offline_q, q, entry);

	return (0);
}

static void
offline_done(void)
{
	struct offline	*q;

	offline_running--;

	while (offline_running < OFFLINE_QUEUEMAX) {
		if ((q = TAILQ_FIRST(&offline_q)) == NULL)
			break; /* all done */
		TAILQ_REMOVE(&offline_q, q, entry);
		offline_enqueue(q->path);
		free(q);
	}
}

static int
parent_forward_open(char *username, char *directory, uid_t uid, gid_t gid)
{
	char pathname[MAXPATHLEN];
	int	fd;

	if (! bsnprintf(pathname, sizeof (pathname), "%s/.forward",
		directory))
		fatal("smtpd: parent_forward_open: snprintf");

	do {
		fd = open(pathname, O_RDONLY);
	} while (fd == -1 && errno == EINTR);
	if (fd == -1) {
		if (errno == ENOENT)
			return -1;
		if (errno == EMFILE || errno == ENFILE || errno == EIO) {
			errno = EAGAIN;
			return -1;
		}
		log_warn("warn: smtpd: parent_forward_open: %s", pathname);
		return -1;
	}

	if (! secure_file(fd, pathname, directory, uid, 1)) {
		log_warnx("warn: smtpd: %s: unsecure file", pathname);
		close(fd);
		return -1;
	}

	return fd;
}

void
imsg_dispatch(struct mproc *p, struct imsg *imsg)
{
	struct timespec		 t0, t1, dt;

	if (imsg == NULL) {
		log_warnx("warn: pipe error with %s", p->name);
		exit(1);
		return;
	}

	log_imsg(smtpd_process, p->proc, imsg);

	if (profiling & PROFILE_IMSG)
		clock_gettime(CLOCK_MONOTONIC, &t0);

	imsg_callback(p, imsg);

	if (profiling & PROFILE_IMSG) {
		clock_gettime(CLOCK_MONOTONIC, &t1);
		timespecsub(&t1, &t0, &dt);

		log_debug("profile-imsg: %s %s %s %i %li.%06li",
		    proc_name(smtpd_process),
		    proc_name(p->proc),
		    imsg_to_str(imsg->hdr.type),
		    (int)imsg->hdr.len,
		    dt.tv_sec * 1000000 + dt.tv_nsec / 1000000,
		    dt.tv_nsec % 1000000);

		if (profiling & PROFILE_TOSTAT) {
			char	key[STAT_KEY_SIZE];
			/* can't profstat control process yet */
			if (smtpd_process == PROC_CONTROL)
				return;

			if (! bsnprintf(key, sizeof key,
				"profiling.imsg.%s.%s.%s",
				proc_name(smtpd_process),
				proc_name(p->proc),
				imsg_to_str(imsg->hdr.type)));
			stat_set(key, stat_timespec(&dt));
		}
	}
}

static void
log_imsg(int to, int from, struct imsg *imsg)
{

	if (to == PROC_CONTROL)
		return;

	if (imsg->fd != -1)
		log_trace(TRACE_IMSG, "imsg: %s <- %s: %s (len=%zu, fd=%i)",
		    proc_name(to),
		    proc_name(from),
		    imsg_to_str(imsg->hdr.type),
		    imsg->hdr.len - IMSG_HEADER_SIZE,
		    imsg->fd);
	else
		log_trace(TRACE_IMSG, "imsg: %s <- %s: %s (len=%zu)",
		    proc_name(to),
		    proc_name(from),
		    imsg_to_str(imsg->hdr.type),
		    imsg->hdr.len - IMSG_HEADER_SIZE);
}

const char *
proc_title(enum smtp_proc_type proc)
{
	switch (proc) {
	case PROC_CONTROL:
		return "control";
	case PROC_LKA:
		return "lookup";
	case PROC_MDA:
		return "delivery";
	case PROC_MFA:
		return "filter";
	case PROC_MTA:
		return "transfer";
	case PROC_PARENT:
		return "[priv]";
	case PROC_QUEUE:
		return "queue";
	case PROC_SCHEDULER:
		return "scheduler";
	case PROC_SMTP:
		return "smtp";
	default:
		return "unknown";
	}
}

const char *
proc_name(enum smtp_proc_type proc)
{
	switch (proc) {
	case PROC_PARENT:
		return "parent";
	case PROC_SMTP:
		return "smtp";
	case PROC_MFA:
		return "mfa";
	case PROC_LKA:
		return "lka";
	case PROC_QUEUE:
		return "queue";
	case PROC_MDA:
		return "mda";
	case PROC_MTA:
		return "mta";
	case PROC_CONTROL:
		return "control";
	case PROC_SCHEDULER:
		return "scheduler";
	default:
		return "unknown";
	}
}

#define CASE(x) case x : return #x

const char *
imsg_to_str(int type)
{
	static char	 buf[32];

	switch (type) {
	CASE(IMSG_NONE);
	CASE(IMSG_CTL_OK);
	CASE(IMSG_CTL_FAIL);
	CASE(IMSG_CTL_SHUTDOWN);
	CASE(IMSG_CTL_VERBOSE);
	CASE(IMSG_CTL_PAUSE_MDA);
	CASE(IMSG_CTL_PAUSE_MTA);
	CASE(IMSG_CTL_PAUSE_SMTP);
	CASE(IMSG_CTL_RESUME_MDA);
	CASE(IMSG_CTL_RESUME_MTA);
	CASE(IMSG_CTL_RESUME_SMTP);
	CASE(IMSG_CTL_LIST_MESSAGES);
	CASE(IMSG_CTL_LIST_ENVELOPES);
	CASE(IMSG_CTL_REMOVE);
	CASE(IMSG_CTL_SCHEDULE);

	CASE(IMSG_CTL_TRACE);
	CASE(IMSG_CTL_UNTRACE);
	CASE(IMSG_CTL_PROFILE);
	CASE(IMSG_CTL_UNPROFILE);

	CASE(IMSG_CONF_START);
	CASE(IMSG_CONF_SSL);
	CASE(IMSG_CONF_LISTENER);
	CASE(IMSG_CONF_TABLE);
	CASE(IMSG_CONF_TABLE_CONTENT);
	CASE(IMSG_CONF_RULE);
	CASE(IMSG_CONF_RULE_SOURCE);
	CASE(IMSG_CONF_RULE_SENDER);
	CASE(IMSG_CONF_RULE_DESTINATION);
	CASE(IMSG_CONF_RULE_MAPPING);
	CASE(IMSG_CONF_RULE_USERS);
	CASE(IMSG_CONF_FILTER);
	CASE(IMSG_CONF_END);

	CASE(IMSG_LKA_UPDATE_TABLE);
	CASE(IMSG_LKA_EXPAND_RCPT);
	CASE(IMSG_LKA_SECRET);
	CASE(IMSG_LKA_SOURCE);
	CASE(IMSG_LKA_HELO);
	CASE(IMSG_LKA_USERINFO);
	CASE(IMSG_LKA_AUTHENTICATE);
	CASE(IMSG_LKA_SSL_INIT);
	CASE(IMSG_LKA_SSL_VERIFY_CERT);
	CASE(IMSG_LKA_SSL_VERIFY_CHAIN);
	CASE(IMSG_LKA_SSL_VERIFY);

	CASE(IMSG_DELIVERY_OK);
	CASE(IMSG_DELIVERY_TEMPFAIL);
	CASE(IMSG_DELIVERY_PERMFAIL);
	CASE(IMSG_DELIVERY_LOOP);

	CASE(IMSG_BOUNCE_INJECT);

	CASE(IMSG_MDA_DELIVER);
	CASE(IMSG_MDA_DONE);

	CASE(IMSG_MFA_REQ_CONNECT);
	CASE(IMSG_MFA_REQ_HELO);
	CASE(IMSG_MFA_REQ_MAIL);
	CASE(IMSG_MFA_REQ_RCPT);
	CASE(IMSG_MFA_REQ_DATA);
	CASE(IMSG_MFA_REQ_EOM);
	CASE(IMSG_MFA_EVENT_RSET);
	CASE(IMSG_MFA_EVENT_COMMIT);
	CASE(IMSG_MFA_EVENT_ROLLBACK);
	CASE(IMSG_MFA_EVENT_DISCONNECT);
	CASE(IMSG_MFA_SMTP_DATA);
	CASE(IMSG_MFA_SMTP_RESPONSE);

	CASE(IMSG_MTA_BATCH);
	CASE(IMSG_MTA_BATCH_ADD);
	CASE(IMSG_MTA_BATCH_END);

	CASE(IMSG_QUEUE_CREATE_MESSAGE);
	CASE(IMSG_QUEUE_SUBMIT_ENVELOPE);
	CASE(IMSG_QUEUE_COMMIT_ENVELOPES);
	CASE(IMSG_QUEUE_REMOVE_MESSAGE);
	CASE(IMSG_QUEUE_COMMIT_MESSAGE);
	CASE(IMSG_QUEUE_MESSAGE_FD);
	CASE(IMSG_QUEUE_MESSAGE_FILE);
	CASE(IMSG_QUEUE_REMOVE);
	CASE(IMSG_QUEUE_EXPIRE);
	CASE(IMSG_QUEUE_BOUNCE);

	CASE(IMSG_PARENT_FORWARD_OPEN);
	CASE(IMSG_PARENT_FORK_MDA);
	CASE(IMSG_PARENT_KILL_MDA);
	CASE(IMSG_PARENT_SEND_CONFIG);

	CASE(IMSG_SMTP_ENQUEUE_FD);

	CASE(IMSG_DNS_HOST);
	CASE(IMSG_DNS_HOST_END);
	CASE(IMSG_DNS_PTR);
	CASE(IMSG_DNS_MX);
	CASE(IMSG_DNS_MX_PREFERENCE);

	CASE(IMSG_STAT_INCREMENT);
	CASE(IMSG_STAT_DECREMENT);
	CASE(IMSG_STAT_SET);

	CASE(IMSG_DIGEST);
	CASE(IMSG_STATS);
	CASE(IMSG_STATS_GET);

	default:
		snprintf(buf, sizeof(buf), "IMSG_??? (%d)", type);

		return buf;
	}
}

int
parent_auth_user(const char *username, const char *password)
{
	char	user[MAXLOGNAME];
	char	pass[MAX_LINE_SIZE + 1];
	int	ret;

	strlcpy(user, username, sizeof(user));
	strlcpy(pass, password, sizeof(pass));

	ret = auth_userokay(user, NULL, "auth-smtp", pass);
	if (ret)
		return LKA_OK;
	return LKA_PERMFAIL;
}

static void
parent_broadcast_verbose(uint32_t v)
{
	m_create(p_lka, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_lka, v);
	m_close(p_lka);
	
	m_create(p_mda, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_mda, v);
	m_close(p_mda);
	
	m_create(p_mfa, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_mfa, v);
	m_close(p_mfa);
	
	m_create(p_mta, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_mta, v);
	m_close(p_mta);
	
	m_create(p_queue, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_queue, v);
	m_close(p_queue);
	
	m_create(p_smtp, IMSG_CTL_VERBOSE, 0, 0, -1, sizeof v);
	m_add_int(p_smtp, v);
	m_close(p_smtp);
}

static void
parent_broadcast_profile(uint32_t v)
{
	m_create(p_lka, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_lka, v);
	m_close(p_lka);
	
	m_create(p_mda, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_mda, v);
	m_close(p_mda);
	
	m_create(p_mfa, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_mfa, v);
	m_close(p_mfa);
	
	m_create(p_mta, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_mta, v);
	m_close(p_mta);
	
	m_create(p_queue, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_queue, v);
	m_close(p_queue);
	
	m_create(p_smtp, IMSG_CTL_PROFILE, 0, 0, -1, sizeof v);
	m_add_int(p_smtp, v);
	m_close(p_smtp);
}
