/*	$OpenBSD: smtpd.c,v 1.44 2009/03/10 10:01:39 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/resource.h>

#include <bsd_auth.h>
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

#include <keynote.h>

#include "smtpd.h"

__dead void	usage(void);
void		parent_shutdown(void);
void		parent_send_config(int, short, void *);
void		parent_dispatch_lka(int, short, void *);
void		parent_dispatch_mda(int, short, void *);
void		parent_dispatch_mfa(int, short, void *);
void		parent_dispatch_smtp(int, short, void *);
void		parent_dispatch_control(int, short, void *);
void		parent_sig_handler(int, short, void *);
int		parent_open_message_file(struct batch *);
int		parent_mailbox_init(struct passwd *, char *);
int		parent_mailbox_open(char *, struct passwd *, struct batch *);
int		parent_filename_open(char *, struct passwd *, struct batch *);
int		parent_mailfile_rename(struct batch *, struct path *);
int		parent_maildir_open(char *, struct passwd *, struct batch *);
int		parent_maildir_init(struct passwd *, char *);
int		parent_external_mda(char *, struct passwd *, struct batch *);
int		parent_forward_open(char *);
int		check_child(pid_t, const char *);
int		setup_spool(uid_t, gid_t);

pid_t	lka_pid = 0;
pid_t	mfa_pid = 0;
pid_t	queue_pid = 0;
pid_t	mda_pid = 0;
pid_t	mta_pid = 0;
pid_t	control_pid = 0;
pid_t	smtp_pid = 0;
pid_t	runner_pid = 0;

struct s_parent	s_parent;

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
parent_shutdown(void)
{
	u_int		i;
	pid_t		pid;
	pid_t		pids[] = {
		lka_pid,
		mfa_pid,
		queue_pid,
		mda_pid,
		mta_pid,
		control_pid,
		smtp_pid,
		runner_pid
	};

	for (i = 0; i < sizeof(pids) / sizeof(pid); i++)
		if (pids[i])
			kill(pids[i], SIGTERM);

	do {
		if ((pid = wait(NULL)) == -1 &&
		    errno != EINTR && errno != ECHILD)
			fatal("wait");
	} while (pid != -1 || (pid == -1 && errno == EINTR));

	log_info("terminating");
	exit(0);
}

void
parent_send_config(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct iovec		iov[3];
	struct listener		*l;
	struct ssl		*s;

	log_debug("parent_send_config: configuring smtp");
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_START,
	    0, 0, -1, NULL, 0);

	SPLAY_FOREACH(s, ssltree, &env->sc_ssl) {
		iov[0].iov_base = s;
		iov[0].iov_len = sizeof(*s);
		iov[1].iov_base = s->ssl_cert;
		iov[1].iov_len = s->ssl_cert_len;
		iov[2].iov_base = s->ssl_key;
		iov[2].iov_len = s->ssl_key_len;

		imsg_composev(env->sc_ibufs[PROC_SMTP], IMSG_CONF_SSL, 0, 0, -1,
		    iov, sizeof(iov)/sizeof(iov[0]));
	}

	TAILQ_FOREACH(l, &env->sc_listeners, entry) {
		smtp_listener_setup(env, l);
		imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_LISTENER,
		    0, 0, l->fd, l, sizeof(*l));
	}
	imsg_compose(env->sc_ibufs[PROC_SMTP], IMSG_CONF_END,
	    0, 0, -1, NULL, 0);
}

void
parent_dispatch_lka(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_FORWARD_OPEN: {
			int ret;
			struct forward_req *fwreq;

			fwreq = imsg.data;
			ret = parent_forward_open(fwreq->pw_name);
			if (ret == -1)
				if (errno == ENOENT)
					fwreq->pw_name[0] = '\0';
			imsg_compose(ibuf, IMSG_PARENT_FORWARD_OPEN, 0, 0, ret, fwreq, sizeof(*fwreq));
			break;
		}
		default:
			log_debug("parent_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_mfa(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MFA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("parent_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_mda(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MDA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_MAILBOX_OPEN: {
			struct batch *batchp;
			struct path *path;
			struct passwd *pw;
			char *pw_name;
			char *file;
			u_int8_t i;
			int desc;
			struct action_handler {
				enum action_type action;
				int (*handler)(char *, struct passwd *, struct batch *);
			} action_hdl_table[] = {
				{ A_MBOX,	parent_mailbox_open },
				{ A_MAILDIR,	parent_maildir_open },
				{ A_EXT,	parent_external_mda },
				{ A_FILENAME,	parent_filename_open }
			};

			batchp = imsg.data;
			path = &batchp->message.recipient;
			if (batchp->type & T_DAEMON_BATCH) {
				path = &batchp->message.sender;
			}
			
			for (i = 0; i < sizeof(action_hdl_table) / sizeof(struct action_handler); ++i)
				if (action_hdl_table[i].action == path->rule.r_action)
					break;
			if (i == sizeof(action_hdl_table) / sizeof(struct action_handler))
				fatalx("parent_dispatch_mda: unknown action");

			file = path->rule.r_value.path;
			pw_name = path->pw_name;
			if (path->rule.r_action == A_FILENAME) {
				file = path->u.filename;
				pw_name = SMTPD_USER;
			}

			errno = 0;
			pw = safe_getpwnam(pw_name);
			if (pw == NULL) {
				if (errno)
					batchp->message.status |= S_MESSAGE_TEMPFAILURE;
				else
					batchp->message.status |= S_MESSAGE_PERMFAILURE;
				imsg_compose(ibuf, IMSG_MDA_MAILBOX_FILE, 0, 0,
				    -1, batchp, sizeof(struct batch));
				break;
			}

			if (setegid(pw->pw_gid) || seteuid(pw->pw_uid))
				fatal("privdrop failed");

			desc = action_hdl_table[i].handler(file, pw, batchp);
			imsg_compose(ibuf, IMSG_MDA_MAILBOX_FILE, 0, 0,
			    desc, batchp, sizeof(struct batch));

			if (setegid(0) || seteuid(0))
				fatal("privdrop failed");

			break;
		}
		case IMSG_PARENT_MESSAGE_OPEN: {
			struct batch *batchp;
			int desc;

			batchp = imsg.data;
			desc = parent_open_message_file(batchp);

			imsg_compose(ibuf, IMSG_MDA_MESSAGE_FILE, 0, 0,
			    desc, batchp, sizeof(struct batch));

			break;
		}
		case IMSG_PARENT_MAILBOX_RENAME: {
			struct batch *batchp;
			struct path *path;
			struct passwd *pw;

			batchp = imsg.data;
			path = &batchp->message.recipient;
			if (batchp->type & T_DAEMON_BATCH) {
				path = &batchp->message.sender;
			}

			pw = safe_getpwnam(path->pw_name);
			if (pw == NULL)
				break;			

			if (seteuid(pw->pw_uid) == -1)
				fatal("privdrop failed");

			parent_mailfile_rename(batchp, path);

			if (seteuid(0) == -1)
				fatal("privraise failed");

			break;
		}
		default:
			log_debug("parent_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_smtp(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_PARENT_SEND_CONFIG: {
			parent_send_config(-1, -1, env);
			break;
		}
		case IMSG_PARENT_AUTHENTICATE: {
			struct session_auth_req *req;
			struct session_auth_reply reply;
			u_int8_t buffer[1024];
			char *pw_name;
			char *pw_passwd;

			req = (struct session_auth_req *)imsg.data;

			reply.session_id = req->session_id;
			reply.value = 0;

			if (kn_decode_base64(req->buffer, buffer, sizeof(buffer)) != -1) {
				pw_name = buffer+1;
				pw_passwd = pw_name+strlen(pw_name)+1;

				if (auth_userokay(pw_name, NULL, "auth-smtp", pw_passwd))
					reply.value = 1;
			}
			imsg_compose(ibuf, IMSG_PARENT_AUTHENTICATE, 0, 0,
			    -1, &reply, sizeof(reply));

			break;
		}
		default:
			log_debug("parent_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_CONTROL];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_control: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_STATS: {
			struct stats *s;

			s = imsg.data;
			s->u.parent = s_parent;
			imsg_compose(ibuf, IMSG_STATS, 0, 0, -1, s, sizeof(*s));
			break;
		}
		default:
			log_debug("parent_dispatch_control: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
parent_sig_handler(int sig, short event, void *p)
{
	int					 i;
	int					 die = 0;
	pid_t					 pid;
	struct mdaproc				*mdaproc;
	struct mdaproc				 lookup;
	struct smtpd				*env = p;
	struct { pid_t p; const char *s; }	 procs[] = {
		{ lka_pid,	"lookup agent" },
		{ mfa_pid,	"mail filter agent" },
		{ queue_pid,	"mail queue" },
		{ mda_pid,	"mail delivery agent" },
		{ mta_pid,	"mail transfer agent" },
		{ control_pid,	"control process" },
		{ smtp_pid,	"smtp server" },
		{ runner_pid,	"runner" },
		{ 0,		NULL },
	};

	switch (sig) {
	case SIGTERM:
	case SIGINT:
		die = 1;
		/* FALLTHROUGH */
	case SIGCHLD:
		for (i = 0; procs[i].s != NULL; i++)
			if (check_child(procs[i].p, procs[i].s)) {
				procs[i].p = 0;
				die = 1;
			}
		if (die)
			parent_shutdown();

		do {
			int status;

			pid = waitpid(-1, &status, WNOHANG);
			if (pid > 0) {
				lookup.pid = pid;
				mdaproc = SPLAY_FIND(mdaproctree, &env->mdaproc_queue, &lookup);
				if (mdaproc == NULL)
					errx(1, "received SIGCHLD but no known child for that pid (#%d)", pid);

				if (WIFEXITED(status) && !WIFSIGNALED(status)) {
					switch (WEXITSTATUS(status)) {
					case EX_OK:
						log_debug("DEBUG: external mda reported success");
						break;
					case EX_TEMPFAIL:
						log_debug("DEBUG: external mda reported temporary failure");
						break;
					default:
						log_debug("DEBUG: external mda reported permanent failure");
					}
				}
				else {
					log_debug("DEBUG: external mda process has terminated in a baaaad way");
				}

				SPLAY_REMOVE(mdaproctree, &env->mdaproc_queue,
				    mdaproc);
				free(mdaproc);
			}
		} while (pid > 0 || (pid == -1 && errno == EINTR));

		/**/
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
	struct rlimit	 rl;
	struct peer	 peers[] = {
		{ PROC_CONTROL,	parent_dispatch_control },
		{ PROC_LKA,	parent_dispatch_lka },
		{ PROC_MDA,	parent_dispatch_mda },
		{ PROC_MFA,	parent_dispatch_mfa },
		{ PROC_SMTP,	parent_dispatch_smtp },
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

	if (env.sc_opts & SMTPD_OPT_NOACTION) {
		fprintf(stderr, "configuration OK\n");
		exit(0);
	}

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");

	if ((env.sc_pw =  getpwnam(SMTPD_USER)) == NULL)
		errx(1, "unknown user %s", SMTPD_USER);
	endpwent();

	if (!setup_spool(env.sc_pw->pw_uid, 0))
		errx(1, "invalid directory permissions");

	log_init(debug);

	if (!debug)
		if (daemon(0, 0) == -1)
			err(1, "failed to daemonize");

	log_info("startup%s", (debug > 1)?" [debug mode]":"");

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("smtpd: failed to get resource limit");

	log_debug("smtpd: max open files %lld", rl.rlim_max);

	/*
	 * Allow the maximum number of open file descriptors for this
	 * login class (which should be the class "daemon" by default).
	 */
	rl.rlim_cur = rl.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("smtpd: failed to set resource limit");

	env.sc_maxconn = (rl.rlim_cur / 4) * 3;
	log_debug("smtpd: will accept at most %d clients", env.sc_maxconn);

	env.sc_instances[PROC_PARENT] = 1;
	env.sc_instances[PROC_LKA] = 1;
	env.sc_instances[PROC_MFA] = 1;
	env.sc_instances[PROC_QUEUE] = 1;
	env.sc_instances[PROC_MDA] = 1;
	env.sc_instances[PROC_MTA] = 1;
	env.sc_instances[PROC_SMTP] = 1;
	env.sc_instances[PROC_CONTROL] = 1;
	env.sc_instances[PROC_RUNNER] = 1;

	init_peers(&env);

	/* start subprocesses */
	lka_pid = lka(&env);
	mfa_pid = mfa(&env);
	queue_pid = queue(&env);
	mda_pid = mda(&env);
	mta_pid = mta(&env);
	smtp_pid = smtp(&env);
	control_pid = control(&env);
	runner_pid = runner(&env);

	setproctitle("parent");
	SPLAY_INIT(&env.mdaproc_queue);

	s_parent.start = time(NULL);

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

	config_pipes(&env, peers, 5);
	config_peers(&env, peers, 5);

	evtimer_set(&env.sc_ev, parent_send_config, &env);
	bzero(&tv, sizeof(tv));
	evtimer_add(&env.sc_ev, &tv);

	event_dispatch();

	return (0);
}


