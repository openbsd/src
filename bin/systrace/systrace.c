/*	$OpenBSD: systrace.c,v 1.19 2002/06/21 15:26:06 provos Exp $	*/
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
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "intercept.h"
#include "systrace.h"

pid_t pid;
int fd;
int connected = 0;		/* Connected to GUI */
int inherit = 0;		/* Inherit policy to childs */
int automatic = 0;		/* Do not run interactively */
int allow = 0;			/* Allow all and generate */
int userpolicy = 1;		/* Permit user defined policies */
char *username = NULL;		/* Username in automatic mode */
char cwd[MAXPATHLEN];		/* Current working directory of process */

short
trans_cb(int fd, pid_t pid, int policynr,
    char *name, int code, char *emulation,
    void *args, int argsize, struct intercept_tlq *tls, void *cbarg)
{
	short action, future;
	struct policy *policy;
	struct intercept_translate *tl;
	struct intercept_pid *ipid;
	struct intercept_replace repl;
	struct filterq *pflq = NULL;
	char output[_POSIX2_LINE_MAX], *p, *line;
	int size;

	action = ICPOLICY_PERMIT;

	if (policynr == -1)
		goto out;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: find %d", __func__, __LINE__,
		    policynr);

	if ((pflq = systrace_policyflq(policy, emulation, name)) == NULL)
		errx(1, "%s:%d: no filter queue", __func__, __LINE__);

	ipid = intercept_getpid(pid);
	ipid->uflags = 0;
	snprintf(output, sizeof(output),
	    "%s, pid: %d(%d), policy: %s, filters: %d, syscall: %s-%s(%d)",
	    ipid->name != NULL ? ipid->name : policy->name, pid, policynr,
	    policy->name, policy->nfilters, emulation, name, code);
	p = output + strlen(output);
	size = sizeof(output) - strlen(output);

	intercept_replace_init(&repl);
	TAILQ_FOREACH(tl, tls, next) {
		if (!tl->trans_valid)
			break;
		line = intercept_translate_print(tl);
		if (line == NULL)
			continue;

		snprintf(p, size, ", %s: %s", tl->name, line);
		p = output + strlen(output);
		size = sizeof(output) - strlen(output);

		intercept_replace_add(&repl, tl->off,
		    tl->trans_data, tl->trans_size);
	}

	action = filter_evaluate(tls, pflq, &ipid->uflags);
	if (action != ICPOLICY_ASK)
		goto replace;
	if (policy->flags & POLICY_UNSUPERVISED) {
		action = ICPOLICY_NEVER;
		syslog(LOG_WARNING, "user: %s, prog: %s", username, output);
		goto out;
	}

	action = filter_ask(tls, pflq, policynr, emulation, name,
	    output, &future, &ipid->uflags);
	if (future != ICPOLICY_ASK)
		systrace_modifypolicy(fd, policynr, name, future);

	if (policy->flags & POLICY_DETACHED) {
		if (intercept_detach(fd, pid) == -1)
			err(1, "intercept_detach");
	} else if (action == ICPOLICY_KILL) {
		kill(pid, SIGKILL);
		action = ICPOLICY_NEVER;
	}
 replace:
	if (action != ICPOLICY_NEVER) {
		/* If we can not rewrite the arguments, system call fails */
		if (intercept_replace(fd, pid, &repl) == -1)
			action = ICPOLICY_NEVER;
	}
 out:
	return (action);
}

short
gen_cb(int fd, pid_t pid, int policynr, char *name, int code,
    char *emulation, void *args, int argsize, void *cbarg)
{
	char output[_POSIX2_LINE_MAX];
	struct policy *policy;
	struct intercept_pid *ipid;
	short action = ICPOLICY_PERMIT;
	short future;

	if (policynr == -1)
		goto out;

	if ((policy = systrace_findpolnr(policynr)) == NULL)
		errx(1, "%s:%d: find %d", __func__, __LINE__,
		    policynr);

	ipid = intercept_getpid(pid);
	ipid->uflags = 0;
	snprintf(output, sizeof(output),
	    "%s, pid: %d(%d), policy: %s, filters: %d, syscall: %s-%s(%d), args: %d",
	    ipid->name != NULL ? ipid->name : policy->name, pid, policynr,
	    policy->name, policy->nfilters, emulation, name, code, argsize);

	if (policy->flags & POLICY_UNSUPERVISED) {
		action = ICPOLICY_NEVER;
		syslog(LOG_WARNING, "user: %s, prog: %s", username, output);
		goto out;
	}

	action = filter_ask(NULL, NULL, policynr, emulation, name,
	    output, &future, &ipid->uflags);
	if (future != ICPOLICY_ASK)
		systrace_modifypolicy(fd, policynr, name, future);

	if (policy->flags & POLICY_DETACHED) {
		if (intercept_detach(fd, pid) == -1)
			err(1, "intercept_detach");
	} else if (action == ICPOLICY_KILL) {
		kill(pid, SIGKILL);
		action = ICPOLICY_NEVER;
	}
 out:
	return (action);
}

