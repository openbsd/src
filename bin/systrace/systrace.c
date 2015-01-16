/*	$OpenBSD: systrace.c,v 1.62 2015/01/16 00:19:12 deraadt Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <event.h>

#include "intercept.h"
#include "systrace.h"
#include "util.h"

#define CRADLE_SERVER "cradle_server"
#define CRADLE_UI     "cradle_ui"

#define VERSION "1.6d (OpenBSD)"

pid_t trpid;
int trfd;
int connected = 0;		/* Connected to GUI */
int inherit = 0;		/* Inherit policy to childs */
int automatic = 0;		/* Do not run interactively */
int allow = 0;			/* Allow all and generate */
int userpolicy = 1;		/* Permit user defined policies */
int noalias = 0;		/* Do not do system call aliasing */
int iamroot = 0;		/* Set if we are running as root */
int cradle = 0;			/* Set if we are running in cradle mode */
int logtofile = 0;		/* Log to file instead of syslog */
FILE *logfile;			/* default logfile to send to if enabled */
char cwd[PATH_MAX];		/* Current working directory */
char home[PATH_MAX];		/* Home directory of user */
char username[LOGIN_NAME_MAX];	/* Username: predicate match and expansion */
char *guipath = _PATH_XSYSTRACE; /* Path to GUI executable */
char dirpath[PATH_MAX];

static struct event ev_read;
static struct event ev_timeout;

static void child_handler(int);
static void log_msg(int, const char *, ...);
static void systrace_read(int, short, void *);
static void systrace_timeout(int, short, void *);
static void usage(void);

void
systrace_parameters(void)
{
	struct passwd *pw;
	char *normcwd;
	uid_t uid = getuid();

	iamroot = getuid() == 0;

	/* Find out current username. */
	if ((pw = getpwuid(uid)) == NULL) {
		snprintf(username, sizeof(username), "uid %u", uid);
	} else {
		strlcpy(username, pw->pw_name, sizeof(username));
		strlcpy(home, pw->pw_dir, sizeof(home));
	}

	/* Determine current working directory for filtering */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd");
	if ((normcwd = normalize_filename(-1, 0, cwd, ICLINK_ALL)) == NULL)
		errx(1, "normalize_filename");
	if (strlcpy(cwd, normcwd, sizeof(cwd)) >= sizeof(cwd))
		errx(1, "cwd too long");
}

/*
 * Generate human readable output and setup replacements if available.
 */

void
make_output(char *output, size_t outlen, const char *binname,
    pid_t pid, pid_t ppid,
    int policynr, const char *policy, int nfilters, const char *emulation,
    const char *name, int code, struct intercept_tlq *tls,
    struct intercept_replace *repl)
{
	struct intercept_translate *tl;
	char *p, *line;
	int size;

	snprintf(output, outlen,
	    "%s, pid: %d(%d)[%d], policy: %s, filters: %d, syscall: %s-%s(%d)",
	    binname, pid, policynr, ppid, policy, nfilters,
	    emulation, name, code);

	p = output + strlen(output);
	size = outlen - strlen(output);

	if (tls == NULL)
		return;

	TAILQ_FOREACH(tl, tls, next) {
		if (!tl->trans_valid)
			continue;
		line = intercept_translate_print(tl);
		if (line == NULL)
			continue;

		snprintf(p, size, ", %s: %s", tl->name, strescape(line));
		p = output + strlen(output);
		size = outlen - strlen(output);

		if (repl != NULL && tl->trans_size)
			intercept_replace_add(repl, tl->off,
			    tl->trans_data, tl->trans_size,
			    tl->trans_flags);
	}
}

short
trans_cb(int fd, pid_t pid, int policynr,
    const char *name, int code, const char *emulation,
    void *args, int argsize,
    struct intercept_replace *repl,
    struct intercept_tlq *tls, void *cbarg)
{
	short action, future;
	struct policy *policy;
	struct intercept_pid *ipid;
	struct intercept_tlq alitls;
	struct intercept_translate alitl[SYSTRACE_MAXALIAS];
	struct systrace_alias *alias = NULL;
	struct filterq *pflq = NULL;
	const char *binname = NULL;
	char output[_POSIX2_LINE_MAX];
	pid_t ppid;
	int done = 0, dolog = 0;

	action = ICPOLICY_PERMIT;

