/*	$OpenBSD: smtpd.c,v 1.292 2018/01/27 08:32:03 anton Exp $	*/

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
#include <fts.h>
#include <imsg.h>
#include <inttypes.h>
#include <login_cap.h>
#include <paths.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/evp.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"

#define SMTPD_MAXARG 32

static void parent_imsg(struct mproc *, struct imsg *);
static void usage(void);
static int smtpd(void);
static void parent_shutdown(void);
static void parent_send_config(int, short, void *);
static void parent_send_config_lka(void);
static void parent_send_config_pony(void);
static void parent_send_config_ca(void);
static void parent_sig_handler(int, short, void *);
static void forkmda(struct mproc *, uint64_t, struct deliver *);
static int parent_forward_open(char *, char *, uid_t, gid_t);
static struct child *child_add(pid_t, int, const char *);
static struct mproc *start_child(int, char **, char *);
static struct mproc *setup_peer(enum smtp_proc_type, pid_t, int);
static void setup_peers(struct mproc *, struct mproc *);
static void setup_done(struct mproc *);
static void setup_proc(void);
static struct mproc *setup_peer(enum smtp_proc_type, pid_t, int);
static int imsg_wait(struct imsgbuf *, struct imsg *, int);

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

	if (imsg == NULL)
		fatalx("process %s socket closed", p->name);

	switch (imsg->hdr.type) {
	case IMSG_LKA_OPEN_FORWARD:
		CHECK_IMSG_DATA_SIZE(imsg, sizeof *fwreq);
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

	case IMSG_CTL_VERBOSE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		log_trace_verbose(v);
		return;

	case IMSG_CTL_PROFILE:
		m_msg(&m, imsg);
		m_get_int(&m, &v);
		m_end(&m);
		profiling = v;
		return;
	}

	errx(1, "parent_imsg: unexpected %s imsg from %s",
	    imsg_to_str(imsg->hdr.type), proc_title(p->proc));
}

static void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-dFhnv] [-D macro=value] "
	    "[-f file] [-P system] [-T trace]\n", __progname);
	exit(1);
}

