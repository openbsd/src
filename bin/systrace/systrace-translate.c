/*	$OpenBSD: systrace-translate.c,v 1.22 2007/06/06 15:14:49 henning Exp $	*/
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
#include <sys/mman.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <pwd.h>
#include <err.h>

#include "../../sys/compat/linux/linux_types.h"
#include "../../sys/compat/linux/linux_fcntl.h"

#include "intercept.h"
#include "systrace.h"

#define FL(w,c)	do { \
	if (flags & (w) && p < str + sizeof str - 1) \
		*p++ = (c); \
} while (0)

static int print_oflags(char *, size_t, struct intercept_translate *);
static int linux_print_oflags(char *, size_t, struct intercept_translate *);
static int print_modeflags(char *, size_t, struct intercept_translate *);
static int print_number(char *, size_t, struct intercept_translate *);
static int print_uname(char *, size_t, struct intercept_translate *);
static int print_pidname(char *, size_t, struct intercept_translate *);
static int print_signame(char *, size_t, struct intercept_translate *);
static int print_fcntlcmd(char *, size_t, struct intercept_translate *);
static int print_memprot(char *, size_t, struct intercept_translate *);
static int print_fileflags(char *, size_t, struct intercept_translate *);
static int get_argv(struct intercept_translate *, int, pid_t, void *);
static int print_argv(char *, size_t, struct intercept_translate *);

static int
print_oflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char str[32], *p;
	int flags = (intptr_t)tl->trans_addr;
	int isread = 0;

	p = str;
	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		strlcpy(p, "ro", str + sizeof str - p);
		isread = 1;
		break;
	case O_WRONLY:
		strlcpy(p, "wo", str + sizeof str - p);
		break;
	case O_RDWR:
		strlcpy(p, "rw", str + sizeof str - p);
		break;
	default:
		strlcpy(p, "--", str + sizeof str - p);
		break;
	}

	/* XXX - Open handling of alias */
	if (isread)
		systrace_switch_alias("native", "open", "native", "fsread");
	else
		systrace_switch_alias("native", "open", "native", "fswrite");

	p += 2;

	FL(O_NONBLOCK, 'n');
	FL(O_APPEND, 'a');
	FL(O_CREAT, 'c');
	FL(O_TRUNC, 't');

	*p = '\0';

	strlcpy(buf, str, buflen);

	return (0);
}

static int
linux_print_oflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char str[32], *p;
	int flags = (intptr_t)tl->trans_addr;
	int isread = 0;

	p = str;
	switch (flags & LINUX_O_ACCMODE) {
	case LINUX_O_RDONLY:
		strlcpy(p, "ro", str + sizeof str - p);
		isread = 1;
		break;
	case LINUX_O_WRONLY:
		strlcpy(p, "wo", str + sizeof str - p);
		break;
	case LINUX_O_RDWR:
		strlcpy(p, "rw", str + sizeof str - p);
		break;
	default:
		strlcpy(p, "--", str + sizeof str - p);
		break;
	}

	/* XXX - Open handling of alias */
	if (isread)
		systrace_switch_alias("linux", "open", "linux", "fsread");
	else
		systrace_switch_alias("linux", "open", "linux", "fswrite");

	p += 2;

	FL(LINUX_O_APPEND, 'a');
	FL(LINUX_O_CREAT, 'c');
	FL(LINUX_O_TRUNC, 't');

	*p = '\0';

	strlcpy(buf, str, buflen);

	return (0);
}

static int
print_modeflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int mode = (intptr_t)tl->trans_addr;

	mode &= 00007777;
	snprintf(buf, buflen, "%o", mode);

	return (0);
}

static int
print_number(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int number = (intptr_t)tl->trans_addr;

	snprintf(buf, buflen, "%d", number);

	return (0);
}

