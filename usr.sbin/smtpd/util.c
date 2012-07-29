/*	$OpenBSD: util.c,v 1.67 2012/07/29 17:21:43 gilles Exp $	*/

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

/* the mkdir_p() function is based on bin/mkdir/mkdir.c that is covered
 * by the following license: */
/*
 * Copyright (c) 1983, 1992, 1993
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
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fts.h>
#include <imsg.h>
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
#include "log.h"

const char *log_in6addr(const struct in6_addr *);
const char *log_sockaddr(struct sockaddr *);

static int temp_inet_net_pton_ipv6(const char *, void *, size_t);

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

/*
 * mkdir -p. Based on bin/mkdir/mkdir.c:mkpath()
 */
int
mkdir_p(char *path, mode_t mode)
{
	struct stat	 sb;
	char		*slash;
	int		 done, exists;
	mode_t		 dir_mode;

	dir_mode = mode | S_IWUSR | S_IXUSR;

	slash = path;

	for (;;) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		/* skip existing path components */
		exists = !stat(path, &sb);
		if (!done && exists && S_ISDIR(sb.st_mode)) {
			*slash = '/';
			continue;
		}

		if (mkdir(path, done ? mode : dir_mode) == 0) {
			if (mode > 0777 && chmod(path, mode) < 0)
				return (-1);
		} else {
			if (!exists) {
				/* Not there */
				return (-1);
			}
			if (!S_ISDIR(sb.st_mode)) {
				/* Is there, but isn't a directory */
				errno = ENOTDIR;
				return (-1);
			}
		}

		if (done)
			break;

		*slash = '/';
	}

	return (0);
}

int
ckdir(const char *path, mode_t mode, uid_t owner, gid_t group, int create)
{
	char		mode_str[12];
	int		ret;
	struct stat	sb;

	if (stat(path, &sb) == -1) {
		if (errno != ENOENT || create == 0) {
			warn("stat: %s", path);
			return (0);
		}

		/* chmod is deferred to avoid umask effect */
		if (mkdir(path, 0) == -1) {
			warn("mkdir: %s", path);
			return (0);
		}

		if (chown(path, owner, group) == -1) {
			warn("chown: %s", path);
			return (0);
		}

		if (chmod(path, mode) == -1) {
			warn("chmod: %s", path);
			return (0);
		}

		if (stat(path, &sb) == -1) {
			warn("stat: %s", path);
			return (0);
		}
	}

	ret = 1;

	/* check if it's a directory */
	if (!S_ISDIR(sb.st_mode)) {
		ret = 0;
		warnx("%s is not a directory", path);
	}

	/* check that it is owned by owner/group */
	if (sb.st_uid != owner) {
		ret = 0;
		warnx("%s is not owned by uid %d", path, owner);
	}
	if (sb.st_gid != group) {
		ret = 0;
		warnx("%s is not owned by gid %d", path, group);
	}

	/* check permission */
	if ((sb.st_mode & 07777) != mode) {
		ret = 0;
		strmode(mode, mode_str);
		mode_str[10] = '\0';
		warnx("%s must be %s (%o)", path, mode_str + 1, mode);
	}

	return ret;
}

int
rmtree(char *path, int keepdir)
{
	char		*path_argv[2];
	FTS		*fts;
	FTSENT		*e;
	int		 ret, depth;

	path_argv[0] = path;
	path_argv[1] = NULL;
	ret = 0;
	depth = 1;

	if ((fts = fts_open(path_argv, FTS_PHYSICAL, NULL)) == NULL) {
		warn("fts_open: %s", path);
		return (-1);
	}

	while ((e = fts_read(fts)) != NULL) {
		if (e->fts_number) {
			depth--;
			if (keepdir && e->fts_number == 1)
				continue;
			if (rmdir(e->fts_path) == -1) {
				warn("rmdir: %s", e->fts_path);
				ret = -1;
			}
			continue;
		}

		if (S_ISDIR(e->fts_statp->st_mode)) {
			e->fts_number = depth++;
			continue;
		}

		if (unlink(e->fts_path) == -1) {
			warn("unlink: %s", e->fts_path);
			ret = -1;
		}
	}

	fts_close(fts);

	return (ret);
}

