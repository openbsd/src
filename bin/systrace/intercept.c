/*	$OpenBSD: intercept.c,v 1.58 2011/10/18 20:01:28 matthew Exp $	*/
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
#include <sys/tree.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <libgen.h>
#include <pwd.h>

#include "intercept.h"

void simplify_path(char *);
char *intercept_realpath(const char *, char *);

struct intercept_syscall {
	SPLAY_ENTRY(intercept_syscall) node;

	char name[64];
	char emulation[16];

	short (*cb)(int, pid_t, int, const char *, int, const char *, void *,
	    int, struct intercept_replace *, struct intercept_tlq *, void *);
	void *cb_arg;

	struct intercept_tlq tls;
};

static int sccompare(struct intercept_syscall *, struct intercept_syscall *);
static int pidcompare(struct intercept_pid *, struct intercept_pid *);
static struct intercept_syscall *intercept_sccb_find(const char *,
    const char *);
static void sigusr1_handler(int);

static SPLAY_HEAD(pidtree, intercept_pid) pids;
static SPLAY_HEAD(sctree, intercept_syscall) scroot;

static volatile int got_sigusr1 = 0;

/* Generic callback functions */

void (*intercept_newimagecb)(int, pid_t, int, const char *, const char *, void *) = NULL;
void *intercept_newimagecbarg = NULL;
short (*intercept_gencb)(int, pid_t, int, const char *, int, const char *, void *, int, void *) = NULL;
void *intercept_gencbarg = NULL;
void (*intercept_pfreecb)(int, void*);
void *intercept_pfreearg = NULL;

int
sccompare(struct intercept_syscall *a, struct intercept_syscall *b)
{
	int diff;

	diff = strcmp(a->emulation, b->emulation);
	if (diff)
		return (diff);
	return (strcmp(a->name, b->name));
}

int
pidcompare(struct intercept_pid *a, struct intercept_pid *b)
{
	int diff = a->pid - b->pid;

	if (diff == 0)
		return (0);
	if (diff > 0)
		return (1);
	return (-1);
}

SPLAY_PROTOTYPE(sctree, intercept_syscall, node, sccompare)
SPLAY_GENERATE(sctree, intercept_syscall, node, sccompare)

SPLAY_PROTOTYPE(pidtree, intercept_pid, next, pidcompare)
SPLAY_GENERATE(pidtree, intercept_pid, next, pidcompare)

extern struct intercept_system intercept;
int ic_abort;

int
intercept_init(void)
{
	SPLAY_INIT(&pids);
	SPLAY_INIT(&scroot);

	intercept_newimagecb = NULL;
	intercept_gencb = NULL;

	return (intercept.init());
}

struct intercept_syscall *
intercept_sccb_find(const char *emulation, const char *name)
{
	struct intercept_syscall tmp;

	strlcpy(tmp.name, name, sizeof(tmp.name));
	strlcpy(tmp.emulation, emulation, sizeof(tmp.emulation));
	return (SPLAY_FIND(sctree, &scroot, &tmp));
}

struct intercept_translate *
intercept_register_translation(char *emulation, char *name, int offset,
    struct intercept_translate *tl)
{
	struct intercept_syscall *tmp;
	struct intercept_translate *tlnew;

	if (offset >= INTERCEPT_MAXSYSCALLARGS)
		errx(1, "%s: %s-%s: offset too large",
		    __func__, emulation, name);

	tmp = intercept_sccb_find(emulation, name);
	if (tmp == NULL)
		errx(1, "%s: %s-%s: can't find call back",
		    __func__, emulation, name);

	tlnew = malloc(sizeof(struct intercept_translate));
	if (tlnew == NULL)
		err(1, "%s: %s-%s: malloc",
		    __func__, emulation, name);

	memcpy(tlnew, tl, sizeof(struct intercept_translate));
	tlnew->off = offset;

	TAILQ_INSERT_TAIL(&tmp->tls, tlnew, next);

	return (tlnew);
}

void *
intercept_sccb_cbarg(char *emulation, char *name)
{
	struct intercept_syscall *tmp;

	if ((tmp = intercept_sccb_find(emulation, name)) == NULL)
		return (NULL);

	return (tmp->cb_arg);
}

