/*	$OpenBSD: util.c,v 1.32 2009/12/23 17:16:03 jacekm Exp $	*/

/*
 * Copyright (c) 2000,2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

int
bsnprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);
	if (ret == -1 || ret >= (int)size)
		return 0;

	return 1;
}

/* Close file, signifying temporary error condition (if any) to the caller. */
int
safe_fclose(FILE *fp)
{
	if (ferror(fp)) {
		fclose(fp);
		return 0;
	}
	if (fflush(fp)) {
		fclose(fp);
		if (errno == ENOSPC)
			return 0;
		fatal("safe_fclose: fflush");
	}
	if (fsync(fileno(fp)))
		fatal("safe_fclose: fsync");
	if (fclose(fp))
		fatal("safe_fclose: fclose");

	return 1;
}

int
hostname_match(char *hostname, char *pattern)
{
	while (*pattern != '\0' && *hostname != '\0') {
		if (*pattern == '*') {
			while (*pattern == '*')
				pattern++;
			while (*hostname != '\0' &&
			    tolower((int)*hostname) != tolower((int)*pattern))
				hostname++;
			continue;
		}

		if (tolower((int)*pattern) != tolower((int)*hostname))
			return 0;
		pattern++;
		hostname++;
	}

	return (*hostname == '\0' && *pattern == '\0');
}

int
recipient_to_path(struct path *path, char *recipient)
{
	char *username;
	char *hostname;

	username = recipient;
	hostname = strrchr(username, '@');

	if (username[0] == '\0') {
		*path->user = '\0';
		*path->domain = '\0';
		return 1;
	}

	if (hostname == NULL) {
		if (strcasecmp(username, "postmaster") != 0)
			return 0;
		hostname = "localhost";
	} else {
		*hostname++ = '\0';
	}

	if (strlcpy(path->user, username, sizeof(path->user))
	    >= sizeof(path->user))
		return 0;

	if (strlcpy(path->domain, hostname, sizeof(path->domain))
	    >= sizeof(path->domain))
		return 0;

	return 1;
}

int
valid_localpart(char *s)
{
#define IS_ATEXT(c)     (isalnum((int)(c)) || strchr("!#$%&'*+-/=?^_`{|}~", (c)))
nextatom:
        if (! IS_ATEXT(*s) || *s == '\0')
                return 0;
        while (*(++s) != '\0') {
                if (*s == '.')
                        break;
                if (IS_ATEXT(*s))
                        continue;
                return 0;
        }
        if (*s == '.') {
                s++;
                goto nextatom;
        }
        return 1;
}

int
valid_domainpart(char *s)
{
nextsub:
        if (!isalnum((int)*s))
                return 0;
        while (*(++s) != '\0') {
                if (*s == '.')
                        break;
                if (isalnum((int)*s) || *s == '-')
                        continue;
                return 0;
        }
        if (s[-1] == '-')
                return 0;
        if (*s == '.') {
		s++;
                goto nextsub;
	}
        return 1;
}

char *
ss_to_text(struct sockaddr_storage *ss)
{
	static char	 buf[NI_MAXHOST + 5];
	char		*p;

	buf[0] = '\0';
	p = buf;

	if (ss->ss_family == PF_INET6) {
		strlcpy(buf, "IPv6:", sizeof(buf));
		p = buf + 5;
	}

	if (getnameinfo((struct sockaddr *)ss, ss->ss_len, p,
	    NI_MAXHOST, NULL, 0, NI_NUMERICHOST))
		fatalx("ss_to_text: getnameinfo");

	return (buf);
}

int
valid_message_id(char *mid)
{
	u_int8_t cnt;

	/* [0-9]{10}\.[a-zA-Z0-9]{16} */
	for (cnt = 0; cnt < 10; ++cnt, ++mid)
		if (! isdigit((int)*mid))
			return 0;

	if (*mid++ != '.')
		return 0;

	for (cnt = 0; cnt < 16; ++cnt, ++mid)
		if (! isalnum((int)*mid))
			return 0;

	return (*mid == '\0');
}

int
valid_message_uid(char *muid)
{
	u_int8_t cnt;

	/* [0-9]{10}\.[a-zA-Z0-9]{16}\.[0-9]{0,} */
	for (cnt = 0; cnt < 10; ++cnt, ++muid)
		if (! isdigit((int)*muid))
			return 0;

	if (*muid++ != '.')
		return 0;

	for (cnt = 0; cnt < 16; ++cnt, ++muid)
		if (! isalnum((int)*muid))
			return 0;

	if (*muid++ != '.')
		return 0;

	for (cnt = 0; *muid != '\0'; ++cnt, ++muid)
		if (! isdigit((int)*muid))
			return 0;

	return (cnt != 0);
}