	if (policynr == -1)
		goto out;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: find %d", __func__, __LINE__,
		    policynr);

	ipid = intercept_getpid(pid);
	ipid->uflags = 0;
	binname = ipid->name != NULL ? ipid->name : policy->name;
	ppid = ipid->ppid;

	/* Required to set up replacements */
	do {
		make_output(output, sizeof(output), binname, pid, ppid,
		    policynr, policy->name, policy->nfilters,
		    emulation, name, code, tls, repl);

		/* Fast-path checking */
		if ((action = policy->kerneltable[code]) != ICPOLICY_ASK)
			goto out;

		pflq = systrace_policyflq(policy, emulation, name);
		if (pflq == NULL)
			errx(1, "%s:%d: no filter queue", __func__, __LINE__);

		action = filter_evaluate(tls, pflq, ipid);
		if (action != ICPOLICY_ASK)
			goto done;

		/* Do aliasing here */
		if (!noalias)
			alias = systrace_find_alias(emulation, name);
		if (alias != NULL) {
			int i;

			/* Set up variables for further filter actions */
			tls = &alitls;
			emulation = alias->aemul;
			name = alias->aname;

			/* Create an aliased list for filter_evaluate */
			TAILQ_INIT(tls);
			for (i = 0; i < alias->nargs; i++) {
				memcpy(&alitl[i], alias->arguments[i], 
				    sizeof(struct intercept_translate));
				TAILQ_INSERT_TAIL(tls, &alitl[i], next);
			}

			if ((pflq = systrace_policyflq(policy,
			    alias->aemul, alias->aname)) == NULL)
				errx(1, "%s:%d: no filter queue",
				    __func__, __LINE__);

			action = filter_evaluate(tls, pflq, ipid);
			if (action != ICPOLICY_ASK)
				goto done;

			make_output(output, sizeof(output), binname, pid, ppid,
			    policynr, policy->name, policy->nfilters,
			    alias->aemul, alias->aname, code, tls, NULL);
		}

		/*
		 * At this point, we have to ask the user, but we may check
		 * if the policy has been updated in the meanwhile.
		 */
		if (systrace_updatepolicy(fd, policy) == -1)
			done = 1;
	} while (!done);

	if (policy->flags & POLICY_UNSUPERVISED) {
		action = ICPOLICY_NEVER;
		dolog = 1;
		goto out;
	}

	action = filter_ask(fd, tls, pflq, policynr, emulation, name,
	    output, &future, ipid);
	if (future != ICPOLICY_ASK)
		filter_modifypolicy(fd, policynr, emulation, name, future);

	if (policy->flags & POLICY_DETACHED) {
		if (intercept_detach(fd, pid) == -1)
			err(1, "intercept_detach");
		return (action);
	} else if (action == ICPOLICY_KILL) {
		kill(pid, SIGKILL);
		return (ICPOLICY_NEVER);
	}
 done:
	if (ipid->uflags & SYSCALL_LOG)
		dolog = 1;

 out:
	if (dolog)
		log_msg(LOG_WARNING, "%s user: %s, prog: %s",
		    action < ICPOLICY_NEVER ? "permit" : "deny",
		    ipid->username, output);

 	/* Argument replacement in intercept might still fail */

	return (action);
}