int
intercept_register_sccb(char *emulation, char *name,
    short (*cb)(int, pid_t, int, const char *, int, const char *, void *, int,
	struct intercept_replace *, struct intercept_tlq *, void *),
    void *cbarg)
{
	struct intercept_syscall *tmp;

	if (intercept_sccb_find(emulation, name))
		return (-1);

	if (intercept.getsyscallnumber(emulation, name) == -1) {
		warnx("%s: %d: unknown syscall: %s-%s", __func__, __LINE__,
		    emulation, name);
		return (-1);
	}

	if ((tmp = calloc(1, sizeof(struct intercept_syscall))) == NULL) {
		warn("%s:%d: malloc", __func__, __LINE__);
		return (-1);
	}

	TAILQ_INIT(&tmp->tls);
	strlcpy(tmp->name, name, sizeof(tmp->name));
	strlcpy(tmp->emulation, emulation, sizeof(tmp->emulation));
	tmp->cb = cb;
	tmp->cb_arg = cbarg;

	SPLAY_INSERT(sctree, &scroot, tmp);

	return (0);
}

int
intercept_register_gencb(short (*cb)(int, pid_t, int, const char *, int, const char *, void *, int, void *), void *arg)
{
	intercept_gencb = cb;
	intercept_gencbarg = arg;

	return (0);
}

int
intercept_register_execcb(void (*cb)(int, pid_t, int, const char *, const char *, void *), void *arg)
{
	intercept_newimagecb = cb;
	intercept_newimagecbarg = arg;

	return (0);
}

int
intercept_register_pfreecb(void (*cb)(int, void *), void *arg)
{
	intercept_pfreecb = cb;
	intercept_pfreearg = arg;

	return (0);
}

/* ARGSUSED */
static void
sigusr1_handler(int signum)
{
	/* all we need to do is pretend to handle it */
	got_sigusr1 = 1;
}

void
intercept_setpid(struct intercept_pid *icpid, uid_t uid, gid_t gid)
{
	struct passwd *pw;

	icpid->uid = uid;
	icpid->gid = gid;
	if ((pw = getpwuid(icpid->uid)) == NULL) {
		snprintf(icpid->username, sizeof(icpid->username),
		    "unknown(%d)", icpid->uid);
		strlcpy(icpid->home, "/var/empty", sizeof(icpid->home));
	} else {
		strlcpy(icpid->username, pw->pw_name, sizeof(icpid->username));
		strlcpy(icpid->home, pw->pw_dir, sizeof(icpid->home));
	}
}

pid_t
intercept_run(int bg, int fd, uid_t uid, gid_t gid,
    char *path, char *const argv[])
{
	struct intercept_pid *icpid;
	sigset_t none, set, oset;
	sig_t ohandler;
	pid_t pid, cpid;
	int status;

	/* Block signals so that timing on signal delivery does not matter */
	sigemptyset(&none);
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	if (sigprocmask(SIG_BLOCK, &set, &oset) == -1)
		err(1, "sigprocmask");
	ohandler = signal(SIGUSR1, sigusr1_handler);
	if (ohandler == SIG_ERR)
		err(1, "signal");

	pid = getpid();
	cpid = fork();
	if (cpid == -1)
		return (-1);

	/*
	 * If the systrace process should be in the background and we're
	 * the parent, or vice versa.
	 */
	if ((!bg && cpid == 0) || (bg && cpid != 0)) {
		/* Needs to be closed */
		close(fd);

		if (bg) {
			/* Wait for child to "detach" */
			cpid = wait(&status);
			if (cpid == -1)
				err(1, "wait");
			if (status != 0)
				errx(1, "wait: child gave up");
		}

		/* Sleep */
		sigsuspend(&none);

		if (!got_sigusr1)
			errx(1, "wrong signal");

		/*
		 * Woken up, restore signal handling state.
		 *
		 * Note that there is either no child or we have no idea
		 * what pid it might have at this point.  If we fail.
		 */
		if (signal(SIGUSR1, ohandler) == SIG_ERR)
			err(1, "signal");
		if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1)
			err(1, "sigprocmask");

		/* Change to different user */
		if (uid || gid) {
			if (setresgid(gid, gid, gid) == -1)
				err(1, "setresgid");
			if (setgroups(1, &gid) == -1)
				err(1, "setgroups");
			if (setresuid(uid, uid, uid) == -1)
				err(1, "setresuid");
		}
		execvp(path, argv);

		/* Error */
		err(1, "execvp");
	}

	/* Choose the pid of the systraced process */
	pid = bg ? pid : cpid;

	icpid = intercept_getpid(pid);
	
	/* Set up user related information */
	if (!uid && !gid) {
		uid = getuid();
		gid = getgid();
	}
	intercept_setpid(icpid, uid, gid);
	
	/* Setup done, restore signal handling state */
	if (signal(SIGUSR1, ohandler) == SIG_ERR) {
		kill(pid, SIGKILL);
		err(1, "signal");
	}
	if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1) {
		kill(pid, SIGKILL);
		err(1, "sigprocmask");
	}

	if (bg) {
		if (daemon(1, 1) == -1) {
			kill(pid, SIGKILL);
			err(1, "daemon");
		}
	}

	return (pid);
}