static void
parent_shutdown(void)
{
	pid_t pid;

	mproc_clear(p_ca);
	mproc_clear(p_pony);
	mproc_clear(p_control);
	mproc_clear(p_lka);
	mproc_clear(p_scheduler);
	mproc_clear(p_queue);

	do {
		pid = waitpid(WAIT_MYPGRP, NULL, 0);
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	unlink(SMTPD_SOCKET);

	log_info("Exiting");
	exit(0);
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
	int		 status, fail;
	pid_t		 pid;
	char		*cause;

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		log_debug("debug: got signal %d", sig);
		parent_shutdown();
		/* NOT REACHED */

	case SIGCHLD:
		do {
			int len;

			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			fail = 0;
			if (WIFSIGNALED(status)) {
				fail = 1;
				len = asprintf(&cause, "terminated; signal %d",
				    WTERMSIG(status));
			} else if (WIFEXITED(status)) {
				if (WEXITSTATUS(status) != 0) {
					fail = 1;
					len = asprintf(&cause,
					    "exited abnormally");
				} else
					len = asprintf(&cause, "exited okay");
			} else
				/* WIFSTOPPED or WIFCONTINUED */
				continue;

			if (len == -1)
				fatal("asprintf");

			if (pid == purge_pid)
				purge_pid = -1;

			child = tree_pop(&children, pid);
			if (child == NULL)
				goto skip;

			switch (child->type) {
			case CHILD_DAEMON:
				if (fail)
					log_warnx("warn: lost child: %s %s",
					    child->title, cause);
				break;

			case CHILD_MDA:
				if (WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGALRM) {
					char *tmp;
					if (asprintf(&tmp,
					    "terminated; timeout") != -1) {
						free(cause);
						cause = tmp;
					}
				}
				else if (child->cause &&
				    WIFSIGNALED(status) &&
				    WTERMSIG(status) == SIGTERM) {
					free(cause);
					cause = child->cause;
					child->cause = NULL;
				}
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
	int		 save_argc = argc;
	char		**save_argv = argv;
	char		*rexec = NULL;
	struct smtpd	 conf;

	env = &conf;

	flags = 0;
	opts = 0;
	debug = 0;
	tracing = 0;

	log_init(1, LOG_MAIL);

	TAILQ_INIT(&offline_q);

	while ((c = getopt(argc, argv, "B:dD:hnP:f:FT:vx:")) != -1) {
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
			foreground_log = 1;
			break;
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("warn: "
				    "could not parse macro definition %s",
				    optarg);
			break;
		case 'h':
			log_info("version: " SMTPD_NAME " " SMTPD_VERSION);
			usage();
			break;
		case 'n':
			debug = 2;
			opts |= SMTPD_OPT_NOACTION;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'F':
			foreground = 1;
			break;

		case 'T':
			if (!strcmp(optarg, "imsg"))
				tracing |= TRACE_IMSG;
			else if (!strcmp(optarg, "io"))
				tracing |= TRACE_IO;
			else if (!strcmp(optarg, "smtp"))
				tracing |= TRACE_SMTP;
			else if (!strcmp(optarg, "mfa") ||
			    !strcmp(optarg, "filter") ||
			    !strcmp(optarg, "filters"))
				tracing |= TRACE_FILTERS;
			else if (!strcmp(optarg, "mta") ||
			    !strcmp(optarg, "transfer"))
				tracing |= TRACE_MTA;
			else if (!strcmp(optarg, "bounce") ||
			    !strcmp(optarg, "bounces"))
				tracing |= TRACE_BOUNCE;
			else if (!strcmp(optarg, "scheduler"))
				tracing |= TRACE_SCHEDULER;
			else if (!strcmp(optarg, "lookup"))
				tracing |= TRACE_LOOKUP;
			else if (!strcmp(optarg, "stat") ||
			    !strcmp(optarg, "stats"))
				tracing |= TRACE_STAT;
			else if (!strcmp(optarg, "rules"))
				tracing |= TRACE_RULES;
			else if (!strcmp(optarg, "mproc"))
				tracing |= TRACE_MPROC;
			else if (!strcmp(optarg, "expand"))
				tracing |= TRACE_EXPAND;
			else if (!strcmp(optarg, "table") ||
			    !strcmp(optarg, "tables"))
				tracing |= TRACE_TABLES;
			else if (!strcmp(optarg, "queue"))
				tracing |= TRACE_QUEUE;
			else if (!strcmp(optarg, "all"))
				tracing |= ~TRACE_DEBUG;
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
			tracing |=  TRACE_DEBUG;
			break;
		case 'x':
			rexec = optarg;
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

	if (parse_config(&conf, conffile, opts))
		exit(1);

	if (strlcpy(env->sc_conffile, conffile, PATH_MAX)
	    >= PATH_MAX)
		errx(1, "config file exceeds PATH_MAX");

	if (env->sc_opts & SMTPD_OPT_NOACTION) {
		if (env->sc_queue_key &&
		    crypto_setup(env->sc_queue_key,
		    strlen(env->sc_queue_key)) == 0) {
			fatalx("crypto_setup:"
			    "invalid key for queue encryption");
		}
		load_pki_tree();
		load_pki_keys();
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	env->sc_flags |= flags;

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	log_init(foreground_log, LOG_MAIL);
	log_trace_verbose(tracing);
	load_pki_tree();
	load_pki_keys();

	log_debug("debug: using \"%s\" queue backend", backend_queue);
	log_debug("debug: using \"%s\" scheduler backend", backend_scheduler);
	log_debug("debug: using \"%s\" stat backend", backend_stat);

	if (env->sc_hostname[0] == '\0')
		errx(1, "machine does not have a hostname set");
	env->sc_uptime = time(NULL);

	if (rexec == NULL) {
		smtpd_process = PROC_PARENT;

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
				char   *buf = NULL;
				size_t	sz = 0;
				ssize_t	len;

				if (strcasecmp(env->sc_queue_key, "stdin") == 0) {
					if ((len = getline(&buf, &sz, stdin)) == -1)
						err(1, "getline");
					if (buf[len - 1] == '\n')
						buf[len - 1] = '\0';
					env->sc_queue_key = buf;
				}
			}
		}

		log_info("info: %s %s starting", SMTPD_NAME, SMTPD_VERSION);

		if (!foreground)
			if (daemon(0, 0) == -1)
				err(1, "failed to daemonize");

		/* setup all processes */

		p_ca = start_child(save_argc, save_argv, "ca");
		p_ca->proc = PROC_CA;

		p_control = start_child(save_argc, save_argv, "control");
		p_control->proc = PROC_CONTROL;

		p_lka = start_child(save_argc, save_argv, "lka");
		p_lka->proc = PROC_LKA;

		p_pony = start_child(save_argc, save_argv, "pony");
		p_pony->proc = PROC_PONY;

		p_queue = start_child(save_argc, save_argv, "queue");
		p_queue->proc = PROC_QUEUE;

		p_scheduler = start_child(save_argc, save_argv, "scheduler");
		p_scheduler->proc = PROC_SCHEDULER;

		setup_peers(p_control, p_ca);
		setup_peers(p_control, p_lka);
		setup_peers(p_control, p_pony);
		setup_peers(p_control, p_queue);
		setup_peers(p_control, p_scheduler);
		setup_peers(p_pony, p_ca);
		setup_peers(p_pony, p_lka);
		setup_peers(p_pony, p_queue);
		setup_peers(p_queue, p_lka);
		setup_peers(p_queue, p_scheduler);

		if (env->sc_queue_key) {
			if (imsg_compose(&p_queue->imsgbuf, IMSG_SETUP_KEY, 0,
			    0, -1, env->sc_queue_key, strlen(env->sc_queue_key)
			    + 1) == -1)
				fatal("imsg_compose");
			if (imsg_flush(&p_queue->imsgbuf) == -1)
				fatal("imsg_flush");
		}

		setup_done(p_ca);
		setup_done(p_control);
		setup_done(p_lka);
		setup_done(p_pony);
		setup_done(p_queue);
		setup_done(p_scheduler);

		log_debug("smtpd: setup done");

		return smtpd();
	}

	if (!strcmp(rexec, "ca")) {
		smtpd_process = PROC_CA;
		setup_proc();

		return ca();
	}

	else if (!strcmp(rexec, "control")) {
		smtpd_process = PROC_CONTROL;
		setup_proc();

		/* the control socket ensures that only one smtpd instance is running */
		control_socket = control_create_socket();

		env->sc_stat = stat_backend_lookup(backend_stat);
		if (env->sc_stat == NULL)
			errx(1, "could not find stat backend \"%s\"", backend_stat);

		return control();
	}

	else if (!strcmp(rexec, "lka")) {
		smtpd_process = PROC_LKA;
		setup_proc();

		return lka();
	}

	else if (!strcmp(rexec, "pony")) {
		smtpd_process = PROC_PONY;
		setup_proc();

		return pony();
	}

	else if (!strcmp(rexec, "queue")) {
		smtpd_process = PROC_QUEUE;
		setup_proc();

		if (env->sc_queue_flags & QUEUE_COMPRESSION)
			env->sc_comp = compress_backend_lookup("gzip");

		if (!queue_init(backend_queue, 1))
			errx(1, "could not initialize queue backend");

		return queue();
	}

	else if (!strcmp(rexec, "scheduler")) {
		smtpd_process = PROC_SCHEDULER;
		setup_proc();

		for (i = 0; i < MAX_BOUNCE_WARN; i++) {
			if (env->sc_bounce_warn[i] == 0)
				break;
			log_debug("debug: bounce warning after %s",
			    duration_to_text(env->sc_bounce_warn[i]));
		}

		return scheduler();
	}

	fatalx("bad rexec: %s", rexec);

	return (1);
}

static struct mproc *
start_child(int save_argc, char **save_argv, char *rexec)
{
	struct mproc *p;
	char *argv[SMTPD_MAXARG];
	int sp[2], argc = 0;
	pid_t pid;

	if (save_argc >= SMTPD_MAXARG - 2)
		fatalx("too many arguments");

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) == -1)
		fatal("socketpair");

	io_set_nonblocking(sp[0]);
	io_set_nonblocking(sp[1]);

	switch (pid = fork()) {
	case -1:
		fatal("%s: fork", save_argv[0]);
	case 0:
		break;
	default:
		close(sp[0]);
		p = calloc(1, sizeof(*p));
		if (p == NULL)
			fatal("calloc");
		if((p->name = strdup(rexec)) == NULL)
			fatal("strdup");
		mproc_init(p, sp[1]);
		p->pid = pid;
		p->handler = parent_imsg;
		return p;
	}

	if (dup2(sp[0], 3) == -1)
		fatal("%s: dup2", rexec);

	if (closefrom(4) == -1)
		fatal("%s: closefrom", rexec);

	for (argc = 0; argc < save_argc; argc++)
		argv[argc] = save_argv[argc];
	argv[argc++] = "-x";
	argv[argc++] = rexec;
	argv[argc++] = NULL;

	execvp(argv[0], argv);
	fatal("%s: execvp", rexec);
}

static void
setup_peers(struct mproc *a, struct mproc *b)
{
	int sp[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sp) == -1)
		fatal("socketpair");

	io_set_nonblocking(sp[0]);
	io_set_nonblocking(sp[1]);

	if (imsg_compose(&a->imsgbuf, IMSG_SETUP_PEER, b->proc, b->pid, sp[0],
	    NULL, 0) == -1)
		fatal("imsg_compose");
	if (imsg_flush(&a->imsgbuf) == -1)
		fatal("imsg_flush");

	if (imsg_compose(&b->imsgbuf, IMSG_SETUP_PEER, a->proc, a->pid, sp[1],
	    NULL, 0) == -1)
		fatal("imsg_compose");
	if (imsg_flush(&b->imsgbuf) == -1)
		fatal("imsg_flush");
}

