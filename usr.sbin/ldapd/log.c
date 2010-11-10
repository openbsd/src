/*	$OpenBSD: log.c,v 1.3 2010/11/10 08:00:54 martinh Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <errno.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "ldapd.h"

int	 debug;
int	 verbose;

void
log_init(int n_debug)
{
	extern char	*__progname;

	debug = n_debug;

	if (!debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	tzset();
}

void
log_verbose(int v)
{
	verbose = v;
}

void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

void
vlog(int pri, const char *fmt, va_list ap)
{
	char		 datebuf[24];
	struct timeval	 tv;
	struct tm	*tm;
	char		*nfmt;
	size_t		 rc;
	time_t		 now;

	if (debug) {
		gettimeofday(&tv, NULL); 
		now = tv.tv_sec;
		tm = localtime(&now);
		rc = strftime(datebuf, sizeof(datebuf), "%b %e %H:%M:%S", tm);
		if (rc == 0)
			datebuf[0] = 0;
		fprintf(stderr, "%s.%03li [%i] ", datebuf, tv.tv_usec / 1000, getpid());

		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "%s\n", fmt) == -1) {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		}
		fflush(stderr);
	} else
		vsyslog(pri, fmt, ap);
}

void
log_warn(const char *emsg, ...)
{
	char	*nfmt;
	va_list	 ap;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_CRIT, "%s", strerror(errno));
	else {
		va_start(ap, emsg);

		if (asprintf(&nfmt, "%s: %s", emsg, strerror(errno)) == -1) {
			/* we tried it... */
			vlog(LOG_CRIT, emsg, ap);
			logit(LOG_CRIT, "%s", strerror(errno));
		} else {
			vlog(LOG_CRIT, nfmt, ap);
			free(nfmt);
		}
		va_end(ap);
	}
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_CRIT, emsg, ap);
	va_end(ap);
}

void
log_info(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_INFO, emsg, ap);
	va_end(ap);
}

void
log_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (verbose) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal: %s", strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal: %s: %s",
			    emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal: %s", emsg);

	exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}

const char *
print_host(struct sockaddr_storage *ss, char *buf, size_t len)
{
	if (getnameinfo((struct sockaddr *)ss, ss->ss_len,
	    buf, len, NULL, 0, NI_NUMERICHOST) != 0) {
		buf[0] = '\0';
		return (NULL);
	}
	return (buf);
}

void
hexdump(void *data, size_t len, const char *fmt, ...)
{
	uint8_t *p = data;
	va_list ap;

	if (verbose < 2 || !debug)
		return;

	va_start(ap, fmt);
	vlog(LOG_DEBUG, fmt, ap);
	va_end(ap);

	while (len--) {
		size_t ofs = p - (uint8_t *)data;
		if (ofs % 16 == 0)
			fprintf(stderr, "%s%04lx:", ofs == 0 ? "" : "\n", ofs);
		else if (ofs % 8 == 0)
			fprintf(stderr, " ");
		fprintf(stderr, " %02x", *p++);
	}
	fprintf(stderr, "\n");
}

/*
 * Display a list of ber elements.
 *
 */
