/*	$OpenBSD: kdump.c,v 1.99 2015/04/17 06:14:36 guenther Exp $	*/

/*-
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>	/* MAXCOMLEN nitems */
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/ptrace.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/siginfo.h>
#include <sys/vmmeter.h>
#include <sys/tty.h>
#include <sys/wait.h>
#define _KERNEL
#include <errno.h>
#undef _KERNEL
#include <ddb/db_var.h>
#include <machine/cpu.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "ktrace.h"
#include "kdump.h"
#include "kdump_subr.h"
#include "extern.h"

int timestamp, decimal, iohex, fancy = 1, maxdata = INT_MAX;
int needtid, tail;
char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;
pid_t pid_opt = -1;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

#include <sys/syscall.h>

#include <compat/linux/linux_syscall.h>

#define KTRACE
#define PTRACE
#define NFSCLIENT
#define NFSSERVER
#define SYSVSEM
#define SYSVMSG
#define SYSVSHM
#define LFS
#include <kern/syscalls.c>

#include <compat/linux/linux_syscalls.c>
#undef KTRACE
#undef PTRACE
#undef NFSCLIENT
#undef NFSSERVER
#undef SYSVSEM
#undef SYSVMSG
#undef SYSVSHM
#undef LFS

struct emulation {
	char *name;		/* Emulation name */
	char **sysnames;	/* Array of system call names */
	int  nsysnames;		/* Number of */
};

static struct emulation emulations[] = {
	{ "native",	syscallnames,		SYS_MAXSYSCALL },
	{ "linux",	linux_syscallnames,	LINUX_SYS_MAXSYSCALL },
	{ NULL,		NULL,			0 }
};

static struct emulation *current;
static struct emulation *def_emul;

struct pid_emul {
	struct emulation *e;	
	pid_t p;
};

static struct pid_emul *pe_table;
static size_t pe_size;


static char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_ATTACH",	"PT_DETACH",	"PT_IO",
	"PT_SET_EVENT_MASK", "PT_GET_EVENT_MASK", "PT_GET_PROCESS_STATE",
	"PT_GET_THREAD_FIRST", "PT_GET_THREAD_NEXT",
};

static void mappidtoemul(pid_t, struct emulation *);
static struct emulation * findemul(pid_t);
static int fread_tail(void *, size_t, size_t);
static void dumpheader(struct ktr_header *);
static void ktrcsw(struct ktr_csw *);
static void ktremul(char *, size_t);
static void ktrgenio(struct ktr_genio *, size_t);
static void ktrnamei(const char *, size_t);
static void ktrpsig(struct ktr_psig *);
static void ktrsyscall(struct ktr_syscall *, size_t);
static const char *kresolvsysctl(int, const int *);
static void ktrsysret(struct ktr_sysret *);
static void ktruser(struct ktr_user *, size_t);
static void setemul(const char *);
static void usage(void);
static void ioctldecode(int);
static void ptracedecode(int);
static void atfd(int);
static void polltimeout(int);
static void wait4pid(int);
static void signame(int);
static void semctlname(int);
static void shmctlname(int);
static void semgetname(int);
static void flagsandmodename(int);
static void clockname(int);
static void sockoptlevelname(int);
static void ktraceopname(int);

int
main(int argc, char *argv[])
{
	int ch, silent;
	size_t ktrlen, size;
	int trpoints = ALL_POINTS;
	void *m;

	def_emul = current = &emulations[0];	/* native */

	while ((ch = getopt(argc, argv, "e:f:dHlm:nRp:Tt:xX")) != -1)
		switch (ch) {
		case 'e':
			setemul(optarg);
			def_emul = current;
			break;
		case 'f':
			tracefile = optarg;
			break;
		case 'd':
			decimal = 1;
			break;
		case 'H':
			needtid = 1;
			break;
		case 'l':
			tail = 1;
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'n':
			fancy = 0;
			break;
		case 'p':
			pid_opt = atoi(optarg);
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		case 'x':
			iohex = 1;
			break;
		case 'X':
			iohex = 2;
			break;
		default:
			usage();
		}
	if (argc > optind)
		usage();

	m = malloc(size = 1025);
	if (m == NULL)
		err(1, NULL);
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	if (fread_tail(&ktr_header, sizeof(struct ktr_header), 1) == 0 ||
	    ktr_header.ktr_type != htobe32(KTR_START))
		errx(1, "%s: not a dump", tracefile);
	while (fread_tail(&ktr_header, sizeof(struct ktr_header), 1)) {
		silent = 0;
		if (pe_size == 0)
			mappidtoemul(ktr_header.ktr_pid, current);
		if (pid_opt != -1 && pid_opt != ktr_header.ktr_pid)
			silent = 1;
		if (silent == 0 && trpoints & (1<<ktr_header.ktr_type))
			dumpheader(&ktr_header);
		ktrlen = ktr_header.ktr_len;
		if (ktrlen > size) {
			void *newm;

			if (ktrlen == SIZE_MAX)
				errx(1, "data too long");
			newm = realloc(m, ktrlen+1);
			if (newm == NULL)
				err(1, "realloc");
			m = newm;
			size = ktrlen;
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if (silent)
			continue;
		if ((trpoints & (1<<ktr_header.ktr_type)) == 0)
			continue;
		current = findemul(ktr_header.ktr_pid);
		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall((struct ktr_syscall *)m, ktrlen);
			break;
		case KTR_SYSRET:
			ktrsysret((struct ktr_sysret *)m);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio((struct ktr_genio *)m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig((struct ktr_psig *)m);
			break;
		case KTR_CSW:
			ktrcsw((struct ktr_csw *)m);
			break;
		case KTR_EMUL:
			ktremul(m, ktrlen);
			mappidtoemul(ktr_header.ktr_pid, current);
			break;
		case KTR_STRUCT:
			ktrstruct(m, ktrlen);
			break;
		case KTR_USER:
			ktruser(m, ktrlen);
			break;
		}
		if (tail)
			(void)fflush(stdout);
	}
	exit(0);
}

static void
mappidtoemul(pid_t pid, struct emulation *emul)
{
	size_t i;
	struct pid_emul *tmp;

	for (i = 0; i < pe_size; i++) {
		if (pe_table[i].p == pid) {
			pe_table[i].e = emul;
			return;
		}
	}
	tmp = reallocarray(pe_table, pe_size + 1, sizeof(*pe_table));
	if (tmp == NULL)
		err(1, NULL);
	pe_table = tmp;
	pe_table[pe_size].p = pid;
	pe_table[pe_size].e = emul;
	pe_size++;
}

static struct emulation*
findemul(pid_t pid)
{
	size_t i;

	for (i = 0; i < pe_size; i++)
		if (pe_table[i].p == pid)
			return pe_table[i].e;
	return def_emul;
}

static int
fread_tail(void *buf, size_t size, size_t num)
{
	int i;

	while ((i = fread(buf, size, num, stdin)) == 0 && tail) {
		(void)sleep(1);
		clearerr(stdin);
	}
	return (i);
}

static void
dumpheader(struct ktr_header *kth)
{
	static struct timespec prevtime;
	char unknown[64], *type;
	struct timespec temp;

	switch (kth->ktr_type) {
	case KTR_SYSCALL:
		type = "CALL";
		break;
	case KTR_SYSRET:
		type = "RET ";
		break;
	case KTR_NAMEI:
		type = "NAMI";
		break;
	case KTR_GENIO:
		type = "GIO ";
		break;
	case KTR_PSIG:
		type = "PSIG";
		break;
	case KTR_CSW:
		type = "CSW";
		break;
	case KTR_EMUL:
		type = "EMUL";
		break;
	case KTR_STRUCT:
		type = "STRU";
		break;
	case KTR_USER:
		type = "USER";
		break;
	default:
		(void)snprintf(unknown, sizeof unknown, "UNKNOWN(%d)",
		    kth->ktr_type);
		type = unknown;
	}

	(void)printf("%6ld", (long)kth->ktr_pid);
	if (needtid)
		(void)printf("/%-7ld", (long)kth->ktr_tid);
	(void)printf(" %-8.*s ", MAXCOMLEN, kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			timespecsub(&kth->ktr_time, &prevtime, &temp);
			prevtime = kth->ktr_time;
		} else
			temp = kth->ktr_time;
		printf("%lld.%06ld ", (long long)temp.tv_sec,
		    temp.tv_nsec / 1000);
	}
	(void)printf("%s  ", type);
}