static void
setup_done(struct mproc *p)
{
	struct imsg imsg;

	if (imsg_compose(&p->imsgbuf, IMSG_SETUP_DONE, 0, 0, -1, NULL, 0) == -1)
		fatal("imsg_compose");
	if (imsg_flush(&p->imsgbuf) == -1)
		fatal("imsg_flush");

	if (imsg_wait(&p->imsgbuf, &imsg, 10000) == -1)
		fatal("imsg_wait");

	if (imsg.hdr.type != IMSG_SETUP_DONE)
		fatalx("expect IMSG_SETUP_DONE");

	log_debug("setup_done: %s[%d] done", p->name, p->pid);

	imsg_free(&imsg);
}

static void
setup_proc(void)
{
	struct imsgbuf *ibuf;
	struct imsg imsg;
        int setup = 1;

	log_procinit(proc_title(smtpd_process));

	p_parent = calloc(1, sizeof(*p_parent));
	if (p_parent == NULL)
		fatal("calloc");
	if((p_parent->name = strdup("parent")) == NULL)
		fatal("strdup");
	p_parent->proc = PROC_PARENT;
	p_parent->handler = imsg_dispatch;
	mproc_init(p_parent, 3);

	ibuf = &p_parent->imsgbuf;

	while (setup) {
		if (imsg_wait(ibuf, &imsg, 10000) == -1)
			fatal("imsg_wait");

		switch (imsg.hdr.type) {
		case IMSG_SETUP_KEY:
			env->sc_queue_key = strdup(imsg.data);
			break;
		case IMSG_SETUP_PEER:
			setup_peer(imsg.hdr.peerid, imsg.hdr.pid, imsg.fd);
			break;
		case IMSG_SETUP_DONE:
			setup = 0;
			break;
		default:
			fatal("bad imsg %d", imsg.hdr.type);
		}
		imsg_free(&imsg);
	}

	if (imsg_compose(ibuf, IMSG_SETUP_DONE, 0, 0, -1, NULL, 0) == -1)
		fatal("imsg_compose");

	if (imsg_flush(ibuf) == -1)
		fatal("imsg_flush");

	log_debug("setup_proc: %s done", proc_title(smtpd_process));
}