void
ldap_debug_elements(struct ber_element *root, int context, const char *fmt, ...)
{
	va_list		 ap;
	static int	 indent = 0;
	long long	 v;
	int		 d;
	char		*buf, *visbuf;
	size_t		 len;
	u_int		 i;
	int		 constructed;
	struct ber_oid	 o;

	if (verbose < 2 || !debug)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		vlog(LOG_DEBUG, fmt, ap);
		va_end(ap);
	}

	/* calculate lengths */
	ber_calc_len(root);

	switch (root->be_encoding) {
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
		constructed = root->be_encoding;
		break;
	default:
		constructed = 0;
		break;
	}

	fprintf(stderr, "%*slen %lu ", indent, "", root->be_len);
	switch (root->be_class) {
	case BER_CLASS_UNIVERSAL:
		fprintf(stderr, "class: universal(%u) type: ", root->be_class);
		switch (root->be_type) {
		case BER_TYPE_EOC:
			fprintf(stderr, "end-of-content");
			break;
		case BER_TYPE_BOOLEAN:
			fprintf(stderr, "boolean");
			break;
		case BER_TYPE_INTEGER:
			fprintf(stderr, "integer");
			break;
		case BER_TYPE_BITSTRING:
			fprintf(stderr, "bit-string");
			break;
		case BER_TYPE_OCTETSTRING:
			fprintf(stderr, "octet-string");
			break;
		case BER_TYPE_NULL:
			fprintf(stderr, "null");
			break;
		case BER_TYPE_OBJECT:
			fprintf(stderr, "object");
			break;
		case BER_TYPE_ENUMERATED:
			fprintf(stderr, "enumerated");
			break;
		case BER_TYPE_SEQUENCE:
			fprintf(stderr, "sequence");
			break;
		case BER_TYPE_SET:
			fprintf(stderr, "set");
			break;
		}
		break;
	case BER_CLASS_APPLICATION:
		fprintf(stderr, "class: application(%u) type: ",
		    root->be_class);
		switch (root->be_type) {
		case LDAP_REQ_BIND:
		case LDAP_RES_BIND:
			fprintf(stderr, "bind");
			break;
		case LDAP_REQ_UNBIND_30:
			fprintf(stderr, "unbind");
			break;
		case LDAP_REQ_SEARCH:
			fprintf(stderr, "search");
			break;
		case LDAP_RES_SEARCH_ENTRY:
			fprintf(stderr, "search entry");
			break;
		case LDAP_RES_SEARCH_RESULT:
			fprintf(stderr, "search result");
			break;
		case LDAP_REQ_MODIFY:
		case LDAP_RES_MODIFY:
			fprintf(stderr, "modify");
			break;
		case LDAP_REQ_ADD:
		case LDAP_RES_ADD:
			fprintf(stderr, "add");
			break;
		case LDAP_REQ_DELETE_30:
		case LDAP_RES_DELETE:
			fprintf(stderr, "delete");
			break;
		case LDAP_REQ_MODRDN:
		case LDAP_RES_MODRDN:
			fprintf(stderr, "modrdn");
			break;
		case LDAP_REQ_COMPARE:
		case LDAP_RES_COMPARE:
			fprintf(stderr, "compare");
			break;
		case LDAP_REQ_ABANDON_30:
			fprintf(stderr, "abandon");
			break;
		case LDAP_REQ_EXTENDED:
		case LDAP_RES_EXTENDED:
			fprintf(stderr, "extended");
			break;
		}
		break;
	case BER_CLASS_PRIVATE:
		fprintf(stderr, "class: private(%u) type: ", root->be_class);
		fprintf(stderr, "encoding (%lu) type: ", root->be_encoding);
		break;
	case BER_CLASS_CONTEXT:
		fprintf(stderr, "class: context(%u) type: ", root->be_class);
		switch (context) {
		case LDAP_REQ_BIND:
			switch(root->be_type) {
			case LDAP_AUTH_SIMPLE:
				fprintf(stderr, "auth simple");
				break;
			}
			break;
		case LDAP_REQ_SEARCH:
			switch(root->be_type) {
			case LDAP_FILT_AND:
				fprintf(stderr, "and");
				break;
			case LDAP_FILT_OR:
				fprintf(stderr, "or");
				break;
			case LDAP_FILT_NOT:
				fprintf(stderr, "not");
				break;
			case LDAP_FILT_EQ:
				fprintf(stderr, "equal");
				break;
			case LDAP_FILT_SUBS:
				fprintf(stderr, "substring");
				break;
			case LDAP_FILT_GE:
				fprintf(stderr, "greater-or-equal");
				break;
			case LDAP_FILT_LE:
				fprintf(stderr, "less-or-equal");
				break;
			case LDAP_FILT_PRES:
				fprintf(stderr, "presence");
				break;
			case LDAP_FILT_APPR:
				fprintf(stderr, "approximate");
				break;
			}
			break;
		}
		break;
	default:
		fprintf(stderr, "class: <INVALID>(%u) type: ", root->be_class);
		break;
	}
	fprintf(stderr, "(%lu) encoding %lu ",
	    root->be_type, root->be_encoding);

	if (constructed)
		root->be_encoding = constructed;

	switch (root->be_encoding) {
	case BER_TYPE_BOOLEAN:
		if (ber_get_boolean(root, &d) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "%s(%d)\n", d ? "true" : "false", d);
		break;
	case BER_TYPE_INTEGER:
		if (ber_get_integer(root, &v) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "value %lld\n", v);
		break;
	case BER_TYPE_ENUMERATED:
		if (ber_get_enumerated(root, &v) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "value %lld\n", v);
		break;
	case BER_TYPE_BITSTRING:
		if (ber_get_bitstring(root, (void *)&buf, &len) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "hexdump ");
		for (i = 0; i < len; i++)
			fprintf(stderr, "%02x", buf[i]);
		fprintf(stderr, "\n");
		break;
	case BER_TYPE_OBJECT:
		if (ber_get_oid(root, &o) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		fprintf(stderr, "\n");
		break;
	case BER_TYPE_OCTETSTRING:
		if (ber_get_nstring(root, (void *)&buf, &len) == -1) {
			fprintf(stderr, "<INVALID>\n");
			break;
		}
		if ((visbuf = malloc(len * 4 + 1)) != NULL) {
			strvisx(visbuf, buf, len, 0);
			fprintf(stderr, "string \"%s\"\n",  visbuf);
			free(visbuf);
		}
		break;
	case BER_TYPE_NULL:	/* no payload */
	case BER_TYPE_EOC:
	case BER_TYPE_SEQUENCE:
	case BER_TYPE_SET:
	default:
		fprintf(stderr, "\n");
		break;
	}

	if (constructed && root->be_sub) {
		indent += 2;
		ldap_debug_elements(root->be_sub, context, NULL);
		indent -= 2;
	}
	if (root->be_next)
		ldap_debug_elements(root->be_next, context, NULL);
}