static int
print_sockdom(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int domain = (intptr_t)tl->trans_addr;
	char *what = NULL;

	switch (domain) {
	case AF_UNIX:
		what = "AF_UNIX";
		break;
	case AF_INET:
		what = "AF_INET";
		break;
	case AF_INET6:
		what = "AF_INET6";
		break;
	case AF_IMPLINK:
		what = "AF_IMPLINK";
		break;
	default:
		snprintf(buf, buflen, "AF_UNKNOWN(%d)", domain);
		break;
	}

	if (what != NULL)
		strlcpy(buf, what, buflen);

	return (0);
}

static int
print_socktype(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int type = (intptr_t)tl->trans_addr;
	char *what = NULL;

	switch (type) {
	case SOCK_STREAM:
		what = "SOCK_STREAM";
		break;
	case SOCK_DGRAM:
		what = "SOCK_DGRAM";
		break;
	case SOCK_RAW:
		what = "SOCK_RAW";
		break;
	case SOCK_SEQPACKET:
		what = "SOCK_SEQPACKET";
		break;
	case SOCK_RDM:
		what = "SOCK_RDM";
		break;
	default:
		snprintf(buf, buflen, "SOCK_UNKNOWN(%d)", type);
		break;
	}

	if (what != NULL)
		strlcpy(buf, what, buflen);

	return (0);
}

static int
print_uname(char *buf, size_t buflen, struct intercept_translate *tl)
{
	struct passwd *pw;
	uid_t uid = (intptr_t)tl->trans_addr;

	pw = getpwuid(uid);
	strlcpy(buf, pw != NULL ? pw->pw_name : "<unknown>", buflen);

	return (0);
}

static int
print_pidname(char *buf, size_t buflen, struct intercept_translate *tl)
{
	struct intercept_pid *icpid;
	pid_t pid = (intptr_t)tl->trans_addr;

	if (pid > 0) {
		icpid = intercept_findpid(pid);
		strlcpy(buf, icpid != NULL ? icpid->name : "<unknown>", buflen);
	} else if (pid == 0) {
		strlcpy(buf, "<own process group>", buflen);
	} else if (pid == -1) {
		strlcpy(buf, "<every process: -1>", buflen);
	} else {
		/* pid is negative but not -1 - trying to signal pgroup */
		pid = -pid;
		icpid = intercept_findpid(pid);
		strlcpy(buf, "pg:", buflen);
		strlcat(buf, icpid != NULL ? icpid->name : "unknown", buflen);
	}

	return (0);
}

static int
print_signame(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int sig = (intptr_t)tl->trans_addr;
	char *name;

	switch (sig) {
	case SIGHUP: 
		name = "SIGHUP"; 
		break;
	case SIGINT: 
		name = "SIGINT"; 
		break;
	case SIGQUIT: 
		name = "SIGQUIT"; 
		break;
	case SIGILL: 
		name = "SIGILL"; 
		break;
	case SIGABRT: 
		name = "SIGABRT"; 
		break;
	case SIGFPE: 
		name = "SIGFPE"; 
		break;
	case SIGKILL: 
		name = "SIGKILL"; 
		break;
	case SIGBUS: 
		name = "SIGBUS"; 
		break;
	case SIGSEGV: 
		name = "SIGSEGV"; 
		break;
	case SIGSYS: 
		name = "SIGSYS"; 
		break;
	case SIGPIPE: 
		name = "SIGPIPE"; 
		break;
	case SIGALRM: 
		name = "SIGALRM"; 
		break;
	case SIGTERM: 
		name = "SIGTERM"; 
		break;
	case SIGURG: 
		name = "SIGURG"; 
		break;
	case SIGSTOP: 
		name = "SIGSTOP"; 
		break;
	case SIGTSTP: 
		name = "SIGTSTP"; 
		break;
	case SIGCONT: 
		name = "SIGCONT"; 
		break;
	case SIGCHLD: 
		name = "SIGCHLD"; 
		break;
	case SIGTTIN: 
		name = "SIGTTIN"; 
		break;
	case SIGTTOU: 
		name = "SIGTTOU"; 
		break;
	case SIGIO: 
		name = "SIGIO"; 
		break;
	case SIGPROF: 
		name = "SIGPROF"; 
		break;
	case SIGWINCH: 
		name = "SIGWINCH"; 
		break;
#ifndef __linux__
	case SIGINFO: 
		name = "SIGINFO"; 
		break;
#endif /* !__linux__ */
	case SIGUSR1: 
		name = "SIGUSR1"; 
		break;
	case SIGUSR2: 
		name = "SIGUSR2"; 
		break;
	default:
		snprintf(buf, buflen, "<unknown>: %d", sig);
		return (0);
	}

	strlcpy(buf, name, buflen);
	return (0);
}