static struct mproc *
setup_peer(enum smtp_proc_type proc, pid_t pid, int sock)
{
	struct mproc *p, **pp;

	log_debug("setup_peer: %s -> %s[%u] fd=%d", proc_title(smtpd_process),
	    proc_title(proc), pid, sock);

	if (sock == -1)
		fatalx("peer socket not received");

	switch (proc) {
	case PROC_LKA:
		pp = &p_lka;
		break;
	case PROC_QUEUE:
		pp = &p_queue;
		break;
	case PROC_CONTROL:
		pp = &p_control;
		break;
	case PROC_SCHEDULER:
		pp = &p_scheduler;
		break;
	case PROC_PONY:
		pp = &p_pony;
		break;
	case PROC_CA:
		pp = &p_ca;
		break;
	default:
		fatalx("unknown peer");
	}

	if (*pp)
		fatalx("peer already set");

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		fatal("calloc");
	if((p->name = strdup(proc_title(proc))) == NULL)
		fatal("strdup");
	mproc_init(p, sock);
	p->pid = pid;
	p->proc = proc;
	p->handler = imsg_dispatch;

	*pp = p;

	return p;
}

static int
imsg_wait(struct imsgbuf *ibuf, struct imsg *imsg, int timeout)
{
	struct pollfd pfd[1];
	ssize_t n;

	pfd[0].fd = ibuf->fd;
	pfd[0].events = POLLIN;
	
	while (1) {
		if ((n = imsg_get(ibuf, imsg)) == -1)
			return -1;
		if (n)
			return 1;

		n = poll(pfd, 1, timeout);
		if (n == -1)
			return -1;
		if (n == 0) {
			errno = ETIMEDOUT;
			return -1;
		}

		if (((n = imsg_read(ibuf)) == -1 && errno != EAGAIN) || n == 0)
			return -1;
	}
}