/*
 * Base Formatters
 */

/* some syscalls have padding that shouldn't be shown */
static int
pad(long arg)
{
	/* nothing printed */
	return (1);
}

/* a formatter that just saves the argument for the next formatter */
int arg1;
static int
pass_two(long arg)
{
	arg1 = (int)arg;

	/* nothing printed */
	return (1);
}

static int
pdeclong(long arg)
{
	(void)printf("%ld", arg);
	return (0);
}

static int
pdeculong(long arg)
{
	(void)printf("%lu", arg);
	return (0);
}

static int
phexlong(long arg)
{
	(void)printf("%#lx", arg);
	return (0);
}

static int
pnonfancy(long arg)
{
	if (decimal)
		(void)printf("%ld", arg);
	else
		(void)printf("%#lx", arg);
	return (0);
}

static void
pdecint(int arg)
{
	(void)printf("%d", arg);
}

static void
pdecuint(int arg)
{
	(void)printf("%u", arg);
}

static void
phexint(int arg)
{
	(void)printf("%#x", arg);
}

static void
poctint(int arg)
{
	(void)printf("%#o", arg);
}


#ifdef __LP64__

/* on LP64, long long arguments are the same as long arguments */
#define Phexlonglong	Phexlong
#define phexll		NULL		/* not actually used on LP64 */

#else /* __LP64__ */

/* on ILP32, long long arguments are passed as two 32bit args */
#define Phexlonglong	PASS_LONGLONG, Phexll

static int
phexll(long arg2)
{
	long long val;

#if _BYTE_ORDER == _LITTLE_ENDIAN
	val = ((long long)arg2 << 32) | ((long long)arg1 & 0xffffffff);
#else
	val = ((long long)arg1 << 32) | ((long long)arg2 & 0xffffffff);
#endif

	if (fancy || !decimal)
		(void)printf("%#llx", val);
	else
		(void)printf("%lld", val);
	return (0);
}

#endif /* __LP64__ */

static int (*long_formatters[])(long) = {
	NULL,
	pdeclong,
	pdeculong,
	phexlong,
	pass_two,
	pass_two,
	phexll,
	pad,
	pnonfancy,
};

static void (*formatters[])(int) = {
	NULL,
	pdecint,
	phexint,
	poctint,
	pdecuint,
	ioctldecode,
	ptracedecode,
	atfd,
	polltimeout,
	wait4pid,
	signame,
	semctlname,
	shmctlname,
	semgetname,
	flagsandmodename,
	clockname,
	sockoptlevelname,
	ktraceopname,
	fcntlcmdname,
	modename,
	flagsname,
	openflagsname,
	atflagsname,
	accessmodename,
	mmapprotname,
	mmapflagsname,
	wait4optname,
	sendrecvflagsname,
	mountflagsname,
	rebootoptname,
	flockname,
	sockoptname,
	sockipprotoname,
	socktypename,
	sockflagsname,
	sockfamilyname,
	mlockallname,
	shmatname,
	whencename,
	pathconfname,
	rlimitname,
	shutdownhowname,
	prioname,
	madvisebehavname,
	msyncflagsname,
	clocktypename,
	rusagewho,
	sigactionflagname,
	sigprocmaskhowname,
	minheritname,
	quotactlname,
	sigill_name,
	sigtrap_name,
	sigemt_name,
	sigfpe_name,
	sigbus_name,
	sigsegv_name,
	sigchld_name,
	ktracefacname,
	itimername,
	sigset,
	uidname,
	gidname,
};

enum {
	/* the end of the (known) arguments is recognized by the zero fill */
	end_of_args	=  0,