int
check_child(pid_t pid, const char *pname)
{
	int	status;

	if (waitpid(pid, &status, WNOHANG) > 0) {
		if (WIFEXITED(status)) {
			log_warnx("check_child: lost child: %s exited", pname);
			return (1);
		}
		if (WIFSIGNALED(status)) {
			log_warnx("check_child: lost child: %s terminated; "
			    "signal %d", pname, WTERMSIG(status));
			return (1);
		}
	}

	return (0);
}

int
setup_spool(uid_t uid, gid_t gid)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_ENQUEUE, PATH_QUEUE,
				     PATH_RUNQUEUE, PATH_RUNQUEUELOW,
				     PATH_RUNQUEUEHIGH, PATH_PURGE };
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
	for (n = 0; n < sizeof(paths)/sizeof(paths[0]); n++) {
		if (! bsnprintf(pathname, sizeof(pathname), "%s%s", PATH_SPOOL,
			paths[n]))
			fatal("snprintf");

		if (stat(pathname, &sb) == -1) {
			if (errno != ENOENT) {
				warn("stat: %s", pathname);
				ret = 0;
				continue;
			}

			if (mkdir(pathname, 0700) == -1) {
				ret = 0;
				warn("mkdir: %s", pathname);
			}

			if (chown(pathname, uid, gid) == -1) {
				ret = 0;
				warn("chown: %s", pathname);
			}

			if (stat(pathname, &sb) == -1)
				err(1, "stat: %s", pathname);
		}

		/* check if it's a directory */
		if (!S_ISDIR(sb.st_mode)) {
			ret = 0;
			warnx("%s is not a directory", pathname);
		}

		/* check that it is owned by uid/gid */
		if (sb.st_uid != uid) {
			ret = 0;
			warnx("%s is not owned by uid %d", pathname, uid);
		}
		if (sb.st_gid != gid) {
			ret = 0;
			warnx("%s is not owned by gid %d", pathname, gid);
		}

		/* check permission */
		if ((sb.st_mode & (S_IRUSR|S_IWUSR|S_IXUSR)) != (S_IRUSR|S_IWUSR|S_IXUSR) ||
		    (sb.st_mode & (S_IRGRP|S_IWGRP|S_IXGRP)) ||
		    (sb.st_mode & (S_IROTH|S_IWOTH|S_IXOTH))) {
			ret = 0;
			warnx("%s must be rwx------ (0700)", pathname);
		}
	}
	return ret;
}

