/*	$OpenBSD: smtpd.c,v 1.238 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

static void parent_imsg(struct mproc *, struct imsg *);
static void usage(void);
static void parent_shutdown(int);
static void parent_send_config(int, short, void *);
static void parent_send_config_lka(void);
static void parent_send_config_pony(void);
static void parent_send_config_ca(void);
static void parent_sig_handler(int, short, void *);
static void forkmda(struct mproc *, uint64_t, struct deliver *);
static int parent_forward_open(char *, char *, uid_t, gid_t);
static void fork_peers(void);
static struct child *child_add(pid_t, int, const char *);

static void	offline_scan(int, short, void *);
static int	offline_add(char *);
static void	offline_done(void);
static int	offline_enqueue(char *);

static void	purge_task(void);
static int	parent_auth_user(const char *, const char *);
static void	load_pki_tree(void);
static void	load_pki_keys(void);

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

static struct event		config_ev;
static struct event		offline_ev;
static struct timeval		offline_timeout;

static pid_t			purge_pid = -1;

extern char	**environ;
void		(*imsg_callback)(struct mproc *, struct imsg *);

enum smtp_proc_type	smtpd_process;

struct smtpd	*env = NULL;

struct mproc	*p_control = NULL;
struct mproc	*p_lka = NULL;
struct mproc	*p_parent = NULL;
struct mproc	*p_queue = NULL;
struct mproc	*p_scheduler = NULL;
struct mproc	*p_pony = NULL;
struct mproc	*p_ca = NULL;

const char	*backend_queue = "fs";
const char	*backend_scheduler = "ramqueue";
const char	*backend_stat = "ram";

int	profiling = 0;
int	verbose = 0;
int	debug = 0;
int	foreground = 0;
int	control_socket = -1;

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

	if (p->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_LKA_OPEN_FORWARD:
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
			m_compose(p, IMSG_LKA_OPEN_FORWARD, 0, 0, fd,
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

			m_create(p, IMSG_LKA_AUTHENTICATE, 0, 0, -1);
			m_add_id(p, reqid);
			m_add_int(p, ret);
			m_close(p);
			return;
		}
	}

	if (p->proc == PROC_PONY) {
		switch (imsg->hdr.type) {
		case IMSG_MDA_FORK:
			m_msg(&m, imsg);
			m_get_id(&m, &reqid);
			m_get_data(&m, &data, &sz);
			m_end(&m);
			if (sz != sizeof(deliver))
				fatalx("expected deliver");
			memmove(&deliver, data, sz);
			forkmda(p, reqid, &deliver);
			return;

		case IMSG_MDA_KILL:
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
				log_debug("debug: smtpd: "
				    "kill request: proc not found");
				return;
			}

			c->cause = xstrdup(cause, "parent_imsg");
			log_debug("debug: smtpd: kill requested for %u: %s",
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
			return;

		case IMSG_CTL_PROFILE:
			m_msg(&m, imsg);
			m_get_int(&m, &v);
			m_end(&m);
			profiling = v;
			return;

		case IMSG_CTL_SHUTDOWN:
			parent_shutdown(0);
			return;
		}
	}

	errx(1, "parent_imsg: unexpected %s imsg from %s",
	    imsg_to_str(imsg->hdr.type), proc_title(p->proc));
}

static void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dhnv] [-D macro=value] "
	    "[-f file] [-P system] [-T trace]\n", __progname);
	exit(1);
}

static void
parent_shutdown(int ret)
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

	unlink(SMTPD_SOCKET);

	log_warnx("warn: parent terminating");
	exit(ret);
}

static void
parent_send_config(int fd, short event, void *p)
{
	parent_send_config_lka();
	parent_send_config_pony();
	parent_send_config_ca();
	purge_config(PURGE_PKI);
}

static void
parent_send_config_pony(void)
{
	log_debug("debug: parent_send_config: configuring pony process");
	m_compose(p_pony, IMSG_CONF_START, 0, 0, -1, NULL, 0);
	m_compose(p_pony, IMSG_CONF_END, 0, 0, -1, NULL, 0);
}

void
parent_send_config_lka()
{
	log_debug("debug: parent_send_config_ruleset: reloading");
	m_compose(p_lka, IMSG_CONF_START, 0, 0, -1, NULL, 0);
	m_compose(p_lka, IMSG_CONF_END, 0, 0, -1, NULL, 0);
}

static void
parent_send_config_ca(void)
{
	log_debug("debug: parent_send_config: configuring ca process");
	m_compose(p_ca, IMSG_CONF_START, 0, 0, -1, NULL, 0);
	m_compose(p_ca, IMSG_CONF_END, 0, 0, -1, NULL, 0);
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
				m_create(p_pony, IMSG_MDA_DONE, 0, 0,
				    child->mda_out);
				m_add_id(p_pony, child->mda_id);
				m_add_string(p_pony, cause);
				m_close(p_pony);
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
			parent_shutdown(1);
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

	env = &smtpd;

	flags = 0;
	opts = 0;
	debug = 0;
	verbose = 0;

	log_init(1);

	TAILQ_INIT(&offline_q);

	while ((c = getopt(argc, argv, "B:dD:hnP:f:T:v")) != -1) {
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
			foreground = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("warn: "
				    "could not parse macro definition %s",
				    optarg);
			break;
		case 'h':
			log_info("version: OpenSMTPD " SMTPD_VERSION);
			usage();
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
			else if (!strcmp(optarg, "mfa") ||
			    !strcmp(optarg, "filter") ||
			    !strcmp(optarg, "filters"))
				verbose |= TRACE_FILTERS;
			else if (!strcmp(optarg, "mta") ||
			    !strcmp(optarg, "transfer"))
				verbose |= TRACE_MTA;
			else if (!strcmp(optarg, "bounce") ||
			    !strcmp(optarg, "bounces"))
				verbose |= TRACE_BOUNCE;
			else if (!strcmp(optarg, "scheduler"))
				verbose |= TRACE_SCHEDULER;
			else if (!strcmp(optarg, "lookup"))
				verbose |= TRACE_LOOKUP;
			else if (!strcmp(optarg, "stat") ||
			    !strcmp(optarg, "stats"))
				verbose |= TRACE_STAT;
			else if (!strcmp(optarg, "rules"))
				verbose |= TRACE_RULES;
			else if (!strcmp(optarg, "mproc"))
				verbose |= TRACE_MPROC;
			else if (!strcmp(optarg, "expand"))
				verbose |= TRACE_EXPAND;
			else if (!strcmp(optarg, "table") ||
			    !strcmp(optarg, "tables"))
				verbose |= TRACE_TABLES;
			else if (!strcmp(optarg, "queue"))
				verbose |= TRACE_QUEUE;
			else if (!strcmp(optarg, "all"))
				verbose |= ~TRACE_DEBUG;
			else if (!strcmp(optarg, "profstat"))
				profiling |= PROFILE_TOSTAT;
			else if (!strcmp(optarg, "profile-imsg"))
				profiling |= PROFILE_IMSG;
			else if (!strcmp(optarg, "profile-queue"))
				profiling |= PROFILE_QUEUE;
			else if (!strcmp(optarg, "profile-buffers"))
				profiling |= PROFILE_BUFFERS;
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
			verbose |=  TRACE_DEBUG;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	if (argc || *argv)
		usage();

	ssl_init();

	if (parse_config(&smtpd, conffile, opts))
		exit(1);

	if (strlcpy(env->sc_conffile, conffile, PATH_MAX)
	    >= PATH_MAX)
		errx(1, "config file exceeds PATH_MAX");

	if (env->sc_opts & SMTPD_OPT_NOACTION) {
		load_pki_tree();
		load_pki_keys();
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	env->sc_flags |= flags;

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	/* the control socket ensures that only one smtpd instance is running */
	control_socket = control_create_socket();

	if (!queue_init(backend_queue, 1))
		errx(1, "could not initialize queue backend");

	env->sc_stat = stat_backend_lookup(backend_stat);
	if (env->sc_stat == NULL)
		errx(1, "could not find stat backend \"%s\"", backend_stat);

	if (env->sc_queue_flags & QUEUE_ENCRYPTION) {
		if (env->sc_queue_key == NULL) {
			char	*password;

			password = getpass("queue key: ");
			if (password == NULL)
				err(1, "getpass");

			env->sc_queue_key = strdup(password);
			explicit_bzero(password, strlen(password));
			if (env->sc_queue_key == NULL)
				err(1, "strdup");
		}
		else {
			char   *buf;
			char   *lbuf;
			size_t	len;

			if (strcasecmp(env->sc_queue_key, "stdin") == 0) {
				lbuf = NULL;
				buf = fgetln(stdin, &len);
				if (buf[len - 1] == '\n') {
					lbuf = calloc(len, 1);
					if (lbuf == NULL)
						err(1, "calloc");
					memcpy(lbuf, buf, len-1);
				}
				else {
					lbuf = calloc(len+1, 1);
					if (lbuf == NULL)
						err(1, "calloc");
					memcpy(lbuf, buf, len);
				}
				env->sc_queue_key = lbuf;
			}
		}
	}

	if (env->sc_queue_flags & QUEUE_COMPRESSION)
		env->sc_comp = compress_backend_lookup("gzip");

	log_init(foreground);
	log_verbose(verbose);

	load_pki_tree();

	log_info("info: %s %s starting", SMTPD_NAME, SMTPD_VERSION);

	if (! foreground)
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
	log_info("info: startup%s", (verbose & TRACE_DEBUG)?" [debug mode]":"");

	if (env->sc_hostname[0] == '\0')
		errx(1, "machine does not have a hostname set");
	env->sc_uptime = time(NULL);

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
	config_peer(PROC_QUEUE);
	config_peer(PROC_CA);
	config_peer(PROC_PONY);
	config_done();

	evtimer_set(&config_ev, parent_send_config, NULL);
	memset(&tv, 0, sizeof(tv));
	evtimer_add(&config_ev, &tv);

	/* defer offline scanning for a second */
	evtimer_set(&offline_ev, offline_scan, NULL);
	offline_timeout.tv_sec = 1;
	offline_timeout.tv_usec = 0;
	evtimer_add(&offline_ev, &offline_timeout);

	if (pidfile(NULL) < 0)
		err(1, "pidfile");

	purge_task();

	if (event_dispatch() < 0)
		fatal("smtpd: event_dispatch");

	return (0);
}