int
mvpurge(char *from, char *to)
{
	size_t		 n;
	int		 retry;
	const char	*sep;
	char		 buf[MAXPATHLEN];

	if ((n = strlen(to)) == 0)
		fatalx("to is empty");

	sep = (to[n - 1] == '/') ? "" : "/";
	retry = 0;

    again:
	snprintf(buf, sizeof buf, "%s%s%u", to, sep, arc4random());
	if (rename(from, buf) == -1) {
		/* ENOTDIR has actually 2 meanings, and incorrect input
		 * could lead to an infinite loop. Consider that after
		 * 20 tries something is hopelessly wrong.
		 */
		if (errno == ENOTEMPTY || errno == EISDIR || errno == ENOTDIR) {
			if ((retry++) >= 20)
				return (-1);
			goto again;
		}
		return -1;
	}

	return 0;
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
valid_localpart(const char *s)
{
/*
 * RFC 5322 defines theses characters as valid: !#$%&'*+-/=?^_`{|}~
 * some of them are potentially dangerous, and not so used after all.
 */
#define IS_ATEXT(c)     (isalnum((int)(c)) || strchr("%+-=_", (c)))
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
valid_domainpart(const char *s)
{
	struct in_addr	 ina;
	struct in6_addr	 ina6;
	char		*c, domain[MAX_DOMAINPART_SIZE];

	if (*s == '[') {
		strlcpy(domain, s + 1, sizeof domain);

		c = strchr(domain, (int)']');
		if (!c || c[1] != '\0')
			return 0;

		*c = '\0';

		if (inet_pton(AF_INET6, domain, &ina6) == 1)
			return 1;
		if (inet_pton(AF_INET, domain, &ina) == 1)
			return 1;

		return 0;
	}

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

int
email_to_mailaddr(struct mailaddr *maddr, char *email)
{
	char *username;
	char *hostname;

	username = email;
	hostname = strrchr(username, '@');

	if (username[0] == '\0') {
		*maddr->user = '\0';
		*maddr->domain = '\0';
		return 1;
	}

	if (hostname == NULL) {
		if (strcasecmp(username, "postmaster") != 0)
			return 0;
		hostname = "localhost";
	} else {
		*hostname++ = '\0';
	}

	if (strlcpy(maddr->user, username, sizeof(maddr->user))
	    >= sizeof(maddr->user))
		return 0;

	if (strlcpy(maddr->domain, hostname, sizeof(maddr->domain))
	    >= sizeof(maddr->domain))
		return 0;

	return 1;
}

char *
ss_to_text(struct sockaddr_storage *ss)
{
	static char	 buf[NI_MAXHOST + 5];
	char		*p;

	buf[0] = '\0';
	p = buf;

	if (ss->ss_family == PF_INET) {
		in_addr_t addr;
		
		addr = ((struct sockaddr_in *)ss)->sin_addr.s_addr;
                addr = ntohl(addr);
                bsnprintf(p, NI_MAXHOST,
                    "%d.%d.%d.%d",
                    (addr >> 24) & 0xff,
                    (addr >> 16) & 0xff,
                    (addr >> 8) & 0xff,
                    addr & 0xff);
	}

	if (ss->ss_family == PF_INET6) {
		struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)ss;
		struct in6_addr	*in6_addr;

		strlcpy(buf, "IPv6:", sizeof(buf));
		p = buf + 5;
		in6_addr = &in6->sin6_addr;
		bsnprintf(p, NI_MAXHOST, "%s", log_in6addr(in6_addr));
	}

	return (buf);
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

int
text_to_netaddr(struct netaddr *netaddr, char *s)
{
	struct sockaddr_storage	ss;
	struct sockaddr_in	ssin;
	struct sockaddr_in6	ssin6;
	int			bits;

	if (strncmp("IPv6:", s, 5) == 0)
		s += 5;

	if (strchr(s, '/') != NULL) {
		/* dealing with netmask */

		bzero(&ssin, sizeof(struct sockaddr_in));
		bits = inet_net_pton(AF_INET, s, &ssin.sin_addr,
		    sizeof(struct in_addr));

		if (bits != -1) {
			ssin.sin_family = AF_INET;
			memcpy(&ss, &ssin, sizeof(ssin));
			ss.ss_len = sizeof(struct sockaddr_in);
		}
		else {
			bzero(&ssin6, sizeof(struct sockaddr_in6));
			bits = inet_net_pton(AF_INET6, s, &ssin6.sin6_addr,
			    sizeof(struct in6_addr));
			if (bits == -1) {

				/* XXX - until AF_INET6 support gets in base */
				if (errno != EAFNOSUPPORT) {
					log_warn("inet_net_pton");
					return 0;
				}
				bits = temp_inet_net_pton_ipv6(s,
				    &ssin6.sin6_addr,
				    sizeof(struct in6_addr));
			}
			if (bits == -1) {
				log_warn("inet_net_pton");
				return 0;
			}
			ssin6.sin6_family = AF_INET6;
			memcpy(&ss, &ssin6, sizeof(ssin6));
			ss.ss_len = sizeof(struct sockaddr_in6);
		}
	}
	else {
		/* IP address ? */
		if (inet_pton(AF_INET, s, &ssin.sin_addr) == 1) {
			ssin.sin_family = AF_INET;
			bits = 32;
			memcpy(&ss, &ssin, sizeof(ssin));
			ss.ss_len = sizeof(struct sockaddr_in);
		}
		else if (inet_pton(AF_INET6, s, &ssin6.sin6_addr) == 1) {
			ssin6.sin6_family = AF_INET6;
			bits = 128;
			memcpy(&ss, &ssin6, sizeof(ssin6));
			ss.ss_len = sizeof(struct sockaddr_in6);
		}
		else return 0;
	}

	netaddr->ss   = ss;
	netaddr->bits = bits;
	return 1;
}

int
text_to_relayhost(struct relayhost *relay, char *s)
{
	u_int32_t		 i;
	struct schema {
		char		*name;
		u_int8_t	 flags;
	} schemas [] = {
		{ "smtp://",		0				},
		{ "smtps://",		F_SMTPS				},
		{ "tls://",		F_STARTTLS			},
		{ "smtps+auth://",     	F_SMTPS|F_AUTH			},
		{ "tls+auth://",	F_STARTTLS|F_AUTH		},
		{ "ssl://",		F_SMTPS|F_STARTTLS		},
		{ "ssl+auth://",	F_SMTPS|F_STARTTLS|F_AUTH	}
	};
	const char	*errstr = NULL;
	char	*p;
	char	*sep;
	int	 len;

	for (i = 0; i < nitems(schemas); ++i)
		if (strncasecmp(schemas[i].name, s, strlen(schemas[i].name)) == 0)
			break;

	if (i == nitems(schemas)) {
		/* there is a schema, but it's not recognized */
		if (strstr(s, "://"))
			return 0;

		/* no schema, default to smtp:// */
		i = 0;
		p = s;
	}
	else
		p = s + strlen(schemas[i].name);

	relay->flags = schemas[i].flags;

	if ((sep = strrchr(p, ':')) != NULL) {
		relay->port = strtonum(sep+1, 1, 0xffff, &errstr);
		if (errstr)
			return 0;
		len = sep - p;
	}
	else 
		len = strlen(p);

	if (strlcpy(relay->hostname, p, sizeof (relay->hostname))
	    >= sizeof (relay->hostname))
		return 0;

	relay->hostname[len] = 0;

	return 1;
}

/*
 * Check file for security. Based on usr.bin/ssh/auth.c.
 */
int
secure_file(int fd, char *path, char *userdir, uid_t uid, int mayread)
{
	char		 buf[MAXPATHLEN];
	char		 homedir[MAXPATHLEN];
	struct stat	 st;
	char		*cp;

	if (realpath(path, buf) == NULL)
		return 0;

	if (realpath(userdir, homedir) == NULL)
		homedir[0] = '\0';

	/* Check the open file to avoid races. */
	if (fstat(fd, &st) < 0 ||
	    !S_ISREG(st.st_mode) ||
	    (st.st_uid != 0 && st.st_uid != uid) ||
	    (st.st_mode & (mayread ? 022 : 066)) != 0)
		return 0;

	/* For each component of the canonical path, walking upwards. */
	for (;;) {
		if ((cp = dirname(buf)) == NULL)
			return 0;
		strlcpy(buf, cp, sizeof(buf));

		if (stat(buf, &st) < 0 ||
		    (st.st_uid != 0 && st.st_uid != uid) ||
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

int
lowercase(char *buf, char *s, size_t len)
{
	if (len == 0)
		return 0;

	if (strlcpy(buf, s, len) >= len)
		return 0;

	while (*buf != '\0') {
		*buf = tolower((int)*buf);
		buf++;
	}

	return 1;
}

void
xlowercase(char *buf, char *s, size_t len)
{
	if (len == 0)
		fatalx("lowercase: len == 0");

	if (! lowercase(buf, s, len))
		fatalx("lowercase: truncation");
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

u_int64_t
generate_uid(void)
{
	static u_int32_t id = 0;

	return ((uint64_t)(id++) << 32 | arc4random());
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
	int		error;
	socklen_t	len;

	len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1)
		fatal("session_socket_error: getsockopt");

	return (error);
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;
	u_int16_t		tmp16;

	bzero(&sa_in6, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	memcpy(&sa_in6.sin6_addr, addr, sizeof(sa_in6.sin6_addr));

	/* XXX thanks, KAME, for this ugliness... adopted from route/show.c */
	if (IN6_IS_ADDR_LINKLOCAL(&sa_in6.sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sa_in6.sin6_addr)) {
		memcpy(&tmp16, &sa_in6.sin6_addr.s6_addr[2], sizeof(tmp16));
		sa_in6.sin6_scope_id = ntohs(tmp16);
		sa_in6.sin6_addr.s6_addr[2] = 0;
		sa_in6.sin6_addr.s6_addr[3] = 0;
	}

	return (log_sockaddr((struct sockaddr *)&sa_in6));
}

const char *
log_sockaddr(struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

u_int32_t
evpid_to_msgid(u_int64_t evpid)
{
	return (evpid >> 32);
}

u_int64_t
msgid_to_evpid(u_int32_t msgid)
{
	return ((u_int64_t)msgid << 32);
}

const char *
parse_smtp_response(char *line, size_t len, char **msg, int *cont)
{
	size_t	 i;

	if (len >= SMTP_LINE_MAX)
		return "line too long";

	if (len > 3) {
		if (msg)
			*msg = line + 4;
		if (cont)
			*cont = (line[3] == '-');
	} else if (len == 3) {
		if (msg)
			*msg = line + 3;
		if (cont)
			*cont = 0;
	} else
		return "line too short";

	/* validate reply code */
	if (line[0] < '2' || line[0] > '5' || !isdigit(line[1]) ||
	    !isdigit(line[2]))
		return "reply code out of range";

	/* validate reply message */
	for (i = 0; i < len; i++)
		if (!isprint(line[i]))
			return "non-printable character in reply";

	return NULL;
}

static int
temp_inet_net_pton_ipv6(const char *src, void *dst, size_t size)
{
	int	ret;
	int	bits;
	char	buf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255:255:255:255/128")];
	char		*sep;
	const char	*errstr;

	if (strlcpy(buf, src, sizeof buf) >= sizeof buf) {
		errno = EMSGSIZE;
		return (-1);
	}

	sep = strchr(buf, '/');
	if (sep != NULL)
		*sep++ = '\0';

	ret = inet_pton(AF_INET6, buf, dst);
	if (ret != 1)
		return (-1);

	if (sep == NULL)
		return 128;

	bits = strtonum(sep, 0, 128, &errstr);
	if (errstr)
		return (-1);

	return bits;
}