void
execres_cb(int fd, pid_t pid, int policynr, char *emulation, char *name, void *arg)
{
	struct policy *policy;

	if (policynr != -1) {
		struct intercept_pid *ipid;

		if (inherit)
			return;

		ipid = intercept_getpid(pid);
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
child_handler(int sig)
{
	int s = errno, status;

	if (signal(SIGCHLD, child_handler) == SIG_ERR) {
		close(fd);
	}

	while (wait4(-1, &status, WNOHANG, NULL) > 0)
		;

	errno = s;
}

#define X(x)	if ((x) == -1) \
	err(1, "%s:%d: intercept failed", __func__, __LINE__)

void
systrace_initcb(void)
{
	X(intercept_init());

	X(intercept_register_gencb(gen_cb, NULL));
	X(intercept_register_sccb("native", "open", trans_cb, NULL));
	X(intercept_register_transfn("native", "open", 0));
	X(intercept_register_translation("native", "open", 1, &oflags));

	X(intercept_register_sccb("native", "connect", trans_cb, NULL));
	X(intercept_register_translation("native", "connect", 1,
	    &ic_translate_connect));
	X(intercept_register_sccb("native", "sendto", trans_cb, NULL));
	X(intercept_register_translation("native", "sendto", 4,
	    &ic_translate_connect));
	X(intercept_register_sccb("native", "bind", trans_cb, NULL));
	X(intercept_register_translation("native", "bind", 1,
	    &ic_translate_connect));
	X(intercept_register_sccb("native", "execve", trans_cb, NULL));
	X(intercept_register_transfn("native", "execve", 0));
	X(intercept_register_sccb("native", "stat", trans_cb, NULL));
	X(intercept_register_transfn("native", "stat", 0));
	X(intercept_register_sccb("native", "lstat", trans_cb, NULL));
	X(intercept_register_translink("native", "lstat", 0));
	X(intercept_register_sccb("native", "unlink", trans_cb, NULL));
	X(intercept_register_transfn("native", "unlink", 0));
	X(intercept_register_sccb("native", "chown", trans_cb, NULL));
	X(intercept_register_transfn("native", "chown", 0));
	X(intercept_register_translation("native", "chown", 1, &uidt));
	X(intercept_register_translation("native", "chown", 2, &gidt));
	X(intercept_register_sccb("native", "fchown", trans_cb, NULL));
	X(intercept_register_translation("native", "fchown", 0, &fdt));
	X(intercept_register_translation("native", "fchown", 1, &uidt));
	X(intercept_register_translation("native", "fchown", 2, &gidt));
	X(intercept_register_sccb("native", "chmod", trans_cb, NULL));
	X(intercept_register_transfn("native", "chmod", 0));
	X(intercept_register_translation("native", "chmod", 1, &modeflags));
	X(intercept_register_sccb("native", "readlink", trans_cb, NULL));
	X(intercept_register_translink("native", "readlink", 0));
	X(intercept_register_sccb("native", "chdir", trans_cb, NULL));
	X(intercept_register_transfn("native", "chdir", 0));
	X(intercept_register_sccb("native", "access", trans_cb, NULL));
	X(intercept_register_transfn("native", "access", 0));
	X(intercept_register_sccb("native", "mkdir", trans_cb, NULL));
	X(intercept_register_transfn("native", "mkdir", 0));
	X(intercept_register_sccb("native", "rmdir", trans_cb, NULL));
	X(intercept_register_transfn("native", "rmdir", 0));
	X(intercept_register_sccb("native", "rename", trans_cb, NULL));
	X(intercept_register_transfn("native", "rename", 0));
	X(intercept_register_transfn("native", "rename", 1));
	X(intercept_register_sccb("native", "symlink", trans_cb, NULL));
	X(intercept_register_transstring("native", "symlink", 0));
	X(intercept_register_translink("native", "symlink", 1));

	X(intercept_register_sccb("linux", "open", trans_cb, NULL));
	X(intercept_register_translink("linux", "open", 0));
	X(intercept_register_translation("linux", "open", 1, &linux_oflags));
	X(intercept_register_sccb("linux", "stat", trans_cb, NULL));
	X(intercept_register_translink("linux", "stat", 0));
	X(intercept_register_sccb("linux", "lstat", trans_cb, NULL));
	X(intercept_register_translink("linux", "lstat", 0));
	X(intercept_register_sccb("linux", "execve", trans_cb, NULL));
	X(intercept_register_translink("linux", "execve", 0));
	X(intercept_register_sccb("linux", "access", trans_cb, NULL));
	X(intercept_register_translink("linux", "access", 0));
	X(intercept_register_sccb("linux", "symlink", trans_cb, NULL));
	X(intercept_register_transstring("linux", "symlink", 0));
	X(intercept_register_translink("linux", "symlink", 1));
	X(intercept_register_sccb("linux", "readlink", trans_cb, NULL));
	X(intercept_register_translink("linux", "readlink", 0));
	X(intercept_register_sccb("linux", "rename", trans_cb, NULL));
	X(intercept_register_translink("linux", "rename", 0));
	X(intercept_register_translink("linux", "rename", 1));
	X(intercept_register_sccb("linux", "mkdir", trans_cb, NULL));
	X(intercept_register_translink("linux", "mkdir", 0));
	X(intercept_register_sccb("linux", "rmdir", trans_cb, NULL));
	X(intercept_register_translink("linux", "rmdir", 0));
	X(intercept_register_sccb("linux", "unlink", trans_cb, NULL));
	X(intercept_register_translink("linux", "unlink", 0));
	X(intercept_register_sccb("linux", "chmod", trans_cb, NULL));
	X(intercept_register_translink("linux", "chmod", 0));
	X(intercept_register_translation("linux", "chmod", 1, &modeflags));

	X(intercept_register_execcb(execres_cb, NULL));
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: systrace [-ait] [-g gui] [-f policy] [-p pid] command ...\n");
	exit(1);
}

int
requestor_start(char *path)
{
	char *argv[2];
	int pair[2];
	pid_t pid;

	argv[0] = path;
	argv[1] = NULL;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
		err(1, "socketpair");

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		close(pair[0]);
		if (dup2(pair[1], fileno(stdin)) == -1)
			err(1, "dup2");
		if (dup2(pair[1], fileno(stdout)) == -1)
			err(1, "dup2");
		setlinebuf(stdout);

		close(pair[1]);

		execvp(path, argv);

		err(1, "execvp: %s", path);
	}

	close(pair[1]);
	if (dup2(pair[0], fileno(stdin)) == -1)
		err(1, "dup2");

	if (dup2(pair[0], fileno(stdout)) == -1)
		err(1, "dup2");

	close(pair[0]);

	setlinebuf(stdout);

	connected = 1;

	return (0);
}

int
main(int argc, char **argv)
{
	int i, c;
	char **args;
	char *filename = NULL;
	char *guipath = _PATH_XSYSTRACE;
	pid_t pidattach = 0;
	int usex11 = 1;

	while ((c = getopt(argc, argv, "aAitUg:f:p:")) != -1) {
		switch (c) {
		case 'a':
			automatic = 1;
			break;
		case 'A':
			allow = 1;
			break;
		case 'i':
			inherit = 1;
			break;
		case 'g':
			guipath = optarg;
			break;
		case 'f':
			filename = optarg;
			break;
		case 'p':
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

	/* Username for automatic mode, and policy predicates */
	username = uid_to_name(getuid());

	/* Determine current working directory for filtering */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		err(1, "getcwd");

	if (signal(SIGCHLD, child_handler) == SIG_ERR)
		err(1, "signal");

	/* Local initalization */
	systrace_initpolicy(filename);
	systrace_initcb();

	if ((fd = intercept_open()) == -1)
		exit(1);

	if (pidattach == 0) {
		/* Run a command and attach to it */
		if ((args = malloc((argc + 1) * sizeof(char *))) == NULL)
			err(1, "malloc");

		for (i = 0; i < argc; i++)
			args[i] = argv[i];
		args[i] = NULL;

		pid = intercept_run(fd, args[0], args);
		if (pid == -1)
			err(1, "fork");

		if (intercept_attach(fd, pid) == -1)
			err(1, "attach");

		if (kill(pid, SIGCONT) == -1)
			err(1, "kill");
	} else {
		/* Attach to a running command */

		if (intercept_attachpid(fd, pidattach, argv[0]) == -1)
			err(1, "attachpid");
	}

	/* Start the policy gui if necessary */
	if (usex11 && !automatic && !allow)
		requestor_start(guipath);

	while (intercept_read(fd) != -1)
		if (!intercept_existpids())
			break;

	if (userpolicy)
		systrace_dumppolicy();

	close(fd);

	exit(0);
}