	/* negative are the negative of the index into long_formatters[] */
	Pdeclong	= -1,
	Pdeculong	= -2,
	Phexlong	= -3,
	PASS_TWO	= -4,

/* the remaining long formatters still get called when non-fancy (-n option) */
#define FMT_IS_NONFANCY(x)	((x) <= PASS_LONGLONG)
	PASS_LONGLONG	= -5,
	Phexll		= -6,
	PAD		= -7,
	Pnonfancy	= -8,

	/* positive values are the index into formatters[] */
	Pdecint		= 1,
	Phexint,
	Poctint,
	Pdecuint,
	Ioctldecode,
	Ptracedecode,
	Atfd,
	Polltimeout,
	Wait4pid,
	Signame,
	Semctlname,
	Shmctlname,
	Semgetname,
	Flagsandmodename,
	Clockname,
	Sockoptlevelname,
	Ktraceopname,
	Fcntlcmdname,
	Modename,
	Flagsname,
	Openflagsname,
	Atflagsname,
	Accessmodename,
	Mmapprotname,
	Mmapflagsname,
	Wait4optname,
	Sendrecvflagsname,
	Mountflagsname,
	Rebootoptname,
	Flockname,
	Sockoptname,
	Sockipprotoname,
	Socktypename,
	Sockflagsname,
	Sockfamilyname,
	Mlockallname,
	Shmatname,
	Whencename,
	Pathconfname,
	Rlimitname,
	Shutdownhowname,
	Prioname,
	Madvisebehavname,
	Msyncflagsname,
	Clocktypename,
	Rusagewho,
	Sigactionflagname,
	Sigprocmaskhowname,
	Minheritname,
	Quotactlname,
	Sigill_name,
	Sigtrap_name,
	Sigemt_name,
	Sigfpe_name,
	Sigbus_name,
	Sigsegv_name,
	Sigchld_name,
	Ktracefacname,
	Itimername,
	Sigset,
	Uidname,
	Gidname,
};

#define Pptr		Phexlong
#define	Psize		Pdeculong	/* size_t for small buffers */
#define	Pbigsize	Phexlong	/* size_t for I/O buffers */
#define Pcount		Pdecint		/* int for a count of something */
#define Pfd		Pdecint
#define Ppath		Phexlong
#define Pdev_t		Pdecint
#define Ppid_t		Pdecint
#define Ppgid		Pdecint		/* pid or negative pgid */
#define Poff_t		Phexlonglong
#define Pmsqid		Pdecint
#define Pshmid		Pdecint
#define Psemid		Pdecint
#define Pkey_t		Pdecint
#define Pucount		Pdecuint
#define Chflagsname	Phexlong	/* to be added */
#define Sockprotoname	Phexlong	/* to be added */
#define Swapctlname	Phexlong	/* to be added */
#define Msgflgname	Phexlong	/* to be added */