int
intercept_existpids(void)
{
	return (SPLAY_ROOT(&pids) != NULL);
}

void
intercept_freepid(pid_t pidnr)
{
	struct intercept_pid *pid, tmp2;

	tmp2.pid = pidnr;
	pid = SPLAY_FIND(pidtree, &pids, &tmp2);
	if (pid == NULL)
		return;

	intercept.freepid(pid);

	SPLAY_REMOVE(pidtree, &pids, pid);
	if (pid->name)
		free(pid->name);
	if (pid->newname)
		free(pid->newname);
	free(pid);
}

struct intercept_pid *
intercept_findpid(pid_t pid)
{
	struct intercept_pid *tmp, tmp2;

	tmp2.pid = pid;
	tmp = SPLAY_FIND(pidtree, &pids, &tmp2);

	return (tmp);
}

struct intercept_pid *
intercept_getpid(pid_t pid)
{
	struct intercept_pid *tmp, tmp2;

	tmp2.pid = pid;
	tmp = SPLAY_FIND(pidtree, &pids, &tmp2);

	if (tmp)
		return (tmp);

	if ((tmp = malloc(sizeof(struct intercept_pid))) == NULL)
		err(1, "%s: malloc", __func__);

	memset(tmp, 0, sizeof(struct intercept_pid));
	tmp->pid = pid;

	SPLAY_INSERT(pidtree, &pids, tmp);

	return (tmp);
}

int
intercept_open(void)
{
	int fd;

	if ((fd = intercept.open()) == -1)
		return (-1);

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		warn("fcntl(O_NONBLOCK)");

	return (fd);
}

int
intercept_attach(int fd, pid_t pid)
{
	return (intercept.attach(fd, pid));
}

int
intercept_attachpid(int fd, pid_t pid, char *name)
{
	struct intercept_pid *icpid;
	int res;

	res = intercept.attach(fd, pid);
	if (res == -1)
		return (-1);

	icpid = intercept_getpid(pid);

	if ((icpid->newname = strdup(name)) == NULL)
		err(1, "strdup");

	if (intercept.report(fd, pid) == -1)
		return (-1);

	/* Indicates a running attach */
	icpid->execve_code = -1;

	return (0);
}

int
intercept_detach(int fd, pid_t pid)
{
	int res;

	res = intercept.detach(fd, pid);
	if (res != -1)
		intercept_freepid(pid);
	return (res);
}

int
intercept_read(int fd)
{
	struct pollfd pollfd;
	int n;

	pollfd.fd = fd;
	pollfd.events = POLLIN;

	do  {
		n = poll(&pollfd, 1, -1);
		if (n == -1) {
			if (errno != EINTR && errno != EAGAIN)
				return (-1);
		}
	} while (n <= 0);

	if (!(pollfd.revents & (POLLIN|POLLRDNORM)))
		return (-1);

	return (intercept.read(fd));
}

int
intercept_replace_init(struct intercept_replace *repl)
{
	memset(repl, 0, sizeof(struct intercept_replace));

	return (0);
}

int
intercept_replace_add(struct intercept_replace *repl, int off,
    u_char *addr, size_t len, u_int flags)
{
	int ind = repl->num;

	if (ind >= INTERCEPT_MAXSYSCALLARGS)
		return (-1);

	repl->ind[ind] = off;
	repl->address[ind] = addr;
	repl->len[ind] = len;
	repl->flags[ind] = flags;

	repl->num++;

	return (0);
}

