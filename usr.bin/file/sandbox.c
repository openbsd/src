/* $OpenBSD: sandbox.c,v 1.4 2015/04/30 14:30:53 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>

#include <dev/systrace.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include "file.h"
#include "magic.h"
#include "xmalloc.h"

static const struct
{
	int syscallnum;
	int action;
} allowed_syscalls[] = {
	{ SYS_open, SYSTR_POLICY_NEVER }, /* for strerror */

	{ SYS_close, SYSTR_POLICY_PERMIT },
	{ SYS_exit, SYSTR_POLICY_PERMIT },
	{ SYS_getdtablecount, SYSTR_POLICY_PERMIT },
	{ SYS_getentropy, SYSTR_POLICY_PERMIT },
	{ SYS_getpid, SYSTR_POLICY_PERMIT },
	{ SYS_getrlimit, SYSTR_POLICY_PERMIT },
	{ SYS_issetugid, SYSTR_POLICY_PERMIT },
	{ SYS_madvise, SYSTR_POLICY_PERMIT },
	{ SYS_mmap, SYSTR_POLICY_PERMIT },
	{ SYS_mprotect, SYSTR_POLICY_PERMIT },
	{ SYS_mquery, SYSTR_POLICY_PERMIT },
	{ SYS_munmap, SYSTR_POLICY_PERMIT },
	{ SYS_read, SYSTR_POLICY_PERMIT },
	{ SYS_recvmsg, SYSTR_POLICY_PERMIT },
	{ SYS_sendmsg, SYSTR_POLICY_PERMIT },
	{ SYS_sigprocmask, SYSTR_POLICY_PERMIT },
	{ SYS_write, SYSTR_POLICY_PERMIT },

	{ -1, -1 }
};

static int
sandbox_find(int syscallnum)
{
	int	i;

	for (i = 0; allowed_syscalls[i].syscallnum != -1; i++) {
		if (allowed_syscalls[i].syscallnum == syscallnum)
			return (allowed_syscalls[i].action);
	}
	return (SYSTR_POLICY_KILL);
}

static int
sandbox_child(const char *user)
{
	struct passwd	*pw;

	/*
	 * If we don't set stream buffering explicitly, stdio calls isatty()
	 * which means ioctl() - too nasty to let through the systrace policy.
	 */
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if (geteuid() == 0) {
		pw = getpwnam(user);
		if (pw == NULL)
			errx(1, "unknown user %s", user);
		if (setgroups(1, &pw->pw_gid) != 0)
			err(1, "setgroups");
		if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) != 0)
			err(1, "setresgid");
		if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) != 0)
			err(1, "setresuid");
	}

	if (kill(getpid(), SIGSTOP) != 0)
		err(1, "kill(SIGSTOP)");
	return (0);
}

int
sandbox_fork(const char *user)
{
	pid_t			 pid;
	int			 status, devfd, fd, i;
	struct systrace_policy	 policy;

	switch (pid = fork()) {
	case -1:
		err(1, "fork");
	case 0:
		return (sandbox_child(user));
	}

	/*
	 * Wait for the child to stop itself with SIGSTOP before assigning the
	 * policy, before that it might still be calling syscalls the policy
	 * would block.
	 */
	do {
		pid = waitpid(pid, &status, WUNTRACED);
	} while (pid == -1 && errno == EINTR);
	if (!WIFSTOPPED(status))
		errx(1, "child not stopped");

	devfd = open("/dev/systrace", O_RDONLY);
	if (devfd == -1)
		err(1, "open(\"/dev/systrace\")");
	if (ioctl(devfd, STRIOCCLONE, &fd) == -1)
		err(1, "ioctl(STRIOCCLONE)");
	close(devfd);

	if (ioctl(fd, STRIOCATTACH, &pid) == -1)
		err(1, "ioctl(STRIOCATTACH)");

	memset(&policy, 0, sizeof policy);
	policy.strp_op = SYSTR_POLICY_NEW;
	policy.strp_maxents = SYS_MAXSYSCALL;
	if (ioctl(fd, STRIOCPOLICY, &policy) == -1)
		err(1, "ioctl(STRIOCPOLICY/NEW)");
	policy.strp_op = SYSTR_POLICY_ASSIGN;
	policy.strp_pid = pid;
	if (ioctl(fd, STRIOCPOLICY, &policy) == -1)
		err(1, "ioctl(STRIOCPOLICY/ASSIGN)");

	for (i = 0; i < SYS_MAXSYSCALL; i++) {
		policy.strp_op = SYSTR_POLICY_MODIFY;
		policy.strp_code = i;
		policy.strp_policy = sandbox_find(i);
		if (ioctl(fd, STRIOCPOLICY, &policy) == -1)
			err(1, "ioctl(STRIOCPOLICY/MODIFY)");
	}

	if (kill(pid, SIGCONT) != 0)
		err(1, "kill(SIGCONT)");
	return (pid);
}