typedef signed char formatter;
static const formatter scargs[][8] = {
    [SYS_exit]		= { Pdecint },
    [SYS_read]		= { Pfd, Pptr, Pbigsize },
    [SYS_write]		= { Pfd, Pptr, Pbigsize },
    [SYS_open]		= { Ppath, PASS_TWO, Flagsandmodename },
    [SYS_close]		= { Pfd },
    [SYS_getentropy]	= { Pptr, Psize },
    [SYS___tfork]	= { Pptr, Psize },
    [SYS_link]		= { Ppath, Ppath },
    [SYS_unlink]	= { Ppath },
    [SYS_wait4]		= { Wait4pid, Pptr, Wait4optname },
    [SYS_chdir]		= { Ppath },
    [SYS_fchdir]	= { Pfd },
    [SYS_mknod]		= { Ppath, Modename, Pdev_t },
    [SYS_chmod]		= { Ppath, Modename },
    [SYS_chown]		= { Ppath, Uidname, Gidname },
    [SYS_break]		= { Pptr },
    [SYS_getrusage]	= { Rusagewho, Pptr },
    [SYS_mount]		= { Pptr, Ppath, Mountflagsname, Pptr },
    [SYS_unmount]	= { Ppath, Mountflagsname },
    [SYS_setuid]	= { Uidname },
    [SYS_ptrace]	= { Ptracedecode, Ppid_t, Pptr, Pdecint },
    [SYS_recvmsg]	= { Pfd, Pptr, Sendrecvflagsname },
    [SYS_sendmsg]	= { Pfd, Pptr, Sendrecvflagsname },
    [SYS_recvfrom]	= { Pfd, Pptr, Pbigsize, Sendrecvflagsname },
    [SYS_accept]	= { Pfd, Pptr, Pptr },
    [SYS_getpeername]	= { Pfd, Pptr, Pptr },
    [SYS_getsockname]	= { Pfd, Pptr, Pptr },
    [SYS_access]	= { Ppath, Accessmodename },
    [SYS_chflags]	= { Ppath, Chflagsname },
    [SYS_fchflags]	= { Pfd, Chflagsname },
    [SYS_kill]		= { Ppgid, Signame },
    [SYS_stat]		= { Ppath, Pptr },
    [SYS_lstat]		= { Ppath, Pptr },
    [SYS_dup]		= { Pfd },
    [SYS_fstatat]	= { Atfd, Ppath, Pptr, Atflagsname },
    [SYS_profil]	= { Pptr, Pbigsize, Pbigsize, Pdecuint },
    [SYS_ktrace]	= { Ppath, Ktraceopname, Ktracefacname, Ppgid },
    [SYS_sigaction]	= { Signame, Pptr, Pptr },
    [SYS_sigprocmask]	= { Sigprocmaskhowname, Sigset },
    [SYS_getlogin]	= { Pptr, Pucount },
    [SYS_setlogin]	= { Pptr },
    [SYS_acct]		= { Ppath },
    [SYS_fstat]		= { Pfd, Pptr },
    [SYS_ioctl]		= { Pfd, Ioctldecode, Pptr },
    [SYS_reboot]	= { Rebootoptname },
    [SYS_revoke]	= { Ppath },
    [SYS_symlink]	= { Ppath, Ppath },
    [SYS_readlink]	= { Ppath, Pptr, Psize },
    [SYS_execve]	= { Ppath, Pptr, Pptr },
    [SYS_umask]		= { Modename },
    [SYS_chroot]	= { Ppath },
    [SYS_getfsstat]	= { Pptr, Pbigsize, Mountflagsname },
    [SYS_statfs]	= { Ppath, Pptr },
    [SYS_fstatfs]	= { Pfd, Pptr },
    [SYS_fhstatfs]	= { Pptr, Pptr },
    [SYS_gettimeofday]	= { Pptr, Pptr },
    [SYS_settimeofday]	= { Pptr, Pptr },
    [SYS_setitimer]	= { Itimername, Pptr, Pptr },
    [SYS_getitimer]	= { Itimername, Pptr },
    [SYS_select]	= { Pcount, Pptr, Pptr, Pptr, Pptr },
    [SYS_kevent]	= { Pfd, Pptr, Pcount, Pptr, Pcount, Pptr },
    [SYS_munmap]	= { Pptr, Pbigsize },
    [SYS_mprotect]	= { Pptr, Pbigsize, Mmapprotname },
    [SYS_madvise]	= { Pptr, Pbigsize, Madvisebehavname },
    [SYS_utimes]	= { Ppath, Pptr },
    [SYS_futimes]	= { Pfd, Pptr },
    [SYS_mincore]	= { Pptr, Pbigsize, Pptr },
    [SYS_getgroups]	= { Pcount, Pptr },
    [SYS_setgroups]	= { Pcount, Pptr },
    [SYS_setpgid]	= { Ppid_t, Ppid_t },
    [SYS_sendsyslog]	= { Pptr, Psize },
    [SYS_utimensat]	= { Atfd, Ppath, Pptr, Atflagsname },
    [SYS_futimens]	= { Pfd, Pptr },
    [SYS_clock_gettime]	= { Clockname, Pptr },
    [SYS_clock_settime]	= { Clockname, Pptr },
    [SYS_clock_getres]	= { Clockname, Pptr },
    [SYS_dup2]		= { Pfd, Pfd },
    [SYS_nanosleep]	= { Pptr, Pptr },
    [SYS_fcntl]		= { Pfd, PASS_TWO, Fcntlcmdname },
    [SYS_accept4]	= { Pfd, Pptr, Pptr, Sockflagsname },
    [SYS___thrsleep]	= { Pptr, Clockname, Pptr, Pptr, Pptr },
    [SYS_fsync]		= { Pfd },
    [SYS_setpriority]	= { Prioname, Ppid_t, Pdecint },
    [SYS_socket]	= { Sockfamilyname, Socktypename, Sockprotoname },
    [SYS_connect]	= { Pfd, Pptr, Pucount },
    [SYS_getdents]	= { Pfd, Pptr, Pbigsize },
    [SYS_getpriority]	= { Prioname, Ppid_t },
    [SYS_pipe2]		= { Pptr, Flagsname },
    [SYS_dup3]		= { Pfd, Pfd, Flagsname },
    [SYS_sigreturn]	= { Pptr },
    [SYS_bind]		= { Pfd, Pptr, Pucount },
    [SYS_setsockopt]	= { Pfd, PASS_TWO, Sockoptlevelname, Pptr, Pdecint },
    [SYS_listen]	= { Pfd, Pdecint },
    [SYS_chflagsat]	= { Atfd, Ppath, Chflagsname, Atflagsname },
    [SYS_ppoll]		= { Pptr, Pucount, Pptr, Pptr },
    [SYS_pselect]	= { Pcount, Pptr, Pptr, Pptr, Pptr, Pptr },
    [SYS_sigsuspend]	= { Sigset },
    [SYS_getsockopt]	= { Pfd, PASS_TWO, Sockoptlevelname, Pptr, Pptr },
    [SYS_readv]		= { Pfd, Pptr, Pcount },
    [SYS_writev]	= { Pfd, Pptr, Pcount },
    [SYS_fchown]	= { Pfd, Uidname, Gidname },
    [SYS_fchmod]	= { Pfd, Modename },
    [SYS_setreuid]	= { Uidname, Uidname },
    [SYS_setregid]	= { Gidname, Gidname },
    [SYS_rename]	= { Ppath, Ppath },
    [SYS_flock]		= { Pfd, Flockname },
    [SYS_mkfifo]	= { Ppath, Modename },
    [SYS_sendto]	= { Pfd, Pptr, Pbigsize, Sendrecvflagsname },
    [SYS_shutdown]	= { Pfd, Shutdownhowname },
    [SYS_socketpair]	= { Sockfamilyname, Socktypename, Sockprotoname, Pptr },
    [SYS_mkdir]		= { Ppath, Modename },
    [SYS_rmdir]		= { Ppath },
    [SYS_adjtime]	= { Pptr, Pptr },
    [SYS_quotactl]	= { Ppath, Quotactlname, Uidname, Pptr },
    [SYS_nfssvc]	= { Phexint, Pptr },
    [SYS_getfh]		= { Ppath, Pptr },
    [SYS_sysarch]	= { Pdecint, Pptr },
    [SYS_pread]		= { Pfd, Pptr, Pbigsize, PAD, Poff_t },
    [SYS_pwrite]	= { Pfd, Pptr, Pbigsize, PAD, Poff_t },
    [SYS_setgid]	= { Gidname },
    [SYS_setegid]	= { Gidname },
    [SYS_seteuid]	= { Uidname },
    [SYS_pathconf]	= { Ppath, Pathconfname },
    [SYS_fpathconf]	= { Pfd, Pathconfname },
    [SYS_swapctl]	= { Swapctlname, Pptr, Pdecint },
    [SYS_getrlimit]	= { Rlimitname, Pptr },
    [SYS_setrlimit]	= { Rlimitname, Pptr },
    [SYS_mmap]		= { Pptr, Pbigsize, Mmapprotname, Mmapflagsname, Pfd, PAD, Poff_t },
    [SYS_lseek]		= { Pfd, PAD, Poff_t, Whencename },
    [SYS_truncate]	= { Ppath, PAD, Poff_t },
    [SYS_ftruncate]	= { Pfd, PAD, Poff_t },
    /* [SYS___sysctl]	= { }, Magic */
    [SYS_mlock]		= { Pptr, Pbigsize },
    [SYS_munlock]	= { Pptr, Pbigsize },
    [SYS_getpgid]	= { Ppid_t },
    [SYS_utrace]	= { Pptr, Pptr, Psize },
    [SYS_semget]	= { Pkey_t, Pcount, Semgetname },
    [SYS_msgget]	= { Pkey_t, Msgflgname },
    [SYS_msgsnd]	= { Pmsqid, Pptr, Psize, Msgflgname },
    [SYS_msgrcv]	= { Pmsqid, Pptr, Psize, Pdeclong, Msgflgname },
    [SYS_shmat]		= { Pshmid, Pptr, Shmatname },
    [SYS_shmdt]		= { Pptr },
    [SYS_minherit]	= { Pptr, Pbigsize, Minheritname },
    [SYS_poll]		= { Pptr, Pucount, Polltimeout },
    [SYS_lchown]	= { Ppath, Uidname, Gidname },
    [SYS_getsid]	= { Ppid_t },
    [SYS_msync]		= { Pptr, Pbigsize, Msyncflagsname },
    [SYS_pipe]		= { Pptr },
    [SYS_fhopen]	= { Pptr, Openflagsname },
    [SYS_preadv]	= { Pfd, Pptr, Pcount, PAD, Poff_t },
    [SYS_pwritev]	= { Pfd, Pptr, Pcount, PAD, Poff_t },
    [SYS_mlockall]	= { Mlockallname },
    [SYS_getresuid]	= { Pptr, Pptr, Pptr },
    [SYS_setresuid]	= { Uidname, Uidname, Uidname },
    [SYS_getresgid]	= { Pptr, Pptr, Pptr },
    [SYS_setresgid]	= { Gidname, Gidname, Gidname },
    [SYS_mquery]	= { Pptr, Pbigsize, Mmapprotname, Mmapflagsname, Pfd, PAD, Poff_t },
    [SYS_closefrom]	= { Pfd },
    [SYS_sigaltstack]	= { Pptr, Pptr },
    [SYS_shmget]	= { Pkey_t, Pbigsize, Semgetname },
    [SYS_semop]		= { Psemid, Pptr, Psize },
    [SYS_fhstat]	= { Pptr, Pptr },
    [SYS___semctl]	= { Psemid, Pcount, Semctlname, Pptr },
    [SYS_shmctl]	= { Pshmid, Shmctlname, Pptr },
    [SYS_msgctl]	= { Pmsqid, Shmctlname, Pptr },
    [SYS___thrwakeup]	= { Pptr, Pcount },
    [SYS___threxit]	= { Pptr },
    [SYS___thrsigdivert] = { Sigset, Pptr, Pptr },
    [SYS___getcwd]	= { Pptr, Psize },
    [SYS_adjfreq]	= { Pptr, Pptr },
    [SYS_setrtable]	= { Pdecint },
    [SYS_faccessat]	= { Atfd, Ppath, Accessmodename, Atflagsname },
    [SYS_fchmodat]	= { Atfd, Ppath, Modename, Atflagsname },
    [SYS_fchownat]	= { Atfd, Ppath, Uidname, Gidname, Atflagsname },
    [SYS_linkat]	= { Atfd, Ppath, Atfd, Ppath, Atflagsname },
    [SYS_mkdirat]	= { Atfd, Ppath, Modename },
    [SYS_mkfifoat]	= { Atfd, Ppath, Modename },
    [SYS_mknodat]	= { Atfd, Ppath, Modename, Pdev_t },
    [SYS_openat]	= { Atfd, Ppath, PASS_TWO, Flagsandmodename },
    [SYS_readlinkat]	= { Atfd, Ppath, Pptr, Psize },
    [SYS_renameat]	= { Atfd, Ppath, Atfd, Ppath },
    [SYS_symlinkat]	= { Ppath, Atfd, Ppath },
    [SYS_unlinkat]	= { Atfd, Ppath, Atflagsname },
    [SYS___set_tcb]	= { Pptr },
};