int
smtpd(void) {
	struct event	 ev_sigint;
	struct event	 ev_sigterm;
	struct event	 ev_sigchld;
	struct event	 ev_sighup;
	struct timeval	 tv;

	imsg_callback = parent_imsg;

	tree_init(&children);

	child_add(p_queue->pid, CHILD_DAEMON, proc_title(PROC_QUEUE));
	child_add(p_control->pid, CHILD_DAEMON, proc_title(PROC_CONTROL));
	child_add(p_lka->pid, CHILD_DAEMON, proc_title(PROC_LKA));
	child_add(p_scheduler->pid, CHILD_DAEMON, proc_title(PROC_SCHEDULER));
	child_add(p_pony->pid, CHILD_DAEMON, proc_title(PROC_PONY));
	child_add(p_ca->pid, CHILD_DAEMON, proc_title(PROC_CA));

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

	evtimer_set(&config_ev, parent_send_config, NULL);
	memset(&tv, 0, sizeof(tv));
	evtimer_add(&config_ev, &tv);

	/* defer offline scanning for a second */
	evtimer_set(&offline_ev, offline_scan, NULL);
	offline_timeout.tv_sec = 1;
	offline_timeout.tv_usec = 0;
	evtimer_add(&offline_ev, &offline_timeout);

	purge_task();

	if (pledge("stdio rpath wpath cpath fattr flock tmppath "
	    "getpw sendfd proc exec id inet unix", NULL) == -1)
		err(1, "pledge");

	event_dispatch();
	fatalx("exited event loop");

	return (0);
}

