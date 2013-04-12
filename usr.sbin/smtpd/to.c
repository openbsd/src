/*	$OpenBSD: to.c,v 1.6 2013/04/12 18:22:49 eric Exp $	*/

/*
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fts.h>
#include <imsg.h>
#include <inttypes.h>
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

static const char *in6addr_to_text(const struct in6_addr *);
static int alias_is_filter(struct expandnode *, const char *, size_t);
static int alias_is_username(struct expandnode *, const char *, size_t);
static int alias_is_address(struct expandnode *, const char *, size_t);
static int alias_is_filename(struct expandnode *, const char *, size_t);
static int alias_is_include(struct expandnode *, const char *, size_t);

const char *
sockaddr_to_text(struct sockaddr *sa)
{
	static char	buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, buf, sizeof(buf), NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf);
}

static const char *
in6addr_to_text(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;
	uint16_t		tmp16;

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

	return (sockaddr_to_text((struct sockaddr *)&sa_in6));
}

int
text_to_mailaddr(struct mailaddr *maddr, const char *email)
{
	char *username;
	char *hostname;
	char  buffer[SMTPD_MAXLINESIZE];

	if (strlcpy(buffer, email, sizeof buffer) >= sizeof buffer)
		return 0;

	bzero(maddr, sizeof *maddr);

	username = buffer;
	hostname = strrchr(username, '@');

	if (hostname == NULL) {
		if (strlcpy(maddr->user, username, sizeof maddr->user)
		    >= sizeof maddr->user)
			return 0;
	}
	else if (username == hostname) {
		*hostname++ = '\0';
		if (strlcpy(maddr->domain, hostname, sizeof maddr->domain)
		    >= sizeof maddr->domain)
			return 0;
	}
	else {
		*hostname++ = '\0';
		if (strlcpy(maddr->user, username, sizeof maddr->user)
		    >= sizeof maddr->user)
			return 0;
		if (strlcpy(maddr->domain, hostname, sizeof maddr->domain)
		    >= sizeof maddr->domain)
			return 0;
	}	

	return 1;
}

const char *
mailaddr_to_text(const struct mailaddr *maddr)
{
	static char  buffer[SMTPD_MAXLINESIZE];

	strlcpy(buffer, maddr->user, sizeof buffer);
	strlcat(buffer, "@", sizeof buffer);
	if (strlcat(buffer, maddr->domain, sizeof buffer) >= sizeof buffer)
		return NULL;

	return buffer;
}


const char *
sa_to_text(const struct sockaddr *sa)
{
	static char	 buf[NI_MAXHOST + 5];
	char		*p;

	buf[0] = '\0';
	p = buf;

	if (sa->sa_family == AF_LOCAL)
		strlcpy(buf, "local", sizeof buf);
	else if (sa->sa_family == AF_INET) {
		in_addr_t addr;

		addr = ((const struct sockaddr_in *)sa)->sin_addr.s_addr;
		addr = ntohl(addr);
		bsnprintf(p, NI_MAXHOST, "%d.%d.%d.%d",
		    (addr >> 24) & 0xff, (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff, addr & 0xff);
	}
	else if (sa->sa_family == AF_INET6) {
		const struct sockaddr_in6 *in6;
		const struct in6_addr	*in6_addr;

		in6 = (const struct sockaddr_in6 *)sa;
		strlcpy(buf, "IPv6:", sizeof(buf));
		p = buf + 5;
		in6_addr = &in6->sin6_addr;
		bsnprintf(p, NI_MAXHOST, "%s", in6addr_to_text(in6_addr));
	}

	return (buf);
}

const char *
ss_to_text(const struct sockaddr_storage *ss)
{
	return (sa_to_text((const struct sockaddr*)ss));
}

const char *
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
	if (! bsnprintf(buf, sizeof(buf),
	    "%s, %d %s %d %02d:%02d:%02d %c%02d%02d (%s)",
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

const char *
duration_to_text(time_t t)
{
	static char	dst[64];
	char		buf[64];
	int		h, m, s;
	long long	d;

	if (t == 0) {
		strlcpy(dst, "0s", sizeof dst);
		return (dst);
	}

	dst[0] = '\0';
	if (t < 0) {
		strlcpy(dst, "-", sizeof dst);
		t = -t;
	}

	s = t % 60;
	t /= 60;
	m = t % 60;
	t /= 60;
	h = t % 24;
	d = t / 24;

	if (d) {
		snprintf(buf, sizeof buf, "%llid", d);
		strlcat(dst, buf, sizeof dst);
	}
	if (h) {
		snprintf(buf, sizeof buf, "%ih", h);
		strlcat(dst, buf, sizeof dst);
	}
	if (m) {
		snprintf(buf, sizeof buf, "%im", m);
		strlcat(dst, buf, sizeof dst);
	}
	if (s) {
		snprintf(buf, sizeof buf, "%is", s);
		strlcat(dst, buf, sizeof dst);
	}

	return (dst);
}

int
text_to_netaddr(struct netaddr *netaddr, const char *s)
{
	struct sockaddr_storage	ss;
	struct sockaddr_in	ssin;
	struct sockaddr_in6	ssin6;
	int			bits;

	bzero(&ssin, sizeof(struct sockaddr_in));
	bzero(&ssin6, sizeof(struct sockaddr_in6));

	if (strncmp("IPv6:", s, 5) == 0)
		s += 5;

	if (strchr(s, '/') != NULL) {
		/* dealing with netmask */
		bits = inet_net_pton(AF_INET, s, &ssin.sin_addr,
		    sizeof(struct in_addr));
		if (bits != -1) {
			ssin.sin_family = AF_INET;
			memcpy(&ss, &ssin, sizeof(ssin));
			ss.ss_len = sizeof(struct sockaddr_in);
		}
		else {
			bits = inet_net_pton(AF_INET6, s, &ssin6.sin6_addr,
			    sizeof(struct in6_addr));
			if (bits == -1) {
				log_warn("warn: inet_net_pton");
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
text_to_relayhost(struct relayhost *relay, const char *s)
{
	static const struct schema {
		const char	*name;
		uint8_t		 flags;
	} schemas [] = {
		{ "smtp://",		0				},
		{ "lmtp://",		F_LMTP				},
		{ "smtp+tls://",       	F_TLS_OPTIONAL 			},
		{ "smtps://",		F_SMTPS				},
		{ "tls://",		F_STARTTLS			},
		{ "smtps+auth://",	F_SMTPS|F_AUTH			},
		{ "tls+auth://",	F_STARTTLS|F_AUTH		},
		{ "ssl://",		F_SMTPS|F_STARTTLS		},
		{ "ssl+auth://",	F_SMTPS|F_STARTTLS|F_AUTH	},
		{ "backup://",		F_BACKUP       			}
	};
	const char     *errstr = NULL;
	char	       *p, *q;
	char		buffer[1024];
	char	       *sep;
	size_t		i;
	int		len;

	bzero(buffer, sizeof buffer);
	if (strlcpy(buffer, s, sizeof buffer) >= sizeof buffer)
		return 0;

	for (i = 0; i < nitems(schemas); ++i)
		if (strncasecmp(schemas[i].name, s,
		    strlen(schemas[i].name)) == 0)
			break;

	if (i == nitems(schemas)) {
		/* there is a schema, but it's not recognized */
		if (strstr(buffer, "://"))
			return 0;

		/* no schema, default to smtp+tls:// */
		i = 1;
		p = buffer;
	}
	else
		p = buffer + strlen(schemas[i].name);

	relay->flags = schemas[i].flags;

	/* need to specify an explicit port for LMTP */
	if (relay->flags & F_LMTP)
		relay->port = 0;

	if ((sep = strrchr(p, ':')) != NULL) {
		*sep = 0;
		relay->port = strtonum(sep+1, 1, 0xffff, &errstr);
		if (errstr)
			return 0;
		len = sep - p;
	}
	else
		len = strlen(p);

	if ((relay->flags & F_LMTP) && (relay->port == 0))
		return 0;

	relay->hostname[len] = 0;

	q = strchr(p, '@');
	if (q == NULL && relay->flags & F_AUTH)
		return 0;
	if (q && !(relay->flags & F_AUTH))
		return 0;

	if (q == NULL) {
		if (strlcpy(relay->hostname, p, sizeof (relay->hostname))
		    >= sizeof (relay->hostname))
			return 0;
	} else {
		*q = 0;
		if (strlcpy(relay->authlabel, p, sizeof (relay->authlabel))
		    >= sizeof (relay->authlabel))
			return 0;
		if (strlcpy(relay->hostname, q + 1, sizeof (relay->hostname))
		    >= sizeof (relay->hostname))
			return 0;
	}
	return 1;
}

const char *
relayhost_to_text(const struct relayhost *relay)
{
	static char	buf[4096];
	char		port[4096];

	bzero(buf, sizeof buf);
	switch (relay->flags) {
	case F_SMTPS|F_STARTTLS|F_AUTH:
		strlcat(buf, "ssl+auth://", sizeof buf);
		break;
	case F_SMTPS|F_STARTTLS:
		strlcat(buf, "ssl://", sizeof buf);
		break;
	case F_STARTTLS|F_AUTH:
		strlcat(buf, "tls+auth://", sizeof buf);
		break;
	case F_SMTPS|F_AUTH:
		strlcat(buf, "smtps+auth://", sizeof buf);
		break;
	case F_STARTTLS:
		strlcat(buf, "tls://", sizeof buf);
		break;
	case F_SMTPS:
		strlcat(buf, "smtps://", sizeof buf);
		break;
	case F_BACKUP:
		strlcat(buf, "backup://", sizeof buf);
		break;
	case F_TLS_OPTIONAL:
		strlcat(buf, "smtp+tls://", sizeof buf);
		break;
	case F_LMTP:
		strlcat(buf, "lmtp://", sizeof buf);
		break;
	default:
		strlcat(buf, "smtp://", sizeof buf);
		break;
	}
	if (relay->authlabel[0]) {
		strlcat(buf, relay->authlabel, sizeof buf);
		strlcat(buf, "@", sizeof buf);
	}
	strlcat(buf, relay->hostname, sizeof buf);
	if (relay->port) {
		strlcat(buf, ":", sizeof buf);
		snprintf(port, sizeof port, "%d", relay->port);
		strlcat(buf, port, sizeof buf);
	}
	return buf;
}

uint32_t
evpid_to_msgid(uint64_t evpid)
{
	return (evpid >> 32);
}

uint64_t
msgid_to_evpid(uint32_t msgid)
{
	return ((uint64_t)msgid << 32);
}

uint64_t
text_to_evpid(const char *s)
{
	uint64_t ulval;
	char	 *ep;

	errno = 0;
	ulval = strtoull(s, &ep, 16);
	if (s[0] == '\0' || *ep != '\0')
		return 0;
	if (errno == ERANGE && ulval == ULLONG_MAX)
		return 0;
	if (ulval == 0)
		return 0;
	return (ulval);
}

uint32_t
text_to_msgid(const char *s)
{
	uint64_t ulval;
	char	 *ep;

	errno = 0;
	ulval = strtoull(s, &ep, 16);
	if (s[0] == '\0' || *ep != '\0')
		return 0;
	if (errno == ERANGE && ulval == ULLONG_MAX)
		return 0;
	if (ulval == 0)
		return 0;
	if (ulval > 0xffffffff)
		return 0;
	return (ulval & 0xffffffff);
}

const char *
rule_to_text(struct rule *r)
{
	static char buf[4096];

	bzero(buf, sizeof buf);
	strlcpy(buf, r->r_decision == R_ACCEPT  ? "accept" : "reject", sizeof buf);
	if (r->r_tag[0]) {
		strlcat(buf, " on ", sizeof buf);
		strlcat(buf, r->r_tag, sizeof buf);
	}
	strlcat(buf, " from ", sizeof buf);
	strlcat(buf, r->r_sources->t_name, sizeof buf);

	switch (r->r_desttype) {
	case DEST_DOM:
		if (r->r_destination == NULL) {
			strlcat(buf, " for any", sizeof buf);
			break;
		}
		strlcat(buf, " for domain ", sizeof buf);
		strlcat(buf, r->r_destination->t_name, sizeof buf);
		if (r->r_mapping) {
			strlcat(buf, " alias ", sizeof buf);
			strlcat(buf, r->r_mapping->t_name, sizeof buf);
		}
		break;
	case DEST_VDOM:
		if (r->r_destination == NULL) {
			strlcat(buf, " for any virtual ", sizeof buf);
			strlcat(buf, r->r_mapping->t_name, sizeof buf);
			break;
		}
		strlcat(buf, " for domain ", sizeof buf);
		strlcat(buf, r->r_destination->t_name, sizeof buf);
		strlcat(buf, " virtual ", sizeof buf);
		strlcat(buf, r->r_mapping->t_name, sizeof buf);
		break;
	}

	switch (r->r_action) {
	case A_RELAY:
		strlcat(buf, " relay", sizeof buf);
		break;
	case A_RELAYVIA:
		strlcat(buf, " relay via ", sizeof buf);
		strlcat(buf, relayhost_to_text(&r->r_value.relayhost), sizeof buf);
		break;
	case A_MAILDIR:
		strlcat(buf, " deliver to maildir \"", sizeof buf);
		strlcat(buf, r->r_value.buffer, sizeof buf);
		strlcat(buf, "\"", sizeof buf);
		break;
	case A_MBOX:
		strlcat(buf, " deliver to mbox", sizeof buf);
		break;
	case A_FILENAME:
		strlcat(buf, " deliver to filename \"", sizeof buf);
		strlcat(buf, r->r_value.buffer, sizeof buf);
		strlcat(buf, "\"", sizeof buf);
		break;
	case A_MDA:
		strlcat(buf, " deliver to mda \"", sizeof buf);
		strlcat(buf, r->r_value.buffer, sizeof buf);
		strlcat(buf, "\"", sizeof buf);
		break;
	}
	    
	return buf;
}

int
text_to_userinfo(struct userinfo *userinfo, const char *s)
{
	char		buf[MAXPATHLEN];
	char	       *p;
	const char     *errstr;

	bzero(buf, sizeof buf);
	p = buf;
	while (*s && *s != ':')
		*p++ = *s++;
	if (*s++ != ':')
		goto error;

	if (strlcpy(userinfo->username, buf,
		sizeof userinfo->username) >= sizeof userinfo->username)
		goto error;

	bzero(buf, sizeof buf);
	p = buf;
	while (*s && *s != ':')
		*p++ = *s++;
	if (*s++ != ':')
		goto error;
	userinfo->uid = strtonum(buf, 0, UID_MAX, &errstr);
	if (errstr)
		goto error;

	bzero(buf, sizeof buf);
	p = buf;
	while (*s && *s != ':')
		*p++ = *s++;
	if (*s++ != ':')
		goto error;
	userinfo->gid = strtonum(buf, 0, GID_MAX, &errstr);
	if (errstr)
		goto error;

	if (strlcpy(userinfo->directory, s,
		sizeof userinfo->directory) >= sizeof userinfo->directory)
		goto error;

	return 1;

error:
	return 0;
}

int
text_to_credentials(struct credentials *creds, const char *s)
{
	char   *p;
	char	buffer[SMTPD_MAXLINESIZE];
	size_t	offset;

	p = strchr(s, ':');
	if (p == NULL) {
		creds->username[0] = '\0';
		if (strlcpy(creds->password, s, sizeof creds->password)
		    >= sizeof creds->password)
			return 0;
		return 1;
	}

	offset = p - s;

	bzero(buffer, sizeof buffer);
	if (strlcpy(buffer, s, sizeof buffer) >= sizeof buffer)
		return 0;
	p = buffer + offset;
	*p = '\0';

	if (strlcpy(creds->username, buffer, sizeof creds->username)
	    >= sizeof creds->username)
		return 0;
	if (strlcpy(creds->password, p+1, sizeof creds->password)
	    >= sizeof creds->password)
		return 0;

	return 1;
}

int
text_to_expandnode(struct expandnode *expandnode, const char *s)
{
	size_t	l;

	l = strlen(s);
	if (alias_is_include(expandnode, s, l) ||
	    alias_is_filter(expandnode, s, l) ||
	    alias_is_filename(expandnode, s, l) ||
	    alias_is_address(expandnode, s, l) ||
	    alias_is_username(expandnode, s, l))
		return (1);

	return (0);
}

const char *
expandnode_to_text(struct expandnode *expandnode)
{
	switch (expandnode->type) {
	case EXPAND_FILTER:
	case EXPAND_FILENAME:
	case EXPAND_INCLUDE:
		return expandnode->u.buffer;
	case EXPAND_USERNAME:
		return expandnode->u.user;
	case EXPAND_ADDRESS:
		return mailaddr_to_text(&expandnode->u.mailaddr);
	case EXPAND_INVALID:
		break;
	}

	return NULL;
}


/******/
static int
alias_is_filter(struct expandnode *alias, const char *line, size_t len)
{
	if (*line == '|') {
		if (strlcpy(alias->u.buffer, line + 1,
			sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
			return 0;
		alias->type = EXPAND_FILTER;
		return 1;
	}
	return 0;
}

static int
alias_is_username(struct expandnode *alias, const char *line, size_t len)
{
	bzero(alias, sizeof *alias);

	if (strlcpy(alias->u.user, line,
	    sizeof(alias->u.user)) >= sizeof(alias->u.user))
		return 0;

	while (*line) {
		if (!isalnum((int)*line) &&
		    *line != '_' && *line != '.' && *line != '-')
			return 0;
		++line;
	}

	alias->type = EXPAND_USERNAME;
	return 1;
}

static int
alias_is_address(struct expandnode *alias, const char *line, size_t len)
{
	char *domain;

	bzero(alias, sizeof *alias);

	if (len < 3)	/* x@y */
		return 0;

	domain = strchr(line, '@');
	if (domain == NULL)
		return 0;

	/* @ cannot start or end an address */
	if (domain == line || domain == line + len - 1)
		return 0;

	/* scan pre @ for disallowed chars */
	*domain++ = '\0';
	strlcpy(alias->u.mailaddr.user, line, sizeof(alias->u.mailaddr.user));
	strlcpy(alias->u.mailaddr.domain, domain,
	    sizeof(alias->u.mailaddr.domain));

	while (*line) {
		char allowedset[] = "!#$%*/?|^{}`~&'+-=_.";
		if (!isalnum((int)*line) &&
		    strchr(allowedset, *line) == NULL)
			return 0;
		++line;
	}

	while (*domain) {
		char allowedset[] = "-.";
		if (!isalnum((int)*domain) &&
		    strchr(allowedset, *domain) == NULL)
			return 0;
		++domain;
	}

	alias->type = EXPAND_ADDRESS;
	return 1;
}

static int
alias_is_filename(struct expandnode *alias, const char *line, size_t len)
{
	bzero(alias, sizeof *alias);

	if (*line != '/')
		return 0;

	if (strlcpy(alias->u.buffer, line,
	    sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
		return 0;
	alias->type = EXPAND_FILENAME;
	return 1;
}

static int
alias_is_include(struct expandnode *alias, const char *line, size_t len)
{
	size_t skip;

	bzero(alias, sizeof *alias);

	if (strncasecmp(":include:", line, 9) == 0)
		skip = 9;
	else if (strncasecmp("include:", line, 8) == 0)
		skip = 8;
	else
		return 0;

	if (! alias_is_filename(alias, line + skip, len - skip))
		return 0;

	alias->type = EXPAND_INCLUDE;
	return 1;
}