short
gen_cb(int fd, pid_t pid, int policynr, const char *name, int code,
    const char *emulation, void *args, int argsize, void *cbarg)
{
	char output[_POSIX2_LINE_MAX];
	struct policy *policy;
	struct intercept_pid *ipid;
	struct filterq *pflq = NULL;
	short action = ICPOLICY_PERMIT;
	short future;
	int off, done = 0, dolog = 0;
	size_t len;

	if (policynr == -1)
		goto out;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: find %d", __func__, __LINE__,
		    policynr);

	ipid = intercept_getpid(pid);
	ipid->uflags = 0;

	make_output(output, sizeof(output),
	    ipid->name != NULL ? ipid->name : policy->name,
	    pid, ipid->ppid, policynr,
	    policy->name, policy->nfilters, emulation, name, code,
	    NULL, NULL);

	off = strlen(output);
	len = sizeof(output) - off;
	if (len > 0)
		snprintf(output + off, len, ", args: %d", argsize);

	if ((pflq = systrace_policyflq(policy, emulation, name)) == NULL)
		errx(1, "%s:%d: no filter queue", __func__, __LINE__);

	do {
		/* Fast-path checking */
		if ((action = policy->kerneltable[code]) != ICPOLICY_ASK)
			goto out;

		action = filter_evaluate(NULL, pflq, ipid);

		if (action != ICPOLICY_ASK)
			goto haveresult;
		/*
		 * At this point, we have to ask the user, but we may check
		 * if the policy has been updated in the meanwhile.
		 */
		if (systrace_updatepolicy(fd, policy) == -1)
			done = 1;
	} while (!done);

	if (policy->flags & POLICY_UNSUPERVISED) {
		action = ICPOLICY_NEVER;
		dolog = 1;
		goto haveresult;
	}

	action = filter_ask(fd, NULL, pflq, policynr, emulation, name,
	    output, &future, ipid);
	if (future != ICPOLICY_ASK)
		systrace_modifypolicy(fd, policynr, name, future);

	if (policy->flags & POLICY_DETACHED) {
		if (intercept_detach(fd, pid) == -1)
			err(1, "intercept_detach");
	} else if (action == ICPOLICY_KILL) {
		kill(pid, SIGKILL);
		return (ICPOLICY_NEVER);
	}

 haveresult:
	if (ipid->uflags & SYSCALL_LOG)
		dolog = 1;
	if (dolog)
		log_msg(LOG_WARNING, "%s user: %s, prog: %s",
		    action < ICPOLICY_NEVER ? "permit" : "deny",
		    ipid->username, output);
 out:
	return (action);
}

void
execres_cb(int fd, pid_t pid, int policynr, const char *emulation,
    const char *name, void *arg)
{
	struct policy *policy;

	if (policynr != -1) {
		struct intercept_pid *ipid;

		ipid = intercept_getpid(pid);
		if (ipid->uflags & PROCESS_DETACH) {
			if (intercept_detach(fd, pid) == -1)
				err(1, "%s: intercept_detach", __func__);
			return;
		}
		if (inherit)
			return;

		if (ipid->uflags & PROCESS_INHERIT_POLICY)
			return;
	}
	if ((policy = systrace_newpolicy(emulation, name)) == NULL)
		goto error;

	/* See if this policies runs without interactive feedback */
	if (automatic)
		policy->flags |= POLICY_UNSUPERVISED;

	policynr = policy->policynr;

	/* Try to find existing policy in file system */
	if (policynr == -1 && TAILQ_FIRST(&policy->prefilters) == NULL)
		systrace_addpolicy(name);

	if (policy->flags & POLICY_DETACHED) {
		if (intercept_detach(fd, pid) == -1)
			err(1, "intercept_detach");
		return;
	}

	if (policynr == -1) {
		policynr = systrace_newpolicynr(fd, policy);
		if (policynr == -1)
			goto error;
	}

	if (intercept_assignpolicy(fd, pid, policynr) == -1)
		goto error;

	if (TAILQ_FIRST(&policy->prefilters) != NULL)
		filter_prepolicy(fd, policy);

	return;

 error:
	kill(pid, SIGKILL);
	fprintf(stderr, "Terminating %d: %s\n", pid, name);
}

void
policyfree_cb(int policynr, void *arg)
{
	struct policy *policy;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: find %d", __func__, __LINE__,
		    policynr);

	systrace_freepolicy(policy);
}

/* ARGSUSED */
static void
child_handler(int sig)
{
	int s = errno, status;

	if (signal(SIGCHLD, child_handler) == SIG_ERR) {
		close(trfd);
	}

	while (wait4(-1, &status, WNOHANG, NULL) > 0)
		;

	errno = s;
}

static void
log_msg(int priority, const char *fmt, ...)
{
	char buf[_POSIX2_LINE_MAX];
	extern char *__progname;
	va_list ap;

	va_start(ap, fmt);

	if (logtofile) {
		vsnprintf(buf, sizeof(buf), fmt, ap);
		fprintf(logfile, "%s: %s\n", __progname, buf);
	} else
		vsyslog(priority, fmt, ap);

	va_end(ap);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: systrace [-AaCeitUuV] [-c user:group] [-d policydir] [-E logfile]\n"
	    "\t [-f file] [-g gui] [-p pid] command ...\n");
	exit(1);
}