static void
load_pki_tree(void)
{
	struct pki	*pki;
	struct ca	*sca;
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

		if (!ssl_load_certificate(pki, pki->pki_cert_file))
			fatalx("load_pki_tree: failed to load certificate file");
	}

	log_debug("debug: init ca-tree");
	iter_dict = NULL;
	while (dict_iter(env->sc_ca_dict, &iter_dict, &k, (void **)&sca)) {
		log_debug("info: loading CA information for %s", k);
		if (!ssl_load_cafile(sca, sca->ca_cert_file))
			fatalx("load_pki_tree: failed to load CA file");
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

		if (!ssl_load_keyfile(pki, pki->pki_key_file, k))
			fatalx("load_pki_keys: failed to load key file");
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

		execl(path, procname, arg, (char *)NULL);
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
			if ((pw = getpwnam(SMTPD_QUEUE_USER)) == NULL)
				fatalx("unknown user " SMTPD_QUEUE_USER);
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

	if (deliver->userinfo.uid == 0 && !db->allow_root) {
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
	allout = mkstemp(sfn);
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
	char		*path_argv[2];
	FTS		*fts = arg;
	FTSENT		*e;
	int		 n = 0;

	path_argv[0] = PATH_SPOOL PATH_OFFLINE;
	path_argv[1] = NULL;

	if (fts == NULL) {
		log_debug("debug: smtpd: scanning offline queue...");
		fts = fts_open(path_argv, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
		if (fts == NULL) {
			log_warn("fts_open: %s", path_argv[0]);
			return;
		}
	}

	while ((e = fts_read(fts)) != NULL) {
		if (e->fts_info != FTS_F)
			continue;

		/* offline files must be at depth 1 */
		if (e->fts_level != 1)
			continue;

		/* offline file group must match parent directory group */
		if (e->fts_statp->st_gid != e->fts_parent->fts_statp->st_gid)
			continue;

		if (e->fts_statp->st_size == 0) {
			if (unlink(e->fts_accpath) == -1)
				log_warnx("warn: smtpd: could not unlink %s", e->fts_accpath);
			continue;
		}

		if (offline_add(e->fts_name)) {
			log_warnx("warn: smtpd: "
			    "could not add offline message %s", e->fts_name);
			continue;
		}

		if ((n++) == OFFLINE_READMAX) {
			evtimer_set(&offline_ev, offline_scan, fts);
			offline_timeout.tv_sec = 0;
			offline_timeout.tv_usec = 100000;
			evtimer_add(&offline_ev, &offline_timeout);
			return;
		}
	}

	log_debug("debug: smtpd: offline scanning done");
	fts_close(fts);
}

static int
offline_enqueue(char *name)
{
	char		*path;
	struct stat	 sb;
	pid_t		 pid;
	struct child	*child;
	struct passwd	*pw;
	int		 pathlen;

	pathlen = asprintf(&path, "%s/%s", PATH_SPOOL PATH_OFFLINE, name);
	if (pathlen == -1) {
		log_warnx("warn: smtpd: asprintf");
		return (-1);
	}

	if (pathlen >= PATH_MAX) {
		log_warnx("warn: smtpd: pathname exceeds PATH_MAX");
		free(path);
		return (-1);
	}

	log_debug("debug: smtpd: enqueueing offline message %s", path);

	if ((pid = fork()) == -1) {
		log_warn("warn: smtpd: fork");
		free(path);
		return (-1);
	}

	if (pid == 0) {
		char	*envp[2], *p = NULL, *tmp;
		int	 fd;
		FILE	*fp;
		size_t	 sz = 0;
		ssize_t	 len;
		arglist	 args;

		if (closefrom(STDERR_FILENO + 1) == -1)
			_exit(1);

		memset(&args, 0, sizeof(args));

		if ((fd = open(path, O_RDONLY|O_NOFOLLOW|O_NONBLOCK)) == -1) {
			log_warn("warn: smtpd: open: %s", path);
			_exit(1);
		}

		if (fstat(fd, &sb) == -1) {
			log_warn("warn: smtpd: fstat: %s", path);
			_exit(1);
		}

		if (!S_ISREG(sb.st_mode)) {
			log_warnx("warn: smtpd: file %s (uid %d) not regular",
			    path, sb.st_uid);
			_exit(1);
		}

		if (sb.st_nlink != 1) {
			log_warnx("warn: smtpd: file %s is hard-link", path);
			_exit(1);
		}

		pw = getpwuid(sb.st_uid);
		if (pw == NULL) {
			log_warnx("warn: smtpd: getpwuid for uid %d failed",
			    sb.st_uid);
			_exit(1);
		}

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			_exit(1);

		if ((fp = fdopen(fd, "r")) == NULL)
			_exit(1);

		if (chdir(pw->pw_dir) == -1 && chdir("/") == -1)
			_exit(1);

		if (setsid() == -1 ||
		    signal(SIGPIPE, SIG_DFL) == SIG_ERR ||
		    dup2(fileno(fp), STDIN_FILENO) == -1)
			_exit(1);

		if ((len = getline(&p, &sz, fp)) == -1)
			_exit(1);

		if (p[len - 1] != '\n')
			_exit(1);
		p[len - 1] = '\0';

		addargs(&args, "%s", "sendmail");
		addargs(&args, "%s", "-S");

		while ((tmp = strsep(&p, "|")) != NULL)
			addargs(&args, "%s", tmp);

		free(p);
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

	if (!bsnprintf(pathname, sizeof (pathname), "%s/.forward",
		directory)) {
		log_warnx("warn: smtpd: %s: pathname too large", pathname);
		return -1;
	}

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
		fd = open(pathname, O_RDONLY|O_NOFOLLOW|O_NONBLOCK);
	} while (fd == -1 && errno == EINTR);
	if (fd == -1) {
		if (errno == ENOENT)
			return -1;
		if (errno == EMFILE || errno == ENFILE || errno == EIO) {
			errno = EAGAIN;
			return -1;
		}
		if (errno == ELOOP)
			log_warnx("warn: smtpd: parent_forward_open: %s: "
			    "cannot follow symbolic links", pathname);
		else
			log_warn("warn: smtpd: parent_forward_open: %s", pathname);
		return -1;
	}

	if (!secure_file(fd, pathname, directory, uid, 1)) {
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
		imsg_callback(p, imsg);
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

			if (!bsnprintf(key, sizeof key,
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
	case PROC_CLIENT:
		return "client";
	}
	return "unknown";
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
	CASE(IMSG_CTL_TRACE_DISABLE);
	CASE(IMSG_CTL_TRACE_ENABLE);
	CASE(IMSG_CTL_UPDATE_TABLE);
	CASE(IMSG_CTL_VERBOSE);
	CASE(IMSG_CTL_DISCOVER_EVPID);
	CASE(IMSG_CTL_DISCOVER_MSGID);
	CASE(IMSG_CTL_UNCORRUPT_MSGID);

	CASE(IMSG_CTL_SMTP_SESSION);

	CASE(IMSG_SETUP_KEY);
	CASE(IMSG_SETUP_PEER);
	CASE(IMSG_SETUP_DONE);

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
	CASE(IMSG_QUEUE_DISCOVER_EVPID);
	CASE(IMSG_QUEUE_DISCOVER_MSGID);
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
	CASE(IMSG_MTA_TLS_INIT);
	CASE(IMSG_MTA_TLS_VERIFY_CERT);
	CASE(IMSG_MTA_TLS_VERIFY_CHAIN);
	CASE(IMSG_MTA_TLS_VERIFY);

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
	CASE(IMSG_SMTP_CHECK_SENDER);
	CASE(IMSG_SMTP_EXPAND_RCPT);
	CASE(IMSG_SMTP_LOOKUP_HELO);
	CASE(IMSG_SMTP_TLS_INIT);
	CASE(IMSG_SMTP_TLS_VERIFY_CERT);
	CASE(IMSG_SMTP_TLS_VERIFY_CHAIN);
	CASE(IMSG_SMTP_TLS_VERIFY);

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