int
intercept_replace(int fd, pid_t pid, u_int16_t seqnr,
    struct intercept_replace *repl)
{
	if (repl->num == 0)
		return (0);

	return (intercept.replace(fd, pid, seqnr, repl));
}

char *
intercept_get_string(int fd, pid_t pid, void *addr)
{
	static char name[ARG_MAX];
	int off = 0, done = 0, stride;

	if (addr == NULL)
		return (NULL);

	stride = 32;
	do {
		if (intercept.io(fd, pid, INTERCEPT_READ, (char *)addr + off,
		    &name[off], stride) == -1) {
			/* Did the current system call get interrupted? */
			if (errno == EBUSY)
				return (NULL);
			if (errno != EINVAL || stride == 1) {
				warn("%s: ioctl", __func__);
				return (NULL);
			}

			/* Try smaller stride */
			stride /= 2;
			continue;
		}

		off += stride;
		name[off] = '\0';
		if (strlen(name) < off)
			done = 1;

	} while (!done && off + stride + 1 < sizeof(name));

	if (!done) {
		warnx("%s: string too long", __func__);
		return (NULL);
	}

	return (name);
}

char *
intercept_filename(int fd, pid_t pid, void *addr, int userp, char *before)
{
	return (intercept_filenameat(fd, pid, AT_FDCWD, addr, userp, before));
}

char *
intercept_filenameat(int fd, pid_t pid, int atfd, void *addr, int userp, char *before)
{
	char *name;

	if ((name = intercept_get_string(fd, pid, addr)) == NULL)
		goto abort;

	if (before != NULL)
		strlcpy(before, name, MAXPATHLEN);

	if ((name = normalize_filenameat(fd, pid, atfd, name, userp)) == NULL)
		goto abort;

	return (name);

 abort:
	ic_abort = 1;
	return (NULL);
}

/*
 * Normalizes a pathname so that Systrace policies entries are
 * invariant to symlinks.
 */

char *
normalize_filename(int fd, pid_t pid, char *name, int userp)
{
	return (normalize_filenameat(fd, pid, AT_FDCWD, name, userp));
}

char *
normalize_filenameat(int fd, pid_t pid, int atfd, char *name, int userp)
{
	static char cwd[2*MAXPATHLEN];
	int havecwd = 0;

	/*
	 * The empty filename does not receive normalization.
	 * System calls are supposed to fail on it.
	 */
	if (strcmp(name, "") == 0)
		return (name);

	if (fd != -1 && intercept.setcwd(fd, pid, atfd) == -1) {
		if (errno == EBUSY)
			return (NULL);
	getcwderr:
		if (strcmp(name, "/") == 0)
			return (name);

		err(1, "%s: getcwd", __func__);
	}

	if (userp == ICLINK_NONE) {
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			goto getcwderr;
		havecwd = 1;
	}

	if (havecwd && name[0] != '/') {
		if (strlcat(cwd, "/", sizeof(cwd)) >= sizeof(cwd))
			return (NULL);
		if (strlcat(cwd, name, sizeof(cwd)) >= sizeof(cwd))
			return (NULL);
	} else {
		if (strlcpy(cwd, name, sizeof(cwd)) >= sizeof(cwd))
			return (NULL);
	}

	if (userp != ICLINK_NONE) {
		static char rcwd[2*MAXPATHLEN];
		char *base = basename(cwd);
		int failed = 0;

		/* The dot maybe used by rmdir("/tmp/something/.") */
		if (strcmp(base, ".") == 0)
			goto nolast;

		if (userp == ICLINK_NOLAST) {
			/* Check if the last component has special meaning */
			if (strcmp(base, "..") == 0 || strcmp(base, "/") == 0)
				userp = ICLINK_ALL;
			else
				goto nolast;
		}

		/* If realpath fails then the filename does not exist,
		 * or we are supposed to not resolve the last component */
		if (intercept_realpath(cwd, rcwd) == NULL) {
			char *dir, last_char;
			struct stat st;
			int base_is_dir;

			if (errno != EACCES) {
				failed = 1;
				goto out;
			}

		nolast:
			/* Component of path could not be entered */
			if (strlcpy(rcwd, cwd, sizeof(rcwd)) >= sizeof(rcwd))
				goto error;
			last_char = rcwd[strlen(rcwd)-1];
			if (last_char == '/')
				base_is_dir = 1;
			else
				base_is_dir = 0;
			if ((base = basename(rcwd)) == NULL)
				goto error;
			if ((dir = dirname(rcwd)) == NULL)
				goto error;

			/* So, try again */
			if (intercept_realpath(dir, rcwd) == NULL) {
				failed = 1;
				goto out;
			}
			/* If path is not "/" append a "/" */
			if (strlen(rcwd) > 1 &&
			    strlcat(rcwd, "/", sizeof(rcwd)) >= sizeof(rcwd))
				goto error;
			if (strlcat(rcwd, base, sizeof(rcwd)) >= sizeof(rcwd))
				goto error;
			if (base_is_dir &&
			    strlcat(rcwd, "/", sizeof(rcwd)) >= sizeof(rcwd))
				goto error;
			/* 
			 * At this point, filename has to exist and has to
			 * be a directory.
			 */
			if (userp != ICLINK_NOLAST) {
				if (lstat(rcwd, &st) == -1 ||
				    !S_ISDIR(st.st_mode))
					failed = 1;
			}
		}
	out:
		if (failed)
			snprintf(rcwd, sizeof(rcwd),
			    "/<non-existent filename>: %s", cwd);
		name = rcwd;
	} else {
		simplify_path(cwd);
		name = cwd;
	}


	/* Restore working directory and change root space after realpath */
	if (fd != -1 && intercept.restcwd(fd) == -1)
		err(1, "%s: restcwd", __func__);

	return (name);

 error:
	errx(1, "%s: filename too long", __func__);
	/* NOTREACHED */
}