void
imsg_event_add(struct imsgbuf *ibuf)
{
	if (ibuf->handler == NULL) {
		imsg_flush(ibuf);
		return;
	}

	ibuf->events = EV_READ;
	if (ibuf->w.queued)
		ibuf->events |= EV_WRITE;

	event_del(&ibuf->ev);
	event_set(&ibuf->ev, ibuf->fd, ibuf->events, ibuf->handler, ibuf->data);
	event_add(&ibuf->ev, NULL);
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
		PATH_SPOOL, PATH_QUEUE, hval, batchp->message_id)) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	fd = open(pathname, O_RDONLY);
	return fd;
}

int
parent_mailbox_init(struct passwd *pw, char *pathname)
{
	int fd;
	int ret = 1;
	int mode = O_CREAT|O_EXCL;

	/* user cannot create mailbox */
	if (seteuid(0) == -1)
		fatal("privraise failed");

	errno = 0;
	fd = open(pathname, mode, 0600);

	if (fd == -1) {
		if (errno != EEXIST)
			ret = 0;
	}

	if (fd != -1) {
		if (fchown(fd, pw->pw_uid, 0) == -1)
			fatal("fchown");
		close(fd);
	}

	if (seteuid(pw->pw_uid) == -1)
		fatal("privdropfailed");
		
	return ret;
}