static int
print_fcntlcmd(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int cmd = (intptr_t)tl->trans_addr;
	char *name;

	switch (cmd) {
	case F_DUPFD:
		name = "F_DUPFD";
		break;
	case F_GETFD:
		name = "F_GETFD";
		break;
	case F_SETFD:
		name = "F_SETFD";
		break;
	case F_GETFL:
		name = "F_GETFL";
		break;
	case F_SETFL:
		name = "F_SETFL";
		break;
	case F_GETOWN:
		name = "F_GETOWN";
		break;
	case F_SETOWN:
		name = "F_SETOWN";
		break;
	case F_GETLK:
		name = "F_GETLK";
		break;
	case F_SETLK:
		name = "F_SETLK";
		break;
	case F_SETLKW:
		name = "F_SETLKW";
		break;
	default:
		snprintf(buf, buflen, "<unknown>: %d", cmd);
		return (0);
	}

	snprintf(buf, buflen, "%s", name);
	return (0);
}

struct linux_i386_mmap_arg_struct {
	unsigned long addr;
	unsigned long len;
	unsigned long prot;
	unsigned long flags;
	unsigned long fd;
	unsigned long offset;
};

static int
get_linux_memprot(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	struct linux_i386_mmap_arg_struct arg;
	size_t len = sizeof(arg);
	extern struct intercept_system intercept;

	if (intercept.io(fd, pid, INTERCEPT_READ, addr,
	    (void *)&arg, len) == -1)
		return (-1);

	trans->trans_addr = (void *)arg.prot;

	return (0);
}

static int
print_memprot(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int prot = (intptr_t)tl->trans_addr;
	char lbuf[64];

	if (prot == PROT_NONE) {
		strlcpy(buf, "PROT_NONE", buflen);
		return (0);
	} else
		*buf = '\0';

	while (prot) {
		if (*buf)
			strlcat(buf, "|", buflen);

		if (prot & PROT_READ) {
			strlcat(buf, "PROT_READ", buflen);
			prot &= ~PROT_READ;
			continue;
		}

		if (prot & PROT_WRITE) {
			strlcat(buf, "PROT_WRITE", buflen);
			prot &= ~PROT_WRITE;
			continue;
		}

		if (prot & PROT_EXEC) {
			strlcat(buf, "PROT_EXEC", buflen);
			prot &= ~PROT_EXEC;
			continue;
		}

		if (prot) {
			snprintf(lbuf, sizeof(lbuf), "<unknown:0x%x>", prot);
			strlcat(buf, lbuf, buflen);
			prot = 0;
			continue;
		}
	}

	return (0);
}

