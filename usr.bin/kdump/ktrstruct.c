/*	$OpenBSD: ktrstruct.c,v 1.24 2017/12/12 01:12:34 deraadt Exp $	*/

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
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/event.h>
#include <sys/un.h>
#include <ufs/ufs/quota.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <vis.h>

#include "kdump.h"
#include "kdump_subr.h"

#define TIME_FORMAT	"%b %e %T %Y"

static void
ktrsockaddr(struct sockaddr *sa)
{
	/*
	 * TODO: Support additional address families
	 *	#include <netmpls/mpls.h>
	 *	struct sockaddr_mpls	*mpls;
	 */

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
		char addr[64];

		sa_in = (struct sockaddr_in *)sa;
		check_sockaddr_len(in);
		inet_ntop(AF_INET, &sa_in->sin_addr, addr, sizeof addr);
		printf("%s:%u", addr, ntohs(sa_in->sin_port));
		break;
	}
	case AF_INET6: {
		struct sockaddr_in6	*sa_in6;
		char addr[64];

		sa_in6 = (struct sockaddr_in6 *)sa;
		check_sockaddr_len(in6);
		inet_ntop(AF_INET6, &sa_in6->sin6_addr, addr, sizeof addr);
		printf("[%s]:%u", addr, htons(sa_in6->sin6_port));
		break;
	}
	case AF_UNIX: {
		struct sockaddr_un *sa_un;
		char path[4 * sizeof(sa_un->sun_path) + 1];
		size_t len;

		sa_un = (struct sockaddr_un *)sa;
		len = sa_un->sun_len;
		if (len <= offsetof(struct sockaddr_un, sun_path)) {
			printf("invalid");
			break;
		}
		len -= offsetof(struct sockaddr_un, sun_path);
		if (len > sizeof(sa_un->sun_path)) {
			printf("too long");
			break;
		}
		/* format, stopping at first NUL */
		len = strnlen(sa_un->sun_path, len);
		strvisx(path, sa_un->sun_path, len,
		    VIS_CSTYLE | VIS_DQ | VIS_TAB | VIS_NL);
		printf("\"%s\"", path);
		break;
	}
	default:
		printf("unknown address family");
	}
	printf(" }\n");
}

static void
print_time(time_t t, int relative, int have_subsec)
{
	char timestr[PATH_MAX + 4];
	struct tm *tm;

	if (t < 0 && have_subsec) {
		/* negative times with non-zero subsecs require care */
		printf("-%jd", -(intmax_t)(t + 1));
	} else
		printf("%jd", (intmax_t)t);

	/* 1970s times are probably relative */
	if (!relative && t > (10 * 365 * 24 * 3600)) {
		tm = localtime(&t);
		if (tm != NULL) {
			(void)strftime(timestr, sizeof(timestr), TIME_FORMAT,
			    tm);
			printf("<\"%s\">", timestr);
		}
	}
}

static void
print_timespec(const struct timespec *tsp, int relative)
{
	if (tsp->tv_nsec == UTIME_NOW)
		printf("UTIME_NOW");
	else if (tsp->tv_nsec == UTIME_OMIT)
		printf("UTIME_OMIT");
	else {
		print_time(tsp->tv_sec, relative, tsp->tv_nsec);
		if (tsp->tv_nsec != 0)
			printf(".%09ld", tsp->tv_sec >= 0 ? tsp->tv_nsec :
			    1000000000 - tsp->tv_nsec);
	}
}

void
uidname(int uid)
{
	const char *name;

	if (uid == -1)
		printf("-1");
	else {
		printf("%u<", (unsigned)uid);
		if (uid > UID_MAX || (name = user_from_uid(uid, 1)) == NULL)
			printf("unknown>");
		else
			printf("\"%s\">", name);
	}
}

void
gidname(int gid)
{
	const char *name;

	if (gid == -1)
		printf("-1");
	else {
		printf("%u<", (unsigned)gid);
		if (gid > GID_MAX || (name = group_from_gid(gid, 1)) == NULL)
			printf("unknown>");
		else
			printf("\"%s\">", name);
	}
}