char *
time_to_text(time_t when)
{
	struct tm *lt;
	static char buf[40]; 
	char *day[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char *month[] = {"Jan","Feb","Mar","Apr","May","Jun",
		       "Jul","Aug","Sep","Oct","Nov","Dec"};

	lt = localtime(&when);
	if (lt == NULL || when == 0) 
		fatalx("time_to_text: localtime");

	/* We do not use strftime because it is subject to locale substitution*/
	if (! bsnprintf(buf, sizeof(buf), "%s, %d %s %d %02d:%02d:%02d %c%02d%02d (%s)",
		day[lt->tm_wday], lt->tm_mday, month[lt->tm_mon],
		lt->tm_year + 1900,
		lt->tm_hour, lt->tm_min, lt->tm_sec,
		lt->tm_gmtoff >= 0 ? '+' : '-',
		abs((int)lt->tm_gmtoff / 3600),
		abs((int)lt->tm_gmtoff % 3600) / 60,
		lt->tm_zone))
		fatalx("time_to_text: bsnprintf");
	
	return buf;
}

/*
 * Check file for security. Based on usr.bin/ssh/auth.c.
 */
int
secure_file(int fd, char *path, struct passwd *pw, int mayread)
{
	char		 buf[MAXPATHLEN];
	char		 homedir[MAXPATHLEN];
	struct stat	 st;
	char		*cp;

	if (realpath(path, buf) == NULL)
		return 0;

	if (realpath(pw->pw_dir, homedir) == NULL)
		homedir[0] = '\0';

	/* Check the open file to avoid races. */
	if (fstat(fd, &st) < 0 ||
	    !S_ISREG(st.st_mode) ||
	    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
	    (st.st_mode & (mayread ? 022 : 066)) != 0)
		return 0;

	/* For each component of the canonical path, walking upwards. */
	for (;;) {
		if ((cp = dirname(buf)) == NULL)
			return 0;
		strlcpy(buf, cp, sizeof(buf));

		if (stat(buf, &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != pw->pw_uid) ||
		    (st.st_mode & 022) != 0)
			return 0;

		/* We can stop checking after reaching homedir level. */
		if (strcmp(homedir, buf) == 0)
			break;

		/*
		 * dirname should always complete with a "/" path,
		 * but we can be paranoid and check for "." too
		 */
		if ((strcmp("/", buf) == 0) || (strcmp(".", buf) == 0))
			break;
	}

	return 1;
}

void
addargs(arglist *args, char *fmt, ...)
{
	va_list ap;
	char *cp;
	u_int nalloc;
	int r;

	va_start(ap, fmt);
	r = vasprintf(&cp, fmt, ap);
	va_end(ap);
	if (r == -1)
		fatal("addargs: argument too long");

	nalloc = args->nalloc;
	if (args->list == NULL) {
		nalloc = 32;
		args->num = 0;
	} else if (args->num+2 >= nalloc)
		nalloc *= 2;

	if (SIZE_T_MAX / nalloc < sizeof(char *))
		fatalx("addargs: nalloc * size > SIZE_T_MAX");
	args->list = realloc(args->list, nalloc * sizeof(char *));
	if (args->list == NULL)
		fatal("addargs: realloc");
	args->nalloc = nalloc;
	args->list[args->num++] = cp;
	args->list[args->num] = NULL;
}

void
lowercase(char *buf, char *s, size_t len)
{
	if (len == 0)
		fatalx("lowercase: len == 0");

	if (strlcpy(buf, s, len) >= len)
		fatalx("lowercase: truncation");

	while (*buf != '\0') {
		*buf = tolower((int)*buf);
		buf++;
	}
}

void
message_set_errormsg(struct message *messagep, char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);

	ret = vsnprintf(messagep->session_errorline, MAX_LINE_SIZE, fmt, ap);
	if (ret >= MAX_LINE_SIZE)
		strlcpy(messagep->session_errorline + (MAX_LINE_SIZE - 4), "...", 4);

	/* this should not happen */
	if (ret == -1)
		err(1, "vsnprintf");

	va_end(ap);
}

char *
message_get_errormsg(struct message *messagep)
{
	return messagep->session_errorline;
}

void
sa_set_port(struct sockaddr *sa, int port)
{
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct addrinfo hints, *res;
	int error;

	error = getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST);
	if (error)
		fatalx("sa_set_port: getnameinfo failed");

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICHOST|AI_NUMERICSERV;

	snprintf(sbuf, sizeof(sbuf), "%d", port);

	error = getaddrinfo(hbuf, sbuf, &hints, &res);
	if (error)
		fatalx("sa_set_port: getaddrinfo failed");

	memcpy(sa, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
}

struct path *
path_dup(struct path *path)
{
	struct path *pathp;

	pathp = calloc(sizeof(struct path), 1);
	if (pathp == NULL)
		fatal("calloc");

	*pathp = *path;

	return pathp;
}

u_int64_t
generate_uid(void)
{
	u_int64_t	id;
	struct timeval	tp;

	if (gettimeofday(&tp, NULL) == -1)
		fatal("generate_uid: time");

	id = (u_int32_t)tp.tv_sec;
	id <<= 32;
	id |= (u_int32_t)tp.tv_usec;
	usleep(1);

	return (id);
}

void
fdlimit(double percent)
{
	struct rlimit rl;

	if (percent < 0 || percent > 1)
		fatalx("fdlimit: parameter out of range");
	if (getrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("fdlimit: getrlimit");
	rl.rlim_cur = percent * rl.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rl) == -1)
		fatal("fdlimit: setrlimit");
}

int
availdesc(void)
{
	int avail;

	avail = getdtablesize();
	avail -= 3;		/* stdin, stdout, stderr */
	avail -= PROC_COUNT;	/* imsg channels */
	avail -= 5;		/* safety buffer */

	return (avail);
}

void
session_socket_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

void
session_socket_no_linger(int fd)
{
	struct linger	 lng;

	bzero(&lng, sizeof(lng));
	if (setsockopt(fd, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		fatal("session_socket_no_linger");
}

int
session_socket_error(int fd)
{
	int	 error, len;

	len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
		fatal("session_socket_error: getsockopt");

	return (error);
}