static void
ktrsyscall(struct ktr_syscall *ktr, size_t ktrlen)
{
	register_t *ap;
	int narg;
	char sep;

	if (ktr->ktr_argsize > ktrlen)
		errx(1, "syscall argument length %d > ktr header length %zu",
		    ktr->ktr_argsize, ktrlen);

	narg = ktr->ktr_argsize / sizeof(register_t);
	sep = '\0';

	if (ktr->ktr_code >= current->nsysnames || ktr->ktr_code < 0)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", current->sysnames[ktr->ktr_code]);
	ap = (register_t *)((char *)ktr + sizeof(struct ktr_syscall));
	(void)putchar('(');

	if (current != &emulations[0])
		goto nonnative;

	if (ktr->ktr_code == SYS___sysctl) {
		const char *s;
		int n, i, *top;

		if (!fancy)
			goto nonnative;
		n = ap[1];
		if (n > CTL_MAXNAME)
			n = CTL_MAXNAME;
		if (n < 0)
			errx(1, "invalid sysctl length %d", n);
		if (n > 0) {
			top = (int *)(ap + 6);
			printf("%d", top[0]);
			for (i = 1; i < n; i++)
				printf(".%d", top[i]);
			if ((s = kresolvsysctl(0, top)) != NULL) {
				printf("<%s", s);
				for (i = 1; i < n; i++) {
					if ((s = kresolvsysctl(i, top)) != NULL)
						printf(".%s", s);
					else
						printf(".%d", top[i]);
				}
				putchar('>');
			}
		}

		sep = ',';
		ap += 2;
		narg -= 2;
	} else if (ktr->ktr_code < nitems(scargs)) {
		const formatter *fmts = scargs[ktr->ktr_code];
		int fmt;

		while (narg && (fmt = *fmts) != 0) {
			if (sep)
				putchar(sep);
			sep = ',';
			if (!fancy && !FMT_IS_NONFANCY(fmt))
				fmt = Pnonfancy;
			if (fmt > 0)
				formatters[fmt]((int)*ap);
			else if (long_formatters[-fmt](*ap))
				sep = '\0';
			fmts++;
			ap++;
			narg--;
		}
	}

nonnative:
	while (narg > 0) {
		if (sep)
			putchar(sep);
		if (decimal)
			(void)printf("%ld", (long)*ap);
		else
			(void)printf("%#lx", (long)*ap);
		sep = ',';
		ap++;
		narg--;
	}
	(void)printf(")\n");
}