static void
ktrstat(const struct stat *statp)
{
	char mode[12];

	/*
	 * note: ktrstruct() has already verified that statp points to a
	 * buffer exactly sizeof(struct stat) bytes long.
	 */
	printf("struct stat { ");
	strmode(statp->st_mode, mode);
	printf("dev=%d, ino=%llu, mode=%s, nlink=%u, uid=",
	    statp->st_dev, (unsigned long long)statp->st_ino,
	    mode, statp->st_nlink);
	uidname(statp->st_uid);
	printf(", gid=");
	gidname(statp->st_gid);
	printf(", rdev=%d, ", statp->st_rdev);
	printf("atime=");
	print_timespec(&statp->st_atim, 0);
	printf(", mtime=");
	print_timespec(&statp->st_mtim, 0);
	printf(", ctime=");
	print_timespec(&statp->st_ctim, 0);
	printf(", size=%lld, blocks=%lld, blksize=%d, flags=0x%x, gen=0x%x",
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
	print_time(tvp->tv_sec, relative, tvp->tv_usec);
	if (tvp->tv_usec != 0)
		printf(".%06ld", tvp->tv_sec >= 0 ? tvp->tv_usec :
		    1000000 - tvp->tv_usec);
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
	/*
	 * Fuck!  Comparison of function pointers on hppa assumes you can
	 * dereference them if they're plabels!  Cast everything to void *
	 * to suppress that extra logic; sorry folks, the address we report
	 * here might not match what you see in your executable...
	 */
	printf("struct sigaction { ");
	if ((void *)sa->sa_handler == (void *)SIG_DFL)
		printf("handler=SIG_DFL");
	else if ((void *)sa->sa_handler == (void *)SIG_IGN)
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
ktrfds(const char *data, size_t count)
{
	size_t i;
	int fd;

	for (i = 0; i < count - 1; i++) {
		memcpy(&fd, &data[i * sizeof(fd)], sizeof(fd));
		printf("fd[%zu] = %d, ", i, fd);
	}
	memcpy(&fd, &data[i * sizeof(fd)], sizeof(fd));
	printf("fd[%zu] = %d\n", i, fd);
}

static void
ktrfdset(struct fd_set *fds, int len)
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
	print_time(quota->dqb_btime, 0, 0);
	printf(", itime=");
	print_time(quota->dqb_itime, 0, 0);
	printf(" }\n");
}

static void
ktrmsghdr(const struct msghdr *msg)
{
	printf("struct msghdr { name=%p, namelen=%u, iov=%p, iovlen=%u,"
	    " control=%p, controllen=%u, flags=",
	    msg->msg_name, msg->msg_namelen, msg->msg_iov, msg->msg_iovlen,
	    msg->msg_control, msg->msg_controllen);
	sendrecvflagsname(msg->msg_flags);
	printf(" }\n");
}

static void
ktriovec(const char *data, int count)
{
	struct iovec iov;
	int i;

	printf("struct iovec");
	if (count > 1)
		printf(" [%d]", count);
	for (i = 0; i < count; i++) {
		memcpy(&iov, data, sizeof(iov));
		data += sizeof(iov);
		printf(" { base=%p, len=%lu }", iov.iov_base, iov.iov_len);
	}
	printf("\n");
}

static void
ktrevent(const char *data, int count)
{
	struct kevent kev;
	int i;

	printf("struct kevent");
	if (count > 1)
		printf(" [%d]", count);
	for (i = 0; i < count; i++) {
		memcpy(&kev, data, sizeof(kev));
		data += sizeof(kev);
		printf(" { ident=%lu, filter=", kev.ident);
		evfiltername(kev.filter);
		printf(", flags=");
		evflagsname(kev.flags);
		printf(", fflags=");
		evfflagsname(kev.filter, kev.fflags);
		printf(", data=%llu", kev.data);
		if ((kev.flags & EV_ERROR) && fancy) {
			printf("<\"%s\">", strerror(kev.data));
		}
		printf(", udata=%p }", kev.udata);
	}
	printf("\n");
}

static void
ktrpollfd(const char *data, int count)
{
	struct pollfd pfd;
	int i;

	printf("struct pollfd");
	if (count > 1)
		printf(" [%d]", count);
	for (i = 0; i < count; i++) {
		memcpy(&pfd, data, sizeof(pfd));
		data += sizeof(pfd);
		printf(" { fd=%d, events=", pfd.fd);
		pollfdeventname(pfd.events);
		printf(", revents=");
		pollfdeventname(pfd.revents);
		printf(" }");
	}
	printf("\n");
}

static void
ktrcmsghdr(char *data, socklen_t len)
{
	struct msghdr msg;
	struct cmsghdr *cmsg;
	int i, count, *fds;

	msg.msg_control = data;
	msg.msg_controllen = len;

	/* count the control messages */
	count = 0;
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		count++;
	}

	printf("struct cmsghdr");
	if (count > 1)
		printf(" [%d]", count);

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		printf(" { len=%u, level=", cmsg->cmsg_len);
		if (cmsg->cmsg_level == SOL_SOCKET) {
			printf("SOL_SOCKET, type=");
			switch (cmsg->cmsg_type) {
			case SCM_RIGHTS:
				printf("SCM_RIGHTS, data=");
				fds = (int *)CMSG_DATA(cmsg);
				for (i = 0;
				    cmsg->cmsg_len > CMSG_LEN(sizeof(int) * i);
				    i++) {
					printf("%s%d", i ? "," : "", fds[i]);
				}
				break;
			case SCM_TIMESTAMP:
			default:
				printf("%d", cmsg->cmsg_type);
				break;
			}
		} else {
			struct protoent *p = getprotobynumber(cmsg->cmsg_level);

			printf("%u<%s>, type=%d", cmsg->cmsg_level,
			    p != NULL ? p->p_name : "unknown", cmsg->cmsg_type);
		}
		printf(" }");
	}
	printf("\n");
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
	} else if (strcmp(name, "fds") == 0) {
		if (datalen % sizeof(int))
			goto invalid;
		ktrfds(data, datalen / sizeof(int));
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
	} else if (strcmp(name, "msghdr") == 0) {
		struct msghdr msg;

		if (datalen != sizeof(msg))
			goto invalid;
		memcpy(&msg, data, datalen);
		ktrmsghdr(&msg);
	} else if (strcmp(name, "iovec") == 0) {
		if (datalen % sizeof(struct iovec))
			goto invalid;
		ktriovec(data, datalen / sizeof(struct iovec));
	} else if (strcmp(name, "kevent") == 0) {
		if (datalen % sizeof(struct kevent))
			goto invalid;
		ktrevent(data, datalen / sizeof(struct kevent));
	} else if (strcmp(name, "pollfd") == 0) {
		if (datalen % sizeof(struct pollfd))
			goto invalid;
		ktrpollfd(data, datalen / sizeof(struct pollfd));
	} else if (strcmp(name, "cmsghdr") == 0) {
		char *cmsg;

		if ((cmsg = malloc(datalen)) == NULL)
			err(1, "malloc");
		memcpy(cmsg, data, datalen);
		ktrcmsghdr(cmsg, datalen);
		free(cmsg);
	} else if (strcmp(name, "pledgereq") == 0) {
		printf("promise=");
		showbufc(basecol + sizeof("promise=") - 1,
		    (unsigned char *)data, datalen, VIS_DQ | VIS_TAB | VIS_NL);
	} else if (strcmp(name, "pledgeexecreq") == 0) {
		printf("execpromise=");
		showbufc(basecol + sizeof("execpromise=") - 1,
		    (unsigned char *)data, datalen, VIS_DQ | VIS_TAB | VIS_NL);
	} else {
		printf("unknown structure %s\n", name);
	}
	return;
invalid:
	printf("invalid record\n");
}