void
intercept_syscall(int fd, pid_t pid, u_int16_t seqnr, int policynr,
    const char *name, int code, const char *emulation, void *args, int argsize)
{
	short action, flags = 0;
	struct intercept_syscall *sc;
	struct intercept_pid *icpid;
	struct elevate *elevate = NULL;
	int error = 0;

	action = ICPOLICY_PERMIT;
	flags = 0;

	icpid = intercept_getpid(pid);

	/* Special handling for the exec call */
	if (!strcmp(name, "execve")) {
		void *addr;
		char *argname, before[MAXPATHLEN];

		icpid->execve_code = code;
		icpid->policynr = policynr;

		if (icpid->newname)
			free(icpid->newname);

		intercept.getarg(0, args, argsize, &addr);
		argname = intercept_filename(fd, pid, addr, ICLINK_ALL, before);
		if (argname == NULL)
			err(1, "%s:%d: intercept_filename",
			    __func__, __LINE__);

		if (intercept.scriptname(fd, pid, before) != 0)
			err(1, "%s:%d: ioctl", __func__, __LINE__);

		icpid->newname = strdup(argname);
		if (icpid->newname == NULL)
			err(1, "%s:%d: strdup", __func__, __LINE__);

		/* We need to know the result from this system call */
		flags = ICFLAGS_RESULT;
	}

	icpid->elevate = NULL;

	sc = intercept_sccb_find(emulation, name);
	if (sc != NULL) {
		struct intercept_translate *tl;

		ic_abort = 0;
		TAILQ_FOREACH(tl, &sc->tls, next) {
			if (intercept_translate(tl, fd, pid, tl->off,
				args, argsize) == -1)
				break;
		}

		if (!ic_abort) {
			struct intercept_replace repl;

			intercept_replace_init(&repl);

			action = (*sc->cb)(fd, pid, policynr, name, code,
			    emulation, args, argsize, &repl,
			    &sc->tls, sc->cb_arg);

			if (action < ICPOLICY_NEVER) {
				/* if we can not rewrite the arguments,
				 * system call fails.
				 */
				if (intercept_replace(fd, pid, seqnr, &repl) == -1)
					action = ICPOLICY_NEVER;
			}
		} else
			action = ICPOLICY_NEVER;
	} else if (intercept_gencb != NULL)
		action = (*intercept_gencb)(fd, pid, policynr, name, code,
		    emulation, args, argsize, intercept_gencbarg);

	if (action > 0) {
		error = action;
		action = ICPOLICY_NEVER;
	} else {
		icpid = intercept_findpid(pid);
		if (icpid != NULL)
			elevate = icpid->elevate;
		else
			elevate = NULL;
	}

	/* Resume execution of the process */
	intercept.answer(fd, pid, seqnr, action, error, flags, elevate);
}