static struct ctlname topname[] = CTL_NAMES;
static struct ctlname kernname[] = CTL_KERN_NAMES;
static struct ctlname vmname[] = CTL_VM_NAMES;
static struct ctlname fsname[] = CTL_FS_NAMES;
static struct ctlname netname[] = CTL_NET_NAMES;
static struct ctlname hwname[] = CTL_HW_NAMES;
static struct ctlname debugname[CTL_DEBUG_MAXID];
static struct ctlname kernmallocname[] = CTL_KERN_MALLOC_NAMES;
static struct ctlname forkstatname[] = CTL_KERN_FORKSTAT_NAMES;
static struct ctlname nchstatsname[] = CTL_KERN_NCHSTATS_NAMES;
static struct ctlname kernprocname[] = {
	{ NULL },
	{ "all" },
	{ "pid" },
	{ "pgrp" },
	{ "session" },
	{ "tty" },
	{ "uid" },
	{ "ruid" },
	{ "kthread" },
};
static struct ctlname ttysname[] = CTL_KERN_TTY_NAMES;
static struct ctlname semname[] = CTL_KERN_SEMINFO_NAMES;
static struct ctlname shmname[] = CTL_KERN_SHMINFO_NAMES;
static struct ctlname watchdogname[] = CTL_KERN_WATCHDOG_NAMES;
static struct ctlname tcname[] = CTL_KERN_TIMECOUNTER_NAMES;
#ifdef CTL_MACHDEP_NAMES
static struct ctlname machdepname[] = CTL_MACHDEP_NAMES;
#endif
static struct ctlname ddbname[] = CTL_DDB_NAMES;

#ifndef nitems
#define nitems(_a)    (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define SETNAME(name) do { names = (name); limit = nitems(name); } while (0)

static const char *
kresolvsysctl(int depth, const int *top)
{
	struct ctlname *names;
	size_t		limit;
	int		idx = top[depth];

	names = NULL;

	switch (depth) {
	case 0:
		SETNAME(topname);
		break;
	case 1:
		switch (top[0]) {
		case CTL_KERN:
			SETNAME(kernname);
			break;
		case CTL_VM:
			SETNAME(vmname);
			break;
		case CTL_FS:
			SETNAME(fsname);
			break;
		case CTL_NET:
			SETNAME(netname);
			break;
		case CTL_DEBUG:
			SETNAME(debugname);
			break;
		case CTL_HW:
			SETNAME(hwname);
			break;
#ifdef CTL_MACHDEP_NAMES
		case CTL_MACHDEP:
			SETNAME(machdepname);
			break;
#endif
		case CTL_DDB:
			SETNAME(ddbname);
			break;
		}
		break;
	case 2:
		switch (top[0]) {
		case CTL_KERN:
			switch (top[1]) {
			case KERN_MALLOCSTATS:
				SETNAME(kernmallocname);
				break;
			case KERN_FORKSTAT:
				SETNAME(forkstatname);
				break;
			case KERN_NCHSTATS:
				SETNAME(nchstatsname);
				break;
			case KERN_TTY:
				SETNAME(ttysname);
				break;
			case KERN_SEMINFO:
				SETNAME(semname);
				break;
			case KERN_SHMINFO:
				SETNAME(shmname);
				break;
			case KERN_WATCHDOG:
				SETNAME(watchdogname);
				break;
			case KERN_PROC:
				idx++;	/* zero is valid at this level */
				SETNAME(kernprocname);
				break;
			case KERN_TIMECOUNTER:
				SETNAME(tcname);
				break;
			}
		}
		break;
	}
	if (names != NULL && idx > 0 && idx < limit)
		return (names[idx].ctl_name);
	return (NULL);
}

static void
ktrsysret(struct ktr_sysret *ktr)
{
	register_t ret = ktr->ktr_retval;
	int error = ktr->ktr_error;
	int code = ktr->ktr_code;

	if (code >= current->nsysnames || code < 0)
		(void)printf("[%d] ", code);
	else {
		(void)printf("%s ", current->sysnames[code]);
		if (ret > 0 && (strcmp(current->sysnames[code], "fork") == 0 ||
		    strcmp(current->sysnames[code], "vfork") == 0 ||
		    strcmp(current->sysnames[code], "__tfork") == 0 ||
		    strcmp(current->sysnames[code], "clone") == 0))
			mappidtoemul(ret, current);
	}

	if (error == 0) {
		if (fancy) {
			switch (current == &emulations[0] ? code : -1) {
			case SYS_sigprocmask:
			case SYS_sigpending:
				sigset(ret);
				break;
			case SYS___thrsigdivert:
				signame(ret);
				break;
			case SYS_getuid:
			case SYS_geteuid:
				uidname(ret);
				break;
			case SYS_getgid:
			case SYS_getegid:
				gidname(ret);
				break;
			case -1:	/* non-default emulation */
			default:
				(void)printf("%ld", (long)ret);
				if (ret < 0 || ret > 9)
					(void)printf("/%#lx", (long)ret);
			}
		} else {
			if (decimal)
				(void)printf("%ld", (long)ret);
			else
				(void)printf("%#lx", (long)ret);
		}
	} else if (error == ERESTART)
		(void)printf("RESTART");
	else if (error == EJUSTRETURN)
		(void)printf("JUSTRETURN");
	else {
		(void)printf("-1 errno %d", ktr->ktr_error);
		if (fancy)
			(void)printf(" %s", strerror(ktr->ktr_error));
	}
	(void)putchar('\n');
}

static void
ktrnamei(const char *cp, size_t len)
{
	(void)printf("\"%.*s\"\n", (int)len, cp);
}

static void
ktremul(char *cp, size_t len)
{
	char name[1024];

	if (len >= sizeof(name))
		errx(1, "Emulation name too long");

	strncpy(name, cp, len);
	name[len] = '\0';
	(void)printf("\"%s\"\n", name);

	setemul(name);
}

