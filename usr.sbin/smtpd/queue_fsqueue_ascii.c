/*	$OpenBSD: queue_fsqueue_ascii.c,v 1.6 2011/11/06 16:55:32 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"


#define	KW_VERSION		"version"
#define	KW_EVPID		"evpid"
#define	KW_TYPE			"type"
#define	KW_HELO			"helo"
#define	KW_HOSTNAME    		"hostname"
#define	KW_ERRORLINE   		"errorline"
#define	KW_SOCKADDR   		"sockaddr"
#define	KW_SENDER		"sender"
#define	KW_RCPT			"rcpt"
#define	KW_DEST   		"dest"
#define	KW_CTIME		"ctime"
#define	KW_EXPIRE		"expire"
#define	KW_RETRY		"retry"
#define	KW_LAST_TRY		"last-try"
#define	KW_FLAGS		"flags"

#define	KW_MDA_METHOD		"mda-method"
#define	KW_MDA_BUFFER		"mda-buffer"
#define	KW_MDA_USER		"mda-user"

#define	KW_MTA_RELAY_HOST	"mta-relay-hostname"
#define	KW_MTA_RELAY_PORT	"mta-relay-port"
#define	KW_MTA_RELAY_FLAGS	"mta-relay-flags"
#define	KW_MTA_RELAY_CERT	"mta-relay-cert"
#define	KW_MTA_RELAY_AUTHMAP	"mta-relay-authmap"

int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);
int	fsqueue_dump_envelope_ascii(FILE *, struct envelope *);

static int
ascii_load_version(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->version = strtonum(buf, 0, 0xffffffff, &errstr);
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_version(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %d\n", KW_VERSION, SMTPD_ENVELOPE_VERSION);
	return 1;
}


static int
ascii_load_evpid(struct envelope *ep, char *buf)
{
	char *endptr;
	
	ep->id = strtoull(buf, &endptr, 16);
	if (buf[0] == '\0' || *endptr != '\0')
		return 0;
	if (errno == ERANGE && ep->id == ULLONG_MAX)
		return 0;
	return 1;
}

static int
ascii_dump_evpid(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %016" PRIx64 "\n", KW_EVPID, ep->id);
	return 1;
}


static int
ascii_load_type(struct envelope *ep, char *buf)
{
	if (strcasecmp(buf, "mda") == 0)
		ep->type = D_MDA;
	else if (strcasecmp(buf, "mta") == 0)
		ep->type = D_MTA;
	else if (strcasecmp(buf, "bounce") == 0)
		ep->type = D_BOUNCE;
	else
		return 0;
	return 1;
}

static int
ascii_dump_type(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: ", KW_TYPE);
	switch (ep->type) {
	case D_MDA:
		fprintf(fp, "mda\n");
		break;
	case D_MTA:
		fprintf(fp, "mta\n");
		break;
	case D_BOUNCE:
		fprintf(fp, "bounce\n");
		break;
	default:
		return 0;
	}
	return 1;
}


static int
ascii_load_mda_method(struct envelope *ep, char *buf)
{
	if (strcasecmp(buf, "mbox") == 0)
		ep->agent.mda.method = A_MBOX;
	else if (strcasecmp(buf, "maildir") == 0)
		ep->agent.mda.method = A_MAILDIR;
	else if (strcasecmp(buf, "filename") == 0)
		ep->agent.mda.method = A_FILENAME;
	else if (strcasecmp(buf, "external") == 0)
		ep->agent.mda.method = A_EXT;
	else
		return 0;
	return 1;
}

static int
ascii_dump_mda_method(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: ", KW_MDA_METHOD);
	switch (ep->agent.mda.method) {
	case A_MAILDIR:
		fprintf(fp, "maildir\n");
		break;
	case A_MBOX:
		fprintf(fp, "mbox\n");
		break;
	case A_FILENAME:
		fprintf(fp, "filename\n");
		break;
	case A_EXT:
		fprintf(fp, "external\n");
		break;
	default:
		return 0;
	}
	return 1;
}

static int
ascii_load_mda_buffer(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->agent.mda.to.buffer, buf,
		sizeof (ep->agent.mda.to.buffer))
	    >= sizeof (ep->agent.mda.to.buffer))
		return 0;
	return 1;
}

static int
ascii_dump_mda_buffer(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s\n", KW_MDA_BUFFER,
	    ep->agent.mda.to.buffer);
	return 1;
}


static int
ascii_load_mda_user(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->agent.mda.as_user, buf,
		sizeof (ep->agent.mda.as_user))
	    >= sizeof (ep->agent.mda.as_user))
		return 0;
	return 1;
}

static int
ascii_dump_mda_user(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s\n", KW_MDA_USER,
	    ep->agent.mda.as_user);
	return 1;
}


static int
ascii_load_helo(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->helo, buf,
		sizeof (ep->helo))
	    >= sizeof (ep->helo))
		return 0;
	return 1;
}

static int
ascii_dump_helo(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s\n", KW_HELO, ep->helo);
	return 1;
}


static int
ascii_load_hostname(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->hostname, buf,
		sizeof (ep->hostname))
	    >= sizeof (ep->hostname))
		return 0;
	return 1;
}

static int
ascii_dump_hostname(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s\n", KW_HOSTNAME, ep->hostname);
	return 1;
}

static int
ascii_load_errorline(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->errorline, buf,
		sizeof(ep->errorline)) >=
		sizeof(ep->errorline))
	    return 0;
		
	return 1;
}

static int
ascii_dump_errorline(struct envelope *ep, FILE *fp)
{
	if (ep->errorline[0])
		fprintf(fp, "%s: %s\n", KW_ERRORLINE,
		    ep->errorline);
	return 1;
}


static int
ascii_load_sockaddr(struct envelope *ep, char *buf)
{
	struct sockaddr_in6 ssin6;
	struct sockaddr_in  ssin;

	if (strncasecmp("IPv6:", buf, 5) == 0) {
		if (inet_pton(AF_INET6, buf + 5, &ssin6.sin6_addr) != 1)
			return 0;
		ssin6.sin6_family = AF_INET6;
		memcpy(&ep->ss, &ssin6, sizeof(ssin6));
		ep->ss.ss_len = sizeof(struct sockaddr_in6);
	}
	else {
		if (inet_pton(AF_INET, buf, &ssin.sin_addr) != 1)
			return 0;
		ssin.sin_family = AF_INET;
		memcpy(&ep->ss, &ssin6, sizeof(ssin));
		ep->ss.ss_len = sizeof(struct sockaddr_in);
	}
	return 1;
}

static int
ascii_dump_sockaddr(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s\n", KW_SOCKADDR, ss_to_text(&ep->ss));
	return 1;
}


static int
ascii_load_sender(struct envelope *ep, char *buf)
{
	if (! email_to_mailaddr(&ep->sender, buf))
		return 0;
	return 1;
}

static int
ascii_dump_sender(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s@%s\n", KW_SENDER,
	    ep->sender.user, ep->sender.domain);
	return 1;
}


static int
ascii_load_rcpt(struct envelope *ep, char *buf)
{
	if (! email_to_mailaddr(&ep->rcpt, buf))
		return 0;
	ep->dest = ep->rcpt;
	return 1;
}

static int
ascii_dump_rcpt(struct envelope *ep, FILE *fp)
{
	fprintf(fp, "%s: %s@%s\n", KW_RCPT,
	    ep->dest.user, ep->dest.domain);
	return 1;
}


static int
ascii_load_dest(struct envelope *ep, char *buf)
{
	if (! email_to_mailaddr(&ep->dest, buf))
		return 0;
	return 1;
}

static int
ascii_dump_dest(struct envelope *ep, FILE *fp)
{
	if (strcmp(ep->dest.user, ep->dest.user) != 0 ||
	    strcmp(ep->dest.domain, ep->dest.domain) != 0)
		fprintf(fp, "%s: %s@%s\n", KW_DEST,
		    ep->dest.user,
		    ep->dest.domain);
	return 1;
}


static int
ascii_load_mta_relay_host(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->agent.mta.relay.hostname, buf,
		sizeof(ep->agent.mta.relay.hostname))
	    >= sizeof(ep->agent.mta.relay.hostname))
		return 0;
	return 1;
}

static int
ascii_dump_mta_relay_host(struct envelope *ep, FILE *fp)
{
	if (ep->agent.mta.relay.hostname[0])
		fprintf(fp, "%s: %s\n", KW_MTA_RELAY_HOST,
		    ep->agent.mta.relay.hostname);
	return 1;
}

static int
ascii_load_mta_relay_port(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->agent.mta.relay.port = htons(strtonum(buf, 0, 0xffff, &errstr));
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_mta_relay_port(struct envelope *ep, FILE *fp)
{
	if (ep->agent.mta.relay.port)
		fprintf(fp, "%s: %d\n", KW_MTA_RELAY_PORT,
		    ntohs(ep->agent.mta.relay.port));
	return 1;
}

static int
ascii_load_mta_relay_cert(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->agent.mta.relay.cert, buf,
		sizeof(ep->agent.mta.relay.cert))
	    >= sizeof(ep->agent.mta.relay.cert))
		return 0;
	return 1;
}

static int
ascii_dump_mta_relay_cert(struct envelope *ep, FILE *fp)
{
	if (ep->agent.mta.relay.cert[0])
		fprintf(fp, "%s: %s\n", KW_MTA_RELAY_CERT,
		    ep->agent.mta.relay.cert);
	return 1;
}

static int
ascii_load_mta_relay_authmap(struct envelope *ep, char *buf)
{
	if (strlcpy(ep->agent.mta.relay.authmap, buf,
		sizeof(ep->agent.mta.relay.authmap))
	    >= sizeof(ep->agent.mta.relay.authmap))
		return 0;
	return 1;
}

static int
ascii_dump_mta_relay_authmap(struct envelope *ep, FILE *fp)
{
	if (ep->agent.mta.relay.authmap[0])
		fprintf(fp, "%s: %s\n", KW_MTA_RELAY_AUTHMAP,
		    ep->agent.mta.relay.authmap);
	return 1;
}

static int
ascii_load_mta_relay_flags(struct envelope *ep, char *buf)
{
	char *flag;

	while ((flag = strsep(&buf, " ,|")) != NULL) {
		if (strcasecmp(flag, "smtps") == 0)
			ep->agent.mta.relay.flags |= F_SMTPS;
		else if (strcasecmp(flag, "tls") == 0)
			ep->agent.mta.relay.flags |= F_STARTTLS;
		else if (strcasecmp(flag, "auth") == 0)
			ep->agent.mta.relay.flags |= F_AUTH;
		else
			return 0;
	}

	return 1;
}

static int
ascii_dump_mta_relay_flags(struct envelope *ep, FILE *fp)
{
	if (ep->agent.mta.relay.flags) {
		fprintf(fp, "%s:", KW_MTA_RELAY_FLAGS);
		if (ep->agent.mta.relay.flags & F_SMTPS)
			fprintf(fp, " smtps");
		if (ep->agent.mta.relay.flags & F_STARTTLS)
			fprintf(fp, " tls");
		if (ep->agent.mta.relay.flags & F_AUTH)
			fprintf(fp, " auth");
		fprintf(fp, "\n");
	}
	return 1;
}

static int
ascii_load_ctime(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->creation = strtonum(buf, 0, 0xffffffff, &errstr);
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_ctime(struct envelope *ep, FILE *fp)
{
	if (ep->creation)
		fprintf(fp, "%s: %" PRId64 "\n", KW_CTIME,
		    (int64_t) ep->creation);
	return 1;
}


static int
ascii_load_expire(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->expire = strtonum(buf, 0, 0xffffffff, &errstr);
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_expire(struct envelope *ep, FILE *fp)
{
	if (ep->expire)
		fprintf(fp, "%s: %" PRId64 "\n", KW_EXPIRE,
		    (int64_t) ep->expire);
	return 1;
}

static int
ascii_load_retry(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->retry = strtonum(buf, 0, 0xffffffff, &errstr);
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_retry(struct envelope *ep, FILE *fp)
{
	if (ep->retry)
		fprintf(fp, "%s: %d\n", KW_RETRY,
		    ep->retry);
	return 1;
}

static int
ascii_load_lasttry(struct envelope *ep, char *buf)
{
	const char *errstr;

	ep->lasttry = strtonum(buf, 0, 0xffffffff, &errstr);
	if (errstr)
		return 0;
	return 1;
}

static int
ascii_dump_lasttry(struct envelope *ep, FILE *fp)
{
	if (ep->lasttry)
		fprintf(fp, "%s: %" PRId64 "\n", KW_LAST_TRY,
		    (int64_t) ep->lasttry);
	return 1;
}

static int
ascii_dump_flags(struct envelope *ep, FILE *fp)
{
	if (ep->flags) {
		fprintf(fp, "%s:", KW_FLAGS);
		if (ep->flags & DF_AUTHENTICATED)
			fprintf(fp, " authenticated");
		if (ep->flags & DF_ENQUEUED)
			fprintf(fp, " enqueued");
		if (ep->flags & DF_BOUNCE)
			fprintf(fp, " bounce");
		if (ep->flags & DF_INTERNAL)
			fprintf(fp, " internal");
		fprintf(fp, "\n");
	}
	return 1;
}

static int
ascii_load_flags(struct envelope *ep, char *buf)
{
	char *flag;

	while ((flag = strsep(&buf, " ,|")) != NULL) {
		if (strcasecmp(flag, "authenticated") == 0)
			ep->flags |= DF_AUTHENTICATED;
		else if (strcasecmp(flag, "enqueued") == 0)
			ep->flags |= DF_ENQUEUED;
		else if (strcasecmp(flag, "bounce") == 0)
			ep->flags |= DF_BOUNCE;
		else if (strcasecmp(flag, "internal") == 0)
			ep->flags |= DF_INTERNAL;
		else
			return 0;
	}
	return 1;
}

static int
ascii_dump_agent(struct envelope *ep, FILE *fp)
{
	if (! ascii_dump_type(ep, fp))
		return 0;

	switch (ep->type) {
	case D_MDA:
		if (! ascii_dump_mda_method(ep, fp) ||
		    ! ascii_dump_mda_buffer(ep, fp) ||
		    ! ascii_dump_mda_user(ep, fp))
			return 0;
		break;

	case D_MTA:
		if (! ascii_dump_mta_relay_host(ep, fp)  ||
		    ! ascii_dump_mta_relay_port(ep, fp)  ||
		    ! ascii_dump_mta_relay_cert(ep, fp)  ||
		    ! ascii_dump_mta_relay_authmap(ep, fp)  ||
		    ! ascii_dump_mta_relay_flags(ep, fp))
			return 0;
		break;

	case D_BOUNCE:
		/* nothing ! */
		break;

	default:
		return 0;
	}

	return 1;
}