static int
print_fileflags(char *buf, size_t buflen, struct intercept_translate *tl)
{
	unsigned int flags = (intptr_t)tl->trans_addr;
	char lbuf[64];

	*buf = '\0';

	while (flags) {
		if (*buf)
			strlcat(buf, "|", buflen);

		if (flags & UF_NODUMP) {
			strlcat(buf, "UF_NODUMP", buflen);
			flags &= ~UF_NODUMP;
			continue;
		}

		if (flags & UF_IMMUTABLE) {
			strlcat(buf, "UF_IMMUTABLE", buflen);
			flags &= ~UF_IMMUTABLE;
			continue;
		}

		if (flags & UF_APPEND) {
			strlcat(buf, "UF_APPEND", buflen);
			flags &= ~UF_APPEND;
			continue;
		}

		if (flags & UF_OPAQUE) {
			strlcat(buf, "UF_OPAQUE", buflen);
			flags &= ~UF_OPAQUE;
			continue;
		}

		if (flags & SF_ARCHIVED) {
			strlcat(buf, "SF_ARCHIVED", buflen);
			flags &= ~SF_ARCHIVED;
			continue;
		}

		if (flags & SF_IMMUTABLE) {
			strlcat(buf, "SF_IMMUTABLE", buflen);
			flags &= ~SF_IMMUTABLE;
			continue;
		}

		if (flags & SF_APPEND) {
			strlcat(buf, "SF_APPEND", buflen);
			flags &= ~SF_APPEND;
			continue;
		}

		if (flags) {
			snprintf(lbuf, sizeof(lbuf), "<unknown:0x%x>", flags);
			strlcat(buf, lbuf, buflen);
			flags = 0;
			continue;
		}
	}

	return (0);
}

static int
get_argv(struct intercept_translate *trans, int fd, pid_t pid, void *addr)
{
	char *arg;
	char buf[_POSIX2_LINE_MAX], *p;
	int i, off = 0;
	size_t len;
	extern struct intercept_system intercept;

	i = 0;
	buf[0] = '\0';
	while (1) {
		if (intercept.io(fd, pid, INTERCEPT_READ, (char *)addr + off,
			(void *)&arg, sizeof(char *)) == -1) {
			warn("%s: ioctl", __func__);
			return (-1);
		}
		if (arg == NULL)
			break;

		p = intercept_get_string(fd, pid, arg);
		if (p == NULL)
			return (-1);

		if (i > 0)
			strlcat(buf, " ", sizeof(buf));
		strlcat(buf, p, sizeof(buf));

		off += sizeof(char *);
		i++;
	}
	
	len = strlen(buf) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	/* XXX - No argument replacement */
	trans->trans_size = 0;
	memcpy(trans->trans_data, buf, len);

	return (0);
}

static int
print_argv(char *buf, size_t buflen, struct intercept_translate *tl)
{
	strlcpy(buf, (char *)tl->trans_data, buflen);

	return (0);
}

struct intercept_translate ic_trargv = {
	"argv",
	get_argv, print_argv,
};

struct intercept_translate ic_oflags = {
	"oflags",
	NULL, print_oflags,
};

struct intercept_translate ic_linux_oflags = {
	"oflags",
	NULL, linux_print_oflags,
};

struct intercept_translate ic_modeflags = {
	"mode",
	NULL, print_modeflags,
};

struct intercept_translate ic_uidt = {
	"uid",
	NULL, print_number,
};

struct intercept_translate ic_uname = {
	"uname",
	NULL, print_uname,
};

struct intercept_translate ic_gidt = {
	"gid",
	NULL, print_number,
};

struct intercept_translate ic_fdt = {
	"fd",
	NULL, print_number,
};

struct intercept_translate ic_sockdom = {
	"sockdom",
	NULL, print_sockdom,
};

struct intercept_translate ic_socktype = {
	"socktype",
	NULL, print_socktype,
};

struct intercept_translate ic_pidname = {
	"pidname",
	NULL, print_pidname,
};

struct intercept_translate ic_signame = {
	"signame",
	NULL, print_signame,
};

struct intercept_translate ic_fcntlcmd = {
	"cmd",
	NULL, print_fcntlcmd,
};

struct intercept_translate ic_memprot = {
	"prot",
	NULL, print_memprot,
};

struct intercept_translate ic_linux_memprot = {
	"prot",
	get_linux_memprot, print_memprot,
};

struct intercept_translate ic_fileflags = {
	"flags",
	NULL, print_fileflags,
};