static void
load_pki_tree(void)
{
	struct pki	*pki;
	const char	*k;
	void		*iter_dict;

	log_debug("debug: init ssl-tree");
	iter_dict = NULL;
	while (dict_iter(env->sc_pki_dict, &iter_dict, &k, (void **)&pki)) {
		log_debug("info: loading pki information for %s", k);
		if (pki->pki_cert_file == NULL)
			fatalx("load_pki_tree: missing certificate file");
		if (pki->pki_key_file == NULL)
			fatalx("load_pki_tree: missing key file");

		if (! ssl_load_certificate(pki, pki->pki_cert_file))
			fatalx("load_pki_tree: failed to load certificate file");

		if (pki->pki_ca_file)
			if (! ssl_load_cafile(pki, pki->pki_ca_file))
				fatalx("load_pki_tree: failed to load CA file");
		if (pki->pki_dhparams_file)
			if (! ssl_load_dhparams(pki, pki->pki_dhparams_file))
				fatalx("load_pki_tree: failed to load dhparams file");
	}
}

void
load_pki_keys(void)
{
	struct pki	*pki;
	const char	*k;
	void		*iter_dict;

	log_debug("debug: init ssl-tree");
	iter_dict = NULL;
	while (dict_iter(env->sc_pki_dict, &iter_dict, &k, (void **)&pki)) {
		log_debug("info: loading pki keys for %s", k);

		if (! ssl_load_keyfile(pki, pki->pki_key_file, k))
			fatalx("load_pki_keys: failed to load key file");
	}
}

