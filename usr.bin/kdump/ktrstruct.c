/*	$OpenBSD: ktrstruct.c,v 1.6 2014/10/13 03:46:33 guenther Exp $	*/

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

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <ufs/ufs/quota.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

#include "kdump.h"
#include "kdump_subr.h"

#define TIME_FORMAT	"%b %e %T %Y"

static void
ktrsockaddr(struct sockaddr *sa)
{
/*
 TODO: Support additional address families
	#include <netmpls/mpls.h>
	struct sockaddr_mpls	*mpls;
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

static void
print_time(time_t t, int relative)
{
	char timestr[PATH_MAX + 4];
	struct tm *tm;

	if (resolv == 0 || relative)
		printf("%jd", (intmax_t)t);
	else {
		tm = localtime(&t);
		(void)strftime(timestr, sizeof(timestr), TIME_FORMAT, tm);
		printf("\"%s\"", timestr);
	}
}

static void
print_timespec(const struct timespec *tsp, int relative)
{
	if (tsp->tv_nsec == UTIME_NOW)
		printf("UTIME_NOW");
	else if (tsp->tv_nsec == UTIME_OMIT)
		printf("UTIME_OMIT");
	else if ((resolv == 0 || relative) && tsp->tv_sec < 0 &&
	    tsp->tv_nsec > 0) {
		/* negative relative times with non-zero nsecs require care */
		printf("-%jd.%09ld", -(intmax_t)(tsp->tv_sec+1),
		    1000000000 - tsp->tv_nsec);
	} else {
		print_time(tsp->tv_sec, relative);
		if (tsp->tv_nsec != 0)
			printf(".%09ld", tsp->tv_nsec);
	}
}

static void
ktrstat(const struct stat *statp)
{
	char mode[12];
	struct passwd *pwd;
	struct group  *grp;

	/*
	 * note: ktrstruct() has already verified that statp points to a
	 * buffer exactly sizeof(struct stat) bytes long.
	 */
	printf("struct stat { ");
	strmode(statp->st_mode, mode);
	printf("dev=%d, ino=%llu, mode=%s, nlink=%u, ",
	    statp->st_dev, (unsigned long long)statp->st_ino,
	    mode, statp->st_nlink);
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
	print_timespec(&statp->st_atim, 0);
	printf(", mtime=");
	print_timespec(&statp->st_mtim, 0);
	printf(", ctime=");
	print_timespec(&statp->st_ctim, 0);
	printf(", size=%lld, blocks=%lld, blksize=%u, flags=0x%x, gen=0x%x",
	    statp->st_size, statp->st_blocks, statp->st_blksize,
	    statp->st_flags, statp->st_gen);
	printf(" }\n");
}

static void
ktrtimespec(const struct timespec *tsp, int relative)
{
	printf("struct timespec { ");
	print_timespec(tsp, relative);
	printf(" }\n");
}

static void
print_timeval(const struct timeval *tvp, int relative)
{
	if ((resolv == 0 || relative) && tvp->tv_sec < 0 &&
	    tvp->tv_usec > 0) {
		/* negative relative times with non-zero usecs require care */
		printf("-%jd.%06ld", -(intmax_t)(tvp->tv_sec+1),
		    1000000 - tvp->tv_usec);
	} else {
		print_time(tvp->tv_sec, relative);
		if (tvp->tv_usec != 0)
			printf(".%06ld", tvp->tv_usec);
	}
}

static void
ktrtimeval(const struct timeval *tvp, int relative)
{
	printf("struct timeval { ");
	print_timeval(tvp, relative);
	printf(" }\n");
}

static void
ktrsigaction(const struct sigaction *sa)
{
	/*
	 * note: ktrstruct() has already verified that sa points to a
	 * buffer exactly sizeof(struct sigaction) bytes long.
	 */
	printf("struct sigaction { ");
	if (sa->sa_handler == SIG_DFL)
		printf("handler=SIG_DFL");
	else if (sa->sa_handler == SIG_IGN)
		printf("handler=SIG_IGN");
	else if (sa->sa_flags & SA_SIGINFO)
		printf("sigaction=%p", (void *)sa->sa_sigaction);
	else
		printf("handler=%p", (void *)sa->sa_handler);
	printf(", mask=");
	sigset(sa->sa_mask);
	printf(", flags=");
	sigactionflagname(sa->sa_flags);
	printf(" }\n");
}

static void
print_rlim(rlim_t lim)
{
	if (lim == RLIM_INFINITY)
		printf("infinite");
	else
		printf("%llu", (unsigned long long)lim);
}

static void
ktrrlimit(const struct rlimit *limp)
{
	printf("struct rlimit { ");
	printf("cur=");
	print_rlim(limp->rlim_cur);
	printf(", max=");
	print_rlim(limp->rlim_max);
	printf(" }\n");
}

static void
ktrtfork(const struct __tfork *tf)
{
	printf("struct __tfork { tcb=%p, tid=%p, stack=%p }\n",
	    tf->tf_tcb, (void *)tf->tf_tid, tf->tf_stack);
}

