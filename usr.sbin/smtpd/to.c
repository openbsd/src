/*	$OpenBSD: to.c,v 1.19 2015/01/20 17:37:54 deraadt Exp $	*/

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
#include <limits.h>
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
static int alias_is_error(struct expandnode *, const char *, size_t);

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

	memset(&sa_in6, 0, sizeof(sa_in6));
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
	char  buffer[LINE_MAX];

	if (strlcpy(buffer, email, sizeof buffer) >= sizeof buffer)
		return 0;

	memset(maddr, 0, sizeof *maddr);

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
	static char  buffer[LINE_MAX];

	(void)strlcpy(buffer, maddr->user, sizeof buffer);
	(void)strlcat(buffer, "@", sizeof buffer);
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
		(void)strlcpy(buf, "local", sizeof buf);
	else if (sa->sa_family == AF_INET) {
		in_addr_t addr;

		addr = ((const struct sockaddr_in *)sa)->sin_addr.s_addr;
		addr = ntohl(addr);
		(void)bsnprintf(p, NI_MAXHOST, "%d.%d.%d.%d",
		    (addr >> 24) & 0xff, (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff, addr & 0xff);
	}
	else if (sa->sa_family == AF_INET6) {
		const struct sockaddr_in6 *in6;
		const struct in6_addr	*in6_addr;

		in6 = (const struct sockaddr_in6 *)sa;
		(void)strlcpy(buf, "IPv6:", sizeof(buf));
		p = buf + 5;
		in6_addr = &in6->sin6_addr;
		(void)bsnprintf(p, NI_MAXHOST, "%s", in6addr_to_text(in6_addr));
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
		(void)strlcpy(dst, "0s", sizeof dst);
		return (dst);
	}

	dst[0] = '\0';
	if (t < 0) {
		(void)strlcpy(dst, "-", sizeof dst);
		t = -t;
	}

	s = t % 60;
	t /= 60;
	m = t % 60;
	t /= 60;
	h = t % 24;
	d = t / 24;

	if (d) {
		(void)snprintf(buf, sizeof buf, "%lldd", d);
		(void)strlcat(dst, buf, sizeof dst);
	}
	if (h) {
		(void)snprintf(buf, sizeof buf, "%dh", h);
		(void)strlcat(dst, buf, sizeof dst);
	}
	if (m) {
		(void)snprintf(buf, sizeof buf, "%dm", m);
		(void)strlcat(dst, buf, sizeof dst);
	}
	if (s) {
		(void)snprintf(buf, sizeof buf, "%ds", s);
		(void)strlcat(dst, buf, sizeof dst);
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

	memset(&ssin, 0, sizeof(struct sockaddr_in));
	memset(&ssin6, 0, sizeof(struct sockaddr_in6));

	if (strncasecmp("IPv6:", s, 5) == 0)
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
		uint16_t       	 flags;
	} schemas [] = {
		/*
		 * new schemas should be *appended* otherwise the default
		 * schema index needs to be updated later in this function.
		 */
		{ "smtp://",		0				},
		{ "lmtp://",		F_LMTP				},
		{ "smtp+tls://",       	F_TLS_OPTIONAL 			},
		{ "smtps://",		F_SMTPS				},
		{ "tls://",		F_STARTTLS			},
		{ "smtps+auth://",	F_SMTPS|F_AUTH			},
		{ "tls+auth://",	F_STARTTLS|F_AUTH		},
		{ "secure://",		F_SMTPS|F_STARTTLS		},
		{ "secure+auth://",	F_SMTPS|F_STARTTLS|F_AUTH	},
		{ "backup://",		F_BACKUP       			}
	};
	const char     *errstr = NULL;
	char	       *p, *q;
	char		buffer[1024];
	char	       *sep;
	size_t		i;
	int		len;

	memset(buffer, 0, sizeof buffer);
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
		i = 2;
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
	uint16_t	mask = F_SMTPS|F_STARTTLS|F_AUTH|F_TLS_OPTIONAL|F_LMTP|F_BACKUP;

	memset(buf, 0, sizeof buf);
	switch (relay->flags & mask) {
	case F_SMTPS|F_STARTTLS|F_AUTH:
		(void)strlcat(buf, "secure+auth://", sizeof buf);
		break;
	case F_SMTPS|F_STARTTLS:
		(void)strlcat(buf, "secure://", sizeof buf);
		break;
	case F_STARTTLS|F_AUTH:
		(void)strlcat(buf, "tls+auth://", sizeof buf);
		break;
	case F_SMTPS|F_AUTH:
		(void)strlcat(buf, "smtps+auth://", sizeof buf);
		break;
	case F_STARTTLS:
		(void)strlcat(buf, "tls://", sizeof buf);
		break;
	case F_SMTPS:
		(void)strlcat(buf, "smtps://", sizeof buf);
		break;
	case F_BACKUP:
		(void)strlcat(buf, "backup://", sizeof buf);
		break;
	case F_TLS_OPTIONAL:
		(void)strlcat(buf, "smtp+tls://", sizeof buf);
		break;
	case F_LMTP:
		(void)strlcat(buf, "lmtp://", sizeof buf);
		break;
	default:
		(void)strlcat(buf, "smtp://", sizeof buf);
		break;
	}
	if (relay->authlabel[0]) {
		(void)strlcat(buf, relay->authlabel, sizeof buf);
		(void)strlcat(buf, "@", sizeof buf);
	}
	(void)strlcat(buf, relay->hostname, sizeof buf);
	if (relay->port) {
		(void)strlcat(buf, ":", sizeof buf);
		(void)snprintf(port, sizeof port, "%d", relay->port);
		(void)strlcat(buf, port, sizeof buf);
	}
	return buf;
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

	memset(buf, 0, sizeof buf);
	(void)strlcpy(buf, r->r_decision == R_ACCEPT  ? "accept" : "reject", sizeof buf);
	if (r->r_tag[0]) {
		(void)strlcat(buf, " tagged ", sizeof buf);
		if (r->r_nottag)
			(void)strlcat(buf, "! ", sizeof buf);
		(void)strlcat(buf, r->r_tag, sizeof buf);
	}
	(void)strlcat(buf, " from ", sizeof buf);
	if (r->r_notsources)
		(void)strlcat(buf, "! ", sizeof buf);
	(void)strlcat(buf, r->r_sources->t_name, sizeof buf);

	(void)strlcat(buf, " for ", sizeof buf);
	if (r->r_notdestination)
		(void)strlcat(buf, "! ", sizeof buf);
	switch (r->r_desttype) {
	case DEST_DOM:
		if (r->r_destination == NULL) {
			(void)strlcat(buf, " any", sizeof buf);
			break;
		}
		(void)strlcat(buf, " domain ", sizeof buf);
		(void)strlcat(buf, r->r_destination->t_name, sizeof buf);
		if (r->r_mapping) {
			(void)strlcat(buf, " alias ", sizeof buf);
			(void)strlcat(buf, r->r_mapping->t_name, sizeof buf);
		}
		break;
	case DEST_VDOM:
		if (r->r_destination == NULL) {
			(void)strlcat(buf, " any virtual ", sizeof buf);
			(void)strlcat(buf, r->r_mapping->t_name, sizeof buf);
			break;
		}
		(void)strlcat(buf, " domain ", sizeof buf);
		(void)strlcat(buf, r->r_destination->t_name, sizeof buf);
		(void)strlcat(buf, " virtual ", sizeof buf);
		(void)strlcat(buf, r->r_mapping->t_name, sizeof buf);
		break;
	}

	switch (r->r_action) {
	case A_RELAY:
		(void)strlcat(buf, " relay", sizeof buf);
		break;
	case A_RELAYVIA:
		(void)strlcat(buf, " relay via ", sizeof buf);
		(void)strlcat(buf, relayhost_to_text(&r->r_value.relayhost), sizeof buf);
		break;
	case A_MAILDIR:
		(void)strlcat(buf, " deliver to maildir \"", sizeof buf);
		(void)strlcat(buf, r->r_value.buffer, sizeof buf);
		(void)strlcat(buf, "\"", sizeof buf);
		break;
	case A_MBOX:
		(void)strlcat(buf, " deliver to mbox", sizeof buf);
		break;
	case A_FILENAME:
		(void)strlcat(buf, " deliver to filename \"", sizeof buf);
		(void)strlcat(buf, r->r_value.buffer, sizeof buf);
		(void)strlcat(buf, "\"", sizeof buf);
		break;
	case A_MDA:
		(void)strlcat(buf, " deliver to mda \"", sizeof buf);
		(void)strlcat(buf, r->r_value.buffer, sizeof buf);
		(void)strlcat(buf, "\"", sizeof buf);
		break;
	case A_LMTP:
		(void)strlcat(buf, " deliver to lmtp \"", sizeof buf);
		(void)strlcat(buf, r->r_value.buffer, sizeof buf);
		(void)strlcat(buf, "\"", sizeof buf);
		break;
	case A_NONE:
		break;
	}
	    
	return buf;
}