int
requestor_start(char *path, int docradle)
{
	char *argv[3];
	int pair[2];
	pid_t pid;

	argv[0] = path;
	argv[1] = docradle ? "-C" : NULL;
	argv[2] = NULL;

	if (!docradle && socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		err(1, "socketpair");

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		if (!docradle) {
			close(pair[0]);
			if (dup2(pair[1], fileno(stdin)) == -1)
				err(1, "dup2");
			if (dup2(pair[1], fileno(stdout)) == -1)
				err(1, "dup2");
			setvbuf(stdout, NULL, _IOLBF, 0);

			close(pair[1]);
		}

		execvp(path, argv);

		err(1, "execvp: %s", path);

	}

	if (!docradle) {
		close(pair[1]);
		if (dup2(pair[0], fileno(stdin)) == -1)
			err(1, "dup2");

		if (dup2(pair[0], fileno(stdout)) == -1)
			err(1, "dup2");

		close(pair[0]);

		setvbuf(stdout, NULL, _IOLBF, 0);

		connected = 1;
	}

	return (0);
}


static void
cradle_setup(char *pathtogui)
{
	struct stat sb;
	char cradlepath[PATH_MAX], cradleuipath[PATH_MAX];

	snprintf(dirpath, sizeof(dirpath), "/tmp/systrace-%d", getuid());

	if (stat(dirpath, &sb) == -1) {
		if (errno != ENOENT)
			err(1, "stat()");
		if (mkdir(dirpath, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
			err(1, "mkdir()");
	} else {
		if (sb.st_uid != getuid())
			errx(1, "Wrong owner on directory %s", dirpath);
		if (sb.st_mode != (S_IFDIR | S_IRUSR | S_IWUSR | S_IXUSR))
			errx(1, "Wrong permissions on directory %s", dirpath);
	}

	strlcpy(cradlepath, dirpath, sizeof (cradlepath));
	strlcat(cradlepath, "/" CRADLE_SERVER, sizeof (cradlepath));

	strlcpy(cradleuipath, dirpath, sizeof (cradleuipath));
	strlcat(cradleuipath, "/" CRADLE_UI, sizeof (cradleuipath));

	cradle_start(cradlepath, cradleuipath, pathtogui);
}

static int
get_uid_gid(const char *argument, uid_t *uid, gid_t *gid)
{
	struct group *gp;
	struct passwd *pw;
	unsigned long ulval;
	char uid_gid_str[128];
	char *endp, *g, *u;

	strlcpy(uid_gid_str, argument, sizeof(uid_gid_str));
	g = uid_gid_str;
	u = strsep(&g, ":");

	if ((pw = getpwnam(u)) != NULL) {
		explicit_bzero(pw->pw_passwd, strlen(pw->pw_passwd));
		*uid = pw->pw_uid;
		*gid = pw->pw_gid;
		/* Ok if group not specified. */
		if (g == NULL)
			return (0);
	} else {
		errno = 0;
		ulval = strtoul(u, &endp, 10);
		if (u[0] == '\0' || *endp != '\0')
			errx(1, "no such user '%s'", u);
		if (errno == ERANGE && ulval == ULONG_MAX)
			errx(1, "invalid uid %s", u);
		*uid = (uid_t)ulval;
	}

	if (g == NULL)
		return (-1);

	if ((gp = getgrnam(g)) != NULL)
		*gid = gp->gr_gid;
	else {
		errno = 0;
		ulval = strtoul(g, &endp, 10);
		if (g[0] == '\0' || *endp != '\0')
			errx(1, "no such group '%s'", g);
		if (errno == ERANGE && ulval == ULONG_MAX)
			errx(1, "invalid gid %s", g);
		*gid = (gid_t)ulval;
	}

	return (0);
}

static void
systrace_timeout(int fd, short what, void *arg)
{
	struct timeval tv;

	/* Reschedule timeout */
	timerclear(&tv);
	tv.tv_sec = SYSTRACE_UPDATETIME;
	evtimer_add(&ev_timeout, &tv);

	systrace_updatepolicies(trfd);
	if (userpolicy)
		systrace_dumppolicies(trfd);
}

/*
 * Read from the kernel if something happened.
 */

static void
systrace_read(int fd, short what, void *arg)
{
	intercept_read(fd);

	if (!intercept_existpids()) {
		event_del(&ev_read);
		event_del(&ev_timeout);
	}
}

int
main(int argc, char **argv)
{
	int i, c;
	char **args;
	char *filename = NULL;
	char *policypath = NULL;
	struct timeval tv;
	pid_t pidattach = 0;
	int usex11 = 1;
	int background;
	int setcredentials = 0;
	uid_t cr_uid;
	gid_t cr_gid;

	while ((c = getopt(argc, argv, "Vc:aAeE:ituUCd:g:f:p:")) != -1) {
		switch (c) {
		case 'V':
			fprintf(stderr, "%s V%s\n", argv[0], VERSION);
			exit(0);
		case 'c':
			setcredentials = 1;
			if (get_uid_gid(optarg, &cr_uid, &cr_gid) == -1)
				usage();
			break;
		case 'a':
			if (allow)
				usage();
			automatic = 1;
			break;
		case 'd':
			policypath = optarg;
			break;
		case 'e':
			logtofile = 1;
			logfile = stderr;
			break;
		case 'E':
			logtofile = 1;
			logfile = fopen(optarg, "a");
			if (logfile == NULL)
				err(1, "Cannot open \"%s\" for writing",
				    optarg);
			break;
		case 'A':
			if (automatic)
				usage();
			allow = 1;
			break;
		case 'u':
			noalias = 1;
			break;
		case 'i':
			inherit = 1;
			break;
		case 'g':
			guipath = optarg;
			break;
		case 'C':
			cradle = 1;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'p':
			if (setcredentials)
				usage();
			if ((pidattach = atoi(optarg)) == 0) {
				warnx("bad pid: %s", optarg);
				usage();
			}
			break;
		case 't':
			usex11 = 0;
			break;
		case 'U':
			userpolicy = 0;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 || (pidattach && *argv[0] != '/'))
		usage();

	systrace_parameters();

	if (setcredentials && !iamroot) {
		fprintf(stderr, "Need to be root to change credentials.\n");
		usage();
	}

	/* Initialize libevent but without kqueue because of systrace fd */
	setenv("EVENT_NOKQUEUE", "yes", 0);
	event_init();

	/* Local initialization */
	systrace_initalias();
	systrace_initpolicy(filename, policypath);
	systrace_initcb();

	if ((trfd = intercept_open()) == -1)
		exit(1);

	/* See if we can run the systrace process in the background */
	background = usex11 || automatic || allow;

	if (pidattach == 0) {
		/* Run a command and attach to it */
		args = reallocarray(NULL, argc + 1, sizeof(char *));
		if (args == NULL)
			err(1, "malloc");

		for (i = 0; i < argc; i++)
			args[i] = argv[i];
		args[i] = NULL;

		if (setcredentials)
			trpid = intercept_run(background, &trfd,
			    cr_uid, cr_gid, args[0], args);
		else
			trpid = intercept_run(background, &trfd, 0, 0,
			    args[0], args);
		if (trpid == -1)
			err(1, "fork");

		if (intercept_attach(trfd, trpid) == -1)
			err(1, "attach");

		if (kill(trpid, SIGUSR1) == -1)
			err(1, "kill");
	} else {
		/* Attach to a running command */
		if (intercept_attachpid(trfd, pidattach, argv[0]) == -1)
			err(1, "attachpid");

		if (background) {
			if (daemon(1, 1) == -1)
				err(1, "daemon");
		}
	}

	if (signal(SIGCHLD, child_handler) == SIG_ERR)
		err(1, "signal");

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		err(1, "signal");

	/* Start the policy GUI or cradle if necessary */
	if (usex11 && (!automatic && !allow)) {
		if (cradle)
			cradle_setup(guipath);
		else
			requestor_start(guipath, 0);

	}

	/* Register read events */
	event_set(&ev_read, trfd, EV_READ|EV_PERSIST, systrace_read, NULL);
	event_add(&ev_read, NULL);

	if (userpolicy || automatic) {
		evtimer_set(&ev_timeout, systrace_timeout, &ev_timeout);
		timerclear(&tv);
		tv.tv_sec = SYSTRACE_UPDATETIME;
		evtimer_add(&ev_timeout, &tv);
	}

	/* Wait for events */
	event_dispatch();

	if (userpolicy)
		systrace_dumppolicies(trfd);

	close(trfd);

	exit(0);
}