void
intercept_syscall_result(int fd, pid_t pid, u_int16_t seqnr, int policynr,
    const char *name, int code, const char *emulation, void *args, int argsize,
    int result, void *rval)
{
	struct intercept_pid *icpid;

	if (result > 0)
		goto out;

	icpid = intercept_getpid(pid);
	if (!strcmp("execve", name)) {
		intercept_newimage(fd, pid, policynr,
		    emulation, icpid->newname, icpid);
		/* we might have detached by now */
		if (intercept_findpid(pid) == NULL)
			return;
	}

 out:
	/* Resume execution of the process */
	intercept.answer(fd, pid, seqnr, 0, 0, 0, NULL);
}

void
intercept_newimage(int fd, pid_t pid, int policynr,
    const char *emulation, char *newname, struct intercept_pid *icpid)
{
	if (icpid == NULL)
		icpid = intercept_getpid(pid);

	if (icpid->name)
		free(icpid->name);
	if ((icpid->name = strdup(newname)) == NULL)
		err(1, "%s:%d: strdup", __func__, __LINE__);

	if (icpid->newname != NULL) {
		free(icpid->newname);
		icpid->newname = NULL;
	}

	if (intercept_newimagecb != NULL)
		(*intercept_newimagecb)(fd, pid, policynr, emulation,
		    icpid->name, intercept_newimagecbarg);
}

int
intercept_newpolicy(int fd)
{
	int policynr;

	policynr = intercept.newpolicy(fd);

	return (policynr);
}

int
intercept_assignpolicy(int fd, pid_t pid, int policynr)
{
	return (intercept.assignpolicy(fd, pid, policynr));
}

int
intercept_modifypolicy_nr(int fd, int policynr, int code, short policy)
{
	return (intercept.policy(fd, policynr, code, policy));
}

int
intercept_modifypolicy(int fd, int policynr, const char *emulation,
    const char *name, short policy)
{
	int code;

	code = intercept.getsyscallnumber(emulation, name);
	if (code == -1)
		return (-1);

	return (intercept.policy(fd, policynr, code, policy));
}

void
intercept_child_info(pid_t opid, pid_t npid)
{
	struct intercept_pid *ipid, *inpid, tmp;

	/* A child just died on us */
	if (npid == -1) {
		intercept_freepid(opid);
		return;
	}

	tmp.pid = opid;
	ipid = SPLAY_FIND(pidtree, &pids, &tmp);
	if (ipid == NULL)
		return;

	inpid = intercept_getpid(npid);

	inpid->policynr = ipid->policynr;
	if (ipid->name != NULL) {
		inpid->name = strdup(ipid->name);
		if (inpid->name == NULL)
			err(1, "%s:%d: strdup", __func__, __LINE__);
	}

	/* Process tree */
	inpid->ppid = opid;

	/* Copy some information */
	inpid->uid = ipid->uid;
	inpid->gid = ipid->gid;
	strlcpy(inpid->username, ipid->username, sizeof(inpid->username));
	strlcpy(inpid->home, ipid->home, sizeof(inpid->home));

	/* XXX - keeps track of emulation */
	intercept.clonepid(ipid, inpid);
}

void
intercept_ugid(struct intercept_pid *icpid, uid_t uid, gid_t gid)
{
	/* Update current home dir */
	if (icpid->uid != uid) {
		struct passwd *pw;

		if ((pw = getpwuid(uid)) == NULL) {
			snprintf(icpid->username, sizeof(icpid->username),
			    "uid %d", uid);
			strlcpy(icpid->home, "/", sizeof(icpid->home));
		} else {
			strlcpy(icpid->username, pw->pw_name,
			    sizeof(icpid->username));
			strlcpy(icpid->home, pw->pw_dir, sizeof(icpid->home));
		}
	}

	icpid->uid = uid;
	icpid->gid = gid;
}