int
text_to_userinfo(struct userinfo *userinfo, const char *s)
{
	char		buf[PATH_MAX];
	char	       *p;
	const char     *errstr;

	memset(buf, 0, sizeof buf);
	p = buf;
	while (*s && *s != ':')
		*p++ = *s++;
	if (*s++ != ':')
		goto error;

	if (strlcpy(userinfo->username, buf,
		sizeof userinfo->username) >= sizeof userinfo->username)
		goto error;

	memset(buf, 0, sizeof buf);
	p = buf;
	while (*s && *s != ':')
		*p++ = *s++;
	if (*s++ != ':')
		goto error;
	userinfo->uid = strtonum(buf, 0, UID_MAX, &errstr);
	if (errstr)
		goto error;

	memset(buf, 0, sizeof buf);
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
	char	buffer[LINE_MAX];
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

	memset(buffer, 0, sizeof buffer);
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
	if (alias_is_error(expandnode, s, l) ||
	    alias_is_include(expandnode, s, l) ||
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
	case EXPAND_ERROR:
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
	int	v = 0;

	if (*line == '"')
		v = 1;
	if (*(line+v) == '|') {
		if (strlcpy(alias->u.buffer, line + v + 1,
		    sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
			return 0;
		if (v) {
			v = strlen(alias->u.buffer);
			if (v == 0)
				return (0);
			if (alias->u.buffer[v-1] != '"')
				return (0);
			alias->u.buffer[v-1] = '\0';
		}
		alias->type = EXPAND_FILTER;
		return (1);
	}
	return (0);
}

static int
alias_is_username(struct expandnode *alias, const char *line, size_t len)
{
	memset(alias, 0, sizeof *alias);

	if (strlcpy(alias->u.user, line,
	    sizeof(alias->u.user)) >= sizeof(alias->u.user))
		return 0;

	while (*line) {
		if (!isalnum((unsigned char)*line) &&
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

	memset(alias, 0, sizeof *alias);

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
	(void)strlcpy(alias->u.mailaddr.user, line, sizeof(alias->u.mailaddr.user));
	(void)strlcpy(alias->u.mailaddr.domain, domain,
	    sizeof(alias->u.mailaddr.domain));

	while (*line) {
		char allowedset[] = "!#$%*/?|^{}`~&'+-=_.";
		if (!isalnum((unsigned char)*line) &&
		    strchr(allowedset, *line) == NULL)
			return 0;
		++line;
	}

	while (*domain) {
		char allowedset[] = "-.";
		if (!isalnum((unsigned char)*domain) &&
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
	memset(alias, 0, sizeof *alias);

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

	memset(alias, 0, sizeof *alias);

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

static int
alias_is_error(struct expandnode *alias, const char *line, size_t len)
{
	size_t	skip;

	memset(alias, 0, sizeof *alias);

	if (strncasecmp(":error:", line, 7) == 0)
		skip = 7;
	else if (strncasecmp("error:", line, 6) == 0)
		skip = 6;
	else
		return 0;

	if (strlcpy(alias->u.buffer, line + skip,
	    sizeof(alias->u.buffer)) >= sizeof(alias->u.buffer))
		return 0;

	if (strlen(alias->u.buffer) < 5)
		return 0;

	/* [45][0-9]{2} [a-zA-Z0-9].* */
	if (alias->u.buffer[3] != ' ' ||
	    !isalnum((unsigned char)alias->u.buffer[4]) ||
	    (alias->u.buffer[0] != '4' && alias->u.buffer[0] != '5') ||
	    !isdigit((unsigned char)alias->u.buffer[1]) ||
	    !isdigit((unsigned char)alias->u.buffer[2]))
		return 0;

	alias->type = EXPAND_ERROR;
	return 1;
}