static void
ktrfdset(const struct fd_set *fds, int len)
{
	int nfds, i, start = -1;
	char sep = ' ';

	nfds = len * NBBY;
	printf("struct fd_set {");
	for (i = 0; i <= nfds; i++)
		if (i != nfds && FD_ISSET(i, fds)) {
			if (start == -1)
				start = i;
		} else if (start != -1) {
			putchar(sep);
			if (start == i - 1)
				printf("%d", start);
			else if (start == i - 2)
				printf("%d,%d", start, i - 1);
			else
				printf("%d-%d", start, i - 1);
			sep = ',';
			start = -1;
		}

	printf(" }\n");
}

static void
ktrrusage(const struct rusage *rup)
{
	printf("struct rusage { utime=");
	print_timeval(&rup->ru_utime, 1);
	printf(", stime=");
	print_timeval(&rup->ru_stime, 1);
	printf(", maxrss=%ld, ixrss=%ld, idrss=%ld, isrss=%ld,"
	    " minflt=%ld, majflt=%ld, nswap=%ld, inblock=%ld,"
	    " oublock=%ld, msgsnd=%ld, msgrcv=%ld, nsignals=%ld,"
	    " nvcsw=%ld, nivcsw=%ld }\n",
	    rup->ru_maxrss, rup->ru_ixrss, rup->ru_idrss, rup->ru_isrss,
	    rup->ru_minflt, rup->ru_majflt, rup->ru_nswap, rup->ru_inblock,
	    rup->ru_oublock, rup->ru_msgsnd, rup->ru_msgrcv, rup->ru_nsignals,
	    rup->ru_nvcsw, rup->ru_nivcsw);
}

static void
ktrquota(const struct dqblk *quota)
{
	printf("struct dqblk { bhardlimit=%u, bsoftlimit=%u, curblocks=%u,"
	    " ihardlimit=%u, isoftlimit=%u, curinodes=%u, btime=",
	    quota->dqb_bhardlimit, quota->dqb_bsoftlimit,
	    quota->dqb_curblocks, quota->dqb_ihardlimit,
	    quota->dqb_isoftlimit, quota->dqb_curinodes);
	print_time(quota->dqb_btime, 0);
	printf(", itime=");
	print_time(quota->dqb_itime, 0);
	printf(" }\n");
}

void
ktrstruct(char *buf, size_t buflen)
{
	char *name, *data;
	size_t namelen, datalen;
	int i;

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
		struct stat sb;

		if (datalen != sizeof(struct stat))
			goto invalid;
		memcpy(&sb, data, datalen);
		ktrstat(&sb);
	} else if (strcmp(name, "sockaddr") == 0) {
		struct sockaddr_storage ss;

		if (datalen > sizeof(ss))
			goto invalid;
		memcpy(&ss, data, datalen);
		if ((ss.ss_family != AF_UNIX && 
		    datalen < sizeof(struct sockaddr)) || datalen != ss.ss_len)
			goto invalid;
		ktrsockaddr((struct sockaddr *)&ss);
	} else if (strcmp(name, "abstimespec") == 0 ||
	    strcmp(name, "reltimespec") == 0) {
		struct timespec ts;

		if (datalen != sizeof(ts))
			goto invalid;
		memcpy(&ts, data, datalen);
		ktrtimespec(&ts, name[0] == 'r');
	} else if (strcmp(name, "abstimeval") == 0 ||
	    strcmp(name, "reltimeval") == 0) {
		struct timeval tv;

		if (datalen != sizeof(tv))
			goto invalid;
		memcpy(&tv, data, datalen);
		ktrtimeval(&tv, name[0] == 'r');
	} else if (strcmp(name, "sigaction") == 0) {
		struct sigaction sa;

		if (datalen != sizeof(sa))
			goto invalid;
		memcpy(&sa, data, datalen);
		ktrsigaction(&sa);
	} else if (strcmp(name, "rlimit") == 0) {
		struct rlimit lim;

		if (datalen != sizeof(lim))
			goto invalid;
		memcpy(&lim, data, datalen);
		ktrrlimit(&lim);
	} else if (strcmp(name, "rusage") == 0) {
		struct rusage ru;

		if (datalen != sizeof(ru))
			goto invalid;
		memcpy(&ru, data, datalen);
		ktrrusage(&ru);
	} else if (strcmp(name, "tfork") == 0) {
		struct __tfork tf;

		if (datalen != sizeof(tf))
			goto invalid;
		memcpy(&tf, data, datalen);
		ktrtfork(&tf);
	} else if (strcmp(name, "fdset") == 0) {
		struct fd_set *fds;
		if ((fds = malloc(datalen)) == NULL)
			err(1, "malloc");
		memcpy(fds, data, datalen);
		ktrfdset(fds, datalen);
		free(fds);
	} else if (strcmp(name, "quota") == 0) {
		struct dqblk quota;

		if (datalen != sizeof(quota))
			goto invalid;
		memcpy(&quota, data, datalen);
		ktrquota(&quota);
	} else {
		printf("unknown structure %s\n", name);
	}
	return;
invalid:
	printf("invalid record\n");
}