int
fsqueue_load_envelope_ascii(FILE *fp, struct envelope *ep)
{
	char *buf, *lbuf;
	size_t	len;
	struct ascii_load_handler {
		char *kw;
		int (*hdl)(struct envelope *, char *);
	} ascii_load_handlers[] = {
		{ KW_VERSION,		ascii_load_version },
		{ KW_EVPID,		ascii_load_evpid },

		{ KW_HOSTNAME,		ascii_load_hostname },
		{ KW_SOCKADDR,		ascii_load_sockaddr },

		{ KW_HELO,		ascii_load_helo },
		{ KW_SENDER,		ascii_load_sender },
		{ KW_RCPT,		ascii_load_rcpt },

		{ KW_TYPE,		ascii_load_type },
		{ KW_DEST,		ascii_load_dest },

		{ KW_CTIME,		ascii_load_ctime },
		{ KW_EXPIRE,		ascii_load_expire },
		{ KW_RETRY,		ascii_load_retry },
		{ KW_LAST_TRY,		ascii_load_lasttry },

		{ KW_FLAGS,		ascii_load_flags },

		{ KW_ERRORLINE,		ascii_load_errorline },

		{ KW_MDA_METHOD,       	ascii_load_mda_method },
		{ KW_MDA_BUFFER,       	ascii_load_mda_buffer },
		{ KW_MDA_USER,		ascii_load_mda_user },

		{ KW_MTA_RELAY_HOST,   	ascii_load_mta_relay_host },
		{ KW_MTA_RELAY_PORT,   	ascii_load_mta_relay_port },
		{ KW_MTA_RELAY_FLAGS,  	ascii_load_mta_relay_flags },
		{ KW_MTA_RELAY_CERT,  	ascii_load_mta_relay_cert },
		{ KW_MTA_RELAY_AUTHMAP,	ascii_load_mta_relay_authmap },
	};
	int	i;
	int	n;
	int	ret;

	n = sizeof(ascii_load_handlers) / sizeof(struct ascii_load_handler);

	bzero(ep, sizeof (*ep));
	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		for (i = 0; i < n; ++i) {
			len = strlen(ascii_load_handlers[i].kw);
			if (! strncasecmp(ascii_load_handlers[i].kw, buf, len)) {
				/* skip kw and tailing whitespaces */
				buf += len;
				while (*buf && isspace(*buf))
					buf++;

				/* we *want* ':' */
				if (*buf != ':')
					continue;
				buf++;

				/* skip whitespaces after separator */
				while (*buf && isspace(*buf))
				    buf++;

				ret = ascii_load_handlers[i].hdl(ep, buf);
				if (ret == 0)
					goto err;
				break;
			}
		}

		/* unknown keyword */
		if (i == n)
			goto err;
	}
	free(lbuf);

	return 1;

err:
	free(lbuf);
	return 0;
}

int
fsqueue_dump_envelope_ascii(FILE *fp, struct envelope *ep)
{
	if (! ascii_dump_version(ep, fp)   ||
	    ! ascii_dump_evpid(ep, fp)	   ||
	    ! ascii_dump_agent(ep, fp)	   ||
	    ! ascii_dump_helo(ep, fp)	   ||
	    ! ascii_dump_hostname(ep, fp)  ||
	    ! ascii_dump_errorline(ep, fp) ||
	    ! ascii_dump_sockaddr(ep, fp)  ||
	    ! ascii_dump_sender(ep, fp)	   ||
	    ! ascii_dump_rcpt(ep, fp)	   ||
	    ! ascii_dump_dest(ep, fp)	   ||
	    ! ascii_dump_ctime(ep, fp)	   ||
	    ! ascii_dump_lasttry(ep, fp)   ||
	    ! ascii_dump_expire(ep, fp)	   ||
	    ! ascii_dump_retry(ep, fp)	   ||
	    ! ascii_dump_flags(ep, fp))
		goto err;

	if (fflush(fp) != 0)
		goto err;

	return 1;

err:
	return 0;
}