static void
fork_peers(void)
{
	tree_init(&children);

	init_pipes();

	child_add(queue(), CHILD_DAEMON, proc_title(PROC_QUEUE));
	child_add(control(), CHILD_DAEMON, proc_title(PROC_CONTROL));
	child_add(lka(), CHILD_DAEMON, proc_title(PROC_LKA));
	child_add(scheduler(), CHILD_DAEMON, proc_title(PROC_SCHEDULER));
	child_add(pony(), CHILD_DAEMON, proc_title(PROC_PONY));
	child_add(ca(), CHILD_DAEMON, proc_title(PROC_CA));
	post_fork(PROC_PARENT);
}

void
post_fork(int proc)
{
	if (proc != PROC_QUEUE && env->sc_queue_key)
		explicit_bzero(env->sc_queue_key, strlen(env->sc_queue_key));

	if (proc != PROC_CONTROL) {
		close(control_socket);
		control_socket = -1;
	}

	if (proc == PROC_CA) {
		load_pki_keys();
	}
}

int
fork_proc_backend(const char *key, const char *conf, const char *procname)
{
	pid_t		pid;
	int		sp[2];
	char		path[PATH_MAX];
	char		name[PATH_MAX];
	char		*arg;

	if (strlcpy(name, conf, sizeof(name)) >= sizeof(name)) {
		log_warnx("warn: %s-proc: conf too long", key);
		return (0);
	}

	arg = strchr(name, ':');
	if (arg)
		*arg++ = '\0';

	if (snprintf(path, sizeof(path), PATH_LIBEXEC "/%s-%s", key, name) >=
	    (ssize_t)sizeof(path)) {
		log_warn("warn: %s-proc: exec path too long", key);
		return (-1);
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) == -1) {
		log_warn("warn: %s-proc: socketpair", key);
		return (-1);
	}

	if ((pid = fork()) == -1) {
		log_warn("warn: %s-proc: fork", key);
		close(sp[0]);
		close(sp[1]);
		return (-1);
	}

	if (pid == 0) {
		/* child process */
		dup2(sp[0], STDIN_FILENO);
		if (closefrom(STDERR_FILENO + 1) < 0)
			exit(1);

		if (procname == NULL)
			procname = name;

		execl(path, procname, arg, NULL);
		err(1, "execl: %s", path);
	}

	/* parent process */
	close(sp[0]);

	return (sp[1]);
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
purge_task(void)
{
	struct passwd	*pw;
	DIR		*d;
	int		 n;
	uid_t		 uid;
	gid_t		 gid;

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
			if ((pw = getpwnam(SMTPD_USER)) == NULL)
				fatalx("unknown user " SMTPD_USER);
			if (chroot(PATH_SPOOL PATH_PURGE) == -1)
				fatal("smtpd: chroot");
			if (chdir("/") == -1)
				fatal("smtpd: chdir");
			uid = pw->pw_uid;
			gid = pw->pw_gid;
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

static void
forkmda(struct mproc *p, uint64_t id, struct deliver *deliver)
{
	char		 ebuf[128], sfn[32];
	struct delivery_backend	*db;
	struct child	*child;
	pid_t		 pid;
	int		 allout, pipefd[2];
	mode_t		 omode;

	log_debug("debug: smtpd: forking mda for session %016"PRIx64
	    ": \"%s\" as %s", id, deliver->to, deliver->user);

	db = delivery_backend_lookup(deliver->mode);
	if (db == NULL) {
		(void)snprintf(ebuf, sizeof ebuf, "could not find delivery backend");
		m_create(p_pony, IMSG_MDA_DONE, 0, 0, -1);
		m_add_id(p_pony, id);
		m_add_string(p_pony, ebuf);
		m_close(p_pony);
		return;
	}

	if (deliver->userinfo.uid == 0 && ! db->allow_root) {
		(void)snprintf(ebuf, sizeof ebuf, "not allowed to deliver to: %s",
		    deliver->user);
		m_create(p_pony, IMSG_MDA_DONE, 0, 0, -1);
		m_add_id(p_pony, id);
		m_add_string(p_pony, ebuf);
		m_close(p_pony);
		return;
	}

	if (pipe(pipefd) < 0) {
		(void)snprintf(ebuf, sizeof ebuf, "pipe: %s", strerror(errno));
		m_create(p_pony, IMSG_MDA_DONE, 0, 0, -1);
		m_add_id(p_pony, id);
		m_add_string(p_pony, ebuf);
		m_close(p_pony);
		return;
	}

	/* prepare file which captures stdout and stderr */
	(void)strlcpy(sfn, "/tmp/smtpd.out.XXXXXXXXXXX", sizeof(sfn));
	omode = umask(7077);
	allout = mkstemp(sfn);
	umask(omode);
	if (allout < 0) {
		(void)snprintf(ebuf, sizeof ebuf, "mkstemp: %s", strerror(errno));
		m_create(p_pony, IMSG_MDA_DONE, 0, 0, -1);
		m_add_id(p_pony, id);
		m_add_string(p_pony, ebuf);
		m_close(p_pony);
		close(pipefd[0]);
		close(pipefd[1]);
		return;
	}
	unlink(sfn);

	pid = fork();
	if (pid < 0) {
		(void)snprintf(ebuf, sizeof ebuf, "fork: %s", strerror(errno));
		m_create(p_pony, IMSG_MDA_DONE, 0, 0, -1);
		m_add_id(p_pony, id);
		m_add_string(p_pony, ebuf);
		m_close(p_pony);
		close(pipefd[0]);
		close(pipefd[1]);
		close(allout);
		return;
	}

	/* parent passes the child fd over to mda */
	if (pid > 0) {
		child = child_add(pid, CHILD_MDA, NULL);
		child->mda_out = allout;
		child->mda_id = id;
		close(pipefd[0]);
		m_create(p, IMSG_MDA_FORK, 0, 0, pipefd[1]);
		m_add_id(p, id);
		m_close(p);
		return;
	}

	if (chdir(deliver->userinfo.directory) < 0 && chdir("/") < 0)
		err(1, "chdir");
	if (setgroups(1, &deliver->userinfo.gid) ||
	    setresgid(deliver->userinfo.gid, deliver->userinfo.gid, deliver->userinfo.gid) ||
	    setresuid(deliver->userinfo.uid, deliver->userinfo.uid, deliver->userinfo.uid))
		err(1, "forkmda: cannot drop privileges");
	if (dup2(pipefd[0], STDIN_FILENO) < 0 ||
	    dup2(allout, STDOUT_FILENO) < 0 ||
	    dup2(allout, STDERR_FILENO) < 0)
		err(1, "forkmda: dup2");
	if (closefrom(STDERR_FILENO + 1) < 0)
		err(1, "closefrom");
	if (setsid() < 0)
		err(1, "setsid");
	if (signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
	    signal(SIGINT, SIG_DFL) == SIG_ERR ||
	    signal(SIGTERM, SIG_DFL) == SIG_ERR ||
	    signal(SIGCHLD, SIG_DFL) == SIG_ERR ||
	    signal(SIGHUP, SIG_DFL) == SIG_ERR)
		err(1, "signal");

	/* avoid hangs by setting 5m timeout */
	alarm(300);

	db->open(deliver);
}

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
	char		 t[PATH_MAX], *path;
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

		memset(&args, 0, sizeof(args));

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
		addargs(&args, "%s", "-S");

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
	char		pathname[PATH_MAX];
	int		fd;
	struct stat	sb;

	if (! bsnprintf(pathname, sizeof (pathname), "%s/.forward",
		directory))
		fatal("smtpd: parent_forward_open: snprintf");

	if (stat(directory, &sb) < 0) {
		log_warn("warn: smtpd: parent_forward_open: %s", directory);
		return -1;
	}
	if (sb.st_mode & S_ISVTX) {
		log_warnx("warn: smtpd: parent_forward_open: %s is sticky",
		    directory);
		errno = EAGAIN;
		return -1;
	}

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
	struct timespec	t0, t1, dt;
	int		msg;

	if (imsg == NULL) {
		exit(1);
		return;
	}

	log_imsg(smtpd_process, p->proc, imsg);

	if (profiling & PROFILE_IMSG)
		clock_gettime(CLOCK_MONOTONIC, &t0);

	msg = imsg->hdr.type;
	imsg_callback(p, imsg);

	if (profiling & PROFILE_IMSG) {
		clock_gettime(CLOCK_MONOTONIC, &t1);
		timespecsub(&t1, &t0, &dt);

		log_debug("profile-imsg: %s %s %s %d %lld.%09ld",
		    proc_name(smtpd_process),
		    proc_name(p->proc),
		    imsg_to_str(msg),
		    (int)imsg->hdr.len,
		    (long long)dt.tv_sec,
		    dt.tv_nsec);

		if (profiling & PROFILE_TOSTAT) {
			char	key[STAT_KEY_SIZE];
			/* can't profstat control process yet */
			if (smtpd_process == PROC_CONTROL)
				return;

			if (! bsnprintf(key, sizeof key,
				"profiling.imsg.%s.%s.%s",
				proc_name(smtpd_process),
				proc_name(p->proc),
				imsg_to_str(msg)))
				return;
			stat_set(key, stat_timespec(&dt));
		}
	}
}

