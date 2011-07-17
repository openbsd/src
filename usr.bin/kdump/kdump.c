/*	$OpenBSD: kdump.c,v 1.60 2011/07/17 07:49:34 otto Exp $	*/

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
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define _KERNEL
#include <sys/errno.h>
#undef _KERNEL

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <vis.h>

#include "ktrace.h"
#include "kdump.h"
#include "kdump_subr.h"
#include "extern.h"

int timestamp, decimal, iohex, fancy = 1, tail, maxdata = INT_MAX, resolv;
char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;
pid_t pid = -1;

#define TIME_FORMAT	"%b %e %T %Y"
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
static void ktrsysret(struct ktr_sysret *);
static void ktrstruct(char *, size_t);
static void setemul(const char *);
static void usage(void);

int
main(int argc, char *argv[])
{
	int ch, silent;
	size_t ktrlen, size;
	int trpoints = ALL_POINTS;
	void *m;

	def_emul = current = &emulations[0];	/* native */

	while ((ch = getopt(argc, argv, "e:f:dlm:nrRp:Tt:xX")) != -1)
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
			pid = atoi(optarg);
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
	while (fread_tail(&ktr_header, sizeof(struct ktr_header), 1)) {
		silent = 0;
		if (pe_size == 0)
			mappidtoemul(ktr_header.ktr_pid, current);
		if (pid != -1 && pid != ktr_header.ktr_pid)
			silent = 1;
		if (silent == 0 && trpoints & (1<<ktr_header.ktr_type))
			dumpheader(&ktr_header);
		ktrlen = ktr_header.ktr_len;
		if (ktrlen > size) {
			void *newm;

			newm = realloc(m, ktrlen+1);
			if (newm == NULL)
				err(1, NULL);
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
	tmp = realloc(pe_table, (pe_size + 1) * sizeof(*pe_table));
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
	static struct timeval prevtime;
	char unknown[64], *type;
	struct timeval temp;

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
	default:
		(void)snprintf(unknown, sizeof unknown, "UNKNOWN(%d)",
		    kth->ktr_type);
		type = unknown;
	}

	(void)printf("%6ld %-8.*s ", (long)kth->ktr_pid, MAXCOMLEN,
	    kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			timersub(&kth->ktr_time, &prevtime, &temp);
			prevtime = kth->ktr_time;
		} else
			temp = kth->ktr_time;
		(void)printf("%ld.%06ld ", temp.tv_sec, temp.tv_usec);
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
#ifdef PT_SETFPREGS
	case PT_SETFPREGS:
		(void)printf("PT_SETFPREGS");
		break;
#endif
	case PT_SETREGS:
		(void)printf("PT_SETREGS");
		break;
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
		int *np, n;

		if (!fancy)
			break;
		n = ap[1];
		if (n > CTL_MAXNAME)
			n = CTL_MAXNAME;
		np = (int *)(ap + 6);
		for (; n--; np++) {
			if (sep)
				putchar(sep);
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
	case SYS_access:
		pn(NULL);
		pn(accessmodename);
		break;
	case SYS_chmod:
	case SYS_fchmod: 
		pn( NULL);
		pn(modename);
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
		pn(NULL);
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
	case SYS_sigaction:
		pn(signame);
		break;
	case SYS_sigprocmask:
		pn(sigprocmaskhowname);
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
		pn(NULL);
		pn(NULL);
		pn(wait4optname);
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
		    strcmp(current->sysnames[code], "rfork") == 0 ||
		    strcmp(current->sysnames[code], "clone") == 0))
			mappidtoemul(ret, current);
	}

	if (error == 0) {
		if (fancy) {
			(void)printf("%ld", (long)ret);
			if (ret < 0 || ret > 9)
				(void)printf("/%#lx", (long)ret);
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
ktrgenio(struct ktr_genio *ktr, size_t len)
{
	unsigned char *dp = (unsigned char *)ktr + sizeof(struct ktr_genio);
	int i, j;
	size_t datalen = len - sizeof(struct ktr_genio);
	static int screenwidth = 0;
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
	printf("fd %d %s %zu bytes\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen);
	if (maxdata == 0)
		return;
	if (datalen > maxdata)
		datalen = maxdata;
	if (iohex && !datalen)
		return;
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
ktrpsig(struct ktr_psig *psig)
{
	(void)printf("SIG%s ", sys_signame[psig->signo]);
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL code %d", psig->code);
	else
		(void)printf("caught handler=0x%lx mask=0x%x",
		    (u_long)psig->action, psig->mask);
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



void
ktrsockaddr(struct sockaddr *sa)
{
/*
 TODO: Support additional address families
	#include <netnatm/natm.h>
	struct sockaddr_natm	*natm;
	#include <netsmb/netbios.h>
	struct sockaddr_nb	*nb;
*/
	char addr[64];

	/*
	 * note: ktrstruct() has already verified that sa points to a
	 * buffer at least sizeof(struct sockaddr) bytes long and exactly
	 * sa->sa_len bytes long.
	 */
	printf("struct sockaddr { ");
	sockfamilyname(sa->sa_family);
	printf(", ");

#define check_sockaddr_len(n)					\
	if (sa_##n->s##n##_len < sizeof(struct sockaddr_##n)) {	\
		printf("invalid");				\
		break;						\
	}

	switch(sa->sa_family) {
	case AF_INET: {
		struct sockaddr_in	*sa_in;

		sa_in = (struct sockaddr_in *)sa;
		check_sockaddr_len(in);
		inet_ntop(AF_INET, &sa_in->sin_addr, addr, sizeof addr);
		printf("%s:%u", addr, ntohs(sa_in->sin_port));
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6	*sa_in6;

		sa_in6 = (struct sockaddr_in6 *)sa;
		check_sockaddr_len(in6);
		inet_ntop(AF_INET6, &sa_in6->sin6_addr, addr, sizeof addr);
		printf("[%s]:%u", addr, htons(sa_in6->sin6_port));
		break;
	}
#ifdef IPX
	case AF_IPX: {
		struct sockaddr_ipx	*sa_ipx;

		sa_ipx = (struct sockaddr_ipx *)sa;
		check_sockaddr_len(ipx);
		/* XXX wish we had ipx_ntop */
		printf("%s", ipx_ntoa(sa_ipx->sipx_addr));
		break;
	}
#endif
	case AF_UNIX: {
		struct sockaddr_un *sa_un;

		sa_un = (struct sockaddr_un *)sa;
		if (sa_un->sun_len <= sizeof(sa_un->sun_len) +
		    sizeof(sa_un->sun_family)) {
			printf("invalid");
			break;
		}
		printf("\"%.*s\"", (int)(sa_un->sun_len -
		    sizeof(sa_un->sun_len) - sizeof(sa_un->sun_family)),
		    sa_un->sun_path);
		break;
	}
	default:
		printf("unknown address family");
	}
	printf(" }\n");
}

void
ktrstat(struct stat *statp)
{
	char mode[12], timestr[PATH_MAX + 4];
	struct passwd *pwd;
	struct group  *grp;
	struct tm *tm;

	/*
	 * note: ktrstruct() has already verified that statp points to a
	 * buffer exactly sizeof(struct stat) bytes long.
	 */
	printf("struct stat {");
	strmode(statp->st_mode, mode);
	printf("dev=%d, ino=%u, mode=%s, nlink=%u, ",
	    statp->st_dev, statp->st_ino, mode, statp->st_nlink);
	if (resolv == 0 || (pwd = getpwuid(statp->st_uid)) == NULL)
		printf("uid=%u, ", statp->st_uid);
	else
		printf("uid=\"%s\", ", pwd->pw_name);
	if (resolv == 0 || (grp = getgrgid(statp->st_gid)) == NULL)
		printf("gid=%u, ", statp->st_gid);
	else
		printf("gid=\"%s\", ", grp->gr_name);
	printf("rdev=%d, ", statp->st_rdev);
	printf("atime=");
	if (resolv == 0)
		printf("%jd", (intmax_t)statp->st_atim.tv_sec);
	else {
		tm = localtime(&statp->st_atim.tv_sec);
		(void)strftime(timestr, sizeof(timestr), TIME_FORMAT, tm);
		printf("\"%s\"", timestr);
	}
	if (statp->st_atim.tv_nsec != 0)
		printf(".%09ld, ", statp->st_atim.tv_nsec);
	else
		printf(", ");
	printf("stime=");
	if (resolv == 0)
		printf("%jd", (intmax_t)statp->st_mtim.tv_sec);
	else {
		tm = localtime(&statp->st_mtim.tv_sec);
		(void)strftime(timestr, sizeof(timestr), TIME_FORMAT, tm);
		printf("\"%s\"", timestr);
	}
	if (statp->st_mtim.tv_nsec != 0)
		printf(".%09ld, ", statp->st_mtim.tv_nsec);
	else
		printf(", ");
	printf("ctime=");
	if (resolv == 0)
		printf("%jd", (intmax_t)statp->st_ctim.tv_sec);
	else {
		tm = localtime(&statp->st_ctim.tv_sec);
		(void)strftime(timestr, sizeof(timestr), TIME_FORMAT, tm);
		printf("\"%s\"", timestr);
	}
	if (statp->st_ctim.tv_nsec != 0)
		printf(".%09ld, ", statp->st_ctim.tv_nsec);
	else
		printf(", ");
	printf("size=%lld, blocks=%lld, blksize=%u, flags=0x%x, gen=0x%x",
	    statp->st_size, statp->st_blocks, statp->st_blksize,
	    statp->st_flags, statp->st_gen);
	printf(" }\n");
}

void
ktrstruct(char *buf, size_t buflen)
{
	char *name, *data;
	size_t namelen, datalen;
	int i;
	struct stat sb;
	struct sockaddr_storage ss;

	for (name = buf, namelen = 0; namelen < buflen && name[namelen] != '\0';
	     ++namelen)
		/* nothing */;
	if (namelen == buflen)
		goto invalid;
	if (name[namelen] != '\0')
		goto invalid;
	data = buf + namelen + 1;
	datalen = buflen - namelen - 1;
	if (datalen == 0)
		goto invalid;
	/* sanity check */
	for (i = 0; i < namelen; ++i)
		if (!isalpha((unsigned char)name[i]))
			goto invalid;
	if (strcmp(name, "stat") == 0) {
		if (datalen != sizeof(struct stat))
			goto invalid;
		memcpy(&sb, data, datalen);
		ktrstat(&sb);
	} else if (strcmp(name, "sockaddr") == 0) {
		if (datalen > sizeof(ss))
			goto invalid;
		memcpy(&ss, data, datalen);
		if ((ss.ss_family != AF_UNIX && 
		    datalen < sizeof(struct sockaddr)) || datalen != ss.ss_len)
			goto invalid;
		ktrsockaddr((struct sockaddr *)&ss);
	} else {
		printf("unknown structure %s\n", name);
	}
	return;
invalid:
	printf("invalid record\n");
}

static void
usage(void)
{

	extern char *__progname;
	fprintf(stderr, "usage: %s "
	    "[-dlnRrTXx] [-e emulation] [-f file] [-m maxdata] [-p pid]\n"
	    "%*s[-t [ceinsw]]\n",
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