/*
 * Returns the number of a system call
 */

int
intercept_getsyscallnumber(const char *emulation, const char *name)
{
	int nr = intercept.getsyscallnumber(emulation, name);

	if (nr >= INTERCEPT_MAXSYSCALLNR)
		err(1, "%s: system call number too high: %d", __func__, nr);

	return (nr);
}

/*
 * Checks if the given emulation has a certain system call.
 * This is a very slow function.
 */

int
intercept_isvalidsystemcall(const char *emulation, const char *name)
{
	int res;

	res = intercept.getsyscallnumber(emulation, name);

	return (res != -1);
}

/*
 * Call back when a user has exhausted the number of allowed policies
 * in the kernel.  The kernel returns the policy number of a policy
 * that has been purged.
 */

void
intercept_policy_free(int policynr)
{
	(*intercept_pfreecb)(policynr, intercept_pfreearg);
}

char *
intercept_realpath(const char *path, char *resolved)
{
	struct stat sb;
	int idx = 0, n, nlnk = 0, serrno = errno;
	const char *q;
	char *p, wbuf[2][MAXPATHLEN];
	size_t len;

	/*
	 * Build real path one by one with paying an attention to .,
	 * .. and symbolic link.
	 */

	/*
	 * `p' is where we'll put a new component with prepending
	 * a delimiter.
	 */
	p = resolved;

	if (*path == 0) {
		*p = 0;
		errno = ENOENT;
		return (NULL);
	}

	/* If relative path, start from current working directory. */
	if (*path != '/') {
		if (getcwd(resolved, MAXPATHLEN) == NULL) {
			p[0] = '.';
			p[1] = 0;
			return (NULL);
		}
		len = strlen(resolved);
		if (len > 1)
			p += len;
	}

loop:
	/* Skip any slash. */
	while (*path == '/')
		path++;

	if (*path == 0) {
		if (p == resolved)
			*p++ = '/';
		*p = 0;
		return (resolved);
	}

	/* Find the end of this component. */
	q = path;
	do
		q++;
	while (*q != '/' && *q != 0);

	/* Test . or .. */
	if (path[0] == '.') {
		if (q - path == 1) {
			path = q;
			goto loop;
		}
		if (path[1] == '.' && q - path == 2) {
			/* Trim the last component. */
			if (p != resolved)
				while (*--p != '/')
					;
			path = q;
			goto loop;
		}
	}

	/* Append this component. */
	if (p - resolved + 1 + q - path + 1 > MAXPATHLEN) {
		errno = ENAMETOOLONG;
		if (p == resolved)
			*p++ = '/';
		*p = 0;
		return (NULL);
	}
	p[0] = '/';
	memcpy(&p[1], path,
	    /* LINTED We know q > path. */
	    q - path);
	p[1 + q - path] = 0;

	/*
	 * If this component is a symlink, toss it and prepend link
	 * target to unresolved path.
	 */
	if (lstat(resolved, &sb) == -1) {
		/* Allow nonexistent component if this is the last one. */
		while (*q == '/')
			q++;

		if (*q == 0  && errno == ENOENT) {
			errno = serrno;
			return (resolved);
		}

		return (NULL);
	}
	if (S_ISLNK(sb.st_mode)) {
		if (nlnk++ >= MAXSYMLINKS) {
			errno = ELOOP;
			return (NULL);
		}
		n = readlink(resolved, wbuf[idx], sizeof(wbuf[0]) - 1);
		if (n < 0)
			return (NULL);
		if (n == 0) {
			errno = ENOENT;
			return (NULL);
		}

		/* Append unresolved path to link target and switch to it. */
		if (n + (len = strlen(q)) + 1 > sizeof(wbuf[0])) {
			errno = ENAMETOOLONG;
			return (NULL);
		}
		memcpy(&wbuf[idx][n], q, len + 1);
		path = wbuf[idx];
		idx ^= 1;

		/* If absolute symlink, start from root. */
		if (*path == '/')
			p = resolved;
		goto loop;
	}
	if (*q == '/' && !S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (NULL);
	}

	/* Advance both resolved and unresolved path. */
	p += 1 + q - path;
	path = q;
	goto loop;
}