void
log_imsg(int to, int from, struct imsg *imsg)
{

	if (to == PROC_CONTROL && imsg->hdr.type == IMSG_STAT_SET)
		return;

	if (imsg->fd != -1)
		log_trace(TRACE_IMSG, "imsg: %s <- %s: %s (len=%zu, fd=%d)",
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
	case PROC_PARENT:
		return "[priv]";
	case PROC_LKA:
		return "lookup";
	case PROC_QUEUE:
		return "queue";
	case PROC_CONTROL:
		return "control";
	case PROC_SCHEDULER:
		return "scheduler";
	case PROC_PONY:
		return "pony express";
	case PROC_CA:
		return "klondike";
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
	case PROC_LKA:
		return "lka";
	case PROC_QUEUE:
		return "queue";
	case PROC_CONTROL:
		return "control";
	case PROC_SCHEDULER:
		return "scheduler";
	case PROC_PONY:
		return "pony";
	case PROC_CA:
		return "ca";
	case PROC_FILTER:
		return "filter-proc";
	case PROC_CLIENT:
		return "client-proc";
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

	CASE(IMSG_CTL_GET_DIGEST);
	CASE(IMSG_CTL_GET_STATS);
	CASE(IMSG_CTL_LIST_MESSAGES);
	CASE(IMSG_CTL_LIST_ENVELOPES);
	CASE(IMSG_CTL_MTA_SHOW_HOSTS);
	CASE(IMSG_CTL_MTA_SHOW_RELAYS);
	CASE(IMSG_CTL_MTA_SHOW_ROUTES);
	CASE(IMSG_CTL_MTA_SHOW_HOSTSTATS);
	CASE(IMSG_CTL_MTA_BLOCK);
	CASE(IMSG_CTL_MTA_UNBLOCK);
	CASE(IMSG_CTL_MTA_SHOW_BLOCK);
	CASE(IMSG_CTL_PAUSE_EVP);
	CASE(IMSG_CTL_PAUSE_MDA);
	CASE(IMSG_CTL_PAUSE_MTA);
	CASE(IMSG_CTL_PAUSE_SMTP);
	CASE(IMSG_CTL_PROFILE);
	CASE(IMSG_CTL_PROFILE_DISABLE);
	CASE(IMSG_CTL_PROFILE_ENABLE);
	CASE(IMSG_CTL_RESUME_EVP);
	CASE(IMSG_CTL_RESUME_MDA);
	CASE(IMSG_CTL_RESUME_MTA);
	CASE(IMSG_CTL_RESUME_SMTP);
	CASE(IMSG_CTL_RESUME_ROUTE);
	CASE(IMSG_CTL_REMOVE);
	CASE(IMSG_CTL_SCHEDULE);
	CASE(IMSG_CTL_SHOW_STATUS);
	CASE(IMSG_CTL_SHUTDOWN);
	CASE(IMSG_CTL_TRACE_DISABLE);
	CASE(IMSG_CTL_TRACE_ENABLE);
	CASE(IMSG_CTL_UPDATE_TABLE);
	CASE(IMSG_CTL_VERBOSE);

	CASE(IMSG_CTL_SMTP_SESSION);

	CASE(IMSG_CONF_START);
	CASE(IMSG_CONF_END);

	CASE(IMSG_STAT_INCREMENT);
	CASE(IMSG_STAT_DECREMENT);
	CASE(IMSG_STAT_SET);

	CASE(IMSG_LKA_AUTHENTICATE);
	CASE(IMSG_LKA_OPEN_FORWARD);
	CASE(IMSG_LKA_ENVELOPE_SUBMIT);
	CASE(IMSG_LKA_ENVELOPE_COMMIT);

	CASE(IMSG_QUEUE_DELIVER);
	CASE(IMSG_QUEUE_DELIVERY_OK);
	CASE(IMSG_QUEUE_DELIVERY_TEMPFAIL);
	CASE(IMSG_QUEUE_DELIVERY_PERMFAIL);
	CASE(IMSG_QUEUE_DELIVERY_LOOP);
	CASE(IMSG_QUEUE_ENVELOPE_ACK);
	CASE(IMSG_QUEUE_ENVELOPE_COMMIT);
	CASE(IMSG_QUEUE_ENVELOPE_REMOVE);
	CASE(IMSG_QUEUE_ENVELOPE_SCHEDULE);
	CASE(IMSG_QUEUE_ENVELOPE_SUBMIT);
	CASE(IMSG_QUEUE_HOLDQ_HOLD);
	CASE(IMSG_QUEUE_HOLDQ_RELEASE);
	CASE(IMSG_QUEUE_MESSAGE_COMMIT);
	CASE(IMSG_QUEUE_MESSAGE_ROLLBACK);
	CASE(IMSG_QUEUE_SMTP_SESSION);
	CASE(IMSG_QUEUE_TRANSFER);

	CASE(IMSG_MDA_DELIVERY_OK);
	CASE(IMSG_MDA_DELIVERY_TEMPFAIL);
	CASE(IMSG_MDA_DELIVERY_PERMFAIL);
	CASE(IMSG_MDA_DELIVERY_LOOP);
	CASE(IMSG_MDA_DELIVERY_HOLD);
	CASE(IMSG_MDA_DONE);
	CASE(IMSG_MDA_FORK);
	CASE(IMSG_MDA_HOLDQ_RELEASE);
	CASE(IMSG_MDA_LOOKUP_USERINFO);
	CASE(IMSG_MDA_KILL);
	CASE(IMSG_MDA_OPEN_MESSAGE);

	CASE(IMSG_MTA_DELIVERY_OK);
	CASE(IMSG_MTA_DELIVERY_TEMPFAIL);
	CASE(IMSG_MTA_DELIVERY_PERMFAIL);
	CASE(IMSG_MTA_DELIVERY_LOOP);
	CASE(IMSG_MTA_DELIVERY_HOLD);
	CASE(IMSG_MTA_DNS_HOST);
	CASE(IMSG_MTA_DNS_HOST_END);
	CASE(IMSG_MTA_DNS_PTR);
	CASE(IMSG_MTA_DNS_MX);
	CASE(IMSG_MTA_DNS_MX_PREFERENCE);
	CASE(IMSG_MTA_HOLDQ_RELEASE);
	CASE(IMSG_MTA_LOOKUP_CREDENTIALS);
	CASE(IMSG_MTA_LOOKUP_SOURCE);
	CASE(IMSG_MTA_LOOKUP_HELO);
	CASE(IMSG_MTA_OPEN_MESSAGE);
	CASE(IMSG_MTA_SCHEDULE);
	CASE(IMSG_MTA_SSL_INIT);
	CASE(IMSG_MTA_SSL_VERIFY_CERT);
	CASE(IMSG_MTA_SSL_VERIFY_CHAIN);
	CASE(IMSG_MTA_SSL_VERIFY);

	CASE(IMSG_SCHED_ENVELOPE_BOUNCE);
	CASE(IMSG_SCHED_ENVELOPE_DELIVER);
	CASE(IMSG_SCHED_ENVELOPE_EXPIRE);
	CASE(IMSG_SCHED_ENVELOPE_INJECT);
	CASE(IMSG_SCHED_ENVELOPE_REMOVE);
	CASE(IMSG_SCHED_ENVELOPE_TRANSFER);

	CASE(IMSG_SMTP_AUTHENTICATE);
	CASE(IMSG_SMTP_DNS_PTR);
	CASE(IMSG_SMTP_MESSAGE_COMMIT);
	CASE(IMSG_SMTP_MESSAGE_CREATE);
	CASE(IMSG_SMTP_MESSAGE_ROLLBACK);
	CASE(IMSG_SMTP_MESSAGE_OPEN);
	CASE(IMSG_SMTP_EXPAND_RCPT);
	CASE(IMSG_SMTP_LOOKUP_HELO);
	CASE(IMSG_SMTP_SSL_INIT);
	CASE(IMSG_SMTP_SSL_VERIFY_CERT);
	CASE(IMSG_SMTP_SSL_VERIFY_CHAIN);
	CASE(IMSG_SMTP_SSL_VERIFY);

	CASE(IMSG_SMTP_REQ_CONNECT);
	CASE(IMSG_SMTP_REQ_HELO);
	CASE(IMSG_SMTP_REQ_MAIL);
	CASE(IMSG_SMTP_REQ_RCPT);
	CASE(IMSG_SMTP_REQ_DATA);
	CASE(IMSG_SMTP_REQ_EOM);
	CASE(IMSG_SMTP_EVENT_RSET);
	CASE(IMSG_SMTP_EVENT_COMMIT);
	CASE(IMSG_SMTP_EVENT_ROLLBACK);
	CASE(IMSG_SMTP_EVENT_DISCONNECT);

	CASE(IMSG_CA_PRIVENC);
	CASE(IMSG_CA_PRIVDEC);
	default:
		(void)snprintf(buf, sizeof(buf), "IMSG_??? (%d)", type);

		return buf;
	}
}

int
parent_auth_user(const char *username, const char *password)
{
	char	user[LOGIN_NAME_MAX];
	char	pass[LINE_MAX];
	int	ret;

	(void)strlcpy(user, username, sizeof(user));
	(void)strlcpy(pass, password, sizeof(pass));

	ret = auth_userokay(user, NULL, "auth-smtp", pass);
	if (ret)
		return LKA_OK;
	return LKA_PERMFAIL;
}
