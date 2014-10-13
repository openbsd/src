/*	$OpenBSD: kdump.c,v 1.91 2014/10/13 03:46:33 guenther Exp $	*/

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

#include <sys/param.h>
#include <sys/time.h>
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
int needtid, resolv, tail;
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

static int narg;
static register_t *ap;
static char sep;

static void mappidtoemul(pid_t, struct emulation *);
static struct emulation * findemul(pid_t);
static int fread_tail(void *, size_t, size_t);
static void dumpheader(struct ktr_header *);
static void ktrcsw(struct ktr_csw *);
static void ktremul(char *, size_t);
static void ktrgenio(struct ktr_genio *, size_t);
static void ktrnamei(const char *, size_t);
static void ktrpsig(struct ktr_psig *);
static void ktrsyscall(struct ktr_syscall *);
static const char *kresolvsysctl(int, int *, int);
static void ktrsysret(struct ktr_sysret *);
static void ktruser(struct ktr_user *, size_t);
static void setemul(const char *);
static void usage(void);
static void atfd(int);
static void polltimeout(int);
static void pgid(int);
static void wait4pid(int);
static void signame(int);
static void semctlname(int);
static void shmctlname(int);
static void semgetname(int);
static void flagsandmodename(int, int);
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

	while ((ch = getopt(argc, argv, "e:f:dHlm:nrRp:Tt:xX")) != -1)
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
		case 'r':
			resolv = 1;
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
			ktrsyscall((struct ktr_syscall *)m);
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

static void
ioctldecode(u_long cmd)
{
	char dirbuf[4], *dir = dirbuf;

	if (cmd & IOC_IN)
		*dir++ = 'W';
	if (cmd & IOC_OUT)
		*dir++ = 'R';
	*dir = '\0';

	printf(decimal ? ",_IO%s('%c',%lu" : ",_IO%s('%c',%#lx",
	    dirbuf, (int)((cmd >> 8) & 0xff), cmd & 0xff);
	if ((cmd & IOC_VOID) == 0)
		printf(decimal ? ",%lu)" : ",%#lx)", (cmd >> 16) & 0xff);
	else
		printf(")");
}

static void
ptracedecode(void)
{
	if (*ap >= 0 && *ap <
	    sizeof(ptrace_ops) / sizeof(ptrace_ops[0]))
		(void)printf("%s", ptrace_ops[*ap]);
	else switch(*ap) {
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
		(void)printf("%ld", (long)*ap);
		break;
	}
	sep = ',';
	ap++;
	narg--;
}

static void
pn(void (*f)(int))
{
	if (sep)
		(void)putchar(sep);
	if (fancy && f != NULL)
		f((int)*ap);
	else if (decimal)
		(void)printf("%ld", (long)*ap);
	else
		(void)printf("%#lx", (long)*ap);
	ap++;
	narg--;
	sep = ',';
}

#ifdef __LP64__
#define plln()	pn(NULL)
#elif _BYTE_ORDER == _LITTLE_ENDIAN
static void
plln(void)
{
	long long val = ((long long)*ap) & 0xffffffff;
	ap++;
	val |= ((long long)*ap) << 32;
	ap++;
	narg -= 2;
	if (sep)
		(void)putchar(sep);
	if (decimal)
		(void)printf("%lld", val);
	else
		(void)printf("%#llx", val);
	sep = ',';
}
#else
static void
plln(void)
{
	long long val = ((long long)*ap) << 32;
	ap++;
	val |= ((long long)*ap) & 0xffffffff;
	ap++;
	narg -= 2;
	if (sep)
		(void)putchar(sep);
	if (decimal)
		(void)printf("%lld", val);
	else
		(void)printf("%#llx", val);
	sep = ',';
}
#endif