static void
showbuf(unsigned char *dp, size_t datalen)
{
	int i, j;
	static int screenwidth;
	int col = 0, width, bpl;
	unsigned char visbuf[5], *cp, c;

	if (screenwidth == 0) {
		struct winsize ws;

		if (fancy && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}
	if (iohex == 1) {
		putchar('\t');
		col = 8;
		for (i = 0; i < datalen; i++) {
			printf("%02x", dp[i]);
			col += 3;
			if (i < datalen - 1) {
				if (col + 3 > screenwidth) {
					printf("\n\t");
					col = 8;
				} else
					putchar(' ');
			}
		}
		putchar('\n');
		return;
	}
	if (iohex == 2) {
		bpl = (screenwidth - 13)/4;
		if (bpl <= 0)
			bpl = 1;
		for (i = 0; i < datalen; i += bpl) {
			printf("   %04x:  ", i);
			for (j = 0; j < bpl; j++) {
				if (i+j >= datalen)
					printf("   ");
				else
					printf("%02x ", dp[i+j]);
			}
			putchar(' ');
			for (j = 0; j < bpl; j++) {
				if (i+j >= datalen)
					break;
				c = dp[i+j];
				if (!isprint(c))
					c = '.';
				putchar(c);
			}
			putchar('\n');
		}
		return;
	}
	(void)printf("       \"");
	col = 8;
	for (; datalen > 0; datalen--, dp++) {
		(void)vis(visbuf, *dp, VIS_CSTYLE, *(dp+1));
		cp = visbuf;

		/*
		 * Keep track of printables and
		 * space chars (like fold(1)).
		 */
		if (col == 0) {
			(void)putchar('\t');
			col = 8;
		}
		switch (*cp) {
		case '\n':
			col = 0;
			(void)putchar('\n');
			continue;
		case '\t':
			width = 8 - (col&07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth-2)) {
			(void)printf("\\\n\t");
			col = 8;
		}
		col += width;
		do {
			(void)putchar(*cp++);
		} while (*cp);
	}
	if (col == 0)
		(void)printf("       ");
	(void)printf("\"\n");
}

static void
ktrgenio(struct ktr_genio *ktr, size_t len)
{
	unsigned char *dp = (unsigned char *)ktr + sizeof(struct ktr_genio);
	size_t datalen;

	if (len < sizeof(struct ktr_genio))
		errx(1, "invalid ktr genio length %zu", len);

	datalen = len - sizeof(struct ktr_genio);

	printf("fd %d %s %zu bytes\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen);
	if (maxdata == 0)
		return;
	if (datalen > maxdata)
		datalen = maxdata;
	if (iohex && !datalen)
		return;
	showbuf(dp, datalen);
}

static void
ktrpsig(struct ktr_psig *psig)
{
	signame(psig->signo);
	printf(" ");
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL");
	else {
		(void)printf("caught handler=0x%lx mask=",
		    (u_long)psig->action);
		sigset(psig->mask);
	}
	if (psig->code) {
		printf(" code ");
		if (fancy) {
			switch (psig->signo) {
			case SIGILL:
				sigill_name(psig->code);
				break;
			case SIGTRAP:
				sigtrap_name(psig->code);
				break;
			case SIGEMT:
				sigemt_name(psig->code);
				break;
			case SIGFPE:
				sigfpe_name(psig->code);
				break;
			case SIGBUS:
				sigbus_name(psig->code);
				break;
			case SIGSEGV:
				sigsegv_name(psig->code);
				break;
			case SIGCHLD:
				sigchld_name(psig->code);
				break;
			}
		}
		printf("<%d>", psig->code);
	}

	switch (psig->signo) {
	case SIGSEGV:
	case SIGILL:
	case SIGBUS:
	case SIGFPE:
		printf(" addr=%p trapno=%d", psig->si.si_addr,
		    psig->si.si_trapno);
		break;
	default:
		break;
	}
	printf("\n");
}

static void
ktrcsw(struct ktr_csw *cs)
{
	(void)printf("%s %s\n", cs->out ? "stop" : "resume",
	    cs->user ? "user" : "kernel");
}

static void
ktruser(struct ktr_user *usr, size_t len)
{
	if (len < sizeof(struct ktr_user))
		errx(1, "invalid ktr user length %zu", len);
	len -= sizeof(struct ktr_user);
	printf("%.*s:", KTR_USER_MAXIDLEN, usr->ktr_id);
	printf(" %zu bytes\n", len);
	showbuf((unsigned char *)(usr + 1), len);
}

static void
usage(void)
{

	extern char *__progname;
	fprintf(stderr, "usage: %s "
	    "[-dHlnRTXx] [-e emulation] [-f file] [-m maxdata] [-p pid]\n"
	    "%*s[-t [ceinstuw]]\n",
	    __progname, (int)(sizeof("usage: ") + strlen(__progname)), "");
	exit(1);
}

static void
setemul(const char *name)
{
	int i;

	for (i = 0; emulations[i].name != NULL; i++)
		if (strcmp(emulations[i].name, name) == 0) {
			current = &emulations[i];
			return;
		}
	warnx("Emulation `%s' unknown", name);
}

/*
 * FORMATTERS
 */

static void
ioctldecode(int cmd)
{
	char dirbuf[4], *dir = dirbuf;
	const char *cp;

	if ((cp = ioctlname((unsigned)cmd)) != NULL) {
		(void)printf("%s", cp);
		return;
	}

	if (cmd & IOC_IN)
		*dir++ = 'W';
	if (cmd & IOC_OUT)
		*dir++ = 'R';
	*dir = '\0';

	printf("_IO%s('%c',%lu",
	    dirbuf, (int)((cmd >> 8) & 0xff), cmd & 0xff);
	if ((cmd & IOC_VOID) == 0)
		printf(decimal ? ",%u)" : ",%#x)", (cmd >> 16) & 0xff);
	else
		printf(")");
}

static void
ptracedecode(int request)
{
	if (request >= 0 && request < nitems(ptrace_ops))
		(void)printf("%s", ptrace_ops[request]);
	else switch(request) {
#ifdef PT_GETFPREGS
	case PT_GETFPREGS:
		(void)printf("PT_GETFPREGS");
		break;
#endif
	case PT_GETREGS:
		(void)printf("PT_GETREGS");
		break;
#ifdef PT_GETXMMREGS
	case PT_GETXMMREGS:
		(void)printf("PT_GETXMMREGS");
		break;
#endif
#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
		(void)printf("PT_SETFPREGS");
		break;
#endif
	case PT_SETREGS:
		(void)printf("PT_SETREGS");
		break;
#ifdef PT_SETXMMREGS
	case PT_SETXMMREGS:
		(void)printf("PT_SETXMMREGS");
		break;
#endif
#ifdef PT_STEP
	case PT_STEP:
		(void)printf("PT_STEP");
		break;
#endif
#ifdef PT_WCOOKIE
	case PT_WCOOKIE:
		(void)printf("PT_WCOOKIE");
		break;
#endif
	default:
		pdecint(request);
	}
}


static void
atfd(int fd)
{
	if (fd == AT_FDCWD)
		(void)printf("AT_FDCWD");
	else
		pdecint(fd);
}

static void
polltimeout(int timeout)
{
	if (timeout == INFTIM)
		(void)printf("INFTIM");
	else
		pdecint(timeout);
}

static void
wait4pid(int pid)
{
	if (pid == WAIT_ANY)
		(void)printf("WAIT_ANY");
	else if (pid == WAIT_MYPGRP)
		(void)printf("WAIT_MYPGRP");
	else
		pdecint(pid);		/* ppgid */
}

static void
signame(int sig)
{
	if (sig > 0 && sig < NSIG)
		(void)printf("SIG%s", sys_signame[sig]);
	else
		(void)printf("SIG %d", sig);
}

void
sigset(int ss)
{
	int	or = 0;
	int	cnt = 0;
	int	i;

	for (i = 1; i < NSIG; i++)
		if (sigismember(&ss, i))
			cnt++;
	if (cnt > (NSIG-1)/2) {
		ss = ~ss;
		putchar('~');
	}

	if (ss == 0) {
		(void)printf("0<>");
		return;
	}

	printf("%#x<", ss);
	for (i = 1; i < NSIG; i++)
		if (sigismember(&ss, i)) {
			if (or) putchar('|'); else or=1;
			signame(i);
		}
	printf(">");
}

static void
semctlname(int cmd)
{
	switch (cmd) {
	case GETNCNT:
		(void)printf("GETNCNT");
		break;
	case GETPID:
		(void)printf("GETPID");
		break;
	case GETVAL:
		(void)printf("GETVAL");
		break;
	case GETALL:
		(void)printf("GETALL");
		break;
	case GETZCNT:
		(void)printf("GETZCNT");
		break;
	case SETVAL:
		(void)printf("SETVAL");
		break;
	case SETALL:
		(void)printf("SETALL");
		break;
	case IPC_RMID:
		(void)printf("IPC_RMID");
		break;
	case IPC_SET:
		(void)printf("IPC_SET");
		break;
	case IPC_STAT:
		(void)printf("IPC_STAT");
		break;
	default: /* Should not reach */
		(void)printf("<invalid=%d>", cmd);
	}
}

static void
shmctlname(int cmd)
{
	switch (cmd) {
	case IPC_RMID:
		(void)printf("IPC_RMID");
		break;
	case IPC_SET:
		(void)printf("IPC_SET");
		break;
	case IPC_STAT:
		(void)printf("IPC_STAT");
		break;
	default: /* Should not reach */
		(void)printf("<invalid=%d>", cmd);
	}
}


static void
semgetname(int flag)
{
	int	or = 0;
	if_print_or(flag, IPC_CREAT, or);
	if_print_or(flag, IPC_EXCL, or);
	if_print_or(flag, SEM_R, or);
	if_print_or(flag, SEM_A, or);
	if_print_or(flag, (SEM_R>>3), or);
	if_print_or(flag, (SEM_A>>3), or);
	if_print_or(flag, (SEM_R>>6), or);
	if_print_or(flag, (SEM_A>>6), or);

	if (flag & ~(IPC_CREAT|IPC_EXCL|SEM_R|SEM_A|((SEM_R|SEM_A)>>3)|
	    ((SEM_R|SEM_A)>>6)))
		printf("<invalid=%#x>", flag);
}


/*
 * Only used by SYS_open and SYS_openat. Unless O_CREAT is set in flags, the
 * mode argument is unused (and often bogus and misleading).
 */
static void
flagsandmodename(int mode)
{
	openflagsname(arg1);
	if ((arg1 & O_CREAT) == O_CREAT) {
		(void)putchar(',');
		modename(mode);
	} else if (!fancy)
		(void)printf(",<unused>%#o", mode);
}

static void
clockname(int clockid)
{
	clocktypename(__CLOCK_TYPE(clockid));
	if (__CLOCK_PTID(clockid) != 0)
		printf("(%d)", __CLOCK_PTID(clockid));
}

/*
 * [g|s]etsockopt's level argument can either be SOL_SOCKET or a value
 * referring to a line in /etc/protocols . It might be appropriate
 * to use getprotoent(3) here.
 */
static void
sockoptlevelname(int level)
{
	if (level == SOL_SOCKET)
		(void)printf("SOL_SOCKET");
	else
		pdecint(level);
}

static void
ktraceopname(int ops)
{
	int invalid = 0;

	printf("%#x<", ops);
	switch (KTROP(ops)) {
	case KTROP_SET:
		printf("KTROP_SET");
		break;
	case KTROP_CLEAR:
		printf("KTROP_CLEAR");
		break;
	case KTROP_CLEARFILE:
		printf("KTROP_CLEARFILE");
		break;
	default:
		printf("KTROP(%d)", KTROP(ops));
		invalid = 1;
		break;
	}
	if (ops & KTRFLAG_DESCEND) printf("|KTRFLAG_DESCEND");
	printf(">");
	if (invalid || (ops & ~(KTROP((unsigned)-1) | KTRFLAG_DESCEND)))
		(void)printf("<invalid>%d", ops);
}