int
parent_mailbox_open(char *path, struct passwd *pw, struct batch *batchp)
{
	pid_t pid;
	int pipefd[2];
	struct mdaproc *mdaproc;
	char sender[MAX_PATH_SIZE];

	/* This can never happen, but better safe than sorry. */
	if (! bsnprintf(sender, MAX_PATH_SIZE, "%s@%s",
		batchp->message.sender.user,
		batchp->message.sender.domain)) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	if (! parent_mailbox_init(pw, path)) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	log_debug("executing mail.local");

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1) {
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	/* raise privileges because mail.local needs root to
	 * deliver to user mailboxes.
	 */
	if (seteuid(0) == -1)
		fatal("privraise failed");

	pid = fork();
	if (pid == -1) {
		close(pipefd[0]);
		close(pipefd[1]);
		batchp->message.status |= S_MESSAGE_PERMFAILURE;
		return -1;
	}

	if (pid == 0) {
		setproctitle("mail.local");

		close(pipefd[0]);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		dup2(pipefd[1], 0);

		execlp(PATH_MAILLOCAL, "mail.local", "-f", sender, pw->pw_name, (void *)NULL);
		_exit(1);
	}

	if (seteuid(pw->pw_uid) == -1)
		fatal("privdrop failed");

	mdaproc = calloc(1, sizeof (struct mdaproc));
	if (mdaproc == NULL)
		fatal("calloc");
	mdaproc->pid = pid;

	SPLAY_INSERT(mdaproctree, &batchp->env->mdaproc_queue, mdaproc);

	close(pipefd[1]);
	return pipefd[0];
}