static void
ktrsyscall(struct ktr_syscall *ktr)
{
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

	switch (ktr->ktr_code) {
	case SYS_ioctl: {
		const char *cp;

		pn(NULL);
		if (!fancy)
			break;
		if ((cp = ioctlname(*ap)) != NULL)
			(void)printf(",%s", cp);
		else
			ioctldecode(*ap);
		ap++;
		narg--;
		break;
	}
	case SYS___sysctl: {
		const char *s;
		int *np, n, i, *top;

		if (!fancy)
			break;
		n = ap[1];
		if (n > CTL_MAXNAME)
			n = CTL_MAXNAME;
		np = top = (int *)(ap + 6);
		for (i = 0; n--; np++, i++) {
			if (sep)
				putchar(sep);
			if (resolv && (s = kresolvsysctl(i, top, *np)) != NULL)
				printf("%s", s);
			else
				printf("%d", *np);
			sep = '.';
		}

		sep = ',';
		ap += 2;
		narg -= 2;
		break;
	}
	case SYS_ptrace: 
		if (!fancy)
			break;
		ptracedecode();
		break;
	case SYS_accept4:
		pn(NULL);
		pn(NULL);
		pn(NULL);
		pn(sockflagsname);
		break;
	case SYS_access:
		pn(NULL);
		pn(accessmodename);
		break;
	case SYS_chmod:
	case SYS_fchmod: 
	case SYS_mkdir:
	case SYS_mkfifo:
	case SYS_mknod:
		pn(NULL);
		pn(modename);
		break;
	case SYS_umask:
		pn(modename);
		break;
	case SYS_dup3:
		pn(NULL);
		pn(NULL);
		pn(flagsname);
		break;
	case SYS_fcntl: {
		int cmd;
		int arg;
		pn(NULL);
		if (!fancy)
			break;
		cmd = ap[0];
		arg = ap[1];
		(void)putchar(',');
		fcntlcmdname(cmd, arg);
		ap += 2;
		narg -= 2;
		break;
	}
	case SYS_flock:
		pn(NULL);
		pn(flockname);
		break;
	case SYS_getrlimit:
	case SYS_setrlimit:
		pn(rlimitname);
		break;
	case SYS_getsockopt:
	case SYS_setsockopt: {
		int level;

		pn(NULL);
		level = *ap;
		pn(sockoptlevelname);
		if (level == SOL_SOCKET)
			pn(sockoptname);
		break;
	}
	case SYS_kill:
		pn(pgid);
		pn(signame);
		break;
	case SYS_lseek:
		pn(NULL);
		/* skip padding */
		ap++;
		narg--;
		plln();
		pn(whencename);
		break;
	case SYS_madvise:
		pn(NULL);
		pn(NULL);
		pn(madvisebehavname);
		break;
	case SYS_minherit:
		pn(NULL);
		pn(NULL);
		pn(minheritname);
		break;
	case SYS_mlockall:
		pn(mlockallname);
		break;
	case SYS_mmap:
	case SYS_mquery:
		pn(NULL);
		pn(NULL);
		pn(mmapprotname);
		pn(mmapflagsname);
		pn(NULL);
		/* skip padding */
		ap++;
		narg--;
		plln();
		break;
	case SYS_mprotect:
		pn(NULL);
		pn(NULL);
		pn(mmapprotname);
		break;
	case SYS_msync:
		pn(NULL);
		pn(NULL);
		pn(msyncflagsname);
		break;
	case SYS_msgctl:
		pn(NULL);
		pn(shmctlname);
		break;
	case SYS_open: {
		int     flags;
		int     mode;

		pn(NULL);
		if (!fancy)
			break;
		flags = ap[0];
		mode = ap[1];
		(void)putchar(',');
		flagsandmodename(flags, mode);
		ap += 2;
		narg -= 2;
		break;
	}
	case SYS_pipe2:
		pn(NULL);
		pn(flagsname);
		break;
	case SYS_pread:
	case SYS_preadv:
	case SYS_pwrite:
	case SYS_pwritev:
		pn(NULL);
		pn(NULL);
		pn(NULL);
		/* skip padding */
		ap++;
		narg--;
		plln();
		break;
	case SYS_recvmsg:
	case SYS_sendmsg:
		pn(NULL);
		pn(NULL);
		pn(sendrecvflagsname);
		break;
	case SYS_recvfrom:
	case SYS_sendto:
		pn(NULL);
		pn(NULL);
		pn(NULL);
		pn(sendrecvflagsname);
		break;
	case SYS_shutdown:
		pn(NULL);
		pn(shutdownhowname);
		break;
	case SYS___semctl:
		pn(NULL);
		pn(NULL);
		pn(semctlname);
		break;
	case SYS_semget:
		pn(NULL);
		pn(NULL);
		pn(semgetname);
		break;
	case SYS_shmat:
		pn(NULL);
		pn(NULL);
		pn(shmatname);
		break;
	case SYS_shmctl:
		pn(NULL);
		pn(shmctlname);
		break;
	case SYS_clock_gettime:
	case SYS_clock_settime:
	case SYS_clock_getres:
		pn(clockname);
		break;
	case SYS_poll:
		pn(NULL);
		pn(NULL);
		pn(polltimeout);
		break;
	case SYS_sigaction:
		pn(signame);
		break;
	case SYS_sigprocmask:
		pn(sigprocmaskhowname);
		pn(sigset);
		break;
	case SYS_sigsuspend:
		pn(sigset);
		break;
	case SYS_socket: {
		int sockdomain = *ap;

		pn(sockdomainname);
		pn(socktypename);
		if (sockdomain == PF_INET || sockdomain == PF_INET6)
			pn(sockipprotoname);
		break;
	}
	case SYS_socketpair:
		pn(sockdomainname);
		pn(socktypename);
		break;
	case SYS_truncate:
	case SYS_ftruncate:
		pn(NULL);
		/* skip padding */
		ap++;
		narg--;
		plln();
		break;
	case SYS_wait4:
		pn(wait4pid);
		pn(NULL);
		pn(wait4optname);
		break;
	case SYS_getrusage:
		pn(rusagewho);
		break;
	case SYS___thrsleep:
		pn(NULL);
		pn(clockname);
		break;
	case SYS___thrsigdivert:
		pn(sigset);
		break;
	case SYS_faccessat:
		pn(atfd);
		pn(NULL);
		pn(accessmodename);
		pn(atflagsname);
		break;
	case SYS_fchmodat:
		pn(atfd);
		pn(NULL);
		pn(modename);
		pn(atflagsname);
		break;
	case SYS_fchownat:
		pn(atfd);
		pn(NULL);
		pn(NULL);
		pn(NULL);
		pn(atflagsname);
		break;
	case SYS_fstatat:
		pn(atfd);
		pn(NULL);
		pn(NULL);
		pn(atflagsname);
		break;
	case SYS_linkat:
		pn(atfd);
		pn(NULL);
		pn(atfd);
		pn(NULL);
		pn(atflagsname);
		break;
	case SYS_mkdirat:
	case SYS_mkfifoat:
	case SYS_mknodat:
		pn(atfd);
		pn(NULL);
		pn(modename);
		break;
	case SYS_openat: {
		int     flags;
		int     mode;

		pn(atfd);
		pn(NULL);
		if (!fancy)
			break;
		flags = ap[0];
		mode = ap[1];
		(void)putchar(',');
		flagsandmodename(flags, mode);
		ap += 2;
		narg -= 2;
		break;
	}
	case SYS_readlinkat:
		pn(atfd);
		break;
	case SYS_renameat:
		pn(atfd);
		pn(NULL);
		pn(atfd);
		break;
	case SYS_symlinkat:
		pn(NULL);
		pn(atfd);
		break;
	case SYS_unlinkat:
		pn(atfd);
		pn(NULL);
		pn(atflagsname);
		break;
	case SYS_utimensat:
		pn(atfd);
		pn(NULL);
		pn(NULL);
		pn(atflagsname);
		break;
	case SYS_pathconf:
	case SYS_fpathconf: 
		pn(NULL);
		pn(pathconfname);
		break;
	case SYS_ktrace:
		pn(NULL);
		pn(ktraceopname);
		pn(ktracefacname);
		pn(pgid);
		break;
	case SYS_setitimer:
	case SYS_getitimer:
		pn(itimername);
		break;
	case SYS_quotactl:
		pn(NULL);
		pn(quotactlcmdname);
		break;
	}

nonnative:
	while (narg) {
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
kresolvsysctl(int depth, int *top, int idx)
{
	struct ctlname *names;
	size_t		limit;

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
	size_t datalen = len - sizeof(struct ktr_genio);

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
	(void)printf("SIG%s ", sys_signame[psig->signo]);
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
	    "[-dHlnRrTXx] [-e emulation] [-f file] [-m maxdata] [-p pid]\n"
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

static void
atfd(int fd)
{
	if (fd == AT_FDCWD)
		(void)printf("AT_FDCWD");
	else if (decimal)
		(void)printf("%d", fd);
	else
		(void)printf("%#x", fd);
}

static void
polltimeout(int timeout)
{
	if (timeout == INFTIM)
		(void)printf("INFTIM");
	else if (decimal)
		(void)printf("%d", timeout);
	else
		(void)printf("%#x", timeout);
}

static void
pgid(int pid)
{
	(void)printf("%d", pid);
}

static void
wait4pid(int pid)
{
	if (pid == WAIT_ANY)
		(void)printf("WAIT_ANY");
	else if (pid == WAIT_MYPGRP)
		(void)printf("WAIT_MYPGRP");
	else
		pgid(pid);
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
		(void)printf("<invalid=%ld>", (long)cmd);
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
		(void)printf("<invalid=%ld>", (long)cmd);
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
}


/*
 * Only used by SYS_open. Unless O_CREAT is set in flags, the
 * mode argument is unused (and often bogus and misleading).
 */
static void
flagsandmodename(int flags, int mode)
{
	doflagsname(flags, 1);
	if ((flags & O_CREAT) == O_CREAT) {
		(void)putchar(',');
		modename (mode);
	} else if (!fancy) {
		(void)putchar(',');
		if (decimal) {
			(void)printf("<unused>%ld", (long)mode);
		} else {
			(void)printf("<unused>%#lx", (long)mode);
		}
	}
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
	if (level == SOL_SOCKET) {
		(void)printf("SOL_SOCKET");
	} else {
		if (decimal) {
			(void)printf("%ld", (long)level);
		} else {
			(void)printf("%#lx", (long)level);
		}
	}
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
	if (ops & KTRFLAG_DESCEND) printf ("|%s", "KTRFLAG_DESCEND");
	printf(">");
	if (invalid || (ops & ~(KTROP((unsigned)-1) | KTRFLAG_DESCEND)))
		(void)printf("<invalid>%ld", (long)ops);
}