int
parent_maildir_init(struct passwd *pw, char *root)
{
	u_int8_t i;
	char pathname[MAXPATHLEN];
	char *subdir[] = { "/", "/tmp", "/cur", "/new" };

	for (i = 0; i < sizeof (subdir) / sizeof (char *); ++i) {
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
	int fd;
	char tmp[MAXPATHLEN];
	int mode = O_CREAT|O_RDWR|O_TRUNC|O_SYNC;

	if (! parent_maildir_init(pw, path)) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	if (! bsnprintf(tmp, sizeof(tmp), "%s/tmp/%s", path,
		batchp->message.message_uid)) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	fd = open(tmp, mode, 0600);
	if (fd == -1) {
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	return fd;
}

int
parent_mailfile_rename(struct batch *batchp, struct path *path)
{
	char srcpath[MAXPATHLEN];
	char dstpath[MAXPATHLEN];

	if (! bsnprintf(srcpath, sizeof(srcpath), "%s/tmp/%s",
		path->rule.r_value.path, batchp->message.message_uid) ||
	    ! bsnprintf(dstpath, sizeof(dstpath), "%s/new/%s",
		path->rule.r_value.path, batchp->message.message_uid))
		return 0;

	if (rename(srcpath, dstpath) == -1) {
		if (unlink(srcpath) == -1)
			fatal("unlink");
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return 0;
	}

	return 1;
}

int
parent_external_mda(char *path, struct passwd *pw, struct batch *batchp)
{
	pid_t pid;
	int pipefd[2];
	arglist args;
	char *word;
	struct mdaproc *mdaproc;

	log_debug("executing filter as user: %s", pw->pw_name);

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipefd) == -1) {
		if (errno == ENFILE) {
			log_warn("parent_external_mda: socketpair");
			batchp->message.status |= S_MESSAGE_TEMPFAILURE;
			return -1;
		}
		fatal("parent_external_mda: socketpair");
	}

	/* raise privileges before fork so that the child can
	 * revoke them permanently instead of inheriting the
	 * saved uid.
	 */
	if (seteuid(0) == -1)
		fatal("privraise failed");

	pid = fork();
	if (pid == -1) {
		log_warn("parent_external_mda: fork");
		close(pipefd[0]);
		close(pipefd[1]);
		batchp->message.status |= S_MESSAGE_TEMPFAILURE;
		return -1;
	}

	if (pid == 0) {
		setproctitle("external MDA");

		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			fatal("mta: cannot drop privileges");

		bzero(&args, sizeof(args));
		while ((word = strsep(&path, " \t")) != NULL)
			if (*word != '\0')
				addargs(&args, "%s", word);

		close(pipefd[0]);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		dup2(pipefd[1], 0);

		execvp(args.list[0], args.list);
		_exit(1);
	}

	if (seteuid(pw->pw_uid) == -1)
		fatal("privdrop failed");

	mdaproc = calloc(1, sizeof (struct mdaproc));
	if (mdaproc == NULL)
		fatal("calloc");
	mdaproc->pid = pid;

	SPLAY_INSERT(mdaproctree, &batchp->env->mdaproc_queue, mdaproc);

	close(pipefd[1]);
	return pipefd[0];
}

int
parent_filename_open(char *path, struct passwd *pw, struct batch *batchp)
{
	int fd;
	int mode = O_CREAT|O_APPEND|O_RDWR|O_SYNC|O_NONBLOCK;

	fd = open(path, mode, 0600);
	if (fd == -1) {
		/* XXX - this needs to be discussed ... */
		switch (errno) {
		case ENOTDIR:
		case ENOENT:
		case EACCES:
		case ELOOP:
		case EROFS:
		case EDQUOT:
		case EINTR:
		case EIO:
		case EMFILE:
		case ENFILE:
		case ENOSPC:
			batchp->message.status |= S_MESSAGE_TEMPFAILURE;
			break;
		case EWOULDBLOCK:
			goto lockfail;
		default:
			batchp->message.status |= S_MESSAGE_PERMFAILURE;
		}
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

	batchp->message.status |= S_MESSAGE_TEMPFAILURE|S_MESSAGE_LOCKFAILURE;
	return -1;
}

int
parent_forward_open(char *username)
{
	struct passwd *pw;
	struct stat sb;
	char pathname[MAXPATHLEN];
	int fd;

	pw = safe_getpwnam(username);
	if (pw == NULL)
		return -1;

	if (! bsnprintf(pathname, sizeof (pathname), "%s/.forward", pw->pw_dir))
		return -1;

	fd = open(pathname, O_RDONLY);
	if (fd == -1) {
		if (errno == ENOENT)
			goto clear;
		return -1;
	}

	/* make sure ~/ is not writable by anyone but owner */
	if (stat(pw->pw_dir, &sb) == -1)
		goto clearlog;

	if (sb.st_uid != pw->pw_uid || sb.st_mode & (S_IWGRP|S_IWOTH))
		goto clearlog;

	/* make sure ~/.forward is not writable by anyone but owner */
	if (fstat(fd, &sb) == -1)
		goto clearlog;

	if (sb.st_uid != pw->pw_uid || sb.st_mode & (S_IWGRP|S_IWOTH))
		goto clearlog;

	return fd;

clearlog:
	log_info("cannot process forward file for user %s due to wrong permissions", username);

clear:
	username[0] = '\0';
	return -1;
}

int
mdaproc_cmp(struct mdaproc *s1, struct mdaproc *s2)
{
	if (s1->pid < s2->pid)
		return (-1);

	if (s1->pid > s2->pid)
		return (1);

	return (0);
}

SPLAY_GENERATE(mdaproctree, mdaproc, mdaproc_nodes, mdaproc_cmp);
